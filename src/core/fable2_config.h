// fable_2 - ReXGlue Recompiled Project
//
// Recomp user configuration (fable2_config.toml), separate from the ReXGlue
// SDK cvar config (fable_2.toml). Loaded once at startup from next to the
// exe; created with defaults if missing.

#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

namespace fable2::config {

// Default host keyboard -> guest gamepad map (see src/input/keyboard_gamepad.h).
// It lives here, in the config layer, so the mapping is editable in
// fable2_config.toml instead of being baked into the binary; the
// keyboard_gamepad_map cvar itself defaults to empty and is seeded from
// here at startup (see Fable2App::OnPostInitLogging).
inline constexpr std::string_view kDefaultKeyboardGamepadMap =
    "LMB:X,RMB:Y,MMB:B,Shift:A,E:A,"
    "Q:LT,Tab:RB,R:RT,Z:LB,B:B,"
    "W:StickUp,S:StickDown,A:StickLeft,D:StickRight,"
    "Escape:Pause,M:Select,"
    "Up:Up,Down:Down,Left:Left,Right:Right";

// Parsed values from fable2_config.toml. To add a setting:
//   1. Add a member here with its built-in default.
//   2. Read it in Load() below (ReadInt/ReadBool/ReadString helpers).
//   3. Document the key in the template (DefaultTemplate /
//      config/fable2_config.toml - keep the two in sync).
// Missing keys keep their defaults; wrong types log a warning and keep the
// default, so the file can grow without breaking the game.
struct Values {
  // [general]
  int32_t config_version = 1;
  // [input]
  std::string keyboard_gamepad_map{std::string{kDefaultKeyboardGamepadMap}};
  // Keep in sync with the cvar defaults in main.cpp (mouse_look,
  // mouse_look_scale); the cvar keeps its compiled default as a fallback so
  // an empty/missing config can never wedge input.
  bool mouse_look = true;
  // Shared mouse-look sensitivity slider (1..4096, 256 = 1x): legacy stick
  // units-per-pixel, and a multiplier on mouse_look_*_sens for direct look.
  int32_t mouse_look_scale = 256;  // range 1..4096 (cvar constraint)
  // Direct mouse-to-camera look (see src/input/mouse_look.h). EXPERIMENTAL.
  // When true, mouse movement bypasses the synthetic pad's right stick and is
  // applied directly to the active camera's orientation by
  // fable2_hook_direct_mouse_look, giving a desktop-FPS "move the mouse that
  // far, turn that far" feel with no rate cap. When false, the legacy
  // right-stick mapping (mouse_look_scale) is used. Defaults to false but is
  // still considered experimental.
  bool direct_mouse_look = false;
  // Fallback for direct_mouse_look. When true (and direct_mouse_look is on) the
  // OLD re-implemented orbit runs: the hook at CECCameraUpdateFrame rotates the
  // world-space eye around a low-passed hero pivot and writes a look-at
  // quaternion (2-axis orbit with independent yaw). This is a working but
  // hand-rolled orbit (not 1:1 with the stick). When false the direct-mouse
  // path offsets the game's target orbit angle (cam+68) and lets the native
  // easing drive the camera -- the intended 1:1 path, but it is still being
  // tuned (see plans/plan-direct-mouse-1to1.md), so the default is the legacy
  // orbit until that path is confirmed to move the eye.
  bool mouse_look_legacy_orbit = true;
  // Camera-look sensitivity: radians of camera rotation per host mouse pixel.
  // Signed so an axis direction can be flipped without a code change.
  float mouse_look_yaw_sens = -0.0022f;   // horizontal mouse -> camera yaw
  float mouse_look_pitch_sens = -0.0022f;  // vertical mouse -> camera pitch
  // Native look-orbit angle limits, in the game's own orbit-angle scale (the
  // target angle at camera+68, which the right-stick look integrates and clamps).
  // A full-range stick test (hero stationary) shows the native orbit clamps this
  // angle to [floor, ceil]: floor = camera low / looking up, ceil = camera high /
  // looking down, with the rest position in between (~2.81 at spawn).
  //
  // The mouse adds its cumulative pitch ON TOP of the native angle, and is
  // clamped so the TOTAL (native orbit + mouse) stays within [floor, ceil]. This
  // makes the mouse and the stick share one orbit and one set of bounds, so the
  // two can't compound past the native limits (the mouse's headroom shrinks as
  // the stick approaches a limit).
  float mouse_look_pitch_floor = 1.458f;  // native floor angle (camera low)
  float mouse_look_pitch_ceil = 4.067f;   // native ceiling angle (camera high)
  // Camera local axis each look direction rotates around (0=X, 1=Y, 2=Z). The
  // camera looks along its local Y, so a correct orbit turns around the local up
  // (Z) and pitches around the local right (X). Negate the matching *_sens to flip
  // a look direction. No rebuild needed to change these.
  int32_t mouse_look_yaw_axis = 2;   // default Z (local up -> orbit)
  int32_t mouse_look_pitch_axis = 0; // default X (local right -> pitch)
  // 1:1 native right-stick injection: RightStickLookProcess_832BB018 (the game's
  // active per-frame right-stick-Y consumer) reads the stick Y u16 and drives the
  // camera orbit. The hotfunc override adds the per-frame mouse Y delta, scaled by
  // this factor, to that u16 so the native orbit/easing/clamp pipeline runs with
  // combined stick + mouse input (identical bounds to the stick). Scale is in
  // stick u16 units per host mouse pixel. The stick is an int16 centered at 0 with
  // full deflection at ~32768, so 512 px of mouse travel = a full deflection at
  // the default 64. Signed: negative flips the direction.
  int32_t mouse_look_stick_scale = 64;
  // 1:1 native right-stick injection mode. When true (and mouse_look +
  // direct_mouse_look are on) the RightStickLookProcess_832BB018 hotfunc override
  // adds the per-frame mouse Y delta (scaled by mouse_look_stick_scale) to the
  // right-stick Y u16, so the game's OWN orbit/easing/clamp pipeline runs with
  // combined stick + mouse input. The mouse shares the stick's exact orbit
  // bounds (no compounding past the native limits) because it feeds the same
  // u16 the stick does. The legacy direct-mouse hook (fable2_hook_direct_mouse_
  // look) resets the mouse delta once per frame but skips its orbit logic in this
  // mode. Off by default (the legacy external orbit remains the default).
  bool mouse_look_native_inject = false;
  // Log the rest (pre-injection) and injected right-stick-Y u16 in the
  // RightStickLookProcess override, capped, to calibrate mouse_look_stick_scale.
  // Off by default.
  bool mouse_look_stick_diag = false;
  // Active-camera (CECCamera) field offsets used by the direct-mouse-look hook
  // (bytes from the camera object). The look-orbit is reproduced by rotating
  // BOTH the orientation quaternions AND the world-space eye position around the
  // hero, exactly like the game's own right-stick look:
  //   * camera_rot_tgt_off (160) / camera_rot_cur_off (176): the rotation-state
  //     orientation quaternions (x,y,z,w, |m|=1) the follow logic eases the view
  //     through. Rotated (pre-multiplied) by the orbit rotation so the camera
  //     keeps facing the hero.
  //   * camera_eye_off (144): the world-space camera EYE position (large values,
  //     e.g. ~204,~93,~55). When the stick orbits the camera (hero stationary)
  //     this sweeps a ~3.1-radius circle around the hero. Rotated around the
  //     hero pivot by the same orbit rotation so the eye physically moves (a
  //     true orbit rather than an in-place turn).
  // The hero pivot is derived as eye + forward * camera_distance, where forward
  // is the camera's local +Y rotated by the target orientation. All of these
  // can be tuned at runtime with no rebuild (see mouse_look_camera_diag).
  int32_t camera_rot_tgt_off = 160;  // rotation-state target (quaternion)
  int32_t camera_rot_cur_off = 176;  // rotation-state current (quaternion)
  int32_t camera_eye_off = 144;      // world-space eye position (x,y,z)
  // "Is this the hero-orbit camera?" flag offset. The direct-mouse-look hook
  // fires for EVERY CECCamera, including cinematic/cutscene cameras that the
  // game scripts on its own (opening movie, cut-in shots, ...). Orbiting those
  // fights the cutscene and makes the camera go wild. The hero (player)
  // camera has a flag of 1.0 at this offset; cutscene/secondary cameras have
  // 0.0. The hook only orbits the camera when this flag is >= 0.5, leaving
  // cinematic cameras untouched so cutscenes play normally.
  int32_t camera_hero_flag_off = 108;
  // Distance from the hero to the camera eye (= the orbit radius). The game's
  // native stick orbit has a horizontal radius of ~3.1 (fit from a live
  // capture); tune this so the mouse orbit radius matches the game's.
  float camera_distance = 3.1f;
  // Log the active camera's orientation fields once, the first frame the mouse
  // moves, to confirm the offset guesses above (and to spot which fields are
  // position vs. angle vs. the transform). Off by default.
  bool mouse_look_camera_diag = false;
  // Dump EVERY 4-byte field of the active camera object (offset 0..260) plus the
  // orientation quaternions, once per frame (capped), to the [ml-dump] log. Used
  // to empirically find which fields the game's own right-stick look changes so
  // the mouse look can drive the same field(s). Off by default.
  bool mouse_look_camera_dump = false;
  // [patches] - toggles for the recomp-level (mid-asm hook) patches. The
  // hook bodies consult these at runtime (src/core/fable2_hooks.cpp), so a patch
  // can be A/B'd with no rebuild. high_tick_rate / higher_hf_tick_rate also
  // drive the guest-image data writes in src/core/fable2_patches.cpp.
  // Unlock the Guild-chest items that were obtainable from the (now-dead)
  // Fable 2 website. Forces both the registration gates and the grant-method
  // result in GuildChest_GetWebsiteItem_8256E368 (hooks
  // fable2_hook_website_g1/g1b/grantnew in src/core/fable2_hooks.cpp).
  bool unlock_website = true;
  // Unlock the Collectors Edition chest content. Forces both the
  // registration gates and the grant-method result in
  // GuildChest_GetCEContent_824B3528 (hooks fable2_hook_ce_g1/g1b/grantavail
  // in src/core/fable2_hooks.cpp).
  bool unlock_ce = true;
  // Skip the Microsoft and Lionhead logo videos at boot (just-harry's "Skip
  // intro videos"; hook fable2_hook_skip_intro_videos in
  // src/core/fable2_hooks.cpp). false = original.
  bool skip_intro_videos = false;
  // [patches] high_tick_rate: LF tick 15 -> 30 Hz (Xenia "High Tick Rate" by
  // Guy; data write in src/core/fable2_patches.cpp + hook
  // fable2_hook_high_tick_rate_skip_store). false = original.
#if defined(__ANDROID__)
  bool high_tick_rate = true;  // input delay: game reads input at the LF tick
#else
  bool high_tick_rate = false;
#endif
  // [patches] higher_hf_tick_rate: HF tick 30 -> 60 Hz (Xenia "Higher HF Tick
  // Rate" by Ultra; data write in src/core/fable2_patches.cpp + hook
  // fable2_hook_high_hf_tick_rate_skip_store). Requires high_tick_rate.
#if defined(__ANDROID__)
  bool higher_hf_tick_rate = true;  // keeps the 2:1 HF:LF ratio with high_tick_rate
#else
  bool higher_hf_tick_rate = false;
#endif
  // [patches] high_tick_rate_stairs_fix: Xenia "High Tick Rate Stairs & Ledge
  // Fix" (Ultra): caps hkpCharacterProxy::integrate to 1 substep so the 30 Hz
  // LF tick does not wedge the hero on stairs and ledges (hook
  // fable2_hook_tick_rate_stairs_fix). Only acts with high_tick_rate on.
  bool high_tick_rate_stairs_fix = true;
  // [patches] disable_motion_blur: zero the full-screen motion blur amount the
  // camera passes to the renderer (hook fable2_hook_disable_motion_blur in
  // src/core/fable2_hooks.cpp). false = original.
  bool disable_motion_blur = false;
  // [patches] realtime_texture_morphing: build the hero/dog morphed skin
  // textures on the GPU (the game's own RealTimeTextureMorphing mode) instead
  // of CPU-compressing a resolved scratch texture, so they are not black on a
  // split-memory host (hook fable2_hook_realtime_texture_morphing in
  // src/core/fable2_hooks.cpp; plans/hero-dog-realtime-texture-morphing.md).
  // Replaces hero_dog_texture_readback. false = original.
  bool realtime_texture_morphing = true;
  // [patches] disable_texture_morphing: port of Xenia's "Disable Texture
  // Morphing" patch (Guy; be16 0x8220EF10 = 0x4280). Skips the TextureMorphs
  // block in sub_8220EC00, so the hero/dog keep their base skin textures
  // instead of morphed ones that come out black when the realtime mode is not
  // honoured by the host GPU backend (Vulkan). Hook
  // fable2_hook_disable_texture_morphing. Default on for Android only.
#if defined(__ANDROID__)
  bool disable_texture_morphing = true;
#else
  bool disable_texture_morphing = false;
#endif
  // Force a CPU readback of the render-to-texture resolve that (re)generates
  // the hero/dog face+skin texture, so the character does not render black on
  // a split-memory host (see plans/hero-dog-texture-readback.md). Approach + the
  // guest base 0x12704000 come from just-harry's Unofficial Xenia femtofork for
  // Fable II. This seeds the SDK cvar readback_resolve_force_addresses in
  // Fable2App; readback then happens only for that resolve, not every frame.
  // Superseded by realtime_texture_morphing (only takes effect with that off).
  bool hero_dog_texture_readback = false;
  // Give the dog's shell-fur layers (pixel shader 014F8A02DB7B19CA) a small
  // extra depth bias so they don't Z-fight with the body and with each other.
  // The fur relies on exact depth ties in the Xbox 360's 24-bit float depth
  // buffer (greater-or-equal test); the host's 32-bit depth breaks those ties
  // at random, which shows as flickering patches on the dog. Seeds the SDK
  // cvars depth_bias_shader(_units) in Fable2App. false = no bias.
  bool dog_fur_depth_bias = true;
  // [perf] - hot-function override tuning.
  // hotfunc_yield_every: NtYieldExecution batching factor for the hotfunc
  // overrides (see src/core/hotfunc/hotfunc_yield.h). Every Nth call does
  // the real SwitchToThread; the others take a full memory fence. 1 =
  // original (yield every call), 0 = never yield. Larger = fewer context
  // switches, less frequent CPU rotation to other guest threads.
  int32_t hotfunc_yield_every = 1;
};

// Load the config from `path`.
//   - File missing  -> writes the default template, then parses it (returns
//                      true).
//   - Parse failure -> logs the error, shows a dialog, falls back to the
//                      in-memory defaults (returns false).
// Unknown top-level sections log a warning; everything is otherwise ignored.
// Safe to call only after the SDK logging is initialized (OnPostInitLogging).
bool Load(const std::filesystem::path& path);

// Parsed values (in-memory defaults until Load() succeeds).
const Values& Get();

// The default template written when the file is missing. Must match
// config/fable2_config.toml in the repo (staged next to the exe by the build).
const char* DefaultTemplate();

}  // namespace fable2::config
