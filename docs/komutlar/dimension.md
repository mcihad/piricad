# ÖLÇÜ — Ölçülendirme

## Ne yapar

İki nokta arasını, bir yarıçapı, çapı ya da açıyı **ölçer ve çizer**: uzatma çizgileri,
ölçü çizgisi, oklar ve ölçülen değerin yazısı. Sonuç bir [ölçü nesnesidir](../nesneler/olcu.md);
resim saklanmaz, her seferinde tanım noktalarından kurulur, DXF'e `DIMENSION` olarak gider.

Ölçünün ok boyu, uzatma çizgileri, yazı yüksekliği, ondalık sayısı ve ondalık ayracı bir
**ölçü stilinden** gelir. Stiller koddan değil `data/catalogs/dxf/olcu-stili.json`
dosyasından okunur (`TERCİH ölçü_stilleri`): `ISO-25` (varsayılan; 2,5 mm ok, iki
ondalık, virgül), `STANDARD` (AutoCAD; dört ondalık, nokta), `MIMARI` (45° çentik).
Değerler **kâğıt** mikrometresidir ve [`AYAR plan_ölçeği`](setting.md) ile zemine iner:
2,5 mm ok 1/1000 paftada 2,5 m'dir. Yazı [`AYAR çizim_birimi`](setting.md)
birimindedir: 12 500 mm metrede `12,50`.

### Türler

| `tur` | Ne ölçer | Noktalar |
|---|---|---|
| `hizali` (varsayılan) | İki nokta arasındaki uzaklığı, kendi doğrultusunda | `birinci`, `ikinci`, `konum` |
| `dogrusal` | Yatay ya da düşey uzaklığı; ölçü çizgisi noktaların üstünde/altındaysa yatay, yanındaysa düşey | `birinci`, `ikinci`, `konum` |
| `yaricap` | Merkezden çember üstü noktaya | `birinci` merkez, `ikinci` çember üstü, `konum` yazı yeri |
| `cap` | İki karşı nokta arasını | `birinci`, `ikinci`, `konum` yazı yeri |
| `acisal` | Tepeden çıkan iki kol arasındaki açıyı | `birinci`, `ikinci` kol uçları, `tepe`, `konum` yayın geçtiği nokta |
| `koordinat` | Bir noktanın başlangıçtan **sağa** ya da **yukarı** değerini | `birinci` başlangıç, `ikinci` ölçülecek nokta, `konum` yazının yeri |
| `yay` | Bir yayın **boyunca** uzunluğunu | `birinci` merkez, `ikinci` başlangıç, `bitis` bitiş, `konum` yazının yeri |

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `ÖLÇÜ` | `OLCU` | `DIMENSION` | `ÖÇ` |

## Sözdizimi

```text
ÖLÇÜ birinci=<sağa>,<yukarı> ikinci=<sağa>,<yukarı> konum=<sağa>,<yukarı> [tur=<tür>] [stil=<ad>] [metin=<yazı>]
ÖLÇÜ birinci=<sağa>,<yukarı> ikinci=<sağa>,<yukarı> tepe=<sağa>,<yukarı> konum=<sağa>,<yukarı> tur=acisal
```

### Koordinat (ordinat) ölçüsü

Bir Türk aplikasyon paftasında bir binanın köşeleri bir ordinat tablosuyla
verilir: her köşenin bir başlangıç noktasından **sağa** ve **yukarı** değerleri.
`tur=koordinat` bunu tek tek çizer.

**Ekseni jest belirler.** Yazıyı noktadan **yana** çekerseniz sağa değerini,
**yukarı/aşağı** çekerseniz yukarı değerini okur. Bir ordinat tablosu tam böyle
kurulur ve bu size fazladan bir cevap maliyeti çıkarmaz.

Başlangıcın batısındaki bir nokta **eksi** okur; işaret cevabın parçasıdır,
çünkü artı yazmak köşeyi paftanın öbür tarafına koymak olur.

