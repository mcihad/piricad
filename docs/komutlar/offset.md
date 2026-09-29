# OFSET — Paralel Çizme

Bir yol şeridini eksenden, çekme mesafesini parsel sınırından ya da bir kanalın
kenarını ekseninden çıkaran herkes için; bu sayfayı bitirdiğinizde paralel çizmeyi
arayüzden, komut satırından ve betikten yapmayı bileceksiniz.

## Ne yapar

Seçili nesnelerin verilen mesafede, **gösterdiğiniz tarafta** paralelini çizer.
Paralel, kaynağıyla **aynı türde** bir nesnedir:

| Kaynak | Paraleli |
|---|---|
| Açık çizgi, çoklu çizgi | Tek yanda **açık** bir çizgi — `(0,0)→(10,0)` çizgisinin sol 2 m paraleli `(0,2)→(10,2)`'dir |
| Alan (delikli de olsa) | Alan; delikler delik olarak kalır |
| Daire | Daire; yalnız yarıçapı değişir |
| Yay | Aynı merkezli yay |
| Yaylı çoklu çizgi, köşesi yuvarlanmış parsel | **Yaylı çoklu çizgi**: her yay aynı merkezli bir yay, yarıçapı mesafe kadar büyümüş ya da küçülmüş; düz kenarlar kaydırılmış düz kenarlar. İçeri alırken yarıçaptan derine inilirse yay kalkar, köşe sivri olur |
| Elips, spline | Kendi türünde paraleli yoktur: çizildiği hâlinin tam paraleli alınır ve **çoklu çizgi** olur; komut çizimin gerçek eğriden en çok ne kadar saptığını yazar |

Nokta, yazı, ölçü, kılavuz çizgi, tarama ve blok referansının paraleli olmaz; komut
sebebini söyler (taramada sınırın, ölçüde ölçtüğü çizginin paralelini alın).

**Taraf.** Açık bir çizginin tarafları **sol** ve **sağ**dır — çizildiği yöne
bakarak. Çizginin yönünü çevirirseniz sol ile sağ da yer değiştirir. Kapalı bir
şeklin tarafları **dış** ve **iç**tir; yayda dış merkezden uzak, iç merkeze
yakındır. **İki** her iki yanı birden çizer.

**Aslını yerinde bırakır.** Paralel yeni bir nesnedir; sınır ölçülmüş olandır ve
oynatılmaz. Paralel, kaynağın katmanına ve stiline çizilir; istenirse etkin
katmana (`ozellik=aktif`) ve kaynağı silerek (`kaynak=sil`).

**Öznitelikleri de taşır.** Kaynağın öznitelik değerleri (örneğin yolun adı)
paralele aktarılır — [`KOPYALA`](copy.md)'nın ve [`BÖL`](split.md)'ün yaptığı
gibi: bir yol ekseninin bordür çizgileri o yolundur. Paralel kaynağın kendisi
değilse — bir parselin içine çizilen çekme hattı gibi — `oznitelik=aktarma` verin;
yoksa öznitelik tablosunda aynı ada/parsel numarasını taşıyan iki satır olur.

**Bant değildir.** Bu komut önceden açık bir çizginin iki yanını saran kapalı bir
alan (bant) üretiyordu. O bir paralel değil, bir **tampondur**; artık üretilmez.

### Kapanan şekiller

Bir parseli kendi genişliğinin yarısından fazla içeri alırsanız, ya da bir daireyi
yarıçapından fazla küçültürseniz, geriye bir şey kalmaz. Komut bunu nesne nesne
söyler; hiçbir nesnenin paraleli kalmıyorsa hiçbir şey çizmez ve hata verir.

Bel veren bir şekli içeri almak onu **ikiye bölebilir**; o zaman iki paralel birden
çizilir. İkisi de doğru cevaptır.

### Yuvarlak köşe gerçek yaydır

`kose=YUVARLAK` ile dış köşeler **gerçek bir yay** olur: merkezi kaynağın köşesi,
yarıçapı paralel mesafesi. Paralel bu yüzden **yaylı çoklu çizgi** (ya da yaylı
kenarlı alan) olarak çizilir. 20 × 10 m'lik bir parselin 1 m dış paraleli
yuvarlak köşeyle 263,14 m² tutar: 200 m², her kenar boyunca 1 m'lik şerit (60 m²)
ve dört köşede birleşince tam bir 1 m'lik daire (π m²). Köşenin **iç** tarafı
yuvarlanmaz, kesilir; açık bir L'nin iç yanı keskin köşeli kalır.

