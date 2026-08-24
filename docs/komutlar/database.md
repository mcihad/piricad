# VERİTABANI — PostGIS ile Çalışma

Kurumsal verisi PostgreSQL/PostGIS üzerinde duran kullanıcı için; bu sayfayı
bitirdiğinizde bir sunucuya bağlanabilecek, bir katmanı QGIS'in de okuyabileceği
bir tablo olarak yazabilecek ve bütün bir projeyi veritabanına kaydedip geri
açabileceksiniz.

## Ne yapar

`VERİTABANI`, çizimi bir **PostGIS** veritabanına bağlar. İki ayrı şey yazılır ve
bunlar aynı şey değildir:

**Katman → tablo.** `katmanyaz`, bir katmanı **sıradan bir mekansal tablo** yapar:
nesne başına bir satır, çizimin kendi SRID'iyle bir `geom` sütunu ve tanımlı her
öznitelik için bir sütun. QGIS, `ogr2ogr` ve düz bir `SELECT` bunu okur; özel bir
kodlama değildir. "CBS katmanı olarak kaydet" bunu demektir.

**Proje → kayıt.** `projekaydet`, çizimin **tamamını**, `.pcad` dosyasının
baytları olarak saklar. Bir çizim geometrisinden ibaret değildir: stil tablosunu,
gömülü gösterim resimlerini, katman ağacını, ayarlarını ve koordinat sistemini de
taşır. Bunların hepsini tablolara dağıtmak, hiç kimsenin gidiş-dönüşünü
denetlemediği ikinci bir dosya biçimi icat etmek olurdu. Yerli biçim zaten tam
gidiş-dönüş yapar ve her açılışta parmak iziyle denetlenir.

Tablo, yazılırken **yerine yazılır** — eklenmez. Komutu iki kez çalıştırmak her
parselden iki tane oluşturmamalıdır; satırları ikilenmiş bir kadastro tablosu
hiç tablo olmamasından kötüdür.

Bütün yazma işi **tek bir işlemdedir**. Yarıda kalan bir yazma, belediyenin canlı
tablosunda çizimin bir parçasını bırakırdı; PiriCAD bunu reddeder — ya hepsi
yazılır ya hiçbiri.

## Adlar

| Ad | Tür |
|---|---|
| `VERİTABANI` | Türkçe, birincil |
| `VERITABANI` | Türkçe karaktersiz klavye için |
| `DATABASE` | İngilizce karşılık |
| `VT` | Kısaltma |
| `core.database` | Komut kimliği |

## Sözdizimi

```text
VERİTABANI <islem>
VERİTABANI <islem> hedef=<hedef>
VERİTABANI katmanyaz katman=<katman-adı> hedef=<tablo-adı>
```

İşlemler:

| İşlem | Ne yapar | `hedef` |
|---|---|---|
| `baglan` | Sunucuya bağlanır | Bağlantı dizesi. Zorunlu |
| `kes` | Bağlantıyı kapatır | — |
| `tablolar` | Sunucudaki mekansal tabloları listeler | — |
| `katmanyaz` | Bir katmanı tablo olarak yazar | Tablo adı. Verilmezse katman adından üretilir |
| `projekaydet` | Çizimin tamamını kaydeder | Proje adı. Zorunlu |
| `projeac` | Kayıtlı bir projeyi açar | Proje adı. Zorunlu |
| `projeler` | Kayıtlı projeleri listeler | — |
| `projesil` | Kayıtlı bir projeyi siler | Proje adı. Zorunlu |

## Parametreler

| Parametre | Ne işe yarar |
|---|---|
| `islem` | Yukarıdaki işlemlerden biri. Zorunlu |
| `hedef` | İşleme göre bağlantı dizesi, tablo adı ya da proje adı |
| `katman` | Yalnız `katmanyaz`: yazılacak katman. Verilmezse etkin katman |

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

## Parola nereye yazılır

