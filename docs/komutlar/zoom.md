# YAKINLAŞ — Görünüm Ayarlama

Çiziminde gezinen herkes için; bu sayfayı bitirdiğinizde görünümü kapsama sığdırmayı,
oranla yakınlaştırmayı, önceki görünümlere geri dönmeyi ve bunu çalışan bir komutu
bozmadan yapmayı bileceksiniz.

## Ne yapar

Harita görünümünü değiştirir. Beş kipi vardır: çizimin tamamını pencereye sığdırmak, bir
çarpanla yakınlaştırıp uzaklaştırmak, başlangıç görünümüne dönmek, ve görünüm geçmişinde
bir adım geri ya da ileri gitmek.

`YAKINLAŞ` çizime dokunmaz. Görünüm ayarıdır, çizimin verisi değildir; bu yüzden geri
alma yığınına girmez ve `GERİAL` ile geri gelmez.

**Şeffaf komuttur:** başka bir komut çalışırken araya girebilir. Çizgi çizerken
yakınlaşıp kaldığınız yerden devam edebilirsiniz.

## Adlar

| Ad | Tür |
|---|---|
| `YAKINLAŞ` | Türkçe, birincil |
| `YAKINLAS` | Türkçe karaktersiz klavye için |
| `LİMİTBUL` | Türkçe eş ad: Netcad'deki adı (Limit Bul). Tek başına yazılınca, `YAKINLAŞ` gibi, çizimin kapsamına yakınlaşır |
| `LIMITBUL` | ASCII karşılık |
| `ZOOM` | İngilizce karşılık |
| `Z` | Kısaltma |
| `core.zoom` | Komut kimliği |

## Sözdizimi

```
YAKINLAŞ
YAKINLAŞ KAPSAM
YAKINLAŞ ÇARPAN carpan=<sayı>
YAKINLAŞ SIFIRLA
YAKINLAŞ ÖNCEKİ
YAKINLAŞ SONRAKİ
```

Kip verilmezse `KAPSAM` varsayılır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `mod` | `KAPSAM`, `ÇARPAN`, `SIFIRLA`, `ÖNCEKİ` ya da `SONRAKİ`. İngilizce karşılıkları `EXTENTS`, `FACTOR`, `RESET`, `PREVIOUS`, `NEXT` de kabul edilir; Türkçe harfsiz yazım da geçer (`ONCEKI`, `SONRAKI`) |
| `carpan` | `ÇARPAN` kipinde ölçek katsayısı. Birden büyük yakınlaştırır, birden küçük uzaklaştırır |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

### Kipler

| Kip | Ne yapar |
|---|---|
| `KAPSAM` | Görünür bütün nesneleri, kenarlarda pay bırakarak pencereye sığdırır. Çizim boşsa başlangıç görünümüne döner |
| `ÇARPAN` | Görünümün merkezini koruyarak `carpan` kadar ölçekler |
| `SIFIRLA` | Başlangıç görünümüne döner |
| `ÖNCEKİ` | Bir önceki görünüme döner. Netcad'in Önceki Pencere'si gibi otuz adım geri gider |
| `SONRAKİ` | `ÖNCEKİ` ile geri dönülen görünümden bir adım ileri gider |

### Görünüm geçmişi

Program her görünüm değişikliğini hatırlar: `YAKINLAŞ`'ın her kipi, `KAYDIR`, fare
tekerleği ve orta tuşla sürükleme. `ÖNCEKİ` bunlardan otuz adım geri, `SONRAKİ` geri
gidilen adımlar kadar ileri gider.

| Ne olur | Geçmişte |
|---|---|
| Fare tekerleğiyle art arda yakınlaşmak | **Tek adım**: birbirinden en fazla 0,7 saniye arayla dönen çentikler bir bakıştır, on çentiği geri almak için on kez `ÖNCEKİ` gerekmez |
| Orta tuşla bir sürükleme | Tek adım, ne kadar uzun sürerse sürsün |
| Görünümü değiştirmeyen bir hamle (görünüm zaten kapsamdayken `KAPSAM`) | Adım değildir |
| `ÖNCEKİ`'den sonra yeni bir hamle | İleri adımlar silinir; tarayıcıdaki gibi yeni bir dal başlar |
| `YENİ` ile yeni çizim | Geçmiş silinir: eski çizimin görünümüne geri gidilmez |
| Otuz birinci adım | En eskisi düşer |

