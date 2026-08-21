# Koordinat Sistemleri

Türkiye'de ölçüm ve çizim yapan harita mühendisi için; bu sayfayı bitirdiğinizde PiriCAD'in
koordinatları nasıl sakladığını, hangi dilimlerle çalıştığını ve bugün neyin yapılıp
neyin yapılamadığını bileceksiniz.

## Koordinatlar milimetre olarak saklanır

PiriCAD bütün koordinatları **64 bitlik tam sayı milimetre** olarak saklar. Ondalıklı
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

Yeni çizim **TUREF/TM30** ile açılır: TUREF datumu, 30° orta meridyenli 3 derecelik dilim.
Dokümanın koordinat sistemi durum çubuğunun sağ ucunda ve **Öznitelikler** panelinde
yazar.

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
X 485337.433   Y 4310246.714
```

Yanındaki bölme ölçeği verir: bir ekran pikselinin kaç metreye karşılık geldiği.

```text
1 px = 0.1418 m
```

## Büyük koordinatlar ve ekran titremesi

TUREF/TM3 koordinatları yedi basamaklıdır. Böyle bir sayı doğrudan ekran hassasiyetine
indirgenirse çizim metrelerce titrer. PiriCAD bunu, koordinatları ekrana göndermeden önce
görünüm merkezine göre kaydırarak önler; bu yüzden yakınlaştırdığınızda çizgiler yerinde
durur.

## Bu sürümde yapılabilenler ve yapılamayanlar

Bugün PiriCAD koordinat sistemini bir **kimlik** olarak taşır: çizimin hangi sistemde
olduğunu bilir, gösterir ve günlüğe yazar. Gerçek koordinat **dönüşümü** henüz yoktur.

Yapılamayanlar ve ne zaman geleceği:

| Yetenek | Ne zaman |
|---|---|
| Dilimler arası dönüşüm (TM30 ↔ TM33 gibi) | Faz 1, PROJ ile |
| ED50 / UTM 6° ve ITRF ↔ ED50 bölgesel dönüşüm | Faz 1 |
| Türkiye Jeoit Modeli ile ortometrik yükseklik | Faz 1 |
| TUSAGA-Aktif / CORS-TR, RINEX, NTRIP | Faz 2 |
| Epok ve hız alanı yönetimi | Faz 2 |
| Poligon, nirengi ve GNSS baz dengelemesi | Faz 2 |

Dönüşüm kütüphanesi PROJ'dur ve `PIRICAD_WITH_PROJ` seçeneğinin arkasındadır; bugün
kapalıdır. Devreye girdiğinde doğruluğu TKGM referans koordinatlarıyla karşılaştırılarak
sınanacak. Ayrıntı: `CLAUDE.md` Article 8.2.

## Sırada ne var

- [Betik yazma](../betik/README.md) — betikte milimetre kullanmak
- [Sözlük](../sozluk.md) — TUREF, ITRF96, ED50 ve diğer terimler
