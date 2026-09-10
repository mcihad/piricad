# DIŞAAKTAR — Dış Biçime Yazma

Çizimini başka bir programa ya da kuruma teslim eden kullanıcı için; bu sayfayı
bitirdiğinizde çizimi DXF ya da GeoPackage olarak yazabilecek, koordinat
sisteminin nasıl taşındığını ve neyin aktarılmadığını bileceksiniz.

## Ne yapar

Çizimdeki **görünür ve silinmemiş** nesneleri, verdiğiniz yola dış bir veri
biçiminde yazar. Her KentOSCad katmanı hedef dosyada bir katman olur.

Dışa aktarma **biçime göre kayıplıdır**. GeoPackage'a öznitelik sütunları alan
olarak, her nesnenin türü ve kalıcı anahtarı, yazının metni, yüksekliği, açısı ve
hizası da yazılır; daire, yay ve elips ekranda göründükleri çokgen olarak gider.
DXF'e (libdxfrw ile) her tür kendi varlığı olarak gider: daire `CIRCLE`, yay `ARC`,
elips ve kısmi elips `ELLIPSE`, nokta `POINT`, yazı `TEXT`, çizgi ve alan
`LWPOLYLINE`, yaylı çoklu çizgi şişkinlikli `LWPOLYLINE`, spline `SPLINE`, tarama
`HATCH`, blok tanımları `BLOCK` ve referansları `INSERT`, ölçü `DIMENSION` (stili
`DIMSTYLE` tablosuna), lider `LEADER`; katmanlar rengi, kalınlığı, görünürlüğü ve
kilidiyle; nesnenin kendi rengi ve kalınlığı; öznitelikler `KENTOSCAD` uygulama
verisi (XDATA) olarak — KentOSCad geri okurken sütunlarına döner; başka programın
XDATA'sı geldiği gibi. Çizgi tipleri bu sürümde yazılmaz ve söylenir. Çalışma
dosyanız her zaman
[`.pcad`](../veri/proje-dosyasi.md) olmalıdır; dış biçimler teslim içindir.

Hangi biçimlerin yazıldığı ve neyin taşındığı:
[Dış veri biçimleri](../veri/dis-formatlar.md).

## Adlar

| Ad | Tür |
|---|---|
| `DIŞAAKTAR` | Türkçe, birincil |
| `DISAAKTAR` | Türkçe karaktersiz klavye için |
| `EXPORT` | İngilizce karşılık |
| `DAKTAR` | Kısaltma |
| `core.export` | Komut kimliği |

## Sözdizimi

```text
DIŞAAKTAR
DIŞAAKTAR <dosya-yolu>
DIŞAAKTAR <dosya-yolu> <bicim>
DIŞAAKTAR dosya=<dosya-yolu> bicim=<sürücü-adı>
```

Biçim verilmezse uzantıdan bulunur. İçinde boşluk olan yol tırnak içine alınır.

## Parametreler

| Parametre | Ne işe yarar |
|---|---|
| `dosya` | Yazılacak dosyanın yolu. Zorunlu |
| `bicim` | Sürücü adı: `DXF` ya da `GPKG`. Verilmezse uzantıdan bulunur |

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Önce çizimin koordinat sistemini bildirin — etiketsiz koordinat dışa
aktarılamaz:

```
AYAR koordinat_sistemi EPSG:5254
```

Sonra yazın:

```
DIŞAAKTAR <ada12-teslim>.gpkg
```

Transkript şunu yazar:

```text
Dışa aktarıldı: ada12-teslim.gpkg  (14 öğe, 3 katman, GPKG)
  not: 2 öznitelik sütunu alan olarak yazıldı; stil bilgisi yazılmadı.
```

DXF'e yazarken KentOSCad yanına bir `.prj` dosyası da koyar ve bunu söyler:

```text
Dışa aktarıldı: ada12-teslim.dxf  (14 nesne, 3 katman, DXF AC1021, EPSG:5256)
  not: Çizgi tipleri bu sürümde DXF'e yazılmadı; her katman CONTINUOUS.
```

DXF sürümü `surum=` ile seçilir: `2000`, `2004`, `2007` (varsayılan), `2010`, `2013`,
`2018`. 2007 ve sonrası UTF-8'dir; daha eski bir sürüm istenirse dosyaya
`$DWGCODEPAGE ANSI_1254` yazılır ki Türkçe harfler AutoCAD'de doğru çıksın.

```
DIŞAAKTAR dosya="ada12-teslim.dxf" surum=2000
```

**İki dosyayı da teslim edin.** `.prj` olmadan DXF'iniz etiketsiz koordinat
taşır ve KentOSCad dâhil hiçbir program hangi projeksiyonda olduğunu bilemez.

### Daire, yay, elips, nokta ve yazı nasıl yazılır

Dış biçimlerin dairesi, yayı ya da yazısı yoktur; KentOSCad bunları **nesnenin
türüne göre** yazar. GeoPackage'da her katmanın tablosu şu alanları taşır:

| Alan | İçeriği |
|---|---|
| `tur` | `core.polyline`, `core.circle`, `core.arc`, `core.ellipse`, `core.point`, `core.text` |
| `anahtar` | nesnenin kalıcı anahtarı |
| `yazi`, `yukseklik_mm`, `aci`, `hizalama` | yazının metni, zemin milimetresi yüksekliği, saat yönünün tersine açısı, hizası |
| çizimdeki her öznitelik sütunu | kendi adıyla, şemadaki türüyle (tam sayı, ondalık, tarih…) |

