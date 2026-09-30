# İÇEAKTAR — Dış Veri Alma

Başka bir programdan ya da kurumdan veri alan kullanıcı için; bu sayfayı
bitirdiğinizde bir DXF, GeoPackage ya da Netcad NCZ dosyasını çizime ekleyebilecek,
koordinat sisteminin nasıl taşındığını ve içe aktarmanın tek adımda nasıl geri
alındığını bileceksiniz.

## Ne yapar

Dış bir veri dosyasını okur ve **var olan çizime ekler**. Çizimdeki hiçbir şey
silinmez; dosyanın katmanları çizimin katmanlarına eklenir, aynı adlı bir katman
varsa nesneler ona konur.

İçe aktarmanın tamamı **tek bir işlemdir**: bir öğe okunamazsa o ana kadar
eklenen her şey geri alınır ve çiziminiz içe aktarmadan önceki hâlinde kalır.
Yarım aktarılmış bir veri kümesi bırakılmaz.

Bir KentOSCad proje dosyası (`.pcad`) içe aktarılmaz, **açılır**:
[AÇ](open.md) kullanın.

Hangi biçimlerin okunduğu ve neyin taşındığı:
[Dış veri biçimleri](../veri/dis-formatlar.md).

## Adlar

| Ad | Tür |
|---|---|
| `İÇEAKTAR` | Türkçe, birincil |
| `ICEAKTAR` | Türkçe karaktersiz klavye için |
| `IMPORT` | İngilizce karşılık |
| `IAKTAR` | Kısaltma |
| `core.import` | Komut kimliği |

## Sözdizimi

```text
İÇEAKTAR
İÇEAKTAR <dosya-yolu>
İÇEAKTAR <dosya-yolu> <bicim>
İÇEAKTAR dosya=<dosya-yolu> bicim=<sürücü-adı>
İÇEAKTAR dosya=<dosya-yolu> katmanlar="<ad>,<ad>,<ad>" alanlar="<alan>,<alan>"
İÇEAKTAR dosya=<dosya-yolu> alanlar=*
```

Biçim verilmezse uzantıdan bulunur. İçinde boşluk olan yol tırnak içine alınır.

## Parametreler

| Parametre | Ne işe yarar |
|---|---|
| `dosya` | İçe aktarılacak dosyanın yolu. Zorunlu |
| `bicim` | Sürücü adı: `DXF`, `GPKG` ya da `NCZ`. Verilmezse uzantıdan bulunur |
| `katmanlar` | Yalnızca bu katmanlar okunur, virgülle ayrılır. Verilmezse dosyadaki bütün katmanlar okunur |
| `alanlar` | Sütun olarak okunacak öznitelik alanları, virgülle; `*` hepsini okur. Verilmezse hiçbir alan okunmaz, yalnız geometri gelir |

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```
İÇEAKTAR <belediye-imar>.gpkg
```

Uzantı yanıltıcıysa sürücüyü açıkça söyleyin:

```
İÇEAKTAR dosya=<olcum>.txt bicim=DXF
```

Transkript şunu yazar:

```text
İçe aktarıldı: 128 nesne, 3 katman (GPKG, EPSG:5254)
  not: Dosyada 4 öznitelik alanı var; sütun olarak okumak için alanlar=* ya da alanlar="ad,ad" verin.
  not: Okunan türler: Polygon 96, Line String 32
```

Koordinat sistemi çizimden farklıysa bu bir **uyarı** olarak söylenir:

```text
İçe aktarıldı: 128 nesne, 3 katman (GPKG, EPSG:5255)
  uyarı: Dosyanın koordinat sistemi EPSG:5255, çizimin ki EPSG:5254. Koordinatlar
         dönüştürülmedi; AYAR koordinat_sistemi ile denetleyin.
  not: Okunan türler: Polygon 96, Line String 32
```

### Yalnızca istediğiniz katmanlar

