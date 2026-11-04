# DDC/CI (NVIDIA yolu) — durum, kurulum, geri alma (branch `ddc-ci-claude`)

Kişisel, ticari olmayan fork (thebcd69). NullMoth kodu © NullMoth Systems, PolyForm Noncommercial 1.0.0 (`LICENSE`,
`NOTICE`); bu daldaki değişiklikler aynı lisansla. Upstream'e PR/issue açılmaz (`AGENTS.md`).
Taban: `fork/ddc` 87e01de (Mac'teki ajanın çalışması); `ddc` ve `main` değiştirilmez.
Öncelik: önce Intel iGPU görüntü/Metal/USB-C, DDC en son — bu dal sahibi bekletmez, paralel yürür.

## Erken reddin nedeni (bulundu, düzeltildi — donanımda henüz denenmedi)
`ddc` dalı, çerçeve tamponunun hangi RM ekranı olduğunu RM'in **önbellekteki EDID'i** (`GET_EDID_V2` + `COPY_CACHE`)
ile bizim EDID'i karşılaştırarak buluyordu. NVKMS, DisplayPort hedefinin EDID'ini RM üzerinden değil **kendi DP
kütüphanesiyle** okur (`nvkms-dpy.c` → `ReadEdidFromDP`); RM önbelleği DP ekranı için boş kalır. PG27AQDM NVIDIA'da
USB-C/DP üzerinden bağlı olduğundan her istek `display` aşamasında `kIOReturnNoDevice` ile, veri dönmeden düşer.

Düzeltme (`023f65c`): NVKMS ekran tanıtıcısı (`fDisplay`), MST olmayan bir ekranda bağlayıcının RM displayId bitidir
(`nvkms-rm.c` bağlayıcıları `GET_SUPPORTED` maskesinden kurar; `nvAllocDpyEvo(.., displayId)`; KAPI tanıtıcısı
`nvDpyIdToNvU32`). Artık doğrudan o kullanılıyor; MST'de (dinamik id) bus kapalı kalır.

Eski hipotez düzeltmesi: "`nvrm-kext-version` 2.7.1 değil" **yanlıştı** — resmî 2.7.1 paketinde NVRM `CFBundleVersion`
= 2.7.1. Dört talimat-bayt imzası da resmî NVRM'de (sha256 `9afb717c…`) birebir tutuyor (`tools/ddc/sig-check.py`).

## Bu daldaki değişiklikler
| Değişiklik | Neden |
|---|---|
| displayId ile eşleme (EDID önbelleği yerine) | yukarıdaki erken red |
| Sürüm dizesi yalnız bilgi; imzalar karar verir | yeniden derlenmiş aynı NVRM'i reddetmemek; farklı derleme imzada düşer |
| Yanıt beklemesinde (≥40 ms) ne `gS.lock` ne NVRM PM kapısı tutulur; yanıt aşamasında `NVRMGpuReady` yeniden sorulur | uyku başlarken istek beklemesi uykuyu geciktirmesin; GPU uykudaysa kapalı hata |
| `sendBytes`/`replyBytes` gerçekte taşınanı gösterir (DDC/CI yanıtı: uzunluk baytı + 3) | IOI2CInterface.h sözleşmesi |
| Adres kuralı `ddcXfer`'de ikinci kez: yazma yalnız 0x37 (ve EDID okumasını başlatan 1 baytlık 0x50 ofseti), okuma 0x37/0x50 | savunma derinliği |
| Gönderim ≤ 64 bayt, yanıt ≤ 256 bayt, gecikme ≤ 500 ms | sınırlı boyut/süre |
| Kapatma: `nvddc=0` **veya** `-nvrmnoddc` | iki ad da çalışsın |
| `tools/ddc/abi-check.sh`, `tools/ddc/sig-check.py` | RM struct kopyaları ↔ NVIDIA başlıkları; NVRM imzaları ↔ ikili |
| `.github/workflows/ddc.yml` (macos-14) | derleme + kontroller + ön sürüm `ddc-claude-<sha>` |

Linux'ta MacKernelSDK ile `nvrm-fb.cpp` sözdizimi denetimi `-Wall -Wextra` uyarısız geçti.

## CI
**Son başarılı çalışma:** `0ef8bed` (macos-14, Xcode 16.2) → ön sürüm **`ddc-claude-0ef8bed`**
(`NVRMFB-ddc-claude-0ef8bed.zip`: NVRMFB.kext, ddcprobe, ddc-install.sh, bu belge, SHA256SUMS, new-symbols.txt).
Resmî 2.7.1 NVRMFB'ye göre yeni çekirdek sembolleri yalnız `OSNumber::withNumber(ull,uint)`, `__memcpy_chk`,
`__strlcpy_chk`, `bzero`, `vsnprintf` — hepsi xnu `Libkern.exports`'ta.
`ddc.yml`: ABI kontrolü → resmî 2.7.1 paketinden NVRM indirilip SHA-256 ve imza kontrolü → `kexts/NVRMFB/build.sh`
(MacKernelSDK + OGKM 615.78.08) → `ddcprobe` → zip → ön sürüm `ddc-claude-<kısa sha>`.
**NVRM.kext CI'da derlenemez:** RM'in Darwin derlemesi (`_out/Darwin_x86_64/compile_cmds.sh`, `libnvkernel.a`,
`libnvkms.a`, NullMoth'un `build-nvrm.sh`'i) depoda yok; `rmcc.py` ve `build-completion-archive.py` bunları hazır
varsayar. Gerekli olan: NullMoth'un yayınlamadığı RM Darwin derleme betikleri. Bu yüzden DDC yalnız NVRMFB'de;
NVRM'e dokunulmaz (RM girişleri imzayla bulunur).

## Kurulum (Mac'te, NullMoth 2.7.1 kuruluyken)
1401 uygulamasının imzalı güncelleme denetimi değiştirilmiş kext'i kabul etmez; elle kurulur.
1. Ön sürümden `NVRMFB-ddc-claude-<sha>.zip` indir, aç: `shasum -a 256 -c SHA256SUMS`.
2. `sudo ./ddc-install.sh install NVRMFB.kext/Contents/MacOS/NVRMFB` — önce test koleksiyonu kurar, sonra yedekler
   (`/Library/NullMoth/backup-ddc-<tarih>.XXXX`), yalnız ikiliyi değiştirir, AuxKC'yi `kmutil` ile yeniden kurar.
3. Yeniden başlat. Monitör menüsünde **DDC/CI açık** olmalı.
4. Sına: `./ddcprobe` (VCP 0x10 okur), `./ddcprobe <fb> 10 50` (parlaklığı 50 yap).
5. Durum (sudo gerekmez): `ioreg -r -w0 -c NVRMNVDAFramebuffer -d1 | grep -E 'nvrm-ddc'`
   — `nvrm-ddc-io` her isteğin durduğu aşamayı ve hata kodunu gösterir.

## Geri alma
- `sudo ./ddc-install.sh rollback /Library/NullMoth/backup-ddc-<tarih>.XXXX` + yeniden başlat
  (önceki deneme yedeği: `/Library/NullMoth/backup-ddc-20261104-165944.NNlg`).
- Ya da resmî NullMoth 2.7.1 paketini `install.sh` ile yeniden kur.
- Açılmıyorsa: OpenCore menüsünde boot-arg'a `-nvrmnoddc` ekle (yalnız DDC kapanır) veya
  "1401: Remove NVIDIA driver" aracı. OpenCore config'i bu dal değiştirmez.

## PG27AQDM ve VCP 0x10
Doğrulanmış kaynak bulunamadı. OSD'de parlaklık olduğundan 0x10 beklenir (OLED'de ABL sınırlar). Donanımda `ddcprobe` ile
doğrulanacak.

## Sonraki adımlar
1. CI ön sürümünü kur, `ddcprobe` + `nvrm-ddc-io` çıktısını kaydet.
2. `at send`/`at reply` + RM hata kodu görülürse: `DP_AUXCH_I2C_TRANSFER_CTRL` GSP'de reddediliyor olabilir →
   yedek yol `NV0073_CTRL_CMD_DP_AUXCH_CTRL` (ham AUX, I2C-over-AUX komutlarıyla, ≤16 bayt parçalar).
3. ghelper-mac `ddc-slider` ve Intel `gu605-ddc` (ayrı depolar).

Ayrıntılı inceleme: `docs/DDC-REVIEW.md`.
