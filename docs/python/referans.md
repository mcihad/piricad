<!-- ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN. -->
<!-- Kaynak: kentos::command::Registry.  Yeniden üret: make reference -->
<!-- Python yüzeyi de aynı kayıttan üretilir; elle yazılan ikinci bir -->
<!-- bağlama listesi yoktur (CLAUDE.md 5.10, 5.20). -->

# Python API Referansı

Bu sayfa komut kaydından üretilir. Programa bir komut eklendiğinde Python
fonksiyonu ve bu satır kendiliğinden gelir.

Nasıl kullanıldığı: [Python betikleri](../betik/python.md).

## Kurallar

- Her çağrı **yalnız anahtar kelime** alır; konumsal argüman yoktur.
- Anahtar kelimeler **İngilizcedir**. Komutun kendi adı Türkçe kalır.
- `Coord` bir koordinattır: `cad.Point(east, north)` ya da iki elemanlı
  tam sayı listesi. `Coords` bunlardan bir liste.
- Koordinatlar **milimetre tam sayıdır** ve `[Sağa, Yukarı]` sırasındadır.
  Metre yazmak isterseniz `cad.run("ÇİZGİ 0,0 10,10")` komut satırının
  dilbilgisini kullanır.
- Dönen değer, komutun kaç ilkel düzenleme yaptığıdır.
- Bir hata istisna yükseltir; yakalamazsanız betiğin tamamı geri alınır.

## Değer tipleri

Bunlar programın kendi tipleridir, Python tarafında yeniden tanımlanmış
kopyaları değil: `cad.Point`, `core::Point2`'nin ta kendisidir.

**Eksen adları harf DEĞİL.** `Point2.x` doğuya gider ve bir paftada **Y**
yazar; `.y` kuzeye gider ve paftada **X** yazar. Harflerden hangisini
seçersek okuyucuların yarısı tersini anlar, bu yüzden API `east` ve `north`
der. Sıralama komut satırı ve günlükle aynıdır: önce doğu.

| Çağrı | Döndürdüğü |
|---|---|
| `cad.Point(east, north)` | Bir koordinat, milimetre tam sayı |
| `p.east` · `p.north` | `int` |
| `p[0]` · `p[1]` · `list(p)` | doğu, kuzey sırasıyla |
| `p.distance_to(q)` | `float` — **metre** |
| `cad.Box(min_east, min_north, max_east, max_north)` | Bir dikdörtgen |
| `b.min_east` · `b.min_north` · `b.max_east` · `b.max_north` | `int` |
| `b.width` · `b.height` | `int` — milimetre, boş kutuda 0 |
| `b.center` | `cad.Point` |
| `b.corners` | dört `cad.Point`, sol alttan saat yönünün tersine |
| `b.contains(point)` | `bool` — kenarlar dahil |
| `b.is_empty()` | `bool` — boş kutu gerçek bir durumdur, hata değil |

Bir `Point`, iki sayılık liste kabul eden her yere doğrudan verilebilir:
`cad.line(points=[a, b])`.

## Görünüm

`cad.viewport`, pencerenin o an baktığı yeri **değer olarak** verir.
`GÖRÜNÜMBİLGİSİ` komutu aynı kaynağı okur ama transkripte bir cümle yazar:
biri insanın, öbürü betiğin okuduğu biçimdir.

| Çağrı | Döndürdüğü |
|---|---|
| `cad.viewport.exists()` | `bool` — başsız çalıştırmada, oynatmada ve testte `False` |
| `cad.viewport.bbox()` | `cad.Box` — görünen dikdörtgen |
| `cad.viewport.center()` | `cad.Point` |
| `cad.viewport.scale()` | `int` — 1:N'deki N, bilinmiyorsa 0 |
| `cad.viewport.mm_per_pixel()` | `float` |
| `cad.viewport.size_px()` | `(genişlik, yükseklik)` piksel |
| `cad.viewport.crs()` | `str` |

Pencere yoksa `exists()` dışındaki çağrılar hata verir — uydurulmuş bir
dikdörtgen, sonraki çizimi kimsenin bakmadığı bir yere koyardı.

## Çizimden okuma

Bunlar komut değildir, çizimi değiştirmezler ve `cad.doc` altındadır.

| Çağrı | Döndürdüğü |
|---|---|
| `cad.doc.layers()` | `list[str]` — katman adları |
| `cad.doc.layer_count()` | `int` |
| `cad.doc.active_layer()` | `str` |
| `cad.doc.entity_count()` | `int` |
| `cad.doc.selection_count()` | `int` |
| `cad.doc.crs()` | `str` — örneğin `TUREF/TM30` |
| `cad.doc.setting(id)` | `bool` \| `int` \| `str` |
| `cad.run(satır)` | `int` — komut satırını çalıştırır |
| `cad.sandbox()` | `str` — `güvenli`, `proje` ya da `tam` |
| `cad.read_file(path)` | `str` — kum havuzu izin verirse |
| `cad.write_file(path, text)` | — kum havuzu izin verirse |

## Komutlar