Kırk katmanlı bir imar DXF'inden yalnızca ada kenarı ile kaldırımı almak için:

```
İÇEAKTAR dosya=<kontrol>.dxf katmanlar="ADAKENARI,KALDIRIM"
```

```text
İçe aktarıldı: 174 nesne, 2 katman (DXF, EPSG:5256)
  not: Dosya birim bildirmiyor; çizim metre olarak okundu (AYAR çizim_birimi). Yanlışsa
       GERİAL ile geri alın, AYAR çizim_birimi ile doğrusunu kurun ve yeniden İÇEAKTAR.
  not: Okunan türler: LWPOLYLINE 121, LINE 53
```

Katman adları **Türkçe kurallarıyla** karşılaştırılır: `kaldırım` `KALDIRIM`
katmanını bulur, `kaldirim` bulmaz — noktalı `i` ile noktasız `ı` Türkçede ayrı
harflerdir. Dosyada olmayan bir ad hata değildir, o addan hiçbir şey okunmaz.

Katman adında virgül bulunamaz (AutoCAD de izin vermez), bu yüzden ayıraç
belirsiz değildir.

**DXF ve koordinat sistemi.** DXF biçiminin koordinat sistemi için yeri yoktur.
KentOSCad önce aynı adlı `.prj` dosyasını arar; bulamazsa **çizimin kendi
sistemini varsayar ve bunu transkriptte açıkça söyler**:

```text
  not: Dosya koordinat sistemi bildirmiyor (DXF taşıyamaz). Çizimin kendi sistemi
       varsayıldı: EPSG:5256. Yanlışsa GERİAL ile geri alın, AYAR
       koordinat_sistemi ile doğrusunu kurun ve yeniden aktarın.
```

Tehlike sessizlikte olduğu için varsayım her seferinde yazılır. Koordinatlardan
bölge tahmin edilmez: 583 000 doğu değeri birden çok Türkiye diliminde
geçerlidir ve aralarında tahmin yürütmek tam olarak bu kuralın önlediği
hatadır.

### Transkriptin söylediği

Komut, okuduğu dosyada **neyi olduğu gibi aldığını, neyi eksik aldığını ve neyi
almadığını** transkriptin sonunda satır satır söyler. Her satırın başında
seviyesi vardır:

| Ön ek | Anlamı |
|---|---|
| `not:` | bilmeniz iyi olur; bir kayıp değil |
| `uyarı:` | okuyucu bir şey varsaydı; çizim yanlış yerde ya da yanlış boyda olabilir |
| `düşürme:` | dosyadaki bir şey, taşıdığından daha az bilgiyle okundu |
| `atlandı:` | dosyadaki bir şey hiç okunmadı |
| `hata:` | okuyucunun bir şeyi dışarıda bırakarak atlattığı bir arıza |

Serbest notlar sekizde durur; kalanı `… ve N not daha.` diye sayılır. Şu bilgiler
ise not değil **sayım**dır ve her zaman yazılır: varsayılan birim, kâğıt alanı
sayısı, atlanan öğe sayısı ve ilk sebebi, okunan türlerin sayımı (`Okunan
türler: LWPOLYLINE 4021, LINE 1200; parçalanan: ELLIPSE 3; atlanan: DIMENSION 27`).

**DXF birimi.** Bir DXF **her zaman** [`AYAR çizim_birimi`](setting.md)
ayarındaki birimde okunur; varsayılan metredir. Dosyanın `$INSUNITS` başlığı bu
ayarla karşılaştırılır ve **söylenir, ama asla ayarın yerine geçmez**. Başlığı
dosyayı son kaydeden program yazar ve Türkiye'deki kadastro ve imar DXF'lerinin
çoğu, sayıları metre iken başlıkta "milimetre" (kod 4) der; başlığa inanmak koca
bir ilçeyi sekiz metreye sığdırır ve her daireyi birer milimetreye yuvarlar. Üç
durum, üç satır:

```text
  not: Dosya birim bildirmiyor; çizim metre olarak okundu (AYAR çizim_birimi). Yanlışsa
       GERİAL ile geri alın, AYAR çizim_birimi ile doğrusunu kurun ve yeniden İÇEAKTAR.
  uyarı: Çizim metre olarak okundu (AYAR çizim_birimi); dosya başlığı milimetre diyor.
         Sayılar gerçekten milimetre ise GERİAL ile geri alın, AYAR çizim_birimi milimetre
         deyin ve yeniden İÇEAKTAR.
  not: Çizim santimetre olarak okundu (AYAR çizim_birimi); dosya başlığı da öyle diyor.
       Koordinatlar milimetreye ölçeklendi.
```

Başlık ile ayar metrede anlaşıyorsa hiçbir satır yazılmaz. Başlık ayarın
sunmadığı bir birim (inç, fit, kilometre…) diyorsa uyarı bunu da söyler; böyle bir
dosya kaynağında metreye çevrilip aktarılır. GeoPackage ve Shapefile'ın birimi
koordinat sisteminin metresidir; onlarda birim sorusu yoktur.

**Kâğıt alanı.** Bir DXF'in layout'larındaki antet, pafta çerçevesi ve bakış
pencereleri çizim değildir; okunmaz ve sayılır (`atlandı: 3 öğe kâğıt alanında
(layout) olduğu için atlandı`).

**DXF notları.** DXF libdxfrw ile okunur ([nasıl okunduğu](../veri/dis-formatlar.md)):
daire, yay, elips ve kısmi elips gerçek eğri; şişkinlikli çizgi yaylı çoklu çizgi;
spline, tarama, ölçü ve kılavuz çizgi kendi türleri; blok tanımları ve referansları yapısıyla;
katman rengi, kalınlığı ve durumu; nesnenin kendi rengi ve kalınlığı; sabit yükseklik
`kot` sütununa; tutamak `kaynak_kimlik` sütununa; XDATA bayt bayt.
**Düşürülerek alınanlar**, her biri `düşürme:` ile sayılır: yalnız uydurma noktalı
spline'ın uydurma noktaları kontrol noktası sayılır; katalogda olmayan tarama deseni
çizilmez; çizgi tipi ve değişen çoklu çizgi kalınlığı okunmaz; üst/orta yazı hizaları
en yakın hizaya çevrilir; dosyanın yazı stilleri bu programda olmayan bir yazı tipi
istiyorsa (`romans.shx`, `arial.ttf` …) yazılar IBM Plex Sans ile çizilir ve bu, yazı
tipleri ve kaç yazının istediğiyle birlikte söylenir — `düşürme: 12 yazının istediği
yazı tipi bu programda yok: 'romans.shx' (12 yazı). …` —, çünkü harf genişlikleri ve
satır kırılmaları kaynaktan farklı olabilir. **Çoklu kılavuzlar** (`MULTILEADER`, libdxfrw'nun
okumadığı varlık) GDAL ile okunur: her kılavuz çizgisi bir kılavuz çizgi, yazısı bir yazı
nesnesi olur; tek satırlı ve inişin hizasındaki yazı kılavuzun ucuna bağlanır —
`not: 3 MULTILEADER 3 kılavuz çizgi olarak okundu (GDAL ile); 2 yazı kılavuzun ucuna
bağlandı; 1 yazı bağlanmadı — çok satırlı, dönük ya da ucundan ayrı; yerinde duruyor.`
**Alınmayanlar**, `atlandı:` ile sayılır: sonsuz doğru, bakış
penceresi, raster resim, ağ, anonim bloklar. Bir örnek:

```text
İçe aktarıldı: 28098 nesne, 3 katman (DXF AC1015, EPSG:5256)
  düşürme: 1410 öğede köşeler farklı yüksekliklerdeydi; çizim iki boyutludur, kot yazılmadı.
  not: 33 öğenin yüksekliği (Z) `kot` sütununa yazıldı.
  atlandı: 79 öğe geometrisi kullanılamadığı için atlandı. İlki: LINE: sıfır uzunlukta çizgi
  not: Okunan türler: LINE 22444, ARC 3423, CIRCLE 2223, POINT 5, TEXT 3; atlanan: LINE 56, ARC 7
```

### Öznitelik alanları

Bir Shapefile ya da GeoPackage geometrinin yanında bir tablo taşır: `ada`, `parsel`,
`nitelik`, `alan_m2`. `alanlar` bu alanların hangilerinin çizimde **sütun** olacağını
söyler; her alan katmanına özel bir sütun olarak tanımlanır ve her nesne kendi
değerini alır:

```text
İÇEAKTAR dosya="kadastro.gpkg" alanlar="ada,parsel"
İÇEAKTAR dosya="kadastro.gpkg" alanlar=*
```

Alan türleri sütun türüne şöyle çevrilir: tam sayı → `tam_sayi`, ondalık → `ondalik`
(dosyanın bildirdiği basamak sayısıyla, bildirmezse iki), tarih → `tarih`, geri kalanı
`metin`. Sütun kimliği alan adının küçük harfe indirilmiş biçimidir: `ADA_NO` → `ada_no`.
Belgede aynı kimlikte ve aynı türde bir sütun varsa yeniden kullanılır; türü farklıysa
alan okunmaz ve rapor bunu söyler. Bir DXF'in alanları (katman adı, çizgi tipi, tutamak)
OGR'ın kendi defter kayıtlarıdır ve hiçbir zaman sunulmaz.

`alanlar` verilmezse dosyanın kaç alanı olduğu raporda yazılır, böylece bir şeyin
okunmadığı sessizce geçmez.

### Netcad NCZ

Netcad'in kendi çizim dosyası (`.ncz`) uzantısından tanınır ve doğrudan okunur; uzantısı
yanıltıcı bir dosyayı `bicim=NCZ` NCZ diye açar. Nesneler katmanlarıyla, renkleriyle ve çizgi
kalınlıklarıyla gelir. Koordinatlar **dönüştürülmez**: çizimin sisteminde okunur ve dosyanın
bildirdiği dilim çizimin diliminden farklıysa uyarı yazılır; bu yüzden içe aktarmadan önce
`AYAR koordinat_sistemi` ile çizimin sistemini kurun. NCZ'de `alanlar` dosyadan gelen bir liste
değil, okuyucunun sabit on yedi alanının adıdır (`layer_name`, `entity_type`, `label`…); `*`
hepsini sütun yapar.

```
İÇEAKTAR "plan.ncz"
İÇEAKTAR dosya="plan.ncz" katmanlar="PINDEX_1000" alanlar="layer_name,label"
```

Nesne eşlemesi, akıllı nesneler, pafta çerçeveleri, koordinat sistemi ve raporun her satırı:
[Netcad NCZ çizimleri](../veri/netcad-ncz.md).

### Arayüz

**KentOS CAD ▸ İçe Aktar…** ya da şeritteki **Harita ▸ Veri ▸ İçe Aktar…** **İçe Aktar
penceresini** açar. Pencere tek sayfadır: üstte dosya, ortada dosyanın çizimi, sağda katmanlar
ve alanlar, altta pencerenin çalıştıracağı komut satırı.

**Dosya.** Yolu üstteki **Dosya** kutusuna yazın, **Gözat…** ile seçin ya da dosyayı
Finder'dan veya Dosya Gezgini'nden pencerenin üstüne bırakın; bıraktığınız yerin çerçevesi o
sırada mavileşir. Kutunun altında dosyanın biçimi, boyutu ve son değişiklik tarihi yazar.
Dosya seçilmeden önce ortada ne yapılacağı yazar, sağ sütunda ise bu yapının okuyabildiği
biçimler, her birinin yanında **okunur**, **okunur, yazılır** ya da **okunmaz** etiketiyle
listelenir.

