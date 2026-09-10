# TARAMA — Tarama Çizme

## Ne yapar

Kapalı bir sınırın içini bir **desenle** doldurur: dolu (`SOLID`), 45° çizgiler
(`ANSI31`), ağ (`NET`), toprak (`EARTH`)… Sınırı ya seçili kapalı nesnelerden alır —
alan, daire, elips, kapalı çoklu çizgi — ya da `noktalar=` ile doğrudan köşelerden.
Sonuç bir [tarama nesnesidir](../nesneler/tarama.md): sınır halkaları ve desen
birlikte saklanır, DXF'e `HATCH` olarak gider.

### Desen kataloğu

Desenler koddan değil, `data/catalogs/dxf/tarama-desenleri.json` dosyasından gelir
(`TERCİH desen_kataloğu`). Her desen çizgi ailelerinden oluşur: açı, taban, bir
çizgiden ötekine kayma ve kesik dizisi, **desen mikrometresi** olarak. Kendi
deseninizi eklemek için dosyayı kopyalayın, satır ekleyin, yolunu tercihe ya da
`katalog=` argümanına verin. Bugün gelenler: `SOLID`, `ANSI31`, `ANSI32`, `ANSI33`,
`ANSI34`, `ANSI37`, `LINE`, `NET`, `DOTS`, `EARTH`, `GRASS`.

### Ölçek

Desen ölçüleri çizim değil **kâğıt** düşünülerek verilmiştir: `ANSI31` 3,175 mm
aralıklıdır. `olcek` bu sayıyı çarpar. Vermezseniz pafta ölçeğinin paydası
([`AYAR plan_ölçeği`](setting.md)) kullanılır: 1/1000 paftada aralık zeminde 3,175 m
olur ve kâğıtta 3,175 mm çıkar.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `TARAMA` | `TARAMA` | `HATCH` | `TRM` |

## Sözdizimi

```text
TARAMA noktalar=<sağa>,<yukarı> <sağa>,<yukarı> <sağa>,<yukarı> ... [desen=<ad>] [aci=<derece>] [olcek=<çarpan>]
TARAMA nesneler=<kimlik> ... [desen=<ad>] [aci=<derece>] [olcek=<çarpan>]
TARAMA desen=<ad>            ← etkin seçimi tarar
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Sınır köşeleri, en az üç; nesne seçmek yerine |
| `nesneler` | Sınırı verecek kapalı nesnelerin kimlikleri; verilmezse etkin seçim, o da yoksa sorulur |
| `desen` | Katalogdaki desen adı; varsayılan `SOLID` |
| `aci` | Desenin dönme açısı, derece; varsayılan 0 |
| `olcek` | Desen ölçeği; varsayılan pafta ölçeğinin paydası |
| `katalog` | Desen kataloğu dosyası; varsayılan `TERCİH desen_kataloğu` |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```text
KATMAN ad=LEKELER
TARAMA noktalar=0,0 20,0 20,20 0,20 desen=ANSI31 olcek=1000
```

20 m'lik karenin içi 45° çizgilerle, 3,175 m aralıkla taranır.

```text
TARAMA noktalar=30,0 50,0 50,20 30,20 desen=NET aci=30 olcek=2000
TARAMA noktalar=60,0 80,0 80,20 60,20
```

İkincisi dolu taramadır. Seçili nesneleri taramak için önce seçin:

```text
KATMAN ad=PARSEL
ALAN 100,0 120,0 120,20 100,20
SEÇ KATMAN katman=PARSEL
TARAMA desen=EARTH olcek=1000
```

### Arayüz

**Çizim ▸ Tarama**. Kapalı nesneleri seçip Enter'a basın ya da köşeleri tıklayın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "LEKELER" } },
    { "cmd": "core.hatch",
      "args": { "noktalar": [[0,0],[20000,0],[20000,20000],[0,20000]],
                "desen": "ANSI31", "aci": 0, "olcek": 1000 } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` taramayı kaldırır; sınır olarak seçilen nesnelere dokunulmaz.

## Betikten kullanım

`noktalar` milimetre çiftleridir. Günlüğe sınırın nasıl verildiği (`noktalar` ya da
`nesneler`), desen adı, açı ve **kullanılan** ölçek yazılır — varsayılandan geldiyse
de yazılır, böylece pafta ölçeği değişse tekrar aynı taramayı kurar.

## Hatalar

> `Tarama sınırı en az üç nokta ister; verilen 2.`

`noktalar` ile iki nokta verildi.

> `Nesne 7 kapalı değil; tarama sınırı kapalı bir alan, daire, elips ya da kapalı çoklu çizgi olmalı.`

Seçilen nesne açık bir çizgi.

> `Tanınmayan tarama deseni: 'CIMEN'. Katalogdaki desenler: SOLID, ANSI31, …`

Desen adı katalogda yok; adı düzeltin ya da kataloğa ekleyin.

> `Tarama deseni kataloğu bulunamadı: 'data/catalogs/dxf/tarama-desenleri.json'. TERCİH desen_kataloğu ile yolunu kurun ya da katalog= verin.`

Katalog dosyası bu makinede yok.

> `Tarama ölçeği sıfırdan büyük olmalı.`

`olcek=0` ya da eksi.

## İlgili

- [ALAN](area.md) — taranacak kapalı sınırı çizmek
- [STİL](style.md) — bir alanı katalogdaki gösterimle doldurmak; yönetmelik lekesi için o kullanılır
- [Tarama türü](../nesneler/tarama.md) — saklanış ve DXF eşlemesi
- [TERCİH](preference.md) — desen kataloğu yolu
