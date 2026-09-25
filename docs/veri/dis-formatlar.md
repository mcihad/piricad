# Dış Veri Biçimleri

Başka bir programdan veri alan ya da başka bir kuruma veri teslim eden kullanıcı
için; bu sayfayı bitirdiğinizde hangi biçimlerin okunup yazıldığını, neyin
aktarıldığını, neyin aktarılmadığını ve koordinat sisteminin nasıl taşındığını
bileceksiniz.

Komutlar: [İÇEAKTAR](../komutlar/import.md), [DIŞAAKTAR](../komutlar/export.md).
Kendi proje dosyanız için: [KentOSCad proje dosyası](proje-dosyasi.md).

## Bu sürümde çalışan biçimler

| Biçim | Uzantı | Okuma | Yazma |
|---|---|---|---|
| AutoCAD DXF | `.dxf` | evet² | evet² |
| AutoCAD DWG | `.dwg` | evet¹ | **hayır** — aşağıya bakın |
| ESRI Shapefile | `.shp` | evet | **hayır** — aşağıya bakın |
| OGC GeoPackage | `.gpkg` | evet | evet |

¹ DWG okuma bir yapı seçeneğidir ve **kapalı gelir**: LibreDWG'nin okuyucusu bu
sürümde altı nesne türünü tanır ve derlemesi kendi uyarılarını taşır. Açmak için
`-DKENTOS_WITH_DWG=ON` ile yeniden yapılandırın; ağa çıkamayan bir yapıda
(`KENTOS_FETCH_DEPENDENCIES=OFF`) kaynak indirilemediği için açılamaz. Kapalıyken
bir `.dwg` açmaya çalışmak ne yapmanız gerektiğini yazan bir hata verir; içe
aktarma penceresi de DWG yerine DXF kaydedip getirmenizi söyler.

² DXF **libdxfrw** ile okunur ve yazılır (`KENTOS_WITH_DXFRW`, kaynak indirilebilen
her yapıda açık gelir). libdxfrw dosyayı grup kodu düzeyinde okur: daire daire, yay yay,
elips elips, blok referansı açılmış üyeleriyle, XDATA baytıyla gelir; yazarken de her
tür kendi DXF varlığı olarak gider. Kütüphane kapalıysa (`-DKENTOS_WITH_DXFRW=OFF`)
DXF GDAL'ın sürücüsüyle okunur ve yazılır; o yol eğrileri parçalar, blokları ve
XDATA'yı düşürür ve bunu transkriptte söyler.

### DWG okunur, yazılmaz

KentOSCad DWG'yi **LibreDWG** ile okur — var olan tek GPL uyumlu DWG
uygulamasıdır. r13'ten 2018'e kadar bütün sürümler okunur.

Yazma yok, ve iki ayrı sebeple:

- Yerel bir DWG yazıcısı, hangi varlık türlerinin ne oranda okunduğunu ölçen bir
  kapsam raporu çıkarılmadan açılmayacak. Yanlış yazılmış bir DWG, teslim
  edildiği yerde açılmaz.
- Kütüphane bu yapıda **yazma kodu olmadan** derleniyor
  (`LIBREDWG_DISABLE_WRITE`), yani bu bir söz değil ikilinin bir özelliği.

DWG çıktısı gerekiyorsa **DXF** olarak dışa aktarın; her CAD programı okur.

KentOSCad **hiçbir zaman** ODA Drawings SDK kullanmayacaktır: kapalı kaynaklıdır
ve projenin GPLv3 lisansıyla bağdaşmaz.

### DWG'de ne okunur

| Okunan | Okunmayan |
|---|---|
| `LINE`, `LWPOLYLINE` (kapalıysa **alan**) | Bloklar (`INSERT`) — parçalanmadan atlanır |
| `POLYLINE` — eski usul çoklu çizgi, kapalıysa **alan** | Ölçülendirme (`DIMENSION`) |
| `POINT` — nirengi, poligon noktası, röper | Tarama (`HATCH`) |
| `TEXT` — ada ve parsel numaraları, yüksekliğiyle | Kâğıt alanı (layout) — çizim değildir, alınmaz |
| `CIRCLE` ve `ARC` — **gerçek daire ve yay olarak**, çizgiye bölünmeden | Katman rengi ve çizgi tipi |
| Katman adları ve her katmandaki nesne sayısı | |

