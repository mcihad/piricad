# checklist.md Denetim Raporu

*KentOSCad — DXF/CAD çekirdeği, 9 Eylül 2026. `checklist.md`'nin 45 maddesi kod tabanına karşı,
dosya:satır kanıtıyla denetlendi. Bu bir rapordur; §1'in "doğrudan geliştirmeye geç" talimatı
bu turda uygulanmadı, çünkü istenen rapordu.*

## Özet

Çekirdek model checklist'in korktuğu şey **değil**: DXF bir "GIS feature koleksiyonu" olarak
tutulmuyor. `core::Document` biçimden bağımsız, sabit noktalı (int64 mm), Struct-of-Arrays,
sanal fonksiyonsuz bir CAD belgesi; daire, yay ve elips **tanımıyla** saklanıyor (merkez, yarıçap,
eksen vektörleri), yakalama motoru 17 kipi eğrilerde **analitik** hesaplıyor, STR-paketlenmiş
R-tree var, seçim belge dışında, geri alma işlem tabanlı. Bunların hepsi checklist'in istediği
yönde ve yeniden yazılmamalı.

Zayıflık **kenarlarda**: DXF içe aktarma GDAL/OGR'nin DXF sürücüsünden geçiyor ve OGR eğrileri
görmeden önce parçalıyor; daire ve yay kesişik noktalardan **geri uydurularak** kurtarılıyor, elips,
spline, bulge, blok, tarama deseni, ölçü, XDATA, handle, kâğıt alanı, `$INSUNITS` ve Z bu yolda
**sessizce** kayboluyor. Dışa aktarma ise nesne türüne bakmıyor: daire iki noktalı bir çizgi,
yazı hiç yazılmıyor. Yani checklist'in nihai hedefindeki "DXF → OGR → geometri → render" zinciri
modelde değil, **io katmanında** hâlâ geçerli.

| Durum | Madde sayısı |
|---|---|
| **Var** — istenen biçimde mevcut | 13 |
| **Kısmen** — çekirdek var, kenar eksik | 18 |
| **Yok** — hiç yok ya da sadece ertelendiği yazılı | 9 |
| **Çatışır** — anayasa kararı gerekiyor | 3 |
| Plan maddeleri (§44 sıra, §45 öncelik) | 2 |

Üç madde `CLAUDE.md` ve `.claude/model.md` ile çatışıyor ve kodla değil **kararla** çözülür:
§14 (double), §22 (XDATA/ham metadata), §39 (Z). §3/§20/§26'nın QGIS'e özgü kısımları bu
ürüne uygulanmaz; adaptör sınırı zaten var.

## Denetim nasıl yapıldı

Dört paralel tarama (içe aktarma yolu, çekirdek model, render/stil, test/bağımlılık) ve ardından
en ağır bulguların elle doğrulanması. Her satırdaki kanıt `dosya:satır` biçiminde; satır
numaraları bugünkü ağaca aittir. Kural kitapları (`model.md`, `io.md`, `core.md`, `render.md`,
`kentoscad.md` §7/§9/§10) hakem olarak okundu.

## 1. Mevcut mimari — checklist §1'in soruları

