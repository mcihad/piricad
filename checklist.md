Mevcut KentOS CAD projesini baştan yazma. Önce mevcut mimariyi, kullanılan kütüphaneleri, veri modellerini, DXF import akışını, QGIS sembol motoru entegrasyonunu, Qt katmanını, GDAL/GEOS/PROJ kullanımını ve varsa kullanılan açık kaynak CAD/DXF kütüphanelerini ayrıntılı biçimde incele.

Amaç mevcut çalışan sistemi koruyarak DXF/CAD altyapısını profesyonel CAD uygulaması seviyesine yaklaştırmak, eksik veya hatalı mimari kararları düzeltmek, parçalı çözümleri standartlaştırmak ve ileride DXF export, DWG desteği, hassas çizim, snapping, topoloji, imar planlama ve Netcad/QGIS seviyesinde düzenleme özelliklerinin üzerine güvenle kurulabileceği sağlam bir CAD çekirdeği oluşturmaktır.

Temel kural: Mevcut sistemde aşağıdaki maddelerden zaten doğru yapılmış olanları yeniden yazma. Önce tespit et, kalite ve mimari açıdan değerlendir, gerekiyorsa iyileştir. Eksik olanları ekle. Kısmen yapılmış olanları tamamla. Aynı işi yapan birden fazla yapı varsa ortak ve standart bir yapıda birleştir. Çalışan özellikleri gereksiz yere bozma.

## 1. Önce mevcut mimariyi analiz et

Kod tabanını inceleyerek aşağıdakileri tespit et:

* DXF hangi kütüphane veya parser ile okunuyor?
* GDAL DXF sürücüsü mü kullanılıyor?
* libdxfrw, LibreCAD tabanlı bir kütüphane veya başka bir açık kaynak CAD kütüphanesi kullanılıyor mu?
* DXF entity'leri doğrudan QgsGeometry, GEOS geometry veya başka bir GIS geometrisine mi dönüştürülüyor?
* Uygulamanın bağımsız bir CAD document/entity modeli var mı?
* ARC, CIRCLE, SPLINE ve benzeri gerçek CAD primitive'leri korunuyor mu?
* BLOCK/INSERT ilişkileri korunuyor mu?
* DXF stil bilgileri nasıl tutuluyor?
* BYLAYER/BYBLOCK davranışı mevcut mu?
* TEXT/MTEXT nasıl render ediliyor?
* HATCH nasıl ele alınıyor?
* DIMENSION, LEADER ve benzeri nesneler destekleniyor mu?
* OCS/WCS transform uygulanıyor mu?
* Layer tabloları ve DXF tabloları korunuyor mu?
* DXF XDATA ve bilinmeyen group code bilgileri korunuyor mu?
* Model Space / Paper Space ayrımı var mı?
* Koordinat hassasiyeti hangi numeric type ile tutuluyor?
* Snapping altyapısı mevcut mu?
* Spatial index kullanılıyor mu?
* Render geometrisi ile gerçek CAD geometrisi birbirinden ayrılmış mı?
* DXF import → düzenleme → DXF export sırasında veri kaybına yol açacak bir mimari var mı?

Bunların sonucuna göre doğrudan geliştirmeye geç. Sadece rapor hazırlayıp bırakma.

## 2. Canonical CAD Document Model oluştur veya mevcut olanı standartlaştır

DXF hiçbir zaman yalnızca “GIS feature koleksiyonu” olarak ele alınmamalıdır.

Uygulamada dosya formatından bağımsız bir ana CAD modeli bulunmalıdır. Mevcutsa geliştir; yoksa oluştur.

Örnek kavramsal yapı:

```text
CadDocument
 ├── DrawingSettings
 ├── Units
 ├── CoordinateSystem
 ├── Layers
 ├── LineTypes
 ├── TextStyles
 ├── DimensionStyles
 ├── Blocks
 ├── Entities
 ├── ModelSpace
 ├── PaperSpaces
 └── Metadata
```

Temel entity hiyerarşisi örneğin:

```text
CadEntity
 ├── CadPoint
 ├── CadLine
 ├── CadRay
 ├── CadXLine
 ├── CadArc
 ├── CadCircle
 ├── CadEllipse
 ├── CadPolyline
 ├── CadSpline
 ├── CadHatch
 ├── CadText
 ├── CadMText
 ├── CadBlockReference
 ├── CadDimension
 ├── CadLeader
 ├── CadMultiLeader
 ├── CadSolid
 ├── CadFace
 └── diğer gerekli entity türleri
```

Bu isimleri birebir kullanmak zorunlu değildir. Mevcut proje isimlendirme ve mimari standartlarına uy.

Ancak temel prensip değişmemelidir:

**CAD entity → gerçek CAD semantiğini korumalıdır.**

## 3. DXF'yi doğrudan QgsGeometry'ye flatten etme

DXF import sırasında ARC, CIRCLE, ELLIPSE, SPLINE, POLYLINE BULGE vb. primitive'leri doğrudan LINESTRING segmentlerine çevirerek kaynak semantiği yok etme.

Şu ayrımı oluştur:

```text
Canonical CAD Geometry
        ↓
Render representation
        ↓
GIS representation
        ↓
GEOS representation
```

Aynı CAD entity gerektiğinde farklı adapter'lar tarafından farklı biçimlere dönüştürülebilmelidir.

Örneğin CadCircle gerçek:

* center
* radius
* normal
* extrusion
* thickness

bilgilerini korumalıdır.

Render için tessellate edilebilir.

GEOS işlemi için polygon/linestring'e çevrilebilir.

QGIS üzerinde gösterim için QgsGeometry/QgsCurve'a dönüştürülebilir.

Ama canonical nesne hiçbir zaman tessellation sonucuyla değiştirilmemelidir.

## 4. ARC / CIRCLE / ELLIPSE / SPLINE desteğini profesyonel hale getir

Şunları doğru ve kayıpsız destekle:

### ARC

* center
* radius
* start angle
* end angle
* normal/extrusion
* elevation

### CIRCLE

* center
* radius
* plane
* normal
* thickness

### ELLIPSE

* center
* major axis
* minor axis ratio
* start/end parameter
* orientation

### SPLINE

Mümkün olduğu ölçüde gerçek NURBS bilgisini sakla:

* degree
* knots
* control points
* weights
* flags
* closed/open
* periodic
* rational

Spline'ı yalnızca çizgi segmenti olarak saklama.

## 5. POLYLINE ve LWPOLYLINE özellikle doğru ele alınmalı

Her vertex için gerekli CAD verilerini koru:

```text
position
bulge
startWidth
endWidth
flags
```

`bulge != 0` olan segment yaydır.

Bulge bilgisini import sırasında kaybetme.

Snapping, ölçüm, export ve yeniden düzenleme için gerçek segment tipinin bilinmesi gerekir.

Polyline içerisinde line segment ve arc segment ayrımı yapılabilen bir model tercih et.

## 6. BLOCK / INSERT mimarisini doğru kur

DXF block yapısını import sırasında gereksiz yere explode etme.

Şu kavramlar ayrı olmalıdır:

```text
BlockDefinition
BlockReference / Insert
```

BlockDefinition gerçek entity listesini tutar.

BlockReference:

* block id/name
* insertion point
* rotation
* scaleX
* scaleY
* scaleZ
* normal
* attributes
* array bilgisi varsa row/column parametreleri

taşımalıdır.

Desteklenmesi gerekenler:

* nested block
* recursive block kontrolü
* negative scale
* mirror
* non-uniform scale
* rotation
* ATTDEF
* ATTRIB
* block attribute değerleri
* anonymous block
* dynamic/unsupported block metadata'sını mümkünse koruma

Render sırasında block instance transform ile çizilebilir.

GIS'e export gerektiğinde isteğe bağlı explode işlemi yapılabilir.

Canonical modelde block ilişkisini kaybetme.

## 7. Stil sistemi standardize edilmeli

CAD stil bilgisini doğrudan RGB veya QGIS symbol olarak saklama.

DXF semantiğini koru.

Örneğin renk:

```text
ByLayer
ByBlock
ExplicitACI
TrueColor
```

olabilir.

Benzer biçimde:

* color
* linetype
* lineweight
* transparency
* plot style

için inheritance mantığını koru.

Ortak bir:

```text
CadStyle
CadStyleResolver
ResolvedCadStyle
```

veya projedeki isimlendirmeye uygun eşdeğer yapı kullan.

Style resolve sırası DXF davranışına uygun olmalıdır.

Örneğin:

```text
Entity explicit style
      ↓
BYBLOCK context
      ↓
Layer style
      ↓
Drawing defaults
```

QGIS symbol motoruna yalnızca resolve edilmiş stil verilmelidir.

QGIS sembol motoru canonical CAD stilinin kendisi olmamalıdır.

## 8. Linetype Engine oluştur veya mevcut olanı geliştir

Sadece dash array desteklemek yeterli değildir.

Linetype modeli mümkün olduğu ölçüde:

* dash
* gap
* dot
* text element
* shape element
* repetition
* scale
* alignment

desteklemelidir.

Entity linetype scale ve global linetype scale kavramlarını dikkate al.

Complex DXF linetype'ları mümkün olduğunca kayıpsız içeri al.

QGIS sembol motoruyla gösterilebilenleri QGIS sembollerine dönüştür.

QGIS'in doğrudan karşılamadığı durumlarda custom symbol layer veya özel render çözümü kullan.

Bu motor ileride Mekânsal Planlar Yapım Yönetmeliğindeki karmaşık çizgi gösterimlerini de destekleyebilecek kadar genel tasarlanmalıdır.

## 9. HATCH gerçek CAD nesnesi olarak ele alınmalı

HATCH'i yalnızca polygon fill'e dönüştürme.

Şunları mümkün olduğu ölçüde sakla:

* boundary loops
* outer boundary
* inner islands
* pattern name
* predefined/user/custom pattern
* angle
* scale
* spacing
* pattern lines
* origin
* associative flag
* solid fill
* gradient bilgileri desteklenebiliyorsa

Boundary içerisinde:

* line
* arc
* ellipse
* spline

olabilir.

Boundary'nin gerçek geometrisini mümkün olduğunca koru.

Render sırasında QGIS fill symbol/pattern veya özel renderer kullanılabilir.

## 10. TEXT ve MTEXT altyapısını güçlendir

TEXT ve MTEXT'i ayrı entity türleri olarak koru.

Desteklenmesi gereken başlıca bilgiler:

* content
* raw content
* style
* font
* SHX
* TrueType
* insertion point
* alignment point
* height
* width factor
* rotation
* oblique angle
* horizontal alignment
* vertical alignment
* attachment point
* line spacing
* direction
* extrusion
* background mask mümkünse
* MTEXT inline formatting

Türkçe karakterlerde test yap:

```text
Ç Ğ İ I ı Ö Ş Ü
ç ğ i ö ş ü
```

Encoding sorunlarını merkezi ve standart bir şekilde çöz.

Qt font altyapısını kullanabilirsin ancak SHX font desteğinin ayrı bir konu olduğunu kabul et ve mimariyi genişletilebilir yap.

Desteklenmeyen font veya biçimleme varsa sessizce veri kaybetme; fallback render uygula ama raw CAD bilgisini koru.

## 11. DIMENSION semantik nesne olarak kalmalı

DIMENSION'ı import sırasında line/text grubuna explode etme.

Mümkün olduğunca destekle:

* aligned
* linear
* rotated
* angular
* radial
* diameter
* ordinate
* arc length vb.

Dimension şu bilgileri taşıyabilmelidir:

* referenced geometry/points
* measurement
* displayed text
* text override
* dimension style
* precision
* unit
* arrows
* extension lines
* dimension line
* text placement

DIMSTYLE tablosunu değerlendir.

Render ile semantic dimension modelini birbirinden ayır.

Aynı yaklaşımı:

* LEADER
* MULTILEADER

için de uygula.

## 12. Coordinate System ile Drawing Unit kavramlarını ayır

Şu iki kavram kesinlikle birbirine karıştırılmamalıdır:

```text
Drawing Units
```

ve

```text
Coordinate Reference System
```

Drawing unit örneğin:

* unitless
* millimeter
* centimeter
* meter
* kilometer
* inch
* foot

olabilir.

CRS ise:

* Unknown
* EPSG:xxxx
* TUREF
* ITRF
* ED50
* UTM
* yerel koordinat sistemi

olabilir.

DXF koordinatlarından CRS otomatik ve güvenilmez biçimde tahmin edilmemeli.

PROJ yalnızca CRS dönüşümü için kullanılmalıdır.

CAD drawing unit scale mekanizması ayrı olmalıdır.

## 13. OCS / WCS dönüşümlerini doğru uygula

DXF entity koordinatlarını yalnızca XY kabul etme.

Şunları dikkate al:

* WCS
* OCS
* extrusion direction
* normal vector
* elevation
* entity plane

Ortak bir transform altyapısı oluştur.

Örneğin:

```text
CadTransform
CadMatrix
CadPlane
```

gibi merkezi bir model kullan.

Farklı entity implementasyonlarının kendi başına OCS dönüşümü yapıp tutarsızlık oluşturmasına izin verme.

## 14. Tüm koordinatlarda double precision kullan

Canonical CAD geometrisi için kesinlikle float kullanma.

Koordinatlar:

```cpp
double
```

veya eşdeğer 64 bit kayan nokta hassasiyetinde tutulmalıdır.

Render tarafında GPU float kullanılması gerekiyorsa büyük koordinatların precision problemi için local origin / floating origin yaklaşımı uygula.

Örnek:

```text
World:
475123.43872
4421687.92831

Render origin:
475000
4421000

GPU:
123.43872
687.92831
```

Canonical veriyi hiçbir zaman GPU precision seviyesine düşürme.

## 15. GEOS'u CAD kernel olarak kullanma

GEOS şu işler için kullanılabilir:

* topology
* intersection
* union
* difference
* buffer
* contains
* within
* validity
* polygon operations

Ancak CAD primitive modelinin sahibi GEOS olmamalıdır.

Şunlar kendi CAD geometry katmanında kalmalıdır:

* exact line
* arc
* circle
* ellipse
* spline
* tangent
* perpendicular
* fillet
* chamfer
* CAD intersection
* snap calculations
* parametric curves

GEOS için ayrı adapter oluştur.

## 16. CAD Entity ortak kontratını standardize et

Her entity için mümkün olduğunca ortak davranış tanımla.

Örnek:

```text
id
type
layerId
style
metadata

bounds()
transform()
clone()

snapPoints()
nearestPoint()
intersections()

renderRepresentation()
gisRepresentation()
```

Tam isimleri mevcut kod standardına göre seç.

Entity'ler render teknolojisine bağımlı olmamalıdır.

Örneğin `CadLine` sınıfı doğrudan QPainter veya QgsSymbol çağırmamalıdır.

## 17. Snapping altyapısını şimdiden doğru tasarla

Snapping'i daha sonra eklenecek küçük bir UI özelliği olarak düşünme.

CAD çekirdeğinin temel servislerinden biri olsun.

En azından ileride şu snap türlerini destekleyebilecek mimari kur:

* endpoint
* midpoint
* center
* quadrant
* intersection
* apparent intersection
* nearest
* perpendicular
* tangent
* extension
* parallel
* node/vertex
* insertion point

Her entity'nin kendi analitik snap noktalarını sağlayabilmesi tercih edilir.

Örneğin Circle için center ve quadrant noktaları gerçek circle üzerinden hesaplanmalıdır; tessellated polyline üzerinden değil.

## 18. Spatial index oluştur

Büyük DXF dosyalarında mouse hareketinde tüm entity'leri tarama.

Entity bounding box'ları için spatial index kullan.

Uygun açık kaynak çözümü mevcut projede varsa kullan.

Yoksa:

* R-tree
* QGIS spatial index
* Boost.Geometry index
* başka uygun ve performanslı çözüm

değerlendir.

Akış:

```text
mouse world position
      ↓
search tolerance bbox
      ↓
spatial index
      ↓
candidate entities
      ↓
exact CAD snap calculation
```

olmalıdır.

## 19. Render modelini CAD modelinden ayır

Bir CAD entity = bir QObject veya QGraphicsItem yaklaşımıyla yüz binlerce nesne üretme.

Render için batch/cache yaklaşımı kullan.

Şu yapı tercih edilir:

```text
CadDocument
      ↓
Visibility / Spatial Query
      ↓
Render Scene
      ↓
Geometry Cache / Tessellation Cache
      ↓
QGIS Symbol Engine / Qt Renderer
```

Tessellation cache invalidation entity version veya geometry revision ile yapılabilir.

## 20. QGIS sembol motorunun rolünü doğru sınırla

QGIS sembol motorunu mümkün olduğunca güçlü biçimde kullan.

Ancak QGIS symbol object'lerini canonical CAD modeline gömme.

Araya adapter koy:

```text
CadStyle
      ↓
CadStyleResolver
      ↓
QgisSymbolAdapter
      ↓
QGIS Symbol Engine
```

Bu sayede ileride:

* başka renderer
* PDF renderer
* plot renderer
* web renderer

eklenebilir.

## 21. DXF TABLES ve HEADER bilgisini mümkün olduğunca koru

İlgili DXF bölümlerindeki bilgileri kaybetme.

Özellikle:

* HEADER variables
* LAYER
* LTYPE
* STYLE
* DIMSTYLE
* APPID
* BLOCK_RECORD
* UCS
* VIEW
* VPORT

gibi tabloları desteklenen ölçüde canonical modelde sakla.

Şu an kullanılmayan ama ileride export için gerekli olabilecek bilgileri discard etme.

## 22. XDATA ve unknown DXF data preservation

XDATA desteği ekle veya iyileştir.

Özellikle:

```text
APPID
1000+
group codes
extended entity data
```

korunmalıdır.

Tanımadığımız metadata için:

```text
RawCadMetadata
```

benzeri bir yapı oluştur.

Temel prensip:

**Anlamadığımız DXF bilgisini mümkün olduğunca kaybetmeden sakla.**

İleride:

```text
DXF import → değişiklik → DXF export
```

yapıldığında kullanıcı verisini gereksiz yere yok etmemeliyiz.

## 23. Model Space / Paper Space ayrımını koru

DXF'teki:

* Model Space
* Paper Space
* layout

kavramlarını yok sayma.

İlk sürümde yalnızca Model Space aktif kullanılacak olsa bile veri modelinde bu ayrımı koru.

Viewport ve layout desteğinin ileride eklenebilmesine izin ver.

## 24. Entity handle ve referansları koru

DXF HANDLE bilgilerini kaybetme.

Entity'ler arası referanslar varsa mümkün olduğunca preserve et.

Import sırasında internal UUID/id üretilecekse:

```text
internalId
sourceHandle
```

ayrı kavramlar olsun.

Kaynak DXF handle'ı internal database id yerine kullanılmamalıdır.

## 25. Açık kaynak CAD kütüphanesi kullanılıyorsa doğru biçimde değerlendir

Projede:

* libdxfrw
* LibreCAD bileşenleri
* dxflib
* Open Design Alliance dışındaki başka açık kaynak çözüm
* başka CAD geometry library

kullanılıyorsa hemen değiştirme.

Önce yeteneklerini değerlendir.

Parser iyi çalışıyorsa kullanmaya devam et.

Ama parser'ın kendi entity modelini uygulamanın canonical modeline dönüştüren açık bir adapter katmanı oluştur.

Örneğin:

```text
libdxfrw
      ↓
DxfrwImporterAdapter
      ↓
KentCAD canonical model
```

Parser veri modelini tüm uygulamaya yayma.

Bu sayede ileride parser değiştirilebilir.

## 26. GDAL'ın rolünü doğru sınırla

GDAL projede kesinlikle kalabilir ve çok değerlidir.

GDAL:

* GIS formatları
* raster
* shapefile
* GeoPackage
* GeoJSON
* diğer OGR formatları

için kullanılabilir.

Ancak DXF'yi profesyonel CAD fidelity ile içeri alma gereksiniminde yalnızca GDAL DXF sürücüsüne bağımlı kalma.

Mevcut kod GDAL DXF kullanıyorsa:

* tamamen sökmeden önce kullanım noktalarını analiz et;
* gerekiyorsa CAD importer için ayrı parser ekle;
* GDAL DXF desteğini “GIS import mode” olarak bırakabilirsin.

Örneğin:

```text
DXF Open as CAD
DXF Import as GIS
```

