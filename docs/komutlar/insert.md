# BLOKEKLE — Blok Yerleştirme

## Ne yapar

Tanımlı bir bloğu bir noktaya yerleştirir: ölçekle, açıyla, eksi ölçekle **aynalı**,
ve `sutun`/`satir` ile bir **dizi** hâlinde. `yontem=2n` ile de **eni iki noktanın
arasına oturacak** biçimde yerleştirir. Sonuç bir
[blok referansıdır](../nesneler/blokreferansi.md): tanımın nesneleri kopyalanmaz,
referans onları çizerken yerleştirir. Dönüşüm kesin oran ve mikroderece olarak
saklanır; 90° döndürülmüş bir sembol tam olarak 90° döner.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `BLOKEKLE` | `BLOKEKLE` | `INSERT` | `BE` |

## Sözdizimi

```text
BLOKEKLE ad=<ad> nokta=<sağa>,<yukarı> [olcek=<çarpan>] [olcek_y=<çarpan>] [aci=<derece>]
         [sutun=<n> satir=<n> sutun_aralik=<mm> satir_aralik=<mm>]
         [deger=<sütun>:<değer> …]
BLOKEKLE ad=<ad> yontem=2n noktalar=<ilk nokta> <ikinci nokta>
         [sutun=<n> satir=<n> sutun_aralik=<mm> satir_aralik=<mm>]
BLOKEKLE dosya=<kitaplık> [ad=<blok>] nokta=<sağa>,<yukarı> [...]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Yerleştirilecek bloğun adı |
| `nokta` | Ekleme noktası: bloğun taban noktasının konacağı yer. `yontem=2n` ile verilmez |
| `yontem` | `2n`: bloğun eni iki noktanın arasına oturur; ölçek ve açı iki noktadan gelir. Verilmezse blok `nokta`ya yerleştirilir |
| `noktalar` | `yontem=2n` için iki nokta: ilki bloğun sol ucunun, ikincisi sağ ucunun geleceği yer |
| `olcek` | Ölçek; varsayılan 1. Eksi değer aynalar — ama `olcek_y` verilmezse o da eksi olur ve iki eksi ayna birlikte **yarım dönüştür**: yalnız x'te aynalamak için `olcek=-1 olcek_y=1` yazın |
| `olcek_y` | Y ölçeği farklıysa; varsayılan `olcek` |
| `aci` | Dönme açısı, derece, saat yönünün tersine; varsayılan 0 |
| `sutun`, `satir` | Dizi sütun ve satır sayısı; varsayılan 1 |
| `sutun_aralik`, `satir_aralik` | Dizide kopyalar arası, **milimetre**, döndürülmüş eksende, ölçeklenmez |
| `deger` | Bloğun alanlarının bu referanstaki değerleri, `sütun:değer` biçiminde; birden çok alan için anahtarı yineleyin. Verilmezse elle yerleştirmede her alan sorulur |
| `dosya` | Blok kitaplığı: bloğun alınacağı proje (`.pcad`), DXF ya da DWG dosyası. `ad=` o dosyadaki bloğu seçer |
| `katman` | Çizilenlerin katmanı, adıyla; verilmezse etkin katman. Etkin katmanı değiştirmez; komut soru sorarken de yazılabilir. Bkz. [Çizimin katmanı](komut-satiri.md#çizimin-katmanı-katman) |

### İki noktayla: bloğun eni iki noktanın arasına

Bir kapı, bir pencere, bir sütun sembolü çoğu zaman *şu iki nokta arasına* konur: ölçü
krokisinde iki ucu bellidir, kaç kat büyüyeceği belli değildir. `yontem=2n` ile iki
noktayı verirsiniz; ölçek ve açı bunlardan çıkar (Netcad'in sembol ve blok yerleştirmesindeki
"2 nokta ile"si):

- **Açı.** Bloğun x ekseni birinci noktadan ikinciye bakar.
- **Ölçek.** Bloğun **eni** — çizilmiş biçiminin x doğrultusundaki genişliği — iki nokta
  arasındaki uzaklığa eşitlenir. Ölçek tek sayıdır: yükseklik de aynı oranda büyür. Tam
  oran olarak saklanır (2 m'lik blok 5 m'ye sığınca 5/2), ondalık olarak yuvarlanmaz.
- **Yer.** Blok, eni iki noktanın arasında kalacak biçimde konur. Taban noktası bloğun sol
  ucundaysa taban birinci noktaya oturur; ortadaysa iki noktanın ortasına düşer. Tabanın
  kendi yüksekliği iki noktayı birleştiren doğru üzerinde kalır.

`nokta`, `olcek`, `olcek_y` ve `aci` `2n` ile birlikte verilmez — iki nokta bunları zaten
söyler — ve `noktalar` `2n` olmadan verilmez. Dizi (`sutun`, `satir`) ilk kopyanın eniyle
kurulur.

Arayüzde bloğun adı seçilir, sonra iki nokta tıklanır; ikinci nokta fareyle aranırken
birinciden fareye bir çizgi izler: blok o çizgi boyunca yatacaktır.

2 m eninde, taban noktası sol ucunda bir kapı bloğu; önce (10,0)–(14,0) arasına, sonra
(20,0)–(23,4) arasına:

<!-- örnek: yeni çizim -->
```
DİKDÖRTGEN 0,0 2,1
BLOK ad=KAPI taban=0,0 nesneler=1
BLOKEKLE ad=KAPI yontem=2n noktalar=10,0 14,0
```

```text
'KAPI' bloğu iki noktanın arasına yerleştirildi (ölçek 2,000, yön 100,0000 grad).
```

Dört metre iki metrenin iki katıdır; blok doğuya bakar (100 grad) ve sağ ucu ikinci noktaya
gelir. İkinci yerleştirme eğiktir:

```
BLOKEKLE ad=KAPI yontem=2n noktalar=20,0 23,4
```

```text
'KAPI' bloğu iki noktanın arasına yerleştirildi (ölçek 2,500, yön 40,9666 grad).
```

3-4-5: iki nokta 5 m arayla, blok 2 m; ölçek 5/2, blok kuzeyden saat yönünde 40,9666 grad
dönmüştür. Sağ ucu ikinci noktaya milimetresine dek gelir.

### Kitaplıktan blok

Semboller ayrı bir dosyada — bir proje, DXF ya da DWG — tutulabilir. `dosya=` o dosyadaki
bloğu **bütün** olarak (üyeleri ve içindeki bloklarla) bu çizime getirir ve her zamanki
gibi yerleştirir:

| Dosyada | Ne alınır |
|---|---|
| `ad=` ile adı verilen blok | O blok |
| Tek blok | O blok, adı sorulmadan |
| Birden çok blok, `ad=` yok | Arayüzde hangisi olduğu sorulur; betikte bloklar adlarıyla söylenir |
| Hiç blok yok | **Bütün çizim**, dosyanın adıyla bir blok olur (taban noktası 0,0) |

Çizimde aynı adlı bir blok zaten varsa **çizimdeki tanım** kullanılır, kitaplıktaki
alınmaz — her CAD programının kuralı budur; kitaplıktaki yeni hâli istiyorsanız çizimdeki
bloğu [`BLOKDÜZENLE`](block_edit.md) ile güncelleyin. Kitaplıktaki DXF ya da DWG koordinat
sistemi bildirmiyorsa çizimin sistemi varsayılır: bir sembol tanımının kendi
koordinatında durur, sistemin burada bir anlamı yoktur.

### Alanlar ve değerleri

Bloğun içinde `{no}` gibi süslü ayraçlı bir yazı varsa `no` o bloğun **alanıdır**
(AutoCAD'deki öznitelik tanımı, DXF'in `ATTDEF`'i). Her referans alanın **kendi değerini**
taşır — referansın `no` sütunundaki hücresi — ve yazıyı o değerle çizer; aynı nokta
sembolü her yerde kendi numarasını yazar. Değeri eklerken `deger=no:K-12` ile verirsiniz;
arayüzde bloğu tıklayıp yerleştirince her alan, yazısının duracağı yerde açılan kutuda
sorulur (boş Enter boş bırakır). Sonradan değiştirmek için referansı seçip nitelik
panelinde ya da [`ÖZNİTELİK`](attribute.md) ile hücresini düzenleyin. Alanın sütunu yoksa
metin sütunu olarak tanımlanır.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```text
KATMAN ad=ROGAR2
DAİRE merkez=0,0 cevre=0.6,0
SEÇ KATMAN katman=ROGAR2
BLOK ad=BACA taban=0,0
BLOKEKLE ad=BACA nokta=20,0
BLOKEKLE ad=BACA nokta=40,0 olcek=2 aci=90
BLOKEKLE ad=BACA nokta=60,0 olcek=-1 olcek_y=1
BLOKEKLE ad=BACA nokta=80,0 sutun=3 satir=2 sutun_aralik=5000 satir_aralik=4000
```

Sırayla: olduğu gibi, iki kat büyütülüp çeyrek tur dönmüş, x'te aynalanmış, ve 5 m'ye
4 m aralıklı 3×2 dizi.

Kitaplıktaki bir rögar sembolü, iki kat büyük:

```text
BLOKEKLE dosya="semboller.pcad" ad=ROGAR nokta=100,100 olcek=2
```

Numaralı bir nokta sembolü, iki kez, kendi numaralarıyla:

```text
METİN noktalar=0,1 yazi={no} yukseklik=500
DAİRE merkez=0,0 cevre=0.3,0
BLOK ad=NOKTA taban=0,0 nesneler=1 nesneler=2
BLOKEKLE ad=NOKTA nokta=10,10 deger=no:K-1
BLOKEKLE ad=NOKTA nokta=20,10 deger=no:K-2
```

### Arayüz

**Çizim ▸ Blok ▸ Blok Ekle** (bir blok seçiliyken beliren **Blok** sekmesinde de vardır).
Bloğun adını yazın, ekleme noktasını tıklayın. Bir kitaplık dosyasından eklemek için
**Çizim ▸ Blok ▸ Kitaplıktan Ekle**: dosyayı seçin, dosyada birden çok blok varsa hangisi
olduğu sorulur.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "ROGAR2" } },
    { "cmd": "core.circle_draw", "args": { "merkez": [0,0], "cevre": [600,0] } },
    { "cmd": "core.block", "args": { "ad": "BACA", "taban": [0,0], "nesneler": [1] } },
    { "cmd": "core.insert", "args": { "ad": "BACA", "nokta": [20000,0], "olcek": 2, "aci": 90 } }
  ]
}
```

