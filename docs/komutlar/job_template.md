# İŞŞABLONU — İş Şablonları

Sık yapılan bir işi — atlas, kadastro kontrolü, parsel raporu — **doğru sırayla** yapmak
isteyen harita mühendisi ve ona yardım eden ajan için; bu sayfayı bitirdiğinizde şablonları
listelemeyi, bir şablonun adımlarını okumayı ve bu adımların neden **çalıştırılmadığını**
bileceksiniz.

## Ne yapar

Programın bildiği **iş şablonlarını** verir. Bir şablon, bir işin komut satırlarıdır:
adlandırılmış, sürümlü ve **sırasıyla**.

> **Hiçbir şey çalıştırmaz.** Şablon size satırları verir; satırları siz (ya da bir ajan)
> olağan yoldan gönderirsiniz. Yazan her adım yine önizlemeli bir **öneri** olur ve
> bilgisayar başındaki kişi uygular. Kendi kendini çalıştıran bir şablon, arayüzün ve
> ajanın eşit istemci olduğu kuralın yanından geçen bir hızlı yol olurdu; öyle bir yol yok.

Bir atlas altı komuttur ve **sıra** tahmin edilemeyen kısımdır: yerleşimi kur, üstüne
harita çerçevesi koy, atlası bir katmana nişanla, sayfayı denetle, sonra bas. Denetlemeyi
basmadan **önce** yapmak ile sonra yapmak arasındaki fark, kopuk bir bağı ekranda görmek
ile imzalanmış bir PDF'de görmek arasındaki farktır.

Şablonlar **veridir**, C++ değil: `data/catalogs/ai/is-sablonlari.json`. Daha iyi bir sıra
bulunduğunda bu bir **veri yayımıdır**, yeniden derleme değil. Her adım, canlı komut
kütüğüne karşı sınanır: var olmayan bir komut ya da o komutun tanımadığı bir argüman
yapıyı kırar.

Her şablonun **kendi sürümü** vardır, paketinkinden ayrı. Bir istemci adımları önbelleğe
almış olabilir; sıranın değiştiğini oradan anlar, ve bir işi güncellemek diğerlerinin
değiştiğini söylemez.

### Şablonun yer tutucuları

Adımlarda `<ad>` biçiminde yazılan yerler, şablonun parametreleridir. Doldurmadığınız bir
yer tutucu **olduğu gibi kalır**: boşaltmak `ad=""` gibi bitmiş görünen ama bitmemiş bir
satır üretirdi.

## Adlar

| Ad | Tür |
|---|---|
| `İŞŞABLONU` | Türkçe, birincil |
| `ISSABLONU` | ASCII karşılığı |
| `JOBTEMPLATE` | İngilizce karşılık |
| `İŞŞ` | Kısaltma |
| `core.job_template` | Komut kimliği |

## Sözdizimi

```text
İŞŞABLONU islem=listele
İŞŞABLONU islem=goster sablon=<kimlik>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `islem` | Yapılacak iş. Zorunlu. `listele` ya da `goster` |
| `sablon` | Hangi şablon: `goster` için zorunlu. Kimlikleri `islem=listele` ile görün |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Şablonlar

| Kimlik | Ne üretir |
|---|---|
| `atlas-pafta` | Bir katmandaki her nesne için bir sayfa üreten çıktı yerleşimi kurar, denetler ve tek PDF'e basar |
| `kadastro-kontrol` | Çizimin şeklini, katmanlarını, öznitelik şemasını ve yerleşim eksiklerini sırayla okur. Tamamı okumadır |
| `parsel-raporu` | Bir katmanın özniteliklerini tablo olarak taşıyan tek sayfalık yerleşim kurar ve basar |

## Örnekler

### Komut satırı

```text
İŞŞABLONU islem=listele
```

```text
3 iş şablonu (paket 1.0.0):
  atlas-pafta — Atlas: her parsel için bir sayfa
  kadastro-kontrol — Kadastro kontrolü: çizimi basmadan önce okumak
  parsel-raporu — Parsel raporu: tablolu tek sayfa
