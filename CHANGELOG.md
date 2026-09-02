# Değişiklik Günlüğü

Bu proje [Semantik Sürümleme](https://semver.org/lang/tr/) kullanır.
Mevzuat kataloğu değişiklikleri, sebebi olan yönetmelik veya genelge adıyla
birlikte kaydedilir (CLAUDE.md Article 9).

## [Yayımlanmamış]

### Değişti — uygulamanın adı KentOSCad oldu (görünen ad)

- Pencere başlığı, hakkında kutusu, dosya süzgeçleri ve 29 belge sayfası yeni adı
  taşıyor. `QApplication` kimliği `KentOSCad`, alan adı `kentoscad.org`.
- **Kullanıcı verisi taşınıyor, kaybolmuyor.** Qt her kullanıcı yolunu uygulama
  adından türetir; ad değişince ayar dosyası, stil kütüphanesinin yazdığı yapılandırma
  dizini ve otomatik kaydın veri dizini başka yere düşerdi. `migrate_user_data()`
  eski yolları Qt'ye *sordurup* (tahmin etmeden, üç platformun yerleşimi farklı)
  yenisine taşıyor; hedef zaten varsa dokunmuyor.
- İsim uzayı, `#include` yolları, CMake hedefleri ve `PIRICAD_*` makroları bu adımda
  **değişmedi**; onlar tek mekanik değişiklik olarak ayrı iniyor.

### Eklendi — dinamik girdi ve çizim adımı

- **Kılavuz artık ölçüsünü yazıyor**: sürüklenen lastik bandın üzerinde uzunluk ve
  azimut. Azimut kuzeyden saat yönündedir — aletten okunan değer — ve birimi
  `açı_birimi` tercihine uyar; varsayılan **grad**, çünkü Türkiye'de nirengi,
  poligon ve aplikasyon hesapları gradla yürür.
- **ADIM kilidi** (`core.yakalama.adim`): imlecin bir önceki noktaya olan
  uzaklığını verilen değerin katına yuvarlar. 12 cm dendiyse çizgi 12, 24, 36 cm'de
  durur. Yön kilitleriyle birlikte çalışır — dik mod/kutupsal yönü, adım uzunluğu
  seçer — ve gerçek bir nesne yakalaması her zaman adımın önündedir.
  `Shift+F3 ▸ Adım…` ya da `MOD ad=adım deger=120`.
- Adımın aritmetiği tam sayıdır (`segment_length` + tam bölme), yani üç platformda
  bit-birebir aynıdır (§7.3).
- **`core.arayuz.dinamik_girdi` ayarı eklendi.** Durum çubuğundaki DİNAMİK GİRDİ
  çipi bir yıldır var olmayan bir ayara bağlıydı: tıklandığında "bu yardımcı henüz
  bir ayara bağlı değil" diyordu.

### Eklendi — nesne yakalama modları arayüzden seçilebiliyor

- Yakalama motoru on üç kip taşıyor ve kabuk üçünü gösteriyordu: F3 "herhangi
  biri", F8 dik mod, F9 ızgara. KESİŞİM, DİK AYAK, EN YAKIN, DÜĞÜM, UZANTI,
  PARALEL ve UZATILMIŞ KESİŞİM yazılmış, sınanmış ve programdan erişilemez
  durumdaydı.
- **OSNAP kip listesi**: durum çubuğundaki OSNAP çipine sağ tık, `Shift+F3`, ya da
  `Görünüm ▸ Yakalama Modları…`. Her satır `core::snap_mode_label`'dan gelir —
  motora eklenen bir kip listede kendiliğinden belirir (5.10) — ve her değişiklik
  `MOD ad=yakalama_modları` komutunu gönderir.
- **DÜĞÜM'ün glifi eklendi.** Kip `default`'a düşüyor ve hiçbir şey çizmiyordu:
  bir röpere yakalanıyordunuz, işaretçi tutmadığını söylüyordu. Kadastroda her
  sınır bir röperden ölçüldüğü için bu kip sıralamada en üsttedir.
- Belge: [`docs/baslangic/arayuz.md`](docs/baslangic/arayuz.md) — on iki kipin
  tamamı, öncelik sırası ve hassasiyet ayarı.

### Eklendi — KOORDİNAT komutu

- `KOORDİNAT` / `KOORDINAT` / `COORDINATE` / `KRD`: tıklanan noktanın sağa ve yukarı
  değerini belgenin koordinat sisteminde yazar. Salt okunur — geri alma yığınına
  girmez. Araç kutusundaki devre dışı "Koordinat Oku" düğmesinin yerine geçti.
- Belge: [`docs/komutlar/coordinate.md`](docs/komutlar/coordinate.md).

### Düzeltildi — çalıştığı hâlde kullanıcıya ulaşmayan iki komut

- **METİN artık tuvale yazıyor.** Komut çapayı alıp `ctx.text` ile yazı istiyordu;
  istem alttaki komut satırının yer tutucusuna düşüyor, odak tuvalde kalıyordu, ve
  kabuk metin cevabı verecek bir yola sahip değildi — komut süresiz bekliyordu.
  `Controller::supplyText` eklendi ve tuval, çapa tıklamasının hemen ardından
  tıklanan yerde bir yazı kutusu açıyor. Enter yazar, Esc vazgeçer.
- **Komut çıktısı durum çubuğunda.** Her komut `ctx.echo` ile konuşur; bu yalnız
  kullanıcının çoğu zaman kapalı tuttuğu `Geçmiş` sekmesine düşüyordu. ÖLÇ ölçüyor,
  sonucu kimsenin bakmadığı yere yazıyordu — "ölçüm araçları çalışmıyor" bu.
- **ÖLÇ ve KOORDİNAT modal araç oldu**: çalışırken araç kutusunda yanıyorlar.
- **YAY araç kutusuna eklendi.** Tam bir çizim aracı olarak kurulmuş ama sütuna
  konmamıştı; programın çizebildiği tek eğri yalnız adı yazılarak ulaşılabiliyordu.

### Değişti — stil tasarımcısı, `design.md` §8'e hizalandı

- **Özellikler dört başlık altında ve sol etiketli**: KATMAN · DOLGU · KENAR ·
  GEOMETRİ · GÖRÜNÜRLÜK. On dört satırlık düz liste, her satırda etiketi üstte
  taşıyor ve 756 px'lik pencerede üç satır gösterip gerisini kaydırıyordu.
  Şimdi etiket 110 px'lik sol sütunda, değer yanında; yaygın tiplerde hiçbir
  şey kaydırılmıyor. Boş kalan başlık gösterilmiyor.
- **Ön izleme ile sembol katmanları yan yana**, mockup'ın çizdiği gibi. Üst
  üste dururken ikisi 372 px alıyordu; şimdi listenin boyu kadar.
- **Birim alanın içinde**: `Çizgi kalınlığı (µm)` → `Kalınlık` + `µm` soneki,
  `Açı (°)` → `Açı` + `°`. Etiketler kısaldı, hiçbiri sarmıyor.
- Üst şeridin başlıkları 16 px'ten §8'in 10.5 px büyük harfine indi; şerit
  bir sekme değil, iki kontrol gibi okunuyor.
- Renk alanı en az 200 px boyanmayı bırakıp sütununun genişliğini alıyor;
  açılır kutular en uzun öğelerine değil sütuna göre daralıyor. İkisi de sağ
  kenardan taşan satırların sebebiydi.
- Belge: [`docs/baslangic/stil-tasarimcisi.md`](docs/baslangic/stil-tasarimcisi.md).

### Düzeltildi — macOS'ta `make run` çalışmıyordu

- CMake macOS'ta `.app` paketi üretir ve ikili `bin/piricad.app/Contents/MacOS/`
  altına iner; Makefile, `/docs` örnekleri ve gate'ler ise `bin/piricad`'ı arar.
  Derleme artık macOS'ta o yola göreli bir sembolik bağ bırakıyor; üç tüketici
  de değişmeden çalışıyor.
- `libpq` Homebrew'da keg-only olduğundan `find_package(PostgreSQL)` onu
  bulamıyor, PostGIS sessizce kapanıyordu; `brew --prefix libpq` ipucu eklendi.
- `qgis_backend.cpp` `PIRICAD_WITH_QGIS` kapalıyken de derleniyordu; QGIS
  başlıkları olmayan her makinede derleme kırılıyordu.

### Eklendi — gömülü Lua betik motoru (`PIRICAD_WITH_LUA`)

- **`piricad::script::LuaRunner`**, sol2 üzerinden gömülü Lua 5.4. JSON
  çalıştırıcısının yerine geçmez, yanına gelir: `BETİK` hangi motorun çalışacağını
  dosya uzantısından seçer (`.lua` → Lua, gerisi → JSON).
- **Tek yazma yolu `h.komut(...)`**, `Bus::execute_line` üzerinden — komut
  satırıyla aynı ayrıştırıcı, aynı doğrulama, aynı geri alma, aynı günlük.
  Okuma bağlantıları yalnız değer döndürür; çizime hiçbir tutamak verilmez.
- **Kum havuzu üç seviye.** `güvenli` seviyede `io`, `os`, `package`, `debug`,
  `require`, `dofile`, `loadfile` ve `load` hiç açılmaz. `proje` seviyesinde
  dosya erişimi proje dizinine hapsedilir ve yol `weakly_canonical` ile çözülerek
  denetlenir, metin öneki karşılaştırılarak değil. `tam` yalnız o betiğin metnine
  verilmiş onayla çalışır.
- **İptal edilebilir**: `std::stop_token`, 10 000 komutta bir yoklanan bir Lua
  hook'uyla; sonsuz döngü de durur.
- **Bir betik = bir geri alma adımı**; herhangi bir satırın hatası bütün bloğu
  geri alır.
- Lua 5.4.8 ve sol2 3.5.0 sabitlenmiş commit'lerden indirilir; makinede kurulu
  olmaları gerekmez. İkisi de MIT, `/NOTICE`'a işlendi.
- Belge: [`docs/betik/lua.md`](docs/betik/lua.md).

### Eklendi — günlüğün ikinci satır türü: `{kind:"meta"}`

- `Journal::append_meta()` — komut olmayan kayıtlar için (`.claude/command.md`
  R20). İlk kullanıcısı betik çalıştırmalarının kum havuzu seviyesi ve `tam`
  onayıdır (`.claude/script.md` R11, R12); eklenti kimliği ve kullanıcı kararı
  aynı satır türünü kullanacak.
- Tekrar oynatma bu satırları **atlar**, çünkü neyin yapıldığını değil neye izin
  verildiğini anlatırlar. `canonical()` de dışarıda bırakır: üç istemcinin
  bayt-birebir günlük kanıtı (CLAUDE.md 6.4) yalnız betiğin yazdığı bir satır
  yüzünden bozulmamalıdır.
- JSON çalıştırıcısı da artık bu satırı yazıyor; önceden hiçbir konak yazmıyordu.

### Düzeltildi — QRhi tuvalinde çizim çıkmıyordu

Üç kusur, üçü de ekran görüntüsünden görünmeyen cinsten; kare ikiye bölünerek
bulundu (`PIRICAD_RHI_DEBUG`).

- **`firstInstance` taşınabilir değil.** Örneklenmiş çizimlerde çizgi grupları
  `cb->draw(..., firstInstance)` ile ayrılıyordu; bu `QRhi::BaseInstance`
  gerektirir ve OpenGL ES ile ARB_base_instance'sız GL'de yoktur — üstelik
  eksikse çizim reddedilmez, sessizce yanlış olur. Karenin ilk çizgi grubu
  çiziliyor, sonrakiler kayboluyordu: ızgara yarım, kuzey oku ve ölçek çubuğu
  yok. Artık vertex buffer bayt kaydırmasıyla bağlanıyor; hiçbir GPU özelliği
  gerektirmiyor.
- **Belge çizgilerinin kaydırması örnek indeksiydi**, bayt değil. Grid'den sonra
  tampon yarım örnekten okunuyor, çizim ekran dışına düşüyordu. Belge tek başına
  çizdirilince görünüyor olması kusuru bire bir işaret etti.
- **Stil tasarımcısı GPU yapısında çöküyordu**: sembol önizlemeleri `QImage`'a
  çiziyor ama `make_canvas_backend()` GPU arka ucunu döndürünce `QPaintDevice*`
  işaretçisi `RhiFrameTarget*` diye okunuyordu. Önizlemelerin artık kendi
  fabrikası var (`make_preview_backend`).

### Düzeltildi — kare dökümü GPU tuvalini boş gösteriyordu

`QWidget::grab()` arka tampon üzerinden yürür; `QRhiWidget`'ın karesi orada
değil, GPU'dadır. `PIRICAD_FRAME_DUMP` ve `PIRICAD_SHOT_DIR` bu yüzden doğru
çizen bir tuvali boş gösteriyordu. `MapCanvas::grabCanvas()` kareyi kendi
yüzeyinden alıyor ve pencere görüntüsüne yerleştiriliyor.

### Düzeltildi — kayıtlı dock yerleşimi kabuğu bozuk gösteriyordu

`kLayoutVersion` 4'e çıktı. Tuval `QRhiWidget` tabanına geçince merkez pencere
sınıf değiştirdi ve çevresindeki dockların kayıtlı boyutları anlamsız kaldı:
Öznitelikler paneli otuz piksellik boş bir şerit olarak geri yükleniyordu, yani
sanki bütün kabuk dağılmış gibi görünüyordu. Bayat olan **durumdu**, çizici
değil — bunu ayırt etmek bir öğleden sonra aldı, çünkü kayıtlı yerleşim yeniden
derlemeden sağ çıkar ve yeni bir hata gibi görünür.

### Eklendi — GPU tuvalinde metin: SDF atlası (`PIRICAD_WITH_TEXT`)

- `render::TextAtlas` — FreeType konturu → msdfgen çok kanallı mesafe alanı →
  stb_rect_pack ile tek dokuya; HarfBuzz `tr` diliyle şekillendirme
  (`.claude/render.md` R8). Qt'siz, `/src/render` içinde: `/tests` Qt bağlamaz,
  dolayısıyla arka uç kurulamayan bir süitte bile `İ` ile `I`'nın ayrı glif
  olduğu doğrulanabiliyor.
- Ölçek bağımsız: bir 48 px hücre 8 pikselde de 200'de de keskin çıkar, çünkü
  saklanan şey piksel değil **kontur**. Üç kanalın medyanı köşeleri korur; tek
  kanallı bir alan her köşeyi yuvarlar.
- Cetvel sayıları, ölçek çubuğunun rakamları, kuzey okunun `K` harfi ve çizimin
  kendi başlıkları artık GPU'da. Döndürülmüş taban çizgisi, çok satırlı
  TAKS/KAKS etiketi ve dört çapa QPainter arka ucuyla aynı kuralları izler.
- 7 test: beş yüzün açılması, `ÇİĞDEM`'in altı harfinin altı glif olması (bayt
  sayan bir çizici dokuz üretirdi), `İ` ≠ `I`, boşluğun kalem ilerletip
  çizmemesi, mono yüzün gerçekten eşaralıklı olması, alanın gradyan taşıması,
  aynı kelimenin atlası ikinci kez büyütmemesi.