ileride iki ayrı kullanım senaryosu olabilir.

## 27. DXF import işlemini katmanlara böl

Tek dev importer sınıfı oluşturma.

Önerilen sorumluluklar:

```text
DxfReader
DxfHeaderReader
DxfTableReader
DxfBlockReader
DxfEntityReader

DxfEntityConverter
DxfStyleConverter
DxfTransformResolver
DxfBlockResolver
DxfTextResolver

CadDocumentBuilder
```

Mevcut mimari daha iyi isimler kullanıyorsa onları koru.

Ama parser, conversion, semantic model ve rendering sorumluluklarını birbirinden ayır.

## 28. Import warning sistemi oluştur

Desteklenmeyen entity veya özellik geldiğinde sessizce yok etme.

Örneğin:

```text
Unsupported DXF entity: ACAD_PROXY_ENTITY
Unsupported hatch gradient
Missing SHX font
Unknown linetype shape
Invalid block reference
Circular block reference
```

gibi durumları merkezi bir:

```text
ImportDiagnostics
```

yapısında topla.

Seviyeler:

* info
* warning
* error
* fatal

olabilir.

Import mümkün olduğunca devam etsin ancak kullanıcıya hangi verinin tam desteklenmediği bildirilebilsin.

## 29. Test altyapısını ciddi biçimde kur

DXF importer yalnızca görsel olarak test edilmemeli.

Test fixture dosyaları oluştur.

En azından ayrı DXF örnekleri:

```text
line.dxf
arc.dxf
circle.dxf
ellipse.dxf
lwpolyline-bulge.dxf
polyline-width.dxf
spline.dxf
nested-block.dxf
mirrored-block.dxf
scaled-block.dxf
attrib-block.dxf
text.dxf
mtext.dxf
turkish-text.dxf
hatch.dxf
dimension.dxf
leader.dxf
ocs.dxf
xdata.dxf
large-coordinate.dxf
complex-linetype.dxf
paper-space.dxf
```

Her fixture için:

* entity sayısı
* entity tipi
* coordinates
* bbox
* layer
* style
* block relation
* metadata

kontrol edilsin.

## 30. Round-trip test altyapısını şimdiden düşün

DXF export henüz yapılmıyorsa bile canonical model buna uygun tasarlansın.

İleride hedef:

```text
DXF A
 ↓
Import
 ↓
CadDocument
 ↓
Export
 ↓
DXF B
```

olduğunda semantik fark minimum olmalıdır.

Round-trip sırasında özellikle:

* arc → arc
* circle → circle
* spline → spline
* block → block
* hatch → hatch
* text → text
* dimension → dimension

korunabilmelidir.

## 31. Performance kriterleri

Importer ve CAD document modeli büyük dosyalara uygun olmalıdır.

Dikkat:

* gereksiz QObject kullanımından kaçın
* entity başına gereksiz heap allocation azalt
* mümkün olan yerde move semantics kullan
* büyük entity koleksiyonlarında cache locality düşün
* spatial index kur
* tessellation lazy olsun
* render cache kullan
* block geometry cache kullan
* text glyph/cache değerlendirilir
* immutable/shared style tanımları kullanılabilir
* duplicate style ve linetype nesneleri üretme

Profil ölçmeden mikro optimizasyon yapma ancak mimariyi büyük veri için uygun kur.

## 32. Threading

Import süreci UI thread'i kilitlememelidir.

Ancak QGIS/Qt sınıflarının thread safety durumuna dikkat et.

Parser ve canonical document oluşturma mümkünse UI-independent yapılmalıdır.

UI güncellemesi ayrı katmanda olsun.

Threading eklerken race condition yaratma.

## 33. Undo/Redo altyapısına uygun model

Import edilen entity ileride düzenleneceği için canonical model:

* create
* delete
* transform
* property change
* geometry change

işlemlerinin command pattern/undo stack ile yönetilebilmesine uygun olsun.

Qt'nin undo framework'ü kullanılabilir veya mevcut proje altyapısına uyulabilir.

Entity'nin geometry revision/version değeri olması cache invalidation açısından faydalı olabilir.

## 34. Selection modelini render modelinden ayır

