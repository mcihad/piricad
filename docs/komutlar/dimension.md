# ÖLÇÜ — Ölçülendirme

## Ne yapar

İki nokta arasını, bir yarıçapı, çapı ya da açıyı **ölçer ve çizer**: uzatma çizgileri,
ölçü çizgisi, oklar ve ölçülen değerin yazısı. Sonuç bir [ölçü nesnesidir](../nesneler/olcu.md);
resim saklanmaz, her seferinde tanım noktalarından kurulur, DXF'e `DIMENSION` olarak gider.

Ölçünün ok boyu, uzatma çizgileri, yazı yüksekliği, ondalık sayısı ve ondalık ayracı bir
**ölçü stilinden** gelir. Stiller koddan değil `data/catalogs/dxf/olcu-stili.json`
dosyasından okunur (`TERCİH ölçü_stilleri`): `ISO-25` (2,5 mm ok, iki ondalık, virgül),
`ISO-18` ve `ISO-35` (1,8 ve 3,5 mm yazı), `STANDARD` (AutoCAD; dört ondalık, nokta),
`MIMARI` (45° çentik). `stil=` verilmezse projenin varsayılanı
([`AYAR ölçü_stili`](setting.md), başlangıçta `ISO-25`) kullanılır;
[`ÖLÇÜSTİLİ`](dimension_style.md) hepsini boylarıyla listeler.
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

### Yazı: ölçülen değer ve yazılışı

Ölçünün **ölçtüğü** değer noktalardan hesaplanır ve hep saklanır; **yazısı** bu
değerin yazılışıdır:

| Ne | Örnek | Nasıl verilir |
|---|---|---|
| Önek, sonek | `R12,50 m` | `onek=R sonek=" m"` |
| Birim | `1250,00` (cm) | `birim=cm`; varsayılan çizimin birimi |
| Ondalık | `12,500` | `hassasiyet=3`; varsayılan stilinki |
| Simetrik tolerans | `12,50±0,05` | `tolerans=0.05` (metre; açıda derece) |
| Sapma | `12,50+0,05/-0,02` | `tolerans_ust=0.05 tolerans_alt=0.02` |
| Sınır değerler | `12,55/12,48` | `tolerans_bicim=sinir` |
| Yazı şablonu | `12,50 (tapu)` | `metin="<> (tapu)"`: `<>` ölçülen değerdir |
| **Elle yazılmış değer** | `12,48` | `metin="12,48"`: `<>` taşımayan yazı |

**Elle yazılmış değer ölçülen değer diye geçmez.** Tuvalde yazının altında uyarı
renginde **elle yazılmış · ölçülen 12,50** durur (pafta çıktısına girmez),
[`NESNEBİLGİ`](entity_info.md) ikisini yan yana söyler, nitelik panelinde yazı
satırı **ELLE** rozeti taşır; önek, sonek ve tolerans elle yazılmış değere
eklenmez. Bağlı ölçünün köşesi taşınınca yeni değer ölçülür ama elle yazılmış yazı
değişmez ve bu söylenir. Çizilmiş bir ölçünün bunlarını
[`ÖLÇÜDÜZENLE`](dimension_edit.md) değiştirir.

### Pafta ölçeği

Stilin ölçüleri **kâğıttadır**; ÖLÇÜ onları plan ölçeğiyle zemine indirir ve ölçü
**hangi pafta ölçeği için boyutlandığını** saklar. Başka bir ölçekte basılacak
paftada ölçülerin kâğıtta aynı boyda okunması için
[`ÖLÇÜYENİLE`](dimension_refresh.md) onları o ölçeğe uyarlar. Plan ölçeği
değişince ve [`YAZDIR`](print.md) başka bir ölçekte basarken program bunu söyler.

### Bağlı ölçü

Ölçünün bir noktası bir nesnenin **tam** üzerine düşüyorsa — bir köşe, bir dairenin ya da
yayın merkezi, bir yayın ucu; yarıçap ve çap ölçüsünde çemberin üstü — ölçü o nesneye
**bağlanır** ve ÖLÇÜ bunu söyler: `2 noktası ölçtüğü nesneye bağlı`. Yakalamayla
tıklanan, elle yazılan ve betikten verilen köşe aynı milimetredir; üçü de aynı bağı
kurar. Boşlukta bir noktaya çizilen ölçü bağsızdır.

