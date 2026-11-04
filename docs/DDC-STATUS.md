# DDC/CI — durum ve tasarım (branch `ddc-ci-claude`)

**Sahibin kararı (2026-11-04): DDC en son öncelik.** Önce Intel iGPU görüntü verecek, Metal çalışacak ve sol USB-C portu
harici monitörü sürecek; DDC ancak ondan sonra. Bu belge kaldığı yeri ve sonraki adımları kaydeder.

Bu branch `fork/ddc` (87e01de, Mac'teki ajanın çalışması) üzerine kuruludur; `ddc`, `main` değiştirilmedi.

## Yapıldı
- `tools/ddc/abi-check.sh` + `abi-check.cpp`: `nvrm-ddc.inc`'teki RM struct/sabit kopyalarını NVIDIA 615.78.08
  başlıklarıyla derleme zamanında alan alan karşılaştırır (Linux/macOS, `abi-check.sh <ogkm>`). Geçti; negatif test
  (alan yeri değişince derleme hatası) doğrulandı.
- `docs/DDC-REVIEW.md`: `ddc` dalının statik incelemesi, erken red nedeni adayları ve düzeltme önerileri.

## Yapılmadı (bilinçli olarak — öncelik değişti)
- NVIDIA tarafında kod düzeltmesi (öneriler DDC-REVIEW.md'de), CI iş akışı `ddc.yml`, `DDC-KURULUM.md`.
- Intel tarafı (`thebcd69/WTLIntelGfx`, `gu605-ddc`) — dal açılmadı; tasarım aşağıda.
- ghelper-mac `ddc-slider` — dal açılmadı; tasarım aşağıda.

## Araştırma özeti

**macOS DDC (Intel Mac yolu).** `IOFramebuffer::open()` → `IOFramebufferI2CInterface::create()` çerçeve tamponunun
`IOFBI2CInterfaceInfo` dizisindeki her sözlük için bir `IOI2CInterface` nub'ı oluşturur (anahtarlar: `IOI2CInterfaceID`,
`IOI2CBusType`, `IOI2CTransactionTypes`, `IOI2CSupportedCommFlags`). Kullanıcı tarafı `IOFBCopyI2CInterfaceForBus` →
`IOI2CInterfaceOpen` → `IOI2CSendRequest`; çekirdekte `IOI2CInterfaceUserClient::extIO` tamponları 1024 baytlık inline
tampona koyar, `IOFramebufferI2CInterface::startIO` `fbLock()` altında tip/commFlags süzer ve
`IOFramebuffer::doI2CRequest(bus, timing, req)` çağırır. Başarı dönüşünde tamamlama (`completion`) sürücünün işidir.
Kaynak: apple-oss-distributions/IOGraphics (IOGraphics-600) `IOFramebuffer.cpp`, `IOI2CInterface.cpp`, `IOI2CInterface.h`.

**DDC/CI protokolü (VESA DDC/CI 1.1, MCCS).** 7-bit adres 0x37 (yazma 0x6E, okuma 0x6F), kaynak adresi 0x51.
VCP okuma: `51 82 01 <vcp> <chk>`, chk = 0x6E XOR tüm baytlar; ≥40 ms bekle; yanıt 11 bayt
`6E 88 02 <sonuç> <vcp> <tip> <maxH> <maxL> <curH> <curL> <chk>` (chk 0x50 ile başlar). VCP yazma:
`51 84 03 <vcp> <H> <L> <chk>`, sonra ≥50 ms. Parlaklık 0x10, kontrast 0x12, giriş 0x60. EDID 0x50.

**DP ve HDMI.** HDMI: DDC hatları gerçek I2C (NVIDIA: `NV402C_CTRL_CMD_I2C_TRANSACTION`, port = `GET_I2C_PORTID.ddcPortId-1`;
Intel: GMBUS). DP / USB-C DP alt mode: I2C-over-AUX (NVIDIA: `NV0073_CTRL_CMD_DP_AUXCH_I2C_TRANSFER_CTRL`, 7-bit adres,
≤256 bayt, RM 16 baytlık AUX parçalarına böler; Intel: AUX_CH_CTL/DATA ile I2C-over-AUX, MOT biti, 16 bayt).
NVIDIA'nın Linux sürücüsü aynı ikiliyi kullanır (`osapi.c`: `RmDpAuxI2CTransfer`, `RmNonDPAuxI2CTransfer`).

**NullMoth.** NVRMFB yalnız EDID verir (`hasDDCConnect`/`getDDCBlock`), I2C arayüzü yok. NVRMFB RM'e yalnız NVKMS KAPI
tablosu ve `callPlatformFunction` ile ulaşır; KAPI'de I2C yok. NVRM `rm_kernel_rmapi_op`'u dışa vermez (Info.plist'te
`OSBundleCompatibleVersion` yok) ve NVRM bu depodan yeniden derlenemez (RM'in Darwin derlemesi, `compile_cmds.sh`,
`build-nvrm.sh` yayınlanmamış). Bu yüzden `ddc` dalı RM girişlerini `phys-for-va` adresinden sabit ofset + talimat
baytı imzasıyla buluyor (yalnız NullMoth 2.7.1 NVRM ikilisi). Alternatif (kernel değişikliği gerektirmeyen):
NVRM kullanıcı istemcisi (`NVRMUserClient`, `sEsc` → `rm_ioctl`) NVK için tam RM ioctl yolu sunar; HDMI'de
`NV402C I2C_TRANSACTION` NON_PRIVILEGED olduğundan kullanıcı alanından da yapılabilir, DP AUX I2C ise çekirdek ister
(`DP_AUXCH_CTRL` ham AUX root ile mümkün). Upstream'de DDC ile ilgili issue/tartışma bu oturumda incelenemedi
(GitHub API erişimi `nullmoth/*` için kapalıydı).

**PG27AQDM VCP 0x10.** Doğrulanmış bir kaynak bulunamadı (web aramasında modele özgü DDC/CI raporu yok). OLED'de ABL
parlaklığı sınırlar ama OSD'de "Parlaklık" ayarı olduğundan 0x10'un desteklenmesi beklenir; monitör menüsünde
**DDC/CI açık** olmalı (ASUS menülerinde Sistem Kurulumu altında). Donanımda `tools/ddcprobe` ile doğrulanmalı:
`ddcprobe` (tüm çerçeve tamponları, VCP 0x10 okur).

## Intel tasarımı (WTLIntelGfx, gelecekte `gu605-ddc`)
- Kanca: `kexts/XeGfxIntelFB/intelfb.cpp` — `XeGfxIntelFramebuffer : IONDRVFramebuffer`. Zaten MMIO ile eDP AUX
  (`XIFB_AUX_CTL 0x64010`) üzerinden I2C-over-AUX EDID okuyor (`xifbAux`, `xifbI2c`, sabit adres 0x50).
- Ekle: `start()`'ta `super::start()`'tan önce `IOFBI2CInterfaceInfo` (bus 0, tipler No/Simple/DDCci/Combined,
  commFlags `kIOI2CUseSubAddressCommFlag`); `doI2CRequest` override: yalnız 0x37 yazma, 0x37/0x50 okuma, ≤128 bayt,
  16 baytlık AUX parçaları, defer'de ≤7 deneme, toplam süre ≤250 ms, hata → `kIOReturnNoDevice`/`IOError` ve
  `completion`; boot-arg `-xeintelfbnoddc`. `xifbI2c` adresi parametre olmalı (bugün 0x50 sabit).
- Port: harici monitör sol USB-C/TB4 → Type-C DDI (TC1..4). AUX yazmaçları XE_LPDP düzeninde; **doğrulanacak**
  (Linux `drivers/gpu/drm/i915/display/intel_dp_aux_regs.h`, `_XELPDP_USBC1_AUX_CH_CTL` vb. — hafızadan 0x16F610..
  0x16FC10, bu depoda tanımlı değil). TC portunun DP-alt sahipliği/güç isteği (`intel_tc.c`) olmadan AUX yanıt vermez;
  bu yüzden önce harici ekran görüntüsü çalışmalı.
- HDMI (GU605MY'de HDMI NVIDIA'ya bağlı): Intel yolunda GMBUS gerekmez.

## ghelper-mac kaydırıcı tasarımı (gelecekte `ddc-slider`)
`IOServiceGetMatchingServices("IOFramebuffer")` → her biri için `IOFBCopyI2CInterfaceForBus(fb,0)`; VCP 0x10 okuması
(ddcprobe'daki paketler) başarılıysa kaydırıcıyı göster, değilse gizle. Yazma 50 ms aralıkla, sürüklemede son değer
(debounce ~150 ms). EDID üretici "AUS" + ürün koduyla PG27AQDM seçilir.

## Sonraki adımlar (DDC sırası geldiğinde)
1. Mac'te: `ioreg ... | grep nvrm-kext-version` ve `nvrm-ddc-io` — DDC-REVIEW.md §1 tablosuyla erken red aşamasını belirle.
2. `resolve` ise: sürüm dizesini bilgiye indir (imzalar kalsın); kurulu sürüm 2.6.6 ise 2.6.6 imzalarını ekle.
3. DDC-REVIEW.md §2'deki 3, 6, 7. maddeleri uygula; CI'ya `tools/ddc/abi-check.sh` adımını ekle.
4. Intel: harici ekran çalıştıktan sonra yukarıdaki tasarımı `gu605-ddc`'de uygula.
5. ghelper-mac `ddc-slider`.

Geri alma (NVIDIA `ddc` testi için): Mac'teki yedek `/Library/NullMoth/backup-ddc-20261104-165944.NNlg`; ya da resmi
NullMoth paketini `install.sh` ile yeniden kur (1401 uygulamasının imzalı güncelleme kontrolü değiştirilmiş kext'i
kabul etmez; elle kurulum yolu `install.sh`/`tools/ddc-install.sh`).
