# Kurulum ve Derleme

KentOSCad'i kaynaktan derlemek isteyen kullanıcı ve sistem yöneticisi için; bu sayfayı
bitirdiğinizde çalışan bir `kentos_cad` çalıştırılabiliri ve neyin eksik olduğunu söyleyen
bir teşhis çıktınız olacak.

KentOSCad henüz hazır paket olarak dağıtılmıyor. MSI, DMG, AppImage, `.deb` ve `.rpm`
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
git clone <depo-adresi> kentos_cad
cd kentos_cad
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
./build/dev/bin/kentos_cad --betik tests/journal/ornek-parsel.json
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
KentOSCad — build environment

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
olarak **kapalıdır**; kapalıyken KentOSCad çalışır, açıkken ve bağımlılık yoksa
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

KentOSCad'in karşılamak zorunda olduğu hız hedefleri sabittir ve ölçülür. Örnek çıktı:

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
| GPU canvas (`KENTOS_WITH_RHI`) | Varsayılan yapıda harita GPU yerine `QPainter` ile çizilir. Seçenek açıldığında QRhi arka ucu MPYY kataloğunun **on bir sembol katmanı türünün hepsini** çizer — dolgu, çizgi, işaretçi, desen, yayımlanmış görsel — ve `KENTOS_WITH_TEXT` ile metni de. Eksik olan çizim değil ölçüm: kare bütçesi (≤16 ms) henüz koşulmadı | Bütçe ölçülüp karşılandığında varsayılan açık olacak |
| GDAL | DXF ve GeoPackage okunup yazılamaz; `İÇEAKTAR` ve `DIŞAAKTAR` hangi paketin gerektiğini söyleyerek hata döndürür. KentOSCad'in kendi `.pcad` proje dosyası GDAL olmadan da çalışır | Kurulduğunda kendiliğinden açılır |
| PROJ / GEOS / CGAL | Koordinat dönüşümü ve geometri işlemleri sınırlı | Faz 1–2 |
| Python (`KENTOS_WITH_PYTHON`) | Eklenti ve toplu işleme katmanı yok | Faz 2 |

Lua artık eksik değil: `KENTOS_WITH_LUA=ON` ile gömülü Lua 5.4 betik motoru derlenir —
bkz. [Lua betikleri](../betik/lua.md).

## Seçimlik yapılandırma seçenekleri

Hepsi `KENTOS_WITH_<AD>` biçimindedir ve **varsayılan kapalıdır**. Açık ama gereği
kurulu değilse yapılandırma, hangi paketin gerektiğini söyleyerek durur — sessizce
kapanmaz.

| Seçenek | Ne açar | Makinede gereken |
|---|---|---|
| `KENTOS_WITH_LUA` | Gömülü Lua betik motoru | Yok. Lua 5.4 ve sol2 sabitlenmiş commit'lerden indirilir |
| `KENTOS_WITH_RHI` | QRhi GPU canvas | Qt 6.7+, Qt Shader Tools (`qsb`) ve Qt Gui'nin **private** başlıkları |
| `KENTOS_WITH_TEXT` | GPU tuvalinde metin (SDF atlası) | `libfreetype-dev`, `libharfbuzz-dev`. msdfgen ve stb sabitlenmiş commit'ten iner |
| `KENTOS_WITH_GDAL` | DXF / GeoPackage | `libgdal-dev` |
| `KENTOS_WITH_PROJ` | Koordinat dönüşümü | `libproj-dev` |
| `KENTOS_WITH_POSTGIS` | Canlı PostGIS bağlantısı | `libpq-dev` |

### Lua

```bash
cmake --preset dev -DKENTOS_WITH_LUA=ON
cmake --build --preset dev
```

Makinede Lua kurulu olması gerekmez: kaynak, sabitlenmiş commit'ten indirilip
projeyle birlikte derlenir. İlk yapılandırma bu yüzden ağ ister.

### GPU canvas

```bash
cmake --preset dev -DKENTOS_WITH_RHI=ON
```

QRhi, Qt Gui'nin private başlıklarında yaşar ve dağıtımların çoğu bunları ayrı
paketler. Eksikse yapılandırma şöyle durur:

```text
KENTOS_WITH_RHI=ON but <rhi/qrhi.h> was not found. QRhi lives in Qt Gui's
PRIVATE headers, which most distributions package separately from the public ones.
  Debian/Ubuntu: sudo apt install qt6-base-private-dev
  Fedora:        sudo dnf install qt6-qtbase-private-devel
  Arch:          included in qt6-base
  vcpkg:         installed with qtbase
  Or configure with -DKENTOS_WITH_RHI=OFF to use the QPainter backend.
```

Shader paketleri derleme sırasında `qsb` ile pişirilir; çalışma anında hiçbir shader
derlenmez. `qsb` bulunamazsa hata yine hangi paketin gerektiğini söyler.

## Sırada ne var

[İlk adımlar](ilk-adimlar.md) sayfasıyla devam edin.
