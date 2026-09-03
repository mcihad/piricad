<!-- ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN. -->
<!-- Kaynak: kentos::command::Registry.  Yeniden üret: make reference -->
<!-- Bir komutun burada görünmesi için tek yapılması gereken onu kaydetmektir; -->
<!-- projede elle tutulan ikinci bir komut listesi yoktur (CLAUDE.md 5.10). -->

# Komut Referansı

Bu tablo komut kaydından üretilir. Her komutun ayrıntılı kullanım sayfası
`docs/komutlar/` altındadır ve tablodan bağlanır.

| Komut | Adlar | Kategori | Geri alma | Özellikler | Açıklama |
|---|---|---|---|---|---|
| [`core.line`](line.md) | `ÇİZGİ`, `CIZGI`, `LINE`, `Ç`, `L` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | İki veya daha fazla nokta arasında doğru parçaları çizer. |
| [`core.polyline`](polyline.md) | `ÇOKLUÇİZGİ`, `COKLUCIZGI`, `POLYLINE`, `ÇÇ`, `PL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Birden çok noktadan TEK bir çizgi nesnesi çizer. |
| [`core.point_draw`](point_draw.md) | `NOKTA`, `POINT`, `NK` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper. |
| [`core.text`](text.md) | `METİN`, `METIN`, `YAZI`, `TEXT`, `MT` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Çizime metin yazar; yükseklik ve hizalama verilebilir. |
| [`core.exportstyle`](exportstyle.md) | `STİLAKTAR`, `STILAKTAR`, `EXPORTSTYLE`, `STAKTAR` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar. |
| [`core.area`](area.md) | `ALAN`, `AREA`, `POLİGON`, `POLIGON`, `AL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapalı bir alan çizer; istenirse içine delik açar. |
| [`core.rectangle`](rectangle.md) | `DİKDÖRTGEN`, `DIKDORTGEN`, `RECTANGLE`, `DKD`, `REC` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Karşılıklı iki köşeden dört köşeli kapalı bir alan çizer. |
| [`core.circle_draw`](circle_draw.md) | `DAİRE`, `DAIRE`, `CIRCLE`, `DR` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez ve çember üzerindeki bir noktadan daire çizer. |
| [`core.arc_draw`](arc_draw.md) | `YAY`, `ARC`, `YY` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez ve iki uçtan yay çizer; süpürme saat yönünün tersinedir. |
| [`core.vertex_move`](vertex_move.md) | `KÖŞETAŞI`, `KOSETASI`, `MOVEVERTEX`, `KT` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir nesnenin köşesini yeni bir yere taşır. |
| [`core.vertex_insert`](vertex_insert.md) | `KÖŞEEKLE`, `KOSEEKLE`, `ADDVERTEX`, `KE` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir kenarın ortasına yeni köşe ekler. |
| [`core.to_area`](to_area.md) | `ALANAÇEVİR`, `ALANACEVIR`, `TOAREA`, `ALÇ` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Uç uca değen çizgileri tek bir kapalı alana çevirir. |
| [`core.move`](move.md) | `TAŞI`, `TASI`, `MOVE`, `TŞ` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri iki nokta arasındaki kadar taşır. |
| [`core.copy`](copy.md) | `KOPYALA`, `COPY`, `KP` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesnelerin kopyasını iki nokta arasındaki kadar öteye koyar. |
| [`core.array`](array.md) | `DİZİ`, `DIZI`, `ARRAY`, `DZ` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri satır/sütun ya da bir merkez etrafında çoğaltır. |
| [`core.combine`](combine.md) | `BİRLEŞTİR`, `BIRLESTIR`, `COMBINE`, `BRL` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek çizgi yapar. |
| [`core.split`](split.md) | `BÖL`, `BOL`, `SPLIT`, `BL` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir çizgiyi verilen noktadan ikiye böler. |
| [`core.trim`](trim.md) | `BUDA`, `TRIM`, `BD` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir çizgiyi kestiği sınır çizgisine kadar budar. |
| [`core.extend`](extend.md) | `UZAT`, `EXTEND`, `UZ` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir çizgiyi sınır çizgisine ulaşana kadar uzatır. |
| [`core.chamfer`](chamfer.md) | `PAH`, `CHAMFER`, `PH` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir köşeyi düz bir kenarla keser (pah kırar). |
| [`core.fillet`](fillet.md) | `YUVARLA`, `FILLET`, `YV` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir köşeyi verilen yarıçapta yay ile yuvarlatır. |
| [`core.set_layer`](set_layer.md) | `KATMANAT`, `KATMANATA`, `SETLAYER`, `KA` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri başka bir katmana taşır. |
| [`core.match_style`](match_style.md) | `STİLKOPYALA`, `STILKOPYALA`, `MATCHPROP`, `SK` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir nesnenin stilini seçilen nesnelere uygular. |
| [`core.rotate`](rotate.md) | `DÖNDÜR`, `DONDUR`, `ROTATE`, `DÖN` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri bir merkez etrafında döndürür. |
| [`core.scale`](scale.md) | `ÖLÇEKLE`, `OLCEKLE`, `SCALE`, `ÖLÇEK`, `OLCEK` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri bir merkeze göre büyütür ya da küçültür. |
| [`core.mirror`](mirror.md) | `AYNALA`, `MIRROR`, `AYN` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri iki noktadan geçen eksende aynalar. |
| [`core.measure`](measure.md) | `ÖLÇ`, `OLC`, `MEASURE`, `MS` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | İki nokta arasındaki mesafeyi, koordinat farkını ve açıyı yazar. |
| [`core.measure_area`](measure_area.md) | `ALANÖLÇ`, `ALANOLC`, `AREAOF`, `AÖ` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Seçilen nesnelerin alanını ve çevresini yazar. |
| [`core.coordinate`](coordinate.md) | `KOORDİNAT`, `KOORDINAT`, `COORDINATE`, `KRD` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar. |
| [`core.pan`](pan.md) | `KAYDIR`, `PAN`, `KY` | Görünüm | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, şeffaf, salt okunur | Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır. |
| [`core.offset`](offset.md) | `OFSET`, `OFFSET`, `OF` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçili nesnelerin verilen mesafede paralelini çizer. |
| [`core.sector`](sector.md) | `DİLİM`, `DILIM`, `SECTOR`, `DL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir. |
| [`core.annulus`](annulus.md) | `HALKA`, `ANNULUS`, `HLK` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez, iç ve dış yarıçaptan delikli halka çizer. |
| [`core.ellipse_draw`](ellipse_draw.md) | `ELİPS`, `ELIPS`, `ELLIPSE`, `EL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir. |
| [`core.points`](points.md) | `NOKTALAR`, `POINTS`, `NKL` | Dosya | tek işlem | betiklenebilir, AI erişimli | Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod). |
| [`core.guide`](guide.md) | `KILAVUZ`, `GUIDE`, `KLV` | Çizim | tek işlem | betiklenebilir, AI erişimli | Cetvel kılavuzu ekler, listeler ve siler. |
| [`core.attribute`](attribute.md) | `ÖZNİTELİK`, `OZNITELIK`, `ATTRIBUTE`, `ÖZN`, `OZN` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar. |
| [`core.column`](column.md) | `SÜTUN`, `SUTUN`, `COLUMN`, `STN` | Düzenleme | geri alınmaz | betiklenebilir | Belgeye öznitelik sütunu tanımlar ve tanımlı sütunları listeler. |
| [`core.erase`](erase.md) | `SİL`, `SIL`, `ERASE`, `E` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Seçilen nesneleri siler. |
| [`core.select`](select.md) | `SEÇ`, `SEC`, `SELECT`, `S` | Düzenleme | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Nesneleri seçer: tümü, kimlikle, pencere, kesen kutu veya tek nokta. |
| [`core.label`](label.md) | `ETİKET`, `ETIKET`, `LABEL`, `ETK` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Katmandaki nesneleri özniteliklerinden okuyarak etiketler. |
| [`core.layer`](layer.md) | `KATMAN`, `LAYER`, `KAT` | Katman | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Katman oluşturur, aktif yapar ve özelliklerini değiştirir. |
| [`core.style`](style.md) | `STİL`, `STIL`, `STYLE`, `ST` | Katman | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar. |
| [`core.symbol`](symbol.md) | `SEMBOL`, `SEMBOLLER`, `SYMBOL`, `SMB` | Katman | geri alınmaz | betiklenebilir, AI erişimli | Gösterim rafını yükler, ağacında gezer ve içinde arar. |
| [`core.zoom`](zoom.md) | `YAKINLAŞ`, `YAKINLAS`, `ZOOM`, `Z` | Görünüm | geri alınmaz | betiklenebilir, AI erişimli, şeffaf, salt okunur | Görünümü çizim kapsamına veya verilen çarpana ayarlar. |
| [`core.undo`](undo.md) | `GERİAL`, `GERIAL`, `UNDO`, `U` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Son işlemi geri alır. |
| [`core.redo`](redo.md) | `YİNELE`, `YINELE`, `REDO` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Geri alınan işlemi yineler. |
| [`core.open`](open.md) | `AÇ`, `AC`, `OPEN` | Dosya | geri alınmaz | etkileşimli, betiklenebilir | Bir KentOSCad proje dosyasını açar ve çizimin yerine koyar. |
| [`core.save`](save.md) | `KAYDET`, `SAVE`, `KYD` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Çizimi bağlı olduğu KentOSCad proje dosyasına kaydeder. |
| [`core.saveas`](saveas.md) | `FARKLIKAYDET`, `SAVEAS`, `FKAYDET` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Çizimi yeni bir KentOSCad proje dosyasına kaydeder ve ona bağlar. |
| [`core.import`](import.md) | `İÇEAKTAR`, `ICEAKTAR`, `IMPORT`, `IAKTAR` | Dosya | tek işlem | etkileşimli, betiklenebilir | Dış bir veri dosyasını çizime ekler. |
| [`core.export`](export.md) | `DIŞAAKTAR`, `DISAAKTAR`, `EXPORT`, `DAKTAR` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Çizimi dış bir veri biçimine yazar. |
| [`core.script`](script.md) | `BETİK`, `BETIK`, `SCRIPT` | Betik | komuta özel | etkileşimli, betiklenebilir, salt okunur | Bir betik dosyasını komut veri yolu üzerinden çalıştırır. |
| [`core.database`](database.md) | `VERİTABANI`, `VERITABANI`, `DATABASE`, `VT` | Dosya | geri alınmaz | etkileşimli, betiklenebilir | PostGIS veritabanına bağlanır; katmanları tablo, projeleri kayıt olarak yazar. |
| [`core.setting`](setting.md) | `AYAR`, `SETTING`, `AY` | Sistem | tek işlem | betiklenebilir | Proje ayarlarını listeler, okur ve değiştirir. |
| [`core.preference`](preference.md) | `TERCİH`, `TERCIH`, `PREFERENCE`, `PREF` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Uygulama tercihlerini listeler, okur ve değiştirir. |
| [`core.mode`](mode.md) | `MOD`, `MODE`, `MD` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur ve değiştirir. |
| [`core.help`](help.md) | `YARDIM`, `HELP`, `?` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Komut listesini veya tek bir komutun ayrıntısını gösterir. |

