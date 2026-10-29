// fable2_hooks.cpp - mid-asm hook functions for the [[entrypoint.midasm_hook]]
// entries in fable_2_manifest.toml (see docs/patches.md).
//
// Codegen emits an extern prototype for each hook into the generated code
// (e.g. `extern void fable2_hook_website_g1(PPCRegister& r9);`) and calls it at
// the configured instruction address, passing the named registers BY
// REFERENCE, so a hook can read and/or rewrite them. Define each hook with
// plain C++ linkage (NOT extern "C") and exactly once, matching the emitted
// prototype.
//
// Every patch hook is gated on a toggle in fable2_config.toml ([patches]),
// consulted on each call, so a patch can be A/B'd without a rebuild.

#include <rex/ppc/context.h>  // PPCRegister (union with u64/s64/u32/... views)
#include <rex/logging/macros.h>
#include <rex/cvar.h>

#include <string>

#include <fmt/format.h>

#include "fable2_config.h"
#include "mouse_look.h"  // fable2::mouselook (shared host/guest mouse-delta state)

// ---------------------------------------------------------------------------
// Guild chest unlock (fable2.com website items + Collectors Edition content).
//
// An unregistered player who clicks the Guild chest sees "Go to www.fable2.com
// for information on how to access the gold and items your heroic ancestors
// left behind." The chest contents are decided by two getter functions that
// run at save load:
//   - GuildChest_GetWebsiteItem_8256E368 (website items)
//   - GuildChest_GetCEContent_824B3528    (CE content)
//
// Each getter is gated by two "registered" flag bits on the chest object, and
// only when both pass does it find the item and call a per-object GRANT vtable
// method, returning THAT method's result. For an unregistered player the gates
// fail (and even where they pass, the grant method returns 0), so the getter
// reports "not available" and the chest stays locked. We force BOTH:
//   1. the two gate bit-extract results to 1 (g1 / g1b hooks below), and
//   2. the grant method's return to 1 (grantnew / grantavail hooks below).
//
// The getters run at save load, so this populates the chest inventory on load;
// the chest then opens normally and shows the website + CE items.
//
// Toggle: [patches] unlock_website / unlock_ce in fable2_config.toml
// (both default true).
// ---------------------------------------------------------------------------

// Unlock Website Items. GATE 1 (bit6 of *(r4+0x90)): the `rlwinm r9, r10, 0,
// 0x19, 0x19` at 0x8256E384 extracts bit 6 into r9 (0x40 or 0). Force r9 = 1
// so the gate always passes.
void fable2_hook_website_g1(PPCRegister& r9) {
  if (fable2::config::Get().unlock_website) {
    r9.u64 = 1;
  }
}

// Unlock Website Items. GATE 1b (bit0 of *(r4+0x40)): the `clrlwi r8, r9,
// 0x1f` at 0x8256E3AC extracts bit 0 into r8 (1 or 0). Force r8 = 1 so the
// gate always passes.
void fable2_hook_website_g1b(PPCRegister& r8) {
  if (fable2::config::Get().unlock_website) {
    r8.u64 = 1;
  }
}

// Unlock Website Items. Grant method sub_8256D940 (the website-chest
// vtable[1] "is this item new?" check): its final instruction is `xori r3,
// r9, 1` at 0x8256D9E4. It returns 0 when the item hash is already in the
// grant list; force r3 = 1 so the getter reports the item as granted.
void fable2_hook_website_grantnew(PPCRegister& r3) {
  if (fable2::config::Get().unlock_website) {
    r3.u32 = 1;
  }
}

// Unlock Collectors Edition Content. GATE 1 (bit6 of *(r4+0x90)): the
// `rlwinm r10, r11, 0, 0x19, 0x19` at 0x824B3540 extracts bit 6 into r10
// (0x40 or 0). Force r10 = 1 so the gate always passes.
void fable2_hook_ce_g1(PPCRegister& r10) {
  if (fable2::config::Get().unlock_ce) {
    r10.u64 = 1;
  }
}

