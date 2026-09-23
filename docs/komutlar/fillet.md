# YUVARLA — Köşe Yuvarlatma

Yol kurbu, kavşak dönüşü ya da yuvarlatılmış bir yapı köşesi çizen herkes için;
bu sayfayı bitirdiğinizde köşe yuvarlatmayı arayüzden, komut satırından ve
betikten yapmayı bileceksiniz.

## Ne yapar

`YUVARLA`, bir köşeyi verdiğiniz **yarıçapta bir yayla** yuvarlatır. Yay her iki
kenara da **teğettir**: kenarlar yaya kırılmadan bağlanır. Üç biçimde çalışır:

| Biçim | Ne yuvarlanır |
|---|---|
| **Bir köşe** | Bir çizginin ya da alanın kendi köşesi |
| **İki nesne arasında** | Ayrı iki nesnenin buluştuğu köşe: çizgi-çizgi, çizgi-yay, yay-yay |
| **Bütün köşeler** (`hepsi=evet`) | Bir ya da birçok çizginin ve alanın bütün köşeleri, aynı yarıçapla |

Köşe noktası, teğet noktalarıyla değiştirilir. Teğet noktalarının köşeye uzaklığı
`r / tan(θ/2)`'dir — dik bir köşede bu tam olarak `r` kadardır.

### Açık çizgide yay ayrı bir nesnedir

Açık bir çizgide yuvarlatma **üç nesne** bırakır: köşenin iki yanındaki iki çizgi
ve aralarında yeni bir [`YAY`](arc_draw.md).

Bu bir eksiklik değil, dürüstlüktür. Bu belge modelinde çoklu çizgi köşe
noktalarını tutar, "bulge" denen yay katsayılarını değil. Ayrı bir `YAY`
nesnesinin gerçek merkezi ve gerçek yarıçapı vardır, dolayısıyla uzunluğu ve
geometrisi kesindir.

Yayın süpürme yönü köşenin dönüş yönünden anlaşılır; ayrıca belirtmeniz gerekmez.

### Kapalı şekil yerinde yuvarlatılır

Bir **dikdörtgenin, alanın ya da kapalı çizginin** köşesi de yuvarlatılır ve şekil
**aynı nesne** olarak kalır: kimliği, katmanı, öznitelikleri (ada/parsel no) ve
ona bağlı yazılar korunur, alan olmaya devam eder — alanı ölçülür, ifraz edilir,
tampon alınır.

Kapalı bir şekli ikiye bölmek onu bir şeyi çevrelemez hâle getirirdi; bu yüzden yay
şeklin sınırına **köşe noktalarıyla** çizilir: iki teğet noktası ve aralarında,
[`YAY`](arc_draw.md)'ın kendisinin çizildiği noktalar (çeyrek daire için 16 kenar).
Komut, çizilen köşenin gerçek yaydan en çok ne kadar saptığını söyler:

```text
Köşe yuvarlatıldı (yarıçap 5,000 m). Kapalı şeklin sınırı köşe noktalarından oluştuğu için yay 16 kenarla çizildi; gerçek yaydan en çok 6 mm sapar.
```

Alan da yuvarlatılmış hâliyle hesaplanır: 40 × 30 m'lik bir alanın bir köşesi 5 m
yarıçapla yuvarlatılınca [`ALANÖLÇ`](measure_area.md) 1200,00 m² yerine 1194,60 m²
okur. Gerçek yayla alan `r² − πr²/4` kadar, yani 1194,64 m² olurdu; aradaki
0,04 m² yayın 16 kenarla çizilmesinden gelir.

Diğer kurallar [`PAH`](chamfer.md) ile aynıdır: açık çizginin uçları köşe
değildir ve teğet noktaları komşu kenarların dışına taşamaz. Bir alanın **içbükey**
(içe dönük) köşesi de aynı kuralla yuvarlanır: yay köşenin iki kenarına teğettir ve
girintiyi doldurur.

**Hangi köşe?** Nesne verilmemişse ve tek bir nesne seçili değilse komut önce
köşeyi sorar: köşeye **bir kez tıklamak** hem nesneyi hem köşeyi seçer. Köşe
olmayan bir yere tıklarsanız o nesne **iki nesnenin birincisidir** ve komut ikinciyi
ister. Ardından yarıçap sorulur; **yazabilir** ya da tuvalde **gösterebilirsiniz** —
köşeden imlece olan uzaklık yarıçaptır. İmleç hareket ettikçe yuvarlatılmış köşe
tuvalde vurgulu çizilir ve imlecin yanında `yarıçap 5 m` yazar.

