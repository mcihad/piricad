# DÖRDÜNCÜKÖŞE — Üç Köşeden Dördüncü Köşe

Ölçü krokisinde bir binanın üç köşesini ölçmüş, dördüncüsüne ulaşamamış — bir çitin arkasında,
bir ağacın altında, komşunun bahçesinde kalmış — herkes için; bu sayfayı bitirdiğinizde
üç köşeden dördüncüsünü hesaplatıp dört köşeli alanı çizmeyi ve bina dik açılıysa ölçüm
sapmasını okumayı bileceksiniz.

## Ne yapar

Sırayla üç köşe alır: **birinci**, **ikinci** ve **üçüncü**. İkinci köşe, birinci ile
üçüncünün **arasındaki** köşedir; dördüncü köşe onun karşısına düşer ve dörtgeni
paralelkenar olarak tamamlar:

| Köşe | Nerede |
|---|---|
| Birinci, ikinci, üçüncü | Sizin verdikleriniz, sırayla; kenarlar birinciden ikinciye, ikinciden üçüncüye gider |
| Dördüncü | `birinci + üçüncü − ikinci`: ikinciden üçüncüye giden kenarın birinciden başlayan hâli |

Komut, dört köşeden geçen **kapalı bir alan** çizer — [`ALAN`](area.md) ile aynı türden
nesne — ve dördüncü köşenin koordinatını yazar. Netcad'in 4.Köşeyi Oluştur aracının
işidir. Örneğin (0,0), (10,0) ve (10,6) verilince dördüncü köşe (0,6) olur.

### Bina dik açılıysa: `dik=evet`

Ölçülmüş üç köşe hiçbir zaman tam dik açılı değildir; şeridin ve prizmanın payı vardır.
Bina dikdörtgense paralelkenar çizmek, karşı duvarı ölçüm hatası kadar yatık bırakır.
`dik=evet` bunu düzeltir: birinci kenar olduğu gibi kalır, **üçüncü köşe ikinci köşeden
geçen dik doğruya çekilir**, dördüncü köşe de dikdörtgeni tamamlar. Çizilen alan ölçülen
üçüncü köşeden değil, çekilen köşeden geçer.

Komut bunu sessizce yapmaz; **sapmayı söyler**: üçüncü köşe kaç metre kaydı ve ikinci
köşedeki açı dik açıdan ne kadar sapmıştı. 3 cm'lik bir sapma şerit payıdır; 30 cm'lik
bir sapma yanlış okunmuş bir köşedir. Hangisi olduğuna siz karar verirsiniz — komut bir
eşik koymaz, yalnız sayıyı verir.

Açının sapması oturumun açı biriminde yazılır (varsayılan grad; bkz.
[Oturum modları](mode.md)). İşareti artıysa ölçülen açı dik açıdan büyüktü, eksiyse küçüktü.
`dik=evet`, [`DİKDÖRTGEN yontem=3n`](rectangle.md)'in üçüncü noktayı kenarın dikine
izdüşürmesiyle aynı hesabı yapar: fare kılavuzu da çizilecek dikdörtgenin kendisidir.

## Adlar

| Ad | Tür |
|---|---|
| `DÖRDÜNCÜKÖŞE` | Türkçe, birincil |
| `DORDUNCUKOSE` | ASCII karşılık |
| `FOURTHCORNER` | İngilizce karşılık |
| `DKÖ` | Kısaltma |
| `DKO` | Kısaltma, ASCII |
| `core.fourth_corner` | Komut kimliği |

## Sözdizimi

```text
DÖRDÜNCÜKÖŞE <birinci> <ikinci> <üçüncü> [dik=evet]
DÖRDÜNCÜKÖŞE noktalar=<birinci> <ikinci> <üçüncü> [dik=evet]
```

Nokta yazımı [`ÇİZGİ`](line.md) ile aynıdır: mutlak koordinat (`485320,4310220`), göreli
(`@10,0`, her biri bir öncekinden) ve kutupsal (`@100<45`).

## Parametreler