**Hiçbir yere.** PiriCAD parolayı ne ayar dosyasına, ne projeye, ne de günlüğe
yazar. Ayar dosyası düz metindir, yedeklere kopyalanır ve hata bildirimlerine
yapıştırılır; oraya konan bir veritabanı parolası, yanında bir kolaylık hikâyesi
olan bir kimlik sızıntısıdır.

Bunun zaten çözülmüş yolu libpq'nun kendi mekanizmalarıdır:

```bash
# Kalıcı çözüm: satır biçimi sunucu:port:veritabanı:kullanıcı:parola
echo "localhost:5432:piricad:piricad:GİZLİ" >> ~/.pgpass
chmod 600 ~/.pgpass
```

Windows'ta aynı dosya `%APPDATA%\postgresql\pgpass.conf` adındadır.

Tek seferlik bir oturum için ortam değişkeni yeter:

```bash
PGPASSWORD=GİZLİ piricad
```

Komut satırına parola yazarsanız çalışır, ama günlüğe `password=***` olarak
düşer: komut yeniden oynatıldığında parolayı yeniden vermeniz gerekir. Bu
kasıtlıdır.

## Örnekler

### Komut satırı

Bağlanın. Parola `~/.pgpass` dosyasından gelir:

```
VERİTABANI baglan hedef="host=localhost dbname=piricad user=piricad"
```

Transkript şunu yazar:

```text
Bağlanıldı: PostgreSQL 18.4, PostGIS 3.6.3.
```

Sunucuda ne olduğuna bakın:

```
VERİTABANI tablolar
```

```text
2 mekansal tablo:
  public.ada142_parsel  (geom, EPSG:5254, ~37 satır)
  public.yol_ekseni  (geom, EPSG:5254, ~12 satır)
```

Bir katmanı tablo olarak yazın. Önce çizimin koordinat sistemi bilinmelidir —
SRID'i tahmin edilmiş bir tablo, her koordinatı kilometrelerce kaydırır ve bunu
sessizce yapar:

```
AYAR koordinat_sistemi deger=TUREF/TM30
VERİTABANI katmanyaz katman=PARSEL hedef=ada142_parsel
```

```text
'PARSEL' katmanı yazıldı: 37 satır, EPSG:5254.
```

Artık QGIS'ten ya da düz SQL'den okunabilir:

```bash
psql -d piricad -c "select kimlik, ada_no, ST_Area(geom) from ada142_parsel limit 3"
```

Bütün projeyi kaydedin:

```
VERİTABANI projekaydet hedef="Ada 142 imar"
```

```text
Proje veritabanına kaydedildi: 'Ada 142 imar'  (37 nesne, 412 KB).
```

Başka bir makinede geri açın:

```
VERİTABANI baglan hedef="host=sunucu.belediye.gov.tr dbname=piricad user=harita"
VERİTABANI projeac hedef="Ada 142 imar"
```

```text
Veritabanından açıldı: 'Ada 142 imar'  (37 nesne, 4 katman, 214 nokta).
```

`projeac`, ekrandaki çizimin **yerine geçer** ve geri alma yığınını temizler —
tıpkı [AÇ](open.md) gibi. Kaydedilmemiş işiniz varsa önce kaydedin.

### Arayüz

**Dosya > Veritabanı…** (`Ctrl+Shift+D`) modsuz bir pencere açar: üstte bağlantı
alanları ve bağlantı durumu, altta solda sunucudaki mekansal tablolar, sağda
kayıtlı PiriCAD projeleri bulunur. Bağlantı kurulduktan sonra **Yenile** düğmesi
iki listeyi sunucudan yeniden okur.

Pencere yalnızca argümanı toplar. Her düğme bir `VERİTABANI …` satırı kurar ve
onu komut yolundan çalıştırır; bu yüzden farede olan her şey betikte de vardır.

