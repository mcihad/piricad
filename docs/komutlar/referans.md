<!-- ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN. -->
<!-- Kaynak: kentos::command::Registry.  Yeniden üret: make reference -->
<!-- Bir komutun burada görünmesi için tek yapılması gereken onu kaydetmektir; -->
<!-- projede elle tutulan ikinci bir komut listesi yoktur (CLAUDE.md 5.10). -->

# Komut Referansı

Bu tablo komut kaydından üretilir. Her komutun ayrıntılı kullanım sayfası
`docs/komutlar/` altındadır ve tablodan bağlanır.

| Komut | Adı | Adlar | Kategori | Geri alma | Özellikler | Açıklama |
|---|---|---|---|---|---|---|
| [`core.line`](line.md) | Çizgi | `ÇİZGİ`, `CIZGI`, `LINE`, `Ç`, `L` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | İki veya daha fazla nokta arasında doğru parçaları çizer. |
| [`core.polyline`](polyline.md) | Çoklu Çizgi | `ÇOKLUÇİZGİ`, `COKLUCIZGI`, `POLYLINE`, `ÇÇ`, `PL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Birden çok noktadan TEK bir çizgi nesnesi çizer. |
| [`core.point_draw`](point_draw.md) | Nokta | `NOKTA`, `POINT`, `NK` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper. |
| [`core.perp_offset`](perp_offset.md) | Dik Ayak | `DİKAYAK`, `DIKAYAK`, `PERPOFFSET`, `DA` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Taban çizgisine göre dik ayak ve dik boy vererek nokta yerleştirir. |
| [`core.survey_polar`](survey_polar.md) | Alım | `ALIM`, `SURVEY`, `ALM` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | İstasyondan okunan açı ve kenarlardan nokta hesaplar ve yerleştirir. |
| [`core.intersect_point`](intersect_point.md) | Kesişim Noktası | `KESİŞİMNOKTA`, `KESISIMNOKTA`, `INTERSECTPT`, `KSN` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | İki doğrultunun, iki uzaklığın ya da iki doğrunun kesişimine nokta koyar. |
| [`core.point_along`](point_along.md) | Ara Nokta | `ARANOKTA`, `POINTALONG`, `ARN` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | İki nokta arasındaki doğru üzerinde oran, uzaklık ya da eşit bölmeyle nokta koyar. |
| [`core.polygon_regular`](polygon_regular.md) | Düzgün Çokgen | `ÇOKGEN`, `COKGEN`, `POLYGONREG`, `ÇKG`, `CKG` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez ve kenar sayısından düzgün çokgen çizer: içten, dıştan ya da kenar uzunluğundan. |
| [`core.break`](break.md) | Kır | `KIR`, `BREAK`, `KR` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Çizgiden iki nokta arasındaki parçayı çıkarır; tek nokta verilirse boşluk bırakmadan böler. |
| [`core.join`](join.md) | Uç Uca Ekle | `UÇUCA`, `UCUCA`, `JOIN`, `UÇE` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Uçları birbirine değen çizgileri tek bir çizgiye ekler. |
| [`core.lengthen`](lengthen.md) | Uzunluk | `UZUNLUK`, `LENGTHEN`, `UZN` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Çizginin bir ucunu kendi doğrultusunda hareket ettirerek uzunluğunu değiştirir. |
| [`core.explode`](explode.md) | Patlat | `PATLAT`, `EXPLODE`, `PTL` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Çizgiyi tek tek kenarlara, alanı sınırına, blok referansını bileşenlerine ayırır. |
| [`core.align`](align.md) | Hizala | `HİZALA`, `HIZALA`, `ALIGN`, `HZL` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir ya da iki nokta çiftiyle nesneleri taşır, döndürür ve istenirse ölçekler. |
| [`core.divide`](divide.md) | Bölümle | `BÖLÜMLE`, `BOLUMLE`, `DIVIDE`, `BLM` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir nesne boyunca eşit parçalara bölerek ya da sabit aralıkla nokta veya blok yerleştirir. |
| [`core.pedit`](pedit.md) | Çizgi Düzenle | `ÇİZGİDÜZENLE`, `CIZGIDUZENLE`, `PEDIT`, `ÇZD`, `CZD` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Çizgiyi kapatır, açar, yönünü çevirir ya da yakın köşelerini atarak sadeleştirir. |
| [`core.copy_clip`](copy_clip.md) | Panoya Kopyala | `PANOYAKOPYALA`, `PANOKOPYALA`, `COPYCLIP`, `PKP` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Seçili nesneleri çizimin kendi biçiminde panoya yazar. |
| [`core.cut`](cut.md) | Kes | `KES`, `CUT`, `KS` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçili nesneleri panoya alır ve çizimden siler; tek geri alma adımı. |
| [`core.paste`](paste.md) | Yapıştır | `YAPIŞTIR`, `YAPISTIR`, `PASTE`, `YP` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Panodaki nesneleri çizime koyar; tek geri alma adımı. |
| [`core.entity_info`](entity_info.md) | Nesne Bilgisi | `NESNEBİLGİ`, `NESNEBILGI`, `OBJINFO`, `NB` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve özniteliklerini bildirir. |
| [`core.measure_angle`](measure_angle.md) | Açı Ölç | `AÇIÖLÇ`, `ACIOLC`, `MEASUREANGLE`, `AÇÖ` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Bir tepeden çıkan iki kol arasındaki açıyı ölçer, oturumun açı kuralıyla yazar. |
| [`core.stretch`](stretch.md) | Esnet | `ESNET`, `STRETCH`, `ES` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Pencere içindeki köşeleri taşır, dışındakileri yerinde bırakır. |
| [`core.tracking`](tracking.md) | Geçici İzleme | `İZ`, `IZ`, `TRACK`, `TRK` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, şeffaf, salt okunur | Geçici izleme için nokta işaretler; iki işaretin izleri kesişir. |
| [`core.text`](text.md) | Metin | `METİN`, `METIN`, `YAZI`, `TEXT`, `MT` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Çizime metin yazar; yükseklik ve hizalama verilebilir. |
| [`core.edittext`](edittext.md) | Yazıyı Düzenle | `YAZIDÜZENLE`, `YAZIDUZENLE`, `EDITTEXT`, `YZD` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Var olan bir yazının metnini, yüksekliğini ya da hizalamasını değiştirir. |
| [`core.exportstyle`](exportstyle.md) | Stil Aktar | `STİLAKTAR`, `STILAKTAR`, `EXPORTSTYLE`, `STAKTAR` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar. |
| [`core.area`](area.md) | Alan | `ALAN`, `AREA`, `AL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapalı bir alan çizer; istenirse içine delik açar. |
| [`core.rectangle`](rectangle.md) | Dikdörtgen | `DİKDÖRTGEN`, `DIKDORTGEN`, `RECTANGLE`, `DKD`, `REC` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Karşılıklı iki köşeden ya da bir kenar ve yükseklikten dört köşeli kapalı bir alan çizer. |
| [`core.circle_draw`](circle_draw.md) | Daire | `DAİRE`, `DAIRE`, `CIRCLE`, `DR` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez+çevre, çapın iki ucu, çember üzerinde üç nokta ya da iki doğruya teğet yarıçapla daire çizer. |
| [`core.arc_draw`](arc_draw.md) | Yay | `YAY`, `ARC`, `YY` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez+iki uç, yay üzerinde üç nokta, başlangıç+merkez+süpürme ya da başlangıç+bitiş+yarıçapla yay çizer. |
| [`core.vertex_move`](vertex_move.md) | Köşe Taşı | `KÖŞETAŞI`, `KOSETASI`, `MOVEVERTEX`, `KT` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir nesnenin köşesini ya da tutamağını yeni bir yere taşır. |
| [`core.vertex_insert`](vertex_insert.md) | Köşe Ekle | `KÖŞEEKLE`, `KOSEEKLE`, `ADDVERTEX`, `KE` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir kenarın ortasına yeni köşe ekler. |
| [`core.to_area`](to_area.md) | Alana Çevir | `ALANAÇEVİR`, `ALANACEVIR`, `TOAREA`, `ALÇ` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Uç uca değen çizgileri tek bir kapalı alana çevirir. |
| [`core.move`](move.md) | Taşı | `TAŞI`, `TASI`, `MOVE`, `TŞ` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri iki nokta arasındaki kadar taşır. |
| [`core.copy`](copy.md) | Kopyala | `KOPYALA`, `COPY`, `KP` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesnelerin kopyasını verilen her noktaya, başlangıçtan o noktaya kadar öteleyerek koyar. |
| [`core.array`](array.md) | Dizi | `DİZİ`, `DIZI`, `ARRAY`, `DZ` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri satır/sütun ya da bir merkez etrafında çoğaltır. |
| [`core.combine`](combine.md) | Birleştir | `BİRLEŞTİR`, `BIRLESTIR`, `COMBINE`, `BRL` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek çizgi yapar. |
| [`core.split`](split.md) | Böl | `BÖL`, `BOL`, `SPLIT`, `BL` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Nesneleri çizilen bir kesme çizgisiyle böler. |
| [`core.trim`](trim.md) | Buda | `BUDA`, `TRIM`, `BD` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Tıklanan parçayı kesme sınırları arasından atar; çizgide, yayda ve dairede çalışır. |
| [`core.extend`](extend.md) | Uzat | `UZAT`, `EXTEND`, `UZ` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Tıklanan ucu sınıra ulaşana kadar uzatır: çizginin ucunu doğrultusunda, yayınkini çemberi boyunca. |
| [`core.chamfer`](chamfer.md) | Pah | `PAH`, `CHAMFER`, `PH` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir köşeyi düz bir kenarla keser (pah kırar). |
| [`core.fillet`](fillet.md) | Yuvarla | `YUVARLA`, `FILLET`, `YV` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir köşeyi verilen yarıçapta yay ile yuvarlatır. |
| [`core.set_layer`](set_layer.md) | Katmana Ata | `KATMANAT`, `KATMANATA`, `SETLAYER`, `KA` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri başka bir katmana taşır. |
| [`core.match_style`](match_style.md) | Stil Kopyala | `STİLKOPYALA`, `STILKOPYALA`, `MATCHPROP`, `SK` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir nesnenin stilini seçilen nesnelere uygular. |
| [`core.colour`](colour.md) | Renk | `RENK`, `COLOR`, `COLOUR`, `RNK` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçili nesnelerin çizgi ve dolgu rengini değiştirir ya da katmanın rengine döndürür. |
| [`core.rotate`](rotate.md) | Döndür | `DÖNDÜR`, `DONDUR`, `ROTATE`, `DÖN` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri bir merkez etrafında döndürür. |
| [`core.scale`](scale.md) | Ölçekle | `ÖLÇEKLE`, `OLCEKLE`, `SCALE`, `ÖLÇEK`, `OLCEK` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri bir merkeze göre büyütür ya da küçültür. |
| [`core.mirror`](mirror.md) | Aynala | `AYNALA`, `MIRROR`, `AYN` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri iki noktadan geçen eksende aynalar. |
| [`core.measure`](measure.md) | Ölç | `ÖLÇ`, `OLC`, `MEASURE`, `MS` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Noktalar arasındaki mesafeyi, koordinat farkını ve açıyı yazar; ikiden fazla nokta kenarları ve toplam uzunluğu verir. |
| [`core.measure_area`](measure_area.md) | Alan Ölç | `ALANÖLÇ`, `ALANOLC`, `AREAOF`, `AÖ` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Seçilen nesnelerin ya da köşeleri gösterilen bir alanın alanını ve çevresini yazar. |
| [`core.coordinate`](coordinate.md) | Koordinat Oku | `KOORDİNAT`, `KOORDINAT`, `COORDINATE`, `KRD` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar. |
| [`core.pan`](pan.md) | Kaydır | `KAYDIR`, `PAN`, `KY` | Görünüm | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, şeffaf, salt okunur | Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır. |
| [`core.offset`](offset.md) | Ofset | `OFSET`, `OFFSET`, `OF` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçili nesnelerin verilen mesafede, gösterilen tarafta paralelini çizer: açık çizgiye tek yanda çizgi, alana delikleriyle alan, daireye daire. |
| [`core.sector`](sector.md) | Daire Dilimi | `DİLİM`, `DILIM`, `SECTOR`, `DL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir. |
| [`core.annulus`](annulus.md) | Halka | `HALKA`, `ANNULUS`, `HLK` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez, iç ve dış yarıçaptan delikli halka çizer. |
| [`core.ellipse_draw`](ellipse_draw.md) | Elips | `ELİPS`, `ELIPS`, `ELLIPSE`, `EL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir. |
| [`core.spline`](spline.md) | Spline | `SPLINE`, `SPLINE`, `SPLINE`, `SPL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kontrol noktalarından NURBS eğrisi (spline) çizer. |
| [`core.hatch`](hatch.md) | Tarama | `TARAMA`, `TARAMA`, `HATCH`, `TRM` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapalı nesnelerin ya da verilen köşelerin içini katalogdaki bir desenle tarar. |
| [`core.block`](block.md) | Blok Tanımla | `BLOK`, `BLOK`, `BLOCK`, `BLK` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar. |
| [`core.insert`](insert.md) | Blok Ekle | `BLOKEKLE`, `BLOKEKLE`, `INSERT`, `BE` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir. |
| [`core.dimension`](dimension.md) | Ölçü | `ÖLÇÜ`, `OLCU`, `DIMENSION`, `ÖÇ` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | İki nokta arasını, bir yarıçapı, çapı ya da açıyı ölçüp yazısı ve oklarıyla çizer. |
| [`core.leader`](leader.md) | Lider | `LİDER`, `LIDER`, `LEADER`, `LD` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir noktayı gösteren oklu çizgi çizer, istenirse yanına yazı koyar. |
| [`core.points`](points.md) | Nokta Listesi | `NOKTALAR`, `POINTS`, `NKL` | Dosya | tek işlem | betiklenebilir, AI erişimli | Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod). |
| [`core.guide`](guide.md) | Kılavuz | `KILAVUZ`, `GUIDE`, `KLV` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Cetvel kılavuzu ve açılı kılavuz ekler, listeler ve siler. |
| [`core.attribute`](attribute.md) | Öznitelik | `ÖZNİTELİK`, `OZNITELIK`, `ATTRIBUTE`, `ÖZN`, `OZN` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar. |
| [`core.column`](column.md) | Sütun | `SÜTUN`, `SUTUN`, `COLUMN`, `STN` | Düzenleme | geri alınmaz | betiklenebilir | Öznitelik sütunu tanımlar, düzenler, siler; argümansız çağrılınca listeler. |
| [`core.erase`](erase.md) | Sil | `SİL`, `SIL`, `ERASE`, `E` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri siler. |
| [`core.select`](select.md) | Seç | `SEÇ`, `SEC`, `SELECT`, `S` | Düzenleme | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Nesneleri seçer: tümü, kimlikle, katman, pencere, kesen kutu, çokgen, çit, önceki seçim, son nesne ya da tek nokta. |
| [`core.label`](label.md) | Etiket | `ETİKET`, `ETIKET`, `LABEL`, `ETK` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Katmandaki nesneleri özniteliklerinden okuyarak etiketler. |
| [`core.layer`](layer.md) | Katman | `KATMAN`, `LAYER`, `KAT` | Katman | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Katman oluşturur, aktif yapar ve özelliklerini değiştirir. |
| [`core.layer_visibility`](layer_visibility.md) | Katman Görünümü | `KATMANGÖRÜNÜM`, `KATMANGORUNUM`, `LAYERVIEW`, `KGÖ`, `KGO` | Katman | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Katmanların görünürlüğünü toptan değiştirir: bir katmanı gösterir ya da gizler, yalnız onu bırakır, hepsini gösterir veya görünürlüğü ters çevirir. |
| [`core.layout`](layout.md) | Çıktı Yerleşimi | `ÇIKTIYERLEŞİMİ`, `CIKTIYERLESIMI`, `LAYOUT`, `ÇYR`, `CYR` | Dosya | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Çizimin çıktı yerleşimlerini yönetir: yeni yerleşim açar, siler, adlandırır ve kâğıdını değiştirir. Yerleşim çizimle birlikte kaydedilir ve geri alınabilir. |
| [`core.layout_item`](layout_item.md) | Çıktı Öğesi | `ÇIKTIÖĞE`, `CIKTIOGE`, `LAYOUTITEM`, `ÇÖĞ`, `COG` | Dosya | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir çıktı yerleşiminin üzerindeki öğeleri yönetir: harita çerçevesi, başlık, ölçek çubuğu, kuzey oku, lejant, resim, şekil ve tablo ekler, taşır, ayarlar ve siler. |
| [`core.layout_template`](layout_template.md) | Çıktı Şablonu | `ÇIKTIŞABLON`, `CIKTISABLON`, `LAYOUTTEMPLATE`, `ÇŞB`, `CSB` | Dosya | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kurumun standart çıktı yerleşimlerini saklar ve uygular. Şablon çizimin dışında, kullanıcı profilinde durur; her çizime uygulanabilir. Şablon düzeni taşır, zemin koordinatlarını taşımaz. |
| [`core.style`](style.md) | Stil | `STİL`, `STIL`, `STYLE`, `ST` | Katman | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar. |
| [`core.symbol`](symbol.md) | Sembol | `SEMBOL`, `SEMBOLLER`, `SYMBOL`, `SMB` | Katman | geri alınmaz | betiklenebilir, AI erişimli | Gösterim rafını yükler, ağacında gezer ve içinde arar. |
| [`core.zoom`](zoom.md) | Yakınlaş | `YAKINLAŞ`, `YAKINLAS`, `ZOOM`, `Z` | Görünüm | geri alınmaz | betiklenebilir, AI erişimli, şeffaf, salt okunur | Görünümü çizim kapsamına veya verilen çarpana ayarlar. |
| [`core.undo`](undo.md) | Geri Al | `GERİAL`, `GERIAL`, `UNDO`, `U` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Son işlemi geri alır. |
| [`core.redo`](redo.md) | Yinele | `YİNELE`, `YINELE`, `REDO` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Geri alınan işlemi yineler. |
| [`core.new`](new.md) | Yeni | `YENİ`, `YENI`, `NEW` | Dosya | geri alınmaz | betiklenebilir | Boş bir çizim açar; ekrandaki çizimin yerine geçer. |
| [`core.open`](open.md) | Aç | `AÇ`, `AC`, `OPEN` | Dosya | geri alınmaz | etkileşimli, betiklenebilir | Bir KentOSCad proje dosyasını açar ve çizimin yerine koyar. |
| [`core.save`](save.md) | Kaydet | `KAYDET`, `SAVE`, `KYD` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Çizimi bağlı olduğu KentOSCad proje dosyasına kaydeder. |
| [`core.saveas`](saveas.md) | Farklı Kaydet | `FARKLIKAYDET`, `SAVEAS`, `FKAYDET` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Çizimi yeni bir KentOSCad proje dosyasına kaydeder ve ona bağlar. |
| [`core.import`](import.md) | İçe Aktar | `İÇEAKTAR`, `ICEAKTAR`, `IMPORT`, `IAKTAR` | Dosya | tek işlem | etkileşimli, betiklenebilir | Dış bir veri dosyasını çizime ekler. |
| [`core.export`](export.md) | Dışa Aktar | `DIŞAAKTAR`, `DISAAKTAR`, `EXPORT`, `DAKTAR` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Çizimi dış bir veri biçimine yazar. |
| [`core.script`](script.md) | Betik Çalıştır | `BETİK`, `BETIK`, `SCRIPT` | Betik | komuta özel | etkileşimli, betiklenebilir, salt okunur | Bir betik dosyasını komut veri yolu üzerinden çalıştırır. |
| [`core.python`](python.md) | Python Çalıştır | `PYTHON`, `PİTON`, `PITON`, `PY` | Betik | komuta özel | etkileşimli, betiklenebilir, salt okunur | Bir Python parçacığını komut veri yolu üzerinden çalıştırır. |
| [`core.database`](database.md) | Veritabanı | `VERİTABANI`, `VERITABANI`, `DATABASE`, `VT` | Dosya | geri alınmaz | etkileşimli, betiklenebilir | PostGIS veritabanına bağlanır; katmanları tablo, projeleri kayıt olarak yazar. |
| [`core.print`](print.md) | Yazdır | `YAZDIR`, `PRINT`, `PLOT`, `YZDR` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Çizimin bir penceresini bir yazdırma profilinin kâğıdına yerleştirip PDF dosyasına yazar ya da yazıcıya gönderir. |
| [`core.print_profile`](print_profile.md) | Yazdırma Profili | `YAZDIRMAPROFİLİ`, `YAZDIRMAPROFILI`, `PRINTPROFILE`, `YZP` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Yazdırma profillerini listeler, ekler, siler ya da birini varsayılan yapar; profil kâğıdı, yönü, çözünürlüğü ve kenar boşluğunu taşır. |
| [`core.setting`](setting.md) | Proje Ayarı | `AYAR`, `SETTING`, `AY` | Sistem | tek işlem | betiklenebilir | Proje ayarlarını listeler, okur ve değiştirir. |
| [`core.preference`](preference.md) | Uygulama Tercihi | `TERCİH`, `TERCIH`, `PREFERENCE`, `PREF` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Uygulama tercihlerini listeler, okur ve değiştirir. |
| [`core.mode`](mode.md) | Çizim Modları | `MOD`, `MODE`, `MD` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur ve değiştirir. |
| [`core.help`](help.md) | Komut Listesi | `YARDIM`, `HELP`, `?` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Komut listesini veya tek bir komutun ayrıntısını gösterir. |
| [`islem.tampon`](tampon.md) | — | `TAMPON`, `BUFFER`, `TMP` | İşlem | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapsamdaki nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer: çizginin iki yanı, noktanın çevresi, alanın dışı; üst üste binen tamponlar tek alan olur. |
| [`islem.alan_duzenle`](alan_duzenle.md) | — | `ALANDÜZENLE`, `ALANDUZENLE`, `ADJUSTAREA`, `ADZ` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapalı bir alanı istenen alana getirir: bütün kenarları eşit daraltıp genişleterek, bir kenarı kaydırarak ya da bir köşeyi çekerek; şekil bozulmaz. |
| [`islem.uzunluk_yaz`](uzunluk_yaz.md) | — | `UZUNLUKYAZ`, `UZUNLUKYAZ`, `LABELLENGTH`, `UZY` | İşlem | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel bir yazı olarak yazar; yazı kenara bağlıdır, kenar değişince izler ve yenilenir. |
| [`islem.kose_numarala`](kose_numarala.md) | — | `KÖŞENUMARALA`, `KOSENUMARALA`, `NUMBERVERTICES`, `KNM` | İşlem | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapsamdaki her alanın (ve çizginin) köşelerini seçilen köşeden başlayarak sırayla numaralar ve numarayı köşenin dışına yazar; numara köşesine bağlıdır, köşe taşınınca izler. |
| [`islem.bag_coz`](bag_coz.md) | — | `BAĞÇÖZ`, `BAGCOZ`, `DETACH`, `BÇ`, `BC` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapsamdaki yazıların bağını çözer: yazı yerinde kalır, bağlı olduğu nesne bundan sonra tek başına taşınır. |
| [`islem.bagla`](bagla.md) | — | `BAĞLA`, `BAGLA`, `ATTACH`, `BĞ`, `BG` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapsamdaki yazıları seçilen nesnenin en yakın kenarına ya da köşesine bağlar: nesne taşınınca yazı izler; istenirse yazı kenarın uzunluğu olur. |
| [`core.fit`](fit.md) | Oturt (Helmert) | `OTURT`, `FIT`, `GEOREF`, `OTR` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Yerel ölçülmüş çizimi kontrol noktalarıyla haritaya oturtur (2B Helmert). |
| [`core.stakeout`](stakeout.md) | Aplikasyon | `APLİKASYON`, `APLIKASYON`, `STAKEOUT`, `APL` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | İstasyondan her noktaya mesafe ve açı listesi çıkarır (aplikasyon). |
| [`core.reproject`](reproject.md) | Dönüştür | `DÖNÜŞTÜR`, `DONUSTUR`, `REPROJECT`, `DNS` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Çizimin tamamını bir koordinat sisteminden diğerine dönüştürür. |
| [`geodesy.traverse`](traverse.md) | Poligon Hesabı | `POLİGON`, `POLIGON`, `TRAVERSE`, `PLG` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kırılma açısı ve kenarlardan poligon koordinatları hesaplar, kapanma hatalarını dağıtır ve mevzuat toleransına karşı denetler. |
| [`core.merge`](merge.md) | Tevhit | `TEVHİT`, `TEVHIT`, `MERGE`, `TVH` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Komşu parselleri tek parselde birleştirir (tevhit). |
| [`core.split_parcel`](split_parcel.md) | İfraz | `İFRAZ`, `IFRAZ`, `SUBDIVIDE`, `İFR` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir parseli düz bir ayırma çizgisiyle ikiye böler (ifraz). |
| [`core.split_area`](split_area.md) | Alana Göre İfraz | `ALANİFRAZ`, `ALANIFRAZ`, `SPLITAREA`, `ALİF` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Parselden verilen yöne paralel, istenen alanda bir parça ayırır. |
| [`core.topology`](topology.md) | Topoloji Denetimi | `TOPOLOJİ`, `TOPOLOJI`, `TOPOLOGY`, `TPL` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Kendini kesen sınır, sıfır alan ve örtüşen parselleri raporlar. |
| [`core.contour`](contour.md) | Eşyükselti Eğrileri | `EŞYÜKSELTİ`, `ESYUKSELTI`, `CONTOUR`, `EŞY` | Çizim | tek işlem | betiklenebilir, AI erişimli | Kotlu noktalardan eş yükselti eğrileri çizer. |
| [`core.earthwork`](earthwork.md) | Hacim Hesabı | `HACİM`, `HACIM`, `EARTHWORK`, `HCM` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Kotlu noktalardan bir kota göre kazı ve dolgu hacmini hesaplar. |
| [`core.layers`](layers.md) | Katmanları Listele | `KATMANLAR`, `LAYERS`, `KTL` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Katmanları, nesne sayılarını, görünürlük ve kilit durumlarını listeler. |
| [`core.attr_schema`](attr_schema.md) | Öznitelik Şeması | `ÖZNİTELİKŞEMASI`, `OZNITELIKSEMASI`, `ATTRSCHEMA`, `ÖŞ` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Çizimde tanımlı öznitelik sütunlarını ve tiplerini listeler. |
| [`core.query`](query.md) | Sorgula | `SORGULA`, `QUERY`, `SRG` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Katman ve öznitelik koşuluna uyan nesneleri sayar ve anahtarlarını bildirir. |
| [`core.selection_info`](selection_info.md) | Seçim Bilgisi | `SEÇİMBİLGİSİ`, `SECIMBILGISI`, `SELECTIONINFO`, `SÇB` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Kullanıcının o anki seçimini bildirir: kaç nesne ve hangi anahtarlar. |
| [`core.object_points`](object_points.md) | Nesne Noktaları | `NESNENOKTALARI`, `OBJECTPOINTS`, `NNK` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Nesnelerin merkezini, köşelerini, uçlarını, kutusunu ya da kenar ortalarını bildirir; bir ajan bunları yeni çizimin taban noktası olarak kullanır. |
| [`core.view_info`](view_info.md) | Görünüm Bilgisi | `GÖRÜNÜMBİLGİSİ`, `GORUNUMBILGISI`, `VIEWINFO`, `GRB` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Ekranda görünen alanın köşe koordinatlarını, merkezini, ölçeğini ve CRS'ini bildirir. |
| [`core.context`](context.md) | Bağlam | `BAĞLAM`, `BAGLAM`, `CONTEXT`, `BĞL` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Üzerinde çalışılan her şeyi tek çağrıda özetler: belge sürümü, koordinat sistemi, kapsam, katmanlar, çıktı yerleşimleri ve hedefli olup olmadıkları, seçili nesneler ve görünüm. Özet verir, döküm değil. |
| [`core.tool_search`](tool_search.md) | Araç Ara | `ARAÇARA`, `ARACARA`, `TOOLSEARCH`, `ARA` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Ajan araç kataloğunda ad ve özete göre arar. Sonuç her zaman kaç aracın eşleştiğini, kaçının gösterildiğini ve katalogdaki toplam araç sayısını söyler: arama hiçbir aracı gizlemez, tam liste `tools/list` ile alınır. |
| [`core.job_template`](job_template.md) | İş Şablonu | `İŞŞABLONU`, `ISSABLONU`, `JOBTEMPLATE`, `İŞŞ` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Sık yapılan işlerin — atlas, kadastro kontrolü, parsel raporu — komut satırlarını sırasıyla verir. Hiçbirini çalıştırmaz: adımlar olağan araç yüzeyinden gönderilir ve yazan her adım yine öneri olur. |
| [`core.suggestion`](suggestion.md) | Yapay Zeka Önerisi | `ÖNERİ`, `ONERI`, `SUGGESTION`, `ÖN` | Sistem | komuta özel | etkileşimli, betiklenebilir, AI erişimli | Bekleyen yapay zeka önerilerini listeler, durumunu söyler, uygular ya da reddeder. |
| [`core.mcp`](mcp.md) | MCP Sunucusu | `MCPSUNUCU`, `MCPSERVER`, `MCP` | Sistem | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Yapay zeka ajanlarının bağlanacağı MCP sunucusunu başlatır, durdurur, durumunu söyler, yeni bir erişim belirteci üretir, bağlı istemcileri listeler ve tek bir istemcinin yetkisini kaldırır. |
| [`core.ai_provider`](ai_provider.md) | Yapay Zeka Modeli | `YAPAYZEKAMODELİ`, `YAPAYZEKAMODELI`, `AIMODEL`, `YZM` | Sistem | geri alınmaz | etkileşimli, betiklenebilir | Yapay zeka model sağlayıcılarını listeler, ekler, siler, birini varsayılan yapar ya da bağlantısını dener; profil adresi, lehçesi, modeli ve anahtar adını taşır. |

## Parametreler

### `core.line` — ÇİZGİ (Çizgi)

İki veya daha fazla nokta arasında doğru parçaları çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 2 | Ardışık doğru parçalarının köşe noktaları |

Ayrıntılı kullanım: [ÇİZGİ](line.md)

### `core.polyline` — ÇOKLUÇİZGİ (Çoklu Çizgi)

Birden çok noktadan TEK bir çizgi nesnesi çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 2 | Çoklu çizginin köşe noktaları; hepsi tek nesne olur |

Ayrıntılı kullanım: [ÇOKLUÇİZGİ](polyline.md)

### `core.point_draw` — NOKTA (Nokta)

Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 1 | Yerleştirilecek noktalar |

Ayrıntılı kullanım: [NOKTA](point_draw.md)

### `core.perp_offset` — DİKAYAK (Dik Ayak)

Taban çizgisine göre dik ayak ve dik boy vererek nokta yerleştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `baslangic` | point | 1 | Taban çizgisinin ilk noktası (A) |
| `bitis` | point | 1 | Taban çizgisinin ikinci noktası (B) |
| `ayak` | number | en az 0 | A'dan taban boyunca uzaklık (m); boy ile sırayla eşleşir |
| `boy` | number | en az 0 | Tabana dik uzaklık (m); A→B yönünde SOL pozitiftir |
| `cizgi` | bool | isteğe bağlı | Yerleştirilen noktaları verildikleri sırayla çizgiyle birleştirir |

Ayrıntılı kullanım: [DİKAYAK](perp_offset.md)

### `core.survey_polar` — ALIM (Alım)

İstasyondan okunan açı ve kenarlardan nokta hesaplar ve yerleştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `istasyon` | point | 1 | Aletin durduğu bilinen nokta |
| `baglama` | point_list | isteğe bağlı | Bağlama noktası: verilirse açılar ondan itibaren okunmuş sayılır |
| `aci` | number | en az 0 | Okunan açı; kenar ile sırayla eşleşir |
| `kenar` | number | en az 0 | Alete olan uzaklık (m) |
| `cizgi` | bool | isteğe bağlı | Hesaplanan noktaları okundukları sırayla çizgiyle birleştirir |

Ayrıntılı kullanım: [ALIM](survey_polar.md)

### `core.intersect_point` — KESİŞİMNOKTA (Kesişim Noktası)

İki doğrultunun, iki uzaklığın ya da iki doğrunun kesişimine nokta koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `yontem` | text | isteğe bağlı | dogrultu: iki doğrultu · mesafe: iki uzaklık · dogru: iki doğru |
| `birinci` | point | 1 | Birinci bilinen nokta |
| `ikinci` | point_list | isteğe bağlı | İkinci bilinen nokta |
| `ucuncu` | point_list | isteğe bağlı | İkinci doğrunun ilk noktası |
| `dorduncu` | point_list | isteğe bağlı | İkinci doğrunun ikinci noktası |
| `birinci_aci` | number | isteğe bağlı | Birinci noktadan okunan doğrultu |
| `ikinci_aci` | number | isteğe bağlı | İkinci noktadan okunan doğrultu |
| `birinci_mesafe` | number | isteğe bağlı | Birinci noktadan ölçülen uzaklık (m) |
| `ikinci_mesafe` | number | isteğe bağlı | İkinci noktadan ölçülen uzaklık (m) |
| `yon` | text | isteğe bağlı | İki uzaklık kesişiminin hangi çözümü; birinci→ikinci yönüne göre |
| `yon_nokta` | point_list | isteğe bağlı | mesafe: iki çözümden istenenin gösterildiği nokta; yon verilmişse sorulmaz |
| `kesisim` | point_list | isteğe bağlı | Bulunan nokta; günlüğe yazılır |

Ayrıntılı kullanım: [KESİŞİMNOKTA](intersect_point.md)

### `core.point_along` — ARANOKTA (Ara Nokta)

İki nokta arasındaki doğru üzerinde oran, uzaklık ya da eşit bölmeyle nokta koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `birinci` | point | 1 | Doğrunun ilk noktası |
| `ikinci` | point | 1 | Doğrunun ikinci noktası |
| `yontem` | text | isteğe bağlı | oran: 0 ile 1 arası · mesafe: ilk noktadan metre |
| `deger` | number | en az 0 | Oran ya da uzaklık; birden çok verilebilir |
| `sayi` | integer | isteğe bağlı | Doğruyu bu kadar eşit parçaya böler |

Ayrıntılı kullanım: [ARANOKTA](point_along.md)

### `core.polygon_regular` — ÇOKGEN (Düzgün Çokgen)

Merkez ve kenar sayısından düzgün çokgen çizer: içten, dıştan ya da kenar uzunluğundan.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | point | 1 | Çokgenin merkezi |
| `kenar_sayisi` | integer | 1 | Kenar sayısı |
| `yontem` | text | isteğe bağlı | ic: köşeler çemberin üzerinde · dis: kenarlar çembere teğet · kenar: kenar uzunluğundan |
| `yaricap` | number | isteğe bağlı | ic/dis yönteminin yarıçapı (m) |
| `kenar_uzunlugu` | number | isteğe bağlı | kenar yönteminin uzunluğu (m) |
| `aci` | number | isteğe bağlı | İlk köşenin merkeze göre doğrultusu; varsayılan 0 |
| `kose` | point_list | isteğe bağlı | Yerine işaret edilen nokta: yarıçapı ve yönü verir; yaricap verilmişse sorulmaz |

Ayrıntılı kullanım: [ÇOKGEN](polygon_regular.md)

### `core.break` — KIR (Kır)

Çizgiden iki nokta arasındaki parçayı çıkarır; tek nokta verilirse boşluk bırakmadan böler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | en az 0 | Kırılacak çizgi |
| `birinci` | point | 1 | Kırılacak parçanın ilk noktası |
| `ikinci` | point_list | isteğe bağlı | Kırılacak parçanın ikinci noktası; verilmezse boşluk bırakmadan böler |

Ayrıntılı kullanım: [KIR](break.md)

### `core.join` — UÇUCA (Uç Uca Ekle)

Uçları birbirine değen çizgileri tek bir çizgiye ekler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | en az 0 | Uç uca eklenecek çizgiler |
| `tolerans` | number | isteğe bağlı | Uçların değmiş sayılması için en büyük açıklık (m); varsayılan 0,001 |

Ayrıntılı kullanım: [UÇUCA](join.md)

### `core.lengthen` — UZUNLUK (Uzunluk)

Çizginin bir ucunu kendi doğrultusunda hareket ettirerek uzunluğunu değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | en az 0 | Uzunluğu değişecek çizgi |
| `delta` | number | isteğe bağlı | Eklenecek uzunluk (m); eksi kısaltır |
| `yuzde` | number | isteğe bağlı | İstenen uzunluk, şimdikinin yüzdesi |
| `toplam` | number | isteğe bağlı | İstenen toplam uzunluk (m) |
| `uc` | text | isteğe bağlı | Hangi uç hareket eder; varsayılan son |

Ayrıntılı kullanım: [UZUNLUK](lengthen.md)

### `core.explode` — PATLAT (Patlat)

Çizgiyi tek tek kenarlara, alanı sınırına, blok referansını bileşenlerine ayırır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | en az 0 | Patlatılacak nesneler |

Ayrıntılı kullanım: [PATLAT](explode.md)

### `core.align` — HİZALA (Hizala)

Bir ya da iki nokta çiftiyle nesneleri taşır, döndürür ve istenirse ölçekler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | en az 0 | Hizalanacak nesneler |
| `kaynak` | point | 1 | Birinci kaynak nokta |
| `hedef` | point | 1 | Birinci kaynağın gideceği yer |
| `kaynak2` | point_list | isteğe bağlı | İkinci kaynak nokta; verilirse döndürme de yapılır |
| `hedef2` | point_list | isteğe bağlı | İkinci kaynağın gideceği yer |
| `olcekle` | bool | isteğe bağlı | İki çiftin uzunluk oranıyla ölçekler de |

Ayrıntılı kullanım: [HİZALA](align.md)

### `core.divide` — BÖLÜMLE (Bölümle)

Bir nesne boyunca eşit parçalara bölerek ya da sabit aralıkla nokta veya blok yerleştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | en az 0 | Bölünecek nesne |
| `sayi` | integer | isteğe bağlı | Kaç eşit parçaya bölünecek |
| `aralik` | number | isteğe bağlı | Sabit aralık (m); başlangıçtan itibaren yürür |
| `blok` | text | isteğe bağlı | Nokta yerine bu bloğu koyar; blok önceden tanımlı olmalı |
| `hizala` | bool | isteğe bağlı | Bloğu üzerinde durduğu kenarın doğrultusuna çevirir |

Ayrıntılı kullanım: [BÖLÜMLE](divide.md)

### `core.pedit` — ÇİZGİDÜZENLE (Çizgi Düzenle)

Çizgiyi kapatır, açar, yönünü çevirir ya da yakın köşelerini atarak sadeleştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | en az 0 | Düzenlenecek çizgiler |
| `islem` | text | isteğe bağlı | kapat: kapalı alana çevir · ac: aç · ters: yönünü çevir · sadelestir: yakın köşeleri at |
| `tolerans` | number | isteğe bağlı | sadelestir: bu uzaklıktan yakın köşeler atılır (m) |

Ayrıntılı kullanım: [ÇİZGİDÜZENLE](pedit.md)

### `core.copy_clip` — PANOYAKOPYALA (Panoya Kopyala)

Seçili nesneleri çizimin kendi biçiminde panoya yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Panoya alınacak nesneler; verilmezse seçim kullanılır |
| `dosya` | text | isteğe bağlı | Panonun yazılacağı dosya; verilmezse ortak pano dosyası |

Ayrıntılı kullanım: [PANOYAKOPYALA](copy_clip.md)

### `core.cut` — KES (Kes)

Seçili nesneleri panoya alır ve çizimden siler; tek geri alma adımı.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Kesilecek nesneler; verilmezse seçim kullanılır |
| `dosya` | text | isteğe bağlı | Panonun yazılacağı dosya; verilmezse ortak pano dosyası |

Ayrıntılı kullanım: [KES](cut.md)

### `core.paste` — YAPIŞTIR (Yapıştır)

Panodaki nesneleri çizime koyar; tek geri alma adımı.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nokta` | point_list | isteğe bağlı | Yapıştırılacak yerin sol alt köşesi; yerinde=evet ile gereksiz |
| `yerinde` | bool | isteğe bağlı | Kopyalandığı koordinatlara yapıştırır |
| `dosya` | text | isteğe bağlı | Okunacak pano dosyası; verilmezse ortak pano dosyası |

