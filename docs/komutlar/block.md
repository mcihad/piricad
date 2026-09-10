# BLOK — Blok Tanımlama

## Ne yapar

Seçilen nesnelerden adlı bir **blok tanımı** yapar ve yerlerine bir
[blok referansı](../nesneler/blokreferansi.md) koyar. Nesneler türü, yükü, stili,
yazısı ve öznitelikleriyle tanıma kopyalanır, kendileri silinir; taban noktasına konan
referans onları tam durdukları yerde çizer. Sonra aynı bloğu
[`BLOKEKLE`](insert.md) ile istediğiniz kadar yerleştirirsiniz.

Blok adları Türkçe katlanmış hâliyle benzersizdir: `Kapak` ve `KAPAK` aynı bloktur.
Tanım tablosu ekle-yalnızdır; bir tanım silinemez, `GERİAL` referansı ve kopyaları
kaldırıp özgün nesneleri geri getirir ama boş tanım dosyada kalır.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `BLOK` | `BLOK` | `BLOCK` | `BLK` |

## Sözdizimi

```text
BLOK ad=<ad> taban=<sağa>,<yukarı> [nesneler=<kimlik> ...] [aciklama=<metin>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Bloğun adı; benzersiz |
| `taban` | Taban noktası: referanslar bu noktayla yerleştirilir |
| `nesneler` | Bloğa girecek nesnelerin kimlikleri; verilmezse etkin seçim, o da yoksa sorulur |
| `aciklama` | Serbest açıklama |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```text
KATMAN ad=ROGAR
DAİRE merkez=0,0 cevre=0.6,0
ÇİZGİ -0.6,0 0.6,0
SEÇ KATMAN katman=ROGAR
BLOK ad=KAPAK taban=0,0 aciklama="Rögar kapağı"
```

Daire ve çizgi `KAPAK` tanımına girer; (0,0)'da bir referans kalır.

### Arayüz

**Çizim ▸ Blok**. Nesneleri seçin, Enter, adı ve taban noktasını verin.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "ROGAR" } },
    { "cmd": "core.circle_draw", "args": { "merkez": [0,0], "cevre": [600,0] } },
    { "cmd": "core.block", "args": { "ad": "KAPAK", "taban": [0,0], "nesneler": [1] } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` referansı ve tanıma giren kopyaları kaldırır, özgün nesneler
geri gelir. Boş tanım tabloda kalır.

## Betikten kullanım

`nesneler` kalıcı nesne anahtarlarıdır. Günlüğe ad, taban, nesneler ve varsa açıklama
yazılır.

## Hatalar

> `'KAPAK' adında bir blok zaten var; blok adları benzersizdir.`

Aynı adla ikinci tanım yapılamaz; başka ad verin ya da `BLOKEKLE` ile var olanı
yerleştirin.

> `Nesne bulunamadı veya silinmiş: 42`

`nesneler` içinde olmayan bir kimlik.

> `Blok tanımındaki nesne doğrudan düzenlenemez; BLOKDÜZENLE (Faz 2).`

Bir tanımın üyesi başka bir bloğa alınamaz.

## İlgili

- [BLOKEKLE](insert.md) — tanımı yerleştirmek
- [SEÇ](select.md) — bloğa girecek nesneleri seçmek
- [Blok referansı türü](../nesneler/blokreferansi.md)
