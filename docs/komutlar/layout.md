# PAFTA — Sayfa Düzeni

Çizimini kâğıda dökecek herkes için; bu sayfayı bitirdiğinizde bir pafta açmayı,
kâğıdını ve yönünü seçmeyi, adını değiştirmeyi ve silmeyi bileceksiniz.

## Ne yapar

Bir **pafta**, çiziminizin basılacağı sayfa düzenidir: kâğıt boyu, yönü, kenar
boşluğu ve üzerine yerleştirilmiş öğeler — harita çerçevesi, başlık, ölçek çubuğu,
kuzey oku, lejant. `PAFTA` bu sayfaların kendisini yönetir; üzerindeki öğeleri
[`PAFTAÖĞE`](layout_item.md) yönetir.

**Pafta çizimle birlikte kaydedilir.** Yazdırma profillerinden farkı budur: bir
profil bu bilgisayarın ayarıdır, bir pafta ise teslim edilen işin parçasıdır.
Dosyayı bir meslektaşınıza gönderdiğinizde pafta da gider, çizimin parmak izine
(`content_hash`) girer ve her düzenlemesi **tek `Ctrl+Z` ile geri alınır**.

Yeni bir pafta boş bir sayfa değildir: içinde bir harita çerçevesi, bir başlık, bir
ölçek çubuğu ve bir kuzey oku ile gelir. Hepsi taşınabilir, değiştirilebilir ve
silinebilir; amaç ilk anda basılabilir bir şey görmenizdir.

## Adlar

| Ad | Açıklama |
|---|---|
| `PAFTA` | Türkçe birincil ad |
| `LAYOUT` | İngilizce karşılığı |
| `PFT` | Kısaltma |

## Sözdizimi

```
PAFTA islem=listele
PAFTA islem=ekle ad=<ad> [kagit=A4] [yon=dikey] [kenar=10] [dpi=300]
PAFTA islem=ekle ad=<ad> kagit=ozel genislik=<mm> yukseklik=<mm>
PAFTA islem=sil ad=<ad>
PAFTA islem=ad ad=<ad> yeni_ad=<ad>
PAFTA islem=sayfa ad=<ad> kagit=A3 yon=yatay [kenar=<mm>]
```

## Parametreler

| Parametre | Zorunlu | Anlamı |
|---|---|---|
| `islem` | evet | `listele`, `ekle`, `sil`, `ad`, `sayfa` |
| `ad` | `listele` dışında | Paftanın adı. Türkçe katlamayla tekildir: `Ada 1284` ile `ada 1284` aynı paftadır |
| `yeni_ad` | `islem=ad` için | Paftanın yeni adı |
| `kagit` | hayır | `A5`, `A4`, `A3`, `A2`, `A1`, `A0` ya da `ozel` (varsayılan `A4`) |
| `genislik`, `yukseklik` | `ozel` için | Sayfa boyu, milimetre |
| `yon` | hayır | `dikey` ya da `yatay` (varsayılan `dikey`) |
| `kenar` | hayır | Kenar boşluğu, milimetre (varsayılan 10) |
| `dpi` | hayır | Çıktı çözünürlüğü (varsayılan 300) |

`islem=ekle` var olan bir adı **değiştirir**, yenisini eklemez — `YAZDIRMAPROFİLİ`
ve `YAPAYZEKAMODELİ` ile aynı davranış.

## Örnekler

### Komut satırı

Bir ada için A3 yatay pafta:

```
PAFTA islem=ekle ad="Ada 1284" kagit=A3 yon=yatay
```

Program şunu yazar:

```
Pafta: Ada 1284 — A3 420×297 mm, yatay, 4 öğe
```

Çizimdeki paftaları listelemek:

```
PAFTA islem=listele
```

Kâğıdı büyütmek — öğeler **yerinde kalır**, yeniden ölçeklenmez; üstten 20 mm'de
duran bir başlık A3'te de üstten 20 mm'dedir:

