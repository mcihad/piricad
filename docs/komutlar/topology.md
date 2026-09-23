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
için vardır. Yeri belli olan kusurlar (yinelenen, uzunluğu yok, tekrarlanan köşe,
boşluk) tuvalde de işaretlenir.

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

Nesne verilmezse etkin seçim, o da boşsa **bütün çizim** denetlenir. Rapor neyin
denetlendiğini her zaman yazar.

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
Topoloji denetimi (bütün çizim): 1 kusur.
  Nesne 1 ile 2 örtüşüyor: 25,00 m².
  Bu komut hiçbir şeyi düzeltmez: sınır ölçülmüş veridir.
```

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
Topoloji denetimi (bütün çizim): 2 kusur.
  Nesne 5, nesne 1'in aynısı (yinelenen; TEMİZLE islem=onar siler).
  Nesne 1: açık uç, en yakın çizgiye 50 cm (boşluk).
  Bu komut hiçbir şeyi düzeltmez: sınır ölçülmüş veridir.
```

### Arayüz

Sol araç kutusundaki **Topoloji Denetimi** düğmesi. Seçim boşken bütün çizimi
denetler.

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
öncesi denetimi betiğe koymak, paftanın her kaydedilişinde denetlenmesini sağlar.

## Hatalar

Bu komutun hata iletisi yoktur: bulduğunu rapor eder, bulamazsa bulamadığını
yazar.

## İlgili

- [TEMİZLE](cleanup.md) — yinelenenleri ve boş nesneleri bulur, istenirse onarır
- [SINIR](boundary.md) — kapalı bölgenin sınırını çıkarır, açık uçları gösterir
- [TEVHİT](merge.md) — parsel birleştirme
- [İFRAZ](split_parcel.md) — parsel ayırma
- [ALANÖLÇ](measure_area.md) — alan ve çevre
