// Hand-written override: "Disable MSAA" (Xenia patch be8 0x8238DF3F = 0x01).
// The first line makes tools/make_hotfunc_overrides.py skip this file.
//
// sub_8238DE58 initialises the renderer's back-buffer description. Its tail
// (0x8238DF30-0x8238DF68) stores the main render size and multisample mode:
//   li r9,2         (0x8238DF3C)  multisample mode: 2 = 2x MSAA, 1 = none
//   li r10,720  ->  stw r10,152(r31)          height
//   li r11,1120 (960 when a flag is clear) -> stw r11,148(r31)   width
//   stw r9,156(r31)                           multisample mode
// r31 == r3 (this) for the whole function. The Xenia patch rewrites the
// immediate at 0x8238DF3C; the addresses match this build exactly. Here we
// call the original and then overwrite the stored mode, which has the same
// effect (the field is written only here and r9 is dead afterwards).
//
// Without MSAA the 1120x720 colour (8 bytes per sample on host, 4 on guest)
// plus depth fits in one 10 MB EDRAM tile, so the game no longer replays the
// scene once per predicated tile, and the host main pass has half the samples.
//
// Toggle: cvar fable2_disable_msaa (default false = original 2x MSAA).
#include "fable_2_pch.h"
#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(fable2_disable_msaa, false, "GPU",
                    "Render Fable II without its 2x MSAA (Xenia 'Disable MSAA' patch)");

extern "C" void __imp__sub_8238DE58(PPCContext& __restrict ctx, uint8_t* base);

namespace {
constexpr uint32_t kMultisampleModeOffset = 156;
constexpr uint32_t kMultisampleNone = 1;
}  // namespace

extern "C" void sub_8238DE58(PPCContext& __restrict ctx, uint8_t* base) {
  const uint32_t self = ctx.r3.u32;
  __imp__sub_8238DE58(ctx, base);
  if (REXCVAR_GET(fable2_disable_msaa)) {
    REX_STORE_U32(self + kMultisampleModeOffset, kMultisampleNone);
  }
}
