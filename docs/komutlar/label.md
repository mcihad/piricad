# ETİKET — Özniteliklerden Yazı

Bir katmandaki nesneleri, kendi özniteliklerini okuyarak etiketler.

## Ne yapar

Bir paftada TAKS ve KAKS bir daire içinde yazar, cephe genişliği yapı çizgisinin
yanında durur, ada/parsel numarası parselin ortasındadır. Bunların hepsi nesnenin
**özniteliği** ve kâğıdın **yazısıdır**; `ETİKET` birini öbürüne taşır.

Her etiket, bildiğiniz bir **yazı nesnesidir**. Taşınabilir, stili
değiştirilebilir, kendi katmanına konup kapatılabilir; `.pcad` ve DXF ile hiçbir
şey eklemeden gidip gelir. CAD'de etiket zaten hep böyle olmuştur.

Bunun bedeli şudur: etiket, sonradan değişen bir özniteliği **takip etmez**.
Komutu yeniden çalıştırmak hepsini tazeler ve bu, her CAD açıklamasının yaptığı
pazarlıktır.

### Neden bir sembol katmanı değil

`.claude/model.md` P29, kare yolunun öznitelik sütunu okumasını yasaklıyor —
haklı olarak: her nesne için her karede bir sütun araması, 16 ms bütçesinin içine
bir arama koyar. Aynı kural, bu programda bir CBS çizicisinin "stil sütununa
yazan bir komut" olmasının da sebebi. Etiketleyici de aynı biçimdedir:

> **Etiketleyici, yazı nesnesi yazan bir komuttur.**

### Sembol alan bildiriyorsa biçim vermeyin

Katmanın sembolündeki bir `yazi-isaretci` katmanı sabit bir kelime yerine bir
**öznitelik sütunu** adlandırmışsa ([`STİL alan=`](style.md)), `ETİKET` biçimi
sormaz: sembol neyin, nereye ve hangi boyda yazılacağını zaten söylüyordur.

```
KATMAN ad=YAPI
STİL katman=YAPI tip=yazi-isaretci alan=taks alan_tipi=metin birim=zemin kaydirma=2500 boyut=3000
STİL katman=YAPI ekle=evet tip=yazi-isaretci alan=kaks alan_tipi=metin birim=zemin kaydirma=-2500 boyut=3000
ALAN noktalar=485300,4310200 485320,4310200 485320,4310220 485300,4310220
ÖZNİTELİK ad=taks nesne=1 deger="0.40"
ÖZNİTELİK ad=kaks nesne=1 deger="1.20"
ETİKET katman=YAPI
```

Kurulum bir kez yapılır; sonrasında etiketlemenin tamamı son satırdır.

Bu, MPYY'nin yapılaşma koşulu dairesini tek çağrıda doldurur: TAKS çizginin
üstüne, KAKS altına, sembolün bildirdiği kaydırmalarla. Önceden bunun için figür
başına bir `ETİKET` ve elle ölçülmüş bir `kaydirma` gerekiyordu — oysa o iki sayı
zaten sembolün kendi sabit yazılarını çizdiği yerlerdi.

**Boş hücre hiçbir şey yazmaz.** TAKS'ı henüz girilmemiş bir parsel dairesinin
içinde `yok` görmez, boş kalır.

`bicim` yazarsanız sembolün slotları yok sayılır — parametreli bir katmana tek
seferlik başka bir etiket atmak böyle mümkün kalır.

## Adlar

`ETİKET` · `ETIKET` · `LABEL` · `ETK`

## Sözdizimi

