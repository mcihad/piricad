# SPLINE — Spline Çizme

## Ne yapar

Kontrol noktalarından pürüzsüz bir eğri (NURBS) çizer. Noktaları sırayla verirsiniz;
eğri ilk noktada başlar, son noktada biter ve aradakilere **yaklaşır** — üzerinden
geçmez. Derece varsayılan 3'tür (kübik); düğümler düzgün ve uçları bağlıdır.

Saklanan şey **tanımdır**: kontrol noktaları, derece ve düğümler
([spline türü](../nesneler/spline.md)). Çizilen eğri her seferinde bu tanımdan de Boor
algoritmasıyla kurulur ve her bilgisayarda aynı milimetrelere düşer.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `SPLINE` | `SPLINE` | `SPLINE` | `SPL` |

## Sözdizimi

```text
SPLINE noktalar=<sağa>,<yukarı> <sağa>,<yukarı> ... [derece=<1-15>] [kapali=evet]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Kontrol noktaları, en az iki. Aynı `noktalar=` altında art arda yazılır |
| `derece` | Eğrinin derecesi, 1–15; varsayılan 3. Nokta sayısı dereceye yetmiyorsa derece düşürülür ve söylenir |
| `kapali` | `evet` ise son noktadan ilkine düz kapanır; varsayılan hayır |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```text
KATMAN ad=DERE
SPLINE noktalar=0,0 10,20 20,20 30,0
```

Dört kontrol noktalı kübik: (0,0)'da başlar, (30,0)'da biter, ortası tam (15,15)'tedir.

```text
SPLINE noktalar=40,0 50,10 60,0 50,-10 derece=2 kapali=evet
```

İkinci dereceden, kapalı bir eğri.

### Arayüz

**Çizim ▸ Spline**. Kontrol noktalarını sırayla tıklayın, bitirmek için Enter.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "DERE" } },
    { "cmd": "core.spline",
      "args": { "noktalar": [[0,0],[10000,20000],[20000,20000],[30000,0]], "derece": 3 } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` eğriyi kaldırır.

## Betikten kullanım

Kontrol noktaları milimetre çiftleri dizisidir. Günlüğe verilen noktalar, kullanılan
derece ve açıkça istendiyse `kapali` yazılır; bir tekrar aynı düğümleri kurar.

## Hatalar

> `Bir spline en az iki kontrol noktası ister; 1 nokta verildi.`

Tek nokta eğri yapmaz.

> `Spline derecesi 1 ile 15 arasında olmalı; verilen 20.`

`derece` aralık dışında.

> `Nokta sayısı 3 olduğu için derece 2'e düşürüldü.`

Bir uyarı, hata değil: kübik için en az dört nokta gerekir; eğri yine çizilir.

## İlgili

- [ÇOKLUÇİZGİ](polyline.md) — noktalardan düz kenarlı çizgi
- [Spline türü](../nesneler/spline.md) — saklanış, çizim, yakalama