// Unlock Collectors Edition Content. GATE 1b (a bit of *(r4+0x28)): the
// `rlwinm r9, r10, 7, 0x1f, 0x1f` at 0x824B3568 extracts the bit into r9
// (1 or 0). Force r9 = 1 so the gate always passes.
void fable2_hook_ce_g1b(PPCRegister& r9) {
  if (fable2::config::Get().unlock_ce) {
    r9.u64 = 1;
  }
}

// Unlock Collectors Edition Content. Grant method sub_824ACAE0 (the CE-chest
// grant vtable slot called from GetCEContent): its entire body is `lbz r3,
// 993(r3)` (returns the byte at item+0x3E1, the "CE content available" flag).
// Force r3 = 1 so the getter reports the CE content as available.
void fable2_hook_ce_grantavail(PPCRegister& r3) {
  if (fable2::config::Get().unlock_ce) {
    r3.u32 = 1;
  }
}

// Disable Motion Blur. Runs right after `lfs f12, 0xd4(r31)` at 0x822A49E8 in
// the camera update, where the camera's current full-screen motion blur amount
// (+0xD4) is loaded to be copied into the renderer's view settings (+0x7C).
// Forcing f12 to 0 means the renderer never applies the blur, while the camera
// object and any script reading Camera.GetBlur still see the game's value.
// The first non-zero request is logged once, so the log shows whether the game
// actually asked for motion blur during play.
void fable2_hook_disable_motion_blur(PPCRegister& f12) {
  static bool logged = false;
  const bool disable = fable2::config::Get().disable_motion_blur;
  if (!logged && f12.f64 != 0.0) {
    logged = true;
    REXSYS_INFO("[motion-blur] game requested full-screen motion blur {:.3f} ({})", f12.f64,
                disable ? "forced to 0" : "left on");
  }
  if (disable) {
    f12.f64 = 0.0;
  }
}

// Skip Intro Videos (just-harry's "Skip intro videos" patch, as a hook).
// sub_822F4958 builds the boot video queue (microsoft_logo.bik,
// lionhead_logo.bik, terminator) and loops queueing slots until
// CompareString_8229AD78 reports the terminator. This runs right after the
// first CompareString (bl at 0x822F49B8): r3 = 0 means "slot 0 is the
// terminator", so the loop is skipped and no intro video is queued. Harry's
// original NOPs the slot-0 construction instead; the hook leaves all three
// strings constructed and released normally. Runs once per boot.
// fable2_skip_intros is the launcher's "Skip intro videos" switch (fable_2.toml);
// either it or [patches] skip_intro_videos turns the skip on.
REXCVAR_DEFINE_BOOL(fable2_skip_intros, false, "General",
                    "Skip the Microsoft Game Studios and Lionhead intro videos");

void fable2_hook_skip_intro_videos(PPCRegister& r3) {
  if (fable2::config::Get().skip_intro_videos || REXCVAR_GET(fable2_skip_intros)) {
    r3.u64 = 0;
  }
}

// High Tick Rate (Xenia patch by Guy), code half. Runs BEFORE the store at
// 0x8233AEB4 that writes the game's LF tick-rate double (0x83319510);
// returning true jumps past it, which is exactly the Xenia patch's NOP.
// Without this the game overwrote the patched value (15 Hz -> 30 Hz, written
// at load by src/core/fable2_patches.cpp) and the patch had no effect.
// Toggle: [patches] high_tick_rate.
bool fable2_hook_high_tick_rate_skip_store() {
  static const bool enabled = [] {
    const bool on = fable2::config::Get().high_tick_rate;
    if (on) {
      REXSYS_INFO("[tick-rate] LF tick forced to 30 Hz (high_tick_rate)");
    }
    return on;
  }();
  return enabled;
}

