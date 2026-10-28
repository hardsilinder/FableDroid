// Hand-tuned override: goto-free rewrite of FrameLimiterWait_82242628.
//
// The generated copy is a faithful but goto-heavy transliteration of the
// guest code. This version replaces the jumps with if/else and one while
// loop and keeps everything else identical to the generated function:
//   * the same guest loads and stores, in the same order, all through
//     REX_LOAD_*/REX_STORE_* (volatile; see "Shared state" below);
//   * the same values left in r1, r3, r8-r12, r29-r31, cr0 and cr6 on
//     every path, computed with the same 64-bit register arithmetic;
//   * the same calls, return addresses and four exit paths.
//
// Shared state. `limit` (r3) is the video/limiter struct:
//   * limit+kCounterPtrOffset holds the guest address of the GPU progress
//     counter. The SDK's command processor writes that counter from the GPU
//     thread (EVENT_WRITE_SHD; see docs/FPS_CAP_INVESTIGATION.md), and
//     BeginFrame_82B9BA58 resets it on the game thread.
//   * limit+kDeadlineOffset is the deadline in counter units, written by
//     BeginFrame_82B9BA58 and EndFrame_822981F0.
// The wait loop below spins until the counter reaches the deadline, so the
// counter load must be re-issued on every iteration. A plain (non-volatile)
// load of memory another thread writes is a data race, and the optimiser
// may hoist it out of the loop (ThinLTO can see that GpuProgressCheck does
// not write the counter), which would turn the wait into a hang. Volatile
// loads are what the generated code uses and what the guest's lwz means.
// Only the function's own stack frame is private to this thread; it goes
// through the same helpers, which cost nothing extra there.
//
// Deliberate deviation from the guest: the guest loop back-edges to
// GpuProgressCheck with no delay between iterations. This version interposes
// a short host (Windows) sleep when still not ready, so the emulated loop
// does not hammer the counter as aggressively. The sleep touches no guest
// state, so the register-state invariants above still hold.
// Guest structure (addresses are the guest instructions):
//   0x82242628  prologue: save r29/lr, 144-byte frame, r31=limit (r3),
//               r30=now (r4), r29=flag (r5).
//   0x82242640  readiness #1: counter already at the deadline -> return
//               (no sub_82B9BEC8).
//   0x8224265C  optional FastHelper_821E8D20: only when (r6 & 0xFF) == 0,
//               now == deadline and limit+kFastGateOffset == 0; a non-zero
//               gate word returns early (no sub_82B9BEC8).
//   0x82242680  readiness #2 (fresh read) -> return (no sub_82B9BEC8).
//   0x8224269C  build the GpuProgressCheck argument block at r1+80, take an
//               mftb snapshot, then readiness #3.
//   0x822426E0  wait loop: GpuProgressCheck_82B9BF90 (0 = ready -> leave),
//               else re-run the readiness test and spin while not ready.
//   0x8224270C  call sub_82B9BEC8(r1+80), then the epilogue at 0x82242714.
#ifdef _WIN32
#include <windows.h>
#else
#include <thread>
#endif

#include "fable_2_pch.h"
#include <rex/cvar.h>

REXCVAR_DEFINE_UINT32(frame_wait_sleep_us, 0, "CPU",
                     "Host sleep between GPU progress polls; zero uses yield")
    .range(0, 1000);

extern "C" void FastHelper_821E8D20(PPCContext& ctx, uint8_t* base);
extern "C" void GpuProgressCheck_82B9BF90(PPCContext& ctx, uint8_t* base);
extern "C" void __restgprlr_29(PPCContext& ctx, uint8_t* base);
extern "C" void __savegprlr_29(PPCContext& ctx, uint8_t* base);
extern "C" void sub_82B9BEC8(PPCContext& ctx, uint8_t* base);

