#!/bin/bash
# Local build of NullMoth 3.0's NVRMFB (branch ddc-v3 of the thebcd69 fork), with the flags of upstream's build/fb_build.sh.
# Two binaries: ref/ from the unmodified v3.0 nvrm-fb.cpp (compared with the installed official one) and out/ from this branch.
# usage: localbuild.sh <open-gpu-kernel-modules 620.70.06 tree with the patch chain applied> <official NVRMFB binary>
set -uo pipefail
HERE=$(cd "$(dirname "$0")" && pwd); OGKM=$1; OFF=$2; KMS="$OGKM/src/nvidia-modeset"
SDK=$(xcrun --show-sdk-path); KHDR="$SDK/System/Library/Frameworks/Kernel.framework/Headers"
[ -d "$KHDR" ] || { echo "STOP: no Kernel.framework headers in $SDK"; exit 1; }
echo "SDK $SDK ($(xcrun --show-sdk-version)); $(xcrun clang++ --version | head -1)"
build() {  # <kexts dir> <out>
  local SRC=$1 OUT=$2; mkdir -p "$OUT/NVRMFB.kext/Contents/MacOS"
  local INC=( -I"$KHDR" -I"$SRC/NVRMFB" -I"$SRC/NVRMFB/fb"
      -I"$OGKM/src/nvidia/arch/nvalloc/unix/include" -I"$OGKM/src/common/sdk/nvidia/inc" -I"$OGKM/src/common/inc" -I"$OGKM/src/common/shared/inc"
      -I"$KMS/os-interface/include" -I"$KMS/kapi/interface" -I"$KMS/kapi/include" -I"$KMS/interface" -I"$KMS/include"
      -I"$OGKM/src/common/unix/common/inc" -I"$OGKM/src/common/modeset" -I"$OGKM/src/common/unix/common/utils/interface" )
  local CFLAGS=( -arch x86_64 -mmacosx-version-min=15.0 -fapple-kext -mkernel -nostdinc "${INC[@]}" -DKERNEL -DKERNEL_PRIVATE -DDRIVER_PRIVATE -DAPPLE -DNeXT
      -std=c++17 -fno-rtti -fno-exceptions -fno-builtin -fno-common
      -Wall -Wno-unused-parameter -Wno-inconsistent-missing-override -Wno-unused-function -O2 )
  xcrun clang++ "${CFLAGS[@]}" -c "$SRC/NVRMFB/fb/nvrm-fb.cpp" -o "$OUT/nvrm-fb.o" 2>"$OUT/fb.err" || { grep -E "error" "$OUT/fb.err" | head -20; return 1; }
  xcrun clang++ -arch x86_64 -mmacosx-version-min=15.0 -fapple-kext -nostdlib -Xlinker -kext -lkmodc++ -lkmod -lcc_kext "$OUT/nvrm-fb.o" \
      -o "$OUT/NVRMFB.kext/Contents/MacOS/NVRMFB" 2>"$OUT/fbld.err" || { head "$OUT/fbld.err"; return 1; }
  echo "  $(basename "$OUT"): compile warnings $(grep -c ': warning:' "$OUT/fb.err"), link warnings $(grep -c 'warning' "$OUT/fbld.err")"
}
rm -rf "$HERE/refsrc" "$HERE/ref" "$HERE/out"; cp -R "$HERE/kexts" "$HERE/refsrc"; cp "$HERE/nvrm-fb.cpp.v3.0" "$HERE/refsrc/NVRMFB/fb/nvrm-fb.cpp"
echo "source difference (this branch against v3.0):"; diff "$HERE/refsrc/NVRMFB/fb/nvrm-fb.cpp" "$HERE/kexts/NVRMFB/fb/nvrm-fb.cpp" | grep -c '^[<>]'
build "$HERE/refsrc" "$HERE/ref" || { echo "LOCALBUILD STOP: reference build"; exit 1; }
build "$HERE/kexts" "$HERE/out" || { echo "LOCALBUILD STOP: branch build"; exit 1; }
REF=$HERE/ref/NVRMFB.kext/Contents/MacOS/NVRMFB; NEW=$HERE/out/NVRMFB.kext/Contents/MacOS/NVRMFB
sec() { otool -s "$2" "$3" "$1" | tail -n +3 | awk '{ $1 = ""; print }' | shasum -a 256 | cut -c1-16; }
echo "official / reference built here / this branch:"
for b in "$OFF" "$REF" "$NEW"; do
  echo "  $(shasum -a 256 "$b" | cut -c1-16) $(stat -f %z "$b") B  text $(sec "$b" __TEXT __text) cstring $(sec "$b" __TEXT __cstring) const $(sec "$b" __TEXT __const) data $(sec "$b" __DATA __data) text-size $(size -m "$b" | awk '/Section __text/ { print $3; exit }')"
done
nm -u "$OFF" | sort > "$HERE/u-off.txt"; nm -u "$REF" | sort > "$HERE/u-ref.txt"; nm -u "$NEW" | sort > "$HERE/u-new.txt"
echo "undefined symbols: official $(wc -l < "$HERE/u-off.txt" | tr -d ' '), reference $(wc -l < "$HERE/u-ref.txt" | tr -d ' '), branch $(wc -l < "$HERE/u-new.txt" | tr -d ' ')"
echo "official vs reference: $(diff "$HERE/u-off.txt" "$HERE/u-ref.txt" > /dev/null && echo identical || echo DIFFERENT)"; diff "$HERE/u-off.txt" "$HERE/u-ref.txt" | head -8
echo "official vs branch:    $(diff "$HERE/u-off.txt" "$HERE/u-new.txt" > /dev/null && echo identical || echo DIFFERENT)"
file "$NEW" | cut -c1-120
xcrun clang -O2 -Wall -arch x86_64 -mmacosx-version-min=15.0 -framework IOKit -framework CoreFoundation "$HERE/tools/ddcprobe.c" -o "$HERE/out/ddcprobe" && echo "ddcprobe built"
mkdir -p "$HERE/../ddc3"; cp "$NEW" "$HERE/../ddc3/NVRMFB"; cp "$HERE/out/ddcprobe" "$HERE/../ddc3/ddcprobe"
(cd "$HERE/../ddc3" && shasum -a 256 NVRMFB ddcprobe > SHA256SUMS && cat SHA256SUMS)
echo "LOCALBUILD DONE"
