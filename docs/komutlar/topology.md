# TOPOLOJİ — Geometri Denetimi

## Ne yapar

Bir kadastro paftasının teslim edilmeden önce geçmesi gereken denetimleri yapar
ve **rapor eder**:

| Kusur | Ne demek |
|---|---|
| **Sınır kendini kesiyor** | Halka kendi üzerinden geçiyor; alanı belirsiz |
| **Alanı sıfır** | Halka hiçbir şey çevrelemiyor |
| **Örtüşüyor** | İki parsel aynı zemini talep ediyor; örtüşen alan yazılır |
| **Yinelenen** | Aynı tür, aynı katman, aynı köşelerle ikinci kez çizilmiş nesne; hangisinin aynısı olduğu yazılır |
| **Uzunluğu yok** | Bütün köşeleri düğüm toleransı içinde tek noktada duran çizgi; hiçbir şey çizmez |
| **Tekrarlanan köşe** | Bir öncekiyle düğüm toleransı içinde aynı yerde duran köşeler; kaç tane olduğu yazılır |
| **Boşluk** | Bir **çizgi ağında** bir ucun başka bir çizgiye değmeden durduğu yer; genişliği yazılır |

Dört bin parselli bir paftada bunların hiçbiri bakışta görünmez — komut bunun
için vardır. Her kusur tuvalde de işaretlenir: bir örtüşme iki parselin birlikte
talep ettiği zemin olarak (alanıyla), bir boşluk açık uçtan en yakın çizgiye bir çizgi
olarak, ötekiler bir nokta olarak.

### Önce sayı, sonra ilk yirmi