- Shader hedefleri GLES 3.0 / GL 3.3'e çekildi: qsb'nin varsayılanı ESSL 100 ile
  başlar ve orada ne `textureSize` ne türev vardır.

### Eklendi — QRhi GPU canvas'ının ilk dilimi (`PIRICAD_WITH_RHI`)

- `render::Backend`'in GPU uygulaması: poligon dolguları (stencil ile tek-çift
  kuralı, üçgenleyici bağımlılığı olmadan), shader'da genişletilen çizgiler
  (`render.md` R5) ve ızgara/seçim/imleç katmanı. Shader paketleri derleme
  anında `qsb` ile pişirilir; çalışma anında hiçbir shader derlenmez (P11).
- **Metin ve yayımlanmış raster semboller bu dilimde çizilmez.** `handles()` bir
  beyaz listedir ve tanımadığı katman türünü sahiplenmek yerine reddeder — QGIS
  arka ucunun üç raster türünü sessizce düşürmesi bu yüzden bir kapıya bağlandı.
- `MapCanvas` seçeneğe göre `QRhiWidget` ya da `QWidget` tabanlıdır; arada kalan
  her şey aynıdır.
- `scripts/doctor.sh` artık `qsb`'yi Qt'nin kendi dizinlerinde arıyor. Qt
  araçlarını hiçbir platformda PATH'e koymaz, dolayısıyla eski yoklama kurulu
  olan bir makinede "MISSING" diyordu.

### Eklendi — MPYY gösterimlerinin vektör paketi tamamlandı

Sebep: **Mekânsal Planlar Yapım Yönetmeliği (MPYY), EK-1 Gösterimler**
(RG-22/1/2026-33145; EK-1b için RG-14/6/2014-29030).

- **`data/catalogs/mpyy-vektor` 30 satırdan 467 satıra çıktı.** Yönetmeliğin 476
  gösteriminin tamamı ele alındı: 467'si çizildi, 9'u gerekçesiyle atlandı.
  Toplam 1 574 sembol katmanı ve 98 SVG çizim, 431 KB.
- **Sayılar ölçüldü, tahmin edilmedi.** Her kırpma gömülü DPI'sıyla mikrometreye
  çevrildi; çizgi kalınlığı, tekrar adımı, işaretçi aralığı, kesik/boşluk yapısı
  ve mürekkep rengi izdüşüm ölçümünden okundu. Eğik taramalarda projeksiyon adımı
  dik aralığın √2 katıdır; ölçüm bunu böler.
- **17 satırda kaynak kusuru bulundu ve yazıldı.** Yönetmelik ekinin bazı
  hücrelerine gösterim yerine program ekran görüntüsü konmuş; AYRIK, BİTİŞİK,
  BLOK DÜZEN ve KAT ADEDİ satırları aynı düz siyah lekeyle basılmış. Kusur
  satırın `kaynak_kusuru` alanına gerekçesiyle geçti.
- **Dokuz satır çizilmedi** çünkü ekin kendisi o satırların bütün gösterim
  sütunlarını boş basmıştır; gerekçeleri `ATLANANLAR.md` dosyasında.
- **`scripts/ci-gate-mpyy-vektor.py`** paketi ve iş listesini denetler; `ctest`
  ve `make check` içinden çalışır. İş listesinde işaretli ama pakette olmayan bir
  satır kusurdur.

### Eklendi — DİKDÖRTGEN komutu, poligon aracı ve köşegen kilidi

- **`DİKDÖRTGEN` (`core.rectangle`).** Karşılıklı iki köşeden dört köşeli kapalı
  alan çizer; kalan iki köşeyi program hesaplar. Elle tıklanan dört köşe
  "neredeyse" diktir, ve imzalanan bir paftada neredeyse dik bir kusurdur.
  Günlüğe iki köşe yazılır, türetilen dördü değil.
- **Araç kutusunda poligon ve dikdörtgen artık çalışıyor.** İkisi de yer
  kaplayan ama devre dışı birer `placeholder`'dı; `ALAN` komutu ise baştan beri
  vardı ve hiçbir düğmeye bağlı değildi.
- **`core.yakalama.kosegen` — köşegen kilidi.** İmleci öncekinden 45°'nin
  katlarına kilitler; dikdörtgenin ikinci köşesi böyle kilitlenince **kare**
  çıkar. Çizerken **Ctrl** basılı tutmak bu modu basılı tutar, `MOD köşegen=evet`
  aynı anahtarı yazarak açar — Ctrl bir fare hüneri değil, bir modun kısayolu
  (Article 5.15). Motorda yeni bir kısıt yok: 45° adımlı kutupsal izlemedir.
- **Kılavuz artık çizilecek şekli gösteriyor.** `Prompt` bir `RubberShape`
  taşıyor; dikdörtgen çizilirken tuval köşegeni değil **dörtgeni** önizliyor.
  Çizgi olarak önizlenen bir dikdörtgen, ne çizileceğini tıklamadan önce
  söylemez — kilit basılıyken kareyi görmekle çizdikten sonra öğrenmek arasındaki
  fark budur.

### Düzeltildi — alan gösterimleri sembolojide seçilebiliyor

İki ayrı kök neden, ikisi de aynı sonucu veriyordu: bir parsel katmanı için alan
dolgusu seçilemiyordu.

- **Geometri sekmesi katmanın değil sembolün şeklinden seçiliyordu.** Tek
  konturlu bir parsel katmanı "çizgi" sayılıp Çizgi sekmesinde açılıyor, galeri
  de çizgi gösterimlerini listeliyordu. Sekme artık katmanın **nesnelerinden**
  okunuyor: kapalı halkası olan katman alan katmanıdır.
- **Bildirilen katman yığını sınıflandırılmıyordu.** Raf, bir satırın alan mı
  çizgi mi olduğunu yalnız resimli paketin alanlarına (`tarama`, `çizgi_tipi`,
  `sembol`, alan renk kodu) bakarak karar veriyordu. Vektör paketinin 476
  satırının hiçbirinde bunlar yok — hepsi `else` dalına düşüp çizgi oluyordu.
  Artık yığın ne çiziyorsa o: dolduran katman alan, konturlayan çizgi, yalnız
  glif basan nokta. Alan sekmesi 335 gösterim listeliyor.

### Eklendi — gösterimin öteki adı da aynı satıra çıkıyor

