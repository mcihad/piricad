# TARAMA — Tarama Çizme

## Ne yapar

Kapalı bir sınırın içini bir **desenle** doldurur: dolu (`SOLID`), 45° çizgiler
(`ANSI31`), ağ (`NET`), toprak (`EARTH`)… Sınırı ya seçili kapalı nesnelerden alır —
alan, daire, elips, kapalı çoklu çizgi — ya da `noktalar=` ile doğrudan köşelerden.
Sonuç bir [tarama nesnesidir](../nesneler/tarama.md): sınır halkaları ve desen
birlikte saklanır, DXF'e `HATCH` olarak gider.

### Bağlı tarama

Seçilen nesnelerin içini tarayan tarama o nesnelere **bağlanır** ve TARAMA bunu söyler:
`1 sınır nesnesine bağlı`. Sınır değişince tarama komutun sonunda, **aynı geri alma
adımında** yeniden kurulur:

| Ne olursa | Tarama ne yapar |
|---|---|
| Sınırın köşesi taşınır, alanın deliği değişir, daire ölçeklenir | Halkaları sınırdan yeniden kurulur; **delikten taşmaz** |
| Sınır bütün olarak taşınır | Tarama da taşınır; desen parselin üzerinde aynı yerde kalır |
| Tarama ile sınırı birlikte taşınır | Bağ sürer |
| Yalnız tarama taşınır | Tarama sınırından **çözülür**; artık bağımsızdır |
| Sınır silinir | Bağ **kopar**; tarama son hâlinde durur. Birden çok sınırdan biri silinse de tarama bütünüyle izlemeyi bırakır: kalan sınırlardan yeniden kurulsaydı, silinen parselin içindeki havuz taranan alan olurdu |
| Sınır artık kapanmaz (çizgi açılır) | Bağ **kopar**; tarama son hâlinde durur |
| Tarama kilitli katmandadır | İzleyemez ve bunu söyler |

**Delikler iç içelikten gelir.** Bir alanın kendi delikleri, seçtiğiniz bir nesnenin
içindeki başka bir seçili nesne ve onun da içindekiler, DXF'in olağan tarama kuralıyla
sırayla delik ve dolu olur: bir parsel ve içindeki havuzu birlikte seçerseniz havuz
taranmaz. Bu, her yeniden kuruluşta geometriden yeniden bulunur.

**Kopuk bağ görünür.** Sınırı silinmiş ya da artık kapanmayan taramanın ortasında
uyarı renginde üstü çizili bir halka ve **sınır bağı koptu** yazısı durur; pafta
çıktısına girmez. [`NESNEBİLGİ`](entity_info.md) taramanın hangi nesnelere bağlı
olduğunu, bir nesne için de onu kaç bağlı taramanın izlediğini söyler.

`noktalar=` ile köşelerden çizilen tarama bağsızdır. Bağlamak istemediğinizde
`bagla=hayır` verin. DXF'ten gelen bir taramanın "ilişkili" işareti (grup 71) bu
çizimde bir bağ değildir: NESNEBİLGİ bunu ayrıca söyler.

### Desen kataloğu

Desenler koddan değil, `data/catalogs/dxf/tarama-desenleri.json` dosyasından gelir
(`TERCİH desen_kataloğu`). Her desen çizgi ailelerinden oluşur: açı, taban, bir
çizgiden ötekine kayma ve kesik dizisi, **desen mikrometresi** olarak. Kendi
deseninizi eklemek için dosyayı kopyalayın, satır ekleyin, yolunu tercihe ya da
`katalog=` argümanına verin. Bugün gelenler: `SOLID`, `ANSI31`, `ANSI32`, `ANSI33`,
`ANSI34`, `ANSI37`, `LINE`, `NET`, `DOTS`, `EARTH`, `GRASS`.

### Desen yere bağlıdır

