// Hand-tuned override of recompiled ProcessGameFrame_82276C30
// (recompiler output: fable_2_recomp.161.cpp; generated transforms nv=1 mfmsr=1 glock=1 gt=1).
// The "// Hand-tuned override" first line makes tools/make_hotfunc_overrides.py
// skip this file; re-run that script to regenerate the literal reference copy.
//
// Hand-tuned for readability only. The generated copy expressed the function with
// 27 gotos and 22 labels (mirroring the PowerPC branch structure). This version
// restructures the SAME statements into idiomatic C++ control flow:
//   * the frame loop (loc_82276D98 .. back-edge) is a single do/while;
//   * the four mfmsr/mtmsrd/lwarx/stwcx global-counter retry loops are do/while;
//   * forward branch-skips are if/else blocks;
//   * the two break-outs to the epilogue (loc_8227722C) are `break;`;
//   * the two "merge to a shared target" patterns (the active-player-method call
//     and the post-frame flag store) use a local bool (call_active / do_stb)
//     instead of duplicating the shared statement.
// Every guest statement is kept byte-for-byte in the same order (verified against
// the generated copy); only control-flow structure changed, so this is
// behavior-identical: same guest RAM reads/writes (addresses + order), same call
// targets and arguments, same guest return addresses (ctx.lr), and the same
// r1/r3-r31/ctr/cr*/xer/msr side effects at every call site and on return.
//
// Register caveat (as in the other hand-tuned overrides): the volatile
// caller-saved registers (r0, r2, r3-r12, f0-f31) may transiently differ from
// the generated copy mid-function, but the callee-saved registers (r1, r13-r31),
// the condition/interrupt state, every call argument (r3/r4) and the guest return
// address (ctx.lr) are preserved exactly at each call site and on return. The
// generated copy keeps each statement visible so this file stays a 1:1
// comparison against the recompiler output.
//
// The recompiler emits recompiled guest functions as WEAK extern "C" aliases
// of __imp__ProcessGameFrame_82276C30; fable_2_register.cpp registers the ALIAS in the
// indirect-dispatch table. This strong definition intercepts both direct bl
// calls and bctrl dispatch. __imp__ProcessGameFrame_82276C30 remains the original.
//
// Guest logic (structural overview; the exact per-line disassembly references are
// kept inline below). `self` = the frame-context object passed in r3 (held in r31
// for the body). This is the per-frame simulation entry point:
//
//   void ProcessGameFrame(Frame* self)
//   {
//     if (sub_8235D728(self))               // subsystem readiness probe
//       sub_8236CC28(self);                 //   one-shot frame setup
//
//     // -- frame timing: current time / interval / clamped fps from the global
//     //    time & rate table (doubles @ 0x833195xx, 0x8209FFD8, 0x83496EC0) --
//     self->fps = clamp(fps_from_interval); // stfd f11,104(r1)
//     u64 t = timebase;                     // inlined GetTimebase_8221EB58
//
//     if (self->workPending) {              // cmplwi cr6,r3,0  (0 => skip update)
//       // one-time-per-frame setup: budget = 16, budget-counter @ 0x83496EB8,
//       // and the frame-rate/advance pointer bases (r19/r24/r16/r15).
//
//       do {                                // per-frame work loop
//         if (self->b256 && self->b257) {   // per-frame subsystem gate
//           LazyInitSubsystemA(self);
//           RtlEnterCriticalSection(...);
//           RtlLeaveCriticalSection(...);
//           if (self->state52 == 0) break;  // nothing queued -> epilogue
//         }
//         t = timebase;
//         PollAndProcessOsNotifications();  // drain OS messages
//         DispatchOsNotifications();
//         if (self->osFlag) {               // new/advanced entity
//           t = timebase;
//           if (entity_elapsed > threshold) {
//             Construct(); InitializeGameEntity();
//           }
//         }
//         if (advanceOk) { t = timebase; /* advance-timing math */ }
//
//         // grow the active-entity bit-array, then drain the per-frame budget
//         // (one CAS decrement of the shared counter @ 0x83496EB8, in a retry
//         // loop; the two arms differ only in which refcounted slot is freed)
//         if (slotA) { ProcessGrowBitArray(); dec_budget(); }
//         else       { ProcessGrowBitArray(); dec_budget(); }
//
//         // active-player frame update
//         bool active = self->activePlayer;
//         if (!active) active = QueryActivePlayerMethod() != 0;
//         if (active) CallActivePlayerMethod();
//
//         // misc subsystems + frame-callback registration
//         sub_822C1FB8(); LazyInitSubsystemA(); sub_82198860();
//         LazyInitSubsystemB(); sub_822A9B20();
//         if (regEnabled) {
//           ResolveSubsystemReference();
//           if (cbEnabled) {
//             ResolveSubsystemReference();
//             RegisterFrameCallback();
//           }
//         }
//         ProcessGameStateUpdate();
//         // per-frame flag update (possibly via sub_8243A2C8)
//       } while (self->moreWork);
//     }
//
//     // drain / yield: either run the threshold check path, or yield
//     if (thresholdMet) { /* indirect call via vtable */ } else YieldAndCheckThreshold();
//   }
#include "fable_2_pch.h"
#include <rex/cvar.h>
#include <chrono>
#include <thread>

REXCVAR_DEFINE_UINT32(game_idle_sleep_us, 0, "CPU",
                     "Host sleep on the game frame loop's no-work yield path")
    .range(0, 1000);

