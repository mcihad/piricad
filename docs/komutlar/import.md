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
İÇEAKTAR dosya=<dosya-yolu> katmanlar="<ad>,<ad>,<ad>"
```

Biçim verilmezse uzantıdan bulunur. İçinde boşluk olan yol tırnak içine alınır.

## Parametreler

| Parametre | Ne işe yarar |
|---|---|
| `dosya` | İçe aktarılacak dosyanın yolu. Zorunlu |
| `bicim` | Sürücü adı: `DXF` ya da `GPKG`. Verilmezse uzantıdan bulunur |
| `katmanlar` | Yalnızca bu katmanlar okunur, virgülle ayrılır. Verilmezse dosyadaki bütün katmanlar okunur |

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

### Yalnızca istediğiniz katmanlar

Kırk katmanlı bir imar DXF'inden yalnızca ada kenarı ile kaldırımı almak için:

```
İÇEAKTAR dosya=<kontrol>.dxf katmanlar="ADAKENARI,KALDIRIM"
```

```text
İçe aktarıldı: 174 nesne, 2 katman (DXF, EPSG:5256)
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

### Arayüz

**Dosya > İçe Aktar…** menüsü veya **Dosya** araç çubuğundaki **İçe Aktar**
düğmesi **iki adımlı içe aktarma sihirbazını** açar.

**1 · DOSYA.** Yolu yazın ya da **Gözat…** ile seçin. Sayfa dosyanın biçimini,
boyutunu ve son değişiklik tarihini gösterir; altında bu yapının okuyabildiği
bütün biçimler listelenir. **İleri**'ye bastığınızda dosya okunur — okuma
sürerken geçen süre yazılır ve **Okumayı durdur** ile okuma gerçekten
durdurulur. Bu adımda çizime **hiçbir şey eklenmez**.

**2 · KATMANLAR.** Solda dosyanın çizimi, sağda katman listesi. Her satırda
katmanın adı ve o katmandan kaç nesne geleceği yazar; kutucuğu kaldırdığınız
katman soldaki çizimden de kalkar, böylece ne aldığınızı almadan önce
görürsünüz. **Tümü** ve **Hiçbiri** bağlantıları görünen satırlara uygulanır —
arama kutusuna bir şey yazdıysanız yalnızca süzgeçten geçen katmanları
etkilerler. Çizim tekerlekle yakınlaşır, sürüklemeyle kayar.

**İçe Aktar**, işaretlediğiniz katmanlarla tek bir `İÇEAKTAR` satırı kurar ve
onu çalıştırır. Pencere yalnızca argüman toplar: kurduğu satır, aynı işi bir
betikte yazacağınız satırın tıpatıp aynısıdır.

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
| `İçe aktarma iptal edildi; çizim değişmedi.` | Sihirbazda **Okumayı durdur**'a basıldı | Yeniden **İleri**'ye basın |
| `'...' katmanı hiçbir koordinat sistemi bildirmiyor.` | Veri kümesi etiketsiz | Yanına aynı adlı bir `.prj` dosyası koyun |
| `'...' içindeki katmanlar farklı koordinat sistemleri bildiriyor` | Karışık veri kümesi | Tek bir sisteme dönüştürüp yeniden deneyin |
| `'...' okunabilir çizgi ya da alan içermiyor` | Desteklenen geometri yok | Nokta ve eğriler bu sürümde okunmuyor |
| `'...' içindeki N. öğe okunamadı: ...` | Geometri doğrulamayı geçemedi | Mesajın devamı sebebi söyler; kaynak veriyi düzeltin |
| `'...' sanal dosya sistemi yolu.` | `/vsi...` ile başlayan yol | Dosyayı diske alıp yeniden deneyin |
| `'...' bir KentOSCad proje dosyası. Proje dosyası açılır, içe aktarılmaz: AÇ komutunu kullanın.` | `.pcad` içe aktarılmaya çalışıldı | [AÇ](open.md) kullanın |
| `'...' katmanı kilitli.` | Hedef katman kilitli | Katmanın kilidini açın |
| `Dosya motoru bağlı değil; bu ortamda dosya açılıp kaydedilemez.` | Dosya motoru olmayan bir ortam | Uygulama içinden çalıştırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
