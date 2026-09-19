#!/bin/bash
# Build the Thor Turnip driver: Mesa with the patches in ./patches, cross-compiled for
# Android aarch64 with the NDK, packaged as an adrenotools zip.
#
# Host: Ubuntu in WSL with meson, ninja, python3-mako, python3-yaml, ccache, unzip, xz.
# Usage: tools/turnip/build.sh [work-dir]      (default work dir: $HOME/turnip-thor)
# Output: <work-dir>/Turnip-Thor-<mesa>-r<rev>.zip
set -euo pipefail

MESA_VERSION=26.2.2
PACKAGE_REVISION=1
NDK_VERSION=r27c
ROOT=${1:-$HOME/turnip-thor}
HERE=$(cd "$(dirname "$0")" && pwd)
NDK=$ROOT/android-ndk-$NDK_VERSION
T=$NDK/toolchains/llvm/prebuilt/linux-x86_64

mkdir -p "$ROOT/pkgconfig"
cd "$ROOT"

if [ ! -d "$NDK" ]; then
  curl -sSfL -o ndk.zip "https://dl.google.com/android/repository/android-ndk-$NDK_VERSION-linux.zip"
  unzip -q ndk.zip && rm ndk.zip
fi

if [ ! -d mesa ]; then
  curl -sSfL -o mesa.tar.xz "https://archive.mesa3d.org/mesa-$MESA_VERSION.tar.xz"
  tar xJf mesa.tar.xz && mv "mesa-$MESA_VERSION" mesa && rm mesa.tar.xz
  for p in "$HERE"/patches/*.patch; do
    (cd mesa && patch -p1 < "$p")
  done
fi

cat > android-aarch64.ini <<INI
[binaries]
ar = '$T/bin/llvm-ar'
c = ['ccache', '$T/bin/aarch64-linux-android27-clang']
cpp = ['ccache', '$T/bin/aarch64-linux-android27-clang++', '-fno-exceptions', '-fno-unwind-tables', '-fno-asynchronous-unwind-tables', '-static-libstdc++', '-Wno-c++11-narrowing']
c_ld = 'lld'
cpp_ld = 'lld'
strip = '$T/bin/llvm-strip'
pkg-config = ['env', 'PKG_CONFIG_LIBDIR=$ROOT/pkgconfig', '/usr/bin/pkg-config']

[host_machine]
system = 'android'
cpu_family = 'aarch64'
cpu = 'armv8'
endian = 'little'
INI

cat > pkgconfig/zlib.pc <<PC
prefix=$T/sysroot/usr
libdir=\${prefix}/lib/aarch64-linux-android/27
includedir=\${prefix}/include

Name: zlib
Description: zlib compression library
Version: 1.2.11
Libs: -L\${libdir} -lz
Cflags: -I\${includedir}
PC

cd mesa
if [ ! -d build ]; then
  meson setup build --cross-file "$ROOT/android-aarch64.ini" \
    -Dbuildtype=release -Dplatforms=android -Dplatform-sdk-version=27 \
    -Dandroid-stub=true -Dandroid-libbacktrace=disabled -Dandroid-strict=false \
    -Dgallium-drivers= -Dvulkan-drivers=freedreno -Dfreedreno-kmds=kgsl \
    -Dstrip=false -Degl=disabled -Dllvm=disabled -Dshared-llvm=disabled \
    -Dzstd=disabled -Dxmlconfig=disabled -Dtools= -Dbuild-tests=false -Dcpp_rtti=false
fi
ninja -C build src/freedreno/vulkan/libvulkan_freedreno.so

OUT="$ROOT/Turnip-Thor-$MESA_VERSION-r$PACKAGE_REVISION.zip"
python3 "$HERE/package.py" build/src/freedreno/vulkan/libvulkan_freedreno.so "$OUT" \
  "$MESA_VERSION" "$PACKAGE_REVISION"
echo "BUILD_OK $OUT"