| Soru | Cevap | Kanıt |
|---|---|---|
| DXF hangi kütüphaneyle okunuyor? | Yalnız GDAL/OGR DXF sürücüsü; hiçbir DXF açma seçeneği verilmiyor (`DXF_INLINE_BLOCKS`, `DXF_TRANSLATE_ESCAPE_SEQUENCES`, `OGR_ARC_STEPSIZE` … hepsi GDAL varsayılanında) | `cmake/KentOSCadGdalDrivers.cmake:71`; `src/io/src/vector.cpp:638` (open options = `nullptr`) |
| libdxfrw / dxflib / LibreCAD? | Yok | depo genelinde yalnız `checklist.md` ve `kentoscad.md` içinde geçiyor |
| DWG? | LibreDWG, salt okunur, **varsayılan KAPALI**; 6 nesne türü (LINE, LWPOLYLINE, POLYLINE 2D/3D, POINT, CIRCLE, ARC, TEXT); kalanı adıyla sayılıp atlanıyor | `cmake/KentOSCadOptions.cmake` (`KENTOS_WITH_DWG OFF`); `src/io/src/dwg.cpp:220-368` |
| Entity doğrudan GIS geometrisine mi çevriliyor? | Hayır: hedef `core::Document`; QGIS tipi yalnız `src/app/src/qgis_backend.cpp` içinde | `src/render/src/scene.cpp:112`; katmanlama kapısı `scripts/ci-gate-layering.sh` |
| Bağımsız CAD belge modeli var mı? | Var: `Document` = entity tablosu (SoA) + `RingGeometry` + `StyleTable` + `LayerTable` + `DashStore` + `ImageStore` + `TextTable` + `AttrTable` + `Crs` + ayarlar | `src/core/include/kentos_cad/core/document.hpp:53-95` |
| ARC/CIRCLE/SPLINE korunuyor mu? | Daire ve yay evet (tanım olarak); elips evet; **spline yok** | `core/circle.hpp:4-13`, `core/arc.hpp:4-19`, `core/ellipse.hpp:11-16`; `src/` içinde spline/nurbs/knot yok |
| BLOCK/INSERT? | Yok; "Faz 2 teslimi" diye yazılı | `src/core/src/style.cpp:409-411` |
| DXF stil bilgisi nasıl tutuluyor? | Modelde `Appearance` + `Source{Explicit, ByLayer, ByBlock}` (R19); **içe aktarmada hiç okunmuyor** (renk, çizgi tipi, kalınlık atılıyor) | `core/style.hpp:36-73`; `vector.cpp:1016` (stil dizgesi yalnız yazı yüksekliği için) |
| BYLAYER/BYBLOCK? | ByLayer çalışıyor; ByBlock enum'da var, blok tablosu olmadığından ByLayer gibi çözülüyor | `core/style.hpp:39`; `style.cpp:409` |
| TEXT/MTEXT render? | Tek `TextTable`; FreeType+HarfBuzz+msdfgen SDF atlası (`hb_language "tr"`); MTEXT ve font/stil kavramı yok; SHX yok | `core/text_store.hpp:53-120`; `src/render/src/text_atlas.cpp:409-411` |
| HATCH? | Nesne değil, stil özelliği (dolgu katmanları); DXF HATCH içe aktarımda **sessizce düz yüzeye** dönüyor | `vector.cpp:1090`, `:983-996` |
| DIMENSION/LEADER? | Yok; DXF'te GDAL parçalara ayırıyor, parçalar isimsiz geliyor, not yazılmıyor | `vector.cpp:1141-1143` (yalnız Arc/Circle sınıfı tanınıyor) |
| OCS/WCS? | Kodda yok; DXF'te GDAL'a bırakılmış, **DWG yolunda extrusion hiç uygulanmıyor** | `dwg.cpp:78-81` |
| Layer ve DXF tabloları? | Yalnız katman **adı**; LTYPE/STYLE/DIMSTYLE/APPID/BLOCK_RECORD/UCS/VIEW/VPORT okunmuyor; HEADER (`$INSUNITS`, `$EXTMIN`) okunmuyor | `vector.cpp:747, 969`; `GetMetadata*` hiç çağrılmıyor |
| XDATA / bilinmeyen kodlar? | Yok | `src/io/` içinde `XDATA`/`1001`/`RawCode` yok |
| Model / Paper Space? | DWG kâğıt alanını atıyor, **DXF ise çizime karıştırıyor** (`PaperSpace` alanı okunmuyor) | `dwg.cpp:190` ↔ `vector.cpp:747-766` |
| Koordinat hassasiyeti? | `Mm = int64` sabit nokta, 1 mm; `double` yalnız geçici; 128-bit tam çarpım | `core/units.hpp:20, 42, 74-82`; `scripts/ci-gate-model.sh` |
| Snapping? | Var, 17 kip, eğrilerde analitik | `core/snap.hpp:59-147`; `snap.cpp:243-256` |
| Spatial index? | Var: STR toplu yükleme, fanout 16, seçim/yakalama bunu kullanıyor; dosyada saklanmıyor, açılışta yeniden kuruluyor | `core/spatial_index.hpp:28-83`; `format.hpp` `kBlkRtree` ayrılmış-yazılmamış |
| Render ile CAD geometrisi ayrı mı? | Evet: `Document → cull → scene → DrawList → Backend`; ama sahne **her karede** yeniden kuruluyor, tessellation önbelleği yok | `scene.cpp:347-395`; `map_canvas.cpp:1477-1479` |
| Import → düzenleme → export veri kaybı? | **Evet, bugün var**: dışa aktarma türe bakmıyor (daire → 2 noktalı çizgi, yay → 4 noktalı kırık), yazı yazılmıyor | `vector.cpp:1409-1431` (`kind` hiç okunmuyor; `texts()` yazma yolunda yok) |

## 2. Madde madde durum

Durum sütunu: **Var** · **Kısmen** · **Yok** · **Uygulanmaz** · **Çatışır** (anayasa kararı gerekir).

