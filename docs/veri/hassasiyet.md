# Sayısal Doğruluk ve Toleranslar

Harita mühendisi ve planlamacı için; bu sayfayı bitirdiğinizde KentOSCad'de hangi
sayının neye karar verdiğini, hangisini değiştirebileceğinizi ve bir sonucun ne kadar
doğru olduğunu bileceksiniz.

## Beş ayrı büyüklük

Bir CAD/CBS programında "tolerans" sözcüğü beş ayrı şeyi anlatır. KentOSCad bunları
birbirine karıştırmaz: her birinin tek bir yeri, tek bir birimi ve tek bir görevi vardır.

| Büyüklük | Neye karar verir | Değer | Nereden değişir |
|---|---|---|---|
| Saklama çözünürlüğü | Bir koordinatın saklandığı en küçük adım | 1 mm | Değişmez |
| Hesap toleransı | Hesaplanan iki şeyin ne zaman aynı şey sayıldığı | 0,5 – 2 mm | Değişmez |
| Ekrandaki yakalama yarıçapı | Fareyle nişan alınan noktanın hangi köşeye yakalandığı | 16 piksel | `TERCİH yakalama_toleransı` |
| Topoloji düğüm toleransı | Birbirine yakın iki köşenin tek düğüm sayılıp sayılmadığı | 10 mm | `AYAR düğüm_toleransı` |
| Aktarımda eğri sapması | Eğri taşımayan bir dosyaya yazılan kirişlerin eğriden ne kadar uzak durabileceği | 1 mm | `AYAR eğri_sapması` |

