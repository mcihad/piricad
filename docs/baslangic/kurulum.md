# Kurulum ve Derleme

PiriCAD'i kaynaktan derlemek isteyen kullanıcı ve sistem yöneticisi için; bu sayfayı
bitirdiğinizde çalışan bir `piricad` çalıştırılabiliri ve neyin eksik olduğunu söyleyen
bir teşhis çıktınız olacak.

PiriCAD henüz hazır paket olarak dağıtılmıyor. MSI, DMG, AppImage, `.deb` ve `.rpm`
paketleri Faz 1'de gelecek.

## Gereksinimler

| Bileşen | En düşük sürüm | Not |
|---|---|---|
| CMake | 3.28 | Preset desteği için |
| Ninja | herhangi | Üç platformda da tek üreteç |
| GCC | 13 (14 tercih edilir) | Linux |
| Clang | 17 | Linux alternatifi |
| MSVC | 19.40 (VS 2022 17.10+) | Windows |
| AppleClang | Xcode 15.3 | macOS |
| Qt | 6.5 | Core, Gui, Widgets modülleri |

C++20 zorunludur; komutların etkileşim modeli coroutine üzerine kuruludur.

Ubuntu/Debian'da:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev
```

## Derleme

```bash
git clone <depo-adresi> piricad
cd piricad
make build
```

`make build` önce yapılandırır, sonra derler. Çıktı `build/dev/bin/` altına düşer.

## Çalıştırma

```bash
make run
```

Örnek bir çizimle açmak için:

```bash
make run-script SCRIPT=tests/journal/ornek-parsel.json
```

Aynı işi doğrudan da yapabilirsiniz:

```bash
./build/dev/bin/piricad --betik tests/journal/ornek-parsel.json
```

`--betik` seçeneği verilen JSON betiğini açılışta komut veri yolundan çalıştırır ve
sonucu görünüme sığdırır. Ayrıntı: [Betik yazma](../betik/README.md).

## Ortamı denetleme

```bash
make doctor
```

Bu komut makinede neyin bulunup neyin bulunmadığını, eksik olanın neye mal olduğuyla
birlikte listeler. Örnek çıktı:

```text
PiriCAD — build environment

  CMake                      4.2.3
  Ninja                      1.13.2
  C++ compiler               g++ (Ubuntu 15.2.0) 15.2.0
  Qt 6                       6.10.2

Optional dependencies (all gated OFF by default)
  qsb (GPU canvas)           MISSING  — QRhi backend unavailable; QPainter is used
  GDAL                       MISSING  — no format I/O
  PROJ                       MISSING  — no coordinate transformation
```

`MISSING` yazan bir satır derlemeyi engellemez. Bütün dış bağımlılıklar varsayılan
olarak **kapalıdır**; kapalıyken PiriCAD çalışır, açıkken ve bağımlılık yoksa
yapılandırma ne kurulacağını söyleyerek durur.

## Derleme profilleri

`make PRESET=<ad> <hedef>` ile profil seçilir.

| Profil | Kullanım |
|---|---|
| `dev` | Varsayılan. Hata ayıklama bilgisi içeren optimize derleme, testler açık |
| `debug` | Tam hata ayıklama |
| `release` | Optimize, testler kapalı, paketleme girdisi |
| `asan` | Bellek ve tanımsız davranış denetleyicileriyle |
| `headless` | Qt olmadan yalnız çekirdek ve testler |

Örnek:

```bash
make PRESET=release build
```

## Kullanışlı hedefler

```bash
make help            # bütün hedefleri listeler
make test            # testleri ve CI kapılarını çalıştırır
make gates           # yalnız CI kapılarını çalıştırır
make bench           # performans bütçelerini ölçer
make bench-baseline  # bu makinenin temel değerlerini kaydeder
make format          # kaynak biçimlendirmesini uygular
make reference       # komut referansını yeniden üretir
make docs            # belgeleri üretir ve denetler
make clean           # derleme çıktısını siler
make distclean       # bütün derleme ağacını siler
```

## Performans bütçelerini ölçmek

```bash
make bench
```

PiriCAD'in karşılamak zorunda olduğu hız hedefleri sabittir ve ölçülür. Örnek çıktı:

```text
senaryo                          ölçüm    bütçe      temel  durum
------------------------------------------------------------------------------
komut.betikten_gonderim          1.00 µs      10.00        —  tamam
render.pan_zoom_5m                0.00 ms      16.00        —  tamam
io.dwg_200mb_acilis                   —       3000        —  BEKLEMEDE  ...
```

İki ayrı denetim vardır ve karıştırılmamalıdır:

- **Bütçe** — ürünün karşılamak zorunda olduğu mutlak hedef. Her zaman denetlenir.
  Aşılırsa derleme kırılır ve bütçe, ölçüm geçsin diye gevşetilmez.
- **Temel** — aynı makinede en son kaydedilen ölçüm. Yalnız regresyon içindir ve
  yalnız aynı makinede kaydedilmişse denetlenir; başka bir makinenin değeri kodu
  değil o makineyi ölçer.

Kendi makinenizin temel değerlerini kaydetmek için:

```bash
make bench-baseline
```

`BEKLEMEDE` yazan satırlar henüz ölçülemeyen hedeflerdir; sebebi satırın sonunda
yazar ve bu satırlar hiçbir zaman "geçti" saymaz.

## Bu sürümde eksik olanlar

Aşağıdakiler bilinçli, kayıtlı ve kaldırma koşulu yazılı eksiklerdir; ayrıntısı depo
kökündeki `CLAUDE.md` Article 8'dedir.

| Eksik | Sonucu | Ne zaman gelecek |
|---|---|---|
| `qsb` (qt6-shadertools) | Harita GPU yerine `QPainter` ile çizilir | Faz 1'de GPU canvas'ı devreye girecek |
| GDAL / PROJ / GEOS / CGAL | Dosya okuma-yazma ve koordinat dönüşümü yok | Faz 1–2 |
| Lua / Python | Betik motoru yalnız JSON | Faz 2 |

Bunların hepsi `PIRICAD_WITH_<AD>` yapılandırma seçeneğinin arkasındadır. Örneğin
GPU canvas'ını denemek isterseniz:

```bash
cmake --preset dev -DPIRICAD_WITH_RHI=ON
```

`qsb` kurulu değilse yapılandırma şu hatayla durur:

```text
PIRICAD_WITH_RHI=ON but `qsb` was not found. Install qt6-shadertools
(Debian/Ubuntu: qt6-shadertools-dev-tools) or configure with
-DPIRICAD_WITH_RHI=OFF to use the QPainter backend.
```

## Sırada ne var

[İlk adımlar](ilk-adimlar.md) sayfasıyla devam edin.
