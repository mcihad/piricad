# PiriCAD — CAD + GIS + AI geliştirme görevleri

> Araştırma ve kod incelemesi: **22 Eylül 2026**. Bu dosya önceki TODOS.md okunmadan silinerek sıfırdan yazılmıştır. Hedef; hassas CAD üretimi, GIS verisi ve analizi, haritacılık, arazi ve pafta işlerini aynı uygulamada, elle veya AI/MCP/Python üzerinden uçtan uca tamamlamaktır.

## 1. Ürün hedefi ve başarı tanımı

PiriCAD tam bir **CAD/GIS üretim ortamı** olacak. Bir parsel yalnız çizgi ve taramadan oluşmayacak: kalıcı kimliği, gerçek geometrisi, koordinat sistemi, öznitelikleri, komşulukları, ölçüleri, kaynağı ve üretildiği işlem birlikte yönetilecek. Aynı nesne CAD düzenlemesinde, GIS sorgusunda, tabloda, arazi hesabında ve paftada aynı anlamı taşıyacak.

Hedef, Netcad ekosistemindeki işleri daha kolay, hızlı ve denetlenebilir yapmak; QGIS düzeyindeki GIS ve kartografya gücünü hassas CAD düzenleme ile birleştirmek. AI sonradan eklenen bir sohbet kutusu olmayacak: kullanıcı “şu adayı düzenle, hataları düzelt, analizi yap ve paftaları hazırla” dediğinde uygulamanın gerçek araçlarını kullanarak sonuç üretecek. MCP istemcisi de aynı işleri yapabilecek.

“Dünyada ilk ve tek” ürün vizyonunu anlatan bir hedef olabilir; doğrulanmış pazar iddiası değildir. CAD/GIS birleşimi zaten vardır. Farklılaşma hedefimiz **aynı veri üzerinde hassas düzenleme + analiz + ilişkili çıktılar + kesintisiz AI otomasyonu** ve bunun ölçülebilir kullanım kalitesidir. Yapılan iş, işlem sayısı ve hata oranıyla kanıtlanacak; yalnız komut sayısıyla değil.

### Başlangıç ürün senaryosu

Kullanıcı ortofoto, ölçü noktaları, bir DXF ve parsel verisini açar; koordinatları doğrular; çizimi CAD araçlarıyla düzenler; çizdiğini yeniden dönüştürmeden GIS sorgusuna sokar; komşuluk hatalarını bulur; alanları ve etiketleri günceller; araziyle ilişkilendirir; atlas/rapor üretir; DXF, GeoPackage ve PDF teslim eder. Bu zincir hem elle hem tek bir AI göreviyle yürütülebilir; kaynak veri ve yapılan değişiklikler izlenebilir.

### Değişmez tasarım ilkeleri

- CAD nesnesi ile GIS özelliği arasında kimlik kaybettiren kopyalar üretme. Çizim, tablo, analiz ve çıktı aynı kimlik ve revizyona başvursun.
- Yerel düzenlenebilir belge, uzak veri sağlayıcısı ve görüntü referansının yetenekleri açık olsun. Bir WMS görüntüsüne vektör düzenlemesi vaat etme.
- Geometri işlemleri çekirdekte; değişiklikler mevcut Registry/Bus/transaction hattında kalsın. UI, AI, MCP ve Python ayrı geometri hesapları yazmasın.
- Önizleme ile uygulama aynı hesap sonucunu kullansın. Desteklenmeyen eğriyi sessizce kirişe veya çoklu doğruya dönüştürme.
- Mevcut Qt bağımsız çekirdek, sabit noktalı depolama, journal ve lisans sınırları korunsun. Yeni türler ve sağlayıcılar için gerekli model/format değişiklikleri açık tasarım kararı ve göç planıyla yapılsın.
- Uzun işler iptal edilebilir olsun. Geri alınabilir belge işi tek mantıksal işlem olarak geri alınsın; dış dosya/veritabanı etkilerinin geri alma sınırı açıkça gösterilsin.
- Parametreler birim, CRS, nesne türü, kapsam ve yan etki bilgisi taşısın. Ekrandan tıklama gerektiren işlemlerin sayısal/kimlik tabanlı karşılığı bulunsun.

## 2. Araştırma: neyi referans alıyoruz?

### Netcad ürün kapsamı

Aşağıdakiler üreticinin resmî ürün ve yardım sayfalarındaki yeteneklerdir; bu çalışma sırasında kurulu Netcad üzerinde performans veya dosya uyumluluk testi yapılmadı. Uzmanlık modülleri temel Netcad GIS lisansının özelliği gibi değerlendirilmemelidir.