// Higher HF Tick Rate (Xenia patch by Ultra), code half. Runs BEFORE the store
// at 0x8233AE98 that writes the HF tick double (0x83319518, normally 30 Hz =
// twice the 15 Hz LF tick). Skipping it keeps the patched 60 Hz, so with
// high_tick_rate on the HF:LF ratio stays 2:1 (60:30) instead of collapsing
// to 1:1 (30:30), which made cloth physics misbehave.
// Requires high_tick_rate: on its own (HF 60 Hz with LF 15 Hz, 4:1) it would
// break the 2:1 ratio the other way, so it is ignored unless both are on.
// Toggle: [patches] higher_hf_tick_rate (needs [patches] high_tick_rate).
bool fable2_hook_high_hf_tick_rate_skip_store() {
  static const bool enabled = [] {
    const auto& cfg = fable2::config::Get();
    if (cfg.higher_hf_tick_rate && !cfg.high_tick_rate) {
      REXSYS_WARN("[tick-rate] higher_hf_tick_rate ignored: it requires high_tick_rate");
      return false;
    }
    if (cfg.higher_hf_tick_rate) {
      REXSYS_INFO("[tick-rate] HF tick forced to 60 Hz (higher_hf_tick_rate)");
    }
    return cfg.higher_hf_tick_rate;
  }();
  return enabled;
}

// Direct mouse-to-camera look (separates the mouse from the keyboard->gamepad
// mapping; see src/input/mouse_look.h). Runs at 0x822A47F8 in
// CECCameraUpdateFrame_822A47C0, right after the virtual call writes the
// camera's "current" transform to the stack (r1+96), and before the view lerp.
// r31 is the active CECCamera.
//
// The goal is to orbit the camera around the hero EXACTLY like the game's own
// right-stick look. A live [ml-dump] capture (hero stationary, stick pushed
// around) shows the game drives three coupled things on an orbit: the rotation-
// state orientation quaternions (cam+160 target / cam+176 current) AND the
// world-space eye position (cam+144), which sweeps a ~3.1-radius circle around
// the hero. Rotating only the quaternions makes the camera turn in place (no
// orbit); rotating only the eye detaches it from the hero. So the hook does
// both, with the same world-space orbit rotation R:
//   * orbit the eye:        eye' = pivot + R * (eye - pivot)
//   * rotate the quaternions: q' = R * q   (target + current)
// where pivot is the hero (derived as eye + forward * camera_distance, low-passed
// to resist jitter) and R = yaw around world-up then pitch around the resulting
// right axis. The hero-follow recomputes cam+144/160/176 from scratch each frame,
// so the hook re-applies the persistent look offset (running yaw/pitch radians)
// every frame and the look sticks where it is aimed without springing back or
// running away. Offsets + sensitivity + orbit radius are in fable2_config.toml
// [input]; mouse_look_camera_diag / mouse_look_camera_dump log the fields so the
// layout can be confirmed with no rebuild.
// Legacy 2-axis orbit body (defined below, after the hook); see its comment.
static void legacy_orbit_2axis(rex::memory::Memory* mem, uint32_t cam,
                               const fable2::config::Values& cfg,
                               const fable2::mouselook::Vec3& eye, float yaw,
                               float pitch);