Okunamayan bir varlık türüyle karşılaşılırsa **adıyla ve sayısıyla** bildirilir.
Sessizce düşürülmez.

**Eski usul `POLYLINE` de okunur**, ve bu ayrı bir satırı hak ediyor: AutoCAD
`LWPOLYLINE` ortaya çıkmadan önce on yıl boyunca `POLYLINE` yazdı ve o dönemden
gelen her dosya hâlâ onu taşır. Köşeleri nesnenin içinde değil, ayrı `VERTEX`
nesneleri olarak durur ve zinciri bir `SEQEND` kapatır. Bir kadastro çiziminde
parsel sınırlarının tamamı bu türde olabilir — 48 MB'lık örnek çizimde 12 013
`POLYLINE`'a karşılık tek bir `LWPOLYLINE` bile yok.

Daire ve yay her iki yolda da **gerçek daire ve yay** olarak gelir. DWG yolunda
LibreDWG onları öyle verir; DXF yolunda libdxfrw da öyle verir. Yalnız libdxfrw
kapalı derlenmiş bir yapıda GDAL çizgi parçalarına böler ve KentOSCad merkezle
yarıçapı geri kurar — nasıl olduğu aşağıda.

### Shapefile dört dosyadır

Bir shapefile tek dosya değildir. Dördü birlikte taşınır:

| Dosya | İçindekiler | Gerekli mi |
|---|---|---|
| `.shp` | geometri | evet |
| `.shx` | geometri dizini | **evet** |
| `.dbf` | öznitelik tablosu | **evet** |
| `.prj` | koordinat sistemi | yoksa çizimin kendi sistemi varsayılır |

Biri eksikse KentOSCad hangisinin eksik olduğunu ve ne işe yaradığını söyleyip
durur. Size yalnız `.shp` gönderildiyse dosyayı gönderene **dördünü birden**
isteyin — eksik bir set açılamaz.

### Neden yazma yok

Bir shapefile dosya başına **tek bir geometri türü** tutar. Parselleri, sınırları,
nirengileri ve parsel numaralarını birlikte taşıyan bir çizim tek bir `.shp`'ye
yazılamaz; GDAL ilk öğeden sonrasını reddeder. Bir çizimi birden çok dosyaya nasıl
böleceğimiz ve onları nasıl adlandıracağımız ayrı bir karar, ve o karar verilene
kadar çalışmayan bir düğme koymaktansa düğmeyi koymuyoruz.

Teslim için **DXF** ya da **GeoPackage** kullanın; ikisi de çizimin tamamını
tutar.

Bu liste kasten kısadır. KentOSCad'in altındaki GDAL kütüphanesi yüzden fazla biçim
tanır; KentOSCad bunların yalnızca **açıkça izin verilenlerini** açar. Bir dosya
biçimi, üzerinde sınanmamış bir ayrıştırıcı demektir ve dosya okumak bu ürünün en
geniş saldırı yüzeyidir.

Yeni bir biçim eklenmesi, o biçimin fuzz koşumu, gidiş-dönüş sınaması ve bu
sayfada bir satırı ile birlikte gelir.

## Henüz gelmemiş olanlar

| Biçim | Ne zaman | Neden şimdi değil |
|---|---|---|
| DXF çizgi tipleri (LTYPE), tarama ailelerinin kesik dizisi, yaylı ve spline kenarlı tarama sınırı | Faz 2 | Çizgi tipi ve kesikler düz çizilir ve söylenir; eğri sınırlar çizgi parçalarına bölünür |
| DWG (okuma) | Faz 2 | Önce 50+ gerçek dosyalık bir kapsam raporu çıkarılacak; hangi varlık türlerinin ne oranda okunduğu ölçülmeden açılmayacak |
| DWG (yazma) | Planlanmıyor | DWG çıktısı DXF dışa aktarıp dönüştürerek üretilir |
| PlanGML | Faz 2 | Yazmadan önce XSD ile yerinde doğrulanması gerekiyor; e-Plan yüklemesinde reddedilen bir dosya üretmek kabul edilemez |
| LAS / LAZ | Faz 2 | Nokta bulutu görüntüleme boru hattıyla birlikte gelecek |
| Shapefile (yazma) | Faz 1 | Bir çizimin birden çok dosyaya nasıl bölüneceğine karar verilmesi gerekiyor |
| GeoJSON | Faz 1 | İzin listesine eklenmesi için fuzz koşumu ve gidiş-dönüş sınaması gerekiyor |
| WMS, WMTS, WFS-T, WCS | Faz 2 | Servis istemcileri kendi uygunluk sınamalarıyla gelecek |

