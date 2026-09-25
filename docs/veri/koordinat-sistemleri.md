# Koordinat Sistemleri

Türkiye'de ölçüm ve çizim yapan harita mühendisi için; bu sayfayı bitirdiğinizde KentOSCad'in
koordinatları nasıl sakladığını, hangi dilimlerle çalıştığını ve bugün neyin yapılıp
neyin yapılamadığını bileceksiniz.

## Koordinatlar milimetre olarak saklanır

KentOSCad bütün koordinatları **64 bitlik tam sayı milimetre** olarak saklar. Ondalıklı
sayı kullanmaz.

Kullanıcı olarak bunun size üç sonucu vardır:

1. **Milimetre altı yuvarlanır.** `485320.1504` metre girerseniz `485320.150` metre olarak
   saklanır. Kadastro ve imar işi için milimetre fazlasıyla yeterli çözünürlüktür.
2. **Sonuç makineden bağımsızdır.** Aynı çizim Linux, Windows ve macOS'ta bit düzeyinde
   aynı alanları ve aynı kesişimleri verir. Resmî belge üreten bir yazılımda bu bir
   gereklilik, tercih değildir.
3. **Toplama sırası sonucu değiştirmez.** Bir alan hesabını farklı sırada yapmak farklı
   sayı vermez.

Komut satırında **metre** yazarsınız; program milimetreye çevirir. Betikte doğrudan
**milimetre** yazarsınız. Bkz. [Betik yazma](../betik/README.md).

| Metre | Milimetre |
|---|---|
| `485320.150` | `485320150` |
| `4310220.400` | `4310220400` |
| `1.000` | `1000` |
| `0.001` | `1` |

Saklama aralığı yaklaşık ±9,2 × 10¹² metredir; herhangi bir yersel koordinatın kat kat
üzerinde.

## Varsayılan koordinat sistemi

Yeni çizim **TUREF/TM36** ile açılır: TUREF datumu, 36° orta meridyenli 3 derecelik dilim.
Bu dilim Ankara'yı ve Orta Anadolu'yu kapsar. Başka bir dilimde çalışıyorsanız
çizime başlamadan önce kurun:

```text
AYAR koordinat_sistemi TUREF/TM30
```

Dokümanın koordinat sistemi durum çubuğunun sağ ucunda ve **Öznitelikler** panelinde
yazar.

## Koordinat sisteminin birimi: yalnız metre

Çizim koordinatlarını **metre** olarak, milimetre çözünürlükte saklar. Bu yüzden çizimin
koordinat sistemi de koordinatlarını metre sayan bir sistem olmak zorundadır: bir
izdüşüm sistemi (TUREF/TM27 … TUREF/TM45, UTM dilimleri) ya da haritaya henüz
oturtulmamış bir iş için `YEREL`.

**Coğrafi bir sistem (WGS 84, EPSG:4326 gibi) çizimin sistemi olamaz.** Bu sistemler
koordinatlarını derece olarak sayar. Dereceyi metre saymak her köşeyi yüz metrelik bir
ızgaraya oturturdu: 0,001° zeminde yaklaşık yüz metredir. KentOSCad bunu dört yerde
reddeder ve her seferinde nedenini söyler:

| Nerede | Ne olur |
|---|---|
| `AYAR koordinat_sistemi EPSG:4326` | Reddedilir. Çizimin sistemi ve ayar değişmez |
| `OTURT … sistem=EPSG:4326` | Hiçbir nesne taşınmadan reddedilir |
| Derece sayan bir CBS katmanını içe almak (GeoPackage, Shapefile) | Katman reddedilir, dönüştürme yolu söylenir |
| Yanındaki `.prj` derece bildiren bir DXF | DXF reddedilir |

Aynı kural fit (US survey foot) gibi başka birimler sayan sistemler ve yer merkezli
(X/Y/Z) sistemler için de geçerlidir.

Derece sayan bir dosyayı içe almak için önce metre sayan bir sisteme dönüştürün:
QGIS'te **Farklı Kaydet** ile KRS olarak örneğin EPSG:5256 (TUREF/TM36) seçerek ya da
GDAL'ın komutuyla:

```text
ogr2ogr -t_srs EPSG:5256 yeni.gpkg eski.gpkg
```

İçe alırken dönüştürme Faz 1'de gelecek.

### Koordinat sistemi bildirmeyen dosya

