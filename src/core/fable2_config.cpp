// fable_2 - ReXGlue Recompiled Project
//
// fable2_config.toml: the recomp's own user config (separate from the ReXGlue
// SDK cvar config fable_2.toml). Look for it next to the exe, create it with
// defaults if missing, parse it, and expose the values via Get().
//
// Growth model: add a member to Values (with its default), read it in Load()
// with one of the Read* helpers, and document the key in the template below.
// Missing keys -> built-in default. Wrong type -> warning + default. Unknown
// sections -> warning. A broken file -> error + dialog + defaults (the game
// still runs), so a hand edit can never wedge the launch.

#include "fable2_config.h"

#include <array>
#include <format>
#include <fstream>

#include <rex/logging/macros.h>
#include <rex/system.h>

#include <toml++/toml.hpp>

namespace fable2::config {
namespace {

Values g_values;

// Kept in sync with config/fable2_config.toml (staged next to the exe by the
// build); this copy is what a first launch writes out.
const char kTemplate[] =
R"TOML_EOF(# =============================================================================
# fable2_config.toml - Fable 2 recomp user configuration
# =============================================================================
#
# Loaded by fable_2.exe on every launch, from the directory the exe lives in.
# If the file is missing, the game recreates it at startup with these
# defaults (the embedded template in src/core/fable2_config.cpp must stay in sync
# with this file).
#
# - Human-readable TOML: # starts a comment, sections are [bracketed].
# - Missing keys are fine: the game falls back to its built-in defaults.
# - Unknown sections/keys are ignored (a warning is logged for unknown
#   sections), so the file can grow over time without breaking the game.
# - A syntax error does not stop the game: it is logged to logs/, a dialog
#   is shown, and the built-in defaults are used instead.
#
# NOTE: this is the recomp's own config. fable_2.toml (also next to the exe)
# is the ReXGlue SDK cvar config - a different file.
# =============================================================================

[general]
# Bumped when this file's format gains new required structure. The game
# tolerates any version it does not know about.
# Default: 1
config_version = 1

[input]
# Host keyboard/mouse -> guest gamepad map: "Key:Button,Key:Button,..." (one
# line; do not add spaces or newlines inside the string). Key = Windows
# virtual-key name (see README.md, Keyboard controls) or a mouse button
# (LMB/RMB/MMB, or XMB1/XMB2 for the side buttons); Button = A/B/X/Y, LB/RB,
# LT/RT, Up/Down/Left/Right, Pause, Select, L3/R3, StickUp/StickDown/
# StickLeft/StickRight. Empty string disables the keyboard gamepad.
# --keyboard_gamepad_map, REX_* environment variables and fable_2.toml still
# take priority over this value; the F3 console can change it live.
# Default: the built-in layout
#   LMB:X,RMB:Y,MMB:B,Shift:A,E:A,Q:LT,Tab:RB,R:RT,Z:LB,B:B,
#   W:StickUp,S:StickDown,A:StickLeft,D:StickRight,Escape:Pause,M:Select,
#   Up:Up,Down:Down,Left:Left,Right:Right
keyboard_gamepad_map = "LMB:X,RMB:Y,MMB:B,Shift:A,E:A,Q:LT,Tab:RB,R:RT,Z:LB,B:B,W:StickUp,S:StickDown,A:StickLeft,D:StickRight,Escape:Pause,M:Select,Up:Up,Down:Down,Left:Left,Right:Right"

# Map mouse movement to the guest right stick (camera look): sweep to look,
# stop to stop. false disables mouse look entirely.
# Default: true
mouse_look = true

# Mouse-look sensitivity slider (1..4096; 256 = 1x). Shared by BOTH look modes:
# for the legacy right-stick mapping it sets the stick units per pixel; for
# direct mouse look it multiplies mouse_look_yaw_sens / mouse_look_pitch_sens.
# Larger = more sensitive. Default: 256
mouse_look_scale = 256

# Direct mouse-to-camera look: apply mouse movement straight to the active
# camera's orientation (desktop-FPS style, no stick rate cap) instead of
# writing it into the pad's right stick. false = legacy right-stick mapping.
# EXPERIMENTAL (default false; may feel wrong in some scenes).
direct_mouse_look = false

# Fallback for direct_mouse_look. true (default) = use the OLD re-implemented
# 2-axis external orbit (eye rotated around a low-passed hero pivot + look-at
# quaternion, independent yaw); a working hand-rolled orbit. false = offset the
# game's target orbit angle (cam+68) and let the native easing drive the camera
# (the intended 1:1 path, still being tuned -- see plans/plan-direct-mouse-1to1.md).
mouse_look_legacy_orbit = true

# Camera-look sensitivity: radians of camera rotation per host mouse pixel
# (signed; make it negative to flip the axis direction).
mouse_look_yaw_sens = -0.0022
mouse_look_pitch_sens = -0.0022
# Native look-orbit angle limits, in the game's own orbit-angle scale (the
# target angle at camera+68 that the right-stick look integrates and clamps).
# A full-range stick test shows the native orbit clamps this angle to
# [floor, ceil]: floor = camera low / looking up, ceil = camera high / looking
# down. The mouse adds its pitch on top of the native angle and is clamped so
# the TOTAL (native + mouse) stays within [floor, ceil] — so the mouse and the
# stick share one orbit and one set of bounds (no compounding past the limits).
mouse_look_pitch_floor = 1.458
mouse_look_pitch_ceil = 4.067
# Camera local axis each look direction rotates around (0=X, 1=Y, 2=Z). The
# camera looks along its local Y, so a correct orbit turns around the local up
# (Z) and pitches around the local right (X). Negate the matching *_sens to
# flip a look direction.
mouse_look_yaw_axis = 2
mouse_look_pitch_axis = 0
# 1:1 native right-stick injection: RightStickLookProcess_832BB018 adds the
# per-frame mouse Y delta (scaled by this factor, in stick u16 units per host
# mouse pixel) to the right-stick Y u16 so the native orbit pipeline runs with
# combined stick + mouse input (identical bounds to the stick). The stick is an
# int16 centered at 0 with full deflection ~32768, so 512 px = a full
# deflection at the default 64. Negative flips the direction.
mouse_look_stick_scale = 64
# 1:1 native right-stick injection. true = the RightStickLookProcess_832BB018
# hotfunc override adds the mouse Y delta to the right-stick Y u16 so the game's
# OWN orbit/easing/clamp runs with combined stick + mouse input (identical
# bounds to the stick). The legacy direct-mouse hook then only resets the delta
# (no orbit). false = the legacy direct-mouse hook's orbit runs (external or
# native target offset, per mouse_look_legacy_orbit).
mouse_look_native_inject = false
# Log the rest (pre-injection) and injected right-stick-Y u16 (capped) to
# calibrate mouse_look_stick_scale. Off by default.
mouse_look_stick_diag = false

# Active-camera (CECCamera) field offsets (bytes from the camera object). The
# look-orbit is reproduced by rotating BOTH the orientation quaternions AND the
# world-space eye position around the hero, exactly like the game's own
# right-stick look. The rotation-state quaternions (target +160, current +176,
# x,y,z,w) keep the camera facing the hero; the eye position (+144) is the
# world-space camera position that physically sweeps around the hero. The hero
# pivot is eye + forward * camera_distance (forward = camera local +Y). Verify
# with mouse_look_camera_diag and adjust if a look axis or radius is off (no
# rebuild needed).
camera_rot_tgt_off = 160
camera_rot_cur_off = 176
camera_eye_off = 144
# "Is this the hero-orbit camera?" flag. The hook only orbits the hero (player)
# camera (flag 1.0) and skips cinematic/cutscene cameras (flag 0.0), so
# cutscenes and scripted shots play normally instead of being hijacked.
camera_hero_flag_off = 108
# Distance from the hero to the camera eye (= orbit radius). The native stick
# orbit's horizontal radius is ~3.1; tune so the mouse orbit matches.
camera_distance = 3.1

# Log the active camera's orientation fields once, the first frame the mouse
# moves, to confirm the offsets above. Default: false
mouse_look_camera_diag = false

# Dump EVERY 4-byte field of the active camera object (offset 0..260) plus the
# orientation quaternions, once per frame (capped), to the [ml-dump] log. Used
# to find which fields the game's own right-stick look changes. Default: false
mouse_look_camera_dump = false

[patches]
# Toggles for the recomp-level (mid-asm hook) patches, consulted at runtime
# by the hook bodies (src/core/fable2_hooks.cpp) - no rebuild needed.
# Unlock Website Items (mid-asm hooks fable2_hook_website_g1/g1b/grantnew;
# Xenia "Unlock Website Items" by Guy): forces the website-registration gates
# AND the grant-method result in the Guild-chest item getter so the website
# items are granted at save load. false = locked (original).
# Default: true
unlock_website = true

# Unlock Collectors Edition Content (mid-asm hooks fable2_hook_ce_g1/g1b/
# grantavail; Xenia "Unlock Collectors Edition Content" by Guy): forces the
# CE-registration gates AND the grant-method result in the CE-content getter so
# the CE items are granted at save load. false = locked (original).
# Default: true
unlock_ce = true

# Skip Intro Videos (mid-asm hook fable2_hook_skip_intro_videos; just-harry's
# "Skip intro videos" patch from his Unofficial Xenia femtofork): skips the
# Microsoft and Lionhead logo videos at boot. false = videos play (original).
# Default: false
skip_intro_videos = false

# High Tick Rate (Xenia patch by Guy, plus mid-asm hook
# fable2_hook_high_tick_rate_skip_store): doubles the game's LF tick from
# 15 Hz to 30 Hz. Vastly improves in-game UI framerate and input delay.
# Use together with higher_hf_tick_rate. false = original 15 Hz.
# Measured on the AYN Thor (2026-10-28): 25.5 -> 25.0 FPS, about 2% cost.
# Default: true on Android, false elsewhere (leave commented for the default)
# high_tick_rate = true

# Higher HF Tick Rate (Xenia patch by Ultra, plus mid-asm hook
# fable2_hook_high_hf_tick_rate_skip_store): doubles the HF tick from 30 Hz to
# 60 Hz. The game expects HF to run at twice LF; with high_tick_rate alone
# both run at 30 Hz and cloth physics misbehaves. Turn this on whenever
# high_tick_rate is on. Requires high_tick_rate: ignored when that is false.
# false = original 30 Hz.
# Default: true on Android, false elsewhere (leave commented for the default)
# higher_hf_tick_rate = true

# High Tick Rate Stairs & Ledge Fix (Xenia patch by Ultra, mid-asm hook
# fable2_hook_tick_rate_stairs_fix): caps the character controller to one
# physics substep so the faster LF tick does not wedge the hero on stairs and
# ledges. Only acts while high_tick_rate is true.
# Default: true
high_tick_rate_stairs_fix = true

# Disable Motion Blur (mid-asm hook fable2_hook_disable_motion_blur): the
# camera's full-screen motion blur amount (what scripts set via Camera.SetBlur)
# is zeroed on its way to the renderer each frame. The camera's own value is
# untouched. The log shows "[motion-blur] game requested ..." the first time
# the game asks for blur. false = blur as in the original game.
# Default: false
disable_motion_blur = false

# Realtime Texture Morphing (mid-asm hook fable2_hook_realtime_texture_morphing):
# the hero's and dog's morphed skin textures are rendered straight into their
# final texture on the GPU (the game's own RealTimeTextureMorphing mode)
# instead of being read back to the CPU and DXT-compressed. This fixes the
# black hero/dog without hero_dog_texture_readback. The textures are stored
# uncompressed, so they use a few MB more memory. See
# plans/hero-dog-realtime-texture-morphing.md. false = original baked mode.
# Default: true
realtime_texture_morphing = true

# Disable Texture Morphing (port of the Xenia Canary patch by Guy, be16
# 0x8220EF10 = 0x4280; mid-asm hook fable2_hook_disable_texture_morphing):
# skips the TextureMorphs block, so the hero and dog use their base skin
# textures (no aging/morality/scar morphs) instead of morphed textures that
# render pure black on the Vulkan backend. true = skip morphing.
# Default: true on Android, false elsewhere (leave commented for the default)
# disable_texture_morphing = true

# Force a CPU readback of the render-to-texture resolve that regenerates the
# hero/dog face+skin texture, so the character does not render black on a
# split-memory host (see plans/hero-dog-texture-readback.md). Approach + the
# guest base 0x12704000 come from just-harry's Unofficial Xenia femtofork for
# Fable II. Seeds the SDK cvar readback_resolve_force_addresses; readback then
# happens only for that resolve, not every frame. Superseded by
# realtime_texture_morphing; only needed (and only effective) with that off.
# false = original black bug.
# Default: false
hero_dog_texture_readback = false

# Dog Fur Depth Bias: the dog's fur is drawn as 15 thin layers over its body
# that rely on exact depth ties in the Xbox 360's 24-bit depth buffer. The PC's
# 32-bit depth breaks those ties at random, which shows as flickering, blocky
# patches on the dog (worse at higher internal resolutions). This gives the fur
# draws (pixel shader 014F8A02DB7B19CA) a small depth bias so the ties pass as
# on the console. Seeds the SDK cvars depth_bias_shader(_units).
# false = no bias (flickering patches).
# Default: true
dog_fur_depth_bias = true

[perf]
# NtYieldExecution batching for the hot-function overrides (see
# src/core/hotfunc/hotfunc_yield.h): every Nth guest yield does the real
# SwitchToThread; the others take a full memory fence. The work-loop yield
# was the single biggest cost in the 475 render chain; on multi-core hosts
# the yield is only a politeness hint, so batch it. 1 = original (yield
# every call); 0 = never yield; larger = fewer context switches.
# Default: 1
hotfunc_yield_every = 1
)TOML_EOF";

std::string_view TypeName(toml::node_type t) {
  return toml::impl::node_type_friendly_names[static_cast<std::size_t>(t)];
}

// Read a typed key from a section. Missing key -> default (silent); the key
// exists but is the wrong kind -> warning + default.
template <typename T>
T Read(const toml::table& section, std::string_view section_name,
       std::string_view key, std::string_view type_name, T default_value) {
  const toml::path key_path{key};
  const auto view = section[key_path];
  if (!view) return default_value;
  if (auto v = view.value_exact<T>()) return static_cast<T>(*v);
  REXSYS_WARN(
      "[fable2-config] [{}] {}: expected {}, got {}; using built-in default",
      section_name, key, type_name, TypeName(view.type()));
  return default_value;
}

}  // namespace