## Parametreler

### `core.line` — ÇİZGİ

İki veya daha fazla nokta arasında doğru parçaları çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 2 | Ardışık doğru parçalarının köşe noktaları |

Ayrıntılı kullanım: [ÇİZGİ](line.md)

### `core.polyline` — ÇOKLUÇİZGİ

Birden çok noktadan TEK bir çizgi nesnesi çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 2 | Çoklu çizginin köşe noktaları; hepsi tek nesne olur |

Ayrıntılı kullanım: [ÇOKLUÇİZGİ](polyline.md)

### `core.point_draw` — NOKTA

Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 1 | Yerleştirilecek noktalar |

Ayrıntılı kullanım: [NOKTA](point_draw.md)

### `core.text` — METİN

Çizime metin yazar; yükseklik ve hizalama verilebilir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point | 1 | Yazının başlangıç noktası |
| `yazi` | text | 1 | Yazılacak metin |
| `yukseklik` | integer | isteğe bağlı | Yazı yüksekliği, zeminde milimetre; yoksa proje ayarı |
| `bitis` | point_list | isteğe bağlı | Taban çizgisinin bitişi; yoksa yatay |
| `hizalama` | text | isteğe bağlı | sol, orta, sag veya merkez |

Ayrıntılı kullanım: [METİN](text.md)

### `core.exportstyle` — STİLAKTAR

Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `katman` | text | 1 | Stili aktarılacak katmanın adı |
| `dosya` | text | 1 | Yazılacak .qml dosyasının yolu |

Ayrıntılı kullanım: [STİLAKTAR](exportstyle.md)

### `core.area` — ALAN

Kapalı bir alan çizer; istenirse içine delik açar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 3 | Alanın köşe noktaları; kapanış noktası tekrarlanmaz |
| `bolum` | integer | en az 0 | Halka uzunlukları: ilki dış sınır, sonrakiler delik |

Ayrıntılı kullanım: [ALAN](area.md)

### `core.rectangle` — DİKDÖRTGEN

Karşılıklı iki köşeden dört köşeli kapalı bir alan çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | 2 | Karşılıklı iki köşe; kalan ikisi bunlardan türetilir |

Ayrıntılı kullanım: [DİKDÖRTGEN](rectangle.md)

### `core.circle_draw` — DAİRE

Merkez ve çember üzerindeki bir noktadan daire çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | point | 1 | Dairenin merkezi |
| `cevre` | point | 1 | Çember üzerinde bir nokta; yarıçapı bu belirler |

Ayrıntılı kullanım: [DAİRE](circle_draw.md)

### `core.arc_draw` — YAY

Merkez ve iki uçtan yay çizer; süpürme saat yönünün tersinedir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | point | 1 | Yayın merkezi |
| `baslangic` | point | 1 | Yayın başlangıç noktası; yarıçapı bu belirler |
| `bitis` | point | 1 | Yayın bitiş yönü; süpürme saat yönünün tersinedir |

Ayrıntılı kullanım: [YAY](arc_draw.md)

### `core.vertex_move` — KÖŞETAŞI

Bir nesnenin köşesini yeni bir yere taşır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | 1 | Köşesi taşınacak nesnenin kimliği |
| `kose` | integer | 1 | Taşınacak köşenin sırası; ilk köşe 1'dir |
| `nokta` | point | 1 | Köşenin yeni yeri |

Ayrıntılı kullanım: [KÖŞETAŞI](vertex_move.md)

### `core.vertex_insert` — KÖŞEEKLE

Bir kenarın ortasına yeni köşe ekler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | 1 | Köşe eklenecek nesnenin kimliği |
| `kose` | integer | 1 | Yeni köşenin ardına geleceği köşe; ilk köşe 1'dir |
| `nokta` | point | 1 | Yeni köşenin yeri |

Ayrıntılı kullanım: [KÖŞEEKLE](vertex_insert.md)

### `core.to_area` — ALANAÇEVİR

Uç uca değen çizgileri tek bir kapalı alana çevirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Birleştirilecek çizgilerin kimlikleri; yoksa etkin seçim |

Ayrıntılı kullanım: [ALANAÇEVİR](to_area.md)

### `core.move` — TAŞI

Seçilen nesneleri iki nokta arasındaki kadar taşır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Taşınacak nesnelerin kimlikleri; yoksa etkin seçim |
| `baslangic` | point | 1 | Taşımanın başlangıç noktası |
| `bitis` | point | 1 | Taşımanın bitiş noktası |