Adımları görmek için: İŞŞABLONU islem=goster sablon=<kimlik>
```

```text
İŞŞABLONU islem=goster sablon=atlas-pafta
```

```text
Atlas: her parsel için bir sayfa (atlas-pafta 1.0.0), 10 adım:
  BAĞLAM
  ÇIKTIYERLEŞİMİ islem=ekle ad="<yerlesim>" kagit=<kagit> yon=<yon>
  ÇIKTIÖĞE islem=ekle yerlesim="<yerlesim>" tur=harita ad=Harita
  …
  ÇIKTIYERLEŞİMİ islem=denetle ad="<yerlesim>"
  YAZDIR yerlesim="<yerlesim>" dosya="<dosya>"
Bu satırlar çalıştırılmadı.
```

Yer tutucuları doldurup satırları sırayla yazarsınız:

```text
ÇIKTIYERLEŞİMİ islem=ekle ad="Ada 1284 Atlası" kagit=A3 yon=yatay
```

### Arayüz

Arayüzde şablonun karşılığı, adımları **elle** yapmaktır: `Çıktı ▸ Çıktı Yerleşimleri`
penceresinden yerleşimi kurar, öğeleri koyar, **Atlas** sekmesinden katmanı seçer,
**Denetle** ile eksikleri okur ve **Yazdır** dersiniz. Şablon aynı sırayı yazılı hâlde
verir — özellikle bir ajanın okuyabileceği hâlde. Menüden başlatıldığında komut
önce ne yapılacağını (`listele` ya da `goster`) sorar; `goster` için şablonun
kimliğini, kataloğun kimliklerini önererek sorar.

### Betik

```json
{ "cmd": "core.job_template", "args": { "islem": "goster", "sablon": "atlas-pafta" } }
```

Yapılandırılmış sonuç `sablon`, `ad`, `ozet`, `surum`, `adim_sayisi`, `parametreler`,
`adimlar`, `notlar`, `paket_surumu` ve `aciklama` taşır. `aciklama` her seferinde adımların
çalıştırılmadığını söyler.

## Geri alma

`İŞŞABLONU` hiçbir şeyi değiştirmez: geri alınacak bir şey yoktur ve [`GERİAL`](undo.md)
listesine girmez. Komut `NoEffect` taşır, yani bir ajan onu **onay beklemeden**
çalıştırabilir. Şablonun **adımları** ise ayrı iştir: yazan her biri kendi kuralına
uyar.

## Betikten kullanım

Komut betiklerde `core.job_template` kimliğiyle çağrılır.

Komut **etkileşimlidir**: `islem` eksikse sorulur, `islem=goster` verilip `sablon`
yazılmazsa o da sorulur. Bir betikte soracak kimse olmadığı için ikisini de yazın.

Şablon paketi **her çağrıda** okunur, önbelleğe alınmaz: bir veri yayımı programı yeniden
başlatmadan etkisini gösterir — şablonların veri olmasının sebebi zaten budur.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Tanınmayan işlem: 'ac'. İşlemler: listele / goster` | `islem` iki sözcükten biri değil (komut satırında sorulan işlem) | `listele` ya da `goster` yazın |
| `'core.job_template': 'islem' için tanınmayan değer 'ac'. Kabul edilenler: listele / goster` | Aynı hata betikten geldiğinde: veri yolu gövde çalışmadan önce yakalar | İki sözcükten birini yazın |
| `Hangi şablon: sablon=<kimlik>. Kimlikleri İŞŞABLONU islem=listele ile görün.` | `islem=goster` verildi, `sablon` yazılmadı | Kimliği yazın |
| `Böyle bir iş şablonu yok: 'x'. Olanlar: atlas-pafta, kadastro-kontrol, parsel-raporu` | Kimlik yanlış | Listedeki kimliklerden birini yazın |
| `Veri paketi bulunamadı: 'data/catalogs/ai/is-sablonlari.json'. Kurulumda eksikse KENTOS_DATA ile dizini gösterin.` | Şablon paketi kurulumda yok | `KENTOS_DATA` ile veri dizinini gösterin |
| `İş şablonu paketi okunamadı: …` | Paket bozuk JSON | Paketi kurulumdan yeniden alın |

## İlgili

- [`ARAÇARA`](tool_search.md) — katalogda bir araç aramak
- [`ÇIKTIYERLEŞİMİ`](layout.md) — yerleşim kurmak ve atlası nişanlamak
- [`ÇIKTIÖĞE`](layout_item.md) — yerleşimin üstündeki öğeler
- [`YAZDIR`](print.md) — yerleşimi PDF'e ya da yazıcıya basmak
- [Onay ve denetim](../yapay-zeka/onay.md) — yazan bir adım neden öneri olur
