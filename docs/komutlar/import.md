# İÇEAKTAR — Dış Veri Alma

Başka bir programdan ya da kurumdan veri alan kullanıcı için; bu sayfayı
bitirdiğinizde bir DXF ya da GeoPackage dosyasını çizime ekleyebilecek,
koordinat sisteminin nasıl taşındığını ve içe aktarmanın tek adımda nasıl geri
alındığını bileceksiniz.

## Ne yapar

Dış bir veri dosyasını okur ve **var olan çizime ekler**. Çizimdeki hiçbir şey
silinmez; dosyanın katmanları çizimin katmanlarına eklenir, aynı adlı bir katman
varsa nesneler ona konur.

İçe aktarmanın tamamı **tek bir işlemdir**: bir öğe okunamazsa o ana kadar
eklenen her şey geri alınır ve çiziminiz içe aktarmadan önceki hâlinde kalır.
Yarım aktarılmış bir veri kümesi bırakılmaz.

Bir PiriCAD proje dosyası (`.pcad`) içe aktarılmaz, **açılır**:
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
```

Biçim verilmezse uzantıdan bulunur. İçinde boşluk olan yol tırnak içine alınır.

## Parametreler

| Parametre | Ne işe yarar |
|---|---|
| `dosya` | İçe aktarılacak dosyanın yolu. Zorunlu |
| `bicim` | Sürücü adı: `DXF` ya da `GPKG`. Verilmezse uzantıdan bulunur |

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
  not: Öznitelikler bu sürümde okunmadı; belge modeli öznitelik sütunlarını Faz 1'de kazanacak.
```

Koordinat sistemi çizimden farklıysa bu da söylenir:

```text
İçe aktarıldı: 128 nesne, 3 katman (GPKG, EPSG:5255)
  not: Dosyanın koordinat sistemi EPSG:5255, çizimin ki EPSG:5254. Koordinatlar
       dönüştürülmedi; AYAR koordinat_sistemi ile denetleyin.
```

**DXF için `.prj` dosyasını unutmayın.** DXF biçiminin koordinat sistemi için
yeri yoktur; PiriCAD aynı adlı `.prj` dosyasını arar ve bulamazsa dosyayı
reddeder. Etiketsiz koordinat kabul edilmez.

### Arayüz

**Dosya > İçe Aktar…** menüsü veya **Dosya** araç çubuğundaki **İçe Aktar**
düğmesi, izin verilen biçimlerle süzülmüş bir dosya seçme penceresi açar.
Pencere yalnızca argümanı toplar.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.import", "args": { "dosya": "veri/belediye-imar.gpkg" } }
```

Sürücüyü açıkça vererek:

```json
{ "cmd": "core.import", "args": { "dosya": "veri/olcum.dxf", "bicim": "DXF" } }
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
| `'...' katmanı hiçbir koordinat sistemi bildirmiyor.` | Veri kümesi etiketsiz | Yanına aynı adlı bir `.prj` dosyası koyun |
| `'...' içindeki katmanlar farklı koordinat sistemleri bildiriyor` | Karışık veri kümesi | Tek bir sisteme dönüştürüp yeniden deneyin |
| `'...' okunabilir çizgi ya da alan içermiyor` | Desteklenen geometri yok | Nokta ve eğriler bu sürümde okunmuyor |
| `'...' içindeki N. öğe okunamadı: ...` | Geometri doğrulamayı geçemedi | Mesajın devamı sebebi söyler; kaynak veriyi düzeltin |
| `'...' sanal dosya sistemi yolu.` | `/vsi...` ile başlayan yol | Dosyayı diske alıp yeniden deneyin |
| `'...' bir PiriCAD proje dosyası. Proje dosyası açılır, içe aktarılmaz: AÇ komutunu kullanın.` | `.pcad` içe aktarılmaya çalışıldı | [AÇ](open.md) kullanın |
| `'...' katmanı kilitli.` | Hedef katman kilitli | Katmanın kilidini açın |
| `Dosya motoru bağlı değil; bu ortamda dosya açılıp kaydedilemez.` | Dosya motoru olmayan bir ortam | Uygulama içinden çalıştırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
