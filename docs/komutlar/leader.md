# LİDER — Kılavuz Çizgi

## Ne yapar

Bir noktayı gösteren oklu çizgiyi — **kılavuz çizgiyi** — çizer ve istenirse son köşesinin
yanına bir yazı koyar. Ok boyu ve yazı yüksekliği [ölçü stilinden](dimension.md) gelir.
Sonuç bir [kılavuz çizgi nesnesidir](../nesneler/lider.md); yazı ayrı bir metin nesnesidir ve `METİN` gibi
düzenlenir.

**Yazı kılavuzun ucuna bağlıdır.** Son parçanın gösterdiği tarafta, ucun bir yazı aralığı
ötesinde durur ve o taraftan uzağa akar: soldan gelen kılavuzda sağda ve sola yaslı,
sağdan gelende solda ve sağa yaslı — yazı ne kadar uzun olursa olsun çizginin üstüne
binmez. Kılavuzun ucu tutamaktan ya da `TAŞI`, `DÖNDÜR`, `ÖLÇEKLE` ile taşınınca yazı onu
izler; uç öbür tarafa dönerse yazı da döner ve yaslanışını değiştirir. Yazıyı elle
taşırsanız o kayma korunur. Kılavuz silinince yazısı da silinir.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `LİDER` | `LIDER` | `LEADER` | `LD` |

Şeritte, komut listesinde ve iletilerde adı **Kılavuz Çizgi**'dir — AutoCAD'in Türkçe
belgelerinin `LEADER` için kullandığı ad. Komut satırında yazılan sözcük `LİDER` (ya da
`LEADER`) olarak kalır; eski betikler ve `SEÇ tur=LİDER` süzgeci aynen çalışır.

## Sözdizimi

```text
LİDER noktalar=<sağa>,<yukarı> <sağa>,<yukarı> ... [metin=<yazı>] [stil=<ad>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Okun ucundan yazının yanına köşeler, en az iki |
| `metin` | Son köşenin yanına yazılacak metin; verilmezse arayüzde sorulur, betikte yazısız kalır |
| `stil` | Ok ve yazı boyunu veren ölçü stili; verilmezse projenin [`ölçü_stili`](dimension_style.md) ayarı (başlangıçta `ISO-25`) |
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

**Giriş ▸ Açıklama ▸ Kılavuz Çizgi** (ya da **Açıklama ▸ Etiket ▸ Kılavuz Çizgi**). Okun
ucunu, sonra köşeleri tıklayın; Enter ile bitirin. Yazı, kılavuzun ucunun yanında —
yazının duracağı yerde — açılan kutuda sorulur: yazıp **Enter**'a basın; boş **Enter**
kılavuzu yazısız bırakır.

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

Tek adımdır: `GERİAL` kılavuz çizgiyi ve yazısını birlikte kaldırır.

## Betikten kullanım

Günlüğe köşeler, stil ve varsa metin yazılır. Arayüzde sorulan yazı günlüğe `metin`
olarak yazılır, yani arayüz, komut satırı ve betik aynı satırı bırakır. Betik `metin`
vermezse yazı sorulmaz; kılavuz yazısız çizilir. Yazının bağı günlüğe yazılmaz, çünkü
oynatılan `LİDER` onu aynı biçimde yeniden kurar.

## Hatalar

> `Bir kılavuz çizgi en az iki nokta ister: okun ucu ve yazının yanı.`

Tek nokta verildi.

> `Tanınmayan ölçü stili: 'DIN'. Katalogdaki stiller: ISO-25, STANDARD, MIMARI.`

Stil katalogda yok.

## İlgili

- [ÖLÇÜ](dimension.md) — ölçülendirme
- [METİN](text.md) — yazıyı sonradan değiştirmek
- [Kılavuz çizgi türü](../nesneler/lider.md)