| Düğme | Karşılığı |
|---|---|
| **Bağlan** | `VERİTABANI baglan` |
| **Bağlantıyı Kes** | `VERİTABANI kes` |
| **Etkin Katmanı Yaz…** | `VERİTABANI katmanyaz` |
| **Projeyi Kaydet…** | `VERİTABANI projekaydet` |
| **Projeyi Aç** | `VERİTABANI projeac` |
| **Projeyi Sil** | `VERİTABANI projesil` |
| **Yenile** | `VERİTABANI tablolar` + `VERİTABANI projeler` |

**Parola alanı her açılışta boş başlar** ve hiçbir yere yazılmaz. Adres, port,
veritabanı adı ve kullanıcı adı uygulama tercihi olarak hatırlanır; bunları
[TERCİH](preference.md) ile de görebilir ve değiştirebilirsiniz:

```
TERCİH veritabani_sunucu
```

Klavyeyle: `Ctrl+Shift+D` pencereyi açar, `Sekme` alanlar arasında dolaşır,
`Enter` adres ya da parola alanındayken bağlanır, `Esc` pencereyi kapatır.
Pencereyi hiç açmadan da her işi komut satırından yapabilirsiniz.

### Betik

Betikte, her işlem bir satırdır:

```json
[
  { "cmd": "core.database", "args": { "islem": "baglan",
      "hedef": "host=localhost dbname=piricad user=piricad" } },
  { "cmd": "core.setting",  "args": { "ad": "koordinat_sistemi", "deger": "TUREF/TM30" } },
  { "cmd": "core.database", "args": { "islem": "katmanyaz",
      "katman": "PARSEL", "hedef": "ada142_parsel" } },
  { "cmd": "core.database", "args": { "islem": "projekaydet", "hedef": "Ada 142 imar" } },
  { "cmd": "core.database", "args": { "islem": "kes" } }
]
```

Gece çalışan bir toplu iş için tipik kalıp budur: aç, yaz, kes.

## Geri alma

`VERİTABANI` geri alınamaz ve geri alma yığınına girmez.

`katmanyaz`, `projekaydet` ve `projesil` çizimi değil **sunucuyu** değiştirir;
PiriCAD'in geri alması sizin çiziminizi geri alır, başkasının veritabanını değil.
Yanlış tabloya yazdıysanız doğrusuna yeniden yazın; yanlış projeyi sildiyseniz
veritabanının kendi yedeğinden dönmeniz gerekir.

`projeac`, [AÇ](open.md) gibi çizimin yerine geçer: geri alınacak bir "önceki
çizim" kalmaz, yığın temizlenir. Pencere bunu sormadan yapmaz.

## Betikten kullanım

`VERİTABANI` betiklenebilir ama **yapay zekâya kapalıdır** — tıpkı [AÇ](open.md)
gibi. `projeac` ekrandaki çizimi atar, `projesil` başkasının kayıtlı işini siler
ve ikisinin de geri alması yoktur; bir öneri, kullanıcıya geri getiremeyeceği bir
kayba mal olmamalıdır. Yapay zekâ bir PostGIS katmanına, her şeye ulaştığı gibi
ulaşır: birinin açtığı çizimin üzerinde çizerek ve ölçerek.

**Günlüğe yazılır.** `KAYDET` ve `DIŞAAKTAR` yazılmaz; `VERİTABANI` yazılır,
çünkü `projeac` belgenin ne içerdiğine karar verir ve onu kaydetmeyen bir günlük
ürettiği çizimi yeniden üretemez. Bunun bedeli şudur: böyle bir günlüğü yeniden
oynatmak sunucuya **yeniden bağlanır ve yazmaları tekrarlar**. Bir günlüğü
oynatmadan önce hangi satırların veritabanına dokunduğuna bakın.

Bağlantı **oturuma** aittir, projeye değil: bir `.pcad` dosyası hangi sunucudan
geldiğini taşımaz. Bir betik her zaman kendi `baglan` satırıyla başlar.

Yazılan tabloya, satırlar bittikten sonra bir **GIST dizini** kurulur. Toplu
yüklemenin doğru sırası budur ve QGIS'in ilk kaydırmasında isteyeceği dizin de
odur.