**Okunamayan bir dosya seçtiğinizde bunu orada söyler.** Bir `.dwg` (bu yapı DWG
okumuyorsa) ya da listede olmayan bir uzantı seçerseniz ortada ne yapmanız gerektiğini yazan
bir uyarı çıkar ve dosya okunmaz.

**Okunabilen bir dosya hemen okunur.** Bir düğmeye basmanız gerekmez: yazarken okuma,
yazmayı bıraktığınızda başlar. Okuma sürerken dosyanın adı ve geçen süre yazılır;
**Okumayı durdur** okumayı gerçekten durdurur, **Yeniden oku** aynı dosyayı baştan okur. Bu
adımda çizime **hiçbir şey eklenmez**: dosya yalnızca pencerenin kendi önizlemesine okunur.

**Çizim.** Okuma bitince dosyanın çizimi pencerenin ortasını doldurur. Tekerlekle yakınlaşır,
sürüklemeyle kayar. Çizimin üstünde **okuyucunun uyarıları** durur — her biri kendi
şeridinde, kalın satırı olan ve altında ne yapılacağını yazan. En çok gerekeni ilk sıradadır:
*dosya koordinat sistemi bildirmiyor*. Bir DXF koordinat sistemi taşıyamaz, o yüzden çizimin
kendi sistemi varsayılır — ve TM30 ile TM33'ü karıştırmak sessizce yanlış bir tapu üretir,
bu yüzden varsayım küçük yazıyla değil uyarı olarak durur. Dosyanın kendi olguları (sürücü,
nesne ve katman sayısı, sistem, okunan türler) çizimin **altındaki** sessiz şeritte kalır:
olgu olgudur, uyarı uyarıdır.

**Katmanlar.** Sağ sütunun **Katmanlar** bölmesinde her satır katmanın adını ve o katmandan
kaç nesne geleceğini yazar. Hepsi işaretli gelir. Satırın herhangi bir yerine tıklamak ya da
satırdayken **Boşluk**'a basmak işareti değiştirir; işaretini kaldırdığınız katman ortadaki
çizimden de kalkar, böylece ne aldığınızı almadan önce görürsünüz. **Tümü** ve **Hiçbiri**
görünen satırlara uygulanır — arama kutusuna bir şey yazdıysanız yalnızca süzgeçten geçen
katmanları etkilerler. Altta kaç katmanın ve kaç nesnenin aktarılacağı yazar.

**Alanlar.** **Alanlar** bölmesi dosyanın öznitelik alanlarını **adıyla, birer kez**
listeler: satırın üstünde alanın adı, altında olacağı sütun türü, ilk değeri ve hangi
katmandan geldiği (birkaç katmanda geçiyorsa kaç katmanda geçtiği) yazar. Birkaç katmanda
geçen bir alan tek sütun olur, bu yüzden tek satırdır. İşaretlediğiniz alanlar sütun olarak
okunur; **Hiçbiri** yalnız geometriyi getirir. Alanı olmayan bir dosyada bölme bunu söyler.

**Komut ve İçe aktar.** Pencerenin en altında, işaretlediğiniz katmanlar ve alanlarla
kurulmuş `İÇEAKTAR` satırı yazar ve her işaretle güncellenir: bütün katmanlar işaretliyse
`katmanlar` yazılmaz, bütün alanlar işaretliyse `alanlar=*` yazılır. **İçe aktar** bu satırı
çalıştırır. Pencere yalnızca argüman toplar: kurduğu satır, aynı işi bir betikte yazacağınız
satırın tıpatıp aynısıdır; en az bir katman işaretli değilse düğme kapalı durur.