```
ETİKET katman=<ad> bicim=<biçim> [hedef=<ad>] [yukseklik=<tam sayı>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `katman` | Etiketlenecek katmanın adı. Zorunlu |
| `bicim` | Etiket biçimi. `{sutun}` o sütunun değeriyle değişir. Sembol **alan** bildiriyorsa gerekmez |
| `hedef` | Etiketlerin yazılacağı katman. Verilmezse `<katman> ETİKET` |
| `yukseklik` | Yazı yüksekliği, **zemin milimetresi**. Verilmezse 2000 (2 m) |
| `kaydirma` | Nesnenin ortasından dikey kaydırma, **zemin milimetresi**. Artı yukarı |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

### Satır kırma

`\n` iki karakteri satır kırar, çünkü MPYY'nin `yapılaşma koşulu` gösterimi iki
satırdır: çizginin üstünde TAKS değeri, altında KAKS değeri.

```
ETİKET katman=PARSEL bicim="{taks}\n{kaks}" yukseklik=3800
```

Betikte gerçek satır sonu yazılabildiği için kaçışa gerek yoktur; bu, satır
sonunun yazılamadığı tek yer olan komut satırı içindir.

**Tek kaçış, başka yok.** İkincisi bir dilbilgisi olmaya başlardı.

Satırlar noktanın **etrafına** yığılır: iki satırlı bir etiket noktanın altına
sarkmaz, ortasında durur. Daireyi ve ortadaki çizgiyi katmanın kendi sembolü
çizer; bkz. [STİL](style.md).

### Biçim bir dil değildir

`{sutun}` o sütunun değeriyle değişir ve **başka hiçbir şey olmaz**: işleç yok,
iç içe yazım yok, fonksiyon yok, koşul yok, sayı biçimlendirme yok.

CLAUDE.md 5.11 bu projeye tam olarak bir dilbilgisi tanıyor
(`kentos_cad/command/parser.hpp`) ve bunların herhangi biri ikinci bir dilbilgisi
olurdu. Bunu genişletmek bir yama değil, bir anayasa değişikliğidir.

Tanımlı olmayan bir sütun adı **olduğu gibi kalır**, süslü parantezleriyle
birlikte. Sessizce silinseydi, basılmak üzere olan bir paftadaki yazım hatası
görünmez olurdu.

### MPYY yapılaşma koşulu — çemberin içindeki iki sayı

Yönetmeliğin yapılaşma koşulu için bastığı gösterim, içinden yatay bir çizgi geçen bir
çemberdir: **çizginin üstünde kat alanı katsayısı, altında taban alanı katsayısı.** Her
ikisi de parselin **özniteliğidir**, yani aynı sembolü taşıyan iki parsel farklı sayılar
gösterir.

İş ikiye bölünür ve bu bölünme bilinçlidir:

- **Sembol** çemberi ve çizgiyi çizer. Bunlar hiçbir parsel hakkında bir şey söylemez,
  bu yüzden kaç parsel taşırsa taşısın stil sütununda **tek** kayıttır.
- **`ETİKET`** sayıları yazar. Her sayı sıradan bir yazı nesnesi olur: taşınır,
  yeniden stillenir, kendi katmanında kapatılır, `.pcad` ve DXF'e olduğu gibi gider.

```
KATMAN ad=IMAR
SÜTUN kimlik=taks tur=metin
SÜTUN kimlik=kaks tur=metin

STİL katman=IMAR tip=merkez-isaretci sekil=daire birim=zemin boyut=26000
STİL katman=IMAR ekle=evet tip=merkez-isaretci sekil=cizik aci=90000000 birim=zemin boyut=22000

