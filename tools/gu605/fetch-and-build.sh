#!/bin/bash
# Fetches NVIDIA open-gpu-kernel-modules 620.70.06 (sparse, about 42 MB), applies upstream's patch chain, runs localbuild.sh.
# Run from the source folder (kexts/, build/, tools/, nvrm-fb.cpp.v3.0, localbuild.sh). Output: ../ddc3/NVRMFB + SHA256SUMS, build.log here.
cd "$(dirname "$0")"; exec > build.log 2>&1
set -uo pipefail
O=$PWD/ogkm620
if [ ! -d "$O/.git" ]; then
  git clone -q --depth 1 --branch 620.70.06 --filter=blob:none --sparse https://github.com/NVIDIA/open-gpu-kernel-modules "$O" || { echo "LOCALBUILD STOP: clone"; exit 1; }
  git -C "$O" sparse-checkout set src/nvidia-modeset src/common src/nvidia/arch/nvalloc/unix/include src/nvidia/src/kernel/gpu/timer || { echo "LOCALBUILD STOP: sparse"; exit 1; }
  for f in kexts/NVRM/patches/*.patch; do
    ok=0
    for root in "" "src/nvidia-modeset"; do
      if ( cd "$O/$root" && patch -p1 --fuzz=0 --dry-run < "$OLDPWD/$f" > /dev/null 2>&1 ); then ( cd "$O/$root" && patch -p1 --fuzz=0 < "$OLDPWD/$f" > /dev/null ); echo "OK   $(basename "$f")"; ok=1; break; fi
    done
    [ $ok = 1 ] || { echo "LOCALBUILD STOP: patch $(basename "$f")"; exit 1; }
  done
fi
du -sh "$O" | cut -f1
# the official binary to compare with: the one in the extracted release package (NOT a copy in /tmp: /tmp is emptied at restart)
OFF=${OFFICIAL_NVRMFB:-$HOME/Desktop/nvrm-ddc/nullmoth-3.0/pkg/pkgroot/Library/Extensions/NVRMFB.kext/Contents/MacOS/NVRMFB}
bash ./localbuild.sh "$O" "$OFF"