- **`takma_adlar`.** Bir gösterim satırı, yönetmeliğin aynı kullanım için
  kullandığı öteki yazımları taşıyabiliyor; `STİL sinifla=` bir özniteliği
  kimliğe, ada **ve** takma ada göre çözüyor. MPYY EK-1e'nin 379 detay kartından
  40'ı ekteki gösterimden başka yazılmıştır (`KRUVAZİYER LİMANI` / `KRUVAZİYER
  LİMAN`); detay kataloğundan etiketlenmiş veri o satırlarda hiçbir şeye
  eşleşmiyor ve parsel varsayılan renkte, hatasız çiziliyordu.
- **13 satıra takma ad yazıldı.** Her biri yönetmeliğin kendi öteki yazımıdır.
  Bir değerin hangi gösterime ait olduğuna dair **karar** gerektiren hiçbir
  eşleme yazılmadı: yoğunluk kademeleri, EK-1e'de iki kartın birleştiği satırlar
  ve birden çok gösterime yakın duran kartlar `UZMANA.md` dosyasında gerekçesiyle
  duruyor ve imza bekliyor (Article 6.11).
- Takma ad bir gösterimin kendi adını gölgeleyemez; test bunu paketin tamamında
  sınıyor.

### Ölçüldü — çizim arka uçları ve desen dolgusunun bedeli

`PIRICAD_FRAME_TIMES=<n>` eklendi: tuvali n kez boyar, kare maliyetlerinin
ortancasını yazar ve sahne kurulumunu çizimden ayırır. `PIRICAD_FRAME_DUMP` ile
aynı kategoride geliştirici kancasıdır — kullanıcıya bakan bir özellik değildir.

576 parselli bir yaprakta (`tests/bench/sahne/`), aynı yakınlıkta:

| Sahne | QGIS | Dahili |
|---|---|---|
| MPYY gösterimli (desen dolgusu) | 57,8 ms | 58,7 ms |
| Düz dolgu | 1,23 ms | 1,07 ms |
| Sahne kurulumu | 0,065 ms | 0,067 ms |

İki sonuç:

1. **Arka uç seçimi performansla belirlenmiyor**; ikisi desenli sahnede yüzde bir
   içinde. Dahili olan üstelik deseni yanlış çiziyor — aynı satırda %88,8 mürekkep
   basıp ormanı siyah bloğa çeviriyor, QGIS %5,0 basıyor. QGIS varsayılan kalır.
2. **Asıl darboğaz desen dolgusu.** Aynı parsellerde düz dolgu 1,2 ms, desenli
   58 ms — elli kat. 576 parselde §10.1'in 16 ms bütçesi 3,6 kat aşılıyor ve
   bütçe 5 milyon poligon için konmuştu. Bu, GPU tuvalinden önce cevaplanacak
   soru: maliyet parsel başına yeniden döşemede, ve orası CPU'da da düzelebilir.

### Düzeltildi — katman özellikleri paneli form standardına çekildi

- **Etiket girdinin üstüne alındı** (design.md 16.1). Panel 352 px'lik bir sütunda
  sabit 86 px'lik sol etiket kullanıyordu: uzun etiketler iki satıra sarıp satır
  yüksekliğini bozuyor, girdi ile birim kutusu kalanı paylaşamıyor ve ikisi de
  kaydırma çubuğunun altına giriyordu.
- **Renk artık bir alan.** 30 px'lik araç düğmesine iliştirilmiş 40x18 örnek
  yerine, komşusuyla aynı genişlikte, değerin kendisiyle dolu ve onaltılığı
  üstüne yazılı bir alan. Yazı rengi parlaklığa göre seçilir. Biçim yaprağıyla
  değil **boyanarak** yapılır: bu programda tek yaprak vardır.
- **Ön izleme başlığı taşmıyordu artık.** Başlık katman adını tekrar etmiyor —
  pencerenin kendi başlığı zaten yazıyor — ve açıklama notu başlığın altına indi.
- Sütun bütçesi yeniden paylaşıldı: ön izleme ve katman ağacı kısaldı, özellikler
  iki alan yerine dördünü birden gösteriyor.

### Düzeltildi — bildirilen katman bir görseli çağırabiliyor

- **`katmanlar` içindeki `gorsel` alanı okunmuyordu.** Şema onu sayıyordu, C++
  ayrıştırıcısı sessizce atıyordu: `gorsel-cizgi`, `gorsel-isaretci` ve
  `gorsel-dolgu` katmanları resimsiz kalıyor, hiçbir şey çizmiyordu. Artık
  paketin `gorseller` tablosundan çözülüyor ve satır uygulanırken kimliğe
  dönüştürülüyor — böylece bir sınır çizgisi sayılarla yazılıp üzerine çark
  basılabiliyor.
- **Desen dolguları kendi alanını glif rengiyle boyuyordu.** `nokta-desen-dolgu`
  ve `cizgi-desen-dolgu`, `dolgu_renk` ile bütün yüzeyi doldurup glifleri onun
  içinde görünmez bırakıyordu; MPYY'nin orman ve mezarlık gösterimleri düz siyah
  blok olarak çiziliyordu. Alanı boyamak `dolgu` katmanının işidir.

### Eklendi — PostGIS veritabanı desteği

- **`VERİTABANI` komutu (`core.database`).** Bir PostGIS sunucusuna bağlanır;
  `baglan`, `kes`, `tablolar`, `katmanyaz`, `projekaydet`, `projeac`, `projeler`
  ve `projesil` işlemleri. Diğer her şey gibi komut yolundan geçer: farede olan
  betikte de vardır (Article 1.1, 1.2).
- **Katman → mekansal tablo.** `katmanyaz`, katmanı nesne başına bir satır, çizimin
  SRID'iyle bir `geom` sütunu ve tanımlı her öznitelik için bir sütun olacak
  şekilde yazar. QGIS, `ogr2ogr` ve düz `SELECT` okur. Satırlar `COPY` ile yazılır
  ve sonrasında GIST dizini kurulur; tablo eklenmez, **yerine yazılır**.
- **Proje → kayıt.** `projekaydet`, çizimin tamamını `.pcad` baytları olarak
  saklar — stil tablosu, gömülü gösterim resimleri, katman ağacı, ayarlar ve
  koordinat sistemi dâhil. Gidiş-dönüş `content_hash()` ile sınanır
  (`tests/unit/test_database.cpp`).
- **`io::PostgisStore` ve `io::DatabaseService`.** libpqxx 7.9.2 (BSD-3),
  `PIRICAD_WITH_POSTGIS` arkasında, commit SHA ile sabitlenmiş. libpqxx başlıkları
  yalnız `src/io/src/postgis.cpp` içinde, açık başlıkta pimpl arkasında (io.md
  R2/P2). Bütün yazma tek işlemde; yarıda kalan bir yazma yoktur (Article 1.6).
- **Dosya > Veritabanı… penceresi (`Ctrl+Shift+D`).** Modsuz; bağlantı alanları,
  mekansal tablolar ve kayıtlı projeler. Her düğme bir `VERİTABANI …` satırı kurup
  komut yolundan çalıştırır; pencereden belgeye başka yol yoktur.
- **Parola iki bağlantı biçiminden de siliniyor.** `command::redact_conninfo`
  tektir ve hem günlüğe yazan komut hem de ekranda gösteren pencere onu kullanır.
  İlk hâli yalnız `password=...` alanını biliyordu; libpq'nun URI biçiminde
  (`postgresql://kullanici:PAROLA@sunucu/db`) parola başka yerdedir ve olduğu
  gibi günlüğe düşerdi.
- **Dört uygulama ayarı:** `veritabani_sunucu`, `veritabani_port`,
  `veritabani_adi`, `veritabani_kullanici`. **Parola ayarı yoktur ve olmayacaktır**
  — ayar dosyası düz metindir. libpq'nun `~/.pgpass` ve `PGPASSWORD` mekanizmaları
  kullanılır; komut satırına yazılan parola günlüğe `password=***` olarak düşer.
- **[Kullanım kılavuzu](docs/komutlar/database.md).** Kurulum, parola, tablo
  şeması, arayüz, betik ve her hata mesajı.
- **Bu sürümde katman yazılır, okunmaz.** Proje için asimetri yok (`projekaydet`
  ile yazılan `projeac` ile aynen döner); katman için var. Tabloyu katman olarak
  okuyan `katmanoku`, Madde 2.9'un "read, write and edit" cümlesinin kalan yarısı
  olarak Faz 1'e kaldı ve kılavuzda gelecek zamanla yazıldı (Madde 11.8).

### Düzeltildi — stil tasarımcısı çöküyordu ve stili katmana yazmıyordu

- **Yarı kurulmuş satır kendi kendini düzenliyordu.** `refresh()`, sembol katmanı
  listesini kurarken satırı önce listeye ekleyip sonra dolduruyordu. Her `setText`,
  `setIcon`, `setData` çağrısı ayrı bir `itemChanged` yayıyor; ilki, satırın yığın
  sırası daha yazılmadan geliyordu. İşleyici bunu kullanıcı düzenlemesi sanıp
  `layers[0]`'ı okuyor, henüz kurulmamış onay kutusunu **kapalı** görüyor ve
  sembolün ilk katmanını sessizce kapatıyordu — Uygula'nın "katman varsayılanına
  döndü" demesinin sebebi buydu. Ardından `refresh()`'i yeniden çağırıyor, oradaki
  `clear()` dış döngünün elindeki satırı siliyor ve döngü **silinmiş belleğe**
  yazmaya devam ediyordu: `make run` segfault'u. ASan raporu `heap-use-after-free`
  olarak doğruladı.
  Satırlar artık listeden **bağımsız** kurulup bütün hâlde ekleniyor, liste kurulum
  boyunca sessiz ve `refresh()` kendi içinden çağrılamıyor.
- **Koruma bayrağı artık iç içe geçiyor.** `loadSelected()` işini `loading_ = false`
  ile bitiriyordu; `refresh()` onu `true` yapıp `loadSelected()`'ı çağırdığı için
  koruma, çağıranın ortasında düşüyordu. Bayrak artık **önceki değeri** geri koyan
  bir kapsam nesnesiyle tutuluyor.
- **Aynı renk düğmesi iki form satırına konuyordu.** `stroke_`, hem "Yazı rengi"
  hem "Çizgi rengi" olarak ekleniyordu; bir widget'ı tek `QFormLayout`'un iki
  gözüne koymak Qt'de tanımsızdır ve iki satır görünürlük konusunda birbiriyle
  çelişiyordu — `yazi-isaretci` katmanında renk düğmesi hiç görünmüyordu. Tek satır
  kaldı; tipe göre adı değişiyor.
- **Seçili öğesi olmayan bir açılır kutu -1 bildirir** ve bu sayı bir satır sonra
  tabloya indis olarak giriyordu. Artık kırpılıyor.

### Değiştirildi — stil tasarımcısının görünümü