Bağlı ölçü ölçtüğü şeyi izler:

| Ne olursa | Ölçü ne yapar |
|---|---|
| Ölçtüğü köşe ya da yay taşınır (`KÖŞETAŞI`, `TAŞI`, `DÖNDÜR`, `ÖLÇEKLE`, `ESNET` …) | Yeniden kurulur ve yeniden ölçülür. Hizalı ölçünün çizgisi kenardan aynı uzaklıkta ve aynı yakada kalır; kenarla birlikte döner |
| Nesneye köşe eklenir, köşesi silinir, çizgi ters çevrilir | Bağ **köşenin kendisini** izler, sıra numarasını değil; ölçü yerinden oynamaz |
| Ölçtüğü köşe silinir | O bağ **kopar** |
| Ölçtüğü nesne silinir | Bağ **kopar**; ölçü yerinde durur ve yazdığı değer nesnenin eski ölçüsüdür |
| Nesne aynı komutta başka nesneye dönüşür (`UÇUCA`, `PATLAT`, `BİRLEŞTİR`) | Bağ, aynı noktayı taşıyan yeni ya da yeniden biçimlenen nesneye geçer |
| Ölçü ve ölçtüğü nesne birlikte taşınır | Hiçbir şey değişmez; bağ sürer |
| Ölçünün kendi noktası elle taşınır, nesne yerinde durur | O nokta bağından **çözülür**; ölçü artık o ucu izlemez |
| Ölçü kilitli katmandadır | İzleyemez ve bunu söyler; bağ durur, kilit açıldıktan sonraki ilk değişiklikte yetişir |

İzleme, onu doğuran komutla **aynı geri alma adımındadır**: köşeyi geri alan
[`GERİAL`](undo.md) ölçüyü de eski hâline getirir.

**Kopuk bağ görünür.** Tuvalde, bağı kopan her tanım noktasında uyarı renginde üstü çizili
bir halka ve **bağ koptu** yazısı durur; bu işaret yazdırılan paftaya çıkmaz.
[`NESNEBİLGİ`](entity_info.md) bir ölçünün hangi noktasının hangi nesnenin hangi
köşesine bağlı olduğunu, bir nesne için de onu kaç bağlı ölçünün ölçtüğünü söyler —
bir nesneyi silmeden önce sorulacak soru budur.