KentOSCad **hiçbir zaman** ODA Drawings SDK kullanmayacaktır; kapalı kaynaklıdır ve
projenin GPLv3 lisansıyla bağdaşmaz.

## Koordinat sistemi

### DXF koordinat sistemi taşımaz

Bir DXF'in içinde koordinat sistemi için **yer yoktur**, ve hiçbir harita bürosu
yanına `.prj` koymaz. Böyle bir dosyayı açtığınızda KentOSCad **çizimin kendi
sistemini** varsayar ve bunu açıkça söyler:

> Dosya koordinat sistemi bildirmiyor (DXF taşıyamaz). Çizimin kendi sistemi
> varsayıldı: EPSG:5256.

Doğru sistemi **önceden** kurmak sizin işiniz:

```text
AYAR koordinat_sistemi EPSG:5256
İÇEAKTAR "pafta.dxf"
```

Yanlış sistemle aktardıysanız `GERİAL` ile geri alın, doğrusunu kurun ve yeniden
aktarın. Koordinatlardan sistem **tahmin edilmez**: 583 000 gibi bir sağa değeri
Türkiye'de birden çok TM dilimine uyar, ve ikisi arasında tahmin yürütmek bu
kuralın var olma sebebi olan hatanın ta kendisidir.

**Etiketsiz koordinat sessizce okunmaz.** Koordinat sistemini bildirmeyen bir veri
kümesi (yanında `.prj` olmayan bir Shapefile, koordinat sistemi taşımayan bir DXF)
çizimin kendi sistemiyle okunur ve bu varsayım her seferinde **uyarı** olarak söylenir.
Program koordinatlardan dilim tahmin etmez.

Sebebi saha kökenlidir: TM30 ile TM33 karışması sessizdir. Koordinatlar makul
görünür, çizim makul görünür, ve hata ancak tapuya gittiğinde ortaya çıkar.

