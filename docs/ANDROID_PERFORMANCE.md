# Android performance notes

Measurements were taken on an AYN Thor (Snapdragon 8 Gen 2, Adreno 740 at 680 MHz,
Android 13) with Mesa Turnip v27, at native resolution (1120×720 scene, 1280×720
output, scaled to 1920×1080).

The standard benchmark loads a save, then walks forward, back and sideways while panning
the camera, and reports the median FPS of the last nine 5-second windows. "Warm" means
the shader cache already holds the views being visited.

## Where it stands

| | Value |
|---|---|
| Median FPS while moving (warm) | **29.1** |
| Median frame time | 33.6 ms |
| 95th / 99th percentile frame | 39.5 ms / 46.9 ms |
| Worst frame per 5 s window | about 50–60 ms |
| First visit to an area (cold cache) | 25.3 FPS, hitches up to about 260 ms |

GPU time is about 33.8 ms per frame, just above the 33.3 ms needed for a locked 30.

## What made the difference

Gains are listed in the order they were found. Each one was A/B tested on the device,
with screenshots compared for correctness.

| Change | Result |
|---|---|
| CPU wait fixes (precise sleeps, shorter idle waits) and GPU diagnostics off | Up to 10.3 FPS with an older Turnip build |
| Newer Mesa Turnip v27 driver | 10.3 → 15.5 FPS |
| Bulk command writer (`gpu_bulk_type0`) | Halved CPU time in the GPU command thread |
| Shader translator: relaxed zero-multiply handling | 14.6 → 17.1 FPS |
| Implicit-LOD texture sampling | 17.1 → 18.1 FPS |
| Cheaper texture-fetch paths (unsigned variant, gamma-only branch, unconditional unsigned sample) | 18.1 → 21.7 FPS |
| Disable the game's 2x MSAA (`fable2_disable_msaa`) | 21.7 → 26.9 FPS |
| Upload shared memory before draws (`vulkan_shared_memory_upload_first`) once MSAA was off | 26.9 → 29.6 FPS (menu scene) |
| Persistent pipeline cache fixed (it was being wiped every launch) | Removed 200–650 ms hitches on revisits |
| Skip the redundant present clear (`present_render_pass_clear = false`) | About 0.24 ms GPU per frame |

MSAA can be turned back on in the launcher. It is off by default because it is the
single largest cost.

## Tried and rejected

Each was measured and gave no gain (or a loss), so it is not used:

- Three frames in flight instead of two: same FPS, one more frame of input latency.
- Turnip Gen8 V38 driver: same average FPS (29.4) as v27.
- Disabling robust buffer access: slightly slower.
- The game's own anisotropic filtering instead of forced 4x: no change.
- Tessellation factor 6 instead of 8, FIFO present mode, FP16 relaxed precision,
  floating-point contraction, alternative render-target formats, GMEM/tiling driver
  options: neutral or worse.
- Exponent-bias bit construction in shaders: measured regression.

## Why the last millisecond is hard

- The main scene pass runs in system memory rather than tile memory, because the game
  uses tessellation there (Turnip falls back to sysmem with tessellation).
- About 140 render-pass breaks per frame come from barriers. Most are caused by the
  emulated EDRAM model: render-to-texture results are resolved out to guest memory and
  loaded back as textures (bloom chain, scene buffers, shadow maps, 30–55 texture loads
  per frame).
- The slowest frames are CPU-bound bursts (about 47 ms of work between frames with no
  GPU wait) rather than GPU spikes.

The remaining options that keep full quality are structural: drawing directly into
textures without the resolve-and-reload round trip, and hoisting barriers so passes
don't split. Lowering the internal resolution would reach a locked 30 immediately, but
the project keeps native resolution by default.

## Tuned settings

The launcher writes these to `fable_2.toml` on every launch (see
`android/app/src/main/java/org/fable2/recomp/Settings.java`):

```toml
vulkan_android_custom_driver = "turnip27"
frame_wait_sleep_us = 100
posix_precise_wait_sleep = true
game_idle_sleep_us = 100
vulkan_robust_buffer_access = true
gpu_bulk_type0 = true
vulkan_shared_memory_upload_first = true
vulkan_submit_on_primary_buffer_end = true
vulkan_dynamic_rendering = true
spirv_split_temp_registers = true
vulkan_shared_memory_texel_reads = true
spirv_structured_forward_jumps = true
spirv_implicit_lod_sampling_1d = false
spirv_allow_contraction_pixel = false
spirv_allow_contraction_vertex = false
spirv_relaxed_precision_pixel = false
tessellation_factor_cap = 8.0
spirv_unconditional_unsigned_sample = true
spirv_gamma_only_branch = true
spirv_unsigned_texture_variant = true
present_render_pass_clear = false
fable2_disable_msaa = true          # launcher: Anti-aliasing
vulkan_max_frames_in_flight = 2     # launcher: Triple buffering (3)
draw_resolution_scale_x = 1         # launcher: Resolution scale
draw_resolution_scale_y = 1
```

## Correctness fixes found along the way

- **Cyan sky / missing world:** before the main pass the game aliases its depth buffer
  in another format. Each format round trip went float24 → unorm24 → float24, the far
  plane drifted and the sky failed its depth test. Fixed by skipping depth-to-depth
  format-change transfers (`rt_transfer_skip_mask = 4`, set in
  `src/core/fable_2_app.h`).
- **Pipeline cache never loading:** the cache file opened with its read position at
  the end, so it was treated as empty and rewritten each launch. Fixed in the SDK's
  `pipeline_cache.cpp`.
