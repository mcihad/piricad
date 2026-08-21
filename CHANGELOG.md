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
