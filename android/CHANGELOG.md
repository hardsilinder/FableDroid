## Unreleased

- Added launcher GPU driver downloads, SHA-256 verification, bounded installation, installed-driver selection, and system/bundled fallback choices.
- Added a debug-only benchmark launch path that preserves test configuration and enables test touch controls without changing preferences.
- Corrected triple-buffering wording: input-to-display latency has not been measured.

# Android changelog

## 0.6.0 beta — 2026-10-29

First public Android release (beta). Published briefly as 1.0.0 on 2026-10-28, then relabelled.

### Added
- New launcher with a two-pane layout, game status card and gamepad navigation.
- Install directly from an Xbox 360 ISO (XDVDFS reader, no PC extraction needed),
  with a free-space check, progress, cancel, and atomic file replacement.
- Game version detection from `default.xex`, generated from `constants/game_versions.json`.
- Graphics settings: resolution scale (1x/2x/3x), MSAA, triple buffering, bundled
  Turnip driver toggle.
- Controller button remapping with automatic swapping.
- Touch controls: Auto/On/Off mode with live gamepad detection, opacity, size and
  vibration settings.
- Touch layout editor: move, resize and hide each control.
- Save backup to a ZIP file through Android's file picker.
- Bundled Mesa Turnip v27 driver for Adreno 6xx/7xx, loaded through libadrenotools.
- App icon, release signing support and versioned APK names.

### Performance (AYN Thor, native 720p)
- About 29 FPS while moving with a warm shader cache, up from about 10 FPS at the
  first playable build. See `docs/ANDROID_PERFORMANCE.md`.
- Fixed the persistent pipeline cache, which was being wiped every launch.
- MSAA is off by default (largest single cost).

### Fixed
- Cyan sky and missing world geometry (depth format round-trip drift).
- Development-only keys are removed from `fable_2.toml` on launch.

### Changed
- ZIP import only accepts `default.xex`, `data/` and `nxeart/`; `$SystemUpdate/` is skipped.
