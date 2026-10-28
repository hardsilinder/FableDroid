# Guest-image patch system (Xenia patch format)

Runtime patching of the loaded `default.xex` guest image, using the op shape
of Xenia's game-patches (`patch.toml`) format.

## How it works

- `src/core/fable2_patches.h` / `src/core/fable2_patches.cpp` — the patch table +
  applier. Each op is `{be8|be16|be32|be64, address, value}`, exactly the
  `[[patch.be32]] address/value` shape from Xenia's patch files.
- **No patch file.** The table is built in code; each patch's `enabled` comes
  from a `[patches]` key in `fable2_config.toml` (loaded in
  `OnPostInitLogging`, before the patches are applied). The old
  `fable2_patches.toml` is no longer read and can be deleted.
- `Fable2App::OnPostLoadXexImage()` (src/core/fable_2_app.h) calls
  `fable2::patches::ApplyAll(runtime()->memory(), PPCImageConfig)` — the SDK
  hook that runs after `default.xex` is decrypted/expanded into the guest
  arena and before the guest module launches (the SDK documents this hook as
  the place for data patches).
- The SDK marks XEX code/rodata pages **read-only** after load
  (`XexModule::LoadContinue` page-descriptor pass), so `PageWriteGuard`
  flips the touched guest pages to R/W via `heap->Protect()` and restores the
  original protection after the write. Safe at this point: no guest thread
  exists yet.
- Every op is logged: `[patches] <patch> be8 0xADDR: 0xOLD -> 0xNEW
  (data: takes effect at runtime)`, plus a summary line.

## Current patches (both default to **disabled**)

| Config key | Xenia patch | Data op (this table) | Code op (mid-asm hook) |
|---|---|---|---|
| `high_tick_rate` | High Tick Rate (Guy) | be8 0x83319511 = 0x3E (LF tick double 0x83319510: 15.0 → 30.0) | NOP of the store at 0x8233AEB4 → `fable2_hook_high_tick_rate_skip_store` |
| `higher_hf_tick_rate` | Higher HF Tick Rate (Ultra) | be8 0x83319519 = 0x4E (HF tick double 0x83319518: 30.0 → 60.0) | NOP of the store at 0x8233AE98 → `fable2_hook_high_hf_tick_rate_skip_store` |

Both stores are in `sub_8233AE50` (game init) and are the only writers of
those doubles; without the hooks the game overwrote the patched values and
the data ops had no effect. The game runs HF at twice LF (30:15). With only
`high_tick_rate` the ratio becomes 1:1 (30:30) and cloth physics misbehaves,
so turn on `higher_hf_tick_rate` with it (60:30).

This is the fundamental recomp vs. emulator split: **data patches work, code
patches don't** (a code patch here would mean editing the recompiled C++ at
build time, which codegen would clobber on the next run), so every code op is
a mid-asm hook.

## Validation (2026-09-15)

- Built (`build.cmd fable_2`, debug), launched the game (all ops applied,
  ~7 min run, clean shutdown; the 60 FPS entry later moved to a mid-asm
  hook and is no longer a guest-image patch):
  ```
  [patches] applying 'High Tick Rate' by Guy (2 ops) - Doubles tickrate to 30hz. ...
  [patches]   High Tick Rate be32 0x8233AEB4: 0xd8089510 -> 0x60000000 (code region: ...)
  [patches]   High Tick Rate be8 0x83319511: 0x2e -> 0x3e (data: takes effect at runtime)
  [patches] done: 2 op(s) applied, 0 skipped
  ```
- Game ran ~7 minutes with patches applied, clean user-initiated shutdown,
  no access violations or protection errors (first run, before the
  read-only-page handling existed, did fault on the code op — fixed by
  `PageWriteGuard`).

## Recomp-level patches (making code patches work)

Since guest .text is never executed, code-region Xenia patches must be
applied to the **recompiled code**. Two mechanisms, in order of preference:

### 1. Mid-asm hooks (preferred) — SDK-native, declarative

