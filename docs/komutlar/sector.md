# DİLİM — Daire Dilimi Çizme

## Ne yapar

Merkez ve iki kenardan **kapalı bir daire dilimi** çizer: bir kavşak dolgusu, bir
görüş konisi, bir etki sektörü.

Süpürme her zaman **saat yönünün tersinedir**, `YAY`'daki gibi. İki kenarı ters
sırada vermek aynı çemberin diğer dilimini verir; yön denetimi budur, bayrak
yoktur.

### Alan olarak saklanır

`DAİRE` ve `YAY` **tanımıyla** saklanır — merkez ve yarıçap — çünkü bütün
şekilleri iki sayıdan çıkar. Bir dilimin sınırı ise iki **düz** yarıçap ve bir
eğridir; çizildikten sonra sıradan bir halkadır ve model'e bunun için ayrı bir
tür eklemek, programın başka hiçbir yerde tanıması gerekmeyen bir şekli tanımak
zorunda bırakırdı.

Eğrisi `YAY`'ın kullandığı deterministik bölmeyle üretilir, yani bir dilimin yayı
ile üstüne çizilen bir `YAY` tıpatıp aynı köşelere oturur.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `DİLİM` | `DILIM` | `SECTOR` | `DL` |

## Sözdizimi

```text
DİLİM merkez=<sağa>,<yukarı> baslangic=<sağa>,<yukarı> bitis=<sağa>,<yukarı>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `merkez` | Dilimin merkezi |
| `baslangic` | İlk kenarın ucu; **yarıçapı bu belirler** |
| `bitis` | İkinci kenarın yönü |

## Örnekler

### Komut satırı

```text
KATMAN ad=PARSEL
DİLİM merkez=0,0 baslangic=10,0 bitis=0,10
```

Çeyrek daire: 10 m yarıçapta, doğudan kuzeye.

### Arayüz

**Çizim ▸ Daire Dilimi**. Merkezi tıklayın, sonra ilk kenarı — kılavuz o yarıçapın
çemberini gösterir — sonra ikinci kenarı; kılavuz artık süpürülecek yayı çizer.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer",  "args": { "ad": "PARSEL" } },
    { "cmd": "core.sector",
      "args": { "merkez": [0,0], "baslangic": [10000,0], "bitis": [0,10000] } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` dilimi bütünüyle kaldırır.

## Betikten kullanım

Üç noktası da argüman olarak verilebilir; betikte hiçbir şey sorulmaz.

## Hatalar

> `İlk kenar merkezle aynı yerde; yarıçap sıfır olamaz.`

`baslangic` ile `merkez` çakışıyor.

> `İkinci kenar merkezle aynı yerde; dilimin nereye kadar gideceği belirsiz.`

`bitis` ile `merkez` çakışıyor.

> `Bu iki kenar bir dilim kapatmıyor: süpürme sıfır.`

İki kenar aynı yönde; arada kalan alan yok.

## İlgili

- [YAY](arc_draw.md) — kapatmayan eğri
- [DAİRE](circle_draw.md) — tam daire
- [HALKA](annulus.md) — delikli halka