- **Üst sekmeler sekmeye benziyor.** Pencere genişliğine yayılmış üç düz gri çubuk,
  ilk kararı taşıyan denetim için kötü bir görünümdü ("orada sekme olduğu bile belli
  değil"). Sekmeler artık etiketleri kadar geniş, kenarlıklı, geometrisinin küçük
  bir simgesini taşıyor ve seçili olan altındaki ön izleme panosuna **yapışıyor**.
- **Ön izleme, gösterildiği genişlikte çiziliyor.** Pencere kurulurken ölçülen 160
  piksellik etikete çizilip sonra esnetilmiyor; etiketin kendi boyut değişimi
  izleniyor.
- **Zemin birimli semboller artık görünüyor.** Ön izleme sabit 40 mm/piksel
  çalışma ölçeğindeydi; MPYY yapılaşma koşulunun 26 m'lik dairesi bu ölçekte 650
  piksel oluyor ve 44 piksellik kutuyu tamamen ıskalıyordu — satır boş çiziliyor ve
  bozuk gibi duruyordu. Ölçek artık yalnız **gevşiyor**: kâğıt birimli her ön
  izleme piksel piksel aynı kaldı.
- **Raf boşken sol taraf üç boş kutu göstermiyor,** tek cümle gösteriyor. Sembol
  katmanı listesi kaydırmadan beş satır alıyor.

### Eklendi — her ölçünün kendi birimi

- **`STİL` komutuna `boyut_birim`, `aralik_birim`, `aralik_y_birim` ve
  `kaydirma_birim`.** Tek bir sembol katmanı iki ayrımı birden taşır: bir il sınırı
  işaretçisinin **çapı** paftaya (2,6 mm), **aralığı** zemine (15 m) aittir.
  Komutun tek bir `birim`i vardı; tasarımcı ise her ölçünün yanında bir birim
  kutusu gösteriyordu ve çıkışta dördünü birine indiriyordu — yani ekranda yazan
  sembol ile çizime yazılan sembol farklı olabiliyordu. `birim`, kendi birimini
  söylemeyen ölçüler için varsayılan olarak duruyor. Tanınmayan bir birim adı
  sessizce `birim`'e düşmez; komut hangi parametrenin hatalı olduğunu adıyla
  söyleyip durur ve çizime dokunmaz.

### Eklendi — ayar sistemi, harita yardımcıları ve zengin nesne yakalama

**Yakalama: çizimde olmayan, ama çizimin ima ettiği noktalar.** Üç yeni mod, üçü de
kadastro ve imar işinin günlük hâli için:

- **UZANTI** (`1024`) — bir kenarın kendi doğrultusu, kenarın ötesinde. Köşe taşı
  kaybolmuş bir sınır, ayakta kalan kenardan yeniden kurulur; istenen nokta kenarın
  üzerinde değildir ve YAKIN oraya erişemez.
- **PARALEL** (`2048`) — önceki noktadan çıkan, bir kenara paralel ışın. Çekme
  mesafesi, yol kenarı ve ifraz hattı böyle çizilir. Önceki noktadan uzaklık
  korunur, yani yönden sonra yazılan ölçülmüş uzunluk aynen oturur.
- **UZATILMIŞ KESİŞİM** (`4096`) — iki kenarın uzatılsalar buluşacakları köşe.
  KESİŞİM burada hiçbir şey bulmaz, çünkü kenarlar gerçekten kesişmez.

Üçü de öncelik sıralamasında **YAKIN'ın da altındadır**: motorun kurduğu bir nokta,
kullanıcının elindeki gerçek bir köşeyi asla kapmaz — bu yüzden üçünü de açık
bırakmak güvenlidir. Üçü de imlecin altında olmayan bir kenardan nokta ürettiği için
`uzantı_çarpanı` tercihi açıklığın kaç katı ötesine bakılacağını söyler; `0` yazılırsa
maskede açık olsalar bile çalışmazlar — motorun `grid_step` ve `polar_step` için zaten
tuttuğu sözleşmenin aynısı. İşaretleri **açık** biçimlerdir (uçları birleşmeyen
şekiller), böylece kurulmuş bir nokta bir bakışta gerçek bir köşe sanılmaz.

`core::line_intersection` ve `core::closest_point_on_line` eklendi; `segment_intersection`
artık birincinin üzerine yazılıyor, yani iki kod yolu bozuk girdide ayrışamaz.

**DÜĞÜM yazıldı ve aynı gün geri alındı.** Bu belge modeli tek noktalı nesne tutamıyor:
açık halka en az iki tepe ister (model.md R9-R12), `İÇEAKTAR` nokta katmanını "bu sürüm
çizgi ve alan okur" diyerek atlıyor ve nokta çizen komut yok. Var olmayan bir şeye oturan
yakalama modu, programın tutmadığı bir sözdür. 9. bit boş bırakıldı ve `test_snap.cpp`'de
sebebini sabitleyen bir vaka var: nokta nesneleri geldiğinde önce o vaka değişir.

**Yirmi iki yeni ayar.** Uygulama kapsamında: yakalama işaretinin boyu, rengi, ipucu ve
uzantı çarpanı; ızgara rengi, ana çizgi rengi ve ikinci eksen adımı; cetvelin
görünürlüğü, kalınlığı ve birimi; ölçek çubuğu, kuzey oku, koordinat göstergesi, imleç
biçimi ve boyu, yakınlaştırma adımı, ters tekerlek; seçim ve vurgu renkleri. Proje
kapsamında üç tane, üçü de belgenin kendi sayılarının nasıl okunacağını söylediği için:
**plan ölçeği** (1:N), **açı birimi** (varsayılan GRAD — Türkiye'de nirengi, poligon ve
aplikasyon hesapları grad ile yürür) ve **alan birimi** (metrekare / dekar / hektar).
`tuval_arkaplanı` da birimini `0xAARRGGBB` olarak bildiriyor artık, yani renk olduğunu
kendisi söylüyor.

**Harita yardımcıları.** Tuvale cetvel (üstte ve solda, 1-2-5 merdivenine oturan
rakamlarla), ölçek çubuğu, kuzey oku ve koordinat göstergesi eklendi. Gösterge, bir
yakalama tuttuğunda **yakalanmış** noktayı yazar: tıklamanın üreteceği koordinat odur.
Nişan imleci artık tam ekran, kısa ya da kapalı olabiliyor; tekerlek adımı yüzde olarak
ayarlanıyor ve ters çevrilebiliyor.

**Ayarlar penceresi (`Düzen > Ayarlar…`, Ctrl+,).** Tamamı ayar kataloğundan üretilir:
satırın adı ayarın birincil adı, alanı bildirilen tipinden, sınırları aralığından,
ipucu özetinden. Kataloğa eklenen bir ayar pencereye kendiliğinden düşer — CLAUDE.md
5.10'un komut listesine koyduğu kuralın aynısı, aynı gerekçeyle: kendi kopyasını taşıyan
bir pencere, kataloğla er geç ayrışacak ikinci bir listedir. Üç sekme üç kapsamdır ve
her sekme kapsamının ne demek olduğunu kullanıcının kendi diliyle yazar. Her satırda
değerin kimin olduğu (`ayarlanmış` / `varsayılan`) ve varsayılana döndüren bir düğme
var; birimi `0xAARRGGBB` olan ayarlar renk seçici alır.

Satır etiketleri **okunmak için** yazılır, yazılmak için değil: `ızgara_adımı`
bildirimi pencerede `Izgara adımı` olur — ayarın kendi adı, alt çizgisi boşluğa
çevrilmiş ve ilk harfi Türkçe kurallarıyla büyütülmüş (`ızgara` → `Izgara`,
`imleç` → `İmleç`; ASCII bir sınıflandırıcı ikisini de yanlış yapar, CLAUDE.md 5.6).
Etiket ayrıca yazılmaz, addan **türetilir** — ikisi ayrışamasın diye. Yazılacak ad ve
makine kimliği ipucunda durur, çünkü pencerenin ikinci bir işi vardır: burada bir
ayarı bulan kişi onu ayarlayan satırı da yazabilmelidir. Arama kutusu etiketi, her
takma adı, kimliği ve açıklamayı birden tarar.

Grup başlıkları da Türkçedir. Kimlikler ASCII olduğu için `core.cizim` başlığı `Cizim`
diye okunurdu; başlıklar bir tablodadır, tıpkı yakalama işaretinin şekli gibi
(`map_canvas.cpp`) — ikisi de üründe verilmiş kararlardır ve türetilecekleri bir
bildirim yoktur. Tabloda satırı olmayan bir grup yine de okunur bir başlık alır.

`ızgara_görünür` ve `ızgara_dikey_adımı` artık birincil adlar; eski `ızgara` ve
`ızgara_adımı_y` takma ad olarak duruyor, yani yazılmış hiçbir betik bozulmadı.

Penceredeki her değişiklik
kapsamına göre `AYAR`, `TERCİH` ya da `MOD` komutu kurup çalıştırır — transkript, günlük
ve yeniden oynatma pencereden yapılanı komut satırından yazılandan ayırt edemez.

### Eklendi — EK-1a arazi kullanımı gösterimleri, ve satırın kendi resim boyutu

**Altı gösterim çizildi:** ORMAN ALANI (üçgen), ZEYTİNLİK (daire), MERA (artı), DOĞAL
KARAKTERİ KORUNACAK ve DOĞAL VE EKOLOJİK YAPISI KORUNACAK (çim demeti SVG), EKOLOJİK
ÖNEME SAHİP (mercan SVG). İlk üçü mevcut işaretçilerle, son üçü elle çizilmiş SVG ile.

- **Satır artık kendi resminin boyutunu bildirebiliyor** (`boyut`, `tarama_boyut`,
  `sembol_boyut`, `tarama_aralik`). Boyut kodda sabitti ve her satır aynısını alıyordu.
  Bu, taranmış bir kırpma için doğru — boyutu bir şey ifade etmez; **çizilmiş** bir
  sembol için yanlış, çünkü orada boyut çizimin parçasıdır.
- **`gorsel-dolgu` artık aralık okuyor.** Bir fırça resmini uç uca döşer, ki taranmış
  bir tarama için doğrudur: kırpma zaten ekin bastığı aralığı içerir. Çizilmiş bir glif
  için yanlıştır — kendi kutusunu doldurur, uç uca döşenince desen katı bir hasıra
  döner. Resim artık daha büyük saydam bir hücreye yerleştiriliyor ve döşenen o hücre.

**QGIS çeviri hatası:** `Cross` ↔ `Cross2` ters eşlenmişti. QGIS'te `Cross` dik artı,
`Cross2` döndürülmüş çarpı; adların benzerliğine göre eşlemek MERA ALANI'nın artı
ızgarasını çarpı ızgarası olarak çizdiriyordu.

Rafta şu an: 419 resimli, **30 vektör yığın**, 28 düz.

### Düzeltildi — stil düzenleyicisinde bir gösterim seçince form eksik kalıyordu

Galeriden bir satır seçilince form "bozuluyordu": `Görsel işaretçi` katmanı yalnız Boyut
ve Saydamlık gösteriyor, Açı satırı hiç görünmüyordu.

Sebep, iki listenin birbirinden habersiz olması. Düzenleyici hangi satırı göstereceğine
yanlarında elle yazılmış bir tipler tablosundan karar veriyor; boyayıcı hangi özelliği
okuyacağına kendi `switch`inden. İkisini eşleşik tutan hiçbir şey yoktu ve ayrıştılar:
`gorsel-dolgu` fırçasını `angle_udeg` ile döndürüyor ve pencerede o satır yoktu — yani
**boyayıcının okuduğu bir değere kullanıcı ne bakabiliyor ne değiştirebiliyordu.**

- Tablo, boyayıcının gerçekten ne okuduğuna **bakılarak** düzeltildi: `gorsel-dolgu`,
  `gorsel-cizgi`, `gorsel-isaretci` ve `merkez-isaretci` artık açı satırını görüyor.
- **`faz`** satırı da eklendi. Motora geçen tur eklenmişti ama pencerede yoktu.
- **`ci-gate-designer.sh`** eklendi: boyayıcının her katman tipi için okuduğu özellik,
  düzenleyicinin o özelliği yöneten listesinde de olmak zorunda. Kapının yakaladığı, bir
  tip listeden geçici olarak çıkarılıp doğrulandı.

### Düzeltildi — arayüz vektör paketi yüklemiyordu, hep resim çiziyordu

Bir önceki turda motor tarafını düzelttim ve "476 satırın 464'ünde iki motor aynı
çiziyor" dedim. Ölçüm doğruydu ama **yanlış soruya** cevap veriyordu: ikisi de RESİM
çiziyordu. Kullanıcının gördüğü buydu.

Sebep: `core.stil.kutuphane` tek bir paket adı alıyor ve arayüz yalnız onu yüklüyordu.
O paket yönetmeliğin kendi paketi — **476 satırının 439'u resimli, sıfırı vektör.**
Elle çizdiğim vektör satırları ayrı bir pakette duruyordu ve rafa hiç girmiyordu.

- **`core.stil.vektor` ayarı eklendi.** Arayüz iki paketi SIRAYLA yüklüyor: önce
  yönetmeliğin resimli paketi, sonra vektör paketi. Raf her kimlikten bir satır tutar
  ve aynı kimliği yeniden bildiren paket öncekinin yerine geçer — yani vektörü çizilmiş
  bir gösterim vektör olarak, çizilmemiş olan ekin resmiyle görünür ve **hiçbiri
  eksilmez**.
- Tek ayara iki yol sığdırmayı denedim ve **olmadı**: bir metin ayarı 48 bayt alır, iki
  yol 82 bayt. `text_value` sessizce boş bir değer döndürüyor, ayar da "evet/hayır" tipi
  sanılıp kayıt sırasında reddediliyordu. `builtin_setting_failures()` bunu söyledi.
  Ayrı ayar doğru çözüm.

Ölçüldü: açılışta rafta **422 resimli, 27 vektör yığın, 28 düz** satır var. Vektörlerin
azlığı beklenen — 476 satırın 12'si çizildi.

### Düzeltildi — QGIS motoru raster gösterimleri sessizce atlıyordu

Semboloji "komple bozuk, sadece çizgi çiziyor ve sadece rengi değişiyor" hâline geldi.
Sebebi: `QgisBackend::handles()` **çizemediklerini** listeliyor, gerisini "evet"
sayıyordu. Yani hiç öğretilmemiş her katman tipi sessizce sahipleniliyor ve sessizce
atlanıyordu. Üç raster tipi — `gorsel-cizgi`, `gorsel-dolgu`, `gorsel-isaretci` — o
kapıdan çıktı, ki MPYY'nin yayımladığının neredeyse tamamı bunlar: taramalı bir lekesi
düz renk, yayımlanmış bir çizgi tipi düpedüz çizgi olarak çıkıyordu.

Liste artık **beyaz liste**: motorun çizebildikleri sayılıyor, gerisi `default`'a düşüp
reddediliyor ve kare onu çizebilen motora gidiyor. Bir beyaz liste bu şekilde
başarısız olamaz — çevirisi yazılmamış bir tip sahiplenilemez.

Ölçüldü: paketin **476 satırının 464'ünde iki motor birebir aynı mürekkebi koyuyor**,
ayrılan satır yok.

`ci-gate-backends.sh` genişletildi: `handles()`'ın `default` dalının **reddetmesi**
şart. Kapının yakaladığı, dal geçici olarak "kabul et"e çevrilip doğrulandı.

### Düzeltildi — QGIS motoru bindirmeyi hiç çizmiyordu

Izgara, cetvel, ölçek çubuğu, kuzey oku, yakalama işareti, nişan imleci, seçim kutusu
ve çizimin kendi yazıları **tuvalden tamamen kayboldu** — QGIS motoru varsayılan olduğu
anda. Çizim duruyordu, etrafındaki her şey gitmişti.

Sebep: `QgisBackend::render`, bindirmeden yalnız arkaplan rengini okuyordu. Bunları
yazarken atladım ve hiçbir şey fark etmedi, çünkü **bindirmenin hiç testi yoktu**.

- `paint_frame_aids()` ortak fonksiyon oldu ve iki motor da onu çağırıyor. İkinci bir
  kopya, birbiriyle uyumlu tutulacak ikinci bir liste demekti (5.10'un itirazı).
- **`ci-gate-backends.sh`** eklendi: `render::Backend` uygulayan her dosyayı **bularak**
  (listeleyerek değil — listeye eklenmeyi unutulan bir motor, kimsenin denetlemediği bir
  motordur) bindirmeyi çizip çizmediğine bakıyor. Kapının gerçekten yakaladığı, çağrı
  geçici olarak silinip doğrulandı.

Testte değil kapıda, çünkü `/tests` Qt bağlamıyor ve bir arka uç tanımı gereği Qt'dir
(Madde 3.4 `piricad_render`'ı Qt'siz tutuyor, bu yüzden iki motor da `/src/app` içinde).

### Eklendi — işaretçi çizgide FAZ

`faz`, çizgi boyunca ilk işaretçiye kadar olan mesafe. Verilmezse aralığın yarısı, ki
eski davranış budur — yazılmış hiçbir çizim başka türlü çizilmiyor.

Bunun ne işe yaradığı MPYY'nin kendi ekinin ilk sayfasında iki kez görünüyor.
**ETAPLAMA SINIRI** dolu ve boş daireyi sırayla dizer: aynı aralıkta iki işaretçi
çizgisi, ikincisi yarım adım ileride. **ÜLKE SINIRI** kalın bir çubuğun iki **ucuna**
dik birer tik koyar: aynı aralıkta iki tarak çizgisi, biri çubuğun başında öteki
sonunda. Faz olmadan iki katman da aynı yere düşüyor ve sembol söylediğinin yarısını
kaybediyordu — bir önceki turda ikisi de "eksik" diye işaretlenmişti, artık çiziliyorlar.

Faz `SymbolLayer`'da, dosyada (`kBlkSymbolLayerPhase`, isteğe bağlı blok — faz
kullanmayan bir çizim bayt birebir eskisi gibi yazılıyor), `STİL faz=` parametresinde ve
katalog satırında.

İki kayıp daha bulundu ve kapatıldı: faz `pass_of`'a hiç ulaşmıyordu (biçimlendirme
sonrası satır kaydığı için düzenlemem tutmamış), ve QGIS işaretçisinde dolgusu sıfır
olan bir daire çizgi rengiyle doldurulyordu — dolu/boş ayrımı kayboluyordu. `STİL`'in
`has_pictures` denetimi de `has_symbol` oldu: bildirilmiş katmanı olup resmi olmayan bir
satır düz renk yoluna düşüyordu.