**Birim de denetlenir.** Çizim metre sayan bir sistemde saklanır. Koordinatlarını derece
(WGS 84 gibi coğrafi sistemler) ya da başka bir birimle sayan bir katman **reddedilir**
ve ret mesajı dosyayı metre sayan bir sisteme dönüştürmenin yolunu söyler. Ayrıntı:
[Koordinat sisteminin birimi](koordinat-sistemleri.md#koordinat-sisteminin-birimi-yalnız-metre).

| Biçim | Koordinat sistemini nasıl taşır |
|---|---|
| GeoPackage | Dosyanın içinde. Ek bir şey gerekmez |
| DXF | **Taşımaz.** Yanındaki aynı adlı `.prj` dosyasından okunur |

DXF'in koordinat sistemi için yeri yoktur — bu biçimin kendi eksiğidir, KentOSCad'in
değil. Bu yüzden:

- **Dışa aktarırken** KentOSCad `.dxf` ile birlikte bir `.prj` dosyası yazar ve size
  söyler. Çizimi taşırken **iki dosyayı da götürün**.
- **Yalnız DXF metre yazıldıysa.** `.prj` metre sayan bir sistem bildirir ve bir CBS
  programı DXF'in sayılarını bu yüzden metre okur. `çizim_birimi` milimetre ya da
  santimetre iken yazılan DXF'in yanına `.prj` **konmaz**: konsaydı bir CBS programı
  çizimi bin kat uzağa koyardı. Sonuç bunu söyler. Koordinat sistemini taşıyan bir DXF
  için `AYAR çizim_birimi metre` ile yeniden dışa aktarın.
- **İçe aktarırken** KentOSCad aynı adlı `.prj` dosyasını arar. Yoksa çizimin
  kendi sistemini varsayar ve bunu transkriptte açıkça söyler; koordinatlardan
  bölge tahmin etmez. `.prj` varsa ama çizim birimi metre değilse, DXF sizin
  ayarınızla okunur ve bu çelişki bir uyarıyla söylenir.

`.prj`, ülkedeki her CBS yazılımının anladığı ESRI biçiminde yazılır.

İçe aktarılan verinin koordinat sistemi çizimin kendi sisteminden farklıysa
KentOSCad **koordinatları dönüştürmez**; farkı söyler ve kararı size bırakır.
Sessiz bir yeniden projeksiyon, yanlış yere oturmuş bir parselin en kolay yoludur.

Çizimin koordinat sistemini `AYAR koordinat_sistemi` ile bildirin:

```
AYAR koordinat_sistemi EPSG:5254
```

## Ne aktarılır, ne aktarılmaz

| Aktarılan | Eksik alınan ya da alınmayan (her biri transkriptte söylenir) |
|---|---|
| Çizgi ve alan geometrisi, milimetre hassasiyetiyle | Çizgi tipi (LTYPE): bu sürümde uygulanmaz, çizgiler düz; söylenir |
| Yazı stilinin istediği yazı tipi, raporda — `romans.shx (12 yazı)` | Yazı tipinin kendisi: bütün yazılar IBM Plex Sans ile çizilir; bu programda olmayan her yazı tipi kaç yazının istediğiyle birlikte söylenir |
| **Ölçülmüş noktalar** — nirengi, poligon noktası, röper | Yazı tipi; TEXT'in alt hizası ile yaslanmış/sığdırılmış yazı en yakın hizayla çizilir, söylenir |
| **Yazılar**, yüksekliği, **açısı**, dokuz hizası, **satırları**, satır aralığı ve kırılma genişliğiyle ([yazı nesnesi](../nesneler/yazi.md)); çok satırlı yazı MTEXT olarak yazılır; MTEXT biçim kodları soyulur | Yalnız uydurma noktası taşıyan spline: uydurma noktaları kontrol noktası sayılır, `düşürme:` ile söylenir |
| **Daire, yay, elips ve kısmi elips** — gerçek eğri olarak; GeoPackage'a çokgen olarak gider, geri okunuşta eğri olur | Ne katalogda olan ne de kendi çizgilerini taşıyan tarama deseni: sınır, ad, açı korunur, desen çizilmez, söylenir |
| **Yaylı çoklu çizgi** (şişkinlik): her yay merkezi ve yarıçapıyla; **spline**: derece, düğüm, ağırlık | Kenar boyunca değişen çoklu çizgi kalınlığı |
| **Blok tanımları ve referansları** (`BLOCK`/`INSERT`): yapısıyla, ölçek, açı, ayna, dizi; iç içe | Anonim bloklar (`*D1`…): ölçünün kendi çizgileri, ölçü nesnesi zaten okunduğu için |
| [Dış referanslar](../komutlar/xref.md): DXF'e o anki içerikleriyle sıradan blok olarak yazılır, adlardaki `\|` `$0$` olur | DXF'teki dış referans bloğu (XREF): yolu okunmaz, **boş blok** olarak gelir ve adıyla söylenir — `DIŞREFERANS` ile bağlayın |
| [Kırpılmış blok referansları](../komutlar/block_clip.md): referansın kendisi `INSERT` olarak | Kırpma sınırı: AutoCAD onu bir `SPATIAL_FILTER` nesnesinde tutar, libdxfrw yazamaz; referans DXF'te **bütün** görünür ve dışa aktarma kaç referansın sınırının taşınmadığını söyler. İçe aktarılan DXF'teki AutoCAD kırpması (XCLIP) da okunmaz |
| **Tarama** (`HATCH`): sınır döngüleri, desen adı, açı, çizimin birimine göre ölçek ve **desen tanım çizgileri** (grup 78); dosyanın kendi çizgileri varsa aralık, açı ve başlangıç onlardan, yoksa aileler desen kataloğundan | Tarama sınırındaki yay ve spline kenarlar çizgi parçalarına bölünür; ikili DXF'te desen çizgileri okunmaz, katalog geçer |
| **Ölçü** (`DIMENSION`: hizalı, doğrusal, yarıçap, çap, açısal, ordinat) ve **kılavuz çizgi** (`LEADER`) | Ölçü stilinin dosyada olmaması: ISO-25 ölçüleri kullanılır, söylenir |
| **Çoklu kılavuz** (`MULTILEADER`): her kılavuz çizgisi okun ucundan inişin sonuna bir kılavuz çizgi, oku boyuyla; yazısı yeri, yüksekliği, açısı ve hizasıyla bir yazı nesnesi — tek satırlıysa ve inişin hizasındaysa kılavuzun ucuna **bağlı** | Çok satırlı, dönük ya da inişten ayrı yazı bağlanmaz, yerinde durur ve sayılır; blok tanımının içindeki çoklu kılavuz okunmaz. libdxfrw bu varlığı okumadığı için GDAL ile okunur: GDAL'sız bir yapı atlar ve söyler |
| **Katman adı, rengi, kalınlığı**, dondurulmuş/kapalı (görünmez) ve kilitli durumu | |
| **Nesnenin kendi rengi ve kalınlığı** (ACI ve gerçek renk); blok üyesinde ByBlock. ACI 7 ("beyaz/siyah", zemine uyan renk) siyah okunur, siyah ACI 7 yazılır | |
| Boşluklu ve çok parçalı alanlar | |
| Koordinat sistemi (`.prj` yan dosyasından) | Kâğıt alanı (layout), sonsuz doğru, bakış penceresi, raster resim, ağ: okunmaz, sayılır |
| **Yükseklik (Z)**: sabitse `kot` sütununa; köşeden köşeye değişiyorsa atılır ve söylenir | |
| **Kaynak tutamağı** (`kaynak_kimlik` sütunu) ve **XDATA** (bayt bayt, `ek_veri`) | |
| **Öznitelikler** — GeoPackage ve Shapefile'dan (`alanlar=`), GeoPackage'a; DXF'e `KENTOSCAD` XDATA olarak gider ve oradan geri gelir | |
| Nesne türü ve kalıcı anahtar — GeoPackage'a `tur` ve `anahtar` alanı olarak | |

Bu yüzden **çalışma dosyanız `.pcad` olmalıdır**. DXF ve GeoPackage teslim
biçimleridir; bir dışa aktarıp geri alma turu çiziminizi olduğu gibi geri
getirmez.

### DXF'te kapalı çizgi alandır

DXF'in poligonu yoktur: bir parsel, **kapalı bayrağı** açık bir `LWPOLYLINE` ya da
`POLYLINE`'dır. KentOSCad kapalılığı dosyanın bayrağından okur (bayrak yoksa, ilk
köşesi sonuncusuyla aynı olan çizgiyi de kapalı sayar) ve böyle bir çizgiyi
**alan** olarak alır — yoksa dosyadaki her parsel çizgi olarak gelir, dolgusu
olmaz, alanı ölçülemez ve [`İFRAZ`](../komutlar/split_parcel.md) ile
[`TEVHİT`](../komutlar/merge.md) üzerinde çalışamaz. `SOLID`, `TRACE` ve `3DFACE`
de alan olur.