void fable2_hook_direct_mouse_look(PPCRegister& r31) {
  const auto& cfg = fable2::config::Get();
  rex::memory::Memory* mem =
      fable2::mouselook::memory_ref().load(std::memory_order_acquire);
  if (!mem) return;
  const uint32_t cam = r31.u32;
  if (cam == 0) return;

  // Diagnostic: dump EVERY 4-byte field of the active camera object (offset
  // 0..260) once per frame (capped), to the [ml-dump] log, so we can see
  // empirically which fields the game's own right-stick look changes. Only the
  // active hero camera (the one with a non-zero look-at center at +144) is
  // dumped.
  if (cfg.mouse_look_camera_dump) {
    const float cx = fable2::mouselook::read_guest_float(mem, cam + 144);
    if (cx != 0.0f) {
      static int dumped = 0;
      if (dumped < 2400) {
        std::string s = fmt::format("[ml-dump] cam=0x{:08X} ", cam);
        for (uint32_t off = 0; off <= 260; off += 4) {
          const float v = fable2::mouselook::read_guest_float(mem, cam + off);
          s += fmt::format("a{}={:.3f} ", off, v);
        }
        REXSYS_INFO("{}", s.c_str());
        ++dumped;
      }
    }
    // Also dump the camera CONTROLLER (owner object at cam+4) — the native
    // right-stick orbit's accumulated state (angle + clamp) lives here, not on
    // the CECCamera. Only the active hero camera (non-zero eye at +144) is
    // dumped. This lets a full-range stick test reveal the orbit-pitch field
    // and its min/max.
    if (cx != 0.0f) {
      uint32_t owner = 0;
      if (fable2::mouselook::try_read_guest_u32(mem, cam + 4, &owner) &&
          fable2::mouselook::is_plausible_guest_ptr(owner)) {
        static int ctldumped = 0;
        if (ctldumped < 2400) {
          std::string s = fmt::format("[ml-ctl] ctl=0x{:08X} ", owner);
          for (uint32_t off = 0; off <= 256; off += 4) {
            const float v = fable2::mouselook::read_guest_float(mem, owner + off);
            s += fmt::format("c{}={:.3f} ", off, v);
          }
          // The owner's vtable slot-0x74 is the transform UPDATE method (a bctrl
          // in CECCameraUpdateFrame that computes the orbit from the controller
          // state). Resolve its runtime address so we can find and rewrite it via
          // HotFunc (the mouse will drive this exact method, sharing the native
          // orbit bounds).
          {
            uint32_t owner_vtable = 0, vt74 = 0;
            if (fable2::mouselook::try_read_guest_u32(mem, owner + 0, &owner_vtable) &&
                fable2::mouselook::try_read_guest_u32(mem, owner_vtable + 0x74, &vt74)) {
              s += fmt::format(" vt74=0x{:08X}", vt74);
            }
          }
          // Also dump the look sub-object the virtual call reads (owner+124), and
          // resolve the transform method's address: the bctrl in
          // CECCameraUpdateFrame dispatches to *(sub+0)+64 (vtable[16]) with r3 =
          // r1+96. The orbit-angle field we want to offset is somewhere in the
          // sub-object; a full-range stick sweep reveals which field is it.
          uint32_t sub = 0;
          if (fable2::mouselook::try_read_guest_u32(mem, owner + 124, &sub) &&
              fable2::mouselook::is_plausible_guest_ptr(sub)) {
            s += fmt::format(" | sub=0x{:08X} ", sub);
            uint32_t sub_vtable = 0;
            uint32_t vt16 = 0;
            uint32_t vt100 = 0;
            if (fable2::mouselook::try_read_guest_u32(mem, sub + 0, &sub_vtable)) {
              // vtable slot-16 (offset 64): the COPY method (reads the
              // sub-object's transform into the r1+96 buffer).
              if (fable2::mouselook::try_read_guest_u32(mem, sub_vtable + 64, &vt16)) {
                s += fmt::format("vt16=0x{:08X} ", vt16);
              }
              // vtable slot-25 (offset 100): the transform UPDATE method (the
              // second bctrl in CECCameraUpdateFrame, r3 = sub-object, v1 = the
              // lerped transform). This is the function that updates the
              // sub-object's transform from the orbit state.
              if (fable2::mouselook::try_read_guest_u32(mem, sub_vtable + 100, &vt100)) {
                s += fmt::format("vt100=0x{:08X} ", vt100);
              }
            }
            for (uint32_t off = 0; off <= 256; off += 4) {
              const float v = fable2::mouselook::read_guest_float(mem, sub + off);
              s += fmt::format("s{}={:.3f} ", off, v);
            }
          }
          REXSYS_INFO("{}", s.c_str());
          ++ctldumped;
        }
      }
    }
    // Also dump the GLOBAL camera-state object that sub_822CBDC8 integrates the
    // eye offset into (reached via the pointer chain
    // 0x83496940 -> +12 -> +88 -> +4). The look orbit (eye position around the
    // hero) is integrated here, so it may live in this object rather than the
    // CECCamera. Dump it under a distinct [ml-dump] tag.
    uint32_t p0 = 0, p1 = 0, p2 = 0, gs = 0;
    bool ok0 = fable2::mouselook::try_read_guest_u32(mem, 0x83496940u, &p0);
    bool ok1 = ok0 && fable2::mouselook::try_read_guest_u32(mem, p0 + 12, &p1);
    bool ok2 = ok1 && fable2::mouselook::try_read_guest_u32(mem, p1 + 88, &p2);
    bool ok3 = ok2 && fable2::mouselook::try_read_guest_u32(mem, p2 + 4, &gs);
    // Log the pointer chain a few times so we can see where it breaks.
    static int ptrdumped = 0;
    if (ptrdumped < 3) {
      ++ptrdumped;
      REXSYS_INFO("[ml-ptr] base=0x83496940 p0=0x{:08X} p1=0x{:08X} p2=0x{:08X} gs=0x{:08X} ok={}{}{}{}",
                  p0, p1, p2, gs, ok0 ? '0' : 'X', ok1 ? '1' : 'X',
                  ok2 ? '2' : 'X', ok3 ? '3' : 'X');
    }
    if (ok3 && fable2::mouselook::is_plausible_guest_ptr(gs)) {
      static int gsdumped = 0;
      if (gsdumped < 2400) {
        std::string s = fmt::format("[ml-gs] gs=0x{:08X} ", gs);
        for (uint32_t off = 0; off <= 260; off += 4) {
          const float v = fable2::mouselook::read_guest_float(mem, gs + off);
          s += fmt::format("g{}={:.3f} ", off, v);
        }
        REXSYS_INFO("{}", s.c_str());
        ++gsdumped;
      }
    }
  }

  // Diagnostic: log the camera's orbit angle scalars + look-at every frame
  // (capped), independent of direct_mouse_look, so we can watch which fields
  // change when the game's own right-stick look runs. The angle scalars are the
  // camera's accumulated orbit angles: current at +44/48/52, target at +68/72/76.
  if (cfg.mouse_look_camera_diag) {
    static int logged = 0;
    if (logged < 400) {
      auto f = [&](uint32_t addr) {
        return fable2::mouselook::read_guest_float(mem, addr);
      };
      REXSYS_INFO(
          "[ml-d] cam=0x{:08X} ang44={:.3f} ang48={:.3f} ang52={:.3f}  "
          "tgt68={:.3f} tgt72={:.3f} tgt76={:.3f}  center144={:.2f},{:.2f},{:.2f}",
          cam,
          f(cam + 44), f(cam + 48), f(cam + 52),
          f(cam + 68), f(cam + 72), f(cam + 76),
          f(cam + 144), f(cam + 148), f(cam + 152));
      ++logged;
    }
  }

// Direct-mouse-look (re-implemented orbit) is DISABLED. The mouse now drives
// the synthetic pad's right stick (see keyboard_gamepad.h) so the game's own
// right-stick pipeline produces the look 1:1 with a real right stick. The
// diagnostics above still run (gated by their config); the custom orbit code
// below is preserved (not compiled) for reference.
#if 0
  // Direct-mouse camera look. Two paths:
  //   * Default (mouse_look_legacy_orbit = false): NATIVE single-DOF orbit. We
  //     offset the game's own target orbit angle (cam+68, the same angle the
  //     right stick drives) by the accumulated mouse pitch, and let the game's
  //     native easing (cam+44 -> cam+68) drive the camera -- reusing its orbit
  //     radius, easing, and banking 1:1 with the stick.
  //   * Legacy (mouse_look_legacy_orbit = true): the re-implemented 2-axis
  //     external orbit (eye rotated around a low-passed hero pivot + a look-at
  //     quaternion, with an independent yaw).
  // The diagnostics above run regardless of which path is active.
  if (!cfg.direct_mouse_look) return;

  // 1:1 native injection: the RightStickLookProcess_832BB018 hotfunc override
  // already applied the per-frame mouse delta to the right-stick Y u16 (peeked,
  // not consumed) during input processing, so the game's own orbit/easing/clamp
  // pipeline now owns the camera. Reset the delta here (once per frame) and skip
  // all orbit logic so we don't double-apply. This fires for the first CECCamera
  // each frame (hero or otherwise), which is the once-per-frame cadence the
  // peek-and-reset design needs.
  if (cfg.mouse_look_native_inject) {
    int32_t ndx = 0, ndy = 0;
    fable2::mouselook::take_delta(ndx, ndy);
    return;
  }

  // Only the hero (player) camera is orbitable. This hook fires for EVERY
  // CECCamera, including cinematic/cutscene cameras that the game scripts on
  // its own (the opening movie, cut-in shots, ...). The hero camera is flagged
  // 1.0 at camera_hero_flag_off; cutscene/secondary cameras are 0.0. Check this
  // BEFORE consuming the mouse delta so a skipped (scripted) camera can't eat
  // the input, and so we never fight a scripted camera — orbiting one makes the
  // view go wild and breaks cutscenes. The all-zero-eye secondary camera is
  // skipped too so we never write a bogus eye/rotation into it.
  const fable2::mouselook::Vec3 eye = fable2::mouselook::read_guest_vec3(
      mem, cam + uint32_t(cfg.camera_eye_off));
  if (fable2::mouselook::vec3_len(eye) < 1.0f) return;  // secondary camera
  const float hero_flag = fable2::mouselook::read_guest_float(
      mem, cam + uint32_t(cfg.camera_hero_flag_off));
  if (hero_flag < 0.5f) return;  // cinematic / scripted camera

  // Accumulate the persistent look offset (radians). It persists across frames
  // so the look sticks instead of springing back when the cursor stops.
  int32_t dx = 0;
  int32_t dy = 0;
  fable2::mouselook::take_delta(dx, dy);
  // Map the shared sensitivity slider (mouse_look_scale, default 256) onto the
  // direct-look sensitivity too, so one control scales both the legacy right-
  // stick mapping and this direct camera orbit. At 256 the factor is 1.0, so
  // the configured mouse_look_*_sens values are used verbatim (no behavior
  // change at the default); a larger slider value = faster look. Each sens
  // keeps its own sign (axis direction), only the magnitude is scaled.
  const float sens_scale = float(cfg.mouse_look_scale) / 256.0f;
  fable2::mouselook::cum_yaw() += float(dx) * (cfg.mouse_look_yaw_sens * sens_scale);
  fable2::mouselook::cum_pitch() += float(dy) * (cfg.mouse_look_pitch_sens * sens_scale);
  // Native-aware pitch clamp. The game's own right-stick look drives the orbit
  // into the target angle at cam+68 and clamps it to [floor, ceil]. We read
  // that native angle and clamp the mouse's cumulative pitch so the TOTAL
  // (native orbit + mouse) stays within [floor, ceil]. This makes the mouse and
  // the stick share one orbit and one set of bounds: the mouse's headroom shrinks
  // as the stick approaches a limit, so the two can't compound past the native
  // limits. Yaw is left unbounded (a full 360 orbit).
  const float native_orbit = fable2::mouselook::read_guest_float(mem, cam + 68);
  float& cum_pitch = fable2::mouselook::cum_pitch();
  const float pmin = cfg.mouse_look_pitch_floor - native_orbit;
  const float pmax = cfg.mouse_look_pitch_ceil - native_orbit;
  if (cum_pitch > pmax) cum_pitch = pmax;
  if (cum_pitch < pmin) cum_pitch = pmin;
  const float yaw = fable2::mouselook::cum_yaw();
  const float pitch = fable2::mouselook::cum_pitch();

  if (cfg.mouse_look_legacy_orbit) {
    // ---- LEGACY 2-axis external orbit (below) ----
    if (yaw == 0.0f && pitch == 0.0f) return;
    legacy_orbit_2axis(mem, cam, cfg, eye, yaw, pitch);
    return;
  }

  // ---- NATIVE single-DOF orbit (offset the target angle; not the default) ----
  // Offset the target orbit angle (cam+68, the same angle the right stick
  // drives) by the cumulative mouse pitch. The game's own easing then drives
  // cam+44 (the current angle) toward it, so the orbit radius, easing, and
  // banking are all the game's native behavior. Restoring the +1.5/+5.0 coupling
  // keeps a72/a76 consistent with a68 (they're rigidly coupled in the native
  // orbit).
  if (pitch == 0.0f) return;  // at rest: leave the target pure-native
  const float t = native_orbit + pitch;
  fable2::mouselook::write_guest_float(mem, cam + 68, t);
  fable2::mouselook::write_guest_float(mem, cam + 72, t + 1.5f);
  fable2::mouselook::write_guest_float(mem, cam + 76, t + 5.0f);

  // Diagnostic: log to determine the controller's commit cadence and confirm no
  // compounding. cam+68_pre is the value we read (should be the native target);
  // cam+68_post is what we wrote; cam+44 is the current (eased) angle. If cam+68
  // stays ~native+pitch and cam+44 converges to it -> stable. If cam+68 grows
  // unbounded -> the controller freezes cam+68 and we must reconstruct the base.
  if (cfg.mouse_look_camera_diag) {
    static int orb_logged = 0;
    if (orb_logged < 400) {
      REXSYS_INFO(
          "[ml-orb] cam=0x{:08X} a44={:.4f} a68_pre={:.4f} a68_post={:.4f} "
          "cum_pitch={:.4f}",
          cam, fable2::mouselook::read_guest_float(mem, cam + 44), native_orbit, t,
          pitch);
      ++orb_logged;
    }
  }
#endif
}

