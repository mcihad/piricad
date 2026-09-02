# TOPOLOJİ — Geometri Denetimi

## Ne yapar

Bir kadastro paftasının teslim edilmeden önce geçmesi gereken denetimleri yapar
ve **rapor eder**:

| Kusur | Ne demek |
|---|---|
| **Sınır kendini kesiyor** | Halka kendi üzerinden geçiyor; alanı belirsiz |
| **Alanı sıfır** | Halka hiçbir şey çevrelemiyor |
| **Örtüşüyor** | İki parsel aynı zemini talep ediyor; örtüşen alan yazılır |

Dört bin parselli bir paftada bunların hiçbiri bakışta görünmez — komut bunun
için vardır.

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

- [TEVHİT](merge.md) — parsel birleştirme
- [İFRAZ](split_parcel.md) — parsel ayırma
- [ALANÖLÇ](measure_area.md) — alan ve çevre