### DXF nasıl okunur

KentOSCad bir DXF'i **libdxfrw** ile grup kodu düzeyinde okur. Her varlık kendi
türüyle gelir: `CIRCLE` daire, `ARC` yay (saat yönünün tersine süpürme, dosyadaki
yön korunarak), `ELLIPSE` elips ya da kısmi elips, `POINT` nokta, `TEXT` ve `MTEXT`
yazı, `LWPOLYLINE` ve eski usul `POLYLINE` çoklu çizgi, alan ya da — şişkinliği varsa
— [yaylı çoklu çizgi](../nesneler/yaylicizgi.md), `SPLINE`
[spline](../nesneler/spline.md), `HATCH` [tarama](../nesneler/tarama.md), `DIMENSION`
[ölçü](../nesneler/olcu.md), `LEADER` [kılavuz çizgi](../nesneler/lider.md). Katmanlar
rengiyle, kalınlığıyla, dondurulmuş/kapalı ve kilitli durumuyla kurulur; nesnenin
kendi rengi ve kalınlığı varsa stil olur.

`BLOCKS` bölümündeki her adlı **blok tanımı** çizimin blok tablosuna girer ve üyeleri
tanımın içinde, kendi koordinatlarıyla okunur; her `INSERT` bir
[blok referansı](../nesneler/blokreferansi.md) olur: taban noktası, ölçek (eksenlerde
ayrı ayrı, eksi ölçek ayna), dönme ve dizi (satır/sütun) referansın yükünde durur, iç
içe bloklar korunur. `0` katmanındaki üye referansın katmanında, rengi **ByBlock** olan
üye referansın renginde çizilir. Anonim bloklar (`*Model_Space`, `*D1`) okunmaz.