Selection bir entity property olarak kalıcı şekilde yazılmamalıdır.

Şunlar ayrı kavramlar olsun:

```text
CadDocument
SelectionModel
HoverModel
EditSession
```

Selection sonucu renderer tarafından highlight edilir.

## 35. CAD tolerance politikası oluştur

Sistemin çeşitli yerlerinde rastgele:

```cpp
1e-6
1e-9
0.001
```

gibi sabit tolerance değerleri kullanma.

Merkezi bir tolerance/precision politikası oluştur.

Örneğin:

```text
coordinate epsilon
angle epsilon
snap tolerance
topology tolerance
display tolerance
tessellation tolerance
```

birbirinden farklı kavramlar olarak ele alınmalıdır.

Drawing unit'e göre gerektiğinde scale edilebilir.

## 36. Tessellation merkezi ve kontrollü olsun

Arc/spline/ellipse tessellation her component tarafından farklı şekilde yapılmamalıdır.

Ortak:

```text
CadTessellator
```

veya eşdeğer servis oluştur.

Tessellation parametreleri:

* screen tolerance
* world tolerance
* zoom
* export tolerance
* topology tolerance

gibi kullanım amacına göre farklı olabilir.

Render için düşük hata ile yeterli segment.

Analitik geometri için tessellation yerine gerçek primitive.

GEOS conversion için kontrollü tessellation.

## 37. Bounding box hesapları analitik yapılmalı

Circle, arc, ellipse vb. için bbox'ı tessellated geometri üzerinden yaklaşık hesaplama.

Mümkün olduğunca matematiksel exact bounding box hesapla.

Bu spatial index ve selection doğruluğunu artırır.

## 38. Layer modelini güçlü kur

CAD Layer yalnızca string name değildir.

Mümkün olduğunca:

```text
id
name
color
linetype
lineweight
transparency
visible
frozen
locked
printable
plotStyle
metadata
```

sakla.

Layer hiyerarşisi uygulamaya ait ek bir özellik olacaksa DXF layer modelinden ayrı düşün.

## 39. Z koordinatını gereksiz yere yok etme

KentOS başlangıçta 2D ağırlıklı olabilir ancak DXF import sırasında Z değerlerini discard etme.

Canonical coordinate:

```text
x
y
z
```

desteklemelidir.

2D araçlar Z'yi gerektiğinde ignore edebilir.

İleride:

* arazi
* kot
* profil
* 3D
* bina
* altyapı

işleri için gerekli olacaktır.

## 40. QGIS, GDAL, GEOS, PROJ bağımlılık sınırlarını temizle

Hedef mimari yaklaşık olarak şu sorumluluk ayrımına sahip olmalıdır:

```text
Qt
→ uygulama UI/framework

QGIS Symbol Engine
→ kartografik render ve stil

GDAL/OGR
→ genel GIS dosya formatları

GEOS
→ GIS topology ve polygonal geometry operations

PROJ
→ CRS ve koordinat dönüşümü

CAD Parser
→ DXF/DWG CAD dosyası okuma

KentCAD Core
→ gerçek CAD document/entity/geometry modeli
```

Bu sınırlar kod seviyesinde mümkün olduğunca korunmalıdır.

## 41. Kod kalite kuralları

Yeni geliştirmelerde:

* dev sınıflardan kaçın
* SRP uygula
* açık interface/adapter katmanları kullan
* raw pointer kullanımını minimuma indir
* ownership açık olsun
* RAII uygula
* modern C++ kullan
* gereksiz inheritance yerine composition değerlendir
* public API minimal olsun
* parser-specific tipleri core'a sızdırma
* QGIS-specific tipleri core'a sızdırma
* Qt UI tiplerini geometry core'a sızdırma

Mevcut projede C++ standardı ne ise önce onu tespit et; destekleniyorsa modern C++ özelliklerinden faydalan.

## 42. Lisans uyumluluğunu kontrol et

Yeni açık kaynak kütüphane eklemeden önce lisansını kontrol et.

Projenin tamamen açık kaynak kalması gerektiğini dikkate al.

Kullanılan kütüphanenin:

* GPL
* LGPL
* MIT
* BSD
* MPL

gibi lisans şartlarını değerlendir.