`kose=KÖŞE` (öntanımlı) ve `kose=PAH` düz kenarlı şekillerde eskisi gibi çalışır.

İki durum bu sürümde yay yerine kısa kenarlarla çizilir ve komut bunu söyler:
**delikli bir alanın** yuvarlak köşeli paraleli (yaylı kenarlı bir nesne tek halka
taşır, deliği tutamaz) ve **yaylı çoklu çizginin pahlı** (`kose=PAH`) paraleli.

### Ucuna bağla: kenar uzunlukları korunur

`kose=uc`, Netcad'in Paralel aracındaki **Ucuna Bağla** köşe yöntemidir. Her düz kenar
**kendi başına**, uzunluğu değişmeden yana kaydırılır; ardışık iki kaydırılmış kenarın
uçları düz bir çizgiyle birbirine **bağlanır**. Hiçbir şey kesişme noktasına kadar
uzatılmaz, hiçbir şey kırpılmaz.

| Kırığın yanı | Ne olur |
|---|---|
| **Dış** — kırığın döndüğü yönün ters yanı | Bağ, iki kenarın uçlarını birleştiren kiriştir; sonuç `kose=PAH` ile aynıdır |
| **İç** — kırığın döndüğü yön | Kaydırılmış kenarlar birbirinin üstünden geçer, bağ aralarından geri döner ve çizgi **kendini keser** |

`(0,0)`, `(10,0)`, `(10,10)` çizgisinin 2 m **sağı** `(0,-2)`, `(10,-2)`, `(12,0)`,
`(12,10)`'dur: iki kenar da 10 m'dir. 2 m **solu** `(0,2)`, `(10,2)`, `(8,0)`, `(8,10)`'dur:
kenarlar yine 10 m'dir ve çizgi `(8,2)`'de kendini keser; fazlalığı [`BUDA`](trim.md) ile
kesersiniz. `KÖŞE` ve `PAH` ise iç köşeyi kesişimde kendiliğinden kırpar, kenarlar kısalır.

Kapalı bir şekilde: dışa büyütülen dışbükey bir şekil `PAH`'ın sekizgenini verir. **İçe**
alınan şekilde her köşe iç köşedir, sonuç bir **çarkıfelek**tir — kenarlar korunur, ama
halka kendini keser ve alanı anlamlı bir parsel alanı değildir. Yöntem açık çizgiler ve dışa
büyütülen şekiller içindir. Delikli alanda delik de aynı kuralla kaydırılır.

Yalnız **düz kenar** uzunluğunu koruyabilir: bir kenarı yay ya da eğri olan nesnede
(yaylı çoklu çizgi, elips, spline) komut sebebini söyler ve hiçbir şey çizmez. Daire ve yayın
köşesi yoktur; paralelleri her zamanki gibi alınır.

### Hangi kütüphane

Ofset çözülmüş bir problemdir ve CLAUDE.md 5.16 çözülmüş bir problemi yeniden
yazmayı yasaklar. **Yay taşıyan** her paralel — yaylı çoklu çizginin ve yuvarlak
köşelinin — geometri çekirdeği **OpenCASCADE** ile hesaplanır
([Geometri çekirdeği](../veri/geometri-cekirdegi.md)); sonuç bir kez milimetreye
yuvarlanır ve üç platformda aynı milimetreyi verir. Düz kenarlı şeklin keskin ve pahlı
köşeli paraleli **Clipper2** ile kaydırılır; hangi parçanın istenen tarafta kaldığı
kaynak kenara göre tam aritmetikle belirlenir. `kose=uc` için her kenar yine aynı
işlevle, tek başına kaydırılır — tek kenarın çözülecek bir köşesi yoktur — ve sonuçlar
zincirlenir. Clipper2'nin BSL-1.0 lisansı ve OpenCASCADE'in LGPL-2.1 (istisnalı) lisansı
GPLv3 ile uyumludur.

## Adlar