| # | Madde | Durum | Ne var, ne eksik | Kanıt |
|---|---|---|---|---|
| 1 | Mevcut mimari analizi | Var | Bu rapor; yukarıdaki tablo | — |
| 2 | Canonical CAD Document Model | Kısmen | Biçimden bağımsız belge, 5 nesne türü (çokluçizgi, daire, yay, nokta, elips), katman tablosu, stil tablosu, çizgi tipi deposu, resim deposu, öznitelik şeması, CRS+epoch, proje ayarları. Eksik: blok tanımları, yazı stilleri, ölçü stilleri, kâğıt alanları, ham metadata | `entity_kind.cpp:1021-1026`; `document.hpp:53-95` |
| 3 | DXF'yi düzleştirme; Canonical → Render → GIS ayrımı | Kısmen | Model düzleştirmiyor, render `curve_outline` üzerinden alıyor; **içe aktarma OGR'nin düzleştirdiğini geri uyduruyor**, dışa aktarma ise ayrımı hiç kullanmıyor | `entity_kind.hpp:225`; `vector.cpp:517, 1141-1181`; `vector.cpp:1409-1431` |
| 4 | ARC / CIRCLE / ELLIPSE / SPLINE | Kısmen | Daire, yay, elips analitik ve 2B (normal/extrusion/thickness anayasa gereği yok). Spline türü yok. İçe aktarmada yay/daire 5+ noktayla ve 1 mm sapmayla uyarsa kurtarılıyor, açı kodları (50/51) okunmuyor; elips çokluçizgi oluyor, **kapalı elips yüzey** oluyor | `vector.cpp:519, 559-563, 1194-1213`; `add_ellipse` io'da çağrılmıyor |
| 5 | POLYLINE bulge / genişlik / bayrak | Yok | `RingGeometry` köşe başına yalnız x,y; bulge, başlangıç/bitiş genişliği, tür yok. Kapalılık DXF'te geometrik çıkarım, DWG'de bayraktan | `geometry.hpp:80-131`; `commands/corner.cpp:15-17`; `dwg.cpp:243` |
| 6 | BLOCK / INSERT | Yok | Modelde blok kavramı yok; GDAL DXF'te blokları içe patlatıyor (kontrol edilmiyor), DWG'de INSERT atlanıyor; ATTRIB/ATTDEF yok | `style.cpp:409-411`; `dwg.cpp:362` |
| 7 | Stil sistemi (ByLayer/ByBlock, çözümleme sırası) | Kısmen | Model doğru: özellik başına `Source`, `StyleTable`, commit anında `resolve_appearance`, sentinel'ler io'da (R19). Eksik: içe aktarma stil yazmıyor; ByBlock etkisiz; bellekte ACI/TrueColor ayrımı yok (RGBA) | `style.hpp:36-73, 482-532`; `style.cpp:407-430`; `scene.cpp:371-378` |
| 8 | Linetype motoru | Kısmen | `DashPattern` (8 parça, kalınlığa göreli) + MPYY karmaşık çizgileri **sembol katmanı yığını** olarak (MarkerLine, HashLine, TextMarker, RasterLine — katalogda 1 574 katman). Eksik: çizgi tipinde nokta/şekil/yazı elemanı (tasarım kararı), `core.cizim.cizgi_tipi_olcegi` tanımlı ama **hiç okunmuyor**, nesne başına ölçek yok, DXF LTYPE içe alınmıyor | `dash_store.hpp:51-63`; `settings.cpp:1025-1040`; `plan-gosterim.json` |
| 9 | HATCH gerçek nesne | Yok | Dolgu bir stil özelliği (SimpleFill, LinePatternFill, PointPatternFill, RasterFill; açı ve aralık var, **desen adı ve origin yok**). DXF HATCH → düz yüzey, not yok; DWG HATCH atlanıyor; `Appearance::hatch` indeksi QRhi'de hiç okunmuyor | `style.hpp:172-205`; `vector.cpp:1090`; `rhi_backend.cpp:1228-1233` |
| 10 | TEXT / MTEXT | Kısmen | İçerik, yükseklik (zemin mm), 4 hizalama, döndürme (taban çizgisi yönü). Yok: font/stil, genişlik faktörü, eğiklik, MTEXT biçimlendirme, arka plan maskesi, ham/gösterilen ayrımı. Kodlama GDAL varsayılanına bırakılmış; Türkçe içerikli hiçbir DXF fixture yok; **yazı dışa aktarılmıyor** | `text_store.hpp:36-41, 92-95`; `vector.cpp:1008-1058`; fixture'larda ≥0x80 bayt yok |
| 11 | DIMENSION / LEADER / MULTILEADER | Yok | Semantik nesne yok; DXF'te parçalar isimsiz geliyor | `vector.cpp:1141-1143` |
| 12 | Drawing unit ≠ CRS | Kısmen | `Crs` zengin (id, epsg, epoch, meridyen, jeoit, düşey datum; epoch şimdilik boş); PROJ yalnız jeodezi modülünde. **`core.cizim.birim` ayarının tek tüketicisi yok**, içe aktarma metre varsayıyor (`$INSUNITS` okunmuyor), dışa aktarma metreye sabit | `crs.hpp:29-114`; `settings.cpp:1008-1024`; `vector.cpp:211, 226, 1415` |
| 13 | OCS / WCS | Yok | Ortak dönüşüm altyapısı yok; `core/transform.hpp` 2B afin; DWG yolunda extrusion uygulanmıyor | `dwg.cpp:78-81` |
| 14 | Tüm koordinatlarda double | **Çatışır** | Anayasa 2.4 ve `model.md` R21/P8 `int64` mm ister; hiçbir alanda float saklanmıyor, geçici `double` var. Sonuç: 1 mm kuantalama, mm-altı ayrıntı yok. GPU için floating origin **var** ve kapıyla korunuyor; R3'ün 1 km yeniden demirlemesi yok (önbellek olmadığı için sorun çıkmıyor) | `units.hpp:20`; `view.cpp:79-87`; `ci-gate-render.sh:36-45`; `test_jitter.cpp` |
| 15 | GEOS'u CAD çekirdeği yapma | Var | GEOS bağlı değil; boolean/offset Clipper2 ile int64 üzerinde (`offset.hpp` cephesi); yakalama/kesişim/dik çekirdekte analitik. **`core.md` R8'in istediği Shewchuk `predicates.c` ağaçta yok** | `KentOSCadOptions.cmake` (`KENTOS_WITH_GEOS OFF`); `core/offset.cpp:4`; `find -name "predicates*"` boş |
| 16 | Ortak entity kontratı | Var | `KindSpec`: bbox, outline, hit, area, perimeter, read, write serbest fonksiyon işaretçileri, batch başına dispatch, render'dan bağımsız. Yakalama noktaları `KindSpec` içinde değil `snap.cpp` içinde tür dallanmasıyla | `entity_kind.hpp:136-169, 225` |
| 17 | Snapping altyapısı | Var | 17 kip: uç, orta, merkez, kesişim, dik, yakın, ızgara, kutupsal, düğüm, uzantı, paralel, uzatılmış kesişim, kılavuz, ağırlık merkezi, yüzey normali… Eğrilerde merkez+yarıçap ile analitik. Eksik: çeyrek (quadrant), teğet, ekleme noktası (blok yok) | `snap.hpp:59-147, 226`; `snap.cpp:165, 243-256, 729-733` |
| 18 | Spatial index | Var | STR-paketlenmiş R-tree, tembel kurulum, `index_stale_` disiplini; seçim/yakalama `pick_candidates` ile daraltıyor | `spatial_index.hpp:28-83`; `pick.hpp:129-154`; `document.cpp:188` |
| 19 | Render modelini ayır | Var | Belge → cull (yalnız flags + bbox) → sahne → DrawList → Backend; QObject/QGraphicsItem yok. Eksik: tessellation önbelleği yok (`revision()` render'da okunmuyor), R4 LOD döşemeleri yok, R6 kalıcı halka tampon yok, **Tracy bölgesi sıfır** (R14) | `scene.cpp:249-250, 361`; `ROADMAP.md:159-163`; `grep ZoneScoped` = 0 |
| 20 | QGIS sembol motorunun rolü | Var | Sembol modeli tamamen kendi (`Symbol`/`SymbolLayer`, 11 tür, birim Paper/Ground/Pixel); QGIS yalnız bir `render::Backend` uygulaması; QML dışa aktarma yalnız SimpleFill/SimpleLine, tek yönlü | `style.hpp:172-205, 287-407`; `qgis_style.cpp:82-107` |
| 21 | DXF TABLES ve HEADER | Yok | Yalnız katman adı | `vector.cpp:747, 969` |
| 22 | XDATA ve bilinmeyen veri | **Çatışır** | `model.md` R27/P12 nesne başına özellik torbasını ve XDATA blobunu yasaklıyor; sözleşilen yol R26 (bilinmeyen tür yükü bayt bayt korunur) — **o da uygulanmamış**: `kBlkKindPayload` ayrılmış, okuyucu bilinmeyen türü **reddediyor** | `attribute.hpp:4-9`; `format.hpp:284-287`; `project_reader.cpp:240-244` |
| 23 | Model Space / Paper Space | Yok | Kavram yok; DXF'te kâğıt alanı çizime karışıyor, belge "alınmaz" diyor | `vector.cpp:747-766`; `docs/veri/dis-formatlar.md:51` |
| 24 | Handle ve referanslar | Yok | İç kimlik doğru (slot/key ayrımı, monoton `EntityKey`); `sourceHandle` için yer yok, `EntityHandle` alanı okunmuyor | `identity.hpp:49-60, 81-157`; `vector.cpp:317, 789` |
| 25 | Açık kaynak CAD kütüphanesi adaptörü | Kısmen | GDAL yalnız `vector.cpp` içinde görünür (io.md R2, kapıyla), LibreDWG yalnız `dwg.cpp` — adaptör sınırı doğru. Ama GDAL adaptörü "GIS kipi": CAD semantiğini geçirmiyor | `ci-gate-layering.sh`; `vector.cpp`; `dwg.cpp` |
| 26 | GDAL'ın rolünü sınırla | Kısmen | GDAL Shapefile/GeoPackage için doğru yerde; **DXF için tek yol** ve "CAD olarak aç / GIS olarak aktar" ayrımı yok | `KentOSCadGdalDrivers.cmake:70-74` |
| 27 | Importer'ı katmanlara böl | Kısmen | `vector.cpp` ≈1 500 satır: açma, alan planı, geometri, yazı, daire uydurma ve dışa aktarma bir dosyada; DWG ayrı dosyada. Header/Table/Block/Entity okuyucu ayrımı yok | `src/io/src/vector.cpp` |
| 28 | Import uyarı sistemi | Kısmen | DXF: `notes` (en çok 8) + tek sayaç + ilk neden; DWG: tür → sayı haritası (ilk 5 gösterilir). Seviye yok. **Sessiz düşürmeler**: tarama→yüzey, elips→çokluçizgi, ölçü→parça, Z atıldı, birim varsayıldı, renk/çizgi tipi atıldı — hiçbiri not üretmiyor (io.md P11/P13) | `vector.hpp:99-105`; `dwg.hpp:42-45`; `service.cpp:333-338` |
| 29 | Test altyapısı | Kısmen | 5 DXF tohum (01 ≡ 02 bayt bayt aynı), 0 DWG; checklist'in 22 fixture'ından **3'ü** var (line, arc, circle) + büyük koordinat; bulge/XDATA/kâğıt alanı/Türkçe metin/blok/ölçü yok. Testler tür, sayı, katman adı, mm ölçü doğruluyor; stil/blok/metadata doğrulamıyor | `tests/fuzz/tohum/dxf/`; `test_io.cpp:1414-1416, 1720-1731` |
| 30 | Round-trip | Kısmen | Tek bir ÇİZGİ ile gidiş-dönüş testi var; daire/yay/yazı/nokta dönmez (dışa aktarma tür-kör). Altın DXF çıktısı yok | `test_io.cpp:1016-1053`; `vector.cpp:1409-1431` |
| 31 | Performans | Kısmen | SoA, arena, 32-bit indeks, toplu R-tree, kapasite koruyan DrawList, 16 ms kapısı (sahne kurucu 0,0035 ms; tam kapsam 78,9 ms kapısız). Eksik: tessellation/blok/geometri önbelleği yok, LOD yok, `KENTOS_BUILD_BENCH` kapalı olduğundan bench `ctest`'te değil, 200 MB DWG ölçümü "beklemede" | `bench_render.cpp:135-147`; `temel-degerler.json`; `bench_pending.cpp:8-15` |
| 32 | Threading | Yok | Sihirbazın ön okuması `QThread`'de; **gerçek İÇEAKTAR UI iş parçacığında** (`runLine` → `Session::resume_once` → `import_vector`), `io.md` P3'e aykırı; `stop_token` bağlı ama gerçek içe aktarmada kimse `request_stop` çağırmıyor; TSan işi yok | `main_window.cpp:2860` → `controller.cpp:187` → `session.cpp:44-70`; `service.cpp:158-165` |
| 33 | Undo/Redo'ya uygun model | Var | 14 `Op`, ters işlem kaydı, `UndoStack`, doğrulama başarısızlığında tam geri alma (bus.cpp), `UndoPolicy` 3 seçenek (`Custom` kullanılmıyor). Nesne başına sürüm yok, yalnız belge `revision()` | `document.hpp:99-158`; `transaction.hpp:33-213`; `bus.cpp:428-487` |
| 34 | Selection ≠ render | Var | `command::Selection` anahtar listesi, belge dışı, kapıyla korunuyor; vurgu Overlay'de çiziliyor. Hover yalnız widget yerelinde, EditSession kavramı yok | `selection.hpp:4-63`; `ci-gate-model.sh:166-170`; `map_canvas.cpp:350-399` |
| 35 | Tolerans politikası | Kısmen | int64 aritmetik epsilonu yapısal olarak gereksiz kılıyor; `src/` genelinde yalnız **4** kayan nokta epsilonu (paralellik/dejenere payda). Toleranslar ayar kataloğunda ve doğru kapsamda (düğüm toleransı Proje, piksel açıklıkları Uygulama). Merkezi başlık yok; tessellation toleransı sabit 128 parça | `settings.cpp:1087-1110`; `trim.cpp:428`, `corner.cpp:153`, `transform.cpp:409`, `vector.cpp:543` |
| 36 | Merkezi tessellation | Kısmen | `curve_outline` tek giriş; `map_canvas.cpp` ve `sector.cpp` doğrudan `circle_outline`/`arc_outline` çağırıyor; dışa aktarma hiç çağırmıyor; parça sayısı sabit, tolerans/zoom'a bağlı değil; önbellek yok | `entity_kind.hpp:217-225`; `map_canvas.cpp:394, 1308`; `sector.cpp:83` |
| 37 | Analitik bbox | Kısmen | Daire, elips, nokta analitik; yay çizilmiş formundan (doğru ama çağrı başına 2 vektör) | `entity_kind.cpp:308-315, 415-441, 747-766` |
| 38 | Layer modeli | Kısmen | key, name, folded, description, group yolu, visible, locked, plottable, appearance, style, min/max ölçek, opacity, catalog_ref. Yok: frozen, plot style. **opacity, plottable ve ölçek görünürlüğü saklanıyor ama hiç çizilmiyor**; katman ağacı ayrı yapı değil, dizge (R31 bekliyor) | `layer.hpp:28-72`; `scene.cpp:385-395` |
| 39 | Z koordinatı | **Çatışır** | Belge 2B (`model.md` R9); Z bilinçli atılıyor, yükseklik yalnız `kot` öznitelik sütunu; DXF/DWG'de Z atıldığında **not yazılmıyor** | `dwg.cpp:277-279`; `vector.cpp:1005-1006`; `contour_command.cpp:58` |
| 40 | Qt/QGIS/GDAL/GEOS/PROJ sınırları | Var | 39 kapı; core Qt'siz, io yalnız GDAL/LibreDWG, render Qt'siz (8.5), QGIS yalnız app backend'inde, PROJ yalnız jeodezide | `ci-gate-layering.sh:22-43`; `ci-gate-core-purity.sh` |
| 41 | Kod kalitesi | Var | C++20, kayıtlarda sanal/`std::function`/işaretçi yok, `Result<T>`, RAII, katı FP bayrakları iki kez korunuyor. **`-Werror`/`/WX` hiçbir hedefte yok** (`build.md` R14); tidy'de 3 aile hata | `KentOSCadFlags.cmake:28-36`; `.clang-tidy:57` |
| 42 | Lisans uyumluluğu | Var | `NOTICE` ayrıntılı; GPLv3 çıkış; GPLv2-only yok (QGIS GPL-2.0-**or-later**, libdxfrw GPLv2+). Eksik: CycloneDX SBOM yok (6.10); `vcpkg.json` bağımlılık listesi boş, gerçek mekanizma SHA-pinli FetchContent (`build.md` R11 ile çelişki); `CLAUDE.md` 8.2 "pinli ama bağlı değil" derken Clipper2 ve CDT **bağlı** | `NOTICE:19-273`; `vcpkg.json:18`; `KentOSCadDependencies.cmake:242-315` |
| 43 | Yeniden icat etme | Var | Bu raporun yöntemi | — |
| 44 | Aşamalı değişiklik | — | Bölüm 6'daki sıra | — |
| 45 | Öncelikler | — | P0: daire/yay/nokta/çokluçizgi/katman/ByLayer/double(int64)/slot-key **var**; elips içe aktarımı, spline, bulge, blok, OCS, handle **yok**. P1: yazı kısmen, çizgi tipi kısmen, tarama kısmen, XDATA yok, index **var**, snap **var**, diagnostics kısmen. P2: hepsi yok | — |

## 3. Anayasa ile çatışan maddeler — karar gerekiyor

Bu dört madde kodla değil, `CLAUDE.md` Madde 0.5'e göre bir **değişiklik kararıyla** çözülür.
Öneri sütunu benim görüşüm; karar kullanıcının.

| Checklist | Anayasa / kural kitabı | Öneri |
|---|---|---|
| §14 canonical geometri `double` | Madde 2.4, `model.md` R21/P8, `core.md` R2-R3: `int64` mm, saklanan alan hiçbir zaman kayan nokta. Kadastro için mm yeter; mekanik ayrıntı (0,1 mm) kuantalanır | Sabit noktada kal. Checklist'in asıl istediği "float'a düşürme" zaten yasak. Gerekirse birim µm'ye taşınması bir **veri göçüdür** (`model.md` 0.2a), ayrı karar |
| §22 XDATA / ham metadata koru | `model.md` R27/P12: nesne başına torba, XDATA blobu, QVariant haritası yasak. Sözleşilen yol R26: bilinmeyen yük **bayt bayt** korunur, düzenlenemez | R26'yı uygula (`kBlkKindPayload`), DXF XDATA'yı `EntityKey`'e bağlı **opak yan tablo** olarak taşı — özellik olarak sunma. Bu bir `model.md` eki, çelişki değil |
| §39 Z'yi atma | `model.md` R9-R12 belgeyi 2B kuruyor; yükseklik yüzey modülünde `kot` sütunu | Kısa vadede: Z'yi `kot` öznitelik sütununa yaz (bugün `NOKTALAR` yapıyor, DXF yapmıyor) ve atıldığında **not yaz**. Uzun vadede Z sütunu bir `model.md` eki |
| §3/§20/§26 QgsGeometry, QGIS sembol motoru | Uygulanmaz: ürün QGIS üzerine kurulu değil; kendi sembol motoru + `render::Backend` adaptörü var | Değişiklik gerekmez; QML dışa aktarımı genişletilebilir |

## 4. Bugün veri kaybeden bulgular

Sıra, kullanıcının bugün dosya kaybettiği yerlerden başlıyor. Hepsi kanıtlı, hiçbiri yorum değil.

1. **Dışa aktarma nesne türüne bakmıyor.** `vector.cpp:1409-1431` yalnız `ring_role` okuyor; daire (2 saklı köşe) merkezden doğu çeyreğine bir çizgi, yay (4 köşe) kırık çizgi, nokta 1 köşeli LINESTRING oluyor. Gidiş-dönüş testi tek düz çizgi kullandığı için görünmüyor. Düzeltme: yazma yolunda `curve_outline` (render ve seçim zaten bunu kullanıyor) ya da DXF'te gerçek CIRCLE/ARC yazımı.
2. **Yazı dışa aktarılmıyor.** `doc.texts()` yazma yolunda hiç okunmuyor; her ada/parsel numarası ve plan notu DXF/GeoPackage çıkışında düşüyor.
3. **Elips kaydedilip geri açılamıyor.** `project_reader.cpp:250-251` yalnız {çokluçizgi, daire, yay, nokta} kabul ediyor; bir elips içeren `.pcad` "sürümü yükseltin" hatasıyla **hiç açılmıyor**. `src/io/` içinde elips okuma/yazma yok.
4. **DXF kâğıt alanı çizime karışıyor.** `PaperSpace` alanı okunmuyor; antet ve pafta çerçevesi TUREF koordinatlarından uzakta çizime giriyor, `YAKINLAŞ KAPSAM`'ı bozuyor. DWG yolu doğru (`dwg.cpp:190`).
5. **Birim varsayımı.** `$INSUNITS` okunmuyor; milimetre ya da birimsiz yazılmış bir DXF **1000 kat büyük** ve notsuz geliyor.
6. **Gerçek içe aktarma UI iş parçacığında.** 48 MB'lık bir DXF pencereyi donduruyor; `stop_token` var ama sinyalleyen yok. `io.md` P3 açıkça yasaklıyor.
7. **Sessiz düşürmeler.** HATCH→yüzey (ifraz/tevhit onu parsel sanır), kapalı elips→yüzey, ölçü→isimsiz parçalar, renk/çizgi tipi/kalınlık atıldı, Z atıldı — hiçbiri `notes` üretmiyor; DXF raporunda tür başına kanal yok.

## 5. Kural kitabı ile kodun ayrıştığı yerler

Bunlar checklist'in dışında ama denetimde ortaya çıktı; her biri "yazılı ama uygulanmıyor" sınıfında.

| Kural | Durum |
|---|---|
| `render.md` R14 — kare yolundaki her fonksiyon Tracy bölgesi açar, `ci-gate-render.sh` denetler | Sıfır `ZoneScoped`; kapı bunu denetlemiyor |
| `core.md` R8 — orientation/incircle Shewchuk `predicates.c` üzerinden | Dosya ağaçta yok; `pick.cpp` int64 ile gerekçesini yazmış ama kural kalkmamış |
| `core.md` R17 / `build.md` R14 — `-Werror`/`/WX` | Hiçbir hedefte yok |
| `io.md` P3 — UI iş parçacığında dosya okunmaz | Gerçek içe aktarma UI'da |
| `io.md` R14 — DWG kapsam raporu `tests/golden` altında, `dwg-coverage` CI işi | Rapor, korpus ve iş yok |
| `model.md` R26 — bilinmeyen tür korunur | Okuyucu reddediyor |
| `model.md` R31 — katman ağacı ayrı yapı | `Layer::group` dizgesi |
| `model.md` R14 — stil commit anında sütuna yazılır | Kare yolunda katman geri düşümü dizin araması olarak yapılıyor (kural değerlendirmesi değil, ama R14'ün "yaz ve oku" biçimi değil) |
| `CLAUDE.md` 8.2 — Clipper2 ve CDT "pinli, bağlı değil" | İkisi de bağlı |
| `build.md` R3/R7/R9/R11/R15/R18/R25/R26 | Ön ayar adları, `Compilers.cmake`, `-flto=thin`, vcpkg-only, tidy CI işi, SBOM, `SECURITY.md`/`CONTRIBUTING.md` — hepsi yok ve Madde 8'e sapma olarak yazılmamış |
| `test.md` R15 — TSan işi | Yok |
| Katman `opacity`, `plottable`, ölçek görünürlüğü; ayar `core.cizim.cizgi_tipi_olcegi`, `core.cizim.birim` | Saklanıyor, hash'leniyor, ayarlanabiliyor, **hiç okunmuyor** |
| Sembol ön izlemesi | Her zaman QPainter/QGIS ile çizilir; tuval QRhi ile — tarama deseni, uç/birleşim, dolgu saydamlığı iki motorda farklı |

## 6. Önerilen sıra

Checklist §44'ün aşamaları, §45'in öncelikleri ve anayasanın "veri göçü" kuralı birlikte
okunduğunda:

**Hemen (bu sürüm, model değişmez):**
dışa aktarmada tür dispatch'i ve yazı (bulgu 1-2) · elipsin `.pcad` okuma/yazımı (bulgu 3) · DXF
kâğıt alanı süzgeci (4) · `$INSUNITS` okuma + `core.cizim.birim` tüketimi (5) · `İÇEAKTAR`'ı
sihirbazın ön okuması gibi iş parçacığına alma ve `request_stop`'u düğmeye bağlama (6) · DXF
raporuna tür → sayı haritası ve seviye; her sessiz düşürmeye not (7) · fixture seti: bulge,
elips, spline, blok, mtext, Türkçe metin, tarama, ölçü, OCS, XDATA, kâğıt alanı, birimsiz.

**Kısa vade (io, model eklenir ama anlam değişmez — `model.md` 0.2a'ya uygun):**
DXF için CAD-sadakatli okuyucu: GDAL'ı GIS kipinde bırak, DXF'i grup kodu düzeyinde okuyan
adaptör ekle (libdxfrw GPLv2+ lisansı `NOTICE`'ta onaylı; LibreDWG'nin kendi DXF okuyucusu da
seçenek) · yay/daire açı kodlarını doğrudan oku, uydurmayı bırak · katman tablosu renk/çizgi
tipi/kalınlık → `Appearance` + `Source::ByLayer`, LTYPE → `DashStore` · yazı stil/font/hizalama
alanları · handle için `EntityKey`'e bağlı kaynak-kimlik sütunu · R26 yükü (`kBlkKindPayload`)
ve XDATA'nın opak yan tabloya taşınması.

**Orta vade (yeni türler; her biri `KindSpec` + fixture + belge sayfası):**
spline (NURBS yükü) · bulge taşıyan çokluçizgi ya da yay parçalı çokluçizgi türü · blok
tanımı/referansı ve `Source::ByBlock`'un canlanması · tarama nesnesi (desen adı, origin, adalar) ·
ölçü/leader semantik türleri · kâğıt alanı ayrımı · OCS dönüşümü.

**Sürekli:** `-Werror`, Tracy bölgeleri, tessellation önbelleği (`revision()` ile), LOD
döşemeleri, katman opacity/plottable/ölçek görünürlüğünün çizilmesi, `cizgi_tipi_olcegi`
tüketimi, DWG kapsam korpusu ve raporu, TSan işi, SBOM.

## 7. Bu denetim sırasında düzeltilenler

İki tutarsızlık dün bıraktığım değişiklikten kalmıştı; düzeltildi:
`cmake/KentOSCadOptions.cmake` içindeki "ON BY DEFAULT" yorum bloğu artık gerçek varsayılanı
(OFF) ve gerekçesini anlatıyor, kullanılmayan `KENTOS_DWG_AVAILABLE` sondası kaldırıldı;
`docs/veri/dis-formatlar.md`'nin ¹ dipnotu DWG'nin kapalı geldiğini ve nasıl açıldığını söylüyor.
Belge ve ön ayar kapıları yeşil.
