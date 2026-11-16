# GU605MY: NullMoth güncellemesi ve DDC düzeltmesi

Kişisel fork notu (thebcd69/nvidia-macos-driver, dal `ddc-v3`). Upstream'e gönderilmek için değildir.
İlk yazım: 16 Kasım 2026, NullMoth 2.7.1 → 3.0 güncellemesi sırasında. Amaç: bir sonraki NullMoth
güncellemesinde aynı araştırmayı baştan yapmamak.

Makine: ASUS ROG Zephyrus G16 GU605MY, macOS Sequoia 15.8.1 (OpenCore), RTX 4090 Laptop, harici monitör
ASUS PG27AQDM. NullMoth yalnız **Ultimate** ve **Standard** modda (NVIDIA PCI'da görünürken) çalışır;
**Eco**'da masaüstünü bizim Intel sürücümüz (XeGfx, thebcd69/WTLIntelGfx) verir.

---

## 1. Bugünkü durum (16 Kasım 2026, 14:25)

| Parça | Sürüm | SHA-256 (ilk 16) |
|---|---|---|
| NVRM | 3.0 (resmî) | `ba6184e9a0926876` |
| NVAccel | 3.0 (resmî) | `d4ced924df69dea1` |
| NVRMAGDC | 3.0 (resmî) | `97623c55c4a804fd` |
| **NVRMFB** | **3.0 + iki DDC düzeltmesi (yerel derleme, commit 546a67c)** | `6396b5186d92e8a5` |
| NVRMFB resmî 3.0 (yedekte) | 3.0 | `4d3c4c026629f8d3` |
| GPU firmware | 620.70.06 | kurucunun sabitlediği dört dosya |

- `config.plist` boot-args sonuna `nvrmfbddc=1` eklendi (tek satır; öncesi `ed4df99c588bf343…`, sonrası
  `81b2192dab29f8bb…`). Eski dosya EFI'de: `EFI/OC/config.plist.before-nvrmfbddc-20261116-134407`.
- Doğrulananlar (Ultimate): Metal 3, dahili panel 240 Hz, PG27AQDM "2560 × 1440" HiDPI 240 Hz (2.7.1'de 59 Hz'di),
  `ddcprobe` VCP 0x10 okuma/yazma, BetterDisplay'den donanım parlaklığı (sahip doğruladı),
  BetterDisplay CLI `get -namelike=PG27 -ddc -vcp=luminance` → `60`.
- 1401 Mac uygulaması 3.0 `/Applications`'a kuruldu (kurucu olarak kullanılmadı).

## 2. Fork neden hâlâ gerekli: upstream 3.0'daki iki hata

Upstream 2.9'dan beri kendi DDC/CI desteğini taşıyor (`nvrmfbddc=1`, varsayılan kapalı; NVRM tarafında
`kexts/NVRM/nvrm_ddc.cpp`, NVRMFB tarafında `publishDdcBus()` ve `doI2CRequest()`). Eski 2.7.1 fork'umuzdaki
kod (`nvrm-ddc.inc`) artık kullanılmıyor. Ama 3.0'ın NVRMFB'sinde DDC'yi bu makinede işlemez kılan iki eksik var.
İkisi de `kexts/NVRMFB/fb/nvrm-fb.cpp` içinde; yama: `tools/gu605/nvrmfb-ddc-3.0.patch`.

### 2.1 I2C arayüzü hiç oluşmuyor (`IOI2CInterfaceID` yok)

- Belirti: çekirdek kaydında `fb1: DDC/CI bus published (nvrmfbddc=1)` var, framebuffer'da `IOFBI2CInterfaceInfo`
  var, ama kayıt ağacında `IOFramebufferI2CInterface` yok; `ddcprobe` her framebuffer için `I2C bus 0 absent` der.
- Neden: `IOFramebufferI2CInterface::start()` (IOGraphicsFamily, `IOFramebuffer.cpp`) veri yolu sözlüğünde
  `IOI2CInterfaceID` bulamazsa hemen çıkar. Upstream sözlüğe yalnız `IOI2CBusType`, `IOI2CTransactionTypes`,
  `IOI2CSupportedCommFlags` koyuyor.