Geri gidilecek görünüm yoksa `ÖNCEKİ` hata vermez, durumu söyler: `Geri dönülecek görünüm
yok: görünüm geçmişi boş.` Böylece bir betik duruncaya kadar geri gidebilir.

Geçmiş oturumun görünüm durumudur: çizim dosyasına yazılmaz, geri alma yığınına ve komut
günlüğüne girmez.

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

Bir köşeye yakınlaşıp koordinat okuduktan sonra paftaya geri dön:

```
YAKINLAŞ ÖNCEKİ
```

Geri dönülen görünümden yeniden ileri git:

```
YAKINLAŞ SONRAKİ
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
| **Görünüm ▸ Gezinme ▸ Kapsama Yakınlaş** | `YAKINLAŞ KAPSAM` |
| **Görünüm ▸ Gezinme ▸ Yakınlaştır** | `YAKINLAŞ ÇARPAN carpan=1.25` |
| **Görünüm ▸ Gezinme ▸ Uzaklaştır** | `YAKINLAŞ ÇARPAN carpan=0.8` |
| **Görünüm ▸ Gezinme ▸ Önceki Görünüm** ya da **Alt+C** | `YAKINLAŞ ÖNCEKİ` |
| **Görünüm ▸ Gezinme ▸ Sonraki Görünüm** | `YAKINLAŞ SONRAKİ` |
| **Ctrl+0** | `YAKINLAŞ KAPSAM` |
| **Ctrl++** / **Ctrl+-** | Yakınlaştır / uzaklaştır |

Fare tekerleği ve orta tuşla kaydırma her zaman çalışır ve komut göndermez; bunlar
doğrudan görünüm etkileşimleridir. Görünüm geçmişine yine de girerler: `ÖNCEKİ` bir
tekerlek dizisinden ya da bir sürüklemeden önceki görünüme döner.

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

Her çağrı ne olduğunu yapılandırılmış olarak da söyler:

```json
{ "mod": "ÖNCEKİ", "degisti": true, "geri": 4, "ileri": 1 }
```

`degisti` görünümün yerinden oynayıp oynamadığını, `geri` ve `ileri` görünüm geçmişinde
kaç adım kaldığını söyler.

## Geri alma

`YAKINLAŞ` geri alınmaz. Görünüm ayarı çizimin verisi değildir, bu yüzden geri alma
yığınına hiç girmez. `GERİAL` bir önceki **çizim** işlemine gider, bir önceki görünüme
değil.

Bir önceki görünüme dönmek için `YAKINLAŞ ÖNCEKİ` (**Alt+C**) kullanın.

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
| `Beklenen mod: KAPSAM \| ÇARPAN \| SIFIRLA \| ÖNCEKİ \| SONRAKİ. Girilen: 'OLMAYAN'` | Geçersiz kip adı | Beş kipten birini yazın |
| `Geri dönülecek görünüm yok: görünüm geçmişi boş.` | `ÖNCEKİ` ile gidilecek daha eski bir görünüm kalmadı | Hata değildir; görünüm yerinde kalır |
| `İleri gidilecek görünüm yok: ÖNCEKİ ile geri gidilmedi ya da o zamandan beri görünüm değişti.` | `SONRAKİ` için geri gidilmiş bir adım yok | Hata değildir; önce `ÖNCEKİ` ile geri gidin |
| `Görünüm istemcisi bağlı değil (başsız çalışma).` | Arayüz olmadan çalışılıyor | Hata değildir; beklenen davranıştır |
| `'core.zoom': bilinmeyen parametre 'oran'. Tanımlı parametreler: mod, carpan` | Parametre adı yanlış | `carpan` yazın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
