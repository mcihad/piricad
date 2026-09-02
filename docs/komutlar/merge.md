# TEVHİT — Parsel Birleştirme

## Ne yapar

Komşu parselleri **tek parselde** birleştirir. Yeni sınır, girdilerin birlikte
kapladığı alanın dış hattıdır; aradaki dikiş kalkar.

Girdi parseller silinir, yerine tek yeni parsel gelir.

### Bitişik olmayan parseller reddedilir

Birleşme birden çok parça veriyorsa parseller komşu değildir ve komut hiçbir şey
yapmaz:

> `Bu parseller bitişik değil: birleşme 2 ayrı parça veriyor.`

İki ayrı parçayı çizip "tek parsel" demek, TKGM'nin reddedeceği bir kayıt
üretirdi.

### Öznitelikler — uydurmaz

Geometri nesneldir; **öznitelikler değildir.** Birleşmiş bir parselin ada, pafta,
malik ve nitelik bilgileri kadastro uygulamasından ve TKGM'nin kabul ettiğinden
çıkar, aritmetikten değil.

Bu yüzden buradaki kural, yanlış olamayacak tek kuraldır:

- Bütün girdilerde **aynı** olan bir sütun değerini korur.
- Girdilerin **ayrıştığı** bir sütun **boş** gelir, ve komut hangi sütunları
  boşalttığını yazar.

Birinci parselin malikini seçmek bir tapu kaydı uydurmak olurdu.

> **Bu bölüm mevzuat imzası bekliyor** (CLAUDE.md 6.11). Ayrışan sütunları
> dolduran bir kural mevzuata aittir, `/data`'da yaşamalıdır (5.13) ve bir harita
> mühendisinin onayını gerektirir. O gelene kadar komut tahmin etmek yerine
> boş bırakır.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `TEVHİT` | `TEVHIT` | `MERGE` | `TVH` |

## Sözdizimi

```text
TEVHİT [nesneler=<kimlikler>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Birleştirilecek parseller; verilmezse etkin seçim |

## Örnekler

### Komut satırı

```text
KATMAN ad=PARSEL
ALAN noktalar=0,0 10,0 10,10 0,10
ALAN noktalar=10,0 20,0 20,10 10,10
TEVHİT nesneler=1 2
```

```text
2 parsel tevhit edildi.
```

### Arayüz

Parselleri seçin, sol araç kutusundaki **Birleştir — tevhit** düğmesine basın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.area",  "args": { "noktalar": [[0,0],[10000,0],[10000,10000],[0,10000]] } },
    { "cmd": "core.area",  "args": { "noktalar": [[10000,0],[20000,0],[20000,10000],[10000,10000]] } },
    { "cmd": "core.merge", "args": { "nesneler": [1, 2] } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` yeni parseli kaldırır ve girdileri geri getirir.

## Betikten kullanım

`nesneler` verilirse seçime dokunmaz. Herhangi bir parsel bulunamazsa **hiçbiri**
birleştirilmez — işlem bütünüyle geri alınır.

## Hatalar

> `Tevhit en az iki parsel ister.`

Seçim tek parsel ya da boş.

> `Bu parseller bitişik değil: birleşme <n> ayrı parça veriyor.`

Parseller komşu değil.

> `Nesne <kimlik> kapalı bir alan değil; tevhit yalnız alanlar üzerinde çalışır.`

Seçimde bir çizgi ya da nokta var.

## İlgili

- [İFRAZ](split_parcel.md) — parsel ayırma
- [TOPOLOJİ](topology.md) — örtüşme ve sınır denetimi
