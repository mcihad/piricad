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
| [`core.edittext`](edittext.md) | `YAZIDÜZENLE`, `YAZIDUZENLE`, `EDITTEXT`, `YZD` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Var olan bir yazının metnini, yüksekliğini ya da hizalamasını değiştirir. |
| [`core.exportstyle`](exportstyle.md) | `STİLAKTAR`, `STILAKTAR`, `EXPORTSTYLE`, `STAKTAR` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar. |
| [`core.area`](area.md) | `ALAN`, `AREA`, `POLİGON`, `POLIGON`, `AL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapalı bir alan çizer; istenirse içine delik açar. |
| [`core.rectangle`](rectangle.md) | `DİKDÖRTGEN`, `DIKDORTGEN`, `RECTANGLE`, `DKD`, `REC` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Karşılıklı iki köşeden dört köşeli kapalı bir alan çizer. |
| [`core.circle_draw`](circle_draw.md) | `DAİRE`, `DAIRE`, `CIRCLE`, `DR` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez ve çember üzerindeki bir noktadan daire çizer. |
| [`core.arc_draw`](arc_draw.md) | `YAY`, `ARC`, `YY` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez ve iki uçtan yay çizer; süpürme saat yönünün tersinedir. |
| [`core.vertex_move`](vertex_move.md) | `KÖŞETAŞI`, `KOSETASI`, `MOVEVERTEX`, `KT` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir nesnenin köşesini ya da tutamağını yeni bir yere taşır. |
| [`core.vertex_insert`](vertex_insert.md) | `KÖŞEEKLE`, `KOSEEKLE`, `ADDVERTEX`, `KE` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir kenarın ortasına yeni köşe ekler. |
| [`core.to_area`](to_area.md) | `ALANAÇEVİR`, `ALANACEVIR`, `TOAREA`, `ALÇ` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Uç uca değen çizgileri tek bir kapalı alana çevirir. |
| [`core.move`](move.md) | `TAŞI`, `TASI`, `MOVE`, `TŞ` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri iki nokta arasındaki kadar taşır. |
| [`core.copy`](copy.md) | `KOPYALA`, `COPY`, `KP` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesnelerin kopyasını verilen her noktaya, başlangıçtan o noktaya kadar öteleyerek koyar. |
| [`core.array`](array.md) | `DİZİ`, `DIZI`, `ARRAY`, `DZ` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesneleri satır/sütun ya da bir merkez etrafında çoğaltır. |
| [`core.combine`](combine.md) | `BİRLEŞTİR`, `BIRLESTIR`, `COMBINE`, `BRL` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek çizgi yapar. |
| [`core.split`](split.md) | `BÖL`, `BOL`, `SPLIT`, `BL` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Nesneleri çizilen bir kesme çizgisiyle böler. |
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
| [`core.measure_area`](measure_area.md) | `ALANÖLÇ`, `ALANOLC`, `AREAOF`, `AÖ` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Seçilen nesnelerin alanını ve çevresini yazar. |
| [`core.coordinate`](coordinate.md) | `KOORDİNAT`, `KOORDINAT`, `COORDINATE`, `KRD` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar. |
| [`core.pan`](pan.md) | `KAYDIR`, `PAN`, `KY` | Görünüm | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, şeffaf, salt okunur | Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır. |
| [`core.offset`](offset.md) | `OFSET`, `OFFSET`, `OF` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçili nesnelerin verilen mesafede paralelini çizer. |
| [`core.sector`](sector.md) | `DİLİM`, `DILIM`, `SECTOR`, `DL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir. |
| [`core.annulus`](annulus.md) | `HALKA`, `ANNULUS`, `HLK` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez, iç ve dış yarıçaptan delikli halka çizer. |
| [`core.ellipse_draw`](ellipse_draw.md) | `ELİPS`, `ELIPS`, `ELLIPSE`, `EL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir. |
| [`core.spline`](spline.md) | `SPLINE`, `SPLINE`, `SPLINE`, `SPL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kontrol noktalarından NURBS eğrisi (spline) çizer. |
| [`core.hatch`](hatch.md) | `TARAMA`, `TARAMA`, `HATCH`, `TRM` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapalı nesnelerin ya da verilen köşelerin içini katalogdaki bir desenle tarar. |
| [`core.block`](block.md) | `BLOK`, `BLOK`, `BLOCK`, `BLK` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar. |
| [`core.insert`](insert.md) | `BLOKEKLE`, `BLOKEKLE`, `INSERT`, `BE` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir. |
| [`core.dimension`](dimension.md) | `ÖLÇÜ`, `OLCU`, `DIMENSION`, `ÖÇ` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | İki nokta arasını, bir yarıçapı, çapı ya da açıyı ölçüp yazısı ve oklarıyla çizer. |
| [`core.leader`](leader.md) | `LİDER`, `LIDER`, `LEADER`, `LD` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir noktayı gösteren oklu çizgi çizer, istenirse yanına yazı koyar. |
| [`core.points`](points.md) | `NOKTALAR`, `POINTS`, `NKL` | Dosya | tek işlem | betiklenebilir, AI erişimli | Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod). |
| [`core.guide`](guide.md) | `KILAVUZ`, `GUIDE`, `KLV` | Çizim | tek işlem | betiklenebilir, AI erişimli | Cetvel kılavuzu ekler, listeler ve siler. |
| [`core.attribute`](attribute.md) | `ÖZNİTELİK`, `OZNITELIK`, `ATTRIBUTE`, `ÖZN`, `OZN` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar. |
| [`core.column`](column.md) | `SÜTUN`, `SUTUN`, `COLUMN`, `STN` | Düzenleme | geri alınmaz | betiklenebilir | Öznitelik sütunu tanımlar, düzenler, siler; argümansız çağrılınca listeler. |
| [`core.erase`](erase.md) | `SİL`, `SIL`, `ERASE`, `E` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Seçilen nesneleri siler. |
| [`core.select`](select.md) | `SEÇ`, `SEC`, `SELECT`, `S` | Düzenleme | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Nesneleri seçer: tümü, kimlikle, pencere, kesen kutu veya tek nokta. |
| [`core.label`](label.md) | `ETİKET`, `ETIKET`, `LABEL`, `ETK` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Katmandaki nesneleri özniteliklerinden okuyarak etiketler. |
| [`core.layer`](layer.md) | `KATMAN`, `LAYER`, `KAT` | Katman | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Katman oluşturur, aktif yapar ve özelliklerini değiştirir. |
| [`core.layer_visibility`](layer_visibility.md) | `KATMANGÖRÜNÜM`, `KATMANGORUNUM`, `LAYERVIEW`, `KGÖ`, `KGO` | Katman | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Katmanların görünürlüğünü toptan değiştirir: bir katmanı gösterir ya da gizler, yalnız onu bırakır, hepsini gösterir veya görünürlüğü ters çevirir. |
| [`core.layout`](layout.md) | `ÇIKTIYERLEŞİMİ`, `CIKTIYERLESIMI`, `LAYOUT`, `ÇYR`, `CYR` | Dosya | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Çizimin çıktı yerleşimlerini yönetir: yeni yerleşim açar, siler, adlandırır ve kâğıdını değiştirir. Yerleşim çizimle birlikte kaydedilir ve geri alınabilir. |
| [`core.layout_item`](layout_item.md) | `ÇIKTIÖĞE`, `CIKTIOGE`, `LAYOUTITEM`, `ÇÖĞ`, `COG` | Dosya | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir çıktı yerleşiminin üzerindeki öğeleri yönetir: harita çerçevesi, başlık, ölçek çubuğu, kuzey oku, lejant, resim, şekil ve tablo ekler, taşır, ayarlar ve siler. |
| [`core.layout_template`](layout_template.md) | `ÇIKTIŞABLON`, `CIKTISABLON`, `LAYOUTTEMPLATE`, `ÇŞB`, `CSB` | Dosya | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kurumun standart çıktı yerleşimlerini saklar ve uygular. Şablon çizimin dışında, kullanıcı profilinde durur; her çizime uygulanabilir. Şablon düzeni taşır, zemin koordinatlarını taşımaz. |
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
| [`core.print`](print.md) | `YAZDIR`, `PRINT`, `PLOT`, `YZDR` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | Çizimin bir penceresini bir yazdırma profilinin kâğıdına yerleştirip PDF dosyasına yazar ya da yazıcıya gönderir. |
| [`core.print_profile`](print_profile.md) | `YAZDIRMAPROFİLİ`, `YAZDIRMAPROFILI`, `PRINTPROFILE`, `YZP` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Yazdırma profillerini listeler, ekler, siler ya da birini varsayılan yapar; profil kâğıdı, yönü, çözünürlüğü ve kenar boşluğunu taşır. |
| [`core.setting`](setting.md) | `AYAR`, `SETTING`, `AY` | Sistem | tek işlem | betiklenebilir | Proje ayarlarını listeler, okur ve değiştirir. |
| [`core.preference`](preference.md) | `TERCİH`, `TERCIH`, `PREFERENCE`, `PREF` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Uygulama tercihlerini listeler, okur ve değiştirir. |
| [`core.mode`](mode.md) | `MOD`, `MODE`, `MD` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur ve değiştirir. |
| [`core.help`](help.md) | `YARDIM`, `HELP`, `?` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Komut listesini veya tek bir komutun ayrıntısını gösterir. |
| [`islem.alan_duzenle`](alan_duzenle.md) | `ALANDÜZENLE`, `ALANDUZENLE`, `ADJUSTAREA`, `ADZ` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapalı bir alanı istenen alana getirir: bütün kenarları eşit daraltıp genişleterek, bir kenarı kaydırarak ya da bir köşeyi çekerek; şekil bozulmaz. |
| [`islem.uzunluk_yaz`](uzunluk_yaz.md) | `UZUNLUKYAZ`, `UZUNLUKYAZ`, `LABELLENGTH`, `UZY` | İşlem | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel bir yazı olarak yazar; yazı kenara bağlıdır, kenar değişince izler ve yenilenir. |
| [`islem.kose_numarala`](kose_numarala.md) | `KÖŞENUMARALA`, `KOSENUMARALA`, `NUMBERVERTICES`, `KNM` | İşlem | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapsamdaki her alanın (ve çizginin) köşelerini seçilen köşeden başlayarak sırayla numaralar ve numarayı köşenin dışına yazar; numara köşesine bağlıdır, köşe taşınınca izler. |
| [`islem.bag_coz`](bag_coz.md) | `BAĞÇÖZ`, `BAGCOZ`, `DETACH`, `BÇ`, `BC` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapsamdaki yazıların bağını çözer: yazı yerinde kalır, bağlı olduğu nesne bundan sonra tek başına taşınır. |
| [`islem.bagla`](bagla.md) | `BAĞLA`, `BAGLA`, `ATTACH`, `BĞ`, `BG` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapsamdaki yazıları seçilen nesnenin en yakın kenarına ya da köşesine bağlar: nesne taşınınca yazı izler; istenirse yazı kenarın uzunluğu olur. |
| [`core.fit`](fit.md) | `OTURT`, `FIT`, `GEOREF`, `OTR` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Yerel ölçülmüş çizimi kontrol noktalarıyla haritaya oturtur (2B Helmert). |
| [`core.stakeout`](stakeout.md) | `APLİKASYON`, `APLIKASYON`, `STAKEOUT`, `APL` | Sorgu | geri alınmaz | etkileşimli, betiklenebilir, AI erişimli, salt okunur | İstasyondan her noktaya mesafe ve açı listesi çıkarır (aplikasyon). |
| [`core.reproject`](reproject.md) | `DÖNÜŞTÜR`, `DONUSTUR`, `REPROJECT`, `DNS` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Çizimin tamamını bir koordinat sisteminden diğerine dönüştürür. |
| [`core.merge`](merge.md) | `TEVHİT`, `TEVHIT`, `MERGE`, `TVH` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Komşu parselleri tek parselde birleştirir (tevhit). |
| [`core.split_parcel`](split_parcel.md) | `İFRAZ`, `IFRAZ`, `SUBDIVIDE`, `İFR` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir parseli düz bir ayırma çizgisiyle ikiye böler (ifraz). |
| [`core.split_area`](split_area.md) | `ALANİFRAZ`, `ALANIFRAZ`, `SPLITAREA`, `ALİF` | Düzenleme | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Parselden verilen yöne paralel, istenen alanda bir parça ayırır. |
| [`core.topology`](topology.md) | `TOPOLOJİ`, `TOPOLOJI`, `TOPOLOGY`, `TPL` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Kendini kesen sınır, sıfır alan ve örtüşen parselleri raporlar. |
| [`core.contour`](contour.md) | `EŞYÜKSELTİ`, `ESYUKSELTI`, `CONTOUR`, `EŞY` | Çizim | tek işlem | betiklenebilir, AI erişimli | Kotlu noktalardan eş yükselti eğrileri çizer. |
| [`core.earthwork`](earthwork.md) | `HACİM`, `HACIM`, `EARTHWORK`, `HCM` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Kotlu noktalardan bir kota göre kazı ve dolgu hacmini hesaplar. |
| [`core.layers`](layers.md) | `KATMANLAR`, `LAYERS`, `KTL` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Katmanları, nesne sayılarını, görünürlük ve kilit durumlarını listeler. |
| [`core.attr_schema`](attr_schema.md) | `ÖZNİTELİKŞEMASI`, `OZNITELIKSEMASI`, `ATTRSCHEMA`, `ÖŞ` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Çizimde tanımlı öznitelik sütunlarını ve tiplerini listeler. |
| [`core.query`](query.md) | `SORGULA`, `QUERY`, `SRG` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Katman ve öznitelik koşuluna uyan nesneleri sayar ve anahtarlarını bildirir. |
| [`core.selection_info`](selection_info.md) | `SEÇİMBİLGİSİ`, `SECIMBILGISI`, `SELECTIONINFO`, `SÇB` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Kullanıcının o anki seçimini bildirir: kaç nesne ve hangi anahtarlar. |
| [`core.view_info`](view_info.md) | `GÖRÜNÜMBİLGİSİ`, `GORUNUMBILGISI`, `VIEWINFO`, `GRB` | Sorgu | geri alınmaz | betiklenebilir, AI erişimli, salt okunur | Ekranda görünen alanın köşe koordinatlarını, merkezini, ölçeğini ve CRS'ini bildirir. |
| [`core.suggestion`](suggestion.md) | `ÖNERİ`, `ONERI`, `SUGGESTION`, `ÖN` | Sistem | komuta özel | etkileşimli, betiklenebilir | Bekleyen yapay zeka önerilerini listeler, durumunu söyler, uygular ya da reddeder. |
| [`core.mcp`](mcp.md) | `MCPSUNUCU`, `MCPSERVER`, `MCP` | Sistem | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Yapay zeka ajanlarının bağlanacağı MCP sunucusunu başlatır, durdurur, durumunu söyler ya da yeni bir erişim belirteci üretir. |
| [`core.ai_provider`](ai_provider.md) | `YAPAYZEKAMODELİ`, `YAPAYZEKAMODELI`, `AIMODEL`, `YZM` | Sistem | geri alınmaz | etkileşimli, betiklenebilir | Yapay zeka model sağlayıcılarını listeler, ekler, siler, birini varsayılan yapar ya da bağlantısını dener; profil adresi, lehçesi, modeli ve anahtar adını taşır. |

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