| Parametre | Zorunlu | Anlamı |
|---|---|---|
| `noktalar` | evet | Üç köşe, sırayla. İkinci köşe birinci ile üçüncünün arasındadır; dördüncü onun karşısına düşer |
| `dik` | hayır | `evet`: ikinci köşedeki açıyı dik yapar; üçüncü köşe birinci kenarın dikine çekilir ve sapma yazılır. Varsayılan `hayır`: üç köşenin paralelkenarı |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Üç köşe, (0,0), (10,0) ve (10,6):

```
DÖRDÜNCÜKÖŞE 0,0 10,0 10,6
```

Çıktı:

```text
Dördüncü köşe: Y 0,000 m  X 6,000 m; dört köşeli alan çizildi.
```

Üçüncü köşe ikincinin tam üstünde değilse dördüncü köşe aynı yana yatar:

```
DÖRDÜNCÜKÖŞE 0,0 10,0 14,6
```

```text
Dördüncü köşe: Y 4,000 m  X 6,000 m; dört köşeli alan çizildi.
```

Köşeleri bir öncekinden göreli de verebilirsiniz; her `@` bir önceki köşeye göredir:

```
DÖRDÜNCÜKÖŞE 100,200 @10,0 @0,6
```

```text
Dördüncü köşe: Y 100,000 m  X 206,000 m; dört köşeli alan çizildi.
```

Gerçek koordinatlarla, 18 m × 11,5 m bir bina — üçüncü köşe 3 cm yana kaçmış ve bina
dikdörtgen olmalı:

```
DÖRDÜNCÜKÖŞE 485320.15,4310220.4 485338.15,4310220.4 485338.18,4310231.9 dik=evet
```

```text
Dördüncü köşe: Y 485320,150 m  X 4310231,900 m; dört köşeli alan çizildi.
Dik açı dayatıldı: üçüncü köşe 0,030 m kaydırıldı (ikinci köşedeki açı 100,1661 grad ölçülmüştü, sapma +0,1661 grad).
```

Aynı köşeler `dik=evet` olmadan verilse dördüncü köşe `Y 485320,180 m` çıkardı: paralelkenar,
üçüncü köşedeki 3 cm'i karşı duvara taşırdı. Üç köşe zaten dik açılıysa komut bunu da söyler:

```text
Dik açı: üçüncü köşe zaten dik açının üzerinde; sapma yok.
```

### Arayüz

**Ctrl+K** ile komut aramayı açıp `dördüncü` yazın ya da komut satırına `DÖRDÜNCÜKÖŞE`
yazıp **Enter**'a basın. Üç köşeyi sırayla tıklayın. İlk tıklamadan sonra fareyi bir
çizgi izler; ikinci tıklamadan sonra üç köşenin oluşturduğu üçgen izler — dördüncü köşe,
üçüncüyü tıklayınca görünür ve transkriptte yazılır. Nesne yakalama açıkken tıklama köşeye
ve noktaya oturur; yazılan koordinat yazıldığı yere düşer.

Dik açı için komutu `DÖRDÜNCÜKÖŞE dik=evet` diye başlatın. Bu durumda üçüncü tıklamada
fareyi izleyen şekil, çizilecek **dikdörtgenin** kendisidir. **Esc** hiçbir şey çizmeden
çıkar; hiçbir iz bırakmaz.

### Betik

```json
{
  "ad": "Dördüncü köşe",
  "komutlar": [
    { "cmd": "core.fourth_corner",
      "args": { "noktalar": [[0, 0], [10000, 0], [10030, 6000]], "dik": true } }
  ]
}
```

Betikte koordinatlar **milimetredir**. Python'da adı `fourth_corner`'dır ve parametreleri
`points` ile `right_angle`'dır.

### Üçü de aynı

Tuvalde tıklanan, komut satırına yazılan ve betikte verilen aynı üç köşe aynı alanı ve
aynı günlük satırını bırakır.

## Yapılandırılmış cevap

