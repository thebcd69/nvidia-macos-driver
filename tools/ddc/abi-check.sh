#!/bin/bash
# abi-check.sh <open-gpu-kernel-modules 615.78.08 checkout> -- see abi-check.cpp. Runs on Linux or macOS (x86_64
# SysV and macOS x86_64 lay these plain C structs out identically).
set -euo pipefail
OGKM=$1; HERE=$(cd "$(dirname "$0")" && pwd); INC="$HERE/../../kexts/NVRMFB/fb/nvrm-ddc.inc"
T=$(mktemp -d); trap 'rm -rf "$T"' EXIT
# the mirror block: from "namespace nvddc {" up to the entry-point section, then close the namespace
awk '/^namespace nvddc \{/{on=1} /^\/\* ---- NVRM entry points/{on=0} on' "$INC" > "$T/nvddc-mirror.h"
grep -q 'struct I2cTrans' "$T/nvddc-mirror.h" || { echo "STOP: mirror block not found in $INC"; exit 1; }
echo '}' >> "$T/nvddc-mirror.h"
S="$OGKM/src/common/sdk/nvidia/inc"; U="$OGKM/src/nvidia/arch/nvalloc/unix/include"
A=-m64; [ "$(uname)" = Darwin ] && A="-arch x86_64"
${CXX:-clang++} -std=c++17 -fsyntax-only $A -Wno-unused-value \
  -I"$T" -I"$S" -I"$S/ctrl" -I"$S/class" -I"$OGKM/src/common/inc" -I"$U" "$HERE/abi-check.cpp"
echo "abi-check: nvrm-ddc.inc RM mirrors match open-gpu-kernel-modules ($(git -C "$OGKM" describe --tags 2>/dev/null || echo "$OGKM"))"