Bir nesnenin **düzlemi** (OCS, `210` grubu) dikkate alınır: kütüphane her noktayı
çizim düzlemine taşır, aynalı bir düzlemdeki (normal −Z) yayın yönü çevrilir. Eğik
bir düzlem düzleştirilir ve sayılır.

Her nesnenin dosyadaki **tutamağı** `kaynak_kimlik` sütununa, başka bir programın
bağladığı **XDATA** bayt bayt yanına yazılır; ikisi de dışa aktarımda geri gider.
Dosya `$DWGCODEPAGE` bildirmiyorsa uyarı verilir: 2007 öncesi bir dosyada Türkçe
harfler yanlış çıkabilir. Okuma ayrı iş parçacığında sürer ve **Durdur** ile kesilir.

### DXF birimi (`$INSUNITS`)

Bir DXF ve bir DWG **her zaman** [`AYAR çizim_birimi`](../komutlar/setting.md)
ayarındaki birimde okunur (milimetre, santimetre ya da metre; varsayılan metre) ve
koordinatlar milimetreye ölçeklenir. Dosyanın `$INSUNITS` başlığı bu ayarla
karşılaştırılır: başlık yoksa ya da 0 ise hangi birimin kullanıldığı **not**
olarak yazılır, başlık ayardan farklı bir şey diyorsa bu bir **uyarı** olur ve
dosyayı öteki birimde okuyan komut satırı uyarının içindedir. Başlık ayarın yerine
**geçmez**: başlığı dosyayı son kaydeden program yazar ve Türkiye'deki kadastro ve
imar DXF'lerinin çoğu sayıları metre iken "milimetre" der. Dışa aktarırken DXF
aynı ayarın biriminde yazılır ve `$INSUNITS` başlığa işlenir. GeoPackage ve
Shapefile'ın birimi koordinat sisteminin metresidir.

### Kâğıt alanı okunmaz

DXF ve DWG'nin layout'larındaki antet, pafta çerçevesi ve bakış pencereleri çizim
değildir; model alanına alınmaz. Kaç öğe atlandığı transkriptte yazılır.

### Yazı dosyadaki açıyla gelir

Bir DXF yazısı kendi açısını taşır ve plancı bunu kullanır: yol adı yol boyunca,
parsel ve ada numarası parsel boyunca yazılır. Açı okunmazsa hepsi yatay gelir ve
yola göre yazılmış bir etiket yolu keser — Suşehri imar planındaki 13 112 yazının
1 412'si dönüktür.

KentOSCad bir yazıyı **taban çizgisi artı bir metin** olarak tutar, ve yazının
yönü o taban çizgisinin yönüdür. Dosyadaki açı bu yüzden ayrı bir alana değil,
taban çizgisinin kendisine yazılır: yeni bir sütun gerekmez ve yazıyı çizen her
istemci dönük olanı da çizer.

### Daire daire, yay yay olarak gelir

libdxfrw yolunda `CIRCLE` merkez ve yarıçap, `ARC` merkez, yarıçap ve iki uç olarak
okunur; parçalama yoktur. 48 MB'lık Suşehri kadastro dosyasındaki 3 874 daire ve
3 523 yay bu yüzden daire ve yaydır: merkezleri, yarıçapları, πr² alanları ve
merkeze yakalamaları vardır.

libdxfrw kapalı derlenmiş bir yapıda GDAL yolu devreye girer ve GDAL eğriyi çizgi
parçalarına böler. O yolda nesnenin **ne olduğu dosyadan okunur**, şekle bakılarak
tahmin edilmez (`AcDbEntity:AcDbCircle` sınıf zinciri); yalnızca **sayılar** —
merkez ve yarıçap — parçalanmış noktalardan bütün köşelerin en küçük kareler
uydurmasıyla geri kurulur ve kurulan çember dosyadaki her noktaya 1,5 mm içinde
uymuyorsa kabul edilmez, nesne çoklu çizgi kalır. Bir haritacının elle çizdiği 64
kenarlı çokgen çokgen kalır, daireye dönüşmez.