Komut yazdığı cümlelerin yanında cevabını veri olarak da verir; bir betik ya da ajan
Türkçe cümleyi okumak zorunda kalmaz:

```json
{ "dorduncu": [0, 6000],
  "koseler": [[0, 0], [10000, 0], [10000, 6000], [0, 6000]],
  "dik": true,
  "sapma_mm": 30,
  "kayma_mm": [-30, 0],
  "olculen_aci_udeg": 90286477,
  "aci_sapmasi_udeg": 286477,
  "olculen_aci_metin": "100,3183 grad",
  "aci_sapmasi_metin": "+0,3183 grad" }
```

`dorduncu` ve `koseler` her zaman vardır; `koseler` çizilen dört köşedir (`dik=evet` iken
üçüncüsü çekilmiş hâliyle). `sapma_mm`, `kayma_mm` (üçüncü köşenin doğu ve kuzey yönünde
kaydığı miktar) ve açı alanları yalnız `dik=evet` iken yazılır. Açılar mikroderece olarak
ve oturumun biriminde metin olarak verilir.

## Geri alma

Bir `DÖRDÜNCÜKÖŞE` çağrısı **tek** geri alma adımıdır: [`GERİAL`](undo.md) alanın
tamamını kaldırır. Yarım kalmış bir alan belgeye hiç yazılmaz.

## Betikten kullanım

Betiklenebilir ve yapay zekâ erişimlidir. Günlüğe **tıklanan üç köşe** yazılır, çizilen
dört köşe değil — `dik=evet` iken ölçülen üçüncü köşe, çekilmiş hâli değil: günlük yeniden
oynatıldığında aynı hesap aynı dördüncü köşeyi ve aynı sapmayı üretir. `dik` günlük
satırında yalnız `evet` iken bulunur; `dik=hayır` varsayılanı yazmakla aynıdır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Yan yana iki köşe aynı nokta; bir kenar tanımlamıyor.` | Birinci ile ikinci ya da ikinci ile üçüncü köşe aynı yerde | Köşeleri ayrı noktalar olarak verin |
| `Üç köşe bir doğru üzerinde; dördüncü köşe bir alan kapatmaz.` | Üç köşe aynı doğru üzerinde; paralelkenarın alanı sıfır | Krokiyi denetleyin: bir köşe yanlış yazılmış olabilir |
| `Üçüncü köşe birinci kenarın doğrultusunda; dik açı bir alan kapatmaz.` | `dik=evet` ve üçüncü köşe birinci kenarın doğrusunun üzerinde: dik çekilecek bir uzaklık yok | Üçüncü köşeyi kenarın yanına alın ya da `dik=evet` yazmayın |
| `'core.fourth_corner': 'noktalar' parametresi en az 3 değer istiyor, 2 değer geldi.` | Üçten az köşe verildi | Üç köşe verin |
| `'core.fourth_corner': zorunlu 'noktalar' parametresi eksik. Beklenen: nokta listesi` | Komut satırında ya da betikte hiç köşe verilmedi | `noktalar=` ile üç köşe verin |
| `'core.fourth_corner': 'dik' parametresi evet/hayır bekliyor. Girilen: …` | `dik` için evet ya da hayır dışında bir değer, ya da üç köşeden sonra dördüncü bir nokta yazıldı | `dik=evet` ya da `dik=hayır` yazın; dördüncü köşeyi komut hesaplar, siz vermezsiniz |
| `'PARSEL' katmanı kilitli.` | Etkin katman kilitli | [`KATMAN`](layer.md) ile kilidi açın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili sayfalar

- [`DİKDÖRTGEN`](rectangle.md) — iki köşeden, bir kenar ve derinlikten ya da ölçüsünden dikdörtgen
- [`ALAN`](area.md) — dört köşeyi tek tek vermek, çok köşeli ve delikli alanlar
- [`DİKAYAK`](perp_offset.md) — dik ayak ve dik boyla nokta koymak: cephe alımının öteki yarısı
- [Sözlük](../sozluk.md) — dördüncü köşe, derinlik
