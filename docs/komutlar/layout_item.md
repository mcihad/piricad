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
ÇIKTIÖĞE islem=ekle [yerlesim=<ad>] tur=<tür> [ad=<ad>] [x=<mm> y=<mm> genislik=<mm> yukseklik=<mm>] [ayarlar…]
ÇIKTIÖĞE islem=sil [yerlesim=<ad>] ad=<ad>
ÇIKTIÖĞE islem=tasi [yerlesim=<ad>] ad=<ad> x=<mm> y=<mm> genislik=<mm> yukseklik=<mm>
ÇIKTIÖĞE islem=ad [yerlesim=<ad>] ad=<ad> yeni_ad=<ad>
ÇIKTIÖĞE islem=cogalt [yerlesim=<ad>] ad=<ad> [yeni_ad=<ad>] [x=<mm> y=<mm>] [ayarlar…]
ÇIKTIÖĞE islem=ayarla [yerlesim=<ad>] ad=<ad> [metin=<yazı>] [olcek=<N>]
         [pencere=x1,y1 pencere=x2,y2] [izgara=<biçim>] [kilit=evet] …
```

`ekle` aynı satırda verilen **her ayarı** uygular: konum, boyut, yazı, renk… Yeni
öğe önce kendi türünün varsayılanıyla kurulur, sonra satırın söyledikleri üstüne
yazılır. Yalnız `tur=` verilen öğe kâğıdın sol üstünde, kenar payının içinde
belirir.

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
| `islem` | `listele`, `ekle`, `sil`, `tasi`, `ayarla`, `ad`, `cogalt` |
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
| `yeni_ad` | `islem=ad` için öğenin yeni adı; `islem=cogalt` için kopyanın adı (verilmezse `olcek2` gibi türetilir) |
| `katmanlar` | Harita çerçevesinin çizeceği katmanlar; **anahtar birden çok kez yazılır**. Verilmezse görünür bütün katmanlar, `hepsi` listeyi boşaltır |
| `sayfa` | Öğenin duracağı sayfa (1'den başlar); `tasi` ile verilir |
| `aci` | Öğenin dönüşü, derece; sayfada **saat yönünde**, öğenin ortası çevresinde (−360…360) |
| `cerceve_renk` | Çerçevenin rengi — şekilde çizginin. `#RRGGBB`, `#AARRGGBB` ya da bir renk adı: `siyah`, `kırmızı`, `mavi`… |
| `cerceve_kalinlik` | Çerçevenin kalınlığı, kâğıt milimetresi (0…20); `0` kıl çizgi |
| `zemin` | Öğenin arkası zemin rengiyle doldurulsun mu |
| `zemin_renk` | Zeminin rengi — şekilde dolgunun |
| `yazi_renk` | Yazının, ölçek çubuğunun ve kuzey okunun rengi |
| `yatay_hizala` | Metnin kutudaki yatay yeri: `sol`, `orta`, `sag` |
| `dikey_hizala` | Metnin kutudaki dikey yeri: `ust`, `orta`, `alt` |
| `izgara_etiket` | Harita ızgarasının koordinat yazıları: `yok`, `dis` (çerçevenin dışında, öntanımlı), `ic` |
| `izgara_renk` | Harita ızgarasının rengi |
| `izgara_kalinlik` | Izgara çizgisinin kalınlığı, kâğıt milimetresi; `0` kıl çizgi |
| `izgara_yazi` | Izgara yazılarının yüksekliği, kâğıt milimetresi |
| `bolum` | Ölçek çubuğunun bölüm sayısı (1…10) |
| `sekil` | Şekil öğesinin biçimi: `dikdortgen`, `elips`, `cizgi` |

### Tablo öğesi

`metin=` tablonun **katman adıdır**. Sütunlar o katmanın şemasından gelir;
yalnız bazılarını istiyorsanız `sutunlar=` ile sırasıyla yazın — anahtar birden
çok kez yazılır, `hepsi` listeyi boşaltır:

```
ÇIKTIÖĞE islem=ayarla ad=liste metin=PARSEL sutunlar=ada sutunlar=parsel sutunlar=alan
```