### İki nesne arasında

Ayrı iki nesnenin — iki çizginin, bir çizgi ile bir yayın, iki yayın — buluştuğu ya
da buluşacağı köşe yuvarlanır. Nesneleri **kalacak parçalarından** tıklarsınız ve
tıkladığınız yerler hangi köşenin kastedildiğini söyler: kesişen iki çizginin dört
köşesinden, tıkladığınız iki parçanın arasındaki köşe yuvarlanır, karşısındaki asla.

- Yay iki nesneye de **teğettir**; merkezi her birinden tam yarıçap kadar uzaktadır.
- Nesneler **teğet noktalarına kadar kısaltılır ya da uzatılır**; tıkladığınız parça
  kalır, köşenin öbür yanındaki parça gider. Kısaltmadan yalnız yayı koymak için
  `budama=hayir` verin.
- Bir **daire** ya da kapalı şekil bütün kalır; yay ona teğet konur.
- **Sıfır yarıçap** (`yaricap=0`) iki nesneyi yay koymadan, kesiştikleri yerde
  **keskin köşede** buluşturur: kısa kalan uzatılır, uzun gelen kısaltılır.
- Köşeye **yakın tıklamak** yarıçapı sınırlamaz: teğet noktası tıkladığınız yerin
  ötesine düşse de tıkladığınız parça kalır ve teğet noktasına kadar kısalır.
- Yarıçap tıkladığınız parçalara **sığmıyorsa** — köşe o parçanın tamamını
  götürüyorsa — komut bunu söyleyerek reddeder; yayı köşenin öbür yanına koymaz.
- Aynı çoklu çizginin **bitişik iki kenarına** tıklamak, o iki kenarın köşesini
  yuvarlar.
- Yay birinci nesnenin katmanında ve stilindedir.

### Bütün köşeler

`hepsi=evet` bir çizginin ya da alanın bütün köşelerini aynı yarıçapla yuvarlar;
köşeler birbiri ardınca işlenir ve her köşe, bir öncekinin kısalttığı kenara göre
yargılanır. Yarıçapın sığmadığı köşe **atlanır ve sayılır**, işin geri kalanı
reddedilmez. Açık bir çizginin bütün köşeleri yuvarlanınca sonuç **tek bir yaylı
çoklu çizgidir** — yaylar gerçek yaydır, nesne yığını değil; nesnenin kimliği,
öznitelikleri ve bağlı yazıları korunur. Kapalı bir alan alan olarak kalır.

Birden çok nesne verilirse — ya da önceden seçilmişse — hepsinin bütün köşeleri
**tek adımda** yuvarlanır ve tek bir geri alma adımıdır. Köşeli bir çizgi ya da
alan olmayan nesne (daire, yay, yazı) atlanır ve sayılır.

## Adlar

| Ad | Tür |
|---|---|
| `YUVARLA` | Türkçe, birincil |
| `FILLET` | İngilizce karşılık |
| `YV` | Kısaltma |
| `core.fillet` | Komut kimliği |

## Sözdizimi

