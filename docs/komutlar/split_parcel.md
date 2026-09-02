# İFRAZ — Parsel Ayırma

## Ne yapar

Bir parseli **düz bir ayırma çizgisiyle** ikiye böler. Çizgi parselin dışına
uzatılır ve iki yarı düzlem parselle kesiştirilir; **hiçbir alan kaybolmaz.**

Girdi parsel silinir, yerine parçalar gelir. Her parçanın alanı ve toplamı yazılır
— ifrazdan önceki alanla birlikte, karşılaştırabilesiniz diye.

### Neden düz çizgi

Bir ifraz paftada neredeyse her zaman düz bir çizgidir: taraflar arasında
anlaşılmış ya da plandan gelen bir sınır. Sonucu **tam** olan biçim de budur.

Kırıklı bir çizgiyle ayırma farklı bir problemdir ve **yaklaşık olarak yapılmaz**;
çünkü bir parselden eksilen birkaç santimetrekare, birinin sahip olduğu birkaç
santimetrekaredir.

### Alan hedefi yoktur

"Bu parselden 400 m² ayır" bir **alana göre ifrazdır**: bir yineleme ve bir
tolerans gerektirir, ikisi de mevzuata ait kararlardır (6.11, 5.13). Bu komut
söylendiği yerden keser ve çıkanı yazar; mühendis planla karşılaştırır.

### Öznitelikler

Her iki parçaya da **değişmeden** kopyalanır. Bu, ifrazın ne anlama geldiği
hakkında bir kural değildir — yeni ada/parsel numaraları TKGM'den gelir — var
olanla yapılabilecek tek yıkıcı olmayan şeydir.

> **Bu bölüm mevzuat imzası bekliyor** (CLAUDE.md 6.11).

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `İFRAZ` | `IFRAZ` | `SUBDIVIDE` | `İFR` |

## Sözdizimi

```text
İFRAZ [noktalar=<uç> <uç>] [nesneler=<kimlik>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Ayırma çizgisinin iki ucu; verilmezse tıklamanız istenir |
| `nesneler` | Ayrılacak parsel; verilmezse etkin seçim |

## Örnekler

### Komut satırı

```text
KATMAN ad=PARSEL
ALAN noktalar=0,0 20,0 20,10 0,10
İFRAZ noktalar=10,-5 10,15 nesneler=1
```

```text
İfraz: 2 parça.
  100,00 m²
  100,00 m²
  toplam 200,00 m²  ·  ifrazdan önce 200,00 m²
```

### Arayüz

Parseli seçin, sol araç kutusundaki **Parsel Böl — ifraz** düğmesine basın, sonra
ayırma çizgisinin iki ucunu tıklayın. Yakalama açıkken uçlar mevcut köşelere
oturur.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.area",  "args": { "noktalar": [[0,0],[20000,0],[20000,10000],[0,10000]] } },
    { "cmd": "core.split_parcel",
      "args": { "nesneler": [1], "noktalar": [[10000,-5000],[10000,15000]] } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` parçaları kaldırır ve asıl parseli geri getirir.

## Betikten kullanım

İki uç argüman olarak verilebilir; betikte hiçbir şey sorulmaz.

## Hatalar

> `İfraz tek parsel üzerinde çalışır.`

Seçim boş ya da birden çok parsel içeriyor.

> `Bu çizgi parseli kesmiyor: ifraz için çizginin parselin içinden geçmesi gerekir.`

Ayırma çizgisi parselin dışından geçiyor.

> `Ayırma çizgisinin iki ucu aynı yerde; kesme yönü belirsiz.`

İki uç çakışık.

> `Nesne <kimlik> kapalı bir alan değil; ifraz yalnız alanlar üzerinde çalışır.`

Seçili nesne bir çizgi ya da nokta.

## İlgili

- [TEVHİT](merge.md) — parsel birleştirme
- [TOPOLOJİ](topology.md) — örtüşme ve sınır denetimi
