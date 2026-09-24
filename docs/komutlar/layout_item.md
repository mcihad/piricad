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
| `grafik` | Bir öznitelik sütununa göre nesne sayısı: çubuk grafik |
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
ÇIKTIÖĞE islem=ad [yerlesim=<ad>] ad=<ad> yeni_ad=<ad>
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
| `islem` | `listele`, `ekle`, `sil`, `tasi`, `ayarla`, `ad` |
| `yerlesim` | Hangi yerleşim; tek yerleşim varsa gerekmez |
| `ad` | Öğenin adı. `ekle`'de verilmezse türünden türetilir (`harita`, `harita2`…) |
| `tur` | `islem=ekle` için: `harita`, `metin`, `olcek`, `kuzey`, `lejant`, `resim`, `sekil`, `tablo`, `grafik` |
| `x`, `y` | Sol ve **üst** kenardan uzaklık, kâğıt milimetresi. **Ondalık yazılabilir**: `x=0.35` |
| `genislik`, `yukseklik` | Öğenin boyu, kâğıt milimetresi. Ondalık yazılabilir |
| `metin` | Metin öğesinin yazısı; resim öğesinde dosya yolu, tablo ve grafik öğesinde katman adı |
| `yazi` | Yazı yüksekliği, kâğıt milimetresi. Ondalık yazılabilir |
| `olcek` | Harita öğesinin ölçeği `1:N`. `0` = ölçek pencereye uyar |
| `pencere` | Haritanın bakacağı alanın iki köşesi, anahtar **iki kez** yazılarak |
| `izgara` | `yok`, `arti`, `cizgi`, `centik` |
| `izgara_aralik` | Izgara aralığı, zemin milimetresi; `0` ölçeğe göre seçilir |
| `kilit` | Öğeyi taşımaya kapatır |
| `cerceve` | Öğenin çevresine çerçeve çizer |
| `sira` | Çizim sırası; büyük olan üstte |
| `satir_siniri` | Tablo öğesinin yazacağı en çok satır; `0` = kutuya sığdığı kadar |
| `sutunlar` | Tablo öğesinin yazacağı öznitelik sütunları, sırasıyla (`hepsi` listeyi boşaltır); **grafik** öğesinde sayımın yapılacağı tek sütun |
| `harita` | Bu öğenin bağlı olduğu harita çerçevesinin adı; `ilk` bağı kaldırır |
| `yeni_ad` | `islem=ad` için öğenin yeni adı |
| `katmanlar` | Harita çerçevesinin çizeceği katmanlar; **anahtar birden çok kez yazılır**. Verilmezse görünür bütün katmanlar, `hepsi` listeyi boşaltır |
| `sayfa` | Öğenin duracağı sayfa (1'den başlar); `tasi` ile verilir |

### Tablo öğesi

`metin=` tablonun **katman adıdır**. Sütunlar o katmanın şemasından gelir;
yalnız bazılarını istiyorsanız `sutunlar=` ile sırasıyla yazın — anahtar birden
çok kez yazılır, `hepsi` listeyi boşaltır:

```
ÇIKTIÖĞE islem=ayarla ad=liste metin=PARSEL sutunlar=ada sutunlar=parsel sutunlar=alan
```

`satir_siniri` verilmezse kutuya kaç satır sığıyorsa o kadarı yazılır ve
**sığmayanlar sayılarak bildirilir** — hem kâğıdın üstünde ("… 79 satır daha
sığmadı") hem de komutun sonucunda. Sessizce ilk on bir parseli gösteren bir
tablo, eksiksiz sanılarak dosyalanan bir tablodur; kâğıdın üstündeki not onu
elinde tutan içindir, sonuçtaki uyarı da diğer herkes için.

### Metin yer tutucuları

Bir metin öğesinin yazısında şunlar **çizim anında** çözülür ve asla çözülmüş hâlde
saklanmaz — ölçek değiştiğinde yeniden bastığınız yerleşim yeni ölçeği yazar:

`<yerlesim>` · `<proje>` · `<olcek>` · `<tarih>` · `<crs>` · `<kagit>`

Eski `<pafta>` yer tutucusu da çözülmeye devam eder: bir çizimin antedine yazılmış yazı, programın bir sözcük hakkında fikir değiştirmesiyle bozulmaz.

### Ondalık milimetre

Kâğıt ölçüleri **ondalık yazılabilir**; model zaten mikrometre saklıyor, yalnız
kapı tam sayıydı:

```
ÇIKTIÖĞE islem=tasi ad=baslik x=0.35 y=12.5 genislik=100.25
```

0,35 mm tam olarak 350 mikrometredir — yuvarlama yok, dosyaya da öyle gider.

### Hangi katmanları çizer

Bir harita çerçevesi varsayılan olarak **görünür bütün katmanları** çizer. Ayrı bir
liste vermek, aynı sayfada aynı zeminin farklı temalarını gösteren iki çerçeve
kurmanın yoludur:

```
ÇIKTIÖĞE islem=ayarla ad=harita katmanlar=parsel katmanlar=bina
```

Anahtar **birden çok kez** yazılır; virgül kullanılmaz, çünkü bir katman adı
doğrulanmıyor ve virgül içerebilir. `katmanlar=hepsi` listeyi boşaltır ve çerçeve
yine bütün görünür katmanları çizer.

**Liste daraltır, genişletmez:** çizimde gizlenmiş bir katman burada adı geçse de
çizilmez.

### Hangi haritaya bağlı

Bir ölçek çubuğu bir haritanın ölçeğini, bir kuzey oku onun dönüşünü, bir lejant
onun çizdiği sembolleri ve bir `<olcek>` yer tutucusu onun paydasını söyler. Tek
haritalı bir sayfada soru yok. **İki harita çerçeveli bir sayfada vardır**, ve
program bunu "ilk bulduğunu al" diye cevaplayamaz: 1:1000 ve 1:5000 iki çerçeve
varken bir haritanın sayısını öbürünün altına basmak olurdu.

```
ÇIKTIÖĞE islem=ayarla ad=olcek harita=harita2
```

Bağ verilmezse **ilk harita** geçerlidir — tek haritalı sayfanın doğru cevabı ve
bu alandan önce yazılmış her yerleşimin söylediği şey. `harita=ilk` bağı kaldırır.

**Bir haritayı yeniden adlandırmak ona bağlı öğeleri koparmaz.** Bağ ada göre
değil kimliğe göre tutulur; dosyaya yazılırken hedefin **yeni** adı yazılır.

```
ÇIKTIÖĞE islem=ad ad=harita2 yeni_ad=kuzeyharita
```

**Bağlı olduğu harita silinirse öğe sessizce ilk haritaya dönmez.** Çizilmez ve
bildirilir: imzalanan bir belgede başka bir haritanın ölçeğini sessizce yazan bir
ölçek çubuğu, yanlış bir sayıdır.

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

Lejant, her katmanın yanına **o katmanın gerçek sembolünü** çizer — düz bir renk
kutusu değil. Sembol, haritayı çizen **aynı** boru hattından geçer (katman ağacındaki
küçük resim de öyle), dolayısıyla anahtar ile harita birbirinden ayrılamaz: taramalı
bir katman anahtarında da taramalı, kesik çizgili bir sınır anahtarında da kesik
çizgili görünür. Kendi anahtarına uymayan bir lejant, hiç lejant olmamasından
kötüdür — bu imzalanan bir belgedir.

Anahtar PDF'e **vektör** olarak gider, resim olarak değil: bir sembolün fotoğrafı
ölçülemez, seçilemez ve çözünürlüğe bağlıdır.

`katmanlar=` verilirse yalnız o katmanlar listelenir; verilmezse **görünür** olanların
hepsi. Sayfada görünmeyen bir katmanı anahtarda saymak, olmayan bir şeyi açıklamaktır.

Grafik eklemek — bir katmanın bir sütununa göre nesne sayıları:

```
ÇIKTIÖĞE islem=ekle yerlesim=Pafta tur=grafik ad=dagilim
ÇIKTIÖĞE islem=ayarla yerlesim=Pafta ad=dagilim metin=PARSEL sutunlar=nitelik
```

`metin=` katmanı, `sutunlar=` sayımın yapılacağı sütunu verir. Her farklı değer bir
çubuk olur ve çubuğun yüksekliği o değeri taşıyan **nesne sayısıdır** — kaç parsel
`Arsa`, kaç parsel `Tarla`. Sayı çubuğun üstünde yazılıdır: okuyucunun bir eksenden
tahmin etmesi gereken çubuk, yanlış tahmin edilecek çubuktur.

Çubuklar **katmanın kendi rengini** alır, böylece grafik ile haritadaki katman aynı şey
olarak okunur; ilgisiz renklerde bir grafik, okuyucunun öğrenmesi gereken ikinci bir
lejanttır.

Değeri olmayan nesneler `(boş)` çubuğunda toplanır — atılmazlar.

> **Kaynağı kullanılamayan bir grafik boş çizmez, sebebini yazar.** Katman verilmemişse,
> katman yoksa, sütun verilmemişse, sütun yoksa ya da sayılacak nesne yoksa: sayfanın
> üstünde kesik çizgili bir kutu ve sebebi görünür, ve aynı cümle `islem=denetle`
> sonucuna ve dışa aktarma uyarılarına girer. İmzalanan bir sayfadaki boş bir dikdörtgen,
> aylar sonra kimsenin cevaplayamayacağı bir sorudur.

Logo koymak — **yolu projeye göre göreli yazın**:

```
ÇIKTIÖĞE islem=ekle tur=resim ad=logo
ÇIKTIÖĞE islem=ayarla ad=logo metin=logo.png
```

Göreli bir yol **projenin klasörüne göre** çözülür. Bu, projeyi bir meslektaşa ya da
sunucuya kopyaladığınızda logonun kaybolmamasının tek yoludur: dosya çizimin yanında
yolculuk eder. Mutlak bir yol (`/Users/ali/…`) yalnız yazıldığı makinede çalışır, ve
taşındığında sayfa logonun yerine kesik çizgili bir kutu basar — **şikâyet etmeden**.

Başlığı yazmak:

```
ÇIKTIÖĞE islem=ayarla ad=baslik metin="<yerlesim> — <olcek> — <tarih>"
```

### Arayüz

**Çıktı yerleşimi tasarımcısında** (**Çıktı ▸ Yazdır ▸ Çıktı Yerleşimleri** ya da hızlı
erişimdeki yazıcının oku ▸ bir yerleşim) sol
sütun sayfayı ve üzerindeki öğeleri, orta sütun kâğıdı, sağ sütun seçili öğenin
ayarlarını taşır.

- Bir öğeye tıklamak seçer; **sürüklemek taşır**, köşe tutamağından çekmek boyutlandırır.
- **Ok tuşları** birer milimetre kaydırır, **Shift+ok** on milimetre.
- Kilitli bir öğenin tutamağı yoktur ve sürüklenmez; kilidi sağdaki anahtardan açarsınız.
- **Sol alttaki dokuz düğme** dokuz öğe türünü ekler: harita, metin, ölçek, kuzey,
  lejant, resim, şekil, tablo ve grafik.
- Kâğıdın üstünde ve solunda **milimetre cetveli** durur. Seçili öğenin kapladığı
  açıklık iki cetvelde de vurgulanır ve sürükleme boyunca onunla birlikte hareket
  eder; kutunun kâğıdın neresinde durduğunu alandaki sayıyı okumadan görürsünüz.
- Öğe listesi kimliği değil **adı** yazar, yanında kutunun `genişlik×yükseklik`
  ölçüsünü verir; komut satırının `ad=` ile andığı kimlik satırın ipucundadır ve
  sağdaki **Ad** alanından değiştirilir.

Sağ sütun **hiçbir zaman boş kalmaz** ve en üstünde neye baktığınızın **adı**
yazar, altında türü, kimliği ve ölçüsü. Bir öğe seçili değilken sayfanın kendi
ayarlarını gösterir — kâğıt, yön, kenar boşluğu, çözünürlük ve yerleşimin adı —
çünkü sayfa her zaman vardır.

Bir öğe seçiliyken `ayarla`nın kabul ettiği her ayar oradadır, iki bölüm hâlinde:

| Bölüm | Ne karara bağlar |
|---|---|
| **Yerleştirme** | Konum ve boyut, öğenin durduğu sayfa, çizim sırası, adı |
| **İçerik** | Kutunun ne gösterdiği: harita için ölçek, ızgara, ızgara aralığı ve katmanlar; tablo için sütunlar ve satır sınırı; ölçek çubuğu, kuzey oku, lejant ve grafik için hangi haritaya bağlı olduğu |

Kuzey okunun İçerik bölümü yoktur: kuzeyi gösterir, hepsi bu.

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
| `'X' bir harita çerçevesi değil; katmanlar yalnız haritaya verilir.` | `katmanlar=` harita olmayan bir öğeye verildi | Harita öğesinin adını verin |
| `Katman yok: 'X'.` | `katmanlar=` çizimde olmayan bir katmanı gösteriyor | `KATMAN islem=listele` ile adları görün |
| `Öznitelik sütunu yok: 'X'.` | `sutunlar=` tanımlı olmayan bir sütunu gösteriyor | `ÖZNİTELİKŞEMASI` ile adları görün |
| `'X' bir tablo değil; satir_siniri yalnız tabloya verilir.` | Tablo olmayan bir öğeye verildi | Tablo öğesinin adını verin |
| `'X' yerleşiminde 'Y' adlı bir harita çerçevesi yok.` | `harita=` olmayan bir öğeyi gösteriyor | `islem=listele` ile harita adlarını görün |
| `Bir harita çerçevesi başka bir haritaya bağlanmaz.` | `harita=` bir harita öğesine verildi | Ölçek, kuzey, lejant ya da metne verin |
| `'X' yerleşiminde 'Y' yeniden adlandırılamadı; öğe yok ya da 'Z' adı kullanımda.` | `islem=ad` çakışan ya da olmayan bir ada çağrıldı | Başka bir ad verin |
| `'X' yerleşiminde N sayfa var; M. sayfa yok.` | `sayfa=` aralık dışında | Sayfa sayısını görün |
| `Izgara: yok / arti / cizgi / centik` | Tanınmayan ızgara biçimi | Listedeki sözcüklerden birini yazın |
| `'X' ve 'Y' aynı parametrenin iki adı; ikisi birden verilmez. Yeni adı 'Z'.` | Bir parametrenin eski ve yeni adı birlikte verildi | Yalnız yeni adı bırakın |

## İlgili

- [`ÇIKTIYERLEŞİMİ`](layout.md) — yerleşimin kendisi
- [`YAZDIR`](print.md) — çizimi doğrudan kâğıda dökmek
