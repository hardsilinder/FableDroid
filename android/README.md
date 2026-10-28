# Fable II for Android

A native Android build of the Fable II recompilation. The game's Xbox 360 code is
statically recompiled to arm64, and graphics run on Vulkan. Nothing is emulated
at the instruction level.

**You need your own copy of Fable II.** The APK contains no game content. You supply
a disc image (ISO) of a supported version, and the launcher installs it on the device.

> This project does not condone piracy. Any mention of or link to pirated copies
> will be removed and will get you banned from the repository.

<p>
  <img src="../docs/screenshots/android-launcher.png" width="49%" alt="Launcher">
  <img src="../docs/screenshots/android-controls.png" width="49%" alt="Control settings">
</p>
<p>
  <img src="../docs/screenshots/android-touch-editor.png" width="49%" alt="Touch layout editor">
</p>

---

## Features

**Launcher**
- Install straight from an Xbox 360 ISO, no PC tools or extraction needed. Pick the
  file in Android's file picker and the launcher reads the disc image directly.
- Detects the game version (SHA-256 of `default.xex`) and clearly warns when it is
  unsupported or unrecognised.
- Checks free space before installing, shows progress, and can be cancelled. A cancelled
  or failed install never leaves a half-installed game or touches your saves.
- ZIP import also works if you already extracted the disc on a PC.
- One-tap save backup to a ZIP anywhere you choose (Downloads, Google Drive, a USB stick…).
- Works with touch or a gamepad (every control is focusable with a visible focus ring).

**Graphics settings**
- Resolution scale: 1x (native 1120×720), 2x, 3x.
- Anti-aliasing (MSAA) on/off.
- Triple buffering on/off.
- Bundled Mesa Turnip GPU driver for Adreno 6xx/7xx (on by default), or the system driver.
- Tuned renderer defaults for Adreno, written to the game config on every launch.

**Controls**
- Physical gamepads work out of the box (built-in handheld controls, Bluetooth and USB pads).
- Controller button remapping. Pick an Xbox button and press the physical button you
  want for it. Buttons swap, so no action is ever left unbound.
- On-screen touch controls: two analog sticks, ABXY, D-pad, bumpers, analog triggers,
  stick clicks, Start and Back.
- Touch layout editor: drag any control, resize it (50–250%) or hide it, then save.
- Touch opacity, global size and vibration settings.
- Touch mode Auto (shown only when no gamepad is connected, switches live when one is
  plugged in or removed), Always or Off.