Ayrıntılı kullanım: [TAŞI](move.md)

### `core.copy` — KOPYALA

Seçilen nesnelerin kopyasını iki nokta arasındaki kadar öteye koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Kopyalanacak nesnelerin kimlikleri; yoksa etkin seçim |
| `baslangic` | point | 1 | Kopyalamanın başlangıç noktası |
| `bitis` | point | 1 | Kopyanın geleceği nokta |

Ayrıntılı kullanım: [KOPYALA](copy.md)

### `core.array` — DİZİ

Seçilen nesneleri satır/sütun ya da bir merkez etrafında çoğaltır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Dizilecek nesnelerin kimlikleri; yoksa etkin seçim |
| `mod` | text | isteğe bağlı | KUTUPSAL için kutupsal dizi; verilmezse satır/sütun dizisi |
| `satir` | integer | isteğe bağlı | Satır sayısı (dikdörtgen dizi) |
| `sutun` | integer | isteğe bağlı | Sütun sayısı (dikdörtgen dizi) |
| `satir_aralik` | number | isteğe bağlı | Satır aralığı, metre; kuzeye artı |
| `sutun_aralik` | number | isteğe bağlı | Sütun aralığı, metre; doğuya artı |
| `merkez` | point | isteğe bağlı | Dizinin merkezi (kutupsal dizi) |
| `sayi` | integer | isteğe bağlı | Toplam kopya sayısı, özgün dahil (kutupsal dizi) |
| `aci` | number | isteğe bağlı | Süpürülecek toplam açı, derece; verilmezse tam tur |

Ayrıntılı kullanım: [DİZİ](array.md)

### `core.combine` — BİRLEŞTİR

Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek çizgi yapar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Birleştirilecek alanlar ya da çizgiler; yoksa etkin seçim |

Ayrıntılı kullanım: [BİRLEŞTİR](combine.md)

### `core.split` — BÖL

Bir çizgiyi verilen noktadan ikiye böler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | isteğe bağlı | Bölünecek çizginin kimliği; yoksa etkin seçim |
| `nokta` | point | 1 | Bölme noktası |

Ayrıntılı kullanım: [BÖL](split.md)

### `core.trim` — BUDA

Bir çizgiyi kestiği sınır çizgisine kadar budar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | isteğe bağlı | Budanacak çizginin kimliği; yoksa seçili iki çizgiden tıklanan |
| `sinir` | selection | isteğe bağlı | Sınır çizgisinin kimliği; yoksa seçili iki çizgiden diğeri |
| `nokta` | point | 1 | Atılacak parçanın üzerindeki bir nokta |

Ayrıntılı kullanım: [BUDA](trim.md)

### `core.extend` — UZAT

Bir çizgiyi sınır çizgisine ulaşana kadar uzatır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | isteğe bağlı | Uzatılacak çizginin kimliği; yoksa seçili iki çizgiden tıklanan |
| `sinir` | selection | isteğe bağlı | Sınır çizgisinin kimliği; yoksa seçili iki çizgiden diğeri |
| `nokta` | point | 1 | Uzatılacak ucun yakınında bir nokta |

Ayrıntılı kullanım: [UZAT](extend.md)

### `core.chamfer` — PAH

Bir köşeyi düz bir kenarla keser (pah kırar).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | 1 | Köşesi kesilecek nesnenin kimliği |
| `nokta` | point | 1 | İşlem yapılacak köşe |
| `mesafe` | number | 1 | Köşeden her iki kenar boyunca kesilecek mesafe, metre |

Ayrıntılı kullanım: [PAH](chamfer.md)

### `core.fillet` — YUVARLA

Bir köşeyi verilen yarıçapta yay ile yuvarlatır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | 1 | Köşesi yuvarlatılacak nesnenin kimliği |
| `nokta` | point | 1 | İşlem yapılacak köşe |
| `yaricap` | number | 1 | Yuvarlatma yarıçapı, metre |

Ayrıntılı kullanım: [YUVARLA](fillet.md)

### `core.set_layer` — KATMANAT

Seçilen nesneleri başka bir katmana taşır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Taşınacak nesnelerin kimlikleri; yoksa etkin seçim |
| `katman` | text | 1 | Hedef katmanın adı; yoksa oluşturulur |

Ayrıntılı kullanım: [KATMANAT](set_layer.md)

### `core.match_style` — STİLKOPYALA

Bir nesnenin stilini seçilen nesnelere uygular.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `kaynak` | selection | isteğe bağlı | Stili kopyalanacak nesnenin kimliği; yoksa tıklanan nesne |
| `nesneler` | selection | en az 0 | Stili alacak nesnelerin kimlikleri; yoksa etkin seçim |
| `nokta` | point | isteğe bağlı | Kaynak nesnenin üzerinde bir nokta; yalnız kaynak verilmediğinde |

Ayrıntılı kullanım: [STİLKOPYALA](match_style.md)

### `core.rotate` — DÖNDÜR

Seçilen nesneleri bir merkez etrafında döndürür.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Döndürülecek nesnelerin kimlikleri; yoksa etkin seçim |
| `merkez` | point | 1 | Döndürme merkezi |
| `aci` | number | 1 | Dönme açısı, derece; artı yön saat yönünün tersi |

Ayrıntılı kullanım: [DÖNDÜR](rotate.md)

### `core.scale` — ÖLÇEKLE

Seçilen nesneleri bir merkeze göre büyütür ya da küçültür.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Ölçeklenecek nesnelerin kimlikleri; yoksa etkin seçim |
| `merkez` | point | 1 | Ölçekleme merkezi; bu nokta yerinde kalır |
| `carpan` | number | 1 | Ölçek çarpanı; sıfırdan büyük |

Ayrıntılı kullanım: [ÖLÇEKLE](scale.md)

### `core.mirror` — AYNALA

Seçilen nesneleri iki noktadan geçen eksende aynalar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Aynalanacak nesnelerin kimlikleri; yoksa etkin seçim |
| `baslangic` | point | 1 | Ayna ekseninin ilk noktası |
| `bitis` | point | 1 | Ayna ekseninin ikinci noktası |

Ayrıntılı kullanım: [AYNALA](mirror.md)

### `core.measure` — ÖLÇ

İki nokta arasındaki mesafeyi, koordinat farkını ve açıyı yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `baslangic` | point | 1 | Ölçümün ilk noktası |
| `bitis` | point | 1 | Ölçümün ikinci noktası |

Ayrıntılı kullanım: [ÖLÇ](measure.md)

### `core.measure_area` — ALANÖLÇ

Seçilen nesnelerin alanını ve çevresini yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Ölçülecek nesnelerin kimlikleri; yoksa etkin seçim |

Ayrıntılı kullanım: [ALANÖLÇ](measure_area.md)

