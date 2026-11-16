#!/bin/bash
# NullMoth 3.0 package (GitHub release) + NVIDIA 620.70.06 driver package (GSP firmware source). Resumable.
cd "$(dirname "$0")"
R=https://github.com/nullmoth/nvidia-macos-driver/releases/download/v3.0
for f in SHA256SUMS.txt nullmoth-nvidia-3.0.tar.gz.sig nullmoth-nvidia-3.0.tar.gz.bsig nullmoth-nvidia-3.0.tar.gz; do
  curl -fL --proto '=https' --tlsv1.2 --retry 5 --retry-delay 5 -C - -sS -o "$f" "$R/$f" || echo "FAIL $f"
done
echo "PKG $(date +%H:%M:%S) $(shasum -a 256 nullmoth-nvidia-3.0.tar.gz)"
N=NVIDIA-Linux-x86_64-620.70.06.run
for i in 1 2 3 4 5 6 7 8; do
  curl -fL --proto '=https' --tlsv1.2 --retry 5 --retry-delay 5 -C - -sS -o "$N" "https://download.nvidia.com/XFree86/Linux-x86_64/620.70.06/$N" && break
  echo "retry $i $(date +%H:%M:%S)"; sleep 5
done
echo "RUN $(date +%H:%M:%S) $(shasum -a 256 $N)"
echo "DL DONE"
