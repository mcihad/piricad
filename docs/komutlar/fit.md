# OTURT — Yerel Çizimi Haritaya Oturtma

## Ne yapar

Yerel ölçülmüş bir çizimi, yayımlanmış kontrol noktalarını kullanarak haritaya
taşır. **2 boyutlu Helmert** (benzerlik) dönüşümüdür: öteler, döndürür ve tek bir
ölçekle büyütür/küçültür.

Ekibin istasyonu kurup ona 0,0 dediği ve bir hafta oradan çalıştığı iş için
vardır. Çizimin **kendi içinde** her mesafesi ve her açısı doğrudur; olmadığı tek
şey haritanın üzerinde olmaktır.

### Neden benzerlik, afin değil

Bir ölçünün iç geometrisi **veridir**: kendi çizgileri arasındaki açılar ölçülmüş
gerçeklerdir. Kaydırabilen ya da bir ekseni diğerinden fazla geren bir dönüşüm,
kontrolü ölçüye değil ölçüyü kontrole uydururdu. Bu komut asla kaydırmaz.

### Ölçek kilidi

Kalibre şeritle ya da total station'la çalışan bir ekip mesafelerini zaten
indirgemiştir. `olcek_kilitli=evet` ölçeği tam 1'de tutar; böylece bir kontrol
noktası hatası çizimdeki her uzunluğa sessizce dağılmaz. Kilitliyken artıklar
büyür — ve büyümesi gerekir, çünkü hata oradadır.

### Artıklar

- **İki nokta**: tam çözüm, artık yok. Dört parametre iki noktayı tam karşılar.
- **Üç ve fazlası**: en küçük kareler. Her nokta için artık, karesel ortalama
  (RMS) ve en büyük artık yazılır.

Artıklar **uygulanacak** dönüşümle, yuvarlaması dahil ölçülür; rapor çizimin
alamayacağı bir uyum vaat etmez.

## Adlar

| Türkçe | İngilizce | Kısaltma |
|---|---|---|
| `OTURT` | `FIT`, `GEOREF` | `OTR` |

## Sözdizimi

```text
OTURT noktalar=<yerel> <harita> <yerel> <harita> ... [olcek_kilitli=evet] [sistem=<ad>]
```

Noktalar **çift** gelir: önce yereldeki, sonra haritadaki. En az iki çift.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Kontrol çiftleri: yerel, harita, yerel, harita… |
| `olcek_kilitli` | Ölçeği 1'de tutar; saha ölçüsü yeniden ölçeklenmez |
| `sistem` | Oturtulduktan sonraki koordinat sistemi, örnek `TUREF/TM36` |

## Örnekler

### Komut satırı

```text
KATMAN ad=PARSEL
ALAN noktalar=0,0 10,0 10,10 0,10
OTURT noktalar=0,0 485300,4310200 10,0 485310,4310200 sistem=TUREF/TM36
```

```text
Oturtma: 2 kontrol noktası, ölçek 1,000, dönüklük 0,000 grad
  karesel ortalama artık (RMS): 0,000 m
  en büyük artık: 0,000 m
  1. nokta artığı: 0,000 m
  2. nokta artığı: 0,000 m
1 nesne haritaya oturtuldu.
```

### Arayüz

Çizimi `YEREL` sistemde başlatın — durum çubuğu **YEREL · haritaya oturtulmadı**
yazar. İş bitince kontrol noktalarını `OTURT` ile verin.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.area",  "args": { "noktalar": [[0,0],[10000,0],[10000,10000],[0,10000]] } },
    { "cmd": "core.fit",
      "args": { "noktalar": [[0,0],[485300000,4310200000],[10000,0],[485310000,4310200000]],
                "sistem": "TUREF/TM36" } }
  ]
}
```

## Geri alma

**Tek adımdır ve bu bir gerekliliktir.** Çizimin her köşesi ya taşınır ya hiçbiri
taşınmaz; yarı taşınmış bir kadastro paftası Anayasa 1.6'nın adını koyduğu
hatadır ve burada her yerden daha kötüdür, çünkü iki yarısı da makul görünür.

`GERİAL` bütün çizimi eski yerine döndürür.

## Betikten kullanım

Kontrol çiftleri argüman olduğu için bir betik oturtmayı tekrarlanabilir biçimde
yapar. Parametreler komut günlüğüne yazılır: bir paftanın haritaya **nasıl**
oturtulduğu, paftanın kendisi kadar denetlenebilir bir bilgidir.

## Hatalar

> `Oturtma en az iki kontrol noktası ister.`

Tek nokta yalnız ötelemeyi verir; dönüklük ve ölçek bilinmez ve uydurulmaz.

> `Kontrol noktalarının hepsi aynı yerde; dönüklük ve ölçek belirlenemez.`

Yerel noktalar birbirinden ayrışmıyor.

> `OTURT nokta ÇİFTLERİ ister: yerel, harita, yerel, harita...`

Tek sayıda nokta verildi.

## Doğrulama durumu

Bu komutun golden değerleri **jeodezi uzmanı imzası bekliyor** (CLAUDE.md 6.11).
Aritmetiği ve determinizmi birim testleriyle sınanmıştır; TKGM referans
değerleriyle karşılaştırma ayrı bir veri işidir.

## İlgili

- [KOORDİNAT](coordinate.md) — bir noktanın değerini okur
- [AYAR](setting.md) — `koordinat_sistemi`