**Game**
- Playable start to finish (see the main README for the desktop project's status).
- Saves and settings stay on the device and survive reinstalling the game files.
- Persistent shader cache: shaders compile once, then load at boot on later launches.

---

## Requirements

| | |
|---|---|
| Android | 12 (API 31) or newer, 64-bit (arm64-v8a) |
| GPU | Vulkan 1.1+. Adreno 7xx strongly recommended (Snapdragon 8 Gen 1 / 8+ Gen 1 / 8 Gen 2 or newer) |
| RAM | 8 GB minimum, 12 GB+ recommended |
| Storage | About 7 GB for the game files, plus the ISO while installing (about 7.8 GB) |
| Game | A Fable II ISO of a supported version (below) |

Tested on the **AYN Thor** (Snapdragon 8 Gen 2, Adreno 740, Android 13). Phones and
handhelds with the same chip family (Odin 2, Retroid Pocket 5, OnePlus 11/Open, Galaxy
S23…) should behave similarly but have not been verified.

### Supported game versions

| Version | Supported |
|---|---|
| Game of the Year — USA/Europe | Yes (primary test version) |
| Game of the Year — Germany | Yes |
| Game of the Year — France | Yes |
| Game of the Year — Italy | Yes |
| Game of the Year — Russia | Yes |
| Game of the Year — Spain | Yes |
| German retail / Title Update 1 | No (needs its own recompilation profile) |
| Japan, Asia, Latin America | No |

The original development ISO was the GOTY USA/Europe disc,
SHA-256 `685a0d3bea9718812f17bcd155907a5359a548b6d3d8342dd2a6c944f45e35ff`.
Other dumps of the same version work as long as `default.xex` matches. The launcher
tells you which version it detected.

---

## Installing

1. **Install the APK.** Download `Fable2-Android-<version>-release.apk` from the
   [Releases](../../releases) page and open it on the device. Allow installing from
   your browser or file manager if Android asks.
2. **Copy your ISO to the device.** Any folder works, for example `Download`. Use USB,
   a microSD card, or a network share.
3. **Open Fable II.** Tap **Select game ISO** and choose the file. Installation takes
   from about 10 seconds to a few minutes depending on storage speed.
4. When the status card says the game is ready, tap **Play**.

After installing you can delete the ISO to free up space. The game files live in the
app's own storage.

> **Tip:** the first time you visit an area, the game compiles shaders and can hitch
> for a moment. Those shaders are cached, so later visits and later launches are smooth.

### Updating the app

Install the new APK over the old one. Your game files, saves and settings are kept.
**Uninstalling the app deletes everything, including saves.** Back up first.

---

## Settings

All settings are in the launcher and apply the next time you tap **Play**.

| Setting | Default | What it does |
|---|---|---|
| Resolution scale | 1x | Internal render resolution. 1x is the Xbox 360's own 1120×720. 2x and 3x are experimental and need far more GPU power than current handhelds have. |
| Anti-aliasing (MSAA) | Off | The game's own 2x MSAA. Costs about 5 FPS on an Adreno 740. |
| Triple buffering | Off | Lets the CPU queue one more frame. Measured no faster on the Thor and adds a frame of input latency, so it is off by default. |
| Bundled Turnip GPU driver | On | Uses Mesa Turnip (bundled) instead of the phone's driver. Much faster and more correct on Adreno. Turn off on non-Adreno GPUs or to compare. |
| Touch controls | Auto | Auto shows them only when no gamepad is connected. |
| Touch opacity / size | 60% / 100% | Appearance of the on-screen controls. |
| Vibrate on touch | On | Short haptic tick on each touch press. |

The launcher manages these keys in `fable_2.toml`. Any other key you add by hand is
preserved, so advanced options from the desktop README still work.

---

## Controls

### Gamepad
Standard Xbox layout. To change it, open **Remap controller buttons**, select an
Xbox button, then press the physical button you want. Press **Save** to keep the
changes or **Reset to default** to start over. Triggers and sticks are not remappable.

### Touch screen
- **Left stick:** move. **Right stick:** camera.
- Tap the **Hide** button to hide every touch control; tap it again to bring them back.
- **Edit touch layout** opens the editor: drag a control to move it, tap
  **Smaller**/**Larger** to resize the selected control, **Hide** to remove it, and
  **Reset** to restore the default layout. **Save** keeps your changes.

### Keyboard
With a keyboard attached: Enter = A, Escape = B, X, Y, Space = Start, arrow keys = D-pad.

---

## Saves and files

Everything lives in `Android/data/org.fable2.recomp/files/` on internal storage:

| Path | Contents |
|---|---|
| `saves/` | Your save games |
| `fable_2.toml` | Engine settings (written by the launcher) |
| `fable2_config.toml` | Game settings |
| `default.xex`, `data/`, `nxeart/` | Installed game files |
| `*.log` | Logs for troubleshooting |

**Back up saves** in the launcher writes `saves/` and both config files to a ZIP.
To restore, copy the files back into the folder above with a file manager or
`adb push`.

---

## Performance

On the AYN Thor (Adreno 740) at 1x with the default settings, the game runs at
**about 29 FPS while moving and turning the camera**, with a median frame time of
33.6 ms. After shaders are cached, the worst frames are around 50–60 ms.
The GPU is at its limit, about 1 ms per frame short of a locked 30.

2x and 3x boot and run but are far below playable speed on current handhelds.
Details of the performance work are in [docs/ANDROID_PERFORMANCE.md](../docs/ANDROID_PERFORMANCE.md).

---

## Troubleshooting

| Problem | Fix |
|---|---|
| Version shown in red, or "Unrecognised game executable" | The disc's `default.xex` doesn't match a supported version. Check the table above; other versions will crash or misbehave. |
| "This disc image has no default.xex" | The file isn't a Fable II game disc (for example a bonus or video disc). |
| "Not enough free space" | Free up the amount shown. You need the full game size plus 256 MB. |
| Install fails partway | Make sure the ISO is a complete copy (about 7.8 GB). Tap **Select game ISO** to retry; saves are not touched. |
| Black screen or crash on a non-Adreno GPU (Mali, Xclipse, PowerVR) | Turn **Bundled Turnip GPU driver** off. Turnip only supports Adreno. |
| White screen right after starting | That's the intro fading in. Press A/Enter or wait. |
| Hitches the first time you enter an area | Shader compilation. They go away once the shader cache has been built. |
| Touch controls don't show | Touch mode is Auto and a gamepad is connected. Set it to On. |
| Lost buttons after remapping | Open **Remap controller buttons** and tap **Reset to default**. |

When reporting a bug, include your device, Android version, game version (shown in
the launcher) and the newest `.log` file from the folder above.

---

## Building from source

The build has three parts: the ReXGlue SDK built for Android, the game library built
for Android, and the Gradle app. A Mac (Apple Silicon) or Linux host works. The
commands below are what the release was built with on macOS.

### Prerequisites
- Android SDK with NDK **29.0.14206865**, CMake 3.25+ and Ninja
- JDK 21
- Python 3
- A supported `default.xex` in the project root (codegen reads it at build time)

### 1. SDK with the Android patch
The SDK submodule is stock `rexglue-sdk` plus this project's Android changes, kept in
[`thirdparty/rexglue-sdk-android.patch`](../thirdparty/rexglue-sdk-android.patch)
(Vulkan backend fixes and performance work, the shader-cache fix, libadrenotools for
custom driver loading, and Android platform support).

```sh
git submodule update --init --recursive
git -C thirdparty/rexglue-sdk apply ../rexglue-sdk-android.patch
```

Build the host code generator (runs on your machine) and the Android runtime:

```sh
export NDK=$HOME/Library/Android/sdk/ndk/29.0.14206865
cd thirdparty/rexglue-sdk

# Host codegen tool
cmake --preset mac-arm64 && cmake --build out/build/mac-arm64 --config Release --target rexglue

# Android runtime (librexruntime, librexgpu-xenos, libSDL3, hook libraries)
cmake -S . -B out/build/android-arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-31 -DANDROID_STL=c++_shared \
  -DCMAKE_BUILD_TYPE=Release -DREXGLUE_USE_VULKAN=ON \
  -DREXGLUE_ENABLE_FIDELITYFX=OFF -DREXGLUE_ENABLE_TRACY=OFF -DREXGLUE_BUILD_TESTS=OFF
cmake --build out/build/android-arm64
cd ../..
```

### 2. Game library
```sh
cmake -S . -B out/android-arm64 -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-31 -DANDROID_STL=c++_shared \
  -DCMAKE_BUILD_TYPE=Release -DREXGLUE_USE_VULKAN=ON \
  -DREXGLUE_ENABLE_FIDELITYFX=OFF -DREXGLUE_ENABLE_TRACY=OFF \
  -DREXSDK_DIR=$PWD/thirdparty/rexglue-sdk \
  -DFABLE2_CODEGEN_TOOL=$PWD/thirdparty/rexglue-sdk/out/mac-arm64/Release/rexglue
cmake --build out/android-arm64 --target fable_2
```

The registration code (about 40,000 calls) is split into small functions by
`android/chunk_registration.py` so it compiles in reasonable time.

### 3. APK
```sh
python3 android/package_native.py      # checks, copies and strips the .so files into jniLibs/
python3 android/generate_versions.py   # regenerates GameVersions.java from constants/game_versions.json
cd android
JAVA_HOME=/path/to/jdk-21 ./gradlew :app:assembleDebug     # or :app:assembleRelease
```

The APK is written to `android/app/build/outputs/apk/<type>/Fable2-Android-<version>-<type>.apk`.

### Release signing
Create `android/keystore.properties` (never commit it; it is gitignored):

```properties
storeFile=/absolute/path/to/release.jks
storePassword=...
keyAlias=...
keyPassword=...
```

Without it, `assembleRelease` produces an unsigned APK. Keep the same key for every
release, otherwise users can't update without uninstalling (which deletes their saves).

### Project layout
| Path | What |
|---|---|
| `app/src/main/java/org/fable2/recomp/LauncherActivity.java` | Launcher, ISO/ZIP install, settings, save backup |
| `XisoReader.java` | Reads Xbox 360 XDVDFS disc images directly |
| `Settings.java` | Preferences and writing `fable_2.toml` |
| `GameActivity.java` | SDL game activity, button remapping, touch visibility |
| `TouchControls.java`, `TouchLayoutActivity.java` | On-screen gamepad and layout editor |
| `RemapActivity.java` | Controller remapping screen |
| `GameVersions.java` | Generated from `constants/game_versions.json` |
| `app/src/main/assets/gpu_drivers/turnip27/` | Bundled Mesa Turnip driver |

SDL's Android Java classes come from the SDK's SDL3 submodule; they are not copied here.

---

## Credits and licenses

- **Fable II** © Microsoft / Lionhead Studios. This is an unofficial fan project, not
  affiliated with or endorsed by Microsoft. No game content is distributed.
- **ReXGlue SDK** — the recompiler and runtime. See its repository for its license.
- **Mesa Turnip** — MIT license, see `app/src/main/assets/gpu_drivers/turnip27/MESA_LICENSE.txt`.
  Build: Mesa Turnip v27.0.0-20261027-r3 (Mesa main c2d85da), packaged by The412Banner.
- **libadrenotools** — BSD 2-Clause, © Billy Laws.
- **SDL3** — zlib license.
- This project's own code is under the license in the repository root.
