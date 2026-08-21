# Data licences and permits

Every dataset shipped under `/data` is listed here with its source, licence and
Coğrafi Veri İzin Belgesi status. A data file with no entry here fails
`scripts/ci-gate-data-permits.sh` (piricad.md §12).

| Path | Dataset | Source | Licence | Permit | Status |
|---|---|---|---|---|---|
| `catalogs/mpyy/` | MPYY plan gösterim stil paketi (yapı; gösterim satırları henüz boş) | Mekânsal Planlar Yapım Yönetmeliği, EK-1 Gösterimler, 2014-06-14 | Resmî mevzuat metni, serbestçe yeniden yayımlanabilir (public domain) | Coğrafi Veri İzin Belgesi gerekmiyor — yönetmelik eki gösterim tanımı, coğrafi veri değil. Hukuk onayı BEKLİYOR; gösterim satırları eklenmeden önce alınacak (hedef 2026-09-30). | Yayımda (satırlar eksik) |
| `catalogs/bohhbuy/` | (none yet) | — | — | — | Phase 2 |
| `crs/tm3-dilimleri.json` | Türkiye TM 3° dilim orta meridyenleri | BÖHHBÜY, 2018-05-26 | Resmî mevzuat metni, serbestçe yeniden yayımlanabilir (public domain) | Coğrafi Veri İzin Belgesi gerekmiyor — mevzuat metnindeki parametre tablosu, coğrafi veri değil. Hukuk onayı 2026-08-21. | Yayımda |
| `crs/` (jeoit gridleri) | Jeoit gridleri, dönüşüm parametreleri | HGM / TKGM | — | Coğrafi Veri İzin Belgesi süreci beklemede — grid dosyası eklenmeden önce alınır | Faz 1 |
| `corpus/` | (none yet) | — | — | — | Phase 3 |

Legal sign-off precedes shipping any dataset. The Coğrafi Veri İzin Belgesi
process is listed in piricad.md §12 as the legal team's first task.