Desenin çizgileri **zemindedir**: taramanın **başlangıç noktasından** (`baslangic=`;
verilmezse çizimin başlangıç noktası, 0,0 — AutoCAD'in de varsayılanı) geçer ve aralığı
kadar tekrar eder. Harita kaydırılınca ya da yakınlaştırılınca desen parselin üzerinde
yerinde kalır. Aynı desenle ayrı ayrı taranan komşu parseller bu yüzden ortak
kenarlarında kesintisiz birleşir; binlercesi de tek seferde çizilir. Sınır bütün olarak
taşınınca başlangıç da onunla taşınır: desen parselle birlikte kayar.

**Çok sık desen ekranı kilitlemez.** Çizgileri ekranda iki buçuk pikselden sık düşen
bir desen, uzaktan görünen hâliyle — çizgilerin ortalama tonuyla — dolu çizilir;
yakınlaşınca çizgiler geri gelir. Çizim değişmez; yalnız ekranda ve aynı sıklıktaki
baskıda böyle görünür.

### Kendi deseniniz

`aralik=` katalog yerine kendi çizgilerinizi verir: `aci` doğrultusunda, metre
cinsinden aralıklı tek bir çizgi ailesi (DXF'in kullanıcı tanımlı deseni; dosyada adı
`_USER`, program size aralığıyla söyler). `cift=evet` aynı çizgileri dik açıyla bir
kez daha çizer: çapraz tarama.

<!-- örnek: yeni çizim -->
```
ALAN 0,0 20,0 20,10 0,10
TARAMA nesneler=1 aralik=2.5 aci=30 cift=evet
```

```text
2,500 m aralıklı kendi deseninizle çapraz tarama çizildi (1 sınır halkası); 1 sınır nesnesine bağlı, o değişince tarama da güncellenir.
```

### Ölçek

Desen ölçüleri çizim değil **kâğıt** düşünülerek verilmiştir: `ANSI31` 3,175 mm
aralıklıdır. `olcek` bu sayıyı çarpar. Vermezseniz pafta ölçeğinin paydası
([`AYAR plan_ölçeği`](setting.md)) kullanılır: 1/1000 paftada aralık zeminde 3,175 m
olur ve kâğıtta 3,175 mm çıkar.

**Ekranda, PDF'te ve DXF'te aynı aralık.** Ekran ve [`YAZDIR`](print.md) deseni aynı
sembolden çizer; DXF'e ölçek çizimin biriminde ve desenin kendi çizgileriyle yazılır
(ayrıntı: [tarama nesnesi](../nesneler/tarama.md#dosya-ve-dış-biçimler)). 1/500'de
`olcek=500` ile çizilen `ANSI31` kâğıtta 3,175 mm aralıklıdır, DXF'te 1,5875 m.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `TARAMA` | `TARAMA` | `HATCH` | `TRM` |

## Sözdizimi

```text
TARAMA noktalar=<sağa>,<yukarı> <sağa>,<yukarı> <sağa>,<yukarı> ... [desen=<ad>] [aci=<derece>] [olcek=<çarpan>]
TARAMA nesneler=<kimlik> ... [desen=<ad>] [aci=<derece>] [olcek=<çarpan>] [baslangic=<nokta>] [cift=evet]
TARAMA nesneler=<kimlik> ... aralik=<metre> [aci=<derece>] [cift=evet]
TARAMA desen=<ad>            ← etkin seçimi tarar
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Sınır köşeleri, en az üç; nesne seçmek yerine |
| `nesneler` | Sınırı verecek kapalı nesnelerin kimlikleri; verilmezse etkin seçim, o da yoksa sorulur |
| `desen` | Katalogdaki desen adı; varsayılan `SOLID` |
| `aci` | Desenin dönme açısı, derece; varsayılan 0 |
| `olcek` | Desen ölçeği; varsayılan pafta ölçeğinin paydası |
| `katalog` | Desen kataloğu dosyası; varsayılan `TERCİH desen_kataloğu` |
| `bagla` | Seçilen sınır nesnelerine bağlansın mı; varsayılan `evet`. Bkz. [Bağlı tarama](#bağlı-tarama) |
| `aralik` | Kendi desen çizgilerinizin aralığı, metre; `desen=` yerine. Bkz. [Kendi deseniniz](#kendi-deseniniz) |
| `cift` | Desen bir de dik açıyla çizilsin mi (çapraz tarama) |
| `baslangic` | Desenin geçtiği nokta; verilmezse çizimin başlangıç noktası (0,0) |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```text
KATMAN ad=LEKELER
TARAMA noktalar=0,0 20,0 20,20 0,20 desen=ANSI31 olcek=1000
```

20 m'lik karenin içi 45° çizgilerle, 3,175 m aralıkla taranır.

```text
TARAMA noktalar=30,0 50,0 50,20 30,20 desen=NET aci=30 olcek=2000
TARAMA noktalar=60,0 80,0 80,20 60,20
```

İkincisi dolu taramadır. Seçili nesneleri taramak için önce seçin:

```text
KATMAN ad=PARSEL
ALAN 100,0 120,0 120,20 100,20
SEÇ KATMAN katman=PARSEL
TARAMA desen=EARTH olcek=1000
```

Bir parseli taramak ve köşesini taşımak; tarama yeni sınıra oturur:

<!-- örnek: yeni çizim -->
```
ALAN 0,0 20,0 20,10 0,10
TARAMA nesneler=1 desen=ANSI31
KÖŞETAŞI nesne=1 kose=3 nokta=26,14
```

```text
'ANSI31' deseniyle tarama çizildi (1 sınır halkası); 1 sınır nesnesine bağlı, o değişince tarama da güncellenir.
Bağlı 1 tarama sınırını izledi ve yeniden kuruldu.
```

Avlulu bir parsel — ikinci halka delik — ve içindeki bir havuzu birlikte taramak:

<!-- örnek: yeni çizim -->
```
ALAN 0,0 30,0 30,30 0,30 4,4 12,4 12,12 4,12 bolum=4 bolum=4
DAİRE merkez=20,20 cevre=24,20
TARAMA nesneler=1 nesneler=2 desen=ANSI31
```

```text
'ANSI31' deseniyle tarama çizildi (3 sınır halkası, 2 delik); 2 sınır nesnesine bağlı, o değişince tarama da güncellenir.
```

İki buçuk metre aralıklı, 30°'lik kendi çapraz taramanız:

<!-- örnek: yeni çizim -->
```
ALAN 0,0 20,0 20,10 0,10
TARAMA nesneler=1 aralik=2.5 aci=30 cift=evet
```

### Arayüz

**Çizim ▸ Tarama** panelinin galerisinden bir desene ya da **Tarama** düğmesine (**Giriş ▸
Çizim**'de de vardır) basın. Kapalı nesneleri seçip Enter'a basın ya da köşeleri tıklayın.
Çizilmiş bir taramanın desenini, açısını, ölçeğini, aralığını ve ada kuralını
nitelik panelinin **TARAMA** grubundan ya da [`TARAMADÜZENLE`](hatch_edit.md) ile
değiştirin.
Seçerek çizilen tarama bu nesnelere bağlıdır: parselin köşesini tutamağından
sürüklediğinizde tarama da güncellenir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "LEKELER" } },
    { "cmd": "core.hatch",
      "args": { "noktalar": [[0,0],[20000,0],[20000,20000],[0,20000]],
                "desen": "ANSI31", "aci": 0, "olcek": 1000 } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` taramayı kaldırır; sınır olarak seçilen nesnelere dokunulmaz.
Bağlı taramanın sınırını izlemesi, onu doğuran komutla aynı adımdadır.

## Betikten kullanım

`noktalar` milimetre çiftleridir. Günlüğe sınırın nasıl verildiği (`noktalar` ya da
`nesneler`), desen adı, açı ve **kullanılan** ölçek yazılır — varsayılandan geldiyse
de yazılır, böylece pafta ölçeği değişse tekrar aynı taramayı kurar.

## Hatalar

Bağlı taramaların izlemesi şu satırlarla bildirilir:

| Satır | Ne oldu |
|---|---|
| `Bağlı N tarama sınırını izledi ve yeniden kuruldu.` | Sınırları değişti |
| `Sınırı silindiği için N tarama bağı koptu; tarama son hâlinde duruyor.` | Sınır nesnesi silindi |
| `Sınırı artık kapanmadığı için N tarama bağı koptu; tarama son hâlinde duruyor.` | Sınır açıldı |
| `N tarama sınırından ayrı taşındığı için bağından çözüldü.` | Yalnız tarama taşındı |
| `Bağlı N tarama kilitli katmanda olduğu için sınırını izleyemedi.` | Tarama kilitli katmanda |

> `Tarama sınırı en az üç nokta ister; verilen 2.`

`noktalar` ile iki nokta verildi.

> `Nesne 7 kapalı değil; tarama sınırı kapalı bir alan, daire, elips ya da kapalı çoklu çizgi olmalı.`

Seçilen nesne açık bir çizgi.

> `Tanınmayan tarama deseni: 'CIMEN'. Katalogdaki desenler: SOLID, ANSI31, …`

Desen adı katalogda yok; adı düzeltin ya da kataloğa ekleyin.

> `Tarama deseni kataloğu bulunamadı: 'data/catalogs/dxf/tarama-desenleri.json'. TERCİH desen_kataloğu ile yolunu kurun ya da katalog= verin.`

Katalog dosyası bu makinede yok.

> `Tarama ölçeği sıfırdan büyük olmalı.`

`olcek=0` ya da eksi.

## İlgili

- [TARAMADÜZENLE](hatch_edit.md) — çizilmiş taramanın desenini, açısını, ölçeğini, ada kuralını değiştirmek
- [ALAN](area.md) — taranacak kapalı sınırı çizmek
- [STİL](style.md) — bir alanı katalogdaki gösterimle doldurmak; yönetmelik lekesi için o kullanılır
- [Tarama türü](../nesneler/tarama.md) — saklanış ve DXF eşlemesi
- [TERCİH](preference.md) — desen kataloğu yolu
