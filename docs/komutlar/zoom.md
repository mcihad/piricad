# YAKINLAŞ — Görünüm Ayarlama

Çiziminde gezinen herkes için; bu sayfayı bitirdiğinizde görünümü kapsama sığdırmayı,
oranla yakınlaştırmayı ve bunu çalışan bir komutu bozmadan yapmayı bileceksiniz.

## Ne yapar

Harita görünümünü değiştirir. Üç kipi vardır: çizimin tamamını pencereye sığdırmak, bir
çarpanla yakınlaştırıp uzaklaştırmak, ve başlangıç görünümüne dönmek.

`YAKINLAŞ` çizime dokunmaz. Görünüm ayarıdır, çizimin verisi değildir; bu yüzden geri
alma yığınına girmez ve `GERİAL` ile geri gelmez.

**Şeffaf komuttur:** başka bir komut çalışırken araya girebilir. Çizgi çizerken
yakınlaşıp kaldığınız yerden devam edebilirsiniz.

## Adlar

| Ad | Tür |
|---|---|
| `YAKINLAŞ` | Türkçe, birincil |
| `YAKINLAS` | Türkçe karaktersiz klavye için |
| `ZOOM` | İngilizce karşılık |
| `Z` | Kısaltma |
| `core.zoom` | Komut kimliği |

## Sözdizimi

```
YAKINLAŞ
YAKINLAŞ KAPSAM
YAKINLAŞ ÇARPAN carpan=<sayı>
YAKINLAŞ SIFIRLA
```

Kip verilmezse `KAPSAM` varsayılır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `mod` | `KAPSAM`, `ÇARPAN` veya `SIFIRLA`. İngilizce karşılıkları `EXTENTS`, `FACTOR`, `RESET` de kabul edilir |
| `carpan` | `ÇARPAN` kipinde ölçek katsayısı. Birden büyük yakınlaştırır, birden küçük uzaklaştırır |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

### Kipler

| Kip | Ne yapar |
|---|---|
| `KAPSAM` | Görünür bütün nesneleri, kenarlarda pay bırakarak pencereye sığdırır. Çizim boşsa başlangıç görünümüne döner |
| `ÇARPAN` | Görünümün merkezini koruyarak `carpan` kadar ölçekler |
| `SIFIRLA` | Başlangıç görünümüne döner |

## Örnekler

### Komut satırı

Çizimin tamamını göster:

```
YAKINLAŞ KAPSAM
```

Kip yazmadan da olur, `KAPSAM` varsayılandır:

```
YAKINLAŞ
```

Dörtte bir yakınlaştır:

```
YAKINLAŞ ÇARPAN carpan=1.25
```

Uzaklaştır:

```
YAKINLAŞ ÇARPAN carpan=0.8
```

Başlangıç görünümüne dön:

```
YAKINLAŞ SIFIRLA
```

Çizgi çizerken araya girmek — komut kaldığı yerden devam eder:

```
ÇİZGİ                    ← komut "İlk nokta" ister
485300,4310200           ← ilk nokta girildi
YAKINLAŞ KAPSAM          ← araya girer, görünüm değişir
@50,30                   ← çizgi kaldığı yerden devam eder
                         ← Esc
```

### Arayüz

| Yol | Sonuç |
|---|---|
| **Görünüm** araç çubuğunda **Kapsama Yakınlaş** | `YAKINLAŞ KAPSAM` |
| **Görünüm** araç çubuğunda **Yakınlaştır** | `YAKINLAŞ ÇARPAN carpan=1.25` |
| **Görünüm** araç çubuğunda **Uzaklaştır** | `YAKINLAŞ ÇARPAN carpan=0.8` |
| **Görünüm** menüsü | Aynı üç komut |
| **Ctrl+0** | `YAKINLAŞ KAPSAM` |
| **Ctrl++** / **Ctrl+-** | Yakınlaştır / uzaklaştır |

Fare tekerleği ve orta tuşla kaydırma her zaman çalışır ve komut göndermez; bunlar
doğrudan görünüm etkileşimleridir.

Ölçek durum çubuğunda "1 px = 0.1418 m" biçiminde yazar.

### Betik

```json
{
  "ad": "Çiz ve göster",
  "komutlar": [
    { "cmd": "core.line", "args": { "noktalar": [[485300000,4310200000],[485360000,4310245000]] } },
    { "cmd": "core.zoom", "args": { "mod": "KAPSAM" } }
  ]
}
```

`--betik` seçeneğiyle açtığınız betiklerde bunu yazmanıza gerek yoktur; program betik
bittikten sonra kendiliğinden kapsama yakınlaşır.

## Geri alma

`YAKINLAŞ` geri alınmaz. Görünüm ayarı çizimin verisi değildir, bu yüzden geri alma
yığınına hiç girmez. `GERİAL` bir önceki **çizim** işlemine gider, bir önceki görünüme
değil.

Görünümü geri almak isterseniz `YAKINLAŞ KAPSAM` veya `YAKINLAŞ SIFIRLA` kullanın.

## Betikten kullanım

`YAKINLAŞ` betiklenebilir ve AI erişimlidir.

Görünüm istemcisi bağlı olmadan çalıştırılırsa — örneğin başsız bir toplu işlemde —
komut hata vermez, transkripte açıklama yazar:

```text
Görünüm istemcisi bağlı değil (başsız çalışma).
```

Bu sayede aynı betik hem arayüzde hem başsız çalışabilir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Beklenen mod: KAPSAM \| ÇARPAN \| SIFIRLA. Girilen: 'OLMAYAN'` | Geçersiz kip adı | Üç kipten birini yazın |
| `Görünüm istemcisi bağlı değil (başsız çalışma).` | Arayüz olmadan çalışılıyor | Hata değildir; beklenen davranıştır |
| `'core.zoom': bilinmeyen parametre 'oran'. Tanımlı parametreler: mod, carpan` | Parametre adı yanlış | `carpan` yazın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