Bloğun eni iki noktanın arasına (koordinatlar milimetredir):

```json
{
  "ad": "Kapı, iki noktayla",
  "komutlar": [
    { "cmd": "core.rectangle", "args": { "noktalar": [[0,0], [2000,1000]] } },
    { "cmd": "core.block", "args": { "ad": "KAPI", "taban": [0,0], "nesneler": [1] } },
    { "cmd": "core.insert", "args": { "ad": "KAPI", "yontem": "2n",
                                      "noktalar": [[10000,0], [14000,0]] } }
  ]
}
```

Python'da parametreler `method="2n"` ve `points=[...]`'dır.

## Geri alma

Tek adımdır: `GERİAL` referansı kaldırır; tanım kalır.

## Betikten kullanım

Günlüğe ad, nokta, ölçek, açı ve verildiyse dizi sayıları yazılır; ölçek ondalık
olarak yazılır ve altı basamağa yuvarlanmış bir orana çevrilir.

`yontem=2n` günlüğe **iki noktayı** yazar — ad, `yontem=2n` ve `noktalar` — ve ondan
türeyen ölçeği, açıyı ve tabanın konduğu yeri **yazmaz**: aynı soruya iki cevap bir günlük
satırında bulunmaz. Günlük yeniden oynatılınca aynı iki nokta aynı ölçeği, aynı açıyı ve
aynı yeri hesaplar.