bool Load(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) {
      REXSYS_ERROR("[fable2-config] could not create {}", path.string());
      return false;
    }
    out << kTemplate;
    REXSYS_INFO("[fable2-config] {} not found; created with defaults",
                path.string());
  }

  // toml++ (v3, exceptions on) returns the table directly and throws a
  // toml::parse_error on a broken file.
  toml::table root;
  try {
    root = toml::parse_file(path.native());
  } catch (const toml::parse_error& err) {
    REXSYS_ERROR("[fable2-config] {} failed to parse at line {}, column {}: {}",
                 path.string(), err.source().begin.line, err.source().begin.column,
                 err.description());
    const std::string msg = std::format(
        "fable2_config.toml could not be parsed\n\n"
        "{}\n\n"
        "File: {}\n"
        "The game will continue with built-in defaults. Fix the file (or "
        "delete it to recreate the defaults) and start again.",
        err.description(), path.string());
    rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Error, msg);
    return false;
  }

  // Warn about unknown top-level sections (usually a typo or a file written
  // for a newer build).
  static constexpr std::array<std::string_view, 4> kKnownSections = {
      "general", "input", "patches", "perf"};
  for (const auto& [key, value] : root) {
    const bool known =
        std::ranges::find(kKnownSections, key.str()) != kKnownSections.end();
    if (!known && value.is_table())
      REXSYS_WARN("[fable2-config] unknown section [{}] ignored", key.str());
  }

  Values values;
  const toml::path general_path{"general"};
  const auto general = root[general_path];
  if (general.is_table()) {
    values.config_version = static_cast<int32_t>(
        Read<int64_t>(*general.as_table(), "general", "config_version",
                      "integer", 1));
  }
  const toml::path input_path{"input"};
  const auto input = root[input_path];
  if (input.is_table()) {
    const toml::table& input_table = *input.as_table();
    values.keyboard_gamepad_map = Read<std::string>(
        input_table, "input", "keyboard_gamepad_map", "string",
        std::string{kDefaultKeyboardGamepadMap});
    // Defaults come from Values itself (single source of truth).
    values.mouse_look =
        Read<bool>(input_table, "input", "mouse_look", "boolean",
                   values.mouse_look);
    values.mouse_look_scale = static_cast<int32_t>(Read<int64_t>(
        input_table, "input", "mouse_look_scale", "integer",
        values.mouse_look_scale));
    values.direct_mouse_look =
        Read<bool>(input_table, "input", "direct_mouse_look", "boolean",
                   values.direct_mouse_look);
    values.mouse_look_legacy_orbit =
        Read<bool>(input_table, "input", "mouse_look_legacy_orbit", "boolean",
                   values.mouse_look_legacy_orbit);
    values.mouse_look_yaw_sens = static_cast<float>(
        Read<double>(input_table, "input", "mouse_look_yaw_sens", "number",
                     values.mouse_look_yaw_sens));
    values.mouse_look_pitch_sens = static_cast<float>(
        Read<double>(input_table, "input", "mouse_look_pitch_sens", "number",
                     values.mouse_look_pitch_sens));
    values.mouse_look_pitch_floor = static_cast<float>(
        Read<double>(input_table, "input", "mouse_look_pitch_floor", "number",
                     values.mouse_look_pitch_floor));
    values.mouse_look_pitch_ceil = static_cast<float>(
        Read<double>(input_table, "input", "mouse_look_pitch_ceil", "number",
                     values.mouse_look_pitch_ceil));
    values.mouse_look_yaw_axis = static_cast<int32_t>(Read<int64_t>(
        input_table, "input", "mouse_look_yaw_axis", "integer",
        values.mouse_look_yaw_axis));
    values.mouse_look_pitch_axis = static_cast<int32_t>(Read<int64_t>(
        input_table, "input", "mouse_look_pitch_axis", "integer",
        values.mouse_look_pitch_axis));
    values.mouse_look_stick_scale = static_cast<int32_t>(Read<int64_t>(
        input_table, "input", "mouse_look_stick_scale", "integer",
        values.mouse_look_stick_scale));
    values.mouse_look_stick_diag = static_cast<bool>(
        Read<bool>(input_table, "input", "mouse_look_stick_diag", "boolean",
                   values.mouse_look_stick_diag));
    values.mouse_look_native_inject = static_cast<bool>(
        Read<bool>(input_table, "input", "mouse_look_native_inject", "boolean",
                   values.mouse_look_native_inject));
    values.camera_rot_tgt_off = static_cast<int32_t>(Read<int64_t>(
        input_table, "input", "camera_rot_tgt_off", "integer",
        values.camera_rot_tgt_off));
    values.camera_rot_cur_off = static_cast<int32_t>(Read<int64_t>(
        input_table, "input", "camera_rot_cur_off", "integer",
        values.camera_rot_cur_off));
    values.camera_eye_off = static_cast<int32_t>(Read<int64_t>(
        input_table, "input", "camera_eye_off", "integer",
        values.camera_eye_off));
    values.camera_hero_flag_off = static_cast<int32_t>(Read<int64_t>(
        input_table, "input", "camera_hero_flag_off", "integer",
        values.camera_hero_flag_off));
    values.camera_distance = static_cast<float>(Read<double>(
        input_table, "input", "camera_distance", "number",
        values.camera_distance));
    values.mouse_look_camera_diag =
        Read<bool>(input_table, "input", "mouse_look_camera_diag", "boolean",
                   values.mouse_look_camera_diag);
    values.mouse_look_camera_dump =
        Read<bool>(input_table, "input", "mouse_look_camera_dump", "boolean",
                   values.mouse_look_camera_dump);
  }
  const toml::path patches_path{"patches"};
  const auto patches = root[patches_path];
  if (patches.is_table()) {
    const toml::table& patches_table = *patches.as_table();
    values.unlock_website = Read<bool>(patches_table, "patches",
                                       "unlock_website", "boolean",
                                       values.unlock_website);
    values.unlock_ce = Read<bool>(patches_table, "patches", "unlock_ce",
                                  "boolean", values.unlock_ce);
    values.skip_intro_videos =
        Read<bool>(patches_table, "patches", "skip_intro_videos", "boolean",
                   values.skip_intro_videos);
    values.high_tick_rate =
        Read<bool>(patches_table, "patches", "high_tick_rate", "boolean",
                   values.high_tick_rate);
    values.higher_hf_tick_rate =
        Read<bool>(patches_table, "patches", "higher_hf_tick_rate", "boolean",
                   values.higher_hf_tick_rate);
    values.high_tick_rate_stairs_fix =
        Read<bool>(patches_table, "patches", "high_tick_rate_stairs_fix", "boolean",
                   values.high_tick_rate_stairs_fix);
    values.disable_motion_blur =
        Read<bool>(patches_table, "patches", "disable_motion_blur", "boolean",
                   values.disable_motion_blur);
    values.realtime_texture_morphing =
        Read<bool>(patches_table, "patches", "realtime_texture_morphing", "boolean",
                   values.realtime_texture_morphing);
    values.disable_texture_morphing =
        Read<bool>(patches_table, "patches", "disable_texture_morphing", "boolean",
                   values.disable_texture_morphing);
    values.hero_dog_texture_readback =
        Read<bool>(patches_table, "patches", "hero_dog_texture_readback", "boolean",
                   values.hero_dog_texture_readback);
    values.dog_fur_depth_bias =
        Read<bool>(patches_table, "patches", "dog_fur_depth_bias", "boolean",
                   values.dog_fur_depth_bias);
  }
  const toml::path perf_path{"perf"};
  const auto perf = root[perf_path];
  if (perf.is_table()) {
    const toml::table& perf_table = *perf.as_table();
    values.hotfunc_yield_every = static_cast<int32_t>(Read<int64_t>(
        perf_table, "perf", "hotfunc_yield_every", "integer",
        values.hotfunc_yield_every));
  }

  g_values = values;
  REXSYS_INFO("[fable2-config] loaded {} (config_version={})", path.string(),
              values.config_version);
  return true;
}

const Values& Get() { return g_values; }

const char* DefaultTemplate() { return kTemplate; }

}  // namespace fable2::config