### Eklendi — MPYY yapılaşma koşulu gösterimi, parselin kendi sayılarıyla

Yönetmeliğin bastığı gösterim: içinden yatay bir çizgi geçen çember, üstte **kat alanı
katsayısı**, altta **taban alanı katsayısı**. İkisi de parselin özniteliğidir, yani aynı
sembolü taşıyan iki parsel farklı sayılar gösterir.

- **`ETİKET kaydirma=`** eklendi. Bu olmadan iki sayı da nesnenin ortasına, yani
  aralarındaki çizginin üstüne düşüyordu — resmi çekince görülüyor. Her sayının kendi
  `ETİKET` satırı ve kendi kaydırması var; sabit kelimeleri yazan iki `yazi-isaretci`
  katmanının kaydırma taşımasıyla aynı biçim.
- **İş bilinçli olarak ikiye bölünüyor.** Sembol çemberi ve çizgiyi çizer — bunlar
  hiçbir parsel hakkında bir şey söylemez, bu yüzden kaç parsel taşırsa taşısın stil
  sütununda **tek kayıttır** (testte doğrulanıyor). `ETİKET` sayıları yazar ve her sayı
  sıradan bir yazı nesnesi olur: taşınır, yeniden stillenir, kendi katmanında kapatılır,
  `.pcad` ve DXF'e olduğu gibi gider.
- **Sembolün kendisi özniteliği okumuyor** ve okumayacak: `.claude/model.md` R29
  "öznitelik sütunları çerçeve yolunda asla okunmaz" ve P7 "çerçeve yolunda asla ifade
  değerlendirilmez". Kare başına nesne başına bir sütun araması, 16 ms bütçesinin içine
  bir tablo araması koymak demektir. Karşılığında kaybedilen şey söylenmelidir:
  **etiket, sonradan değişen bir özniteliği takip etmez**; komutu yeniden çalıştırmak
  onları tazeler.

### Düzeltildi — veri paketleri dağıtımla birlikte gitmiyordu

Kurulum yalnız ikili dosyayı kuruyordu. MPYY gösterimleri, TM3 dilim tablosu ve `/data`
altındaki her şey — yani bu programı bir çizim düzenleyicisi değil bir Türkiye planlama
programı yapan şeyler — derlendiği makine dışında **hiçbir yerde** yoktu. Üstelik
`core.stil.kutuphane` ayarının varsayılanı `data/catalogs/...` diye **göreli** bir yol
ve göreli yol çalışma dizinine göre çözülür; yani tam da bulunmayacağı yere.

- `install(DIRECTORY data/ ...)` eklendi; paket `share/piricad/data` altına gidiyor.
- **`app::data_root()`** sırayla bakıyor: `$PIRICAD_DATA`, `<exe>/../share/piricad/data`,
  `<exe>/data`, sonra yapılandırıldığı kaynak ağacı. Bir dizin ancak içinde gerçekten
  `catalogs` varsa kabul ediliyor — yarım kurulmuş bir ağacı bulmuş saymak, taze bir
  makinede sessizce boş raf demektir. Derleme zamanında gömülü bir yol değil, çünkü
  paket başka makinede kurulur, taşınır, taşınabilir dizinden çalıştırılır.
- Kurulup ilgisiz bir dizinden çalıştırılarak denendi.

### Eklendi — QGIS semboloji motoru bağlandı

Madde 2.7 ve 5.16: olgun, mükemmel, çok platformlu bir kütüphane kullanılır, yeniden
yazılmaz. Semboloji motoru tam olarak böyle bir şeydir ve QGIS'inki bu alandaki en iyi
özgür motordur — gerçek yerleşim kurallarıyla işaretçi çizgileri, kendi kaydırma ve
dönüklüğü olan çizgi/nokta desen dolguları, parametre yerine koymalı SVG semboller,
gradyan, shapeburst. `painter_backend.cpp`'deki elle yazılmış hâl bunların hepsinde
daha kötü.

**Bağlamama gerekçesi ölçülmeden yazılmıştı; ölçtüm:** `libqgis_core.so` 45 MB ve 246
paylaşımlı nesne, `QgsApplication::initQgis()` **soğuk 517 ms, sıcak 44 ms**. Madde
7'nin iki saniyelik açılış bütçesinin rahat içinde — eski itirazın "bunu kırar" dediği
sayı buydu. Lisans da engel değil: QGIS **GPL-2.0-or-later**, GPLv3 ile uyumlu (yalnız
GPL-2.0-**only** olsaydı Madde 5.5 gereği reddedilirdi).

- **`app::QgisBackend`**, `render::Backend` arayüzünün arkasında. Dikiş orası ve başka
  yer değil: Madde 3.4 `piricad_render`'ı Qt'siz tutuyor, QGIS ise Qt — bu yüzden dosya
  `QPainter` arka ucunun yanında `/src/app` içinde, tam da Madde 8.5'in tarif ettiği
  gibi. Kabuğun altındaki hiçbir katman QGIS'in var olduğunu öğrenmiyor.
- **Çizim listesi sözleşme olarak kalıyor.** Her `PassStyle`, aynı anlama gelen QGIS
  sembol katmanına çevriliyor; geometri, `QPainter` arka ucunun aldığı ekran uzayı
  yığınlarının aynısı. İki motor aynı belgeyi aynı sayılardan çiziyor — karşılaştırmayı
  mümkün kılan şey bu. `PIRICAD_BACKEND=dahili` ile yan yana bakılabiliyor.
- **Sistemden alınıyor, vcpkg'den değil:** QGIS altında GDAL, PROJ, GEOS ve SpatiaLite
  olan bir masaüstü yığını; onu manifestten kurmak QGIS'i kurmak olurdu.
- **QGIS başlıkları `SYSTEM` olarak dâhil ediliyor.** Bu bir susturma değil (CLAUDE.md
  5.14): derleyiciye hangi başlıkların *bizim* olduğunu söylüyor. QGIS'in kendi
  başlıklarındaki dönüşüm ve gölgeleme uyarıları bu depodaki hiçbir düzenlemeyle
  giderilemez, ve bir uyarı duvarı kendi kodumuzdaki gerçek bir bulgunun kaydırılıp
  geçilme biçimidir.

Çeviride bir hata çıktı ve ölçümle yakalandı: kesik deseni sayıları çizgi kalınlığının
katıdır, QGIS'in `setCustomDashVector`'ü ise verildiği birimde uzunluk ister. Ham
sayıları piksel diye vermek sekiz kalınlıklık çizgiyi sekiz piksel çiziyordu — iki motor
yan yana konunca görülüyor.

### Eklendi — gösterimler SVG olabiliyor

