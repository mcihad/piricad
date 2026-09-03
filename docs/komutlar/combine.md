# BİRLEŞTİR — Alan ve Çizgi Birleştirme

## Ne yapar

Seçili nesneleri **tek nesnede** toplar. Seçimin türüne göre iki iş yapar:

- **Alanlar** seçilmişse birleşimlerini alır: yeni sınır, alanların birlikte
  kapladığı yerin dış hattıdır, aradaki dikiş kalkar.
- **Çizgiler** seçilmişse uç uca ekler: uçları birbirine değen çizgiler tek bir
  çizgi olur.

Girdi nesneler silinir, yerine sonuç gelir.

## Bu komut tevhit değildir

`BİRLEŞTİR` **genel bir geometri işlemidir**; kadastro işlemi değildir.

| | `BİRLEŞTİR` | [`TEVHİT`](merge.md) |
|---|---|---|
| Konusu | herhangi bir alan ya da çizgi | parsel |
| Bitişik olmayan girdi | kabul eder, sonucu kaç parça olduğunu yazar | **reddeder** |
| Sonuç | bir ya da birden çok nesne | her zaman **tek** parsel |
| Mevzuat | yok | BÖHHBÜY / TKGM kuralları |

Vadinin iki yakasındaki iki orman lekesi tek bir katman nesnesinde birleşebilir;
bu bir haritacılık işlemidir ve doğrudur. Aynı şeyi iki parselde yapmak TKGM'nin
reddedeceği bir kayıt üretir — bu yüzden tevhit ayrı bir komuttur ve komşuluk
şartını arar.

Parsel birleştirecekseniz [`TEVHİT`](merge.md) kullanın.

### Öznitelikler — uydurmaz

- Bütün girdilerde **aynı** olan sütun değerini korur.
- Girdilerin **ayrıştığı** sütun **boş** gelir, ve komut hangi sütunları
  boşalttığını yazar.

Birinci nesnenin değerini seçmek bir kayıt uydurmak olurdu. Boş kalanları
[`ÖZNİTELİK`](attribute.md) ile doldurun.

### Katman ve stil

Girdilerin hepsi aynı katmandaysa sonuç o katmanda kalır; ayrışıyorlarsa etkin
katmana gider. Stil için de aynı kural geçerlidir: ayrışıyorlarsa sonuç katman
stilini alır.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `BİRLEŞTİR` | `BIRLESTIR` | `COMBINE` | `BRL` |

## Sözdizimi

```text
BİRLEŞTİR [nesneler=<kimlikler>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Birleştirilecek alanlar ya da çizgiler; verilmezse etkin seçim |

## Örnekler

### Komut satırı — iki alan

```text
ALAN noktalar=0,0 10,0 10,10 0,10
ALAN noktalar=10,0 20,0 20,10 10,10
BİRLEŞTİR nesneler=1 2
```

```text
2 alan birleştirildi.
```

Değmeyen iki alan seçilirse komut yine çalışır ve durumu söyler:

```text
2 alan birleştirildi, sonuç 2 ayrı parça: seçilen alanlar birbirine değmiyor.
```

### Komut satırı — iki çizgi

```text
ÇİZGİ baş=0,0 son=10,0
ÇİZGİ baş=10,0 son=10,10
BİRLEŞTİR nesneler=1 2
```

```text
2 çizgi tek çizgide birleştirildi: 3 köşe.
```

### Arayüz

Nesneleri seçin, sol araç kutusundaki **Birleştir** düğmesine basın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.area", "args": { "noktalar": [[0,0],[10000,0],[10000,10000],[0,10000]] } },
    { "cmd": "core.area", "args": { "noktalar": [[10000,0],[20000,0],[20000,10000],[10000,10000]] } },
    { "cmd": "core.combine", "args": { "nesneler": [1, 2] } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` sonucu kaldırır ve girdileri geri getirir.

## Betikten kullanım

`nesneler` verilirse seçime dokunmaz. Herhangi bir nesne bulunamazsa **hiçbiri**
birleştirilmez — işlem bütünüyle geri alınır.

## Hatalar

> `BİRLEŞTİR en az iki nesne ister.`

Seçim tek nesne ya da boş.

> `Seçimde hem alan hem çizgi var (<n> alan, <m> çizgi).`

Bir çizgiyi bir alanla birleştirmenin tahmine dayanmayan bir karşılığı yok:
çizgi alanın yeni bir kenarı mı, içinde bir boşluk mu, yoksa onu kesip geçiyor
mu? Çizgileri önce [`ALANAÇEVİR`](to_area.md) ile alana çevirin.

> `Çizgiler tek bir zincir oluşturmuyor: <n> / <m> çizgi birleşti.`

Kalan çizgilerin ucu zincire değmiyor. Uçları yakalama açıkken yeniden çizin ya
da düğüm toleransını büyütün: `AYAR düğüm_toleransı=50`.

> `Nesne <kimlik> bir eğri ya da nokta.`

Daire, yay ve nokta birleştirilmez; önce [`DÖNÜŞTÜR`](reproject.md) ile çizgiye
çevirin.

## İlgili

- [BÖL](split.md) — çizgiyi bir noktadan ikiye böler
- [TEVHİT](merge.md) — parsel birleştirme (kadastro)
- [ALANAÇEVİR](to_area.md) — kapalı çizgi zincirini alana çevirir
- [ÖZNİTELİK](attribute.md) — sütun değeri yazma