```
PAFTA islem=sayfa ad="Ada 1284" kagit=A2 yon=yatay kenar=15
```

Kuruma özel bir kâğıt:

```
PAFTA islem=ekle ad="Askı Paftası" kagit=ozel genislik=700 yukseklik=500
```

### Arayüz

Araç çubuğundaki **yazdırma düğmesinin yanındaki ok** hem yazdırma profillerini hem
çizimdeki **paftaları** listeler. Listeden bir pafta seçtiğinizde:

1. Tuval, o paftanın **harita çerçevesinin en-boy oranında** bir seçme çerçevesi açar
   — çerçevelediğiniz alan haritanın göstereceği alandır, kâğıdın tamamı değil.
2. Alanı sürükleyip bıraktığınızda **pafta tasarımcısı açılır** ve harita çerçevesi
   o alana bakıyor olur.

Aynı listenin altındaki **Yeni pafta…** bir ad sorar, A3 yatay bir pafta kurar ve
tasarımcıyı açar. Her adım `PAFTA` ve `PAFTAÖĞE` satırları olarak geçer: komut
günlüğünde görünür, tek `Ctrl+Z` ile geri alınır.

### Betik

```json
{ "cmd": "core.layout",
  "args": { "islem": "ekle", "ad": "Ada 1284", "kagit": "A3", "yon": "yatay" } }
```

## Geri alma

Her `PAFTA` çağrısı tek bir işlemdir ve tek `Ctrl+Z` ile geri alınır: silinen pafta
bütün öğeleriyle geri gelir, değiştirilen kâğıt eski boyuna döner. Pafta listesi
bütün hâlinde geri yüklenir, tek tek öğe olarak değil — bir paftanın adı değiştiğinde
ya da bir öğe silindiğinde dizinlerin kayması bunu zorunlu kılar.

## Betikten kullanım

Paftalar çizimin içinde durduğu için bir betik önce çizimi açar, sonra paftayı kurar:

```json
[
  { "cmd": "core.open", "args": { "yol": "ada1284.pcad" } },
  { "cmd": "core.layout", "args": { "islem": "ekle", "ad": "Ada 1284",
                                    "kagit": "A3", "yon": "yatay" } },
  { "cmd": "core.layout_item", "args": { "islem": "ayarla", "ad": "harita",
                                         "olcek": 1000, "izgara": "cizgi" } },
  { "cmd": "core.save", "args": {} }
]
```

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Pafta adı gerekir: ad=<ad>` | `listele` dışında bir işlem adsız çağrıldı | `ad=` ekleyin |
| `Pafta yok: 'X'.` | O adda bir pafta bulunamadı | `PAFTA islem=listele` ile adları görün |
| `Tanınmayan kâğıt: 'X'. Kâğıtlar: A5, A4, A3, A2, A1, A0, ozel.` | Kâğıt adı tabloda yok | Listedeki adlardan birini yazın ya da `ozel` kullanın |
| `ozel kâğıt için genislik ve yukseklik milimetre olarak verilmeli (sıfırdan büyük).` | `kagit=ozel` verildi ama boy verilmedi | `genislik=` ve `yukseklik=` ekleyin |
| `Kenar boşluğu sayfanın içinde kalmalı: 0 ile N mm arası.` | Kenar boşluğu sayfayı yutuyor | Daha küçük bir `kenar=` verin |
| `Yeni ad gerekir: yeni_ad=<ad>` | `islem=ad` çağrıldı ama yeni ad yok | `yeni_ad=` ekleyin |
| `'X' paftasında iki öğe aynı adı taşıyor: 'Y'.` | Öğe adları tekil olmalı | Öğelerden birini yeniden adlandırın |

## İlgili

- [`PAFTAÖĞE`](layout_item.md) — paftanın üzerindeki öğeler
- [`YAZDIR`](print.md) — çizimi doğrudan kâğıda dökmek
- [`YAZDIRMAPROFİLİ`](print_profile.md) — kâğıt profilleri
