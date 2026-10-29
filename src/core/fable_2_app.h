// fable_2 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/cvar.h>
#include <rex/filesystem.h>

#include <rex/perf/counter.h>
#include <rex/rex_app.h>
#include <rex/system.h>
#include <rex/system/gpu_plugin.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <format>
#include <fstream>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "alloc_watch.h"
#include "dir_manifest_heal.h"
#include "fable2_address_scan.h"
#include "fable2_config.h"
#include "fable2_patches.h"
// #include "fable2_deadbeef_overlay.h"
// 30fps-cap instrumentation (writes fps_probe.log next to the exe). Disabled
// now that the cap is lifted via REX_VSYNC=0 (see tools/fable2-uncapped.cmd).
// Re-enable to re-measure the frame pacing:
// #include "fps_probe.h"
#include "keyboard_gamepad.h"
#ifdef FABLE2_REMOTE_CONTROL
#include "remote_control_server.h"
#include "remote_gamepad_driver.h"
// 1-second game-state classifier (src/diagnostics/fable2_state_probe.h, single-TU inline).
// Forward-declared here; the inline definitions live in the state probe header
// (included once, in main.cpp), so the app can start/stop it alongside the
// remote control server without pulling the whole probe into this header.
namespace fable2::stateprobe {
void start();
void stop();
std::int64_t t_ms();
void record_a_press(std::int64_t ms);
}
#endif  // FABLE2_REMOTE_CONTROL
#include "xex_verify.h"
#include "game_branding.h"

namespace {
// The `screenshot` command reads the game's rendered frame from the renderer's
// presenter, but rex::ui::Window keeps presenter() protected. Window is a
// non-final polymorphic base with Window as its only/first base, and
// presenter() is a non-virtual inline that merely returns the presenter_
// pointer - so the standard derived-class accessor idiom (reinterpret to a
// first-base-derived type) safely reads the member without touching any
// vtable. This keeps the fix self-contained in the app (no SDK edits).
class WindowPresenterAccessor final : public rex::ui::Window {
 public:
  rex::ui::Presenter* PresenterPtr() const { return presenter(); }
};
rex::ui::Presenter* GetWindowPresenter(rex::ui::Window* w) {
  return w ? reinterpret_cast<const WindowPresenterAccessor*>(w)->PresenterPtr()
           : nullptr;
}
}  // namespace

class Fable2App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<Fable2App>(new Fable2App(ctx, "fable_2",
        PPCImageConfig));
  }

  // Remote control (AI input channel) - see plans/ai-remote-input-control.md
  // and src/input/remote_control_server.h. The store is shared by the remote pad
  // driver (registered in OnPreSetup) and the command server (started in
  // OnPostInitLogging); the server publishes resolved snapshots and the
  // guest-side driver consumes the latest on each poll. DEBUG builds only
  // (gated on FABLE2_REMOTE_CONTROL; a hidden input channel must not ship in
  // Release builds).
#ifdef FABLE2_REMOTE_CONTROL
  std::shared_ptr<fable2::remote::InputStateStore> remote_state_ =
      std::make_shared<fable2::remote::InputStateStore>();
  std::atomic<bool> remote_pad_enabled_{true};
  fable2::remote::ControlServer remote_server_{remote_state_.get(),
                                               &remote_pad_enabled_};
