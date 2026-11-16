#!/bin/bash
# NullMoth 3.0 install (the package's own install.sh, firmware from the already downloaded NVIDIA package), then checks for the Intel side.
set -u; PATH=/usr/bin:/bin:/usr/sbin:/sbin
D=/Users/baha/Desktop/nvrm-ddc/nullmoth-3.0; P=$D/pkg/pkgroot; R=$D/NVIDIA-Linux-x86_64-620.70.06.run; L=/Users/baha/Desktop/nvrm-ddc/nm30.log
exec >> "$L" 2>&1
echo "== nm30install $(date +%Y%m%d-%H%M%S) =="
ioreg -r -c IOPCIDevice -d 1 | grep -q '"vendor-id" = <de100000>' || { echo "NM30-INSTALL STOP: no NVIDIA GPU on PCI (boot in Ultimate)"; exit 1; }
[ -s /Library/XeGfx/nullmoth-2.7.1-backup.latest ] || { echo "NM30-INSTALL STOP: run nm30backup first"; exit 1; }
cd "$P" && NULLMOTH_FW_RUN="$R" /bin/bash ./install.sh; rc=$?
echo "install.sh rc=$rc"
[ $rc = 0 ] || { echo "NM30-INSTALL STOP: installer returned $rc (it restores its own backup on a failed step; see above)"; exit 1; }
KC=/Library/KernelCollections/AuxiliaryKernelExtensions.kc; INS=$(kmutil inspect -a x86_64 -A "$KC" 2>/dev/null); miss=""
for id in com.nullmoth.NVRM com.nullmoth.NVAccel com.nullmoth.NVRMFB com.nullmoth.NVRMAGDC org.watchthelight.XeGfxIntelFB org.watchthelight.XeGfxAccel org.watchthelight.XeGfxAGDC; do
  printf '%s\n' "$INS" | grep -q -F "$id" && echo "ok  in the collection: $id" || { echo "MISSING from the collection: $id"; miss=1; }
done
for k in NVRM NVAccel NVRMFB NVRMAGDC; do echo "$k $(/usr/libexec/PlistBuddy -c 'Print :CFBundleShortVersionString' /Library/Extensions/$k.kext/Contents/Info.plist) $(shasum -a 256 /Library/Extensions/$k.kext/Contents/MacOS/$k | cut -c1-16)"; done
ls /Library/NullMoth/nvfw/nvidia; ls -d /Library/NullMoth/backup-* | tail -1
# Eco boots have no NVIDIA card on PCI: the new boot guard watches WindowServer together with debug.nvaccelfb, a name the Intel accelerator
# also publishes, so on an Eco boot it must stand aside (it would count Intel-side WindowServer crashes against the NVIDIA driver).
BG=/Library/NullMoth/bootguard.sh
if [ -f "$BG" ] && ! grep -q "gu605: Eco boot" "$BG"; then
  cp -p "$BG" "$BG.nullmoth-3.0" || { echo "NOTE: cannot keep a copy of the boot guard; left as installed"; BGSKIP=1; }
  [ -n "${BGSKIP:-}" ] || awk '{print} /^PATH=/ && !d {print "ioreg -r -c IOPCIDevice -d 1 | grep -q \x27\"vendor-id\" = <de100000>\x27 || exit 0   # gu605: Eco boot, no NVIDIA card: not this driver\x27s start"; d=1}' "$BG.nullmoth-3.0" > "$BG.tmp" \
    && bash -n "$BG.tmp" && chmod 755 "$BG.tmp" && chown root:wheel "$BG.tmp" && mv -f "$BG.tmp" "$BG" && echo "ok  boot guard stands aside on Eco boots (original: $BG.nullmoth-3.0)" || echo "NOTE: boot guard left as installed"
  sed -n '1,6p' "$BG"
fi
[ -z "$miss" ] && echo "NM30-INSTALL DONE" || echo "NM30-INSTALL DONE-BUT-CHECK (a kext is missing from the collection; do not restart yet)"
