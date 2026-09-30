# Netcad NCZ Çizimleri

Netcad'de hazırlanmış bir çizimi (`.ncz`) KentOSCad'e almak ya da altlık olarak bağlamak isteyen
kullanıcı için; bu sayfayı bitirdiğinizde dosyayı nasıl açacağınızı, hangi Netcad nesnesinin
çizimde neye dönüştüğünü, koordinat sistemi konusunda programın neyi karşılaştırıp neyi
karşılaştırmadığını ve neyin okunmadığını bileceksiniz.

Komutlar: [İÇEAKTAR](../komutlar/import.md) ve [DIŞREFERANS](../komutlar/xref.md). Öteki
biçimler ve `.prj` dosyası: [Dış veri biçimleri](dis-formatlar.md).

## Kısaca

- **NCZ, Netcad'in kendi ikili çizim biçimidir** ve KentOSCad onu doğrudan okur: Netcad'in
  kurulu olması ya da dosyanın önce başka bir biçime çevrilmesi gerekmez. Okuma bir
  kütüphaneye dayanmaz; komut satırı ve betik her yapıda NCZ okur. Yalnız paftaların gerçek
  çerçevesini kurmak GDAL ister ([Pafta çerçeveleri](#pafta-çerçeveleri)).
- **NCZ yalnız okunur.** [`DIŞAAKTAR`](../komutlar/export.md) `.ncz` yazmaz;
  [`AÇ`](../komutlar/open.md) yalnız `.pcad` açar.
- Nesneler **katmanlarıyla, renkleriyle ve çizgi kalınlıklarıyla** gelir; daire daire, yay yay
  olur.
- **Koordinatlar dönüştürülmez.** Dosyadaki sayılar çizimin koordinat sisteminde metre olarak
  okunur. Dosya hangi sistemi bildiriyorsa rapor onu yazar ve çizimin diliminden farklıysa
  uyarır; bu yüzden içe aktarmadan **önce** çizimin sistemini doğru kurun
  ([Koordinat sistemi](#koordinat-sistemi)).
- **İçe aktarma tek işlemdir.** Tek `GERİAL` hepsini geri alır; bir hata olursa ya da
  **Durdur**'a basarsanız çizim değişmez.
- Okunamayan kayıtlar, tanınmayan geometri türleri ve çizime alınmayanlar **sayılır ve raporda
  söylenir.** Raporda ayrıca yazılmayanlar da vardır: [Neler okunmaz](#neler-okunmaz).

## Almak mı, bağlamak mı

İki ayrı iştir:

| | [`İÇEAKTAR`](../komutlar/import.md) | [`DIŞREFERANS`](../komutlar/xref.md) |
|---|---|---|
| Ne yapar | Nesneleri bir kez okuyup çizime ekler; gelenler çizimin kendi nesneleri olur | Dosyayı çizime canlı bağlar; nesneleri her açılışta ve yenilemede dosyasından okunur |
| Düzenlenir mi | Evet | Hayır; düzenlemek için [`YERELKOPYA`](../komutlar/local_copy.md) |
| Katman ve alan seçimi | `katmanlar=` ve `alanlar=` (ya da İçe Aktar penceresi) | Yok: bütün katmanlar gelir; sütun olarak yalnız `nokta_no` ve akıllı nesne değerleri |
| Okuyucunun raporu | Transkripte yazılır | Yazılmaz ([Altlık olarak bağlamak](#altlık-olarak-bağlamak)) |
| Uygun olduğu iş | Netcad'de hazırlanmış çizimi çalışma çiziminize katmak | Değişmemesi gereken altlık: halihazır harita, komşu pafta |

## Dosyayı açmak

### Komut satırı

`.ncz` uzantısı yeterlidir; büyük ya da küçük harf fark etmez:

```
İÇEAKTAR "plan.ncz"
```

Uzantı yanıltıcıysa biçimi söyleyin:

```
İÇEAKTAR dosya="plan.dat" bicim=NCZ
```

Yalnız bazı katmanları almak için `katmanlar=`, dosyanın öznitelik alanlarını sütun yapmak
için `alanlar=` kullanılır ([Katmanlar, renkler ve kalınlıklar](#katmanlar-renkler-ve-kalınlıklar),
[Öznitelik alanları](#öznitelik-alanları)):

```
İÇEAKTAR dosya="plan.ncz" katmanlar="PINDEX_1000"
İÇEAKTAR dosya="plan.ncz" alanlar="layer_name,entity_type"
İÇEAKTAR dosya="plan.ncz" alanlar=*
```

Altlık olarak bağlamak için:

```
DIŞREFERANS dosya="altlik.ncz"
```

Yol içinde boşluk varsa tırnak içine alınır. `/vsicurl/` gibi sanal dosya sistemi yolları
reddedilir: dosyayı diske indirip öyle açın.

### Arayüz

**KentOS CAD ▸ İçe Aktar…** ya da şeritteki **Harita ▸ Veri ▸ İçe Aktar…** İçe Aktar
penceresini açar. **Gözat…** penceresinin süzgecinde **Netcad çizimi** satırı vardır;
**Desteklenen tüm dosyalar** satırı da `.ncz` dosyalarını gösterir; dosyayı pencereye
sürükleyip bırakmak da olur. Pencere öteki biçimlerdekiyle aynıdır
([İÇEAKTAR sayfasındaki anlatım](../komutlar/import.md#arayüz)); NCZ'de şunları bilin:

- **Dosya.** Bir `.ncz` seçildiği anda okunur ve çizime hiçbir şey eklenmez. Büyük bir
  dosyada **Okumayı durdur** okumayı keser. Pencere dosyayı **iki kez** okur: bir kez
  önizleme için, bir kez de **İçe aktar**'a bastığınızda komut çalışırken.
- **Katmanlar.** Okuyucunun uyarıları (koordinat sistemi, uzaktaki nesneler, pafta
  çerçeveleri…) çizimin üstünde şeritler olarak durur; sağ sütundaki **Katmanlar**
  bölmesinde kutucukla katman seçilir.
- **Alanlar.** NCZ'nin alanları dosyadan gelmez, okuyucunun sabit **on yedi** alanıdır
  ([Öznitelik alanları](#öznitelik-alanları)). **Alanlar** bölmesinde hepsi işaretli gelir;
  sütun istemiyorsanız **Hiçbiri**'ne basın. Hepsi işaretliyse pencere `alanlar=*`, bir
  kısmı işaretliyse `alanlar="…"` yazar; hiçbiri işaretli değilse `alanlar` yazılmaz.

Pencere yalnızca argüman toplar: kurduğu `İÇEAKTAR` satırı, aynı işi bir betikte yazacağınız
satırın aynısıdır. İçe aktarma bitince görünüm çizimin kapsamına yakınlaşır.

Bağlamak için **Harita ▸ Veri ▸ Dış Referans** bir dosya penceresi açar; süzgeci `.ncz`
dosyalarını da kabul eder ([DIŞREFERANS sayfasındaki anlatım](../komutlar/xref.md#arayüz)).

CBS okuyucusu (GDAL) olmayan bir yapıda İçe Aktar penceresi açılmaz; orada NCZ yine komut
satırından ve betikten okunur.

**Klavyeyle:** komut satırı **Ctrl+9** ile açılır ve yukarıdaki satırlar aynen yazılır. Şerit ve
pencereler klavyeyle gezilir: [Klavyeyle tam kullanım](../baslangic/arayuz.md#klavyeyle-tam-kullanım).

### Betik

İçe aktarma ve bağlama bir JSON betiğinin satırı olabilir; komut kimlikleri `core.import` ve
`core.xref`'tir:

```json
{
  "komutlar": [
    { "cmd": "core.import",
      "args": { "dosya": "veri/plan.ncz", "katmanlar": "PINDEX_1000", "alanlar": "*" } }
  ]
}
```

```json
{
  "komutlar": [
    { "cmd": "core.xref", "args": { "dosya": "veri/altlik.ncz", "ad": "HALIHAZIR" } }
  ]
}
```

Python betiğinde komut satırı `cad.run` ile verilir ([Python betikleri](../betik/python.md)):

```python
cad.run('İÇEAKTAR dosya="veri/plan.ncz" katmanlar="PINDEX_1000"')
```

Komut satırı, pencere ve betik aynı komutu çalıştırır; aynı çizimi ve aynı günlük satırını
bırakırlar. Bir betiğin içindeki `core.import`, betiğin geri kalanıyla birlikte tek geri alma
adımına katılır.

## Koordinat sistemi

Bir NCZ dosyası koordinat sistemini iki yerde bildirebilir; ikisi de olmayabilir:

- **MPROJ** bloğu: datum (WGS-84, ITRF, ED50), projeksiyon türü (coğrafi, 6° dilim ya da
  3° dilim) ve dilim.
- **TILED_XML** bloğu: bir `SRS` bildirimi (raporda `TILED_XML: SRS=5257` diye görünür).

Okuyucu bunları okur ve söyler, ama **hiçbir koordinatı dönüştürmez.** Sayılar Netcad'in
yazdığı gibi, çizimin koordinat sisteminde metre olarak okunur ve milimetreye yuvarlanır.
Sistemi tahmin etmez: 421 000 gibi bir sağa değeri birden çok dilimde geçerlidir
([neden](dis-formatlar.md#koordinat-sistemi)).

Bu yüzden içe aktarmadan **önce** çizimin sistemini dosyanınkine uygun kurun. Dilim listesi:
[Koordinat sistemleri](koordinat-sistemleri.md).

```text
AYAR koordinat_sistemi EPSG:5257
İÇEAKTAR "plan.ncz"
```

`EPSG:5257` TUREF/TM39'dur. Bir çizimin sistemini sonradan değiştirmek sayıları düzeltmez,
yalnız etiketi değiştirir; yanlış sistemde aktarılmış bir dosyayı `GERİAL` ile geri alıp
doğru sistemi kurarak yeniden aktarın.

### Rapor ne söyler

Dosyanın bildirimine göre rapor şu satırlardan birini yazar (tam metinler
[aşağıdaki tabloda](#rapor-satırları-ve-hatalar)):

| Dosya ne bildiriyor | Sonuç |
|---|---|
| MPROJ da TILED_XML de yok | `uyarı:` Dosya koordinat sistemi bildirmiyor; çizimin kendi sistemi varsayıldı |
| 3° dilim ve çizimin orta meridyeniyle aynı | `not:` Dosyanın bildirdiği sistem yazılır; koordinatlar dönüştürülmeden okundu |
| 3° dilim ama çizimin orta meridyeninden **farklı** | `uyarı:` Dilimler farklı; koordinatlar dönüştürülmedi, çizim yanlış yere düşer |
| Coğrafi sistem ve sayılar **derece** aralığında | **Hata:** içe aktarma yapılmaz, çizim değişmez |
| Coğrafi sistem ama sayılar metre büyüklüğünde | `uyarı:` Bildirim yanlış görünüyor; sayılar çizimin sisteminde metre okundu |
| 6° dilim, tanımsız projeksiyon ya da yalnız TILED_XML | `not:` Bildirim yazılır; karşılaştırma yapılmaz |

**Yalnız bir şey karşılaştırılır:** 3° dilim bildiren bir dosyanın orta meridyeni, çizimin orta
meridyeniyle. **Karşılaştırılmayanlar:** datum (ITRF ile ED50 arasındaki fark dönüştürülmez,
yalnız söylenir), 6° dilimler ve `SRS` numarası. Çizimin orta meridyeni bilinmiyorsa (örneğin
çizim `YEREL` sistemdeyse) dilim karşılaştırması da yapılmaz, yalnız bildirim yazılır. TM30 ile
TM33 karışıklığı sessizdir; rapordaki bildirimi çizimin sistemiyle kendiniz de karşılaştırın.

Örnek: dosya TM39'da, çizim de TM39'da.

```text
İçe aktarıldı: 1204 nesne, 6 katman (NCZ 5.2.0.1035N, EPSG:5257)
  not: Dosyanın bildirdiği sistem: ITRF, 3° dilim, orta meridyen 39° (TILED_XML: SRS=5257). Koordinatlar dönüştürülmeden çizimin sistemi EPSG:5257 içinde okundu.
  atlandı: 2 kayıt bu okuyucunun tanımadığı NCZ geometri türlerinde (8, 14); okunmadı.
  not: 31 nesnenin çizgi kalınlığı okundu (0,10–0,70 mm); ekranda görmek için durum çubuğunda KALINLIK açık olmalı.
  not: 12 değer milimetrenin altında ayrıntı taşıyordu; KentOSCad milimetre çözünürlükte saklar ve bunları en çok 0,50 mm kaydırarak yuvarladı. Milimetreden küçük bir ayrıntı bu çözünürlükte kaybolur.
  not: Okunan türler: Line 512, Text 301, Point 244, Polyline 96, Polygon 41, Circle 10; atlanan: NCZ türü 14 1, NCZ türü 8 1
```

Aynı dosya çizimin varsayılan diliminde (TUREF/TM36) açılsaydı ilk `not:` satırının yerine
şu `uyarı:` gelirdi:

```text
  uyarı: Dosya ITRF, 3° dilim, orta meridyen 39° (TILED_XML: SRS=5257) bildiriyor; çizimin sistemi (EPSG:5256) 36° orta meridyenli. Koordinatlar dönüştürülmedi: dilimler farklıysa çizim yanlış yere düşer. GERİAL ile geri alın, AYAR koordinat_sistemi ile doğru dilimi kurun ve yeniden aktarın.
```

Derece sayan bir dosya reddedilir; çizimde hiçbir şey değişmez:

```text
Dosya coğrafi koordinatlarda (WGS-84, coğrafi (enlem, boylam)) ve bütün koordinatları derece aralığında. KentOSCad metre sayan bir sistemde milimetre saklar; çizimi Netcad'de bir TM ya da UTM dilimine dönüştürüp yeniden aktarın.
```

Sebebi şudur: çizim koordinatları milimetre tam sayı olarak saklar ve bir dereceyi metre saymak
her köşeyi yüz metrelik bir ızgaraya ezerdi
([Koordinat sisteminin birimi](koordinat-sistemleri.md#koordinat-sisteminin-birimi-yalnız-metre)).

### Ne yapmalı

- **Dilim uyarısı geldiyse:** hangisinin doğru olduğuna karar verin. Dosya doğruysa `GERİAL`
  yapın, `AYAR koordinat_sistemi` ile çizimin sistemini dosyanınkine getirin ve yeniden
  aktarın.
- **Dosya sistem bildirmiyorsa:** sistemini dosyayı hazırlayan kişiden öğrenin, kurun, sonra
  aktarın. Program sayılara bakarak dilim seçmez; sayıların derece olup olmadığı da yalnız
  MPROJ'da coğrafi sistem bildiren dosyada denetlenir.
- **Dosya, çalışma çiziminizden başka bir dilimdeyse:** çizim yeniyse sistemi dosyanınkine
  kurup içe alın, sonra [`DÖNÜŞTÜR`](../komutlar/reproject.md) ile çizimi istediğiniz
  sisteme geçirin. Çizimde başka veri varsa NCZ'yi ayrı bir çizimde bu yolla dönüştürüp
  `.pcad` olarak kaydedin ve çalışma çizimine [`DIŞREFERANS`](../komutlar/xref.md) ile bağlayın:
  proje dosyaları bağlanırken çizimin sistemine dönüştürülür. Nesneleri çalışma çiziminin
  kendisi yapmak için [`YERELKOPYA`](../komutlar/local_copy.md) kullanılır.
- **Dosya derece sayıyorsa:** Netcad'de bir TM ya da UTM dilimine dönüştürüp yeniden kaydedin.

### Altlık olarak bağlamak

`DIŞREFERANS` NCZ'yi aynı okuyucuyla, çizimin sisteminde ve dönüştürmeden okur; dosya kendi
koordinatlarında, yerinde çizilir. Ama yukarıdaki **rapor yazılmaz**: dilim karşılaştırması ve
uyarılar yalnızca `İÇEAKTAR`'da söylenir. Bir NCZ'nin sistemine güvenmiyorsanız önce boş bir
çizimde `İÇEAKTAR` ile açıp raporuna bakın. Derece sayan dosya burada da reddedilir.

## Netcad nesneleri çizimde neye dönüşür

Tablodaki ilk sütun, Netcad türünün raporda (`Okunan türler:`), `entity_type` alanında ve
atlama nedenlerinde görünen adıdır.

| Netcad türü | Çizimde | Ayrıntı |
|---|---|---|
| `Point`: ölçülmüş nokta | [Nokta](../nesneler/nokta.md) | Adı `nokta_no` sütununa yazılır (`n(1284)` ile bulunur); adı boşsa sütun yazılmaz |
| `Symbol`: nokta sembolü | Nokta | Yalnız yeri gelir; sembolün resmi çizilmez, kodu `S12` gibi `etiket` alanında durur |
| `Block`: blok yerleşimi | Nokta | Yalnız yeri gelir; bloğun adı `etiket` alanında durur, bloğun içi okunmaz |
| `Line`: doğru parçası | [Çoklu çizgi](../nesneler/coklucizgi.md) | İki köşeli |
| `Polyline`: açık çok köşeli çizgi | Çoklu çizgi | |
| `Polygon`: kapalı çizgi, dönük dikdörtgen kutu | Kapalı alan ([Çoklu çizgi ve alan](../nesneler/coklucizgi.md)) | Tek halka; delik yok. Kapanış köşesi yazılmaz |
| `MapSheet`: pafta çerçevesi | Kapalı alan | [Pafta çerçeveleri](#pafta-çerçeveleri) |
| `Triangle`: üçgen | Kapalı alan | Üç köşeli |
| `Circle`: daire | [Daire](../nesneler/daire.md) | Merkez ve yarıçap; parçalanmaz |
| `Arc`: yay | [Yay](../nesneler/yay.md) | Merkez, yarıçap ve iki uç; parçalanmaz |
| `Text`: yazı | [Yazı](../nesneler/yazi.md) | Taban çizgisi ve metin |
| `SmartObject`: akıllı nesne | Blok başvurusu, alan ya da nokta | [Netcad 8 akıllı nesneleri](#netcad-8-akıllı-nesneleri) |

Bu türlerin dışındaki kayıtlar okunmaz; sayılır ve raporda `NCZ türü 14` gibi Netcad'in tür
numarasıyla söylenir.

**Kapalı çizgiler alan olur.** Çok köşeli bir çizginin son köşesi ilk köşesiyle bir milimetre
içinde çakışıyorsa çizgi kapalı sayılır ve alan olur. Beş ve daha çok köşeli çizgide ucu tam
değmese de aralarındaki açıklık ilk ve son kenardan kısasının beşte birini (en az 5 cm) aşmıyorsa
aynı şey olur. Bir çizgi neden alan oldu diye sorarsanız cevap budur.

**Yaylar.** Netcad yayı merkez, yarıçap ve iki açıyla saklar; KentOSCad onu saat yönünün
tersine ilk uçtan ikinci uca süpürür. Açılar dosyada radyan ya da derece olabilir: ikisi de tam
turun (2π) biraz üstünü aşmıyorsa radyan, aşıyorsa derece sayılır. Bitiş açısı başlangıçtan
küçükse 360° eklenir. Bir tam turu aşan yay **daire** olarak okunur (`düşürme:`); başı sonuna
denk düşen tam tur, on turdan uzun yay ve iki ucu milimetrede aynı noktaya düşen yay atlanır.

**Yazılar.** Yazı, eklenme noktasından dosyadaki dönüklükle giden bir taban çizgisi ve metin
olur; taban çizgisinin sol ucu eklenme noktasıdır. Yükseklik dosyadaki değerdir (en az 1 mm).
Metinler ve katman adları Türkçe (Windows-1254) kodlamasıyla çözülür. Dönüklüğü sayı olmayan
yazı yatay yazılır (`düşürme:`).

**Noktalar.** Bir noktanın adı `nokta_no` sütununa yazılır; adı tam sayı olan noktaya
[nokta fonksiyonlarındaki](../komutlar/komut-satiri.md#nokta-fonksiyonları) `n(1284)` ile
ulaşılır. `nokta_no` her zaman yazılır, `alanlar=` vermeniz gerekmez.

## Pafta çerçeveleri

Netcad bir paftayı (`MapSheet`) yalnız **iki nokta ve bir ad** olarak saklar (örneğin
`H40-D-07-B-1-C`). İki nokta paftanın kendi köşeleri değil, **sınırlayıcı kutusunun**
karşılıklı köşeleridir. Pafta bir enlem-boylam hücresidir; bir TM dilimine yansıtıldığında
orta meridyenden uzaklaştıkça eksenlere paralel bir dikdörtgen değil, biraz **dönmüş bir
dörtgen** olur ve saklanan kutu onu kuşatan dikdörtgendir. Kutu olduğu gibi çizilseydi komşu
paftalar birkaç metre üst üste biner ve yana kayardı.

Bu yüzden KentOSCad paftanın **gerçek çerçevesini** kurar: dosyanın kendi bildirdiği sistemde
(MPROJ: datum ve 3° ya da 6° dilim; GDAL/PROJ ile) kutunun hangi enlem-boylam hücresinden
geldiğini bulur ve hücrenin dört köşesini alan olarak çizer. Komşu paftalar köşe köşe buluşur.
Koordinatlar yine dönüştürülmez: köşeler dosyanın kendi sisteminde, dosyadaki sayılarla aynı
düzlemde hesaplanır.

Çerçeve **saklanan dikdörtgen kutu olarak** kalır, ve rapor kaç paftanın neden öyle çizildiğini
söyler, şu durumlarda:

| Rapordaki neden | Anlamı | Ne yapılır |
|---|---|---|
| `dosya koordinat sistemi bildirmiyor` | Dosyada MPROJ yok: kutunun hangi dilimde hesaplandığı bilinmiyor | Dosyayı Netcad'de projeksiyonuyla birlikte kaydedin |
| `dosya bir TM ya da UTM dilimi bildirmiyor` | Dosya coğrafi ya da tanımsız bir projeksiyon bildiriyor | Aynı |
| `dosyanın datumu tanınmıyor` | Datum WGS-84, ITRF ya da ED50 değil | Aynı |
| `dosyanın dilim bilgisi bir meridyen vermiyor` | Dilim baytı geçerli bir orta meridyene çevrilemiyor | Aynı |
| `dosyanın dilimi bir projeksiyona çevrilemedi` | PROJ dosyanın dilimini kuramadı | GDAL ve PROJ kurulumunu denetleyin (`make doctor`) |
| `bu yapıda GDAL yok (KENTOS_WITH_GDAL=OFF)` | Bu yapı GDAL'sız derlenmiş | GDAL'lı bir yapı kullanın |
| `kutusu bir enlem-boylam paftasına oturmuyor (yerel bir pafta olabilir)` | Kutu bir enlem-boylam hücresinin kutusuyla bir santimetre içinde örtüşmüyor ya da paftanın ölçeğinin ızgarasına oturmuyor | Bir şey yanlış değil: yerel bir paftanın çerçevesi zaten dikdörtgendir |

Netcad'in pafta indeksi katmanları genellikle `PINDEX_1000` gibi (ölçek paydasıyla) adlanır;
yalnız onu almak için `katmanlar="PINDEX_1000"` yazın. Rapor satırları:

```text
  not: 12 pafta çerçevesi, dosyanın bildirdiği ITRF, 3° dilim, orta meridyen 39° (TILED_XML: SRS=5257) sisteminde gerçek biçimiyle, dönük dörtgen olarak çizildi: dosya bir paftanın yalnız sınırlayıcı kutusunu saklar.
  uyarı: 1 pafta çerçevesi dosyanın sakladığı sınırlayıcı kutuyla çizildi: kutusu bir enlem-boylam paftasına oturmuyor (yerel bir pafta olabilir).
```

Sistem bildirmeyen bir dosyada:

```text
  uyarı: 12 pafta çerçevesi dosyanın sakladığı sınırlayıcı kutuyla çizildi, çünkü dosya koordinat sistemi bildirmiyor. Kutu paftanın kendisi değildir: TM diliminde komşu paftalar birkaç metre üst üste biner.
```

## Katmanlar, renkler ve kalınlıklar

**Katman adı** dosyanın katman tablosundan, nesnenin katman koduna göre alınır. Tablo bir kod
için ad vermiyorsa katman `KATMAN_<kod>` adıyla açılır (`KATMAN_4` gibi). Adlardaki denetim
karakterleri atılır (`düşürme:`). Aynı adlı bir katman çizimde varsa nesneler ona konur
([İÇEAKTAR](../komutlar/import.md)). Katmandan yalnız **adı ve rengi** okunur; görünürlüğü,
kilidi ve çizgi tipi okunmaz: hepsi görünür ve açık gelir.

**`katmanlar=`** yalnız adı verilen katmanları alır. Adlar virgülle ayrılır ve **Türkçe
kurallarıyla** karşılaştırılır (`kaldırım` ile `KALDIRIM` aynıdır). Katman adında virgül varsa
o katman `katmanlar=` ile seçilemez; virgül ayırıcıdır. Dosyada olmayan bir ad hata değildir;
ama hiçbir ad eşleşmezse hiçbir şey okunmaz ve içe aktarma `okunabilir geometri içermiyor`
diye durur. Katman adlarını görmek için `katmanlar=` vermeden İçe Aktar penceresinin ikinci
adımına bakın.

**Renk.** Dosyanın renk tablosundaki katman rengi katmanın rengi olur. Nesnenin renk kodu 1 ise
nesne mavi, 255 ise kırmızı çizilir; 0 ve öteki kodlarda nesne katmanın renginde kalır. Bir renk
yalnız katmanınkinden farklıysa nesneye yazılır. Renk tablosu yoksa katman varsayılan renginde
kalır.

**Çizgi kalınlığı.** Dosya her nesnenin kalınlığını 0,1 mm'nin katı olarak taşır. Sıfırdan
büyük ve 100 mm'yi aşmayan değerler nesnenin kendi kalınlığı olur; sıfır ve eksi değerler
yazılmaz, nesne katmanın kalınlığında kalır. Rapor kaç nesnenin kalınlığının okunduğunu ve
aralığını yazar. Kalınlığı ekranda görmek için durum çubuğundaki **KALINLIK** anahtarı açık
olmalıdır ([Durum çubuğu](../baslangic/arayuz.md#durum-çubuğu)); kalınlık nesnede durur, yalnız
ekran değişir.

## Öznitelik alanları

Bir NCZ'nin öznitelik alanları dosyadan gelen bir liste değildir: okuyucu her nesne için aşağıdaki
**on yedi** alanı sunar. `alanlar=` hangilerinin çizimde **sütun** olacağını söyler; verilmezse
hiçbiri okunmaz. Adlar tablonun ilk sütunundaki gibi, küçük harfle ve altçizgiyle yazılır:

```
İÇEAKTAR dosya="plan.ncz" alanlar="layer_name,label"
İÇEAKTAR dosya="plan.ncz" alanlar=*
```

Bu on yedi addan başkası okunmaz ve yazım hatası hata vermez: tanınmayan ad yok sayılır.
Sütun kimliği alanın adıdır; belgede aynı kimlikte ve aynı türde bir sütun varsa yeniden
kullanılır, türü farklıysa alan okunmaz ve rapor söyler. Ondalık sütunlar altı ondalıklıdır.

| Alan | Sütun adı | Tür | Nesnelerde |
|---|---|---|---|
| `source_file` | kaynak dosya | metin | Hepsi: dosyanın adı, uzantısız (`plan.ncz` için `plan`) |
| `layer_code` | katman kodu | tam sayı | Hepsi: Netcad'in katman numarası |
| `layer_name` | katman adı | metin | Katmanın adı olanlar |
| `entity_type` | nesne türü | metin | Hepsi: `Point`, `Line`, `Polygon`… ([tablo](#netcad-nesneleri-çizimde-neye-dönüşür)) |
| `name` | ad | metin | `Point`: noktanın adı |
| `label` | etiket | metin | Çok köşeli çizgi ve kutu (kaydındaki metin ya da kutunun adı), `Text` (metin), `Symbol` (`S12`), `Block` (blok adı), `MapSheet` (pafta adı), `SmartObject` (sınıf adı ya da Netcad'in verdiği ad) |
| `color_argb` | renk (ARGB) | metin | Rengi olanlar: ARGB sözcüğü onluk sayı olarak (kırmızı için `4294901760`) |
| `radius` | yarıçap | ondalık | `Circle`, `Arc`: metre |
| `start_ang` | başlangıç açısı | ondalık | `Arc`: dosyadaki değer (görülen dosyalarda radyan) |
| `end_ang` | bitiş açısı | ondalık | `Arc`: dosyadaki değer |
| `text_h` | yazı yüksekliği | ondalık | `Text`: yazının yüksekliği, metre; `Symbol`: sembol boyu |
| `rotation` | dönüklük | ondalık | `Text`, `Symbol`, `Block`, `SmartObject`, çok köşeli çizgi ve kutu (dikdörtgen değilse 0): derece |
| `box_width` | kutu genişliği | ondalık | Çok köşeli çizgi ve kutu (dikdörtgen değilse 0), `MapSheet`, dikdörtgenli `SmartObject`: metre |
| `box_height` | kutu yüksekliği | ondalık | Aynı: metre |
| `scale` | ölçek | ondalık | `SmartObject`: nesne boyutu |
| `grid_x` | ızgara x | ondalık | `SmartObject`: dosyadaki değer |
| `grid_y` | ızgara y | ondalık | `SmartObject`: dosyadaki değer |

Bir nesne için değeri olmayan alan o nesnede boş kalır. Pafta için `box_width` ve `box_height`
dosyada saklanan kutuyu yansıtır, çizilen dönük çerçeveyi değil. İçe Aktar penceresinin üçüncü
adımı bu on yedi alanı ilk değerleriyle listeler. Sütunlar **Öznitelikler** panelinde ve
[öznitelik tablosunda](oznitelik-tablosu.md) görünür.

## Netcad 8 akıllı nesneleri

Netcad 8'in Planet modülü plana yerleşim, yapılaşma, yol genişliği, plan notu ve fonksiyon adı
sembolleri koyar. Dosya bu sembollerin **resmini değil özelliklerini** saklar (`nizam=AYRIK`,
`kat=3`, `taks=0.4`…); sembolü Netcad ekranda özelliklerinden çizer. Netcad bunlara **akıllı
nesne** der. KentOSCad sembolü aynı özelliklerden yeniden çizer ve değerlerini nesnenin
sütunlarına yazar.

| Sınıf | Çizimde |
|---|---|
| `Yerleşim` | Daire; ortada nizamın kısaltması ve kat (`A – 3` gibi), üstte ön ve varsa arka bahçe mesafesi (`5-4`), altta yan bahçe mesafesi. Nizam `AYRIK` ise `A`, `BİTİŞİK` ise `B`, `BLOK` ise `BL` yazılır; başka bir değer olduğu gibi yazılır |
| `Yapılaşma` | Daire; bir çizginin üstünde TAKS, altında KAKS (asgari değer de varsa `0.30-0.40` biçiminde aralık). TAKS ve KAKS gösterilmiyorsa değerler dairesiz satırlar olarak yazılır: `E=1.50` (Emsal), `Hmax=…`, `Yençok=12.50 m` |
| `Yol` | Daire; yol genişliğinin tam metresi büyük, iki ondalığı yukarıda, küçük ve altı çizili |
| `Plan Notu` | Notun kutusu ve içinde notun düz metni, kutuya sığacak biçimde satırlara bölünmüş; yazı tipleri ve biçimler alınmaz |
| `Fonksiyon Adı` | Fonksiyonun adı yazı olarak |
| Başka bir sınıf | Nokta; değerleri sütunlarında (`düşürme:` satırı söyler) |

Bir sembolde değeri boş olmayan ve Netcad'de kapatılmamış özellikler çizilir. Gösterilecek hiçbir
değeri olmayan sembol nokta olarak gelir.

Semboller **blok** olarak tanımlanır (`NCZ Yerleşim…`, `NCZ Yapılaşma…`, `NCZ Yol…`,
`NCZ Plan notu…`, `NCZ Fonksiyon…` adlarıyla): aynı şeyi söyleyen bütün nesneler tek tanımı
paylaşır, her nesne o bloğun bir [blok başvurusudur](../nesneler/blokreferansi.md). Sembolün
çizgileri nesnenin renginde çizilir. Boyut ve dönüklük dosyadan gelir; nesne boyutu 1
iken sembolün yarıçapı 10 m'dir. Boyutu okunamayan nesne 1 ile çizilir (`düşürme:`). Aynı adlı bir
blok çizimde varsa çizimdeki tanım kullanılır ve `not:` satırı bunu söyler.

Semboller Netcad'in yardım belgelerindeki çizimlere bakılarak yeniden çizilir; oranlar
ölçülmüştür, ama Netcad'in kendi çiziminin birebir aynısı olduğu söylenemez.

**Sütunlar.** Her akıllı nesnede `akilli_nesne` sütunu sınıfın adını taşır. Ayrıca kullanıcının
girdiği ve kapatılmamış her değer kendi sütununa yazılır. Sütun adı özelliğin Netcad'deki
başlığıdır; kimliği ondan türetilir (Türkçe harfler ASCII'ye indirilir, harf ve rakam
dışındakiler `_` olur): `Nizam` → `nizam`, `Genişlik` → `genislik`, `Fonksiyon Adı` → `fonksiyon_adi`, `Ön bahçe` → `on_bahce`.
Hangi sütunların açılacağı dosyaya bağlıdır. Bu sütunlar `alanlar=` gerektirmez.

**Dikdörtgenli akıllı nesneler.** Planet sembolü olmayan, döndürülmüş bir dikdörtgen olarak
saklanmış akıllı nesne alan olarak gelir; Netcad'in verdiği ad (`BASIC` gibi) `etiket` alanında
durur. Dikdörtgeni olmayanlar yerinde nokta okunur (`not:` satırı söyler).
Dikdörtgenli akıllı nesne içeren bir çizimde 0 katmanındaki `S0` sembolleri (ızgara işaretleri)
ayrıca okunmaz: akıllı nesne onları kendisi çizer.

## Öznitelik tabloları

Bir NCZ dosyası `@TAB1`, `@TAB23` gibi adlarla öznitelik tabloları taşıyabilir. Okuyucu bunları
okur ve sayar, ama tablo satırlarının hangi nesneye ait olduğunu bilmediği için **çizime
aktarmaz**; rapor tabloların adını ve satır sayısını `atlandı:` satırında yazar. Tabloların
içeriği çizimde yer almaz.

## Uzaktaki nesneler

Gerçek bir çizimde bir sayısallaştırma hatası birkaç nesneyi çizimin geri kalanından yüzlerce
kilometre öteye, örneğin (0, 0) noktasına bırakmış olabilir. Okuyucu bunları olduğu gibi okur:
dosyanın verisidir. Ama içe aktarma bitince görünüm çizimin kapsamına yakınlaştığı için çizimin asıl
kısmı köşede küçük bir nokta kalır. Bu yüzden, en az yüz nesnelik bir dosyada nesnelerin yüzde biri
ya da azı çizimin geri kalanının bulunduğu bölgeden en az 100 km (bölge genişse boyunun yirmi katı)
uzaktaysa rapor bir `uyarı:` yazar: kaç nesne olduğunu, hangi katmanlarda durduklarını ve ilkinin
yakınındaki Y ve X'i. Nesneleri seçip silmek sizin kararınızdır.

## Büyük dosyalar, Durdur ve geri alma

Dosya belleğe eşlenir ve iki geçişte okunur: ilkinde katman adları, renk tablosu ve koordinat
sistemi bildirimi (Netcad bu tabloları geometrinin ardından da yazabilir), ikincisinde nesneler.
Okuma ayrı bir iş parçacığında sürer ve pencereyi dondurmaz; durum çubuğunda
`İçe aktarılıyor: <dosya>` yazar, altında kayan bir şerit ve yanında **Durdur** çipi görünür
([Uzun işler](../baslangic/arayuz.md#uzun-işler)).

**Durdur** ya da **Esc** okumayı keser: en geç bir sonraki denetim noktasında (her 4 MB'ta ve
her 4096 nesnede) durur, çizim değişmez ve transkript `İçe aktarma durduruldu; çizim değişmedi.`
der. Nesneler çizime ancak okuma bitince, **tek geri alma adımı** olarak girer; `GERİAL`
hepsini kaldırır; `YİNELE` hepsini geri getirir. İçe aktarma başarısız olursa geri alınacak bir
şey kalmaz. `GERİAL`, içe aktarmanın açtığı boş katmanları, sembol bloklarını ve tanımlanan
sütunları kaldırmaz: geri alma bir tabloyu kesmez
([İÇEAKTAR ▸ Geri alma](../komutlar/import.md#geri-alma)).

## Neler okunmaz

| Okunmayan | Raporda |
|---|---|
| **Noktanın yüksekliği (Z).** Dosyada dolu olsa da `kot` sütununa yazılmaz: gerçek planlarda bu alan az noktada dolu ve her zaman bir yükseklik değil (1 088 m'lik bir kotun yanında 0,12 m). Kot gerekiyorsa noktaları kotlarıyla bir nokta listesinden [NOKTALAR](../komutlar/points.md) ile alın | `atlandı:` satırı, kaç noktada dolu olduğu ve en düşük–en yüksek değerle |
| Çizgilerin tepelerindeki yükseklik | Ayrıca yazılmaz: bu alanlar gerçek dosyalarda yükseklik taşımıyor |
| Katmanın görünürlüğü, kilidi ve çizgi tipi; nesnenin çizgi tipi, dolgusu, yazı tipi ve yazı stili | Ayrıca yazılmaz |
| Sembol resimleri ve blok tanımlarının içi (yalnız yerleri gelir) | Ayrıca yazılmaz |
| Öznitelik tabloları (`@TAB`) | `atlandı:` satırı |
| Tanınmayan geometri türleri | `atlandı:` satırı, tür numaralarıyla |
| Bozuk kayıtlar | `atlandı:` satırı, sayıyla |
| Akıllı nesnenin tanınmayan sınıfı | `düşürme:` satırı; nokta olarak gelir |
| Milimetreden küçük ayrıntı | `not:` satırı; milimetreye yuvarlanır |

Bunlara ihtiyacınız varsa aynı çizimi Netcad'de DXF olarak dışa aktarıp
[DXF olarak](dis-formatlar.md) alabilirsiniz; hangi nesnelerin DXF'e gittiği Netcad'in
dışa aktarımına bağlıdır.

## Rapor satırları ve hatalar

İçe aktarma raporunun her satırı bir düzeyle başlar: `not:`, `uyarı:`, `düşürme:`, `atlandı:`,
`hata:` ([anlamları](../komutlar/import.md#transkriptin-söylediği)). Serbest notlar sekizde durur,
kalanı `… ve N not daha.` diye sayılır; sistem satırı her zaman ilk sıradadır, ama sayımlar
(`Okunan türler:` gibi) hiçbir zaman gizlenmez. NCZ'de `Okunan türler:` satırındaki
`parçalanan:` grubu, bir şeyi eksik okunan nesneleri sayar (tam turdan geniş yay, dönüklüğü
okunamayan yazı).

### İçe aktarmayı durduran iletiler

Bunlardan biri çıkarsa çizim içe aktarmadan önceki hâlinde kalır.

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `'…' sanal dosya sistemi yolu. KentOSCad bir veri dosyasının ağdan ya da arşivin içinden okunmasına izin vermez; dosyayı diske alıp yeniden deneyin.` | Yol `/vsi…` ile başlıyor | Dosyayı diske indirip yolunu verin |
| `İçe aktarma durduruldu; çizim değişmedi.` | **Durdur**'a ya da **Esc**'e basıldı | Hata değildir; hazır olunca yeniden çalıştırın |
| `'…' koordinatları çizimin sisteminde okunur ama çizimin koordinat sistemi yok. AYAR koordinat_sistemi ile kurun ve yeniden aktarın.` | Çizimin koordinat sistemi boşaltılmış | `AYAR koordinat_sistemi EPSG:5257` gibi bir sistem kurun |
| `'…' okunamadı: '…' açılamadı: … Yolu ve okuma iznini denetleyin.` | Dosya yok ya da okuma izni yok; işletim sisteminin nedeni iletide yazar | Yolu ve izinleri denetleyin |
| `'…' okunamadı: '…' boş; KentOSCad proje dosyası değil.` | Dosya 0 bayt: kopyalama ya da indirme yarım kalmış | Dosyayı yeniden alın. İletinin son sözü genel bir ifadedir, NCZ için de aynıdır |
| `'…' okunamadı: '…' sıradan bir dosya değil; proje dosyası bekleniyordu.` | Verilen yol bir klasör ya da özel dosya (macOS ve Linux) | Dosyanın yolunu verin |
| `'…' okunamadı: '…' belleğe eşlenemedi: …` | İşletim sistemi dosyayı belleğe eşleyemedi (`boyutu okunamadı`, Windows'ta `görünümü açılamadı` da olabilir); iletinin devamı nedeni söyler | Dosyayı yerel bir diske kopyalayıp yeniden deneyin |
| `Dosya coğrafi koordinatlarda (…) ve bütün koordinatları derece aralığında. …` | Dosya coğrafi sistem bildiriyor ve sayıları derece | Netcad'de bir TM ya da UTM dilimine dönüştürüp yeniden kaydedin |
| `'…' okunabilir geometri içermiyor; çizime hiçbir şey eklenmedi.` | Dosya NCZ değil ya da bozuk; çizim boş; hiçbir kayıt okunamadı; ya da `katmanlar=` hiçbir katmanla eşleşmedi | Dosyayı Netcad'de açıp nesne olduğunu doğrulayın; `katmanlar=`'ı kaldırıp katman adlarına bakın |
| `'…' katmanı kilitli.` | Çizimde aynı adlı ve kilitli bir katman var | Katmanın kilidini açıp yeniden aktarın ([KATMAN](../komutlar/layer.md)) |

Uzantı `.ncz` olan her dosya NCZ diye açılır; dosyanın içeriği bir imzayla denetlenmez. NCZ
olmayan ya da bozuk bir dosya çoğu zaman `okunabilir geometri içermiyor` der.

### Rapor satırları

| Düzey ve mesaj | Sebep | Çözüm |
|---|---|---|
| `uyarı:` `Dosya koordinat sistemi bildirmiyor. Çizimin kendi sistemi varsayıldı: …` | MPROJ da TILED_XML de yok | Dosyanın sistemini öğrenin; gerekirse `GERİAL`, `AYAR koordinat_sistemi`, yeniden aktarın |
| `uyarı:` `Dosya … bildiriyor; çizimin sistemi (…) N° orta meridyenli. Koordinatlar dönüştürülmedi: dilimler farklıysa çizim yanlış yere düşer. …` | Dosyanın 3° dilimi çizimin diliminden farklı | `GERİAL`; `AYAR koordinat_sistemi` ile doğru dilimi kurun; yeniden aktarın |
| `uyarı:` `Dosya coğrafi sistem bildiriyor (…) ama koordinatları metre büyüklüğünde; bildirim yanlış görünüyor. …` | Bildirim ile sayılar çelişiyor | Sayıların gerçek sistemini bulup çizimin sistemini ona kurun |
| `uyarı:` `N nesne çizimin geri kalanından çok uzakta (…; ilki Y …, X … yakınında). YAKINLAŞ KAPSAM bu yüzden çizimi küçük gösterir; kaynakta yanlış yere düşmüşlerse silin.` | Az sayıda nesne (yüzde biri geçmez), çizimin geri kalanından en az 100 km uzakta; kaynakta yanlış yere sayısallaştırılmış olabilir | Mesajdaki katman ve konuma bakıp nesneleri seçin, yanlışsa silin |
| `uyarı:` `Dosya kesik ya da bozuk görünüyor: N. bayttaki kayıt dosyanın bittiği yerden M bayt öteye uzanıyor. Sonrası kayıt kayıt değil, bayt bayt aranarak okundu; eksik nesne olabilir. …` | Bir kayıt dosyanın bittiği yerden ötesine uzanıyor: dosya kesilmiş (yarım kalan kopyalama ya da indirme) ya da o kaydın uzunluğu bozuk. Okuma kaldığı yerden bayt bayt sürer | Dosyayı yeniden alın; sorun sürerse iletinin dediği gibi Netcad'de açıp yeniden kaydedin |
| `uyarı:` `N pafta çerçevesi dosyanın sakladığı sınırlayıcı kutuyla çizildi, çünkü … Kutu paftanın kendisi değildir: TM diliminde komşu paftalar birkaç metre üst üste biner.` | Paftanın gerçek çerçevesi kurulamadı; nedeni iletide yazar | [Pafta çerçeveleri](#pafta-çerçeveleri) tablosu |
| `uyarı:` `N pafta çerçevesi dosyanın sakladığı sınırlayıcı kutuyla çizildi: kutusu bir enlem-boylam paftasına oturmuyor (yerel bir pafta olabilir).` | Kutu bir enlem-boylam paftasının kutusu değil | Yerel pafta ise bir şey yanlış değil |
| `düşürme:` `N akıllı nesnenin (…) sembolü çizilemedi: sınıfı bu okuyucunun tanımadığı bir sınıf ya da gösterecek değeri yok. Yerinde nokta olarak okundu, değerleri sütunlarında.` | Sınıf tanınmıyor ya da çizilecek değer yok | Değerler sütunlarında; gerekirse sembolü elle çizin |
| `düşürme:` `N akıllı nesnenin boyutu okunamadı; Netcad'in varsayılanı olan 1 ile çizildi.` | Nesne boyutu sayı değil, sıfır ya da eksi, ya da 1000'den büyük | Boyutu Netcad'de denetleyin |
| `düşürme:` `N yay tam turdan geniş olduğu için daire olarak okundu.` | Yayın açıklığı 360°'yi aşıyor | Bir şey gerekmez |
| `düşürme:` `N yazının dönüklüğü sayı değildi; yatay yazıldı.` | Yazının dönüklük değeri geçersiz | Yazıyı [DÖNDÜR](../komutlar/rotate.md) ile döndürün |
| `düşürme:` `N katman adındaki denetim karakterleri atıldı.` | Katman adı denetim karakteri taşıyor | Katman karakterleri atılmış adıyla açılır; `katmanlar=`'da bu adı yazın |
| `düşürme:` `N sayı değeri sütuna sığmadığı ya da sayı olmadığı için yazılmadı.` | Bir alanın değeri sütuna sığmayacak kadar büyük ya da sayı değil | Bir şey gerekmez; o nesnede alan boş kalır |
| `düşürme:` `'…' sütunu belgede başka türde tanımlı; alan okunmadı.` | Çizimde aynı kimlikli ama başka türde bir sütun var | Belgedeki sütunun kimliğini ya da türünü değiştirin ([SÜTUN](../komutlar/column.md)) ve yeniden aktarın |
| `atlandı:` `N kayıt okunamadı: türü için kısa, koordinatı ±100 000 km dışında, metni ya da yüksekliği olmayan, alanı sıfır ya da tek noktalı.` | Kayıtlar bozuk ya da eksik | Sayı büyükse dosyanın kaynağını denetleyin |
| `atlandı:` `N kayıt bu okuyucunun tanımadığı NCZ geometri türlerinde (…); okunmadı.` | Netcad'in bu okuyucunun bilmediği kayıt türleri; numaraları iletide | Bu nesneler bu sürümde gelmez; DXF dışa aktarımını deneyebilirsiniz |
| `atlandı:` `Dosyada N öznitelik tablosu var (…); satırları bir nesneye bağlanmadığı için çizime aktarılmadı.` | `@TAB` tabloları | [Öznitelik tabloları](#öznitelik-tabloları) |
| `atlandı:` `N noktada dosyanın yükseklik alanı dolu (a–b m); bu okuyucu onu kot olarak yazmaz. Kot gerekiyorsa noktaları kotlarıyla bir nokta listesinden NOKTALAR ile aktarın.` | Noktaların yükseklik alanı dolu ama kot olarak alınmadı | Kotlar gerekiyorsa noktaları Netcad'den nokta listesi (NCN) olarak verip [NOKTALAR](../komutlar/points.md) ile alın |
| `atlandı:` `N öğe geometrisi kullanılamadığı için atlandı. İlki: <tür>: <neden>` | Nesnelerin geometrisi çizime alınamadı; ilkinin nedeni yazılır (aşağıda) | Nedene bakın |
| `not:` `Dosyanın bildirdiği sistem: … Koordinatlar dönüştürülmeden çizimin sistemi … içinde okundu.` | Dosya bir sistem bildiriyor ve karşılaştırılacak fark yok | Bildirimi çizimin sistemiyle kendiniz de karşılaştırın |
| `not:` `N nesne, eski NCZ okuyucularının atladığı M bölümden okundu (Netcad 8 düzeni: ayarlar geometrinin arasında).` | Netcad 8 dosyasında ayarlar geometrinin arasına yazılmış | Bir şey gerekmez |
| `not:` `N Netcad akıllı nesnesi (…) sembol olarak çizildi: M farklı sembol blok olarak tanımlandı, her nesne bir blok başvurusu. …` | Akıllı nesneler çizildi | [Netcad 8 akıllı nesneleri](#netcad-8-akıllı-nesneleri) |
| `not:` ``N akıllı nesnenin okunabilir bir dikdörtgeni yok; plan notasyonu olarak yerinde nokta okundu, etiketi `label` alanında.`` | Boyutsuz akıllı nesne | Bir şey gerekmez |
| `not:` `N nesnenin çizgi kalınlığı okundu (… mm); ekranda görmek için durum çubuğunda KALINLIK açık olmalı.` | Kalınlıklar nesnelere yazıldı | **KALINLIK** anahtarını açın |
| `not:` `N ızgara işareti (katman 0'daki S0 sembolü) akıllı nesnenin kendisi çizdiği için ayrıca okunmadı.` | Akıllı nesne içeren çizimde ızgara işaretleri | Bir şey gerekmez |
| `not:` `N pafta çerçevesi, dosyanın bildirdiği … sisteminde gerçek biçimiyle, dönük dörtgen olarak çizildi: dosya bir paftanın yalnız sınırlayıcı kutusunu saklar.` | Paftaların gerçek çerçevesi kuruldu | [Pafta çerçeveleri](#pafta-çerçeveleri) |
| `not:` `N değer milimetrenin altında ayrıntı taşıyordu; KentOSCad milimetre çözünürlükte saklar ve bunları en çok … mm kaydırarak yuvarladı. …` | Dosyadaki sayılar milimetreden ince | Bir şey gerekmez |
| `not:` `Okunan türler: …; parçalanan: …; atlanan: …` | Netcad türü başına sayım | Bilgi |
| `not:` `'…' bloğu çizimde zaten vardı; çizimdeki tanım kullanıldı, gelen tanımın üyeleri alınmadı.` | Aynı adlı sembol bloğu çizimde var (aynı dosyayı yeniden aktarmak gibi) | Bir şey gerekmez |
| `not:` `'…' sütunu çizimde başka türde; dosyadaki değerler atlandı.` | Çizimde aynı kimlikli ama başka türde bir sütun var | Yukarıdaki `düşürme:` satırının çözümü |

`İlki:` satırında görülebilecek nedenler:

| Neden | Anlamı |
|---|---|
| `koordinatı sayı değil ya da ±100 000 km dışında` | Koordinat geçersiz |
| `kapalı şekil milimetrede üç köşeye ulaşmıyor` | Alan milimetreye yuvarlanınca üç köşe kalmıyor |
| `yarıçapı sayı değil ya da sıfır`, `yarıçap bir milimetrenin altında` | Daire ya da yay çizilemeyecek kadar küçük |
| `yarıçapı ya da açıları sayı değil`, `açıları birbirinden çok uzak` | Yayın sayıları geçersiz |
| `yay açıklığı boş ya da on turdan fazla`, `yay tam tur; başı ile sonu aynı`, `yayın iki ucu milimetrede aynı noktaya düşüyor` | Yay çizilemez |
| `1. halka dış halka sıfır alanlı: noktalar doğrusal, çakışık ya da halka kendi üzerine katlanmış. …` gibi geometri doğrulama iletileri | Alanın köşeleri çizgi üstünde ya da çakışık |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## QGIS eklentisiyle farkları

NCZ okuyucusunun ayrıştırıcısı QGIS eklentisi *NCZ Reader*'ınkinin taşınmış hâlidir
([Kaynak](#kaynak)); nesneler ve değerler, aşağıdaki farklar dışında eklentinin okuduklarıyla
aynıdır. Farklar:

- Daire ve yay, eklentinin QGIS için ürettiği 72 ve 48 parçalı halkalar yerine **gerçek daire
  ve yay** olur.
- Netcad 8 dosyalarında ayarların geometrinin arasına yazıldığı bölümler de taranır, eklentinin
  atladığı nesneler okunur; akıllı nesneler sembol olarak çizilir, dikdörtgeni olmayanlar nokta
  olarak korunur.
- Çizgi kalınlığı okunur.
- Pafta çerçeveleri sınırlayıcı kutu yerine gerçek biçimiyle kurulur.
- Eklentinin sessizce düşürdüğü kayıtlar burada sayılır ve raporda söylenir.

## Kaynak

KentOSCad'in NCZ okuyucusunun ayrıştırıcısı, Erdinç Örsan ÜNAL'ın QGIS eklentisi *NCZ Reader*'ın
(sürüm 1.4.3, `ncz_pure.py`) C++'a taşınmış hâlidir. Eklenti GPL-2.0-or-later ile yayımlanmıştır
ve burada GPL-3.0-or-later koşullarıyla kullanılır; proje adresi
[github.com/erdincunal/Jeomatik-NCZ-Reader](https://github.com/erdincunal/Jeomatik-NCZ-Reader).
«Jeomatik» adı yazarın markasıdır. Atıf metni ve marka notu [`NOTICE`](../../NOTICE)
dosyasındadır.