Bunlara bir de koşul eklenir. Çizimin koordinat sistemi koordinatlarını **metre** olarak
saymalıdır. Bkz. [Koordinat sisteminin birimi](koordinat-sistemleri.md#koordinat-sisteminin-birimi-yalnız-metre).

## Saklama: milimetre

Her koordinat 64 bitlik bir tam sayı olarak, **milimetre** cinsinden saklanır. Yazdığınız
metre değeri bir kez, yarımdan uzağa yuvarlanarak milimetreye çevrilir:

| Yazılan | Saklanan |
|---|---|
| `485320.150` | 485 320 150 mm |
| `485320.1504` | 485 320 150 mm |
| `485320.1505` | 485 320 151 mm |

Saklanan bir değer hiçbir zaman "yaklaşık" değildir. Aynı çizim her bilgisayarda aynı
alanları ve aynı kesişimleri verir.

**Büyük koordinatlar milimetreyi kaybetmez.** TUREF/TM koordinatları yedi basamaklıdır;
iki noktanın arasındaki 1 mm'lik fark, 4 310 220 m'lik yukarı değerde de 1 mm kalır.
Uzunluk ve alan hesabının ara çarpımları 128 bitte yapılır. Bu yüzden bir parselin alanı
TM30 koordinatlarında da milimetrekaresine kadar kesindir.

Bir halkanın alanı en çok **4,6 milyon km²** olabilir; bu, Türkiye'nin altı katıdır. Daha
büyük bir halka saklanmaz ve bunun nedeni söylenir. Böyle bir halka neredeyse her zaman
birimi yanlış okunmuş bir dosyadan gelir: milimetre diye okunan metreler uzunluğu bin,
alanı bir milyon kat büyütür.

### Neler sınandı

Bir parselin 1 mm × 1 mm'lik köşesi, 1 mm'lik bir çizgi ve 1 mm arayla iki nokta TM30
koordinatlarında (sağa 485 320 m, yukarı 4 310 220 m) şu adımların her birinden
**bit bit aynı** çıkar: bin kilometre uzağa taşıyıp geri getirmek, çeyrek tur döndürüp
geri çevirmek, kaydedip açmak, DXF'e ve GeoPackage'a yazıp geri okumak. Alan her adımda
1 mm², çevre 4 mm, çizgi 1 mm'dir. TM30'dan TM33'e ve geri dönüştürmek her koordinatı her
adımda bir kez milimetreye yuvarlar; noktalar yerlerine en çok 1 mm yakın döner.

## Milimetre altı: karar

KentOSCad **milimetre** çözünürlükte kalır; saklama biçimi değişmez ve bir biçim göçü
gerekmez. Karar bir örnek çizim üzerinde verildi: milimetre biriminde çizilmiş, saklama
çözünürlüğünden ince bir detay (`tests/fuzz/tohum/dxf/29-milimetre-alti.dxf`).

| Çizimdeki | KentOSCad'de |
|---|---|
| 12,345 mm'lik çizgi | 12 mm |
| Çizgiden sonra 0,3 mm'lik boşluk | 1 mm |
| 0,4 mm yarıçaplı daire | Okunmaz; atlanır ve bu söylenir |
| 2,5 mm yüksekliğinde yazı | 3 mm |

İçe aktarma bunu her seferinde söyler:

```text
not: 4 değer milimetrenin altında ayrıntı taşıyordu; KentOSCad milimetre çözünürlükte
saklar ve bunları en çok 0,50 mm kaydırarak yuvarladı. Milimetreden küçük bir ayrıntı
bu çözünürlükte kaybolur.
```

Kayıp yalnız milimetreden küçük **ayrıntılarda** ortaya çıkar: bir makine parçası ya da
milimetre biriminde çizilmiş bir mimari detay. Metre biriminde, milimetresine kadar
çizilmiş bir harita ya da kadastro paftası hiçbir şey kaybetmez ve bu not görünmez.
KentOSCad'in işi haritacılık, kadastro, imar ve arazi işidir; bu işlerde milimetre
yeterlidir.

Daha ince bir çözünürlük (mikrometre) bugünkü aralığı taşıyabilirdi; ama kaydedilmiş her
çizimin, her günlüğün ve her altın örneğin sayılarını değiştirirdi. Bunu haklı çıkaracak
bir iş bugün yoktur. Böyle bir gereksinim çıkarsa sürümlü bir biçim göçüyle gelir.

## Hesap: milimetreden türeyen eşikler

Bir kesişim, bir teğet ya da bir bölme noktası hesaplandığında sonuç milimetreye
yuvarlanır. Hesaplanan şeylerin karşılaştırılması da bu yüzden saklama çözünürlüğünden
türeyen sabit eşiklerle yapılır. Bunlar ayar değildir, çünkü birer tercih değil,
aritmetiğin özellikleridir:

| Eşik | Anlamı |
|---|---|
| 0,5 mm | Hesaplanan bir nokta bir eğrinin **üzerindedir**. Yarım milimetre içindeki iki koordinat aynı milimetreye yuvarlanır |
| 1 mm | Hesaplanan iki nokta **aynı noktadır**. İki yandan bulunan bir kesişim ya da bir kesimin üstüne yapılan tıklama böyle birleşir |
| 1,5 mm | Bir dosyadan okunan köşeler bir **daireye oturur**. Yuvarlanan her köşede en çok 0,7 mm sapma bulunur |
| 2 mm | Saklanan bir yayın merkezi ile iki ucu **birbiriyle tutarlıdır**. Üçü de ayrı ayrı yuvarlanmıştır |

**Uzunluk ve alan eğrinin kendisinden ölçülür, ekrandaki kirişlerden değil.** Bir elipsin
ya da spline'ın boyu eliptik integralin ve spline eğrisinin sabit bir Gauss kuralıyla
toplanmasıdır. Kapalı bir spline'ın alanı da eğrinin çevrelediği alandır. Ekranın çizim
sıklığı ölçülen hiçbir sayıyı değiştirmez.

## Ekran: piksel

Yakalama yarıçapı `yakalama_toleransı` (varsayılan 16 piksel) ve seçme kutusu
`seçim_toleransı` (varsayılan 6 piksel) **ekran pikselidir**. Göz ekrana bakarak nişan
alır; bir pikselin zemindeki karşılığı yakınlaştırmayla değişir, bu yüzden tolerans da
değişir.

Yakalama **yalnız fareyle nişan aldığınız noktaya** uygulanır. Komut satırına yazdığınız,
bir betikte ya da bir yapay zekâ önerisinde gelen koordinat yazıldığı yere düşer:

```text
ÇİZGİ 485320.150,4310220.400 485330,4310230
```

Bu çizgi, ekranda ne kadar uzaklaşmış olursanız olun ve yakında hangi köşe bulunursa
bulunsun, tam `485320.150,4310220.400`'den başlar. Ayrıntı:
[Komut satırı](../komutlar/komut-satiri.md).

## Topoloji: düğüm toleransı

`düğüm_toleransı` (varsayılan 10 mm) birbirine bundan yakın iki köşenin **tek düğüm**
sayıldığı mesafedir. SINIR, ALANÜRET, TEMİZLE, TOPOLOJİ, BİRLEŞTİR ve ALANAÇEVİR bu değeri
kullanır. Bir proje ayarıdır: bir ifrazın ya da bir topoloji denetiminin sonucunu
değiştirir ve dosyayla birlikte gider.

```text
AYAR düğüm_toleransı 20
```

Ekranın yakalama yarıçapıyla ilgisi yoktur: bir sınırın kapanıp kapanmadığı,
yakınlaştırmaya göre değişmez.

## Dışa aktarma: eğri sapması

GeoPackage ve PostGIS eğri taşımaz; daire, yay, elips, yaylı çizgi ve spline bu
dosyalara **kirişlere kırılarak** yazılır. `eğri_sapması` her kirişin eğriden en çok ne
kadar uzak durabileceğidir (varsayılan 1 mm, en çok 1000 mm):

```text
AYAR eğri_sapması 5
```

Dışa aktarma sonucu kaç eğrinin kırıldığını ve kirişlerin eğriden en çok ne kadar uzakta
kaldığını söyler:

```text
2 eğri (daire, yay, elips, yaylı çizgi, spline) GPKG eğri taşımadığı için kirişlere
kırılarak yazıldı; kirişler eğriden en çok 0,99 mm uzakta (AYAR eğri_sapması 1 mm;
noktalar ayrıca milimetreye yuvarlanır).
```

DXF eğriyi eğri olarak yazar: `CIRCLE`, `ARC`, `ELLIPSE`, `SPLINE` ve şişkinlikli
`LWPOLYLINE`. Orada bu ayarın bir etkisi yoktur.

**Ekrandaki çizim bundan ayrıdır.** Tuval her daireyi sabit bir sıklıkla, 128 kirişle
çizer; bu bir resimdir ve dosyaya gitmez:

| Yarıçap | Ekranda kirişin sapması | GeoPackage'da (varsayılan 1 mm ile) |
|---|---|---|
| 5 m | 1,5 mm | en çok 1 mm |
| 50 m | 15 mm | en çok 1 mm |
| 300 m | 90 mm | en çok 1 mm |

## Hata mesajları

| Mesaj | Neden | Çözüm |
|---|---|---|
| `… halka saklanamayacak kadar büyük: alanı 4,6 milyon km²'yi aşıyor …` | Halkanın alanı milimetrekare çözünürlükte tam yazılamayacak kadar büyük; çoğu zaman yanlış okunmuş bir birim | Dosyanın birimini denetleyin (`AYAR çizim_birimi`) ve yeniden aktarın |
| `Nesnenin halkalarının toplam alanı 4,6 milyon km²'yi aşıyor …` | Bir nesnenin parçalarının toplamı sınırı aşıyor | Aynı neden ve çözüm |
| `'core.aktarim.egri_sapmasi' için 0 değeri [1, 1000] aralığının dışında; 1 değerine kırpıldı.` | Eğri sapması 1 mm'den küçük istendi | Hata değildir: saklama çözünürlüğünün altında bir sapma istenemez |

## Henüz gelmemiş olanlar

| Yetenek | Ne zaman |
|---|---|
| Elips ve spline üzerinde yakalamanın ekrandaki kirişlerden değil eğrinin kendisinden yapılması | Faz 1 |
| Kırpıntı eşiğinin (`en_küçük_alan`) topoloji denetiminde kullanılması | Faz 1 |

## İlgili

- [Koordinat sistemleri](koordinat-sistemleri.md) — milimetre depolama, TM dilimleri, birim
- [Dış veri biçimleri](dis-formatlar.md) — hangi biçimin neyi taşıdığı
- [AYAR](../komutlar/setting.md) — proje ayarları
- [Komut satırı](../komutlar/komut-satiri.md) — yazılan koordinatın kesinliği
