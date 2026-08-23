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
| `domain.topoloji_100k_parsel` | ≤ 2 s | BEKLEMEDE — `/src/domain` boş |
| `uygulama.soguk_acilis` | ≤ 2 s | BEKLEMEDE — Qt içinde ölçülmeli |
| `uygulama.bos_proje_ram` | ≤ 300 MB | BEKLEMEDE — Qt içinde ölçülmeli |
| `arayuz.tus_ekran_gecikmesi` | ≤ 30 ms | BEKLEMEDE — Qt olay döngüsü gerekiyor |

A pending scenario is listed rather than omitted, reports BEKLEMEDE with its
reason, and never counts as passing.

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
