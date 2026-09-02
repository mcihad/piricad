# HACİM — Kazı ve Dolgu Hesabı

## Ne yapar

Kotlu noktalardan kurulan yüzeyi verilen bir kotla karşılaştırır ve **kazı** ile
**dolgu** hacimlerini hesaplar. Bir saha düzenlemesinin, bir yol platformunun ya
da bir havuzun keşif hesabı budur.

## Kazı ve dolgu ayrı yazılır

**Netlenmezler.** 500 m³ kazı ve 500 m³ dolgusu olan bir saha bir haftalık makine
işidir; net hacmi sıfır olduğu için hiçbir şeyin kıpırdamadığı bir saha hiç iş
değildir. Makineler **ayrı** rakamlara göre tutulur, farkına göre değil.

Fark yine de yazılır — ama en sonda ve adı konarak. Farkı önce okumak, dengeli bir
sahayı işsiz bir saha sanmanın yoludur.

## Nasıl hesaplanır

Noktalar üçgenlenir ve her üçgen ile karşılaştırma düzlemi arasındaki prizmanın
hacmi alınır: **alan × ortalama yükseklik**, ki düzlem üstünde düzlem için tamdır.

Düzlemin **kestiği** bir üçgen ikiye ayrılır: yalnız kalan köşenin küçük üçgeni ve
geri kalan dörtgen. Bir üçgeni bütün hâlde tek tarafa saymak, dolguyu kazı
sütununa yazmak olurdu — ve makineler o sütuna göre tutulur.

Ara çarpımlar 128 bitte taşınır: bir sahanın alanı ile derinliğinin çarpımı int64'ü
kolayca aşar.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `HACİM` | `HACIM` | `EARTHWORK` | `HCM` |

## Sözdizimi

```text
HACİM kot=<mm>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `kot` | Karşılaştırma kotu, **milimetre** — 845 m = `845000` |

Seçim varsa yalnız seçili noktalar, yoksa çizimdeki bütün kotlu noktalar
kullanılır. Kotu olmayan nokta kullanılmaz, sıfır sayılmaz.

## Örnekler

### Komut satırı

```text
KATMAN ad=NIRENGI
NOKTALAR dosya="nivelman.txt"
HACİM kot=845000
```

```text
Hacim — karşılaştırma kotu 845,000 m
  kazı  (kotun üstünde): 12450,30 m³
  dolgu (kotun altında): 3180,75 m³
  fark (kazı - dolgu):   9269,55 m³
  hesap alanı: 10000,00 m², 169 kotlu noktadan.
```

### Arayüz

Kotlu bir nokta listesi okuyun, sonra `HACİM kot=845000` yazın. Sonuç sağ
paneldeki **Geçmiş** sekmesinde durur.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer",     "args": { "ad": "NIRENGI" } },
    { "cmd": "core.points",    "args": { "dosya": "nivelman.txt" } },
    { "cmd": "core.earthwork", "args": { "kot": 845000 } }
  ]
}
```

## Geri alma

Geri alınacak bir şey yoktur: `HACİM` çizimi değiştirmez ve geri alma yığınına
adım eklemez.

## Betikten kullanım

Salt okunur olduğu için bir betik aynı araziyi birkaç kota göre hesaplayıp
dengeleyen kotu arayabilir.

## Hatalar

> `Çizimde 'kot' sütunu yok. Kotlu bir nokta listesini NOKTALAR ile okuyun.`

Hiç kot bilgisi yok.

> `Kotlu nokta sayısı yetersiz: N. Hacim hesabı en az üç kotlu nokta ister.`

Üçten az noktanın kotu var.

> `Hacim hesabı en az üç FARKLI kotlu nokta ister.`

Noktalar var ama çoğu aynı yerde.

> `Bu noktalardan yüzey kurulamadı: hepsi aynı doğru üzerinde olabilir.`

Noktalar doğrusal; üçgenlenecek alan yok.

## İlgili

- [EŞYÜKSELTİ](contour.md) — aynı yüzeyden eş yükselti eğrileri
- [NOKTALAR](points.md) — kotlu nokta listesi okuma
