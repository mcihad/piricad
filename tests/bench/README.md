# Benchmarks

The piricad.md §10.1 budgets, enforced as gates. A benchmark more than **10%
worse than the stored baseline breaks the build**, and a budget is never relaxed
to make a benchmark pass (CLAUDE.md Article 7).

```bash
make bench            # ölç ve bütçelere karşı denetle
make bench-baseline   # bu makinenin temel değerlerini kaydet
```

Ölçümü **Google Benchmark** yapar; kapı bize aittir. Kütüphane yineleme sayısını
seçer, zamanlayıcıyı kurulumun dışında tutar ve tekrarları yürütür. Bütçe ile
temel karşılaştırması `support.cpp` içindedir, çünkü 16 ms'nin bir ürün
gereksinimi olduğunu hiçbir kütüphane bilmez.

Google Benchmark'ın kendi bayrakları da geçerlidir; bir senaryo üzerinde
çalışırken en çok işe yarayan şudur:

```bash
./build/dev/bin/piricad_bench --benchmark_filter='yakalama.*'
```

## Two different checks

| | Ne | Ne zaman denetlenir |
|---|---|---|
| **Bütçe** | §10.1'in mutlak hedefi. Ürün gereksinimi | Her zaman |
| **Temel** | Aynı makinede en son ölçüm. Regresyon koruması | Yalnız temel aynı makinede kaydedilmişse |

A baseline recorded elsewhere measures the machine, not the code, so the harness
disables the regression gate rather than compare across machines. A regression
must be both more than 10% and larger than the run's own sample spread —
otherwise a sub-microsecond case reports scheduler jitter as a regression.

## Scenarios

| Kimlik | Bütçe | Durum |
|---|---|---|
| `render.pan_zoom_5m` | ≤ 16 ms | ölçülüyor |
| `render.duzenleme_sonrasi_kare` | ≤ 16 ms | ölçülüyor |
| `komut.betikten_gonderim` | ≤ 10 µs | ölçülüyor |
| `render.pan_zoom_5m_tam_kapsam` | — | bilgilendirme; LOD Faz 1 |
| `core.toplu_yukleme_1m` | — | bilgilendirme |
| `core.indeks_kurulumu_1m` | — | bilgilendirme |
| `komut.toplu_is_100k` | — | bilgilendirme |
| `bellek.5m_parsel` | — | bilgilendirme |
| `io.dwg_200mb_acilis` | ≤ 3 s | BEKLEMEDE — `/src/io` boş |
| `io.laz_50m_ilk_goruntu` | ≤ 5 s | BEKLEMEDE — `/src/io` boş |
| `domain.topoloji_100k_parsel` | ≤ 2 s | ölçülüyor |
| `domain.topoloji_1000_ortusme` | — | bilgilendirme; 1000 gerçek OCCT kesişimi |
| `domain.kapsama_1000_alan` | — | bilgilendirme; tek OCCT birleşiminde 1000 alan, 250 kapalı boşluk |
| `uygulama.soguk_acilis` | ≤ 2 s | BEKLEMEDE — Qt içinde ölçülmeli |
| `uygulama.bos_proje_ram` | ≤ 300 MB | BEKLEMEDE — Qt içinde ölçülmeli |
| `arayuz.tus_ekran_gecikmesi` | ≤ 30 ms | BEKLEMEDE — Qt olay döngüsü gerekiyor |

A pending scenario is listed rather than omitted, reports BEKLEMEDE with its
reason, and never counts as passing.

### Yerel tarama doğrulaması — 1 Ekim 2026

macOS, optimize Clang/OCCT yapısında zoom fazı düzeltmesi sonrası
`render.pan_zoom_5m` sahne kurma ölçümü **0.003 ms**. Gerçek QRhi penceresinde
20 karelik ortanca: `desen-yuku.json` **0.359 ms / 81 çizim çağrısı**,
`yogun-tarama.json` **0.128 ms / 27 çağrı**. İki sahne de 16 ms ve 100 çağrı
bütçeleri içinde. Kayıtlı Linux temel değeri farklı makineye ait olduğundan
yüzde regresyon karşılaştırması yapılmaz; bunlar yerel doğrulama ölçümleridir.

### Yerel örtüşme doğrulaması — 2 Ekim 2026

Aynı macOS makinesinde, aynı sahnelerle üçer koşumun ortancası; her koşum
20 kareyi ölçer. Başlangıçta eski dolgu kuralı, sonrasında ayrı dış halkaları
koruyan dolgu kuralı kullanıldı:

| Sahne | Önce | Sonra | Çizim çağrısı |
|---|---:|---:|---:|
| `desen-yuku.json` | 0.438 ms | 0.449 ms | 81 → 81 |
| `yogun-tarama.json` | 0.098 ms | 0.097 ms | 28 → 28 |

Farklar %10 sınırının altında; iki sahne de 16 ms / 100 çağrı bütçesinde.
`render.pan_zoom_5m` sahne kurma ölçümü 0.002 ms. Ayrı alanların kesişimi,
iç içe alan, delik ve içbükey sınır gerçek GPU görüntüsünde doğrulandı.

