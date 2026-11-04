#!/bin/bash
# build.sh <MacKernelSDK> <open-gpu-kernel-modules 615.78.08 checkout> <out dir> [version]  ->  <out>/NVRMFB.kext
# thebcd69 fork: NVRMFB had no build script upstream; flags follow kexts/NMHDA/build.sh.
set -euo pipefail
cd "$(dirname "$0")/fb"
MKSDK=$1; OGKM=$2; OUT=$3; VER=${4:-2.7.1}
mkdir -p "$OUT"; OUT=$(cd "$OUT" && pwd)
K="$MKSDK/Headers"; [ -d "$K" ] || { echo "STOP: no MacKernelSDK at $MKSDK"; exit 1; }
[ -f "$OGKM/kernel-open/nvidia-modeset/nvkms.h" ] || { echo "STOP: no open-gpu-kernel-modules at $OGKM"; exit 1; }
rm -rf "$OUT/NVRMFB.kext"; mkdir -p "$OUT/NVRMFB.kext/Contents/MacOS"
FLAGS=(-arch x86_64 -mmacosx-version-min=15.0 -fapple-kext -mkernel -nostdinc -I"$K" -I.. -I.
       -I"$OGKM/kernel-open/nvidia-modeset" -I"$OGKM/kernel-open/common/inc"
       -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT
       -std=c++17 -fno-rtti -fno-exceptions -fno-builtin -fno-common -O2 -Wall -Wno-unused-parameter -g0)
xcrun clang++ "${FLAGS[@]}" -c nvrm-fb.cpp -o "$OUT/nvrm-fb.o"
xcrun clang++ -arch x86_64 -mmacosx-version-min=15.0 -fapple-kext -nostdlib -Xlinker -kext -L"$MKSDK/Library/x86_64" -lkmodc++ -lkmod -lcc_kext \
  "$OUT/nvrm-fb.o" -o "$OUT/NVRMFB.kext/Contents/MacOS/NVRMFB"
cp Info.plist "$OUT/NVRMFB.kext/Contents/Info.plist"   # $VER is informational only: the official kext keeps 0.1
plutil -lint "$OUT/NVRMFB.kext/Contents/Info.plist"
/usr/libexec/PlistBuddy -c 'Print :CFBundleVersion' "$OUT/NVRMFB.kext/Contents/Info.plist"
echo "NVRMFB $(shasum -a 256 "$OUT/NVRMFB.kext/Contents/MacOS/NVRMFB" | cut -c1-8) $(stat -f %z "$OUT/NVRMFB.kext/Contents/MacOS/NVRMFB") B"
echo "undefined symbols:"; nm -u "$OUT/NVRMFB.kext/Contents/MacOS/NVRMFB" | sort > "$OUT/undefined.txt"; wc -l < "$OUT/undefined.txt"