- **`ImageStore` SVG tanıyor**, uzantıdan değil imzadan: `<svg` kökü aranıyor, prolog ve
  yorum toleranslı, ilk 1 KB'la sınırlı (düşmanca bir dosya megabaytlarca yorumla
  gelmesin).
- **Boyayıcı SVG'yi `QSvgRenderer` ile çiziyor** — QGIS'in de SVG için kullandığı motor.
  **Çizileceği boyutta** rasterleştiriliyor ve önbellek anahtarı o boyutu da içeriyor:
  rasterin tek çözünürlükte açılıp her yakınlaştırmada yeniden örneklenmesi, mevzuatın
  keskin çizdiği çizgiyi her seferinde biraz daha yumuşatan şeydi.
- **`data/catalogs/mpyy-vektor/` paketi kuruldu.** Çıkarılan pakete karıştırılmadı:
  elle çizilen şey kaynaktan yeniden üretilemez ve `ci-gate-mpyy` bunu haklı olarak
  denetliyor. İzin belgesi satırı **türetilmiş eser** diyor, her satır `belirsiz: true`
  ve `cizim-yorumu` ile işaretli, ve uzman onayı olmadan pakete bir plan uygulanmıyor.
  İlk dört sembol çizildi ve tuvalde doğrulandı.

**Otomatik izleme denendi ve bırakıldı.** Tarama karolarını Radon izdüşümü ve bağlı
bileşen analiziyle okuyup açı/aralık/kalınlık çıkarmayı denedim; okumayı geri çizip
aslıyla yan yana koyunca **altı örnekten ikisi** doğru çıktı. Kalınlık 104 piksel
okunup siyah blok çiziliyor, JPEG'de parçalanmış daire konturu 2×1 glif sanılıyor,
seyrek bir sembol 3 piksellik kafes okunuyor. Aile sınıflandırması (tarama / nokta
deseni) altıda beş doğru ama **sayılar katalog kalitesinde değil** — ve yanlış bir açı
imzalanan bir plana yanlış gösterim yazar. Çıkarıcının kendi doktrini uydurmayı
yasaklıyor; bu yüzden semboller **çiziliyor**, izlenmiyor.

### Eklendi — çizgi tipi artık bir desen, resim değil

MPYY il sınırını bir çizgi, bir boşluk, bir nokta ve bir boşluk olarak basar. Bu dört
sayıdır; program onu JPEG kırpması olarak taşıyordu ve bir resmin veremediği her şeyi
kaybediyordu — yeniden renklendirilemez, yeniden ölçeklenirken yeniden örneklenir,
DWG/DXF/GML'e çizgi tipi olarak yazılamaz ve hepsinden önemlisi **köşe dönemez**.
Damgalanan resim katı bir dikdörtgendir; bir kenarın açısına döner ve her kıvrımın
dışında kama biçiminde bir boşluk bırakır.

- **`core::DashStore`.** Çizimin taşıdığı çizgi tipleri, içerikle tekilleştirilmiş —
  `ImageStore` ile birebir aynı biçim ve aynı gerekçe: desenler **çizimin içinde
  gider**. `Appearance.dash` zaten "desen tablosuna indeks" diye bildirilmişti; eksik
  olan tablonun kendisiydi. Yalnız katalog paketinde yaşayan bir tablo, paketin kurulu
  olmadığı bir bilgisayarda paftanın başka çizilmesi demekti — pafta hukuki bir belge.
- **Birim, çizginin kendi kalınlığıdır.** Desen kalınlığın katı olarak saklanır; bu, tek
  bir tanımın 0,2 mm'de de 1,0 mm'de de doğru kalmasını sağlar ve `QPen::setDashPattern`
  zaten bu birimi ister. Ekin bastığı örnek de bunu söyler.
- **`STİL desen=` parametresi bağlandı.** Bildirilmiş ama kullanılmıyordu.
  `desen="8 1 1 1"` kesik-noktalı, `desen=sürekli` düz. Tek sayıda parça, sekizden çok
  parça ve sayı olmayan bir sözcük **reddedilir** — hiçbiri sessizce düz çizgiye
  dönmez, çünkü düz çizilen bir sınır paftada farklı bir hukuki beyandır.
- **Dosya formatına `kBlkDashes` bloğu.** İsteğe bağlı olduğu için sürüm yükseltmesi
  değil (io.md R10): çizgi tipleri var olmadan yazılmış her dosya boş tabloyla okunur ve
  içindeki her çizgi düz kalır — zaten öyleydi.
- **Desenli çizgi düz uçla çizilir.** Qt ucu her çizgi parçasına uygular; yuvarlak uçta
  her parça iki ucundan yarım kalınlık uzar ve bir kalınlık genişliğindeki boşluk tamamen
  kapanır. Yayımlanmış kesik-noktalı bir sınır **düz çizgi olarak** çıkıyordu. Bildirilen
  uç biçimi çizginin iki gerçek ucunu anlatır, içindeki her parçayı değil.

Beş yeni test: desenin çizime yazılması, tekilleştirme, `sürekli`, bozuk desenin
reddi ve çizime dokunmaması, dosya gidiş-dönüşü ve desensiz eski dosyanın okunması.

### Düzeltildi — damgalanan gösterimlerde kâğıt lekesi ve köşe deliği

- **JPEG'in kâğıdı artık çözümleme anında saydamlaşıyor.** Damgalar çarpma kipiyle
  çiziliyordu; çarpma beyazı olduğu gibi bırakır ama JPEG'in beyazı 255 değil ~250'dir
  ve her çizginin çevresinde halkalanma vardır — ekrana ulaşan şey, her damganın altında
  **soluk gri bir kutu** oldu. Alfa artık pikselin kendisinden geliyor:
  `alfa = 255 - min(r,g,b)`. **Süreklidir**, yani hangi grinin mürekkep olduğuna dair bir
  karar vermez — eşiklemeye yapılan haklı itiraz buydu. En küçük kanala bakması, doygun
  bir rengin opak kalmasını sağlar: MPYY'nin kırmızı sınır noktaları yarı-koyu sayılmak
  yerine tam güçte kırmızı kalır. Damga artık yalnız koyulaştırmıyor, **boyuyor** — koyu
  bir dolgu üzerine beyaz bir glif bunu gerektirir.
- **Damgalar halkanın tamamı boyunca, yay uzunluğuyla yürüyor.** Önceki hâl her kenarı
  ayrı yürüyor, iki ucunda yarım damgalık pay bırakıyor ve fazı her köşede sıfırlıyordu.
  Üçü de tek başına savunulabilirdi; birlikte, kullanıcının bildirdiği resmi ürettiler:
  **her parselin her köşesinde bir delik**, kenardan kenara değişen bir aralık, ve
  kenarları bir damgadan kısa olan bir sınırda **hiçbir şey**. Artık adım bütün koşu için
  bir kez seçiliyor, köşe yürüyüş için özel bir yer değil.

**Kalan kusur bitmap'in kendisindedir.** Altmış piksel eninde katı bir dikdörtgen köşe
dönemez; damga bir kenarın açısına göre döner ve dönüşün dışında bir kama boşluk kalır.
QGIS'in raster çizgi sembollerinde de aynı sınır vardır — QGIS kesik-noktalı çizgi için
raster kullanmaz, vektör kesik deseni kullanır ve köşeyi gerçek bir birleşimle döner.
`Appearance.dash` alanı çizimde **vardır ve bağlı değildir**: boyayıcı, ölçülü segment
uzunlukları yerine sabit bir `Qt::PenStyle` dizisini vekil olarak kullanıyor. Eksik olan
yarı budur.

### Düzeltildi — MPYY gösterimleri artık mevzuatın bastığı gibi çiziliyor

- **Kâğıt milimetresi bir ekran pikseli sayılıyordu.** `render/scene.cpp` içindeki
  `kPixelsPerPaperMm = 1.0` — dosyanın kendi yorumunda "PLACEHOLDER" diye
  işaretliydi. Mevzuatın 8 mm bastığı bir gösterim tuvale **8 piksel** olarak
  geliyordu; 96 dpi'da 8 mm otuz pikseldir. Kâğıt birimli her şey yaklaşık dört kat
  küçüktü. Çizgi kalınlığı da aynı yerden geliyordu: 0,5 mm'lik bir sınır yarım
  piksel istiyor, tabandan 1'e yuvarlanıyordu — yönetmeliğin 0,2 / 0,5 / 1,0 mm
  ayrımı tek bir saç teline çöküyordu. Çözünürlük artık `SceneOptions`'tan geliyor
  ve `MapCanvas` onu bulunduğu ekrandan okuyor; QGIS de render bağlamının DPI'ını
  aynı şekilde kullanır.
- **Tarama karosunun beyaz kâğıdı, satırın dolgu rengini siliyordu.**
  `drawRasterFill`, diğer iki raster yolunun (`drawRasterAlong`,
  `drawRasterCentres`) kullandığı çarpma kipini kullanmıyor, düz doku fırçasıyla
  boyuyordu. MPYY görselleri JPEG olduğu için alfası yoktur ve opak beyaz üstünde
  gelir; sonuç, MEVCUT KONUT ALANI'nın kahverengi yerine bembeyaz çıkmasıydı.
  **Paketin 476 satırından 277'si bu yoldan geçiyor.**

### Değiştirildi — ön izleme sembolü kutuya sığdırıyor

- **Tek yakınlaştırma, iki birim ailesine birden.** Ön izleme yalnız küçültür: kutuya
  zaten sığan bir sembol tuvaldeki ölçeğiyle çizilir, ki bir örneklik ancak o zaman
  çizim hakkında bir söz olur. İkisine birden, çünkü kâğıt ve zemin ölçülerini
  karıştıran bir sembolün oranları yalnız birini küçültmekle bozulurdu.
- **Görselin en-boy oranı hesaba katılıyor.** Damgalanan bir çizgi tipi bildirdiği
  boyut kadar değil, kendi resmi kadar geniştir (MPYY sınır görselleri iki-bire
  yakın) ve bir damganın parçaya sığıp sığmadığına **eni** karar verir;
  `render::distribute_along`, damgadan kısa bir kenara hiç damga koymaz. Oran
  `QImageReader` ile yalnız başlıktan okunur, çözme maliyeti yoktur.
- **Dar bir örneklikte zikzak düzleşiyor.** Zikzak, desenin köşede ne yaptığını
  göstermek için vardır ve büyük ön izlemede yerini hak eder; 44 piksellik bir liste
  simgesinde ise koşuyu üç güdük parçaya bölüyor ve hiçbiri yayımlanmış bir çizgi
  tipinin tek damgasını taşıyamıyordu — simge boş çıkıyordu. 120 pikselin altında
  örneklik köşeyi değil deseni gösteriyor.

### Düzeltildi — iki yüzlü parsel artık delikli parsel sanılmıyor

- **Çok parçalı yüz `MULTIPOLYGON` olarak yazılıyor.** Nesnenin halkaları düz bir
  listedir ve onları gruplayan şey rolleridir. Kodun ilk hâli listeyi "ilk halka
  sınır, gerisi delik" diye okuyordu; **yolla ikiye bölünmüş bir parselin ikinci
  yüzü delik oluyordu**. Sonuç, alanları yanlış olan ve buna rağmen `ST_IsValid`
  dâhil hiçbir denetimin şikâyet etmediği bir tablo. Böyle bir parsel KentOSCad'e
  `İÇEAKTAR` ile, TKGM'den gelen bir GeoPackage'ın `MULTIPOLYGON` kaydı olarak
  girer; yani hata canlıydı. Açık halkalar için `MULTILINESTRING` de aynı anda
  eklendi.
