#!/bin/bash
# NullMoth 3.0 DDC/CI: adds the boot-arg nvrmfbddc=1 to OpenCore's config.plist on the KIOXIA EFI (found by media name).
# One text change (the boot-args string). The old file stays beside it as config.plist.before-nvrmfbddc-<time>; written file is read back.
# Runs only with the owner's OK (file ~/Desktop/nvrm-ddc/nm30-ddc-ok).
set -euo pipefail
D=/Users/baha/Desktop/nvrm-ddc; t=$(date +%Y%m%d-%H%M%S); L=$D/nm30.log
exec >> "$L" 2>&1
echo "== nm30ddc $t =="
[ -f "$D/nm30-ddc-ok" ] || { echo "NM30-DDC STOP: no owner OK"; exit 1; }
id=""
for d in $(diskutil list physical | sed -nE 's#^/dev/(disk[0-9]+) .*#\1#p'); do
  diskutil info "$d" | grep -q 'Media Name: *KIOXIA' || continue
  diskutil list "$d" | grep -q 'Apple_APFS' || continue
  efi=$(diskutil list "$d" | awk '$2 == "EFI" { print $NF; exit }'); [ -n "$efi" ] || continue
  id=$efi; break
done
[ -n "$id" ] || { echo "NM30-DDC STOP: no KIOXIA EFI"; exit 1; }
mp=$(diskutil info "$id" | sed -nE 's/.*Mount Point: *//p')
if [ -z "$mp" ]; then diskutil mount "$id" > /dev/null; mp=$(diskutil info "$id" | sed -nE 's/.*Mount Point: *//p'); fi
C=$mp/EFI/OC/config.plist; [ -f "$C" ] || { echo "NM30-DDC STOP: no config.plist at $mp/EFI/OC"; exit 1; }
echo "EFI $id ($(diskutil info "${id%s*}" | sed -nE 's/.*Media Name: *//p')) at $mp"
S0=$(shasum -a 256 "$C" | cut -d' ' -f1); echo "before: $S0"
N=/private/tmp/config.nm30ddc.$t.plist
rc=0
/usr/bin/python3 - "$C" "$N" <<'PY' || rc=$?
import re, sys
b = open(sys.argv[1], 'rb').read()
m = re.findall(rb'<key>boot-args</key>\s*<string>([^<]*)</string>', b)
if len(m) != 1: sys.exit("boot-args: %d matches" % len(m))
old = m[0]
if b'nvrmfbddc=' in old: sys.exit(3)
new = old.rstrip() + b' nvrmfbddc=1'
out = re.sub(rb'(<key>boot-args</key>\s*<string>)[^<]*(</string>)', lambda k: k.group(1) + new + k.group(2), b, count=1)
assert len(out) == len(b) + len(new) - len(old)
open(sys.argv[2], 'wb').write(out)
print("boot-args now: " + new.decode())
PY
[ $rc != 3 ] || { echo "nvrmfbddc is already in boot-args: nothing to do"; echo "NM30-DDC DONE (already)"; exit 0; }
[ $rc = 0 ] || { echo "NM30-DDC STOP: edit rc $rc"; exit 1; }
plutil -lint "$N" > /dev/null || { echo "NM30-DDC STOP: the edited file does not parse"; exit 1; }
echo "difference:"; diff <(cat "$C") <(cat "$N") | cut -c1-80 | head -6 || true
[ "$(diff <(cat "$C") <(cat "$N") | grep -c '^[<>]')" = 2 ] || { echo "NM30-DDC STOP: more than one line differs"; exit 1; }
WANT=$(shasum -a 256 "$N" | cut -d' ' -f1)
B=$mp/EFI/OC/config.plist.before-nvrmfbddc-$t
cp -p "$C" "$B"; [ "$(shasum -a 256 "$B" | cut -d' ' -f1)" = "$S0" ] || { echo "NM30-DDC STOP: backup copy differs"; exit 1; }
cp "$N" "$C.new-$t"; sync
[ "$(shasum -a 256 "$C.new-$t" | cut -d' ' -f1)" = "$WANT" ] || { echo "NM30-DDC STOP: the copy on the EFI differs (config.plist untouched)"; mv "$C.new-$t" "$C.badcopy-$t"; exit 1; }
mv -f "$C.new-$t" "$C"; sync
S1=$(shasum -a 256 "$C" | cut -d' ' -f1); echo "after:  $S1"
[ "$S1" = "$WANT" ] || { echo "MISMATCH: putting the old file back"; cp -p "$B" "$C"; sync; exit 1; }
find "$mp/EFI/OC" -maxdepth 1 -name '._config*' -print | sed 's/^/stray: /'
echo "backup: $B"; echo "$B" > "$D/nm30-ddc-backup.txt"; chown baha:staff "$D/nm30-ddc-backup.txt"
cat > "$D/root/nm30ddcundo.sh" <<EOR
#!/bin/bash
# puts the config.plist from before nvrmfbddc=1 ($t) back on the KIOXIA EFI; the edited one is kept beside it
set -euo pipefail
exec >> "$L" 2>&1
id=""
for d in \$(diskutil list physical | sed -nE 's#^/dev/(disk[0-9]+) .*#\\1#p'); do
  diskutil info "\$d" | grep -q 'Media Name: *KIOXIA' || continue
  efi=\$(diskutil list "\$d" | awk '\$2 == "EFI" { print \$NF; exit }'); [ -n "\$efi" ] || continue; id=\$efi; break
done
mp=\$(diskutil info "\$id" | sed -nE 's/.*Mount Point: *//p'); [ -n "\$mp" ] || { diskutil mount "\$id" > /dev/null; mp=\$(diskutil info "\$id" | sed -nE 's/.*Mount Point: *//p'); }
C=\$mp/EFI/OC/config.plist; B=\$C.before-nvrmfbddc-$t
[ "\$(shasum -a 256 "\$B" | cut -d' ' -f1)" = "$S0" ] || { echo "NM30-DDC-UNDO STOP: backup differs"; exit 1; }
cp -p "\$C" "\$C.with-nvrmfbddc-$t"; cp -p "\$B" "\$C"; sync
[ "\$(shasum -a 256 "\$C" | cut -d' ' -f1)" = "$S0" ] && echo "NM30-DDC-UNDO DONE" || echo "NM30-DDC-UNDO MISMATCH"
EOR
chmod 700 "$D/root/nm30ddcundo.sh"
echo "NM30-DDC DONE"