Yanında `.prj` dosyası olmayan bir Shapefile ya da koordinat sistemi taşımayan bir DXF,
çizimin kendi sistemiyle okunur ve bu bir **uyarıyla** söylenir. Bir CBS katmanının
bütün koordinatları −180…180 ve −90…90 aralığındaysa bunlar büyük olasılıkla boylam ve
enlemdir. Program bu durumda ayrıca uyarır. Her nesne milimetrelik bir noktaya ezildiği
için hiçbir şey okunamadıysa ret mesajı nedeni söyler. Çizim `YEREL` sistemdeyse küçük
sayılar olağandır ve bu uyarı verilmez.

### Bu kuraldan önce kaydedilmiş çizim

Kural gelmeden önce coğrafi bir sistemle kaydedilmiş bir çizim yine açılır, ama açılırken
sayılarının metre olmadığı söylenir. Sistemin adını değiştirmek bu sayıları düzeltmez:
nesneleri kaynak dosyasından, metre sayan bir sisteme dönüştürüp yeniden aktarın.

## Koordinat hassasiyeti

`koordinat_hassasiyeti` ayarı bir koordinatın kaç ondalıkla **yazılacağını** belirler:
[`KOORDİNAT`](../komutlar/coordinate.md) okumasında ve
[`NOKTALAR`](../komutlar/points.md) ile yazılan nokta listesinde. Varsayılan 3'tür, yani
saklanan milimetrenin kendisi. En çok 3 olabilir: dördüncü ondalık her zaman sıfır
olurdu. Daha az ondalıkta değer, tam sayılarla ve yarımdan uzağa yuvarlanır:

```text
AYAR koordinat_hassasiyeti 2
KOORDİNAT nokta=485320.155,4310220.254
```

```text
Sağa: 485320,16 m   Yukarı: 4310220,25 m   (TUREF/TM36)
```

Bu ayar yalnız yazılanı değiştirir. Saklanan koordinat milimetre olarak kalır.

## TM 3 derece dilimleri

Türkiye'nin 3 derecelik dilimleri ve orta meridyenleri:

| Dilim | Orta meridyen | EPSG |
|---|---|---|
| TM27 | 27° | 5253 |
| TM30 | 30° | 5254 |
| TM33 | 33° | 5255 |
| TM36 | 36° | 5256 |
| TM39 | 39° | 5257 |
| TM42 | 42° | 5258 |
| TM45 | 45° | 5259 |

Ortak projeksiyon parametreleri:

| Parametre | Değer |
|---|---|
| Projeksiyon | Transverse Mercator |
| Ölçek katsayısı | 1.0 |
| Yalancı doğu | 500 000 m |
| Yalancı kuzey | 0 |
| Datum | TUREF (ITRF96) |

Bu tablo programın içine gömülü değildir; `data/crs/tm3-dilimleri.json` dosyasında **veri**
olarak durur. Mevzuat değişirse yazılım yeniden derlenmez, veri paketi güncellenir.
Dosyanın kaynağı BÖHHBÜY (Büyük Ölçekli Harita ve Harita Bilgileri Üretim Yönetmeliği),
yayım tarihi 2018-05-26 olarak kayıtlıdır.

## Ekranda koordinat okumak

İmleci harita alanında gezdirdiğinizde durum çubuğu koordinatı metre cinsinden, üç
ondalıkla gösterir:

```text
Sağa (Y) 485337.433   Yukarı (X) 4310246.714
```

## Y sağa, X yukarı

Türk haritacılık konvansiyonunda **Y sağa değer, X yukarı değerdir** — matematikteki
alışkanlığın tersi. Bu bir tercih değil, koordinat sisteminin kendi tanımıdır:
EPSG:5254 (TUREF/TM30) eksenlerini şöyle bildirir:

```text
AXIS["northing (X)", ORDER 1]
AXIS["easting (Y)",  ORDER 2]
```

KentOSCad bu konvansiyona uyar: gördüğünüz her etikette Y sağa değeri, X yukarı değeri
gösterir.

Komut satırına ve betiğe **sağa değer önce** yazılır:

```text
ÇİZGİ 485320.150,4310220.400
      ─────┬────  ─────┬─────
        sağa (Y)    yukarı (X)
```

Bu sıra yaygın Türk CAD pratiğiyle aynıdır. Dönüşüm yapan kütüphaneler
(PROJ, EPSG:5254) koordinatı **yukarı değer önce** bekler; KentOSCad bu çevrimi sınırda
kendisi yapar, sizin bir şey yapmanız gerekmez.

Yanındaki bölme ölçeği verir: bir ekran pikselinin kaç metreye karşılık geldiği.

```text
1 px = 0.1418 m
```

## Büyük koordinatlar ve ekran titremesi

