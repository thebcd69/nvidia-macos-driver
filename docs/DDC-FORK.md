# DDC/CI fork (thebcd69, branch `ddc`)

Personal fork for an ASUS ROG Zephyrus G16 GU605MY driving an ASUS PG27AQDM over the USB-C DP alt-mode port.
Not a NullMoth release and not offered upstream (see AGENTS.md).

## What it adds
NVRMFB publishes an I2C bus per external display (`IOFBI2CInterfaceInfo`) and implements
`IOFramebuffer::doI2CRequest`, so apps that use DDC/CI on Intel Macs (ASUS DisplayWidget Center, BetterDisplay DDC,
ddcctl, MonitorControl) can set the monitor's own brightness/contrast instead of dimming in software.
Code: `kexts/NVRMFB/fb/nvrm-ddc.inc` (header comment explains the RM path and the safety checks).

- DP sinks: `NV0073_CTRL_CMD_DP_AUXCH_I2C_TRANSFER_CTRL` (I2C over AUX); others: `NV402C_CTRL_CMD_I2C_TRANSACTION`.
- Writes only to 0x37 (DDC/CI); reads from 0x37 and 0x50 (EDID). Internal panel: no bus.
- RM entry points are located from NVRM's `phys-for-va` address and verified against NullMoth **2.7.1**'s NVRM
  instruction bytes; with any other NVRM the feature turns itself off.
- `nvddc=0` boot-arg disables it. Status: `ioreg -r -w0 -c NVRMNVDAFramebuffer -d1 | grep nvrm-ddc`.

## Build
`kexts/NVRMFB/build.sh <MacKernelSDK> <open-gpu-kernel-modules 615.78.08> <out>`; CI: `.github/workflows/nvrmfb-ddc.yml`
(pre-release `nvrmfb-ddc-<sha>` with `NVRMFB.kext`).