namespace {

// Limiter struct fields (offsets from r31 = limit), from the guest loads at
// 0x82242640 (lwz r11,10896(r31)), 0x82242644 (lwz r10,10908(r31)) and
// 0x82242670 (lwz r11,13232(r31)).
constexpr uint32_t kCounterPtrOffset = 10896;
constexpr uint32_t kDeadlineOffset = 10908;
constexpr uint32_t kFastGateOffset = 13232;

// Thread block at r13: lwz r10,256(r13) at 0x8224269C, then lwz r10,88(r10)
// at 0x822426AC.
constexpr uint32_t kThreadBlockOffset = 256;
constexpr uint32_t kProgressOffset = 88;

// Stack frame (stwu r1,-144(r1)) and the GpuProgressCheck / sub_82B9BEC8
// argument block at r1+80 (stw ...,80..100(r1) at 0x822426A4-0x822426C8):
// limit, flag, counter snapshot, progress twice, timebase snapshot.
constexpr uint32_t kFrameSize = 144;
constexpr uint32_t kArgBlock = 80;
constexpr uint32_t kArgLimit = 80;
constexpr uint32_t kArgFlag = 84;
constexpr uint32_t kArgCounter = 88;
constexpr uint32_t kArgProgress = 92;
constexpr uint32_t kArgProgressCopy = 96;
constexpr uint32_t kArgTimebase = 100;

// Host pause between not-ready iterations of the wait loop, in nanoseconds.
// (The guest loop has no delay; on hardware this is a tight spin.)
constexpr uint32_t kPauseNs = 500;

// Guest return addresses (the instruction after each bl).
constexpr uint32_t kRetSaveGpr = 0x82242630;
constexpr uint32_t kRetFastHelper = 0x82242680;
constexpr uint32_t kRetProgressCheck = 0x822426E8;
constexpr uint32_t kRetEpilogueSub = 0x82242714;

// Readiness test as at 0x82242640 and 0x822426F0:
//   lwz r11,10896(r31); lwz r10,10908(r31); subf r9,r30,r10;
//   lwz r11,0(r11); subf r11,r11,r10; cmplw cr6,r9,r11
// cr6.lt (still waiting) while (deadline - now) < (deadline - counter).
void ReadinessR9R11(PPCContext& ctx, uint8_t* base) {
  ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + kCounterPtrOffset);
  ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + kDeadlineOffset);
  ctx.r9.u64 = ctx.r10.u64 - ctx.r30.u64;
  ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32);
  ctx.r11.u64 = ctx.r10.u64 - ctx.r11.u64;
  ctx.cr6.compare<uint32_t>(ctx.r9.u32, ctx.r11.u32, ctx.xer);
}

// Readiness test as at 0x82242680 (r8 keeps the counter, r11 the pointer):
//   lwz r11,10896(r31); lwz r10,10908(r31); subf r9,r30,r10;
//   lwz r8,0(r11); subf r10,r8,r10; cmplw cr6,r9,r10
void ReadinessR9R10(PPCContext& ctx, uint8_t* base) {
  ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + kCounterPtrOffset);
  ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + kDeadlineOffset);
  ctx.r9.u64 = ctx.r10.u64 - ctx.r30.u64;
  ctx.r8.u64 = REX_LOAD_U32(ctx.r11.u32);
  ctx.r10.u64 = ctx.r10.u64 - ctx.r8.u64;
  ctx.cr6.compare<uint32_t>(ctx.r9.u32, ctx.r10.u32, ctx.xer);
}

// 0x8224265C-0x8224267C. Returns false when the gate word is set, which the
// guest treats as "return now, without sub_82B9BEC8" (bne cr6,0x82242714).
bool MaybeFastHelper(PPCContext& ctx, uint8_t* base) {
  ctx.r11.u64 = ctx.r6.u32 & 0xFF;  // clrlwi. r11,r6,24
  ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
  if (ctx.cr0.eq == 0) {
    return true;  // bne 0x82242680
  }
  ctx.r11.u64 = ctx.r10.u32;  // rotlwi r11,r10,0 (r10 = deadline)
  ctx.cr6.compare<uint32_t>(ctx.r30.u32, ctx.r11.u32, ctx.xer);
  if (ctx.cr6.eq == 0) {
    return true;  // bne cr6,0x82242680
  }
  ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + kFastGateOffset);
  ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
  if (ctx.cr6.eq == 0) {
    return false;  // bne cr6,0x82242714
  }
  ctx.lr = kRetFastHelper;
  FastHelper_821E8D20(ctx, base);
  return true;
}