Ayrıntılı kullanım: [YAPIŞTIR](paste.md)

### `core.entity_info` — NESNEBİLGİ (Nesne Bilgisi)

Nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve özniteliklerini bildirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Bilgisi istenen nesneler |

Ayrıntılı kullanım: [NESNEBİLGİ](entity_info.md)

### `core.measure_angle` — AÇIÖLÇ (Açı Ölç)

Bir tepeden çıkan iki kol arasındaki açıyı ölçer, oturumun açı kuralıyla yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `tepe` | point | 1 | Açının tepe noktası |
| `birinci` | point | 1 | Birinci kolun üzerinde bir nokta |
| `ikinci` | point | 1 | İkinci kolun üzerinde bir nokta |

Ayrıntılı kullanım: [AÇIÖLÇ](measure_angle.md)

### `core.stretch` — ESNET (Esnet)

Pencere içindeki köşeleri taşır, dışındakileri yerinde bırakır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `pencere` | point_list | 2 | Esnetme penceresinin iki köşesi; içindeki köşeler taşınır |
| `baslangic` | point | 1 | Esnetmenin başlangıç noktası |
| `bitis` | point | 1 | Esnetmenin bitiş noktası |
| `nesneler` | selection | en az 0 | Yalnız bu nesneler esnetilir; verilmezse pencerenin dokunduğu her nesne |

Ayrıntılı kullanım: [ESNET](stretch.md)

### `core.tracking` — İZ (Geçici İzleme)

Geçici izleme için nokta işaretler; iki işaretin izleri kesişir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nokta` | point_list | isteğe bağlı | İşaretlenecek nokta; yoksa işaretler listelenir |
| `sil` | bool | isteğe bağlı | Bütün işaretleri siler |

Ayrıntılı kullanım: [İZ](tracking.md)

### `core.text` — METİN (Metin)

Çizime metin yazar; yükseklik ve hizalama verilebilir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point | 1 | Yazının başlangıç noktası |
| `yazi` | text | 1 | Yazılacak metin |
| `yukseklik` | integer | isteğe bağlı | Yazı yüksekliği, zeminde milimetre; yoksa proje ayarı |
| `bitis` | point_list | isteğe bağlı | Taban çizgisinin bitişi; yoksa yatay |
| `hizalama` | text | isteğe bağlı | sol, orta, sag veya merkez |

Ayrıntılı kullanım: [METİN](text.md)

### `core.edittext` — YAZIDÜZENLE (Yazıyı Düzenle)

Var olan bir yazının metnini, yüksekliğini ya da hizalamasını değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Düzenlenecek yazılar; verilmezse seçim |
| `yazi` | text | isteğe bağlı | Yeni metin; verilmezse değişmez |
| `yukseklik` | integer | isteğe bağlı | Yeni yükseklik, zeminde milimetre; verilmezse değişmez |
| `hizalama` | text | isteğe bağlı | sol, orta, sag veya merkez; verilmezse değişmez |

Ayrıntılı kullanım: [YAZIDÜZENLE](edittext.md)

### `core.exportstyle` — STİLAKTAR (Stil Aktar)

Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `katman` | text | 1 | Stili aktarılacak katmanın adı |
| `dosya` | text | 1 | Yazılacak .qml dosyasının yolu |

Ayrıntılı kullanım: [STİLAKTAR](exportstyle.md)

### `core.area` — ALAN (Alan)

Kapalı bir alan çizer; istenirse içine delik açar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 3 | Alanın köşe noktaları; kapanış noktası tekrarlanmaz |
| `bolum` | integer | en az 0 | Halka uzunlukları: ilki dış sınır, sonrakiler delik |

Ayrıntılı kullanım: [ALAN](area.md)

### `core.rectangle` — DİKDÖRTGEN (Dikdörtgen)

Karşılıklı iki köşeden ya da bir kenar ve yükseklikten dört köşeli kapalı bir alan çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | 2–3 | 2n: karşılıklı iki köşe · 3n: bir kenarın iki köşesi ve karşı kenarın geçtiği nokta |
| `yontem` | text | isteğe bağlı | 2n: karşılıklı iki köşe, eksenlere paralel · 3n: bir kenar ve yükseklik, döndürülmüş |

Ayrıntılı kullanım: [DİKDÖRTGEN](rectangle.md)

### `core.circle_draw` — DAİRE (Daire)

Merkez+çevre, çapın iki ucu, çember üzerinde üç nokta ya da iki doğruya teğet yarıçapla daire çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | point_list | isteğe bağlı | Dairenin merkezi |
| `cevre` | point_list | isteğe bağlı | Çember üzerinde bir nokta; yarıçapı bu belirler |
| `yontem` | text | isteğe bağlı | merkez: merkez + çevre · 2n: çapın iki ucu · 3n: çember üzerinde üç nokta · ttr: iki doğruya teğet, verilen yarıçapla |
| `birinci` | point_list | isteğe bağlı | 2n: çapın bir ucu · 3n: birinci nokta · ttr: birinci doğrunun ilk noktası |
| `ikinci` | point_list | isteğe bağlı | İkinci nokta |
| `ucuncu` | point_list | isteğe bağlı | 3n: üçüncü nokta · ttr: ikinci doğrunun ilk noktası |
| `dorduncu` | point_list | isteğe bağlı | ttr: ikinci doğrunun ikinci noktası |
| `yaricap` | number | isteğe bağlı | ttr: teğet dairenin yarıçapı (m) |
| `yon` | point_list | isteğe bağlı | ttr: dairenin geleceği köşe; dört çözümden en yakını alınır |

Ayrıntılı kullanım: [DAİRE](circle_draw.md)

### `core.arc_draw` — YAY (Yay)

Merkez+iki uç, yay üzerinde üç nokta, başlangıç+merkez+süpürme ya da başlangıç+bitiş+yarıçapla yay çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | point_list | isteğe bağlı | Yayın merkezi |
| `baslangic` | point_list | isteğe bağlı | Yayın başlangıç noktası; merkez yönteminde yarıçapı bu belirler |
| `bitis` | point_list | isteğe bağlı | Yayın bitiş noktası; süpürme saat yönünün tersinedir |
| `yontem` | text | isteğe bağlı | merkez: merkez + iki uç · 3n: yay üzerinde üç nokta · bma: başlangıç, merkez ve süpürme açısı · bby: başlangıç, bitiş ve yarıçap |
| `uzerinden` | point_list | isteğe bağlı | 3n: yayın üzerinden geçtiği nokta |
| `supurme` | number | isteğe bağlı | bma: süpürme açısı |
| `yaricap` | number | isteğe bağlı | bby: yarıçap (m) |
| `yon_nokta` | point_list | isteğe bağlı | bby: yayın hangi yandan geçeceği gösterilen nokta; yon verilmişse sorulmaz |
| `yon` | text | isteğe bağlı | bby: yayın hangi tarafa kavis yaptığı; başlangıç→bitiş yönüne göre |

Ayrıntılı kullanım: [YAY](arc_draw.md)

### `core.vertex_move` — KÖŞETAŞI (Köşe Taşı)

Bir nesnenin köşesini ya da tutamağını yeni bir yere taşır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | 1 | Köşesi taşınacak nesnenin kimliği |
| `kose` | integer | 1 | Taşınacak köşenin sırası; ilk köşe 1'dir |
| `yer` | point | isteğe bağlı | Köşeyi gösteren nokta: kose verilmezse en yakın köşe, nesne de verilmezse altındaki nesne |
| `nokta` | point | 1 | Köşenin yeni yeri |

Ayrıntılı kullanım: [KÖŞETAŞI](vertex_move.md)

### `core.vertex_insert` — KÖŞEEKLE (Köşe Ekle)

Bir kenarın ortasına yeni köşe ekler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | 1 | Köşe eklenecek nesnenin kimliği |
| `kose` | integer | 1 | Yeni köşenin ardına geleceği köşe; ilk köşe 1'dir |
| `yer` | point | isteğe bağlı | Kenarı gösteren nokta: kose verilmezse en yakın kenar, nesne de verilmezse altındaki nesne |
| `nokta` | point | 1 | Yeni köşenin yeri |

Ayrıntılı kullanım: [KÖŞEEKLE](vertex_insert.md)

### `core.to_area` — ALANAÇEVİR (Alana Çevir)

Uç uca değen çizgileri tek bir kapalı alana çevirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Birleştirilecek çizgilerin kimlikleri; yoksa etkin seçim |

Ayrıntılı kullanım: [ALANAÇEVİR](to_area.md)

### `core.move` — TAŞI (Taşı)

Seçilen nesneleri iki nokta arasındaki kadar taşır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Taşınacak nesnelerin kimlikleri; yoksa etkin seçim |
| `baslangic` | point | 1 | Taşımanın başlangıç noktası |
| `bitis` | point | 1 | Taşımanın bitiş noktası |

Ayrıntılı kullanım: [TAŞI](move.md)

### `core.copy` — KOPYALA (Kopyala)

Seçilen nesnelerin kopyasını verilen her noktaya, başlangıçtan o noktaya kadar öteleyerek koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Kopyalanacak nesnelerin kimlikleri; yoksa etkin seçim |
| `baslangic` | point | 1 | Kopyalamanın başlangıç noktası |
| `bitis` | point_list | en az 1 | Kopyaların geleceği noktalar; her nokta bir kopya |

Ayrıntılı kullanım: [KOPYALA](copy.md)

### `core.array` — DİZİ (Dizi)

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

### `core.combine` — BİRLEŞTİR (Birleştir)

Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek çizgi yapar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Birleştirilecek alanlar ya da çizgiler; yoksa etkin seçim |

Ayrıntılı kullanım: [BİRLEŞTİR](combine.md)

### `core.split` — BÖL (Böl)

Nesneleri çizilen bir kesme çizgisiyle böler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | en az 0 | Kesilecek nesneler; yoksa etkin seçim |
| `noktalar` | point_list | 0–2 | Kesme çizgisinin iki noktası; arayüzde çizilir |
| `nokta` | point | isteğe bağlı | Bölme noktası (tek çizgi; eski biçim) |

Ayrıntılı kullanım: [BÖL](split.md)

### `core.trim` — BUDA (Buda)

Tıklanan parçayı kesme sınırları arasından atar; çizgide, yayda ve dairede çalışır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | en az 0 | Budanan nesneler, tıklama sırasıyla; yoksa her tıklamanın altındaki nesne |
| `sinir` | selection | en az 0 | Kesme sınırları; yoksa seçili nesneler, o da yoksa tıklanan nesnenin yakınındaki her nesne |
| `hepsi` | bool | isteğe bağlı | Tıklanan nesnenin yakınındaki her nesne sınırdır (seçim ve sinir yokken öntanımlı) |
| `nokta` | point_list | en az 0 | Atılacak her parçanın üzerinde bir nokta, sırayla |

Ayrıntılı kullanım: [BUDA](trim.md)

### `core.extend` — UZAT (Uzat)

Tıklanan ucu sınıra ulaşana kadar uzatır: çizginin ucunu doğrultusunda, yayınkini çemberi boyunca.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | en az 0 | Uzatılan nesneler, tıklama sırasıyla; yoksa her tıklamanın altındaki nesne |
| `sinir` | selection | en az 0 | Uzatılacak sınırlar; yoksa seçili nesneler, o da yoksa tıklanan nesnenin yakınındaki her nesne |
| `hepsi` | bool | isteğe bağlı | Tıklanan nesnenin yakınındaki her nesne sınırdır (seçim ve sinir yokken öntanımlı) |
| `nokta` | point_list | en az 0 | Uzatılacak her ucun yakınında bir nokta, sırayla |

Ayrıntılı kullanım: [UZAT](extend.md)

### `core.chamfer` — PAH (Pah)

Bir köşeyi düz bir kenarla keser (pah kırar).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | 1 | Köşesi kesilecek nesnenin kimliği |
| `nokta` | point | 1 | İşlem yapılacak köşe |
| `mesafe` | number | 1 | Köşeden her iki kenar boyunca kesilecek mesafe, metre |

Ayrıntılı kullanım: [PAH](chamfer.md)

### `core.fillet` — YUVARLA (Yuvarla)

Bir köşeyi verilen yarıçapta yay ile yuvarlatır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | 1 | Köşesi yuvarlatılacak nesnenin kimliği |
| `nokta` | point | 1 | İşlem yapılacak köşe |
| `yaricap` | number | 1 | Yuvarlatma yarıçapı, metre |

Ayrıntılı kullanım: [YUVARLA](fillet.md)

### `core.set_layer` — KATMANAT (Katmana Ata)

Seçilen nesneleri başka bir katmana taşır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Taşınacak nesnelerin kimlikleri; yoksa etkin seçim |
| `katman` | text | 1 | Hedef katmanın adı; yoksa oluşturulur |

Ayrıntılı kullanım: [KATMANAT](set_layer.md)

### `core.match_style` — STİLKOPYALA (Stil Kopyala)

Bir nesnenin stilini seçilen nesnelere uygular.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `kaynak` | selection | isteğe bağlı | Stili kopyalanacak nesnenin kimliği; yoksa tıklanan nesne |
| `nesneler` | selection | en az 0 | Stili alacak nesnelerin kimlikleri; yoksa etkin seçim |
| `nokta` | point | isteğe bağlı | Kaynak nesnenin üzerinde bir nokta; yalnız kaynak verilmediğinde |

Ayrıntılı kullanım: [STİLKOPYALA](match_style.md)

### `core.colour` — RENK (Renk)

Seçili nesnelerin çizgi ve dolgu rengini değiştirir ya da katmanın rengine döndürür.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Rengi değişecek nesneler; verilmezse etkin seçim, o da boşsa sorulur |
| `renk` | text | isteğe bağlı | Çizgi rengi: #RRGGBB (ya da saydamlıkla #AARRGGBB) veya katman |
| `dolgu` | text | isteğe bağlı | Dolgu rengi: #RRGGBB, yok (dolgusuz) ya da katman |

Ayrıntılı kullanım: [RENK](colour.md)

### `core.rotate` — DÖNDÜR (Döndür)

Seçilen nesneleri bir merkez etrafında döndürür.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Döndürülecek nesnelerin kimlikleri; yoksa etkin seçim |
| `merkez` | point | 1 | Döndürme merkezi |
| `aci` | number | isteğe bağlı | Dönme açısı, derece; artı yön saat yönünün tersi. Verilmezse yeni doğrultu gösterilir |
| `aci_nokta` | point_list | isteğe bağlı | Dönme açısının gösterildiği nokta; aci verilmişse sorulmaz |

Ayrıntılı kullanım: [DÖNDÜR](rotate.md)

### `core.scale` — ÖLÇEKLE (Ölçekle)

Seçilen nesneleri bir merkeze göre büyütür ya da küçültür.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Ölçeklenecek nesnelerin kimlikleri; yoksa etkin seçim |
| `merkez` | point | 1 | Ölçekleme merkezi; bu nokta yerinde kalır |
| `carpan` | number | isteğe bağlı | Ölçek çarpanı; sıfırdan büyük. Verilmezse merkezden uzaklık gösterilir |
| `carpan_nokta` | point_list | isteğe bağlı | Çarpanın gösterildiği nokta; carpan verilmişse sorulmaz |

Ayrıntılı kullanım: [ÖLÇEKLE](scale.md)

### `core.mirror` — AYNALA (Aynala)

Seçilen nesneleri iki noktadan geçen eksende aynalar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Aynalanacak nesnelerin kimlikleri; yoksa etkin seçim |
| `baslangic` | point | 1 | Ayna ekseninin ilk noktası |
| `bitis` | point | 1 | Ayna ekseninin ikinci noktası |

Ayrıntılı kullanım: [AYNALA](mirror.md)

### `core.measure` — ÖLÇ (Ölç)

Noktalar arasındaki mesafeyi, koordinat farkını ve açıyı yazar; ikiden fazla nokta kenarları ve toplam uzunluğu verir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `baslangic` | point | 1 | Ölçümün ilk noktası |
| `bitis` | point | 1 | Ölçümün ikinci noktası |
| `devam` | point_list | en az 0 | Sonraki noktalar: her biri bir kenar daha ekler, toplam da yazılır |

Ayrıntılı kullanım: [ÖLÇ](measure.md)

### `core.measure_area` — ALANÖLÇ (Alan Ölç)

Seçilen nesnelerin ya da köşeleri gösterilen bir alanın alanını ve çevresini yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Ölçülecek nesnelerin kimlikleri; yoksa etkin seçim |
| `yontem` | text | isteğe bağlı | nesne: seçilen nesnelerin alanı (öntanımlı); nokta: köşeleri gösterilen alan |
| `noktalar` | point_list | en az 0 | yontem=nokta için alanın köşeleri; verilirse yöntem kendiliğinden nokta olur |

Ayrıntılı kullanım: [ALANÖLÇ](measure_area.md)

### `core.coordinate` — KOORDİNAT (Koordinat Oku)

Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nokta` | point | 1 | Okunacak nokta |

