# ÖLÇÜSTİLİ — Ölçü Stilleri

Paftasına hangi ölçü stilini seçeceğine karar vermek, bir stilin kâğıtta ve bu
paftada ne büyüklükte çıkacağını görmek ya da yeni ölçülerin varsayılan stilini
değiştirmek isteyen herkes için; bu sayfayı bitirdiğinizde stilleri listelemeyi ve
varsayılanı kurmayı bileceksiniz.

## Ne yapar

`ÖLÇÜSTİLİ`, ölçü stili kataloğundaki (`data/catalogs/dxf/olcu-stili.json`,
[`TERCİH ölçü_stilleri`](preference.md)) her stili **kâğıttaki** boylarıyla ve **bu
paftadaki** karşılığıyla listeler: yazı yüksekliği, ok biçimi ve boyu, ondalık
sayısı, ondalık ayracı, [baz ölçü](dimension_baseline.md) aralığı ve plan ölçeğinde
yazının zeminde kaç metre olduğu. Hangisinin **varsayılan** olduğunu söyler. Hiçbir
şeyi değiştirmez.

Katalogdaki stiller:

| Stil | Yazı | Ok | Ondalık | Ne için |
|---|---|---|---|---|
| `ISO-25` | 2,5 mm | kapalı, 2,5 mm | 2, virgül | ISO metrik varsayılan |
| `ISO-18` | 1,8 mm | kapalı, 1,8 mm | 2, virgül | Sık paftalar |
| `ISO-35` | 3,5 mm | kapalı, 3,5 mm | 2, virgül | Seyrek ya da uzaktan okunacak paftalar |
| `STANDARD` | 1,8 mm | kapalı, 1,8 mm | 4, nokta | AutoCAD STANDARD |
| `MIMARI` | 2,5 mm | 45° çentik, 2 mm | 2, virgül | Mimari çizim |

Yazı yükseklikleri ISO 3098-1 dizisindendir; değerler yönetmelik değeri değildir.
Stiller koddan değil katalogdan gelir: yeni bir stil bir veri güncellemesidir.

**Varsayılan stil** projenin ayarıdır: [`AYAR ölçü_stili`](setting.md) değiştirir,
dosyayla birlikte gider. [`ÖLÇÜ`](dimension.md), [`ZİNCİRÖLÇÜ`](dimension_continue.md),
[`BAZÖLÇÜ`](dimension_baseline.md) ve [`LİDER`](leader.md) `stil=` verilmediğinde
onu kullanır. Çizilmiş bir ölçünün stilini [`ÖLÇÜDÜZENLE stil=`](dimension_edit.md)
değiştirir.

## Adlar

| Ad | Tür |
|---|---|
| `ÖLÇÜSTİLİ` | Türkçe, birincil |
| `OLCUSTILI` | ASCII karşılık |
| `DIMSTYLE` | İngilizce karşılık |
| `ÖST`, `OST` | Kısaltma |
| `core.dimension_style` | Komut kimliği |

## Sözdizimi

```text
ÖLÇÜSTİLİ [ad=<stil>] [katalog=<dosya>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Yalnız bu stili gösterir; verilmezse hepsini |
| `katalog` | Stil kataloğu dosyası; varsayılan `TERCİH ölçü_stilleri` |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Stilleri görmek, varsayılanı değiştirmek ve yeni stille çizmek:

<!-- örnek: yeni çizim -->
```
ÖLÇÜSTİLİ
AYAR ölçü_stili ISO-35
ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-4
ÖLÇÜSTİLİ ad=ISO-35
```

### Arayüz

**Açıklama ▸ Ölçü ▸ Ölçü Stilleri** listeyi Geçmiş paneline yazar. Varsayılanı aynı
sekmedeki **Stil** kutusu ya da **KentOS CAD ▸ Proje Ayarları…** penceresinin **Plot ve
Çıktı** bölümündeki **ölçü_stili** değiştirir; ikisi de aynı `AYAR` satırını çalıştırır.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.dimension_style", "args": { "ad": "ISO-25" } }
  ]
}
```

Betik ve yapay zekâ istemcileri cümleyi değil yapılandırılmış cevabı okur:
`katalog` (sürüm), `varsayilan`, `plan_olcegi` ve her stil için `ad`, `varsayilan`,
`ok`, `ok_boyu_um`, `uzatma_fazlasi_um`, `uzatma_boslugu_um`, `yazi_boslugu_um`,
`yazi_yuksekligi_um`, `baz_araligi_um`, `ondalik`, `ondalik_ayraci`, `aciklama` —
boylar **kâğıt mikrometresi**.

## Geri alma

Komut hiçbir şeyi değiştirmez; geri alınacak bir şeyi yoktur.

## Betikten kullanım

Betikten çağrıldığında hiçbir şey sormaz. Günlüğe belge değişikliği olarak düşmez.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Tanınmayan ölçü stili: '…'. Katalogdaki stiller: …` | `ad` katalogda yok | Listelenen stillerden birini yazın |
| `Ölçü stili kataloğu bulunamadı: '…'. TERCİH ölçü_stilleri ile yolunu kurun ya da katalog= verin.` | Katalog dosyası bu makinede yok | Yolunu kurun |

## İlgili

- [`ÖLÇÜ`](dimension.md) — ölçü çizmek
- [`ÖLÇÜDÜZENLE`](dimension_edit.md) — çizilmiş ölçünün stilini değiştirmek
- [`ÖLÇÜYENİLE`](dimension_refresh.md) — ölçüleri başka bir pafta ölçeğine uyarlamak
