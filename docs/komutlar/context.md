# BAĞLAM — Üzerinde Çalışılanın Özeti

Bir betik ya da bir yapay zeka ajanı yazan herkes için; bu sayfayı bitirdiğinizde
programın neyin üzerinde çalıştığını tek çağrıda öğrenmeyi bileceksiniz.

## Ne yapar

**Bir ajanın ilk sorduğu soruyu cevaplar:** ben neyin üzerinde çalışıyorum?

Belge sürümü, koordinat sistemi, çizimin kapsamı, nesne sayısı, katman adları,
çıktı yerleşimleri ve her birinin **hedefli olup olmadığı**, seçili nesnelerin
kalıcı anahtarları ve görünüm penceresi — hepsi tek JSON sonucunda.

**Özet verir, döküm değil.** Geometri yok, öznitelik satırı yok, nesne listesi
yok: çizimle birlikte büyüyen bir bağlam, beş milyon parsellik bir paftayı bir
isteme sokardı. Sayılar ve adlar; gerisini dar araçlar cevaplar
([`KATMANLAR`](layers.md), [`SORGULA`](query.md), [`SEÇİMBİLGİSİ`](selection_info.md),
[`GÖRÜNÜMBİLGİSİ`](view_info.md), [`ÖZNİTELİKŞEMASI`](attr_schema.md)).

Bunlar olmadan bir ajan ilk turunu, çizimde bir yerleşim olduğunu ve hiçbir şeyin
seçili olmadığını keşfetmekle harcar.

## Adlar

| Ad | Açıklama |
|---|---|
| `BAĞLAM` | Türkçe birincil ad |
| `BAGLAM` | ASCII karşılığı |
| `CONTEXT` | İngilizce karşılığı |
| `BĞL` | Kısaltma |

## Sözdizimi

```
BAĞLAM
```

## Parametreler

Yoktur. `BAĞLAM` her zaman aynı soruyu sorar: üzerinde çalışılan ne?

## Sonuç

| Alan | Anlamı |
|---|---|
| `surum` | Belgenin sürümü. Bir sonraki çağrıyı buna göre kurun |
| `crs` | Koordinat sisteminin kimliği |
| `kapsam` | Çizimin sınır kutusu `[min_x, min_y, max_x, max_y]`, milimetre. Çizim boşsa yok |
| `nesne_sayisi` | Canlı nesne sayısı |
| `katmanlar` | Katman adları |
| `cikti_yerlesimleri` | Her biri için `ad`, `sayfa`, `oge`, `kagit`, **`hedefli`** ve varsa `sorun` sayısı |
| `secili` | Seçili nesnelerin **kalıcı anahtarları** (yuva numarası değil) |
| `gorunum` | `pencere` ve varsa `olcek`; **ekran yoksa `null`** |

**`hedefli`** bir ajanın gerçekten sorduğu sorudur: var olup hiçbir yere bakmayan
bir yerleşim boş bir kutu basar, ve bunu PDF yazıldıktan sonra öğrenmek geç
öğrenmektir ([`ÇIKTIYERLEŞİMİ islem=denetle`](layout.md)).

**`gorunum` `null` olabilir ve bu dürüst bir cevaptır.** Başsız bir çalıştırmanın
penceresi yoktur; uydurulmuş bir dikdörtgen, ajanın bir sonraki çizimini kimsenin
bakmadığı bir yere koyardı.

## Örnekler

### Komut satırı

```
BAĞLAM
```

```
Bağlam: 1 nesne, 2 katman, 1 çıktı yerleşimi, 0 seçili.
```

### Arayüz

Komut satırına `BAĞLAM` yazmak yeter; yapısal sonuç komut günlüğünde durur.

### Betik

```json
{ "cmd": "core.context", "args": {} }
```

## Geri alma

Hiçbir şeyi değiştirmez; geri alma yığınına girmez.

## Betikten kullanım

Bir betiğin ya da ajanın ilk adımı olarak, sonraki adımları neye göre kuracağını
öğrenmek için:

```json
[
  { "cmd": "core.context", "args": {} },
  { "cmd": "core.layout", "args": { "islem": "ekle", "ad": "Ada 1284",
                                    "kagit": "A3", "yon": "yatay" } }
]
```

## Hatalar

Bu komutun kendi hatası yoktur: her zaman cevap verir. Bulunmayan bir şey, hata
değil **yokluk** olarak bildirilir — ekran yoksa `gorunum` `null` olur, çizim
boşsa `kapsam` hiç yazılmaz.

## İlgili

- [`KATMANLAR`](layers.md) — katmanların ayrıntısı
- [`SEÇİMBİLGİSİ`](selection_info.md) — seçimin ayrıntısı
- [`GÖRÜNÜMBİLGİSİ`](view_info.md) — görünümün ayrıntısı
- [`ÇIKTIYERLEŞİMİ`](layout.md) — çıktı yerleşimleri