ETİKET katman=IMAR bicim="{kaks}" hedef=KOSUL_UST yukseklik=3200 kaydirma=4500
ETİKET katman=IMAR bicim="{taks}" hedef=KOSUL_ALT yukseklik=3200 kaydirma=-7000
```

`kaydirma` olmadan iki sayı da nesnenin ortasına, yani aralarındaki çizginin üstüne
düşer. Her sayının kendi `ETİKET` satırı ve kendi kaydırması vardır.

**Neden sembolün kendisi özniteliği okumuyor.** `.claude/model.md` R29 ve P7:
öznitelik sütunları çerçeve yolunda asla okunmaz ve çerçeve yolunda asla ifade
değerlendirilmez. Kare başına nesne başına bir sütun araması, 16 ms bütçesinin içine
bir tablo araması koymak demektir. Bunun karşılığında bir şey kaybedilir ve söylenmesi
gerekir: **etiket, sonradan değişen bir özniteliği takip etmez.** Komutu yeniden
çalıştırmak onları tazeler; bu, her CAD açıklamasının yaptığı pazarlığın aynısıdır.

### Değerler nasıl yazılır

| Sütun türü | Kâğıtta |
|---|---|
| `metin`, `kod` | Olduğu gibi |
| `tam_sayi` | Olduğu gibi |
| `uzunluk` | **Metre** olarak, sondaki sıfırlar atılmış (`1500` → `1,5`) |
| `evet_hayir` | `evet` / `hayır` |
| Boş hücre | **Hiçbir şey** — ölçülmemiş bir cephe ile sıfır cephe aynı şey değildir |

## Örnekler

### Komut satırı

Ada/parsel numarası:

```
KATMAN ad=PARSEL
SÜTUN kimlik=ada tur=tam_sayi
SÜTUN kimlik=parsel tur=tam_sayi
ETİKET katman=PARSEL bicim="{ada}/{parsel}"
```

TAKS ve KAKS, bir dairenin içinde. Daireyi parselin **kendi sembolü** çizer,
yazıyı `ETİKET` yazar; ikisi de nesnenin ortasına geldiği için üst üste düşerler:

```
SÜTUN kimlik=taks tur=metin
SÜTUN kimlik=kaks tur=metin
STİL katman=PARSEL ekle=evet tip=merkez-isaretci sekil=daire birim=zemin boyut=22000 renk=4289396768 kalinlik=400
ETİKET katman=PARSEL bicim="{taks}/{kaks}" yukseklik=3000
```

Etiketleri ayrı bir katmana:

```
ETİKET katman=PARSEL bicim="{ada}/{parsel}" hedef=NUMARALAR
```

### Arayüz

Katmanlar panelinde katmana **sağ tık → Özniteliklerden etiketle…**, biçimi yazın.

### Betik

Betik kendi kendine yeter — betik bloğu kendi belgesinde çalışır, o yüzden
katmanı, sütunu ve nesneyi de kendisi kurar:

```json
[
  { "cmd": "core.layer",  "args": { "ad": "PARSEL" } },
  { "cmd": "core.column", "args": { "kimlik": "ada", "tur": "tam_sayi" } },
  { "cmd": "core.area",   "args": { "noktalar": [[0,0],[40000,0],[40000,30000],[0,30000]] } },
  { "cmd": "core.attribute", "args": { "ad": "ada", "nesne": 1, "deger": "1234" } },
  { "cmd": "core.label",  "args": { "katman": "PARSEL", "bicim": "Ada {ada}", "yukseklik": 2500 } }
]
```

## Geri alma

Tek bir geri alma adımıdır: `GERİAL` bütün etiketleri birlikte kaldırır.

Komut **iki geçişlidir**: her yazı üretilir ve her konum hesaplanır, ancak ondan
sonra ilk yazma yapılır. Yani okunamayan bir sütun adı çizimi yarı etiketli
bırakmaz, hiç dokunmaz (Article 1.6).

## Betikten kullanım

Komut kimliği `core.label`. Betikten ve yapay zekâdan erişilebilir.

Etiket katmanı yoksa oluşturulur; varsa üstüne eklenir. Tazelemek için önce eski
etiketleri silin (`SEÇ` + `SİL`) ya da `GERİAL` ile geri alın.

## Hatalar

| İleti | Sebep | Çözüm |
|---|---|---|
| `Katman bulunamadı: '<ad>'` | Etiketlenecek katman yok | Önce `KATMAN` ile oluşturun |
| `Etiket yüksekliği sıfırdan büyük olmalı.` | `yukseklik=0` ya da eksi | Zemin milimetresi olarak pozitif bir değer verin |
| `'<ad>' katmanında etiketlenecek bir şey bulunamadı.` | Katman boş, ya da biçim her nesne için boş metin üretti | Sütun adlarını `SÜTUN` ile listeleyip denetleyin |