**Okuma pencereyi dondurmaz.** Dosya ayrı bir iş parçacığında okunur; bu sürede
durum çubuğunda `İçe aktarılıyor: <dosya>` yazısı, altında kayan bir şerit ve
yanında **Durdur** çipi görünür. **Durdur** (ya da **Esc**) okumayı keser: çizime
hiçbir şey eklenmez ve transkript `İçe aktarma durduruldu; çizim değişmedi.` der.
**Enter** ya da sağ tık okumayı durdurmaz. Okuma sürerken çizim değiştirilemez (bkz.
[Uzun işler](../baslangic/arayuz.md#uzun-işler)). Okuma bitince nesneler çizime tek
seferde, tek geri alma adımı olarak girer.
Komut satırından yazılan `İÇEAKTAR` da aynı yolu izler; bir betiğin içindeki
`core.import` ise betiğin kendi sırasında, bekleyerek okur — iki yol da aynı çizimi
ve aynı günlük satırını üretir.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.import", "args": { "dosya": "veri/belediye-imar.gpkg" } }
```

Sürücüyü açıkça vererek:

```json
{ "cmd": "core.import", "args": { "dosya": "veri/olcum.dxf", "bicim": "DXF" } }
```

Sihirbazın kurduğu satırın betik karşılığı:

```json
{ "cmd": "core.import",
  "args": { "dosya": "veri/kontrol.dxf", "katmanlar": "ADAKENARI,KALDIRIM" } }
```

## Geri alma

İçe aktarmanın tamamı **tek bir geri alma adımıdır**. Yüz yirmi sekiz nesne
eklenmiş olsa bile tek `GERİAL` hepsini kaldırır:

```text
İÇEAKTAR belediye-imar.gpkg     ← 128 nesne, 3 katman
GERİAL                          ← hepsi kalkar
YİNELE                          ← hepsi geri gelir
```

İçe aktarma başarısız olursa geri alacak bir şey kalmaz: hata anında her şey
zaten geri alınmıştır.

`GERİAL` içe aktarmanın oluşturduğu **boş katmanları kaldırmaz**. Boş bir katman
zararsızdır ve kaldırılması saklanmış katman numaralarını geçersiz kılardı.

## Betikten kullanım

`İÇEAKTAR` betiklenebilirdir. Bir betiğin içindeki içe aktarma, betiğin geri
kalanıyla birlikte **tek bir geri alma adımına** katılır.

Sanal dosya sistemi yolları (`/vsicurl/`, `/vsis3/`, `/vsizip/`) reddedilir.
Bir dosya adı, ağ isteğine ya da arşiv içine erişime dönüşemez — komut
satırından, betikten ya da yapay zekâ önerisinden gelmiş olması fark etmez.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `io.no_driver: '...' için sürücü bulunamadı.` | Uzantı izin listesinde değil | `bicim` ile sürücüyü söyleyin ya da biçimi çevirin |
| `io.no_driver: ... sürücüsü okuma için açık değil.` | Biçim yalnız yazılıyor | Desteklenen bir biçime çevirin |
| `io.no_driver: Dış biçim desteği KAPALI.` | GDAL olmadan derlenmiş yapı | Mesajdaki kurulum komutunu izleyin |
| `'...' açılamadı: ...` | Dosya yok, okunamıyor ya da bozuk | Yolu ve izinleri denetleyin |
| `İçe aktarma iptal edildi; çizim değişmedi.` | Okuma sürerken durduruldu | İçe Aktar penceresinde **Yeniden oku**'ya basın ya da komutu yeniden çalıştırın |
| `'...' dosyasının '...' katmanı içe alınmadı. '...' coğrafi bir koordinat sistemi …` | Katman koordinatlarını derece (ya da metre dışında bir birimle) sayıyor | Dosyayı metre sayan bir sisteme dönüştürüp yeniden alın: `ogr2ogr -t_srs EPSG:5256 yeni.gpkg eski.gpkg`. Ayrıntı: [Koordinat sisteminin birimi](../veri/koordinat-sistemleri.md#koordinat-sisteminin-birimi-yalnız-metre) |
| `'...' okunabilir çizgi ya da alan içermiyor; … büyük olasılıkla boylam ve enlem (derece) …` | Sistem bildirmeyen dosyanın derece sayıları metre okunup ezildi | Dosyanın sistemini bulup dönüştürün ve yeniden aktarın |
| `'...' katmanı hiçbir koordinat sistemi bildirmiyor.` | Veri kümesi etiketsiz | Yanına aynı adlı bir `.prj` dosyası koyun |
| `'...' içindeki katmanlar farklı koordinat sistemleri bildiriyor` | Karışık veri kümesi | Tek bir sisteme dönüştürüp yeniden deneyin |
| `'...' okunabilir çizgi ya da alan içermiyor` | Dosyada çizgi, alan, nokta ya da yazı yok; ya da hepsi kâğıt alanında | Dosyayı bir CAD programında açıp model alanında ne olduğuna bakın |
| `'...' içindeki N. öğe okunamadı: ...` | Geometri doğrulamayı geçemedi | Mesajın devamı sebebi söyler; kaynak veriyi düzeltin |
| `'...' sanal dosya sistemi yolu.` | `/vsi...` ile başlayan yol | Dosyayı diske alıp yeniden deneyin |
| `'...' bir KentOSCad proje dosyası. Proje dosyası açılır, içe aktarılmaz: AÇ komutunu kullanın.` | `.pcad` içe aktarılmaya çalışıldı | [AÇ](open.md) kullanın |
| `'...' katmanı kilitli.` | Hedef katman kilitli | Katmanın kilidini açın |
| `Dosya motoru bağlı değil; bu ortamda dosya açılıp kaydedilemez.` | Dosya motoru olmayan bir ortam | Uygulama içinden çalıştırın |
| `Dosya coğrafi koordinatlarda (…) ve bütün koordinatları derece aralığında. …` | Bir Netcad NCZ dosyası derece sayan coğrafi bir sistem bildiriyor | Netcad'de bir TM ya da UTM dilimine dönüştürüp yeniden kaydedin ([Netcad NCZ](../veri/netcad-ncz.md#koordinat-sistemi)) |
| `'…' okunabilir geometri içermiyor; çizime hiçbir şey eklenmedi.` | Bir Netcad NCZ dosyasından hiçbir nesne okunamadı: dosya bozuk, çizim boş ya da `katmanlar=` hiçbir katmanla eşleşmedi | [Netcad NCZ ▸ Hatalar](../veri/netcad-ncz.md#rapor-satırları-ve-hatalar) |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## Bozuk öğeler atlanır, sayılır ve söylenir

Gerçek bir çizim bozuk öğe taşır. Bir CAD programının bıraktığı, elli beş
noktasının hepsi aynı yerde olan bir çokgen; sıfır uzunlukta bir çizgi; üç
köşesi olmayan bir alan. Bunlar **atlanır**, çizimin geri kalanı okunur ve
transkript kaç tanesinin neden atlandığını yazar:

```text
İçe aktarıldı: 71 820 nesne, 112 katman (DXF, EPSG:5256)
  atlandı: 3 öğe geometrisi kullanılamadığı için atlandı. İlki: 1. halka dış halka
           en az 3 tepe noktası ister, verilen: 1
```

Bir öğe yüzünden bütün dosyayı reddetmek doğru değildir: 48 MB'lık bir kadastro
çiziminde tek bozuk çokgen, 18 497 sağlam nesnenin de çöpe gitmesi demekti.
Kayıp **sessiz de değildir** — atlanan her şey sayılır ve sebebi yazılır.

Dosyanın tamamı okunamıyorsa (bozuk başlık, tanınmayan biçim) durum farklıdır:
o zaman içe aktarma **tamamen** başarısız olur ve çizim değişmez.