| Fonksiyon | Komut | Adı | Ne yapar |
|---|---|---|---|
| [`cad.line`](#cadline) | `core.line` | `ÇİZGİ` | İki veya daha fazla nokta arasında doğru parçaları çizer. |
| [`cad.polyline`](#cadpolyline) | `core.polyline` | `ÇOKLUÇİZGİ` | Birden çok noktadan TEK bir çizgi nesnesi çizer. |
| [`cad.point_draw`](#cadpoint_draw) | `core.point_draw` | `NOKTA` | Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper. |
| [`cad.perp_offset`](#cadperp_offset) | `core.perp_offset` | `DİKAYAK` | Taban çizgisine göre dik ayak ve dik boy vererek nokta yerleştirir. |
| [`cad.survey_polar`](#cadsurvey_polar) | `core.survey_polar` | `ALIM` | İstasyondan okunan açı ve kenarlardan nokta hesaplar ve yerleştirir. |
| [`cad.intersect_point`](#cadintersect_point) | `core.intersect_point` | `KESİŞİMNOKTA` | İki doğrultunun, iki uzaklığın ya da iki doğrunun kesişimine nokta koyar. |
| [`cad.point_along`](#cadpoint_along) | `core.point_along` | `ARANOKTA` | İki nokta arasındaki doğru üzerinde oran, uzaklık ya da eşit bölmeyle nokta koyar. |
| [`cad.polygon_regular`](#cadpolygon_regular) | `core.polygon_regular` | `ÇOKGEN` | Merkez ve kenar sayısından düzgün çokgen çizer: içten, dıştan ya da kenar uzunluğundan. |
| [`cad.break`](#cadbreak) | `core.break` | `KIR` | Çizgiden, yaydan, daireden ya da yaylı çoklu çizgiden iki nokta arasındaki parçayı çıkarır; tek nokta açık bir nesneyi boşluk bırakmadan böler. |
| [`cad.join`](#cadjoin) | `core.join` | `UÇUCA` | Uçları birbirine değen çizgileri, yayları ve yaylı çoklu çizgileri tek bir nesneye ekler; yaylar yay kalır, boşluklar söylenir. |
| [`cad.lengthen`](#cadlengthen) | `core.lengthen` | `UZUNLUK` | Çizginin bir ucunu kendi doğrultusunda hareket ettirerek uzunluğunu değiştirir. |
| [`cad.explode`](#cadexplode) | `core.explode` | `PATLAT` | Çizgiyi tek tek kenarlara, alanı sınırına, blok referansını bileşenlerine ayırır. |
| [`cad.align`](#cadalign) | `core.align` | `HİZALA` | Bir ya da iki nokta çiftiyle nesneleri taşır, döndürür ve istenirse ölçekler. |
| [`cad.divide`](#caddivide) | `core.divide` | `BÖLÜMLE` | Bir nesne boyunca eşit parçalara bölerek ya da sabit aralıkla nokta veya blok yerleştirir. |
| [`cad.pedit`](#cadpedit) | `core.pedit` | `ÇİZGİDÜZENLE` | Çizgiyi kapatır, açar, yönünü çevirir ya da yakın köşelerini atarak sadeleştirir. |
| [`cad.copy_clip`](#cadcopy_clip) | `core.copy_clip` | `PANOYAKOPYALA` | Seçili nesneleri çizimin kendi biçiminde panoya yazar. |
| [`cad.cut`](#cadcut) | `core.cut` | `KES` | Seçili nesneleri panoya alır ve çizimden siler; tek geri alma adımı. |
| [`cad.paste`](#cadpaste) | `core.paste` | `YAPIŞTIR` | Panodaki nesneleri çizime koyar; tek geri alma adımı. |
| [`cad.entity_info`](#cadentity_info) | `core.entity_info` | `NESNEBİLGİ` | Nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve özniteliklerini bildirir. |
| [`cad.measure_angle`](#cadmeasure_angle) | `core.measure_angle` | `AÇIÖLÇ` | Bir tepeden çıkan iki kol arasındaki açıyı ölçer, oturumun açı kuralıyla yazar. |
| [`cad.stretch`](#cadstretch) | `core.stretch` | `ESNET` | Pencere içindeki köşeleri taşır, dışındakileri yerinde bırakır. |
| [`cad.tracking`](#cadtracking) | `core.tracking` | `İZ` | Geçici izleme için nokta işaretler; iki işaretin izleri kesişir. |
| [`cad.text`](#cadtext) | `core.text` | `METİN` | Çizime metin yazar; yükseklik ve hizalama verilebilir. |
| [`cad.edittext`](#cadedittext) | `core.edittext` | `YAZIDÜZENLE` | Var olan bir yazının metnini, yüksekliğini ya da hizalamasını değiştirir. |
| [`cad.exportstyle`](#cadexportstyle) | `core.exportstyle` | `STİLAKTAR` | Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar. |
| [`cad.area`](#cadarea) | `core.area` | `ALAN` | Kapalı bir alan çizer; istenirse içine delik açar. |
| [`cad.rectangle`](#cadrectangle) | `core.rectangle` | `DİKDÖRTGEN` | Karşılıklı iki köşeden ya da bir kenar ve yükseklikten dört köşeli kapalı bir alan çizer. |
| [`cad.circle_draw`](#cadcircle_draw) | `core.circle_draw` | `DAİRE` | Merkez+çevre, çapın iki ucu, çember üzerinde üç nokta ya da iki doğruya teğet yarıçapla daire çizer. |
| [`cad.arc_draw`](#cadarc_draw) | `core.arc_draw` | `YAY` | Merkez+iki uç, yay üzerinde üç nokta, başlangıç+merkez+süpürme ya da başlangıç+bitiş+yarıçapla yay çizer. |
| [`cad.vertex_move`](#cadvertex_move) | `core.vertex_move` | `KÖŞETAŞI` | Bir nesnenin köşesini ya da tutamağını yeni bir yere taşır. |
| [`cad.vertex_insert`](#cadvertex_insert) | `core.vertex_insert` | `KÖŞEEKLE` | Bir kenarın ortasına yeni köşe ekler. |
| [`cad.vertex_delete`](#cadvertex_delete) | `core.vertex_delete` | `KÖŞESİL` | Bir çizginin, alanın, yaylı çoklu çizginin ya da spline'ın köşesini siler; iki kenar tek kenar olur. |
| [`cad.edge_kind`](#cadedge_kind) | `core.edge_kind` | `KENARTÜRÜ` | Bir kenarın türünü değiştirir: düz kenarı bir noktadan geçen yaya, yayı düz kenara çevirir; nesnenin kimliği korunur. |
| [`cad.to_area`](#cadto_area) | `core.to_area` | `ALANAÇEVİR` | Uç uca değen çizgileri tek bir kapalı alana çevirir. |
| [`cad.boundary`](#cadboundary) | `core.boundary` | `SINIR` | İçine tıklanan kapalı bölgenin sınırını yeni bir alan olarak çıkarır; içerideki adalar delik olur, açık uçlar gösterilir. |
| [`cad.cleanup`](#cadcleanup) | `core.cleanup` | `TEMİZLE` | Yinelenen, boş ve tekrarlanan köşeli nesneleri bulur; istenirse tek adımda onarır ve değişen alanları önce/sonra raporlar. |
| [`cad.move`](#cadmove) | `core.move` | `TAŞI` | Seçilen nesneleri iki nokta arasındaki kadar taşır. |
| [`cad.copy`](#cadcopy) | `core.copy` | `KOPYALA` | Seçilen nesnelerin kopyasını verilen her noktaya, başlangıçtan o noktaya kadar öteleyerek koyar. |
| [`cad.array`](#cadarray) | `core.array` | `DİZİ` | Seçilen nesneleri satır/sütun, bir merkez etrafında ya da bir yol boyunca çoğaltır. |
| [`cad.combine`](#cadcombine) | `core.combine` | `BİRLEŞTİR` | Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek çizgi yapar. |
| [`cad.split`](#cadsplit) | `core.split` | `BÖL` | Nesneleri bir kesme çizgisiyle, üstündeki noktalardan, kesişimlerinden, baştan bir uzaklıktan ya da eşit parçalara böler; yaylar yay kalır. |
| [`cad.trim`](#cadtrim) | `core.trim` | `BUDA` | Tıklanan parçayı kesme sınırları arasından atar; çizgide, yayda ve dairede çalışır. |
| [`cad.extend`](#cadextend) | `core.extend` | `UZAT` | Tıklanan ucu sınıra ulaşana kadar uzatır: çizginin ucunu doğrultusunda, yayınkini çemberi boyunca. |
| [`cad.chamfer`](#cadchamfer) | `core.chamfer` | `PAH` | Bir köşeyi ya da iki çizgi arasındaki köşeyi düz bir kenarla keser (pah kırar). |
| [`cad.fillet`](#cadfillet) | `core.fillet` | `YUVARLA` | Bir köşeyi ya da iki nesne (çizgi, yay) arasındaki köşeyi verilen yarıçapta yayla yuvarlatır; 0 yarıçap keskin köşe kurar. |
| [`cad.set_layer`](#cadset_layer) | `core.set_layer` | `KATMANAT` | Seçilen nesneleri başka bir katmana taşır. |
| [`cad.match_style`](#cadmatch_style) | `core.match_style` | `STİLKOPYALA` | Bir nesnenin stilini seçilen nesnelere uygular. |
| [`cad.colour`](#cadcolour) | `core.colour` | `RENK` | Seçili nesnelerin çizgi ve dolgu rengini değiştirir ya da katmanın rengine döndürür. |
| [`cad.rotate`](#cadrotate) | `core.rotate` | `DÖNDÜR` | Seçilen nesneleri bir merkez etrafında döndürür; açı verilir, gösterilir ya da bir referans doğrultudan bulunur. |
| [`cad.scale`](#cadscale) | `core.scale` | `ÖLÇEKLE` | Seçilen nesneleri bir merkeze göre büyütür ya da küçültür; iki çarpanla eşit olmayan ölçek, referans uzunlukla ölçek. |
| [`cad.mirror`](#cadmirror) | `core.mirror` | `AYNALA` | Seçilen nesneleri iki noktadan geçen eksende aynalar. |
| [`cad.measure`](#cadmeasure) | `core.measure` | `ÖLÇ` | Noktalar arasındaki mesafeyi, koordinat farkını ve açıyı yazar; ikiden fazla nokta kenarları ve toplam uzunluğu verir. |
| [`cad.measure_area`](#cadmeasure_area) | `core.measure_area` | `ALANÖLÇ` | Seçilen nesnelerin ya da köşeleri gösterilen bir alanın alanını ve çevresini yazar. |
| [`cad.coordinate`](#cadcoordinate) | `core.coordinate` | `KOORDİNAT` | Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar. |
| [`cad.pan`](#cadpan) | `core.pan` | `KAYDIR` | Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır. |
| [`cad.offset`](#cadoffset) | `core.offset` | `OFSET` | Seçili nesnelerin verilen mesafede, gösterilen tarafta paralelini çizer: açık çizgiye tek yanda çizgi, alana delikleriyle alan, daireye daire. |
| [`cad.sector`](#cadsector) | `core.sector` | `DİLİM` | Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir. |
| [`cad.annulus`](#cadannulus) | `core.annulus` | `HALKA` | Merkez, iç ve dış yarıçaptan delikli halka çizer. |
| [`cad.ellipse_draw`](#cadellipse_draw) | `core.ellipse_draw` | `ELİPS` | Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir. |
| [`cad.spline`](#cadspline) | `core.spline` | `SPLINE` | Kontrol noktalarından NURBS eğrisi (spline) çizer. |
| [`cad.hatch`](#cadhatch) | `core.hatch` | `TARAMA` | Kapalı nesnelerin ya da verilen köşelerin içini katalogdaki bir desenle tarar. |
| [`cad.block`](#cadblock) | `core.block` | `BLOK` | Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar. |
| [`cad.insert`](#cadinsert) | `core.insert` | `BLOKEKLE` | Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir. |
| [`cad.dimension`](#caddimension) | `core.dimension` | `ÖLÇÜ` | İki nokta arasını, bir yarıçapı, çapı ya da açıyı ölçüp yazısı ve oklarıyla çizer. |
| [`cad.leader`](#cadleader) | `core.leader` | `LİDER` | Bir noktayı gösteren oklu çizgi çizer, istenirse yanına yazı koyar. |
| [`cad.points`](#cadpoints) | `core.points` | `NOKTALAR` | Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod). |
| [`cad.guide`](#cadguide) | `core.guide` | `KILAVUZ` | Cetvel kılavuzu ve açılı kılavuz ekler, listeler ve siler. |
| [`cad.attribute`](#cadattribute) | `core.attribute` | `ÖZNİTELİK` | Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar. |
| [`cad.column`](#cadcolumn) | `core.column` | `SÜTUN` | Öznitelik sütunu tanımlar, düzenler, siler; argümansız çağrılınca listeler. |
| [`cad.erase`](#caderase) | `core.erase` | `SİL` | Seçilen nesneleri siler. |
| [`cad.select`](#cadselect) | `core.select` | `SEÇ` | Nesneleri seçer: tümü, kimlikle, katman, pencere, kesen kutu, çokgen, çit, önceki seçim, son nesne ya da tek nokta. |
| [`cad.label`](#cadlabel) | `core.label` | `ETİKET` | Katmandaki nesneleri özniteliklerinden okuyarak etiketler. |
| [`cad.layer`](#cadlayer) | `core.layer` | `KATMAN` | Katman oluşturur, aktif yapar ve özelliklerini değiştirir. |
| [`cad.layer_visibility`](#cadlayer_visibility) | `core.layer_visibility` | `KATMANGÖRÜNÜM` | Katmanların görünürlüğünü toptan değiştirir: bir katmanı gösterir ya da gizler, yalnız onu bırakır, hepsini gösterir veya görünürlüğü ters çevirir. |
| [`cad.layout`](#cadlayout) | `core.layout` | `ÇIKTIYERLEŞİMİ` | Çizimin çıktı yerleşimlerini yönetir: yeni yerleşim açar, siler, adlandırır ve kâğıdını değiştirir. Yerleşim çizimle birlikte kaydedilir ve geri alınabilir. |
| [`cad.layout_item`](#cadlayout_item) | `core.layout_item` | `ÇIKTIÖĞE` | Bir çıktı yerleşiminin üzerindeki öğeleri yönetir: harita çerçevesi, başlık, ölçek çubuğu, kuzey oku, lejant, resim, şekil ve tablo ekler, taşır, ayarlar ve siler. |
| [`cad.layout_template`](#cadlayout_template) | `core.layout_template` | `ÇIKTIŞABLON` | Kurumun standart çıktı yerleşimlerini saklar ve uygular. Şablon çizimin dışında, kullanıcı profilinde durur; her çizime uygulanabilir. Şablon düzeni taşır, zemin koordinatlarını taşımaz. |
| [`cad.style`](#cadstyle) | `core.style` | `STİL` | Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar. |
| [`cad.symbol`](#cadsymbol) | `core.symbol` | `SEMBOL` | Gösterim rafını yükler, ağacında gezer ve içinde arar. |
| [`cad.zoom`](#cadzoom) | `core.zoom` | `YAKINLAŞ` | Görünümü çizim kapsamına veya verilen çarpana ayarlar. |
| [`cad.undo`](#cadundo) | `core.undo` | `GERİAL` | Son işlemi geri alır. |
| [`cad.redo`](#cadredo) | `core.redo` | `YİNELE` | Geri alınan işlemi yineler. |
| [`cad.new`](#cadnew) | `core.new` | `YENİ` | Boş bir çizim açar; ekrandaki çizimin yerine geçer. |
| [`cad.open`](#cadopen) | `core.open` | `AÇ` | Bir KentOSCad proje dosyasını açar ve çizimin yerine koyar. |
| [`cad.save`](#cadsave) | `core.save` | `KAYDET` | Çizimi bağlı olduğu KentOSCad proje dosyasına kaydeder. |
| [`cad.saveas`](#cadsaveas) | `core.saveas` | `FARKLIKAYDET` | Çizimi yeni bir KentOSCad proje dosyasına kaydeder ve ona bağlar. |
| [`cad.import`](#cadimport) | `core.import` | `İÇEAKTAR` | Dış bir veri dosyasını çizime ekler. |
| [`cad.export`](#cadexport) | `core.export` | `DIŞAAKTAR` | Çizimi dış bir veri biçimine yazar. |
| [`cad.script`](#cadscript) | `core.script` | `BETİK` | Bir betik dosyasını komut veri yolu üzerinden çalıştırır. |
| [`cad.python`](#cadpython) | `core.python` | `PYTHON` | Bir Python parçacığını komut veri yolu üzerinden çalıştırır. |
| [`cad.database`](#caddatabase) | `core.database` | `VERİTABANI` | PostGIS veritabanına bağlanır; katmanları tablo, projeleri kayıt olarak yazar. |
| [`cad.print`](#cadprint) | `core.print` | `YAZDIR` | Çizimin bir penceresini bir yazdırma profilinin kâğıdına yerleştirip PDF dosyasına yazar ya da yazıcıya gönderir. |
| [`cad.print_profile`](#cadprint_profile) | `core.print_profile` | `YAZDIRMAPROFİLİ` | Yazdırma profillerini listeler, ekler, siler ya da birini varsayılan yapar; profil kâğıdı, yönü, çözünürlüğü ve kenar boşluğunu taşır. |
| [`cad.setting`](#cadsetting) | `core.setting` | `AYAR` | Proje ayarlarını listeler, okur ve değiştirir. |
| [`cad.preference`](#cadpreference) | `core.preference` | `TERCİH` | Uygulama tercihlerini listeler, okur ve değiştirir. |
| [`cad.mode`](#cadmode) | `core.mode` | `MOD` | Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur ve değiştirir. |
| [`cad.help`](#cadhelp) | `core.help` | `YARDIM` | Komut listesini veya tek bir komutun ayrıntısını gösterir. |
| [`cad.buffer`](#cadbuffer) | `islem.tampon` | `TAMPON` | Kapsamdaki nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer: çizginin iki yanı, noktanın çevresi, alanın dışı; üst üste binen tamponlar tek alan olur. |
| [`cad.adjust_area`](#cadadjust_area) | `islem.alan_duzenle` | `ALANDÜZENLE` | Kapalı bir alanı istenen alana getirir: bütün kenarları eşit daraltıp genişleterek, bir kenarı kaydırarak ya da bir köşeyi çekerek; şekil bozulmaz. |
| [`cad.label_length`](#cadlabel_length) | `islem.uzunluk_yaz` | `UZUNLUKYAZ` | Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel bir yazı olarak yazar; yazı kenara bağlıdır, kenar değişince izler ve yenilenir. |
| [`cad.number_vertices`](#cadnumber_vertices) | `islem.kose_numarala` | `KÖŞENUMARALA` | Kapsamdaki her alanın (ve çizginin) köşelerini seçilen köşeden başlayarak sırayla numaralar ve numarayı köşenin dışına yazar; numara köşesine bağlıdır, köşe taşınınca izler. |
| [`cad.detach`](#caddetach) | `islem.bag_coz` | `BAĞÇÖZ` | Kapsamdaki yazıların bağını çözer: yazı yerinde kalır, bağlı olduğu nesne bundan sonra tek başına taşınır. |
| [`cad.attach`](#cadattach) | `islem.bagla` | `BAĞLA` | Kapsamdaki yazıları seçilen nesnenin en yakın kenarına ya da köşesine bağlar: nesne taşınınca yazı izler; istenirse yazı kenarın uzunluğu olur. |
| [`cad.polygonize`](#cadpolygonize) | `islem.alan_uret` | `ALANÜRET` | Kapsamdaki çizgilerin kapattığı her gözü ayrı bir alan olarak çizer; içerideki adalar delik olur, açık uçlar sayılıp gösterilir ve hiçbiri kendiliğinden kapanmaz. |
| [`cad.fit`](#cadfit) | `core.fit` | `OTURT` | Yerel ölçülmüş çizimi kontrol noktalarıyla haritaya oturtur (2B Helmert). |
| [`cad.stakeout`](#cadstakeout) | `core.stakeout` | `APLİKASYON` | İstasyondan her noktaya mesafe ve açı listesi çıkarır (aplikasyon). |
| [`cad.reproject`](#cadreproject) | `core.reproject` | `DÖNÜŞTÜR` | Çizimin tamamını bir koordinat sisteminden diğerine dönüştürür. |
| [`cad.traverse`](#cadtraverse) | `geodesy.traverse` | `POLİGON` | Kırılma açısı ve kenarlardan poligon koordinatları hesaplar, kapanma hatalarını dağıtır ve mevzuat toleransına karşı denetler. |
| [`cad.merge`](#cadmerge) | `core.merge` | `TEVHİT` | Komşu parselleri tek parselde birleştirir (tevhit). |
| [`cad.split_parcel`](#cadsplit_parcel) | `core.split_parcel` | `İFRAZ` | Bir parseli düz bir ayırma çizgisiyle ikiye böler (ifraz). |
| [`cad.split_area`](#cadsplit_area) | `core.split_area` | `ALANİFRAZ` | Parselden verilen yöne paralel, istenen alanda bir parça ayırır. |
| [`cad.topology`](#cadtopology) | `core.topology` | `TOPOLOJİ` | Kendini kesen sınır, sıfır alan ve örtüşen parselleri; yinelenen ve boş nesneleri, tekrarlanan köşeleri ve çizgi ağındaki boşlukları raporlar. |
| [`cad.contour`](#cadcontour) | `core.contour` | `EŞYÜKSELTİ` | Kotlu noktalardan eş yükselti eğrileri çizer. |
| [`cad.earthwork`](#cadearthwork) | `core.earthwork` | `HACİM` | Kotlu noktalardan bir kota göre kazı ve dolgu hacmini hesaplar. |
| [`cad.layers`](#cadlayers) | `core.layers` | `KATMANLAR` | Katmanları, nesne sayılarını, görünürlük ve kilit durumlarını listeler. |
| [`cad.attr_schema`](#cadattr_schema) | `core.attr_schema` | `ÖZNİTELİKŞEMASI` | Çizimde tanımlı öznitelik sütunlarını ve tiplerini listeler. |
| [`cad.query`](#cadquery) | `core.query` | `SORGULA` | Katman ve öznitelik koşuluna uyan nesneleri sayar ve anahtarlarını bildirir. |
| [`cad.selection_info`](#cadselection_info) | `core.selection_info` | `SEÇİMBİLGİSİ` | Kullanıcının o anki seçimini bildirir: kaç nesne ve hangi anahtarlar. |
| [`cad.object_points`](#cadobject_points) | `core.object_points` | `NESNENOKTALARI` | Nesnelerin merkezini, köşelerini, uçlarını, kutusunu ya da kenar ortalarını bildirir; bir ajan bunları yeni çizimin taban noktası olarak kullanır. |
| [`cad.view_info`](#cadview_info) | `core.view_info` | `GÖRÜNÜMBİLGİSİ` | Ekranda görünen alanın köşe koordinatlarını, merkezini, ölçeğini ve CRS'ini bildirir. |
| [`cad.context`](#cadcontext) | `core.context` | `BAĞLAM` | Üzerinde çalışılan her şeyi tek çağrıda özetler: belge sürümü, koordinat sistemi, kapsam, katmanlar, çıktı yerleşimleri ve hedefli olup olmadıkları, seçili nesneler ve görünüm. Özet verir, döküm değil. |
| [`cad.tool_search`](#cadtool_search) | `core.tool_search` | `ARAÇARA` | Ajan araç kataloğunda ad ve özete göre arar. Sonuç her zaman kaç aracın eşleştiğini, kaçının gösterildiğini ve katalogdaki toplam araç sayısını söyler: arama hiçbir aracı gizlemez, tam liste `tools/list` ile alınır. |
| [`cad.job_template`](#cadjob_template) | `core.job_template` | `İŞŞABLONU` | Sık yapılan işlerin — atlas, kadastro kontrolü, parsel raporu — komut satırlarını sırasıyla verir. Hiçbirini çalıştırmaz: adımlar olağan araç yüzeyinden gönderilir ve yazan her adım yine öneri olur. |
| [`cad.suggestion`](#cadsuggestion) | `core.suggestion` | `ÖNERİ` | Bekleyen yapay zeka önerilerini listeler, durumunu söyler, uygular ya da reddeder. |
| [`cad.mcp`](#cadmcp) | `core.mcp` | `MCPSUNUCU` | Yapay zeka ajanlarının bağlanacağı MCP sunucusunu başlatır, durdurur, durumunu söyler, yeni bir erişim belirteci üretir, bağlı istemcileri listeler ve tek bir istemcinin yetkisini kaldırır. |
| [`cad.ai_provider`](#cadai_provider) | `core.ai_provider` | `YAPAYZEKAMODELİ` | Yapay zeka model sağlayıcılarını listeler, ekler, siler, birini varsayılan yapar ya da bağlantısını dener; profil adresi, lehçesi, modeli ve anahtar adını taşır. |

---

### `cad.line`

İki veya daha fazla nokta arasında doğru parçaları çizer.

Komut: `core.line` — `ÇİZGİ`

```python
cad.line(
    points: Coords,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `points` | `Coords` | `noktalar` | Ardışık doğru parçalarının köşe noktaları [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/line.md)

### `cad.polyline`

Birden çok noktadan TEK bir çizgi nesnesi çizer.

Komut: `core.polyline` — `ÇOKLUÇİZGİ`

```python
cad.polyline(
    points: Coords,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `points` | `Coords` | `noktalar` | Çoklu çizginin köşe noktaları; hepsi tek nesne olur [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/polyline.md)

### `cad.point_draw`

Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper.

Komut: `core.point_draw` — `NOKTA`

```python
cad.point_draw(
    points: Coords,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `points` | `Coords` | `noktalar` | Yerleştirilecek noktalar [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/point_draw.md)

### `cad.perp_offset`

Taban çizgisine göre dik ayak ve dik boy vererek nokta yerleştirir.

Komut: `core.perp_offset` — `DİKAYAK`

```python
cad.perp_offset(
    start: Coord,
    end: Coord,
    chainage: list[float],
    offset: list[float],
    connect: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `start` | `Coord` | `baslangic` | Taban çizgisinin ilk noktası (A) [mm, Sağa (Y) önce] |
| `end` | `Coord` | `bitis` | Taban çizgisinin ikinci noktası (B) [mm, Sağa (Y) önce] |
| `chainage` | `list[float]` | `ayak` | A'dan taban boyunca uzaklık (m); boy ile sırayla eşleşir |
| `offset` | `list[float]` | `boy` | Tabana dik uzaklık (m); A→B yönünde SOL pozitiftir |
| `connect` | `bool` | `cizgi` | Yerleştirilen noktaları verildikleri sırayla çizgiyle birleştirir |

[Komut sayfası](../komutlar/perp_offset.md)

### `cad.survey_polar`

İstasyondan okunan açı ve kenarlardan nokta hesaplar ve yerleştirir.

Komut: `core.survey_polar` — `ALIM`

```python
cad.survey_polar(
    station: Coord,
    backsight: Coord,
    angle: list[float],
    distance: list[float],
    connect: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `station` | `Coord` | `istasyon` | Aletin durduğu bilinen nokta [mm, Sağa (Y) önce] |
| `backsight` | `Coord` | `baglama` | Bağlama noktası: verilirse açılar ondan itibaren okunmuş sayılır [mm, Sağa (Y) önce] |
| `angle` | `list[float]` | `aci` | Okunan açı; kenar ile sırayla eşleşir [oturumun açı birimi] |
| `distance` | `list[float]` | `kenar` | Alete olan uzaklık (m) [m] |
| `connect` | `bool` | `cizgi` | Hesaplanan noktaları okundukları sırayla çizgiyle birleştirir |

[Komut sayfası](../komutlar/survey_polar.md)

### `cad.intersect_point`

İki doğrultunun, iki uzaklığın ya da iki doğrunun kesişimine nokta koyar.

Komut: `core.intersect_point` — `KESİŞİMNOKTA`

```python
cad.intersect_point(
    method: str,
    first: Coord,
    second: Coord,
    third: Coord,
    fourth: Coord,
    first_angle: float,
    second_angle: float,
    first_distance: float,
    second_distance: float,
    side: str,
    side_point: Coord,
    intersection: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `method` | `str` | `yontem` | dogrultu: iki doğrultu · mesafe: iki uzaklık · dogru: iki doğru |
| `first` | `Coord` | `birinci` | Birinci bilinen nokta [mm, Sağa (Y) önce] |
| `second` | `Coord` | `ikinci` | İkinci bilinen nokta [mm, Sağa (Y) önce] |
| `third` | `Coord` | `ucuncu` | İkinci doğrunun ilk noktası [mm, Sağa (Y) önce] |
| `fourth` | `Coord` | `dorduncu` | İkinci doğrunun ikinci noktası [mm, Sağa (Y) önce] |
| `first_angle` | `float` | `birinci_aci` | Birinci noktadan okunan doğrultu |
| `second_angle` | `float` | `ikinci_aci` | İkinci noktadan okunan doğrultu |
| `first_distance` | `float` | `birinci_mesafe` | Birinci noktadan ölçülen uzaklık (m) |
| `second_distance` | `float` | `ikinci_mesafe` | İkinci noktadan ölçülen uzaklık (m) |
| `side` | `str` | `yon` | İki uzaklık kesişiminin hangi çözümü; birinci→ikinci yönüne göre |
| `side_point` | `Coord` | `yon_nokta` | mesafe: iki çözümden istenenin gösterildiği nokta; yon verilmişse sorulmaz [mm, Sağa (Y) önce] |
| `intersection` | `Coord` | `kesisim` | Bulunan nokta; günlüğe yazılır [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/intersect_point.md)

### `cad.point_along`

İki nokta arasındaki doğru üzerinde oran, uzaklık ya da eşit bölmeyle nokta koyar.

Komut: `core.point_along` — `ARANOKTA`

```python
cad.point_along(
    first: Coord,
    second: Coord,
    method: str,
    value: list[float],
    count: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `first` | `Coord` | `birinci` | Doğrunun ilk noktası [mm, Sağa (Y) önce] |
| `second` | `Coord` | `ikinci` | Doğrunun ikinci noktası [mm, Sağa (Y) önce] |
| `method` | `str` | `yontem` | oran: 0 ile 1 arası · mesafe: ilk noktadan metre |
| `value` | `list[float]` | `deger` | Oran ya da uzaklık; birden çok verilebilir |
| `count` | `int` | `sayi` | Doğruyu bu kadar eşit parçaya böler |

[Komut sayfası](../komutlar/point_along.md)

### `cad.polygon_regular`

Merkez ve kenar sayısından düzgün çokgen çizer: içten, dıştan ya da kenar uzunluğundan.

Komut: `core.polygon_regular` — `ÇOKGEN`

```python
cad.polygon_regular(
    center: Coord,
    sides: int,
    method: str,
    radius: float,
    side_length: float,
    angle: float,
    corner: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `center` | `Coord` | `merkez` | Çokgenin merkezi [mm, Sağa (Y) önce] |
| `sides` | `int` | `kenar_sayisi` | Kenar sayısı |
| `method` | `str` | `yontem` | ic: köşeler çemberin üzerinde · dis: kenarlar çembere teğet · kenar: kenar uzunluğundan |
| `radius` | `float` | `yaricap` | ic/dis yönteminin yarıçapı (m) [m] |
| `side_length` | `float` | `kenar_uzunlugu` | kenar yönteminin uzunluğu (m) [m] |
| `angle` | `float` | `aci` | İlk köşenin merkeze göre doğrultusu; varsayılan 0 |
| `corner` | `Coord` | `kose` | Yerine işaret edilen nokta: yarıçapı ve yönü verir; yaricap verilmişse sorulmaz [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/polygon_regular.md)

### `cad.break`

Çizgiden, yaydan, daireden ya da yaylı çoklu çizgiden iki nokta arasındaki parçayı çıkarır; tek nokta açık bir nesneyi boşluk bırakmadan böler.

Komut: `core.break` — `KIR`

```python
cad.break(
    object: list[int],
    first: Coord,
    second: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Kırılacak nesne: çizgi, yay, daire ya da yaylı çoklu çizgi [kalıcı nesne anahtarı] |
| `first` | `Coord` | `birinci` | Kırılacak parçanın ilk noktası [mm, Sağa (Y) önce] |
| `second` | `Coord` | `ikinci` | Kırılacak parçanın ikinci noktası; verilmezse boşluk bırakmadan böler [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/break.md)

### `cad.join`

Uçları birbirine değen çizgileri, yayları ve yaylı çoklu çizgileri tek bir nesneye ekler; yaylar yay kalır, boşluklar söylenir.

Komut: `core.join` — `UÇUCA`

```python
cad.join(
    object: list[int],
    tolerance: float,
    on_conflict: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Uç uca eklenecek çizgiler, yaylar ve yaylı çoklu çizgiler [kalıcı nesne anahtarı] |
| `tolerance` | `float` | `tolerans` | Uçların değmiş sayılması için en büyük açıklık (m); varsayılan 0,001. Aradaki boşluk doğru parçasıyla kapatılır ve söylenir [m] |
| `on_conflict` | `str` | `cakisma` | ilk: katman, stil ve öznitelikler ilk nesneden, farklar söylenir · reddet: katman ya da öznitelik farklıysa birleştirmez |

[Komut sayfası](../komutlar/join.md)

### `cad.lengthen`

Çizginin bir ucunu kendi doğrultusunda hareket ettirerek uzunluğunu değiştirir.

Komut: `core.lengthen` — `UZUNLUK`

```python
cad.lengthen(
    object: list[int],
    delta: float,
    percent: float,
    total: float,
    which_end: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Uzunluğu değişecek çizgi [kalıcı nesne anahtarı] |
| `delta` | `float` | `delta` | Eklenecek uzunluk (m); eksi kısaltır [m] |
| `percent` | `float` | `yuzde` | İstenen uzunluk, şimdikinin yüzdesi |
| `total` | `float` | `toplam` | İstenen toplam uzunluk (m) [m] |
| `which_end` | `str` | `uc` | Hangi uç hareket eder; varsayılan son |

[Komut sayfası](../komutlar/lengthen.md)

### `cad.explode`

Çizgiyi tek tek kenarlara, alanı sınırına, blok referansını bileşenlerine ayırır.

Komut: `core.explode` — `PATLAT`

```python
cad.explode(
    object: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Patlatılacak nesneler [kalıcı nesne anahtarı] |

[Komut sayfası](../komutlar/explode.md)

### `cad.align`

Bir ya da iki nokta çiftiyle nesneleri taşır, döndürür ve istenirse ölçekler.

Komut: `core.align` — `HİZALA`

```python
cad.align(
    object: list[int],
    source: Coord,
    target: Coord,
    source2: Coord,
    target2: Coord,
    scale: bool,
    source3: Coord,
    target3: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Hizalanacak nesneler [kalıcı nesne anahtarı] |
| `source` | `Coord` | `kaynak` | Birinci kaynak nokta [mm, Sağa (Y) önce] |
| `target` | `Coord` | `hedef` | Birinci kaynağın gideceği yer [mm, Sağa (Y) önce] |
| `source2` | `Coord` | `kaynak2` | İkinci kaynak nokta; verilirse döndürme de yapılır [mm, Sağa (Y) önce] |
| `target2` | `Coord` | `hedef2` | İkinci kaynağın gideceği yer [mm, Sağa (Y) önce] |
| `scale` | `bool` | `olcekle` | İki çiftin uzunluk oranıyla ölçekler de |
| `source3` | `Coord` | `kaynak3` | Üçüncü kaynak nokta: hedefi ilk iki hedefin öbür yanındaysa nesneler ters çevrilir [mm, Sağa (Y) önce] |
| `target3` | `Coord` | `hedef3` | Üçüncü kaynağın gideceği yan [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/align.md)

### `cad.divide`

Bir nesne boyunca eşit parçalara bölerek ya da sabit aralıkla nokta veya blok yerleştirir.

Komut: `core.divide` — `BÖLÜMLE`

```python
cad.divide(
    object: list[int],
    count: int,
    spacing: float,
    block: str,
    align: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Bölünecek nesne [kalıcı nesne anahtarı] |
| `count` | `int` | `sayi` | Kaç eşit parçaya bölünecek |
| `spacing` | `float` | `aralik` | Sabit aralık (m); başlangıçtan itibaren yürür [m] |
| `block` | `str` | `blok` | Nokta yerine bu bloğu koyar; blok önceden tanımlı olmalı |
| `align` | `bool` | `hizala` | Bloğu üzerinde durduğu kenarın doğrultusuna çevirir |

[Komut sayfası](../komutlar/divide.md)

### `cad.pedit`

Çizgiyi kapatır, açar, yönünü çevirir ya da yakın köşelerini atarak sadeleştirir.

Komut: `core.pedit` — `ÇİZGİDÜZENLE`

```python
cad.pedit(
    object: list[int],
    action: str,
    tolerance: float,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Düzenlenecek çizgiler [kalıcı nesne anahtarı] |
| `action` | `str` | `islem` | kapat: kapalı alana çevir · ac: aç · ters: yönünü çevir · sadelestir: yakın köşeleri at |
| `tolerance` | `float` | `tolerans` | sadelestir: bu uzaklıktan yakın köşeler atılır (m) [m] |

[Komut sayfası](../komutlar/pedit.md)

### `cad.copy_clip`

Seçili nesneleri çizimin kendi biçiminde panoya yazar.

Komut: `core.copy_clip` — `PANOYAKOPYALA`

```python
cad.copy_clip(
    objects: list[int],
    file: str,
    base_point: Coord,
    with_base: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Panoya alınacak nesneler; verilmezse seçim kullanılır [kalıcı nesne anahtarı] |
| `file` | `str` | `dosya` | Panonun yazılacağı dosya; verilmezse ortak pano dosyası |
| `base_point` | `Coord` | `taban` | Yapıştırırken gösterilen yere gelecek taban noktası [mm, Sağa (Y) önce] |
| `with_base` | `bool` | `tabanli` | evet: taban noktası nesneler seçildikten sonra sorulur |

[Komut sayfası](../komutlar/copy_clip.md)

### `cad.cut`

Seçili nesneleri panoya alır ve çizimden siler; tek geri alma adımı.

Komut: `core.cut` — `KES`

```python
cad.cut(
    objects: list[int],
    file: str,
    base_point: Coord,
    with_base: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Kesilecek nesneler; verilmezse seçim kullanılır [kalıcı nesne anahtarı] |
| `file` | `str` | `dosya` | Panonun yazılacağı dosya; verilmezse ortak pano dosyası |
| `base_point` | `Coord` | `taban` | Yapıştırırken gösterilen yere gelecek taban noktası [mm, Sağa (Y) önce] |
| `with_base` | `bool` | `tabanli` | evet: taban noktası nesneler seçildikten sonra sorulur |

[Komut sayfası](../komutlar/cut.md)

### `cad.paste`

Panodaki nesneleri çizime koyar; tek geri alma adımı.

Komut: `core.paste` — `YAPIŞTIR`

```python
cad.paste(
    point: Coord,
    in_place: bool,
    file: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `point` | `Coord` | `nokta` | Yapıştırılacak yerin sol alt köşesi; yerinde=evet ile gereksiz [mm, Sağa (Y) önce] |
| `in_place` | `bool` | `yerinde` | Kopyalandığı koordinatlara yapıştırır |
| `file` | `str` | `dosya` | Okunacak pano dosyası; verilmezse ortak pano dosyası |

[Komut sayfası](../komutlar/paste.md)

### `cad.entity_info`

Nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve özniteliklerini bildirir.

Komut: `core.entity_info` — `NESNEBİLGİ`

```python
cad.entity_info(
    objects: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Bilgisi istenen nesneler [kalıcı nesne anahtarı] |

[Komut sayfası](../komutlar/entity_info.md)

### `cad.measure_angle`

Bir tepeden çıkan iki kol arasındaki açıyı ölçer, oturumun açı kuralıyla yazar.

Komut: `core.measure_angle` — `AÇIÖLÇ`

```python
cad.measure_angle(
    apex: Coord,
    first: Coord,
    second: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `apex` | `Coord` | `tepe` | Açının tepe noktası [mm, Sağa (Y) önce] |
| `first` | `Coord` | `birinci` | Birinci kolun üzerinde bir nokta [mm, Sağa (Y) önce] |
| `second` | `Coord` | `ikinci` | İkinci kolun üzerinde bir nokta [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/measure_angle.md)

### `cad.stretch`

Pencere içindeki köşeleri taşır, dışındakileri yerinde bırakır.

Komut: `core.stretch` — `ESNET`

```python
cad.stretch(
    window: Coords,
    start: Coord,
    end: Coord,
    objects: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `window` | `Coords` | `pencere` | Esnetme penceresinin iki köşesi; içindeki köşeler taşınır [mm, Sağa (Y) önce] |
| `start` | `Coord` | `baslangic` | Esnetmenin başlangıç noktası [mm, Sağa (Y) önce] |
| `end` | `Coord` | `bitis` | Esnetmenin bitiş noktası [mm, Sağa (Y) önce] |
| `objects` | `list[int]` | `nesneler` | Yalnız bu nesneler esnetilir; verilmezse pencerenin dokunduğu her nesne [kalıcı nesne anahtarı] |

[Komut sayfası](../komutlar/stretch.md)

### `cad.tracking`

Geçici izleme için nokta işaretler; iki işaretin izleri kesişir.

Komut: `core.tracking` — `İZ`

```python
cad.tracking(
    point: Coord,
    delete: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `point` | `Coord` | `nokta` | İşaretlenecek nokta; yoksa işaretler listelenir [mm, Sağa (Y) önce] |
| `delete` | `bool` | `sil` | Bütün işaretleri siler |

[Komut sayfası](../komutlar/tracking.md)

### `cad.text`

Çizime metin yazar; yükseklik ve hizalama verilebilir.

Komut: `core.text` — `METİN`

```python
cad.text(
    points: Coord,
    text: str,
    height: int,
    end: Coord,
    alignment: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `points` | `Coord` | `noktalar` | Yazının başlangıç noktası [mm, Sağa (Y) önce] |
| `text` | `str` | `yazi` | Yazılacak metin |
| `height` | `int` | `yukseklik` | Yazı yüksekliği, zeminde milimetre; yoksa proje ayarı |
| `end` | `Coord` | `bitis` | Taban çizgisinin bitişi; yoksa yatay [mm, Sağa (Y) önce] |
| `alignment` | `str` | `hizalama` | sol, orta, sag veya merkez |

[Komut sayfası](../komutlar/text.md)

### `cad.edittext`

Var olan bir yazının metnini, yüksekliğini ya da hizalamasını değiştirir.

Komut: `core.edittext` — `YAZIDÜZENLE`

```python
cad.edittext(
    objects: list[int],
    text: str,
    height: int,
    alignment: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Düzenlenecek yazılar; verilmezse seçim [kalıcı nesne anahtarı] |
| `text` | `str` | `yazi` | Yeni metin; verilmezse değişmez |
| `height` | `int` | `yukseklik` | Yeni yükseklik, zeminde milimetre; verilmezse değişmez |
| `alignment` | `str` | `hizalama` | sol, orta, sag veya merkez; verilmezse değişmez |

[Komut sayfası](../komutlar/edittext.md)

### `cad.exportstyle`

Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar.

Komut: `core.exportstyle` — `STİLAKTAR`

```python
cad.exportstyle(
    layer: str,
    file: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `layer` | `str` | `katman` | Stili aktarılacak katmanın adı |
| `file` | `str` | `dosya` | Yazılacak .qml dosyasının yolu |

[Komut sayfası](../komutlar/exportstyle.md)

### `cad.area`

Kapalı bir alan çizer; istenirse içine delik açar.

Komut: `core.area` — `ALAN`

```python
cad.area(
    points: Coords,
    rings: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `points` | `Coords` | `noktalar` | Alanın köşe noktaları; kapanış noktası tekrarlanmaz [mm, Sağa (Y) önce] |
| `rings` | `list[int]` | `bolum` | Halka uzunlukları: ilki dış sınır, sonrakiler delik |

[Komut sayfası](../komutlar/area.md)

### `cad.rectangle`

Karşılıklı iki köşeden ya da bir kenar ve yükseklikten dört köşeli kapalı bir alan çizer.

Komut: `core.rectangle` — `DİKDÖRTGEN`

```python
cad.rectangle(
    points: Coords,
    method: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `points` | `Coords` | `noktalar` | 2n: karşılıklı iki köşe · 3n: bir kenarın iki köşesi ve karşı kenarın geçtiği nokta [mm, Sağa (Y) önce] |
| `method` | `str` | `yontem` | 2n: karşılıklı iki köşe, eksenlere paralel · 3n: bir kenar ve yükseklik, döndürülmüş |

[Komut sayfası](../komutlar/rectangle.md)

### `cad.circle_draw`

Merkez+çevre, çapın iki ucu, çember üzerinde üç nokta ya da iki doğruya teğet yarıçapla daire çizer.

Komut: `core.circle_draw` — `DAİRE`

```python
cad.circle_draw(
    center: Coord,
    rim: Coord,
    method: str,
    first: Coord,
    second: Coord,
    third: Coord,
    fourth: Coord,
    radius: float,
    side: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `center` | `Coord` | `merkez` | Dairenin merkezi [mm, Sağa (Y) önce] |
| `rim` | `Coord` | `cevre` | Çember üzerinde bir nokta; yarıçapı bu belirler [mm, Sağa (Y) önce] |
| `method` | `str` | `yontem` | merkez: merkez + çevre · 2n: çapın iki ucu · 3n: çember üzerinde üç nokta · ttr: iki doğruya teğet, verilen yarıçapla |
| `first` | `Coord` | `birinci` | 2n: çapın bir ucu · 3n: birinci nokta · ttr: birinci doğrunun ilk noktası [mm, Sağa (Y) önce] |
| `second` | `Coord` | `ikinci` | İkinci nokta [mm, Sağa (Y) önce] |
| `third` | `Coord` | `ucuncu` | 3n: üçüncü nokta · ttr: ikinci doğrunun ilk noktası [mm, Sağa (Y) önce] |
| `fourth` | `Coord` | `dorduncu` | ttr: ikinci doğrunun ikinci noktası [mm, Sağa (Y) önce] |
| `radius` | `float` | `yaricap` | ttr: teğet dairenin yarıçapı (m) [m] |
| `side` | `Coord` | `yon` | ttr: dairenin geleceği köşe; dört çözümden en yakını alınır [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/circle_draw.md)

### `cad.arc_draw`

Merkez+iki uç, yay üzerinde üç nokta, başlangıç+merkez+süpürme ya da başlangıç+bitiş+yarıçapla yay çizer.

Komut: `core.arc_draw` — `YAY`

```python
cad.arc_draw(
    center: Coord,
    start: Coord,
    end: Coord,
    method: str,
    through: Coord,
    sweep: float,
    radius: float,
    side_point: Coord,
    side: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `center` | `Coord` | `merkez` | Yayın merkezi [mm, Sağa (Y) önce] |
| `start` | `Coord` | `baslangic` | Yayın başlangıç noktası; merkez yönteminde yarıçapı bu belirler [mm, Sağa (Y) önce] |
| `end` | `Coord` | `bitis` | Yayın bitiş noktası; süpürme saat yönünün tersinedir [mm, Sağa (Y) önce] |
| `method` | `str` | `yontem` | merkez: merkez + iki uç · 3n: yay üzerinde üç nokta · bma: başlangıç, merkez ve süpürme açısı · bby: başlangıç, bitiş ve yarıçap |
| `through` | `Coord` | `uzerinden` | 3n: yayın üzerinden geçtiği nokta [mm, Sağa (Y) önce] |
| `sweep` | `float` | `supurme` | bma: süpürme açısı |
| `radius` | `float` | `yaricap` | bby: yarıçap (m) [m] |
| `side_point` | `Coord` | `yon_nokta` | bby: yayın hangi yandan geçeceği gösterilen nokta; yon verilmişse sorulmaz [mm, Sağa (Y) önce] |
| `side` | `str` | `yon` | bby: yayın hangi tarafa kavis yaptığı; başlangıç→bitiş yönüne göre |

[Komut sayfası](../komutlar/arc_draw.md)

### `cad.vertex_move`

Bir nesnenin köşesini ya da tutamağını yeni bir yere taşır.

Komut: `core.vertex_move` — `KÖŞETAŞI`

```python
cad.vertex_move(
    object: list[int],
    vertex: int,
    at: Coord,
    shared_point: Coord,
    point: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Köşesi taşınacak nesne; birden çok nesne verilirse ortak köşeleri birlikte taşınır [kalıcı nesne anahtarı] |
| `vertex` | `int` | `kose` | Taşınacak köşenin sırası; ilk köşe 1'dir. Birden çok nesnede birincinin köşesi; verilmezse yer ya da kaynak |
| `at` | `Coord` | `yer` | Köşeyi gösteren nokta: kose verilmezse en yakın köşe, nesne de verilmezse altındaki nesne [mm, Sağa (Y) önce] |
| `shared_point` | `Coord` | `kaynak` | Ortak köşenin bugünkü yeri: verilen nesnelerin o noktadaki bütün köşe ve tutamakları birlikte taşınır [mm, Sağa (Y) önce] |
| `point` | `Coord` | `nokta` | Köşenin yeni yeri [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/vertex_move.md)

### `cad.vertex_insert`

Bir kenarın ortasına yeni köşe ekler.

Komut: `core.vertex_insert` — `KÖŞEEKLE`

```python
cad.vertex_insert(
    object: list[int],
    vertex: int,
    at: Coord,
    point: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Köşe eklenecek nesnenin kimliği [kalıcı nesne anahtarı] |
| `vertex` | `int` | `kose` | Yeni köşenin ardına geleceği köşe; ilk köşe 1'dir |
| `at` | `Coord` | `yer` | Kenarı gösteren nokta: kose verilmezse en yakın kenar, nesne de verilmezse altındaki nesne [mm, Sağa (Y) önce] |
| `point` | `Coord` | `nokta` | Yeni köşenin yeri [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/vertex_insert.md)

### `cad.vertex_delete`

Bir çizginin, alanın, yaylı çoklu çizginin ya da spline'ın köşesini siler; iki kenar tek kenar olur.

Komut: `core.vertex_delete` — `KÖŞESİL`

```python
cad.vertex_delete(
    object: list[int],
    vertex: int,
    at: Coord,
    shared_point: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Köşesi silinecek nesne; birden çok nesne verilirse ortak köşeleri birlikte silinir [kalıcı nesne anahtarı] |
| `vertex` | `int` | `kose` | Silinecek köşenin sırası; ilk köşe 1'dir. Verilmezse yer ya da kaynak |
| `at` | `Coord` | `yer` | Köşeyi gösteren nokta: kose verilmezse en yakın köşe, nesne de verilmezse altındaki nesne [mm, Sağa (Y) önce] |
| `shared_point` | `Coord` | `kaynak` | Ortak köşenin yeri: verilen nesnelerin o noktadaki köşesi birlikte silinir [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/vertex_delete.md)

### `cad.edge_kind`

Bir kenarın türünü değiştirir: düz kenarı bir noktadan geçen yaya, yayı düz kenara çevirir; nesnenin kimliği korunur.

Komut: `core.edge_kind` — `KENARTÜRÜ`

```python
cad.edge_kind(
    object: list[int],
    edge: int,
    at: Coord,
    kind: str,
    point: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Kenarı değişecek nesnenin kimliği [kalıcı nesne anahtarı] |
| `edge` | `int` | `kenar` | Değişecek kenarın sırası; ilk kenar 1'dir. Verilmezse yer |
| `at` | `Coord` | `yer` | Kenarı gösteren nokta: kenar verilmezse en yakın kenar, nesne de verilmezse altındaki nesne [mm, Sağa (Y) önce] |
| `kind` | `str` | `tur` | yay: düz kenar yay olur; duz: yay düz olur. Verilmezse kenarın öbür türü |
| `point` | `Coord` | `nokta` | tur=yay için yayın geçeceği nokta [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/edge_kind.md)

### `cad.to_area`

Uç uca değen çizgileri tek bir kapalı alana çevirir.

Komut: `core.to_area` — `ALANAÇEVİR`

```python
cad.to_area(
    objects: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Birleştirilecek çizgilerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı] |

[Komut sayfası](../komutlar/to_area.md)

### `cad.boundary`

İçine tıklanan kapalı bölgenin sınırını yeni bir alan olarak çıkarır; içerideki adalar delik olur, açık uçlar gösterilir.

Komut: `core.boundary` — `SINIR`

```python
cad.boundary(
    point: Coord,
    islands: bool,
    gap: int,
    objects: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `point` | `Coord` | `nokta` | Sınırı çıkarılacak bölgenin içindeki nokta; yoksa sorulur [mm, Sağa (Y) önce] |
| `islands` | `bool` | `ada` | İçerideki kapalı çizgiler delik olsun mu; varsayılan evet |
| `gap` | `int` | `bosluk` | Bu genişliğe kadar açık uçları köprüle, milimetre; varsayılan 0: hiçbir boşluk kendiliğinden kapanmaz [mm] |
| `objects` | `list[int]` | `nesneler` | Sınır sayılacak nesneler; yoksa görünen her çizgi [kalıcı nesne anahtarı] |

[Komut sayfası](../komutlar/boundary.md)

### `cad.cleanup`

Yinelenen, boş ve tekrarlanan köşeli nesneleri bulur; istenirse tek adımda onarır ve değişen alanları önce/sonra raporlar.

Komut: `core.cleanup` — `TEMİZLE`

```python
cad.cleanup(
    objects: list[int],
    action: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Bakılacak nesneler; yoksa seçim, o da boşsa bütün çizim [kalıcı nesne anahtarı] |
| `action` | `str` | `islem` | bul: bulur, seçer ve işaretler, hiçbir şeyi değiştirmez · onar: yinelenenleri ve boş nesneleri siler, tekrarlanan köşeleri çıkarır |

[Komut sayfası](../komutlar/cleanup.md)

### `cad.move`

Seçilen nesneleri iki nokta arasındaki kadar taşır.

Komut: `core.move` — `TAŞI`

```python
cad.move(
    objects: list[int],
    start: Coord,
    end: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Taşınacak nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `start` | `Coord` | `baslangic` | Taşımanın başlangıç noktası [mm, Sağa (Y) önce] |
| `end` | `Coord` | `bitis` | Taşımanın bitiş noktası [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/move.md)

### `cad.copy`

Seçilen nesnelerin kopyasını verilen her noktaya, başlangıçtan o noktaya kadar öteleyerek koyar.

Komut: `core.copy` — `KOPYALA`

```python
cad.copy(
    objects: list[int],
    start: Coord,
    end: Coords,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Kopyalanacak nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `start` | `Coord` | `baslangic` | Kopyalamanın başlangıç noktası [mm, Sağa (Y) önce] |
| `end` | `Coords` | `bitis` | Kopyaların geleceği noktalar; her nokta bir kopya [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/copy.md)

### `cad.array`

Seçilen nesneleri satır/sütun, bir merkez etrafında ya da bir yol boyunca çoğaltır.

Komut: `core.array` — `DİZİ`

```python
cad.array(
    objects: list[int],
    mode: str,
    rows: int,
    columns: int,
    row_spacing: float,
    column_spacing: float,
    center: Coord,
    count: int,
    angle: float,
    path: list[int],
    path_point: Coord,
    spacing: float,
    follow: bool,
    base_point: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Dizilecek nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `mode` | `str` | `mod` | KUTUPSAL için kutupsal dizi, YOL için yol boyunca dizi; verilmezse satır/sütun dizisi |
| `rows` | `int` | `satir` | Satır sayısı (dikdörtgen dizi) |
| `columns` | `int` | `sutun` | Sütun sayısı (dikdörtgen dizi) |
| `row_spacing` | `float` | `satir_aralik` | Satır aralığı, metre; kuzeye artı |
| `column_spacing` | `float` | `sutun_aralik` | Sütun aralığı, metre; doğuya artı |
| `center` | `Coord` | `merkez` | Dizinin merkezi (kutupsal dizi) [mm, Sağa (Y) önce] |
| `count` | `int` | `sayi` | Toplam kopya sayısı, özgün dahil (kutupsal ve yol boyunca dizi) |
| `angle` | `float` | `aci` | Süpürülecek toplam açı, derece; verilmezse tam tur |
| `path` | `list[int]` | `yol` | mod=yol için dizinin izleyeceği yol: çizgi, yay, daire ya da yaylı çoklu çizgi [kalıcı nesne anahtarı] |
| `path_point` | `Coord` | `yol_nokta` | Yolu gösteren nokta; yol verilmişse sorulmaz [mm, Sağa (Y) önce] |
| `spacing` | `float` | `aralik` | mod=yol için kopyalar arası uzaklık, metre; verilmezse sayi |
| `follow` | `bool` | `hizala` | mod=yol için kopyalar yolun doğrultusuna döndürülsün mü; varsayılan evet |
| `base_point` | `Coord` | `taban` | mod=yol için nesnelerin yola taşınan taban noktası; varsayılan yolun başı [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/array.md)

### `cad.combine`

Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek çizgi yapar.

Komut: `core.combine` — `BİRLEŞTİR`

```python
cad.combine(
    objects: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Birleştirilecek alanlar ya da çizgiler; yoksa etkin seçim [kalıcı nesne anahtarı] |

[Komut sayfası](../komutlar/combine.md)

### `cad.split`

Nesneleri bir kesme çizgisiyle, üstündeki noktalardan, kesişimlerinden, baştan bir uzaklıktan ya da eşit parçalara böler; yaylar yay kalır.

Komut: `core.split` — `BÖL`

```python
cad.split(
    object: list[int],
    points: Coords,
    point: Coord,
    method: str,
    distance: float,
    count: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Kesilecek nesneler; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `points` | `Coords` | `noktalar` | cizgi: kesme çizgisinin iki noktası · nokta: nesnenin üstündeki bölme noktaları [mm, Sağa (Y) önce] |
| `point` | `Coord` | `nokta` | Bölme noktası (tek çizgi; eski biçim) [mm, Sağa (Y) önce] |
| `method` | `str` | `yontem` | cizgi: çizilen kesme çizgisinden · nokta: nesnenin üstündeki noktalardan · kesisim: seçilenlerin birbirini kestiği yerlerden · mesafe: baştan verilen uzaklıktan · esit: eşit parçalara |
| `distance` | `float` | `mesafe` | mesafe: baştan uzaklık (m) [m] |
| `count` | `int` | `sayi` | esit: kaç eşit parça |

[Komut sayfası](../komutlar/split.md)

### `cad.trim`

Tıklanan parçayı kesme sınırları arasından atar; çizgide, yayda ve dairede çalışır.

Komut: `core.trim` — `BUDA`

```python
cad.trim(
    object: list[int],
    boundary: list[int],
    every_edge: bool,
    point: Coords,
    method: str,
    fence: Coords,
    keep: bool,
    carry_edges: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Budanan nesneler, tıklama sırasıyla; yoksa her tıklamanın altındaki nesne [kalıcı nesne anahtarı] |
| `boundary` | `list[int]` | `sinir` | Kesme sınırları; yoksa seçili nesneler, o da yoksa tıklanan nesnenin yakınındaki her nesne [kalıcı nesne anahtarı] |
| `every_edge` | `bool` | `hepsi` | Tıklanan nesnenin yakınındaki her nesne sınırdır (seçim ve sinir yokken öntanımlı) |
| `point` | `Coords` | `nokta` | Atılacak her parçanın üzerinde bir nokta, sırayla [mm, Sağa (Y) önce] |
| `method` | `str` | `yontem` | Parçalar nasıl gösterilir: tek tek tıklayarak (öntanımlı) ya da çizilen bir çitle |
| `fence` | `Coords` | `cit` | Çitin köşeleri; çitin geçtiği her parça budanır [mm, Sağa (Y) önce] |
| `keep` | `bool` | `tut` | Gösterilen parça kalır; iki yanındaki kesimlerin dışında kalan gider |
| `carry_edges` | `bool` | `uzanti` | Sınırlar kendi yolunda uzatılmış sayılır; nesneye yetişmeyen bir sınır da keser |

[Komut sayfası](../komutlar/trim.md)

### `cad.extend`

Tıklanan ucu sınıra ulaşana kadar uzatır: çizginin ucunu doğrultusunda, yayınkini çemberi boyunca.

Komut: `core.extend` — `UZAT`

```python
cad.extend(
    object: list[int],
    boundary: list[int],
    every_edge: bool,
    point: Coords,
    method: str,
    fence: Coords,
    carry_edges: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Uzatılan nesneler, tıklama sırasıyla; yoksa her tıklamanın altındaki nesne [kalıcı nesne anahtarı] |
| `boundary` | `list[int]` | `sinir` | Uzatılacak sınırlar; yoksa seçili nesneler, o da yoksa tıklanan nesnenin yakınındaki her nesne [kalıcı nesne anahtarı] |
| `every_edge` | `bool` | `hepsi` | Tıklanan nesnenin yakınındaki her nesne sınırdır (seçim ve sinir yokken öntanımlı) |
| `point` | `Coords` | `nokta` | Uzatılacak her ucun yakınında bir nokta, sırayla [mm, Sağa (Y) önce] |
| `method` | `str` | `yontem` | Uçlar nasıl gösterilir: tek tek tıklayarak (öntanımlı) ya da çizilen bir çitle |
| `fence` | `Coords` | `cit` | Çitin köşeleri; çitin yanından geçtiği her uç uzatılır [mm, Sağa (Y) önce] |
| `carry_edges` | `bool` | `uzanti` | Sınırlar kendi yolunda uzatılmış sayılır; ucun doğrultusuna yetişmeyen bir sınıra da ulaşılır |

[Komut sayfası](../komutlar/extend.md)

### `cad.chamfer`

Bir köşeyi ya da iki çizgi arasındaki köşeyi düz bir kenarla keser (pah kırar).

Komut: `core.chamfer` — `PAH`

```python
cad.chamfer(
    object: list[int],
    point: Coord,
    distance: float,
    second_point: Coord,
    second_distance: float,
    trim: bool,
    every_corner: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Köşesi kesilecek nesne; iki nesne verilirse aralarındaki köşe; hepsi=evet ile bir ya da daha çok nesne [kalıcı nesne anahtarı] |
| `point` | `Coord` | `nokta` | Tek nesnede işlem yapılacak köşe; iki nesnede birincinin kalacak parçası; hepsi=evet ise verilmez [mm, Sağa (Y) önce] |
| `distance` | `float` | `mesafe` | Köşeden her iki kenar boyunca kesilecek mesafe, metre |
| `second_point` | `Coord` | `ikinci_nokta` | İki nesnede ikincinin kalacak parçası [mm, Sağa (Y) önce] |
| `second_distance` | `float` | `ikinci_mesafe` | İki çizgi arasında ikinci çizgi boyunca kesilecek mesafe, metre; verilmezse mesafe [m] |
| `trim` | `bool` | `budama` | İki nesnede nesneler köşeye kadar kısaltılıp uzatılsın mı; varsayılan evet |
| `every_corner` | `bool` | `hepsi` | Verilen nesnelerin bütün köşeleri aynı değerle; sığmayan köşe atlanır |

[Komut sayfası](../komutlar/chamfer.md)

### `cad.fillet`

Bir köşeyi ya da iki nesne (çizgi, yay) arasındaki köşeyi verilen yarıçapta yayla yuvarlatır; 0 yarıçap keskin köşe kurar.

Komut: `core.fillet` — `YUVARLA`

```python
cad.fillet(
    object: list[int],
    point: Coord,
    radius: float,
    second_point: Coord,
    trim: bool,
    every_corner: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `object` | `list[int]` | `nesne` | Köşesi yuvarlatılacak nesne; iki nesne verilirse aralarındaki köşe; hepsi=evet ile bir ya da daha çok nesne [kalıcı nesne anahtarı] |
| `point` | `Coord` | `nokta` | Tek nesnede işlem yapılacak köşe; iki nesnede birincinin kalacak parçası; hepsi=evet ise verilmez [mm, Sağa (Y) önce] |
| `radius` | `float` | `yaricap` | Yuvarlatma yarıçapı, metre; iki nesnede 0 keskin köşe |
| `second_point` | `Coord` | `ikinci_nokta` | İki nesnede ikincinin kalacak parçası [mm, Sağa (Y) önce] |
| `trim` | `bool` | `budama` | İki nesnede nesneler teğet noktalarına kadar kısaltılıp uzatılsın mı; varsayılan evet |
| `every_corner` | `bool` | `hepsi` | Verilen nesnelerin bütün köşeleri aynı değerle; sığmayan köşe atlanır |

[Komut sayfası](../komutlar/fillet.md)

### `cad.set_layer`

Seçilen nesneleri başka bir katmana taşır.

Komut: `core.set_layer` — `KATMANAT`

```python
cad.set_layer(
    objects: list[int],
    layer: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Taşınacak nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `layer` | `str` | `katman` | Hedef katmanın adı; yoksa oluşturulur |

[Komut sayfası](../komutlar/set_layer.md)

### `cad.match_style`

Bir nesnenin stilini seçilen nesnelere uygular.

Komut: `core.match_style` — `STİLKOPYALA`

```python
cad.match_style(
    source: list[int],
    objects: list[int],
    point: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `source` | `list[int]` | `kaynak` | Stili kopyalanacak nesnenin kimliği; yoksa tıklanan nesne [kalıcı nesne anahtarı] |
| `objects` | `list[int]` | `nesneler` | Stili alacak nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `point` | `Coord` | `nokta` | Kaynak nesnenin üzerinde bir nokta; yalnız kaynak verilmediğinde [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/match_style.md)

### `cad.colour`

Seçili nesnelerin çizgi ve dolgu rengini değiştirir ya da katmanın rengine döndürür.

Komut: `core.colour` — `RENK`

```python
cad.colour(
    objects: list[int],
    color: str,
    fill: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Rengi değişecek nesneler; verilmezse etkin seçim, o da boşsa sorulur [kalıcı nesne anahtarı] |
| `color` | `str` | `renk` | Çizgi rengi: #RRGGBB (ya da saydamlıkla #AARRGGBB) veya katman |
| `fill` | `str` | `dolgu` | Dolgu rengi: #RRGGBB, yok (dolgusuz) ya da katman |

[Komut sayfası](../komutlar/colour.md)

### `cad.rotate`

Seçilen nesneleri bir merkez etrafında döndürür; açı verilir, gösterilir ya da bir referans doğrultudan bulunur.

Komut: `core.rotate` — `DÖNDÜR`

```python
cad.rotate(
    objects: list[int],
    center: Coord,
    angle: float,
    angle_point: Coord,
    method: str,
    reference: float,
    reference_point: Coords,
    copy: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Döndürülecek nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `center` | `Coord` | `merkez` | Döndürme merkezi [mm, Sağa (Y) önce] |
| `angle` | `float` | `aci` | Dönme açısı, derece; artı yön saat yönünün tersi. Verilmezse yeni doğrultu gösterilir |
| `angle_point` | `Coord` | `aci_nokta` | Dönme açısının gösterildiği nokta; aci verilmişse sorulmaz [mm, Sağa (Y) önce] |
| `method` | `str` | `yontem` | referans: bir doğrultu yenisine döndürülür; referans doğrultu iki noktayla gösterilir |
| `reference` | `float` | `referans` | Referans doğrultunun açısı, derece; aci onun yeni açısıdır |
| `reference_point` | `Coords` | `referans_nokta` | Referans doğrultuyu gösteren iki nokta [mm, Sağa (Y) önce] |
| `copy` | `bool` | `kopya` | evet: nesnelerin kendisi değil kopyası dönüştürülür; özgün yerinde kalır |

[Komut sayfası](../komutlar/rotate.md)

### `cad.scale`

Seçilen nesneleri bir merkeze göre büyütür ya da küçültür; iki çarpanla eşit olmayan ölçek, referans uzunlukla ölçek.

Komut: `core.scale` — `ÖLÇEKLE`

```python
cad.scale(
    objects: list[int],
    center: Coord,
    factor: float,
    factor_point: Coord,
    factor_y: float,
    method: str,
    reference: float,
    new_length: float,
    reference_point: Coords,
    copy: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Ölçeklenecek nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `center` | `Coord` | `merkez` | Ölçekleme merkezi; bu nokta yerinde kalır [mm, Sağa (Y) önce] |
| `factor` | `float` | `carpan` | Ölçek çarpanı; sıfırdan büyük. Verilmezse merkezden uzaklık gösterilir |
| `factor_point` | `Coord` | `carpan_nokta` | Çarpanın gösterildiği nokta; carpan verilmişse sorulmaz [mm, Sağa (Y) önce] |
| `factor_y` | `float` | `carpan_y` | Yukarı yöndeki çarpan; verilirse carpan yalnız sağa yöndeki çarpandır ve daire elips olur |
| `method` | `str` | `yontem` | referans: bir uzunluk yenisine ölçeklenir; referans uzunluk iki noktayla gösterilir |
| `reference` | `float` | `referans` | Referans uzunluk, metre; yeni onun olacağı uzunluktur |
| `new_length` | `float` | `yeni` | Referans uzunluğun yeni değeri, metre |
| `reference_point` | `Coords` | `referans_nokta` | Referans uzunluğu gösteren iki nokta [mm, Sağa (Y) önce] |
| `copy` | `bool` | `kopya` | evet: nesnelerin kendisi değil kopyası dönüştürülür; özgün yerinde kalır |

[Komut sayfası](../komutlar/scale.md)

### `cad.mirror`

Seçilen nesneleri iki noktadan geçen eksende aynalar.

Komut: `core.mirror` — `AYNALA`

```python
cad.mirror(
    objects: list[int],
    start: Coord,
    end: Coord,
    copy: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Aynalanacak nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `start` | `Coord` | `baslangic` | Ayna ekseninin ilk noktası [mm, Sağa (Y) önce] |
| `end` | `Coord` | `bitis` | Ayna ekseninin ikinci noktası [mm, Sağa (Y) önce] |
| `copy` | `bool` | `kopya` | evet: nesnelerin kendisi değil kopyası dönüştürülür; özgün yerinde kalır |

[Komut sayfası](../komutlar/mirror.md)

### `cad.measure`

Noktalar arasındaki mesafeyi, koordinat farkını ve açıyı yazar; ikiden fazla nokta kenarları ve toplam uzunluğu verir.

Komut: `core.measure` — `ÖLÇ`

```python
cad.measure(
    start: Coord,
    end: Coord,
    more: Coords,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `start` | `Coord` | `baslangic` | Ölçümün ilk noktası [mm, Sağa (Y) önce] |
| `end` | `Coord` | `bitis` | Ölçümün ikinci noktası [mm, Sağa (Y) önce] |
| `more` | `Coords` | `devam` | Sonraki noktalar: her biri bir kenar daha ekler, toplam da yazılır [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/measure.md)

### `cad.measure_area`

Seçilen nesnelerin ya da köşeleri gösterilen bir alanın alanını ve çevresini yazar.

Komut: `core.measure_area` — `ALANÖLÇ`

```python
cad.measure_area(
    objects: list[int],
    method: str,
    points: Coords,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Ölçülecek nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `method` | `str` | `yontem` | nesne: seçilen nesnelerin alanı (öntanımlı); nokta: köşeleri gösterilen alan |
| `points` | `Coords` | `noktalar` | yontem=nokta için alanın köşeleri; verilirse yöntem kendiliğinden nokta olur [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/measure_area.md)

### `cad.coordinate`

Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar.

Komut: `core.coordinate` — `KOORDİNAT`

```python
cad.coordinate(
    point: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `point` | `Coord` | `nokta` | Okunacak nokta [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/coordinate.md)

### `cad.pan`

Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır.

Komut: `core.pan` — `KAYDIR`

```python
cad.pan(
    start: Coord,
    end: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `start` | `Coord` | `baslangic` | Kaydırmanın tutulacağı nokta [mm, Sağa (Y) önce] |
| `end` | `Coord` | `bitis` | O noktanın taşınacağı yer [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/pan.md)

### `cad.offset`

Seçili nesnelerin verilen mesafede, gösterilen tarafta paralelini çizer: açık çizgiye tek yanda çizgi, alana delikleriyle alan, daireye daire.

Komut: `core.offset` — `OFSET`

```python
cad.offset(
    objects: list[int],
    distance: int,
    corner: str,
    side: str,
    through: Coord,
    source: str,
    properties: str,
    attributes: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Ofseti alınacak nesneler; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `distance` | `int` | `mesafe` | Paralel mesafesi, milimetre. Taraf verilmez ve gösterilmezse işaret anlam taşır: kapalı şekilde artı dışarı, eksi içeri |
| `corner` | `str` | `kose` | KÖŞE | YUVARLAK | PAH — dış köşenin biçimi |
| `side` | `str` | `taraf` | Paralelin tarafı: açık çizgide sol ya da sag (çizim yönüne göre), kapalı şekilde dis ya da ic, iki her iki yan |
| `through` | `Coord` | `nokta` | Tarafı gösteren nokta: her nesnenin paraleli bu noktanın olduğu yana düşer [mm, Sağa (Y) önce] |
| `source` | `str` | `kaynak` | Kaynak nesne: koru (öntanımlı) ya da paralel çizilince sil |
| `properties` | `str` | `ozellik` | Paralelin katmanı ve stili: kaynak nesneninki (öntanımlı) ya da etkin katman |
| `attributes` | `str` | `oznitelik` | Kaynağın öznitelik değerleri: paralele aktar (öntanımlı) ya da aktarma — parselin içine çizilen çekme hattı gibi kaynağın kendisi olmayan bir çizgi için |

[Komut sayfası](../komutlar/offset.md)

### `cad.sector`

Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir.

Komut: `core.sector` — `DİLİM`

```python
cad.sector(
    center: Coord,
    start: Coord,
    end: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `center` | `Coord` | `merkez` | Dilimin merkezi [mm, Sağa (Y) önce] |
| `start` | `Coord` | `baslangic` | İlk kenarın ucu; yarıçapı bu belirler [mm, Sağa (Y) önce] |
| `end` | `Coord` | `bitis` | İkinci kenarın yönü; süpürme saat yönünün tersinedir [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/sector.md)

### `cad.annulus`

Merkez, iç ve dış yarıçaptan delikli halka çizer.

Komut: `core.annulus` — `HALKA`

```python
cad.annulus(
    center: Coord,
    inner: Coord,
    outer: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `center` | `Coord` | `merkez` | Halkanın merkezi [mm, Sağa (Y) önce] |
| `inner` | `Coord` | `ic` | İç çember üzerinde bir nokta [mm, Sağa (Y) önce] |
| `outer` | `Coord` | `dis` | Dış çember üzerinde bir nokta [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/annulus.md)

### `cad.ellipse_draw`

Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir.

Komut: `core.ellipse_draw` — `ELİPS`

```python
cad.ellipse_draw(
    center: Coord,
    first: Coord,
    second: Coord,
    method: str,
    second_end: Coord,
    start: float,
    end: float,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `center` | `Coord` | `merkez` | Elipsin merkezi [mm, Sağa (Y) önce] |
| `first` | `Coord` | `birinci` | merkez: birinci eksenin ucu · eksen: birinci eksenin bir ucu [mm, Sağa (Y) önce] |
| `second` | `Coord` | `ikinci` | İkinci eksenin uzaklığı; eksene dik ölçülür [mm, Sağa (Y) önce] |
| `method` | `str` | `yontem` | merkez: merkez + eksen ucu · eksen: eksenin iki ucu |
| `second_end` | `Coord` | `ikinci_uc` | eksen: birinci eksenin öteki ucu [mm, Sağa (Y) önce] |
| `start` | `float` | `baslangic` | Kısmi elips: başlangıç açısı, derece, birinci eksenden saat yönünün tersine |
| `end` | `float` | `bitis` | Kısmi elips: bitiş açısı, derece; baslangic ile birlikte |

[Komut sayfası](../komutlar/ellipse_draw.md)

### `cad.spline`

Kontrol noktalarından NURBS eğrisi (spline) çizer.

Komut: `core.spline` — `SPLINE`

```python
cad.spline(
    points: Coords,
    degree: int,
    closed: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `points` | `Coords` | `noktalar` | Kontrol noktaları [mm, Sağa (Y) önce] |
| `degree` | `int` | `derece` | Eğrinin derecesi, 1–15; varsayılan 3 |
| `closed` | `bool` | `kapali` | Son noktadan ilkine kapansın mı; varsayılan hayır |

[Komut sayfası](../komutlar/spline.md)

### `cad.hatch`

Kapalı nesnelerin ya da verilen köşelerin içini katalogdaki bir desenle tarar.

Komut: `core.hatch` — `TARAMA`

```python
cad.hatch(
    points: Coords,
    objects: list[int],
    pattern: str,
    angle: float,
    scale: float,
    catalog: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `points` | `Coords` | `noktalar` | Sınır köşeleri, nesne seçmek yerine; en az üç nokta [mm, Sağa (Y) önce] |
| `objects` | `list[int]` | `nesneler` | Sınırı verecek kapalı nesneler; yoksa etkin seçim ya da noktalar= [kalıcı nesne anahtarı] |
| `pattern` | `str` | `desen` | Katalogdaki desen adı: SOLID, ANSI31, NET…; varsayılan SOLID |
| `angle` | `float` | `aci` | Desenin dönme açısı, derece; varsayılan 0 |
| `scale` | `float` | `olcek` | Desen ölçeği; varsayılan pafta ölçeğinin paydası (AYAR plan_ölçeği) |
| `catalog` | `str` | `katalog` | Desen kataloğu dosyası; varsayılan TERCİH desen_kataloğu |

[Komut sayfası](../komutlar/hatch.md)

### `cad.block`

Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar.

Komut: `core.block` — `BLOK`

```python
cad.block(
    name: str,
    base: Coord,
    objects: list[int],
    note: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `name` | `str` | `ad` | Bloğun adı; Türkçe katlanmış hâliyle benzersiz |
| `base` | `Coord` | `taban` | Taban noktası: referansların yerleştirildiği nokta [mm, Sağa (Y) önce] |
| `objects` | `list[int]` | `nesneler` | Bloğa girecek nesneler; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `note` | `str` | `aciklama` | Serbest açıklama |

[Komut sayfası](../komutlar/block.md)

### `cad.insert`

Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir.

Komut: `core.insert` — `BLOKEKLE`

```python
cad.insert(
    name: str,
    point: Coord,
    scale: float,
    scale_y: float,
    angle: float,
    columns: int,
    rows: int,
    column_spacing: int,
    row_spacing: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `name` | `str` | `ad` | Yerleştirilecek bloğun adı |
| `point` | `Coord` | `nokta` | Ekleme noktası [mm, Sağa (Y) önce] |
| `scale` | `float` | `olcek` | Ölçek; eksi değer x'te aynalar; varsayılan 1 |
| `scale_y` | `float` | `olcek_y` | Y ölçeği, farklıysa; varsayılan olcek |
| `angle` | `float` | `aci` | Dönme açısı, derece; varsayılan 0 |
| `columns` | `int` | `sutun` | Dizi sütun sayısı; varsayılan 1 |
| `rows` | `int` | `satir` | Dizi satır sayısı; varsayılan 1 |
| `column_spacing` | `int` | `sutun_aralik` | Sütunlar arası, milimetre, döndürülmüş eksende |
| `row_spacing` | `int` | `satir_aralik` | Satırlar arası, milimetre, döndürülmüş eksende |

[Komut sayfası](../komutlar/insert.md)

### `cad.dimension`

İki nokta arasını, bir yarıçapı, çapı ya da açıyı ölçüp yazısı ve oklarıyla çizer.

Komut: `core.dimension` — `ÖLÇÜ`

```python
cad.dimension(
    first: Coord,
    second: Coord,
    position: Coord,
    type: str,
    apex: Coord,
    end: Coord,
    style: str,
    text: str,
    catalog: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `first` | `Coord` | `birinci` | Birinci nokta; açısal ölçüde birinci kolun ucu [mm, Sağa (Y) önce] |
| `second` | `Coord` | `ikinci` | İkinci nokta; açısal ölçüde ikinci kolun ucu [mm, Sağa (Y) önce] |
| `position` | `Coord` | `konum` | Ölçü çizgisinin yeri; açısal ölçüde yayın geçtiği nokta [mm, Sağa (Y) önce] |
| `type` | `str` | `tur` | hizali (varsayılan), dogrusal, yaricap, cap, acisal, koordinat, yay |
| `apex` | `Coord` | `tepe` | Açısal ölçünün tepe noktası [mm, Sağa (Y) önce] |
| `end` | `Coord` | `bitis` | Yay uzunluğu ölçüsünün bitiş noktası [mm, Sağa (Y) önce] |
| `style` | `str` | `stil` | Katalogdaki ölçü stili: ISO-25 (varsayılan), STANDARD, MIMARI |
| `text` | `str` | `metin` | Ölçülen değer yerine yazılacak metin |
| `catalog` | `str` | `katalog` | Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri |

[Komut sayfası](../komutlar/dimension.md)

### `cad.leader`

Bir noktayı gösteren oklu çizgi çizer, istenirse yanına yazı koyar.

Komut: `core.leader` — `LİDER`

```python
cad.leader(
    points: Coords,
    text: str,
    style: str,
    catalog: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `points` | `Coords` | `noktalar` | Okun ucundan yazının yanına köşeler [mm, Sağa (Y) önce] |
| `text` | `str` | `metin` | Son köşenin yanına yazılacak metin |
| `style` | `str` | `stil` | Ok ve yazı boyunu veren ölçü stili; varsayılan ISO-25 |
| `catalog` | `str` | `katalog` | Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri |

[Komut sayfası](../komutlar/leader.md)

### `cad.points`

Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod).

Komut: `core.points` — `NOKTALAR`

```python
cad.points(
    file: str,
    mode: str,
    objects: list[int],
    axis_order: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `file` | `str` | `dosya` | Nokta listesi dosyasının yolu |
| `mode` | `str` | `yon` | oku (varsayılan) | yaz |
| `objects` | `list[int]` | `nesneler` | yon=yaz ile: köşeleri yazılacak nesneler; verilmezse çizimdeki noktalar [kalıcı nesne anahtarı] |
| `axis_order` | `str` | `eksen` | Sütun sırası: YX (varsayılan, Türkiye'de olağan) | XY |

[Komut sayfası](../komutlar/points.md)

### `cad.guide`

Cetvel kılavuzu ve açılı kılavuz ekler, listeler ve siler.

Komut: `core.guide` — `KILAVUZ`

```python
cad.guide(
    direction: str,
    value: int,
    point: Coord,
    type: str,
    delete: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `direction` | `str` | `yon` | yatay | düşey | bir açı (45, 45g, 30d); yoksa kılavuzlar listelenir |
| `value` | `int` | `deger` | Kılavuzun koordinatı, milimetre — yatayda yukarı, düşeyde sağa |
| `point` | `Coord` | `nokta` | Açılı kılavuzun geçtiği nokta; yalnız `yon` bir açıysa [mm, Sağa (Y) önce] |
| `type` | `str` | `tur` | doğru: iki yöne sonsuz · ışın: noktadan ileriye |
| `delete` | `bool` | `sil` | Verilen yerdeki kılavuzu siler |

[Komut sayfası](../komutlar/guide.md)

### `cad.attribute`

Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar.

Komut: `core.attribute` — `ÖZNİTELİK`

```python
cad.attribute(
    name: str,
    object: int,
    value: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `name` | `str` | `ad` | Öznitelik kimliği; yoksa tanımlı sütunlar listelenir |
| `object` | `int` | `nesne` | Nesnenin kalıcı kimliği |
| `value` | `str` | `deger` | Yeni değer; yoksa yalnızca okur. 'yok' hücreyi boşaltır |

[Komut sayfası](../komutlar/attribute.md)

### `cad.column`

Öznitelik sütunu tanımlar, düzenler, siler; argümansız çağrılınca listeler.

Komut: `core.column` — `SÜTUN`

```python
cad.column(
    id: str,
    type: str,
    name: str,
    note: str,
    required: bool,
    catalog: str,
    digits: int,
    layer: str,
    delete: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `id` | `str` | `kimlik` | Sütun kimliği; yoksa tanımlı sütunlar listelenir |
| `type` | `str` | `tur` | tam_sayi, ondalik, uzunluk, evet_hayir, metin, tarih, kod |
| `name` | `str` | `ad` | Panelde görünen Türkçe ad |
| `note` | `str` | `aciklama` | Tek satırlık açıklama |
| `required` | `bool` | `zorunlu` | Her satır bir değer taşımalı mı |
| `catalog` | `str` | `katalog` | Yalnız 'kod' türü için: katalog kimliği |
| `digits` | `int` | `basamak` | Yalnız 'ondalik' için: noktadan sonraki basamak sayısı |
| `layer` | `str` | `katman` | Sütunu yalnız bu katmana tanımlar; yoksa proje geneli |
| `delete` | `bool` | `sil` | Sütunu ve içindeki bütün değerleri siler |

[Komut sayfası](../komutlar/column.md)

### `cad.erase`

Seçilen nesneleri siler.

Komut: `core.erase` — `SİL`

```python
cad.erase(
    objects: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Silinecek nesnelerin kimlikleri; yoksa etkin seçim [kalıcı nesne anahtarı] |

[Komut sayfası](../komutlar/erase.md)

### `cad.select`

Nesneleri seçer: tümü, kimlikle, katman, pencere, kesen kutu, çokgen, çit, önceki seçim, son nesne ya da tek nokta.

Komut: `core.select` — `SEÇ`

```python
cad.select(
    mode: str,
    points: Coords,
    type: str,
    objects: list[int],
    layer: str,
    action: str,
    tolerance: float,
    order: float,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `mode` | `str` | `mod` | TÜMÜ | TEMİZLE | NESNE | KATMAN | PENCERE | KESEN | KUTU | NOKTA | ÇOKGEN | ÇOKGENKESEN | ÇİT | ÖNCEKİ | SON |
| `points` | `Coords` | `noktalar` | Kutu köşeleri (iki nokta), çokgen/çit köşeleri ya da tek tıklama noktası [mm, Sağa (Y) önce] |
| `type` | `str` | `tur` | Yalnız bu türdeki nesneler: ÇOKLUÇİZGİ, DAİRE, YAY, NOKTA, ELİPS… |
| `objects` | `list[int]` | `nesneler` | NESNE modunda nesne kimlikleri [kalıcı nesne anahtarı] |
| `layer` | `str` | `katman` | KATMAN modunda katman adı |
| `action` | `str` | `islem` | DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE |
| `tolerance` | `float` | `tolerans` | NOKTA modunda arama yarıçapı, metre; yoksa seçim toleransı |
| `order` | `float` | `sira` | NOKTA modunda kaçıncı nesne: 1 en yakını, 2 altındaki |

[Komut sayfası](../komutlar/select.md)

### `cad.label`

Katmandaki nesneleri özniteliklerinden okuyarak etiketler.

Komut: `core.label` — `ETİKET`

```python
cad.label(
    layer: str,
    format: str,
    target_layer: str,
    height: int,
    offset: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `layer` | `str` | `katman` | Etiketlenecek katmanın adı |
| `format` | `str` | `bicim` | Etiket biçimi; {sutun} o sütunun değeriyle değişir, \n satır kırar. Sembol alan bildiriyorsa gerekmez |
| `target_layer` | `str` | `hedef` | Etiketlerin yazılacağı katman; yoksa '<katman> ETİKET' |
| `height` | `int` | `yukseklik` | Yazı yüksekliği, zemin milimetresi |
| `offset` | `int` | `kaydirma` | Nesnenin ortasından dikey kaydırma, zemin milimetresi; artı yukarı |

[Komut sayfası](../komutlar/label.md)

### `cad.layer`

Katman oluşturur, aktif yapar ve özelliklerini değiştirir.

Komut: `core.layer` — `KATMAN`

```python
cad.layer(
    name: str,
    group: str,
    visible: bool,
    locked: bool,
    color: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `name` | `str` | `ad` | Katman adı; yoksa oluşturulur ve aktif yapılır |
| `group` | `str` | `grup` | Katman ağacındaki yer, düzeyler '>' ile ayrılır; boş = kök |
| `visible` | `bool` | `gorunur` | Katmanın görünürlüğü |
| `locked` | `bool` | `kilitli` | Katmanın kilit durumu |
| `color` | `int` | `renk` | Çizim rengi, 0xAARRGGBB |

[Komut sayfası](../komutlar/layer.md)

### `cad.layer_visibility`

Katmanların görünürlüğünü toptan değiştirir: bir katmanı gösterir ya da gizler, yalnız onu bırakır, hepsini gösterir veya görünürlüğü ters çevirir.

Komut: `core.layer_visibility` — `KATMANGÖRÜNÜM`

```python
cad.layer_visibility(
    action: str,
    layer: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `action` | `str` | `islem` | goster, gizle, yalniz (yalnız bu katman), tumu (hepsini göster) ya da tersine |
| `layer` | `str` | `katman` | Katman adı; goster, gizle ve yalniz için gerekir, tersine için isteğe bağlı (verilmezse bütün katmanlar), tumu ile verilemez |

[Komut sayfası](../komutlar/layer_visibility.md)

### `cad.layout`

Çizimin çıktı yerleşimlerini yönetir: yeni yerleşim açar, siler, adlandırır ve kâğıdını değiştirir. Yerleşim çizimle birlikte kaydedilir ve geri alınabilir.

Komut: `core.layout` — `ÇIKTIYERLEŞİMİ`

```python
cad.layout(
    action: str,
    name: str,
    new_name: str,
    paper: str,
    width: int,
    height: int,
    orientation: str,
    margin: int,
    dpi: int,
    page: int,
    new_order: int,
    layer: str,
    sort_by: str,
    group_by: str,
    margin_percent: int,
    single_file: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `action` | `str` | `islem` | Ne yapılacağı |
| `name` | `str` | `ad` | Yerleşimin adı; listele dışında gerekir |
| `new_name` | `str` | `yeni_ad` | islem=ad için yeni yerleşim adı |
| `paper` | `str` | `kagit` | A5, A4, A3, A2, A1, A0 ya da ozel (varsayılan A4) |
| `width` | `int` | `genislik` | ozel kâğıt için sayfa genişliği [kâğıt mm] |
| `height` | `int` | `yukseklik` | ozel kâğıt için sayfa yüksekliği [kâğıt mm] |
| `orientation` | `str` | `yon` | Sayfa yönü (varsayılan dikey) |
| `margin` | `int` | `kenar` | Kenar boşluğu (varsayılan 10) [kâğıt mm] |
| `dpi` | `int` | `dpi` | Çıktı çözünürlüğü (varsayılan 300) |
| `page` | `int` | `sayfa` | Hangi sayfa (1'den başlar). sayfa işleminde verilmezse bütün sayfalar değişir |
| `new_order` | `int` | `yeni_sira` | sayfatasi için sayfanın gideceği sıra |
| `layer` | `str` | `katman` | atlas: hangi katmanın nesneleri için bir sayfa basılacak; 'yok' atlası kapatır |
| `sort_by` | `str` | `sirala` | atlas: sayfaların sıralanacağı ve adlandırılacağı öznitelik sütunu; verilmezse nesne anahtarı |
| `group_by` | `str` | `grup` | rapor: bölümlerin oluşturulacağı öznitelik sütunu (ada_no gibi); verilmezse tek bölüm |
| `margin_percent` | `int` | `kenar_payi` | atlas: nesnenin çevresinde bırakılacak pay, yüzde (varsayılan 10) |
| `single_file` | `bool` | `tek_dosya` | atlas: tek çok sayfalı belge mi, nesne başına bir dosya mı (varsayılan evet) |

[Komut sayfası](../komutlar/layout.md)

### `cad.layout_item`

Bir çıktı yerleşiminin üzerindeki öğeleri yönetir: harita çerçevesi, başlık, ölçek çubuğu, kuzey oku, lejant, resim, şekil ve tablo ekler, taşır, ayarlar ve siler.

Komut: `core.layout_item` — `ÇIKTIÖĞE`

```python
cad.layout_item(
    action: str,
    layout: str,
    name: str,
    type: str,
    x: float,
    y: float,
    width: float,
    height: float,
    text: str,
    text_height: float,
    scale: int,
    window: Coords,
    grid: str,
    grid_spacing: int,
    locked: bool,
    frame: bool,
    page: int,
    new_name: str,
    max_rows: int,
    fields: list[str],
    layers: list[str],
    map: str,
    order: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `action` | `str` | `islem` | Ne yapılacağı |
| `layout` | `str` | `yerlesim` | Hangi çıktı yerleşimi; çizimde tek yerleşim varsa gerekmez |
| `name` | `str` | `ad` | Öğe adı; ekle dışında gerekir, ekle'de verilmezse türetilir |
| `type` | `str` | `tur` | islem=ekle için öğe türü |
| `x` | `float` | `x` | Sol kenardan uzaklık [kâğıt mm] |
| `y` | `float` | `y` | ÜST kenardan uzaklık [kâğıt mm] |
| `width` | `float` | `genislik` | Genişlik [kâğıt mm] |
| `height` | `float` | `yukseklik` | Yükseklik [kâğıt mm] |
| `text` | `str` | `metin` | Metin öğesinin yazısı; <yerlesim>, <olcek>, <tarih>, <crs> yer tutucuları çizim anında çözülür |
| `text_height` | `float` | `yazi` | Yazı yüksekliği [kâğıt mm] |
| `scale` | `int` | `olcek` | Harita öğesinin ölçeği 1:N; 0 kapsama uyar |
| `window` | `Coords` | `pencere` | Harita çerçevesinin bakacağı alanın iki köşesi, anahtar iki kez yazılarak: pencere=x1,y1 pencere=x2,y2. Tuvalden çerçeve seçmek bu satırı yazar [ZEMİN koordinatı — kâğıt değil] |
| `grid` | `str` | `izgara` | Harita öğesinin koordinat ızgarası |
| `grid_spacing` | `int` | `izgara_aralik` | Izgara aralığı, zemin milimetresi; 0 ölçeğe göre seçilir |
| `locked` | `bool` | `kilit` | Öğeyi taşımaya kapatır |
| `frame` | `bool` | `cerceve` | Öğenin çevresine çerçeve çizer |
| `page` | `int` | `sayfa` | Öğenin duracağı sayfa (1'den başlar); tasi ile verilir |
| `new_name` | `str` | `yeni_ad` | islem=ad için öğenin yeni adı |
| `max_rows` | `int` | `satir_siniri` | Tablo öğesinin yazacağı en çok satır; 0 = kutuya kaç satır sığıyorsa o kadar |
| `fields` | `list[str]` | `sutunlar` | Tablo öğesinin yazacağı öznitelik sütunları, sırasıyla; anahtar birden çok kez yazılır. Verilmezse katmanın bütün sütunları, 'hepsi' listeyi boşaltır |
| `layers` | `list[str]` | `katmanlar` | Harita çerçevesinin çizeceği katmanlar; anahtar birden çok kez yazılır. Verilmezse görünür bütün katmanlar, 'hepsi' listeyi boşaltır |
| `map` | `str` | `harita` | Bu öğenin bağlı olduğu harita çerçevesinin adı. Verilmezse ilk harita. 'ilk' bağı kaldırır |
| `order` | `int` | `sira` | Çizim sırası; büyük olan üstte |

[Komut sayfası](../komutlar/layout_item.md)

### `cad.layout_template`

Kurumun standart çıktı yerleşimlerini saklar ve uygular. Şablon çizimin dışında, kullanıcı profilinde durur; her çizime uygulanabilir. Şablon düzeni taşır, zemin koordinatlarını taşımaz.

Komut: `core.layout_template` — `ÇIKTIŞABLON`

```python
cad.layout_template(
    action: str,
    name: str,
    layout: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `action` | `str` | `islem` | Ne yapılacağı |
| `name` | `str` | `ad` | Şablonun adı; listele dışında gerekir |
| `layout` | `str` | `yerlesim` | kaydet: hangi yerleşim saklanacak (tek yerleşim varsa gerekmez). uygula: kurulacak yerleşimin adı (verilmezse şablonun adı) |

[Komut sayfası](../komutlar/layout_template.md)

### `cad.style`

Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar.

Komut: `core.style` — `STİL`

```python
cad.style(
    layer: str,
    package: str,
    scale_min: int,
    scale_max: int,
    classify_by: str,
    code: str,
    scale: int,
    color: int,
    line_width: int,
    fill: int,
    order: int,
    reset: bool,
    layer_type: str,
    add: bool,
    shape: str,
    placement: str,
    unit: str,
    size_unit: str,
    spacing_unit: str,
    spacing_y_unit: str,
    offset_unit: str,
    size: int,
    spacing: int,
    spacing_y: int,
    angle: int,
    offset: int,
    phase: int,
    phase_unit: str,
    opacity: int,
    pattern: str,
    text: str,
    fields: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `layer` | `str` | `katman` | Stilin yazılacağı katmanın adı; katman var olmalı |
| `package` | `str` | `paket` | Stil kataloğu paketinin dosya yolu |
| `scale_min` | `int` | `olcek_min` | Bu ölçek paydasından daha yakında çizilmez (1:N'deki N) |
| `scale_max` | `int` | `olcek_max` | Bu ölçek paydasından daha uzakta çizilmez |
| `classify_by` | `str` | `sinifla` | Sınıflandırmada kullanılacak öznitelik; her nesne kendi değerine göre stillenir |
| `code` | `str` | `kod` | Katalogdaki satırın kimliği; verilmezse katalog kuralları eşleşir |
| `scale` | `int` | `olcek` | Ölçek paydası (1:N); 0 = ölçekten bağımsız |
| `color` | `int` | `renk` | Çizgi rengi, 0xAARRGGBB |
| `line_width` | `int` | `kalinlik` | Çizgi kalınlığı, kâğıt mikrometresi (1000 = 1 mm) |
| `fill` | `int` | `dolgu` | Dolgu rengi, 0xAARRGGBB; 0 = dolgusuz |
| `order` | `int` | `sira` | Çizim sırası; büyük olan üste gelir |
| `reset` | `bool` | `sifirla` | Stili siler; nesneler katman varsayılanına döner |
| `layer_type` | `str` | `tip` | Sembol katmanı tipi: cizgi, isaretci-cizgi, tarak-cizgi, dolgu, cizgi-desen-dolgu, nokta-desen-dolgu, merkez-isaretci, isaretci |
| `add` | `bool` | `ekle` | Katmanı mevcut sembolün üstüne ekler; yoksa sembolü değiştirir |
| `shape` | `str` | `sekil` | İşaretçi şekli: daire, kare, ucgen, baklava, yildiz, arti, carpi, ok, yarim-daire, besgen, altigen, cizik |
| `placement` | `str` | `yerlesim` | İşaretçinin çizgi üzerindeki yeri: aralik, tepe, ilk, son, orta |
| `unit` | `str` | `birim` | Ölçülerin birimi: kagit (µm), zemin (mm), piksel |
| `size_unit` | `str` | `boyut_birim` | Yalnız `boyut` için birim; verilmezse `birim` geçerlidir |
| `spacing_unit` | `str` | `aralik_birim` | Yalnız `aralik` için birim; verilmezse `birim` geçerlidir |
| `spacing_y_unit` | `str` | `aralik_y_birim` | Yalnız `aralik_y` için birim; verilmezse `birim` geçerlidir |
| `offset_unit` | `str` | `kaydirma_birim` | Yalnız `kaydirma` için birim; verilmezse `birim` geçerlidir |
| `size` | `int` | `boyut` | İşaretçi çapı ya da tarak dişinin boyu, `birim` cinsinden |
| `spacing` | `int` | `aralik` | Çizgi boyunca ya da desende birinci eksende aralık |
| `spacing_y` | `int` | `aralik_y` | Nokta deseninde ikinci eksen; verilmezse kare desen |
| `angle` | `int` | `aci` | Desen açısı ya da işaretçi dönüklüğü, mikro derece |
| `offset` | `int` | `kaydirma` | Geometriden dik kaydırma, `birim` cinsinden |
| `phase` | `int` | `faz` | İlk işaretçinin çizgi boyunca kaç birim ileride başlayacağı; verilmezse aralığın yarısı |
| `phase_unit` | `str` | `faz_birim` | Yalnız `faz` için birim; verilmezse `birim` geçerlidir |
| `opacity` | `int` | `saydamlik` | Katman saydamlığı 0-255; 255 tam opak |
| `pattern` | `str` | `desen` | Çizgi tipi: sürekli, ya da çizgi kalınlığının katı olarak çizgi/boşluk uzunlukları — '8 1 1 1' gibi (kesik-nokta) |
| `text` | `str` | `yazi` | yazi-isaretci katmanının yazdığı sabit metin |
| `fields` | `str` | `alan` | Nesneden alınacak parametreler, virgülle: sütun[:özellik[:tür]] — 'kod:yazi:metin, kat:kalinlik'. Sütun yoksa tanımlanır |

[Komut sayfası](../komutlar/style.md)

### `cad.symbol`

Gösterim rafını yükler, ağacında gezer ve içinde arar.

Komut: `core.symbol` — `SEMBOL`

```python
cad.symbol(
    package: str,
    group: str,
    search: str,
    code: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `package` | `str` | `paket` | Yüklenecek gösterim paketinin dosya yolu |
| `group` | `str` | `grup` | Gezilecek grup yolu, düzeyler '>' ile ayrılır |
| `search` | `str` | `ara` | Etikette, kimlikte ve grup yolunda arar |
| `code` | `str` | `kod` | Tek bir gösterimin ayrıntısı |

[Komut sayfası](../komutlar/symbol.md)

### `cad.zoom`

Görünümü çizim kapsamına veya verilen çarpana ayarlar.

Komut: `core.zoom` — `YAKINLAŞ`

```python
cad.zoom(
    mode: str,
    factor: float,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `mode` | `str` | `mod` | KAPSAM | ÇARPAN | SIFIRLA |
| `factor` | `float` | `carpan` | ÇARPAN modunda ölçek katsayısı |

[Komut sayfası](../komutlar/zoom.md)

### `cad.undo`

Son işlemi geri alır.

Komut: `core.undo` — `GERİAL`

```python
cad.undo() -> int
```

[Komut sayfası](../komutlar/undo.md)

### `cad.redo`

Geri alınan işlemi yineler.

Komut: `core.redo` — `YİNELE`

```python
cad.redo() -> int
```

[Komut sayfası](../komutlar/redo.md)

### `cad.new`

Boş bir çizim açar; ekrandaki çizimin yerine geçer.

Komut: `core.new` — `YENİ`

```python
cad.new() -> int
```

[Komut sayfası](../komutlar/new.md)

### `cad.open`

Bir KentOSCad proje dosyasını açar ve çizimin yerine koyar.

Komut: `core.open` — `AÇ`

```python
cad.open(
    file: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `file` | `str` | `dosya` | Açılacak KentOSCad proje dosyasının yolu (.pcad) |

[Komut sayfası](../komutlar/open.md)

### `cad.save`

Çizimi bağlı olduğu KentOSCad proje dosyasına kaydeder.

Komut: `core.save` — `KAYDET`

```python
cad.save(
    file: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `file` | `str` | `dosya` | Hedef yol; verilmezse çizimin bağlı olduğu dosyaya yazılır |

[Komut sayfası](../komutlar/save.md)

### `cad.saveas`

Çizimi yeni bir KentOSCad proje dosyasına kaydeder ve ona bağlar.

Komut: `core.saveas` — `FARKLIKAYDET`

```python
cad.saveas(
    file: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `file` | `str` | `dosya` | Yeni proje dosyasının yolu (.pcad) |

[Komut sayfası](../komutlar/saveas.md)

### `cad.import`

Dış bir veri dosyasını çizime ekler.

Komut: `core.import` — `İÇEAKTAR`

```python
cad.import(
    file: str,
    format: str,
    layers: str,
    fields: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `file` | `str` | `dosya` | İçe aktarılacak dosyanın yolu |
| `format` | `str` | `bicim` | Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur |
| `layers` | `str` | `katmanlar` | Yalnızca bu katmanlar okunur, virgülle ayrılır; verilmezse tümü |
| `fields` | `str` | `alanlar` | Sütun olarak okunacak öznitelik alanları, virgülle; * hepsi; verilmezse alan okunmaz |

[Komut sayfası](../komutlar/import.md)

### `cad.export`

Çizimi dış bir veri biçimine yazar.

Komut: `core.export` — `DIŞAAKTAR`

```python
cad.export(
    file: str,
    format: str,
    version: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `file` | `str` | `dosya` | Yazılacak dosyanın yolu |
| `format` | `str` | `bicim` | Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur |
| `version` | `int` | `surum` | DXF sürümü: 2000, 2004, 2007 (varsayılan), 2010, 2013, 2018 |

[Komut sayfası](../komutlar/export.md)

### `cad.script`

Bir betik dosyasını komut veri yolu üzerinden çalıştırır.

Komut: `core.script` — `BETİK`

```python
cad.script(
    file: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `file` | `str` | `dosya` | Çalıştırılacak betik dosyasının yolu |

[Komut sayfası](../komutlar/script.md)

### `cad.python`

Bir Python parçacığını komut veri yolu üzerinden çalıştırır.

Komut: `core.python` — `PYTHON`

```python
cad.python(
    code: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `code` | `str` | `kod` | Çalıştırılacak Python kaynağı |

[Komut sayfası](../komutlar/python.md)

### `cad.database`

PostGIS veritabanına bağlanır; katmanları tablo, projeleri kayıt olarak yazar.

Komut: `core.database` — `VERİTABANI`

```python
cad.database(
    action: str,
    target: str,
    layer: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `action` | `str` | `islem` | baglan | kes | tablolar | katmanyaz | projekaydet | projeac | projeler | projesil |
| `target` | `str` | `hedef` | baglan: bağlantı dizesi; katmanyaz: tablo adı; proje işlemleri: proje adı |
| `layer` | `str` | `katman` | katmanyaz: yazılacak katman; yoksa etkin katman |

[Komut sayfası](../komutlar/database.md)

### `cad.print`

Çizimin bir penceresini bir yazdırma profilinin kâğıdına yerleştirip PDF dosyasına yazar ya da yazıcıya gönderir.

Komut: `core.print` — `YAZDIR`

```python
cad.print(
    window: Coords,
    center: Coord,
    scale: int,
    layout: str,
    file: str,
    printer: str,
    profile: str,
    paper: str,
    width: int,
    height: int,
    orientation: str,
    dpi: int,
    margin: int,
    title: str,
    author: str,
    password: str,
    owner_password: str,
    printable: bool,
    copyable: bool,
    modifiable: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `window` | `Coords` | `pencere` | Yazdırılacak alanın iki köşesi; merkez verilmezse ve bu da verilmezse tıklatılır [mm, Sağa (Y) önce] |
| `center` | `Coord` | `merkez` | Kâğıdın ortalanacağı nokta; pencere yerine kullanılır [mm, Sağa (Y) önce] |
| `scale` | `int` | `olcek` | Ölçek paydası (1000 = 1/1000); merkez ile kullanılır, verilmezse projenin plan ölçeği |
| `layout` | `str` | `yerlesim` | Basılacak çıktı yerleşiminin adı (ÇIKTIYERLEŞİMİ ile kurulur). Verildiğinde kâğıt, kenar ve harita penceresi yerleşimden gelir; pencere, merkez, olcek ve profil ile birlikte verilmez |
| `file` | `str` | `dosya` | PDF yazılacak dosya; yazici ile birlikte verilmez |
| `printer` | `str` | `yazici` | Yazıcının adı; "" sistem varsayılanı. dosya ile birlikte verilmez |
| `profile` | `str` | `profil` | Yazdırma profili; verilmezse varsayılan profil |
| `paper` | `str` | `kagit` | Kâğıt: A5, A4, A3, A2, A1, A0 ya da ozel (genislik ve yukseklik ile) |
| `width` | `int` | `genislik` | ozel kâğıdın eni, milimetre (dikey duruşta) |
| `height` | `int` | `yukseklik` | ozel kâğıdın boyu, milimetre (dikey duruşta) |
| `orientation` | `str` | `yon` | dikey ya da yatay |
| `dpi` | `int` | `dpi` | Çözünürlük, inç başına nokta (72–4800) |
| `margin` | `int` | `kenar` | Dört yandaki kenar boşluğu, milimetre |
| `title` | `str` | `baslik` | PDF belge başlığı |
| `author` | `str` | `yazar` | PDF yazar alanı |
| `password` | `str` | `sifre` | PDF açma şifresi (kullanıcı şifresi); günlüğe yazılmaz |
| `owner_password` | `str` | `sahip_sifresi` | PDF izinlerini değiştirme şifresi (sahip şifresi); günlüğe yazılmaz |
| `printable` | `bool` | `yazdirilabilir` | Şifreli PDF: sahip şifresi olmayan yazdırabilir mi; varsayılan evet |
| `copyable` | `bool` | `kopyalanabilir` | Şifreli PDF: metin ve grafik kopyalanabilir mi; varsayılan evet |
| `modifiable` | `bool` | `degistirilebilir` | Şifreli PDF: belge değiştirilebilir mi; varsayılan evet |

[Komut sayfası](../komutlar/print.md)

### `cad.print_profile`

Yazdırma profillerini listeler, ekler, siler ya da birini varsayılan yapar; profil kâğıdı, yönü, çözünürlüğü ve kenar boşluğunu taşır.

Komut: `core.print_profile` — `YAZDIRMAPROFİLİ`

```python
cad.print_profile(
    action: str,
    name: str,
    paper: str,
    width: int,
    height: int,
    orientation: str,
    dpi: int,
    margin: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `action` | `str` | `islem` | listele, ekle, sil ya da varsayilan |
| `name` | `str` | `ad` | Profilin adı (ekle, sil, varsayilan) |
| `paper` | `str` | `kagit` | Kâğıt: A5, A4, A3, A2, A1, A0 ya da ozel; ekle için, varsayılan A4 |
| `width` | `int` | `genislik` | ozel kâğıdın eni, milimetre |
| `height` | `int` | `yukseklik` | ozel kâğıdın boyu, milimetre |
| `orientation` | `str` | `yon` | dikey ya da yatay; varsayılan dikey |
| `dpi` | `int` | `dpi` | Çözünürlük; varsayılan 300 |
| `margin` | `int` | `kenar` | Kenar boşluğu, milimetre; varsayılan 10 |

[Komut sayfası](../komutlar/print_profile.md)

### `cad.setting`

Proje ayarlarını listeler, okur ve değiştirir.

Komut: `core.setting` — `AYAR`

```python
cad.setting(
    name: str,
    value: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `name` | `str` | `ad` | Ayar adı veya kimliği; yoksa liste |
| `value` | `str` | `deger` | Yeni değer; yoksa yalnızca okur |

[Komut sayfası](../komutlar/setting.md)

### `cad.preference`

Uygulama tercihlerini listeler, okur ve değiştirir.

Komut: `core.preference` — `TERCİH`

```python
cad.preference(
    name: str,
    value: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `name` | `str` | `ad` | Tercih adı veya kimliği; yoksa liste |
| `value` | `str` | `deger` | Yeni değer; yoksa yalnızca okur |

[Komut sayfası](../komutlar/preference.md)

### `cad.mode`

Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur ve değiştirir.

Komut: `core.mode` — `MOD`

```python
cad.mode(
    name: str,
    value: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `name` | `str` | `ad` | Mod adı veya kimliği; yoksa liste |
| `value` | `str` | `deger` | Yeni değer; yoksa yalnızca okur |

[Komut sayfası](../komutlar/mode.md)

### `cad.help`

Komut listesini veya tek bir komutun ayrıntısını gösterir.

Komut: `core.help` — `YARDIM`

```python
cad.help(
    command: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `command` | `str` | `komut` | Ayrıntısı istenen komut adı |

[Komut sayfası](../komutlar/help.md)

### `cad.buffer`

Kapsamdaki nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer: çizginin iki yanı, noktanın çevresi, alanın dışı; üst üste binen tamponlar tek alan olur.

Komut: `islem.tampon` — `TAMPON`

```python
cad.buffer(
    objects: list[int],
    scope: str,
    window: Coords,
    layer: str,
    distance: float,
    dissolve: bool,
    corner: str,
    end: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı] |
| `scope` | `str` | `kapsam` | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `window` | `Coords` | `pencere` | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce] |
| `layer` | `str` | `katman` | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |
| `distance` | `float` | `mesafe` | Tampon mesafesi, metre; eksi değer yalnız alanları içeri aşındırır |
| `dissolve` | `bool` | `birlestir` | Üst üste binen tamponları tek alanda birleştir; kapalıysa her nesnenin tamponu ayrı alan olur; varsayılan evet |
| `corner` | `str` | `kose` | Dış köşelerin biçimi (yuvarlak / koseli / pah); varsayılan yuvarlak |
| `end` | `str` | `uc` | Çizgi uçlarının biçimi (yuvarlak / duz / kare); varsayılan yuvarlak |

[Komut sayfası](../komutlar/tampon.md)

### `cad.adjust_area`

Kapalı bir alanı istenen alana getirir: bütün kenarları eşit daraltıp genişleterek, bir kenarı kaydırarak ya da bir köşeyi çekerek; şekil bozulmaz.

Komut: `islem.alan_duzenle` — `ALANDÜZENLE`

```python
cad.adjust_area(
    objects: list[int],
    scope: str,
    window: Coords,
    layer: str,
    area: float,
    mode: str,
    edge: int,
    vertex: int,
    point: Coord,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı] |
| `scope` | `str` | `kapsam` | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `window` | `Coords` | `pencere` | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce] |
| `layer` | `str` | `katman` | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |
| `area` | `float` | `alan` | Hedef alan, metrekare |
| `mode` | `str` | `mod` | Nasıl getirileceği (hepsi / kenar / kose); varsayılan hepsi |
| `edge` | `int` | `kenar` | Kaydırılacak kenar (ilk köşeden çıkan kenar 1); mod=kenar |
| `vertex` | `int` | `kose` | Çekilecek köşe; mod=kose |
| `point` | `Coord` | `nokta` | Kenarın ya da köşenin gideceği yer; verilmezse arayüz sürükletir, komut satırı hedefe tam oturtur [mm, Sağa (Y) önce] |

[Komut sayfası](../komutlar/alan_duzenle.md)

### `cad.label_length`

Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel bir yazı olarak yazar; yazı kenara bağlıdır, kenar değişince izler ve yenilenir.

Komut: `islem.uzunluk_yaz` — `UZUNLUKYAZ`

```python
cad.label_length(
    objects: list[int],
    scope: str,
    window: Coords,
    layer: str,
    unit: str,
    decimals: int,
    format: str,
    decimal_separator: str,
    side: str,
    height: int,
    gap: int,
    min_length: int,
    attach: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı] |
| `scope` | `str` | `kapsam` | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `window` | `Coords` | `pencere` | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce] |
| `layer` | `str` | `katman` | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |
| `unit` | `str` | `birim` | Uzunluğun yazılacağı birim (metre / santimetre / milimetre / kilometre); varsayılan metre |
| `decimals` | `int` | `ondalik` | Virgülden sonraki basamak sayısı; varsayılan 2 |
| `format` | `str` | `bicim` | Yazının kalıbı; {} sayının yerini tutar (örnek: "{} m", "L={}") |
| `decimal_separator` | `str` | `ayrac` | Ondalık ayracı (virgul / nokta); varsayılan virgul |
| `side` | `str` | `taraf` | Yazının kenarın hangi yanına düşeceği (otomatik / sol / sag / dis / ic); varsayılan otomatik |
| `height` | `int` | `yukseklik` | Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm; varsayılan 0 |
| `gap` | `int` | `bosluk` | Kenar ile yazı arası, milimetre; 0 = yüksekliğin yarısı; varsayılan 0 |
| `min_length` | `int` | `enaz` | Bundan kısa kenarlara yazı yazılmaz, milimetre; varsayılan 0 |
| `attach` | `bool` | `bagla` | Yazıyı kenarına bağla: kenar taşınınca yazı izler, uzunluk yeniden yazılır; varsayılan evet |

[Komut sayfası](../komutlar/uzunluk_yaz.md)

### `cad.number_vertices`

Kapsamdaki her alanın (ve çizginin) köşelerini seçilen köşeden başlayarak sırayla numaralar ve numarayı köşenin dışına yazar; numara köşesine bağlıdır, köşe taşınınca izler.

Komut: `islem.kose_numarala` — `KÖŞENUMARALA`

```python
cad.number_vertices(
    objects: list[int],
    scope: str,
    window: Coords,
    layer: str,
    start: Coord,
    direction: str,
    prefix: str,
    digits: int,
    pad: str,
    first_number: int,
    suffix: str,
    height: int,
    gap: int,
    attach: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı] |
| `scope` | `str` | `kapsam` | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `window` | `Coords` | `pencere` | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce] |
| `layer` | `str` | `katman` | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |
| `start` | `Coord` | `baslangic` | Sayımın başlayacağı köşeye en yakın nokta; verilmezse ilk köşe [mm, Sağa (Y) önce] |
| `direction` | `str` | `yon` | Sayım yönü (ters / saat); varsayılan ters |
| `prefix` | `str` | `onek` | Numaranın önüne gelen yazı (örnek: A, K-) |
| `digits` | `int` | `basamak` | Numaranın en az basamak sayısı; eksikler dolgu ile tamamlanır; varsayılan 0 |
| `pad` | `str` | `dolgu` | Basamak dolgusu; varsayılan 0 |
| `first_number` | `int` | `ilk` | İlk köşenin numarası; varsayılan 1 |
| `suffix` | `str` | `sonek` | Numaranın arkasına gelen yazı |
| `height` | `int` | `yukseklik` | Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm; varsayılan 0 |
| `gap` | `int` | `bosluk` | Köşe ile yazı arası, milimetre; 0 = yüksekliğin yarısı; varsayılan 0 |
| `attach` | `bool` | `bagla` | Numarayı köşesine bağla: köşe taşınınca numara izler; varsayılan evet |

[Komut sayfası](../komutlar/kose_numarala.md)

### `cad.detach`

Kapsamdaki yazıların bağını çözer: yazı yerinde kalır, bağlı olduğu nesne bundan sonra tek başına taşınır.

Komut: `islem.bag_coz` — `BAĞÇÖZ`

```python
cad.detach(
    objects: list[int],
    scope: str,
    window: Coords,
    layer: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı] |
| `scope` | `str` | `kapsam` | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `window` | `Coords` | `pencere` | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce] |
| `layer` | `str` | `katman` | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |

[Komut sayfası](../komutlar/bag_coz.md)

### `cad.attach`

Kapsamdaki yazıları seçilen nesnenin en yakın kenarına ya da köşesine bağlar: nesne taşınınca yazı izler; istenirse yazı kenarın uzunluğu olur.

Komut: `islem.bagla` — `BAĞLA`

```python
cad.attach(
    objects: list[int],
    scope: str,
    window: Coords,
    layer: str,
    source: list[int],
    attach_to: str,
    type: str,
    unit: str,
    decimals: int,
    format: str,
    decimal_separator: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı] |
| `scope` | `str` | `kapsam` | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `window` | `Coords` | `pencere` | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce] |
| `layer` | `str` | `katman` | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |
| `source` | `list[int]` | `kaynak` | Yazıların bağlanacağı nesne (çizgi ya da alan) [kalıcı nesne anahtarı] |
| `attach_to` | `str` | `bag` | Neye bağlanacağı: en yakın kenar ya da en yakın köşe (kenar / kose); varsayılan kenar |
| `type` | `str` | `tur` | Yazının sözü: kendi yazısı kalır ya da kenarın uzunluğu olur (sabit / uzunluk); varsayılan sabit |
| `unit` | `str` | `birim` | Uzunluğun birimi (tur=uzunluk) (metre / santimetre / milimetre / kilometre); varsayılan metre |
| `decimals` | `int` | `ondalik` | Virgülden sonraki basamak sayısı (tur=uzunluk); varsayılan 2 |
| `format` | `str` | `bicim` | Uzunluk yazısının kalıbı; {} sayının yerini tutar (tur=uzunluk) |
| `decimal_separator` | `str` | `ayrac` | Ondalık ayracı (tur=uzunluk) (virgul / nokta); varsayılan virgul |

[Komut sayfası](../komutlar/bagla.md)

### `cad.polygonize`

Kapsamdaki çizgilerin kapattığı her gözü ayrı bir alan olarak çizer; içerideki adalar delik olur, açık uçlar sayılıp gösterilir ve hiçbiri kendiliğinden kapanmaz.

Komut: `islem.alan_uret` — `ALANÜRET`

```python
cad.polygonize(
    objects: list[int],
    scope: str,
    window: Coords,
    layer: str,
    islands: bool,
    gap: float,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz [kalıcı nesne anahtarı] |
| `scope` | `str` | `kapsam` | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `window` | `Coords` | `pencere` | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir [mm, Sağa (Y) önce] |
| `layer` | `str` | `katman` | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |
| `islands` | `bool` | `ada` | Bir gözün içindeki kapalı çizgiler o alanın deliği olsun; kapalıysa göz dış sınırıyla dolu çizilir; varsayılan evet |
| `gap` | `float` | `bosluk` | Bu genişliğe kadar açık uçları köprüle, metre; 0: hiçbir boşluk kendiliğinden kapanmaz; varsayılan 0 |

[Komut sayfası](../komutlar/alan_uret.md)

### `cad.fit`

Yerel ölçülmüş çizimi kontrol noktalarıyla haritaya oturtur (2B Helmert).

Komut: `core.fit` — `OTURT`

```python
cad.fit(
    points: Coords,
    scale_locked: bool,
    crs: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `points` | `Coords` | `noktalar` | Kontrol çiftleri: yerel, harita, yerel, harita... [mm, Sağa (Y) önce] |
| `scale_locked` | `bool` | `olcek_kilitli` | Ölçeği 1'de tutar; saha ölçüsü yeniden ölçeklenmez |
| `crs` | `str` | `sistem` | Oturtulduktan sonraki koordinat sistemi, örnek TUREF/TM36 |

[Komut sayfası](../komutlar/fit.md)

### `cad.stakeout`

İstasyondan her noktaya mesafe ve açı listesi çıkarır (aplikasyon).

Komut: `core.stakeout` — `APLİKASYON`

```python
cad.stakeout(
    station: Coord,
    backsight: Coord,
    objects: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `station` | `Coord` | `istasyon` | Aletin durduğu nokta [mm, Sağa (Y) önce] |
| `backsight` | `Coord` | `baglama` | Bağlama (arka görüş) noktası; verilirse açılar ondan ölçülür [mm, Sağa (Y) önce] |
| `objects` | `list[int]` | `nesneler` | Aplike edilecek noktalar; yoksa seçim, o da boşsa çizimdeki bütün noktalar [kalıcı nesne anahtarı] |

[Komut sayfası](../komutlar/stakeout.md)

### `cad.reproject`

Çizimin tamamını bir koordinat sisteminden diğerine dönüştürür.

Komut: `core.reproject` — `DÖNÜŞTÜR`

```python
cad.reproject(
    target: str,
    source: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `target` | `str` | `hedef` | Hedef koordinat sistemi, örnek EPSG:5256 ya da TUREF/TM36 |
| `source` | `str` | `kaynak` | Kaynak sistem; yoksa çizimin kendi koordinat sistemi |

[Komut sayfası](../komutlar/reproject.md)

### `cad.traverse`

Kırılma açısı ve kenarlardan poligon koordinatları hesaplar, kapanma hatalarını dağıtır ve mevzuat toleransına karşı denetler.

Komut: `geodesy.traverse` — `POLİGON`

```python
cad.traverse(
    start: Coord,
    backsight: Coord,
    angle: list[float],
    distance: list[float],
    end: Coord,
    end_backsight: Coord,
    tolerance_class: str,
    first_number: int,
    distribution: str,
    connect: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `start` | `Coord` | `baslangic` | Başlangıç istasyonu (bilinen) [mm, Sağa (Y) önce] |
| `backsight` | `Coord` | `baglama` | Başlangıçtaki bağlama noktası (bilinen) [mm, Sağa (Y) önce] |
| `angle` | `list[float]` | `aci` | Her istasyonda okunan kırılma açısı, ölçü karnesi sırasıyla |
| `distance` | `list[float]` | `kenar` | Her istasyondan sonraki kenar (m) [m] |
| `end` | `Coord` | `bitis` | Bitiş istasyonu (bilinen); verilirse kapanma hesaplanır [mm, Sağa (Y) önce] |
| `end_backsight` | `Coord` | `bitis_baglama` | Bitişteki bağlama noktası; açı kapanması için gerekir [mm, Sağa (Y) önce] |
| `tolerance_class` | `str` | `sinif` | Tolerans sınıfı; katalogdan okunur |
| `first_number` | `int` | `ilk_no` | İlk istasyonun nokta numarası; varsayılan 1 |
| `distribution` | `str` | `dagitim` | Kenar kapanmasının dağıtımı: eşit ya da kenar orantılı |
| `connect` | `bool` | `cizgi` | Güzergâhı çizgiyle bağlar; varsayılan evet |

[Komut sayfası](../komutlar/traverse.md)

### `cad.merge`

Komşu parselleri tek parselde birleştirir (tevhit).

Komut: `core.merge` — `TEVHİT`

```python
cad.merge(
    objects: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Birleştirilecek parseller; yoksa etkin seçim [kalıcı nesne anahtarı] |

[Komut sayfası](../komutlar/merge.md)

### `cad.split_parcel`

Bir parseli düz bir ayırma çizgisiyle ikiye böler (ifraz).

Komut: `core.split_parcel` — `İFRAZ`

```python
cad.split_parcel(
    points: Coords,
    objects: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `points` | `Coords` | `noktalar` | Ayırma çizgisinin iki ucu [mm, Sağa (Y) önce] |
| `objects` | `list[int]` | `nesneler` | Ayrılacak parsel; yoksa etkin seçim [kalıcı nesne anahtarı] |

[Komut sayfası](../komutlar/split_parcel.md)

### `cad.split_area`

Parselden verilen yöne paralel, istenen alanda bir parça ayırır.

Komut: `core.split_area` — `ALANİFRAZ`

```python
cad.split_area(
    direction: Coords,
    objects: list[int],
    area: int,
    tolerance: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `direction` | `Coords` | `yon` | Ayırma çizgisinin YÖNÜ: iki nokta (yol cephesi, mevcut sınır) [mm, Sağa (Y) önce] |
| `objects` | `list[int]` | `nesneler` | Ayrılacak parsel; yoksa etkin seçim [kalıcı nesne anahtarı] |
| `area` | `int` | `alan` | Ayrılacak alan, mm² (400 m² = 400000000) |
| `tolerance` | `int` | `tolerans` | Kabul toleransı, mm²; varsayılan 10000 (0,01 m²) |

[Komut sayfası](../komutlar/split_area.md)

### `cad.topology`

Kendini kesen sınır, sıfır alan ve örtüşen parselleri; yinelenen ve boş nesneleri, tekrarlanan köşeleri ve çizgi ağındaki boşlukları raporlar.

Komut: `core.topology` — `TOPOLOJİ`

```python
cad.topology(
    objects: list[int],
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Denetlenecek nesneler; yoksa seçim, o da boşsa bütün çizim [kalıcı nesne anahtarı] |

[Komut sayfası](../komutlar/topology.md)

### `cad.contour`

Kotlu noktalardan eş yükselti eğrileri çizer.

Komut: `core.contour` — `EŞYÜKSELTİ`

```python
cad.contour(
    interval: int,
    layer: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `interval` | `int` | `aralik` | Eş yükselti aralığı, milimetre; varsayılan 1000 (1 m) |
| `layer` | `str` | `katman` | Eğrilerin çizileceği katman; varsayılan ESYUKSELTI |

[Komut sayfası](../komutlar/contour.md)

### `cad.earthwork`

Kotlu noktalardan bir kota göre kazı ve dolgu hacmini hesaplar.

Komut: `core.earthwork` — `HACİM`

```python
cad.earthwork(
    elevation: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `elevation` | `int` | `kot` | Karşılaştırma kotu, milimetre (845 m = 845000) |

[Komut sayfası](../komutlar/earthwork.md)

### `cad.layers`

Katmanları, nesne sayılarını, görünürlük ve kilit durumlarını listeler.

Komut: `core.layers` — `KATMANLAR`

```python
cad.layers() -> int
```

[Komut sayfası](../komutlar/layers.md)

### `cad.attr_schema`

Çizimde tanımlı öznitelik sütunlarını ve tiplerini listeler.

Komut: `core.attr_schema` — `ÖZNİTELİKŞEMASI`

```python
cad.attr_schema() -> int
```

[Komut sayfası](../komutlar/attr_schema.md)

### `cad.query`

Katman ve öznitelik koşuluna uyan nesneleri sayar ve anahtarlarını bildirir.

Komut: `core.query` — `SORGULA`

```python
cad.query(
    layer: str,
    field: str,
    value: str,
    limit: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `layer` | `str` | `katman` | Hangi katmanda aranacağı; verilmezse bütün çizim |
| `field` | `str` | `alan` | Öznitelik sütunu; verilirse o sütunu taşıyan nesneler |
| `value` | `str` | `deger` | Sütunun eşit olması istenen değer; yalnız 'alan' ile birlikte |
| `limit` | `int` | `sinir` | En çok kaç nesne bildirileceği; varsayılan 200 |

[Komut sayfası](../komutlar/query.md)

### `cad.selection_info`

Kullanıcının o anki seçimini bildirir: kaç nesne ve hangi anahtarlar.

Komut: `core.selection_info` — `SEÇİMBİLGİSİ`

```python
cad.selection_info() -> int
```

[Komut sayfası](../komutlar/selection_info.md)

### `cad.object_points`

Nesnelerin merkezini, köşelerini, uçlarını, kutusunu ya da kenar ortalarını bildirir; bir ajan bunları yeni çizimin taban noktası olarak kullanır.

Komut: `core.object_points` — `NESNENOKTALARI`

```python
cad.object_points(
    objects: list[int],
    which: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `objects` | `list[int]` | `nesneler` | Noktaları istenen nesneler [kalıcı nesne anahtarı] |
| `which` | `str` | `tur` | Hangi noktalar: merkez (alanın ağırlık merkezi, çizginin uzunluk ortası, dairenin merkezi), köşeler, uçlar, kutunun köşeleri ya da kenar ortaları; varsayılan merkez |

[Komut sayfası](../komutlar/object_points.md)

### `cad.view_info`

Ekranda görünen alanın köşe koordinatlarını, merkezini, ölçeğini ve CRS'ini bildirir.

Komut: `core.view_info` — `GÖRÜNÜMBİLGİSİ`

```python
cad.view_info() -> int
```

[Komut sayfası](../komutlar/view_info.md)

### `cad.context`

Üzerinde çalışılan her şeyi tek çağrıda özetler: belge sürümü, koordinat sistemi, kapsam, katmanlar, çıktı yerleşimleri ve hedefli olup olmadıkları, seçili nesneler ve görünüm. Özet verir, döküm değil.

Komut: `core.context` — `BAĞLAM`

```python
cad.context() -> int
```

[Komut sayfası](../komutlar/context.md)

### `cad.tool_search`

Ajan araç kataloğunda ad ve özete göre arar. Sonuç her zaman kaç aracın eşleştiğini, kaçının gösterildiğini ve katalogdaki toplam araç sayısını söyler: arama hiçbir aracı gizlemez, tam liste `tools/list` ile alınır.

Komut: `core.tool_search` — `ARAÇARA`

```python
cad.tool_search(
    query: str,
    field: str,
    limit: int,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `query` | `str` | `sorgu` | Aranan sözcük; ad ve özet içinde Türkçe katlamayla eşleşir |
| `field` | `str` | `alan` | Nerede aranacağı: hepsi (öntanımlı), ad ya da ozet |
| `limit` | `int` | `sinir` | En çok kaç sonuç gösterilsin; öntanımlı 20. Eşleşme sayısı her hâlde bildirilir |

[Komut sayfası](../komutlar/tool_search.md)

### `cad.job_template`

Sık yapılan işlerin — atlas, kadastro kontrolü, parsel raporu — komut satırlarını sırasıyla verir. Hiçbirini çalıştırmaz: adımlar olağan araç yüzeyinden gönderilir ve yazan her adım yine öneri olur.

Komut: `core.job_template` — `İŞŞABLONU`

```python
cad.job_template(
    action: str,
    template: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `action` | `str` | `islem` | Ne yapılacağı: listele ya da goster |
| `template` | `str` | `sablon` | Şablonun kimliği; goster için gerekir |

[Komut sayfası](../komutlar/job_template.md)

### `cad.suggestion`

Bekleyen yapay zeka önerilerini listeler, durumunu söyler, uygular ya da reddeder.

Komut: `core.suggestion` — `ÖNERİ`

```python
cad.suggestion(
    action: str,
    suggestion: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `action` | `str` | `islem` | Ne yapılacağı: uygula, reddet, durum ya da listele |
| `suggestion` | `str` | `oneri` | Öneri kimliği; uygula, reddet ve durum için gerekir |

[Komut sayfası](../komutlar/suggestion.md)

### `cad.mcp`

Yapay zeka ajanlarının bağlanacağı MCP sunucusunu başlatır, durdurur, durumunu söyler, yeni bir erişim belirteci üretir, bağlı istemcileri listeler ve tek bir istemcinin yetkisini kaldırır.

Komut: `core.mcp` — `MCPSUNUCU`

```python
cad.mcp(
    action: str,
    port: int,
    name: str,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `action` | `str` | `islem` | Ne yapılacağı: baslat, durdur, durum, belirtec (yeni belirteç üretir), istemciler, iptal (bir istemcinin yetkisini kaldırır), izin (geri verir) ya da sina (bağlantıyı sınar) |
| `port` | `int` | `port` | Yalnız bu başlatma için port; verilmezse ayardaki port |
| `name` | `str` | `ad` | İstemcinin adı; iptal ve izin için gerekir. Adları islem=istemciler ile görün |

[Komut sayfası](../komutlar/mcp.md)

### `cad.ai_provider`

Yapay zeka model sağlayıcılarını listeler, ekler, siler, birini varsayılan yapar ya da bağlantısını dener; profil adresi, lehçesi, modeli ve anahtar adını taşır.

Komut: `core.ai_provider` — `YAPAYZEKAMODELİ`

```python
cad.ai_provider(
    action: str,
    name: str,
    dialect: str,
    endpoint: str,
    path: str,
    model: str,
    key_ref: str,
    context: int,
    max_tokens: int,
    temperature: float,
    stream: bool,
    thinking: bool,
    tools: bool,
) -> int
```

| Anahtar | Tür | Türkçe adı | Açıklama |
|---|---|---|---|
| `action` | `str` | `islem` | Ne yapılacağı: listele, ekle, sil, varsayilan ya da dene (bağlantıyı dener) |
| `name` | `str` | `ad` | Profilin adı; ekle, sil, varsayilan ve dene için gerekir |
| `dialect` | `str` | `lehce` | Uç noktanın konuştuğu telli dil; ekle için, varsayılan openai_chat |
| `endpoint` | `str` | `adres` | Uç noktanın adresi: http:// ya da https:// ile başlar, satıcının ön eki dahil |
| `path` | `str` | `yol` | Adresin altındaki uç nokta; '/' ile başlar: /chat/completions, /messages, /api/chat |
| `model` | `str` | `model` | Model kimliği, uç noktanın yazdığı gibi |
| `key_ref` | `str` | `anahtar_ref` | Anahtarı tutan kaydın adı — anahtar zincirindeki kayıt ya da bir ortam değişkeni (örnek: DEEPSEEK_API_KEY). Anahtarın kendisi buraya yazılmaz |
| `context` | `int` | `baglam` | Bağlam penceresi, jeton; 0 bilinmiyor demektir |
| `max_tokens` | `int` | `azami` | Çıktı jeton sınırı; 0 demek 'bu alanı hiç gönderme' |
| `temperature` | `float` | `sicaklik` | Örnekleme sıcaklığı, 0 ile 2 arasında; verilmezse hiç gönderilmez |
| `stream` | `bool` | `akis` | Cevap parça parça mı istensin; varsayılan evet |
| `thinking` | `bool` | `dusunme` | Modelin düşünme metni gösterilsin mi |
| `tools` | `bool` | `araclar` | Uç noktaya araç kataloğu gönderilsin mi; varsayılan evet |

[Komut sayfası](../komutlar/ai_provider.md)
