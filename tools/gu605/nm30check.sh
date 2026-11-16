#!/bin/bash
# Dry run of the NullMoth 3.0 installer (CHECK=1: package integrity + test kernel collection with every kext of /Library/Extensions; changes nothing).
set -u; PATH=/usr/bin:/bin:/usr/sbin:/sbin
D=/Users/baha/Desktop/nvrm-ddc/nullmoth-3.0; P=$D/pkg/pkgroot; R=$D/NVIDIA-Linux-x86_64-620.70.06.run; L=/Users/baha/Desktop/nvrm-ddc/nm30.log
exec >> "$L" 2>&1
echo "== nm30check $(date +%Y%m%d-%H%M%S) =="
ioreg -r -c IOPCIDevice -d 1 | grep -q '"vendor-id" = <de100000>' || { echo "NM30-CHECK STOP: no NVIDIA GPU on PCI (boot in Ultimate)"; exit 1; }
[ "$(shasum -a 256 "$R" | cut -c1-64)" = 29118e3abd08c07559bcca3d9c76238d039490af0362a2e5bcfcbe7204afd4b1 ] || { echo "NM30-CHECK STOP: the NVIDIA package does not match its pin"; exit 1; }
echo "ok  NVIDIA 620.70.06 package matches the pin"
cd "$P" && CHECK=1 NULLMOTH_FW_RUN="$R" /bin/bash ./install.sh; rc=$?
echo "NM30-CHECK rc=$rc"