extern "C" void CallActivePlayerMethod_821A11E8(PPCContext& ctx, uint8_t* base);
extern "C" void ConstructRefCounted_8222CF18(PPCContext& ctx, uint8_t* base);
extern "C" void DispatchFrameCallback_821A6010(PPCContext& ctx, uint8_t* base);
extern "C" void DispatchOsNotifications_82185080(PPCContext& ctx, uint8_t* base);
extern "C" void InitializeGameEntity_8233CAC8(PPCContext& ctx, uint8_t* base);
extern "C" void LazyInitSubsystemA_8217FD08(PPCContext& ctx, uint8_t* base);
extern "C" void LazyInitSubsystemB_822C6C60(PPCContext& ctx, uint8_t* base);
extern "C" void PollAndProcessOsNotifications_8229A9D8(PPCContext& ctx, uint8_t* base);
extern "C" void ProcessGameStateUpdate_821C9008(PPCContext& ctx, uint8_t* base);
extern "C" void ProcessGrowBitArray_821EC668(PPCContext& ctx, uint8_t* base);
extern "C" void QueryActivePlayerMethod_821EC8A0(PPCContext& ctx, uint8_t* base);
extern "C" void RegisterFrameCallback_821DCF10(PPCContext& ctx, uint8_t* base);
extern "C" void Release_RefCounted_821C67D8(PPCContext& ctx, uint8_t* base);
extern "C" void ResolveSubsystemReference_821F8760(PPCContext& ctx, uint8_t* base);
extern "C" void Yield_82CBD098(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__RtlEnterCriticalSection(PPCContext& ctx, uint8_t* base);
extern "C" void __imp__RtlLeaveCriticalSection(PPCContext& ctx, uint8_t* base);
extern "C" void __restfpr_21(PPCContext& ctx, uint8_t* base);
extern "C" void __restgprlr_14(PPCContext& ctx, uint8_t* base);
extern "C" void __savefpr_21(PPCContext& ctx, uint8_t* base);
extern "C" void __savegprlr_14(PPCContext& ctx, uint8_t* base);
extern "C" void sub_82198860(PPCContext& ctx, uint8_t* base);
extern "C" void sub_821EE858(PPCContext& ctx, uint8_t* base);
extern "C" void sub_82278C90(PPCContext& ctx, uint8_t* base);
extern "C" void sub_822A9B20(PPCContext& ctx, uint8_t* base);
extern "C" void sub_822C1FB8(PPCContext& ctx, uint8_t* base);
extern "C" void sub_822C93C0(PPCContext& ctx, uint8_t* base);
extern "C" void sub_8235D728(PPCContext& ctx, uint8_t* base);
extern "C" void sub_8236CC28(PPCContext& ctx, uint8_t* base);
extern "C" void sub_8243A2C8(PPCContext& ctx, uint8_t* base);

// Non-volatile equivalents of the pch's REX_LOAD_*/REX_STORE_* macros
// (identical address math, bswap, and byte order; no volatile).
#define GV8(x)   (*(const uint8_t*)(REX_RAW_ADDR(x)))
#define GV16(x)  __builtin_bswap16(*(const uint16_t*)(REX_RAW_ADDR(x)))
#define GV32(x)  __builtin_bswap32(*(const uint32_t*)(REX_RAW_ADDR(x)))
#define GV64(x)  __builtin_bswap64(*(const uint64_t*)(REX_RAW_ADDR(x)))
#define SV8(x, y)   (*(uint8_t*)(REX_RAW_ADDR(x)) = (y))
#define SV16(x, y)  (*(uint16_t*)(REX_RAW_ADDR(x)) = __builtin_bswap16(y))
#define SV32(x, y)  (*(uint32_t*)(REX_RAW_ADDR(x)) = __builtin_bswap32(y))
#define SV64(x, y)  (*(uint64_t*)(REX_RAW_ADDR(x)) = __builtin_bswap64(y))
// Cached reference to the SDK's global critical region mutex: same mutex and
// same lock/unlock order as REX_ENTER/LEAVE_GLOBAL_LOCK, without the
// per-use DLL call to global_critical_region::mutex().
static inline std::recursive_mutex& hotfunc_gcr_mutex() {
  static thread_local std::recursive_mutex& cached =
      rex::thread::global_critical_region::mutex();
  return cached;
}

// --- Guest global addresses ---
// The frame loop reads/writes a fixed set of globals. The recompiler builds
// each as `lis rN,hi16; addi rM,rN,lo16`; the constants below are the
// resulting 32-bit guest addresses. (The 0x...0000 "hi16" values in the body
// are the scratch-register address bases, kept in registers as the recompiler
// emits them; only the fully-formed addresses here are semantically meaningful.)
constexpr u32 kBudgetCounterAddr = 0x83496EB8;  // per-frame work budget (decremented once per processed item; 0 => yield)
constexpr u32 kFrameTimeDouble   = 0x82100CE0;  // current frame time (double), read for the timing math
constexpr u32 kBitArrayHolder    = 0x8349E6EC;  // active-entity bit-array holder

// --- Fast correctly-rounded FMA ---
// The PPC `fmadd` compiles to std::fma, which on this x86 target (SSE4.1, no FMA
// instruction) tail-calls the libm fma() -- a ~100-300 cycle function call. That
// call sits on the hot per-iteration timing path, so the multiply-add goes through
// a target("fma") helper instead: the SAME single-rounding IEEE FMA becomes one
// vfmadd*sd instruction (~5 cycles), an identical result far cheaper than libm.
// The target attribute keeps it a tiny out-of-line call (the compiler will not
// inline FMA code into the non-FMA callers), still a large win. Needs an
// FMA3-capable x86-64 host (any modern CPU).
static inline double guest_fma(double a, double b, double c)
    __attribute__((target("fma")));
static inline double guest_fma(double a, double b, double c) {
	return __builtin_fma(a, b, c);
}


// ===== Shared helpers + extracted per-frame regions (behavior-identical) =====
// ProcessGameFrame_82276C30 is split into a readable skeleton. Every guest
// statement is preserved verbatim and in order; `ctx` carries all guest register
// and memory state, so each region reads/writes the same ctx members it did
// inline. `temp`/`ea` are the per-region guest temp register + address scratch.

// Atomically decrement the 32-bit guest counter at *addr (retrying lwarx/stwcx
// CAS) via the PPC global-lock sequence mfmsr/mtmsrd/lock/lwarx/addi/stwcx/
// unlock. `val` receives the decremented value; `msr_save` holds the saved MSR
// (ME/RI bits); cr0.eq is left set on success.
static __attribute__((noinline)) void decrement_global_counter(PPCContext& ctx, uint8_t* base,
                                            PPCRegister* addr, PPCRegister* val,
                                            PPCRegister* msr_save) {
	uint32_t ea{};
	do {
		msr_save->u64 = ppc_global_lock_count_().load(std::memory_order_acquire) ? 0 : 0x8000;
		std::atomic_thread_fence(std::memory_order_seq_cst);
		ctx.msr = (ctx.r13.u32 & 0x8020) | (ctx.msr & ~0x8020);
		hotfunc_gcr_mutex().lock(); ppc_global_lock_count_().fetch_add(1);
		ea = addr->u32;
		ctx.reserved.u32 = *(uint32_t*)REX_RAW_ADDR(ea);
		val->u64 = __builtin_bswap32(ctx.reserved.u32);
		val->s64 = val->s64 + -1;
		ea = addr->u32;
		ctx.cr0.lt = 0;
		ctx.cr0.gt = 0;
		ctx.cr0.eq = __sync_bool_compare_and_swap(reinterpret_cast<uint32_t*>(REX_RAW_ADDR(ea)), ctx.reserved.s32, __builtin_bswap32(val->s32));
		ctx.cr0.so = ctx.xer.so;
		std::atomic_thread_fence(std::memory_order_seq_cst);
		ctx.msr = (msr_save->u32 & 0x8020) | (ctx.msr & ~0x8020);
		{ auto old_ = ppc_global_lock_count_().fetch_sub(1); assert(old_ >= 1); }
		hotfunc_gcr_mutex().unlock();
	} while (!ctx.cr0.eq);
}

// Inlined GetTimebase_8221EB58: r11 = mftb (re-read if 0), stored to the guest
// address in r3; r3 left = 1. Caller sets ctx.lr to the guest return address
// first (matching the original bl).
static __attribute__((noinline)) void query_guest_timebase(PPCContext& ctx, uint8_t* base) {
	ctx.r11.u64 = REX_QUERY_TIMEBASE();
	ctx.r10.u64 = __builtin_rotateleft32(ctx.r11.u32, 0);
	ctx.cr0.compare<int32_t>(ctx.r10.s32, 0, ctx.xer);
	if (ctx.cr0.eq) {
		ctx.r11.u64 = REX_QUERY_TIMEBASE();
	}
	SV64(ctx.r3.u32 + 0, ctx.r11.u64);
	ctx.r3.s64 = 1;
}

static __attribute__((noinline)) void probe_and_setup(PPCContext& ctx, uint8_t* base) {
	// lis r11,-31927
	ctx.r11.s64 = (int64_t)(int32_t)0x83490000;
	// ===== self := r3 (frame context); readiness probe + one-shot setup =====
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// lwz r11,26912(r11)
	ctx.r11.u64 = GV32(ctx.r11.u32 + 26912);
	// lwz r10,8(r11)
	ctx.r10.u64 = GV32(ctx.r11.u32 + 8);
	// lwz r3,36(r10)
	ctx.r3.u64 = GV32(ctx.r10.u32 + 36);
	// bl 0x8235d728
	ctx.lr = 0x82276C5C;
	sub_8235D728(ctx, base);
	// clrlwi r9,r3,24
	ctx.r9.u64 = ctx.r3.u32 & 0xFF;
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r9.u32, 0, ctx.xer);
	// bne cr6,0x82276c88  (block runs when cr6.eq)
	if (ctx.cr6.eq) {
	// lwz r11,108(r31)
	ctx.r11.u64 = GV32(ctx.r31.u32 + 108);
	// lfd f1,272(r31)
	ctx.fpscr.disableFlushMode();
	ctx.f1.u64 = GV64(ctx.r31.u32 + 272);
	// lwz r3,104(r31)
	ctx.r3.u64 = GV32(ctx.r31.u32 + 104);
	// lwz r10,4(r11)
	ctx.r10.u64 = GV32(ctx.r11.u32 + 4);
	// mr r9,r10
	ctx.r9.u64 = ctx.r10.u64;
	// lwz r6,8(r10)
	ctx.r6.u64 = GV32(ctx.r10.u32 + 8);
	// lwz r5,0(r9)
	ctx.r5.u64 = GV32(ctx.r9.u32 + 0);
	// bl 0x8236cc28
	ctx.lr = 0x82276C88;
	sub_8236CC28(ctx, base);
	}
}