Satırlar `COPY` ile yazılır, satır satır `insert` ile değil: beş milyon parsellik
bir katmanda aradaki fark saniyelerle kahve molası arasındaki farktır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Veritabanı motoru bağlı değil. Bu yapı PostgreSQL desteği olmadan derlenmiş olabilir.` | Veritabanı motoru olmayan bir ortam | Uygulama içinden çalıştırın |
| `Bu PiriCAD yapısı PostgreSQL desteği olmadan derlenmiş.` | `PIRICAD_WITH_POSTGIS=OFF` ile derlenmiş | Kaynaktan `PIRICAD_WITH_POSTGIS=ON` ile yapılandırın |
| `Bilinmeyen işlem: '...'. Geçerli işlemler: ...` | İşlem adı yanlış yazılmış | Listedeki adlardan birini yazın |
| `Veritabanı bağlantı dizesi boş. Örnek: host=localhost dbname=postgres user=postgres password=...` | `hedef` boş verilmiş | Bağlantı dizesini yazın |
| `Veritabanına bağlanılamadı: ...` | Sunucu kapalı, adres yanlış ya da parola geçersiz | Mesajdaki sunucu yanıtını okuyun; `~/.pgpass` dosyasını denetleyin |
| `Bu veritabanında PostGIS eklentisi yok. Kurmak için: CREATE EXTENSION postgis;` | Veritabanı düz PostgreSQL | Mesajdaki komutu çalıştırın |
| `Veritabanına bağlı değilsiniz. Önce: VERİTABANI baglan hedef="..."` | Bağlanmadan iş istenmiş | Önce bağlanın |
| `Katman bulunamadı: '...'` | Adı yanlış yazılmış katman | [KATMAN](layer.md) ile listeyi görün |
| `Yazılacak katman belirlenemedi. katman=<ad> verin.` | Etkin katman yok | `katman=` ile adı verin |
| `Çizimin koordinat sistemi çözülmemiş, tabloya SRID yazılamaz.` | Koordinat sistemi ayarlanmamış ya da çözülememiş | `AYAR koordinat_sistemi deger=TUREF/TM30` |
| `'...' özniteliği tabloda '...' sütunu olurdu; bu ad zaten kullanılıyor.` | Öznitelik adı `kimlik`/`geom` ile ya da başka bir öznitelikle çakışıyor | Özniteliği yeniden adlandırın |
| `'...' tablosuna yazılamadı: ...` | Sunucu reddetti — izin ya da disk | Mesajdaki sunucu yanıtını okuyun |
| `Proje adı boş olamaz.` | `hedef` boş | Bir ad verin |
| `Kaydedilecek proje boş.` | Boş çizim kaydedilmeye çalışıldı | Önce çizin |
| `Veritabanında böyle bir proje yok: '...'` | Ad yanlış | `VERİTABANI projeler` ile listeyi görün |
| `Veritabanında böyle bir proje yoktu: '...'` | `projesil` zaten silinmiş bir adı sildi | Hata değildir; bilgi satırıdır |
| `Geçici dizin oluşturulamadı; sistem geçici dizinini denetleyin.` | `/tmp` yazılamıyor ya da dolu | Geçici dizin izinlerini ve boş alanı denetleyin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## Tablo neye benzer

`katmanyaz` sonrası tablo şudur:

```text
                  Table "public.ada142_parsel"
  Column   |          Type           | Nullable
-----------+-------------------------+----------
 kimlik    | bigint                  | not null
 geom      | geometry(Geometry,5254) |
 ada_no    | bigint                  |
 malik     | text                    |
 yuzolcumu | double precision        |
Indexes:
    "ada142_parsel_pkey" PRIMARY KEY, btree (kimlik)
    "ada142_parsel_geom_idx" gist (geom)