TUREF/TM3 koordinatları yedi basamaklıdır. Böyle bir sayı doğrudan ekran hassasiyetine
indirgenirse çizim metrelerce titrer. KentOSCad bunu, koordinatları ekrana göndermeden önce
görünüm merkezine göre kaydırarak önler; bu yüzden yakınlaştırdığınızda çizgiler yerinde
durur.

## Koordinat dönüşümü

KentOSCad dönüşüm için **PROJ** kullanır — otuz yıldır bu işi yapan, üç platformda da
çalışan standart kütüphane. Yedi TUREF dilimi de tanınır (EPSG:5253–5259) ve
dilimler arası dönüşüm çalışır.

Ölçülen gidiş-dönüş hatası milimetrenin çok altındadır; sakladığımız birim
milimetre olduğu için dönüşüm pratikte kayıpsızdır.

### İki tuzak, ikisi de kapatıldı

**Eksen sırası.** EPSG:5254 koordinatı *yukarı değer önce* bekler. KentOSCad sağa
değeri önce saklar. Dönüşüm bu çevrimi sınırda kendisi yapar; siz bir şey yapmazsınız.
Bu çevrim atlanırsa nokta Kuzey Denizi'ne düşer — testle tutuluyor.

**Derece ve milimetre.** Hedef coğrafi bir sistemse (WGS84 gibi) sonuç derecedir.
Dereceyi milimetre olarak saklamak noktayı yüz metre kaydırır, o yüzden KentOSCad bu
durumda çizim geometrisini dönüştürmeyi **reddeder** ve açık bir hata verir.
Coğrafi okuma ekranda ve dışa aktarımda kullanılır, çizimin içinde değil.

### Hata mesajları

| Mesaj | Neden | Çözüm |
|---|---|---|
| `Çizimin koordinat sistemi değişmedi. 'EPSG:4326' coğrafi bir koordinat sistemi: koordinatlarını derece olarak sayar. …` | `AYAR koordinat_sistemi` ile derece sayan bir sistem istendi | Metre sayan bir izdüşüm sistemi seçin, ör. `AYAR koordinat_sistemi TUREF/TM36` |
| `Çizimin koordinat sistemi değişmedi. 'EPSG:2263' koordinatlarını 'US survey foot' birimiyle sayar. …` | Metre dışında bir birim sayan bir sistem istendi | Metre sayan bir izdüşüm sistemi seçin |
| `Çizim bu sisteme oturtulamaz. …` | `OTURT sistem=` derece ya da başka bir birim sayan bir sistem gösteriyor | `sistem=` için metre sayan bir sistem verin |
| `'yollar.gpkg' dosyasının 'yollar' katmanı içe alınmadı. 'EPSG:4326' coğrafi bir koordinat sistemi …` | İçe alınan katman derece sayıyor | Dosyayı metre sayan bir sisteme dönüştürüp yeniden alın (yukarıdaki `ogr2ogr` satırı) |
| `'cizim.dxf' içe alınmadı. …` | DXF'in yanındaki `.prj` derece sayan bir sistem bildiriyor | `.prj` yanlışsa düzeltin; doğruysa DXF'i metre sayan bir sisteme dönüştürün |
| `… okunabilir çizgi ya da alan içermiyor; … büyük olasılıkla boylam ve enlem (derece) …` | Sistem bildirmeyen dosyanın derece sayıları metre okunup ezildi | Dosyanın sistemini bulun, metre sayan bir sisteme dönüştürüp yeniden aktarın |
| `Çizimin koordinat sistemi metre saymıyor. …` (açılışta uyarı) | Çizim, bu kuraldan önce coğrafi bir sistemle kaydedilmiş | Nesneleri kaynak dosyasından doğru sistemde yeniden aktarın |

### Henüz gelmemiş olanlar

| Yetenek | Ne zaman |
|---|---|
| Derece ya da başka bir sistemdeki dosyayı içe alırken dönüştürme | Faz 1 |
| Türkiye Jeoit Modeli ile ortometrik yükseklik | Faz 1 |
| ED50 / UTM 6° ve ITRF ↔ ED50 bölgesel dönüşüm | Faz 1 |
| TKGM referans koordinatlarıyla doğrulama | referans veri geldiğinde |
| TUSAGA-Aktif / CORS-TR, RINEX, NTRIP | Faz 2 |
| Epok ve hız alanı yönetimi | Faz 2 |
| Poligon, nirengi ve GNSS baz dengelemesi | Faz 2 |

## Sırada ne var

- [Betik yazma](../betik/README.md) — betikte milimetre kullanmak
- [Sözlük](../sozluk.md) — TUREF, ITRF96, ED50 ve diğer terimler
