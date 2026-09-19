# ÇIKTIÖĞE — Çıktı Yerleşimi Öğeleri

Çıktı yerleşimini kuran herkes için; bu sayfayı bitirdiğinizde bir yerleşime harita
çerçevesi, başlık, ölçek çubuğu, kuzey oku ve lejant eklemeyi, bunları taşımayı ve
haritanın nereye bakacağını söylemeyi bileceksiniz.

## Ne yapar

Bir çıktı yerleşiminin üzerindeki **öğeleri** yönetir. Sekiz tür vardır:

| Tür | Ne çizer |
|---|---|
| `harita` | Çizimin bir penceresi — kendi ölçeği ve koordinat ızgarasıyla |
| `metin` | Başlık, ada/parsel satırı, not, tarih |
| `olcek` | Ölçek çubuğu; ölçeğini yerleşimin haritasından alır |
| `kuzey` | Kuzey oku |
| `lejant` | Hangi gösterimin ne demek olduğu |
| `resim` | Logo ya da taranmış bir damga, dosya yolundan |
| `sekil` | Dikdörtgen, elips ya da çizgi — çerçeveler ve cetveller |
| `tablo` | Bir katmanın öznitelik satırları — başlıklar şemadan, değerler çizimden |

Konumlar **kâğıt milimetresidir** ve sayfanın **sol ÜST köşesinden** ölçülür. Bu,
programın geri kalanından farklıdır: zeminde `Yukarı (X)` yukarı artar, kâğıtta `y`
aşağı artar. Sebebi kâğıdın yukarıdan okunmasıdır; "üstten 20 mm" demek isteyen
kimse eksi sayı yazmak zorunda kalmaz. İki çerçeve yalnız harita öğesinin içinde
buluşur ve çevirme orada yapılır.

## Adlar

| Ad | Açıklama |
|---|---|
| `ÇIKTIÖĞE` | Türkçe birincil ad |
| `CIKTIOGE` | ASCII karşılığı |
| `LAYOUTITEM` | İngilizce karşılığı |
| `ÇÖĞ` / `COG` | Kısaltma |

## Sözdizimi

```
ÇIKTIÖĞE islem=listele [yerlesim=<ad>]
ÇIKTIÖĞE islem=ekle [yerlesim=<ad>] tur=<tür> [ad=<ad>]
ÇIKTIÖĞE islem=sil [yerlesim=<ad>] ad=<ad>
ÇIKTIÖĞE islem=tasi [yerlesim=<ad>] ad=<ad> x=<mm> y=<mm> genislik=<mm> yukseklik=<mm>
ÇIKTIÖĞE islem=ayarla [yerlesim=<ad>] ad=<ad> [metin=<yazı>] [olcek=<N>]
         [pencere=x1,y1 pencere=x2,y2] [izgara=<biçim>] [kilit=evet] …
```

`yerlesim=` **çizimde tek yerleşim varsa gerekmez**. İki ya da daha fazlası varsa
zorunludur: hangisinin kastedildiğini tahmin etmek, yanlış yerleşimin düzenlenmesidir.


### Eski ad: `pafta=`

Bu parametrenin adı önceden `pafta` idi. Program **eski adı okur, yeni adı yazar**:
eskiden yazılmış bir betik ya da komut günlüğü aynı işi yapmaya devam eder, ama
programın ürettiği her satır `yerlesim=` der. İkisini bir arada vermek hatadır —
tek argümanın iki yazımı, hangisinin kastedildiğini bilmeyen bir çağrıdır.

## Parametreler

