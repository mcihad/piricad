# ALANİFRAZ — Alana Göre Parsel Ayırma

## Ne yapar

Bir parselden, **verilen yöne paralel**, **istenen alanda** bir parça ayırır.

"Bu parselden yola paralel 400 m² ayır" bir harita mühendisinden gerçekte istenen
ifraz budur. [`İFRAZ`](split_parcel.md) söylendiği yerden keser ve çıkanı yazar;
bu komuta cevap verilir ve kesimi o bulur.

## Yön niye veriliyor

Ayırma çizgisi, verdiğiniz **yöne paralel** kayar: yol cephesi, mevcut bir sınır,
plandan gelen bir hat. Arama bu yüzden güvenilirdir — sabit yönlü bir çizgi
parselin üzerinde kaydıkça arkasında kalan alan **tek yönlü** büyür, dolayısıyla
ikiye bölme yöntemi tek bir cevaba yakınsar ve ikinci bir cevaba düşemez.

Bir nokta etrafında dönen kesimin böyle bir garantisi yoktur; bazen başka bir
geçerli cevap bulan bir çözücü, üzerine tapu kaydı hesaplanacak bir şey değildir.

## Tolerans raporlanır

İstenen alana **tam** ulaşılmaz, bir toleransla ulaşılır: sınır milimetre
ızgarasına oturur. Komut **elde ettiğini** yazar, istediğinizi değil — istenen
sayıyı yazan bir komut, tapuya gidecek bir sayı hakkında yalan söylerdi.

Varsayılan tolerans **0,01 m²**. Aşılırsa ifraz **yapılmaz**:

> `İstenen alana bu yönde ulaşılamadı. İstenen: 400,00 m², en yakın: 386,20 m² …`

Bu, parsel hakkında bir gerçektir; toleransı aşan bir sınır çizmek tapuya yanlış
bir sayı yazmak olurdu.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `ALANİFRAZ` | `ALANIFRAZ` | `SPLITAREA` | `ALİF` |

## Sözdizimi

```text
ALANİFRAZ yon=<Y>,<X> <Y>,<X> [nesneler=<kimlik>] alan=<mm²> [tolerans=<mm²>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `yon` | Ayırma çizgisinin **yönü**: iki nokta |
| `nesneler` | Ayrılacak parsel; verilmezse etkin seçim |
| `alan` | Ayrılacak alan, **mm²** — 400 m² = `400000000` |
| `tolerans` | Kabul toleransı, mm²; varsayılan `10000` (0,01 m²) |

## Örnekler

### Komut satırı

```text
KATMAN ad=PARSEL
ALAN noktalar=0,0 20,0 20,10 0,10
ALANİFRAZ yon=0,0 0,10 nesneler=1 alan=80000000
```

```text
Alana göre ifraz:
  ayrılan: 80,00 m²
  kalan: 120,00 m²
  istenen 80,00 m², elde edilen 80,00 m²  (fark 0,00 m², tolerans 0,01 m²)
  toplam 200,00 m²  ·  ifrazdan önce 200,00 m²
```

### Arayüz

Parseli seçin, `ALANİFRAZ` yazın, yön çizgisinin iki ucunu tıklayın ve alanı
girin.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.area",  "args": { "noktalar": [[0,0],[20000,0],[20000,10000],[0,10000]] } },
    { "cmd": "core.split_area",
      "args": { "nesneler": [1], "yon": [[0,0],[0,10000]], "alan": 80000000 } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` iki parçayı kaldırır ve asıl parseli geri getirir.

## Betikten kullanım

Yön ve alan argüman olduğu için bir betik bir ifraz listesini sırayla
uygulayabilir. Tolerans aşılırsa o ifraz **yapılmaz** ve işlem bütünüyle geri
alınır — yarım uygulanmış bir ifraz olmaz.

## Hatalar

> `Alana göre ifraz tek parsel üzerinde çalışır.`

Seçim boş ya da birden çok parsel içeriyor.

> `Yön çizgisinin iki ucu aynı yerde; ayırma yönü belirsiz.`

`yon`'un iki noktası çakışık.

> `İstenen alan (…) parselin tamamından (…) küçük olmalı.`

Parselden büyük ya da ona eşit bir alan istendi.

> `İstenen alana bu yönde ulaşılamadı.`

Bu yönde hiçbir paralel kesim istenen alanı toleransla vermiyor. Yönü değiştirin
ya da toleransı büyütün.

## Doğrulama durumu

**Mevzuat imzası bekliyor** (CLAUDE.md 6.11). Bir ifrazın hangi toleransla kabul
edilebileceği ve ada/parsel numaralarının hangi parçada kalacağı mevzuata ait
sorulardır, `/data`'ya aittir ve harita mühendisi onayı gerektirir. Bu komut
geometriyi yapar ve ne yaptığını yazar; ikisine de karar vermez.

## İlgili

- [İFRAZ](split_parcel.md) — söylenen yerden kesme
- [TEVHİT](merge.md) — parsel birleştirme
- [ALANÖLÇ](measure_area.md) — alan ve çevre
