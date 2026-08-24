# STİL — Nesne Stili ve Gösterim Kataloğu

Çizimini renklendiren, paftaya hazırlayan ve plan gösterimlerini uygulayan herkes için;
bu sayfayı bitirdiğinizde bir katmandaki nesnelere doğrudan stil vermeyi, stil kataloğu
paketinden gösterim uygulamayı ve stili geri almayı bileceksiniz.

## Ne yapar

Bir katmandaki **canlı nesnelerin her birine bir stil yazar**.

PiriCAD'de görünüm çizim anında hesaplanmaz. `STİL` çalıştığında görünüm bir kez çözülür,
çizimin stil tablosuna tek satır olarak yazılır ve her nesne o satırın numarasını taşır.
Ekran çizerken kural işletmez, öznitelik okumaz, ifade değerlendirmez; tek bir sayı okur.
Beş milyon parselli bir katmanda kaydırmanın akıcı kalmasının sebebi budur.

Stil üç kaynaktan gelebilir; sıralama şudur:

1. **Katalog satırı** — `paket=` ile bir stil kataloğu verirseniz, satır ya `kod=` ile
   doğrudan seçilir ya da katalogdaki eşleme kuralları nesneye bakarak seçer.
2. **Komut satırında verilen değerler** — `renk`, `kalinlik`, `dolgu`, `sira`. Bunlar
   katalog satırının üzerine yazar.
3. **Katmanın kendi görünümü** — yukarıdaki ikisinin dokunmadığı her özellik katmandan
   gelmeye devam eder. Katman rengini değiştirdiğinizde bu özellikler de değişir.

`sifirla=evet` yazarsanız stil silinir ve nesneler tamamen katman görünümüne döner.

Aynı görünüm iki kez istendiğinde stil tablosunda **tek satır** açılır: on bin parsele aynı
gösterimi vermek tabloya bir satır ekler, on bin satır değil.

### Gösterim katalogları hakkında

Plan gösterimleri koda gömülmez; `data/catalogs/` altında veri olarak durur. Bir yönetmelik
değişikliği veri paketinin güncellenmesidir, programın yeniden derlenmesi değildir.

Mekânsal Planlar Yapım Yönetmeliği'nin (MPYY) gösterim ekleri EK-1a (ortak gösterimler),
EK-1b (mekânsal strateji planı), EK-1c (çevre düzeni planı), EK-1ç (nazım imar planı),
EK-1d (uygulama imar planı) ve EK-1e (detay kataloğu) olarak paketlenir. PiriCAD'in bu
paketi `data/catalogs/mpyy/plan-gosterim.json` dosyasındadır ve **476 gösterim satırı**
içerir. Paketin hangi Resmî Gazete sürümüne dayandığı, ne çıkarıldığı ve neyin eksik
kaldığı [MPYY plan gösterimleri](../veri/mpyy-gosterimleri.md) sayfasında yazılıdır.

**Paketin eşleme kuralları BOŞTUR ve uzman onayı BEKLİYOR.** Satırların kendisi resmî ek
metninden çıkarılmıştır, ama hangi nesnenin hangi satırı alacağını söyleyen `kurallar`
dizisi boştur: bu, ek metninden okunabilecek bir şey değil, plan türü ve öznitelik
şemasıyla birlikte verilen bir uzman kararıdır. Bu yüzden bugün satırı `kod=` ile adıyla
seçersiniz; `paket=` tek başına verildiğinde komut `Stil kataloğunda bu nesneye uyan kural
yok` hatasını verir.

Paket harita mühendisi / şehir plancısı onayından geçmeden resmî bir plan paftasında
kullanılmaz. Kurallar ve onay **Faz 3'te** gelecektir. Dilediğiniz zaman `paket=` ile kendi
stil kataloğunuzu verebilirsiniz; biçim
`data/catalogs/schema/plan-gosterim.schema.json` dosyasında tanımlıdır.

## Adlar

| Ad | Tür |
|---|---|
| `STİL` | Türkçe, birincil |
| `STIL` | Türkçe, ASCII karşılık |
| `STYLE` | İngilizce karşılık |
| `ST` | Kısaltma |
| `core.style` | Komut kimliği |