Tablonun katmanı ya da sütunlarından biri çizimde yoksa — başka bir çizim için
hazırlanmış bir yerleşim, sonradan silinmiş bir sütun — tablo **boş basılmaz**: kutunun
yerinde kesikli bir çerçeve içinde `tablo: 'X' adlı katman yok` yazar ve
`ÇIKTIYERLEŞİMİ islem=denetle` bunu söyler.

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

### Görünüm: döndürme, renk, zemin, hizalama

Her öğe döndürülebilir, çerçevesi ve zemini renklendirilebilir; metin kutunun içinde
dokuz yere yaslanabilir. Renk `#RRGGBB`, saydam bir renk `#AARRGGBB` ya da bir renk
adıyla yazılır; komut günlüğü rengi her zaman `#RRGGBB` biçiminde tutar.

<!-- örnek: yeni çizim -->
```
ALAN 0,0 100,0 100,80 0,80
ÇIKTIYERLEŞİMİ islem=ekle ad=Pafta kagit=A3 yon=yatay
ÇIKTIÖĞE islem=ekle tur=metin ad=antet x=312 y=12 genislik=96 yukseklik=18 metin="Ada 1284 · Pafta 3" yazi=5 yatay_hizala=sol zemin=evet zemin_renk=#F2F2F2 cerceve=evet cerceve_kalinlik=0.35
ÇIKTIÖĞE islem=ayarla ad=harita izgara=cizgi izgara_etiket=ic izgara_renk=#808080 izgara_kalinlik=0.18
ÇIKTIÖĞE islem=ayarla ad=olcek bolum=5
ÇIKTIÖĞE islem=ekle tur=sekil ad=damga sekil=elips x=330 y=250 genislik=40 yukseklik=30 cerceve_renk=mavi
```

Izgara, ölçek bölümü ve şekil biçimi yalnız kendi türlerine verilir; başka bir öğeye
verildiğinde komut hangi türe ait olduğunu söyleyerek reddeder.

### Çoğaltmak

`islem=cogalt` öğenin bir kopyasını **5 mm sağına ve altına**, aynı sayfaya ve bütün
öğelerin üstüne koyar. Bütün ayarları ve bağları gelir — çoğaltılan bir ölçek çubuğu
aslının haritasının ölçeğini yazmaya devam eder — ama kilidi gelmez. Aynı satırda
verilen ayarlar kopyaya uygulanır:

<!-- örnek: yeni çizim -->
```
ALAN 0,0 100,0 100,80 0,80
ÇIKTIYERLEŞİMİ islem=ekle ad=Pafta
ÇIKTIÖĞE islem=cogalt ad=olcek yeni_ad=olcek_alt x=20 y=250
```

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

**Çıktı yerleşimi tasarımcısı** (**Çıktı ▸ Yazdır ▸ Yerleşimler** ya da hızlı erişimdeki
yazıcının oku ▸ bir yerleşim) dört parçadan oluşur: üstte **araç satırı**, solda **araç
sütunu**, ortada **kâğıt**, sağda **öğeler ve denetçi**. En altta durum satırı imlecin
kâğıt üzerindeki yerini milimetre olarak yazar.

**Araç sütunu** — kâğıda ne eklendiği. En üstteki **Seç** aracı öğeleri seçer ve taşır;
altındaki dokuz araç dokuz öğe türünü ekler: harita, metin, lejant, ölçek çubuğu ve kuzey
oku; resim ve şekil; tablo ve grafik. Bir araca basıp **kâğıtta sürüklediğinizde** öğe
çizdiğiniz kutuya yerleşir; sürüklemeden **tıklarsanız** türün öntanımlı boyunda,
tıkladığınız yerin çevresine konur. Sürüklerken **Shift** kutuyu kare tutar, **Esc**
vazgeçer. Öğe eklenince araç kendiliğinden **Seç**'e döner ve yeni öğe seçili gelir.

**Araç satırı** — seçili öğelere ne yapıldığı. Soldan sağa:

| Grup | Düğmeler | Ne zaman açık |
|---|---|---|
| Geçmiş | Geri al (`Ctrl+Z`), Yinele (`Ctrl+Shift+Z`) | her zaman |
| Hizala | Sol, yatay orta, sağ; üst, dikey orta, alt | en az bir öğe seçiliyken |
| Dağıt | Yatayda, dikeyde — aradaki boşluklar eşitlenir | en az üç öğe seçiliyken |
| Sıra | En öne getir, en arkaya gönder | en az bir öğe seçiliyken |
| Öğe | Çoğalt (`Ctrl+D`), kilitle (`Ctrl+L`), sil (`Delete`) | en az bir öğe seçiliyken |
| Sayfa | `‹`, **Sayfa 1 / 3 ▾**, `›` | çok sayfalı yerleşimde |
| Görünüm | Yakala; uzaklaş, yakınlaştırma oranı ▾, yakınlaş, sayfayı sığdır, gerçek boy | her zaman |

Tek bir öğe hizalanırken **kenar payına** göre hizalanır: tek seçili başlığa **yatayda
ortala** demek onu sayfanın ortasına alır. Birden çok öğe, birlikte kapladıkları kutuya
göre hizalanır. Seçimdeki öğelerin hepsi kilitliyken kilit düğmesi basılı görünür ve
basmak kilidi açar. Düğmelerin adı üzerlerine gelince görünen ipucundadır.

**Kâğıt.** Tekerlek imlecin olduğu yere doğru yakınlaştırır; **Boşluk** tuşunu basılı
tutup ya da orta düğmeyle sürüklemek kâğıdı kaydırır; `+`, `−` ve `0` (sığdır) tuşları da
çalışır. **Gerçek boy** kâğıdı ekranda kendi ölçüsünde gösterir.

- Bir öğeye tıklamak seçer; **sürüklemek taşır**, köşe ya da kenar tutamağından çekmek
  boyutlandırır. Köşeden çekerken **Shift** oranı korur, taşırken **Shift** hareketi tek
  eksende tutar.
- **Shift ile tıklamak** seçime ekler ya da çıkarır; boş kâğıtta **çerçeve çekmek**
  çerçevenin değdiği bütün öğeleri seçer; `Ctrl+A` sayfadaki her şeyi seçer.
- **Yakala** açıkken sürüklenen kutu sayfanın kenarlarına, ortasına, kenar payına ve
  öteki öğelerin kenar ve ortalarına tutunur; tutunduğu çizgi mavi bir kılavuz olarak
  görünür. Yakalanmadığında kutu tam milimetreye oturur.
- **Ok tuşları** seçimi birer milimetre kaydırır, **Shift+ok** on milimetre.
- Sürükleme ya da boyutlandırma boyunca kutunun yeri ya da ölçüsü yanındaki etikette
  milimetre olarak yazar.
- Kilitli bir öğenin tutamağı yoktur ve sürüklenmez.
- Sağ tık öne/arkaya, çoğalt, kilitle ve sil seçeneklerini açar; bir öğeye çift tıklamak
  denetçide ilk alanına gider.
- Kâğıdın üstünde ve solunda **milimetre cetveli** durur. Seçili öğelerin kapladığı
  açıklık iki cetvelde de vurgulanır ve sürükleme boyunca onunla birlikte hareket eder.

**Öğeler ve denetçi.** Sağ sütunun üstünde bu sayfadaki öğeler, **en üstte çizilen en
başta** olmak üzere listelenir. Satır öğenin adını ve `genişlik×yükseklik` ölçüsünü
yazar; satırın sağındaki **kilit** tıklanarak öğe kilitlenir ya da kilidi açılır. Listede
de **Shift/Ctrl** ile birden çok öğe seçilir. Komut satırının `ad=` ile andığı kimlik
satırın ipucundadır.

Listenin altındaki **Öğe | Sayfa** seçimi denetçinin neyi gösterdiğini belirler ve
denetçinin başında neye baktığınızın adı, türü, kimliği ve ölçüsü yazar. Her ayar bir
satırdır: solda adı, sağda değeri; birimi alanın içinde yazar. Ayarlar kararın verildiği
sırayla gruplanmıştır:

