# ÖLÇÜYENİLE — Ölçüleri Pafta Ölçeğine Uyarlama

1/1000 için çizdiği ölçüleri 1/500 bir aplikasyon paftasına ya da 1/5000 bir
halihazır paftasına koyan, çizim birimini değiştirdikten sonra ölçü yazılarını yeni
birimle görmek isteyen herkes için; bu sayfayı bitirdiğinizde ölçülerin her paftada
kâğıtta aynı boyda okunmasını sağlamayı bileceksiniz.

## Ne yapar

Bir ölçü stili ölçüleri **kâğıt** üzerinde tanımlar: `ISO-25` 2,5 mm ok, 2,5 mm yazı
ister. [`ÖLÇÜ`](dimension.md) bunları çizerken [plan ölçeğiyle](setting.md) zemine
indirir — 1/1000 paftada 2,5 mm yazı zeminde 2,5 m'dir — ve ölçü **hangi pafta
ölçeği için boyutlandığını** saklar. Aynı ölçü 1/5000 basılırsa yazısı kâğıtta
0,5 mm olur ve kimse okuyamaz; 1/500 basılırsa 5 mm olup paftayı kaplar.

`ÖLÇÜYENİLE`, ölçüleri verilen pafta ölçeğine uyarlar: oklar, uzatma çizgileri, yazı
boşluğu ve yazı yüksekliği o ölçekte **kâğıtta stilin boyunda** kalacak şekilde
yeniden boyutlanır, ölçü yeniden kurulur ve yazısı çizimin bugünkü birimiyle yeniden
yazılır. **Ölçülen değer değişmez**; değişen, onun paftadaki boyudur.

Ne zaman gerektiği söylenir:

- [`AYAR plan_ölçeği`](setting.md) değişince, başka ölçek için boyutlu ölçü varsa:
  `Bu çizimdeki N ölçü 1/1000 paftası için boyutlandırılmış; 1/500 paftasında kâğıtta
  aynı boyda kalmaları için: ÖLÇÜYENİLE`;
- [`AYAR çizim_birimi`](setting.md) değişince, yazısı çizimin birimini izleyen ölçü varsa;
- [`YAZDIR`](print.md) ölçülerin boyutlandığı ölçekten başka bir ölçekte basarken:
  `N ölçü 1/1000 paftası için boyutlandırılmış; 1/5000 çıktıda yazıları 0,5 mm olur.`
  Pafta yine basılır; uyarı sonuçla birlikte döner.
- Nitelik panelinin **ÖLÇÜ** grubunda, ölçünün pafta ölçeği plan ölçeğinden farklıysa
  **UYARLA** rozeti.

**Ölçeği bilinmeyen ölçü.** Bu özellikten önce kaydedilmiş bir dosyadan ya da bir
DXF'ten gelen ölçü hangi ölçek için çizildiğini bilmez. Stili katalogdaysa, stilin
kâğıttaki ok boyu ile ölçünün zemindeki ok boyu ölçeği verir; değilse ölçü
**atlanır ve söylenir** — `eski_olcek=<N>` ile hangi ölçek için çizildiğini siz
söylersiniz.

## Adlar

| Ad | Tür |
|---|---|
| `ÖLÇÜYENİLE` | Türkçe, birincil |
| `OLCUYENILE` | ASCII karşılık |
| `DIMREFRESH` | İngilizce karşılık |
| `ÖYN`, `OYN` | Kısaltma |
| `core.dimension_refresh` | Komut kimliği |

## Sözdizimi

```text
ÖLÇÜYENİLE [nesneler=<kimlik> …] [olcek=<N>] [eski_olcek=<N>]
```

`nesneler` verilmezse çizimin **bütün** ölçüleri; `olcek` verilmezse plan ölçeği.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Uyarlanacak ölçüler. Verilmezse çizimin bütün ölçüleri |
| `olcek` | Paftanın ölçeği, 1/N'nin **N**'si. Verilmezse [`AYAR plan_ölçeği`](setting.md) |
| `eski_olcek` | Ölçeği bilinmeyen ölçülerin hangi ölçek için çizildiği |
| `katalog` | Stil kataloğu dosyası; varsayılan `TERCİH ölçü_stilleri` |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

1/1000 için çizilmiş bir ölçüyü 1/500 paftaya uyarlamak; ikinci çağrı bir şey yapmaz:

<!-- örnek: yeni çizim -->
```
ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3
ÖLÇÜYENİLE olcek=500
ÖLÇÜYENİLE olcek=500
```

```text
1 ölçü 1/1000'den 1/500 paftasına uyarlandı: oklar, uzatma çizgileri ve yazılar kâğıtta aynı boyda kalır (yazı 2,5 mm).
1 ölçü zaten 1/500 için.
```

Plan ölçeğini değiştirip uyarlamak:

<!-- örnek: yeni çizim -->
```
ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3
AYAR core.plan.olcek 5000
ÖLÇÜYENİLE
```

### Arayüz

**Değiştir ▸ Ölçüleri Pafta Ölçeğine Uyarla**: çizimin bütün ölçüleri plan ölçeğine
uyarlanır. Yalnız bazılarını uyarlamak ya da başka bir ölçek vermek için komut
satırını kullanın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.dimension",
      "args": { "birinci": [0, 0], "ikinci": [20000, 0], "konum": [10000, -3000] } },
    { "cmd": "core.dimension_refresh", "args": { "olcek": 500 } }
  ]
}
```

### Üçü de aynı

Menü girişi, komut satırı ve betik aynı belgeyi bırakır (`tests/unit/test_dimtext.cpp`,
`ÖLÇÜDÜZENLE KANIT`).

## Geri alma

Tek adımdır: [`GERİAL`](undo.md) bütün ölçüleri önceki boylarına döndürür.

## Betikten kullanım

Betikten çağrıldığında komut hiçbir şey sormaz. Günlüğe çözülmüş `olcek` (verilmediyse
o anki plan ölçeği) yazılır; böylece oynatma, plan ölçeği sonradan değişse de aynı
boyları kurar.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Yenilenecek ölçü yok.` | Çizimde ya da verilen kimliklerde ölçü yok | Ölçüleri denetleyin |
| `Pafta ölçeği bilinmiyor: olcek=<N> verin …` | Plan ölçeği kurulmamış ve `olcek` verilmedi | `olcek=` verin ya da `AYAR plan_ölçeği` kurun |
| `N ölçünün hangi pafta ölçeği için çizildiği bilinmiyor; eski_olcek=<N> ile söyleyin.` (not) | Dosyadan gelen ölçünün ölçeği kayıtlı değil, stili de katalogda yok | `eski_olcek=` verin |
| `N ölçü kilitli katmanda, atlandı.` (not) | Ölçünün katmanı kilitli | [`KATMAN`](layer.md) ile kilidi açıp yeniden çalıştırın |

## İlgili

- [`ÖLÇÜ`](dimension.md) — ölçü çizmek
- [`ÖLÇÜDÜZENLE`](dimension_edit.md) — ölçünün yazısını, birimini, toleransını değiştirmek
- [`YAZDIR`](print.md) — pafta çıktısı
- [Ölçü nesnesi](../nesneler/olcu.md)