Daire ve elips çokgen, yay çizgi olarak gider; yazı taban çizgisinin başındaki
nokta olarak. Bu dosyayı KentOSCad geri okurken `tur` alanını tanır: daire daire,
yay yay, yazı yazı olarak geri gelir (elips bugün alan olarak gelir ve bunu söyler).

DXF'te alan yoktur: yazı `TEXT` olarak yüksekliği, açısı ve hizasıyla, parsel kapalı
`LWPOLYLINE` olarak (boşluğu kendi kapalı `LWPOLYLINE`'ı), daire `CIRCLE`, yay `ARC`,
elips `ELLIPSE`, nokta `POINT`, yaylı çoklu çizgi şişkinlikli `LWPOLYLINE`, spline
`SPLINE`, tarama sınır döngüleri ve desen adıyla `HATCH` (desen tanım çizgileri
yazılmaz; AutoCAD deseni adıyla bulur), blok tanımı üyeleriyle `BLOCK`, referansı
`INSERT`, ölçü türüyle `DIMENSION`, lider `LEADER` olarak yazılır. Koordinatlar
[`AYAR çizim_birimi`](setting.md) ayarındaki birimde yazılır ve `$INSUNITS` başlığa
işlenir. Öznitelikler her nesnenin `KENTOSCAD` uygulama verisine `ada#0=12` biçiminde
(ad, sütun türü, değer) yazılır; KentOSCad bu dosyayı geri okurken sütunu yoksa
kurar ve değeri yerine koyar.

### Arayüz

**Dosya > Dışa Aktar…** menüsü, **Dosya** araç çubuğundaki **Dışa Aktar** düğmesi
ve öznitelik tablosunun araç satırındaki **Dışa aktar** işareti aynı
[Dışa Aktar](../baslangic/disa-aktarma.md) penceresini açar: solda yazılabilen
biçimler, sağda dosya, altta pencerenin çalıştıracağı `DIŞAAKTAR` satırı. Pencere
yalnızca argümanları toplar; dosyayı komut yazar.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.export", "args": { "dosya": "teslim/ada12.gpkg" } }
```

Sürücüyü açıkça vererek:

```json
{ "cmd": "core.export", "args": { "dosya": "teslim/ada12.dxf", "bicim": "DXF" } }
```

## Geri alma

`DIŞAAKTAR` çizimi değiştirmez, dolayısıyla geri alınacak bir şey yoktur ve geri
alma yığınına girmez. Yazılmış bir dosyayı geri almanın yolu yoktur; yanlış
yazdıysanız doğrusunu yazın.

## Betikten kullanım

`DIŞAAKTAR` betiklenebilirdir ve salt okunur işaretlidir. Günlüğe yazılmaz: bir
oturumu yeniden oynatmak, o oturumdaki her dışa aktarmayı yeniden yapmamalıdır.

Toplu teslim üretiminde tipik kullanım, betiğin sonunda tek çağrıdır.

Sanal dosya sistemi yolları (`/vsicurl/`, `/vsis3/`) reddedilir: KentOSCad ağa ya
da arşivin içine yazmaz.

`DIŞAAKTAR` yapay zekâya kapalıdır: bir öneri, kullanıcının diskinde dosya
oluşturmamalıdır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Çizimin koordinat sistemi belirsiz.` | Proje ayarı boş | `AYAR koordinat_sistemi EPSG:5254` |
| `Çizimin koordinat sistemi '...' dışa aktarım için çözülemedi.` | Ayar bir EPSG kodu değil | EPSG kodu verin, örnek `EPSG:5254` |
| `io.no_driver: '...' için sürücü bulunamadı.` | Uzantı izin listesinde değil | `bicim` ile sürücüyü söyleyin |
| `io.no_driver: ... sürücüsü yazma için açık değil. DWG için DXF dışa aktarıp dönüştürün` | Biçim yalnız okunuyor | DXF yazıp dönüştürün |
| `io.no_driver: Dış biçim desteği KAPALI.` | GDAL olmadan derlenmiş yapı | Mesajdaki kurulum komutunu izleyin |
| `io.no_driver: GDAL '...' sürücüsünü tanımıyor.` | GDAL bu sürücü olmadan derlenmiş | GDAL kurulumunu denetleyin |
| `'...' oluşturulamadı: ... Dizin izinlerini ve boş alanı denetleyin.` | İzin yok ya da disk dolu | İzinleri ve yeri denetleyin |
| `'...' katmanı yazılamadı: ...` | Sürücü katmanı kabul etmedi | Katman adında sürücünün kabul etmediği bir karakter olabilir |
| `Çizimde dışa aktarılacak nesne yok; '...' yazılmadı.` | Çizim boş | Önce çizin |
| `'...' sanal dosya sistemi yolu. KentOSCad ağa ya da arşivin içine yazmaz.` | `/vsi...` ile başlayan yol | Yerel bir yol verin |
| `'...' bir KentOSCad proje dosyası uzantısı taşıyor.` | `.pcad` dışa aktarılmaya çalışıldı | [FARKLIKAYDET](saveas.md) kullanın |
| `'...' yazılamadı. Koordinat sistemi olmayan bir dışa aktarım eksik veridir` | `.prj` yazılamadı | Dizin izinlerini denetleyin |
| `Dosya motoru bağlı değil; bu ortamda dosya açılıp kaydedilemez.` | Dosya motoru olmayan bir ortam | Uygulama içinden çalıştırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