### `core.edittext` — YAZIDÜZENLE

Var olan bir yazının metnini, yüksekliğini ya da hizalamasını değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Düzenlenecek yazılar; verilmezse seçim |
| `yazi` | text | isteğe bağlı | Yeni metin; verilmezse değişmez |
| `yukseklik` | integer | isteğe bağlı | Yeni yükseklik, zeminde milimetre; verilmezse değişmez |
| `hizalama` | text | isteğe bağlı | sol, orta, sag veya merkez; verilmezse değişmez |

Ayrıntılı kullanım: [YAZIDÜZENLE](edittext.md)

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

Bir nesnenin köşesini ya da tutamağını yeni bir yere taşır.

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

Seçilen nesnelerin kopyasını verilen her noktaya, başlangıçtan o noktaya kadar öteleyerek koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Kopyalanacak nesnelerin kimlikleri; yoksa etkin seçim |
| `baslangic` | point | 1 | Kopyalamanın başlangıç noktası |
| `bitis` | point_list | en az 1 | Kopyaların geleceği noktalar; her nokta bir kopya |

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

Nesneleri çizilen bir kesme çizgisiyle böler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | selection | en az 0 | Kesilecek nesneler; yoksa etkin seçim |
| `noktalar` | point_list | 0–2 | Kesme çizgisinin iki noktası; arayüzde çizilir |
| `nokta` | point | isteğe bağlı | Bölme noktası (tek çizgi; eski biçim) |

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
| `baslangic` | number | isteğe bağlı | Kısmi elips: başlangıç açısı, derece, birinci eksenden saat yönünün tersine |
| `bitis` | number | isteğe bağlı | Kısmi elips: bitiş açısı, derece; baslangic ile birlikte |

