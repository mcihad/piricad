# DIŞAAKTAR — Dış Biçime Yazma

Çizimini başka bir programa ya da kuruma teslim eden kullanıcı için; bu sayfayı
bitirdiğinizde çizimi DXF ya da GeoPackage olarak yazabilecek, koordinat
sisteminin nasıl taşındığını ve neyin aktarılmadığını bileceksiniz.

## Ne yapar

Çizimdeki **görünür ve silinmemiş** nesneleri, verdiğiniz yola dış bir veri
biçiminde yazar. Her KentOSCad katmanı hedef dosyada bir katman olur.

Dışa aktarma **kayıplıdır**. Öznitelikler, nesne başına stil ve kalıcı nesne
anahtarları aktarılmaz. Çalışma dosyanız her zaman
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
  not: Öznitelik ve stil bilgisi bu sürümde yazılmadı; yalnız geometri ve katman adı aktarıldı.
```

DXF'e yazarken KentOSCad yanına bir `.prj` dosyası da koyar ve bunu söyler:

```text
Dışa aktarıldı: ada12-teslim.dxf  (14 öğe, 3 katman, DXF)
  not: DXF biçimi koordinat sistemi taşımaz; sistem 'ada12-teslim.prj' dosyasına
       yazıldı. Çizimi taşırken bu dosyayı da götürün, yoksa koordinatlar etiketsiz kalır.
  not: Öznitelik ve stil bilgisi bu sürümde yazılmadı; yalnız geometri ve katman adı aktarıldı.
```

**İki dosyayı da teslim edin.** `.prj` olmadan DXF'iniz etiketsiz koordinat
taşır ve KentOSCad dâhil hiçbir program hangi projeksiyonda olduğunu bilemez.

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
