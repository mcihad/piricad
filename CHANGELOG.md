# Değişiklik Günlüğü

Bu proje [Semantik Sürümleme](https://semver.org/lang/tr/) kullanır.
Mevzuat kataloğu değişiklikleri, sebebi olan yönetmelik veya genelge adıyla
birlikte kaydedilir (CLAUDE.md Article 9).

## [Yayımlanmamış]

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

### Bilinen sapmalar

Üçü de CLAUDE.md Article 8'de kayıtlı ve kaldırma koşulu yazılı:
canvas `QPainter` (qsb yok), Qt dışında bağımlılık yok, betik motoru JSON.