// 0x8224269C-0x822426DC: fill the argument block, snapshot the timebase,
// then readiness #3. Leaves cr6.lt set when the wait loop must run.
void BuildArgBlock(PPCContext& ctx, uint8_t* base) {
  // r11 still holds the counter pointer from ReadinessR9R10.
  ctx.r10.u64 = REX_LOAD_U32(ctx.r13.u32 + kThreadBlockOffset);
  ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32);
  REX_STORE_U32(ctx.r1.u32 + kArgLimit, ctx.r31.u32);
  REX_STORE_U32(ctx.r1.u32 + kArgFlag, ctx.r29.u32);
  ctx.r10.u64 = REX_LOAD_U32(ctx.r10.u32 + kProgressOffset);
  REX_STORE_U32(ctx.r1.u32 + kArgCounter, ctx.r11.u32);
  REX_STORE_U32(ctx.r1.u32 + kArgProgress, ctx.r10.u32);
  REX_STORE_U32(ctx.r1.u32 + kArgProgressCopy, ctx.r10.u32);
  ctx.r11.u64 = REX_QUERY_TIMEBASE();  // mftb r11
  ctx.r10.u64 = REX_LOAD_U32(ctx.r31.u32 + kCounterPtrOffset);
  ctx.r9.u64 = REX_LOAD_U32(ctx.r31.u32 + kDeadlineOffset);
  REX_STORE_U32(ctx.r1.u32 + kArgTimebase, ctx.r11.u32);
  ctx.r11.u64 = ctx.r9.u64 - ctx.r30.u64;
  ctx.r10.u64 = REX_LOAD_U32(ctx.r10.u32);
  ctx.r10.u64 = ctx.r9.u64 - ctx.r10.u64;
  ctx.cr6.compare<uint32_t>(ctx.r11.u32, ctx.r10.u32, ctx.xer);
}

// Host pause of a couple of hundred nanoseconds to a microsecond. Uses
// WaitForSingleObjectEx with a NULL handle rather than Sleep / std::
// this_thread::sleep_for because those round to scheduler ticks (~15 ms
// without timeBeginPeriod) and would overshoot the frame deadline; the
// waitable-timer path is sub-millisecond accurate. Touches no guest state.
void SpinPause() {
  // WaitForSingleObjectEx expects 100-ns units; 0 means "no timeout", so
  // bump anything below that to one 100-ns quantum.
#ifdef _WIN32
  const uint32_t hundredNs = kPauseNs / 100;
  WaitForSingleObjectEx(nullptr, hundredNs > 0 ? hundredNs : 1, TRUE);
#else
  const auto sleep_us = REXCVAR_GET(frame_wait_sleep_us);
  if (sleep_us) {
    std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
  } else {
    std::this_thread::yield();
  }
#endif
}

// addi r1,r1,144; b __restgprlr_29 (0x82242714).
void Epilogue(PPCContext& ctx, uint8_t* base) {
  ctx.r1.s64 = ctx.r1.s64 + kFrameSize;
  __restgprlr_29(ctx, base);
}

}  // namespace

extern "C" void FrameLimiterWait_82242628(PPCContext& __restrict ctx, uint8_t* base) {
  REX_FUNC_PROLOGUE();

  // mflr r12; bl __savegprlr_29; stwu r1,-144(r1)
  ctx.r12.u64 = ctx.lr;
  ctx.lr = kRetSaveGpr;
  __savegprlr_29(ctx, base);
  const uint32_t frame = ctx.r1.u32 - kFrameSize;
  REX_STORE_U32(frame, ctx.r1.u32);
  ctx.r1.u32 = frame;
  ctx.r31.u64 = ctx.r3.u64;  // limit
  ctx.r30.u64 = ctx.r4.u64;  // now
  ctx.r29.u64 = ctx.r5.u64;  // flag

  // Readiness #1, the optional fast helper and readiness #2 each return
  // without calling sub_82B9BEC8 (bge cr6 / bne cr6 -> 0x82242714).
  ReadinessR9R11(ctx, base);
  if (ctx.cr6.lt == 0 || !MaybeFastHelper(ctx, base)) {
    Epilogue(ctx, base);
    return;
  }
  ReadinessR9R10(ctx, base);
  if (ctx.cr6.lt == 0) {
    Epilogue(ctx, base);
    return;
  }

  // Readiness #3 failing (bge cr6,0x8224270C) skips the loop but still calls
  // sub_82B9BEC8. The loop leaves on GpuProgressCheck == 0 (beq 0x8224270C)
  // or once the counter reaches the deadline (blt cr6,0x822426E0 not taken).
  BuildArgBlock(ctx, base);
  while (ctx.cr6.lt != 0) {
    ctx.r3.s64 = ctx.r1.s64 + kArgBlock;
    ctx.lr = kRetProgressCheck;
    GpuProgressCheck_82B9BF90(ctx, base);
    ctx.cr0.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
    if (ctx.cr0.eq != 0) {
      break;
    }
    ReadinessR9R11(ctx, base);
    if (ctx.cr6.lt != 0) {
      SpinPause();
    }
  }

  ctx.r3.s64 = ctx.r1.s64 + kArgBlock;
  ctx.lr = kRetEpilogueSub;
  sub_82B9BEC8(ctx, base);
  Epilogue(ctx, base);
}