## Hatalar

> `'BACA' adında blok yok. Tanımlı bloklar: KAPAK.`

Önce `BLOK` ile tanımlayın ya da adı düzeltin.

> `BLOKEKLE yontem=2n iki nokta ister: bloğun sol ucunun ve sağ ucunun geleceği yer; 1 nokta verildi.`

Betikte ya da tek başına çalışan satırda `yontem=2n`'in ikinci noktası yok; arayüzde başlatılan
komut onu sorar. İki noktayı da `noktalar=` ile verin.

> `Blok ölçeği sıfır olamaz; aynalamak için eksi bir ölçek verin.`

`olcek=0`.

> `Birden çok sütun ya da satır için aralık (milimetre) verin: sutun_aralik= ve satir_aralik=.`

Dizi istendi, aralık verilmedi.

> `Yerleştirilecek blok verilmedi: ad=<blok> ya da dosya=<kitaplık>.`

Betik bloğun adını vermedi.

> `Blok 'X' 'Y' alanını taşımıyor. Alanları: …`

`deger=` bloğun taşımadığı bir alan adı verdi; alanları mesajda yazılıdır.

> `deger 'sutun:değer' biçiminde yazılır; verilen: '…'.`

`deger=` içinde `:` yok.

> `'…' içinde birden çok blok var; hangisi: ad=<blok>. Bloklar: …`

