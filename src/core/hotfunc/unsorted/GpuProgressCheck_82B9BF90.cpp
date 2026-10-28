// Hand-tuned override: goto-free rewrite of GpuProgressCheck_82B9BF90.
// Jumps/labels from the recompiler output (fable_2_recomp.284.cpp) were
// replaced with a while loop (spin-wait) and nested if/else blocks; every
// guest load/store, call/return-address pair, compare, and exit path is
// otherwise identical to the generated body, with the same register side
// effects. The first line makes tools/make_hotfunc_overrides.py skip this file.
#include "fable_2_pch.h"
#include "fable_2_funcs.284.h"

extern "C" void GetGlobalField_8221EE38(PPCContext&, uint8_t*);
extern "C" void sub_82BA0E80(PPCContext&, uint8_t*);

extern "C" void GpuProgressCheck_82B9BF90(PPCContext& __restrict ctx, uint8_t* base) {
	REX_FUNC_PROLOGUE();
	uint32_t ea{};
	// mflr r12
	ctx.r12.u64 = ctx.lr;
	// bl 0x82ca2bec
	ctx.lr = 0x82B9BF98;
	__savegprlr_29(ctx, base);
	// stwu r1,-112(r1)
	ea = -112 + ctx.r1.u32;
	REX_STORE_U32(ea, ctx.r1.u32);
	ctx.r1.u32 = ea;
	// mr r31,r3
	ctx.r31.u64 = ctx.r3.u64;
	// li r11,4
	ctx.r11.s64 = 4;
	// lwz r29,0(r31)
	ctx.r29.u64 = REX_LOAD_U32(ctx.r31.u32 + 0);
	// cctpl 
	while (true) {
		// db16cyc 
		// db16cyc 
		// db16cyc 
		// db16cyc 
		// db16cyc 
		// db16cyc 
		// db16cyc 
		// db16cyc 
		// addic. r11,r11,-1
		ctx.xer.ca = ctx.r11.u32 > 0;
		ctx.r11.s64 = ctx.r11.s64 + -1;
		ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
		// bne 0x82b9bfac
		if (!ctx.cr0.eq) continue;
		break;
	}
	// cctpm 
	// lbz r11,10941(r29)
	ctx.r11.u64 = REX_LOAD_U8(ctx.r29.u32 + 10941);
	// rlwinm. r11,r11,0,30,30
	ctx.r11.u64 = __builtin_rotateleft64(ctx.r11.u32 | (ctx.r11.u64 << 32), 0) & 0x2;
	ctx.cr0.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
	// bne 0x82b9c05c
	if (!ctx.cr0.eq) {
		// li r3,0
		ctx.r3.s64 = 0;
	}
	else {
		// lwz r11,10896(r29)
		ctx.r11.u64 = REX_LOAD_U32(ctx.r29.u32 + 10896);
		// lwz r10,256(r13)
		ctx.r10.u64 = REX_LOAD_U32(ctx.r13.u32 + 256);
		// lwz r9,8(r31)
		ctx.r9.u64 = REX_LOAD_U32(ctx.r31.u32 + 8);
		// lwz r8,0(r11)
		ctx.r8.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
		// lwz r30,88(r10)
		ctx.r30.u64 = REX_LOAD_U32(ctx.r10.u32 + 88);
		// cmplw cr6,r9,r8
		ctx.cr6.compare<uint32_t>(ctx.r9.u32, ctx.r8.u32, ctx.xer);
		// beq cr6,0x82b9c00c
		if (!ctx.cr6.eq) {
			// lwz r11,0(r11)
			ctx.r11.u64 = REX_LOAD_U32(ctx.r11.u32 + 0);
			// stw r30,12(r31)
			REX_STORE_U32(ctx.r31.u32 + 12, ctx.r30.u32);
			// stw r11,8(r31)
			REX_STORE_U32(ctx.r31.u32 + 8, ctx.r11.u32);
		}
		// bl 0x8221ee38
		ctx.lr = 0x82B9C010;
		GetGlobalField_8221EE38(ctx, base);
		// lwz r11,10888(r29)
		ctx.r11.u64 = REX_LOAD_U32(ctx.r29.u32 + 10888);
		// cmplw cr6,r11,r3
		ctx.cr6.compare<uint32_t>(ctx.r11.u32, ctx.r3.u32, ctx.xer);
		// bne cr6,0x82b9c02c
		if (ctx.cr6.eq) {
			// lwz r11,11004(r29)
			ctx.r11.u64 = REX_LOAD_U32(ctx.r29.u32 + 11004);
			// cmpwi cr6,r11,0
			ctx.cr6.compare<int32_t>(ctx.r11.s32, 0, ctx.xer);
			// beq cr6,0x82b9c02c
			if (!ctx.cr6.eq) {
				// stw r30,12(r31)
				REX_STORE_U32(ctx.r31.u32 + 12, ctx.r30.u32);
			}
		}
		// lwz r11,12(r31)
		ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 12);
		// subf r11,r11,r30
		ctx.r11.u64 = ctx.r30.u64 - ctx.r11.u64;
		// cmplwi cr6,r11,5000
		ctx.cr6.compare<uint32_t>(ctx.r11.u32, 5000, ctx.xer);
		// bge cr6,0x82b9c044
		if (ctx.cr6.lt) {
			// li r3,1
			ctx.r3.s64 = 1;
		}
		else {
			// mr r3,r29
			ctx.r3.u64 = ctx.r29.u64;
			// bl 0x82ba0e80
			ctx.lr = 0x82B9C04C;
			sub_82BA0E80(ctx, base);
			// cmpwi r3,0
			ctx.cr0.compare<int32_t>(ctx.r3.s32, 0, ctx.xer);
			// bne 0x82b9c05c
			if (!ctx.cr0.eq) {
				// li r3,0
				ctx.r3.s64 = 0;
			}
			else {
				// stw r30,12(r31)
				REX_STORE_U32(ctx.r31.u32 + 12, ctx.r30.u32);
				// li r3,1
				ctx.r3.s64 = 1;
			}
		}
	}
	// addi r1,r1,112
	ctx.r1.s64 = ctx.r1.s64 + 112;
	// b 0x82ca2c3c
	__restgprlr_29(ctx, base);
	return;
}