static __attribute__((noinline)) void compute_frame_timing(PPCContext& ctx, uint8_t* base) {
	// ===== Frame timing: interval / clamped fps from the global time table =====
	// lis r11,-32246
	ctx.r11.s64 = (int64_t)(int32_t)0x820A0000;
	// lis r10,-31950
	ctx.r10.s64 = (int64_t)(int32_t)0x83320000;
	// addi r30,r11,-27456
	ctx.r30.s64 = ctx.r11.s64 + -27456;
	// lis r9,-31950
	ctx.r9.s64 = (int64_t)(int32_t)0x83320000;
	// addi r3,r1,104
	ctx.r3.s64 = ctx.r1.s64 + 104;
	// lfd f13,-27368(r10)
	ctx.fpscr.disableFlushMode();
	ctx.f13.u64 = GV64(ctx.r10.u32 + -27368);
	// lfd f0,27416(r30)
	ctx.f0.u64 = GV64(ctx.r30.u32 + 27416);
	// fdiv f27,f0,f13
	ctx.f27.f64 = ctx.f0.f64 / ctx.f13.f64;
	// lfd f13,-27376(r9)
	ctx.f13.u64 = GV64(ctx.r9.u32 + -27376);
	// fdiv f13,f0,f13
	ctx.f13.f64 = ctx.f0.f64 / ctx.f13.f64;
	// fdiv f25,f0,f27
	ctx.f25.f64 = ctx.f0.f64 / ctx.f27.f64;
	// fmul f12,f25,f13
	ctx.f12.f64 = ctx.f25.f64 * ctx.f13.f64;
	// fctiwz f11,f12
	ctx.f11.s64 = std::isnan(ctx.f12.f64) ? int64_t(0x80000000U) : (ctx.f12.f64 >= double(INT_MAX)) ? INT_MAX : simde_mm_cvttsd_si32(simde_mm_load_sd(&ctx.f12.f64));
	// stfd f11,104(r1)
	SV64(ctx.r1.u32 + 104, ctx.f11.u64);
	// lwz r10,108(r1)
	ctx.r10.u64 = GV32(ctx.r1.u32 + 108);
	// addi r11,r10,-1
	ctx.r11.s64 = ctx.r10.s64 + -1;
	// srawi r8,r11,31
	ctx.xer.ca = (ctx.r11.s32 < 0) & ((ctx.r11.u32 & 0x7FFFFFFF) != 0);
	ctx.r8.s64 = ctx.r11.s32 >> 31;
	// and r7,r8,r11
	ctx.r7.u64 = ctx.r8.u64 & ctx.r11.u64;
	// subf r25,r7,r10
	ctx.r25.u64 = ctx.r10.u64 - ctx.r7.u64;
	// bl 0x8221eb58
	ctx.lr = 0x82276CD8;
	query_guest_timebase(ctx, base);  // inlined GetTimebase_8221EB58: r11=timebase (retry if 0); [r3]=r11; r3=1
	// lfd f10,104(r1)
	ctx.fpscr.disableFlushMode();
	ctx.f10.u64 = GV64(ctx.r1.u32 + 104);
	// lis r27,-31927
	ctx.r27.s64 = (int64_t)(int32_t)0x83490000;
	// fcfid f9,f10
	ctx.f9.f64 = double(ctx.f10.s64);
	// lis r26,-31927
	ctx.r26.s64 = (int64_t)(int32_t)0x83490000;
	// addi r3,r1,112
	ctx.r3.s64 = ctx.r1.s64 + 112;
	// lfd f0,28352(r27)
	ctx.f0.u64 = GV64(ctx.r27.u32 + 28352);
	// lfd f13,28360(r26)
	ctx.f13.u64 = GV64(ctx.r26.u32 + 28360);
	// fsub f8,f9,f0
	ctx.f8.f64 = ctx.f9.f64 - ctx.f0.f64;
	// fdiv f23,f8,f13
	ctx.f23.f64 = ctx.f8.f64 / ctx.f13.f64;
	// bl 0x8221eb58
	ctx.lr = 0x82276D00;
	query_guest_timebase(ctx, base);  // inlined GetTimebase_8221EB58: r11=timebase (retry if 0); [r3]=r11; r3=1
	// lfd f7,112(r1)
	ctx.fpscr.disableFlushMode();
	ctx.f7.u64 = GV64(ctx.r1.u32 + 112);
	// lis r22,-31926
	ctx.r22.s64 = (int64_t)(int32_t)0x834A0000;
	// fcfid f6,f7
	ctx.f6.f64 = double(ctx.f7.s64);
	// lfd f0,28352(r27)
	ctx.f0.u64 = GV64(ctx.r27.u32 + 28352);
	// lis r6,-32256
	ctx.r6.s64 = (int64_t)(int32_t)0x82000000;
	// lfd f13,28360(r26)
	ctx.f13.u64 = GV64(ctx.r26.u32 + 28360);
	// li r20,0
	ctx.r20.s64 = 0;
	// lwz r3,-6420(r22): active-entity bit-array holder (0x8349E6EC)
	ctx.r3.u64 = GV32(kBitArrayHolder);
	// mr r23,r20
	ctx.r23.u64 = ctx.r20.u64;
	// stw r20,96(r1)
	SV32(ctx.r1.u32 + 96, ctx.r20.u32);
	// mr r28,r20
	ctx.r28.u64 = ctx.r20.u64;
	// lfd f24,3376(r6)
	ctx.f24.u64 = GV64(ctx.r6.u32 + 3376);
	// fmr f29,f24
	ctx.f29.f64 = ctx.f24.f64;
	// lwz r5,0(r3)
	ctx.r5.u64 = GV32(ctx.r3.u32 + 0);
	// fsub f5,f6,f0
	ctx.f5.f64 = ctx.f6.f64 - ctx.f0.f64;
	// lwz r4,16(r5)
	ctx.r4.u64 = GV32(ctx.r5.u32 + 16);
	// fdiv f26,f5,f13
	ctx.f26.f64 = ctx.f5.f64 / ctx.f13.f64;
	// mtctr r4
	ctx.ctr.u64 = ctx.r4.u64;
	// bctrl 
	ctx.lr = 0x82276D4C;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
	// clrlwi r3,r3,24
	ctx.r3.u64 = ctx.r3.u32 & 0xFF;
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
}