| Grup | Ne karara bağlar |
|---|---|
| **Konum ve boyut** | X (soldan), Y (üstten), genişlik, yükseklik, döndürme; çok sayfalı yerleşimde öğenin sayfası |
| **Türe özgü** | Harita: ölçek (yazılır ya da plan ölçeklerinden seçilir), kapsam (**Çizimin tamamı**, **Ana pencereden al**), katmanlar, koordinat ızgarası. Metin: yazı, **Alan ekle ▾** ile yer tutucu, yazı boyu, renk, yatay ve dikey hizalama. Ölçek çubuğu: bağlı harita, bölüm sayısı, yazı boyu, renk. Kuzey oku: bağlı harita, renk. Lejant: başlık, bağlı harita, yazı. Resim: dosya. Şekil: biçim, çizgi, dolgu. Tablo: katman, sütunlar, satır sınırı. Grafik: katman, sayılan sütun, bağlı harita |
| **Çerçeve ve zemin** | Çerçeve, rengi ve kalınlığı; zemin ve rengi |
| **Öğe** | Kilit ve öğenin adı |

Bir alanın değeri **Enter**'a bastığınızda ya da alandan çıktığınızda yazılır; değeri
değiştirmeden çıkmak hiçbir şey yazmaz. Birden çok öğe seçiliyken denetçi seçilenleri
sayar; hizalama, dağıtma, sıralama, çoğaltma ve kilit araç satırından hepsine birden
uygulanır ve tek adımda geri alınır.

**Sayfa** bölümü kâğıdın boyunu ve yönünü, kenar payını, çözünürlüğü, bu sayfayı
**ekle**, **çoğalt** ve **sil** düğmelerini, yerleşimin adını ve **denetimin**
bulduklarını gösterir. Denetim uyarıları durum satırında da sayılır; o sayıya tıklamak
Sayfa bölümünü açar.

Her jest **bırakıldığında tek bir komut** yazar — sürükleme boyunca değil. Bu yüzden
sayfanın bir ucundan öbürüne taşıdığınız bir kutu tek `Ctrl+Z` ile eski yerine döner,
dört yüz adımda değil; birden çok öğeyi birlikte taşımak, hizalamak ya da silmek de tek
adımdır. Yaptığınız her şey komut günlüğünde durur ve bir betiğin yazabileceği
satırlardır.

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
| `Açı -360 ile 360 derece arasında olmalı; 400 verildi.` | `aci=` aralık dışında | Açıyı −360…360 arasında verin |
| `cerceve_renk: tanınmayan renk 'X'. #RRGGBB, #AARRGGBB ya da bir renk adı yazın: siyah, kırmızı, mavi…` | Renk okunamadı (`zemin_renk`, `yazi_renk`, `izgara_renk` için aynı biçim) | Rengi onaltılık ya da adıyla yazın |
| `Çerçeve kalınlığı 0 ile 20 mm arasında olmalı.` | `cerceve_kalinlik=` aralık dışında | 0…20 mm verin |
| `'X' bir harita çerçevesi değil; izgara_etiket yalnız bir harita çerçevesi öğesine verilir.` | Izgara ayarı harita olmayan bir öğeye verildi (`izgara_renk`, `izgara_kalinlik`, `izgara_yazi` için aynı biçim) | Harita öğesinin adını verin |
| `'X' bir ölçek çubuğu değil; bolum yalnız bir ölçek çubuğu öğesine verilir.` | `bolum=` ölçek çubuğu olmayan bir öğeye verildi | Ölçek çubuğunun adını verin |
| `'X' bir şekil değil; sekil yalnız bir şekil öğesine verilir.` | `sekil=` şekil olmayan bir öğeye verildi | Şekil öğesinin adını verin |

## İlgili

- [`ÇIKTIYERLEŞİMİ`](layout.md) — yerleşimin kendisi
- [`YAZDIR`](print.md) — çizimi doğrudan kâğıda dökmek
