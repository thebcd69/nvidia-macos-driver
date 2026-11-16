#!/bin/bash
# kernel log lines about DDC/I2C from NullMoth since boot
O=/Users/baha/Desktop/nvrm-ddc/ddclog.txt
/usr/bin/log show --last boot --style compact --predicate 'process == "kernel" AND (eventMessage CONTAINS[c] "DDC" OR eventMessage CONTAINS[c] "i2c" OR eventMessage CONTAINS[c] "nvrmfbddc")' 2>&1 | grep -v -i "AppleSMC\|cs35\|vi2c\|VoodooI2C\|I2CHID\|touchpad" | tail -n 150 > "$O.tmp"
mv "$O.tmp" "$O"; chown baha "$O"; echo "DDCLOG DONE $(date +%H:%M:%S)" >> "$O"