static __attribute__((noinline)) void setup_frame(PPCContext& ctx, uint8_t* base) {
	PPCRegister temp{};
	// lis r7,-32240
	ctx.r7.s64 = (int64_t)(int32_t)0x82100000;
	// lfs f21,0(r30)
	ctx.fpscr.disableFlushMode();
	temp.u32 = GV32(ctx.r30.u32 + 0);
	ctx.f21.f64 = double(temp.f32);
	// lis r8,-32256
	ctx.r8.s64 = (int64_t)(int32_t)0x82000000;
	// lis r11,-31927
	ctx.r11.s64 = (int64_t)(int32_t)0x83490000;
	// lis r10,-32256
	ctx.r10.s64 = (int64_t)(int32_t)0x82000000;
	// lis r9,-32256
	ctx.r9.s64 = (int64_t)(int32_t)0x82000000;
	// addi r6,r8,3224
	ctx.r6.s64 = ctx.r8.s64 + 3224;
	// lfd f22,3296(r7): current frame time (double @ 0x82100CE0)
	ctx.f22.u64 = GV64(kFrameTimeDouble);
	// ===== Per-frame setup: budget = 16, budget-counter ptr, rate/advance bases =====
	// li r19,16
	ctx.r19.s64 = 16;
	// lis r17,-31927
	ctx.r17.s64 = (int64_t)(int32_t)0x83490000;
	// stw r6,112(r1)
	SV32(ctx.r1.u32 + 112, ctx.r6.u32);
	// lis r18,-31927
	ctx.r18.s64 = (int64_t)(int32_t)0x83490000;
	// lis r21,-31927
	ctx.r21.s64 = (int64_t)(int32_t)0x83490000;
	// addi r24,r11,28344: r24 = per-frame work-budget counter (0x83496EB8)
	ctx.r24.s64 = (int64_t)(int32_t)kBudgetCounterAddr;
	// addi r16,r10,4220
	ctx.r16.s64 = ctx.r10.s64 + 4220;
	// addi r15,r9,4168
	ctx.r15.s64 = ctx.r9.s64 + 4168;
}

static __attribute__((noinline)) bool frame_gate(PPCContext& ctx, uint8_t* base) {
	// lbz r11,256(r31)
	ctx.r11.u64 = GV8(ctx.r31.u32 + 256);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq cr6,0x82276dd8  (block runs when !cr6.eq)
	if (!ctx.cr6.eq) {
	// lbz r11,257(r31)
	ctx.r11.u64 = GV8(ctx.r31.u32 + 257);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq cr6,0x82276dd8  (critical-section block runs when !cr6.eq)
	if (!ctx.cr6.eq) {
	// bl 0x8217fd08
	ctx.lr = 0x82276DB4;
	LazyInitSubsystemA_8217FD08(ctx, base);
	// mr r29,r3
	ctx.r29.u64 = ctx.r3.u64;
	// addi r30,r29,68
	ctx.r30.s64 = ctx.r29.s64 + 68;
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// bl 0x832b227c
	ctx.lr = 0x82276DC4;
	__imp__RtlEnterCriticalSection(ctx, base);
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// lwz r30,52(r29)
	ctx.r30.u64 = GV32(ctx.r29.u32 + 52);
	// bl 0x832b226c
	ctx.lr = 0x82276DD0;
	__imp__RtlLeaveCriticalSection(ctx, base);
	// cmpwi cr6,r30,0
	ctx.cr6.compare<int32_t>(ctx.r30.s32, 0, ctx.xer);
	// beq cr6,0x8227722c  (break out of the frame loop)
	if (ctx.cr6.eq) {
		return false;  // break out of the frame loop
	}
	}
	}
	return true;
}

static __attribute__((noinline)) void compute_loop_timing(PPCContext& ctx, uint8_t* base) {
	// addi r3,r1,120
	ctx.r3.s64 = ctx.r1.s64 + 120;
	// bl 0x8221eb58
	ctx.lr = 0x82276DE0;
	query_guest_timebase(ctx, base);  // inlined GetTimebase_8221EB58: r11=timebase (retry if 0); [r3]=r11; r3=1
	// lfd f10,120(r1)
	ctx.fpscr.disableFlushMode();
	ctx.f10.u64 = GV64(ctx.r1.u32 + 120);
	// fcfid f9,f10
	ctx.f9.f64 = double(ctx.f10.s64);
	// lfd f0,28352(r27)
	ctx.f0.u64 = GV64(ctx.r27.u32 + 28352);
	// lfd f13,28360(r26)
	ctx.f13.u64 = GV64(ctx.r26.u32 + 28360);
	// extsw r11,r23
	ctx.r11.s64 = ctx.r23.s32;
	// std r11,192(r1)
	SV64(ctx.r1.u32 + 192, ctx.r11.u64);
	// lfd f12,192(r1)
	ctx.f12.u64 = GV64(ctx.r1.u32 + 192);
	// fcfid f11,f12
	ctx.f11.f64 = double(ctx.f12.s64);
	// fsub f8,f9,f0
	ctx.f8.f64 = ctx.f9.f64 - ctx.f0.f64;
	// fdiv f31,f8,f13
	ctx.f31.f64 = ctx.f8.f64 / ctx.f13.f64;
	// fsub f7,f31,f26
	ctx.f7.f64 = ctx.f31.f64 - ctx.f26.f64;
	// fmadd f6,f7,f25,f11
	ctx.f6.f64 = guest_fma(ctx.f7.f64, ctx.f25.f64, ctx.f11.f64);
	// fctiwz f5,f6
	ctx.f5.s64 = std::isnan(ctx.f6.f64) ? int64_t(0x80000000U) : (ctx.f6.f64 >= double(INT_MAX)) ? INT_MAX : simde_mm_cvttsd_si32(simde_mm_load_sd(&ctx.f6.f64));
	// stfd f5,144(r1)
	SV64(ctx.r1.u32 + 144, ctx.f5.u64);
	// lwz r29,148(r1)
	ctx.r29.u64 = GV32(ctx.r1.u32 + 148);
	// subf r10,r23,r29
	ctx.r10.u64 = ctx.r29.u64 - ctx.r23.u64;
	// extsw r9,r10
	ctx.r9.s64 = ctx.r10.s32;
	// subf. r30,r28,r29
	ctx.r30.u64 = ctx.r29.u64 - ctx.r28.u64;
	ctx.cr0.compare<int32_t>(ctx.r30.s32, 0, ctx.xer);
	// std r9,176(r1)
	SV64(ctx.r1.u32 + 176, ctx.r9.u64);
	// lfd f4,176(r1)
	ctx.f4.u64 = GV64(ctx.r1.u32 + 176);
	// fcfid f3,f4
	ctx.f3.f64 = double(ctx.f4.s64);
	// fmadd f2,f3,f27,f26
	ctx.f2.f64 = guest_fma(ctx.f3.f64, ctx.f27.f64, ctx.f26.f64);
	// stfd f2,272(r31)
	SV64(ctx.r31.u32 + 272, ctx.f2.u64);
}