Ayrıntılı kullanım: [ELİPS](ellipse_draw.md)

### `core.spline` — SPLINE

Kontrol noktalarından NURBS eğrisi (spline) çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 2 | Kontrol noktaları |
| `derece` | integer | isteğe bağlı | Eğrinin derecesi, 1–15; varsayılan 3 |
| `kapali` | bool | isteğe bağlı | Son noktadan ilkine kapansın mı; varsayılan hayır |

Ayrıntılı kullanım: [SPLINE](spline.md)

### `core.hatch` — TARAMA

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

### `core.block` — BLOK

Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | 1 | Bloğun adı; Türkçe katlanmış hâliyle benzersiz |
| `taban` | point | 1 | Taban noktası: referansların yerleştirildiği nokta |
| `nesneler` | selection | en az 0 | Bloğa girecek nesneler; yoksa etkin seçim |
| `aciklama` | text | isteğe bağlı | Serbest açıklama |

Ayrıntılı kullanım: [BLOK](block.md)

### `core.insert` — BLOKEKLE

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

### `core.dimension` — ÖLÇÜ

İki nokta arasını, bir yarıçapı, çapı ya da açıyı ölçüp yazısı ve oklarıyla çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `birinci` | point | 1 | Birinci nokta; açısal ölçüde birinci kolun ucu |
| `ikinci` | point | 1 | İkinci nokta; açısal ölçüde ikinci kolun ucu |
| `konum` | point | 1 | Ölçü çizgisinin yeri; açısal ölçüde yayın geçtiği nokta |
| `tur` | text | isteğe bağlı | hizali (varsayılan), dogrusal, yaricap, cap, acisal |
| `tepe` | point_list | isteğe bağlı | Açısal ölçünün tepe noktası |
| `stil` | text | isteğe bağlı | Katalogdaki ölçü stili: ISO-25 (varsayılan), STANDARD, MIMARI |
| `metin` | text | isteğe bağlı | Ölçülen değer yerine yazılacak metin |
| `katalog` | text | isteğe bağlı | Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri |

Ayrıntılı kullanım: [ÖLÇÜ](dimension.md)

### `core.leader` — LİDER

Bir noktayı gösteren oklu çizgi çizer, istenirse yanına yazı koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 2 | Okun ucundan yazının yanına köşeler |
| `metin` | text | isteğe bağlı | Son köşenin yanına yazılacak metin |
| `stil` | text | isteğe bağlı | Ok ve yazı boyunu veren ölçü stili; varsayılan ISO-25 |
| `katalog` | text | isteğe bağlı | Stil kataloğu dosyası; varsayılan TERCİH ölçü_stilleri |

Ayrıntılı kullanım: [LİDER](leader.md)

### `core.points` — NOKTALAR

Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Nokta listesi dosyasının yolu |
| `yon` | text | isteğe bağlı | oku (varsayılan) | yaz |
| `nesneler` | selection | en az 0 | yon=yaz ile: köşeleri yazılacak nesneler; verilmezse çizimdeki noktalar |
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
| `katman` | text | isteğe bağlı | KATMAN modunda katman adı |
| `islem` | text | isteğe bağlı | DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE |
| `tolerans` | number | isteğe bağlı | NOKTA modunda arama yarıçapı, metre; yoksa seçim toleransı |
| `sira` | number | isteğe bağlı | NOKTA modunda kaçıncı nesne: 1 en yakını, 2 altındaki |

Ayrıntılı kullanım: [SEÇ](select.md)

### `core.label` — ETİKET

Katmandaki nesneleri özniteliklerinden okuyarak etiketler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `katman` | text | 1 | Etiketlenecek katmanın adı |
| `bicim` | text | isteğe bağlı | Etiket biçimi; {sutun} o sütunun değeriyle değişir, \n satır kırar. Sembol alan bildiriyorsa gerekmez |
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

### `core.layer_visibility` — KATMANGÖRÜNÜM

Katmanların görünürlüğünü toptan değiştirir: bir katmanı gösterir ya da gizler, yalnız onu bırakır, hepsini gösterir veya görünürlüğü ters çevirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | goster, gizle, yalniz (yalnız bu katman), tumu (hepsini göster) ya da tersine |
| `katman` | text | isteğe bağlı | Katman adı; goster, gizle ve yalniz için gerekir, tersine için isteğe bağlı (verilmezse bütün katmanlar), tumu ile verilemez |

Ayrıntılı kullanım: [KATMANGÖRÜNÜM](layer_visibility.md)

### `core.layout` — ÇIKTIYERLEŞİMİ

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

Ayrıntılı kullanım: [ÇIKTIYERLEŞİMİ](layout.md)

### `core.layout_item` — ÇIKTIÖĞE

