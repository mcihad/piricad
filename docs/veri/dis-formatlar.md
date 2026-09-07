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
| AutoCAD DXF | `.dxf` | evet | evet |
| AutoCAD DWG | `.dwg` | evet¹ | **hayır** — aşağıya bakın |
| ESRI Shapefile | `.shp` | evet | **hayır** — aşağıya bakın |
| OGC GeoPackage | `.gpkg` | evet | evet |

¹ DWG okuma **isteğe bağlı bir yapı seçeneğidir**. Paketlenmiş sürümde açık
gelir; kendiniz derliyorsanız `-DKENTOS_WITH_DWG=ON` gerekir. Kapalıysa bir
`.dwg` açmaya çalışmak ne yapmanız gerektiğini yazan bir hata verir.

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
| `POINT` — nirengi, poligon noktası, röper | Ölçülendirme (`DIMENSION`) |
| `TEXT` — ada ve parsel numaraları, yüksekliğiyle | Tarama (`HATCH`) |
| `CIRCLE` ve `ARC` — **gerçek daire ve yay olarak**, çizgiye bölünmeden | Kâğıt alanı (layout) — çizim değildir, alınmaz |
| Katman adları | Katman rengi ve çizgi tipi |

Okunamayan bir varlık türüyle karşılaşılırsa **adıyla ve sayısıyla** bildirilir.
Sessizce düşürülmez.

Daire ve yay her iki yolda da **gerçek daire ve yay** olarak gelir. DWG yolunda
LibreDWG onları zaten öyle verir; DXF yolunda GDAL çizgi parçalarına böler ve
KentOSCad merkezle yarıçapı geri kurar — nasıl olduğu aşağıda.

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
- **İçe aktarırken** KentOSCad aynı adlı `.prj` dosyasını arar. Yoksa dosyayı
  reddeder ve `.prj` koymanızı ister.

`.prj`, ülkedeki her CBS yazılımının anladığı ESRI biçiminde yazılır.

İçe aktarılan verinin koordinat sistemi çizimin kendi sisteminden farklıysa
KentOSCad **koordinatları dönüştürmez**; farkı söyler ve kararı size bırakır.
Sessiz bir yeniden projeksiyon, yanlış yere oturmuş bir parselin en kolay yoludur.

Çizimin koordinat sistemini `AYAR koordinat_sistemi` ile bildirin:

```
AYAR koordinat_sistemi EPSG:5254
```

## Ne aktarılır, ne aktarılmaz

| Aktarılan | Aktarılmayan |
|---|---|
| Çizgi ve alan geometrisi, milimetre hassasiyetiyle | Öznitelikler — belge modeli öznitelik sütunlarını Faz 1'de kazanacak |
| **Ölçülmüş noktalar** — nirengi, poligon noktası, röper | Katman rengi, çizgi tipi, ölçek sınırları |
| **Yazılar**, yüksekliğiyle birlikte | Nesne başına stil |
| Katman adları | Nesne ve katman anahtarları |
| Boşluklu ve çok parçalı alanlar | Yazı tipi, yazının açısı |
| Koordinat sistemi | |

Bu yüzden **çalışma dosyanız `.pcad` olmalıdır**. DXF ve GeoPackage teslim
biçimleridir; bir dışa aktarıp geri alma turu çiziminizi olduğu gibi geri
getirmez.

### DXF'te kapalı çizgi alandır

DXF'in poligonu yoktur: bir parsel, kapalı bayrağı açık bir `LWPOLYLINE`'dır.
KentOSCad ilk köşesi sonuncusuyla aynı olan bir çizgiyi **alan** olarak okur —
yoksa dosyadaki her parsel çizgi olarak gelir, dolgusu olmaz, alanı ölçülemez ve
[`İFRAZ`](../komutlar/split_parcel.md) ile [`TEVHİT`](../komutlar/merge.md)
üzerinde çalışamaz.

### Daire daire, yay yay olarak gelir

GDAL bir DXF'teki `CIRCLE` ve `ARC` varlığını KentOSCad'e ulaşmadan önce çizgi
parçalarına böler. Böyle bırakılsa nesne artık daire olmazdı: merkezi, yarıçapı,
πr² alanı ve merkeze yakalama olmazdı — 48 MB'lık bir kadastro dosyasında 3 874
daire ve 3 523 yay çokgene dönerdi. AutoCAD ve FreeCAD onları eğri olarak tutar,
KentOSCad de tutar.

Nesnenin **ne olduğu dosyadan okunur**, şekle bakılarak tahmin edilmez: DXF her
varlığın kendi sınıf zincirini yazar (`AcDbEntity:AcDbCircle`). Yalnızca
**sayılar** — merkez ve yarıçap — parçalanmış noktalardan geri kurulur, ve
kurulan çember dosyadaki her noktaya milimetre içinde uymuyorsa kabul edilmez;
o zaman çoklu çizgi olarak kalır. Yani bir haritacının elle çizdiği 64 kenarlı
çokgen çokgen kalır, daireye dönüşmez.

Yayın hangi yöne süpürdüğü de dosyadan değil, parçalanmanın kendisinden okunur.

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