Kitaplık dosyasında birden çok blok var ve betik hangisi olduğunu söylemedi.

> `'…' içinde 'X' bloğu yok.`

`ad=` kitaplıkta olmayan bir blok; dosyadaki bloklar mesajda yazılıdır.

> `Blok kitaplığı bulunamadı: …`

`dosya=` yolu yanlış.

> `` `nokta` yontem=2n ile verilmez: ekleme yeri, ölçek ve açı iki noktadan gelir. ``

`yontem=2n` ile birlikte `nokta`, `olcek`, `olcek_y` ya da `aci` verildi (mesaj hangisi
olduğunu yazar); kaldırın — iki nokta bunları zaten belirler.

> `` `noktalar` yalnız yontem=2n ile verilir; öbür yerleştirme `nokta` ister. ``

`noktalar` verildi ama `yontem=2n` yazılmadı.

> `'core.insert': 'noktalar' parametresi en fazla 2 değer alır, 3 değer geldi.`

`yontem=2n` iki noktayla çalışır: bloğun iki ucu.

> `İki nokta aynı yerde; blok bir uzaklığa sığdırılır, sıfır uzaklığa değil.`

İkinci nokta birinciyle aynı; aralarında bloğun eni sığacak bir uzaklık yok.

> `'DIK' bloğunun eni sıfır (bütün üyeleri aynı düşey doğru üzerinde); iki noktaya sığdırılamaz.`

Bloğun çizilmiş biçiminin genişliği sıfır: örneğin tek bir düşey çizgiden oluşuyor.
Ölçek `en` ile bölünerek bulunduğu için böyle bir blok `2n` ile yerleştirilemez; `nokta`
ve `olcek` ile yerleştirin.

> `'X' bloğunda çizilecek bir şey yok; eni ölçülemez.`

Bloğun üyeleri çizilecek bir şey vermiyor.

> `Blok iki noktanın arasına yerleştirilemedi.`

Önceki denetimlerin yakalayamadığı bir durum; iki noktayı ve bloğu denetleyin.

## İlgili

- [BLOK](block.md) — tanım yapmak
- [DİZİ](array.md) — herhangi bir nesneyi çoğaltmak
- [DİKDÖRTGEN](rectangle.md) — kutu ve bina: ölçüyle ve derinlikle dikdörtgen
- [Blok referansı türü](../nesneler/blokreferansi.md)