- Düzeltme: `publishDdcBus()` içinde sözlüğe `kIOI2CInterfaceIDKey = 0` eklemek.
- Düzeldiğinin kanıtı: `ioreg -r -c IOFramebufferI2CInterface -d 1` bir düğüm gösterir; `ddcprobe`
  `framebuffer 1: I2C bus 0 present` ve `VCP 0x10 = 60 / 100` der.

### 2.2 Harici ekranın kayıt yolu çözülmüyor (`NVRMDisplay` düğümlerinin adresi yok)

- Belirti: 2.1'den sonra `ddcprobe` okur ve yazar, ama BetterDisplay'de DDC yok; BetterDisplay CLI `Failed.` der;
  çekirdek kaydında DDC hata satırı yok (istek sürücüye hiç gelmiyor).
- Neden: NVRM her ekran için aynı adlı, adressiz bir `NVRMDisplay` düğümü yayınlar (bu makinede dört tane).
  Upstream birim adresini yalnız framebuffer'a veriyor (`NVRMFramebuffer@1`). Yol
  `…/GFX0@0/NVRM/NVRMDisplay/NVRMFramebuffer@1` aranınca ilk `NVRMDisplay` (dahili panelin düğümü) alınır,
  altında `@1` yoktur: `IORegistryEntryFromPath` boş döner. BetterDisplay Intel Mac'lerde framebuffer'a bu yoldan gider.