// Legacy 2-axis external orbit (mouse_look_legacy_orbit = true). Rotates the
// world-space eye (cam+144) around a low-passed hero pivot and writes a
// roll-free look-at orientation to the rotation-state quaternions (cam+160/
// 176), with an independent yaw. Superseded by the native single-DOF path
// above but kept as an opt-in fallback.
static void legacy_orbit_2axis(rex::memory::Memory* mem, uint32_t cam,
                               const fable2::config::Values& cfg,
                               const fable2::mouselook::Vec3& eye, float yaw,
                               float pitch) {
  using Quat = fable2::mouselook::Quat;
  using Vec3 = fable2::mouselook::Vec3;
  using fable2::mouselook::axis_quat_vec;
  using fable2::mouselook::axis_vec;
  using fable2::mouselook::quat_from_basis;
  using fable2::mouselook::quat_mul;
  using fable2::mouselook::quat_rotate_vec;
  using fable2::mouselook::vec3_cross;
  using fable2::mouselook::vec3_normalize;

  // Hero pivot: the eye looks along the camera's local +Y (forward) toward the
  // hero, so the hero sits `camera_distance` in front of the eye. Use the FRESH
  // estimate every frame (no low-pass) — the low-pass (old alpha 0.15) lagged the
  // pivot behind the hero, which was the "floaty" feel.
  const Quat q_tgt = fable2::mouselook::read_guest_quat(
      mem, cam + uint32_t(cfg.camera_rot_tgt_off));
  const Vec3 fwd = quat_rotate_vec(q_tgt, Vec3{0.0f, 1.0f, 0.0f});
  const float dist = cfg.camera_distance;
  const Vec3 pivot{eye.x + fwd.x * dist, eye.y + fwd.y * dist,
                   eye.z + fwd.z * dist};

  // 1-axis pitch orbit, matching the native right stick (which is pitch-only,
  // single-DOF): rotate the eye around the hero. The mouse x (yaw) is
  // intentionally ignored so the orbit matches the native look behavior; the
  // mouse y (pitch) drives the camera up/down around the hero exactly as the
  // right stick does.
  //
  // The pitch axis is the camera's OWN right axis (derived from its orientation
  // quaternion), not the world X axis: the camera faces at an angle, so the
  // world X axis would swing the eye in a slanted circle (up-and-to-the-left)
  // instead of straight up/down.
  const Vec3 cam_right = quat_rotate_vec(q_tgt, Vec3{1.0f, 0.0f, 0.0f});
  // Negate the pitch so mouse-down orbits the camera up and mouse-up orbits it
  // down (the direction the user wants).
  const Quat R = axis_quat_vec(cam_right, -pitch);
  const Vec3 o{eye.x - pivot.x, eye.y - pivot.y, eye.z - pivot.z};
  const Vec3 o_rot = quat_rotate_vec(R, o);
  const Vec3 eye_new{pivot.x + o_rot.x, pivot.y + o_rot.y, pivot.z + o_rot.z};
  fable2::mouselook::write_guest_vec3(mem, cam + uint32_t(cfg.camera_eye_off), eye_new);

  // Orientation: a clean look-at from the new eye toward the hero, with world-up
  // as up. Building it from scratch (instead of rotating the follow logic's own
  // orientation, which carries a slight bank/tilt) removes the in-plane twist
  // that shows up on up/down. right/forward/up form the camera's local X/Y/Z.
  Vec3 forward = vec3_normalize(
      Vec3{pivot.x - eye_new.x, pivot.y - eye_new.y, pivot.z - eye_new.z});
  Vec3 right = vec3_cross(forward, Vec3{0.0f, 0.0f, 1.0f});
  if (fable2::mouselook::vec3_len(right) < 1e-4f)
    right = vec3_cross(forward, Vec3{1.0f, 0.0f, 0.0f});  // forward ~ vertical
  right = vec3_normalize(right);
  const Vec3 up = vec3_cross(right, forward);
  const Quat q_new = quat_from_basis(right, forward, up);
  fable2::mouselook::write_guest_quat(
      mem, cam + uint32_t(cfg.camera_rot_tgt_off), q_new);
  fable2::mouselook::write_guest_quat(
      mem, cam + uint32_t(cfg.camera_rot_cur_off), q_new);
}

