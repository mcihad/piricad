# Değişiklik Günlüğü

Bu proje [Semantik Sürümleme](https://semver.org/lang/tr/) kullanır.
Mevzuat kataloğu değişiklikleri, sebebi olan yönetmelik veya genelge adıyla
birlikte kaydedilir (CLAUDE.md Article 9).

## [Yayımlanmamış]

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
  `cmake/PiriCADGdalDrivers.cmake` içindeki açık izin listesinden gelir, tam
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