### G-05 topoloji — 2 Ekim 2026

macOS, optimize Clang, OpenCASCADE 7.9.3: mevcut
`domain.topoloji_100k_parsel` örneği önce **370.351 ± 9.838 ms**, OCCT geçerlilik
denetimi ve örtüşme yolu sonrasında **132.680 ± 4.910 ms**; 2 s bütçesinde.
Bu örnek, yedi farklı düz kenarlı parsel şeklinin bir ızgarada tekrarıdır:
kutular ortak kenarda değer, pozitif alanla örtüşmez. Yeni yol kenar boyunca
değen kutuları alan işleminden önce eler ve öteleme ile aynı kalan şekillerin
OCCT geçerlilik sonucunu denetim boyunca paylaşır. Bu ölçüm 100 bin benzersiz
karmaşık eğrinin veya yoğun pozitif örtüşmenin süresi olarak yorumlanmaz.
`domain.topoloji_1000_ortusme`, öteleme önbelleği ve komşu elemesi olmadan
1000 farklı pozitif örtüşmeyi doğrudan OCCT Common + GProp + gösterim sınırı
yolunda ölçer: **271.245 ± 10.038 ms** (bilgilendirme, ayrı bir mutlak bütçe yok).
Eğri dalları ayrı birim vakalarında sınanır; farklı makinenin kayıtlı temel
değeri değiştirilmedi.

Kapsama kuralı eklendikten sonra olağan 100k ölçümü **135.913 ± 7.446 ms**
oldu (önce 132.680 ms, yaklaşık %2,4; aynı 2 s bütçesi). Bu koşum varsayılan
`kapsama=false` denetimidir. `domain.kapsama_1000_alan` ayrı tek native birleşim,
yüzey birleştirme ve fark işlemlerini ölçer: 250 ayrı dört-parsel grubunda 1000
alan, her grupta tam 100 m² kapalı boşluk; **185.218 ± 9.468 ms**. Eşik ve alan
sonuçları ölçüm sırasında da doğrulanır. Bu örneğin mutlak süre bütçesi yoktur;
100k alanın kapsama birleşimine aynı süreyi veya 2 s sınırını atfetmez.
Gerçek GPU pencere bütçeleri de yeniden geçti: desen yükü **284 µs / 81 çağrı**,
yoğun tarama **107 µs / 27 çağrı**; ikisi de 16 ms ve 100 çağrı sınırında.

## Fixtures

`fixtures.hpp` generates the cadastral grid deterministically: a five-million
parcel layer is far too large to commit, and the same code produces the same
document on every machine, which is what makes a cross-platform comparison
meaningful (§7.3).

## Geliştirici ortam değişkenleri

| Değişken | Ne yapar |
|---|---|
| `PIRICAD_BENCH_RECORD` | Ölçümleri bu makinenin temel değeri olarak kaydeder |
| `PIRICAD_BENCH_BASELINE` | Temel değer dosyasının yolunu değiştirir |
| `PIRICAD_FRAME_DUMP` | `piricad` uygulaması bir kare çizip verilen PNG yoluna yazar ve çıkar. `QT_QPA_PLATFORM=offscreen` ile ekransız çalışır; tuvalin doğruluğu bir resim olduğu için bir çizim değişikliğini gözden geçirilebilir kılan şey budur |
| `PIRICAD_OPEN_DESIGNER` | Verilen katmanda stil tasarımcısını açar; `PIRICAD_FRAME_DUMP` etkin pencereyi çektiği için tasarımcının kendi karesi alınabilir |

Hiçbiri kullanıcıya dönük değildir; bu yüzden komut satırı seçeneği değil ortam
değişkenidirler (CLAUDE.md 5.17 bir CLI bayrağı için kendi `/docs` sayfasını
ister ve bunların bir haritacıya faydası yoktur).

## Bir senaryo yazarken

`iterations = 0` varsayılandır ve yineleme sayısını Google Benchmark seçer. Bir
gövde **kendi düzeneğini değiştiriyorsa** — belgeye nesne ekliyor, dosya
yazıyor, ya da tek başına bir saniye sürüyorsa — `iterations = 1` verilir.
Verilmezse gövde binlerce kez koşar ve ölçtüğü şey artık senaryonun tarif ettiği
şey olmaz: bu göç sırasında tam olarak bu oldu, `render.duzenleme_sonrasi_kare`
paylaşılan 5M düzeneği şişirdi ve ardından koşan bütün yakalama senaryoları on
kat yavaş göründü.

Zamanlayıcı dışında kalması gereken yıkım işi `state.PauseTiming()` içinde
**açıkça** serbest bırakılır. Nesneyi döngü gövdesinde tanımlayıp duraklatma
çağırmak işe yaramaz: yıkıcı kapanış ayracında, yani `ResumeTiming()`'den sonra
çalışır.