Rapor önce **ne kadar** ve **ne tür** kusur bulunduğunu söyler, sonra ilk yirmisini tek
tek yazar; gerisi sayılır ("… ve 480 kusur daha; hepsi tuvalde işaretli."). Dört bin
örtüşmeli bir paftada dört bin satırlık bir döküm, hiç yazılmamış bir döküm kadar
okunmaz. Kusurların **tamamı** yapılandırılmış cevaptadır (bkz. [Betikten
kullanım](#betikten-kullanım)).

### Uzun bir iş: ilerleme ve Durdur

Yüz bin parsellik bir pafta saniyeler sürer ve bu sürede pencere donmaz: denetim ayrı
bir iş parçacığında koşar, durum çubuğu `Topoloji denetimi · %40` gibi ilerlemesini
gösterir ve yanında **Durdur** çipi durur. **Durdur** ya da **Esc** denetimi keser:

```text
Topoloji denetimi durduruldu; sonuç verilmedi, çizim değişmedi.
```

Yarım bir denetim daha küçük bir denetim değildir — "kusur bulunamadı" demesi yalan
olurdu — bu yüzden durdurulan denetim hiçbir kusur söylemez ve komut günlüğüne
yazılmaz. **Enter** ya da sağ tık işi durdurmaz; iş kendi bitince biter. Denetim
sürerken çizim değiştirilemez; yazılan bir komut `Bir iş sürüyor (Topoloji
denetimi)…` cevabını alır (bkz. [Uzun işler](../baslangic/arayuz.md#uzun-işler)).

Denetim, parsellerin sınır kutuları üzerine kurulan bir mekânsal dizinle (R-ağacı)
yalnız **komşu** parselleri karşılaştırır; yüz bin parsel bir saniyenin altında
denetlenir.

### Aynı çekirdek

Yinelenen, uzunluğu olmayan ve tekrarlanan köşeli nesneler [`TEMİZLE`](cleanup.md)'nin
bulduğuyla aynı bulucudan gelir; çizgi ağındaki boşluklar [`SINIR`](boundary.md) ve
[`ALANÜRET`](alan_uret.md)'in kapatmayı reddettiği boşluklarla aynı ağdan. Denetim ile
onarım aynı nesneleri aynı gerekçeyle görür. Boşluk yalnız açık çizgilerde aranır; bir
ucun kendi çizgisi, az önce kestiği çizgi ya da kendi zincirinden uzun bir mesafe
boşluk sayılmaz. İki ucu birbirini gören bir boşluk bir kez yazılır.

Birbirine projenin düğüm toleransından (`core.topoloji.dugum_toleransi`, varsayılan
1 cm) yakın iki köşe aynı köşe sayılır.

### Hiçbir şeyi düzeltmez

Otomatik bir düzeltme **sınırı oynatırdı**, ve sınır ölçülmüş veridir. Bir kusurun
ne anlama geldiğine mühendis karar verir ve sonucu yalnız o imzalayabilir
(CLAUDE.md 6.11).

### Ortak sınır örtüşme değildir

Sınır paylaşan iki parsel o sınır boyunca değer; ortak bir köşenin milimetreye
yuvarlanması birkaç milimetrekarelik bir dilim bırakabilir. Bu bir kusur olarak
bildirilseydi gerçek örtüşmeler kimsenin okumadığı bir raporun altında kalırdı, o
yüzden bir santimetrekarelik pay vardır.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `TOPOLOJİ` | `TOPOLOJI` | `TOPOLOGY` | `TPL` |

## Sözdizimi

```text
TOPOLOJİ [nesneler=<kimlikler>]
```

Nesne verilmezse etkin seçim, o da boşsa **bütün çizim** denetlenir. Rapor neyin ve
kaç nesnenin denetlendiğini her zaman yazar.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Denetlenecek nesneler; yoksa seçim, o da boşsa bütün çizim |

## Örnekler

### Komut satırı

```text
KATMAN ad=PARSEL
ALAN noktalar=0,0 10,0 10,10 0,10
ALAN noktalar=5,5 15,5 15,15 5,15
TOPOLOJİ
```

```text
Topoloji denetimi (bütün çizim, 2 nesne): 1 kusur — 1 örtüşme.
  Nesne 1 ile 2 örtüşüyor: 25,00 m².
  Bu komut hiçbir şeyi düzeltmez: sınır ölçülmüş veridir.
```

Tuvalde iki parselin birlikte talep ettiği 5 m × 5 m'lik kare, `örtüşme 25,00 m²`
yazısıyla işaretlenir.

Bir çizgi ağında köşesine 50 cm varmayan bir kenar ve iki kez çizilmiş bir çizgi:

```text
ÇİZGİ 60,0 70,0
ÇİZGİ 70,0 70,10
ÇİZGİ 70,10 60,10
ÇİZGİ 60,10 60,0.5
ÇİZGİ 60,0 70,0
TOPOLOJİ
```

```text
Topoloji denetimi (bütün çizim, 5 nesne): 2 kusur — 1 yinelenen nesne, 1 boşluk.
  Nesne 5, nesne 1'in aynısı (yinelenen; TEMİZLE islem=onar siler).
  Nesne 1: açık uç, en yakın çizgiye 50 cm (boşluk).
  Bu komut hiçbir şeyi düzeltmez: sınır ölçülmüş veridir.
```

### Arayüz

Şeritte **Kadastro ▸ Denetim ▸ Topoloji Denetimi** (kapalı bir alan seçiliyken beliren
**Alan** sekmesinde de vardır). Seçim boşken bütün çizimi denetler. Denetim sürerken
durum çubuğu ilerlemeyi yüzde olarak gösterir; **Durdur** çipi ya da **Esc** keser.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer",    "args": { "ad": "PARSEL" } },
    { "cmd": "core.area",     "args": { "noktalar": [[0,0],[10000,0],[10000,10000],[0,10000]] } },
    { "cmd": "core.topology", "args": {} }
  ]
}
```

## Geri alma

Geri alınacak bir şey yoktur: `TOPOLOJİ` çizimi değiştirmez ve geri alma yığınına
bir adım eklemez.

## Betikten kullanım

Salt okunur olduğu için bir betiğin herhangi bir yerinde çağrılabilir. Teslim
öncesi denetimi betiğe koymak, paftanın her kaydedilişinde denetlenmesini sağlar. Bir
betiğin içinde denetim yerinde, betiğin sırasında koşar; cevabı arayüzdekiyle
kelimesi kelimesine aynıdır.

**Yapılandırılmış cevap** — bir betiğin ya da yapay zekâ istemcisinin okuduğu —
bütün kusurları taşır: `kapsam` (`cizim` ya da `secim`), `bakilan` (kaç nesne
denetlendi), `kusur` (kaç kusur), `turler` (türe göre sayılar: `ortusme`,
`kendini_kesen`, `sifir_alan`, `yinelenen`, `bos`, `tekrarlanan_kose`, `bosluk`) ve
`kusurlar`: her biri için `tur`, `nesne`, varsa `diger` (öteki nesne), örtüşmede
`alan_mm2`, tekrarlanan köşede `kose`, boşlukta `uc` ve `genislik_mm`, `nokta`
(tuvalde işaretlendiği yer, milimetre) ve `aciklama` (transkriptteki cümle).

## Hatalar

Bu komutun hata iletisi yoktur: bulduğunu rapor eder, bulamazsa bulamadığını
yazar. Durdurulursa bunu söyler:

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Topoloji denetimi durduruldu; sonuç verilmedi, çizim değişmedi.` | Denetim sürerken **Durdur** ya da **Esc**'e basıldı | Hata değildir; denetimi yeniden çalıştırın |

## İlgili

- [TEMİZLE](cleanup.md) — yinelenenleri ve boş nesneleri bulur, istenirse onarır
- [SINIR](boundary.md) — kapalı bölgenin sınırını çıkarır, açık uçları gösterir
- [TEVHİT](merge.md) — parsel birleştirme
- [İFRAZ](split_parcel.md) — parsel ayırma
- [ALANÖLÇ](measure_area.md) — alan ve çevre
