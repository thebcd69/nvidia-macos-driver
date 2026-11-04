# `ddc` branch incelemesi (fork/ddc @ 87e01de) — erken red nedeni ve bulgular

> **Güncelleme (023f65c):** §1'deki "en olası: `resolve`" hipotezi resmî 2.7.1 paketiyle çürütüldü (NVRM
> `CFBundleVersion` = 2.7.1, imzalar tutuyor). Asıl neden `display` aşaması: DP hedefinin EDID'i RM önbelleğinde yok.
> Ayrıntı ve düzeltme: `docs/DDC-STATUS.md`. §2'deki 3, 5, 6, 7. maddeler uygulandı.

Statik inceleme; donanım erişimi yok. Kaynaklar: Apple IOGraphics-600 (`IOFramebuffer.cpp`, `IOI2CInterface.cpp`),
NVIDIA open-gpu-kernel-modules 615.78.08, NullMoth v2.7.1 kaynağı.

## 1. Erken red — hangi ön koşul düşebilir?

`doI2CRequest` sırası: `bus/internal/lost` → `type` → `address` → `buffer` → `gpu-ready` → `resolve` → `client` →
`display` → `send`/`reply`. IOGraphics'in gönderdiği isteğe göre tek tek:

| Aşama | Düşer mi? | Gerekçe |
|---|---|---|
| `bus != 0` | hayır | `IOFramebufferI2CInterface::withFramebuffer` `fBusID`'yi `IOI2CInterfaceID`'den alır = 0 (IOFramebuffer.cpp ~15136). |
| tip / commFlags | hayır | `startIO` zaten `IOI2CTransactionTypes` ve `IOI2CSupportedCommFlags` ile süzer; ddcprobe/ddcctl Simple + DDCciReply, commFlags 0 gönderir. |
| adres | hayır | 0x6E/0x6F → 7-bit 0x37. |
| tampon | hayır | `extIO` tamponu çekirdekteki 1024 baytlık inline tampona koyar, boyutu `MIN(...,1024)`; 255/256 sınırı yeterli. |
| `fInternal` | düşük olasılık | Harici ekran için NVKMS `internal=0` olmalı; ioreg'deki `nvrm-ddc` "bus published" diyorsa zaten geçti (iç panelde bus yayınlanmaz). |
| `gpu-ready` | düşük | `NVRMGpuReady` yalnız sistem uykusunda NotReady döner. |
| **`resolve`** | **en olası** | `nvrm-kext-version` **tam olarak** `"2.7.1"` olmalı. Bu değer `kmod_info.version`, yani yükleyicinin damgaladığı `CFBundleVersion`. Depodaki NVRM `Info.plist`'inde `CFBundleVersion` = **0.1**; sürüm damgası yayın paketlemesinde yapılıyorsa 2.7.1 olur, yapılmıyorsa "0.1" görünür ve özellik kendini kapatır (`kIOReturnUnsupported`, veri yok). Ayrıca sahibin kurulu sürümü 2.6.6 ise (görev özetinde 2.6.6 yazıyor) imzalar 2.7.1'e göre olduğundan yine kapanır. |
| `client`/`display` | orta | `GET_EDID_V2 COPY_CACHE` eşleşmezse "no RM display has this EDID". NVRMFB'nin EDID'de değiştirdiği baytlar (12..15 seri, 127 checksum) atlanıyor; uzantı bloğu farkı karşılaştırılmıyor (yalnız ilk 128 bayt) — bu doğru. |

**Doğrulama (Mac'te, sudo gerekmez):**
```
ioreg -r -c NVRM -d1 | grep nvrm-kext-version
ioreg -r -w0 -c NVRMNVDAFramebuffer -d1 | grep -E 'nvrm-ddc|nvrm-ddc-io'
```
`nvrm-ddc-io` içinde `at resolve` görülürse neden yukarıdaki satırdır; `nvrm-ddc` metni sürüm/imza ayrımını söyler.

**Önerilen düzeltme (henüz uygulanmadı — sahibin kararı DDC'yi en sona aldı):** sürüm dizesini yalnız bilgi olarak
kaydet; asıl güvenlik zaten dört talimat-bayt imzası (`kBase`, `kRmapiOp`, `kPmEnter`, `kPmExit`). İmzalar tutuyorsa
sürüm farkı özelliği kapatmasın, tutmuyorsa kapatsın. 2.6.6 için ayrı bir imza tablosu gerekir (2.6.6 → 2.7.1 arasında
yalnız `kexts/NVRM/os-xnu.cpp` değişti, ama `__text` düzeni kaydığı için ofsetler farklıdır; 2.6.6 NVRM ikilisinden
`llvm-nm`/`llvm-objdump` ile çıkarılmalı — bu oturumda yayın dosyalarına erişim yoktu).

## 2. Diğer bulgular

1. **ABI doğru.** `tools/ddc/abi-check.sh` tüm RM struct kopyalarını (NVOS00/54/64, ATTACH_IDS, NV0080/2080 alloc,
   GET_SUPPORTED, GET_EDID_V2, GET_I2C_PORTID, OR_GET_INFO, DP_AUXCH_I2C_TRANSFER_CTRL, NV402C I2C_TRANSACTION) ve
   komut/sınıf sabitlerini NVIDIA başlıklarıyla alan alan karşılaştırır; hepsi tuttu.
2. **Yetki.** `DP_AUXCH_I2C_TRANSFER_CTRL` bayrakları 0x40 = KERNEL_PRIVILEGED; `rm_kernel_rmapi_op` çekirdek istemcisi
   olduğundan geçer. `NV402C I2C_TRANSACTION` 0x48 (NON_PRIVILEGED). `OR_GET_INFO` 0x46 (PRIVILEGED) — çekirdekten geçer.
3. **Kilit süresi.** `startIO` `fbLock()` tutarken çağırır; `doI2CRequest` ≥40 ms `IOSleep` yapıyor ve bu sırada
   NVRM PM kapısını (`pmEnter`) ve `gS.lock`'u da tutuyor → sistem uykusu bir isteğin süresi kadar gecikir. Öneri:
   gönderim → `pmExit` → uyku → `pmEnter` → okuma.
4. **`minReplyDelay`** mutlak zaman birimi; x86'da 1 tick = 1 ns, `absolutetime_to_nanoseconds` doğru.
5. **`replyBytes`/`sendBytes` güncellenmiyor** (başlık "gerçek bayt sayısı yazılır" der). İstemciler checksum'a bakıyor,
   pratikte zararsız.
6. **Aynı EDID'li iki monitör**: ilk eşleşen alınır (yanlış monitöre DDC — zararsız ama yanlış). Daha kesin eşleme:
   `fDisplay` (NVKMS `NvKmsKapiDisplay` = `nvDpyIdToNvU32(dpyId)`, MST olmayan ekranda RM displayId biti) doğrudan
   `displayMask` içinde aranabilir; EDID karşılaştırması yedek kalır.
7. **Boot-arg adı**: görev özeti `-nvrmnoddc` istiyordu, dal `nvddc=0` kullanıyor. İkisini de kabul etmek tek satır.
8. **0x50 yazımı**: alt adresli okumada 0x50'ye 1 bayt (ofset işaretçisi) yazılıyor; 1 bayt veri yazmaz, güvenli.
   2+ baytlık yazım 0x50'ye engelli — doğru.