- **`io::entity_ewkb` açık başlığa çıkarıldı.** Sunucu gerektirmeyen saf aritmetik
  olduğu için PostGIS kapalı derlenmiş bir yapıda da derleniyor ve sınanıyor —
  doğru olması gereken parça, çalışan bir veritabanı isteyen bir testle
  korunamaz.
- **`PIRICAD_WITH_POSTGIS=OFF` yapısı derlenmiyordu.** `postgis.cpp` koşulsuz
  olarak `<pqxx/pqxx>` içeriyordu. Artık `vector.cpp`'nin GDAL için kullandığı
  kalıpta: bağlantı yarısı korumalı, kodlama yarısı her yapıda derleniyor,
  `PostgisStore` her giriş noktasında desteğin kapalı olduğunu söylüyor.

### Düzeltildi — tırnak içindeki değer artık gerçekten değişmez

- **Tırnaklı bir değer ikinci kez ayrıştırılıyordu.** `hedef="host=localhost
  dbname=x"` yazıldığında ayrıştırıcı, tırnakların kaybolduğunu unutup değeri
  kendi `=` işaretinden yeniden bölüyor ve komut "metin bekliyor" diyerek
  reddediyordu. Aynı hata `=` içeren her yol, katman adı ve biçim dizesini de
  vururdu. Tek dilbilgisi (CLAUDE.md 5.11) artık tırnaklı bir değeri **harfi
  harfine** alıyor: kendi `=` işaretinden bölünmez, virgül taşıyor diye koordinat
  sanılmaz, sayıya benziyor diye sayıya çevrilmez.
- **Tırnak sınırlar, tür değiştirmez.** `ÖLÇEK "500"` artık `ÖLÇEK 500` ile aynı
  şeyi, `gorunur="evet"` de `gorunur=evet` ile aynı şeyi yapıyor. Sayı,
  ayrıştırıcının kendi ifade değerlendiricisiyle okunuyor — ikinci bir sayı
  ayrıştırması eklenmedi.

### Eklendi — dosya açma ve kaydetme

- **`piricad_io` modülü.** Biçim okuma-yazmanın tamamı `/src/io` altında; Qt yok,
  GDAL başlıkları yalnız `.cpp` dosyalarında, dışa açılan başlıklarda yalnız core
  ve command tipleri (`.claude/io.md` R1–R3, P2).
- **Yerel proje biçimi `.pcad`.** Sütunlu (SoA), 8 bayt hizalı, `u64` konumla
  adreslenen, belleğe eşlenebilir tek dosya. Koordinatların tamamı `int64`
  milimetre; dosyada hiçbir yerde ondalıklı sayı yok (io.md R5, R7; model.md R21).
- **Sürümleme ve ileri uyumluluk.** İlk 32 baytta imza, yazan sürüm ve gereken en
  düşük okuyucu sürümü. Tanınmayan blok uzunluğuna bakılarak atlanır ve ölümcül
  değildir; okunamayacak kadar yeni bir dosya, gereken sürümü söyleyerek
  reddedilir ve yarım yüklenmez (io.md R8, R9, R10).
- **Kalıcı kimlikler korunuyor.** Nesne ve katman anahtarları, silinmiş nesnelerin
  satırları dahil dosyaya yazılır ve okunurken birebir doğrulanır. Anahtar
  boşlukları sıkıştırılmaz: emekli bir anahtarın başka bir parsele verilmesi
  "bu parsel hangisiydi?" sorusunu cevapsız bırakırdı (model.md R4, P5).
- **Proje ayarları dosyayla gidiyor.** Proje kapsamlı ayarlar `.pcad` içinde
  taşınır ve belgenin parmak izinin parçasıdır; uygulama ve oturum kapsamlıları
  dosyaya girmez (model.md R39, R40).
- **Kesintiye dayanıklı kaydetme.** Önce yanına geçici dosya yazılır, ancak son
  bayt diske indikten sonra yerine konur. Yarıda kesilen bir kaydetme bir önceki
  kaydı bozmaz.
- **Güvenilmeyen girdi savunması.** Dosyadaki her uzunluk, konum, sayaç ve çapraz
  dizin gerçek dosya boyutuna karşı denetlenir; taşan toplama, çakışan blok,
  yuva dışı gösterim ve sıra dışı anahtar reddedilir (io.md R18, P6).
- **Beş dosya komutu.** `AÇ`, `KAYDET`, `FARKLIKAYDET`, `İÇEAKTAR`, `DIŞAAKTAR` —
  `Registry`'de kayıtlı, başsız çalışabilen, arayüz-komut satırı-betik eşitliği
  sınanan komutlar. Dosya seçme penceresi yalnız argümanı toplar (Article 1.2).
- **GDAL/OGR ile DXF ve GeoPackage.** `PIRICAD_WITH_GDAL` arkasında; sürücüler
  `cmake/KentOSCadGdalDrivers.cmake` içindeki açık izin listesinden gelir, tam
  sürücü kümesi asla açılmaz (io.md P7). `/vsicurl` gibi sanal dosya sistemi
  yolları reddedilir (P14). Kapalıyken komutlar hangi paketin gerektiğini söyler,
  sessizce başarılı olmaz.
- **Etiketsiz koordinat reddediliyor.** Koordinat sistemi bildirmeyen veri kümesi
  içe aktarılmaz; DXF'in yeri olmadığı için `.prj` yardımcı dosyası yazılır ve
  okunur (io.md R20).
- **libFuzzer koşumları ve tohum korpusu.** `piricad_fuzz_proje` ve
  `piricad_fuzz_dxf`, ASan + UBSan altında; tohumlar Clang olmayan yapılarda da
  `piricad_tests` tarafından aynı okuyucudan geçirilir (io.md R19, CLAUDE.md 6.7).
- **Belgeler.** `docs/veri/proje-dosyasi.md`, `docs/veri/dis-formatlar.md` ve beş
  komut sayfası; sözlük ve sorun giderme genişletildi.

### Eklendi — seçim ve nesne yakalama motoru

- **Nesne yakalama.** Uç nokta, orta nokta, merkez, kesişim, dik ayak, en yakın,
  ızgara ve kutupsal; hepsi `core.yakalama.modlar` bit maskesinden sürülüyor.
  `piricad/core/snap.hpp` istemciyi bilmez: bir nişan `Point2`, bir tolerans
  mesafedir.
- **Yardımlar tek yolda uygulanıyor.** `co_await ctx.point(...)` ne fareyi ne
  betiği tanır; yakalama, dik mod ve kutupsal izleme `InputAwaiter` içinde,
  değerin kaynağı sorulmadan çalışır (piricad.md §2.4, `CLAUDE.md` 1.2).
- **Seçim.** `EntityKey` kümesi, oturum kapsamında; `content_hash()`'e dokunmaz,
  geri alınmaz, belge değişikliği olarak günlüğe girmez (`model.md` R43, R44).
- **`SEÇ` komutu.** Tümü, kimlik, pencere, kesen, yön okuyan kutu ve tek nokta;
  ekle/çıkar/tersine işlemleri. Fareyle çizilen kutu ile komut satırına yazılan
  `SEÇ KUTU` aynı komuttur.
- **Tuvalde geri bildirim.** Her yakalama modu için ayrı işaret ve adı, seçili
  nesne vurgusu, pencere/kesen kutusunun ayırt edilebilir çerçevesi.
- **Kısayollar.** **F3** nesne yakalama, **F8** dik mod, **F9** ızgaraya yakalama,
  **Ctrl+A** / **Ctrl+Shift+A** tümünü seç / seçimi temizle. Her biri `MOD` veya
  `SEÇ` gönderir; ikinci bir mod listesi yok.

### Düzeltildi

- **`SİL` artık kalıcı anahtar konuşuyor.** `nesneler` parametresi yoğun slot
  yerine `EntityKey` alıyor (`model.md` R5/P4): günlüğe giren bir slot, tekrar
  oynatıldığında komşu parsele düşerdi. Kimlikler `1`'den başlar. Argümansız
  `SİL` etkin seçimi siler.
- **Liste parametreleri artık birikiyor.** `SİL nesneler=1 nesneler=2` iki nesneyi
  siliyor; önceden ikinci değer birinciyi sessizce eziyordu (`command.md` P15).
- **İki elemanlı JSON dizisi.** `{"nesneler": [1, 2]}` artık kimlik çifti olarak
  okunuyor; ayrımı komut bildirimi yapıyor, JSON'un biçimi değil.

### Eklendi — Faz 0 iskeleti

- **Komut veri yolu.** `Bus` → doğrulama → `Transaction` → `Journal`. Arayüz,
  komut satırı, betik, AI ve toplu iş eşit istemciler; hiçbirinin ayrıcalığı yok.
- **`Task<T>` coroutine tipi.** Etkileşimli komutlar elle yazılmış durum makinesi
  değil, düz coroutine akışı (piricad.md §2.4).
- **Tek kaynaklı komut tanımı.** `PIRICAD_COMMAND` makrosu ve `Registry`; komut
  satırı yardımı, AI araç şeması ve dokümantasyon buradan üretiliyor.
- **Tek gramer.** `piricad/command/parser.hpp` hem komut satırını hem betiği
  ayrıştırır: mutlak, göreli (`@50,30`), kutupsal (`@100<45`) ve satır içi ifade
  (`@(100*3),0`).
- **Sabit-nokta koordinat.** İç depoda `int64` milimetre; platformlar arası
  bit-birebir sonuç.
- **Geri alma / yineleme.** Ters-işlem günlüğü; bir komut = bir adım, bir betik
  bloğu = tek birleşik adım.
- **Komut günlüğü.** JSONL, ayrı thread'de asenkron yazım, oynatılabilir.
- **Sekiz çekirdek komut.** ÇİZGİ, SİL, KATMAN, YAKINLAŞ, GERİAL, YİNELE, BETİK,
  YARDIM.
- **Qt 6 kabuğu.** Harita canvas'ı, komut satırı widget'ı, katman paneli, komut
  günlüğü paneli, transkript, durum çubuğu.
- **Faz 0 kanıtı.** Aynı `ÇİZGİ` komutu arayüzden, komut satırından ve JSON
  betiğinden çalıştırıldığında tıpatıp aynı dokümanı ve tıpatıp aynı günlüğü
  üretiyor (piricad.md §16.5). `tests/unit/test_proof.cpp`.

### Eklendi — stil / gösterim motoru ve MPYY gösterim paketi

- **Stil kademesi çalışır hâlde.** `model.md` R13–R19'un tarif ettiği çözüm artık
  gerçek: görünüm çerçeve başında türetilmiyor, komut işlem içinde çözüyor,
  `StyleTable`'a intern ediyor ve nesne başına tek bir `StyleId` yazıyor. Çizici
  bir `u32` okuyor, kural işletmiyor.
- **`STİL` komutu (`core.style`).** Bir katmandaki nesnelerin stilini stil
  kataloğu paketinden veya doğrudan verilen renk/kalınlık/dolgu/sıra
  değerlerinden yazar; `sifirla=evet` ile katman varsayılanına döndürür. Arayüz,
  komut satırı ve betikten aynı belgeyi ve aynı günlüğü üretiyor
  (`tests/unit/test_style_rule.cpp`).
- **Bildirimsel kural değerlendirici** (`piricad/core/style_rule.hpp`). Kural dili
  bilerek kapalı: eşitlik, küme üyeliği, tam sayı aralığı, varlık. İfade, öncelik,
  olumsuzlama ve aritmetik yok — projede tek gramer `command/parser.hpp`'dir
  (CLAUDE.md 5.11). Kurallar dosya sırasına göre denenir, ilk uyan kazanır; sıra
  paketin içerik özetinin parçasıdır.
