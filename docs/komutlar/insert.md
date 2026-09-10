# BLOKEKLE — Blok Yerleştirme

## Ne yapar

Tanımlı bir bloğu bir noktaya yerleştirir: ölçekle, açıyla, eksi ölçekle **aynalı**,
ve `sutun`/`satir` ile bir **dizi** hâlinde. Sonuç bir
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
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Yerleştirilecek bloğun adı |
| `nokta` | Ekleme noktası |
| `olcek` | Ölçek; eksi değer x'te aynalar; varsayılan 1 |
| `olcek_y` | Y ölçeği farklıysa; varsayılan `olcek` |
| `aci` | Dönme açısı, derece, saat yönünün tersine; varsayılan 0 |
| `sutun`, `satir` | Dizi sütun ve satır sayısı; varsayılan 1 |
| `sutun_aralik`, `satir_aralik` | Dizide kopyalar arası, **milimetre**, döndürülmüş eksende, ölçeklenmez |

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
BLOKEKLE ad=BACA nokta=60,0 olcek=-1
BLOKEKLE ad=BACA nokta=80,0 sutun=3 satir=2 sutun_aralik=5000 satir_aralik=4000
```

Sırayla: olduğu gibi, iki kat büyütülüp çeyrek tur dönmüş, x'te aynalanmış, ve 5 m'ye
4 m aralıklı 3×2 dizi.

### Arayüz

**Çizim ▸ Blok Ekle**. Bloğun adını yazın, ekleme noktasını tıklayın.

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

## Geri alma

Tek adımdır: `GERİAL` referansı kaldırır; tanım kalır.

## Betikten kullanım

Günlüğe ad, nokta, ölçek, açı ve verildiyse dizi sayıları yazılır; ölçek ondalık
olarak yazılır ve altı basamağa yuvarlanmış bir orana çevrilir.

## Hatalar

> `'BACA' adında blok yok. Tanımlı bloklar: KAPAK.`

Önce `BLOK` ile tanımlayın ya da adı düzeltin.

> `Blok ölçeği sıfır olamaz; aynalamak için eksi bir ölçek verin.`

`olcek=0`.

> `Birden çok sütun ya da satır için aralık (milimetre) verin: sutun_aralik= ve satir_aralik=.`

Dizi istendi, aralık verilmedi.

## İlgili

- [BLOK](block.md) — tanım yapmak
- [DİZİ](array.md) — herhangi bir nesneyi çoğaltmak
- [Blok referansı türü](../nesneler/blokreferansi.md)