Bağlamak istemediğinizde `bagla=hayır` verin. [`KOPYALA`](copy.md) ile çoğaltılan ölçü
bağsızdır: kopya, kopyalanan nesneye bağlanmaz.

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
| `bitis` | Yay uzunluğu ölçüsünde yayın bitiş noktası (başlangıçtan saat yönünün tersine) |
| `stil` | Katalogdaki ölçü stili; varsayılan projenin `ölçü_stili` ayarı (başlangıçta `ISO-25`) |
| `metin` | Yazı; `<>` ölçülen değerdir, `<>` taşımayan metin elle yazılmış sayılır |
| `katalog` | Stil kataloğu dosyası; varsayılan `TERCİH ölçü_stilleri` |
| `bagla` | Tam denk geldiği köşeye, merkeze ya da yay ucuna bağlansın mı; varsayılan `evet`. Bkz. [Bağlı ölçü](#bağlı-ölçü) |
| `onek`, `sonek` | Değerin önüne ve ardına yazılanlar. Bkz. [Yazı](#yazı-ölçülen-değer-ve-yazılışı) |
| `birim` | `cizim` (varsayılan), `mm`, `cm`, `m`, `km` |
| `hassasiyet` | Ondalık basamak sayısı, 0–8; varsayılan stilinki |
| `tolerans` | Simetrik tolerans, uzunlukta metre, açıda derece |
| `tolerans_ust`, `tolerans_alt` | Sapma; ikisi de pozitif yazılır |
| `tolerans_bicim` | `simetrik`, `sapma`, `sinir` |

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

Bir parselin kenarını ölçüp köşesini taşımak; ölçü köşeyi izler ve yeniden ölçülür:

<!-- örnek: yeni çizim -->
```
ALAN 0,0 20,0 20,10 0,10
ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3
KÖŞETAŞI nesne=1 kose=2 nokta=25,0
```

```text
Ölçü çizildi: 20,00 (ISO-25); 2 noktası ölçtüğü nesneye bağlı, o değişince ölçü de güncellenir.
Bağlı 1 ölçü kaynağını izledi ve yeniden ölçüldü.
```

Ölçünün yazısı artık `25,00`; ölçü çizgisi kenarın yine 3 m altında.

Önek, sonek, tolerans ve üç ondalıkla:

<!-- örnek: yeni çizim -->
```
ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3 onek=R sonek=" m" tolerans=0.05 hassasiyet=3
```

```text
Ölçü çizildi: R20,000±0,050 m (ISO-25).
```

### Arayüz

**Açıklama ▸ Ölçü ▸ Ölçü** (ya da **Giriş ▸ Açıklama ▸ Ölçü**). İki noktayı, sonra ölçü
çizgisinin yerini tıklayın. Noktaları
yakalamayla bir köşeye ya da merkeze oturtursanız ölçü oraya bağlanır; köşeyi tutamağından
sürüklediğinizde ölçü onunla birlikte güncellenir.

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

Tek adımdır: `GERİAL` ölçüyü de bağlarını da kaldırır. Bağlı bir ölçünün kaynağını
izlemesi, onu doğuran komutla aynı adımdadır; o komutu geri alan `GERİAL` ölçüyü de geri
alır, `YİNELE` ikisini birlikte yineler.

## Betikten kullanım

Günlüğe noktalar, tür, kullanılan stil ve varsa metin yazılır; ölçülen değer yazılmaz,
tekrar yeniden ölçer. Bağlar günlüğe yazılmaz, çünkü noktalardan yeniden bulunurlar:
oynatılan `ÖLÇÜ` aynı köşelere aynı bağları kurar. `bagla=hayır` verilmişse o da yazılır.

Bağlı ölçülerin izlemesi şu satırlarla bildirilir:

| Satır | Ne oldu |
|---|---|
| `Bağlı N ölçü kaynağını izledi ve yeniden ölçüldü.` | Ölçtükleri köşe ya da yay taşındı |
| `Ölçtüğü nesne silindiği için N ölçü bağı koptu; ölçü yerinde duruyor ve artık bir şey ölçmüyor.` | Ölçülen nesne silindi |
| `Ölçtüğü köşe kaldırıldığı için N ölçü bağı koptu; ölçü yerinde duruyor ve artık bir şey ölçmüyor.` | Ölçülen köşe silindi |
| `N ölçü bağı, aynı noktada yerini alan nesneye aktarıldı.` | Nesne aynı komutta başka nesneye dönüştü |
| `N ölçü noktası elle taşındığı için bağından çözüldü.` | Ölçünün kendi noktası taşındı |
| `Bağlı N ölçü kilitli katmanda olduğu ya da yeniden kurulamadığı için kaynağını izleyemedi.` | Ölçü kilitli katmanda, ya da yeni noktalarla kurulamıyor (iki nokta çakıştı) |
| `Bağlı N ölçünün yazısı elle yazılmış; yeniden ölçülen değeri göstermiyor. …` | Ölçü yeniden ölçüldü ama yazısı elle yazılmış; `ÖLÇÜDÜZENLE sifirla=metin` ölçüye döndürür |

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

- [ZİNCİRÖLÇÜ](dimension_continue.md) ve [BAZÖLÇÜ](dimension_baseline.md) — bir ölçüden
  başlayarak aynı çizgide art arda ya da aynı ilk noktadan üst üste ölçüler
- [ÖLÇÜSTİLİ](dimension_style.md) — stiller ve varsayılan
- [ÖLÇÜDÜZENLE](dimension_edit.md) — çizilmiş ölçünün yazısını, birimini, toleransını değiştirmek
- [ÖLÇÜYENİLE](dimension_refresh.md) — ölçüleri başka bir pafta ölçeğine uyarlamak
- [ÖLÇ](measure.md) — çizmeden ölçmek
- [LİDER](leader.md) — oklu not çizgisi
- [Ölçü türü](../nesneler/olcu.md) — saklanış, çizim, DXF eşlemesi