| Parametre | Anlamı |
|---|---|
| `islem` | `listele`, `ekle`, `sil`, `tasi`, `ayarla` |
| `yerlesim` | Hangi yerleşim; tek yerleşim varsa gerekmez |
| `ad` | Öğenin adı. `ekle`'de verilmezse türünden türetilir (`harita`, `harita2`…) |
| `tur` | `islem=ekle` için: `harita`, `metin`, `olcek`, `kuzey`, `lejant`, `resim`, `sekil`, `tablo` |
| `x`, `y` | Sol ve **üst** kenardan uzaklık, milimetre |
| `genislik`, `yukseklik` | Öğenin boyu, milimetre |
| `metin` | Metin öğesinin yazısı; resim öğesinde dosya yolu, tablo öğesinde katman adı |
| `yazi` | Yazı yüksekliği, milimetre |
| `olcek` | Harita öğesinin ölçeği `1:N`. `0` = ölçek pencereye uyar |
| `pencere` | Haritanın bakacağı alanın iki köşesi, anahtar **iki kez** yazılarak |
| `izgara` | `yok`, `arti`, `cizgi`, `centik` |
| `izgara_aralik` | Izgara aralığı, zemin milimetresi; `0` ölçeğe göre seçilir |
| `kilit` | Öğeyi taşımaya kapatır |
| `cerceve` | Öğenin çevresine çerçeve çizer |
| `sira` | Çizim sırası; büyük olan üstte |

### Tablo öğesi

`metin=` tablonun **katman adıdır**. Sütunlar o katmanın şemasından gelir; yalnız
bazılarını istiyorsanız — bu tur henüz komut satırından değil, dosyadan — öğenin
sütun listesi kullanılır. `satir_siniri` verilmezse kutuya kaç satır sığıyorsa o
kadarı yazılır ve **sığmayanlar sayılarak bildirilir**: sessizce ilk on bir parseli
gösteren bir tablo, eksiksiz sanılarak dosyalanan bir tablodur.

### Metin yer tutucuları

Bir metin öğesinin yazısında şunlar **çizim anında** çözülür ve asla çözülmüş hâlde
saklanmaz — ölçek değiştiğinde yeniden bastığınız yerleşim yeni ölçeği yazar:

`<yerlesim>` · `<proje>` · `<olcek>` · `<tarih>` · `<crs>` · `<kagit>`

Eski `<pafta>` yer tutucusu da çözülmeye devam eder: bir çizimin antedine yazılmış yazı, programın bir sözcük hakkında fikir değiştirmesiyle bozulmaz.

### Ölçek mi pencere mi

İkisi birlikte çalışır ama **bildirilen ölçek kazanır**:

- `olcek=0` (varsayılan): pencere neredeyse odur; ölçek ondan hesaplanır.
- `olcek=1000`: çerçevenin kâğıt boyu ölçekle çarpılır ve pencerenin **merkezine**
  oturtulur. Bu yüzden 1:1000 bir yerleşimin kâğıdını büyütmek daha ÇOK zemin gösterir,
  aynı zemini küçültmez — bir harita ölçeğinin anlamı budur.

## Örnekler

### Komut satırı

Yerleşimdeki öğeleri görmek:

```
ÇIKTIÖĞE islem=listele
```

Haritayı bir alana bakacak şekilde hedeflemek — köşeler **metre** cinsindendir ve
anahtar iki kez yazılır:

```
ÇIKTIÖĞE islem=ayarla ad=harita pencere=485200,4310100 pencere=485420,4310200
```

Haritayı 1:1000'e sabitlemek ve çizgi ızgarası vermek:

```
ÇIKTIÖĞE islem=ayarla ad=harita olcek=1000 izgara=cizgi izgara_aralik=50000
```

Lejant eklemek ve sağ üste koymak:

```
ÇIKTIÖĞE islem=ekle tur=lejant ad=lejant
ÇIKTIÖĞE islem=tasi ad=lejant x=300 y=40 genislik=80 yukseklik=60
```

Başlığı yazmak:

```
ÇIKTIÖĞE islem=ayarla ad=baslik metin="<yerlesim> — <olcek> — <tarih>"
```

### Arayüz

**Çıktı yerleşimi tasarımcısında** (araç çubuğu ▸ yazdırma oku ▸ bir yerleşim) sol
sütun öğeleri çizim sırasına göre listeler, orta sütun sayfayı gösterir, sağ sütun
seçili öğenin özelliklerini taşır.

