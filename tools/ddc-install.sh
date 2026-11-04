#!/bin/bash
# thebcd69 fork (branch ddc): put a fork-built NVRMFB binary into the installed NullMoth 2.7.1 NVRMFB.kext.
#
#   sudo tools/ddc-install.sh install  <NVRMFB binary>   back up, test-build, install, rebuild the aux collection
#   sudo tools/ddc-install.sh rollback <backup dir>      put the backed-up NVRMFB.kext back, rebuild the collection
#
# Only Contents/MacOS/NVRMFB changes; the installed Info.plist (and so the bundle version NullMoth wrote) stays.
# Nothing is deleted: the old kext is moved to the backup directory. A restart is needed afterwards.
# NullMoth's own boot guard still applies: two starts without a display turn the driver off and the Mac boots on the
# firmware framebuffer; then run rollback.
set -euo pipefail
EXT=/Library/Extensions; KC=/Library/KernelCollections/AuxiliaryKernelExtensions.kc
KB=/System/Library/KernelCollections/BootKernelExtensions.kc; KS=/System/Library/KernelCollections/SystemKernelExtensions.kc
K=/System/Library/Kernels/kernel; KARG=(--allow-missing-kdk); [ -f "$K" ] && KARG+=(--kernel "$K")
die() { echo "STOP: $*" >&2; exit 1; }
[ "$(id -u)" = 0 ] || die "run with sudo"
[ -f /Library/NullMoth/driver-version ] && [ "$(cat /Library/NullMoth/driver-version)" = 2.7.1 ] || die "NullMoth 2.7.1 is not the installed driver"

build_kc() {  # <repository> <output kc>
  kmutil create -n aux --volume-root / "${KARG[@]}" -B "$KB" -S "$KS" --repository "$1" -A "$2" -z > "$2.log" 2>&1 \
    || { tail -20 "$2.log"; die "kmutil create failed ($2.log)"; }
  [ -s "$2" ] || die "kmutil produced no collection"
  local ins; ins=$(kmutil inspect -a x86_64 -A "$2" 2>/dev/null) || die "cannot inspect $2"
  for id in com.nullmoth.NVRM com.nullmoth.NVRMFB com.nullmoth.NVAccel com.nullmoth.NVRMAGDC; do
    printf '%s\n' "$ins" | grep -Fq "$id" || { tail -20 "$2.log"; die "$id missing from $2"; }
  done
}

case "${1:-}" in
install)
  BIN=${2:-}; [ -f "$BIN" ] || die "no NVRMFB binary given"
  file "$BIN" | grep -q "Mach-O 64-bit kext bundle x86_64" || die "$BIN is not an x86_64 kext binary"
  [ -d "$EXT/NVRMFB.kext" ] || die "no $EXT/NVRMFB.kext"
  BK=$(mktemp -d "/Library/NullMoth/backup-ddc-$(date +%Y%m%d-%H%M%S).XXXX")
  T=$(mktemp -d /var/tmp/nvrmfb-ddc.XXXX); mkdir -p "$T/repo"
  echo "1. test collection with the new NVRMFB"
  for x in "$EXT"/*.kext; do [ "$(basename "$x")" = NVRMFB.kext ] || ditto "$x" "$T/repo/$(basename "$x")"; done
  ditto "$EXT/NVRMFB.kext" "$T/repo/NVRMFB.kext"; cp "$BIN" "$T/repo/NVRMFB.kext/Contents/MacOS/NVRMFB"
  chown -R root:wheel "$T/repo"; chmod -R go-w "$T/repo"
  build_kc "$T/repo" "$T/test.kc"
  echo "2. back up -> $BK"
  ditto "$EXT/NVRMFB.kext" "$BK/NVRMFB.kext"; cp -p "$KC" "$BK/AuxiliaryKernelExtensions.kc"
  shasum -a 256 "$EXT/NVRMFB.kext/Contents/MacOS/NVRMFB" "$BIN" > "$BK/SHA256"
  echo "3. install"
  mv "$EXT/NVRMFB.kext" "$BK/NVRMFB.kext.replaced"
  ditto "$T/repo/NVRMFB.kext" "$EXT/NVRMFB.kext"; chown -R root:wheel "$EXT/NVRMFB.kext"; chmod -R 755 "$EXT/NVRMFB.kext"
  echo "4. live collection"
  build_kc "$EXT" "$T/new.kc"
  mv -f "$T/new.kc" "$KC"
  echo "OK. Backup: $BK   Undo: sudo $0 rollback $BK   Then restart."
  ;;
rollback)
  BK=${2:-}; [ -d "$BK/NVRMFB.kext" ] || die "no NVRMFB.kext in backup $BK"
  T=$(mktemp -d /var/tmp/nvrmfb-ddc.XXXX)
  [ ! -d "$EXT/NVRMFB.kext" ] || mv "$EXT/NVRMFB.kext" "$BK/NVRMFB.kext.ddc-$(date +%H%M%S)"
  ditto "$BK/NVRMFB.kext" "$EXT/NVRMFB.kext"; chown -R root:wheel "$EXT/NVRMFB.kext"; chmod -R 755 "$EXT/NVRMFB.kext"
  build_kc "$EXT" "$T/new.kc"
  mv -f "$T/new.kc" "$KC"
  echo "OK: the original NVRMFB is back. Restart."
  ;;
*) die "usage: $0 install <NVRMFB binary> | rollback <backup dir>" ;;
esac
