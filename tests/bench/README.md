# Benchmarks

The piricad.md §10.1 budgets, enforced as gates. A benchmark more than **10%
worse than the stored baseline breaks the build**, and a budget is never relaxed
to make a benchmark pass (CLAUDE.md Article 7).

```bash
make bench            # ölç ve bütçelere karşı denetle
make bench-baseline   # bu makinenin temel değerlerini kaydet
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

Google Benchmark replaces this harness when the dependency set lands (§9.11); the
scenario definitions do not change.
