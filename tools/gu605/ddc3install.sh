#!/bin/bash
# thebcd69 fork (branch ddc-v3): puts the fork-built NVRMFB binary into the installed NullMoth 3.0 NVRMFB.kext.
# Only Contents/MacOS/NVRMFB changes. The kext as installed is copied to the backup first; the auxiliary collection is test-built
# before anything is replaced, then rebuilt from /Library/Extensions (all kexts kept). Restart afterwards.
# Input: ~/Desktop/nvrm-ddc/ddc3/NVRMFB + SHA256SUMS (from the fork's pre-release). Undo: ddc3undo.sh (written here).
set -euo pipefail
PATH=/usr/bin:/bin:/usr/sbin:/sbin
D=/Users/baha/Desktop/nvrm-ddc; S=$D/ddc3; L=$D/nm30.log; t=$(date +%Y%m%d-%H%M%S)
exec >> "$L" 2>&1
echo "== ddc3install $t =="
EXT=/Library/Extensions; KC=/Library/KernelCollections/AuxiliaryKernelExtensions.kc
KB=/System/Library/KernelCollections/BootKernelExtensions.kc; KS=/System/Library/KernelCollections/SystemKernelExtensions.kc
K=/System/Library/Kernels/kernel; KARG=(--allow-missing-kdk); [ -f "$K" ] && KARG+=(--kernel "$K")
die() { echo "DDC3-INSTALL STOP: $*"; exit 1; }
[ "$(cat /Library/NullMoth/driver-version 2>/dev/null)" = 3.0 ] || die "NullMoth 3.0 is not the installed driver"
BIN=$S/NVRMFB; [ -f "$BIN" ] && [ -f "$S/SHA256SUMS" ] || die "no $S/NVRMFB + SHA256SUMS"
WANT=$(awk '$2 == "NVRMFB" { print $1 }' "$S/SHA256SUMS"); [ -n "$WANT" ] || die "SHA256SUMS has no NVRMFB line"
[ "$(shasum -a 256 "$BIN" | cut -d' ' -f1)" = "$WANT" ] || die "the binary does not match SHA256SUMS"
file "$BIN" | grep -q "Mach-O 64-bit kext bundle x86_64" || die "not an x86_64 kext binary"
OLD=$(shasum -a 256 "$EXT/NVRMFB.kext/Contents/MacOS/NVRMFB" | cut -d' ' -f1); echo "installed: $OLD"; echo "new:       $WANT"
[ "$OLD" != "$WANT" ] || { echo "already installed"; echo "DDC3-INSTALL DONE (already)"; exit 0; }
IDS="com.nullmoth.NVRM com.nullmoth.NVAccel com.nullmoth.NVRMFB com.nullmoth.NVRMAGDC org.watchthelight.XeGfxIntelFB org.watchthelight.XeGfxAccel org.watchthelight.XeGfxAGDC"
build_kc() {  # <repository> <output kc>
  kmutil create -n aux --volume-root / "${KARG[@]}" -B "$KB" -S "$KS" --repository "$1" -A "$2" -z > "$2.log" 2>&1 || { tail -20 "$2.log"; die "kmutil create failed"; }
  [ -s "$2" ] || die "kmutil produced no collection"
  local ins; ins=$(kmutil inspect -a x86_64 -A "$2" 2>/dev/null) || die "cannot inspect $2"
  for id in $IDS; do printf '%s\n' "$ins" | grep -Fq "$id" || { tail -20 "$2.log"; die "$id missing from the collection"; }; done
}
BK=/Library/XeGfx/nvrmfb-3.0-backup-$t; T=$(mktemp -d /var/tmp/nvrmfb-ddc3.XXXX); mkdir -p "$T/repo"
echo "1. test collection with the new NVRMFB"
for x in "$EXT"/*.kext; do ditto "$x" "$T/repo/$(basename "$x")"; done
cp "$BIN" "$T/repo/NVRMFB.kext/Contents/MacOS/NVRMFB"; chown -R root:wheel "$T/repo"; chmod -R go-w "$T/repo"
build_kc "$T/repo" "$T/test.kc"; echo "   ok"
echo "2. back up -> $BK"
mkdir -p "$BK"; ditto "$EXT/NVRMFB.kext" "$BK/NVRMFB.kext"; cp -p "$KC" "$BK/AuxiliaryKernelExtensions.kc"
[ "$(shasum -a 256 "$BK/NVRMFB.kext/Contents/MacOS/NVRMFB" | cut -d' ' -f1)" = "$OLD" ] || die "backup copy differs"
echo "3. install"
cp "$BIN" "$EXT/NVRMFB.kext/Contents/MacOS/NVRMFB.new" && chown root:wheel "$EXT/NVRMFB.kext/Contents/MacOS/NVRMFB.new" && chmod 755 "$EXT/NVRMFB.kext/Contents/MacOS/NVRMFB.new"
mv -f "$EXT/NVRMFB.kext/Contents/MacOS/NVRMFB.new" "$EXT/NVRMFB.kext/Contents/MacOS/NVRMFB"
[ "$(shasum -a 256 "$EXT/NVRMFB.kext/Contents/MacOS/NVRMFB" | cut -d' ' -f1)" = "$WANT" ] || { ditto "$BK/NVRMFB.kext" "$EXT/NVRMFB.kext"; die "copy differs; the old kext is back"; }
echo "4. live collection"
if ! ( build_kc "$EXT" "$T/new.kc" ); then ditto "$BK/NVRMFB.kext" "$EXT/NVRMFB.kext"; die "live collection failed; the old kext is back, the old collection untouched"; fi
mv -f "$T/new.kc" "$KC"; sync
cat > "$D/root/ddc3undo.sh" <<EOR
#!/bin/bash
# puts the official NullMoth 3.0 NVRMFB.kext and the collection from before ddc3 ($t) back
set -euo pipefail
exec >> "$L" 2>&1
echo "== ddc3undo \$(date +%Y%m%d-%H%M%S) =="
mkdir -p "$BK/replaced-\$\$"; ditto /Library/Extensions/NVRMFB.kext "$BK/replaced-\$\$/NVRMFB.kext"
cp "$BK/NVRMFB.kext/Contents/MacOS/NVRMFB" /Library/Extensions/NVRMFB.kext/Contents/MacOS/NVRMFB
cp -p "$BK/AuxiliaryKernelExtensions.kc" "$KC"; sync
[ "\$(shasum -a 256 /Library/Extensions/NVRMFB.kext/Contents/MacOS/NVRMFB | cut -d' ' -f1)" = "$OLD" ] && echo "DDC3-UNDO DONE" || echo "DDC3-UNDO MISMATCH"
EOR
chmod 700 "$D/root/ddc3undo.sh"
echo "backup: $BK"; echo "DDC3-INSTALL DONE"
