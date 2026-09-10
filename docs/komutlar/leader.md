# LİDER — Lider Çizme

## Ne yapar

Bir noktayı gösteren oklu çizgi çizer ve istenirse son köşesinin yanına bir yazı koyar.
Ok boyu ve yazı yüksekliği [ölçü stilinden](dimension.md) gelir. Sonuç bir
[lider nesnesidir](../nesneler/lider.md); yazı ayrı bir metin nesnesidir ve `METİN` gibi
düzenlenir.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `LİDER` | `LIDER` | `LEADER` | `LD` |

## Sözdizimi

```text
LİDER noktalar=<sağa>,<yukarı> <sağa>,<yukarı> ... [metin=<yazı>] [stil=<ad>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Okun ucundan yazının yanına köşeler, en az iki |
| `metin` | Son köşenin yanına yazılacak metin |
| `stil` | Ok ve yazı boyunu veren ölçü stili; varsayılan `ISO-25` |
| `katalog` | Stil kataloğu dosyası; varsayılan `TERCİH ölçü_stilleri` |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```text
KATMAN ad=NOTLAR
LİDER noktalar=0,0 3,3 6,3 metin="Rögar kapağı"
LİDER noktalar=10,0 13,3
```

### Arayüz

**Çizim ▸ Lider**. Okun ucunu, sonra köşeleri tıklayın; Enter ile bitirin, yazıyı
yazın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "NOTLAR" } },
    { "cmd": "core.leader",
      "args": { "noktalar": [[0,0],[3000,3000],[6000,3000]], "metin": "Rögar kapağı" } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` lideri ve yazısını birlikte kaldırır.

## Betikten kullanım

Günlüğe köşeler, stil ve varsa metin yazılır.

## Hatalar

> `Bir lider en az iki nokta ister: okun ucu ve yazının yanı.`

Tek nokta verildi.

> `Tanınmayan ölçü stili: 'DIN'. Katalogdaki stiller: ISO-25, STANDARD, MIMARI.`

Stil katalogda yok.

## İlgili

- [ÖLÇÜ](dimension.md) — ölçülendirme
- [METİN](text.md) — yazıyı sonradan değiştirmek
- [Lider türü](../nesneler/lider.md)