Desteklenmeyen bir geometri türüyle karşılaşılırsa o öğe atlanır ve kaç tanesinin
atlandığı transkriptte söylenir. Sessizce düşürülmez.

## Yazma yarıda kalırsa

Bir dışa aktarım, yazdırma ya da liste yazımı **dosyanızın üstüne doğrudan yazmaz.**
Dosya önce hedefin yanında gizli bir hazırlık klasörüne (`.kentos-` ile başlayan) kendi
adıyla yazılır; yazım hatasız biterse yerine taşınır, klasör silinir. Taşıma aynı
diskte bir yeniden adlandırmadır, kopyalama değildir; hedef hiçbir an yarım bir dosya
tutmaz.

- **Yazım yarıda kalırsa** — disk doldu, izin yok, bir sayfa çizilemedi — hedefteki
  dosya **bayt bayt olduğu gibi** kalır; ilk kez yazılacak bir dosya hiç oluşmaz.
  Hazırlık klasörü de kalmaz. Süren bir dışa aktarımı durum çubuğundaki **Durdur**'la
  ya da **Esc**'le [durdurmak](../baslangic/arayuz.md#uzun-işler) da aynı sonucu verir:
  durdurulan dışa aktarım hiçbir şey yazmamış sayılır.
- **Birlikte yazılan dosyalar birlikte taşınır:** DXF ve `.prj`'si, PNG ve `.pgw`'si,
  çok sayfalı bir yerleşimin bütün sayfaları. Üçüncü sayfası çizilemeyen bir yerleşim
  ilk iki sayfayı da değiştirmez; eskiden ilk ikisi yeni, gerisi eski kalıyordu.