- Düzeltme: framebuffer'ın birim adresini sağlayıcı düğüme de vermek
  (`getProvider()->setLocation(loc)`, `super::start()`'tan önce). 2.7.1 fork'undaki 244b8a9 ile aynı fikir.
- Düzeldiğinin kanıtı: `ioreg -r -c NVRM -d 3` çıktısında `NVRMDisplay@1 / NVRMFramebuffer@1`;
  `tools/gu605/regpath.c` ile `…/NVRM/NVRMDisplay@1/NVRMFramebuffer@1` çözülür.

### 2.3 Upstream'e bildirme

Upstream'in `AGENTS.md` dosyası yapay zekâ ile yazılmış issue/PR kabul etmediğini söylüyor; bu fork'un kuralı da
aynı (upstream'e issue/PR yok). Bildirmek istersen kendi sözlerinle, 2.1 ve 2.2'deki belirti ve kanıtlarla
yazabilirsin. Upstream düzeltirse bu fork'a gerek kalmaz.

## 3. Bir sonraki NullMoth güncellemesi: adım adım

Betikler `tools/gu605/` altında, 3.0 için **çalıştırıldıkları halleriyle** duruyor (Mac'te
`~/Desktop/nvrm-ddc/root/` ve `~/Desktop/nvrm-ddc/ddc3src/`). Yeni sürümde değişecek yerler: sürüm adı (`3.0`),
paket klasörü (`nullmoth-3.0`), paket SHA-256'sı, NVIDIA sürümü ve `.run` SHA-256'sı. Kök betikleri
`kms-helper4.sh` üzerinden çalışır (`echo sh-<ad> > /private/tmp/gu605-h4/next`; her açılışta bir parola).

**0. Ön koşullar**
- G-Helper **Ultimate**, monitör sağ (NVIDIA) portta. Kurucu ve kuru çalıştırması PCI'da NVIDIA yoksa durur
  (`no NVIDIA GPU found on PCI`): Eco'da hiçbir adım çalışmaz.
- Sahip bilgisayarın başında; indirme için izin (aşağıdaki boyutlarla); config.plist/EFI yazımı için ayrıca "evet".

**1. Ne geldiğine bak (bulutta ya da Mac'te, bir şey değiştirmeden)**
```
git fetch https://github.com/nullmoth/nvidia-macos-driver --tags
git diff vESKİ vYENİ -- package/install.sh package/fw_fetch.sh        # kurucu neye dokunuyor
git diff vESKİ vYENİ -- kexts/NVRMFB/fb/nvrm-fb.cpp | grep -n -E "publishDdcBus|kIOI2CInterfaceIDKey|nvrmFbLocation|setLocation"
git show vYENİ:CHANGELOG.md | head -80
curl -s 'https://api.github.com/repos/nullmoth/nvidia-macos-driver/releases?per_page=3'    # dosya adları ve boyutlar
```
Kurucuda her seferinde bakılacaklar (3.0'da bulduklarım 5. bölümde): `KEXTS=` listesi (yeni kext geliyor mu,
özellikle `NMIntelFB`), `/Library/GPUBundles`'ta sildiği/yazdığı şeyler, boot-args/NVRAM yazımı, açılış bekçisi
(`bootguard.sh`), firmware sürümü (`fw_fetch.sh` içindeki `VER=` ve `SHA_RUN=`).

**2. Düzeltmeler hâlâ gerekli mi?**
- 2.1: yeni `nvrm-fb.cpp` içinde `publishDdcBus()` `kIOI2CInterfaceIDKey` koyuyor mu?
- 2.2: `nvrmFbLocation` satırında sağlayıcı düğüme de `setLocation` yapılıyor mu (ya da NVRM düğümleri adresli mi)?
- İkisi de upstream'de varsa: resmî paketi kur, 7. adımdaki sınamaları yap, fork'a dokunma.

**3. İndir ve doğrula** (`tools/gu605/nm30dl.sh`)
- `nullmoth-nvidia-<sürüm>.tar.gz` (3.0: 65,6 MB) + `SHA256SUMS.txt`: GitHub release. Tarball'ın SHA-256'sı
  `SHA256SUMS.txt` ile, paket içi `shasum -c SHA256SUMS` ile doğrulanır.
- GPU firmware: kurucu, kurulu firmware sabitlenen değerlerle tutuyorsa **yeniden indirmez**. NVIDIA sürümü
  değişmişse `NVIDIA-Linux-x86_64-<sürüm>.run` gerekir (620.70.06: 550 MB, `download.nvidia.com`); elle indirip
  `NULLMOTH_FW_RUN=<dosya>` ile vermek, kurucunun kendi indirmesinden iyidir (devam ettirilebilir, önceden doğrulanır).
- Paketteki `install.sh`, `fw_fetch.sh`, `uninstall.sh` SHA-256'ları ilgili etiketin git kaynağıyla karşılaştırılır
  (3.0'da birebir aynıydı).

**4. Yedek** (`nm30backup.sh`, her modda çalışır)
Kurulu dört kext, `NVMTLDriver.bundle`, `nvmtl`, `/Library/NullMoth` (yedekleri hariç), LaunchDaemon'lar ve
`AuxiliaryKernelExtensions.kc` → `/Library/XeGfx/nullmoth-<sürüm>-backup-<saat>`. Kurucu ayrıca kendi yedeğini alır
(`/Library/NullMoth/backup-<saat>.XXXX`).

**5. Kuru çalıştırma, sonra kurulum** (`nm30check.sh`, `nm30install.sh`; Ultimate)
- Kuru çalıştırma: paket klasöründe `CHECK=1 NULLMOTH_FW_RUN=<.run> bash ./install.sh` → `CHECK PASS (nothing changed)`.
  Deneme çekirdek koleksiyonunu `/Library/Extensions`'ın **tamamıyla** kurar; bizim Intel kext'lerimiz bozuksa burada durur.
- Kurulum: aynı komut `CHECK=1` olmadan. Etkileşim yok, argüman yok.
- `nm30install.sh` sonrasında şunları yapar: yeni koleksiyonda yedi kimliği arar (`com.nullmoth.NVRM`, `NVAccel`,
  `NVRMFB`, `NVRMAGDC`, `org.watchthelight.XeGfxIntelFB`, `XeGfxAccel`, `XeGfxAGDC`); açılış bekçisine Eco satırını
  ekler (5.3).
- 1401 uygulaması yerine `install.sh` kullanılır: 1401'in "Experimental" bölmesi `NMIntelFB` kurabilir (5.2).

**6. Yeniden başlat: ilk açılış Ultimate olmalı**
Kurucunun otomatik geri dönüşü (`com.nullmoth.lastgood`) yalnız NVIDIA ile açılışta sınanır: iki açılış üst üste
ekran gelmezse eski sürücüyü geri koyup yeniden başlatır. Açılışta kontrol:
`kmutil showloaded --list-only | grep nullmoth`, `system_profiler SPDisplaysDataType`,
`tail /Library/NullMoth/bootguard.log`, `nvram -p | grep nullmoth` (`nullmoth-nv-ok 1`).

**7. DDC**
- `nvram boot-args` içinde `nvrmfbddc=1` var mı? (config.plist'te; `nm30ddc.sh` ekler: KIOXIA EFI'yi ad ile bulur,
  tek satırı değiştirir, eskisini yanına koyar, geri okur. Sahibin "evet"i gerekir.)
- Sınama sırası:
  1. `ioreg -r -c IOFramebufferI2CInterface -d 1` → düğüm var mı (yoksa 2.1)
  2. `~/Desktop/nvrm-ddc/ddc3/ddcprobe` → `framebuffer 1: I2C bus 0 present`, `VCP 0x10 = N / 100`
     (`ddcprobe 1 10 35` yazar; eski değeri geri yaz)
  3. `ioreg -r -c NVRM -d 3 | grep NVRMDisplay` → `NVRMDisplay@1` var mı (yoksa 2.2)
  4. `/Applications/BetterDisplay.app/Contents/MacOS/BetterDisplay get -namelike=PG27 -ddc -vcp=luminance` → sayı
  5. Çekirdek kaydı: `ddclog.sh` (kök; `log show` kullanıcı olarak boş döner)
- Düzeltme gerekiyorsa: 4. bölümdeki yerel derleme → `ddc3install.sh` → yeniden başlat → 1–4'ü tekrarla.

## 4. NVRMFB'yi yerelde derlemek (bir dakika)

GitHub Actions'ı bekleme: 16 Kasım'da macOS çalıştırıcısı işi 9 dakika kuyrukta tuttu. Mac'te Command Line Tools
yetiyor (SDK 15.5, Apple clang 17; Xcode gerekmez).

1. Kaynak klasörü (`~/Desktop/nvrm-ddc/ddc3src/`): yeni sürümün `kexts/` altındaki **bütün başlıklar**
   (`*.h`, `*.inc`; `nvrm-fb.cpp` `../../NMHDA/nmhda_topology.h` gibi komşu kext başlıklarını ister),
   `kexts/NVRMFB/fb/nvrm-fb.cpp` (yamalı), `nvrm-fb.cpp.v<sürüm>` (yamasız, karşılaştırma için),
   `kexts/NVRM/patches/`, `tools/ddcprobe.c`, `localbuild.sh`, `fetch-and-build.sh`.
2. Yama: `git apply tools/gu605/nvrmfb-ddc-3.0.patch` (uymazsa iki küçük değişiklik elle: 2.1 ve 2.2).
3. `fetch-and-build.sh`:
   - NVIDIA `open-gpu-kernel-modules` etiketi = NullMoth'un o sürümde kullandığı NVIDIA sürümü (3.0: `620.70.06`;
     `DEVELOPING.md` ve `fw_fetch.sh` söyler). Seyrek klon, yaklaşık 42 MB:
     `src/nvidia-modeset src/common src/nvidia/arch/nvalloc/unix/include src/nvidia/src/kernel/gpu/timer`.
   - Upstream'in yama zinciri (`kexts/NVRM/patches/*.patch`, `patch -p1 --fuzz=0`, önce ağaç kökünden, olmazsa
     `src/nvidia-modeset`'ten). **Atlanamaz**: `620-color-range.patch` NVRMFB'nin kullandığı `nvkms-kapi.h`'yi
     değiştiriyor; yamasız başlıkla derlenen NVRMFB, NVRM ile yapı düzeni uyuşmazlığına düşer.
   - Bayraklar upstream'in `build/fb_build.sh`'ındakilerle aynı (`localbuild.sh`). Upstream'in betiği doğrudan
     kullanılamıyor: SDK 15.5'in `libkmod`'u 15.0 hedefine "built for newer macOS" uyarısı veriyor, upstream
     her uyarıda duruyor (`warn_ceiling.sh`). Uyarı zararsız (bağlayıcı, dört satır).
4. Çıktı: `~/Desktop/nvrm-ddc/ddc3/NVRMFB` + `SHA256SUMS` + `ddcprobe`. Betik üç dosyayı karşılaştırır:
   resmî / yamasız kaynağın burada derlenmişi / yamalı. 3.0'da: çekirdekten istenen 505 sembol üçünde aynı,
   `__cstring` ve `__const` resmî ile yamasız derlemede aynı; `__text` aynı değil (derleyici sürümü farklı).
   Resmî dosya olarak paketteki NVRMFB'yi ver; `/tmp`'deki kopya yeniden başlatmada silinir.
5. Kurulum: `ddc3install.sh` (kök). Sürücü sürümünü denetler (betikteki `3.0`'ı güncelle), yalnız
   `NVRMFB.kext/Contents/MacOS/NVRMFB`'yi değiştirir, önce deneme koleksiyonu kurar, kext'i ve koleksiyonu
   `/Library/XeGfx/nvrmfb-<sürüm>-backup-<saat>` altına yedekler, canlı koleksiyonu yedi kimlikle doğrular,
   `ddc3undo.sh` yazar. Sonra yeniden başlatma.

CI (`.github/workflows/ddc-v3.yml`) aynı işi yapıp `ddc3-<sha>` ön sürümü yayınlar; yedek yol. Çalıştırıcıda en eski
Xcode 16 seçilmeli (SDK'sının `libkmod`'u 15.0 için; yoksa bağlayıcı uyarısıyla durur).

## 5. Intel (XeGfx) tarafıyla etkileşim: 3.0'da bulunanlar

1. **Dokunulmayanlar**: `/Library/Extensions`'taki XeGfx kext'leri, `XeMTLDriver.bundle`, `xemtl`. Kurucu
   koleksiyonu `/Library/Extensions`'ın tamamından kurar, bizimkiler içinde kalır (kurulumdan sonra doğrulandı).
   Ortak çekirdek sınıf adı sayısı 2.7.1'dekiyle aynı (17; yeni çakışma yok).
2. **`NMIntelFB`** (upstream'in hibrit panel sürücüsü): her Intel ekran aygıtına, 60000 öncelikle eşleşir.
   Varsayılan pakette **yok**; 1401'in "Experimental → Hybrid laptops / Select all" seçeneğiyle gelir. Kurulursa
   Eco'da `XeGfxIntelFB` ile aynı aygıta talip olur. Açma.
3. **Açılış bekçisi** (`/Library/NullMoth/bootguard.sh`, 3.0'da yeni mantık): WindowServer art arda ölürse ve
   `debug.nvaccelfb` 1 okunuyorsa NVIDIA sürücüsünü sonraki açılış için kapatır, giriş ekranındaysa makineyi yeniden
   başlatır. `debug.nvaccelfb`'yi bizim Intel hızlandırıcımız da yayınlar (Eco'da 1). Bu yüzden betiğe, `PATH=`
   satırından sonra, "PCI'da NVIDIA yoksa çık" satırı eklendi; aslı `bootguard.sh.nullmoth-3.0`. **Her NullMoth
   kurulumu bu dosyayı baştan yazar**: `nm30install.sh` satırı yeniden ekler.
4. **`NVIDIAShared.bundle`** 3.0'da siliniyor. `XeMTLDriver` içinde yolu geçiyor ama yalnız `NVMTL_VENDOR_COMPILER`
   geliştirici anahtarıyla kullanılıyor; normalde yüklenmiyor (Finder'ın yüklü kitaplıklarından doğrulandı).
5. **`air-runtime`**: `XeMTLDriver.bundle` kendi kopyasını taşır. 2.7.1'inki ayrıca
   `/Library/XeGfx/nullmoth-2.7.1-backup-20261116-131245/GPUBundles/NVMTLDriver.bundle` içinde.
6. `gfxrst=4` config.plist'te olduğu için kurucu NVRAM boot-args'a yazmadı. Kurucu `aircache`'i "başkaları
   yazabiliyor" diye sıfırladı: ilk girişte gölgelendiriciler yeniden derlenir.
7. Eco'ya ilk dönüşte bakılacak: `gu605-auto` özeti (`came up … first start`, `confirmed`), `debug.nvaccel_xeflip_copied`.
   (16 Kasım 14:25 itibarıyla 3.0 kurulumundan sonra Eco açılışı **henüz yapılmadı**.)

## 6. Yedekler ve geri dönüş

| Ne | Nerede |
|---|---|
| 2.7.1 kurulumunun tamamı (DDC'li eski NVRMFB dahil), 678 MB | `/Library/XeGfx/nullmoth-2.7.1-backup-20261116-131245` |
| Kurucunun kendi 2.7.1 yedeği | `/Library/NullMoth/backup-20261116-133720.cPk5An` |
| Resmî 3.0 NVRMFB.kext + o anki koleksiyon | `/Library/XeGfx/nvrmfb-3.0-backup-20261116-141419` |
| İlk düzeltmeli NVRMFB (yalnız 2.1) | `/Library/XeGfx/nvrmfb-3.0-backup-20261116-142059` |
| `nvrmfbddc=1` öncesi config.plist | EFI: `EFI/OC/config.plist.before-nvrmfbddc-20261116-134407` |
| 3.0 paketi, NVIDIA `.run`, 1401 zip | `~/Desktop/nvrm-ddc/nullmoth-3.0/` |
| Kaynak, betikler, derleme kaydı | `~/Desktop/nvrm-ddc/ddc3src/`, `~/Desktop/nvrm-ddc/root/`, `~/Desktop/nvrm-ddc/nm30.log` |

- Resmî NVRMFB'ye dönmek: `…-141419/NVRMFB.kext/Contents/MacOS/NVRMFB`'yi yerine kopyala,
  `…-141419/AuxiliaryKernelExtensions.kc`'yi `/Library/KernelCollections/`'a koy, yeniden başlat
  (`ddc3undo.sh` son kurulumun bir öncesine döner, yani 142059 yedeğine).
- DDC'yi kapatmak: `nm30ddcundo.sh` (config.plist'i geri koyar) ya da boot-args'tan `nvrmfbddc=1`'i çıkar.
- 2.7.1'e dönmek: paket klasöründe `sudo ./uninstall.sh /Library/NullMoth/backup-20261116-133720.cPk5An`
  (kurucunun söylediği yol; denenmedi). Otomatik geri dönüş işi ilk iyi açılıştan sonra kendini kaldırdı.
- Hiçbir şey silinmedi; eski dosyalar taşındı ya da kopyalandı.

## 7. Bu güncellemede zaman kaybettirenler

- GitHub release sayfasını web'den okumak güvenilmez çıktı; sürüm ve boyutlar için GitHub API ya da `git fetch --tags`.
- CI kuyruğu ve çalıştırıcıdaki yeni Xcode SDK'sı (bkz. 4). Yerel derleme ilk tercih.
- Kaynak paketine yalnız `kexts/NVRMFB` koymak yetmedi (komşu kext başlıkları).
- Karşılaştırma için resmî dosyayı `/tmp`'ye koymak: yeniden başlatmada silindi.
- Buluttan Mac'e aynı adla ikinci kez dosya göndermek eski içeriği yazdı; her gönderimde yeni ad kullan.
- Kök yardımcı her açılışta yeniden başlatılır (parola penceresi); DDC denemeleri üç yeniden başlatma sürdü
  (boot-arg, 2.1, 2.2). 2.1 ve 2.2 birlikte uygulanırsa tek yeniden başlatma yeter.

## 8. Açık kalanlar

- Ultimate'ta dahili panelin her açılışta yanması: BetterDisplay 4.3.7'de "Connection management settings" altında
  harici ekran bağlanınca dahiliyi kesme, "Auto-connect all displays on startup", "Preserve soft-disconnected state
  on startup", "Disconnect reconnected displays upon wake" ayarları var; sahip yerini bulamadı, sonra bakılacak.
- Standard modda "çizim NVIDIA'da, görüntü Intel'in sol USB-C'sinden": upstream'in `nmhybrid` yolu yalnız dahili
  paneli hedefliyor ve hiçbir hibrit dizüstünde çalıştırılmamış. Plan: `XeGfxIntelFB`'yi NullMoth 3.0 hızlandırıcısına
  ekran sağlayıcı olarak bağlamak (`nmHybridImport`, `kexts/NVRM/accel/nm_hybrid_abi.h`). Ayrı iş; bu belgede değil.
- 1401 uygulamasının, değiştirilmiş NVRMFB'yi nasıl gördüğü incelenmedi. 1401'den ya da `install.sh` ile yapılan her
  güncelleme/yeniden kurulum resmî NVRMFB'yi geri yazar; 4. bölüm tekrarlanır.