- **Ölçek penceresi.** Satır ve kural bazında `1:N` payda aralığı, iki ucu dahil,
  `0` = sınırsız. Ölçeğe bağlı gösterim nesne başına değil, tablo başına çözülür
  (`model.md` R16).
- **Kâğıt mikrometresi.** Katalogdaki `kalinlik_um` doğrudan `Appearance::width_um`
  alanına gidiyor; piksel hiçbir yerde saklanmıyor (`model.md` R20).
- **MPYY plan gösterim paketi** — `data/catalogs/mpyy/plan-gosterim.json` ve
  şeması `data/catalogs/schema/plan-gosterim.schema.json`. Kaynak: Mekânsal
  Planlar Yapım Yönetmeliği, EK-1 Gösterimler (EK-1a/1b/1c/1ç/1d + EK-1e Detay
  Kataloğu), yayım 14.06.2014.
- **Paketin gösterim satırları 0.1.0'da BİLEREK BOŞTU.** Paket künyesi, şeması,
  plan türü eşlemesi, çizgi ve tarama sembol tabloları tamdı; `stiller` ve
  `kurallar` dizileri boştu, çünkü EK-1 gösterim kodları, renkleri ve çizgi
  kalınlıkları resmî ek metninden birebir okunmadan girilmez. Bu boşluk aşağıdaki
  0.2.0 kaydıyla kapandı; `kurallar` hâlâ ve bilerek boştur.
- **Determinizm sınandı.** Aynı katalog + aynı belge = aynı `StyleId` dizisi ve
  aynı `content_hash()`; aynı görünüm iki kez istendiğinde stil tablosu
  büyümüyor. Golden senaryosu `tests/golden/senaryolar/stil.txt`.
- **Belge.** [`docs/komutlar/style.md`](docs/komutlar/style.md), sekiz bölüm,
  üç istemci yolu, gösterim satırlarının eksikliği ilk paragrafta ve gelecek
  zamanla yazılı (Article 11.8).

### Eklendi — MPYY gösterim ekleri veri paketi (`mpyy` katalogları 0.1.0 → 0.2.0)

Sebebi olan mevzuat: **Mekânsal Planlar Yapım Yönetmeliği**, EK-1 Gösterimler ve
EK-2 asgari altyapı standartları tablosu. EK-1a, EK-1c, EK-1ç, EK-1d ve EK-1e
metinlerinde **(Değişik:RG-22/1/2026-33145)** damgası vardır; paket bu hâli esas
alır. EK-1b'de değişiklik damgası **yoktur**, yönetmeliğin **RG-14/6/2014-29030**
sayılı ilk hâli esas alınmıştır ve bu tespit paketin `source` alanında yazılıdır.
EK-2 **(Değişik:RG-17/5/2017-30069)** ile değişik hâldedir.

- **`data/catalogs/mpyy/plan-gosterim.json` 0.2.0** — 476 gösterim satırı: EK-1a
  Ortak Gösterimler 89, EK-1b Mekânsal Strateji Planı 30, EK-1c Çevre Düzeni Planı
  37, EK-1ç Nazım İmar Planı 115, EK-1d Uygulama İmar Planı 205. 297 satırda alan
  renk kodu, 48 satırda `ŞEFFAF` hükmü, 8 satırda çizgi rengi, 17 satırda simge
  rengi çözüldü.
- **`data/catalogs/mpyy/detay-katalogu.json` 0.2.0** — EK-1e Detay Kataloğu'nun
  379 detay kartı; 338 kartta renk, 311 kartta plan türü başına çizgi kalınlığı
  (kâğıt mikrometresi, 1000 = 1 mm) çözüldü. Şeması
  `data/catalogs/schema/detay-katalogu.schema.json`.
- **`data/catalogs/mpyy/asgari-standartlar.json` 0.2.0** — EK-2'nin 33 altyapı
  kalemi, 4 nüfus grubu ve 13 maddelik açıklama bloğu. m²/kişi değerleri binde tam
  sayı olarak saklanır (0.5 → 500); kayan nokta saklanmaz (CLAUDE.md 2.4). Şeması
  `data/catalogs/schema/asgari-standartlar.schema.json`.
- **608 sembol görseli** `data/catalogs/mpyy/semboller/` altında, dosya adı içerik
  SHA-256'sının ilk 16 basamağı. Aynı sembol kaç satırda geçerse geçsin tek
  dosyadır; toplam 6,6 MB, en büyüğü 287 KB — `data.md` R15'in 10 MB dosya ve
  250 MB ağaç sınırlarının altında, LFS gerekmez.
- **`plan-gosterim.schema.json` `schema_version` 1 → 2.** Yalnız ALAN EKLENDİ; hiçbir
  alanın anlamı değişmedi (CLAUDE.md 0.2a). Yeni alanlar satırın kaynak izini
  taşır: `sutunlar` (ham hücreler), `gorsel`, `sutun_metinleri`, `bolum`, `grup`,
  `renk_secenekleri`, `simge_renk`, `dolgu.seffaf`, `dolgu.saydamlik_yuzde`,
  `belirsiz` / `belirsiz_nedeni` ve paket düzeyinde `gorseller` tablosu.
- **`scripts/mpyy-cikar.py`.** Katalogları resmî ek dosyalarından üretir; yalnız
  Python standart kütüphanesi. Elle düzenlenmiş bir katalog kabul edilmez: aynı
  kaynaktan iki koşum bayt birebir aynı JSON'u verir.
- **Hiçbir değer uydurulmadı.** Okunamayan renk, çözülemeyen satır ve belirsiz ad
  `belirsiz: true` ve bir gerekçe koduyla işaretlendi: 14 gösterim satırı, 15 detay
  kartı, 6 standart kalemi. Gerekçeler `kapsam.eksikler` bloklarında sayılıdır.
- **`kurallar` hâlâ boş.** Hangi nesnenin hangi gösterim satırını alacağı ek
  metninden okunamaz; plan türü ve öznitelik şemasıyla birlikte uzman kararıdır.
- **Uzman onayı BEKLİYOR.** Üç katalogun da `kapsam.onay` alanı `BEKLİYOR`
  yazıyor; harita mühendisi / şehir plancısı imzası olmadan bu paket bir plana
  uygulanmaz (CLAUDE.md 6.11).
- **`scripts/ci-gate-mpyy.sh`.** Katalog özetini `tests/golden/mpyy/beklenen.txt`
  ile karşılaştırır, üç dosyanın SHA-256'sını sabitler ve kaynak ekler mevcutsa
  çıkarımı yeniden koşup bayt birebir karşılaştırır.

### Eklendi — kullanıcı dokümantasyonu

- **Kati kural.** Kullanıcının yapabildiği her şeyin `/docs` altında, Markdown
  biçiminde, Türkçe ve yayımlanabilir kalitede bir sayfası olacak — komut sistemi
  dahil. Belgelenmemiş özellik yayımlanmamış sayılır (`CLAUDE.md` Article 11,
  5.16–5.17, 6.12; `.claude/docs.md`).
- **Kılavuz.** Kurulum, ilk adımlar, arayüz turu, komut sistemi, komut satırı,
  sekiz komut sayfası, betik yazma, komut günlüğü, koordinat sistemleri, sözlük ve
  sorun giderme.
- **Üretilmiş komut referansı.** `piricad_docgen` komut kaydından
  `docs/komutlar/referans.md` üretir; elle düzenlenirse CI kapısı fark eder
  (`make reference`).
- **`scripts/ci-gate-docs.sh`.** Belgesiz komut, eksik zorunlu bölüm, dizine
  bağlanmamış sayfa, ölü bağlantı, TODO kalıntısı ve bayat referans derlemeyi kırar.
- **`tests/unit/test_docs.cpp`.** Kılavuzdaki her komut satırını ve her JSON betiğini
  doğrudan Markdown'dan okuyup çalıştırır. Örneklerin ikinci bir kopyası yoktur.

### Düzeltildi

- Kullanıcıya görünen bütün hata mesajları Türkçeleştirildi; doğrulama, ayrıştırıcı,
  doküman ve kayıt katmanlarında İngilizce metin kalmamıştı. Yeni bir test İngilizce
  sızıntısını yakalıyor.
- Komut satırında anahtar sonrası tırnaklı değer (`KATMAN ad="YOL KENARI"`)
  ayrıştırılamıyordu; kılavuz örneğini çalıştıran test bunu ortaya çıkardı.
- BÖHHBÜY TM 3° dilim tablosu C++ içinden `data/crs/tm3-dilimleri.json` dosyasına
  taşındı — mevzuat verisi koda gömülmez (`CLAUDE.md` 5.13).

### Eklendi — performans ve determinizm altyapısı (Faz 0)

- **Benchmark kapısı.** `make bench` §10.1 bütçelerini ölçer ve aşılırsa derlemeyi
  kırar. `make bench-baseline` makineye özel temel değer kaydeder. Regresyon,
  hem %10'u hem de ölçümün kendi yayılımını aşmak zorundadır — sıfıra yakın bir
  ölçümde göreli eşik tek başına gürültüyü regresyon sanır.
- **Ölçülemeyen bütçeler listelenir.** DWG, LAZ, topoloji, soğuk açılış, boş proje
  RAM'i ve tuş gecikmesi `BEKLEMEDE` olarak sebebiyle raporlanır; hiçbir zaman
  "geçti" saymaz. Sessizce kaybolan bütçenin sahibi olmaz.
- **Golden data altyapısı.** `tests/golden/senaryolar` altındaki senaryolar
  oynatılır ve belgenin deterministik dökümüyle karşılaştırılır. Fark hangi tepe
  noktasının kaydığını söyler, yalnız "özet değişti" demez.
- **Jitter testi (§11 Faz 0).** 30. dilim TM3 koordinatlarında 1000 ardışık
  milimetrenin float'ta yalnız ~32 farklı değere çöktüğü, `ViewTransform`'un
  origin offset'iyle bin ayrı değer kaldığı ölçülerek kanıtlandı.
- **Mekânsal indeks.** `core::SpatialIndex` — §10.5'in tarif ettiği toplu
  yüklenen STR R-tree. Kaba kuvvetle karşılaştıran altı testi var.
- **Nesne başına önbelleklenmiş sınır kutusu.** Ayrı bir SoA bloğu; eleme artık
  tepe noktalarına hiç dokunmuyor.

### Düzeltildi — ölçümün ortaya çıkardıkları

- **5M poligonda kare süresi 41 ms → 0,003 ms.** 16 ms bütçesi artık beş bin kat
  payla karşılanıyor. Aynı düzenlemeden sonraki kare de bütçe içinde: bir çizgi
  çizmek katmanı yeniden paketlemiyor.
- **Toplu yüklemede O(n²).** `add_polyline` her çağrıda tam boyutla `reserve`
  ediyor, vektörün geometrik büyümesini bozuyordu. 1M parsel kurulumu 2 dakikadan
  63 ms'ye indi.
- **Arayüzde üç ayrı O(n) tarama.** Canlı nesne sayısı ve katman başına nesne
  sayısı artık artımlı tutuluyor; katman ve öznitelik panelleri her doküman
  değişiminde bütün nesneleri dolaşmıyor.

### Bilinen sapmalar

Üçü de CLAUDE.md Article 8'de kayıtlı ve kaldırma koşulu yazılı:
canvas `QPainter` (qsb yok), Qt dışında bağımlılık yok, betik motoru JSON.