static __attribute__((noinline)) void process_frame_work(PPCContext& ctx, uint8_t* base) {
	PPCRegister temp{};
	uint32_t ea{};
	// li r4,0
	ctx.r4.s64 = 0;
	// lwz r3,80(r31)
	ctx.r3.u64 = GV32(ctx.r31.u32 + 80);
	// --- OS notifications: poll + dispatch ---
	// bl 0x8229a9d8
	ctx.lr = 0x82276E4C;
	PollAndProcessOsNotifications_8229A9D8(ctx, base);
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x82185080
	ctx.lr = 0x82276E54;
	DispatchOsNotifications_82185080(ctx, base);
	// lis r11,-31927
	ctx.r11.s64 = (int64_t)(int32_t)0x83490000;
	// lbz r11,26940(r11)
	ctx.r11.u64 = GV8(ctx.r11.u32 + 26940);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq cr6,0x82276eac  (inner block runs when !cr6.eq)
	if (!ctx.cr6.eq) {
	// addi r3,r1,160
	ctx.r3.s64 = ctx.r1.s64 + 160;
	// bl 0x8221eb58
	ctx.lr = 0x82276E6C;
	query_guest_timebase(ctx, base);  // inlined GetTimebase_8221EB58: r11=timebase (retry if 0); [r3]=r11; r3=1
	// lfd f12,160(r1)
	ctx.fpscr.disableFlushMode();
	ctx.f12.u64 = GV64(ctx.r1.u32 + 160);
	// fcfid f11,f12
	ctx.f11.f64 = double(ctx.f12.s64);
	// lfd f0,28352(r27)
	ctx.f0.u64 = GV64(ctx.r27.u32 + 28352);
	// lfd f13,28360(r26)
	ctx.f13.u64 = GV64(ctx.r26.u32 + 28360);
	// fsub f10,f11,f0
	ctx.f10.f64 = ctx.f11.f64 - ctx.f0.f64;
	// fdiv f9,f10,f13
	ctx.f9.f64 = ctx.f10.f64 / ctx.f13.f64;
	// fsub f8,f9,f23
	ctx.f8.f64 = ctx.f9.f64 - ctx.f23.f64;
	// fcmpu cr6,f8,f22
	ctx.cr6.compare(ctx.f8.f64, ctx.f22.f64);
	// ble cr6,0x82276eac  (entity-init runs when cr6.gt)
	if (ctx.cr6.gt) {
	// li r5,-1
	ctx.r5.s64 = -1;
	// lwz r4,112(r1)
	ctx.r4.u64 = GV32(ctx.r1.u32 + 112);
	// addi r3,r1,104
	ctx.r3.s64 = ctx.r1.s64 + 104;
	// bl 0x8222cf18
	ctx.lr = 0x82276EA0;
	ConstructRefCounted_8222CF18(ctx, base);
	// mr r4,r3
	ctx.r4.u64 = ctx.r3.u64;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x8233cac8
	ctx.lr = 0x82276EAC;
	InitializeGameEntity_8233CAC8(ctx, base);
	}
	}
	// addi r3,r1,128
	ctx.r3.s64 = ctx.r1.s64 + 128;
	// bl 0x8221eb58
	ctx.lr = 0x82276EB4;
	query_guest_timebase(ctx, base);  // inlined GetTimebase_8221EB58: r11=timebase (retry if 0); [r3]=r11; r3=1
	// lfd f12,128(r1)
	ctx.fpscr.disableFlushMode();
	ctx.f12.u64 = GV64(ctx.r1.u32 + 128);
	// fcfid f11,f12
	ctx.f11.f64 = double(ctx.f12.s64);
	// lfd f0,28352(r27)
	ctx.f0.u64 = GV64(ctx.r27.u32 + 28352);
	// lfd f13,28360(r26)
	ctx.f13.u64 = GV64(ctx.r26.u32 + 28360);
	// cmpwi cr6,r30,1
	ctx.cr6.compare<int32_t>(ctx.r30.s32, 1, ctx.xer);
	// fsub f10,f11,f0
	ctx.f10.f64 = ctx.f11.f64 - ctx.f0.f64;
	// fdiv f30,f10,f13
	ctx.f30.f64 = ctx.f10.f64 / ctx.f13.f64;
	// ble cr6,0x82276ee8  (advance runs when cr6.gt)
	if (ctx.cr6.gt) {
	// addi r29,r28,1
	ctx.r29.s64 = ctx.r28.s64 + 1;
	// fmadd f0,f27,f24,f31
	ctx.f0.f64 = guest_fma(ctx.f27.f64, ctx.f24.f64, ctx.f31.f64);
	// stfd f0,272(r31)
	SV64(ctx.r31.u32 + 272, ctx.f0.u64);
	// fmr f26,f31
	ctx.f26.f64 = ctx.f31.f64;
	// mr r23,r29
	ctx.r23.u64 = ctx.r29.u64;
	}
	// divw r10,r28,r25
	ctx.r10.u64 = uint32_t((ctx.r25.s32 && !(ctx.r28.s32 == INT32_MIN && ctx.r25.s32 == -1)) ? ctx.r28.s32 / ctx.r25.s32 : 0);
	// divw r11,r29,r25
	ctx.r11.u64 = uint32_t((ctx.r25.s32 && !(ctx.r29.s32 == INT32_MIN && ctx.r25.s32 == -1)) ? ctx.r29.s32 / ctx.r25.s32 : 0);
	// cmpw cr6,r11,r10
	ctx.cr6.compare<int32_t>(ctx.r11.s32, ctx.r10.s32, ctx.xer);
	// li r10,1
	ctx.r10.s64 = 1;
	// bgt cr6,0x82276f00  (r10=r20 when !cr6.gt)
	if (!ctx.cr6.gt) {
	// mr r10,r20
	ctx.r10.u64 = ctx.r20.u64;
	}
	// lwz r9,96(r1)
	ctx.r9.u64 = GV32(ctx.r1.u32 + 96);
	// clrlwi r28,r10,24
	ctx.r28.u64 = ctx.r10.u32 & 0xFF;
	// cmpw cr6,r11,r9
	ctx.cr6.compare<int32_t>(ctx.r11.s32, ctx.r9.s32, ctx.xer);
	// beq cr6,0x82276f14  (store when !cr6.eq)
	if (!ctx.cr6.eq) {
	// stw r11,96(r1)
	SV32(ctx.r1.u32 + 96, ctx.r11.u32);
	}
	// clrlwi r30,r28,24
	ctx.r30.u64 = ctx.r28.u32 & 0xFF;
	// lfd f1,272(r31)
	ctx.fpscr.disableFlushMode();
	ctx.f1.u64 = GV64(ctx.r31.u32 + 272);
	// li r5,6
	ctx.r5.s64 = 6;
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);
	// beq cr6,0x82276fac  (PAIR A when !cr6.eq, PAIR B when cr6.eq)
	if (!ctx.cr6.eq) {
	// addi r3,r1,88
	ctx.r3.s64 = ctx.r1.s64 + 88;
	// bl 0x821ee858
	ctx.lr = 0x82276F30;
	sub_821EE858(ctx, base);
	// mr r14,r3
	ctx.r14.u64 = ctx.r3.u64;
	// mr r4,r15
	ctx.r4.u64 = ctx.r15.u64;
	// li r5,-1
	ctx.r5.s64 = -1;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// bl 0x8222cf18
	ctx.lr = 0x82276F44;
	ConstructRefCounted_8222CF18(ctx, base);
	// mr r4,r14
	ctx.r4.u64 = ctx.r14.u64;
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// --- Grow the active-entity bit-array, then drain the per-frame budget ---
	// bl 0x821ec668
	ctx.lr = 0x82276F50;
	ProcessGrowBitArray_821EC668(ctx, base);
	// addi r3,r1,80
	ctx.r3.s64 = ctx.r1.s64 + 80;
	// bl 0x821c67d8
	ctx.lr = 0x82276F58;
	Release_RefCounted_821C67D8(ctx, base);
	// mr r9,r24
	ctx.r9.u64 = ctx.r24.u64;
	decrement_global_counter(ctx, base, &ctx.r9, &ctx.r11, &ctx.r10);
	// addi r3,r1,88
	ctx.r3.s64 = ctx.r1.s64 + 88;
	// stw r20,80(r1)
	SV32(ctx.r1.u32 + 80, ctx.r20.u32);
	// bl 0x821c67d8
	ctx.lr = 0x82276F84;
	Release_RefCounted_821C67D8(ctx, base);
	// mr r6,r24
	ctx.r6.u64 = ctx.r24.u64;
	decrement_global_counter(ctx, base, &ctx.r6, &ctx.r8, &ctx.r7);
	// stw r20,88(r1)
	SV32(ctx.r1.u32 + 88, ctx.r20.u32);
	} else {
	// addi r3,r1,92
	ctx.r3.s64 = ctx.r1.s64 + 92;
	// bl 0x821ee858
	ctx.lr = 0x82276FB4;
	sub_821EE858(ctx, base);
	// mr r14,r3
	ctx.r14.u64 = ctx.r3.u64;
	// mr r4,r16
	ctx.r4.u64 = ctx.r16.u64;
	// li r5,-1
	ctx.r5.s64 = -1;
	// addi r3,r1,84
	ctx.r3.s64 = ctx.r1.s64 + 84;
	// bl 0x8222cf18
	ctx.lr = 0x82276FC8;
	ConstructRefCounted_8222CF18(ctx, base);
	// mr r4,r14
	ctx.r4.u64 = ctx.r14.u64;
	// addi r3,r1,84
	ctx.r3.s64 = ctx.r1.s64 + 84;
	// bl 0x821ec668
	ctx.lr = 0x82276FD4;
	ProcessGrowBitArray_821EC668(ctx, base);
	// addi r3,r1,84
	ctx.r3.s64 = ctx.r1.s64 + 84;
	// bl 0x821c67d8
	ctx.lr = 0x82276FDC;
	Release_RefCounted_821C67D8(ctx, base);
	// mr r9,r24
	ctx.r9.u64 = ctx.r24.u64;
	decrement_global_counter(ctx, base, &ctx.r9, &ctx.r11, &ctx.r10);
	// addi r3,r1,92
	ctx.r3.s64 = ctx.r1.s64 + 92;
	// stw r20,84(r1)
	SV32(ctx.r1.u32 + 84, ctx.r20.u32);
	// bl 0x821c67d8
	ctx.lr = 0x82277008;
	Release_RefCounted_821C67D8(ctx, base);
	// mr r6,r24
	ctx.r6.u64 = ctx.r24.u64;
	decrement_global_counter(ctx, base, &ctx.r6, &ctx.r8, &ctx.r7);
	// stw r20,92(r1)
	SV32(ctx.r1.u32 + 92, ctx.r20.u32);
	}
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);
	// --- Active-player frame update ---
	// bne cr6,0x8227704c  (call-active when !cr6.eq; else query first)
	{
	bool call_active = !ctx.cr6.eq;
	if (!call_active) {
	// lwz r11,160(r31)
	ctx.r11.u64 = GV32(ctx.r31.u32 + 160);
	// lwz r3,28(r11)
	ctx.r3.u64 = GV32(ctx.r11.u32 + 28);
	// bl 0x821ec8a0
	ctx.lr = 0x82277040;
	QueryActivePlayerMethod_821EC8A0(ctx, base);
	// clrlwi r10,r3,24
	ctx.r10.u64 = ctx.r3.u32 & 0xFF;
	// cmplwi cr6,r10,0
	ctx.cr6.compare<uint32_t>(ctx.r10.u32, 0, ctx.xer);
	// beq cr6,0x82277054  (call-active when !cr6.eq)
	call_active = !ctx.cr6.eq;
	}
	if (call_active) {
	// lwz r3,160(r31)
	ctx.r3.u64 = GV32(ctx.r31.u32 + 160);
	// bl 0x821a11e8
	ctx.lr = 0x82277054;
	CallActivePlayerMethod_821A11E8(ctx, base);
	}
	}
	// lwz r3,-6420(r22): active-entity bit-array holder (0x8349E6EC)
	ctx.r3.u64 = GV32(kBitArrayHolder);
	// lwz r11,0(r3)
	ctx.r11.u64 = GV32(ctx.r3.u32 + 0);
	// lwz r10,20(r11)
	ctx.r10.u64 = GV32(ctx.r11.u32 + 20);
	// mtctr r10
	ctx.ctr.u64 = ctx.r10.u64;
	// bctrl 
	ctx.lr = 0x82277068;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
	// divw r9,r29,r25
	ctx.r9.u64 = uint32_t((ctx.r25.s32 && !(ctx.r29.s32 == INT32_MIN && ctx.r25.s32 == -1)) ? ctx.r29.s32 / ctx.r25.s32 : 0);
	// extsw r8,r25
	ctx.r8.s64 = ctx.r25.s32;
	// stfs f21,26908(r21)
	ctx.fpscr.disableFlushMode();
	temp.f32 = float(ctx.f21.f64);
	SV32(ctx.r21.u32 + 26908, temp.u32);
	// mullw r7,r9,r25
	ctx.r7.s64 = int64_t(ctx.r9.s32) * int64_t(ctx.r25.s32);
	// lwz r6,160(r31)
	ctx.r6.u64 = GV32(ctx.r31.u32 + 160);
	// std r8,136(r1)
	SV64(ctx.r1.u32 + 136, ctx.r8.u64);
	// fmr f31,f30
	ctx.f31.f64 = ctx.f30.f64;
	// lwz r3,64(r6)
	ctx.r3.u64 = GV32(ctx.r6.u32 + 64);
	// subf r11,r7,r29
	ctx.r11.u64 = ctx.r29.u64 - ctx.r7.u64;
	// addi r5,r11,1
	ctx.r5.s64 = ctx.r11.s64 + 1;
	// extsw r4,r5
	ctx.r4.s64 = ctx.r5.s32;
	// std r4,152(r1)
	SV64(ctx.r1.u32 + 152, ctx.r4.u64);
	// lfd f0,136(r1)
	ctx.f0.u64 = GV64(ctx.r1.u32 + 136);
	// fcfid f13,f0
	ctx.f13.f64 = double(ctx.f0.s64);
	// frsp f12,f13
	ctx.f12.f64 = double(float(ctx.f13.f64));
	// lfd f11,152(r1)
	ctx.f11.u64 = GV64(ctx.r1.u32 + 152);
	// fcfid f10,f11
	ctx.f10.f64 = double(ctx.f11.s64);
	// frsp f9,f10
	ctx.f9.f64 = double(float(ctx.f10.f64));
	// fdivs f28,f9,f12
	ctx.f28.f64 = double(float(ctx.f9.f64 / ctx.f12.f64));
	// --- Misc subsystems + frame-callback registration ---
	// bl 0x822c1fb8
	ctx.lr = 0x822770B8;
	sub_822C1FB8(ctx, base);
	// bl 0x8217fd08
	ctx.lr = 0x822770BC;
	LazyInitSubsystemA_8217FD08(ctx, base);
	// bl 0x82198860
	ctx.lr = 0x822770C0;
	sub_82198860(ctx, base);
	// bl 0x822c6c60
	ctx.lr = 0x822770C4;
	LazyInitSubsystemB_822C6C60(ctx, base);
	// bl 0x822a9b20
	ctx.lr = 0x822770C8;
	sub_822A9B20(ctx, base);
	// cmplwi cr6,r30,0
	ctx.cr6.compare<uint32_t>(ctx.r30.u32, 0, ctx.xer);
	// beq cr6,0x82277164  (registration block runs when !cr6.eq)
	if (!ctx.cr6.eq) {
	// lwz r30,156(r31)
	ctx.r30.u64 = GV32(ctx.r31.u32 + 156);
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// bl 0x821f8760
	ctx.lr = 0x822770DC;
	ResolveSubsystemReference_821F8760(ctx, base);
	// cmplwi cr6,r3,0
	ctx.cr6.compare<uint32_t>(ctx.r3.u32, 0, ctx.xer);
	// beq cr6,0x8227712c  (callback registration runs when !cr6.eq)
	if (!ctx.cr6.eq) {
	// mr r3,r30
	ctx.r3.u64 = ctx.r30.u64;
	// bl 0x821f8760
	ctx.lr = 0x822770EC;
	ResolveSubsystemReference_821F8760(ctx, base);
	// mr r4,r3
	ctx.r4.u64 = ctx.r3.u64;
	// addi r3,r1,208
	ctx.r3.s64 = ctx.r1.s64 + 208;
	// bl 0x821dcf10
	ctx.lr = 0x822770F8;
	RegisterFrameCallback_821DCF10(ctx, base);
	// addi r11,r31,16
	ctx.r11.s64 = ctx.r31.s64 + 16;
	// addi r10,r3,16
	ctx.r10.s64 = ctx.r3.s64 + 16;
	// lvx128 v0,r0,r3
	ea = (ctx.r3.u32) & ~0xF;
	simde_mm_store_si128((simde__m128i*)ctx.v0.u8, simde_mm_shuffle_epi8(simde_mm_load_si128((simde__m128i*)REX_RAW_ADDR(ea)), simde_mm_load_si128((simde__m128i*)VectorMaskL)));
	// addi r9,r11,16
	ctx.r9.s64 = ctx.r11.s64 + 16;
	// stvx128 v0,r0,r11
	ea = (ctx.r11.u32) & ~0xF;
	simde_mm_store_si128((simde__m128i*)REX_RAW_ADDR(ea), simde_mm_shuffle_epi8(simde_mm_load_si128((simde__m128i*)ctx.v0.u8), simde_mm_load_si128((simde__m128i*)VectorMaskL)));
	// lvx128 v13,r0,r10
	ea = (ctx.r10.u32) & ~0xF;
	simde_mm_store_si128((simde__m128i*)ctx.v13.u8, simde_mm_shuffle_epi8(simde_mm_load_si128((simde__m128i*)REX_RAW_ADDR(ea)), simde_mm_load_si128((simde__m128i*)VectorMaskL)));
	// stvx128 v13,r0,r9
	ea = (ctx.r9.u32) & ~0xF;
	simde_mm_store_si128((simde__m128i*)REX_RAW_ADDR(ea), simde_mm_shuffle_epi8(simde_mm_load_si128((simde__m128i*)ctx.v13.u8), simde_mm_load_si128((simde__m128i*)VectorMaskL)));
	// lvx128 v12,r10,r19
	ea = (ctx.r10.u32 + ctx.r19.u32) & ~0xF;
	simde_mm_store_si128((simde__m128i*)ctx.v12.u8, simde_mm_shuffle_epi8(simde_mm_load_si128((simde__m128i*)REX_RAW_ADDR(ea)), simde_mm_load_si128((simde__m128i*)VectorMaskL)));
	// stvx128 v12,r9,r19
	ea = (ctx.r9.u32 + ctx.r19.u32) & ~0xF;
	simde_mm_store_si128((simde__m128i*)REX_RAW_ADDR(ea), simde_mm_shuffle_epi8(simde_mm_load_si128((simde__m128i*)ctx.v12.u8), simde_mm_load_si128((simde__m128i*)VectorMaskL)));
	// lfs f0,48(r3)
	ctx.fpscr.disableFlushMode();
	temp.u32 = GV32(ctx.r3.u32 + 48);
	ctx.f0.f64 = double(temp.f32);
	// stfs f0,64(r31)
	temp.f32 = float(ctx.f0.f64);
	SV32(ctx.r31.u32 + 64, temp.u32);
	// lbz r11,52(r3)
	ctx.r11.u64 = GV8(ctx.r3.u32 + 52);
	// stb r11,68(r31)
	SV8(ctx.r31.u32 + 68, ctx.r11.u8);
	}
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// --- Update the global game state ---
	// bl 0x821c9008
	ctx.lr = 0x82277134;
	ProcessGameStateUpdate_821C9008(ctx, base);
	// addi r3,r1,168
	ctx.r3.s64 = ctx.r1.s64 + 168;
	// bl 0x8221eb58
	ctx.lr = 0x8227713C;
	query_guest_timebase(ctx, base);  // inlined GetTimebase_8221EB58: r11=timebase (retry if 0); [r3]=r11; r3=1
	// lfd f12,168(r1)
	ctx.fpscr.disableFlushMode();
	ctx.f12.u64 = GV64(ctx.r1.u32 + 168);
	// fcfid f11,f12
	ctx.f11.f64 = double(ctx.f12.s64);
	// lfd f0,28352(r27)
	ctx.f0.u64 = GV64(ctx.r27.u32 + 28352);
	// lfd f13,28360(r26)
	ctx.f13.u64 = GV64(ctx.r26.u32 + 28360);
	// stfd f29,200(r31)
	SV64(ctx.r31.u32 + 200, ctx.f29.u64);
	// fsub f10,f11,f0
	ctx.f10.f64 = ctx.f11.f64 - ctx.f0.f64;
	// fdiv f31,f10,f13
	ctx.f31.f64 = ctx.f10.f64 / ctx.f13.f64;
	// fsub f0,f31,f30
	ctx.f0.f64 = ctx.f31.f64 - ctx.f30.f64;
	// stfd f0,208(r31)
	SV64(ctx.r31.u32 + 208, ctx.f0.u64);
	// fmr f29,f0
	ctx.f29.f64 = ctx.f0.f64;
	}
	// mr r5,r28
	ctx.r5.u64 = ctx.r28.u64;
	// fmr f1,f28
	ctx.fpscr.disableFlushMode();
	ctx.f1.f64 = ctx.f28.f64;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// bl 0x821a6010
	ctx.lr = 0x82277174;
	DispatchFrameCallback_821A6010(ctx, base);
	// mr r6,r28
	ctx.r6.u64 = ctx.r28.u64;
	// mr r3,r31
	ctx.r3.u64 = ctx.r31.u64;
	// lfd f1,272(r31)
	ctx.fpscr.disableFlushMode();
	ctx.f1.u64 = GV64(ctx.r31.u32 + 272);
	// fmr f2,f28
	ctx.f2.f64 = ctx.f28.f64;
	// bl 0x82278c90
	ctx.lr = 0x82277188;
	sub_82278C90(ctx, base);
	// lwz r3,27108(r18)
	ctx.r3.u64 = GV32(ctx.r18.u32 + 27108);
	// mr r28,r29
	ctx.r28.u64 = ctx.r29.u64;
	// lbz r11,4(r3)
	ctx.r11.u64 = GV8(ctx.r3.u32 + 4);
	// cmplwi cr6,r11,0
	ctx.cr6.compare<uint32_t>(ctx.r11.u32, 0, ctx.xer);
	// beq cr6,0x822771d0  (flag block runs when !cr6.eq)
	if (!ctx.cr6.eq) {
	// lis r11,-31950
	ctx.r11.s64 = (int64_t)(int32_t)0x83320000;
	// lwz r10,8(r3)
	ctx.r10.u64 = GV32(ctx.r3.u32 + 8);
	// lwz r11,-27380(r11)
	ctx.r11.u64 = GV32(ctx.r11.u32 + -27380);
	// cmpw cr6,r11,r10
	ctx.cr6.compare<int32_t>(ctx.r11.s32, ctx.r10.s32, ctx.xer);
	// bgt cr6,0x822771cc  (stb when cr6.gt)
	bool do_stb = ctx.cr6.gt;
	if (!ctx.cr6.gt) {
	// lwz r11,0(r3)
	ctx.r11.u64 = GV32(ctx.r3.u32 + 0);
	// lbz r10,144(r11)
	ctx.r10.u64 = GV8(ctx.r11.u32 + 144);
	// rlwinm r9,r10,0,25,25
	ctx.r9.u64 = __builtin_rotateleft64(ctx.r10.u32 | (ctx.r10.u64 << 32), 0) & 0x40;
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r9.u32, 0, ctx.xer);
	// beq cr6,0x822771cc  (stb when cr6.eq, else sub_8243A2C8)
	if (ctx.cr6.eq) {
		do_stb = true;
	} else {
	// bl 0x8243a2c8
	ctx.lr = 0x822771C8;
	sub_8243A2C8(ctx, base);
	}
	}
	if (do_stb) {
	// stb r20,4(r3)
	SV8(ctx.r3.u32 + 4, ctx.r20.u8);
	}
	}
	// lwz r3,27284(r17)
	ctx.r3.u64 = GV32(ctx.r17.u32 + 27284);
	// bl 0x822c93c0
	ctx.lr = 0x822771D8;
	sub_822C93C0(ctx, base);
	// addi r3,r1,184
	ctx.r3.s64 = ctx.r1.s64 + 184;
	// bl 0x8221eb58
	ctx.lr = 0x822771E0;
	query_guest_timebase(ctx, base);  // inlined GetTimebase_8221EB58: r11=timebase (retry if 0); [r3]=r11; r3=1
	// lfd f12,184(r1)
	ctx.fpscr.disableFlushMode();
	ctx.f12.u64 = GV64(ctx.r1.u32 + 184);
	// fcfid f11,f12
	ctx.f11.f64 = double(ctx.f12.s64);
	// lfd f0,28352(r27)
	ctx.f0.u64 = GV64(ctx.r27.u32 + 28352);
	// lfd f13,28360(r26)
	ctx.f13.u64 = GV64(ctx.r26.u32 + 28360);
	// fsub f10,f11,f0
	ctx.f10.f64 = ctx.f11.f64 - ctx.f0.f64;
	// fdiv f9,f10,f13
	ctx.f9.f64 = ctx.f10.f64 / ctx.f13.f64;
	// fsub f8,f9,f31
	ctx.f8.f64 = ctx.f9.f64 - ctx.f31.f64;
	// stfd f8,216(r31)
	SV64(ctx.r31.u32 + 216, ctx.f8.u64);
	// fadd f29,f8,f29
	ctx.f29.f64 = ctx.f8.f64 + ctx.f29.f64;
}