Ayrıntılı kullanım: [KOORDİNAT](coordinate.md)

### `core.pan` — KAYDIR (Kaydır)

Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `baslangic` | point | 1 | Kaydırmanın tutulacağı nokta |
| `bitis` | point | 1 | O noktanın taşınacağı yer |

Ayrıntılı kullanım: [KAYDIR](pan.md)

### `core.offset` — OFSET (Ofset)

Seçili nesnelerin verilen mesafede, gösterilen tarafta paralelini çizer: açık çizgiye tek yanda çizgi, alana delikleriyle alan, daireye daire.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Ofseti alınacak nesneler; yoksa etkin seçim |
| `mesafe` | integer | isteğe bağlı | Paralel mesafesi, milimetre. Taraf verilmez ve gösterilmezse işaret anlam taşır: kapalı şekilde artı dışarı, eksi içeri |
| `kose` | text | isteğe bağlı | KÖŞE | YUVARLAK | PAH — dış köşenin biçimi |
| `taraf` | text | isteğe bağlı | Paralelin tarafı: açık çizgide sol ya da sag (çizim yönüne göre), kapalı şekilde dis ya da ic, iki her iki yan |
| `nokta` | point | isteğe bağlı | Tarafı gösteren nokta: her nesnenin paraleli bu noktanın olduğu yana düşer |
| `kaynak` | text | isteğe bağlı | Kaynak nesne: koru (öntanımlı) ya da paralel çizilince sil |
| `ozellik` | text | isteğe bağlı | Paralelin katmanı ve stili: kaynak nesneninki (öntanımlı) ya da etkin katman |
| `oznitelik` | text | isteğe bağlı | Kaynağın öznitelik değerleri: paralele aktar (öntanımlı) ya da aktarma — parselin içine çizilen çekme hattı gibi kaynağın kendisi olmayan bir çizgi için |

Ayrıntılı kullanım: [OFSET](offset.md)

### `core.sector` — DİLİM (Daire Dilimi)

Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | point | 1 | Dilimin merkezi |
| `baslangic` | point | 1 | İlk kenarın ucu; yarıçapı bu belirler |
| `bitis` | point | 1 | İkinci kenarın yönü; süpürme saat yönünün tersinedir |

Ayrıntılı kullanım: [DİLİM](sector.md)

### `core.annulus` — HALKA (Halka)

Merkez, iç ve dış yarıçaptan delikli halka çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | point | 1 | Halkanın merkezi |
| `ic` | point | 1 | İç çember üzerinde bir nokta |
| `dis` | point | 1 | Dış çember üzerinde bir nokta |

Ayrıntılı kullanım: [HALKA](annulus.md)

### `core.ellipse_draw` — ELİPS (Elips)

Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | point_list | isteğe bağlı | Elipsin merkezi |
| `birinci` | point_list | isteğe bağlı | merkez: birinci eksenin ucu · eksen: birinci eksenin bir ucu |
| `ikinci` | point_list | isteğe bağlı | İkinci eksenin uzaklığı; eksene dik ölçülür |
| `yontem` | text | isteğe bağlı | merkez: merkez + eksen ucu · eksen: eksenin iki ucu |
| `ikinci_uc` | point_list | isteğe bağlı | eksen: birinci eksenin öteki ucu |
| `baslangic` | number | isteğe bağlı | Kısmi elips: başlangıç açısı, derece, birinci eksenden saat yönünün tersine |
| `bitis` | number | isteğe bağlı | Kısmi elips: bitiş açısı, derece; baslangic ile birlikte |

Ayrıntılı kullanım: [ELİPS](ellipse_draw.md)

### `core.spline` — SPLINE (Spline)

Kontrol noktalarından NURBS eğrisi (spline) çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 2 | Kontrol noktaları |
| `derece` | integer | isteğe bağlı | Eğrinin derecesi, 1–15; varsayılan 3 |
| `kapali` | bool | isteğe bağlı | Son noktadan ilkine kapansın mı; varsayılan hayır |

Ayrıntılı kullanım: [SPLINE](spline.md)

### `core.hatch` — TARAMA (Tarama)

Kapalı nesnelerin ya da verilen köşelerin içini katalogdaki bir desenle tarar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 0 | Sınır köşeleri, nesne seçmek yerine; en az üç nokta |
| `nesneler` | selection | en az 0 | Sınırı verecek kapalı nesneler; yoksa etkin seçim ya da noktalar= |
| `desen` | text | isteğe bağlı | Katalogdaki desen adı: SOLID, ANSI31, NET…; varsayılan SOLID |
| `aci` | number | isteğe bağlı | Desenin dönme açısı, derece; varsayılan 0 |
| `olcek` | number | isteğe bağlı | Desen ölçeği; varsayılan pafta ölçeğinin paydası (AYAR plan_ölçeği) |
| `katalog` | text | isteğe bağlı | Desen kataloğu dosyası; varsayılan TERCİH desen_kataloğu |

Ayrıntılı kullanım: [TARAMA](hatch.md)

### `core.block` — BLOK (Blok Tanımla)

Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | 1 | Bloğun adı; Türkçe katlanmış hâliyle benzersiz |
| `taban` | point | 1 | Taban noktası: referansların yerleştirildiği nokta |
| `nesneler` | selection | en az 0 | Bloğa girecek nesneler; yoksa etkin seçim |
| `aciklama` | text | isteğe bağlı | Serbest açıklama |

Ayrıntılı kullanım: [BLOK](block.md)

### `core.insert` — BLOKEKLE (Blok Ekle)

Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | 1 | Yerleştirilecek bloğun adı |
| `nokta` | point | 1 | Ekleme noktası |
| `olcek` | number | isteğe bağlı | Ölçek; eksi değer x'te aynalar; varsayılan 1 |
| `olcek_y` | number | isteğe bağlı | Y ölçeği, farklıysa; varsayılan olcek |
| `aci` | number | isteğe bağlı | Dönme açısı, derece; varsayılan 0 |
| `sutun` | integer | isteğe bağlı | Dizi sütun sayısı; varsayılan 1 |
| `satir` | integer | isteğe bağlı | Dizi satır sayısı; varsayılan 1 |
| `sutun_aralik` | integer | isteğe bağlı | Sütunlar arası, milimetre, döndürülmüş eksende |
| `satir_aralik` | integer | isteğe bağlı | Satırlar arası, milimetre, döndürülmüş eksende |

Ayrıntılı kullanım: [BLOKEKLE](insert.md)

### `core.dimension` — ÖLÇÜ (Ölçü)

İki nokta arasını, bir yarıçapı, çapı ya da açıyı ölçüp yazısı ve oklarıyla çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `birinci` | point | 1 | Birinci nokta; açısal ölçüde birinci kolun ucu |
| `ikinci` | point | 1 | İkinci nokta; açısal ölçüde ikinci kolun ucu |
| `konum` | point | 1 | Ölçü çizgisinin yeri; açısal ölçüde yayın geçtiği nokta |
| `tur` | text | isteğe bağlı | hizali (varsayılan), dogrusal, yaricap, cap, acisal, koordinat, yay |
| `tepe` | point_list | isteğe bağlı | Açısal ölçünün tepe noktası |
| `bitis` | point_list | isteğe bağlı | Yay uzunluğu ölçüsünün bitiş noktası |
| `stil` | text | isteğe bağlı | Katalogdaki ölçü stili: ISO-25 (varsayılan), STANDARD, MIMARI |
| `metin` | text | isteğe bağlı | Ölçülen değer yerine yazılacak metin |
| `katalog` | text | isteğe bağlı | Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri |

Ayrıntılı kullanım: [ÖLÇÜ](dimension.md)

### `core.leader` — LİDER (Lider)

Bir noktayı gösteren oklu çizgi çizer, istenirse yanına yazı koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 2 | Okun ucundan yazının yanına köşeler |
| `metin` | text | isteğe bağlı | Son köşenin yanına yazılacak metin |
| `stil` | text | isteğe bağlı | Ok ve yazı boyunu veren ölçü stili; varsayılan ISO-25 |
| `katalog` | text | isteğe bağlı | Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri |

Ayrıntılı kullanım: [LİDER](leader.md)