The SDK's `[[entrypoint.midasm_hook]]` config (manifest) injects a call to a
C++ function at a specific guest instruction address during codegen
(docs: https://rexglue-rexglue-sdk.mintlify.app/config/hooks). Registers
named in the hook are passed **by reference** (`PPCRegister&`), so the hook
function can rewrite them — "value injection".

Wiring (all three pieces):
1. `fable_2_manifest.toml` → `[[entrypoint.midasm_hook]]` with `address`,
   `name`, `registers`, and `after_instruction = true` so the call lands
   right after the patched instruction executes.
2. `src/core/fable2_hooks.cpp` → the hook function, plain C++ linkage matching
   the prototype codegen auto-emits into the generated code
   (`extern void fable2_hook_website_g1(PPCRegister& r9);`).
3. Nothing else — codegen handles the rest, and it survives codegen re-runs
   by construction (the manifest is a codegen input).

**Runtime toggle (done):** the hook body is C++ in the game process and
consults `fable2::config::Get()` on every call — a `[patches]` key in
`fable2_config.toml` enables/disables the patch with no rebuild.
(New hooks should follow the same pattern: read their toggle from
`fable2::config::Get()`.)

### 2. Post-codegen text patches (fallback)

`tools/apply_recomp_patches.py` mirrors a patch into codegen's emitted C++
by anchored text replacement; wired into CMakeLists.txt as a step **between
codegen and compiling fable_2_recomp** (keyed on
generated/default/codegen.build.stamp). Idempotent via a unique
`// [recomp-patch: <name>]` marker; missing anchor = build failure.
`FABLE2_RECOMP_PATCHES=0` skips all (A/B runs). Use only for patches that
can't be expressed as hooks (e.g. constants baked into memory operands).
Currently **empty**.

### Current recomp-level patches

| Patch | Mechanism | Change | Measured effect |
|---|---|---|---|
| Unlock Website Items (Guy) | mid-asm hook `fable2_hook_unlock_website` @ 0x8256E384 (after `rlwinm r9,r10,0,25,25`); toggle: `[patches] unlock_website` | Forces `r9 = 0x40` (bit 6) in `sub_8256E368`, so the website/Guild-chest item lookup reads as unlocked and runs the real lookup. Re-derives the Xenia "Unlock Website Items" intent for THIS build (the stock ops target a different revision's bytes). | Hook confirmed in generated code + clean startup; in-game chest unlock pending manual test |
| Unlock CE Content (Guy) | mid-asm hook `fable2_hook_unlock_ce` @ 0x824B3540 (after `rlwinm r10,r11,0,25,25`); toggle: `[patches] unlock_ce` | Forces `r10 = 0x40` (bit 6) in `sub_824B3528`, so the Collectors-Edition chest item lookup reads as unlocked and runs the real lookup. Same re-derivation approach. | Hook confirmed in generated code + clean startup; in-game chest unlock pending manual test |

To add a new code patch: a `[[entrypoint.midasm_hook]]` entry + hook
function gated on a `[patches]` config key (preferred), or an
`apply_recomp_patches.py` PATCHES entry (fallback). If it also needs a data
write, add it to the table in `src/core/fable2_patches.cpp`, gated on the
same key.

### FPS meter

`src/diagnostics/fps_meter.h` (included from main.cpp): strong override of the weak
recompiled `MainRenderLoop_82B9CD68` (0x82B9CD68, main loop, one call per frame; renamed from `sub_82B9CD68`) that counts
invocations in 5 s windows and appends `mainloop rate=NN.N/s` to
`fps_meter.log` next to the exe. Forward-only (counts, then calls the
original `__imp__` entry). Enabled with `FABLE2_FPS_METER=1`.

## Follow-ups

1. ~~Tie to config~~ **done**: every patch, data or code, is toggled by a
   `[patches]` key in `fable2_config.toml` (the separate
   `fable2_patches.toml` was removed).
2. **Other Xenia patches** for this title (from
   `4D5307F1 - Fable II (GOTY).patch.toml`). Done as mid-asm hooks:
   **Unlock Website Items**, **Unlock CE Content** (all re-derived for THIS
   build — see the recomp-level patches table above). Remaining (still to port,
   each needs the same build-mismatch investigation — the stock ops target a
   different revision): 1280x720 (be16 0x8238DF5A=0x0500), Disable MSAA
   (be8 0x8238DF3F=0x01; ported 2026-10-28 as the override
   src/core/hotfunc/unsorted/RenderConfigInit_8238DE58.cpp, cvar
   fable2_disable_msaa; these two addresses DO match this build), Disable Texture Morphing (be16 0x8220EF10=0x4280),
   21:9 / 32:9 widescreen — all .text/data-in-.text, so expect the same "applied
   but inert" behavior for their code ops if applied to the guest image.
3. **Oddity worth investigating (separate issue):** the recompiled C++ for
   `sub_8233AE50` (generated/default/fable_2_recomp.65.cpp:3211) was compiled
   from a word at 0x8233AEB4 of `0xD9009510` (stfd f0,0x9510(r8)), but the
   current `default.xex` image contains `0xD8089510` (stfd f8,0x1510(r0))
   there — verified two independent ways: an independent AES-CBC +
   basic-compression-block expansion of default.xex, and a live read of the
   running process's guest arena (see scratch/fable2_image_decrypted_expanded.bin
   for the fully expanded image). Neighboring words 0x8233AE54 and
   0x8233AE98 differ too. Same file (SHA-verified, unchanged since
   2026-08-26) and the v0.10.0 XEX loader source is byte-identical to the
   nightly's — so the prebuilt codegen tool's image and the runtime image
   differ by a handful of bytes (register fields transposed / stray bits).
   Possibly a `be<enum>` struct-layout difference in
   `xex2_opt_file_format_info` between the prebuilt 0.10.0 tool and the
   source-built nightly runtime (block table at +8 vs +12). If any recompiled
   function's data operands come from those bytes, the recompiled code is
   subtly out of sync with what the runtime loads. Low priority (the game
   plays fine), but worth a dedicated look if anything mysterious happens.

## XEX2 decoding notes (for future reference)

`default.xex` (GOTY, media 716F0A0D): XEX2, header 0x4000, base 0x82000000,
image_size 0x1620000, session key = AES-CBC(retail_key, secinfo+0x150),
encryption=NORMAL (AES-CBC, zero IV), compression=BASIC = a block table of
`{data_size, zero_size}` pairs: the file stream is the concatenation of the
block data only; each block expands to `data_size` decrypted bytes followed by
`zero_size` zero bytes. Block table (this file):
`(0x168000, 0x8000), (0x1158000, 0x8000), (0x58000, 0x1D0000), (0x120000, 0)`.