### Yay uzunluğu ölçüsü

`tur=yay` yayın **boyunca** uzunluğu verir, üzerindeki kirişi değil. 50 metre
yarıçaplı bir çeyrek yay **boyunca 78,540 m**, kirişi ise **70,711 m**'dir —
kirişi basan bir pafta yanlış boyda bordür sipariş ettirir.

Bir yol geçiş eğrisi, bir bordür dönüşü ve bir boru dirseği hep tekerleğin
katettiği mesafeyle ölçülendirilir.

DXF'te R2007 öncesi bir yay uzunluğu ölçüsü yoktur; dışa aktarımda **açısal**
ölçü olarak yazılır (aynı üç nokta: yay, iki yarıçapı ve bir yazı) ve uzunluğun
kendisi xdata'da gider — yani bu programdan çıkıp geri girince aynen korunur,
başka bir program ise bir açı görüp onu söyler, bir kiriş görüp ona inanmaz.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `birinci`, `ikinci` | Ölçülen iki nokta; açısalda kolların uçları |
| `konum` | Ölçü çizgisinin yeri; yarıçap ve çapta yazının yeri; açısalda yayın geçtiği nokta |
| `tur` | `hizali`, `dogrusal`, `yaricap`, `cap`, `acisal`, `koordinat`, `yay`; varsayılan `hizali` |
| `tepe` | Açısal ölçünün tepe noktası |
| `stil` | Katalogdaki ölçü stili; varsayılan `ISO-25` |
| `metin` | Ölçülen değer yerine yazılacak metin |
| `katalog` | Stil kataloğu dosyası; varsayılan `TERCİH ölçü_stilleri` |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```text
KATMAN ad=OLCULER
ÖLÇÜ birinci=0,0 ikinci=12.5,0 konum=0,3
```

Yazısı `12,50`, ölçü çizgisi noktaların 3 m üstünde.

```text
ÖLÇÜ birinci=20,0 ikinci=23,7 konum=30,3 tur=dogrusal
ÖLÇÜ birinci=40,0 ikinci=45,0 konum=42.5,1 tur=yaricap
ÖLÇÜ birinci=60,0 ikinci=60,10 tepe=70,0 konum=65.4,1.9 tur=acisal
ÖLÇÜ birinci=80,0 ikinci=92.5,0 konum=80,3 stil=MIMARI metin="12,50 m"
```

### Arayüz

**Çizim ▸ Ölçü**. İki noktayı, sonra ölçü çizgisinin yerini tıklayın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "OLCULER" } },
    { "cmd": "core.dimension",
      "args": { "birinci": [0,0], "ikinci": [12500,0], "konum": [0,3000], "tur": "hizali" } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` ölçüyü kaldırır.

## Betikten kullanım

Günlüğe noktalar, tür, kullanılan stil ve varsa metin yazılır; ölçülen değer yazılmaz,
tekrar yeniden ölçer.

## Hatalar

> `Tanınmayan ölçü türü: 'yatay'. Türler: hizali, dogrusal, yaricap, cap, acisal, koordinat, yay.`

`tur` listede yok.

> `İki nokta aynı; ölçülecek bir uzunluk yok.`

`birinci` ve `ikinci` çakışıyor.

> `Tanınmayan ölçü stili: 'DIN'. Katalogdaki stiller: ISO-25, STANDARD, MIMARI.`

Stil katalogda yok.

> `Ölçü stili kataloğu bulunamadı: 'data/catalogs/dxf/olcu-stili.json'. TERCİH ölçü_stilleri ile yolunu kurun ya da katalog= verin.`

Katalog dosyası bu makinede yok.

## İlgili

- [ÖLÇ](measure.md) — çizmeden ölçmek
- [LİDER](leader.md) — oklu not çizgisi
- [Ölçü türü](../nesneler/olcu.md) — saklanış, çizim, DXF eşlemesi
