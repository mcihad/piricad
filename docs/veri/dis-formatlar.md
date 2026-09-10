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

**Etiketsiz koordinat kabul edilmez.** İçe aktarılan bir veri kümesi koordinat
sistemini bildirmiyorsa KentOSCad dosyayı reddeder; "herhâlde TUREF/TM30'dur"
varsayımı yapmaz.

Sebebi saha kökenlidir: TM30 ile TM33 karışması sessizdir. Koordinatlar makul
görünür, çizim makul görünür, ve hata ancak tapuya gittiğinde ortaya çıkar.

| Biçim | Koordinat sistemini nasıl taşır |
|---|---|
| GeoPackage | Dosyanın içinde. Ek bir şey gerekmez |
| DXF | **Taşımaz.** Yanındaki aynı adlı `.prj` dosyasından okunur |

DXF'in koordinat sistemi için yeri yoktur — bu biçimin kendi eksiğidir, KentOSCad'in
değil. Bu yüzden:

- **Dışa aktarırken** KentOSCad `.dxf` ile birlikte bir `.prj` dosyası yazar ve size
  söyler. Çizimi taşırken **iki dosyayı da götürün**.
- **İçe aktarırken** KentOSCad aynı adlı `.prj` dosyasını arar. Yoksa çizimin
  kendi sistemini varsayar ve bunu transkriptte açıkça söyler; koordinatlardan
  bölge tahmin etmez.

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
| **Ölçülmüş noktalar** — nirengi, poligon noktası, röper | Yazı tipi; yazının üst/orta hizaları en yakın desteklenen hizaya çevrilir |
| **Yazılar**, yüksekliği, **açısı** ve hizasıyla birlikte; MTEXT biçim kodları soyulur | Yalnız uydurma noktası taşıyan spline: uydurma noktaları kontrol noktası sayılır, `düşürme:` ile söylenir |
| **Daire, yay, elips ve kısmi elips** — gerçek eğri olarak; GeoPackage'a çokgen olarak gider, geri okunuşta eğri olur | Katalogda olmayan tarama deseni: sınır, ad, açı korunur, desen çizilmez, söylenir |
| **Yaylı çoklu çizgi** (şişkinlik): her yay merkezi ve yarıçapıyla; **spline**: derece, düğüm, ağırlık | Kenar boyunca değişen çoklu çizgi kalınlığı |
| **Blok tanımları ve referansları** (`BLOCK`/`INSERT`): yapısıyla, ölçek, açı, ayna, dizi; iç içe | Anonim bloklar (`*D1`…): ölçünün kendi çizgileri, ölçü nesnesi zaten okunduğu için |
| **Tarama** (`HATCH`): sınır döngüleri, desen adı, açı, ölçek; aileler desen kataloğundan | Tarama sınırındaki yay ve spline kenarlar çizgi parçalarına bölünür |
| **Ölçü** (`DIMENSION`: hizalı, doğrusal, yarıçap, çap, açısal, ordinat) ve **lider** (`LEADER`) | Ölçü stilinin dosyada olmaması: ISO-25 ölçüleri kullanılır, söylenir |
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
[ölçü](../nesneler/olcu.md), `LEADER` [lider](../nesneler/lider.md). Katmanlar
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
| `Çizimin koordinat sistemi belirsiz.` | Proje ayarı boş | `AYAR koordinat_sistemi EPSG:5254` |
| `Çizimin koordinat sistemi '...' dışa aktarım için çözülemedi.` | Ayar bir EPSG kodu değil | EPSG kodu verin, örnek `EPSG:5254` |
| `'...' sanal dosya sistemi yolu.` | `/vsi...` ile başlayan yol | Dosyayı diske alıp yeniden deneyin |
| `Çizimde dışa aktarılacak nesne yok` | Çizim boş ya da her şey silinmiş | Önce çizin |
| `io.no_driver: Dış biçim desteği KAPALI.` | GDAL olmadan derlenmiş yapı | Mesajdaki kurulum komutunu izleyin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