### `core.coordinate` — KOORDİNAT

Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nokta` | point | 1 | Okunacak nokta |

Ayrıntılı kullanım: [KOORDİNAT](coordinate.md)

### `core.pan` — KAYDIR

Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `baslangic` | point | 1 | Kaydırmanın tutulacağı nokta |
| `bitis` | point | 1 | O noktanın taşınacağı yer |

Ayrıntılı kullanım: [KAYDIR](pan.md)

### `core.offset` — OFSET

Seçili nesnelerin verilen mesafede paralelini çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Ofseti alınacak nesneler; yoksa etkin seçim |
| `mesafe` | integer | isteğe bağlı | Ofset mesafesi, milimetre; eksi değer içeri |
| `kose` | text | isteğe bağlı | KÖŞE | YUVARLAK | PAH — dış köşenin biçimi |

Ayrıntılı kullanım: [OFSET](offset.md)

### `core.sector` — DİLİM

Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | point | 1 | Dilimin merkezi |
| `baslangic` | point | 1 | İlk kenarın ucu; yarıçapı bu belirler |
| `bitis` | point | 1 | İkinci kenarın yönü; süpürme saat yönünün tersinedir |

Ayrıntılı kullanım: [DİLİM](sector.md)

### `core.annulus` — HALKA

Merkez, iç ve dış yarıçaptan delikli halka çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | point | 1 | Halkanın merkezi |
| `ic` | point | 1 | İç çember üzerinde bir nokta |
| `dis` | point | 1 | Dış çember üzerinde bir nokta |

Ayrıntılı kullanım: [HALKA](annulus.md)

### `core.ellipse_draw` — ELİPS

Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | point | 1 | Elipsin merkezi |
| `birinci` | point | 1 | Birinci eksenin ucu |
| `ikinci` | point | 1 | İkinci eksenin uzaklığı; eksene dik ölçülür |

Ayrıntılı kullanım: [ELİPS](ellipse_draw.md)

### `core.points` — NOKTALAR

Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Nokta listesi dosyasının yolu |
| `yon` | text | isteğe bağlı | oku (varsayılan) | yaz |
| `eksen` | text | isteğe bağlı | Sütun sırası: YX (varsayılan, Türkiye'de olağan) | XY |

Ayrıntılı kullanım: [NOKTALAR](points.md)

### `core.guide` — KILAVUZ

Cetvel kılavuzu ekler, listeler ve siler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `yon` | text | isteğe bağlı | yatay | düşey; yoksa kılavuzlar listelenir |
| `deger` | integer | isteğe bağlı | Kılavuzun koordinatı, milimetre — yatayda yukarı, düşeyde sağa |
| `sil` | bool | isteğe bağlı | Verilen yerdeki kılavuzu siler |

Ayrıntılı kullanım: [KILAVUZ](guide.md)

### `core.attribute` — ÖZNİTELİK

Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Öznitelik kimliği; yoksa tanımlı sütunlar listelenir |
| `nesne` | integer | isteğe bağlı | Nesnenin kalıcı kimliği |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur. 'yok' hücreyi boşaltır |

Ayrıntılı kullanım: [ÖZNİTELİK](attribute.md)

### `core.column` — SÜTUN

Belgeye öznitelik sütunu tanımlar ve tanımlı sütunları listeler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `kimlik` | text | isteğe bağlı | Sütun kimliği; yoksa tanımlı sütunlar listelenir |
| `tur` | text | isteğe bağlı | tam_sayi, uzunluk, evet_hayir veya metin |

Ayrıntılı kullanım: [SÜTUN](column.md)

### `core.erase` — SİL

Seçilen nesneleri siler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Silinecek nesnelerin kimlikleri; yoksa etkin seçim |

Ayrıntılı kullanım: [SİL](erase.md)

### `core.select` — SEÇ

Nesneleri seçer: tümü, kimlikle, pencere, kesen kutu veya tek nokta.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `mod` | text | isteğe bağlı | TÜMÜ | TEMİZLE | NESNE | PENCERE | KESEN | KUTU | NOKTA |
| `noktalar` | point_list | 0–2 | Kutu köşeleri (iki nokta) veya tek tıklama noktası |
| `nesneler` | selection | en az 0 | NESNE modunda nesne kimlikleri |
| `islem` | text | isteğe bağlı | DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE |
| `tolerans` | number | isteğe bağlı | NOKTA modunda arama yarıçapı, metre; yoksa seçim toleransı |

Ayrıntılı kullanım: [SEÇ](select.md)

### `core.label` — ETİKET

Katmandaki nesneleri özniteliklerinden okuyarak etiketler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `katman` | text | 1 | Etiketlenecek katmanın adı |
| `bicim` | text | 1 | Etiket biçimi; {sutun} o sütunun değeriyle değişir, \n satır kırar |
| `hedef` | text | isteğe bağlı | Etiketlerin yazılacağı katman; yoksa '<katman> ETİKET' |
| `yukseklik` | integer | isteğe bağlı | Yazı yüksekliği, zemin milimetresi |
| `kaydirma` | integer | isteğe bağlı | Nesnenin ortasından dikey kaydırma, zemin milimetresi; artı yukarı |

Ayrıntılı kullanım: [ETİKET](label.md)

### `core.layer` — KATMAN

Katman oluşturur, aktif yapar ve özelliklerini değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | 1 | Katman adı; yoksa oluşturulur ve aktif yapılır |
| `grup` | text | isteğe bağlı | Katman ağacındaki yer, düzeyler '>' ile ayrılır; boş = kök |
| `gorunur` | bool | isteğe bağlı | Katmanın görünürlüğü |
| `kilitli` | bool | isteğe bağlı | Katmanın kilit durumu |
| `renk` | integer | isteğe bağlı | Çizim rengi, 0xAARRGGBB |

Ayrıntılı kullanım: [KATMAN](layer.md)

### `core.style` — STİL

Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `katman` | text | 1 | Stilin yazılacağı katmanın adı; katman var olmalı |
| `paket` | text | isteğe bağlı | Stil kataloğu paketinin dosya yolu |
| `olcek_min` | integer | isteğe bağlı | Bu ölçek paydasından daha yakında çizilmez (1:N'deki N) |
| `olcek_max` | integer | isteğe bağlı | Bu ölçek paydasından daha uzakta çizilmez |
| `sinifla` | text | isteğe bağlı | Sınıflandırmada kullanılacak öznitelik; her nesne kendi değerine göre stillenir |
| `kod` | text | isteğe bağlı | Katalogdaki satırın kimliği; verilmezse katalog kuralları eşleşir |
| `olcek` | integer | isteğe bağlı | Ölçek paydası (1:N); 0 = ölçekten bağımsız |
| `renk` | integer | isteğe bağlı | Çizgi rengi, 0xAARRGGBB |
| `kalinlik` | integer | isteğe bağlı | Çizgi kalınlığı, kâğıt mikrometresi (1000 = 1 mm) |
| `dolgu` | integer | isteğe bağlı | Dolgu rengi, 0xAARRGGBB; 0 = dolgusuz |
| `sira` | integer | isteğe bağlı | Çizim sırası; büyük olan üste gelir |
| `sifirla` | bool | isteğe bağlı | Stili siler; nesneler katman varsayılanına döner |
| `tip` | text | isteğe bağlı | Sembol katmanı tipi: cizgi, isaretci-cizgi, tarak-cizgi, dolgu, cizgi-desen-dolgu, nokta-desen-dolgu, merkez-isaretci, isaretci |
| `ekle` | bool | isteğe bağlı | Katmanı mevcut sembolün üstüne ekler; yoksa sembolü değiştirir |
| `sekil` | text | isteğe bağlı | İşaretçi şekli: daire, kare, ucgen, baklava, yildiz, arti, carpi, ok, yarim-daire, besgen, altigen, cizik |
| `yerlesim` | text | isteğe bağlı | İşaretçinin çizgi üzerindeki yeri: aralik, tepe, ilk, son, orta |
| `birim` | text | isteğe bağlı | Ölçülerin birimi: kagit (µm), zemin (mm), piksel |
| `boyut_birim` | text | isteğe bağlı | Yalnız `boyut` için birim; verilmezse `birim` geçerlidir |
| `aralik_birim` | text | isteğe bağlı | Yalnız `aralik` için birim; verilmezse `birim` geçerlidir |
| `aralik_y_birim` | text | isteğe bağlı | Yalnız `aralik_y` için birim; verilmezse `birim` geçerlidir |
| `kaydirma_birim` | text | isteğe bağlı | Yalnız `kaydirma` için birim; verilmezse `birim` geçerlidir |
| `boyut` | integer | isteğe bağlı | İşaretçi çapı ya da tarak dişinin boyu, `birim` cinsinden |
| `aralik` | integer | isteğe bağlı | Çizgi boyunca ya da desende birinci eksende aralık |
| `aralik_y` | integer | isteğe bağlı | Nokta deseninde ikinci eksen; verilmezse kare desen |
| `aci` | integer | isteğe bağlı | Desen açısı ya da işaretçi dönüklüğü, mikro derece |
| `kaydirma` | integer | isteğe bağlı | Geometriden dik kaydırma, `birim` cinsinden |
| `faz` | integer | isteğe bağlı | İlk işaretçinin çizgi boyunca kaç birim ileride başlayacağı; verilmezse aralığın yarısı |
| `faz_birim` | text | isteğe bağlı | Yalnız `faz` için birim; verilmezse `birim` geçerlidir |
| `saydamlik` | integer | isteğe bağlı | Katman saydamlığı 0-255; 255 tam opak |
| `desen` | text | isteğe bağlı | Çizgi tipi: sürekli, ya da çizgi kalınlığının katı olarak çizgi/boşluk uzunlukları — '8 1 1 1' gibi (kesik-nokta) |
| `yazi` | text | isteğe bağlı | yazi-isaretci katmanının yazdığı sabit metin |

Ayrıntılı kullanım: [STİL](style.md)

### `core.symbol` — SEMBOL

Gösterim rafını yükler, ağacında gezer ve içinde arar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `paket` | text | isteğe bağlı | Yüklenecek gösterim paketinin dosya yolu |
| `grup` | text | isteğe bağlı | Gezilecek grup yolu, düzeyler '>' ile ayrılır |
| `ara` | text | isteğe bağlı | Etikette, kimlikte ve grup yolunda arar |
| `kod` | text | isteğe bağlı | Tek bir gösterimin ayrıntısı |

Ayrıntılı kullanım: [SEMBOL](symbol.md)

### `core.zoom` — YAKINLAŞ

Görünümü çizim kapsamına veya verilen çarpana ayarlar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `mod` | text | isteğe bağlı | KAPSAM | ÇARPAN | SIFIRLA |
| `carpan` | number | isteğe bağlı | ÇARPAN modunda ölçek katsayısı |

Ayrıntılı kullanım: [YAKINLAŞ](zoom.md)

### `core.undo` — GERİAL

Son işlemi geri alır.

Parametre almaz.

Ayrıntılı kullanım: [GERİAL](undo.md)

### `core.redo` — YİNELE

Geri alınan işlemi yineler.

Parametre almaz.

Ayrıntılı kullanım: [YİNELE](redo.md)

### `core.open` — AÇ

Bir KentOSCad proje dosyasını açar ve çizimin yerine koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Açılacak KentOSCad proje dosyasının yolu (.pcad) |

Ayrıntılı kullanım: [AÇ](open.md)

### `core.save` — KAYDET

Çizimi bağlı olduğu KentOSCad proje dosyasına kaydeder.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | isteğe bağlı | Hedef yol; verilmezse çizimin bağlı olduğu dosyaya yazılır |

Ayrıntılı kullanım: [KAYDET](save.md)

### `core.saveas` — FARKLIKAYDET

Çizimi yeni bir KentOSCad proje dosyasına kaydeder ve ona bağlar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Yeni proje dosyasının yolu (.pcad) |

Ayrıntılı kullanım: [FARKLIKAYDET](saveas.md)

### `core.import` — İÇEAKTAR

Dış bir veri dosyasını çizime ekler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | İçe aktarılacak dosyanın yolu |
| `bicim` | text | isteğe bağlı | Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur |

Ayrıntılı kullanım: [İÇEAKTAR](import.md)

### `core.export` — DIŞAAKTAR

Çizimi dış bir veri biçimine yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Yazılacak dosyanın yolu |
| `bicim` | text | isteğe bağlı | Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur |

Ayrıntılı kullanım: [DIŞAAKTAR](export.md)

### `core.script` — BETİK

Bir betik dosyasını komut veri yolu üzerinden çalıştırır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Çalıştırılacak betik dosyasının yolu |

Ayrıntılı kullanım: [BETİK](script.md)

### `core.database` — VERİTABANI

PostGIS veritabanına bağlanır; katmanları tablo, projeleri kayıt olarak yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | baglan | kes | tablolar | katmanyaz | projekaydet | projeac | projeler | projesil |
| `hedef` | text | isteğe bağlı | baglan: bağlantı dizesi; katmanyaz: tablo adı; proje işlemleri: proje adı |
| `katman` | text | isteğe bağlı | katmanyaz: yazılacak katman; yoksa etkin katman |

Ayrıntılı kullanım: [VERİTABANI](database.md)

### `core.setting` — AYAR

Proje ayarlarını listeler, okur ve değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Ayar adı veya kimliği; yoksa liste |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur |

Ayrıntılı kullanım: [AYAR](setting.md)

### `core.preference` — TERCİH

Uygulama tercihlerini listeler, okur ve değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Tercih adı veya kimliği; yoksa liste |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur |

Ayrıntılı kullanım: [TERCİH](preference.md)

### `core.mode` — MOD

Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur ve değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Mod adı veya kimliği; yoksa liste |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur |

Ayrıntılı kullanım: [MOD](mode.md)

### `core.help` — YARDIM

Komut listesini veya tek bir komutun ayrıntısını gösterir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `komut` | text | isteğe bağlı | Ayrıntısı istenen komut adı |

Ayrıntılı kullanım: [YARDIM](help.md)

## AI araç kataloğu

AI'ın görebildiği komutlar `Flags::AiAccessible` bayrağından üretilir.
Elle tutulan ikinci bir araç şeması yoktur (kentoscad.md §2.3, §5.1).

```json
{
  "version": 1,
  "generated_from": "kentos::command::Registry",
  "tools": [
    {
      "id": "core.line",
      "names": [
        "ÇİZGİ",
        "CIZGI",
        "LINE",
        "Ç",
        "L"
      ],
      "category": "Çizim",
      "summary": "İki veya daha fazla nokta arasında doğru parçaları çizer.",
      "params": [
        {
          "name": "noktalar",
          "type": "point_list",
          "min": 2,
          "max": -1,
          "required": true,
          "help": "Ardışık doğru parçalarının köşe noktaları"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.polyline",
      "names": [
        "ÇOKLUÇİZGİ",
        "COKLUCIZGI",
        "POLYLINE",
        "ÇÇ",
        "PL"
      ],
      "category": "Çizim",
      "summary": "Birden çok noktadan TEK bir çizgi nesnesi çizer.",
      "params": [
        {
          "name": "noktalar",
          "type": "point_list",
          "min": 2,
          "max": -1,
          "required": true,
          "help": "Çoklu çizginin köşe noktaları; hepsi tek nesne olur"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.point_draw",
      "names": [
        "NOKTA",
        "POINT",
        "NK"
      ],
      "category": "Çizim",
      "summary": "Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper.",
      "params": [
        {
          "name": "noktalar",
          "type": "point_list",
          "min": 1,
          "max": -1,
          "required": true,
          "help": "Yerleştirilecek noktalar"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.text",
      "names": [
        "METİN",
        "METIN",
        "YAZI",
        "TEXT",
        "MT"
      ],
      "category": "Çizim",
      "summary": "Çizime metin yazar; yükseklik ve hizalama verilebilir.",
      "params": [
        {
          "name": "noktalar",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Yazının başlangıç noktası"
        },
        {
          "name": "yazi",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Yazılacak metin"
        },
        {
          "name": "yukseklik",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yazı yüksekliği, zeminde milimetre; yoksa proje ayarı"
        },
        {
          "name": "bitis",
          "type": "point_list",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Taban çizgisinin bitişi; yoksa yatay"
        },
        {
          "name": "hizalama",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "sol, orta, sag veya merkez"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.area",
      "names": [
        "ALAN",
        "AREA",
        "POLİGON",
        "POLIGON",
        "AL"
      ],
      "category": "Çizim",
      "summary": "Kapalı bir alan çizer; istenirse içine delik açar.",
      "params": [
        {
          "name": "noktalar",
          "type": "point_list",
          "min": 3,
          "max": -1,
          "required": true,
          "help": "Alanın köşe noktaları; kapanış noktası tekrarlanmaz"
        },
        {
          "name": "bolum",
          "type": "integer",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Halka uzunlukları: ilki dış sınır, sonrakiler delik"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.rectangle",
      "names": [
        "DİKDÖRTGEN",
        "DIKDORTGEN",
        "RECTANGLE",
        "DKD",
        "REC"
      ],
      "category": "Çizim",
      "summary": "Karşılıklı iki köşeden dört köşeli kapalı bir alan çizer.",
      "params": [
        {
          "name": "noktalar",
          "type": "point_list",
          "min": 2,
          "max": 2,
          "required": true,
          "help": "Karşılıklı iki köşe; kalan ikisi bunlardan türetilir"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.circle_draw",
      "names": [
        "DAİRE",
        "DAIRE",
        "CIRCLE",
        "DR"
      ],
      "category": "Çizim",
      "summary": "Merkez ve çember üzerindeki bir noktadan daire çizer.",
      "params": [
        {
          "name": "merkez",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Dairenin merkezi"
        },
        {
          "name": "cevre",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Çember üzerinde bir nokta; yarıçapı bu belirler"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.arc_draw",
      "names": [
        "YAY",
        "ARC",
        "YY"
      ],
      "category": "Çizim",
      "summary": "Merkez ve iki uçtan yay çizer; süpürme saat yönünün tersinedir.",
      "params": [
        {
          "name": "merkez",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Yayın merkezi"
        },
        {
          "name": "baslangic",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Yayın başlangıç noktası; yarıçapı bu belirler"
        },
        {
          "name": "bitis",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Yayın bitiş yönü; süpürme saat yönünün tersinedir"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.vertex_move",
      "names": [
        "KÖŞETAŞI",
        "KOSETASI",
        "MOVEVERTEX",
        "KT"
      ],
      "category": "Düzenleme",
      "summary": "Bir nesnenin köşesini yeni bir yere taşır.",
      "params": [
        {
          "name": "nesne",
          "type": "selection",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Köşesi taşınacak nesnenin kimliği"
        },
        {
          "name": "kose",
          "type": "integer",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Taşınacak köşenin sırası; ilk köşe 1'dir"
        },
        {
          "name": "nokta",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Köşenin yeni yeri"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.vertex_insert",
      "names": [
        "KÖŞEEKLE",
        "KOSEEKLE",
        "ADDVERTEX",
        "KE"
      ],
      "category": "Düzenleme",
      "summary": "Bir kenarın ortasına yeni köşe ekler.",
      "params": [
        {
          "name": "nesne",
          "type": "selection",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Köşe eklenecek nesnenin kimliği"
        },
        {
          "name": "kose",
          "type": "integer",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Yeni köşenin ardına geleceği köşe; ilk köşe 1'dir"
        },
        {
          "name": "nokta",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Yeni köşenin yeri"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.to_area",
      "names": [
        "ALANAÇEVİR",
        "ALANACEVIR",
        "TOAREA",
        "ALÇ"
      ],
      "category": "Düzenleme",
      "summary": "Uç uca değen çizgileri tek bir kapalı alana çevirir.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Birleştirilecek çizgilerin kimlikleri; yoksa etkin seçim"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.move",
      "names": [
        "TAŞI",
        "TASI",
        "MOVE",
        "TŞ"
      ],
      "category": "Düzenleme",
      "summary": "Seçilen nesneleri iki nokta arasındaki kadar taşır.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Taşınacak nesnelerin kimlikleri; yoksa etkin seçim"
        },
        {
          "name": "baslangic",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Taşımanın başlangıç noktası"
        },
        {
          "name": "bitis",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Taşımanın bitiş noktası"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.copy",
      "names": [
        "KOPYALA",
        "COPY",
        "KP"
      ],
      "category": "Düzenleme",
      "summary": "Seçilen nesnelerin kopyasını iki nokta arasındaki kadar öteye koyar.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Kopyalanacak nesnelerin kimlikleri; yoksa etkin seçim"
        },
        {
          "name": "baslangic",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Kopyalamanın başlangıç noktası"
        },
        {
          "name": "bitis",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Kopyanın geleceği nokta"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.array",
      "names": [
        "DİZİ",
        "DIZI",
        "ARRAY",
        "DZ"
      ],
      "category": "Düzenleme",
      "summary": "Seçilen nesneleri satır/sütun ya da bir merkez etrafında çoğaltır.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Dizilecek nesnelerin kimlikleri; yoksa etkin seçim"
        },
        {
          "name": "mod",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "KUTUPSAL için kutupsal dizi; verilmezse satır/sütun dizisi"
        },
        {
          "name": "satir",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Satır sayısı (dikdörtgen dizi)"
        },
        {
          "name": "sutun",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Sütun sayısı (dikdörtgen dizi)"
        },
        {
          "name": "satir_aralik",
          "type": "number",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Satır aralığı, metre; kuzeye artı"
        },
        {
          "name": "sutun_aralik",
          "type": "number",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Sütun aralığı, metre; doğuya artı"
        },
        {
          "name": "merkez",
          "type": "point",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Dizinin merkezi (kutupsal dizi)"
        },
        {
          "name": "sayi",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Toplam kopya sayısı, özgün dahil (kutupsal dizi)"
        },
        {
          "name": "aci",
          "type": "number",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Süpürülecek toplam açı, derece; verilmezse tam tur"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.combine",
      "names": [
        "BİRLEŞTİR",
        "BIRLESTIR",
        "COMBINE",
        "BRL"
      ],
      "category": "Düzenleme",
      "summary": "Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek çizgi yapar.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Birleştirilecek alanlar ya da çizgiler; yoksa etkin seçim"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.split",
      "names": [
        "BÖL",
        "BOL",
        "SPLIT",
        "BL"
      ],
      "category": "Düzenleme",
      "summary": "Bir çizgiyi verilen noktadan ikiye böler.",
      "params": [
        {
          "name": "nesne",
          "type": "selection",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Bölünecek çizginin kimliği; yoksa etkin seçim"
        },
        {
          "name": "nokta",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Bölme noktası"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.trim",
      "names": [
        "BUDA",
        "TRIM",
        "BD"
      ],
      "category": "Düzenleme",
      "summary": "Bir çizgiyi kestiği sınır çizgisine kadar budar.",
      "params": [
        {
          "name": "nesne",
          "type": "selection",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Budanacak çizginin kimliği; yoksa seçili iki çizgiden tıklanan"
        },
        {
          "name": "sinir",
          "type": "selection",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Sınır çizgisinin kimliği; yoksa seçili iki çizgiden diğeri"
        },
        {
          "name": "nokta",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Atılacak parçanın üzerindeki bir nokta"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.extend",
      "names": [
        "UZAT",
        "EXTEND",
        "UZ"
      ],
      "category": "Düzenleme",
      "summary": "Bir çizgiyi sınır çizgisine ulaşana kadar uzatır.",
      "params": [
        {
          "name": "nesne",
          "type": "selection",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Uzatılacak çizginin kimliği; yoksa seçili iki çizgiden tıklanan"
        },
        {
          "name": "sinir",
          "type": "selection",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Sınır çizgisinin kimliği; yoksa seçili iki çizgiden diğeri"
        },
        {
          "name": "nokta",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Uzatılacak ucun yakınında bir nokta"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.chamfer",
      "names": [
        "PAH",
        "CHAMFER",
        "PH"
      ],
      "category": "Düzenleme",
      "summary": "Bir köşeyi düz bir kenarla keser (pah kırar).",
      "params": [
        {
          "name": "nesne",
          "type": "selection",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Köşesi kesilecek nesnenin kimliği"
        },
        {
          "name": "nokta",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "İşlem yapılacak köşe"
        },
        {
          "name": "mesafe",
          "type": "number",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Köşeden her iki kenar boyunca kesilecek mesafe, metre"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.fillet",
      "names": [
        "YUVARLA",
        "FILLET",
        "YV"
      ],
      "category": "Düzenleme",
      "summary": "Bir köşeyi verilen yarıçapta yay ile yuvarlatır.",
      "params": [
        {
          "name": "nesne",
          "type": "selection",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Köşesi yuvarlatılacak nesnenin kimliği"
        },
        {
          "name": "nokta",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "İşlem yapılacak köşe"
        },
        {
          "name": "yaricap",
          "type": "number",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Yuvarlatma yarıçapı, metre"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.set_layer",
      "names": [
        "KATMANAT",
        "KATMANATA",
        "SETLAYER",
        "KA"
      ],
      "category": "Düzenleme",
      "summary": "Seçilen nesneleri başka bir katmana taşır.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Taşınacak nesnelerin kimlikleri; yoksa etkin seçim"
        },
        {
          "name": "katman",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Hedef katmanın adı; yoksa oluşturulur"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.match_style",
      "names": [
        "STİLKOPYALA",
        "STILKOPYALA",
        "MATCHPROP",
        "SK"
      ],
      "category": "Düzenleme",
      "summary": "Bir nesnenin stilini seçilen nesnelere uygular.",
      "params": [
        {
          "name": "kaynak",
          "type": "selection",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Stili kopyalanacak nesnenin kimliği; yoksa tıklanan nesne"
        },
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Stili alacak nesnelerin kimlikleri; yoksa etkin seçim"
        },
        {
          "name": "nokta",
          "type": "point",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Kaynak nesnenin üzerinde bir nokta; yalnız kaynak verilmediğinde"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.rotate",
      "names": [
        "DÖNDÜR",
        "DONDUR",
        "ROTATE",
        "DÖN"
      ],
      "category": "Düzenleme",
      "summary": "Seçilen nesneleri bir merkez etrafında döndürür.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Döndürülecek nesnelerin kimlikleri; yoksa etkin seçim"
        },
        {
          "name": "merkez",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Döndürme merkezi"
        },
        {
          "name": "aci",
          "type": "number",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Dönme açısı, derece; artı yön saat yönünün tersi"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.scale",
      "names": [
        "ÖLÇEKLE",
        "OLCEKLE",
        "SCALE",
        "ÖLÇEK",
        "OLCEK"
      ],
      "category": "Düzenleme",
      "summary": "Seçilen nesneleri bir merkeze göre büyütür ya da küçültür.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Ölçeklenecek nesnelerin kimlikleri; yoksa etkin seçim"
        },
        {
          "name": "merkez",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Ölçekleme merkezi; bu nokta yerinde kalır"
        },
        {
          "name": "carpan",
          "type": "number",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Ölçek çarpanı; sıfırdan büyük"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.mirror",
      "names": [
        "AYNALA",
        "MIRROR",
        "AYN"
      ],
      "category": "Düzenleme",
      "summary": "Seçilen nesneleri iki noktadan geçen eksende aynalar.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Aynalanacak nesnelerin kimlikleri; yoksa etkin seçim"
        },
        {
          "name": "baslangic",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Ayna ekseninin ilk noktası"
        },
        {
          "name": "bitis",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Ayna ekseninin ikinci noktası"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.measure",
      "names": [
        "ÖLÇ",
        "OLC",
        "MEASURE",
        "MS"
      ],
      "category": "Sorgu",
      "summary": "İki nokta arasındaki mesafeyi, koordinat farkını ve açıyı yazar.",
      "params": [
        {
          "name": "baslangic",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Ölçümün ilk noktası"
        },
        {
          "name": "bitis",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Ölçümün ikinci noktası"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible",
        "read_only"
      ],
      "undo": "none"
    },
    {
      "id": "core.measure_area",
      "names": [
        "ALANÖLÇ",
        "ALANOLC",
        "AREAOF",
        "AÖ"
      ],
      "category": "Sorgu",
      "summary": "Seçilen nesnelerin alanını ve çevresini yazar.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Ölçülecek nesnelerin kimlikleri; yoksa etkin seçim"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible",
        "read_only"
      ],
      "undo": "none"
    },
    {
      "id": "core.coordinate",
      "names": [
        "KOORDİNAT",
        "KOORDINAT",
        "COORDINATE",
        "KRD"
      ],
      "category": "Sorgu",
      "summary": "Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar.",
      "params": [
        {
          "name": "nokta",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Okunacak nokta"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible",
        "read_only"
      ],
      "undo": "none"
    },
    {
      "id": "core.pan",
      "names": [
        "KAYDIR",
        "PAN",
        "KY"
      ],
      "category": "Görünüm",
      "summary": "Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır.",
      "params": [
        {
          "name": "baslangic",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Kaydırmanın tutulacağı nokta"
        },
        {
          "name": "bitis",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "O noktanın taşınacağı yer"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible",
        "transparent",
        "read_only"
      ],
      "undo": "none"
    },
    {
      "id": "core.offset",
      "names": [
        "OFSET",
        "OFFSET",
        "OF"
      ],
      "category": "Düzenleme",
      "summary": "Seçili nesnelerin verilen mesafede paralelini çizer.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Ofseti alınacak nesneler; yoksa etkin seçim"
        },
        {
          "name": "mesafe",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Ofset mesafesi, milimetre; eksi değer içeri"
        },
        {
          "name": "kose",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "KÖŞE | YUVARLAK | PAH — dış köşenin biçimi"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.sector",
      "names": [
        "DİLİM",
        "DILIM",
        "SECTOR",
        "DL"
      ],
      "category": "Çizim",
      "summary": "Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir.",
      "params": [
        {
          "name": "merkez",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Dilimin merkezi"
        },
        {
          "name": "baslangic",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "İlk kenarın ucu; yarıçapı bu belirler"
        },
        {
          "name": "bitis",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "İkinci kenarın yönü; süpürme saat yönünün tersinedir"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.annulus",
      "names": [
        "HALKA",
        "ANNULUS",
        "HLK"
      ],
      "category": "Çizim",
      "summary": "Merkez, iç ve dış yarıçaptan delikli halka çizer.",
      "params": [
        {
          "name": "merkez",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Halkanın merkezi"
        },
        {
          "name": "ic",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "İç çember üzerinde bir nokta"
        },
        {
          "name": "dis",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Dış çember üzerinde bir nokta"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.ellipse_draw",
      "names": [
        "ELİPS",
        "ELIPS",
        "ELLIPSE",
        "EL"
      ],
      "category": "Çizim",
      "summary": "Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir.",
      "params": [
        {
          "name": "merkez",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Elipsin merkezi"
        },
        {
          "name": "birinci",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Birinci eksenin ucu"
        },
        {
          "name": "ikinci",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "İkinci eksenin uzaklığı; eksene dik ölçülür"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.points",
      "names": [
        "NOKTALAR",
        "POINTS",
        "NKL"
      ],
      "category": "Dosya",
      "summary": "Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod).",
      "params": [
        {
          "name": "dosya",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Nokta listesi dosyasının yolu"
        },
        {
          "name": "yon",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "oku (varsayılan) | yaz"
        },
        {
          "name": "eksen",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Sütun sırası: YX (varsayılan, Türkiye'de olağan) | XY"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.guide",
      "names": [
        "KILAVUZ",
        "GUIDE",
        "KLV"
      ],
      "category": "Çizim",
      "summary": "Cetvel kılavuzu ekler, listeler ve siler.",
      "params": [
        {
          "name": "yon",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "yatay | düşey; yoksa kılavuzlar listelenir"
        },
        {
          "name": "deger",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Kılavuzun koordinatı, milimetre — yatayda yukarı, düşeyde sağa"
        },
        {
          "name": "sil",
          "type": "bool",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Verilen yerdeki kılavuzu siler"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.attribute",
      "names": [
        "ÖZNİTELİK",
        "OZNITELIK",
        "ATTRIBUTE",
        "ÖZN",
        "OZN"
      ],
      "category": "Düzenleme",
      "summary": "Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar.",
      "params": [
        {
          "name": "ad",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Öznitelik kimliği; yoksa tanımlı sütunlar listelenir"
        },
        {
          "name": "nesne",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Nesnenin kalıcı kimliği"
        },
        {
          "name": "deger",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yeni değer; yoksa yalnızca okur. 'yok' hücreyi boşaltır"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.erase",
      "names": [
        "SİL",
        "SIL",
        "ERASE",
        "E"
      ],
      "category": "Düzenleme",
      "summary": "Seçilen nesneleri siler.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Silinecek nesnelerin kimlikleri; yoksa etkin seçim"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.label",
      "names": [
        "ETİKET",
        "ETIKET",
        "LABEL",
        "ETK"
      ],
      "category": "Çizim",
      "summary": "Katmandaki nesneleri özniteliklerinden okuyarak etiketler.",
      "params": [
        {
          "name": "katman",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Etiketlenecek katmanın adı"
        },
        {
          "name": "bicim",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Etiket biçimi; {sutun} o sütunun değeriyle değişir, \\n satır kırar"
        },
        {
          "name": "hedef",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Etiketlerin yazılacağı katman; yoksa '<katman> ETİKET'"
        },
        {
          "name": "yukseklik",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yazı yüksekliği, zemin milimetresi"
        },
        {
          "name": "kaydirma",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Nesnenin ortasından dikey kaydırma, zemin milimetresi; artı yukarı"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.layer",
      "names": [
        "KATMAN",
        "LAYER",
        "KAT"
      ],
      "category": "Katman",
      "summary": "Katman oluşturur, aktif yapar ve özelliklerini değiştirir.",
      "params": [
        {
          "name": "ad",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Katman adı; yoksa oluşturulur ve aktif yapılır"
        },
        {
          "name": "grup",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katman ağacındaki yer, düzeyler '>' ile ayrılır; boş = kök"
        },
        {
          "name": "gorunur",
          "type": "bool",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katmanın görünürlüğü"
        },
        {
          "name": "kilitli",
          "type": "bool",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katmanın kilit durumu"
        },
        {
          "name": "renk",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizim rengi, 0xAARRGGBB"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.style",
      "names": [
        "STİL",
        "STIL",
        "STYLE",
        "ST"
      ],
      "category": "Katman",
      "summary": "Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar.",
      "params": [
        {
          "name": "katman",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Stilin yazılacağı katmanın adı; katman var olmalı"
        },
        {
          "name": "paket",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Stil kataloğu paketinin dosya yolu"
        },
        {
          "name": "olcek_min",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Bu ölçek paydasından daha yakında çizilmez (1:N'deki N)"
        },
        {
          "name": "olcek_max",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Bu ölçek paydasından daha uzakta çizilmez"
        },
        {
          "name": "sinifla",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Sınıflandırmada kullanılacak öznitelik; her nesne kendi değerine göre stillenir"
        },
        {
          "name": "kod",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katalogdaki satırın kimliği; verilmezse katalog kuralları eşleşir"
        },
        {
          "name": "olcek",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Ölçek paydası (1:N); 0 = ölçekten bağımsız"
        },
        {
          "name": "renk",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizgi rengi, 0xAARRGGBB"
        },
        {
          "name": "kalinlik",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizgi kalınlığı, kâğıt mikrometresi (1000 = 1 mm)"
        },
        {
          "name": "dolgu",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Dolgu rengi, 0xAARRGGBB; 0 = dolgusuz"
        },
        {
          "name": "sira",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizim sırası; büyük olan üste gelir"
        },
        {
          "name": "sifirla",
          "type": "bool",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Stili siler; nesneler katman varsayılanına döner"
        },
        {
          "name": "tip",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Sembol katmanı tipi: cizgi, isaretci-cizgi, tarak-cizgi, dolgu, cizgi-desen-dolgu, nokta-desen-dolgu, merkez-isaretci, isaretci"
        },
        {
          "name": "ekle",
          "type": "bool",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katmanı mevcut sembolün üstüne ekler; yoksa sembolü değiştirir"
        },
        {
          "name": "sekil",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "İşaretçi şekli: daire, kare, ucgen, baklava, yildiz, arti, carpi, ok, yarim-daire, besgen, altigen, cizik"
        },
        {
          "name": "yerlesim",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "İşaretçinin çizgi üzerindeki yeri: aralik, tepe, ilk, son, orta"
        },
        {
          "name": "birim",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Ölçülerin birimi: kagit (µm), zemin (mm), piksel"
        },
        {
          "name": "boyut_birim",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yalnız `boyut` için birim; verilmezse `birim` geçerlidir"
        },
        {
          "name": "aralik_birim",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yalnız `aralik` için birim; verilmezse `birim` geçerlidir"
        },
        {
          "name": "aralik_y_birim",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yalnız `aralik_y` için birim; verilmezse `birim` geçerlidir"
        },
        {
          "name": "kaydirma_birim",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yalnız `kaydirma` için birim; verilmezse `birim` geçerlidir"
        },
        {
          "name": "boyut",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "İşaretçi çapı ya da tarak dişinin boyu, `birim` cinsinden"
        },
        {
          "name": "aralik",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizgi boyunca ya da desende birinci eksende aralık"
        },
        {
          "name": "aralik_y",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Nokta deseninde ikinci eksen; verilmezse kare desen"
        },
        {
          "name": "aci",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Desen açısı ya da işaretçi dönüklüğü, mikro derece"
        },
        {
          "name": "kaydirma",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Geometriden dik kaydırma, `birim` cinsinden"
        },
        {
          "name": "faz",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "İlk işaretçinin çizgi boyunca kaç birim ileride başlayacağı; verilmezse aralığın yarısı"
        },
        {
          "name": "faz_birim",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yalnız `faz` için birim; verilmezse `birim` geçerlidir"
        },
        {
          "name": "saydamlik",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katman saydamlığı 0-255; 255 tam opak"
        },
        {
          "name": "desen",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizgi tipi: sürekli, ya da çizgi kalınlığının katı olarak çizgi/boşluk uzunlukları — '8 1 1 1' gibi (kesik-nokta)"
        },
        {
          "name": "yazi",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "yazi-isaretci katmanının yazdığı sabit metin"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.symbol",
      "names": [
        "SEMBOL",
        "SEMBOLLER",
        "SYMBOL",
        "SMB"
      ],
      "category": "Katman",
      "summary": "Gösterim rafını yükler, ağacında gezer ve içinde arar.",
      "params": [
        {
          "name": "paket",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yüklenecek gösterim paketinin dosya yolu"
        },
        {
          "name": "grup",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Gezilecek grup yolu, düzeyler '>' ile ayrılır"
        },
        {
          "name": "ara",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Etikette, kimlikte ve grup yolunda arar"
        },
        {
          "name": "kod",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Tek bir gösterimin ayrıntısı"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible"
      ],
      "undo": "none"
    },
    {
      "id": "core.zoom",
      "names": [
        "YAKINLAŞ",
        "YAKINLAS",
        "ZOOM",
        "Z"
      ],
      "category": "Görünüm",
      "summary": "Görünümü çizim kapsamına veya verilen çarpana ayarlar.",
      "params": [
        {
          "name": "mod",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "KAPSAM | ÇARPAN | SIFIRLA"
        },
        {
          "name": "carpan",
          "type": "number",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "ÇARPAN modunda ölçek katsayısı"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible",
        "transparent",
        "read_only"
      ],
      "undo": "none"
    }
  ]
}
```
