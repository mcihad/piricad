# KentOSCad Kullanıcı Kılavuzu

Türkiye odaklı CBS + CAD harita yazılımının kullanıcı belgeleri. Harita mühendisi,
şehir plancısı ve kadastro teknisyeni için yazıldı.

## Nereden başlamalı

Daha önce KentOSCad kullanmadıysanız sırayla okuyun:

1. [Kurulum ve derleme](baslangic/kurulum.md) — programı çalışır hâle getirin
2. [İlk adımlar](baslangic/ilk-adimlar.md) — on dakikada ilk çiziminiz
3. [Arayüz](baslangic/arayuz.md) — pencerede ne nerede
4. [Bileşenler](baslangic/bilesenler.md) — düğmeler, girdiler ve seçim denetimleri; durumları ve klavyesi
5. [Stil tasarımcısı](baslangic/stil-tasarimcisi.md) — bir katmanın nasıl çizileceğini tasarlayın
6. [Yazdırma ve PDF](baslangic/yazdirma.md) — yazdırma alanını seçin, ölçeği verin, PDF alın
6. [Komut sistemi](komutlar/README.md) — KentOSCad'in çalışma mantığı

## Komutlar

| Sayfa | İçerik |
|---|---|
| [Komut sistemi](komutlar/README.md) | Komut nedir, istemciler neden eşittir, günlük ne işe yarar |
| [Komut satırı](komutlar/komut-satiri.md) | Koordinat girişi, ifadeler, geçmiş, kısaltmalar |
| [Komut referansı](komutlar/referans.md) | Bütün komutların üretilmiş tablosu |

Tek tek komutlar:

| Komut | Sayfa |
|---|---|
| `YENİ` | [Boş çizim başlatma](komutlar/new.md) |
| `AÇ` | [Proje dosyası açma](komutlar/open.md) |
| `KAYDET` | [Çizimi kaydetme](komutlar/save.md) |
| `FARKLIKAYDET` | [Yeni ada kaydetme](komutlar/saveas.md) |
| `İÇEAKTAR` | [Dış veri alma](komutlar/import.md) |
| `DIŞAAKTAR` | [Dış biçime yazma](komutlar/export.md) |
| `YAZDIR` | [Yazdırma ve PDF](komutlar/print.md) |
| `YAZDIRMAPROFİLİ` | [Yazdırma profilleri](komutlar/print_profile.md) |
| `ÇIKTIYERLEŞİMİ` | [Çıktı yerleşimi](komutlar/layout.md) |
| `ÇIKTIÖĞE` | [Çıktı yerleşimi öğeleri](komutlar/layout_item.md) |
| `ÇIKTIŞABLON` | [Çıktı yerleşimi şablonları](komutlar/layout_template.md) |
| `VERİTABANI` | [PostGIS ile çalışma](komutlar/database.md) |
| `ÇİZGİ` | [Çizgi çizme](komutlar/line.md) |
| `ÇOKLUÇİZGİ` | [Tek nesne olarak çoklu çizgi](komutlar/polyline.md) |
| `NOKTA` | [Ölçülmüş nokta](komutlar/point_draw.md) |
| `DİKAYAK` | [Dik ayak / dik boy ile nokta](komutlar/perp_offset.md) |
| `ALIM` | [Açı ve kenarla nokta](komutlar/survey_polar.md) |
| `KESİŞİMNOKTA` | [Kesişimden nokta](komutlar/intersect_point.md) |
| `ARANOKTA` | [Doğru üzerinde ara nokta](komutlar/point_along.md) |
| `POLİGON` | [Poligon hesabı ve kapanma](komutlar/traverse.md) |
| `ÇOKGEN` | [Düzgün çokgen](komutlar/polygon_regular.md) |
| `KIR` | [Parça çıkar](komutlar/break.md) |
| `UÇUCA` | [Uç uca ekle](komutlar/join.md) |
| `UZUNLUK` | [Uzunluğu değiştir](komutlar/lengthen.md) |
| `PATLAT` | [Parçalarına ayır](komutlar/explode.md) |
| `HİZALA` | [Taşı, döndür, ölçekle](komutlar/align.md) |
| `BÖLÜMLE` | [Nesne boyunca işaret](komutlar/divide.md) |
| `ÇİZGİDÜZENLE` | [Çizgiyi düzenle](komutlar/pedit.md) |
| `PANOYAKOPYALA` | [Panoya al](komutlar/copy_clip.md) |
| `KES` | [Panoya al ve sil](komutlar/cut.md) |
| `YAPIŞTIR` | [Panodakini koy](komutlar/paste.md) |
| `ALAN` | [Kapalı alan çizme](komutlar/area.md) |
| `DİKDÖRTGEN` | [İki köşeden dörtgen ve kare çizme](komutlar/rectangle.md) |
| `DAİRE` | [Daire çizme](komutlar/circle_draw.md) |
| `YAY` | [Yay çizme](komutlar/arc_draw.md) |
| `METİN` | [Çizime yazı yazma](komutlar/text.md) |
| `YAZIDÜZENLE` | [Var olan yazıyı değiştirme](komutlar/edittext.md) |
| `BULDEĞİŞTİR` | [Yazılarda bulma ve toplu değiştirme](komutlar/find_replace.md) |
| `KÖŞETAŞI` | [Köşe taşıma](komutlar/vertex_move.md) |
| `ESNET` | [Pencere içindeki köşeleri taşıma](komutlar/stretch.md) |
| `KÖŞEEKLE` | [Kenara köşe ekleme](komutlar/vertex_insert.md) |
| `KÖŞESİL` | [Köşe silme](komutlar/vertex_delete.md) |
| `KENARTÜRÜ` | [Kenarı yaya ya da düze çevirme](komutlar/edge_kind.md) |
| `UZUNLUKYAZ` | [Kenar uzunluklarını yazma](komutlar/uzunluk_yaz.md) |
| `KÖŞENUMARALA` | [Köşe numaralama](komutlar/kose_numarala.md) |
| `BAĞLA` | [Yazıyı nesneye bağlama](komutlar/bagla.md) |
| `BAĞÇÖZ` | [Yazının bağını çözme](komutlar/bag_coz.md) |
| `ALANDÜZENLE` | [Alanı istenen değere getirme](komutlar/alan_duzenle.md) |
| `TAMPON` | [Tampon bölge çizme](komutlar/tampon.md) |
| `ALANÜRET` | [Çizgilerden alan üretme](komutlar/alan_uret.md) |
| `ALANAÇEVİR` | [Çizgileri kapalı alana çevirme](komutlar/to_area.md) |
| `SINIR` | [Kapalı bölgenin sınırını çıkarma](komutlar/boundary.md) |
| `TEMİZLE` | [Yinelenen ve boş nesneleri temizleme](komutlar/cleanup.md) |
| `TAŞI` | [Nesne taşıma](komutlar/move.md) |
| `KOPYALA` | [Nesne çoğaltma](komutlar/copy.md) |
| `DÖNDÜR` | [Nesne döndürme](komutlar/rotate.md) |
| `ÖLÇEKLE` | [Nesne ölçekleme](komutlar/scale.md) |
| `AYNALA` | [Nesne aynalama](komutlar/mirror.md) |
| `DİZİ` | [Nesne çoğaltma dizisi](komutlar/array.md) |
| `BÖL` | [Kesme çizgisiyle bölme](komutlar/split.md) |
| `BİRLEŞTİR` | [Alan ve çizgi birleştirme](komutlar/combine.md) |
| `BUDA` | [Parçayı kesme sınırlarına kadar budama](komutlar/trim.md) |
| `UZAT` | [Ucu sınıra kadar uzatma](komutlar/extend.md) |
| `PAH` | [Köşe pahı kırma](komutlar/chamfer.md) |
| `YUVARLA` | [Köşe yuvarlatma](komutlar/fillet.md) |
| `KATMANAT` | [Nesneyi başka katmana taşıma](komutlar/set_layer.md) |
| `STİLKOPYALA` | [Stili başka nesneye uygulama](komutlar/match_style.md) |
| `RENK` | [Nesnenin çizgi ve dolgu rengi](komutlar/colour.md) |
| `STİLAKTAR` | [Stili QGIS'e aktarma](komutlar/exportstyle.md) |
| `ÖZNİTELİK` | [Nesnelerin verisi](komutlar/attribute.md) |
| `SÜTUN` | [Öznitelik sütunu tanımlama](komutlar/column.md) |
| `SEÇ` | [Nesne seçme](komutlar/select.md) |
| `SİL` | [Nesne silme](komutlar/erase.md) |
| `KATMAN` | [Katman yönetimi](komutlar/layer.md) |
| `KATMANGÖRÜNÜM` | [Katman görünürlüğü](komutlar/layer_visibility.md) |
| `STİL` | [Nesne stili ve gösterim kataloğu](komutlar/style.md) |
| `SEMBOL` | [Gösterim rafı](komutlar/symbol.md) |
| `ETİKET` | [Özniteliklerden yazı](komutlar/label.md) |
| `ÖLÇ` | [Mesafe ölçme](komutlar/measure.md) |
| `ALANÖLÇ` | [Alan ve çevre ölçme](komutlar/measure_area.md) |
| `AÇIÖLÇ` | [Bir köşedeki açıyı ölçme](komutlar/measure_angle.md) |
| `NESNEBİLGİ` | [Nesnenin türü, katmanı, çevresi, alanı ve öznitelikleri](komutlar/entity_info.md) |
| `KOORDİNAT` | [Nokta koordinatı okuma](komutlar/coordinate.md) |
| `KAYDIR` | [Görünümü kaydırma](komutlar/pan.md) |
| `OFSET` | [Paralel çizme](komutlar/offset.md) |
| `NOKTALAR` | [Ölçü nokta listesi](komutlar/points.md) |
| `APLİKASYON` | [Aplikasyon listesi](komutlar/stakeout.md) |
| `EŞYÜKSELTİ` | [Eş yükselti eğrileri](komutlar/contour.md) |
| `HACİM` | [Kazı ve dolgu hesabı](komutlar/earthwork.md) |
| `DİLİM` | [Daire dilimi çizme](komutlar/sector.md) |
| `HALKA` | [Delikli halka çizme](komutlar/annulus.md) |
| `ELİPS` | [Elips çizme](komutlar/ellipse_draw.md) |
| `SPLINE` | [Spline çizme](komutlar/spline.md) |
| `TARAMA` | [Tarama çizme](komutlar/hatch.md) |
| `TARAMADÜZENLE` | [Çizilmiş taramayı düzenleme](komutlar/hatch_edit.md) |
| `BLOK` | [Blok tanımlama](komutlar/block.md) |
| `BLOKEKLE` | [Blok yerleştirme](komutlar/insert.md) |
| `BLOKDÜZENLE` | [Bloğu düzenleme](komutlar/block_edit.md) |
| `ÖLÇÜ` | [Ölçülendirme](komutlar/dimension.md) |
| `ÖLÇÜDÜZENLE` | [Çizilmiş ölçüyü düzenleme](komutlar/dimension_edit.md) |
| `ÖLÇÜYENİLE` | [Ölçüleri pafta ölçeğine uyarlama](komutlar/dimension_refresh.md) |
| `ZİNCİRÖLÇÜ` | [Zincir ölçü](komutlar/dimension_continue.md) |
| `BAZÖLÇÜ` | [Baz ölçü](komutlar/dimension_baseline.md) |
| `ÖLÇÜSTİLİ` | [Ölçü stilleri](komutlar/dimension_style.md) |
| `LİDER` | [Kılavuz çizgi](komutlar/leader.md) |
| `KILAVUZ` | [Cetvel kılavuzu](komutlar/guide.md) |
| `OTURT` | [Yerel çizimi haritaya oturtma](komutlar/fit.md) |
| `DÖNÜŞTÜR` | [Koordinat sistemi dönüşümü](komutlar/reproject.md) |
| `TEVHİT` | [Parsel birleştirme](komutlar/merge.md) |
| `İFRAZ` | [Parsel ayırma](komutlar/split_parcel.md) |
| `ALANİFRAZ` | [Alana göre parsel ayırma](komutlar/split_area.md) |
| `TOPOLOJİ` | [Geometri denetimi](komutlar/topology.md) |
| `YAKINLAŞ` | [Görünüm ayarlama](komutlar/zoom.md) |
| `GERİAL` | [Geri alma](komutlar/undo.md) |
| `YİNELE` | [Yineleme](komutlar/redo.md) |
| `BETİK` | [Betik çalıştırma](komutlar/script.md) |
| `PYTHON` | [Python parçacığı çalıştırma](komutlar/python.md) |
| `AYAR` | [Proje ayarları](komutlar/setting.md) |
| `TERCİH` | [Uygulama tercihleri](komutlar/preference.md) |
| `MOD` | [Oturum modları](komutlar/mode.md) |
| `İZ` | [Geçici izleme: iki köşenin izlerinin kesişimi](komutlar/tracking.md) |
| `YARDIM` | [Yardım](komutlar/help.md) |
| `BAĞLAM` | [Üzerinde çalışılanın özeti](komutlar/context.md) |
| `KATMANLAR` | [Katman dökümü](komutlar/layers.md) |
| `ÖZNİTELİKŞEMASI` | [Öznitelik sütunlarının dökümü](komutlar/attr_schema.md) |
| `SORGULA` | [Koşula uyan nesneleri sayma](komutlar/query.md) |
| `SEÇİMBİLGİSİ` | [O anki seçimi okuma](komutlar/selection_info.md) |
| `GÖRÜNÜMBİLGİSİ` | [Ekranda görünen alan](komutlar/view_info.md) |
| `NESNENOKTALARI` | [Bir nesnenin merkezi, köşeleri ve uçları](komutlar/object_points.md) |
| `ARAÇARA` | [Araç kataloğunda arama](komutlar/tool_search.md) |
| `İŞŞABLONU` | [İş şablonları](komutlar/job_template.md) |
| `ÖNERİ` | [Bekleyen yapay zeka önerileri](komutlar/suggestion.md) |
| `MCPSUNUCU` | [Ajan sunucusunu yönetme](komutlar/mcp.md) |
| `YAPAYZEKAMODELİ` | [Model sağlayıcıları](komutlar/ai_provider.md) |

## Nesne türleri

Çizimdeki her şey bir **nesne türüdür**: saklanan sayıları, çizilen biçimi, yakalama
noktaları ve ölçüleri türün kendisi söyler. Türlerin tablosu çekirdeğin kaydından
üretilir; her türün kendi sayfası vardır.

| Sayfa | İçerik |
|---|---|
| [Nesne türleri](nesneler/README.md) | Tür nedir, sayılar nasıl saklanır, tanınmayan tür ne olur |
| [Türler referansı](nesneler/referans.md) | Bütün türlerin üretilmiş tablosu |
| [Destek matrisi](nesneler/destek-matrisi.md) | Hangi düzenleme işlemi hangi türde çalışıyor — her hücre gerçekten çalıştırılarak ölçülür |
| [Çoklu çizgi ve alan](nesneler/coklucizgi.md) | Halkalar, parsel, delik, çok parça; alan ve çevre |
| [Daire](nesneler/daire.md) | Merkez ve yarıçap; 128-gen çizim; tam alan |
| [Yay](nesneler/yay.md) | Merkez, yarıçap, iki uç; saat yönünün tersine |
| [Nokta](nesneler/nokta.md) | Ölçülmüş tek nokta; düğüm yakalama |
| [Elips](nesneler/elips.md) | Merkez ve iki eksen ucu; eksen uçlarına yakalama; kısmi elips |
| [Yaylı çoklu çizgi](nesneler/yaylicizgi.md) | Kenarları yay olabilen çizgi ve alan; DXF şişkinliği; alan tam |
| [Spline](nesneler/spline.md) | Kontrol noktaları, derece, düğümler; de Boor ile çizim |
| [Tarama](nesneler/tarama.md) | Sınır halkaları ve desen; desen kataloğu |
| [Blok referansı](nesneler/blokreferansi.md) | Blok tanımını yerleştiren nesne; ölçek, açı, dizi |
| [Ölçü](nesneler/olcu.md) | Uzunluk, yarıçap, çap, açı; stil kataloğu; yazı tam sayıdan |
| [Kılavuz çizgi](nesneler/lider.md) | Oklu not çizgisi (DXF `LEADER`) |
| [Yazı](nesneler/yazi.md) | Taban çizgisi ve metin; çok satır, dokuz hizalama, satır aralığı; TEXT ve MTEXT |

## Yapay zeka

Yapay zeka bu programda geometri üretmez, **komut üretir**; ürettiği her şey bir
**öneridir** ve çizime ancak bilgisayar başındaki mühendisin onayıyla girer.

| Sayfa | İçerik |
|---|---|
| [Yapay zeka ve ajanlar](yapay-zeka/README.md) | Dört kural, hiçbir şeyi değiştirmeyen beş araç, sorumluluk |
| [MCP sunucusu](yapay-zeka/mcp-sunucusu.md) | Bir ajanı bağlama: adres, belirteç, protokol sürümü, hatalar |
| [Onay ve denetim](yapay-zeka/onay.md) | Öneri boru hattı, tutamaklar, tek işlem–tek Ctrl+Z, denetim kaydı |
| [Yapay Zeka paneli](yapay-zeka/sohbet.md) | Uygulama içi sohbet, öneri kartı, bağlam ölçeri |
| [Model sağlayıcıları](yapay-zeka/modeller.md) | Hangi model, hangi adres, hangi lehçe; anahtar nerede durur |
| [Lisans ve ağ yükümlülüğü](yapay-zeka/lisans.md) | Sunucu bileşeni neden AGPLv3, kimi bağlar |

## İleri konular

| Sayfa | İçerik |
|---|---|
| [İşlem araçları](islem/README.md) | Araçlar paneli; kapsam, asenkron çalışma ve Durdur, çıktı katmanı; araç listesi |
| [Bağlı nesneler](islem/bagli-nesneler.md) | Kaynağını izleyen yazılar: uzunluk ve köşe numarası nasıl taşınır, yenilenir, çözülür |
| [Betik yazma](betik/README.md) | JSON betik biçimi, toplu işlem, kum havuzu |
| [Python betikleri](betik/python.md) | Döngü, koşul ve hesapla betik yazma; `kentos.cad` API'si |
| [Python API referansı](python/referans.md) | Üretilmiş: her komutun Python imzası, İngilizce anahtar kelimeleri ve türleri |
| [Komut günlüğü](mimari/gunluk.md) | Yaptığınız işi geri izleme, makro, oturum kaydı |
| [Koordinat sistemleri](veri/koordinat-sistemleri.md) | TUREF/TM30, TM 3° dilimleri, milimetre depolama |
| [KentOSCad proje dosyası](veri/proje-dosyasi.md) | `.pcad` ne taşır, sürüm politikası, bozuk dosya |
| [Dış veri biçimleri](veri/dis-formatlar.md) | DXF ve GeoPackage, koordinat sistemi, `.prj` dosyası |
| [Dışa Aktar penceresi](baslangic/disa-aktarma.md) | Çizimi, bir nesnenin köşelerini ya da bir stili tek pencereden yazma |
| [Öznitelik tablosu](veri/oznitelik-tablosu.md) | Satırları süzme, düzenleme, alan istatistikleri; süzme ifadesinin dilbilgisi |
| [MPYY plan gösterimleri](veri/mpyy-gosterimleri.md) | Gösterim veri paketi: hangi RG sürümü, ne çıkarıldı, ne eksik, nasıl yeniden üretilir |
| [Sürüm ve uyumluluk politikası](api-stability.md) | Neyin sabit kaldığı, neyin değişebileceği |

## Başvuru

| Sayfa | İçerik |
|---|---|
| [Sözlük](sozluk.md) | Haritacılık, imar ve KentOSCad terimleri |
| [Sorun giderme](sorun-giderme.md) | Hata mesajları, sebepleri ve çözümleri |

## Bu kılavuz hakkında

Kullanıcı belgelerinin tamamı `/docs` altında, Markdown biçiminde ve Türkçedir. Bu
bir tercih değil, projenin anayasasında yazılı kati bir kuraldır: belgelenmemiş
özellik yayımlanmamış sayılır (`CLAUDE.md` Article 11). Bir komut kaydedilip
sayfası yazılmazsa derleme kırılır.

[Komut referansı](komutlar/referans.md) elle yazılmaz; komut kaydından üretilir
(`make reference`). Elle düzenlenirse CI kapısı fark eder.

Katkı kuralları ve mimari gerekçeler burada değil, depo kökündeki `CLAUDE.md` ile
`.claude/` altındadır; onlar İngilizcedir ve geliştiriciye hitap eder.