| Ad | Tür |
|---|---|
| `OFSET` | Türkçe, birincil |
| `PARALEL` | Türkçe eş ad: Netcad'deki adı (Paralel) |
| `OFFSET` | İngilizce karşılık |
| `OF` | Kısaltma |
| `core.offset` | Komut kimliği |

## Sözdizimi

```text
OFSET [nesneler=<kimlikler>] [mesafe=<mm>] [taraf=sol|sag|dis|ic|iki] [nokta=<n>]
      [kose=KÖŞE|YUVARLAK|PAH|UC] [kaynak=koru|sil] [ozellik=kaynak|aktif]
      [oznitelik=aktar|aktarma]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Paraleli alınacak nesneler; verilmezse etkin seçim, o da boşsa sorulur |
| `mesafe` | Paralel mesafesi, **milimetre**. Verilmezse **metre** olarak sorulur |
| `taraf` | `sol`, `sag` (açık çizgi), `dis`, `ic` (kapalı şekil, yay), `iki` (iki yan) |
| `nokta` | Tarafı gösteren nokta: her nesnenin paraleli bu noktanın olduğu yana düşer |
| `kose` | Köşenin biçimi: `KÖŞE` ya da `keskin` (öntanımlı), `YUVARLAK`, `PAH`, `UC` (ucuna bağla: kenar uzunlukları korunur) |
| `kaynak` | `koru` (öntanımlı) ya da `sil`: paralel çizilince kaynak silinir |
| `ozellik` | `kaynak` (öntanımlı): paralel kaynağın katmanına ve stiline; `aktif`: etkin katmana |
| `oznitelik` | `aktar` (öntanımlı): kaynağın öznitelik değerleri paralele de yazılır; `aktarma`: paralel boş başlar |

**Ne taraf ne nokta verilirse** mesafenin işareti karar verir: kapalı bir şekilde
artı dışarı, eksi içeri; açık bir çizgide **iki yan** çizilir. Bu yalnız mesafeyi
satırın kendisi verdiğinde geçerlidir — mesafe istemde yazıldıysa taraf her zaman
gösterilir.

## Örnekler

### Komut satırı

Bir yol ekseninin 3,5 m solundaki şerit kenarı:

```text
ÇOKLUÇİZGİ 0,0 50,0 80,20
OFSET nesneler=1 mesafe=3500 taraf=sol
```

```text
1 paralel çizildi (3,500 m).
```

Bir parselin 5 m içerisi (çekme mesafesi), işaretle:

```text
ALAN 0,0 30,0 30,20 0,20
OFSET nesneler=2 mesafe=-5000
```

Tarafı bir noktayla göstermek:

```text
OFSET nesneler=1 mesafe=2000 nokta=25,-5
```

Bir parselin içine öznitelik taşımayan bir çekme hattı çizmek:

```text
ALAN 0,0 30,0 30,20 0,20
OFSET nesneler=1 mesafe=5000 taraf=ic oznitelik=aktarma
```

<!-- örnek: yeni çizim -->
Köşelerde kenar uzunluklarını korumak (**ucuna bağla**): doğuya sonra kuzeye giden bir
çizginin 2 m sağı ve solu.

```
ÇOKLUÇİZGİ 0,0 10,0 10,10
OFSET nesneler=1 mesafe=2000 taraf=sag kose=uc
OFSET nesneler=1 mesafe=2000 taraf=sol kose=uc
```

```text
1 paralel çizildi (2,000 m).
1 paralel çizildi (2,000 m).
```

Sağ yan `(0,-2)`, `(10,-2)`, `(12,0)`, `(12,10)` noktalarından, sol yan `(0,2)`, `(10,2)`,
`(8,0)`, `(8,10)` noktalarından geçer; ikisinin de iki uzun kenarı 10 m'dir. Sol yan
`(8,2)`'de kendini keser.

### Arayüz

Şeritte **Giriş ▸ Değiştir ▸ Ofset**'e (ya da **Değiştir ▸ Dizi ve Ofset ▸ Ofset**'e) basın.

1. Nesneler seçili değilse komut sorar: tıklayın ya da kutu sürükleyin, sonra Enter.
2. **Mesafeyi** metre olarak yazın ve Enter'a basın (`3.5`).
3. İmleci nesnenin bir yanına götürün: paralel **imlecin olduğu tarafta** vurgulu
   çizilir. İmleci karşı tarafa geçirince paralel de geçer. İstediğiniz tarafta
   tıklayın.

Birden çok nesne seçtiyseniz her birinin paraleli, imlecin o nesneye göre olduğu
yana düşer. Esc ya da boş bir Enter hiçbir şey çizmeden vazgeçer.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer",  "args": { "ad": "YOL" } },
    { "cmd": "core.polyline", "args": { "noktalar": [[0,0],[50000,0]] } },
    { "cmd": "core.offset", "args": { "nesneler": [1], "mesafe": 3500, "taraf": "sol" } },
    { "cmd": "core.offset", "args": { "nesneler": [1], "mesafe": 3500, "taraf": "sag" } }
  ]
}
```