```

`kimlik`, nesnenin **kalıcı anahtarıdır** — çizimdeki yuvası değil. Bir yuva
tahsis ayrıntısıdır ve bir kayıttan diğerine yeniden kullanılabilir; veritabanının
çizimden başka bir parseli göstermesi böyle olurdu.

Sütun adları ASCII'ye katlanır: `İMAR PLANI` katmanı `imar_plani` tablosu olur.
Tırnak gerektiren bir PostgreSQL tanımlayıcısı, sonradan yazılan her sorgunun
tırnaklamayı hatırlaması gereken bir tanımlayıcıdır.

Geometri, nesnenin halkalarının rollerine göre yazılır:

| Nesne | Tablodaki tür |
|---|---|
| Bir açık halka | `LINESTRING` |
| Birden çok açık halka | `MULTILINESTRING` |
| Bir dış sınır (istenirse delikleriyle) | `POLYGON` |
| **Birden çok dış sınır** | **`MULTIPOLYGON`**, her yüz bir parça |

Son satır önemlidir: **yolla ikiye bölünmüş bir parsel iki yüzlü tek parseldir**,
delikli bir parsel değil. İkinci yüzü delik olarak yazmak, belediyeye alanları
yanlış olan ve buna rağmen hiçbir denetimin şikâyet etmeyeceği bir tablo verirdi.
Böyle bir parsel PiriCAD'e çoğunlukla [İÇEAKTAR](import.md) ile, TKGM'den gelen
bir GeoPackage'ın `MULTIPOLYGON` kaydı olarak girer.

Boş bir hücre SQL `null`'dur, sıfır değil: ölçülmemiş bir cephe ile sıfır cephe,
parsel hakkında farklı iki olgudur.

## Kurulum

Bir PostGIS sunucusu yoksa Docker ile bir tane açabilirsiniz:

```bash
docker run -d --name postgis -p 5432:5432 \
  -e POSTGRES_PASSWORD=GİZLİ -e POSTGRES_DB=piricad \
  postgis/postgis:18-3.6
```

Sonra eklentiyi bir kez etkinleştirin:

```bash
psql -h localhost -U postgres -d piricad -c "CREATE EXTENSION IF NOT EXISTS postgis"
```

PiriCAD'i kaynaktan derliyorsanız PostgreSQL desteği bir seçenektir:

```bash
cmake --preset dev -DPIRICAD_WITH_POSTGIS=ON
```

Bağımlılık `libpqxx`'tir ve `libpq` geliştirme paketini ister
(`libpq-dev`, `postgresql-devel` ya da `brew install libpq`).

## Henüz olmayan: tabloyu çizime okumak

**Bu sürümde katman veritabanına yazılır, veritabanından okunmaz.** Yazdığınız
tabloyu QGIS'te, `ogr2ogr` ile ve düz SQL ile görebilirsiniz; PiriCAD'e katman
olarak geri getiren bir işlem henüz yok.

Bir projenin tamamı için böyle bir asimetri yoktur: `projekaydet` ile yazılan
proje `projeac` ile aynen geri gelir.

Tabloyu katman olarak okuyan `katmanoku` işlemi Faz 1'de gelecek; kapsamı
`CLAUDE.md` Madde 2.9'un "read, write and edit against a live PostGIS
connection" cümlesidir. O zamana kadar yol, tabloyu QGIS'ten ya da `ogr2ogr` ile
bir GeoPackage'a yazıp [İÇEAKTAR](import.md) ile almaktır:

```bash
ogr2ogr -f GPKG ada142.gpkg PG:"host=localhost dbname=piricad" ada142_parsel
```

## İlgili sayfalar

- [AÇ](open.md), [KAYDET](save.md) — aynı işin dosya karşılıkları
- [DIŞAAKTAR](export.md) — teslim için tek seferlik dış biçim yazma
- [AYAR](setting.md) — projenin koordinat sistemi
- [TERCİH](preference.md) — hatırlanan bağlantı bilgileri
- [SÜTUN](column.md) — tabloda sütun olacak öznitelikleri tanımlama
