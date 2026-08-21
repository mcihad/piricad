# /src/domain/geodesy — jeodezi

`piricad.md` §12 bu modülle açılıyor. Bugün çalışan kısmı: TUREF/TM 3° dilim
kataloğu ve PROJ tabanlı koordinat dönüşümü.

| Dosya | İş |
|---|---|
| `crs_catalog.hpp` | `data/crs/tm3-dilimleri.json` dosyasını okur. Dilim tablosu BÖHHBÜY verisidir, koda gömülmez (CLAUDE.md 5.13) |
| `transform.hpp` | PROJ sarmalayıcı. Matematiği PROJ yapar; sarmalayıcının işi PROJ'un sizin yerinize karar vermediği tek şeydir: **eksen sırası** |

## Neden bir sarmalayıcı var

EPSG:5254 (TUREF/TM30) eksenlerini `AXIS["northing (X)", ORDER 1]`,
`AXIS["easting (Y)", ORDER 2]` diye bildirir — Türk haritacılık konvansiyonu,
matematiğin tersi. PiriCAD sağa değeri `Point2::x` içinde saklar. Her dönüşüm bu
yüzden `proj_normalize_for_visualization()` üzerinden kurulur; o çağrı olmadan
koordinat sessizce takla atar ve makul görünen yanlış bir sonuç çıkar.

İkinci tuzak: hedef coğrafi bir CRS ise çıktı **derecedir**. 29,830716 dereceyi en
yakın milimetreye yuvarlamak noktayı yaklaşık yüz metre kaydırır. Milimetre alan
`forward(span<Point2>)` bu durumu reddeder; derece için skaler sürüm kullanılır.
İkisi de testle tutuluyor (`tests/unit/test_geodesy.cpp`).

## Sonraki fazlar

| Yetenek | Ne zaman |
|---|---|
| Türkiye Jeoit Modeli ile ortometrik yükseklik | Faz 1 |
| ED50 / UTM 6° ve bölgesel ITRF↔ED50 dönüşümü | Faz 1 |
| TKGM referans koordinatlarıyla doğrulama | referans veri geldiğinde |
| Epok ve hız alanı yönetimi | Faz 2 |
| TUSAGA-Aktif / CORS-TR, RINEX, NTRIP | Faz 2 |
| Poligon, nirengi, GNSS baz dengelemesi | Faz 2 |