- Bir öğeye tıklamak seçer; **sürüklemek taşır**, köşe tutamağından çekmek boyutlandırır.
- **Ok tuşları** birer milimetre kaydırır, **Shift+ok** on milimetre.
- Kilitli bir öğenin tutamağı yoktur ve sürüklenmez; kilidi sağdaki anahtardan açarsınız.
- Sol alttaki sekiz simge sekiz öğe türünü ekler.

Her jest **bırakıldığında tek bir komut** yazar — sürükleme boyunca değil. Bu yüzden
sayfanın bir ucundan öbürüne taşıdığınız bir kutu tek `Ctrl+Z` ile eski yerine döner,
dört yüz adımda değil. Yaptığınız her şey komut günlüğünde durur ve bir betiğin
yazabileceği satırlardır.

### Betik

```json
{ "cmd": "core.layout_item",
  "args": { "islem": "ayarla", "ad": "harita", "olcek": 1000, "izgara": "cizgi" } }
```

## Geri alma

Her çağrı tek bir işlemdir: eklenen öğe tek `Ctrl+Z` ile kalkar, taşınan öğe eski
yerine döner. Kilitli bir öğe `islem=tasi` kabul etmez ve nedenini söyler; `ayarla`
ile kilidi açabilirsiniz.

## Betikten kullanım

Bir yerleşimi baştan sona kuran betik:

```json
[
  { "cmd": "core.layout", "args": { "islem": "ekle", "ad": "Ada 1284",
                                    "kagit": "A3", "yon": "yatay" } },
  { "cmd": "core.layout_item", "args": { "islem": "ayarla", "ad": "baslik",
                                         "metin": "<yerlesim> — <olcek>" } },
  { "cmd": "core.layout_item", "args": { "islem": "ayarla", "ad": "harita",
                                         "olcek": 1000, "izgara": "arti" } },
  { "cmd": "core.layout_item", "args": { "islem": "ekle", "tur": "lejant",
                                         "ad": "lejant" } },
  { "cmd": "core.layout_item", "args": { "islem": "tasi", "ad": "lejant",
                                         "x": 300, "y": 40,
                                         "genislik": 80, "yukseklik": 60 } }
]
```

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Çizimde hiç çıktı yerleşimi yok. Önce ÇIKTIYERLEŞİMİ islem=ekle ad=<ad> yazın.` | Ortada yerleşim yok | Önce bir yerleşim açın |
| `Çizimde N çıktı yerleşimi var; hangisi olduğunu yazın: yerlesim=<ad>` | Birden çok yerleşim var | `yerlesim=` ekleyin |
| `Öğe adı gerekir: ad=<ad>` | `ekle` dışında bir işlem adsız çağrıldı | `ad=` ekleyin |
| `'X' yerleşiminde öğe yok: 'Y'.` | O adda öğe bulunamadı | `islem=listele` ile adları görün |
| `'X' yerleşiminde 'Y' adlı bir öğe zaten var.` | Öğe adları tekildir | Başka bir ad verin |
| `'X' kilitli; önce kilidi açın: ÇIKTIÖĞE islem=ayarla ad=X kilit=hayır` | Kilitli öğe taşınmak istendi | Kilidi açın |
| `pencere iki köşe ister: pencere=x1,y1 x2,y2` | Anahtar bir kez yazıldı | `pencere=` anahtarını **iki kez** yazın |
| `'X' bir harita çerçevesi değil; pencere yalnız haritaya verilir.` | `pencere=` harita olmayan bir öğeye verildi | Harita öğesinin adını verin |
| `Izgara: yok / arti / cizgi / centik` | Tanınmayan ızgara biçimi | Listedeki sözcüklerden birini yazın |
| `'X' ve 'Y' aynı parametrenin iki adı; ikisi birden verilmez. Yeni adı 'Z'.` | Bir parametrenin eski ve yeni adı birlikte verildi | Yalnız yeni adı bırakın |

## İlgili

- [`ÇIKTIYERLEŞİMİ`](layout.md) — yerleşimin kendisi
- [`YAZDIR`](print.md) — çizimi doğrudan kâğıda dökmek