#endif  // FABLE2_REMOTE_CONTROL

  // Emulate the Xbox 360 Xenos GPU. Two plugins are staged next to the exe:
  //   rexgpu-xenos[d].dll        -> D3D12 (prebuilt SDK plugin; the default)
  //   rexgpu-xenos-vulkan[d].dll -> Vulkan (built from the SDK source via
  //                                   tools/build_sdk_vulkan.cmd)
  // Swap the renderer at launch with --gpu_plugin, no rebuild required:
  //   --gpu_plugin=xenos            D3D12 (default)
  //   --gpu_plugin=xenos-vulkan     Vulkan
  void OnPreSetup(rex::RuntimeConfig& config) override {
    std::string plugin = config.gpu_plugin.empty() ? "xenos" : config.gpu_plugin;
    config.gpu_plugin = plugin;
    if (plugin == "xenos-vulkan") {
      // Load the source-built plugin and force the Vulkan backend. It is
      // compiled with both D3D12 and Vulkan; the default "any" would pick
      // D3D12, so pass "vulkan" explicitly to get the VulkanGraphicsSystem.
      config.graphics = rex::system::LoadGpuPlugin("xenos-vulkan", "vulkan");
    }
    // Otherwise leave config.graphics null so ReXApp loads
    // LoadGpuPlugin("xenos") -> the prebuilt D3D12 plugin.

    // Build on top of the default input system (SDL gamepad + NOP) and add a
    // synthetic "keyboard gamepad" driver so host keys/mouse buttons can drive
    // the guest. The mapping is the `keyboard_gamepad_map` cvar (default from
    // fable2_config.toml [input], see fable2_config.h). See
    // src/input/keyboard_gamepad.h.
    config.input_factory = [this](bool tool_mode) ->
        std::unique_ptr<rex::system::IInputSystem> {
      auto system = rex::input::CreateDefaultInputSystem(tool_mode);
      system->AddDriver(
          std::make_unique<fable2::KeyboardGamepadDriver>(system->window(), 0));
#ifdef FABLE2_REMOTE_CONTROL
      // Remote (AI) pad: driven over localhost TCP by an external harness
      // (src/input/remote_control_server.h). OR-merges with the pads above; not
      // gated on window focus. Debug builds only.
      system->AddDriver(std::make_unique<fable2::remote::GamepadDriver>(
          system->window(), 0, remote_state_.get(), &remote_pad_enabled_));
#endif  // FABLE2_REMOTE_CONTROL
      return system;  // C++14 unique_ptr<Derived> -> unique_ptr<Base>
    };
  }

  void OnPostSetup() override {
    fable2::branding::Apply(window());
    // Hero/dog black-texture fix (see plans/hero-dog-texture-readback.md).
    // Approach + guest base 0x12704000 credit just-harry's Unofficial Xenia
    // femtofork for Fable II. readback_resolve_force_addresses is defined in the
    // GPU plugin (loaded in OnPreSetup, i.e. after OnPostInitLogging), so it is
    // seeded here, now that the plugin is loaded and its cvar is registered.
    // Force a CPU readback of the render-to-texture resolve that regenerates the
    // hero/dog face+skin texture; the address is Fable-II-specific, so it lives in
    // the app and the SDK only knows "read back resolves to these addresses".
    {
      const fable2::config::Values& cfg = fable2::config::Get();
      const std::string_view force_addr =
          cfg.hero_dog_texture_readback ? "0x12704000" : "";
      if (rex::cvar::GetFlagSource("readback_resolve_force_addresses") ==
          rex::cvar::Source::kDefault) {
        if (rex::cvar::SetFlagByName("readback_resolve_force_addresses", force_addr)) {
          REXSYS_INFO("[fable2-config] seeded readback_resolve_force_addresses "
                      "from fable2_config.toml (hero_dog_texture_readback={})",
                      cfg.hero_dog_texture_readback);
        } else {
          REXSYS_WARN("[fable2-config] cvar readback_resolve_force_addresses "
                      "rejected '{}'; hero/dog readback fix may be inactive",
                      std::string{force_addr});
        }
      }
    }
    // Dog fur Z-fighting fix (docs/RUNTIME_FIXES.md): the shell-fur pixel shader
    // relies on exact 24-bit depth ties; a small host depth bias restores them.
    // depth_bias_shader is a GPU-plugin cvar, so it is seeded here too.
    if (fable2::config::Get().dog_fur_depth_bias &&
        rex::cvar::GetFlagSource("depth_bias_shader") == rex::cvar::Source::kDefault) {
      if (rex::cvar::SetFlagByName("depth_bias_shader", "014F8A02DB7B19CA") &&
          rex::cvar::SetFlagByName("depth_bias_shader_units", "16")) {
        REXSYS_INFO("[fable2-config] seeded dog fur depth bias (dog_fur_depth_bias=true)");
      } else {
        REXSYS_WARN("[fable2-config] dog fur depth bias cvars missing "
                    "(SDK without depth_bias_shader?)");
      }
    }

    // Cyan sky / missing world fix (ANDROID_STATUS.md, 2026-10-27): before the
    // main pass the game aliases the main D24FS8 depth as D24S8 4xMSAA for
    // stencil passes with an unbounded window. On hardware that is a lossless
    // bit reinterpretation; host render targets convert float24 <-> unorm24
    // across the whole EDRAM, so far-plane depth drifts, the GEQUAL sky fails
    // and stale EDRAM shows through. Skipping depth->depth format-change
    // transfers keeps the main depth target's own (unmodified) contents.
    if (rex::cvar::GetFlagSource("rt_transfer_skip_mask") == rex::cvar::Source::kDefault) {
      if (rex::cvar::SetFlagByName("rt_transfer_skip_mask", "4")) {
        REXSYS_INFO("[fable2-config] seeded rt_transfer_skip_mask=4 (depth format aliasing)");
      } else {
        REXSYS_WARN("[fable2-config] rt_transfer_skip_mask missing (SDK without it?)");
      }
    }

    // Performance (ANDROID_STATUS.md, 2026-10-29): the D3D clear quad (this
    // vertex shader, rectangle list, depth/stencil always-write, no color) is
    // drawn at 4x MSAA over the whole EDRAM between 1x shadow passes. It
    // overwrites every sample, so copying the old depth into it is skipped.
    if (rex::cvar::GetFlagSource("rt_clear_quad_vs_hashes") == rex::cvar::Source::kDefault) {
      if (rex::cvar::SetFlagByName("rt_clear_quad_vs_hashes", "1E6883FCCDE1F688")) {
        REXSYS_INFO("[fable2-config] seeded rt_clear_quad_vs_hashes=1E6883FCCDE1F688");
      }
    }

    // Purple bottom strip (ANDROID_STATUS.md, 2026-10-29): those clear quads are
    // unclipped, so without running their vertex shader on the CPU they are
    // assumed to cover the whole EDRAM. The 280x180 4x depth one only reaches
    // row 176, and the stale rows below it (morph pass data) reached the screen.
    if (rex::cvar::GetFlagSource("execute_unclipped_draw_vs_on_cpu") ==
        rex::cvar::Source::kDefault) {
      if (rex::cvar::SetFlagByName("execute_unclipped_draw_vs_on_cpu", "true")) {
        REXSYS_INFO("[fable2-config] seeded execute_unclipped_draw_vs_on_cpu=true");
      }
    }

    // Performance (measured on Adreno 740, gameplay 4.5 -> 7.2 fps, visuals
    // unchanged): upload shared memory ahead of the frame's command buffer
    // instead of breaking render passes for in-order uploads, and don't block
    // the GPU thread every frame copying memexport results back to the CPU.
    for (auto [name, value] : {std::pair{"vulkan_shared_memory_upload_first", "true"},
                               std::pair{"readback_memexport", "false"}}) {
      if (rex::cvar::GetFlagSource(name) == rex::cvar::Source::kDefault) {
        if (rex::cvar::SetFlagByName(name, value)) {
          REXSYS_INFO("[fable2-config] seeded {}={} (performance)", name, value);
        }
      }
    }

    // Feed the F3 debug overlay with guest FPS (below). The GPU plugin
    // records per-guest-swap frame timing into the shared perf registry
    // (rex::perf, state lives in rexruntime.dll and is shared with the
    // plugin); read the last snapshot and expose it as FrameStats so the
    // overlay's "Guest: X FPS (Y ms)" line works on every runtime build.

    // Background monitor for the table-zeroing / init-ordering bug. Runs on a
    // SEPARATE OS thread with hardcoded stable addresses (no recompiled probe
    // dependency), so it does not perturb the recompiled hot path that the
    // race depends on. It samples the allocator table continuously from t=0
    // and distinguishes:
    //   (A) table valid then zeroed  -> "HEAD WENT NULL" after "FIRST VALID"
    //   (B) never valid (consumer first) -> only "null" ever, no "FIRST VALID"
    std::thread([]() {
      using namespace fable2::allocwatch;
      const uintptr_t base = kGuestBase;  // 0x100000000, stable
      auto t0 = std::chrono::steady_clock::now();
      auto ms = [&] { return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count(); };
      // The guest arena is commit-on-fault; a page may be uncommitted when we
      // sample early. Use VirtualQuery to only read committed pages, so an
      // early sample is skipped instead of faulting the monitor thread.
      // (Linux/Android: the analogue is a /proc/self/maps scan; PROT_NONE
      // arena pages show up in maps but are not safe to read, so require
      // r+w permissions, refreshed periodically.)
      auto committed = [](const volatile void* p) {
#ifdef _WIN32
        MEMORY_BASIC_INFORMATION mbi{};
        if (::VirtualQuery(const_cast<void*>(const_cast<const void*>(p)), &mbi, sizeof(mbi)) == 0) return false;
        return mbi.State == MEM_COMMIT &&
               (mbi.Protect & (PAGE_READWRITE | PAGE_READONLY | PAGE_WRITECOPY)) != 0;
#else
        static std::vector<std::pair<uintptr_t, uintptr_t>> rw_ranges;
        static std::chrono::steady_clock::time_point last_refresh{};
        auto now = std::chrono::steady_clock::now();
        if (now - last_refresh > std::chrono::milliseconds(500)) {
          last_refresh = now;
          rw_ranges.clear();
          std::ifstream maps("/proc/self/maps");
          std::string line;
          while (std::getline(maps, line)) {
            uintptr_t lo = 0, hi = 0;
            char dash = 0;
            std::istringstream iss(line);
            if (!(iss >> std::hex >> lo >> dash >> hi)) continue;
            std::string perms;
            iss >> perms;
            if (perms.find('r') != std::string::npos &&
                perms.find('w') != std::string::npos) {
              rw_ranges.emplace_back(lo, hi);
            }
          }
        }
        const auto a = reinterpret_cast<uintptr_t>(p);
        for (const auto& r : rw_ranges) {
          if (a >= r.first && a < r.second) return true;
        }
        return false;
#endif
      };
      auto safe_sample = [&](uint32_t& tbl, uint32_t& flag, uint32_t& head) -> bool {
        tbl = flag = head = 0;
        if (!committed(GuestPtr(base, kTablePtrGuest))) return false;
        tbl = *GuestPtr(base, kTablePtrGuest);
        flag = *GuestByte(base, kFlagGuestAddr);
        if (tbl != 0) {
          if (!committed(GuestPtr(base, tbl + 12))) return false;
          head = *GuestPtr(base, tbl + 12);
        }
        return true;
      };
      bool first_valid = false;
      bool ever_valid = false;
      int null_samples = 0;
      while (true) {
        uint32_t tbl, flag, head;
        if (!safe_sample(tbl, flag, head)) {
          std::this_thread::sleep_for(std::chrono::milliseconds(5));
          continue;  // page not committed yet
        }
        if (tbl != 0 && head != 0 && !first_valid) {
          first_valid = true; ever_valid = true;
          REXSYS_ERROR("[alloc-watch] FIRST VALID head: t={}ms tbl@83496BD4=0x{:08X} head=0x{:08X} flag=0x{:02X}",
                       ms(), tbl, head, flag);
        }
        if (tbl != 0 && head != 0) ever_valid = true;
        // Log the transition valid->null (the zeroing) with full context.
        static uint32_t s_prev_head = 0;
        static bool s_have_prev = false;
        if (tbl != 0) {
          if (s_have_prev && s_prev_head != 0 && head == 0) {
            uint32_t ev = null_samples++;
            if (ev < 6) {
              REXSYS_ERROR("[alloc-watch] HEAD WENT NULL: t={}ms tbl=0x{:08X} prevHead=0x{:08X} flag=0x{:02X} ever_valid={} ; dump:",
                           ms(), tbl, s_prev_head, flag, ever_valid);
              for (int o = 0; o < 20; o += 4)
                REXSYS_ERROR("[alloc-watch]   table+{:2d}=0x{:08X}", o, *GuestPtr(base, tbl + o));
              for (uint32_t a = 0x83496BC0; a < 0x83496BF0; a += 4)
                REXSYS_ERROR("[alloc-watch]   0x{:08X}=0x{:08X}", a, *GuestPtr(base, a));
            }
          }
          s_prev_head = head; s_have_prev = true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
      }
    }).detach();

    SetGuestFrameStats([]() {
      rex::ui::FrameStats stats;
      auto ft_us =
          rex::perf::GetSnapshotCounter(rex::perf::CounterId::kFrameTimeUs);
      if (ft_us > 0) {
        stats.frame_time_ms = double(ft_us) / 1000.0;
        stats.fps = 1000000.0 / double(ft_us);
        stats.frame_count = 1;  // non-zero = "has data" (gates overlay text)
      }
      return stats;
    });
  }

  // Load the recomp's own user config (fable2_config.toml) next to the exe,
  // creating it with defaults on first launch. Runs after the SDK's logging
  // init so load/create/parse problems land in logs/ (see src/core/fable2_config.h).
  // Plain settings are read via fable2::config::Get(); settings that back a
  // cvar are seeded into it below so the console/overlay keep working.
  static std::filesystem::path StorageFolder() {
#ifdef __ANDROID__
    if (const char* storage = std::getenv("REX_ANDROID_STORAGE_DIR")) {
      return storage;
    }
#endif
    return rex::filesystem::GetExecutableFolder();
  }

  void OnPostInitLogging() override {
    // This describes the linked build, not a guarantee about DLLs replaced later.
    REXSYS_INFO("[native-build] configuration={} linked_sdk={}",
                FABLE2_NATIVE_CONFIGURATION, FABLE2_NATIVE_SDK);
    const std::filesystem::path exe_dir =
        StorageFolder();
    fable2::config::Load(exe_dir / "fable2_config.toml");

    // Seed cvars from the config. Only when the cvar is still at its
    // compiled default (source kDefault), so higher-priority sources --
    // fable_2.toml (kConfig), REX_* env vars (kEnvironment), and the command
    // line (kCommandLine) -- keep winning over the recomp config. The F3
    // console can still change the value live afterwards (the driver
    // hot-reloads on text change, see src/input/keyboard_gamepad.h).
    const auto seed_cvar = [](std::string_view cvar, std::string_view value) {
      if (rex::cvar::GetFlagSource(cvar) != rex::cvar::Source::kDefault) return;
      if (!rex::cvar::SetFlagByName(cvar, value)) {
        REXSYS_WARN(
            "[fable2-config] cvar {} rejected config value '{}'; keeping "
            "built-in default",
            cvar, value);
        return;
      }
      REXSYS_INFO("[fable2-config] seeded cvar {} from fable2_config.toml",
                  cvar);
    };
    const fable2::config::Values& cfg = fable2::config::Get();
    // XLanguage profile default; config, environment and CLI still win.
    std::string profile_hash;
    if (fable2::xexverify::Sha256File(game_data_root() / "default.xex", profile_hash) &&
        fable2::xexverify::IsAcceptedHash(profile_hash)) {
      const auto* version = fable2::versions::Find(profile_hash);
      seed_cvar("user_language", std::to_string(version->language));
      REXSYS_INFO("[build-profile] detected {}", version->name);
    }
    seed_cvar("keyboard_gamepad_map", cfg.keyboard_gamepad_map);
    seed_cvar("mouse_look", cfg.mouse_look ? "true" : "false");
    seed_cvar("mouse_look_scale", std::to_string(cfg.mouse_look_scale));
    // NOTE: the hero/dog readback fix (readback_resolve_force_addresses) is a
    // GPU-PLUGIN cvar, so it is seeded in OnPostSetup() (after the plugin is
    // loaded) rather than here - see plans/hero-dog-texture-readback.md.

#ifdef FABLE2_REMOTE_CONTROL
    // Start the remote control server (AI input channel; debug-only, no
    // config knobs - always on with ControlServer::Config defaults: 127.0.0.1:
    // 8791, no auth). Debug builds only (FABLE2_REMOTE_CONTROL); Release
    // builds never open the port.
    if (!remote_server_.Start({})) {
      REXSYS_WARN(
          "[fable2-config] remote control server could not start; remote "
          "input disabled (see logs/)");
    }

    // The `screenshot` command reads the game's rendered frame from the
    // renderer's presenter (CaptureGuestOutput). Query it at command time, not
    // at startup: this hook runs before SetupPresentation, so the window and
    // its presenter may not exist yet when the server starts.
    remote_server_.SetPresenterProvider(
        [this]() -> rex::ui::Presenter* { return GetWindowPresenter(window()); });

    // Observe remote A-presses (the state machine's input transition). The
    // press is brief, so latch its timestamp here (on the server thread) and
    // compare against it in the 1-second classifier sample.
    remote_state_->SetOnPublish([](const fable2::remote::InputSnapshot& s) {
      if ((s.buttons & 0x1000u) != 0)  // X_INPUT_GAMEPAD_A
        fable2::stateprobe::record_a_press(fable2::stateprobe::t_ms());
    });
    // Start the 1-second game-state classifier (feeds the remote `state`
    // command + the [state] in-game log). The state is always tracked in
    // debug builds.
    fable2::stateprobe::start();
#endif  // FABLE2_REMOTE_CONTROL
  }

#ifdef FABLE2_REMOTE_CONTROL
  // Stop the state classifier thread, then the remote control server threads,
  // before anything else tears down.
  void OnShutdown() override {
    fable2::stateprobe::stop();
    remote_server_.Stop();
  }
#endif  // FABLE2_REMOTE_CONTROL
  // Apply the game patches (see src/core/fable2_patches.h) once the SDK has
  // decrypted default.xex into the guest arena, before the module launches.
  // Each patch is switched on by a [patches] key in fable2_config.toml
  // (loaded earlier, in OnPostInitLogging).
  void OnPostLoadXexImage() override {
    fable2::patches::ApplyAll(runtime()->memory(), PPCImageConfig);
    // Lets the address-scan hook (src/core/fable2_address_scan.cpp) read the
    // guest page tables.
    fable2::addressscan::SetMemory(runtime()->memory());
    // Lets the direct-mouse-look hook (src/core/fable2_hooks.cpp) translate the
    // camera object's guest address to a host pointer.
    fable2::mouselook::set_memory(runtime()->memory());
  }

  // Startup integrity check: this build was recompiled against a specific
  // default.xex, so verify its SHA-256 before the runtime loads it (see
  // src/core/xex_verify.h). A previously verified file is skipped via the
  // cache/default.xex.sha256 marker; a mismatch aborts with a dialog that
  // shows how to check the hash yourself.
  void OnLoadXexImage(std::string& xex_image) override {
    // Self-heal the guest VFS index before the title runs: the game indexes
    // all of data/ from data/dir.manifest, and a missing entry means the
    // guest sees null (crashes at startup). Content migrated with an
    // mtime-respecting copy can leave a stale/truncated manifest in place;
    // this appends any file that is on disk but unlisted. Idempotent.
    fable2::manifestheal::EnsureComplete(game_data_root());

    // Resolve the host path the same way the SDK does (game:\ / d:\ ->
    // game_data_root). The SDK's path may use either slash direction
    // (e.g. "game:\\default.xex" or "game:/default.xex"), so strip the
    // device prefix and any following separator generically.
    std::string_view tail = xex_image;
    if (tail.starts_with("game:")) tail.remove_prefix(5);
    else if (tail.starts_with("d:")) tail.remove_prefix(2);
    if (!tail.empty() && (tail.front() == '\\' || tail.front() == '/'))
      tail.remove_prefix(1);
    std::string host_tail{tail};
    std::replace(host_tail.begin(), host_tail.end(), '\\', '/');
    const std::filesystem::path xex = game_data_root() / host_tail;
    std::filesystem::path cache = cache_root();
    if (cache.empty()) cache = game_data_root() / "cache";
    const std::filesystem::path marker = cache / "default.xex.sha256";

    // --skipHash (cvar, see main.cpp): compute and log the default.xex hash,
    // but do not let a mismatch (or an unreadable file) block the launch.
    const bool skip_hash = rex::cvar::GetFlagByName("skipHash") == "true";
    if (skip_hash) {
      REXSYS_INFO("[xex-verify] --skipHash is set: the default.xex hash will be "
                  "logged but NOT enforced (a mismatch will not block the launch)");
    }

    REXSYS_INFO("[xex-verify] {}", xex.string());
    REXSYS_INFO("[xex-verify] accepted SHA-256: {}", fable2::xexverify::kExpectedHashes);
    const auto r = fable2::xexverify::Check(xex, marker);
    switch (r.result) {
      case fable2::xexverify::Result::VerifiedCached:
        REXSYS_INFO("[xex-verify] actual SHA-256: {} (verified on a previous start; "
                    "size/mtime unchanged, hash pass skipped)",
                    r.actual_hash);
        break;
      case fable2::xexverify::Result::VerifiedFresh:
        REXSYS_INFO("[xex-verify] actual SHA-256: {} (MATCH; recorded in {} so "
                    "future starts skip the check)",
                    r.actual_hash, marker.string());
        break;
      case fable2::xexverify::Result::Mismatch: {
        if (skip_hash) {
          REXSYS_INFO("[xex-verify] actual SHA-256: {} (MISMATCH vs expected {}; "
                      "not enforced because --skipHash is set)",
                      r.actual_hash, fable2::xexverify::kExpectedHashes);
          break;
        }
        REXSYS_ERROR("[xex-verify] actual SHA-256: {}", r.actual_hash);
        REXSYS_ERROR("[xex-verify] expected SHA-256: {} (MISMATCH)",
                     fable2::xexverify::kExpectedHashes);
        const std::string msg = std::format(
            "default.xex hash mismatch\n\n"
            "This build only supports verified original GOTY executables.\n"
            "The XEX in the selected game folder is not supported.\n\n"
            "File:     {}\n"
            "Actual:   {}\n"
            "Expected: {}\n\n"
            "You can check the hash yourself in a Windows terminal:\n"
            "  certutil -hashfile \"{}\" SHA256\n"
            "(PowerShell: Get-FileHash \"{}\" -Algorithm SHA256)\n\n"
            "Select a complete matching GOTY dump and start again.\n"
            "Do not mix an XEX from another version with these game files.",
            xex.string(), r.actual_hash, fable2::xexverify::kExpectedHashes,
            xex.string(), xex.string());
        REXSYS_ERROR("[xex-verify] {}", msg);
        rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Error, msg);
        std::exit(1);
      }
      case fable2::xexverify::Result::ReadFailed:
      default:
        if (skip_hash) {
          REXSYS_WARN("[xex-verify] could not hash {} (not enforced because "
                      "--skipHash is set)",
                      xex.string());
          break;
        }
        REXSYS_ERROR("[xex-verify] could not hash {}; refusing to load unverified content", xex.string());
        rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Error,
            "default.xex could not be read. Check the selected game folder and file permissions.");
        std::exit(1);
        break;
    }
    const auto root = game_data_root();
    // The content-compatibility gate depends on the XEX hash identifying a
    // known version, so it is also bypassed by --skipHash.
    if (!skip_hash && !fable2::xexverify::HasCompatibleContent(root, r.actual_hash)) {
      REXSYS_ERROR("[build-profile] incomplete or mixed GOTY content: {}", root.string());
      rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Error,
          "Incomplete or mixed GOTY content. Select an original matching GOTY dump; do not replace its XEX with another version.");
      std::exit(1);
    }
  }

  // Self-contained layout: everything lives next to the executable.
  //   <exe>/               <- content root (default.xex, data/, nxeart/,
  //                           $SystemUpdate/; staged next to the exe by the
  //                           build, see CMakeLists.txt)
  //   <exe>/saves/         <- user data: save files, settings, profiles
  //   <exe>/cache/         <- runtime caches (shader cache, etc.)
  //   <exe>/fable_2.toml   <- cvar config (SDK default)
  //   <exe>/logs/          <- game logs (SDK default)
  // The content root is the exe's own directory ONLY (no ancestor walk), so
  // the whole build folder is portable as-is: copy it anywhere and it runs.
  // Override the content location with --game_data_root=<path>.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    const std::filesystem::path exe_dir =
        StorageFolder();

    if (paths.game_data_root.empty()) {
      if (std::filesystem::is_regular_file(exe_dir / "default.xex")) {
        paths.game_data_root = exe_dir;
      } else {
        // The SDK's fallback for an empty game_data_root is the cryptic
        // "--game_data_root was not provided" dialog; in this
        // self-contained layout the real problem is the missing content,
        // so say that and stop before the SDK's dialog can show.
        // (Logging is not initialized yet at this hook, dialog only.)
        const std::string msg = std::format(
            "default.xex was not found next to fable_2.exe ({}).\n\n"
            "Stage the game content there (the build does this "
            "automatically) or pass --game_data_root=<path>.",
            exe_dir.string());
        rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Error, msg);
        std::exit(1);
      }
    }

    // The game content itself must actually be present too (covers both the
    // exe-dir default and a --game_data_root override): without data/ the
    // guest fails deep inside content loading with cryptic errors, so fail
    // fast with the real problem.
    if (!paths.game_data_root.empty() &&
        !std::filesystem::is_directory(paths.game_data_root / "data")) {
      const std::string msg = std::format(
          "The data folder was not found in {}.",
          paths.game_data_root.string());
      rex::ShowSimpleMessageBox(rex::SimpleMessageBoxType::Error, msg);
      std::exit(1);
    }

    // Keep saves and caches inside the exe's own folder (self-contained).
    // The SDK's default user dir is a per-user platform location, which
    // caused save corruption. NOTE: set unconditionally because the SDK
    // defaults are non-empty, so empty-checks would never trigger (this
    // also overrides the cvar/CLI defaults).
    std::error_code ec;
    paths.user_data_root = exe_dir / "saves";
    std::filesystem::create_directories(paths.user_data_root, ec);
    paths.cache_root = exe_dir / "cache";
    std::filesystem::create_directories(paths.cache_root, ec);

    // Mount the $SystemUpdate folder (inside the content root) as update:\
    // so VdSetGraphicsInterruptCallback-era update partition lookups resolve
    // instead of failing with "device not found".
    if (paths.update_data_root.empty() && !paths.game_data_root.empty()) {
      auto update_dir = paths.game_data_root / "$SystemUpdate";
      if (std::filesystem::is_directory(update_dir)) {
        paths.update_data_root = update_dir;
      }
    }
  }
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    // fable2::deadbeef_overlay::register_overlay(drawer);
  }
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
};