QGIS ve mevcut proje lisans yapısıyla uyumsuz veya kaynak kapatmayı gerektiren bir kütüphane ekleme.

Açık kaynak alternatifi bulunan durumda proprietary SDK'ya bağımlılık oluşturma.

## 43. Mevcut çalışan sistemi yeniden icat etme

Özellikle bu talimata dikkat et:

Bir özellik mevcut projede zaten doğru biçimde yapılmışsa yalnızca bu talimatta geçtiği için tekrar implement etme.

İş akışı:

```text
inspect
→ understand
→ compare against requirements
→ preserve good implementation
→ refactor weak implementation
→ implement missing parts
→ add tests
```

olmalıdır.

## 44. Değişiklikleri aşamalı yap

Tüm sistemi tek commit/dev refactor ile kırma.

Mantıksal olarak böl:

1. canonical model
2. parser adapter
3. primitive geometry
4. block
5. styles
6. text
7. hatch
8. dimensions
9. transforms
10. index/snapping
11. diagnostics
12. tests

Ancak geliştirmeyi sadece planlamakla kalma; kodu gerçekten uygula.

## 45. Öncelik sırası

Şu anda özellikle DXF import aşamasında olduğumuz için öncelik sırası:

### P0 – Kritik

* mevcut importer analizi
* canonical CAD document modeli
* LINE
* ARC
* CIRCLE
* ELLIPSE
* POLYLINE/LWPOLYLINE + BULGE
* SPLINE
* BLOCK/INSERT
* layer
* BYLAYER/BYBLOCK
* double precision
* OCS/WCS
* source handle preservation

### P1 – Çok önemli

* TEXT
* MTEXT
* linetype
* HATCH
* XDATA
* raw metadata
* spatial index
* snapping-ready entity interface
* diagnostics

### P2

* DIMENSION
* LEADER
* MULTILEADER
* Paper Space
* advanced hatch
* complex linetype
* 3D entity çeşitleri

Ancak mevcut uygulama bazı P2 özelliklerini zaten destekliyorsa bunları geriye düşürme veya kaldırma.

## Nihai hedef

DXF importer'ın görevi:

```text
DXF
 ↓
Parser
 ↓
Semantic CAD Import
 ↓
KentCAD Document
```

olmalıdır.

Şu olmamalıdır:

```text
DXF
 ↓
OGR
 ↓
QgsGeometry
 ↓
render
```

İkinci yöntem yalnızca basit bir GIS DXF importer için yeterlidir; profesyonel CAD uygulamasının temeli olmamalıdır.

KentOS CAD'in nihai mimarisi şu yaklaşımı izlemelidir:

```text
                     KentOS CAD

                    Qt Application
                         │
                         ▼
                    KentCAD Core
                         │
       ┌─────────────────┼─────────────────┐
       ▼                 ▼                 ▼
 CAD Geometry      CAD Document       CAD Services
       │                 │                 │
       │                 │            Snap / Edit /
       │                 │            Selection /
       │                 │            Transform
       │                 │
       └─────────────────┼─────────────────┘
                         │
             ┌───────────┼────────────┐
             ▼           ▼            ▼
        QGIS Adapter   GEOS Adapter  IO Adapters
             │                        │
        Symbol Engine           DXF / GIS / ...
                                      │
                          GDAL / CAD Parser
```

Mevcut projeyi bu hedefe doğru evrimleştir.

Özellikle mevcut DXF importer'ı incelemeden “sıfırdan yeni importer” yazma. Kullanılan açık kaynak CAD kütüphanesi iyi ise onu koru ve adapter ile standardize et. Eksik veya veri kaybettiren noktaları düzelt.

Her değişiklikten sonra mevcut DXF dosyalarının açılmaya devam ettiğini doğrula.

Görsel olarak “dosya açılıyor” sonucunu yeterli kabul etme. CAD semantiğinin korunmasını test et.

Ana kalite kriterimiz:

**DXF'yi ekranda göstermek değil, DXF içerisindeki CAD bilgisini doğru anlamak, doğru saklamak, doğru düzenleyebilmek ve ileride mümkün olduğunca kayıpsız tekrar dışarı verebilmektir.**