## Geri alma

Tek adımdır: bir `OFSET` kaç paralel çizmiş olursa olsun tek `GERİAL` hepsini
kaldırır; `kaynak=sil` verildiyse kaynağı da geri getirir.

## Betikten kullanım

`nesneler` verilirse seçime dokunmaz, yani bir betik seçim kurmadan çalışabilir.
Herhangi bir nesne bulunamazsa ya da paraleli olmayan bir nesne varsa **hiçbiri**
çizilmez — işlem bütünüyle geri alınır. Günlüğe `taraf` ya da tarafı gösteren
`nokta` yazılır; yeniden oynatılan satır aynı tarafa çizer.

## Hatalar

> `İşlem yapılacak nesne yok: seçim boş ve 'nesneler' verilmedi.`

Seçim boş ve `nesneler` verilmedi.

> `Paralel mesafesi sıfır olamaz. Kaç metre paralel istediğinizi yazın.`

Sıfır mesafe girdinin kopyasıdır; değeri okumayı unutmuş olma ihtimali yüksek olduğu
için reddedilir.

> `Nesne 1: Bir yazının paraleli olmaz …`

Paraleli olmayan bir tür verildi (nokta, yazı, ölçü, kılavuz çizgi, tarama, blok).

> `Nesne 1: Kapalı bir şeklin solu ya da sağı yoktur; taraf=dis ya da taraf=ic verin.`
>
> `Nesne 1: Açık bir çizginin içi ya da dışı yoktur; taraf=sol ya da taraf=sag verin.`

Kapalı bir şekle `sol`/`sag`, açık bir çizgiye `dis`/`ic` verildi.

> `Nokta çizginin tam üzerinde; paralelin gideceği tarafı çizginin bir yanında gösterin.`

Tarafı gösteren nokta çizginin üstüne düştü.

> `Paralelin tarafı gösterilmedi. …`

Taraf sorulduğunda boş bir Enter verildi.

> `Bu mesafede hiçbir nesnenin paraleli kalmıyor: …`

İçeri mesafe şekillerin genişliğini aştı. Daha küçük bir mesafe deneyin.

> `Tanınmayan taraf: '…'. Taraflar: sol / sag / dis / ic / iki`

`taraf` yanlış yazıldı.

> `'core.offset': 'oznitelik' için tanınmayan değer '…'. Kabul edilenler: aktar / aktarma`

`oznitelik` yanlış yazıldı; `aktar` ya da `aktarma` yazın. `kaynak`, `ozellik` ve `kose`
için de aynı biçimde söylenir (`kose` için: `kose / keskin / yuvarlak / pah / uc`).

> `Nesne 1: kose=uc kenar uzunluklarını korur ve bunu yalnız düz kenar yapabilir; bu nesnenin bir kenarı yay ya da eğri. Düz kenarlı bir çizgi ya da alan verin, ya da kose=KÖŞE, YUVARLAK ya da PAH kullanın.`

`kose=uc` yaylı çoklu çizgiye, elipse ya da spline'a verildi; bu nesnelerin paraleli kenar
uzunluğunu koruyamaz. Başka bir köşe biçimi seçin.

## İlgili

- [ALAN](area.md) — kapalı alan çizme
- [ÇİFTÇİZGİ](double_line.md) — ekseni çizerken iki yanına birden, aynı hesapla
- [BUDA](trim.md) — fazlalığı kesme
- [Nesne türleri destek matrisi](../nesneler/destek-matrisi.md) — hangi türün paraleli alınır