| Referans | Araştırmadan çıkan kapsam | PiriCAD karşılığı |
|---|---|---|
| [Netcad GIS](https://www.netcad.com/tr/urunler/netcad-gis) | CAD/GIS çalışma ortamı; çizim ve düzenleme, katman/semboloji, farklı veri kaynakları, raster/nokta bulutu ve 2D/3D iş akışları | Günlük CAD ve GIS birlikte temel ürün; veri biçimi ve servis desteği ayrı ayrı doğrulanacak |
| [Paralel](https://wiki.netcad.com.tr/display/HELP/Paralel), [Uzat-Kes](https://wiki.netcad.com.tr/display/HELP/Uzat-Kes) | Paralelde mesafe, taraf ve köşe seçenekleri; sınırlarla toplu kesme/uzatma; bazı eğrilerde tür dönüşümü gereksinimi | Gerçek CAD paraleli, eğri farkındalığı, toplu düzenleme ve görünür yaklaşıklaştırma toleransı |
| [Netsurf](https://www.netcad.com/tr/urunler/netsurf) | Arazi modelleme, eş yükselti, kesit/profil ve hacim işleri | Kalıcı yüzey, kırık hat, sınır, profil ve iki yüzey arasında hesap |
| [Netmap](https://www.netcad.com/tr/urunler/netmap) | Parsel/kadastro, ifraz/tevhit, dağıtım ve raporlama iş akışları | Topolojiye bağlı parsel işlemleri, alan kontrolü, sürümlü kural paketleri |
| [Netpro](https://www.netcad.com/tr/urunler/netpro) | Güzergâh, yol, enkesit ve kazı/dolgu üretimi | Yatay/düşey güzergâh, parametrik kesit ve ilişkili mühendislik çıktıları |
| [Planet](https://www.netcad.com/tr/urunler/planet) | Planlama, standart gösterim, plan verisi ve rapor üretimi | Ölçek ve katalog temelli çizim, plan topolojisi, açıklanabilir kalite kontrolü |
| [VGA](https://www.netcad.com/tr/urunler/vga), [Analist](https://www.netcad.com/tr/urunler/analist) | Veritabanıyla ilişkili CAD/GIS düzenleme; mekânsal analiz iş akışları | Sağlayıcı yetenekleri, öznitelik yönetimi ve tekrar kullanılabilir analiz modelleri |
| [Netcad Akademi](https://akademi.netcad.com/) | Tekil çizimden projeksiyon, sayısallaştırma ve çıktıya uzanan eğitim akışları | Eğitim ve kabul testleri gerçek teslim işlerini takip edecek |

### Görsel inceleme notları

Resmî yardım sayfalarındaki aşağıdaki görseller tarayıcıda açılıp görsel olarak incelendi. Netcad 7/8.6 ekranları güncel sürümün görünümü olarak sunulmuyor; iş akışı referansıdır. Erişilemeyen bazı ürün sitesi medya dosyaları değerlendirmeye alınmadı. Tüm Netcad ekranlarının veya modüllerinin incelendiği iddia edilmiyor.

| Görsel ve kaynak | Görülen düzen | Tasarıma dönüşen görev |
|---|---|---|
| [Düzenle şeridi](https://wiki.netcad.com.tr/pages/viewpage.action?pageId=217385414), görsel başlığında Netcad 8.6 | Düzenleme ve dönüşüm araçları gruplu; kes/uzat, paralel, birleştir, böl ve köşe araçları aynı şeritte | U-01: arama, bağlama uygun araçlar ve sabit kısayollarla keşfedilebilirlik |
| [Paralel ayar ekranı](https://wiki.netcad.com.tr/display/HELP/Paralel) | Mesafe, özellik aktarımı, köşe/uç seçenekleri ve tek taraf kontrolü | C-03: çizimin yanında canlı önizleme; mesafe ve tarafı değiştirirken araçtan çıkmama |
| [Uzat/Kes örneği](https://wiki.netcad.com.tr/display/HELP/Uzat-Kes) | Sınır nesneleri ile değişecek bölüm farklı renklerde; daire ve çizgi örnekleri | C-04: tutulacak/silinecek bölümün açık önizlemesi ve aday değiştirme |
| [Obje özellikleri / Çoklu Doğru](https://wiki.netcad.com.tr/pages/viewpage.action?pageId=218041782), Netcad 7 yardım ağacı | Genel, görünüm ve nesne özellikleri; GIS anahtarı/sınıf, tabaka, merkez, nokta sayısı, alan/çevre | U-04 ve G-03: ortak özellik paneli, hesaplanan ve düzenlenebilir alanların ayrımı |
| [Sayısallaştırma Sihirbazı](https://wiki.netcad.com.tr/pages/viewpage.action?pageId=217909474), Netcad 7 | Yapılar, sınırlar, hidrografya, bitki örtüsü gibi sınıflar içeren kalem ağacı ve tanım menüsü | G-04: aranabilir kalem kitaplığı; geometri, öznitelik, gösterim ve doğrulamayı tek seçimle bağlama |

### QGIS ve MCP referansının sürümü

[QGIS 4.2 değişiklik kaydı](https://changelog.qgis.org/en/version/4.2/) 3 Temmuz 2026 çıkış tarihini veriyor. Yerleşim açısından katman gösteriminden grafik kategori/renk üretimi, şekille resim kırpma ve GeoPDF katman ağacının korunması somut referanslar. Bunlar önceki sürümlerden gelen atlas/rapor gibi temel yeteneklerle karıştırılmamalı. LTR takvimi ayrı bir konudur: [resmî takvim açıklaması](https://blog.qgis.org/2025/10/07/update-on-qgis-4-0-release-schedule-and-ltr-plans/) 4.2'nin LTR depolarına geçişini Ekim 2026 için planlıyor.

[QGIS Model Designer belgesi](https://docs.qgis.org/3.44/en/docs/user_manual/processing/modeler.html) girdi, algoritma ve bağımlılık zincirlerini tekrar kullanılabilir işlem haline getiriyor; bu bağlantı açıkça **3.44** belgesidir, 4.2'ye özel belge değildir. PiriCAD'deki modelleyici için davranış referansıdır.

[MCP 2026-07-28 açıklaması](https://blog.modelcontextprotocol.io/posts/2026-07-28/) ve [resmî araç şeması](https://github.com/modelcontextprotocol/modelcontextprotocol/blob/main/docs/specification/2026-07-28/server/tools.mdx) sürüm uyumu için referanstır. Uzun iş desteği [Tasks uzantısında](https://tasks.extensions.modelcontextprotocol.io/specification/draft/tasks) ayrıca tanımlanıyor; açılan belge draft işaretli olduğundan istemci uyumu ve sürüm sabitleme doğrulanmadan temel protokolün zorunlu parçası sayılmayacak.

## 3. Kodda bugün ne var, gerçek açık nerede?

İnceleme tabanı: `main`, başlangıç HEAD `fe4b8b2`; ayrıca çalışma ağacındaki güncel değişiklikler. Python API ve komut metadata çalışmalarının bir kısmı henüz commit edilmemişti. Bu belge onların tamamlandığını veya testlerinin geçtiğini varsaymaz. İnceleme statiktir; aşağıdaki açıklar uygulamada tekrar üretme ve kabul testleriyle kapatılacak.

**Güncelleme (23 Eylül 2026, F-01):** Aşağıdaki tablonun düzenleme satırları artık statik okuma değil **ölçüm**: [destek matrisi](docs/nesneler/destek-matrisi.md) 15 türün her birini kullanıcının yazacağı komutla oluşturup 17 işlemi aynı komut veri yolundan çalıştırıyor ve sonucu sınıflıyor; `scripts/ci-gate-kapsam.sh` onu her değişiklikte yeniden üretip karşılaştırıyor. Python API artık commit edilmiş durumda (`kentos.cad`, komut kaydından üretilen yüzey, konsol ve editör); gömülü Lua kaldırıldı.

Yerelde görünen diğer dal `claude/relaxed-franklin-b73a78`, `3af2fbb` noktasında ve `main`in atasıdır; ayrı, main'e gelmemiş layout değişikliği görülmedi. Mevcut dal referansları ve layout geçmişi incelendi; uzak depodaki tüm silinmiş/erişilmeyen dallar hakkında sonuç çıkarılmadı. Atlas, rapor, bağlı harita ve preflight geliştirmeleri main geçmişinde zaten bulunuyor.

| Alan | Kodda görülen temel | Yeni görevlerin hedefi |
|---|---|---|
| Geometri | `src/core/include/kentos_cad/core/` altında circle, arc, ellipse, arc_polyline, spline, dimension, hatch ve block_reference | Yeni isimler eklemekten çok her türün tüm düzenleme hattını tamamlamak |
| Kes/uzat/böl | **Ölçüldü:** kes/uzat/böl/kır/uç uca yalnız çizgi ve köşeli çoklu çizgide çalışıyor; yay, daire, elips, spline, yaylı çizgi, tarama ve liderde reddediyor. Delikli alanda da kes/böl/kır reddediyor | Yay, daire, elips ve spline için türüne uygun işlemler |
| Paralel | **Düzeltildi (23 Eylül 2026):** OFSET artık `core::entity_parallel` ile çiziyor: `(0,0)→(10,0)` çizgisinin sol 2 m paraleli açık `(0,2)→(10,2)` çizgisi, sağ/sol çizim yönüyle tutarlı; daire daire kalıp yarıçapı değişiyor; delikli alanın paralelinde delik korunuyor; yay yay kalıyor; elips/spline/yaylı çizgi çizildiği hâliyle çoklu çizgi olup sapmasını yazıyor; çöken sonuç nesne nesne söyleniyor. Taraf imleçle gösteriliyor (canlı önizleme), betikte `taraf`/`nokta`/işaretli mesafe; `kaynak=sil`, `ozellik=aktif`. Matris: bant satırları kalmadı. İki yanlı GIS tamponu ayrı araç: TAMPON (`islem.tampon`). Eksik: öznitelik aktarımı | CAD paraleli ile GIS tamponunu ayırmak; delik ve çok parçalı topolojiyi korumak |
| Köşe/birleştir | **Ölçüldü:** açık çizgide yuvarlatma gerçek bir `YAY` nesnesi bırakıyor. **Düzeltildi (23 Eylül 2026):** kapalı şeklin (dikdörtgen, alan) köşesi reddediliyordu; artık yerinde yuvarlatılıyor — aynı nesne, alan olarak kalıyor, yay YAY'ın çizim noktalarıyla sınıra işleniyor ve sapma (5 m yarıçapta 6 mm) söyleniyor; gerçek fare olaylarıyla `KENTOS_REALMOUSE_PROBE` beş köşe durumunu sınıyor. Yaylı çizgi, tarama ve delikli alanda köşe işlemleri reddediliyor. `combine.cpp` başındaki yorum "iki ayrık parça tek nesne olur" diyor, kod her parçayı ayrı nesne yapıyor — bu yüzden **hiçbir komut çok parçalı alan üretmiyor**. Yaylı çoklu çizgiyi de hiçbir komut üretmiyor; tek yolu DXF içe aktarma | Karma çizgi/yay zincirleri ve eğriye uygun köşe davranışı |
| Tutamaç/yakalama | `grips.hpp`, snap altyapısı ve yakın tarihli görsel/önizleme düzeltmeleri var | Karma geometri, iç içe blok, aday yönetimi ve gerçek kullanıcı akışlarında tutarlılık |
| Ölçü/tarama | Çok sayıda ölçü türü ve tarama modeli var; hatch `associative` alanı DXF round-trip amacıyla tutuluyor | Bir bayrağı canlı bağımlılık sanmadan, kaynak geometriyle gerçek ilişkilendirme |
| Öznitelik/GIS | Sütun temelli şema, decimal/date/code türleri, tablo/panel, CRS ve GDAL vektör hattı var | İlişki, doğrulama, büyük veri, mekânsal sorgu ve CAD düzenlemesiyle bütünlük |
| PostGIS | `src/io/include/kentos_cad/io/postgis.hpp`: katmanı uzamsal tabloya ve projeyi bütün olarak kaydetme | Artımlı düzenleme, çakışma, yetenek ve transaction davranışını ayrıca doğrulamak |
| Arazi | `domain/surface` eş yükselti ve referans düzleme kazı/dolgu hesaplıyor; üçgenleme geçici | Kalıcı/düzenlenebilir yüzey, kırık hat, iki yüzey ve ilişkili kesitler |
| Layout | `core/layout.hpp`, `commands/layout.cpp`, `app/layout_*`: bağlı harita, atlas, rapor, çok sayfa ve denetim temelleri | Özelliklerin gerçek çıktı doğruluğu ve QGIS düzeyindeki kalan kapsam |
| AI ayarları | `ai/policy.hpp`, `policy_path.cpp`, settings komutları: onay, soru ve üzerine yazma politikaları var | Ayarların sohbet/MCP/yeniden bağlanma boyunca gerçekten uygulanması; tekrar onay döngülerini bitirmek |
| MCP/Python | `ai/mcp.cpp`, kaynak/araç keşfi, job templates. Python API commit edildi: her komut `cad.<ad>(...)` olarak komut kaydından üretiliyor; `cad.Point`, `cad.Box`, `cad.viewport`; alt panelde konsol, tamamlama ve imza ipucu | Sadece komut listesi değil, bütün işi tamamlayan bağlam ve eşdeğer işlem kapsamı |
| Sessiz ret | **Düzeltildi (23 Eylül 2026).** Ölçüm, reddini transkripte yazıp veri yoluna **başarı** bildiren 38 hücre bulmuştu; aynı desen ölçülmeyen komutlarda da vardı. Artık ret `Context::refuse` ile hata olarak dönüyor: 47 dosyada 353 nokta; rapor, liste, boş sonuç ve iptal ayrı tutuldu (`command.md` R19a). Sessizlik üç gerçek hatayı saklıyordu: betikte çizimden sonra `GERİAL` betikten önceki işi geri dönmez biçimde siliyordu; toplu işte başarısız komut öncekilerin işini de geri sarıyordu (R13a); kılavuzda ~40 örnek hiç çalışmıyordu. Matris sessiz ret listesini boş gösteriyor ve `scripts/ci-gate-kapsam.sh` bir tane bile görürse kırmızı | Ret her zaman hata olarak dönsün (A-07: kısmi hata başarı diye sunulmaz; U-01: araç neden uygulanamadığını söyler) |
| AI çizimi | **Ölçüldü ve düzeltildi (23 Eylül 2026):** katalogda 86 yazma aracı vardı ama bir ajan hiçbirini bir işi bitirecek şekilde çağıramıyordu. Konum yalnız tutamakla verilebiliyor, hiçbir okuma aracı **nokta** tutamağı basmıyordu; sohbet ise tutamağın metnini argümana yazıyordu. Artık `gorunum_bilgisi` ekranın ortasını, `nesne_noktalari` nesnelerin merkez/köşe/uç noktalarını nokta tutamağı olarak veriyor; bir konum bir tutamaktan ölçüyle de söylenebiliyor (`{"taban": …, "dogu": …, "kuzey": …}`); sohbet ve MCP tek derleyiciyi paylaşıyor. MCP probe'u görünüm ortasına kare önerisini uçtan uca doğruluyor | Her yazma aracı etkileşimsiz geçerli parametrelerle bir iş bitirebilsin (A-01) |
| Araç sütunu | **Ölçüldü ve düzeltildi (23 Eylül 2026):** `KENTOS_TOOL_PROBE` artık sütundaki her aile üyesini ve menülerdeki her çizim/düzenleme/sorgu aracını iki kez (önce seç sonra bas, seçmeden bas) basıyor: 108 araç. Seçim yokken sormak yerine reddeden 17 araç vardı (SİL, KES, PAH, YUVARLA, KÖŞETAŞI, KÖŞEEKLE, TEVHİT, İFRAZ, HACİM, OTURT, DÖNÜŞTÜR …); hepsi soruyor, prob 0 kırık diyor. PAH, UZAT, KIR, köşe araçları, UÇUCA, PATLAT, BÖLÜMLE ve 11 araç daha yalnız menüdeydi; sütuna aileler hâlinde girdiler (sütun 20 düğme). `test_preview.cpp` her etkileşimli komutun ikinci ve sonraki nokta istemlerinde önizleme olduğunu sınıyor. **Renk kutuları bağlandı (23 Eylül 2026):** seçili nesnenin ya da etkin katmanın renklerini gösteriyor, tıklayınca renk menüsü açılıyor ve seçim `RENK` ile uygulanıyor | Her düğme ve menü satırı bir işi bitirebilsin; nesne ya da değer eksikse sorsun (U-01) |
| Ölçüm araçları | **Ölçüldü ve düzeltildi (23 Eylül 2026):** ÖLÇ iki noktada duruyor, ALANÖLÇ yalnız nesne ölçüyor (açık çizgiye "0,00 m²"), dört ölçüm komutu da sonucu yalnız geçmiş paneline yazıyordu; bitirmek için Enter salt okunur ölçümde "İptal edildi" diyordu. Artık ÖLÇ noktadan noktaya kenar ve toplam veriyor, ALANÖLÇ köşelerden ölçüyor, sonuçlar tuvalde kalıyor (`command/measure_mark.hpp`, `Bus::on_measure_mark`); arayüz, komut satırı ve betik aynı cevabı ve aynı işareti veriyor (`test_proof.cpp`) | Ölçüm sonucu görülebilir, tekrar edilebilir ve kaydedilebilir olsun (U-02) |
| Raster/servis | Raster sembol çizimi görülüyor; bu, coğrafi raster sağlayıcısı veya WMS desteğinin kanıtı değildir | G-08/G-09 için ayrı veri sağlayıcısı envanteri ve çalışan senaryo |

## 4. Öncelik ve tamamlanma sözleşmesi

- **P0:** Günlük CAD/GIS üretiminde doğruluk, ortak veri modeli ve kesintisiz temel kullanım. İlk teslim kapısı.
- **P1:** Profesyonel proje üretimi, birlikte çalışabilirlik, analiz, pafta ve AI ile tam iş akışları.
- **P2:** İleri parametrik tasarım, uzmanlık modülleri ve geniş ölçekli mühendislik. Ortak çekirdeğin üzerine kurulur.

Kutular yeni geliştirme/doğrulama işlerini gösterir; kodda bir sınıfın varlığı bitmiş ürün sayılmaz. Her iş sahibi, ilgili kabul senaryosunun çıktısını, komut/API kapsamını ve bilinen sınırlarını teslim eder. Desteklenmeyen kombinasyon görünür ve tutarlı hata vermelidir; sessiz veri kaybı kabul edilmez.

**Ortak tamamlanma ölçütü:** İlgili işlem UI ve uygulanabilir otomasyon yüzeylerinde aynı geometri/öznitelik sonucunu verir; önizleme, iptal, undo/redo, kaydet/aç, hata ve büyük koordinat davranışı doğrulanır. Yeni kalıcı veri format göçüyle gelir. Uygun olmayan kombinasyonlar matriste “uygulanamaz” gerekçesiyle işaretlenir. Ekran görüntüsü tek başına geometrik doğruluk kanıtı değildir.

### F — Ortak CAD/GIS temeli

- [x] **F-01 · P0 — Komut × nesne türü × yüzey kapsam matrisi.** Çizgi/polyline, yay, daire, elips, spline, eğrisel polyline, delikli/çok parçalı alan, blok, yazı, ölçü ve taramayı listele; seçme, snap, grip, transform, trim, extend, split, offset, fillet, join, ölçüm ve aktarımı karşılaştır.
  **Kabul:** Her hücre destekli/kısmi/yok/uygulanamaz durumunda ve kanıtıyla kayıtlı; UI, AI, MCP, Python farkları görünür. Menüde bulunup uygulanamayan araç gizli başarı sayılmaz.
  **Başlangıç:** `src/core`, `src/command/src/commands`, `src/ai`, `src/script`.
  **Teslim (23 Eylül 2026):** [docs/nesneler/destek-matrisi.md](docs/nesneler/destek-matrisi.md), `kentos_kapsam` ile **ölçülerek üretiliyor** — her hücre için boş bir çizimde tür kullanıcı komutuyla kurulur, işlem aynı veri yolundan çalışır, sonuç sınıflanır ve kanıtı yanına yazılır; `scripts/ci-gate-kapsam.sh` yeniden üretip karşılaştırır. 15 tür × 17 işlem: 138 destekli, 10 kısmi, 38 yok, 52 uygulanamaz; çok parçalı alan hiçbir komutla oluşturulamadığı için ölçülemedi. Yüzey tablosu her işlemin komut satırı/arayüz, AI/MCP ve Python/JSON erişimini komut bayraklarından gösterir. "Menüde bulunup uygulanamayan araç" artık gizli başarı sayılmıyor: başarı dönüp belgeyi değiştirmeyen her düzenleme **sessiz ret** olarak ayrı listeleniyor. Dosya alışverişi (DXF/DWG/GPKG) bilerek dışarıda — derlemeye bağlıdır, I-01'in matrisidir.

- [ ] **F-02 · P0 — Tek nesne kimliği ve veri kaynağı sözleşmesi.** Kalıcı kimlik, kaynak kimliği, revizyon, CAD geometrisi, GIS şeması ve sağlayıcı düzenlenebilirliği birlikte tanımlansın. Referans katmanı içe almak ile canlı bağlanmak farklı eylemler olsun.
  **Kabul:** Haritada grip ile değiştirilen parselin tablo satırı, seçimi ve ilişkileri korunur; analiz çıktısı kökenini bilir. Salt okunur kaynağı düzenleme isteği yerel kopya seçeneğini açıkça sunar.
  **Bağımlılık:** F-01; mevcut Document/AttrTable ve PostGIS modelini genişlet, ikinci bağımsız CAD/GIS belge modeli kurma.

- [ ] **F-03 · P0 — Sayısal doğruluk ve tolerans sözleşmesi.** Depolama çözünürlüğü, geometrik hesap toleransı, ekrandaki snap yarıçapı, topoloji onarım eşiği ve aktarımda eğri yaklaşım hatası ayrı olsun. CRS birimi ile model birimi açıkça ayrışsın.
  **Kabul:** Büyük doğu/kuzey koordinatlarında 1 mm fark korunur; uzunluk/alan ara işlemleri taşmaz. Coğrafi derece doğrudan `Mm` kabul edilmez. Milimetre altı CAD gereksinimi için örnek veriyle karar ve gerekiyorsa sürümlü format göçü hazırlanır.
  **Bağımlılık:** F-02; `core/units`, geodesy ve IO sınırlarında tek dönüşüm politikası.

- [ ] **F-04 · P0 — İlişkili nesne ve türetilmiş sonuç altyapısı.** Ölçü, tarama sınırı, etiket, alan tablosu, arazi çıktısı ve pafta bağımlılıkları kaynak kimlik/revizyonuyla izlenebilsin; döngü ve kopuk bağ davranışı tanımlansın.
  **Kabul:** Kaynak değişince sonuç güncellenir veya açıkça “güncel değil” olur; yanlış eski sonucu sessizce kullanmaz. Silme/geri alma bağımlılıkları tutarlı geri getirir; yalnız etkilenen sonuçlar yeniden hesaplanır.
  **Bağımlılık:** F-02; mevcut layout bağlantıları ve kalıcı kimlikler üzerine kurulacak.

- [ ] **F-05 · P0 — Tek mantıksal işlem, ortak önizleme ve uzun iş yaşam döngüsü.** Geometri değişiklikleri, öznitelik aktarımı ve bağımlı güncellemeler birlikte uygulanabilsin. Uzun analizler ilerleme, iptal ve sonuç özeti taşısın.
  **Kabul:** 500 nesnelik iş ortasında hata/iptal belgeyi yarım bırakmaz; tek undo işlemi geri alır. Dış etkide atomiklik mümkün değilse hazırlama/yazma/başarısız öğeler ayrı raporlanır; tekrar çağrı çoğaltma yapmaz.
  **Bağımlılık:** F-02/F-04; mevcut Bus/journal hattı.

### C — Hassas ve eksiksiz CAD düzenleme

- [ ] **C-01 · P0 — Ortak eğri sorgu ve kesişim altyapısı.** Nokta değerlendirme, parametre, en yakın nokta, teğet, uzunluk, bounding box ve kesişim sonuçları geometri türüne göre ortak arayüzden gelsin.
  **Kabul:** Çizgi-yay, yay-yay, elips-çizgi ve spline kesişimleri; teğet temas, çakışan bölüm, kapalı eğri dikişi ve çoklu çözüm ayrı sonuçlanır. Sayısal çözüm başarısızlığı ile kesişim yokluğu ayrılır.
  **Bağımlılık:** F-01/F-03; mevcut geometri kütüphanelerini değerlendir, çözülmüş çekirdekleri yeniden yazma.

- [ ] **C-02 · P0 — Çizim yöntemlerini tamamla ve aynı davranışta birleştir.** Çizgi, bağlı yay/çizgi, daire, elips, dikdörtgen, çokgen ve spline varyantlarını mevcut araçlardan ilerlet; sayısal girdi ile işaretleme aynı sonucu üretsin.
  **Kabul:** Merkez/yarıçap, üç nokta, teğet ve mevcut varyantlar matriste sınanır; hayalet önizleme gerçek nesneyle çakışır. Araç tekrarında yöntem korunur; yanlış girilen son nokta tüm çizimi kaybettirmeden geri alınır.
  **Başlangıç:** Son snap/önizleme/araçta kalma düzeltmelerini regresyon paketi olarak koru.

- [ ] **C-03 · P0 — Gerçek CAD paraleli ile GIS tamponunu ayır.** Açık çizgiye tek taraflı paralel, kapalı sınır ofseti ve iki taraflı alan tamponu ayrı anlam taşısın. Taraf, mesafe, köşe, kaynak koruma ve öznitelik aktarımı tanımlansın.
  **Kabul:** `(0,0)→(10,0)` metre çizgisinin sol 2 m paraleli açık `(0,2)→(10,2)` çizgisidir; kapalı bant değildir. Sağ/sol yön ters çevirmeyle tutarlıdır. Daire yarıçapı doğru değişir; delikler korunur; çöken sonuç açıklanır.
  **Ek:** Genel spline/elips paralelinin aynı türden tam temsil edilemeyebileceğini hesaba kat; yaklaşık sonuçta hata sınırı ve tür değişimi görünür olsun. Statik bulgu: mevcut `offset.cpp` bant/alan üretiyor; önce yeniden üretme testi yaz.

- [ ] **C-04 · P0 — Eğrilerde ve toplu seçimde kes/uzat.** Bir veya çok sınır seçimi, tutulacak bölüm, ters taraf ve geçici sınır uzantısı desteklensin; sınır ve hedef nesne rolleri net olsun.
  **Kabul:** Yay ve daire çizgiyle, çizgi eğriyle kesilebilir; kalan yay yay olarak kalır. Teğet ve birden çok kesişim kullanıcıya aday gösterir. Toplu önizleme uygulanacak bölümleri gösterir; tüm işlem tek geri almadır.
  **Bağımlılık:** C-01/F-05; `trim.cpp` açık polyline sınırlamasını tür bazında kaldır.

- [ ] **C-05 · P0 — Böl, kır ve birleştir işlemlerinde gerçek geometri.** Noktadan, kesişimden, mesafeden ve eşit aralıktan bölme; aralık çıkarma; çizgi/yay zinciri birleştirme; açık/kapalı dönüşümü tamamla.
  **Kabul:** Parçaların toplam uzunluğu izin verilen toleransta kaynağa eşit; yaylar düzleşmez. Birleşmede yön, Z, katman ve öznitelik çatışması kurala bağlıdır. Boşluk toleransı kullanıcıdan gizlenmez.
  **Bağımlılık:** C-01/F-03; değişen kalıcı kimlikler için kaynak→sonuç eşlemesi döndür.

- [ ] **C-06 · P0 — Köşe yuvarlatma ve pah.** Çizgi-çizgi, çizgi-yay ve yay-yay için aday köşe/yarıçap; zincire toplu uygulama; kaynakları budama seçeneği.
  **Kabul:** Sonuç teğetlik ve yarıçap koşulunu sağlar; yanlış tarafta çözüm seçilmez. Sığmayan yarıçap, içbükey köşe ve sıfır yarıçap açık davranışa sahiptir; önizleme ve çıktı aynıdır.
  **Bağımlılık:** C-01/C-04; mevcut polyline köşe aracı korunarak genişletilir.

- [ ] **C-07 · P0 — Tutamaç, vertex ve stretch bütünlüğü.** Mevcut semantik grips altyapısını eğriler, çoklu seçim, blok dönüşümleri ve Z düzenleme ile tamamla; vertex ekle/sil/sürükle ve segment tipini değiştir.
  **Kabul:** Dairenin yarıçap tutamacı daireyi bozmadan çalışır; yay ortası ve spline kontrol noktası doğru anlamdadır. Stretch yalnız pencerenin kapsadığı öğeleri etkiler; kilitli/bağlı nesneler açıklanır.
  **Bağımlılık:** C-01/F-04/F-05.

- [ ] **C-08 · P0 — Dönüşümler ve çoğaltma.** Referanslı taşı/döndür/ölçekle, iki/üç noktayla hizala, aynala, dikdörtgensel/kutupsal/yol boyunca dizi ve taban noktalı yapıştırmayı ortak komut davranışında tamamla.
  **Kabul:** Referans açı/uzunlukla dönüşüm, tekrarlı kopya ve negatif ölçek sınanır. Eşit olmayan ölçek daireyi uygun elipse dönüştürür veya destek sınırını açıklar; yazı, ölçü ve bloklar sessizce bozulmaz.
  **Bağımlılık:** F-01/F-03; yakın tarihli dönüşüm önizlemesi iyileştirmeleri korunur.

- [ ] **C-09 · P1 — Alan üretimi, sınır bulma ve geometri temizliği.** Kapalı bölgeyi tıklayarak sınır çıkarma, delik/ada tanıma, çizgi ağından alan üretme, yinelenen ve sıfır uzunluklu öğeleri bulma.
  **Kabul:** Yakın fakat açık uçlar otomatik ve sessiz kapanmaz; boşluklar gösterilir. İç ada delik olarak korunur. Onarım öncesi/sonrası alan ve değişen öğeler raporlanır; GIS topoloji denetimi aynı çekirdeği kullanır.
  **Bağımlılık:** C-01/G-05; ortak sınırları bozan bağımsız sadeleştirme yapılmaz.

- [ ] **C-10 · P1 — Kaynağa bağlı profesyonel ölçülendirme.** Mevcut lineer, hizalı, açısal, radyal, çap, ordinate ve yay uzunluğu modelinin oluşturma/düzenleme/çıktı kapsamını tamamla; zincir, baz ve ölçü stilleri ekle.
  **Kabul:** Kaynak uç/yay değişince ölçü değeri ve yerleşimi güncellenir; kopuk bağ görünür. Birim, hassasiyet, tolerans, önek/sonek ve elle yazılmış değer gerçek ölçümden ayırt edilir; farklı pafta ölçeklerinde okunabilirlik korunur.
  **Bağımlılık:** F-04/C-01; yalnız ölçünün kendi tanım noktalarını değiştirmek canlı ilişki sayılmaz.

- [ ] **C-11 · P1 — Gerçek ilişkili tarama ve dolgu.** Sınır kimlikleri, delikler, desen açısı/aralığı/başlangıcı ve çoklu bölge; sınır değişince kontrollü yeniden hesaplama.
  **Kabul:** Delikli parsel değişince tarama delikten taşmaz; sınır silindiğinde bağ durumu görünür. Yoğun tarama etkileşimi kilitlemez; PDF ve DXF deseni aynı ölçekte taşır.
  **Bağımlılık:** F-04/C-09; mevcut DXF associative bayrağını çalışan bağımlılık yerine sayma.

- [ ] **C-12 · P1 — Yazı ve açıklama kalitesi.** Çok satırlı yazı, hizalama, satır/paragraf aralığı, Unicode/Türkçe, alan ifadeleri, leader/multileader ve arama/değiştirme kapsamını tamamla.
  **Kabul:** Ekran, PDF ve DXF'te satır kırılması/hizalama için karşılaştırmalı örnekler geçer; eksik font görünür. Alan/uzunluk içeren yazı kaynak değişiminde güncellenir; toplu bul/değiştir önizlemelidir.
  **Bağımlılık:** F-04/G-07; font lisansı ve taşınabilirlik çıktı paketinde kaydedilir.

- [ ] **C-13 · P1 — Blok ve sembol üretimi.** Mevcut blok tanımı/referansı üzerine yerinde düzenleme, öznitelikli blok, taban noktası, kitaplık ve kontrollü explode akışını tamamla.
  **Kabul:** İç içe, döndürülmüş ve aynalanmış blokta seçme/snap doğrudur; tanımı güncellemek tüm referanslara yansır. Explode sonrası geometri, görünüm, değerler ve kaynak ilişkisi raporlanır; döngü reddi korunur.
  **Bağımlılık:** F-02/F-04; parametrik blok P2 kapsamına ayrılır.

- [ ] **C-14 · P1 — Harici CAD/GIS referansları.** Referans yöneticisi, göreli yol, yeniden bağlama, görünürlük, kırpma, CRS/dönüşüm, yükleme durumu ve bağlama/kopyalama ayrımı.
  **Kabul:** Kaynak güncellenince kullanıcı değişikliği görür; kayıp referans belgeyi açılmaz yapmaz. Referansa snap mümkünken yanlışlıkla düzenleme olmaz. Proje paketi taşınınca yollar ve kimlikler çözülür.
  **Bağımlılık:** F-02/I-05; DWG xref desteğini genel referans yöneticisinden ayrı uyumluluk hücresiyle doğrula.

- [ ] **C-15 · P2 — Geometrik ve boyutsal kısıtlar.** Yatay/düşey, paralel, dik, teğet, eşmerkez, eşit ve sabit mesafe/yarıçap ilişkileri; çözüm durumunu gösteren arayüz.
  **Kabul:** Az/fazla kısıtlı durumlar açıklanır; çelişen kısıtlar belirlenir. Bir ölçüyü değiştirmek ilgili modeli günceller; sabit kontrol noktasını kaydırmaz. Çözüm tekrarlanabilir ve geri alınabilir olur.
  **Bağımlılık:** C-01/F-04; çözücü/lisans değerlendirmesi yapılır. Bu madde Netcad'de doğrulanmış özellik iddiası değil, PiriCAD geliştirme hedefidir.

- [ ] **C-16 · P1 — 2.5D CAD davranışı.** Z düzenlemesi, eğim, projeksiyonda görünen kesişim ile gerçek uzaysal kesişim ayrımı, hat boyunca kot üretme ve yüzeye oturtma.
  **Kabul:** Planda kesişen farklı kotlu iki hat kendiliğinden topolojik düğüm olmaz. XY taşıma Z'yi korur; 3D uzunluk/plan uzunluğu ayrı sunulur; ölçü ve dışa aktarım aynı Z politikasını kullanır.
  **Bağımlılık:** F-03/G-01/T-01; tam katı modelleme ile 2.5D araziyi aynı kapsam sayma.

### U — Hızlı ve öğrenilebilir çalışma alanı

- [ ] **U-01 · P0 — Ortak araç keşfi ve komut satırı.** Türkçe/İngilizce ad, kısa ad ve doğal dil araması; son kullanılanlar, favoriler ve seçime uygun araçlar; çizim anında komut seçenekleri.
  **Kabul:** “Paralel/ofset/offset” aynı gerçek araca ulaşır. Desteklenmeyen nesnede araç neden uygulanamadığını söyler; klavyeyle tüm temel iş akışı tamamlanabilir.
  **Bağımlılık:** F-01/A-01; metadata tek kaynaktan üretilir.

- [ ] **U-02 · P0 — Dinamik sayısal giriş.** Mutlak/göreli XY(Z), kutupsal mesafe/açı, açı birimi, eğim, uzunluk kilidi, geçici referans ve basit birimli ifadeler.
  **Kabul:** `12.5 m`, `1250 cm` ve desteklenen yerel sayı yazımları aynı sonucu verir; alanlar arasında geçiş ve kilit durumu görünür. Fareyi oynatmak kilitli değeri değiştirmez; son geçerli girdi kaybolmaz.
  **Bağımlılık:** F-03/C-02; UI metin ayrıştırıcısı ile AI/Python birimleri tutarlı olur.

- [ ] **U-03 · P0 — Yakalama ve seçimde kesinlik.** Mevcut snap türlerini karma geometri/bloklarda tamamla; geçici snap, izleme, aday döngüsü, pencere/kesişen pencere, çit ve seçim filtresi.
  **Kabul:** Adayın nesnesi ve türü görünür; zoom değişince piksel toleransı sabit hissedilir. Kilitli/gizli/reference katman davranışı ayarlanabilir; 100 üst üste nesnede istenen nesne seçilebilir.
  **Bağımlılık:** C-01/F-01; son snap simgesi iyileştirmelerine regresyon testi.

- [ ] **U-04 · P0 — Tek özellik paneli ve bağlı tablo seçimi.** CAD geometrisi, görünüm, GIS alanları, kaynak ve kalite durumu aynı nesne panelinde; çoklu seçimde ortak/farklı değerler.
  **Kabul:** Tablodan seçilen nesne haritada, haritadan seçilen satır tabloda bulunur. Hesaplanan alan ile düzenlenebilir alan ayırt edilir; toplu değişiklik kaç nesneyi etkileyeceğini gösterir ve tek geri alınır.
  **Bağımlılık:** F-02/G-03.

- [ ] **U-05 · P1 — Katman, görünüm ve çalışma alanı yönetimi.** CAD/GIS/raster/reference kaynaklarını tek ağaçta, türleri açık biçimde göster; gruplar, kilit, seçilebilirlik, ölçek aralığı, filtre, görünüm teması ve kayıtlı çalışma alanları.
  **Kabul:** Tema değişimi veriyi değiştirmez; görünürlük, basılabilirlik ve seçilebilirlik karışmaz. Aktif araç/katman/CRS/ölçek kullanıcıya her zaman anlaşılır; 1000 katmanda arama ve toplu işlem akıcıdır.
  **Bağımlılık:** F-02/G-06/L-01.

- [ ] **U-06 · P1 — İşe dayalı başlangıç ve yardım.** Ölçüden harita, parsel düzenleme, plan çizimi, GIS analiz ve arazi işi şablonları; kısa örnek projeler ve bağlama uygun açıklamalar.
  **Kabul:** Yeni kullanıcı kurulum dışında yardım almadan bir örnek veriyi açıp düzenler ve doğru ölçekli çıktı alır; uzman kısayolları aynı araçları hızlandırır. Hata mesajı nesneyi ve düzeltilebilir nedeni gösterir.
  **Bağımlılık:** Q-01; başarı gerçek kullanıcı denemesiyle ölçülür.

### G — GIS, CAD kadar temel bir yetenek

- [ ] **G-01 · P0 — CRS, birim ve dönüşüm doğruluğu.** Kaynak CRS, belge çalışma CRS'si ve görünüm CRS'si ayrı tanımlansın; eksen sırası, metre/feet/derece, yatay/düşey datum, grid ve dönüşüm doğruluğu taşınsın.
  **Kabul:** Aynı alanın farklı CRS verileri doğru üst üste gelir. CRS atamak ile yeniden projekte etmek ayrı işlemdir; eksik grid veya bilinmeyen CRS sessiz varsayılmaz. Lokal CAD çizimi açık yerleştirme/dönüşümle haritaya bağlanır.
  **Bağımlılık:** F-03; mevcut PROJ/geodesy servislerini genişlet. Nokta doğruluğu dönüşüm öncesi/sonrası kontrol noktalarıyla ölçülür.

- [ ] **G-02 · P0 — Veri sağlayıcı yetenekleri ve katman yaşam döngüsü.** Dosya/veritabanı/servis kaynaklarının okuma, yazma, filtre, indeks, transaction, eğri, Z/M ve şema yeteneklerini kaydet.
  **Kabul:** Katman açılırken kaynak, kimlik, CRS, satır tahmini ve kısıtlar görünür. Geometriyi salt görüntülemek ile edit buffer'a almak ayrılır; desteklenmeyen yazma işlemi başarılı görünmez.
  **Bağımlılık:** F-02/I-01; formatın GDAL'da varlığı bütün yeteneklerinin ürün tarafından desteklendiği anlamına gelmez.

- [ ] **G-03 · P0 — Öznitelik tablosu, şema ve alan hesaplayıcı.** Mevcut sütun modeli üzerinde filtre/sıralama, sanal görünüm, toplu hesap, null, zorunluluk, benzersizlik, kod listesi, varsayılan ve ilişkili kayıt desteğini tamamla.
  **Kabul:** Bir milyon satırı tamamen arayüze yüklemeden gez; `NULL`, sıfır ve boş metni ayır. Alan hesabı değişim özeti ve geri alma sağlar; bölünen/birleşen nesnede öznitelik aktarımı alan bazında kurallıdır.
  **Bağımlılık:** F-02/F-05; tarih-saat, büyük sayısal alan ve ilişkiler için mevcut AttrType dışı ihtiyaçlar göç kararıyla ele alınır.

- [ ] **G-04 · P0 — Şema bilen CAD sayısallaştırma kalemleri.** “Bina”, “yol ekseni”, “parsel”, “dere” gibi sınıflar geometri türü, katman, kod, varsayılan alan, gösterim, etiket ve doğrulama kurallarını birlikte seçsin.
  **Kabul:** Kullanıcı kalemi seçip çizer; sonuç anında anlamlı GIS kaydıdır. Mevcut CAD nesnelerini sınıfa bağlama önizleme ve alan eşlemesiyle yapılır. Kurum tanımları veri paketidir; C++ içine mevzuat kodlanmaz.
  **Bağımlılık:** G-03/G-06; mevcut katalog altyapısını kullan.

- [ ] **G-05 · P0 — CAD/GIS ortak topoloji denetimi ve düzenleme.** Gap, overlap, self-intersection, duplicate, dangle, geçersiz halka ve ortak sınır kuralları katman/sınıf bazında çalışsın.
  **Kabul:** Komşu parsel sınırı taşınırken topolojik düzenleme seçeneği ikisini birlikte günceller; kapalıyken oluşan hata bulunur. Hata katmanından nesneye gidilir; toplu onarımın toleransı ve alan etkisi önizlenir.
  **Bağımlılık:** F-03/F-05/C-09; mevcut `domain/cadastre/topology` geliştirilir, alan bazlı istisnalar kuralda tanımlanır.

- [ ] **G-06 · P1 — Profesyonel semboloji ve kartografya.** Tek sembol, kategori, dereceli ve kural temelli gösterim; ölçek aralığı, sembol düzeyi, dolgu/çizgi/işaret, ifade tabanlı özellikler ve temalar.
  **Kabul:** Aynı veri ekran/pafta/lejantta tutarlı görünür. CAD nesnesine özel stil ile sınıf/katman gösteriminin önceliği açık olur; stil değiştirmek asıl geometriyi bozmaz. Mevcut QGIS backend sınırları belgelenir.
  **Bağımlılık:** F-02/U-05; QGIS kullanılmayan derlemede destek kaybı sessiz olmaz.

- [ ] **G-07 · P1 — Etiketleme ve ortak ifade sistemi.** Alan, geometri, birim, CRS, proje ve atlas değişkenleri; null ve tür kuralları; çakışma, öncelik, leader, mask ve elle yerleştirme.
  **Kabul:** Aynı alan ifadesi tablo hesabı, etiket, filtre, pafta ve AI sorgusunda aynı değeri verir. Öznitelik veya sınır değişince etiket güncellenir; elle sabitlenen yerleşim kaybolmaz; yoğun haritada çakışmalar raporlanır.
  **Bağımlılık:** F-04/G-03; mevcut expression bileşenleri tek semantiğe bağlanır.

- [ ] **G-08 · P1 — Coğrafi raster ve georeferanslama.** GeoTIFF/COG başta olmak üzere büyük rasterı pencere/overview ile oku; bant, nodata, renk, kontrast, kırpma, mozaik ve kontrol noktasıyla yerleştirme.
  **Kabul:** Büyük ortofoto tümü RAM'e alınmadan CAD çiziminin altında akıcı açılır. Kontrol noktaları artık hata ve RMS ile listelenir; dönüşüm tipi/kaynak CRS kaydedilir. Raster sembol resmi bu yeteneğin yerine geçmez.
  **Bağımlılık:** G-01/G-02/Q-02; ileri raster analizleri G-11'de.

- [ ] **G-09 · P1 — OGC ve harita servisleri.** WMS/WMTS/XYZ görüntü ile WFS/OGC API Features vektör erişimini ayrı sağlayıcılar olarak değerlendir; servis keşfi, CRS, sayfalama, önbellek ve bağlantı kesilmesi.
  **Kabul:** Aynı referans altında CAD çizilir; sunucu hatası arayüzü kilitlemez. Servis kısıtı, sürüm, kaynak atfı ve kullanım koşulları kaydedilir. Sunucunun desteklemediği çevrimdışı indirme veya yazma özelliği vaat edilmez.
  **Bağımlılık:** G-02/G-08; sırlar proje/AI bağlamına düz metin taşınmaz.

- [ ] **G-10 · P1 — Vektör mekânsal analiz çekirdeği.** Konum/mesafe ile seç, clip, intersection, union, difference, dissolve, spatial join, nearest, buffer ve istatistik işlemlerini ürün seviyesine getir.
  **Kabul:** Delik, çok parça, boş sonuç ve çakışan alan adları doğru ele alınır. Planar/geodezik mesafe seçimi açık; CAD paraleli buffer adı altında karışmaz. Girdi revizyonu, parametre, CRS ve algoritma sürümü sonuca eklenir.
  **Bağımlılık:** G-01/G-03/C-03; mevcut alan düzenleme hesapları yeniden kullanılacak.

- [ ] **G-11 · P1 — Raster/vektör birlikte analiz.** Raster hesaplama, yeniden örnekleme, eğim/bakı/gölgeleme, kontur, rasterdan Z, zonal istatistik ve raster-vektör dönüşümü için ortak işlem tanımları.
  **Kabul:** Nodata, hücre boyutu, hizalama ve yeniden örnekleme yöntemi sonuçla kaydedilir. Parsel bazında ortalama kot/eğim doğru hesaplanır; kategorik rastera yanlış interpolasyon sessiz uygulanmaz.
  **Bağımlılık:** G-08/G-10/T-01; mevcut kontur hesabıyla raster yüzey hesabının farkı açıklanır.

- [ ] **G-12 · P1 — Tekrarlanabilir GIS işlem modeli.** Girdi/çıktı türleri ve bağımlılıkları bilinen zincirler, toplu çalıştırma, parametreli şablon, koşullu adım ve kaynak değişince yeniden çalıştırma.
  **Kabul:** “Yol tamponu → etkilenen parseller → alan özeti → atlas” modeli UI, AI, MCP ve Python'dan aynı kayıtlı tanımla çalışır. Adım süreleri/nesne sayıları görünür; başarısız adımdan güvenli yeniden çalışma mümkün olur.
  **Bağımlılık:** F-04/F-05/G-10/A-01; grafik arayüz ortak işlem şemasının görünümü olsun.

### I — Veri alışverişi ve kurumsal kullanım

- [ ] **I-01 · P0 — Format ve veri kaybı matrisi.** DXF, derlemeye bağlı DWG, GeoPackage, GeoJSON, SHP, GML, CSV/XYZ ve veritabanı yolları için gerçek okuma/yazma yeteneklerini çıkar; DGN, LandXML, LAS/LAZ gibi genişlemeleri kanıta göre sırala.
  **Kabul:** Tür, eğri, blok, stil, font, öznitelik, CRS, Z/M ve null korunumu ayrı ölçülür. Veri kaybı içe/dışa aktarmadan önce ve sonra nesne bazında raporlanır; “destekli format” genel etiketi yeterli değildir.
  **Bağımlılık:** F-01/G-02; mevcut IO tanı mekanizması.

- [ ] **I-02 · P1 — Netcad projelerinden geçiş.** Kullanıcının gerçek Netcad örnekleriyle NCZ ekosisteminden DXF + GIS verisi + nokta listesi + stil/katalog eşlemesi üzerinden aktarım rotasını doğrula.
  **Kabul:** Seçilen örnek projelerde eksilen nesne/öznitelik/stil raporu ve düzeltme yolu vardır. NCZ'nin doğrudan tam desteği araştırılmadan vaat edilmez; gerekiyorsa belgeli ve lisansı uygun dönüştürücü/SDK kararı ayrıca verilir.
  **Bağımlılık:** I-01/G-04; hedef “dosya açıldı” değil üretime devam edilebilmesidir.

- [ ] **I-03 · P1 — DXF/DWG profesyonel round-trip.** Eğriler, bulge, blok özniteliği, katman/çizgi tipi, ölçü, tarama, metin, birim, UCS/OCS ve model/paper-space kapsamını referans dosyalarla doğrula.
  **Kabul:** İçeri al→kaydet→bağımsız okuyucuda aç karşılaştırması geometrik tolerans ve semantik değerlerle geçer. LibreDWG/opsiyonel derleme sınırları açık; desteklenmeyen entity için kayıp raporu bulunur.
  **Bağımlılık:** I-01/C-10/C-11/C-13; lisans mimarisiyle çelişen bağımlılık eklenmez.

- [ ] **I-04 · P1 — Canlı PostGIS düzenleme ve çatışma.** Mevcut proje/katman saklama üzerine kalıcı feature kimliği, artımlı değişiklik, yetki, optimistic locking ve yeniden bağlanma sözleşmesi kur.
  **Kabul:** İki oturum aynı parseli değiştirince son yazan sessizce ezmez. Çok nesneli işlem transaction içinde tamamlanır; ağ kesilmesi tekrar yazımda çoğaltma yapmaz. Tabloyu bütünüyle değiştirme ile satır düzenleme UI/API'de ayrıdır.
  **Bağımlılık:** F-02/F-05/G-02; veritabanı integration testleriyle doğrulanır.

- [ ] **I-05 · P1 — Taşınabilir ve kurtarılabilir proje.** Şema/format sürümü, katalog, sembol, font referansı, harici veri, CRS grid ve kaynak envanteri; göreli yollar, autosave ve çökme kurtarma.
  **Kabul:** İkinci makinede proje eksik bağımlılıkları açıkça bildirerek açılır. Kayıt sırasında çökme son sağlam projeyi bozmaz; eski sürüm dosyası kontrollü göç eder. Büyük referans verilerini pakete gömmek isteğe bağlıdır.
  **Bağımlılık:** F-02/C-14; mevcut yerel format/journal üzerine kurulacak.

### S — Haritacılık, kadastro ve plan üretimi

- [ ] **S-01 · P1 — Ölçü noktası ve saha verisi.** Nokta numarası/kodu, kaynak, kalite, Z ve ölçüm sınıfı; yinelenen numara yönetimi, koddan çizgi üretme ve nokta listesi eşlemesi.
  **Kabul:** Aynı ölçü dosyası tekrar alındığında çoğaltma seçimi açık olur; eksik kot ile sıfır kot ayrılır. Kontrol noktaları korunur; kodlu saha verisinden oluşan çizginin kökeni izlenir.
  **Bağımlılık:** G-01/G-03/I-01; mevcut nokta/polar komutları genişletilir.

- [ ] **S-02 · P1 — Jeodezik hesap ve aplikasyon.** Mevcut dönüşüm, fit, traverse ve stakeout akışlarını tek iş ekranında birleştir; artık hata, kapanma, ağırlık ve rapor doğruluğunu tamamla.
  **Kabul:** Bilinen kontrol ağı örnekleriyle hesap doğrulanır; ölçülen, düzeltilen ve aplikasyon koordinatları ayırt edilir. Seçilen yöntem/parametre/CRS ve birimler raporla taşınır.
  **Bağımlılık:** F-03/G-01/S-01; gelişmiş ağ dengelemesi kapsamı kullanılan matematik modeliyle ayrıca tanımlanır.

- [ ] **S-03 · P1 — Parsel düzenleme ve alan denetimi.** Mevcut ifraz/tevhit üzerine hedef alana göre bölme, doğrultu/cephe koşulu, ortak sınır, numaralandırma ve kaynak-soy ilişkisini tamamla.
  **Kabul:** Girdi toplam alanı ile sonuç toplamı, delikler ve dış sınır korunumu kontrol edilir; fark gerekçelendirilir. Malik/öznitelik aktarımı kuralı görünür; taslak işlem ile resmî teslim durumu ayrılır.
  **Bağımlılık:** G-05/F-04/S-02; fiziksel alan ile kayıtlı/tapu alanı farklı alanlarda tutulur.

- [ ] **S-04 · P2 — Planlama, dağıtım ve kurum teslim paketleri.** Ölçek bazlı gösterim, yol/çekme mesafesi, kullanım alanı özeti, parselasyon dağıtımı ve kurum raporlarını sürümlü kural paketleriyle kur.
  **Kabul:** Kullanılan kuralın sürümü/tarihi/kaynağı raporda vardır; kurallar örnek kurum verisi ve uzman kontrolüyle doğrulanır. Mevzuat güncel diye varsayılmaz; ürün sayfasındaki eski mevzuat atfı yazılıma doğrudan kopyalanmaz.
  **Bağımlılık:** G-04/S-03/L-03; AI mevzuata uygunluk veya mühendis imzası uydurmaz.

### T — Arazi, kesit ve mühendislik modeli

- [ ] **T-01 · P1 — Kalıcı ve düzenlenebilir arazi yüzeyi.** Mevcut geçici üçgenleme/contour hesabından ilerle; kaynak noktalar, kırık hatlar, dış sınır, boşluk, üçgen düzenleme ve yüzey sürümü tanımla.
  **Kabul:** Dere sırtı/kırık hat korunur; bina boşluğunda üçgen oluşmaz. Kaynak değişince etkilenen yüzey/çıktı güncellenir veya kirli işaretlenir; aynı kaynak aynı yüzeyi üretir.
  **Bağımlılık:** F-03/F-04/S-01; yeni kalıcı yüzey türü model/format tasarım kararı gerektirir.

- [ ] **T-02 · P1 — Eş yükselti ve yüzey analizi.** Ana/ara eğri, etiket, yumuşatma, sınır kırpma, eğim/bakı ve drenaj gibi analizleri yüzey kimliğiyle ilişkilendir.
  **Kabul:** Eş yükselti etiketi doğru kotu taşır; yumuşatma topoğrafik anlamı bozmaz ve hata sınırı kayıtlıdır. Nodata/boşluk ve düz üçgen uç durumları sınanır.
  **Bağımlılık:** T-01/G-07/G-11; mevcut contour komutu yeniden yazılmadan genişletilir.

- [ ] **T-03 · P1 — Boykesit, enkesit ve iki yüzey hacmi.** Seçilen hat boyunca profil, istasyon/enine mesafe, birden çok yüzey ve sınırlandırılmış kazı/dolgu.
  **Kabul:** Basit prizma ve kesişen yüzey örneklerinde bağımsız analitik sonuçla uyuşur; kazı/dolgu ayrı raporlanır. Hesap alanı, boşluk politikası, yöntem ve yüzey revizyonları sonuçla saklanır.
  **Bağımlılık:** T-01/F-04/L-03; mevcut referans düzleme hacim desteği iki yüzey desteği sayılmaz.

- [ ] **T-04 · P2 — Güzergâh ve koridor.** Doğru/yay/geçiş eğrisi, kilometre/istasyon, düşey güzergâh, tip kesit, şev ve parametrik koridor modeli.
  **Kabul:** Bir güzergâh değişikliği plan, profil, kesit ve hacmi ilişkili günceller; geometrik süreklilik kontrol edilir. Yeniden hesaplanan ve geçersiz sonuçlar açıkça ayrılır.
  **Bağımlılık:** C-01/C-16/T-03; netpro düzeyindeki kapsam aşamalı modül olarak teslim edilir.

- [ ] **T-05 · P2 — Büyük nokta bulutu ve eş zamanlı 2D/3D.** LAS/LAZ/COPC gibi aday formatları veri/bağımlılık analiziyle seç; sınıf/yoğunluk/kot görünümü, LOD, kesit ve kontrollü yüzey üretimi.
  **Kabul:** Veri tamamı RAM'e yüklenmez; 2D/3D seçim aynı kimlik/konumu gösterir. Yerel koordinatlı render hassas model değerini değiştirmez; kot örneklemesinin yöntemi ve yoğunluğu raporlanır.
  **Bağımlılık:** G-02/T-01/Q-02; nokta bulutundan CAD çıkarımı ayrıca kalite ölçütü ister.

### L — Pafta, kartografya ve teslim kalitesi

- [ ] **L-01 · P1 — Mevcut layout sistemini üretim seviyesinde doğrula.** Çok sayfa, sayfa boyutu/yönü, çoklu harita, kilitli ölçek/tema, bağlı ölçek çubuğu/kuzey oku/lejant, grid, overview, hizalama, gruplama ve şablon kapsam matrisi.
  **Kabul:** Bir sayfada farklı CRS/ölçekte iki harita kendi doğru grid/lejant/ölçek öğesine bağlanır; yanlış haritaya bağlı öğe görünür hata verir. Ekran ile çıktı aynı yerleşimi taşır; undo/redo ve save/open geçer.
  **Bağımlılık:** F-04/G-01/G-06; var olan linked_map/atlas/report işleri yeniden başlatılmaz.

- [ ] **L-02 · P1 — Veri güdümlü yerleşim ve QGIS 4.2 farkları.** İfadeyle boyut/konum/metin, katman gösteriminden grafik kategori/renkleri, şekille resim kırpma ve harita kapsamına göre lejant.
  **Kabul:** Veri/sınıflandırma değişince grafik ve lejant tutarlı güncellenir; geçersiz ifade boş çıktı yerine tanı üretir. Kırpma vektör/raster çıktıda aynı sınırı izler.
  **Bağımlılık:** G-07/L-01; QGIS 4.2 referansındaki üç somut yerleşim yeniliği bölüm 2'de kaynaklıdır, GeoPDF L-04'te ele alınır.

- [ ] **L-03 · P1 — Atlas ve mühendislik raporları.** Mevcut atlas/report üzerine filtre/sıralama, sabit/uygun ölçek, grup başlıkları, taşan tablolar, boş veri ve bölüm numaralandırması.
  **Kabul:** 1000 parsel atlasında taşan yazı, yanlış kapsam veya kayıp tablo sessiz geçmez. Parsel özeti ve kesit/hacim raporu doğru kaynak revizyonunu taşır; iptal edilen toplu üretim durumu raporlanır.
  **Bağımlılık:** G-07/L-01/T-03; mevcut report grup modelinin çıktı davranışı test edilir.

- [ ] **L-04 · P1 — PDF/SVG/raster ve GeoPDF teslimi.** Ölçek, çizgi kalınlığı, font, transparanlık, raster DPI, seçilebilir yazı; GeoPDF koordinat ve katman/grup görünürlüğü; toplu adlandırma.
  **Kabul:** Bilinen 100 m çizgi 1:1000 çıktıda 100 mm'dir; farklı PDF okuyucularında ölçü ve yerleşim sınanır. GeoPDF katman ağacı ve öznitelik dahil etme seçimi doğrulanır; rasterleşen öğe ve sebebi bildirilir.
  **Bağımlılık:** L-01/I-01; çıktı desteği backend/derleme koşullarıyla birlikte kaydedilir.

- [ ] **L-05 · P1 — Teslim öncesi kalite kapısı.** Mevcut preflight'ı kayıp font/kaynak, kopuk bağ, eski hesap, CRS, ölçek, dışa taşma, tablo taşması ve düşük raster çözünürlüğü kontrolleriyle tamamla.
  **Kabul:** Her bulgu ilgili nesne/sayfaya gider; kullanıcı düzeltir veya gerekçeli istisna bırakır. Rapor, veri ve çıktıyla aynı revizyonu gösterir; analizden sonra kaynak değişmişse eski “temiz” sonucu kullanmaz.
  **Bağımlılık:** F-04/L-01; ayarlanabilir uyarı ve engel seviyeleri, otomasyonda da aynı sonuçlar.

### A — AI, MCP ve Python ile eksiksiz üretim

- [ ] **A-01 · P0 — Tek yetenek kataloğu ve yüzey eşitliği.** Mevcut CommandSpec/Processing metadata'sından UI, AI araçları, MCP şemaları, Python bağları ve yardım üret; her işin tipli girdi/çıktı, birim, CRS, etki ve önkoşulu olsun.
  **Kabul:** Yeni kullanıcı işlemi kataloğa girdiğinde dört yüzeyde keşfedilir veya gerekçeli kapsam dışıdır. Bir aracın listelenmesi değil, etkileşimsiz geçerli parametrelerle iş bitirmesi ölçülür; schema/yardım/API ayrışması denetlenir.
  **Bağımlılık:** F-01/F-05; devam eden İngilizce parametre ve Python API çalışmalarını tamamla, ikinci binding listesi üretme.

- [ ] **A-02 · P0 — AI'nin doğru proje bağlamını görmesi.** Seçim, aktif katman/kalem, CRS/birim, görünüm alanı, veri kaynakları, şema, sınırlı örnekler, hata ve son işlem sonuçları yapılandırılmış okunabilsin.
  **Kabul:** “Seçili yayları yola kadar uzat” isteğinde model nesne kimliği/türünü doğrular; ekran pikselinden koordinat uydurmaz. Büyük tabloyu tamamen isteme yerine filtre/sayfalama kullanır; bağlam değişince eski kimlik/revizyon fark edilir.
  **Bağımlılık:** F-02/G-02; görsel önizleme yapısal veriyi tamamlar.

- [ ] **A-03 · P0 — Soru ve onay ayarlarını uçtan uca tamamla.** Mevcut `core.ai.onay_politikasi`, `core.ai.soru_politikasi` ve `core.ai.uzerine_yazma` ayarlarını anlaşılır Settings denetimleriyle sohbet ve MCP'ye aynı karar motorundan uygula.
  **Kabul:** “Her değişiklikte / yalnız riskli işlemlerde / otomatik” ile “etkili belirsizlikte sor / yalnız zorunlu / varsayımla ilerle” bağımsızdır. Otomatik ve varsayımla ilerle modunda parametreleri yeterli bir CAD/GIS işi gereksiz onay/soru döngüsü olmadan biter; kullanılan varsayımlar sonuçta görünür.
  **Sınır:** Eksik zorunlu CRS/hedef gibi sonucu belirleyen bilgi uydurulmaz. Kullanıcı Settings'te verilen kapsamı belirler; agent kendi yetkisini/politikasını genişletemez. Bu, henüz yoktan yapılacak ayar değil, mevcut policy/policy_path hattının kullanım ve eşitlik işidir.

- [ ] **A-04 · P1 — Planı gerçek iş sonucuna kadar yürüt.** Plan→önizleme→politika kararı→uygulama→geometrik/öznitelik doğrulama→çıktı zinciri; yeniden deneme, değişen kaynak ve çok adımlı geri alma.
  **Kabul:** “Yolun iki yanına 5 m paralel çiz, etkilenen parselleri bul, alan tablosunu ve atlası üret” işi elle menü açtırmadan tamamlanır. Kullanıcı onayı gerekiyorsa somut plan için bir kez istenir; aynı onaylı plan adımlarında tekrar sorulmaz. Kaynak/plan değişirse eski izin içeriğiyle uygulanmaz.
  **Bağımlılık:** F-05/C-03/G-10/L-03/A-03; özet başarı demeden çıktı ve değişen nesneler kontrol edilir.

- [ ] **A-05 · P1 — MCP'yi tam uygulama yüzeyi yap.** Araç keşfi, proje/nesne/şema kaynakları, yapılandırılmış hatalar, önizleme ve dosya sonuçları; belge oturumu/kimliği açık olsun, istemci bağlamı gizli UI durumuna bağlı kalmasın.
  **Kabul:** Bağımsız MCP istemcisi veri açma, çizim/düzenleme, GIS analiz, layout, dışa aktarma ve geri alma senaryosunu tamamlar. Kullanıcı başka belgeye geçince yanlış belgeye yazmaz; kopma/yeniden bağlanma işlemi iki kez uygulamaz.
  **Ek:** Kodun mevcut 2026-07-28 yolu gerçek istemciyle doğrulanır; uzun iş için müzakere edilen Tasks uzantısı veya belgeli uygulama iş tanıtıcısı kullanılır. Desteklenmeyen protokol özelliği varmış gibi duyurulmaz.

- [ ] **A-06 · P1 — Kaydedilebilir iş tarifleri ve Python otomasyonu.** Elle/AI ile tamamlanan işi parametreli tarif olarak sakla; aynı komut şemalarından Python örnekleri ve belgeler üret, proje revizyonu/bağımlılıkları kaydet.
  **Kabul:** “Nokta dosyası→yüzey→kontur→kontrol→pafta” tarifi farklı girdilerle tekrar çalışır; bağımlılık veya API sürümü değişince anlaşılır tanı verir. İptal/exception yarım belge bırakmaz; keyfi Python çalıştırma ile tipli komut yetkisi aynı şey sayılmaz.
  **Bağımlılık:** A-01/G-12/F-05; mevcut gömülü Python çalışmasını temel al, kaldırılmış Lua hattını geri getirme.

- [ ] **A-07 · P1 — Gözlemlenebilir, kapsamlı ama denetlenebilir AI.** Hangi nesnelerin okunduğu/değiştiği, neden soru/onay istendiği, hangi politikanın uyguladığı ve çıktı doğrulaması görülebilsin; durdur/geri al/yeniden çalıştır kontrolleri.
  **Kabul:** “Tamamlandı” mesajından nesne ve dosyaya gidilir; kısmi hata başarı diye sunulmaz. İstemci izinleri kullanıcı ayarlarıyla uygulanır; proje metni/öznitelik içindeki talimatlar yetki sayılmaz. Dış veri paylaşımı ve veritabanı yazımı belirlenen kapsamla sınırlıdır.
  **Bağımlılık:** A-03/A-05; normal çizim işini gereksiz uyarıya boğmadan mevcut audit ve redaction hattını tamamla.

### Q — Performans, sağlamlık ve ürün doğrulaması

- [ ] **Q-01 · P0 — Gerçek proje kabul veri seti.** Anonimleştirilmiş saha ölçüsü, karma CAD, parsel/imar, büyük GIS, ortofoto ve arazi örneklerini; küçük analitik doğruluk örnekleriyle birlikte sürümle.
  **Kabul:** Her veri setinin kaynak/izin bilgisi, beklenen geometri/alan/öznitelik sonucu ve teslim dosyaları vardır. Aynı senaryolar elle, AI, MCP ve Python için tekrar edilebilir; yalnız unit test sayısı raporlanmaz.
  **Bağımlılık:** F-01; kullanıcı verisi araştırma amacıyla dışarı gönderilmez.

- [ ] **Q-02 · P0 — Ölçülmüş performans bütçeleri.** Donanım, işletim sistemi, derleme, veri seti, görünür nesne miktarı, soğuk/sıcak önbellek ve P50/P95/P99 sürelerini kaydet; mevcut benchmark'ları gerçek işlerle tamamla.
  **Kabul hedefi:** Orta referans projede P95 çizim/snap/önizleme geri bildirimi ≤50 ms; normal gezinmede P95 kare ≤33 ms. 100 bin/1 milyon nesne ve büyük raster kademelerinde açma/RAM/indeks bütçesi ilk ölçümden sonra yazılı sabitlenir; uzun işte iptal isteği ≤1 s içinde algılanır.
  **Not:** Bunlar henüz ölçülmüş sonuç değil, doğrulanacak ürün hedefleridir. Netcad/QGIS'ten hızlı olma iddiası ancak aynı veri/donanım/senaryoyla ölçülür.

- [ ] **Q-03 · P0 — Geometrik doğruluk, round-trip ve bozuk veri testleri.** Analitik örnekler, özellik tabanlı testler, bağımsız kütüphane karşılaştırması ve bozuk dosya fuzz testleri; büyük koordinat/küçük detay ve dönüşüm uç durumları.
  **Kabul:** Alan korunumu, birleştir-böl ilişkisi, transform tersi, curve parametreleri, snap sonucu ve hole topolojisi ölçülür. Hatalı içe aktarım kısmi başarı raporu verir veya atomik geri döner; çökmez.
  **Bağımlılık:** F-03/Q-01; farklı motor farkları tolerans ve model semantiğiyle açıklanır.

- [ ] **Q-04 · P1 — Gerçek etkileşim ve çıktı regresyonu.** Klavye/fareyle seçim, sayı girişi, snap, grip, iptal, tekrar; farklı DPI/tema/platform; pafta ve font görüntü karşılaştırmaları.
  **Kabul:** Tuşa basılması yalnız işleyici çağrısı değil gerçek kullanıcı olaylarıyla sınanır. Görsel farkla birlikte geometri/ölçü doğrulanır; odak kaybı, modal pencere ve araçta kalma hataları kapsanır.
  **Bağımlılık:** U-01/U-03/L-04; mevcut OS olay testlerinden ilerle.

- [ ] **Q-05 · P1 — Kullanım kıyaslaması ve sürüm kapısı.** Netcad/QGIS deneyimli kullanıcılarla aynı teslim işini yap; süre, tıklama/tuş sayısı, düzeltme, yanlış sonuç ve öğrenme ihtiyacını ölç.
  **Kabul:** İlk ölçüm baz alınarak sonraki sürümde hedeflenen iyileşme gösterilir. Açık kritik doğruluk/veri kaybı hatasıyla “profesyonel hazır” etiketi verilmez; destek matrisi, bilinen sınırlamalar ve değişiklik notları sürümle yayınlanır.
  **Bağımlılık:** Q-01/Q-02/Q-04; farklılaşma iddiası bu kanıtlardan çıkarılır.

## 5. Uygulama sırası ve teslim kapıları

Takvim tahmini verilmedi; kapsam ve mevcut altyapı doğrulandıktan sonra işler küçük teslimlere bölünecek. Büyük modüller P0 düzeltmelerini geciktirmemeli. GIS veri kimliği/CRS/öznitelik/topoloji ve otomasyon temeli ilk aşamadan itibaren CAD ile birlikte geliştirilir.

| Kapı | Teslim edilecek sonuç | Başlıca bağımlılıklar | Geçiş kanıtı |
|---|---|---|---|
| **K0 — Ölçülebilir mevcut durum** | Kapsam matrisi, örnek projeler, yeniden üretilen doğruluk açıkları | F-01, Q-01, Q-02, Q-03 | Destekli/kısmi/yok ayrımı ve ilk performans kaydı |
| **K1 — Güvenilir günlük CAD/GIS düzenleme** | Gerçek paralel, eğri kes/uzat/böl, ortak kimlik, CRS, öznitelik, snap ve AI politikası | F-02–F-05, C-01–C-08, U-01–U-04, G-01–G-05, I-01, A-01–A-03 | E1–E4 senaryoları; veri kaybı ve tekrar onay döngüsü yok |
| **K2 — Profesyonel Map CAD/GIS** | İlişkili açıklamalar, kartografya, raster/servis, analiz, IO, parsel ve pafta | C-09–C-14/C-16, G-06–G-12, I, S-01–S-03, L, A-04–A-07 | E5–E8 ve karma teslim paketi; bağımsız çıktı kontrolü |
| **K3 — Arazi ve uzmanlık** | Kalıcı yüzey, profil/hacim; ardından güzergâh, planlama ve parametrik tasarım | T-01–T-05, S-04, C-15 | E9–E10; uzman denetimli referans hesaplar |

### Uçtan uca kabul senaryoları

| ID | Kullanıcının işi | Geçme koşulu |
|---|---|---|
| **E1** | Açık yol ekseninin iki yanına 5 m paralel üret, kavşakta yayla birleştir | Açık eğri semantiği, doğru mesafe/teğetlik, kaynak korunumu ve tek undo |
| **E2** | Daire ve yayları sınır çizgilerine göre kes/uzat; seçili kısmı tekrar düzenle | Geometri türü korunur, doğru çözüm önizlenir, snap/ölçüm sonucu tutarlıdır |
| **E3** | Ortak sınırlı iki parseli grip ile düzenle, tabloda alanını ve paftada etiketini gör | Ortak sınır, kimlik, alan, öznitelik ve ilişkili çıktı birlikte tutarlıdır |
| **E4** | Otomatik AI ayarıyla seçili nesneleri düzenle; aynı işi MCP'den çalıştır | Gerekli bilgi mevcutsa sıfır gereksiz soru/onay; aynı sonuç ve açıklanabilir işlem kaydı |
| **E5** | DXF + GeoPackage + farklı CRS ortofotoyu birleştir, geometri/öznitelik sorgula | Konumsal uyum kontrol noktalarıyla doğru; derece/metre karışmaz; kaynak kimliği korunur |
| **E6** | Dere tamponundaki parselleri bul, etkilenen alanı hesapla, kategorize et ve raporla | Tampon/paralel ayrımı; doğru delik/çok parça ve alan toplamı; tekrarlanabilir analiz |
| **E7** | 1000 parsel için atlas, PDF ve CAD/GIS teslim dosyaları üret | Doğru ölçek, taşmayan etiket/tablo, kaynak revizyonu ve dış okuyucuyla uyumluluk |
| **E8** | PostGIS'te parsel değiştirirken bağlantıyı kes ve başka oturumla çakıştır | Yarım kayıt/çift nesne/sessiz üzerine yazma yok; kurtarma yolu açık |
| **E9** | Saha noktası + kırık hattan arazi üret, eş yükselti ve iki yüzey hacmini hesapla | Sınır/boşluk/kırık hat korunur; analitik örnekle hacim tutar; değişiklik bağımlılara yansır |
| **E10** | Güzergâhı değiştir, plan/profil/kesit/hacim ve paftayı yenile | Eski sonuç güncel görünmez; tek kaynaktan ilişkili üretim ve tekrar çalıştırılabilir tarif |

## 6. İlk somut iş paketi

İlk geliştirme turu **F-01 + Q-01**, ardından **C-03, C-04, G-01/G-03/G-05 ve A-03** etrafında kesilecek. Sebep: yeni araç sayısından önce çizimin doğru üretilmesi, GIS anlamını koruması ve gereksiz AI kesintileri olmadan kullanılabilmesi gerekiyor. C-01/F-03/F-05 bu araçların ihtiyaç duyduğu ölçüde ortaklaştırılacak; bütün uygulamayı baştan yazan bir çerçeve projesine dönüştürülmeyecek.

Her teslimde buradaki ilgili kutu yalnız kabul kanıtıyla kapanacak. Yeni araştırma veya kod değişikliği mevcut durum tablosunu değiştirirse önce o tablo güncellenecek; zaten yapılmış özellik tekrar “eksik” diye görevlendirilmeyecek.