Bir çıktı yerleşiminin üzerindeki öğeleri yönetir: harita çerçevesi, başlık, ölçek çubuğu, kuzey oku, lejant, resim, şekil ve tablo ekler, taşır, ayarlar ve siler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | Ne yapılacağı |
| `yerlesim` | text | isteğe bağlı | Hangi çıktı yerleşimi; çizimde tek yerleşim varsa gerekmez |
| `ad` | text | isteğe bağlı | Öğe adı; ekle dışında gerekir, ekle'de verilmezse türetilir |
| `tur` | text | isteğe bağlı | islem=ekle için öğe türü |
| `x` | integer | isteğe bağlı | Sol kenardan uzaklık |
| `y` | integer | isteğe bağlı | ÜST kenardan uzaklık |
| `genislik` | integer | isteğe bağlı | Genişlik |
| `yukseklik` | integer | isteğe bağlı | Yükseklik |
| `metin` | text | isteğe bağlı | Metin öğesinin yazısı; <yerlesim>, <olcek>, <tarih>, <crs> yer tutucuları çizim anında çözülür |
| `yazi` | integer | isteğe bağlı | Yazı yüksekliği |
| `olcek` | integer | isteğe bağlı | Harita öğesinin ölçeği 1:N; 0 kapsama uyar |
| `pencere` | point_list | 0–2 | Harita çerçevesinin bakacağı alanın iki köşesi, anahtar iki kez yazılarak: pencere=x1,y1 pencere=x2,y2. Tuvalden çerçeve seçmek bu satırı yazar |
| `izgara` | text | isteğe bağlı | Harita öğesinin koordinat ızgarası |
| `izgara_aralik` | integer | isteğe bağlı | Izgara aralığı, zemin milimetresi; 0 ölçeğe göre seçilir |
| `kilit` | bool | isteğe bağlı | Öğeyi taşımaya kapatır |
| `cerceve` | bool | isteğe bağlı | Öğenin çevresine çerçeve çizer |
| `sayfa` | integer | isteğe bağlı | Öğenin duracağı sayfa (1'den başlar); tasi ile verilir |
| `yeni_ad` | text | isteğe bağlı | islem=ad için öğenin yeni adı |
| `katmanlar` | text | 0–64 | Harita çerçevesinin çizeceği katmanlar; anahtar birden çok kez yazılır. Verilmezse görünür bütün katmanlar, 'hepsi' listeyi boşaltır |
| `harita` | text | isteğe bağlı | Bu öğenin bağlı olduğu harita çerçevesinin adı. Verilmezse ilk harita. 'ilk' bağı kaldırır |
| `sira` | integer | isteğe bağlı | Çizim sırası; büyük olan üstte |

Ayrıntılı kullanım: [ÇIKTIÖĞE](layout_item.md)

### `core.layout_template` — ÇIKTIŞABLON

Kurumun standart çıktı yerleşimlerini saklar ve uygular. Şablon çizimin dışında, kullanıcı profilinde durur; her çizime uygulanabilir. Şablon düzeni taşır, zemin koordinatlarını taşımaz.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | Ne yapılacağı |
| `ad` | text | isteğe bağlı | Şablonun adı; listele dışında gerekir |
| `yerlesim` | text | isteğe bağlı | kaydet: hangi yerleşim saklanacak (tek yerleşim varsa gerekmez). uygula: kurulacak yerleşimin adı (verilmezse şablonun adı) |

Ayrıntılı kullanım: [ÇIKTIŞABLON](layout_template.md)

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
| `alan` | text | isteğe bağlı | Nesneden alınacak parametreler, virgülle: sütun[:özellik[:tür]] — 'kod:yazi:metin, kat:kalinlik'. Sütun yoksa tanımlanır |

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
| `katmanlar` | text | isteğe bağlı | Yalnızca bu katmanlar okunur, virgülle ayrılır; verilmezse tümü |
| `alanlar` | text | isteğe bağlı | Sütun olarak okunacak öznitelik alanları, virgülle; * hepsi; verilmezse alan okunmaz |

Ayrıntılı kullanım: [İÇEAKTAR](import.md)

### `core.export` — DIŞAAKTAR

Çizimi dış bir veri biçimine yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Yazılacak dosyanın yolu |
| `bicim` | text | isteğe bağlı | Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur |
| `surum` | integer | isteğe bağlı | DXF sürümü: 2000, 2004, 2007 (varsayılan), 2010, 2013, 2018 |

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

### `core.print` — YAZDIR

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

### `core.print_profile` — YAZDIRMAPROFİLİ

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

### `core.fit` — OTURT

Yerel ölçülmüş çizimi kontrol noktalarıyla haritaya oturtur (2B Helmert).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 4 | Kontrol çiftleri: yerel, harita, yerel, harita... |
| `olcek_kilitli` | bool | isteğe bağlı | Ölçeği 1'de tutar; saha ölçüsü yeniden ölçeklenmez |
| `sistem` | text | isteğe bağlı | Oturtulduktan sonraki koordinat sistemi, örnek TUREF/TM36 |

Ayrıntılı kullanım: [OTURT](fit.md)

### `core.stakeout` — APLİKASYON

İstasyondan her noktaya mesafe ve açı listesi çıkarır (aplikasyon).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `istasyon` | point | 1 | Aletin durduğu nokta |
| `baglama` | point_list | isteğe bağlı | Bağlama (arka görüş) noktası; verilirse açılar ondan ölçülür |
| `nesneler` | selection | en az 0 | Aplike edilecek noktalar; yoksa seçim, o da boşsa çizimdeki bütün noktalar |

Ayrıntılı kullanım: [APLİKASYON](stakeout.md)

### `core.reproject` — DÖNÜŞTÜR

Çizimin tamamını bir koordinat sisteminden diğerine dönüştürür.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `hedef` | text | 1 | Hedef koordinat sistemi, örnek EPSG:5256 ya da TUREF/TM36 |
| `kaynak` | text | isteğe bağlı | Kaynak sistem; yoksa çizimin kendi koordinat sistemi |

Ayrıntılı kullanım: [DÖNÜŞTÜR](reproject.md)

### `core.merge` — TEVHİT

Komşu parselleri tek parselde birleştirir (tevhit).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Birleştirilecek parseller; yoksa etkin seçim |

Ayrıntılı kullanım: [TEVHİT](merge.md)

### `core.split_parcel` — İFRAZ

Bir parseli düz bir ayırma çizgisiyle ikiye böler (ifraz).

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | 0–2 | Ayırma çizgisinin iki ucu |
| `nesneler` | selection | en az 0 | Ayrılacak parsel; yoksa etkin seçim |

Ayrıntılı kullanım: [İFRAZ](split_parcel.md)

### `core.split_area` — ALANİFRAZ

Parselden verilen yöne paralel, istenen alanda bir parça ayırır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `yon` | point_list | 0–2 | Ayırma çizgisinin YÖNÜ: iki nokta (yol cephesi, mevcut sınır) |
| `nesneler` | selection | en az 0 | Ayrılacak parsel; yoksa etkin seçim |
| `alan` | integer | isteğe bağlı | Ayrılacak alan, mm² (400 m² = 400000000) |
| `tolerans` | integer | isteğe bağlı | Kabul toleransı, mm²; varsayılan 10000 (0,01 m²) |

Ayrıntılı kullanım: [ALANİFRAZ](split_area.md)

### `core.topology` — TOPOLOJİ

Kendini kesen sınır, sıfır alan ve örtüşen parselleri raporlar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Denetlenecek nesneler; yoksa seçim, o da boşsa bütün çizim |

Ayrıntılı kullanım: [TOPOLOJİ](topology.md)

### `core.contour` — EŞYÜKSELTİ

Kotlu noktalardan eş yükselti eğrileri çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `aralik` | integer | isteğe bağlı | Eş yükselti aralığı, milimetre; varsayılan 1000 (1 m) |
| `katman` | text | isteğe bağlı | Eğrilerin çizileceği katman; varsayılan ESYUKSELTI |

Ayrıntılı kullanım: [EŞYÜKSELTİ](contour.md)

### `core.earthwork` — HACİM

Kotlu noktalardan bir kota göre kazı ve dolgu hacmini hesaplar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `kot` | integer | 1 | Karşılaştırma kotu, milimetre (845 m = 845000) |

Ayrıntılı kullanım: [HACİM](earthwork.md)

### `core.layers` — KATMANLAR

Katmanları, nesne sayılarını, görünürlük ve kilit durumlarını listeler.

Parametre almaz.

Ayrıntılı kullanım: [KATMANLAR](layers.md)

### `core.attr_schema` — ÖZNİTELİKŞEMASI

Çizimde tanımlı öznitelik sütunlarını ve tiplerini listeler.

Parametre almaz.

Ayrıntılı kullanım: [ÖZNİTELİKŞEMASI](attr_schema.md)

### `core.query` — SORGULA

Katman ve öznitelik koşuluna uyan nesneleri sayar ve anahtarlarını bildirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `katman` | text | isteğe bağlı | Hangi katmanda aranacağı; verilmezse bütün çizim |
| `alan` | text | isteğe bağlı | Öznitelik sütunu; verilirse o sütunu taşıyan nesneler |
| `deger` | text | isteğe bağlı | Sütunun eşit olması istenen değer; yalnız 'alan' ile birlikte |
| `sinir` | integer | isteğe bağlı | En çok kaç nesne bildirileceği; varsayılan 200 |

Ayrıntılı kullanım: [SORGULA](query.md)

### `core.selection_info` — SEÇİMBİLGİSİ

Kullanıcının o anki seçimini bildirir: kaç nesne ve hangi anahtarlar.

Parametre almaz.

Ayrıntılı kullanım: [SEÇİMBİLGİSİ](selection_info.md)

### `core.view_info` — GÖRÜNÜMBİLGİSİ

Ekranda görünen alanın köşe koordinatlarını, merkezini, ölçeğini ve CRS'ini bildirir.

Parametre almaz.

Ayrıntılı kullanım: [GÖRÜNÜMBİLGİSİ](view_info.md)

### `core.suggestion` — ÖNERİ

Bekleyen yapay zeka önerilerini listeler, durumunu söyler, uygular ya da reddeder.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | Ne yapılacağı: uygula, reddet, durum ya da listele |
| `oneri` | text | isteğe bağlı | Öneri kimliği; uygula, reddet ve durum için gerekir |

Ayrıntılı kullanım: [ÖNERİ](suggestion.md)

### `core.mcp` — MCPSUNUCU

Yapay zeka ajanlarının bağlanacağı MCP sunucusunu başlatır, durdurur, durumunu söyler ya da yeni bir erişim belirteci üretir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | text | 1 | Ne yapılacağı: baslat, durdur, durum ya da belirtec (yeni belirteç üretir) |
| `port` | integer | isteğe bağlı | Yalnız bu başlatma için port; verilmezse ayardaki port |

Ayrıntılı kullanım: [MCPSUNUCU](mcp.md)

### `core.ai_provider` — YAPAYZEKAMODELİ

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
    "name": "core_annulus",
    "title": "HALKA",
    "description": "Merkez, iç ve dış yarıçaptan delikli halka çizer.\nKomut: HALKA (ANNULUS, HLK)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "merkez": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Halkanın merkezi — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "ic": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "İç çember üzerinde bir nokta — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "dis": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Dış çember üzerinde bir nokta — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "YAY",
    "description": "Merkez ve iki uçtan yay çizer; süpürme saat yönünün tersinedir.\nKomut: YAY (ARC, YY)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "merkez": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Yayın merkezi — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "baslangic": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Yayın başlangıç noktası; yarıçapı bu belirler — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "bitis": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Yayın bitiş yönü; süpürme saat yönünün tersinedir — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "ALAN",
    "description": "Kapalı bir alan çizer; istenirse içine delik açar.\nKomut: ALAN (AREA, POLİGON, POLIGON, AL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Alanın köşe noktaları; kapanış noktası tekrarlanmaz — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
        "POLİGON",
        "POLIGON",
        "AL"
      ]
    }
  },
  {
    "name": "core_array",
    "title": "DİZİ",
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Dizinin merkezi (kutupsal dizi) — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "ÖZNİTELİK",
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
    "title": "BLOK",
    "description": "Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar.\nKomut: BLOK (BLOK, BLOCK, BLK)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "ad": {
          "type": "string",
          "description": "Bloğun adı; Türkçe katlanmış hâliyle benzersiz (metin)"
        },
        "taban": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Taban noktası: referansların yerleştirildiği nokta — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "name": "core_chamfer",
    "title": "PAH",
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "İşlem yapılacak köşe — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "DAİRE",
    "description": "Merkez ve çember üzerindeki bir noktadan daire çizer.\nKomut: DAİRE (DAIRE, CIRCLE, DR)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "merkez": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Dairenin merkezi — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "cevre": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Çember üzerinde bir nokta; yarıçapı bu belirler — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        }
      },
      "required": [
        "merkez",
        "cevre"
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
    "name": "core_combine",
    "title": "BİRLEŞTİR",
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
    "name": "core_contour",
    "title": "EŞYÜKSELTİ",
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
    "title": "KOORDİNAT",
    "description": "Tıklanan noktanın sağa ve yukarı değerini belgenin koordinat sisteminde yazar.\nKomut: KOORDİNAT (KOORDINAT, COORDINATE, KRD)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nokta": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Okunacak nokta — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "KOPYALA",
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kopyalamanın başlangıç noktası — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "bitis": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kopyaların geleceği noktalar; her nokta bir kopya — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "name": "core_dimension",
    "title": "ÖLÇÜ",
    "description": "İki nokta arasını, bir yarıçapı, çapı ya da açıyı ölçüp yazısı ve oklarıyla çizer.\nKomut: ÖLÇÜ (OLCU, DIMENSION, ÖÇ)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "birinci": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Birinci nokta; açısal ölçüde birinci kolun ucu — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "ikinci": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "İkinci nokta; açısal ölçüde ikinci kolun ucu — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "konum": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ölçü çizgisinin yeri; açısal ölçüde yayın geçtiği nokta — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "tur": {
          "type": "string",
          "description": "hizali (varsayılan), dogrusal, yaricap, cap, acisal (metin)"
        },
        "tepe": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Açısal ölçünün tepe noktası — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "name": "core_earthwork",
    "title": "HACİM",
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
    "title": "YAZIDÜZENLE",
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
    "title": "ELİPS",
    "description": "Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir.\nKomut: ELİPS (ELIPS, ELLIPSE, EL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "merkez": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Elipsin merkezi — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "birinci": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Birinci eksenin ucu — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "ikinci": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "İkinci eksenin uzaklığı; eksene dik ölçülür — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
      "required": [
        "merkez",
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
    "name": "core_erase",
    "title": "SİL",
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
    "name": "core_extend",
    "title": "UZAT",
    "description": "Bir çizgiyi sınır çizgisine ulaşana kadar uzatır.\nKomut: UZAT (EXTEND, UZ)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uzatılacak çizginin kimliği; yoksa seçili iki çizgiden tıklanan — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "sinir": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Sınır çizgisinin kimliği; yoksa seçili iki çizgiden diğeri — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "nokta": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Uzatılacak ucun yakınında bir nokta — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "YUVARLA",
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "İşlem yapılacak köşe — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "OTURT",
    "description": "Yerel ölçülmüş çizimi kontrol noktalarıyla haritaya oturtur (2B Helmert).\nKomut: OTURT (FIT, GEOREF, OTR)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kontrol çiftleri: yerel, harita, yerel, harita... — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "KILAVUZ",
    "description": "Cetvel kılavuzu ekler, listeler ve siler.\nKomut: KILAVUZ (GUIDE, KLV)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "yon": {
          "type": "string",
          "description": "yatay | düşey; yoksa kılavuzlar listelenir (metin)"
        },
        "deger": {
          "type": "integer",
          "description": "Kılavuzun koordinatı, milimetre — yatayda yukarı, düşeyde sağa (tam sayı)"
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
    "title": "TARAMA",
    "description": "Kapalı nesnelerin ya da verilen köşelerin içini katalogdaki bir desenle tarar.\nKomut: TARAMA (TARAMA, HATCH, TRM)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Sınır köşeleri, nesne seçmek yerine; en az üç nokta — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "BLOKEKLE",
    "description": "Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir.\nKomut: BLOKEKLE (BLOKEKLE, INSERT, BE)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "ad": {
          "type": "string",
          "description": "Yerleştirilecek bloğun adı (metin)"
        },
        "nokta": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ekleme noktası — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "name": "core_label",
    "title": "ETİKET",
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
    "title": "KATMAN",
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
    "title": "KATMANGÖRÜNÜM",
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
    "title": "ÇIKTIYERLEŞİMİ",
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
            "denetle"
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
    "title": "ÇIKTIÖĞE",
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
            "tablo"
          ],
          "description": "islem=ekle için öğe türü (metin)"
        },
        "x": {
          "type": "integer",
          "minimum": -10000,
          "maximum": 10000,
          "description": "Sol kenardan uzaklık [kâğıt mm] (tam sayı)"
        },
        "y": {
          "type": "integer",
          "minimum": -10000,
          "maximum": 10000,
          "description": "ÜST kenardan uzaklık [kâğıt mm] (tam sayı)"
        },
        "genislik": {
          "type": "integer",
          "minimum": 0,
          "maximum": 10000,
          "description": "Genişlik [kâğıt mm] (tam sayı)"
        },
        "yukseklik": {
          "type": "integer",
          "minimum": 0,
          "maximum": 10000,
          "description": "Yükseklik [kâğıt mm] (tam sayı)"
        },
        "metin": {
          "type": "string",
          "description": "Metin öğesinin yazısı; <yerlesim>, <olcek>, <tarih>, <crs> yer tutucuları çizim anında çözülür (metin)"
        },
        "yazi": {
          "type": "integer",
          "minimum": 1,
          "maximum": 200,
          "description": "Yazı yüksekliği [kâğıt mm] (tam sayı)"
        },
        "olcek": {
          "type": "integer",
          "minimum": 0,
          "maximum": 100000000,
          "description": "Harita öğesinin ölçeği 1:N; 0 kapsama uyar (tam sayı)"
        },
        "pencere": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Harita çerçevesinin bakacağı alanın iki köşesi, anahtar iki kez yazılarak: pencere=x1,y1 pencere=x2,y2. Tuvalden çerçeve seçmek bu satırı yazar [ZEMİN koordinatı — kâğıt değil] — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "ÇIKTIŞABLON",
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
    "title": "LİDER",
    "description": "Bir noktayı gösteren oklu çizgi çizer, istenirse yanına yazı koyar.\nKomut: LİDER (LIDER, LEADER, LD)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Okun ucundan yazının yanına köşeler — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "name": "core_line",
    "title": "ÇİZGİ",
    "description": "İki veya daha fazla nokta arasında doğru parçaları çizer.\nKomut: ÇİZGİ (CIZGI, LINE, Ç, L)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ardışık doğru parçalarının köşe noktaları — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "STİLKOPYALA",
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kaynak nesnenin üzerinde bir nokta; yalnız kaynak verilmediğinde — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "ÖLÇ",
    "description": "İki nokta arasındaki mesafeyi, koordinat farkını ve açıyı yazar.\nKomut: ÖLÇ (OLC, MEASURE, MS)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "baslangic": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ölçümün ilk noktası — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "bitis": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ölçümün ikinci noktası — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "name": "core_measure_area",
    "title": "ALANÖLÇ",
    "description": "Seçilen nesnelerin alanını ve çevresini yazar.\nKomut: ALANÖLÇ (ALANOLC, AREAOF, AÖ)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesneler": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ölçülecek nesnelerin kimlikleri; yoksa etkin seçim — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "TEVHİT",
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
    "title": "AYNALA",
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ayna ekseninin ilk noktası — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "bitis": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ayna ekseninin ikinci noktası — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "TAŞI",
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Taşımanın başlangıç noktası — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "bitis": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Taşımanın bitiş noktası — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "OFSET",
    "description": "Seçili nesnelerin verilen mesafede paralelini çizer.\nKomut: OFSET (OFFSET, OF)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
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
          "description": "Ofset mesafesi, milimetre; eksi değer içeri (tam sayı)"
        },
        "kose": {
          "type": "string",
          "description": "KÖŞE | YUVARLAK | PAH — dış köşenin biçimi (metin)"
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
    "title": "KAYDIR",
    "description": "Görünümü, tutulan noktayı verilen noktaya getirecek biçimde kaydırır.\nKomut: KAYDIR (PAN, KY)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "baslangic": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kaydırmanın tutulacağı nokta — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "bitis": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "O noktanın taşınacağı yer — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "name": "core_point_draw",
    "title": "NOKTA",
    "description": "Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper.\nKomut: NOKTA (POINT, NK)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Yerleştirilecek noktalar — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "NOKTALAR",
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
    "name": "core_polyline",
    "title": "ÇOKLUÇİZGİ",
    "description": "Birden çok noktadan TEK bir çizgi nesnesi çizer.\nKomut: ÇOKLUÇİZGİ (COKLUCIZGI, POLYLINE, ÇÇ, PL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Çoklu çizginin köşe noktaları; hepsi tek nesne olur — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "YAZDIR",
    "description": "Çizimin bir penceresini bir yazdırma profilinin kâğıdına yerleştirip PDF dosyasına yazar ya da yazıcıya gönderir.\nKomut: YAZDIR (PRINT, PLOT, YZDR)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "pencere": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Yazdırılacak alanın iki köşesi; merkez verilmezse ve bu da verilmezse tıklatılır — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "merkez": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kâğıdın ortalanacağı nokta; pencere yerine kullanılır — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "DİKDÖRTGEN",
    "description": "Karşılıklı iki köşeden dört köşeli kapalı bir alan çizer.\nKomut: DİKDÖRTGEN (DIKDORTGEN, RECTANGLE, DKD, REC)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Karşılıklı iki köşe; kalan ikisi bunlardan türetilir — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "DÖNÜŞTÜR",
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
    "title": "DÖNDÜR",
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Döndürme merkezi — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "aci": {
          "type": "number",
          "description": "Dönme açısı, derece; artı yön saat yönünün tersi (sayı)"
        }
      },
      "required": [
        "merkez",
        "aci"
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
    "title": "ÖLÇEKLE",
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ölçekleme merkezi; bu nokta yerinde kalır — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "carpan": {
          "type": "number",
          "description": "Ölçek çarpanı; sıfırdan büyük (sayı)"
        }
      },
      "required": [
        "merkez",
        "carpan"
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
    "title": "DİLİM",
    "description": "Merkez ve iki kenardan daire dilimi çizer; süpürme saat yönünün tersinedir.\nKomut: DİLİM (DILIM, SECTOR, DL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "merkez": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Dilimin merkezi — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "baslangic": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "İlk kenarın ucu; yarıçapı bu belirler — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "bitis": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "İkinci kenarın yönü; süpürme saat yönünün tersinedir — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "KATMANAT",
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
    "title": "SPLINE",
    "description": "Kontrol noktalarından NURBS eğrisi (spline) çizer.\nKomut: SPLINE (SPLINE, SPLINE, SPL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kontrol noktaları — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "BÖL",
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kesme çizgisinin iki noktası; arayüzde çizilir — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "nokta": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Bölme noktası (tek çizgi; eski biçim) — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "ALANİFRAZ",
    "description": "Parselden verilen yöne paralel, istenen alanda bir parça ayırır.\nKomut: ALANİFRAZ (ALANIFRAZ, SPLITAREA, ALİF)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "yon": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ayırma çizgisinin YÖNÜ: iki nokta (yol cephesi, mevcut sınır) — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "İFRAZ",
    "description": "Bir parseli düz bir ayırma çizgisiyle ikiye böler (ifraz).\nKomut: İFRAZ (IFRAZ, SUBDIVIDE, İFR)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Ayırma çizgisinin iki ucu — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "APLİKASYON",
    "description": "İstasyondan her noktaya mesafe ve açı listesi çıkarır (aplikasyon).\nKomut: APLİKASYON (APLIKASYON, STAKEOUT, APL)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "istasyon": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Aletin durduğu nokta — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "baglama": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Bağlama (arka görüş) noktası; verilirse açılar ondan ölçülür — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "name": "core_style",
    "title": "STİL",
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
    "name": "core_symbol",
    "title": "SEMBOL",
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
    "title": "METİN",
    "description": "Çizime metin yazar; yükseklik ve hizalama verilebilir.\nKomut: METİN (METIN, YAZI, TEXT, MT)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "noktalar": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Yazının başlangıç noktası — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Taban çizgisinin bitişi; yoksa yatay — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "ALANAÇEVİR",
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
    "name": "core_topology",
    "title": "TOPOLOJİ",
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
    "name": "core_trim",
    "title": "BUDA",
    "description": "Bir çizgiyi kestiği sınır çizgisine kadar budar.\nKomut: BUDA (TRIM, BD)\nBu araç ÇAĞRILDIĞINDA HİÇBİR ŞEY UYGULAMAZ: bir öneri kaydı açar, komut satırlarını geri döndürür ve bilgisayar başındaki mühendis uygulayana kadar bekler.",
    "inputSchema": {
      "type": "object",
      "properties": {
        "nesne": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Budanacak çizginin kimliği; yoksa seçili iki çizgiden tıklanan — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "sinir": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Sınır çizgisinin kimliği; yoksa seçili iki çizgiden diğeri — nesne seçimi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "nokta": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Atılacak parçanın üzerindeki bir nokta — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "KÖŞEEKLE",
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
        "nokta": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Yeni köşenin yeri — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "KÖŞETAŞI",
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
        "nokta": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Köşenin yeni yeri — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "YAKINLAŞ",
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
    "name": "gorunum_bilgisi",
    "title": "GÖRÜNÜMBİLGİSİ",
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Kenarın ya da köşenin gideceği yer; verilmezse arayüz sürükletir, komut satırı hedefe tam oturtur — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
        },
        "katman": {
          "type": "string",
          "description": "Sonucun yazılacağı katman; yoksa oluşturulur, verilmezse etkin katman (metin)"
        },
        "baslangic": {
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "Sayımın başlayacağı köşeye en yakın nokta; verilmezse ilk köşe — nokta — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
          "type": "string",
          "pattern": "^@[0-9a-f]{16}(\\.[0-9]+)?$",
          "description": "gorunum kapsamı için görünümün iki köşesi; arayüz kendisi verir — nokta listesi — bir okuma aracının döndürdüğü tutamak (@0123456789abcdef.3). Koordinat yazılamaz: konum her zaman bir araç sonucundan gelir."
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
    "title": "KATMANLAR",
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
    "name": "oznitelik_semasi",
    "title": "ÖZNİTELİKŞEMASI",
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
    "title": "SEÇİMBİLGİSİ",
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
    "title": "SORGULA",
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