// High Tick Rate Stairs & Ledge Fix (Xenia patch by Ultra, be32 0x82E41F54 =
// `li r11, 1`). hkpCharacterProxy::integrate (sub_82E41E00) loads its substep
// count into r11 at 0x82E41F54; with the LF tick doubled, more than one
// substep wedges the hero on stairs and ledges. Cap it at 1.
void fable2_hook_tick_rate_stairs_fix(PPCRegister& r11) {
  static const bool enabled = [] {
    const auto& cfg = fable2::config::Get();
    const bool on = cfg.high_tick_rate && cfg.high_tick_rate_stairs_fix;
    if (on) {
      REXSYS_INFO("[tick-rate] character proxy capped to 1 substep (stairs/ledge fix)");
    }
    return on;
  }();
  if (enabled) {
    r11.u64 = 1;
  }
}

// Disable Texture Morphing (Xenia Canary patch by Guy, be16 0x8220EF10 =
// 0x4280). Runs before `beq cr6, 0x8220F08C` in sub_8220EC00 and takes the
// branch unconditionally, skipping the TextureMorphs block. On the Vulkan
// backend the realtime morph output is not picked up, so morphed hero/dog
// skins come out pure black; the base textures render correctly.
bool fable2_hook_disable_texture_morphing() {
  static const bool enabled = [] {
    const bool on = fable2::config::Get().disable_texture_morphing;
    if (on) {
      REXSYS_INFO("[texture-morph] texture morphing disabled (Xenia patch port)");
    }
    return on;
  }();
  return enabled;
}

// Realtime Texture Morphing (hero/dog black textures without CPU readback;
// plans/hero-dog-realtime-texture-morphing.md). sub_82A76018 turns each
// texture morph request into a morph job and copies the request's
// RealTimeTextureMorphing byte with `lbz r9, 0x20(r30)` at 0x82A7607C. With
// it set, the morph renderer (sub_82A69728) draws straight into the final
// uncompressed texture and builds its mips on the GPU, instead of resolving to
// a scratch texture that the CPU reads back and DXT-compresses (that CPU read
// is what returns black on a split-memory host).
void fable2_hook_realtime_texture_morphing(PPCRegister& r9) {
  if (!fable2::config::Get().realtime_texture_morphing) {
    return;
  }
  static bool logged = false;
  if (!logged) {
    logged = true;
    REXSYS_INFO("[texture-morph] building hero/dog textures in realtime (GPU) mode");
  }
  r9.u64 = 1;
}