- **Taşıma bile yarıda kalabilir:** takımdan bir dosya başka bir programda açıksa
  (Windows'ta) ya da onun yerinde bir klasör duruyorsa yerine konamaz. Program bunu
  başarı saymaz; hangi dosyaların yenilendiğini ve hangilerinin yenilenemediğini **tek
  tek** söyler:

  ```text
  'teslim/ada12.dxf' yerine tam konamadı. Yerine konan: teslim/ada12.dxf. Konamayan: teslim/ada12.prj (dosya başka bir programda açık). Hedefteki dosya takımı eski ve yeni dosyaların karışımı olabilir; dosyaları kullanan programı kapatıp dışa aktarmayı yineleyin.
  ```

  Parantezdeki neden şunlardan biridir: *yerinde aynı adlı bir klasör var*, *dosya başka
  bir programda açık*, *izin yok ya da dosya başka bir programda açık*, *disk dolu*, *disk
  salt okunur*; bunların dışındaki bir nedeni işletim sistemi nasıl söylediyse öyle
  yazılır. Takımdan önce yardımcı dosyalar, en son sizin adını verdiğiniz dosya taşınır.
- **Aynı komutu yeniden çalıştırmak çoğaltmaz:** dosya yeniden, eskisinin yerine yazılır;
  ekleme yapılmaz, yanına ikinci bir kopya konmaz.
- **Metre dışa aktarımından kalan `.prj`**, aynı dosya milimetre olarak yeniden
  yazılınca kaldırılır ve not bunu söyler: yerinde kalsaydı yeni dosyanın sayılarını
  metre diye etiketlerdi.

Proje dosyası (`.pcad`) da aynı güvencededir; o, kendi yanına yazıp yerine koyar
([KAYDET](../komutlar/save.md)).

## Ağdan veri okunmaz

`/vsicurl/`, `/vsis3/`, `/vsizip/` gibi sanal dosya sistemi yolları reddedilir.
Bir veri dosyasının adı, ağ isteğine ya da arşiv içine erişime dönüşemez —
komut satırından, betikten ya da yapay zekâ önerisinden gelmiş olması fark
etmez. Dosyayı diske indirip öyle açın.

## Dış biçim desteği kapalıysa

KentOSCad, GDAL kütüphanesi olmadan da derlenebilir. O yapıda `İÇEAKTAR` ve
`DIŞAAKTAR` **hata döndürür** ve hangi paketin kurulması gerektiğini söyler —
sessizce boş bir katman döndürmez.

Durumu görmek için:

```bash
make doctor
```

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `io.no_driver: '...' için sürücü bulunamadı.` | Uzantı izin listesinde değil | `bicim` parametresiyle sürücüyü söyleyin ya da desteklenen bir biçime çevirin |
| `io.no_driver: ... sürücüsü yazma için açık değil.` | Biçim yalnız okunuyor | DWG için DXF dışa aktarıp dönüştürün |
| `'...' katmanı hiçbir koordinat sistemi bildirmiyor.` | Veri kümesi etiketsiz | Yanına aynı adlı bir `.prj` dosyası koyun |
| `'...' içindeki katmanlar farklı koordinat sistemleri bildiriyor` | Karışık veri kümesi | Tek bir sisteme dönüştürüp yeniden deneyin |
| `'...' okunabilir çizgi ya da alan içermiyor` | Dosyada desteklenen geometri yok | Dosyayı denetleyin; nokta ve eğriler bu sürümde okunmuyor |
| `'...' okunabilir çizgi ya da alan içermiyor; … büyük olasılıkla boylam ve enlem (derece) …` | Sistem bildirmeyen dosyanın derece sayıları metre okunup milimetrelik noktalara ezildi | Dosyanın sistemini bulun, metre sayan bir sisteme dönüştürüp yeniden aktarın |
| `'...' dosyasının '...' katmanı içe alınmadı. '...' coğrafi bir koordinat sistemi …` | Katman derece sayıyor | `ogr2ogr -t_srs EPSG:5256 yeni.gpkg eski.gpkg` ile dönüştürüp yeniden alın |
| `'...' içe alınmadı. … derece …` (DXF) | DXF'in yanındaki `.prj` derece sayan bir sistem bildiriyor | `.prj` yanlışsa düzeltin, doğruysa DXF'i dönüştürün |
| `Yanındaki .prj metre sayan bir sistem bildiriyor … ama çizim milimetre olarak okundu` (uyarı) | `.prj` ile `çizim_birimi` uyuşmuyor | Dosya metre ise `AYAR çizim_birimi metre` ile yeniden aktarın |
| `DXF'in yanına .prj yazılmadı: …` (dışa aktarma notu) | `çizim_birimi` metre değil | Koordinat sistemini taşıyan bir DXF için `AYAR çizim_birimi metre` |
| `Çizimin koordinat sistemi belirsiz.` | Proje ayarı boş | `AYAR koordinat_sistemi EPSG:5254` |
| `Çizimin koordinat sistemi '...' dışa aktarım için çözülemedi.` | Ayar bir EPSG kodu değil | EPSG kodu verin, örnek `EPSG:5254` |
| `'...' sanal dosya sistemi yolu.` | `/vsi...` ile başlayan yol | Dosyayı diske alıp yeniden deneyin |
| `Çizimde dışa aktarılacak nesne yok` | Çizim boş ya da her şey silinmiş | Önce çizin |
| `io.no_driver: Dış biçim desteği KAPALI.` | GDAL olmadan derlenmiş yapı | Mesajdaki kurulum komutunu izleyin |
| `Dışa aktarma durduruldu; dosya yazılmadı, '...' olduğu gibi.` | Yazım sürerken **Durdur**'a ya da **Esc**'e basıldı | Hata değildir; eski dosya yerinde. Hazır olduğunuzda yeniden çalıştırın |
| `'...' yerine tam konamadı. Yerine konan: … Konamayan: …` | Takımdan bir dosya başka bir programda açık ya da yerinde bir klasör var | Dosyaları kullanan programı kapatıp yineleyin ([Yazma yarıda kalırsa](#yazma-yarıda-kalırsa)) |
| `'....prj' kaldırıldı: önceki bir metre dışa aktarımından kalmıştı …` (not) | Metre DXF'in yerine milimetre DXF yazıldı | Bir şey gerekmez; `.prj` isteniyorsa `AYAR çizim_birimi metre` ile yeniden aktarın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
