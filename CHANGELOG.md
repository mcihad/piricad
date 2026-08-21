# Değişiklik Günlüğü

Bu proje [Semantik Sürümleme](https://semver.org/lang/tr/) kullanır.
Mevzuat kataloğu değişiklikleri, sebebi olan yönetmelik veya genelge adıyla
birlikte kaydedilir (CLAUDE.md Article 9).

## [Yayımlanmamış]

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