static __attribute__((noinline)) void check_loop_backedge(PPCContext& ctx, uint8_t* base) {
	// lwz r3,-6420(r22): active-entity bit-array holder (0x8349E6EC)
	ctx.r3.u64 = GV32(kBitArrayHolder);
	// lwz r11,0(r3)
	ctx.r11.u64 = GV32(ctx.r3.u32 + 0);
	// lwz r10,16(r11)
	ctx.r10.u64 = GV32(ctx.r11.u32 + 16);
	// mtctr r10
	ctx.ctr.u64 = ctx.r10.u64;
	// bctrl 
	ctx.lr = 0x82277220;
	REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
	// clrlwi r9,r3,24
	ctx.r9.u64 = ctx.r3.u32 & 0xFF;
	// cmplwi cr6,r9,0
	ctx.cr6.compare<uint32_t>(ctx.r9.u32, 0, ctx.xer);
}
extern "C" void ProcessGameFrame_82276C30(PPCContext& __restrict ctx, uint8_t* base) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};

	// ===== Prologue: save caller regs + allocate the 512-byte guest frame =====
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82ca2bb0
	ctx.lr = 0x82276C38;
	__savegprlr_14(ctx, base);
	// addi r12,r1,-152
	ctx.r12.s64 = ctx.r1.s64 + -152;
	// bl 0x82ca74ec
	ctx.lr = 0x82276C40;
	__savefpr_21(ctx, base);
	// stwu r1,-512(r1)
	ea = -512 + ctx.r1.u32;
	SV32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	probe_and_setup(ctx, base);
	compute_frame_timing(ctx, base);
	// beq cr6,0x8227722c  (setup+loop runs when !cr6.eq)
	if (!ctx.cr6.eq) {
	setup_frame(ctx, base);
	// ===== Main per-frame work loop: drain pending work, decrement the budget =====
	do {
	if (!frame_gate(ctx, base)) break;
	compute_loop_timing(ctx, base);
	// ble 0x82277208  (big block runs when cr0.gt, else yield)
	if (ctx.cr0.gt) {
	process_frame_work(ctx, base);
	} else {
	// --- (no work on this path) yield + re-check the frame threshold ---
	// bl 0x82cbd098
	ctx.lr = 0x8227720C;
	Yield_82CBD098(ctx, base);
	// Only the no-work path pauses; preserve the guest yield and all its
	// register/barrier side effects before polling the frame threshold again.
	if (const auto sleep_us = REXCVAR_GET(game_idle_sleep_us)) {
		std::this_thread::sleep_for(std::chrono::microseconds(sleep_us));
	}
	}
	check_loop_backedge(ctx, base);
	} while (!ctx.cr6.eq);
	}
	// ===== Epilogue: restore the frame + callee-saved regs, return =====
	// lwz r3,84(r31)
	ctx.r3.u64 = GV32(ctx.r31.u32 + 84);
	// addi r1,r1,512
	ctx.r1.s64 = ctx.r1.s64 + 512;
	// addi r12,r1,-152
	ctx.r12.s64 = ctx.r1.s64 + -152;
	// bl 0x82ca7538
	ctx.lr = 0x8227723C;
	__restfpr_21(ctx, base);
	// b 0x82ca2c00
	__restgprlr_14(ctx, base);
	return;
}

