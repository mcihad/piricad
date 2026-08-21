# Dış Veri Biçimleri

Başka bir programdan veri alan ya da başka bir kuruma veri teslim eden kullanıcı
için; bu sayfayı bitirdiğinizde hangi biçimlerin okunup yazıldığını, neyin
aktarıldığını, neyin aktarılmadığını ve koordinat sisteminin nasıl taşındığını
bileceksiniz.

Komutlar: [İÇEAKTAR](../komutlar/import.md), [DIŞAAKTAR](../komutlar/export.md).
Kendi proje dosyanız için: [PiriCAD proje dosyası](proje-dosyasi.md).

## Bu sürümde çalışan biçimler

| Biçim | Uzantı | Okuma | Yazma |
|---|---|---|---|
| AutoCAD DXF | `.dxf` | evet | evet |
| OGC GeoPackage | `.gpkg` | evet | evet |

Bu liste kasten kısadır. PiriCAD'in altındaki GDAL kütüphanesi yüzden fazla biçim
tanır; PiriCAD bunların yalnızca **açıkça izin verilenlerini** açar. Bir dosya
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
| Shapefile, GeoJSON | Faz 1 | İzin listesine eklenmeleri için fuzz koşumu ve gidiş-dönüş sınaması gerekiyor |
| WMS, WMTS, WFS-T, WCS | Faz 2 | Servis istemcileri kendi uygunluk sınamalarıyla gelecek |

PiriCAD **hiçbir zaman** ODA Drawings SDK kullanmayacaktır; kapalı kaynaklıdır ve
projenin GPLv3 lisansıyla bağdaşmaz.

## Koordinat sistemi

**Etiketsiz koordinat kabul edilmez.** İçe aktarılan bir veri kümesi koordinat
sistemini bildirmiyorsa PiriCAD dosyayı reddeder; "herhâlde TUREF/TM30'dur"
varsayımı yapmaz.

Sebebi saha kökenlidir: TM30 ile TM33 karışması sessizdir. Koordinatlar makul
görünür, çizim makul görünür, ve hata ancak tapuya gittiğinde ortaya çıkar.

| Biçim | Koordinat sistemini nasıl taşır |
|---|---|
| GeoPackage | Dosyanın içinde. Ek bir şey gerekmez |
| DXF | **Taşımaz.** Yanındaki aynı adlı `.prj` dosyasından okunur |

DXF'in koordinat sistemi için yeri yoktur — bu biçimin kendi eksiğidir, PiriCAD'in
değil. Bu yüzden:

- **Dışa aktarırken** PiriCAD `.dxf` ile birlikte bir `.prj` dosyası yazar ve size
  söyler. Çizimi taşırken **iki dosyayı da götürün**.
- **İçe aktarırken** PiriCAD aynı adlı `.prj` dosyasını arar. Yoksa dosyayı
  reddeder ve `.prj` koymanızı ister.

`.prj`, ülkedeki her CBS yazılımının anladığı ESRI biçiminde yazılır.

İçe aktarılan verinin koordinat sistemi çizimin kendi sisteminden farklıysa
PiriCAD **koordinatları dönüştürmez**; farkı söyler ve kararı size bırakır.
Sessiz bir yeniden projeksiyon, yanlış yere oturmuş bir parselin en kolay yoludur.

Çizimin koordinat sistemini `AYAR koordinat_sistemi` ile bildirin:

```
AYAR koordinat_sistemi EPSG:5254
```

## Ne aktarılır, ne aktarılmaz

| Aktarılan | Aktarılmayan |
|---|---|
| Çizgi ve alan geometrisi, milimetre hassasiyetiyle | Öznitelikler — belge modeli öznitelik sütunlarını Faz 1'de kazanacak |
| Katman adları | Katman rengi, çizgi tipi, ölçek sınırları |
| Boşluklu ve çok parçalı alanlar | Nesne başına stil |
| Koordinat sistemi | Nesne ve katman anahtarları |

Bu yüzden **çalışma dosyanız `.pcad` olmalıdır**. DXF ve GeoPackage teslim
biçimleridir; bir dışa aktarıp geri alma turu çiziminizi olduğu gibi geri
getirmez.

Desteklenmeyen bir geometri türüyle karşılaşılırsa (nokta, çoklu nokta, eğri)
o öğe atlanır ve kaç tanesinin atlandığı transkriptte söylenir. Sessizce
düşürülmez.

## Ağdan veri okunmaz

`/vsicurl/`, `/vsis3/`, `/vsizip/` gibi sanal dosya sistemi yolları reddedilir.
Bir veri dosyasının adı, ağ isteğine ya da arşiv içine erişime dönüşemez —
komut satırından, betikten ya da yapay zekâ önerisinden gelmiş olması fark
etmez. Dosyayı diske indirip öyle açın.

## Dış biçim desteği kapalıysa

PiriCAD, GDAL kütüphanesi olmadan da derlenebilir. O yapıda `İÇEAKTAR` ve
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