```text
YUVARLA nesne=<k> nokta=<n> yaricap=<metre>
YUVARLA nesne=<k> <k> nokta=<n> ikinci_nokta=<n> yaricap=<metre> [budama=hayir]
YUVARLA nesne=<k> [<k> …] hepsi=evet yaricap=<metre>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Köşesi yuvarlatılacak nesne; iki nesne verilirse aralarındaki köşe; `hepsi=evet` ile bir ya da daha çok nesne |
| `nokta` | Tek nesnede işlem yapılacak köşe (en yakın köşe seçilir); iki nesnede birincinin kalacak parçası |
| `ikinci_nokta` | İki nesnede ikincinin kalacak parçası |
| `yaricap` | Yuvarlatma yarıçapı, metre; iki nesnede `0` keskin köşe |
| `budama` | İki nesnede nesneler teğet noktalarına kadar kısaltılıp uzatılsın mı; varsayılan `evet` |
| `hepsi` | `evet`: verilen nesnelerin bütün köşeleri; sığmayan köşe ve köşesi olmayan nesne atlanır |

## Örnekler

### Komut satırı

Açık bir çizginin köşesi:

```text
YUVARLA nesne=1 nokta=485300,4310200 yaricap=8
```

```text
Köşe yuvarlatıldı.
```

Bir dikdörtgenin köşesi — şekil aynı nesne kalır:

```text
DİKDÖRTGEN 0,0 20,12
YUVARLA nesne=1 nokta=20,12 yaricap=5
```

Yeni bir çizimde, L biçiminde buluşan iki ayrı çizgi arasındaki köşe:

```text
ÇİZGİ 0,0 10,0
ÇİZGİ 10,0 10,10
YUVARLA nesne=1 2 nokta=5,0 ikinci_nokta=10,5 yaricap=2
```

```text
İki nesne arasında köşe yuvarlatıldı (yarıçap 2,000 m).
```

Yeni bir çizimde, birbirine yetişmeyen iki çizgiyi keskin köşede buluşturmak:

```text
ÇİZGİ 0,0 8,0
ÇİZGİ 10,2 10,10
YUVARLA nesne=1 2 nokta=4,0 ikinci_nokta=10,6 yaricap=0
```

```text
İki nesne keskin köşede buluştu.
```

Yeni bir çizimde, bir çizginin bütün köşeleri — sonuç tek bir yaylı çoklu çizgi:

```text
ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10
YUVARLA nesne=1 hepsi=evet yaricap=2
```

```text
2 köşe yuvarlatıldı; çizgi tek bir yaylı çoklu çizgi oldu.
```

Yeni bir çizimde, bir parselin ve bir çizginin bütün köşeleri birden:

```text
ALAN 0,0 10,0 10,10 0,10
ÇOKLUÇİZGİ 0,20 10,20 10,30 20,30
YUVARLA nesne=1 2 hepsi=evet yaricap=2
```

```text
6 köşe yuvarlatıldı (2 nesnede); 1 açık çizgi yaylı çoklu çizgi oldu.
```

### Arayüz

Sol araç sütununda **köşe ailesinin** düğmesini basılı tutun ya da sağ tıklayın ve
**Yuvarla**'yı seçin; aynı araç **Değiştir → Yuvarla** menüsündedir.

1. Yuvarlatılacak köşeye tıklayın. Nesne de bu tıklamayla seçilir.
2. İmleci köşeden uzaklaştırın: yay ve iki bacak tuvalde vurgulu çizilir.
3. İstediğiniz yerde tıklayın **ya da** yarıçapı komut satırına yazıp Enter'a
   basın (`8`).

İki nesne arasında: birinci nesneye **kalacak parçasından** tıklayın, sonra ikinci
nesneye. İmleci iki nesnenin buluştuğu yerden uzaklaştırdıkça yay ve kısaltılmış
nesneler tuvalde çizilir ve imlecin yanında `yarıçap X m` yazar; yarıçap sığmıyorsa
sebebi yazar. Tıklayın ya da yarıçapı yazın.

**Yuvarla — bütün köşeler** köşe ailesinin kartında ve **Değiştir** menüsündedir:
nesneye tıklayın ya da önce birden çok nesne seçip düğmeye basın; yarıçapı yazın ya
da gösterin. Bütün nesnelerin bütün köşeleri birlikte önizlenir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.polyline",
      "args": { "noktalar": [[485300000, 4310260000], [485300000, 4310200000],
                             [485360000, 4310200000]] } },
    { "cmd": "core.fillet",
      "args": { "nesne": [1], "nokta": [485300000, 4310200000], "yaricap": 8 } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`YUVARLA` tek bir geri alma adımıdır: [`GERİAL`](undo.md) açık çizgide hem yayı
kaldırır hem köşeyi geri getirir; kapalı şekilde köşeyi eski hâline döndürür.

## Betikten kullanım

Betikten çağrıldığında `nesne`, `nokta` ve `yaricap` verilmelidir; iki nesnede
`ikinci_nokta` da. Yapılandırılmış cevap, kısaltılan nesnelerin kimliklerini
(`duzenlenen`) ve eklenen yayın kimliğini (`eklenen`) söyler.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `YUVARLA için nesne belirtilmedi. Örnek: YUVARLA nesne=1 nokta=10,10 yaricap=3` | Betik ne nesneyi ne köşeyi verdi | `nesne=` ve `nokta=` verin |
| `Orada köşesi kesilecek bir çizgi ya da alan yok. ...` | Tıklanan yerde nesne yok | Bir çizginin iki kenarının buluştuğu köşeye tıklayın |
| `Nesne N bir eğri, yazı ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda çalışır.` | Daire, yay, yazı ya da nokta verildi | Çizgi ya da alan seçin |
| `Burada iki kenarın buluştuğu bir köşe yok. ...` | Açık bir çizginin ucu gösterildi | İki kenarın buluştuğu bir köşe gösterin |
| `Bu köşede kenarlar aynı doğrultuda; kesilecek bir köşe yok.` | Kenarlar doğrusal | Gerçek bir köşe gösterin |
| `Yarıçap sıfırdan büyük olmalı.` | Sıfır ya da eksi yarıçap | Artı bir yarıçap verin |
| `Kesim komşu kenardan uzun: kenarlar 12,000 m ve 20,000 m, gereken 21,000 m. ...` | Yarıçap bu köşeye büyük | Daha küçük bir yarıçap verin ya da daha yakına tıklayın |
| `Bu köşe yuvarlatılamıyor: kenarlar üst üste geliyor.` | Kenarlar aynı doğrultuda geri dönüyor | Gerçek bir köşe gösterin |
| `Bu yarıçapta, seçtiğiniz taraflarda iki nesneye de teğet bir yay yok. ...` | İki nesne paralel ya da seçimler bu yarıçapa uymuyor | Yarıçapı değiştirin ya da nesneleri köşeye yakın yerlerinden seçin |
| `Yarıçap sığmıyor: birinci nesnede teğet noktası köşenin öbür yanına düşüyor. ...` | Bu yarıçaptaki yay seçilen köşede değil karşısındaki köşede kalıyor | Daha küçük bir yarıçap verin |
| `Köşe sığmıyor: seçtiğiniz parçanın tamamını götürüyor. Daha küçük bir değer verin.` | Teğet noktası tıklanan parçanın ötesinde | Daha küçük bir yarıçap verin |
| `Nesneleri köşenin kendisinden değil, kalacak parçalarından seçin.` | Tıklama tam iki nesnenin kesiştiği yerde | Nesneye köşeden biraz uzakta, kalacak parçasından tıklayın |
| `Yarıçap eksi olamaz.` | Eksi yarıçap | `0` ya da artı bir yarıçap verin |
| `Köşe, çoklu çizginin ortadaki bir kenarının uzantısına düşüyor; ...` | Ortadaki bir kenar uzatılmak isteniyor | Çizginin ucundaki kenarı seçin |
| `İki nesne hiçbir yerde kesişmiyor; keskin köşe kurulamaz. ...` | `yaricap=0` ile paralel ya da ayrık nesneler | Bir yarıçap verin |
| `İkinci tıklamanın altında bir nesne yok.` | İkinci tıklama boşluğa | İkinci nesnenin üstüne tıklayın |
| `İki tıklama aynı nesnenin bitişik olmayan yerlerinde; köşesini işlemek için köşeye tıklayın.` | Aynı çizginin bitişik olmayan kenarları | Köşeye tıklayın |
| `Nesne N iki nesne arasındaki köşede kullanılamıyor; çizgi, yay, daire ya da yaylı çoklu çizgi seçin.` | Elips, spline, nokta ya da yazı | Uygun bir nesne seçin |
| `Bu değer hiçbir köşeye sığmıyor; daha küçük bir değer verin.` | `hepsi=evet` ile hiçbir köşe yarıçapı almıyor | Daha küçük bir yarıçap verin |
| `Bu çizginin köşesi yok.` | `hepsi=evet` iki köşeli bir çizgiye | Köşesi olan bir çizgi seçin |
| `Seçilen nesnelerin hiçbirinde işlenecek köşe yok; köşeli bir çizgi ya da alan seçin.` | `hepsi=evet` ile verilen nesnelerin hiçbiri köşeli çizgi ya da alan değil | Köşeli bir çizgi ya da alan seçin |
| `Köşe iki nesne arasında kurulur; N nesnenin bütün köşeleri için hepsi=evet verin.` | `hepsi` olmadan ikiden çok nesne | İki nesne verin ya da `hepsi=evet` ekleyin |

## İlgili

- [`PAH`](chamfer.md) — köşeyi yay yerine düz kenarla keser
- [`YAY`](arc_draw.md) — üretilen yayın kendisi
