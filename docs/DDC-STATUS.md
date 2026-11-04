# DDC/CI — durum (branch `ddc-ci-claude`)

Bu branch `fork/ddc` (87e01de, Mac'teki ajanın çalışması) üzerine kuruludur; `ddc` ve `main`'e dokunulmaz.

| Kalem | Durum |
|---|---|
| `tools/ddc/abi-check.sh`: nvrm-ddc.inc içindeki RM struct kopyaları ↔ NVIDIA 615.78.08 başlıkları | yapıldı, Linux'ta geçti (negatif test: alan yeri değişince derleme hatası) |
| Erken red nedeni (statik inceleme) | sürüyor — bkz. docs/DDC-REVIEW.md (gelecek commit) |
| CI (`.github/workflows/ddc.yml`) | yapılmadı |

Öncelik değişti: birincil iş Intel iGPU yolu (thebcd69/WTLIntelGfx, branch `gu605-ddc`). Bu branch ikincil.