### `core.points` — NOKTALAR (Nokta Listesi)

Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Nokta listesi dosyasının yolu |
| `yon` | text | isteğe bağlı | oku (varsayılan) | yaz |
| `nesneler` | selection | en az 0 | yon=yaz ile: köşeleri yazılacak nesneler; verilmezse çizimdeki noktalar |
| `eksen` | text | isteğe bağlı | Sütun sırası: YX (varsayılan, Türkiye'de olağan) | XY |

Ayrıntılı kullanım: [NOKTALAR](points.md)

### `core.guide` — KILAVUZ (Kılavuz)

Cetvel kılavuzu ve açılı kılavuz ekler, listeler ve siler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `yon` | text | isteğe bağlı | yatay | düşey | bir açı (45, 45g, 30d); yoksa kılavuzlar listelenir |
| `deger` | integer | isteğe bağlı | Kılavuzun koordinatı, milimetre — yatayda yukarı, düşeyde sağa |
| `nokta` | point_list | isteğe bağlı | Açılı kılavuzun geçtiği nokta; yalnız `yon` bir açıysa |
| `tur` | text | isteğe bağlı | doğru: iki yöne sonsuz · ışın: noktadan ileriye |
| `sil` | bool | isteğe bağlı | Verilen yerdeki kılavuzu siler |

Ayrıntılı kullanım: [KILAVUZ](guide.md)

### `core.attribute` — ÖZNİTELİK (Öznitelik)

Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Öznitelik kimliği; yoksa tanımlı sütunlar listelenir |
| `nesne` | integer | isteğe bağlı | Nesnenin kalıcı kimliği |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur. 'yok' hücreyi boşaltır |

Ayrıntılı kullanım: [ÖZNİTELİK](attribute.md)

### `core.column` — SÜTUN (Sütun)

Öznitelik sütunu tanımlar, düzenler, siler; argümansız çağrılınca listeler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `kimlik` | text | isteğe bağlı | Sütun kimliği; yoksa tanımlı sütunlar listelenir |
| `tur` | text | isteğe bağlı | tam_sayi, ondalik, uzunluk, evet_hayir, metin, tarih, kod |
| `ad` | text | isteğe bağlı | Panelde görünen Türkçe ad |
| `aciklama` | text | isteğe bağlı | Tek satırlık açıklama |
| `zorunlu` | bool | isteğe bağlı | Her satır bir değer taşımalı mı |
| `katalog` | text | isteğe bağlı | Yalnız 'kod' türü için: katalog kimliği |
| `basamak` | integer | isteğe bağlı | Yalnız 'ondalik' için: noktadan sonraki basamak sayısı |
| `katman` | text | isteğe bağlı | Sütunu yalnız bu katmana tanımlar; yoksa proje geneli |
| `sil` | bool | isteğe bağlı | Sütunu ve içindeki bütün değerleri siler |

Ayrıntılı kullanım: [SÜTUN](column.md)

### `core.erase` — SİL (Sil)

Seçilen nesneleri siler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Silinecek nesnelerin kimlikleri; yoksa etkin seçim |

Ayrıntılı kullanım: [SİL](erase.md)

### `core.select` — SEÇ (Seç)

Nesneleri seçer: tümü, kimlikle, katman, pencere, kesen kutu, çokgen, çit, önceki seçim, son nesne ya da tek nokta.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `mod` | text | isteğe bağlı | TÜMÜ | TEMİZLE | NESNE | KATMAN | PENCERE | KESEN | KUTU | NOKTA | ÇOKGEN | ÇOKGENKESEN | ÇİT | ÖNCEKİ | SON |
| `noktalar` | point_list | en az 0 | Kutu köşeleri (iki nokta), çokgen/çit köşeleri ya da tek tıklama noktası |
| `tur` | text | isteğe bağlı | Yalnız bu türdeki nesneler: ÇOKLUÇİZGİ, DAİRE, YAY, NOKTA, ELİPS… |
| `nesneler` | selection | en az 0 | NESNE modunda nesne kimlikleri |
| `katman` | text | isteğe bağlı | KATMAN modunda katman adı |
| `islem` | text | isteğe bağlı | DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE |
| `tolerans` | number | isteğe bağlı | NOKTA modunda arama yarıçapı, metre; yoksa seçim toleransı |
| `sira` | number | isteğe bağlı | NOKTA modunda kaçıncı nesne: 1 en yakını, 2 altındaki |

Ayrıntılı kullanım: [SEÇ](select.md)

### `core.label` — ETİKET (Etiket)

Katmandaki nesneleri özniteliklerinden okuyarak etiketler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `katman` | text | 1 | Etiketlenecek katmanın adı |
| `bicim` | text | isteğe bağlı | Etiket biçimi; {sutun} o sütunun değeriyle değişir, \n satır kırar. Sembol alan bildiriyorsa gerekmez |
| `hedef` | text | isteğe bağlı | Etiketlerin yazılacağı katman; yoksa '<katman> ETİKET' |
| `yukseklik` | integer | isteğe bağlı | Yazı yüksekliği, zemin milimetresi |
| `kaydirma` | integer | isteğe bağlı | Nesnenin ortasından dikey kaydırma, zemin milimetresi; artı yukarı |

Ayrıntılı kullanım: [ETİKET](label.md)

### `core.layer` — KATMAN (Katman)

Katman oluşturur, aktif yapar ve özelliklerini değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | 1 | Katman adı; yoksa oluşturulur ve aktif yapılır |
| `grup` | text | isteğe bağlı | Katman ağacındaki yer, düzeyler '>' ile ayrılır; boş = kök |
| `gorunur` | bool | isteğe bağlı | Katmanın görünürlüğü |
| `kilitli` | bool | isteğe bağlı | Katmanın kilit durumu |
| `renk` | integer | isteğe bağlı | Çizim rengi, 0xAARRGGBB |

Ayrıntılı kullanım: [KATMAN](layer.md)

### `core.layer_visibility` — KATMANGÖRÜNÜM (Katman Görünümü)

Katmanların görünürlüğünü toptan değiştirir: bir katmanı gösterir ya da gizler, yalnız onu bırakır, hepsini gösterir veya görünürlüğü ters çevirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | goster, gizle, yalniz (yalnız bu katman), tumu (hepsini göster) ya da tersine |
| `katman` | text | isteğe bağlı | Katman adı; goster, gizle ve yalniz için gerekir, tersine için isteğe bağlı (verilmezse bütün katmanlar), tumu ile verilemez |

Ayrıntılı kullanım: [KATMANGÖRÜNÜM](layer_visibility.md)

### `core.layout` — ÇIKTIYERLEŞİMİ (Çıktı Yerleşimi)

Çizimin çıktı yerleşimlerini yönetir: yeni yerleşim açar, siler, adlandırır ve kâğıdını değiştirir. Yerleşim çizimle birlikte kaydedilir ve geri alınabilir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | Ne yapılacağı |
| `ad` | text | isteğe bağlı | Yerleşimin adı; listele dışında gerekir |
| `yeni_ad` | text | isteğe bağlı | islem=ad için yeni yerleşim adı |
| `kagit` | text | isteğe bağlı | A5, A4, A3, A2, A1, A0 ya da ozel (varsayılan A4) |
| `genislik` | integer | isteğe bağlı | ozel kâğıt için sayfa genişliği |
| `yukseklik` | integer | isteğe bağlı | ozel kâğıt için sayfa yüksekliği |
| `yon` | text | isteğe bağlı | Sayfa yönü (varsayılan dikey) |
| `kenar` | integer | isteğe bağlı | Kenar boşluğu (varsayılan 10) |
| `dpi` | integer | isteğe bağlı | Çıktı çözünürlüğü (varsayılan 300) |
| `sayfa` | integer | isteğe bağlı | Hangi sayfa (1'den başlar). sayfa işleminde verilmezse bütün sayfalar değişir |
| `yeni_sira` | integer | isteğe bağlı | sayfatasi için sayfanın gideceği sıra |
| `katman` | text | isteğe bağlı | atlas: hangi katmanın nesneleri için bir sayfa basılacak; 'yok' atlası kapatır |
| `sirala` | text | isteğe bağlı | atlas: sayfaların sıralanacağı ve adlandırılacağı öznitelik sütunu; verilmezse nesne anahtarı |
| `grup` | text | isteğe bağlı | rapor: bölümlerin oluşturulacağı öznitelik sütunu (ada_no gibi); verilmezse tek bölüm |
| `kenar_payi` | integer | isteğe bağlı | atlas: nesnenin çevresinde bırakılacak pay, yüzde (varsayılan 10) |
| `tek_dosya` | bool | isteğe bağlı | atlas: tek çok sayfalı belge mi, nesne başına bir dosya mı (varsayılan evet) |

Ayrıntılı kullanım: [ÇIKTIYERLEŞİMİ](layout.md)

### `core.layout_item` — ÇIKTIÖĞE (Çıktı Öğesi)

Bir çıktı yerleşiminin üzerindeki öğeleri yönetir: harita çerçevesi, başlık, ölçek çubuğu, kuzey oku, lejant, resim, şekil ve tablo ekler, taşır, ayarlar ve siler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | Ne yapılacağı |
| `yerlesim` | text | isteğe bağlı | Hangi çıktı yerleşimi; çizimde tek yerleşim varsa gerekmez |
| `ad` | text | isteğe bağlı | Öğe adı; ekle dışında gerekir, ekle'de verilmezse türetilir |
| `tur` | text | isteğe bağlı | islem=ekle için öğe türü |
| `x` | number | isteğe bağlı | Sol kenardan uzaklık |
| `y` | number | isteğe bağlı | ÜST kenardan uzaklık |
| `genislik` | number | isteğe bağlı | Genişlik |
| `yukseklik` | number | isteğe bağlı | Yükseklik |
| `metin` | text | isteğe bağlı | Metin öğesinin yazısı; <yerlesim>, <olcek>, <tarih>, <crs> yer tutucuları çizim anında çözülür |
| `yazi` | number | isteğe bağlı | Yazı yüksekliği |
| `olcek` | integer | isteğe bağlı | Harita öğesinin ölçeği 1:N; 0 kapsama uyar |
| `pencere` | point_list | 0–2 | Harita çerçevesinin bakacağı alanın iki köşesi, anahtar iki kez yazılarak: pencere=x1,y1 pencere=x2,y2. Tuvalden çerçeve seçmek bu satırı yazar |
| `izgara` | text | isteğe bağlı | Harita öğesinin koordinat ızgarası |
| `izgara_aralik` | integer | isteğe bağlı | Izgara aralığı, zemin milimetresi; 0 ölçeğe göre seçilir |
| `kilit` | bool | isteğe bağlı | Öğeyi taşımaya kapatır |
| `cerceve` | bool | isteğe bağlı | Öğenin çevresine çerçeve çizer |
| `sayfa` | integer | isteğe bağlı | Öğenin duracağı sayfa (1'den başlar); tasi ile verilir |
| `yeni_ad` | text | isteğe bağlı | islem=ad için öğenin yeni adı |
| `satir_siniri` | integer | isteğe bağlı | Tablo öğesinin yazacağı en çok satır; 0 = kutuya kaç satır sığıyorsa o kadar |
| `sutunlar` | text | 0–64 | Tablo öğesinin yazacağı öznitelik sütunları, sırasıyla; anahtar birden çok kez yazılır. Verilmezse katmanın bütün sütunları, 'hepsi' listeyi boşaltır |
| `katmanlar` | text | 0–64 | Harita çerçevesinin çizeceği katmanlar; anahtar birden çok kez yazılır. Verilmezse görünür bütün katmanlar, 'hepsi' listeyi boşaltır |
| `harita` | text | isteğe bağlı | Bu öğenin bağlı olduğu harita çerçevesinin adı. Verilmezse ilk harita. 'ilk' bağı kaldırır |
| `sira` | integer | isteğe bağlı | Çizim sırası; büyük olan üstte |

Ayrıntılı kullanım: [ÇIKTIÖĞE](layout_item.md)

### `core.layout_template` — ÇIKTIŞABLON (Çıktı Şablonu)

Kurumun standart çıktı yerleşimlerini saklar ve uygular. Şablon çizimin dışında, kullanıcı profilinde durur; her çizime uygulanabilir. Şablon düzeni taşır, zemin koordinatlarını taşımaz.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | Ne yapılacağı |
| `ad` | text | isteğe bağlı | Şablonun adı; listele dışında gerekir |
| `yerlesim` | text | isteğe bağlı | kaydet: hangi yerleşim saklanacak (tek yerleşim varsa gerekmez). uygula: kurulacak yerleşimin adı (verilmezse şablonun adı) |

Ayrıntılı kullanım: [ÇIKTIŞABLON](layout_template.md)

### `core.style` — STİL (Stil)

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
| `alan` | text | isteğe bağlı | Nesneden alınacak parametreler, virgülle: sütun[:özellik[:tür]] — 'kod:yazi:metin, kat:kalinlik'. Sütun yoksa tanımlanır |

Ayrıntılı kullanım: [STİL](style.md)

### `core.symbol` — SEMBOL (Sembol)

Gösterim rafını yükler, ağacında gezer ve içinde arar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `paket` | text | isteğe bağlı | Yüklenecek gösterim paketinin dosya yolu |
| `grup` | text | isteğe bağlı | Gezilecek grup yolu, düzeyler '>' ile ayrılır |
| `ara` | text | isteğe bağlı | Etikette, kimlikte ve grup yolunda arar |
| `kod` | text | isteğe bağlı | Tek bir gösterimin ayrıntısı |

Ayrıntılı kullanım: [SEMBOL](symbol.md)

### `core.zoom` — YAKINLAŞ (Yakınlaş)

Görünümü çizim kapsamına veya verilen çarpana ayarlar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `mod` | text | isteğe bağlı | KAPSAM | ÇARPAN | SIFIRLA |
| `carpan` | number | isteğe bağlı | ÇARPAN modunda ölçek katsayısı |

Ayrıntılı kullanım: [YAKINLAŞ](zoom.md)

### `core.undo` — GERİAL (Geri Al)

Son işlemi geri alır.

Parametre almaz.

Ayrıntılı kullanım: [GERİAL](undo.md)

### `core.redo` — YİNELE (Yinele)

Geri alınan işlemi yineler.

Parametre almaz.

Ayrıntılı kullanım: [YİNELE](redo.md)

### `core.new` — YENİ (Yeni)

Boş bir çizim açar; ekrandaki çizimin yerine geçer.

Parametre almaz.

Ayrıntılı kullanım: [YENİ](new.md)

### `core.open` — AÇ (Aç)

Bir KentOSCad proje dosyasını açar ve çizimin yerine koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Açılacak KentOSCad proje dosyasının yolu (.pcad) |

Ayrıntılı kullanım: [AÇ](open.md)

### `core.save` — KAYDET (Kaydet)

Çizimi bağlı olduğu KentOSCad proje dosyasına kaydeder.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | isteğe bağlı | Hedef yol; verilmezse çizimin bağlı olduğu dosyaya yazılır |

Ayrıntılı kullanım: [KAYDET](save.md)

### `core.saveas` — FARKLIKAYDET (Farklı Kaydet)

Çizimi yeni bir KentOSCad proje dosyasına kaydeder ve ona bağlar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Yeni proje dosyasının yolu (.pcad) |

Ayrıntılı kullanım: [FARKLIKAYDET](saveas.md)

### `core.import` — İÇEAKTAR (İçe Aktar)

Dış bir veri dosyasını çizime ekler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | İçe aktarılacak dosyanın yolu |
| `bicim` | text | isteğe bağlı | Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur |
| `katmanlar` | text | isteğe bağlı | Yalnızca bu katmanlar okunur, virgülle ayrılır; verilmezse tümü |
| `alanlar` | text | isteğe bağlı | Sütun olarak okunacak öznitelik alanları, virgülle; * hepsi; verilmezse alan okunmaz |

Ayrıntılı kullanım: [İÇEAKTAR](import.md)

### `core.export` — DIŞAAKTAR (Dışa Aktar)

Çizimi dış bir veri biçimine yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Yazılacak dosyanın yolu |
| `bicim` | text | isteğe bağlı | Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur |
| `surum` | integer | isteğe bağlı | DXF sürümü: 2000, 2004, 2007 (varsayılan), 2010, 2013, 2018 |

Ayrıntılı kullanım: [DIŞAAKTAR](export.md)

### `core.script` — BETİK (Betik Çalıştır)

Bir betik dosyasını komut veri yolu üzerinden çalıştırır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Çalıştırılacak betik dosyasının yolu |

Ayrıntılı kullanım: [BETİK](script.md)

### `core.python` — PYTHON (Python Çalıştır)

Bir Python parçacığını komut veri yolu üzerinden çalıştırır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `kod` | text | 1 | Çalıştırılacak Python kaynağı |

Ayrıntılı kullanım: [PYTHON](python.md)

### `core.database` — VERİTABANI (Veritabanı)

PostGIS veritabanına bağlanır; katmanları tablo, projeleri kayıt olarak yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | baglan | kes | tablolar | katmanyaz | projekaydet | projeac | projeler | projesil |
| `hedef` | text | isteğe bağlı | baglan: bağlantı dizesi; katmanyaz: tablo adı; proje işlemleri: proje adı |
| `katman` | text | isteğe bağlı | katmanyaz: yazılacak katman; yoksa etkin katman |

Ayrıntılı kullanım: [VERİTABANI](database.md)

### `core.print` — YAZDIR (Yazdır)

Çizimin bir penceresini bir yazdırma profilinin kâğıdına yerleştirip PDF dosyasına yazar ya da yazıcıya gönderir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `pencere` | point_list | 0–2 | Yazdırılacak alanın iki köşesi; merkez verilmezse ve bu da verilmezse tıklatılır |
| `merkez` | point | isteğe bağlı | Kâğıdın ortalanacağı nokta; pencere yerine kullanılır |
| `olcek` | integer | isteğe bağlı | Ölçek paydası (1000 = 1/1000); merkez ile kullanılır, verilmezse projenin plan ölçeği |
| `yerlesim` | text | isteğe bağlı | Basılacak çıktı yerleşiminin adı (ÇIKTIYERLEŞİMİ ile kurulur). Verildiğinde kâğıt, kenar ve harita penceresi yerleşimden gelir; pencere, merkez, olcek ve profil ile birlikte verilmez |
| `dosya` | text | isteğe bağlı | PDF yazılacak dosya; yazici ile birlikte verilmez |
| `yazici` | text | isteğe bağlı | Yazıcının adı; "" sistem varsayılanı. dosya ile birlikte verilmez |
| `profil` | text | isteğe bağlı | Yazdırma profili; verilmezse varsayılan profil |
| `kagit` | text | isteğe bağlı | Kâğıt: A5, A4, A3, A2, A1, A0 ya da ozel (genislik ve yukseklik ile) |
| `genislik` | integer | isteğe bağlı | ozel kâğıdın eni, milimetre (dikey duruşta) |
| `yukseklik` | integer | isteğe bağlı | ozel kâğıdın boyu, milimetre (dikey duruşta) |
| `yon` | text | isteğe bağlı | dikey ya da yatay |
| `dpi` | integer | isteğe bağlı | Çözünürlük, inç başına nokta (72–4800) |
| `kenar` | integer | isteğe bağlı | Dört yandaki kenar boşluğu, milimetre |
| `baslik` | text | isteğe bağlı | PDF belge başlığı |
| `yazar` | text | isteğe bağlı | PDF yazar alanı |
| `sifre` | text | isteğe bağlı | PDF açma şifresi (kullanıcı şifresi); günlüğe yazılmaz |
| `sahip_sifresi` | text | isteğe bağlı | PDF izinlerini değiştirme şifresi (sahip şifresi); günlüğe yazılmaz |
| `yazdirilabilir` | bool | isteğe bağlı | Şifreli PDF: sahip şifresi olmayan yazdırabilir mi; varsayılan evet |
| `kopyalanabilir` | bool | isteğe bağlı | Şifreli PDF: metin ve grafik kopyalanabilir mi; varsayılan evet |
| `degistirilebilir` | bool | isteğe bağlı | Şifreli PDF: belge değiştirilebilir mi; varsayılan evet |

Ayrıntılı kullanım: [YAZDIR](print.md)

### `core.print_profile` — YAZDIRMAPROFİLİ (Yazdırma Profili)

Yazdırma profillerini listeler, ekler, siler ya da birini varsayılan yapar; profil kâğıdı, yönü, çözünürlüğü ve kenar boşluğunu taşır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | listele, ekle, sil ya da varsayilan |
| `ad` | text | isteğe bağlı | Profilin adı (ekle, sil, varsayilan) |
| `kagit` | text | isteğe bağlı | Kâğıt: A5, A4, A3, A2, A1, A0 ya da ozel; ekle için, varsayılan A4 |
| `genislik` | integer | isteğe bağlı | ozel kâğıdın eni, milimetre |
| `yukseklik` | integer | isteğe bağlı | ozel kâğıdın boyu, milimetre |
| `yon` | text | isteğe bağlı | dikey ya da yatay; varsayılan dikey |
| `dpi` | integer | isteğe bağlı | Çözünürlük; varsayılan 300 |
| `kenar` | integer | isteğe bağlı | Kenar boşluğu, milimetre; varsayılan 10 |

Ayrıntılı kullanım: [YAZDIRMAPROFİLİ](print_profile.md)

### `core.setting` — AYAR (Proje Ayarı)

Proje ayarlarını listeler, okur ve değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Ayar adı veya kimliği; yoksa liste |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur |

Ayrıntılı kullanım: [AYAR](setting.md)

### `core.preference` — TERCİH (Uygulama Tercihi)

Uygulama tercihlerini listeler, okur ve değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Tercih adı veya kimliği; yoksa liste |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur |

Ayrıntılı kullanım: [TERCİH](preference.md)

### `core.mode` — MOD (Çizim Modları)

Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur ve değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Mod adı veya kimliği; yoksa liste |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur |

Ayrıntılı kullanım: [MOD](mode.md)

### `core.help` — YARDIM (Komut Listesi)

Komut listesini veya tek bir komutun ayrıntısını gösterir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `komut` | text | isteğe bağlı | Ayrıntısı istenen komut adı |

Ayrıntılı kullanım: [YARDIM](help.md)

### `islem.tampon` — TAMPON

Kapsamdaki nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer: çizginin iki yanı, noktanın çevresi, alanın dışı; üst üste binen tamponlar tek alan olur.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz |
| `kapsam` | text | isteğe bağlı | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `pencere` | point_list | 0–2 | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir |
| `katman` | text | isteğe bağlı | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |
| `mesafe` | number | isteğe bağlı | Tampon mesafesi, metre; eksi değer yalnız alanları içeri aşındırır |
| `birlestir` | bool | isteğe bağlı | Üst üste binen tamponları tek alanda birleştir; kapalıysa her nesnenin tamponu ayrı alan olur; varsayılan evet |
| `kose` | text | isteğe bağlı | Dış köşelerin biçimi (yuvarlak / koseli / pah); varsayılan yuvarlak |
| `uc` | text | isteğe bağlı | Çizgi uçlarının biçimi (yuvarlak / duz / kare); varsayılan yuvarlak |

Ayrıntılı kullanım: [TAMPON](tampon.md)

### `islem.alan_duzenle` — ALANDÜZENLE

Kapalı bir alanı istenen alana getirir: bütün kenarları eşit daraltıp genişleterek, bir kenarı kaydırarak ya da bir köşeyi çekerek; şekil bozulmaz.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz |
| `kapsam` | text | isteğe bağlı | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `pencere` | point_list | 0–2 | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir |
| `katman` | text | isteğe bağlı | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |
| `alan` | number | isteğe bağlı | Hedef alan, metrekare |
| `mod` | text | isteğe bağlı | Nasıl getirileceği (hepsi / kenar / kose); varsayılan hepsi |
| `kenar` | integer | isteğe bağlı | Kaydırılacak kenar (ilk köşeden çıkan kenar 1); mod=kenar |
| `kose` | integer | isteğe bağlı | Çekilecek köşe; mod=kose |
| `nokta` | point | isteğe bağlı | Kenarın ya da köşenin gideceği yer; verilmezse arayüz sürükletir, komut satırı hedefe tam oturtur |

Ayrıntılı kullanım: [ALANDÜZENLE](alan_duzenle.md)

### `islem.uzunluk_yaz` — UZUNLUKYAZ

Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel bir yazı olarak yazar; yazı kenara bağlıdır, kenar değişince izler ve yenilenir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz |
| `kapsam` | text | isteğe bağlı | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `pencere` | point_list | 0–2 | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir |
| `katman` | text | isteğe bağlı | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |
| `birim` | text | isteğe bağlı | Uzunluğun yazılacağı birim (metre / santimetre / milimetre / kilometre); varsayılan metre |
| `ondalik` | integer | isteğe bağlı | Virgülden sonraki basamak sayısı; varsayılan 2 |
| `bicim` | text | isteğe bağlı | Yazının kalıbı; {} sayının yerini tutar (örnek: "{} m", "L={}") |
| `ayrac` | text | isteğe bağlı | Ondalık ayracı (virgul / nokta); varsayılan virgul |
| `taraf` | text | isteğe bağlı | Yazının kenarın hangi yanına düşeceği (otomatik / sol / sag / dis / ic); varsayılan otomatik |
| `yukseklik` | integer | isteğe bağlı | Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm; varsayılan 0 |
| `bosluk` | integer | isteğe bağlı | Kenar ile yazı arası, milimetre; 0 = yüksekliğin yarısı; varsayılan 0 |
| `enaz` | integer | isteğe bağlı | Bundan kısa kenarlara yazı yazılmaz, milimetre; varsayılan 0 |
| `bagla` | bool | isteğe bağlı | Yazıyı kenarına bağla: kenar taşınınca yazı izler, uzunluk yeniden yazılır; varsayılan evet |

Ayrıntılı kullanım: [UZUNLUKYAZ](uzunluk_yaz.md)

### `islem.kose_numarala` — KÖŞENUMARALA

Kapsamdaki her alanın (ve çizginin) köşelerini seçilen köşeden başlayarak sırayla numaralar ve numarayı köşenin dışına yazar; numara köşesine bağlıdır, köşe taşınınca izler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz |
| `kapsam` | text | isteğe bağlı | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `pencere` | point_list | 0–2 | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir |
| `katman` | text | isteğe bağlı | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |
| `baslangic` | point | isteğe bağlı | Sayımın başlayacağı köşeye en yakın nokta; verilmezse ilk köşe |
| `yon` | text | isteğe bağlı | Sayım yönü (ters / saat); varsayılan ters |
| `onek` | text | isteğe bağlı | Numaranın önüne gelen yazı (örnek: A, K-) |
| `basamak` | integer | isteğe bağlı | Numaranın en az basamak sayısı; eksikler dolgu ile tamamlanır; varsayılan 0 |
| `dolgu` | text | isteğe bağlı | Basamak dolgusu; varsayılan 0 |
| `ilk` | integer | isteğe bağlı | İlk köşenin numarası; varsayılan 1 |
| `sonek` | text | isteğe bağlı | Numaranın arkasına gelen yazı |
| `yukseklik` | integer | isteğe bağlı | Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm; varsayılan 0 |
| `bosluk` | integer | isteğe bağlı | Köşe ile yazı arası, milimetre; 0 = yüksekliğin yarısı; varsayılan 0 |
| `bagla` | bool | isteğe bağlı | Numarayı köşesine bağla: köşe taşınınca numara izler; varsayılan evet |

Ayrıntılı kullanım: [KÖŞENUMARALA](kose_numarala.md)

### `islem.bag_coz` — BAĞÇÖZ

Kapsamdaki yazıların bağını çözer: yazı yerinde kalır, bağlı olduğu nesne bundan sonra tek başına taşınır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz |
| `kapsam` | text | isteğe bağlı | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `pencere` | point_list | 0–2 | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir |
| `katman` | text | isteğe bağlı | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |

Ayrıntılı kullanım: [BAĞÇÖZ](bag_coz.md)

### `islem.bagla` — BAĞLA

Kapsamdaki yazıları seçilen nesnenin en yakın kenarına ya da köşesine bağlar: nesne taşınınca yazı izler; istenirse yazı kenarın uzunluğu olur.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz |
| `kapsam` | text | isteğe bağlı | secili (varsayılan), gorunum ya da proje: nesneler nereden alınır |
| `pencere` | point_list | 0–2 | gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir |
| `katman` | text | isteğe bağlı | Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman |
| `kaynak` | selection | isteğe bağlı | Yazıların bağlanacağı nesne (çizgi ya da alan) |
| `bag` | text | isteğe bağlı | Neye bağlanacağı: en yakın kenar ya da en yakın köşe (kenar / kose); varsayılan kenar |
| `tur` | text | isteğe bağlı | Yazının sözü: kendi yazısı kalır ya da kenarın uzunluğu olur (sabit / uzunluk); varsayılan sabit |
| `birim` | text | isteğe bağlı | Uzunluğun birimi (tur=uzunluk) (metre / santimetre / milimetre / kilometre); varsayılan metre |
| `ondalik` | integer | isteğe bağlı | Virgülden sonraki basamak sayısı (tur=uzunluk); varsayılan 2 |
| `bicim` | text | isteğe bağlı | Uzunluk yazısının kalıbı; {} sayının yerini tutar (tur=uzunluk) |
| `ayrac` | text | isteğe bağlı | Ondalık ayracı (tur=uzunluk) (virgul / nokta); varsayılan virgul |

Ayrıntılı kullanım: [BAĞLA](bagla.md)

### `core.fit` — OTURT (Oturt (Helmert))

Yerel ölçülmüş çizimi kontrol noktalarıyla haritaya oturtur (2B Helmert).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 4 | Kontrol çiftleri: yerel, harita, yerel, harita... |
| `olcek_kilitli` | bool | isteğe bağlı | Ölçeği 1'de tutar; saha ölçüsü yeniden ölçeklenmez |
| `sistem` | text | isteğe bağlı | Oturtulduktan sonraki koordinat sistemi, örnek TUREF/TM36 |

Ayrıntılı kullanım: [OTURT](fit.md)

### `core.stakeout` — APLİKASYON (Aplikasyon)

İstasyondan her noktaya mesafe ve açı listesi çıkarır (aplikasyon).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `istasyon` | point | 1 | Aletin durduğu nokta |
| `baglama` | point_list | isteğe bağlı | Bağlama (arka görüş) noktası; verilirse açılar ondan ölçülür |
| `nesneler` | selection | en az 0 | Aplike edilecek noktalar; yoksa seçim, o da boşsa çizimdeki bütün noktalar |

Ayrıntılı kullanım: [APLİKASYON](stakeout.md)

### `core.reproject` — DÖNÜŞTÜR (Dönüştür)

Çizimin tamamını bir koordinat sisteminden diğerine dönüştürür.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `hedef` | text | 1 | Hedef koordinat sistemi, örnek EPSG:5256 ya da TUREF/TM36 |
| `kaynak` | text | isteğe bağlı | Kaynak sistem; yoksa çizimin kendi koordinat sistemi |

Ayrıntılı kullanım: [DÖNÜŞTÜR](reproject.md)

### `geodesy.traverse` — POLİGON (Poligon Hesabı)

Kırılma açısı ve kenarlardan poligon koordinatları hesaplar, kapanma hatalarını dağıtır ve mevzuat toleransına karşı denetler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `baslangic` | point | 1 | Başlangıç istasyonu (bilinen) |
| `baglama` | point | 1 | Başlangıçtaki bağlama noktası (bilinen) |
| `aci` | number | en az 0 | Her istasyonda okunan kırılma açısı, ölçü karnesi sırasıyla |
| `kenar` | number | en az 0 | Her istasyondan sonraki kenar (m) |
| `bitis` | point_list | isteğe bağlı | Bitiş istasyonu (bilinen); verilirse kapanma hesaplanır |
| `bitis_baglama` | point_list | isteğe bağlı | Bitişteki bağlama noktası; açı kapanması için gerekir |
| `sinif` | text | isteğe bağlı | Tolerans sınıfı; katalogdan okunur |
| `ilk_no` | integer | isteğe bağlı | İlk istasyonun nokta numarası; varsayılan 1 |
| `dagitim` | text | isteğe bağlı | Kenar kapanmasının dağıtımı: eşit ya da kenar orantılı |
| `cizgi` | bool | isteğe bağlı | Güzergâhı çizgiyle bağlar; varsayılan evet |

Ayrıntılı kullanım: [POLİGON](traverse.md)

### `core.merge` — TEVHİT (Tevhit)

Komşu parselleri tek parselde birleştirir (tevhit).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Birleştirilecek parseller; yoksa etkin seçim |

Ayrıntılı kullanım: [TEVHİT](merge.md)

### `core.split_parcel` — İFRAZ (İfraz)

Bir parseli düz bir ayırma çizgisiyle ikiye böler (ifraz).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | 0–2 | Ayırma çizgisinin iki ucu |
| `nesneler` | selection | en az 0 | Ayrılacak parsel; yoksa etkin seçim |

Ayrıntılı kullanım: [İFRAZ](split_parcel.md)

### `core.split_area` — ALANİFRAZ (Alana Göre İfraz)

Parselden verilen yöne paralel, istenen alanda bir parça ayırır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `yon` | point_list | 0–2 | Ayırma çizgisinin YÖNÜ: iki nokta (yol cephesi, mevcut sınır) |
| `nesneler` | selection | en az 0 | Ayrılacak parsel; yoksa etkin seçim |
| `alan` | integer | isteğe bağlı | Ayrılacak alan, mm² (400 m² = 400000000) |
| `tolerans` | integer | isteğe bağlı | Kabul toleransı, mm²; varsayılan 10000 (0,01 m²) |

Ayrıntılı kullanım: [ALANİFRAZ](split_area.md)

### `core.topology` — TOPOLOJİ (Topoloji Denetimi)

Kendini kesen sınır, sıfır alan ve örtüşen parselleri raporlar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Denetlenecek nesneler; yoksa seçim, o da boşsa bütün çizim |

Ayrıntılı kullanım: [TOPOLOJİ](topology.md)

### `core.contour` — EŞYÜKSELTİ (Eşyükselti Eğrileri)

Kotlu noktalardan eş yükselti eğrileri çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `aralik` | integer | isteğe bağlı | Eş yükselti aralığı, milimetre; varsayılan 1000 (1 m) |
| `katman` | text | isteğe bağlı | Eğrilerin çizileceği katman; varsayılan ESYUKSELTI |

Ayrıntılı kullanım: [EŞYÜKSELTİ](contour.md)

### `core.earthwork` — HACİM (Hacim Hesabı)

Kotlu noktalardan bir kota göre kazı ve dolgu hacmini hesaplar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `kot` | integer | 1 | Karşılaştırma kotu, milimetre (845 m = 845000) |

Ayrıntılı kullanım: [HACİM](earthwork.md)

### `core.layers` — KATMANLAR (Katmanları Listele)

Katmanları, nesne sayılarını, görünürlük ve kilit durumlarını listeler.

Parametre almaz.

Ayrıntılı kullanım: [KATMANLAR](layers.md)

### `core.attr_schema` — ÖZNİTELİKŞEMASI (Öznitelik Şeması)

Çizimde tanımlı öznitelik sütunlarını ve tiplerini listeler.

Parametre almaz.

Ayrıntılı kullanım: [ÖZNİTELİKŞEMASI](attr_schema.md)

### `core.query` — SORGULA (Sorgula)

Katman ve öznitelik koşuluna uyan nesneleri sayar ve anahtarlarını bildirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `katman` | text | isteğe bağlı | Hangi katmanda aranacağı; verilmezse bütün çizim |
| `alan` | text | isteğe bağlı | Öznitelik sütunu; verilirse o sütunu taşıyan nesneler |
| `deger` | text | isteğe bağlı | Sütunun eşit olması istenen değer; yalnız 'alan' ile birlikte |
| `sinir` | integer | isteğe bağlı | En çok kaç nesne bildirileceği; varsayılan 200 |

Ayrıntılı kullanım: [SORGULA](query.md)

### `core.selection_info` — SEÇİMBİLGİSİ (Seçim Bilgisi)

Kullanıcının o anki seçimini bildirir: kaç nesne ve hangi anahtarlar.

Parametre almaz.

Ayrıntılı kullanım: [SEÇİMBİLGİSİ](selection_info.md)

### `core.object_points` — NESNENOKTALARI (Nesne Noktaları)

Nesnelerin merkezini, köşelerini, uçlarını, kutusunu ya da kenar ortalarını bildirir; bir ajan bunları yeni çizimin taban noktası olarak kullanır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 1 | Noktaları istenen nesneler |
| `tur` | text | isteğe bağlı | Hangi noktalar: merkez (alanın ağırlık merkezi, çizginin uzunluk ortası, dairenin merkezi), köşeler, uçlar, kutunun köşeleri ya da kenar ortaları; varsayılan merkez |

Ayrıntılı kullanım: [NESNENOKTALARI](object_points.md)

### `core.view_info` — GÖRÜNÜMBİLGİSİ (Görünüm Bilgisi)

Ekranda görünen alanın köşe koordinatlarını, merkezini, ölçeğini ve CRS'ini bildirir.

Parametre almaz.

Ayrıntılı kullanım: [GÖRÜNÜMBİLGİSİ](view_info.md)

### `core.context` — BAĞLAM (Bağlam)

Üzerinde çalışılan her şeyi tek çağrıda özetler: belge sürümü, koordinat sistemi, kapsam, katmanlar, çıktı yerleşimleri ve hedefli olup olmadıkları, seçili nesneler ve görünüm. Özet verir, döküm değil.

Parametre almaz.

Ayrıntılı kullanım: [BAĞLAM](context.md)

### `core.tool_search` — ARAÇARA (Araç Ara)

Ajan araç kataloğunda ad ve özete göre arar. Sonuç her zaman kaç aracın eşleştiğini, kaçının gösterildiğini ve katalogdaki toplam araç sayısını söyler: arama hiçbir aracı gizlemez, tam liste `tools/list` ile alınır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `sorgu` | text | 1 | Aranan sözcük; ad ve özet içinde Türkçe katlamayla eşleşir |
| `alan` | text | isteğe bağlı | Nerede aranacağı: hepsi (öntanımlı), ad ya da ozet |
| `sinir` | integer | isteğe bağlı | En çok kaç sonuç gösterilsin; öntanımlı 20. Eşleşme sayısı her hâlde bildirilir |

Ayrıntılı kullanım: [ARAÇARA](tool_search.md)

### `core.job_template` — İŞŞABLONU (İş Şablonu)

Sık yapılan işlerin — atlas, kadastro kontrolü, parsel raporu — komut satırlarını sırasıyla verir. Hiçbirini çalıştırmaz: adımlar olağan araç yüzeyinden gönderilir ve yazan her adım yine öneri olur.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | Ne yapılacağı: listele ya da goster |
| `sablon` | text | isteğe bağlı | Şablonun kimliği; goster için gerekir |

Ayrıntılı kullanım: [İŞŞABLONU](job_template.md)

### `core.suggestion` — ÖNERİ (Yapay Zeka Önerisi)

Bekleyen yapay zeka önerilerini listeler, durumunu söyler, uygular ya da reddeder.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | Ne yapılacağı: uygula, reddet, durum ya da listele |
| `oneri` | text | isteğe bağlı | Öneri kimliği; uygula, reddet ve durum için gerekir |

Ayrıntılı kullanım: [ÖNERİ](suggestion.md)

### `core.mcp` — MCPSUNUCU (MCP Sunucusu)

Yapay zeka ajanlarının bağlanacağı MCP sunucusunu başlatır, durdurur, durumunu söyler, yeni bir erişim belirteci üretir, bağlı istemcileri listeler ve tek bir istemcinin yetkisini kaldırır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | Ne yapılacağı: baslat, durdur, durum, belirtec (yeni belirteç üretir), istemciler, iptal (bir istemcinin yetkisini kaldırır), izin (geri verir) ya da sina (bağlantıyı sınar) |
| `port` | integer | isteğe bağlı | Yalnız bu başlatma için port; verilmezse ayardaki port |
| `ad` | text | isteğe bağlı | İstemcinin adı; iptal ve izin için gerekir. Adları islem=istemciler ile görün |

Ayrıntılı kullanım: [MCPSUNUCU](mcp.md)

### `core.ai_provider` — YAPAYZEKAMODELİ (Yapay Zeka Modeli)

Yapay zeka model sağlayıcılarını listeler, ekler, siler, birini varsayılan yapar ya da bağlantısını dener; profil adresi, lehçesi, modeli ve anahtar adını taşır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | Ne yapılacağı: listele, ekle, sil, varsayilan ya da dene (bağlantıyı dener) |
| `ad` | text | isteğe bağlı | Profilin adı; ekle, sil, varsayilan ve dene için gerekir |
| `lehce` | text | isteğe bağlı | Uç noktanın konuştuğu telli dil; ekle için, varsayılan openai_chat |
| `adres` | text | isteğe bağlı | Uç noktanın adresi: http:// ya da https:// ile başlar, satıcının ön eki dahil |
| `yol` | text | isteğe bağlı | Adresin altındaki uç nokta; '/' ile başlar: /chat/completions, /messages, /api/chat |
| `model` | text | isteğe bağlı | Model kimliği, uç noktanın yazdığı gibi |
| `anahtar_ref` | text | isteğe bağlı | Anahtarı tutan kaydın adı — anahtar zincirindeki kayıt ya da bir ortam değişkeni (örnek: DEEPSEEK_API_KEY). Anahtarın kendisi buraya yazılmaz |
| `baglam` | integer | isteğe bağlı | Bağlam penceresi, jeton; 0 bilinmiyor demektir |
| `azami` | integer | isteğe bağlı | Çıktı jeton sınırı; 0 demek 'bu alanı hiç gönderme' |
| `sicaklik` | number | isteğe bağlı | Örnekleme sıcaklığı, 0 ile 2 arasında; verilmezse hiç gönderilmez |
| `akis` | bool | isteğe bağlı | Cevap parça parça mı istensin; varsayılan evet |
| `dusunme` | bool | isteğe bağlı | Modelin düşünme metni gösterilsin mi |
| `araclar` | bool | isteğe bağlı | Uç noktaya araç kataloğu gönderilsin mi; varsayılan evet |

Ayrıntılı kullanım: [YAPAYZEKAMODELİ](ai_provider.md)

## AI araç kataloğu

AI'ın görebildiği komutlar `Flags::AiAccessible` bayrağından üretilir.
Elle tutulan ikinci bir araç şeması yoktur (kentoscad.md §2.3, §5.1).

```json
[
  {
    "name": "core_align",
    "title": "Hizala",
    "description": "Bir ya da iki nokta çiftiyle nesneleri taşır, döndürür ve istenirse ölçekler.\nKomut: HİZALA (HIZALA, ALIGN, HZL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Hizalanacak nesneler — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "kaynak": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Birinci kaynak nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "hedef": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Birinci kaynağın gideceği yer — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "kaynak2": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "İkinci kaynak nokta; verilirse döndürme de yapılır — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "hedef2": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "İkinci kaynağın gideceği yer — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "olcekle": {
          "type": "boolean",
          "description": "İki çiftin uzunluk oranıyla ölçekler de (evet/hayır)"
        }
      },
      "required": [
        "kaynak",
        "hedef"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.align",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "HİZALA",
        "HIZALA",
        "ALIGN",
        "HZL"
      ]
    }
  },
  {
    "name": "core_annulus",
    "title": "Halka",
    "description": "Merkez, iç ve dış yarıçaptan delikli halka çizer.\nKomut: HALKA (ANNULUS, HLK)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "merkez": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Halkanın merkezi — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "ic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "İç çember üzerinde bir nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "dis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Dış çember üzerinde bir nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [
        "merkez",
        "ic",
        "dis"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.annulus",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "HALKA",
        "ANNULUS",
        "HLK"
      ]
    }
  },
  {
    "name": "core_arc_draw",
    "title": "Yay",
    "description": "Merkez+iki uç, yay üzerinde üç nokta, başlangıç+merkez+süpürme ya da başlangıç+bitiş+yarıçapla yay çizer.\nKomut: YAY (ARC, YY)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "merkez": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Yayın merkezi — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "baslangic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Yayın başlangıç noktası; merkez yönteminde yarıçapı bu belirler — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Yayın bitiş noktası; süpürme saat yönünün tersinedir — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "yontem": {
          "type": "string",
          "enum": [
            "merkez",
            "3n",
            "bma",
            "bby",
            "devam"
          ],
          "description": "merkez: merkez + iki uç · 3n: yay üzerinde üç nokta · bma: başlangıç, merkez ve süpürme açısı · bby: başlangıç, bitiş ve yarıçap (metin)"
        },
        "uzerinden": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "3n: yayın üzerinden geçtiği nokta — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "supurme": {
          "type": "number",
          "description": "bma: süpürme açısı (sayı)"
        },
        "yaricap": {
          "type": "number",
          "description": "bby: yarıçap (m) [m] (sayı)"
        },
        "yon_nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "bby: yayın hangi yandan geçeceği gösterilen nokta; yon verilmişse sorulmaz — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "yon": {
          "type": "string",
          "enum": [
            "sol",
            "sag"
          ],
          "description": "bby: yayın hangi tarafa kavis yaptığı; başlangıç→bitiş yönüne göre (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.arc_draw",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "YAY",
        "ARC",
        "YY"
      ]
    }
  },
  {
    "name": "core_area",
    "title": "Alan",
    "description": "Kapalı bir alan çizer; istenirse içine delik açar.\nKomut: ALAN (AREA, AL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "minItems": 3
            }
          ],
          "description": "Alanın köşe noktaları; kapanış noktası tekrarlanmaz — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "bolum": {
          "type": "array",
          "items": {
            "type": "integer"
          },
          "description": "Halka uzunlukları: ilki dış sınır, sonrakiler delik (tam sayı)"
        }
      },
      "required": [
        "noktalar"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.area",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ALAN",
        "AREA",
        "AL"
      ]
    }
  },
  {
    "name": "core_array",
    "title": "Dizi",
    "description": "Seçilen nesneleri satır/sütun ya da bir merkez etrafında çoğaltır.\nKomut: DİZİ (DIZI, ARRAY, DZ)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Dizilecek nesnelerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "mod": {
          "type": "string",
          "description": "KUTUPSAL için kutupsal dizi; verilmezse satır/sütun dizisi (metin)"
        },
        "satir": {
          "type": "integer",
          "description": "Satır sayısı (dikdörtgen dizi) (tam sayı)"
        },
        "sutun": {
          "type": "integer",
          "description": "Sütun sayısı (dikdörtgen dizi) (tam sayı)"
        },
        "satir_aralik": {
          "type": "number",
          "description": "Satır aralığı, metre; kuzeye artı (sayı)"
        },
        "sutun_aralik": {
          "type": "number",
          "description": "Sütun aralığı, metre; doğuya artı (sayı)"
        },
        "merkez": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Dizinin merkezi (kutupsal dizi) — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "sayi": {
          "type": "integer",
          "description": "Toplam kopya sayısı, özgün dahil (kutupsal dizi) (tam sayı)"
        },
        "aci": {
          "type": "number",
          "description": "Süpürülecek toplam açı, derece; verilmezse tam tur (sayı)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.array",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "DİZİ",
        "DIZI",
        "ARRAY",
        "DZ"
      ]
    }
  },
  {
    "name": "core_attribute",
    "title": "Öznitelik",
    "description": "Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar.\nKomut: ÖZNİTELİK (OZNITELIK, ATTRIBUTE, ÖZN, OZN)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "ad": {
          "type": "string",
          "description": "Öznitelik kimliği; yoksa tanımlı sütunlar listelenir (metin)"
        },
        "nesne": {
          "type": "integer",
          "description": "Nesnenin kalıcı kimliği (tam sayı)"
        },
        "deger": {
          "type": "string",
          "description": "Yeni değer; yoksa yalnızca okur. 'yok' hücreyi boşaltır (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.attribute",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÖZNİTELİK",
        "OZNITELIK",
        "ATTRIBUTE",
        "ÖZN",
        "OZN"
      ]
    }
  },
  {
    "name": "core_block",
    "title": "Blok Tanımla",
    "description": "Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar.\nKomut: BLOK (BLOK, BLOCK, BLK)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "ad": {
          "type": "string",
          "description": "Bloğun adı; Türkçe katlanmış hâliyle benzersiz (metin)"
        },
        "taban": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Taban noktası: referansların yerleştirildiği nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Bloğa girecek nesneler; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "aciklama": {
          "type": "string",
          "description": "Serbest açıklama (metin)"
        }
      },
      "required": [
        "ad",
        "taban"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.block",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "BLOK",
        "BLOK",
        "BLOCK",
        "BLK"
      ]
    }
  },
  {
    "name": "core_break",
    "title": "Kır",
    "description": "Çizgiden iki nokta arasındaki parçayı çıkarır; tek nokta verilirse boşluk bırakmadan böler.\nKomut: KIR (BREAK, KR)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kırılacak çizgi — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "birinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Kırılacak parçanın ilk noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "ikinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Kırılacak parçanın ikinci noktası; verilmezse boşluk bırakmadan böler — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [
        "birinci"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.break",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KIR",
        "BREAK",
        "KR"
      ]
    }
  },
  {
    "name": "core_chamfer",
    "title": "Pah",
    "description": "Bir köşeyi düz bir kenarla keser (pah kırar).\nKomut: PAH (CHAMFER, PH)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Köşesi kesilecek nesnenin kimliği — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "İşlem yapılacak köşe — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "mesafe": {
          "type": "number",
          "description": "Köşeden her iki kenar boyunca kesilecek mesafe, metre (sayı)"
        }
      },
      "required": [
        "nesne",
        "nokta",
        "mesafe"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.chamfer",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "PAH",
        "CHAMFER",
        "PH"
      ]
    }
  },
  {
    "name": "core_circle_draw",
    "title": "Daire",
    "description": "Merkez+çevre, çapın iki ucu, çember üzerinde üç nokta ya da iki doğruya teğet yarıçapla daire çizer.\nKomut: DAİRE (DAIRE, CIRCLE, DR)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "merkez": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Dairenin merkezi — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "cevre": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Çember üzerinde bir nokta; yarıçapı bu belirler — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "yontem": {
          "type": "string",
          "enum": [
            "merkez",
            "2n",
            "3n",
            "ttr"
          ],
          "description": "merkez: merkez + çevre · 2n: çapın iki ucu · 3n: çember üzerinde üç nokta · ttr: iki doğruya teğet, verilen yarıçapla (metin)"
        },
        "birinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "2n: çapın bir ucu · 3n: birinci nokta · ttr: birinci doğrunun ilk noktası — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "ikinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "İkinci nokta — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "ucuncu": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "3n: üçüncü nokta · ttr: ikinci doğrunun ilk noktası — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "dorduncu": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "ttr: ikinci doğrunun ikinci noktası — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "yaricap": {
          "type": "number",
          "description": "ttr: teğet dairenin yarıçapı (m) [m] (sayı)"
        },
        "yon": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "ttr: dairenin geleceği köşe; dört çözümden en yakını alınır — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.circle_draw",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "DAİRE",
        "DAIRE",
        "CIRCLE",
        "DR"
      ]
    }
  },
  {
    "name": "core_colour",
    "title": "Renk",
    "description": "Seçili nesnelerin çizgi ve dolgu rengini değiştirir ya da katmanın rengine döndürür.\nKomut: RENK (COLOR, COLOUR, RNK)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Rengi değişecek nesneler; verilmezse etkin seçim, o da boşsa sorulur — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "renk": {
          "type": "string",
          "description": "Çizgi rengi: #RRGGBB (ya da saydamlıkla #AARRGGBB) veya katman (metin)"
        },
        "dolgu": {
          "type": "string",
          "description": "Dolgu rengi: #RRGGBB, yok (dolgusuz) ya da katman (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.colour",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "RENK",
        "COLOR",
        "COLOUR",
        "RNK"
      ]
    }
  },
  {
    "name": "core_combine",
    "title": "Birleştir",
    "description": "Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek çizgi yapar.\nKomut: BİRLEŞTİR (BIRLESTIR, COMBINE, BRL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Birleştirilecek alanlar ya da çizgiler; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.combine",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "BİRLEŞTİR",
        "BIRLESTIR",
        "COMBINE",
        "BRL"
      ]
    }
  },
  {
    "name": "core_context",
    "title": "Bağlam",
    "description": "Üzerinde çalışılan her şeyi tek çağrıda özetler: belge sürümü, koordinat sistemi, kapsam, katmanlar, çıktı yerleşimleri ve hedefli olup olmadıkları, seçili nesneler ve görünüm. Özet verir, döküm değil.\nKomut: BAĞLAM (BAGLAM, CONTEXT, BĞL)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {},
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.context",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "BAĞLAM",
        "BAGLAM",
        "CONTEXT",
        "BĞL"
      ]
    }
  },
  {
    "name": "core_contour",
    "title": "Eşyükselti Eğrileri",
    "description": "Kotlu noktalardan eş yükselti eğrileri çizer.\nKomut: EŞYÜKSELTİ (ESYUKSELTI, CONTOUR, EŞY)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "aralik": {
          "type": "integer",
          "description": "Eş yükselti aralığı, milimetre; varsayılan 1000 (1 m) (tam sayı)"
        },
        "katman": {
          "type": "string",
          "description": "Eğrilerin çizileceği katman; varsayılan ESYUKSELTI (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.contour",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "EŞYÜKSELTİ",
        "ESYUKSELTI",
        "CONTOUR",
        "EŞY"
      ]
    }
  },
  {
    "name": "core_coordinate",
    "title": "Koordinat Oku",
    "description": "Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar.\nKomut: KOORDİNAT (KOORDINAT, COORDINATE, KRD)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Okunacak nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [
        "nokta"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.coordinate",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KOORDİNAT",
        "KOORDINAT",
        "COORDINATE",
        "KRD"
      ]
    }
  },
  {
    "name": "core_copy",
    "title": "Kopyala",
    "description": "Seçilen nesnelerin kopyasını verilen her noktaya, başlangıçtan o noktaya kadar öteleyerek koyar.\nKomut: KOPYALA (COPY, KP)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kopyalanacak nesnelerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "baslangic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Kopyalamanın başlangıç noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "minItems": 1
            }
          ],
          "description": "Kopyaların geleceği noktalar; her nokta bir kopya — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [
        "baslangic",
        "bitis"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.copy",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KOPYALA",
        "COPY",
        "KP"
      ]
    }
  },
  {
    "name": "core_copy_clip",
    "title": "Panoya Kopyala",
    "description": "Seçili nesneleri çizimin kendi biçiminde panoya yazar.\nKomut: PANOYAKOPYALA (PANOKOPYALA, COPYCLIP, PKP)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Panoya alınacak nesneler; verilmezse seçim kullanılır — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "dosya": {
          "type": "string",
          "description": "Panonun yazılacağı dosya; verilmezse ortak pano dosyası (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": true
    },
    "_meta": {
      "cad.kentos/commandId": "core.copy_clip",
      "cad.kentos/category": "Dosya",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "PANOYAKOPYALA",
        "PANOKOPYALA",
        "COPYCLIP",
        "PKP"
      ]
    }
  },
  {
    "name": "core_cut",
    "title": "Kes",
    "description": "Seçili nesneleri panoya alır ve çizimden siler; tek geri alma adımı.\nKomut: KES (CUT, KS)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kesilecek nesneler; verilmezse seçim kullanılır — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "dosya": {
          "type": "string",
          "description": "Panonun yazılacağı dosya; verilmezse ortak pano dosyası (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.cut",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KES",
        "CUT",
        "KS"
      ]
    }
  },
  {
    "name": "core_dimension",
    "title": "Ölçü",
    "description": "İki nokta arasını, bir yarıçapı, çapı ya da açıyı ölçüp yazısı ve oklarıyla çizer.\nKomut: ÖLÇÜ (OLCU, DIMENSION, ÖÇ)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "birinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Birinci nokta; açısal ölçüde birinci kolun ucu — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "ikinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "İkinci nokta; açısal ölçüde ikinci kolun ucu — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "konum": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Ölçü çizgisinin yeri; açısal ölçüde yayın geçtiği nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "tur": {
          "type": "string",
          "description": "hizali (varsayılan), dogrusal, yaricap, cap, acisal, koordinat, yay (metin)"
        },
        "tepe": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Açısal ölçünün tepe noktası — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Yay uzunluğu ölçüsünün bitiş noktası — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "stil": {
          "type": "string",
          "description": "Katalogdaki ölçü stili: ISO-25 (varsayılan), STANDARD, MIMARI (metin)"
        },
        "metin": {
          "type": "string",
          "description": "Ölçülen değer yerine yazılacak metin (metin)"
        },
        "katalog": {
          "type": "string",
          "description": "Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri (metin)"
        }
      },
      "required": [
        "birinci",
        "ikinci",
        "konum"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.dimension",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÖLÇÜ",
        "OLCU",
        "DIMENSION",
        "ÖÇ"
      ]
    }
  },
  {
    "name": "core_divide",
    "title": "Bölümle",
    "description": "Bir nesne boyunca eşit parçalara bölerek ya da sabit aralıkla nokta veya blok yerleştirir.\nKomut: BÖLÜMLE (BOLUMLE, DIVIDE, BLM)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Bölünecek nesne — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "sayi": {
          "type": "integer",
          "minimum": 2,
          "maximum": 10000,
          "description": "Kaç eşit parçaya bölünecek (tam sayı)"
        },
        "aralik": {
          "type": "number",
          "description": "Sabit aralık (m); başlangıçtan itibaren yürür [m] (sayı)"
        },
        "blok": {
          "type": "string",
          "description": "Nokta yerine bu bloğu koyar; blok önceden tanımlı olmalı (metin)"
        },
        "hizala": {
          "type": "boolean",
          "description": "Bloğu üzerinde durduğu kenarın doğrultusuna çevirir (evet/hayır)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.divide",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "BÖLÜMLE",
        "BOLUMLE",
        "DIVIDE",
        "BLM"
      ]
    }
  },
  {
    "name": "core_earthwork",
    "title": "Hacim Hesabı",
    "description": "Kotlu noktalardan bir kota göre kazı ve dolgu hacmini hesaplar.\nKomut: HACİM (HACIM, EARTHWORK, HCM)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "kot": {
          "type": "integer",
          "description": "Karşılaştırma kotu, milimetre (845 m = 845000) (tam sayı)"
        }
      },
      "required": [
        "kot"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.earthwork",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "HACİM",
        "HACIM",
        "EARTHWORK",
        "HCM"
      ]
    }
  },
  {
    "name": "core_edittext",
    "title": "Yazıyı Düzenle",
    "description": "Var olan bir yazının metnini, yüksekliğini ya da hizalamasını değiştirir.\nKomut: YAZIDÜZENLE (YAZIDUZENLE, EDITTEXT, YZD)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Düzenlenecek yazılar; verilmezse seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "yazi": {
          "type": "string",
          "description": "Yeni metin; verilmezse değişmez (metin)"
        },
        "yukseklik": {
          "type": "integer",
          "description": "Yeni yükseklik, zeminde milimetre; verilmezse değişmez (tam sayı)"
        },
        "hizalama": {
          "type": "string",
          "description": "sol, orta, sag veya merkez; verilmezse değişmez (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.edittext",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "YAZIDÜZENLE",
        "YAZIDUZENLE",
        "EDITTEXT",
        "YZD"
      ]
    }
  },
  {
    "name": "core_ellipse_draw",
    "title": "Elips",
    "description": "Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir.\nKomut: ELİPS (ELIPS, ELLIPSE, EL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "merkez": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Elipsin merkezi — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "birinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "merkez: birinci eksenin ucu · eksen: birinci eksenin bir ucu — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "ikinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "İkinci eksenin uzaklığı; eksene dik ölçülür — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "yontem": {
          "type": "string",
          "enum": [
            "merkez",
            "eksen"
          ],
          "description": "merkez: merkez + eksen ucu · eksen: eksenin iki ucu (metin)"
        },
        "ikinci_uc": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "eksen: birinci eksenin öteki ucu — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "baslangic": {
          "type": "number",
          "description": "Kısmi elips: başlangıç açısı, derece, birinci eksenden saat yönünün tersine (sayı)"
        },
        "bitis": {
          "type": "number",
          "description": "Kısmi elips: bitiş açısı, derece; baslangic ile birlikte (sayı)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.ellipse_draw",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ELİPS",
        "ELIPS",
        "ELLIPSE",
        "EL"
      ]
    }
  },
  {
    "name": "core_entity_info",
    "title": "Nesne Bilgisi",
    "description": "Nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve özniteliklerini bildirir.\nKomut: NESNEBİLGİ (NESNEBILGI, OBJINFO, NB)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Bilgisi istenen nesneler — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.entity_info",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "NESNEBİLGİ",
        "NESNEBILGI",
        "OBJINFO",
        "NB"
      ]
    }
  },
  {
    "name": "core_erase",
    "title": "Sil",
    "description": "Seçilen nesneleri siler.\nKomut: SİL (SIL, ERASE, E)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Silinecek nesnelerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.erase",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "SİL",
        "SIL",
        "ERASE",
        "E"
      ]
    }
  },
  {
    "name": "core_explode",
    "title": "Patlat",
    "description": "Çizgiyi tek tek kenarlara, alanı sınırına, blok referansını bileşenlerine ayırır.\nKomut: PATLAT (EXPLODE, PTL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Patlatılacak nesneler — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.explode",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "PATLAT",
        "EXPLODE",
        "PTL"
      ]
    }
  },
  {
    "name": "core_extend",
    "title": "Uzat",
    "description": "Tıklanan ucu sınıra ulaşana kadar uzatır: çizginin ucunu doğrultusunda, yayınkini çemberi boyunca.\nKomut: UZAT (EXTEND, UZ)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uzatılan nesneler, tıklama sırasıyla; yoksa her tıklamanın altındaki nesne — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "sinir": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uzatılacak sınırlar; yoksa seçili nesneler, o da yoksa tıklanan nesnenin yakınındaki her nesne — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "hepsi": {
          "type": "boolean",
          "description": "Tıklanan nesnenin yakınındaki her nesne sınırdır (seçim ve sinir yokken öntanımlı) (evet/hayır)"
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              }
            }
          ],
          "description": "Uzatılacak her ucun yakınında bir nokta, sırayla — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.extend",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "UZAT",
        "EXTEND",
        "UZ"
      ]
    }
  },
  {
    "name": "core_fillet",
    "title": "Yuvarla",
    "description": "Bir köşeyi verilen yarıçapta yay ile yuvarlatır.\nKomut: YUVARLA (FILLET, YV)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Köşesi yuvarlatılacak nesnenin kimliği — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "İşlem yapılacak köşe — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "yaricap": {
          "type": "number",
          "description": "Yuvarlatma yarıçapı, metre (sayı)"
        }
      },
      "required": [
        "nesne",
        "nokta",
        "yaricap"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.fillet",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "YUVARLA",
        "FILLET",
        "YV"
      ]
    }
  },
  {
    "name": "core_fit",
    "title": "Oturt (Helmert)",
    "description": "Yerel ölçülmüş çizimi kontrol noktalarıyla haritaya oturtur (2B Helmert).\nKomut: OTURT (FIT, GEOREF, OTR)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "minItems": 4
            }
          ],
          "description": "Kontrol çiftleri: yerel, harita, yerel, harita... — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "olcek_kilitli": {
          "type": "boolean",
          "description": "Ölçeği 1'de tutar; saha ölçüsü yeniden ölçeklenmez (evet/hayır)"
        },
        "sistem": {
          "type": "string",
          "description": "Oturtulduktan sonraki koordinat sistemi, örnek TUREF/TM36 (metin)"
        }
      },
      "required": [
        "noktalar"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.fit",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "OTURT",
        "FIT",
        "GEOREF",
        "OTR"
      ]
    }
  },
  {
    "name": "core_guide",
    "title": "Kılavuz",
    "description": "Cetvel kılavuzu ve açılı kılavuz ekler, listeler ve siler.\nKomut: KILAVUZ (GUIDE, KLV)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "yon": {
          "type": "string",
          "description": "yatay | düşey | bir açı (45, 45g, 30d); yoksa kılavuzlar listelenir (metin)"
        },
        "deger": {
          "type": "integer",
          "description": "Kılavuzun koordinatı, milimetre — yatayda yukarı, düşeyde sağa (tam sayı)"
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Açılı kılavuzun geçtiği nokta; yalnız `yon` bir açıysa — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "tur": {
          "type": "string",
          "enum": [
            "dogru",
            "isin"
          ],
          "description": "doğru: iki yöne sonsuz · ışın: noktadan ileriye (metin)"
        },
        "sil": {
          "type": "boolean",
          "description": "Verilen yerdeki kılavuzu siler (evet/hayır)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.guide",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KILAVUZ",
        "GUIDE",
        "KLV"
      ]
    }
  },
  {
    "name": "core_hatch",
    "title": "Tarama",
    "description": "Kapalı nesnelerin ya da verilen köşelerin içini katalogdaki bir desenle tarar.\nKomut: TARAMA (TARAMA, HATCH, TRM)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              }
            }
          ],
          "description": "Sınır köşeleri, nesne seçmek yerine; en az üç nokta — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Sınırı verecek kapalı nesneler; yoksa etkin seçim ya da noktalar= — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "desen": {
          "type": "string",
          "description": "Katalogdaki desen adı: SOLID, ANSI31, NET…; varsayılan SOLID (metin)"
        },
        "aci": {
          "type": "number",
          "description": "Desenin dönme açısı, derece; varsayılan 0 (sayı)"
        },
        "olcek": {
          "type": "number",
          "description": "Desen ölçeği; varsayılan pafta ölçeğinin paydası (AYAR plan_ölçeği) (sayı)"
        },
        "katalog": {
          "type": "string",
          "description": "Desen kataloğu dosyası; varsayılan TERCİH desen_kataloğu (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.hatch",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "TARAMA",
        "TARAMA",
        "HATCH",
        "TRM"
      ]
    }
  },
  {
    "name": "core_insert",
    "title": "Blok Ekle",
    "description": "Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir.\nKomut: BLOKEKLE (BLOKEKLE, INSERT, BE)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "ad": {
          "type": "string",
          "description": "Yerleştirilecek bloğun adı (metin)"
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Ekleme noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "olcek": {
          "type": "number",
          "description": "Ölçek; eksi değer x'te aynalar; varsayılan 1 (sayı)"
        },
        "olcek_y": {
          "type": "number",
          "description": "Y ölçeği, farklıysa; varsayılan olcek (sayı)"
        },
        "aci": {
          "type": "number",
          "description": "Dönme açısı, derece; varsayılan 0 (sayı)"
        },
        "sutun": {
          "type": "integer",
          "description": "Dizi sütun sayısı; varsayılan 1 (tam sayı)"
        },
        "satir": {
          "type": "integer",
          "description": "Dizi satır sayısı; varsayılan 1 (tam sayı)"
        },
        "sutun_aralik": {
          "type": "integer",
          "description": "Sütunlar arası, milimetre, döndürülmüş eksende (tam sayı)"
        },
        "satir_aralik": {
          "type": "integer",
          "description": "Satırlar arası, milimetre, döndürülmüş eksende (tam sayı)"
        }
      },
      "required": [
        "ad",
        "nokta"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.insert",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "BLOKEKLE",
        "BLOKEKLE",
        "INSERT",
        "BE"
      ]
    }
  },
  {
    "name": "core_intersect_point",
    "title": "Kesişim Noktası",
    "description": "İki doğrultunun, iki uzaklığın ya da iki doğrunun kesişimine nokta koyar.\nKomut: KESİŞİMNOKTA (KESISIMNOKTA, INTERSECTPT, KSN)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "yontem": {
          "type": "string",
          "enum": [
            "dogrultu",
            "mesafe",
            "dogru"
          ],
          "description": "dogrultu: iki doğrultu · mesafe: iki uzaklık · dogru: iki doğru (metin)"
        },
        "birinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Birinci bilinen nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "ikinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "İkinci bilinen nokta — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "ucuncu": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "İkinci doğrunun ilk noktası — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "dorduncu": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "İkinci doğrunun ikinci noktası — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "birinci_aci": {
          "type": "number",
          "description": "Birinci noktadan okunan doğrultu (sayı)"
        },
        "ikinci_aci": {
          "type": "number",
          "description": "İkinci noktadan okunan doğrultu (sayı)"
        },
        "birinci_mesafe": {
          "type": "number",
          "description": "Birinci noktadan ölçülen uzaklık (m) (sayı)"
        },
        "ikinci_mesafe": {
          "type": "number",
          "description": "İkinci noktadan ölçülen uzaklık (m) (sayı)"
        },
        "yon": {
          "type": "string",
          "enum": [
            "sol",
            "sag"
          ],
          "description": "İki uzaklık kesişiminin hangi çözümü; birinci→ikinci yönüne göre (metin)"
        },
        "yon_nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "mesafe: iki çözümden istenenin gösterildiği nokta; yon verilmişse sorulmaz — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "kesisim": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Bulunan nokta; günlüğe yazılır — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [
        "birinci"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.intersect_point",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KESİŞİMNOKTA",
        "KESISIMNOKTA",
        "INTERSECTPT",
        "KSN"
      ]
    }
  },
  {
    "name": "core_job_template",
    "title": "İş Şablonu",
    "description": "Sık yapılan işlerin — atlas, kadastro kontrolü, parsel raporu — komut satırlarını sırasıyla verir. Hiçbirini çalıştırmaz: adımlar olağan araç yüzeyinden gönderilir ve yazan her adım yine öneri olur.\nKomut: İŞŞABLONU (ISSABLONU, JOBTEMPLATE, İŞŞ)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "islem": {
          "type": "string",
          "enum": [
            "listele",
            "goster"
          ],
          "description": "Ne yapılacağı: listele ya da goster (metin)"
        },
        "sablon": {
          "type": "string",
          "description": "Şablonun kimliği; goster için gerekir (metin)"
        }
      },
      "required": [
        "islem"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.job_template",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "İŞŞABLONU",
        "ISSABLONU",
        "JOBTEMPLATE",
        "İŞŞ"
      ]
    }
  },
  {
    "name": "core_join",
    "title": "Uç Uca Ekle",
    "description": "Uçları birbirine değen çizgileri tek bir çizgiye ekler.\nKomut: UÇUCA (UCUCA, JOIN, UÇE)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uç uca eklenecek çizgiler — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "tolerans": {
          "type": "number",
          "description": "Uçların değmiş sayılması için en büyük açıklık (m); varsayılan 0,001 [m] (sayı)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.join",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "UÇUCA",
        "UCUCA",
        "JOIN",
        "UÇE"
      ]
    }
  },
  {
    "name": "core_label",
    "title": "Etiket",
    "description": "Katmandaki nesneleri özniteliklerinden okuyarak etiketler.\nKomut: ETİKET (ETIKET, LABEL, ETK)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "katman": {
          "type": "string",
          "description": "Etiketlenecek katmanın adı (metin)"
        },
        "bicim": {
          "type": "string",
          "description": "Etiket biçimi; {sutun} o sütunun değeriyle değişir, \\n satır kırar. Sembol alan bildiriyorsa gerekmez (metin)"
        },
        "hedef": {
          "type": "string",
          "description": "Etiketlerin yazılacağı katman; yoksa '<katman> ETİKET' (metin)"
        },
        "yukseklik": {
          "type": "integer",
          "description": "Yazı yüksekliği, zemin milimetresi (tam sayı)"
        },
        "kaydirma": {
          "type": "integer",
          "description": "Nesnenin ortasından dikey kaydırma, zemin milimetresi; artı yukarı (tam sayı)"
        }
      },
      "required": [
        "katman"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.label",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ETİKET",
        "ETIKET",
        "LABEL",
        "ETK"
      ]
    }
  },
  {
    "name": "core_layer",
    "title": "Katman",
    "description": "Katman oluşturur, aktif yapar ve özelliklerini değiştirir.\nKomut: KATMAN (LAYER, KAT)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "ad": {
          "type": "string",
          "description": "Katman adı; yoksa oluşturulur ve aktif yapılır (metin)"
        },
        "grup": {
          "type": "string",
          "description": "Katman ağacındaki yer, düzeyler '>' ile ayrılır; boş = kök (metin)"
        },
        "gorunur": {
          "type": "boolean",
          "description": "Katmanın görünürlüğü (evet/hayır)"
        },
        "kilitli": {
          "type": "boolean",
          "description": "Katmanın kilit durumu (evet/hayır)"
        },
        "renk": {
          "type": "integer",
          "description": "Çizim rengi, 0xAARRGGBB (tam sayı)"
        }
      },
      "required": [
        "ad"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.layer",
      "cad.kentos/category": "Katman",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KATMAN",
        "LAYER",
        "KAT"
      ]
    }
  },
  {
    "name": "core_layer_visibility",
    "title": "Katman Görünümü",
    "description": "Katmanların görünürlüğünü toptan değiştirir: bir katmanı gösterir ya da gizler, yalnız onu bırakır, hepsini gösterir veya görünürlüğü ters çevirir.\nKomut: KATMANGÖRÜNÜM (KATMANGORUNUM, LAYERVIEW, KGÖ, KGO)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "islem": {
          "type": "string",
          "description": "goster, gizle, yalniz (yalnız bu katman), tumu (hepsini göster) ya da tersine (metin)"
        },
        "katman": {
          "type": "string",
          "description": "Katman adı; goster, gizle ve yalniz için gerekir, tersine için isteğe bağlı (verilmezse bütün katmanlar), tumu ile verilemez (metin)"
        }
      },
      "required": [
        "islem"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.layer_visibility",
      "cad.kentos/category": "Katman",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KATMANGÖRÜNÜM",
        "KATMANGORUNUM",
        "LAYERVIEW",
        "KGÖ",
        "KGO"
      ]
    }
  },
  {
    "name": "core_layout",
    "title": "Çıktı Yerleşimi",
    "description": "Çizimin çıktı yerleşimlerini yönetir: yeni yerleşim açar, siler, adlandırır ve kâğıdını değiştirir. Yerleşim çizimle birlikte kaydedilir ve geri alınabilir.\nKomut: ÇIKTIYERLEŞİMİ (CIKTIYERLESIMI, LAYOUT, ÇYR, CYR)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "islem": {
          "type": "string",
          "enum": [
            "listele",
            "ekle",
            "sil",
            "ad",
            "sayfa",
            "sayfaekle",
            "sayfasil",
            "sayfacogalt",
            "sayfatasi",
            "denetle",
            "atlas",
            "rapor"
          ],
          "description": "Ne yapılacağı (metin)"
        },
        "ad": {
          "type": "string",
          "description": "Yerleşimin adı; listele dışında gerekir (metin)"
        },
        "yeni_ad": {
          "type": "string",
          "description": "islem=ad için yeni yerleşim adı (metin)"
        },
        "kagit": {
          "type": "string",
          "description": "A5, A4, A3, A2, A1, A0 ya da ozel (varsayılan A4) (metin)"
        },
        "genislik": {
          "type": "integer",
          "minimum": 1,
          "maximum": 10000,
          "description": "ozel kâğıt için sayfa genişliği [kâğıt mm] (tam sayı)"
        },
        "yukseklik": {
          "type": "integer",
          "minimum": 1,
          "maximum": 10000,
          "description": "ozel kâğıt için sayfa yüksekliği [kâğıt mm] (tam sayı)"
        },
        "yon": {
          "type": "string",
          "enum": [
            "dikey",
            "yatay"
          ],
          "description": "Sayfa yönü (varsayılan dikey) (metin)"
        },
        "kenar": {
          "type": "integer",
          "minimum": 0,
          "maximum": 200,
          "description": "Kenar boşluğu (varsayılan 10) [kâğıt mm] (tam sayı)"
        },
        "dpi": {
          "type": "integer",
          "minimum": 72,
          "maximum": 4800,
          "description": "Çıktı çözünürlüğü (varsayılan 300) (tam sayı)"
        },
        "sayfa": {
          "type": "integer",
          "minimum": 1,
          "maximum": 10000,
          "description": "Hangi sayfa (1'den başlar). sayfa işleminde verilmezse bütün sayfalar değişir (tam sayı)"
        },
        "yeni_sira": {
          "type": "integer",
          "minimum": 1,
          "maximum": 10000,
          "description": "sayfatasi için sayfanın gideceği sıra (tam sayı)"
        },
        "katman": {
          "type": "string",
          "description": "atlas: hangi katmanın nesneleri için bir sayfa basılacak; 'yok' atlası kapatır (metin)"
        },
        "sirala": {
          "type": "string",
          "description": "atlas: sayfaların sıralanacağı ve adlandırılacağı öznitelik sütunu; verilmezse nesne anahtarı (metin)"
        },
        "grup": {
          "type": "string",
          "description": "rapor: bölümlerin oluşturulacağı öznitelik sütunu (ada_no gibi); verilmezse tek bölüm (metin)"
        },
        "kenar_payi": {
          "type": "integer",
          "minimum": 0,
          "maximum": 200,
          "description": "atlas: nesnenin çevresinde bırakılacak pay, yüzde (varsayılan 10) (tam sayı)"
        },
        "tek_dosya": {
          "type": "boolean",
          "description": "atlas: tek çok sayfalı belge mi, nesne başına bir dosya mı (varsayılan evet) (evet/hayır)"
        }
      },
      "required": [
        "islem"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": true
    },
    "_meta": {
      "cad.kentos/commandId": "core.layout",
      "cad.kentos/category": "Dosya",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÇIKTIYERLEŞİMİ",
        "CIKTIYERLESIMI",
        "LAYOUT",
        "ÇYR",
        "CYR"
      ]
    }
  },
  {
    "name": "core_layout_item",
    "title": "Çıktı Öğesi",
    "description": "Bir çıktı yerleşiminin üzerindeki öğeleri yönetir: harita çerçevesi, başlık, ölçek çubuğu, kuzey oku, lejant, resim, şekil ve tablo ekler, taşır, ayarlar ve siler.\nKomut: ÇIKTIÖĞE (CIKTIOGE, LAYOUTITEM, ÇÖĞ, COG)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "islem": {
          "type": "string",
          "enum": [
            "listele",
            "ekle",
            "sil",
            "tasi",
            "ayarla",
            "ad"
          ],
          "description": "Ne yapılacağı (metin)"
        },
        "yerlesim": {
          "type": "string",
          "description": "Hangi çıktı yerleşimi; çizimde tek yerleşim varsa gerekmez (metin)"
        },
        "ad": {
          "type": "string",
          "description": "Öğe adı; ekle dışında gerekir, ekle'de verilmezse türetilir (metin)"
        },
        "tur": {
          "type": "string",
          "enum": [
            "harita",
            "metin",
            "olcek",
            "kuzey",
            "lejant",
            "resim",
            "sekil",
            "tablo",
            "grafik"
          ],
          "description": "islem=ekle için öğe türü (metin)"
        },
        "x": {
          "type": "number",
          "description": "Sol kenardan uzaklık [kâğıt mm] (sayı)"
        },
        "y": {
          "type": "number",
          "description": "ÜST kenardan uzaklık [kâğıt mm] (sayı)"
        },
        "genislik": {
          "type": "number",
          "description": "Genişlik [kâğıt mm] (sayı)"
        },
        "yukseklik": {
          "type": "number",
          "description": "Yükseklik [kâğıt mm] (sayı)"
        },
        "metin": {
          "type": "string",
          "description": "Metin öğesinin yazısı; <yerlesim>, <olcek>, <tarih>, <crs> yer tutucuları çizim anında çözülür (metin)"
        },
        "yazi": {
          "type": "number",
          "description": "Yazı yüksekliği [kâğıt mm] (sayı)"
        },
        "olcek": {
          "type": "integer",
          "minimum": 0,
          "maximum": 100000000,
          "description": "Harita öğesinin ölçeği 1:N; 0 kapsama uyar (tam sayı)"
        },
        "pencere": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 2
            }
          ],
          "description": "Harita çerçevesinin bakacağı alanın iki köşesi, anahtar iki kez yazılarak: pencere=x1,y1 pencere=x2,y2. Tuvalden çerçeve seçmek bu satırı yazar [ZEMİN koordinatı — kâğıt değil] — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "izgara": {
          "type": "string",
          "enum": [
            "yok",
            "arti",
            "cizgi",
            "centik"
          ],
          "description": "Harita öğesinin koordinat ızgarası (metin)"
        },
        "izgara_aralik": {
          "type": "integer",
          "minimum": 0,
          "maximum": 1000000000,
          "description": "Izgara aralığı, zemin milimetresi; 0 ölçeğe göre seçilir (tam sayı)"
        },
        "kilit": {
          "type": "boolean",
          "description": "Öğeyi taşımaya kapatır (evet/hayır)"
        },
        "cerceve": {
          "type": "boolean",
          "description": "Öğenin çevresine çerçeve çizer (evet/hayır)"
        },
        "sayfa": {
          "type": "integer",
          "minimum": 1,
          "maximum": 10000,
          "description": "Öğenin duracağı sayfa (1'den başlar); tasi ile verilir (tam sayı)"
        },
        "yeni_ad": {
          "type": "string",
          "description": "islem=ad için öğenin yeni adı (metin)"
        },
        "satir_siniri": {
          "type": "integer",
          "minimum": 0,
          "maximum": 100000,
          "description": "Tablo öğesinin yazacağı en çok satır; 0 = kutuya kaç satır sığıyorsa o kadar (tam sayı)"
        },
        "sutunlar": {
          "type": "array",
          "items": {
            "type": "string"
          },
          "maxItems": 64,
          "description": "Tablo öğesinin yazacağı öznitelik sütunları, sırasıyla; anahtar birden çok kez yazılır. Verilmezse katmanın bütün sütunları, 'hepsi' listeyi boşaltır (metin)"
        },
        "katmanlar": {
          "type": "array",
          "items": {
            "type": "string"
          },
          "maxItems": 64,
          "description": "Harita çerçevesinin çizeceği katmanlar; anahtar birden çok kez yazılır. Verilmezse görünür bütün katmanlar, 'hepsi' listeyi boşaltır (metin)"
        },
        "harita": {
          "type": "string",
          "description": "Bu öğenin bağlı olduğu harita çerçevesinin adı. Verilmezse ilk harita. 'ilk' bağı kaldırır (metin)"
        },
        "sira": {
          "type": "integer",
          "minimum": -1000,
          "maximum": 1000,
          "description": "Çizim sırası; büyük olan üstte (tam sayı)"
        }
      },
      "required": [
        "islem"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": true
    },
    "_meta": {
      "cad.kentos/commandId": "core.layout_item",
      "cad.kentos/category": "Dosya",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÇIKTIÖĞE",
        "CIKTIOGE",
        "LAYOUTITEM",
        "ÇÖĞ",
        "COG"
      ]
    }
  },
  {
    "name": "core_layout_template",
    "title": "Çıktı Şablonu",
    "description": "Kurumun standart çıktı yerleşimlerini saklar ve uygular. Şablon çizimin dışında, kullanıcı profilinde durur; her çizime uygulanabilir. Şablon düzeni taşır, zemin koordinatlarını taşımaz.\nKomut: ÇIKTIŞABLON (CIKTISABLON, LAYOUTTEMPLATE, ÇŞB, CSB)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "islem": {
          "type": "string",
          "enum": [
            "listele",
            "kaydet",
            "uygula",
            "sil"
          ],
          "description": "Ne yapılacağı (metin)"
        },
        "ad": {
          "type": "string",
          "description": "Şablonun adı; listele dışında gerekir (metin)"
        },
        "yerlesim": {
          "type": "string",
          "description": "kaydet: hangi yerleşim saklanacak (tek yerleşim varsa gerekmez). uygula: kurulacak yerleşimin adı (verilmezse şablonun adı) (metin)"
        }
      },
      "required": [
        "islem"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": true
    },
    "_meta": {
      "cad.kentos/commandId": "core.layout_template",
      "cad.kentos/category": "Dosya",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÇIKTIŞABLON",
        "CIKTISABLON",
        "LAYOUTTEMPLATE",
        "ÇŞB",
        "CSB"
      ]
    }
  },
  {
    "name": "core_leader",
    "title": "Lider",
    "description": "Bir noktayı gösteren oklu çizgi çizer, istenirse yanına yazı koyar.\nKomut: LİDER (LIDER, LEADER, LD)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "minItems": 2
            }
          ],
          "description": "Okun ucundan yazının yanına köşeler — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "metin": {
          "type": "string",
          "description": "Son köşenin yanına yazılacak metin (metin)"
        },
        "stil": {
          "type": "string",
          "description": "Ok ve yazı boyunu veren ölçü stili; varsayılan ISO-25 (metin)"
        },
        "katalog": {
          "type": "string",
          "description": "Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri (metin)"
        }
      },
      "required": [
        "noktalar"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.leader",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "LİDER",
        "LIDER",
        "LEADER",
        "LD"
      ]
    }
  },
  {
    "name": "core_lengthen",
    "title": "Uzunluk",
    "description": "Çizginin bir ucunu kendi doğrultusunda hareket ettirerek uzunluğunu değiştirir.\nKomut: UZUNLUK (LENGTHEN, UZN)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uzunluğu değişecek çizgi — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "delta": {
          "type": "number",
          "description": "Eklenecek uzunluk (m); eksi kısaltır [m] (sayı)"
        },
        "yuzde": {
          "type": "number",
          "description": "İstenen uzunluk, şimdikinin yüzdesi (sayı)"
        },
        "toplam": {
          "type": "number",
          "description": "İstenen toplam uzunluk (m) [m] (sayı)"
        },
        "uc": {
          "type": "string",
          "enum": [
            "son",
            "bas"
          ],
          "description": "Hangi uç hareket eder; varsayılan son (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.lengthen",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "UZUNLUK",
        "LENGTHEN",
        "UZN"
      ]
    }
  },
  {
    "name": "core_line",
    "title": "Çizgi",
    "description": "İki veya daha fazla nokta arasında doğru parçaları çizer.\nKomut: ÇİZGİ (CIZGI, LINE, Ç, L)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "minItems": 2
            }
          ],
          "description": "Ardışık doğru parçalarının köşe noktaları — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [
        "noktalar"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.line",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÇİZGİ",
        "CIZGI",
        "LINE",
        "Ç",
        "L"
      ]
    }
  },
  {
    "name": "core_match_style",
    "title": "Stil Kopyala",
    "description": "Bir nesnenin stilini seçilen nesnelere uygular.\nKomut: STİLKOPYALA (STILKOPYALA, MATCHPROP, SK)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "kaynak": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Stili kopyalanacak nesnenin kimliği; yoksa tıklanan nesne — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Stili alacak nesnelerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Kaynak nesnenin üzerinde bir nokta; yalnız kaynak verilmediğinde — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.match_style",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "STİLKOPYALA",
        "STILKOPYALA",
        "MATCHPROP",
        "SK"
      ]
    }
  },
  {
    "name": "core_measure",
    "title": "Ölç",
    "description": "Noktalar arasındaki mesafeyi, koordinat farkını ve açıyı yazar; ikiden fazla nokta kenarları ve toplam uzunluğu verir.\nKomut: ÖLÇ (OLC, MEASURE, MS)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "baslangic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Ölçümün ilk noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Ölçümün ikinci noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "devam": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              }
            }
          ],
          "description": "Sonraki noktalar: her biri bir kenar daha ekler, toplam da yazılır — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [
        "baslangic",
        "bitis"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.measure",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÖLÇ",
        "OLC",
        "MEASURE",
        "MS"
      ]
    }
  },
  {
    "name": "core_measure_angle",
    "title": "Açı Ölç",
    "description": "Bir tepeden çıkan iki kol arasındaki açıyı ölçer, oturumun açı kuralıyla yazar.\nKomut: AÇIÖLÇ (ACIOLC, MEASUREANGLE, AÇÖ)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "tepe": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Açının tepe noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "birinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Birinci kolun üzerinde bir nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "ikinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "İkinci kolun üzerinde bir nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [
        "tepe",
        "birinci",
        "ikinci"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.measure_angle",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "AÇIÖLÇ",
        "ACIOLC",
        "MEASUREANGLE",
        "AÇÖ"
      ]
    }
  },
  {
    "name": "core_measure_area",
    "title": "Alan Ölç",
    "description": "Seçilen nesnelerin ya da köşeleri gösterilen bir alanın alanını ve çevresini yazar.\nKomut: ALANÖLÇ (ALANOLC, AREAOF, AÖ)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ölçülecek nesnelerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "yontem": {
          "type": "string",
          "enum": [
            "nesne",
            "nokta"
          ],
          "description": "nesne: seçilen nesnelerin alanı (öntanımlı); nokta: köşeleri gösterilen alan (metin)"
        },
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              }
            }
          ],
          "description": "yontem=nokta için alanın köşeleri; verilirse yöntem kendiliğinden nokta olur — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.measure_area",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ALANÖLÇ",
        "ALANOLC",
        "AREAOF",
        "AÖ"
      ]
    }
  },
  {
    "name": "core_merge",
    "title": "Tevhit",
    "description": "Komşu parselleri tek parselde birleştirir (tevhit).\nKomut: TEVHİT (TEVHIT, MERGE, TVH)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Birleştirilecek parseller; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.merge",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "TEVHİT",
        "TEVHIT",
        "MERGE",
        "TVH"
      ]
    }
  },
  {
    "name": "core_mirror",
    "title": "Aynala",
    "description": "Seçilen nesneleri iki noktadan geçen eksende aynalar.\nKomut: AYNALA (MIRROR, AYN)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Aynalanacak nesnelerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "baslangic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Ayna ekseninin ilk noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Ayna ekseninin ikinci noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [
        "baslangic",
        "bitis"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.mirror",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "AYNALA",
        "MIRROR",
        "AYN"
      ]
    }
  },
  {
    "name": "core_move",
    "title": "Taşı",
    "description": "Seçilen nesneleri iki nokta arasındaki kadar taşır.\nKomut: TAŞI (TASI, MOVE, TŞ)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Taşınacak nesnelerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "baslangic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Taşımanın başlangıç noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Taşımanın bitiş noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [
        "baslangic",
        "bitis"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.move",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "TAŞI",
        "TASI",
        "MOVE",
        "TŞ"
      ]
    }
  },
  {
    "name": "core_offset",
    "title": "Ofset",
    "description": "Seçili nesnelerin verilen mesafede, gösterilen tarafta paralelini çizer: açık çizgiye tek yanda çizgi, alana delikleriyle alan, daireye daire.\nKomut: OFSET (OFFSET, OF)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ofseti alınacak nesneler; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "mesafe": {
          "type": "integer",
          "description": "Paralel mesafesi, milimetre. Taraf verilmez ve gösterilmezse işaret anlam taşır: kapalı şekilde artı dışarı, eksi içeri (tam sayı)"
        },
        "kose": {
          "type": "string",
          "description": "KÖŞE | YUVARLAK | PAH — dış köşenin biçimi (metin)"
        },
        "taraf": {
          "type": "string",
          "enum": [
            "sol",
            "sag",
            "dis",
            "ic",
            "iki"
          ],
          "description": "Paralelin tarafı: açık çizgide sol ya da sag (çizim yönüne göre), kapalı şekilde dis ya da ic, iki her iki yan (metin)"
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Tarafı gösteren nokta: her nesnenin paraleli bu noktanın olduğu yana düşer — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "kaynak": {
          "type": "string",
          "enum": [
            "koru",
            "sil"
          ],
          "description": "Kaynak nesne: koru (öntanımlı) ya da paralel çizilince sil (metin)"
        },
        "ozellik": {
          "type": "string",
          "enum": [
            "kaynak",
            "aktif"
          ],
          "description": "Paralelin katmanı ve stili: kaynak nesneninki (öntanımlı) ya da etkin katman (metin)"
        },
        "oznitelik": {
          "type": "string",
          "enum": [
            "aktar",
            "aktarma"
          ],
          "description": "Kaynağın öznitelik değerleri: paralele aktar (öntanımlı) ya da aktarma — parselin içine çizilen çekme hattı gibi kaynağın kendisi olmayan bir çizgi için (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.offset",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "OFSET",
        "OFFSET",
        "OF"
      ]
    }
  },
  {
    "name": "core_pan",
    "title": "Kaydır",
    "description": "Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır.\nKomut: KAYDIR (PAN, KY)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "baslangic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Kaydırmanın tutulacağı nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "O noktanın taşınacağı yer — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [
        "baslangic",
        "bitis"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.pan",
      "cad.kentos/category": "Görünüm",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KAYDIR",
        "PAN",
        "KY"
      ]
    }
  },
  {
    "name": "core_paste",
    "title": "Yapıştır",
    "description": "Panodaki nesneleri çizime koyar; tek geri alma adımı.\nKomut: YAPIŞTIR (YAPISTIR, PASTE, YP)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Yapıştırılacak yerin sol alt köşesi; yerinde=evet ile gereksiz — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "yerinde": {
          "type": "boolean",
          "description": "Kopyalandığı koordinatlara yapıştırır (evet/hayır)"
        },
        "dosya": {
          "type": "string",
          "description": "Okunacak pano dosyası; verilmezse ortak pano dosyası (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.paste",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "YAPIŞTIR",
        "YAPISTIR",
        "PASTE",
        "YP"
      ]
    }
  },
  {
    "name": "core_pedit",
    "title": "Çizgi Düzenle",
    "description": "Çizgiyi kapatır, açar, yönünü çevirir ya da yakın köşelerini atarak sadeleştirir.\nKomut: ÇİZGİDÜZENLE (CIZGIDUZENLE, PEDIT, ÇZD, CZD)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Düzenlenecek çizgiler — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "islem": {
          "type": "string",
          "enum": [
            "kapat",
            "ac",
            "ters",
            "sadelestir"
          ],
          "description": "kapat: kapalı alana çevir · ac: aç · ters: yönünü çevir · sadelestir: yakın köşeleri at (metin)"
        },
        "tolerans": {
          "type": "number",
          "description": "sadelestir: bu uzaklıktan yakın köşeler atılır (m) [m] (sayı)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.pedit",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÇİZGİDÜZENLE",
        "CIZGIDUZENLE",
        "PEDIT",
        "ÇZD",
        "CZD"
      ]
    }
  },
  {
    "name": "core_perp_offset",
    "title": "Dik Ayak",
    "description": "Taban çizgisine göre dik ayak ve dik boy vererek nokta yerleştirir.\nKomut: DİKAYAK (DIKAYAK, PERPOFFSET, DA)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "baslangic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Taban çizgisinin ilk noktası (A) — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Taban çizgisinin ikinci noktası (B) — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "ayak": {
          "type": "number",
          "description": "A'dan taban boyunca uzaklık (m); boy ile sırayla eşleşir (sayı)"
        },
        "boy": {
          "type": "number",
          "description": "Tabana dik uzaklık (m); A→B yönünde SOL pozitiftir (sayı)"
        },
        "cizgi": {
          "type": "boolean",
          "description": "Yerleştirilen noktaları verildikleri sırayla çizgiyle birleştirir (evet/hayır)"
        }
      },
      "required": [
        "baslangic",
        "bitis"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.perp_offset",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "DİKAYAK",
        "DIKAYAK",
        "PERPOFFSET",
        "DA"
      ]
    }
  },
  {
    "name": "core_point_along",
    "title": "Ara Nokta",
    "description": "İki nokta arasındaki doğru üzerinde oran, uzaklık ya da eşit bölmeyle nokta koyar.\nKomut: ARANOKTA (POINTALONG, ARN)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "birinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Doğrunun ilk noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "ikinci": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Doğrunun ikinci noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "yontem": {
          "type": "string",
          "enum": [
            "oran",
            "mesafe"
          ],
          "description": "oran: 0 ile 1 arası · mesafe: ilk noktadan metre (metin)"
        },
        "deger": {
          "type": "number",
          "description": "Oran ya da uzaklık; birden çok verilebilir (sayı)"
        },
        "sayi": {
          "type": "integer",
          "minimum": 2,
          "maximum": 1000,
          "description": "Doğruyu bu kadar eşit parçaya böler (tam sayı)"
        }
      },
      "required": [
        "birinci",
        "ikinci"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.point_along",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ARANOKTA",
        "POINTALONG",
        "ARN"
      ]
    }
  },
  {
    "name": "core_point_draw",
    "title": "Nokta",
    "description": "Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper.\nKomut: NOKTA (POINT, NK)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "minItems": 1
            }
          ],
          "description": "Yerleştirilecek noktalar — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [
        "noktalar"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.point_draw",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "NOKTA",
        "POINT",
        "NK"
      ]
    }
  },
  {
    "name": "core_points",
    "title": "Nokta Listesi",
    "description": "Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod).\nKomut: NOKTALAR (POINTS, NKL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "dosya": {
          "type": "string",
          "description": "Nokta listesi dosyasının yolu (metin)"
        },
        "yon": {
          "type": "string",
          "description": "oku (varsayılan) | yaz (metin)"
        },
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "yon=yaz ile: köşeleri yazılacak nesneler; verilmezse çizimdeki noktalar — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "eksen": {
          "type": "string",
          "description": "Sütun sırası: YX (varsayılan, Türkiye'de olağan) | XY (metin)"
        }
      },
      "required": [
        "dosya"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": true
    },
    "_meta": {
      "cad.kentos/commandId": "core.points",
      "cad.kentos/category": "Dosya",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "NOKTALAR",
        "POINTS",
        "NKL"
      ]
    }
  },
  {
    "name": "core_polygon_regular",
    "title": "Düzgün Çokgen",
    "description": "Merkez ve kenar sayısından düzgün çokgen çizer: içten, dıştan ya da kenar uzunluğundan.\nKomut: ÇOKGEN (COKGEN, POLYGONREG, ÇKG, CKG)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "merkez": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Çokgenin merkezi — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "kenar_sayisi": {
          "type": "integer",
          "minimum": 3,
          "maximum": 1024,
          "description": "Kenar sayısı (tam sayı)"
        },
        "yontem": {
          "type": "string",
          "enum": [
            "ic",
            "dis",
            "kenar"
          ],
          "description": "ic: köşeler çemberin üzerinde · dis: kenarlar çembere teğet · kenar: kenar uzunluğundan (metin)"
        },
        "yaricap": {
          "type": "number",
          "description": "ic/dis yönteminin yarıçapı (m) [m] (sayı)"
        },
        "kenar_uzunlugu": {
          "type": "number",
          "description": "kenar yönteminin uzunluğu (m) [m] (sayı)"
        },
        "aci": {
          "type": "number",
          "description": "İlk köşenin merkeze göre doğrultusu; varsayılan 0 (sayı)"
        },
        "kose": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Yerine işaret edilen nokta: yarıçapı ve yönü verir; yaricap verilmişse sorulmaz — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [
        "merkez",
        "kenar_sayisi"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.polygon_regular",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÇOKGEN",
        "COKGEN",
        "POLYGONREG",
        "ÇKG",
        "CKG"
      ]
    }
  },
  {
    "name": "core_polyline",
    "title": "Çoklu Çizgi",
    "description": "Birden çok noktadan TEK bir çizgi nesnesi çizer.\nKomut: ÇOKLUÇİZGİ (COKLUCIZGI, POLYLINE, ÇÇ, PL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "minItems": 2
            }
          ],
          "description": "Çoklu çizginin köşe noktaları; hepsi tek nesne olur — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [
        "noktalar"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.polyline",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÇOKLUÇİZGİ",
        "COKLUCIZGI",
        "POLYLINE",
        "ÇÇ",
        "PL"
      ]
    }
  },
  {
    "name": "core_print",
    "title": "Yazdır",
    "description": "Çizimin bir penceresini bir yazdırma profilinin kâğıdına yerleştirip PDF dosyasına yazar ya da yazıcıya gönderir.\nKomut: YAZDIR (PRINT, PLOT, YZDR)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "pencere": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 2
            }
          ],
          "description": "Yazdırılacak alanın iki köşesi; merkez verilmezse ve bu da verilmezse tıklatılır — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "merkez": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Kâğıdın ortalanacağı nokta; pencere yerine kullanılır — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "olcek": {
          "type": "integer",
          "description": "Ölçek paydası (1000 = 1/1000); merkez ile kullanılır, verilmezse projenin plan ölçeği (tam sayı)"
        },
        "yerlesim": {
          "type": "string",
          "description": "Basılacak çıktı yerleşiminin adı (ÇIKTIYERLEŞİMİ ile kurulur). Verildiğinde kâğıt, kenar ve harita penceresi yerleşimden gelir; pencere, merkez, olcek ve profil ile birlikte verilmez (metin)"
        },
        "dosya": {
          "type": "string",
          "description": "PDF yazılacak dosya; yazici ile birlikte verilmez (metin)"
        },
        "yazici": {
          "type": "string",
          "description": "Yazıcının adı; \"\" sistem varsayılanı. dosya ile birlikte verilmez (metin)"
        },
        "profil": {
          "type": "string",
          "description": "Yazdırma profili; verilmezse varsayılan profil (metin)"
        },
        "kagit": {
          "type": "string",
          "description": "Kâğıt: A5, A4, A3, A2, A1, A0 ya da ozel (genislik ve yukseklik ile) (metin)"
        },
        "genislik": {
          "type": "integer",
          "description": "ozel kâğıdın eni, milimetre (dikey duruşta) (tam sayı)"
        },
        "yukseklik": {
          "type": "integer",
          "description": "ozel kâğıdın boyu, milimetre (dikey duruşta) (tam sayı)"
        },
        "yon": {
          "type": "string",
          "description": "dikey ya da yatay (metin)"
        },
        "dpi": {
          "type": "integer",
          "description": "Çözünürlük, inç başına nokta (72–4800) (tam sayı)"
        },
        "kenar": {
          "type": "integer",
          "description": "Dört yandaki kenar boşluğu, milimetre (tam sayı)"
        },
        "baslik": {
          "type": "string",
          "description": "PDF belge başlığı (metin)"
        },
        "yazar": {
          "type": "string",
          "description": "PDF yazar alanı (metin)"
        },
        "sifre": {
          "type": "string",
          "description": "PDF açma şifresi (kullanıcı şifresi); günlüğe yazılmaz (metin)"
        },
        "sahip_sifresi": {
          "type": "string",
          "description": "PDF izinlerini değiştirme şifresi (sahip şifresi); günlüğe yazılmaz (metin)"
        },
        "yazdirilabilir": {
          "type": "boolean",
          "description": "Şifreli PDF: sahip şifresi olmayan yazdırabilir mi; varsayılan evet (evet/hayır)"
        },
        "kopyalanabilir": {
          "type": "boolean",
          "description": "Şifreli PDF: metin ve grafik kopyalanabilir mi; varsayılan evet (evet/hayır)"
        },
        "degistirilebilir": {
          "type": "boolean",
          "description": "Şifreli PDF: belge değiştirilebilir mi; varsayılan evet (evet/hayır)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": true
    },
    "_meta": {
      "cad.kentos/commandId": "core.print",
      "cad.kentos/category": "Dosya",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "YAZDIR",
        "PRINT",
        "PLOT",
        "YZDR"
      ]
    }
  },
  {
    "name": "core_rectangle",
    "title": "Dikdörtgen",
    "description": "Karşılıklı iki köşeden ya da bir kenar ve yükseklikten dört köşeli kapalı bir alan çizer.\nKomut: DİKDÖRTGEN (DIKDORTGEN, RECTANGLE, DKD, REC)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "minItems": 2,
              "maxItems": 3
            }
          ],
          "description": "2n: karşılıklı iki köşe · 3n: bir kenarın iki köşesi ve karşı kenarın geçtiği nokta — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "yontem": {
          "type": "string",
          "enum": [
            "2n",
            "3n"
          ],
          "description": "2n: karşılıklı iki köşe, eksenlere paralel · 3n: bir kenar ve yükseklik, döndürülmüş (metin)"
        }
      },
      "required": [
        "noktalar"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.rectangle",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "DİKDÖRTGEN",
        "DIKDORTGEN",
        "RECTANGLE",
        "DKD",
        "REC"
      ]
    }
  },
  {
    "name": "core_reproject",
    "title": "Dönüştür",
    "description": "Çizimin tamamını bir koordinat sisteminden diğerine dönüştürür.\nKomut: DÖNÜŞTÜR (DONUSTUR, REPROJECT, DNS)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "hedef": {
          "type": "string",
          "description": "Hedef koordinat sistemi, örnek EPSG:5256 ya da TUREF/TM36 (metin)"
        },
        "kaynak": {
          "type": "string",
          "description": "Kaynak sistem; yoksa çizimin kendi koordinat sistemi (metin)"
        }
      },
      "required": [
        "hedef"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.reproject",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "DÖNÜŞTÜR",
        "DONUSTUR",
        "REPROJECT",
        "DNS"
      ]
    }
  },
  {
    "name": "core_rotate",
    "title": "Döndür",
    "description": "Seçilen nesneleri bir merkez etrafında döndürür.\nKomut: DÖNDÜR (DONDUR, ROTATE, DÖN)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Döndürülecek nesnelerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "merkez": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Döndürme merkezi — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "aci": {
          "type": "number",
          "description": "Dönme açısı, derece; artı yön saat yönünün tersi. Verilmezse yeni doğrultu gösterilir (sayı)"
        },
        "aci_nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Dönme açısının gösterildiği nokta; aci verilmişse sorulmaz — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [
        "merkez"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.rotate",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "DÖNDÜR",
        "DONDUR",
        "ROTATE",
        "DÖN"
      ]
    }
  },
  {
    "name": "core_scale",
    "title": "Ölçekle",
    "description": "Seçilen nesneleri bir merkeze göre büyütür ya da küçültür.\nKomut: ÖLÇEKLE (OLCEKLE, SCALE, ÖLÇEK, OLCEK)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ölçeklenecek nesnelerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "merkez": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Ölçekleme merkezi; bu nokta yerinde kalır — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "carpan": {
          "type": "number",
          "description": "Ölçek çarpanı; sıfırdan büyük. Verilmezse merkezden uzaklık gösterilir (sayı)"
        },
        "carpan_nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Çarpanın gösterildiği nokta; carpan verilmişse sorulmaz — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [
        "merkez"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.scale",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÖLÇEKLE",
        "OLCEKLE",
        "SCALE",
        "ÖLÇEK",
        "OLCEK"
      ]
    }
  },
  {
    "name": "core_sector",
    "title": "Daire Dilimi",
    "description": "Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir.\nKomut: DİLİM (DILIM, SECTOR, DL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "merkez": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Dilimin merkezi — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "baslangic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "İlk kenarın ucu; yarıçapı bu belirler — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "İkinci kenarın yönü; süpürme saat yönünün tersinedir — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [
        "merkez",
        "baslangic",
        "bitis"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.sector",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "DİLİM",
        "DILIM",
        "SECTOR",
        "DL"
      ]
    }
  },
  {
    "name": "core_set_layer",
    "title": "Katmana Ata",
    "description": "Seçilen nesneleri başka bir katmana taşır.\nKomut: KATMANAT (KATMANATA, SETLAYER, KA)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Taşınacak nesnelerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "katman": {
          "type": "string",
          "description": "Hedef katmanın adı; yoksa oluşturulur (metin)"
        }
      },
      "required": [
        "katman"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.set_layer",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KATMANAT",
        "KATMANATA",
        "SETLAYER",
        "KA"
      ]
    }
  },
  {
    "name": "core_spline",
    "title": "Spline",
    "description": "Kontrol noktalarından NURBS eğrisi (spline) çizer.\nKomut: SPLINE (SPLINE, SPLINE, SPL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "minItems": 2
            }
          ],
          "description": "Kontrol noktaları — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "derece": {
          "type": "integer",
          "description": "Eğrinin derecesi, 1–15; varsayılan 3 (tam sayı)"
        },
        "kapali": {
          "type": "boolean",
          "description": "Son noktadan ilkine kapansın mı; varsayılan hayır (evet/hayır)"
        }
      },
      "required": [
        "noktalar"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.spline",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "SPLINE",
        "SPLINE",
        "SPLINE",
        "SPL"
      ]
    }
  },
  {
    "name": "core_split",
    "title": "Böl",
    "description": "Nesneleri çizilen bir kesme çizgisiyle böler.\nKomut: BÖL (BOL, SPLIT, BL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kesilecek nesneler; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 2
            }
          ],
          "description": "Kesme çizgisinin iki noktası; arayüzde çizilir — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Bölme noktası (tek çizgi; eski biçim) — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.split",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "BÖL",
        "BOL",
        "SPLIT",
        "BL"
      ]
    }
  },
  {
    "name": "core_split_area",
    "title": "Alana Göre İfraz",
    "description": "Parselden verilen yöne paralel, istenen alanda bir parça ayırır.\nKomut: ALANİFRAZ (ALANIFRAZ, SPLITAREA, ALİF)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "yon": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 2
            }
          ],
          "description": "Ayırma çizgisinin YÖNÜ: iki nokta (yol cephesi, mevcut sınır) — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ayrılacak parsel; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "alan": {
          "type": "integer",
          "description": "Ayrılacak alan, mm² (400 m² = 400000000) (tam sayı)"
        },
        "tolerans": {
          "type": "integer",
          "description": "Kabul toleransı, mm²; varsayılan 10000 (0,01 m²) (tam sayı)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.split_area",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ALANİFRAZ",
        "ALANIFRAZ",
        "SPLITAREA",
        "ALİF"
      ]
    }
  },
  {
    "name": "core_split_parcel",
    "title": "İfraz",
    "description": "Bir parseli düz bir ayırma çizgisiyle ikiye böler (ifraz).\nKomut: İFRAZ (IFRAZ, SUBDIVIDE, İFR)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 2
            }
          ],
          "description": "Ayırma çizgisinin iki ucu — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ayrılacak parsel; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.split_parcel",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "İFRAZ",
        "IFRAZ",
        "SUBDIVIDE",
        "İFR"
      ]
    }
  },
  {
    "name": "core_stakeout",
    "title": "Aplikasyon",
    "description": "İstasyondan her noktaya mesafe ve açı listesi çıkarır (aplikasyon).\nKomut: APLİKASYON (APLIKASYON, STAKEOUT, APL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "istasyon": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Aletin durduğu nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "baglama": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Bağlama (arka görüş) noktası; verilirse açılar ondan ölçülür — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Aplike edilecek noktalar; yoksa seçim, o da boşsa çizimdeki bütün noktalar — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        }
      },
      "required": [
        "istasyon"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.stakeout",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "APLİKASYON",
        "APLIKASYON",
        "STAKEOUT",
        "APL"
      ]
    }
  },
  {
    "name": "core_stretch",
    "title": "Esnet",
    "description": "Pencere içindeki köşeleri taşır, dışındakileri yerinde bırakır.\nKomut: ESNET (STRETCH, ES)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "pencere": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "minItems": 2,
              "maxItems": 2
            }
          ],
          "description": "Esnetme penceresinin iki köşesi; içindeki köşeler taşınır — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "baslangic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Esnetmenin başlangıç noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Esnetmenin bitiş noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Yalnız bu nesneler esnetilir; verilmezse pencerenin dokunduğu her nesne — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        }
      },
      "required": [
        "pencere",
        "baslangic",
        "bitis"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.stretch",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ESNET",
        "STRETCH",
        "ES"
      ]
    }
  },
  {
    "name": "core_style",
    "title": "Stil",
    "description": "Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar.\nKomut: STİL (STIL, STYLE, ST)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "katman": {
          "type": "string",
          "description": "Stilin yazılacağı katmanın adı; katman var olmalı (metin)"
        },
        "paket": {
          "type": "string",
          "description": "Stil kataloğu paketinin dosya yolu (metin)"
        },
        "olcek_min": {
          "type": "integer",
          "description": "Bu ölçek paydasından daha yakında çizilmez (1:N'deki N) (tam sayı)"
        },
        "olcek_max": {
          "type": "integer",
          "description": "Bu ölçek paydasından daha uzakta çizilmez (tam sayı)"
        },
        "sinifla": {
          "type": "string",
          "description": "Sınıflandırmada kullanılacak öznitelik; her nesne kendi değerine göre stillenir (metin)"
        },
        "kod": {
          "type": "string",
          "description": "Katalogdaki satırın kimliği; verilmezse katalog kuralları eşleşir (metin)"
        },
        "olcek": {
          "type": "integer",
          "description": "Ölçek paydası (1:N); 0 = ölçekten bağımsız (tam sayı)"
        },
        "renk": {
          "type": "integer",
          "description": "Çizgi rengi, 0xAARRGGBB (tam sayı)"
        },
        "kalinlik": {
          "type": "integer",
          "description": "Çizgi kalınlığı, kâğıt mikrometresi (1000 = 1 mm) (tam sayı)"
        },
        "dolgu": {
          "type": "integer",
          "description": "Dolgu rengi, 0xAARRGGBB; 0 = dolgusuz (tam sayı)"
        },
        "sira": {
          "type": "integer",
          "description": "Çizim sırası; büyük olan üste gelir (tam sayı)"
        },
        "sifirla": {
          "type": "boolean",
          "description": "Stili siler; nesneler katman varsayılanına döner (evet/hayır)"
        },
        "tip": {
          "type": "string",
          "description": "Sembol katmanı tipi: cizgi, isaretci-cizgi, tarak-cizgi, dolgu, cizgi-desen-dolgu, nokta-desen-dolgu, merkez-isaretci, isaretci (metin)"
        },
        "ekle": {
          "type": "boolean",
          "description": "Katmanı mevcut sembolün üstüne ekler; yoksa sembolü değiştirir (evet/hayır)"
        },
        "sekil": {
          "type": "string",
          "description": "İşaretçi şekli: daire, kare, ucgen, baklava, yildiz, arti, carpi, ok, yarim-daire, besgen, altigen, cizik (metin)"
        },
        "yerlesim": {
          "type": "string",
          "description": "İşaretçinin çizgi üzerindeki yeri: aralik, tepe, ilk, son, orta (metin)"
        },
        "birim": {
          "type": "string",
          "description": "Ölçülerin birimi: kagit (µm), zemin (mm), piksel (metin)"
        },
        "boyut_birim": {
          "type": "string",
          "description": "Yalnız `boyut` için birim; verilmezse `birim` geçerlidir (metin)"
        },
        "aralik_birim": {
          "type": "string",
          "description": "Yalnız `aralik` için birim; verilmezse `birim` geçerlidir (metin)"
        },
        "aralik_y_birim": {
          "type": "string",
          "description": "Yalnız `aralik_y` için birim; verilmezse `birim` geçerlidir (metin)"
        },
        "kaydirma_birim": {
          "type": "string",
          "description": "Yalnız `kaydirma` için birim; verilmezse `birim` geçerlidir (metin)"
        },
        "boyut": {
          "type": "integer",
          "description": "İşaretçi çapı ya da tarak dişinin boyu, `birim` cinsinden (tam sayı)"
        },
        "aralik": {
          "type": "integer",
          "description": "Çizgi boyunca ya da desende birinci eksende aralık (tam sayı)"
        },
        "aralik_y": {
          "type": "integer",
          "description": "Nokta deseninde ikinci eksen; verilmezse kare desen (tam sayı)"
        },
        "aci": {
          "type": "integer",
          "description": "Desen açısı ya da işaretçi dönüklüğü, mikro derece (tam sayı)"
        },
        "kaydirma": {
          "type": "integer",
          "description": "Geometriden dik kaydırma, `birim` cinsinden (tam sayı)"
        },
        "faz": {
          "type": "integer",
          "description": "İlk işaretçinin çizgi boyunca kaç birim ileride başlayacağı; verilmezse aralığın yarısı (tam sayı)"
        },
        "faz_birim": {
          "type": "string",
          "description": "Yalnız `faz` için birim; verilmezse `birim` geçerlidir (metin)"
        },
        "saydamlik": {
          "type": "integer",
          "description": "Katman saydamlığı 0-255; 255 tam opak (tam sayı)"
        },
        "desen": {
          "type": "string",
          "description": "Çizgi tipi: sürekli, ya da çizgi kalınlığının katı olarak çizgi/boşluk uzunlukları — '8 1 1 1' gibi (kesik-nokta) (metin)"
        },
        "yazi": {
          "type": "string",
          "description": "yazi-isaretci katmanının yazdığı sabit metin (metin)"
        },
        "alan": {
          "type": "string",
          "description": "Nesneden alınacak parametreler, virgülle: sütun[:özellik[:tür]] — 'kod:yazi:metin, kat:kalinlik'. Sütun yoksa tanımlanır (metin)"
        }
      },
      "required": [
        "katman"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.style",
      "cad.kentos/category": "Katman",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "STİL",
        "STIL",
        "STYLE",
        "ST"
      ]
    }
  },
  {
    "name": "core_suggestion",
    "title": "Yapay Zeka Önerisi",
    "description": "Bekleyen yapay zeka önerilerini listeler, durumunu söyler, uygular ya da reddeder.\nKomut: ÖNERİ (ONERI, SUGGESTION, ÖN)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "islem": {
          "type": "string",
          "enum": [
            "uygula",
            "reddet",
            "durum",
            "listele"
          ],
          "description": "Ne yapılacağı: uygula, reddet, durum ya da listele (metin)"
        },
        "oneri": {
          "type": "string",
          "description": "Öneri kimliği; uygula, reddet ve durum için gerekir (metin)"
        }
      },
      "required": [
        "islem"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.suggestion",
      "cad.kentos/category": "Sistem",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ÖNERİ",
        "ONERI",
        "SUGGESTION",
        "ÖN"
      ]
    }
  },
  {
    "name": "core_survey_polar",
    "title": "Alım",
    "description": "İstasyondan okunan açı ve kenarlardan nokta hesaplar ve yerleştirir.\nKomut: ALIM (SURVEY, ALM)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "istasyon": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Aletin durduğu bilinen nokta — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "baglama": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Bağlama noktası: verilirse açılar ondan itibaren okunmuş sayılır — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "aci": {
          "type": "number",
          "description": "Okunan açı; kenar ile sırayla eşleşir [oturumun açı birimi] (sayı)"
        },
        "kenar": {
          "type": "number",
          "description": "Alete olan uzaklık (m) [m] (sayı)"
        },
        "cizgi": {
          "type": "boolean",
          "description": "Hesaplanan noktaları okundukları sırayla çizgiyle birleştirir (evet/hayır)"
        }
      },
      "required": [
        "istasyon"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.survey_polar",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ALIM",
        "SURVEY",
        "ALM"
      ]
    }
  },
  {
    "name": "core_symbol",
    "title": "Sembol",
    "description": "Gösterim rafını yükler, ağacında gezer ve içinde arar.\nKomut: SEMBOL (SEMBOLLER, SYMBOL, SMB)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "paket": {
          "type": "string",
          "description": "Yüklenecek gösterim paketinin dosya yolu (metin)"
        },
        "grup": {
          "type": "string",
          "description": "Gezilecek grup yolu, düzeyler '>' ile ayrılır (metin)"
        },
        "ara": {
          "type": "string",
          "description": "Etikette, kimlikte ve grup yolunda arar (metin)"
        },
        "kod": {
          "type": "string",
          "description": "Tek bir gösterimin ayrıntısı (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.symbol",
      "cad.kentos/category": "Katman",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "SEMBOL",
        "SEMBOLLER",
        "SYMBOL",
        "SMB"
      ]
    }
  },
  {
    "name": "core_text",
    "title": "Metin",
    "description": "Çizime metin yazar; yükseklik ve hizalama verilebilir.\nKomut: METİN (METIN, YAZI, TEXT, MT)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Yazının başlangıç noktası — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "yazi": {
          "type": "string",
          "description": "Yazılacak metin (metin)"
        },
        "yukseklik": {
          "type": "integer",
          "description": "Yazı yüksekliği, zeminde milimetre; yoksa proje ayarı (tam sayı)"
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Taban çizgisinin bitişi; yoksa yatay — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "hizalama": {
          "type": "string",
          "description": "sol, orta, sag veya merkez (metin)"
        }
      },
      "required": [
        "noktalar",
        "yazi"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.text",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "METİN",
        "METIN",
        "YAZI",
        "TEXT",
        "MT"
      ]
    }
  },
  {
    "name": "core_to_area",
    "title": "Alana Çevir",
    "description": "Uç uca değen çizgileri tek bir kapalı alana çevirir.\nKomut: ALANAÇEVİR (ALANACEVIR, TOAREA, ALÇ)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Birleştirilecek çizgilerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.to_area",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ALANAÇEVİR",
        "ALANACEVIR",
        "TOAREA",
        "ALÇ"
      ]
    }
  },
  {
    "name": "core_tool_search",
    "title": "Araç Ara",
    "description": "Ajan araç kataloğunda ad ve özete göre arar. Sonuç her zaman kaç aracın eşleştiğini, kaçının gösterildiğini ve katalogdaki toplam araç sayısını söyler: arama hiçbir aracı gizlemez, tam liste `tools/list` ile alınır.\nKomut: ARAÇARA (ARACARA, TOOLSEARCH, ARA)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "sorgu": {
          "type": "string",
          "description": "Aranan sözcük; ad ve özet içinde Türkçe katlamayla eşleşir (metin)"
        },
        "alan": {
          "type": "string",
          "enum": [
            "hepsi",
            "ad",
            "ozet"
          ],
          "description": "Nerede aranacağı: hepsi (öntanımlı), ad ya da ozet (metin)"
        },
        "sinir": {
          "type": "integer",
          "minimum": 1,
          "maximum": 200,
          "description": "En çok kaç sonuç gösterilsin; öntanımlı 20. Eşleşme sayısı her hâlde bildirilir (tam sayı)"
        }
      },
      "required": [
        "sorgu"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.tool_search",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "ARAÇARA",
        "ARACARA",
        "TOOLSEARCH",
        "ARA"
      ]
    }
  },
  {
    "name": "core_topology",
    "title": "Topoloji Denetimi",
    "description": "Kendini kesen sınır, sıfır alan ve örtüşen parselleri raporlar.\nKomut: TOPOLOJİ (TOPOLOJI, TOPOLOGY, TPL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Denetlenecek nesneler; yoksa seçim, o da boşsa bütün çizim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.topology",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "TOPOLOJİ",
        "TOPOLOJI",
        "TOPOLOGY",
        "TPL"
      ]
    }
  },
  {
    "name": "core_tracking",
    "title": "Geçici İzleme",
    "description": "Geçici izleme için nokta işaretler; iki işaretin izleri kesişir.\nKomut: İZ (IZ, TRACK, TRK)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "İşaretlenecek nokta; yoksa işaretler listelenir — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "sil": {
          "type": "boolean",
          "description": "Bütün işaretleri siler (evet/hayır)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.tracking",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "İZ",
        "IZ",
        "TRACK",
        "TRK"
      ]
    }
  },
  {
    "name": "core_trim",
    "title": "Buda",
    "description": "Tıklanan parçayı kesme sınırları arasından atar; çizgide, yayda ve dairede çalışır.\nKomut: BUDA (TRIM, BD)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Budanan nesneler, tıklama sırasıyla; yoksa her tıklamanın altındaki nesne — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "sinir": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kesme sınırları; yoksa seçili nesneler, o da yoksa tıklanan nesnenin yakınındaki her nesne — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "hepsi": {
          "type": "boolean",
          "description": "Tıklanan nesnenin yakınındaki her nesne sınırdır (seçim ve sinir yokken öntanımlı) (evet/hayır)"
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              }
            }
          ],
          "description": "Atılacak her parçanın üzerinde bir nokta, sırayla — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.trim",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "BUDA",
        "TRIM",
        "BD"
      ]
    }
  },
  {
    "name": "core_vertex_insert",
    "title": "Köşe Ekle",
    "description": "Bir kenarın ortasına yeni köşe ekler.\nKomut: KÖŞEEKLE (KOSEEKLE, ADDVERTEX, KE)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Köşe eklenecek nesnenin kimliği — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "kose": {
          "type": "integer",
          "description": "Yeni köşenin ardına geleceği köşe; ilk köşe 1'dir (tam sayı)"
        },
        "yer": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Kenarı gösteren nokta: kose verilmezse en yakın kenar, nesne de verilmezse altındaki nesne — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Yeni köşenin yeri — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [
        "nesne",
        "kose",
        "nokta"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.vertex_insert",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KÖŞEEKLE",
        "KOSEEKLE",
        "ADDVERTEX",
        "KE"
      ]
    }
  },
  {
    "name": "core_vertex_move",
    "title": "Köşe Taşı",
    "description": "Bir nesnenin köşesini ya da tutamağını yeni bir yere taşır.\nKomut: KÖŞETAŞI (KOSETASI, MOVEVERTEX, KT)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Köşesi taşınacak nesnenin kimliği — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "kose": {
          "type": "integer",
          "description": "Taşınacak köşenin sırası; ilk köşe 1'dir (tam sayı)"
        },
        "yer": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Köşeyi gösteren nokta: kose verilmezse en yakın köşe, nesne de verilmezse altındaki nesne — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Köşenin yeni yeri — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [
        "nesne",
        "kose",
        "nokta"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.vertex_move",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KÖŞETAŞI",
        "KOSETASI",
        "MOVEVERTEX",
        "KT"
      ]
    }
  },
  {
    "name": "core_zoom",
    "title": "Yakınlaş",
    "description": "Görünümü çizim kapsamına veya verilen çarpana ayarlar.\nKomut: YAKINLAŞ (YAKINLAS, ZOOM, Z)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "mod": {
          "type": "string",
          "description": "KAPSAM | ÇARPAN | SIFIRLA (metin)"
        },
        "carpan": {
          "type": "number",
          "description": "ÇARPAN modunda ölçek katsayısı (sayı)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.zoom",
      "cad.kentos/category": "Görünüm",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "YAKINLAŞ",
        "YAKINLAS",
        "ZOOM",
        "Z"
      ]
    }
  },
  {
    "name": "geodesy_traverse",
    "title": "Poligon Hesabı",
    "description": "Kırılma açısı ve kenarlardan poligon koordinatları hesaplar, kapanma hatalarını dağıtır ve mevzuat toleransına karşı denetler.\nKomut: POLİGON (POLIGON, TRAVERSE, PLG)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "baslangic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Başlangıç istasyonu (bilinen) — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "baglama": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Başlangıçtaki bağlama noktası (bilinen) — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "aci": {
          "type": "number",
          "description": "Her istasyonda okunan kırılma açısı, ölçü karnesi sırasıyla (sayı)"
        },
        "kenar": {
          "type": "number",
          "description": "Her istasyondan sonraki kenar (m) [m] (sayı)"
        },
        "bitis": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Bitiş istasyonu (bilinen); verilirse kapanma hesaplanır — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "bitis_baglama": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 1
            }
          ],
          "description": "Bitişteki bağlama noktası; açı kapanması için gerekir — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "sinif": {
          "type": "string",
          "enum": [
            "ana",
            "ara",
            "tamamlayici"
          ],
          "description": "Tolerans sınıfı; katalogdan okunur (metin)"
        },
        "ilk_no": {
          "type": "integer",
          "description": "İlk istasyonun nokta numarası; varsayılan 1 (tam sayı)"
        },
        "dagitim": {
          "type": "string",
          "enum": [
            "esit",
            "kenar"
          ],
          "description": "Kenar kapanmasının dağıtımı: eşit ya da kenar orantılı (metin)"
        },
        "cizgi": {
          "type": "boolean",
          "description": "Güzergâhı çizgiyle bağlar; varsayılan evet (evet/hayır)"
        }
      },
      "required": [
        "baslangic",
        "baglama"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "geodesy.traverse",
      "cad.kentos/category": "Çizim",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "POLİGON",
        "POLIGON",
        "TRAVERSE",
        "PLG"
      ]
    }
  },
  {
    "name": "gorunum_bilgisi",
    "title": "Görünüm Bilgisi",
    "description": "Ekranda görünen alanın köşe koordinatlarını, merkezini, ölçeğini ve CRS'ini bildirir.\nKomut: GÖRÜNÜMBİLGİSİ (GORUNUMBILGISI, VIEWINFO, GRB)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {},
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.view_info",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "GÖRÜNÜMBİLGİSİ",
        "GORUNUMBILGISI",
        "VIEWINFO",
        "GRB"
      ]
    }
  },
  {
    "name": "islem_alan_duzenle",
    "title": "ALANDÜZENLE",
    "description": "Kapalı bir alanı istenen alana getirir: bütün kenarları eşit daraltıp genişleterek, bir kenarı kaydırarak ya da bir köşeyi çekerek; şekil bozulmaz.\nKomut: ALANDÜZENLE (ALANDUZENLE, ADJUSTAREA, ADZ)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "kapsam": {
          "type": "string",
          "description": "secili (varsayılan), gorunum ya da proje: nesneler nereden alınır (metin)"
        },
        "pencere": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 2
            }
          ],
          "description": "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "katman": {
          "type": "string",
          "description": "Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman (metin)"
        },
        "alan": {
          "type": "number",
          "description": "Hedef alan, metrekare (sayı)"
        },
        "mod": {
          "type": "string",
          "enum": [
            "hepsi",
            "kenar",
            "kose"
          ],
          "description": "Nasıl getirileceği (hepsi / kenar / kose); varsayılan hepsi (metin)"
        },
        "kenar": {
          "type": "integer",
          "minimum": 1,
          "maximum": 1000000,
          "description": "Kaydırılacak kenar (ilk köşeden çıkan kenar 1); mod=kenar (tam sayı)"
        },
        "kose": {
          "type": "integer",
          "minimum": 1,
          "maximum": 1000000,
          "description": "Çekilecek köşe; mod=kose (tam sayı)"
        },
        "nokta": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Kenarın ya da köşenin gideceği yer; verilmezse arayüz sürükletir, komut satırı hedefe tam oturtur — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "islem.alan_duzenle",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "ALANDÜZENLE",
        "ALANDUZENLE",
        "ADJUSTAREA",
        "ADZ"
      ]
    }
  },
  {
    "name": "islem_bag_coz",
    "title": "BAĞÇÖZ",
    "description": "Kapsamdaki yazıların bağını çözer: yazı yerinde kalır, bağlı olduğu nesne bundan sonra tek başına taşınır.\nKomut: BAĞÇÖZ (BAGCOZ, DETACH, BÇ, BC)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "kapsam": {
          "type": "string",
          "description": "secili (varsayılan), gorunum ya da proje: nesneler nereden alınır (metin)"
        },
        "pencere": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 2
            }
          ],
          "description": "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "katman": {
          "type": "string",
          "description": "Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "islem.bag_coz",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "BAĞÇÖZ",
        "BAGCOZ",
        "DETACH",
        "BÇ",
        "BC"
      ]
    }
  },
  {
    "name": "islem_bagla",
    "title": "BAĞLA",
    "description": "Kapsamdaki yazıları seçilen nesnenin en yakın kenarına ya da köşesine bağlar: nesne taşınınca yazı izler; istenirse yazı kenarın uzunluğu olur.\nKomut: BAĞLA (BAGLA, ATTACH, BĞ, BG)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "kapsam": {
          "type": "string",
          "description": "secili (varsayılan), gorunum ya da proje: nesneler nereden alınır (metin)"
        },
        "pencere": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 2
            }
          ],
          "description": "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "katman": {
          "type": "string",
          "description": "Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman (metin)"
        },
        "kaynak": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Yazıların bağlanacağı nesne (çizgi ya da alan) — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "bag": {
          "type": "string",
          "enum": [
            "kenar",
            "kose"
          ],
          "description": "Neye bağlanacağı: en yakın kenar ya da en yakın köşe (kenar / kose); varsayılan kenar (metin)"
        },
        "tur": {
          "type": "string",
          "enum": [
            "sabit",
            "uzunluk"
          ],
          "description": "Yazının sözü: kendi yazısı kalır ya da kenarın uzunluğu olur (sabit / uzunluk); varsayılan sabit (metin)"
        },
        "birim": {
          "type": "string",
          "enum": [
            "metre",
            "santimetre",
            "milimetre",
            "kilometre"
          ],
          "description": "Uzunluğun birimi (tur=uzunluk) (metre / santimetre / milimetre / kilometre); varsayılan metre (metin)"
        },
        "ondalik": {
          "type": "integer",
          "minimum": 0,
          "maximum": 6,
          "description": "Virgülden sonraki basamak sayısı (tur=uzunluk); varsayılan 2 (tam sayı)"
        },
        "bicim": {
          "type": "string",
          "description": "Uzunluk yazısının kalıbı; {} sayının yerini tutar (tur=uzunluk) (metin)"
        },
        "ayrac": {
          "type": "string",
          "enum": [
            "virgul",
            "nokta"
          ],
          "description": "Ondalık ayracı (tur=uzunluk) (virgul / nokta); varsayılan virgul (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "islem.bagla",
      "cad.kentos/category": "Düzenleme",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "BAĞLA",
        "BAGLA",
        "ATTACH",
        "BĞ",
        "BG"
      ]
    }
  },
  {
    "name": "islem_kose_numarala",
    "title": "KÖŞENUMARALA",
    "description": "Kapsamdaki her alanın (ve çizginin) köşelerini seçilen köşeden başlayarak sırayla numaralar ve numarayı köşenin dışına yazar; numara köşesine bağlıdır, köşe taşınınca izler.\nKomut: KÖŞENUMARALA (KOSENUMARALA, NUMBERVERTICES, KNM)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "kapsam": {
          "type": "string",
          "description": "secili (varsayılan), gorunum ya da proje: nesneler nereden alınır (metin)"
        },
        "pencere": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 2
            }
          ],
          "description": "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "katman": {
          "type": "string",
          "description": "Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman (metin)"
        },
        "baslangic": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "object",
              "properties": {
                "taban": {
                  "type": "string",
                  "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                  "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                },
                "dogu": {
                  "type": "integer",
                  "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                },
                "kuzey": {
                  "type": "integer",
                  "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                }
              },
              "required": [
                "taban"
              ],
              "additionalProperties": false,
              "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
            }
          ],
          "description": "Sayımın başlayacağı köşeye en yakın nokta; verilmezse ilk köşe — nokta — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
        },
        "yon": {
          "type": "string",
          "enum": [
            "ters",
            "saat"
          ],
          "description": "Sayım yönü (ters / saat); varsayılan ters (metin)"
        },
        "onek": {
          "type": "string",
          "description": "Numaranın önüne gelen yazı (örnek: A, K-) (metin)"
        },
        "basamak": {
          "type": "integer",
          "minimum": 0,
          "maximum": 12,
          "description": "Numaranın en az basamak sayısı; eksikler dolgu ile tamamlanır; varsayılan 0 (tam sayı)"
        },
        "dolgu": {
          "type": "string",
          "description": "Basamak dolgusu; varsayılan 0 (metin)"
        },
        "ilk": {
          "type": "integer",
          "minimum": 0,
          "maximum": 1000000000,
          "description": "İlk köşenin numarası; varsayılan 1 (tam sayı)"
        },
        "sonek": {
          "type": "string",
          "description": "Numaranın arkasına gelen yazı (metin)"
        },
        "yukseklik": {
          "type": "integer",
          "minimum": 0,
          "maximum": 100000000,
          "description": "Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm; varsayılan 0 (tam sayı)"
        },
        "bosluk": {
          "type": "integer",
          "minimum": 0,
          "maximum": 100000000,
          "description": "Köşe ile yazı arası, milimetre; 0 = yüksekliğin yarısı; varsayılan 0 (tam sayı)"
        },
        "bagla": {
          "type": "boolean",
          "description": "Numarayı köşesine bağla: köşe taşınınca numara izler; varsayılan evet (evet/hayır)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "islem.kose_numarala",
      "cad.kentos/category": "İşlem",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "KÖŞENUMARALA",
        "KOSENUMARALA",
        "NUMBERVERTICES",
        "KNM"
      ]
    }
  },
  {
    "name": "islem_tampon",
    "title": "TAMPON",
    "description": "Kapsamdaki nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer: çizginin iki yanı, noktanın çevresi, alanın dışı; üst üste binen tamponlar tek alan olur.\nKomut: TAMPON (BUFFER, TMP)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "kapsam": {
          "type": "string",
          "description": "secili (varsayılan), gorunum ya da proje: nesneler nereden alınır (metin)"
        },
        "pencere": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 2
            }
          ],
          "description": "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "katman": {
          "type": "string",
          "description": "Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman (metin)"
        },
        "mesafe": {
          "type": "number",
          "description": "Tampon mesafesi, metre; eksi değer yalnız alanları içeri aşındırır (sayı)"
        },
        "birlestir": {
          "type": "boolean",
          "description": "Üst üste binen tamponları tek alanda birleştir; kapalıysa her nesnenin tamponu ayrı alan olur; varsayılan evet (evet/hayır)"
        },
        "kose": {
          "type": "string",
          "enum": [
            "yuvarlak",
            "koseli",
            "pah"
          ],
          "description": "Dış köşelerin biçimi (yuvarlak / koseli / pah); varsayılan yuvarlak (metin)"
        },
        "uc": {
          "type": "string",
          "enum": [
            "yuvarlak",
            "duz",
            "kare"
          ],
          "description": "Çizgi uçlarının biçimi (yuvarlak / duz / kare); varsayılan yuvarlak (metin)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "islem.tampon",
      "cad.kentos/category": "İşlem",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "TAMPON",
        "BUFFER",
        "TMP"
      ]
    }
  },
  {
    "name": "islem_uzunluk_yaz",
    "title": "UZUNLUKYAZ",
    "description": "Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel bir yazı olarak yazar; yazı kenara bağlıdır, kenar değişince izler ve yenilenir.\nKomut: UZUNLUKYAZ (UZUNLUKYAZ, LABELLENGTH, UZY)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uygulanacak nesnelerin kimlikleri; verilirse kapsam okunmaz — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "kapsam": {
          "type": "string",
          "description": "secili (varsayılan), gorunum ya da proje: nesneler nereden alınır (metin)"
        },
        "pencere": {
          "anyOf": [
            {
              "type": "string",
              "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
              "description": "nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
            },
            {
              "type": "array",
              "items": {
                "anyOf": [
                  {
                    "type": "string",
                    "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                    "description": "köşe — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                  },
                  {
                    "type": "object",
                    "properties": {
                      "taban": {
                        "type": "string",
                        "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
                        "description": "taban noktası — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
                      },
                      "dogu": {
                        "type": "integer",
                        "description": "tabandan doğuya (Sağa), milimetre; batı eksi"
                      },
                      "kuzey": {
                        "type": "integer",
                        "description": "tabandan kuzeye (Yukarı), milimetre; güney eksi"
                      }
                    },
                    "required": [
                      "taban"
                    ],
                    "additionalProperties": false,
                    "description": "Bir tutamaktan ölçüyle uzaklaşan nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} tabanın 10 m doğusudur."
                  }
                ],
                "description": "köşe — bir okuma aracının tutamağı ya da ondan ölçüyle uzaklaşan göreli nokta. Koordinat yazılamaz."
              },
              "maxItems": 2
            }
          ],
          "description": "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir — nokta listesi — bir okuma aracının tek tutamağı, ya da her elemanı bir tutamak ya da tutamaktan ölçüyle uzaklaşan göreli nokta olan dizi. Koordinat yazılamaz."
        },
        "katman": {
          "type": "string",
          "description": "Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman (metin)"
        },
        "birim": {
          "type": "string",
          "enum": [
            "metre",
            "santimetre",
            "milimetre",
            "kilometre"
          ],
          "description": "Uzunluğun yazılacağı birim (metre / santimetre / milimetre / kilometre); varsayılan metre (metin)"
        },
        "ondalik": {
          "type": "integer",
          "minimum": 0,
          "maximum": 6,
          "description": "Virgülden sonraki basamak sayısı; varsayılan 2 (tam sayı)"
        },
        "bicim": {
          "type": "string",
          "description": "Yazının kalıbı; {} sayının yerini tutar (örnek: \"{} m\", \"L={}\") (metin)"
        },
        "ayrac": {
          "type": "string",
          "enum": [
            "virgul",
            "nokta"
          ],
          "description": "Ondalık ayracı (virgul / nokta); varsayılan virgul (metin)"
        },
        "taraf": {
          "type": "string",
          "enum": [
            "otomatik",
            "sol",
            "sag",
            "dis",
            "ic"
          ],
          "description": "Yazının kenarın hangi yanına düşeceği (otomatik / sol / sag / dis / ic); varsayılan otomatik (metin)"
        },
        "yukseklik": {
          "type": "integer",
          "minimum": 0,
          "maximum": 100000000,
          "description": "Yazı yüksekliği, zemin milimetresi; 0 = plan ölçeğinde 2,5 mm; varsayılan 0 (tam sayı)"
        },
        "bosluk": {
          "type": "integer",
          "minimum": 0,
          "maximum": 100000000,
          "description": "Kenar ile yazı arası, milimetre; 0 = yüksekliğin yarısı; varsayılan 0 (tam sayı)"
        },
        "enaz": {
          "type": "integer",
          "minimum": 0,
          "maximum": 1000000000,
          "description": "Bundan kısa kenarlara yazı yazılmaz, milimetre; varsayılan 0 (tam sayı)"
        },
        "bagla": {
          "type": "boolean",
          "description": "Yazıyı kenarına bağla: kenar taşınınca yazı izler, uzunluk yeniden yazılır; varsayılan evet (evet/hayır)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": false,
      "destructiveHint": true,
      "idempotentHint": false,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "islem.uzunluk_yaz",
      "cad.kentos/category": "İşlem",
      "cad.kentos/approval": "user-required",
      "cad.kentos/names": [
        "UZUNLUKYAZ",
        "UZUNLUKYAZ",
        "LABELLENGTH",
        "UZY"
      ]
    }
  },
  {
    "name": "katmanlari_listele",
    "title": "Katmanları Listele",
    "description": "Katmanları, nesne sayılarını, görünürlük ve kilit durumlarını listeler.\nKomut: KATMANLAR (LAYERS, KTL)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {},
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.layers",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "KATMANLAR",
        "LAYERS",
        "KTL"
      ]
    }
  },
  {
    "name": "nesne_noktalari",
    "title": "Nesne Noktaları",
    "description": "Nesnelerin merkezini, köşelerini, uçlarını, kutusunu ya da kenar ortalarını bildirir; bir ajan bunları yeni çizimin taban noktası olarak kullanır.\nKomut: NESNENOKTALARI (OBJECTPOINTS, NNK)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Noktaları istenen nesneler — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "tur": {
          "type": "string",
          "enum": [
            "merkez",
            "koseler",
            "uclar",
            "kutu",
            "orta_noktalar"
          ],
          "description": "Hangi noktalar: merkez (alanın ağırlık merkezi, çizginin uzunluk ortası, dairenin merkezi), köşeler, uçlar, kutunun köşeleri ya da kenar ortaları; varsayılan merkez (metin)"
        }
      },
      "required": [
        "nesneler"
      ],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.object_points",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "NESNENOKTALARI",
        "OBJECTPOINTS",
        "NNK"
      ]
    }
  },
  {
    "name": "oznitelik_semasi",
    "title": "Öznitelik Şeması",
    "description": "Çizimde tanımlı öznitelik sütunlarını ve tiplerini listeler.\nKomut: ÖZNİTELİKŞEMASI (OZNITELIKSEMASI, ATTRSCHEMA, ÖŞ)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {},
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.attr_schema",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "ÖZNİTELİKŞEMASI",
        "OZNITELIKSEMASI",
        "ATTRSCHEMA",
        "ÖŞ"
      ]
    }
  },
  {
    "name": "secimi_al",
    "title": "Seçim Bilgisi",
    "description": "Kullanıcının o anki seçimini bildirir: kaç nesne ve hangi anahtarlar.\nKomut: SEÇİMBİLGİSİ (SECIMBILGISI, SELECTIONINFO, SÇB)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {},
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.selection_info",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "SEÇİMBİLGİSİ",
        "SECIMBILGISI",
        "SELECTIONINFO",
        "SÇB"
      ]
    }
  },
  {
    "name": "sorgula",
    "title": "Sorgula",
    "description": "Katman ve öznitelik koşuluna uyan nesneleri sayar ve anahtarlarını bildirir.\nKomut: SORGULA (QUERY, SRG)\nBu araç hiçbir şeyi değiştirmez; doğrudan çalışır ve sonucunu döndürür.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "katman": {
          "type": "string",
          "description": "Hangi katmanda aranacağı; verilmezse bütün çizim (metin)"
        },
        "alan": {
          "type": "string",
          "description": "Öznitelik sütunu; verilirse o sütunu taşıyan nesneler (metin)"
        },
        "deger": {
          "type": "string",
          "description": "Sütunun eşit olması istenen değer; yalnız 'alan' ile birlikte (metin)"
        },
        "sinir": {
          "type": "integer",
          "minimum": 1,
          "maximum": 1000,
          "description": "En çok kaç nesne bildirileceği; varsayılan 200 (tam sayı)"
        }
      },
      "required": [],
      "additionalProperties": false
    },
    "annotations": {
      "readOnlyHint": true,
      "destructiveHint": false,
      "idempotentHint": true,
      "openWorldHint": false
    },
    "_meta": {
      "cad.kentos/commandId": "core.query",
      "cad.kentos/category": "Sorgu",
      "cad.kentos/approval": "none",
      "cad.kentos/names": [
        "SORGULA",
        "QUERY",
        "SRG"
      ]
    }
  }
]
```
