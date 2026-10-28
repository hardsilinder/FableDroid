#!/usr/bin/env python3
"""Stage the completed arm64 native build for Gradle, preserving original symbols."""
import argparse
from pathlib import Path
import shutil
import struct
import subprocess

parser = argparse.ArgumentParser()
parser.add_argument('--build-dir', type=Path, default=Path('out/android-arm64'))
args = parser.parse_args()
project = Path(__file__).resolve().parent.parent
build = (project / args.build_dir).resolve()
cache = {}
for line in (build / 'CMakeCache.txt').read_text().splitlines():
    if '=' in line and ':' in line and not line.startswith(('#', '//')):
        key, value = line.split('=', 1)
        cache[key.split(':', 1)[0]] = value
ndk = Path(cache.get('CMAKE_ANDROID_NDK') or Path(cache['CMAKE_TOOLCHAIN_FILE']).parents[2])
toolchain = next((ndk / 'toolchains/llvm/prebuilt').iterdir())
strip = toolchain / 'bin/llvm-strip'
runtime = project / 'thirdparty/rexglue-sdk/out/linux-arm64'
sources = [build / 'libmain.so', runtime / 'librexruntime.so',
           runtime / 'librexgpu-xenos.so', runtime / 'libSDL3.so',
           runtime / 'libmain_hook.so', runtime / 'libhook_impl.so',
           runtime / 'libfile_redirect_hook.so', runtime / 'libgsl_alloc_hook.so',
           toolchain / 'sysroot/usr/lib/aarch64-linux-android/libc++_shared.so']
# Fail before changing the staged package if anything is missing or wrong-architecture.
for source in sources:
    with source.open('rb') as stream:
        header = stream.read(20)
    if header[:5] != b'\x7fELF\x02' or struct.unpack_from('<H', header, 18)[0] != 183:
        raise SystemExit(f'Not an Android arm64 ELF library: {source}')
target = project / 'android/app/src/main/jniLibs/arm64-v8a'
target.mkdir(parents=True, exist_ok=True)
for source in sources:
    dest = target / source.name
    shutil.copy2(source, dest)
    subprocess.run([str(strip), '--strip-debug', str(dest)], check=True)
    print(f'{dest.name}: {dest.stat().st_size / 1048576:.1f} MiB')
