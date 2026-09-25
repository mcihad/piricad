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
         [deger=<sütun>:<değer> …]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Yerleştirilecek bloğun adı |
| `nokta` | Ekleme noktası |
| `olcek` | Ölçek; varsayılan 1. Eksi değer aynalar — ama `olcek_y` verilmezse o da eksi olur ve iki eksi ayna birlikte **yarım dönüştür**: yalnız x'te aynalamak için `olcek=-1 olcek_y=1` yazın |
| `olcek_y` | Y ölçeği farklıysa; varsayılan `olcek` |
| `aci` | Dönme açısı, derece, saat yönünün tersine; varsayılan 0 |
| `sutun`, `satir` | Dizi sütun ve satır sayısı; varsayılan 1 |
| `sutun_aralik`, `satir_aralik` | Dizide kopyalar arası, **milimetre**, döndürülmüş eksende, ölçeklenmez |
| `deger` | Bloğun alanlarının bu referanstaki değerleri, `sütun:değer` biçiminde; birden çok alan için anahtarı yineleyin. Verilmezse elle yerleştirmede her alan sorulur |

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
Bloğun adını yazın, ekleme noktasını tıklayın.

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
