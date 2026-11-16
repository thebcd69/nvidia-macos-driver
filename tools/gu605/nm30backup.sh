#!/bin/bash
# Our own copy of the installed NullMoth 2.7.1 (with the DDC NVRMFB) before the 3.0 update. Copies only; nothing is removed or changed.
set -u; PATH=/usr/bin:/bin:/usr/sbin:/sbin
TS=$(date +%Y%m%d-%H%M%S); B=/Library/XeGfx/nullmoth-2.7.1-backup-$TS; L=/Users/baha/Desktop/nvrm-ddc/nm30.log
exec >> "$L" 2>&1
echo "== nm30backup $TS =="
stop() { echo "NM30-BACKUP STOP: $*"; exit 1; }
mkdir -p "$B/Extensions" "$B/GPUBundles" "$B/NullMoth" "$B/LaunchDaemons" "$B/KernelCollections" || stop mkdir
for k in NVRM NVAccel NVRMFB NVRMAGDC; do ditto /Library/Extensions/$k.kext "$B/Extensions/$k.kext" || stop "$k"; done
for b in NVMTLDriver.bundle NVIDIAShared.bundle nvmtl nvmtl-allow.txt; do [ ! -e /Library/GPUBundles/$b ] || ditto /Library/GPUBundles/$b "$B/GPUBundles/$b" || stop "$b"; done
for f in /Library/NullMoth/*; do n=$(basename "$f"); case "$n" in backup-*|install.lock) ;; *) ditto "$f" "$B/NullMoth/$n" || stop "$n";; esac; done
cp -p /Library/LaunchDaemons/com.nullmoth.*.plist "$B/LaunchDaemons/" || stop launchdaemons
cp -p /Library/KernelCollections/AuxiliaryKernelExtensions.kc "$B/KernelCollections/" || stop kc
nvram -p 2>/dev/null | grep -i -E "nullmoth|^boot-args" > "$B/nvram.txt"
for k in NVRM NVAccel NVRMFB NVRMAGDC; do
  a=$(shasum -a 256 /Library/Extensions/$k.kext/Contents/MacOS/$k | cut -c1-64); b=$(shasum -a 256 "$B/Extensions/$k.kext/Contents/MacOS/$k" | cut -c1-64)
  [ "$a" = "$b" ] || stop "copy of $k differs"; echo "ok  $k $(echo $a | cut -c1-16)"
done
a=$(shasum -a 256 /Library/KernelCollections/AuxiliaryKernelExtensions.kc | cut -c1-64); b=$(shasum -a 256 "$B/KernelCollections/AuxiliaryKernelExtensions.kc" | cut -c1-64)
[ "$a" = "$b" ] || stop "copy of the collection differs"; echo "ok  aux collection $(echo $a | cut -c1-16)"
[ -f "$B/GPUBundles/NVMTLDriver.bundle/Contents/MacOS/air-runtime" ] && echo "ok  air-runtime kept ($(shasum -a 256 "$B/GPUBundles/NVMTLDriver.bundle/Contents/MacOS/air-runtime" | cut -c1-16))"
du -sh "$B" | cut -f1; echo "$B" > /Library/XeGfx/nullmoth-2.7.1-backup.latest
echo "NM30-BACKUP DONE $B"