## Sözdizimi

```text
STİL
STİL <katman adı>
STİL katman=<ad> [renk=<tamsayı>] [kalinlik=<µm>] [dolgu=<tamsayı>] [sira=<tamsayı>]
STİL katman=<ad> paket=<katalog yolu> [kod=<satır kimliği>] [olcek=<payda>]
STİL katman=<ad> sifirla=evet
```

Argümansız çağırırsanız komut yalnız katman adını sorar; geri kalan her şey argümanla
verilir.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `katman` | Stilin yazılacağı katmanın adı. Zorunlu. Katman var olmalıdır |
| `paket` | Stil kataloğu paketinin dosya yolu. Göreli yol çalışma dizinine göre çözülür |
| `kod` | Katalogdaki satırın kimliği. Verilmezse katalogdaki eşleme kuralları çalışır |
| `sinifla` | Sınıflandırmada kullanılacak öznitelik. Her nesne KENDİ değerine göre stillenir |
| `olcek_min` | Bu ölçek paydasından daha yakında çizilmez (1:N'deki N) |
| `olcek_max` | Bu ölçek paydasından daha uzakta çizilmez |
| `olcek` | Ölçek paydası (1:N). Ölçeğe bağlı satır ve kuralların hangisinin geçerli olduğunu belirler. `0` = ölçekten bağımsız |
| `renk` | Çizgi rengi, `0xAARRGGBB` düzeninde tam sayı |
| `kalinlik` | Çizgi kalınlığı, **kâğıt mikrometresi**. `1000` = paftada 1 mm |
| `dolgu` | Dolgu rengi, `0xAARRGGBB`. `0` = dolgusuz |
| `sira` | Çizim sırası. Büyük olan üste gelir |
| `sifirla` | `evet` yazılırsa stili siler; nesneler katman görünümüne döner |
| `tip` | Sembol katmanının tipi. Aşağıdaki tabloya bakın |
| `ekle` | `evet` yazılırsa katman mevcut sembolün **üstüne** eklenir; yoksa sembolü değiştirir |
| `sekil` | İşaretçi şekli: `daire`, `kare`, `ucgen`, `baklava`, `yildiz`, `arti`, `carpi`, `ok`, `yarim-daire`, `besgen`, `altigen`, `cizik` |
| `yerlesim` | İşaretçinin çizgi üzerindeki yeri: `aralik`, `tepe`, `ilk`, `son`, `orta` |
| `birim` | Aşağıdaki ölçülerin birimi: `kagit` (µm), `zemin` (mm), `piksel` |
| `boyut` | İşaretçi çapı ya da tarak dişinin boyu |
| `boyut_birim` | Yalnız `boyut` için birim. Verilmezse `birim` geçerlidir |
| `aralik` | Çizgi boyunca ya da desende birinci eksende aralık |
| `aralik_birim` | Yalnız `aralik` için birim. Verilmezse `birim` geçerlidir |
| `aralik_y` | Nokta deseninde ikinci eksen. Verilmezse desen karedir |
| `aralik_y_birim` | Yalnız `aralik_y` için birim. Verilmezse `birim` geçerlidir |
| `aci` | Desen açısı ya da işaretçi dönüklüğü, **mikro derece** (45° = `45000000`) |
| `kaydirma` | Geometriden dik kaydırma |
| `kaydirma_birim` | Yalnız `kaydirma` için birim. Verilmezse `birim` geçerlidir |
| `saydamlik` | Katman saydamlığı `0`–`255`. `255` tam opak |
| `desen` | Çizgi tipi: `sürekli`, ya da çizgi/boşluk uzunlukları — `"8 1 1 1"` |
| `yazi` | `yazi-isaretci` katmanının yazdığı sabit metin |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

`renk` ve `dolgu` değerleri `KATMAN` komutundakiyle aynı düzendedir; hazır değerler için
[Katman yönetimi](layer.md) sayfasındaki renk tablosuna bakın.

### Sembol katmanları — bir gösterim tek çizgi değildir

Bir MPYY gösterimi çoğu zaman üst üste birkaç şeydir: bir dolgu, başka renkte bir
sınır, üstünde tekrar eden bir simge. Tek renk-ve-kalınlık kaydı bunu söyleyemez,
o yüzden bir stil **sembol katmanlarından** oluşan bir yığındır ve alttan üste
çizilir.

| `tip` | Ne çizer | Nerede işe yarar |
|---|---|---|
| `cizgi` | Geometri boyunca düz kontur | Sınır, yol, kıyı |
| `isaretci-cizgi` | Çizgi boyunca tekrar eden simge | Sit alanı sınırı, koruma sınırı |
| `tarak-cizgi` | Çizgiyi dik kesen kısa dişler | Demiryolu, şev |
| `dolgu` | Yüzeyin içi: renk ve tarama | İmar lekesi |
| `cizgi-desen-dolgu` | Açılı paralel çizgiler | Tarım alanı, jeolojik sakıncalı alan |
| `nokta-desen-dolgu` | Simge ızgarası | Orman, mezarlık, bataklık |
| `merkez-isaretci` | Yüzeyin ortasında tek simge | Tesis simgesi |
| `isaretci` | Her tepe noktasında simge | Nokta gösterimleri |
| `gorsel-dolgu` | Görseli yüzeye döşer | MPYY `tarama` |
| `gorsel-isaretci` | Görseli lekenin ortasına koyar | MPYY `sembol` |
| `gorsel-cizgi` | Görseli çizgi boyunca tekrarlar | MPYY `çizgi tipi` |
| `yazi-isaretci` | Sembolün **kendi** sabit yazısı | `TAKS`, `KAKS`, `E`, `h` |

Yığın **tek tek** kurulur: ilk `STİL` sembolü kurar, `ekle=evet` ile gelen her
`STİL` üstüne bir katman ekler. Stil tasarımcısı da tam olarak bunu yapar, bir
betik de aynı satırları yazar — üçü de aynı yoldan geçer.

Doğrudan verilen görünüm değerleri katmanın **varsayılan görünümünü** de günceller.
Bu nedenle henüz nesne içermeyen bir katmanda bile renk, dolgu ve çizgi kalınlığı
hemen katman listesinde görünür; daha sonra çizilen nesneler bu temel görünümü
devralır. Var olan nesnelerin sembol yığını ayrıca kendi stil sütununa yazılır.

`isaretci-cizgi` ve `tarak-cizgi` **yalnız simgelerini** çizer, çizgiyi çizmez.
Bir demiryolu bu yüzden iki katmandır: altta düz çizgi, üstünde dişler.

### Mevzuatın kendi görseli

MPYY sembolojisini **resim olarak** yayımlıyor: EK-1 ekleri Word belgesi ve
gösterimlerin taraması, simgesi ve çizgi tipi orada birer görsel. 476 satırın
439'u en az bir görselle geliyor.

`STİL kod=` bir satırı uygularken o satırın **yayımlanmış görsellerini** okur ve
sembolü onlardan kurar: taramayı `gorsel-dolgu`, çizgi tipini `gorsel-cizgi`,
simgeyi `gorsel-isaretci` katmanı olarak. Yani ekranda gördüğünüz, mevzuatın
bastığı şeyin kendisidir — ona benzemesi için seçilmiş bir renk değil.

Bunları vektöre çevirmek 476 kez "bu resim aslında 9 metre aralıklı üçgen ızgara"
demektir; bu düzenleyici bir yorumdur ve harita mühendisi imzası ister. Program
uydurmuyor.

**Görseller belgenin içine gömülür**, yola bağlanmaz. Bir yol dosya taşınınca,
veri paketi kurulu olmayınca, çizim denetleyecek belediyeye e-postayla gidince
kırılır. Aynı tarama dokuz plan türünde kullanılsa bile **tek kopya** saklanır;
depo içeriğe göre tekilleştirir.

Görsellerin künyesi de belgeyle gider: her görsel hangi katalog satırından ve
hangi ekten geldiğini taşır.

### Sembolün yazısı ile nesnenin yazısı ayrı şeylerdir

MPYY'nin `yapılaşma koşulu` gösterimi bir dairedir: içinde yatay bir çizgi, çizginin
üstünde `TAKS`, altında `KAKS`, ve her birinin yanında o parselin değeri.

Bu iki yazı **farklı kaynaklardan** gelir ve bu bir tasarım kararıdır:

| Yazı | Nereden | Nasıl |
|---|---|---|
| `TAKS`, `KAKS` | **Sembolden** — Türkiye'deki her parselde aynı | `tip=yazi-isaretci yazi=TAKS` |
| `0,30`, `1,50` | **Nesneden** — her parselde farklı | [`ETİKET`](label.md) |

Sebebi `.claude/model.md` P29: kare yolu öznitelik sütunu okuyamaz. Bir sembol
katmanı her karede çizilir, dolayısıyla sabit olanı taşıyabilir; değişeni taşıyan
şey `ETİKET`'in yazdığı yazı nesnesidir.

`kaydirma` yazıyı merkezden yukarı (artı) ya da aşağı (eksi) alır, `boyut` da
punto yerine geçer — ikisi de `birim` ile kâğıt ya da zemin olabilir.

### Birim: kâğıt mı, zemin mi

Bir sınırın kalınlığı **kâğıda** aittir — MPYY paftada 0,5 mm der ve pafta ister
1/1000 ister 1/5000 olsun 0,5 mm kalır. Bir orman deseninin sıklığı çoğu zaman
**zemine** aittir: desen alana aittir ve ölçekle küçülmesine izin vermek okunur
bir dokuyu gri bir lekeye çevirir.

`birim` bu ayrımı söyler ve varsayılan `kagit`'tır.

#### Aynı katmanda karışık birim

Tek bir sembol katmanı çoğu zaman iki ayrımı birden taşır. Bir il sınırı gösterimini
düşünün: işaretçinin **çapı** paftaya aittir — mevzuat "paftada 2,6 mm" der ve ölçek ne
olursa olsun 2,6 mm kalır — ama işaretçilerin **aralığı** zemine aittir, çünkü aralık
sınırın kendi uzunluğuyla ilgilidir.

Bunun için her ölçünün kendi birimi olabilir. `birim` birimini söylemeyen ölçüler için
geçerli kalır:

```
KATMAN ad=IL_SINIRI
STİL katman=IL_SINIRI tip=isaretci-cizgi sekil=daire birim=kagit boyut=2600 aralik=15000 aralik_birim=zemin
```

Burada `boyut` kâğıt mikrometresinde (2,6 mm), `aralik` zemin milimetresinde (15 m)
okunur. Stil tasarımcısındaki her ölçünün yanında duran birim kutusu tam olarak bu
parametreleri yazar.

Tanınmayan bir birim adı sessizce `birim`'e düşmez: komut hangi parametrenin hatalı
olduğunu adıyla söyleyerek durur ve çizime dokunmaz. Mesajın kendisi aşağıdaki
[Hatalar](#hatalar) tablosundadır.

### Çizgi tipi bir desendir, resim değil

MPYY il sınırını bir çizgi, bir boşluk, bir nokta ve bir boşluk olarak basar. Bu dört
sayıdır ve `desen` onları alır:

```
KATMAN ad=IL_SINIRI
STİL katman=IL_SINIRI tip=cizgi kalinlik=500 desen="8 1 1 1"
```

Sayılar **çizgi kalınlığının katıdır**, milimetre değil. Bunun sebebi bir desenin her
kalınlıkta doğru kalmasıdır: yukarıdaki satır 0,5 mm'lik bir sınırda da 1,0 mm'lik bir
sınırda da aynı oranları çizer, ikinci bir tanım gerekmez. Sıra çizgiyle başlar ve
çizgi/boşluk çiftleri hâlinde gider; tek sayıda parça, arkasında boşluk olmayan bir
çizgi bırakacağı için reddedilir. En çok sekiz parça yazılabilir.

`desen=sürekli` düz çizgidir ve varsayılandır.

Desen **çizimin içinde taşınır**, katalog paketinde değil. Gömülü görsellerle aynı
gerekçe: paketin kurulu olmadığı bir bilgisayarda açılan pafta aynı çizilmelidir.

Desenli bir çizgi **düz uçla** çizilir, katman `uc` biçimi ne derse desin. Qt ucu her
çizgi parçasına uygular; yuvarlak uçta her çizgi iki ucundan yarım kalınlık uzar ve bir
kalınlık genişliğindeki boşluk tamamen kapanır — kesik-noktalı bir sınır düz çizgi
olarak çıkardı, ki paftada bu farklı bir hukuki beyandır. Bildirilen uç biçimi çizginin
**iki gerçek ucunu** anlatır; içindeki her çizgiyi değil.

### Kalınlık neden mikrometre

Mevzuat çizgi kalınlığını **paftada milimetre** olarak verir. Ekrandaki piksel kalınlığı
ölçekle ve ekran çözünürlüğüyle her karede değişir; kâğıt kalınlığı değişmez. Bu yüzden
kalınlık kâğıt mikrometresinde saklanır ve piksel karşılığı her karede yeniden hesaplanır:

| Paftada | `kalinlik` |
|---|---|
| 0,13 mm | `130` |
| 0,25 mm | `250` |
| 0,35 mm | `350` |
| 0,50 mm | `500` |
| 1,00 mm | `1000` |

### Kategorize çizici

Bir katmandaki nesneleri, hepsine aynı stili yazmak yerine **her birinin kendi
özniteliğine göre** stillemek `sinifla` ile yapılır:

```
KATMAN ad=PLAN
SÜTUN gosterim metin
STİL katman=PLAN paket=data/catalogs/mpyy/plan-gosterim.json sinifla=gosterim
```

Bu tek komut, `PLAN` katmanındaki her nesnenin `gosterim` özniteliğini okur, o değeri
katalogda kimlik ya da ad olarak arar ve bulduğu satırın rengini, kalınlığını ve
dolgusunu o nesneye yazar. Aynı katmandaki iki parsel farklı gösterim taşıyorsa farklı
görünür: **katman üyeliği görünümü belirlemez, nesnenin kendi verisi belirler.**

Değer önce kimlik olarak aranır, bulunamazsa ad olarak. İkisi de kabul edilir çünkü
çizimi etiketleyen bir insandır: `nip-toplu-konut-alani-siniri` paketin satıra verdiği
addır, `TOPLU KONUT ALANI` ise bir plancının öznitelik hücresine yazdığıdır.

Özniteliği olmayan bir nesne **katman varsayılanında kalır** ve sayılır. Gösterimini
bildirmeyen bir parsele gösterim uydurmak, hukuki bir çizimin taşımaması gereken tam
olarak o icattır; komut kaç nesnenin eşleştiğini ve kaçının öznitelik taşımadığını
söyler.

Bağlamanın komutta durması kasıtlıdır: yönetmelik bir gösterimin **neye benzediğini**
söyler, sizin öznitelik sütununuzun **adını** asla söylemez. O yüzden "hangi sütun
gösterim tutuyor" bilgisi katalog paketine değil, çağrı yerine aittir.

Sayısal bir sütunla aralık kuralları da aynı yoldan çalışır — nüfus yoğunluğuna göre
beş kademeli konut lekesi, beş aralık penceresi demektir.

### Ölçeğe bağlı görünürlük

Bir stil yalnız belirli ölçek aralığında çizilebilir:

```
KATMAN ad=CDP_LEKE
STİL katman=CDP_LEKE renk=0xFF6A1B9A dolgu=0xFFD7B8E8 olcek_min=3000
```

Bu leke yalnız 1/3000'den **uzakta** görünür; yakınlaştıkça kaybolur ve altındaki
uygulama imar planı parselleri okunur hâle gelir. Tersi de olur:

```
KATMAN ad=UIP_PARSEL
STİL katman=UIP_PARSEL renk=0xFF2E7D32 dolgu=0xFFE1DFB3 olcek_max=3000
```

Değerler 1:N gösteriminin **N**'idir; büyüyen N uzaklaşmak demektir. Biri
verilmezse o yönde sınır yoktur.

Bu planlama işinde süs değildir: 1/100000 ölçekli bir çevre düzeni planı bir leke
gösterir, 1/1000 ölçekli uygulama imar planı o lekenin parsellerini gösterir, ve
ikisini aynı anda çizmek kimsenin okuyamayacağı bir pafta üretir.

Pencere sembolün üstünde saklanır, yani kare yolunda bir dizi araması ve bir
karşılaştırmadır — hiçbir kural değerlendirilmez.

## Örnekler

### Komut satırı

Bir katman kurun, üzerine çizin ve stilini verin:

```
KATMAN ad=IMAR
ÇİZGİ 485300,4310200 485360,4310200
STİL katman=IMAR renk=4281236786 kalinlik=350
```

Dolgulu bir alan görünümü, üstte çizilsin diye sırası büyük:

```
STİL katman=IMAR renk=4281236786 kalinlik=350 dolgu=4294703769 sira=20
```

Yalnız kalınlığı değiştirin; renk katmandan gelmeye devam etsin diye `renk` vermeyin:

```
STİL katman=IMAR kalinlik=500
```

Stili silin, nesneler katman görünümüne dönsün:

```
STİL katman=IMAR sifirla=evet
```

Adında boşluk olan katman:

```
KATMAN ad="YOL KENARI"
ÇİZGİ 485300,4310260 485360,4310260
STİL katman="YOL KENARI" renk=4284310640
```

MPYY paketinden bir gösterim satırını adıyla uygulamak şöyle görünür:

```text
STİL katman=IMAR paket=<depo kökü>/data/catalogs/mpyy/plan-gosterim.json kod=uip-ticaret-alani
1 nesneye stil yazıldı: 'IMAR', katalog satırı 'TİCARET ALANI', stil kimliği 4.
```

Bu blok bir iskelettir, olduğu gibi çalıştırılamaz: `paket=` yolu çalışma dizinine göre
çözülür, bu yüzden `<depo kökü>` yerine kendi yolunuzu yazmanız gerekir. Satır kimlikleri
`<ek kısaltması>-<ad>` biçimindedir: `ortak-` (EK-1a), `msp-` (EK-1b), `cdp-` (EK-1c),
`nip-` (EK-1ç), `uip-` (EK-1d).

`kod=` vermeden yalnız `paket=` verirseniz komut `Stil kataloğunda bu nesneye uyan kural
yok` hatasını verir: pakette eşleme kuralı yoktur. **Faz 3'te** kurallar eklendiğinde
katmanı tek tek kodlamak gerekmeyecektir.

### Arayüz

Komutu pencerenin altındaki **komut satırına** yazın; sonuç **Transkript** panelinde
görünür. Arayüzün hiçbir ayrıcalığı yoktur: menüden yapılan da, komut satırından yazılan
da aynı komuttur, aynı doğrulamadan geçer ve aynı günlüğe yazılır.

Komut satırına `STİL` yazıp **Enter**'a basarsanız katman adı sorulur; adı yazıp yeniden
**Enter**'a basmak yeter. **Esc** komutu iptal eder ve çizimde hiçbir iz bırakmaz.

**Katmanlar** panelinde bir katmana sağ tıklayarak açılan **Stil** iletişim kutusu ve
gösterim kataloğu seçici **Faz 1'de** gelecek; ikisi de bu komutu gönderecek, ikinci bir
stil listesi olmayacak.

### Yığılmış gösterim — orman

```
KATMAN ORMAN
ALAN 485300000,4310200000 485370000,4310200000 485370000,4310250000 485300000,4310250000
STİL katman=ORMAN tip=dolgu dolgu=805568546
STİL katman=ORMAN ekle=evet tip=nokta-desen-dolgu sekil=ucgen birim=zemin boyut=3000 aralik=9000 renk=4280645666
```

Önce soluk yeşil bir dolgu, üstüne zeminde 9 metre aralıklı üçgen ızgara.

### Yığılmış gösterim — demiryolu

```
KATMAN DEMIRYOLU
ÇİZGİ 485300000,4310200000 485400000,4310200000
STİL katman=DEMIRYOLU tip=cizgi renk=4278190080 kalinlik=900
STİL katman=DEMIRYOLU ekle=evet tip=tarak-cizgi birim=zemin boyut=4000 aralik=5000 renk=4278190080 kalinlik=250
```

Altta siyah çizgi, üstünde 5 metrede bir 4 metre boyunda dişler.

### Mevzuatın yayımladığı gösterim

```
KATMAN OSB
ALAN 485300000,4310200000 485385000,4310200000 485385000,4310260000 485300000,4310260000
STİL katman=OSB paket=data/catalogs/mpyy/plan-gosterim.json kod=ortak-organize-sanayi-bolgesi
```

EK-1a'nın ORGANİZE SANAYİ BÖLGESİ satırı: taraması yüzeye döşenir, çizgi tipi
sınıra, simgesi lekenin ortasına. Hangi satırın olduğunu
[`SEMBOL`](symbol.md) ile bulabilirsiniz.

### Yapılaşma koşulu — sıfırdan

Hazır görsel kullanmadan, tasarımcının kendi parçalarıyla:

```
KATMAN ad=PARSEL
ALAN 485300000,4310200000 485370000,4310200000 485370000,4310252000 485300000,4310252000
STİL katman=PARSEL tip=dolgu dolgu=584376224
STİL katman=PARSEL ekle=evet tip=cizgi renk=4282203457 kalinlik=500
STİL katman=PARSEL ekle=evet tip=merkez-isaretci sekil=daire birim=zemin boyut=26000 renk=4278190080 kalinlik=350 dolgu=0
STİL katman=PARSEL ekle=evet tip=merkez-isaretci sekil=cizik aci=90000000 birim=zemin boyut=22000 renk=4278190080 kalinlik=300
STİL katman=PARSEL ekle=evet tip=yazi-isaretci yazi=TAKS birim=zemin boyut=2600 kaydirma=9500 renk=4286611584
STİL katman=PARSEL ekle=evet tip=yazi-isaretci yazi=KAKS birim=zemin boyut=2600 kaydirma=-9500 renk=4286611584
```

Altı sembol katmanı: dolgu, sınır, daire, ortadaki yatay çizgi, üstteki `TAKS`,
alttaki `KAKS`. Değerler [`ETİKET`](label.md) ile gelir:

```
ETİKET katman=PARSEL bicim="{taks}\n{kaks}" yukseklik=3800 hedef=YAPILAŞMA
```

`aci=90000000` mikro derece, yani 90° — `cizik` şekli dik çizilir, doksan derece
onu yatay yapar.

### Betik

```json
{
  "ad": "Katman stilleri",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "IMAR" } },
    { "cmd": "core.line",  "args": { "noktalar": [[485300000,4310200000],[485360000,4310200000]] } },
    { "cmd": "core.style", "args": { "katman": "IMAR", "renk": 4281236786, "kalinlik": 350, "sira": 20 } }
  ]
}
```

## Geri alma

Stil değişikliği geri alınabilir ve bir komut bir adımdır:

```
GERİAL
```

`GERİAL` stil sütununu eski hâline döndürür. Stil tablosuna eklenen satır tabloda kalır:
tablo yalnız büyür, verilen bir stil numarası çizimin ömrü boyunca geçerli kalır. Bu
bilinçlidir — bir satırı geri almak, ona işaret eden başka nesnelerin ve günlükteki eski
değerlerin numarasını kaydırırdı. Kullanılmayan satır zararsızdır; hiçbir nesne onu
göstermez.

Komut hata verirse çizime **hiç dokunulmaz**. Katalog okunamazsa, satır bulunamazsa veya
hiçbir kural uymazsa karar aşamasında durulur; yarısı stillenmiş bir katman oluşmaz.

## Betikten kullanım

`STİL` betiklenebilir ve AI erişimlidir. Tipik kullanım, bir betiğin sonunda bütün
katmanların görünümünü tek seferde kurmaktır. Betiğin tamamı tek geri alma adımıdır:

```json
[
  { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
  { "cmd": "core.line",  "args": { "noktalar": [[485300000,4310200000],[485360000,4310200000]] } },
  { "cmd": "core.layer", "args": { "ad": "YOL" } },
  { "cmd": "core.line",  "args": { "noktalar": [[485300000,4310260000],[485360000,4310260000]] } },
  { "cmd": "core.style", "args": { "katman": "PARSEL", "renk": 4281236786, "kalinlik": 350, "sira": 20 } },
  { "cmd": "core.style", "args": { "katman": "YOL", "renk": 4284310640, "kalinlik": 500, "sira": 10 } }
]
```

Katalog kullanan bir betikte `paket` alanına dosya yolunu yazarsınız; yol göreliyse betiği
çalıştıran işlemin çalışma dizinine göre çözülür, bu yüzden paylaşılan betiklerde tam yol
tercih edilir.

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Katman bulunamadı: 'IMAR'. Önce KATMAN komutuyla oluşturun.` | Verilen adda katman yok | Adı denetleyin veya `KATMAN ad=IMAR` ile oluşturun |
| `'core.style': zorunlu 'katman' parametresi eksik. Beklenen: metin` | Katman adı verilmemiş | `katman=` ile ad verin |
| `'kod' verildi ama 'paket' verilmedi: hangi katalogdan okunacağı belirsiz. 'paket=' ile katalog dosyasını verin.` | Satır kimliği var, katalog yok | `paket=` ile katalog dosyasını da verin |
| `'sifirla' ile 'paket' aynı komutta kullanılamaz: biri stili siler, diğeri yazar.` | İki zıt istek aynı komutta | İki ayrı komut çalıştırın |
| `Stil kataloğu okunamadı: '...'. Dosya yolunu denetleyin; göreli yol çalışma dizinine göre çözülür.` | Dosya yok veya okunamıyor | Yolu denetleyin, tam yol yazın |
| `Stil kataloğu geçerli JSON değil: '...'` | Paket bozuk | Dosyayı bir JSON doğrulayıcıdan geçirin |
| `Stil kataloğu: zorunlu 'published' alanı eksik veya boş. Beklenen: metin.` | Paket künyesi eksik | `schema_version`, `package_version`, `id`, `source`, `published`, `licence` alanlarının hepsini yazın |
| `Stil kataloğunda 'K' kimlikli satır yok. Katalog: mpyy-plan-gosterimleri 0.2.0.` | `kod` katalogda yok | Katalogdaki satır kimliklerini denetleyin |
| `Stil kataloğunda bu nesneye uyan kural yok.` | Hiçbir eşleme kuralı nesneye uymadı | Katalogda koşulsuz bir "kalan hepsi" kuralı tanımlayın veya `kod=` ile satırı doğrudan seçin |
| `Stil kataloğu: 'k1' kuralı 'yok-boyle' satırını gösteriyor, ama katalogda böyle bir satır yok.` | Katalogda kural ile satır kimliği tutmuyor | Kuraldaki `stil` alanını düzeltin |
| `Renk '#RRGGBB' veya '#AARRGGBB' biçiminde olmalı. Girilen: 'kirmizi'` | Katalogdaki renk metni bozuk | Rengi onaltılık yazın |
| `Bilinmeyen birim: 'metre'. Geçerli olanlar: kagit, zemin, piksel.` | `birim` değeri tanınmadı | `kagit`, `zemin` veya `piksel` yazın |
| `Bilinmeyen birim: boyut_birim='fersah'. Geçerli olanlar: kagit, zemin, piksel.` | Bir ölçünün kendi birimi tanınmadı | Hatalı parametre mesajda adıyla yazılıdır; değerini düzeltin |
| `'desen' çizgi ve boşluk çiftlerinden oluşur ve en çok 8 parça taşır. Verilen parça sayısı: 3.` | Tek sayıda ya da sekizden çok parça | Çizgi/boşluk çiftleri hâlinde, en çok sekiz parça yazın |
| `'desen' çizgi kalınlığının katı olarak sayılardan oluşur; okunamayan parça: 'uzun'.` | Desende sayı olmayan bir sözcük | Sayı yazın; düz çizgi için `desen=sürekli` |
| `'core.style': bilinmeyen parametre 'renkler'. Tanımlı parametreler: katman, paket, kod, olcek, renk, kalinlik, dolgu, sira, sifirla` | Parametre adı yanlış yazılmış | Doğru adı kullanın |

Katmanda hiç nesne yoksa hata olmaz; transkriptte `'IMAR' katmanında nesne yok; stil
yazılmadı.` yazar.

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
