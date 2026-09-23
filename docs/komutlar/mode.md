# MOD — Oturum Modları

Ekranda çizim yapan herkes için; bu sayfayı bitirdiğinizde nesne yakalama, dik mod,
kutupsal izleme ve ızgaraya yakalama gibi **çizerken yardımcı olan** modları
listelemeyi, okumayı ve değiştirmeyi bileceksiniz.

Modlar **çalışır durumdadır**: yazdığınız değer imleci gerçekten yönlendirir.
Nesne yakalama, dik mod, kutupsal izleme ve ızgaraya yakalama; hepsi fareyle
çizerken de, komut satırına koordinat yazarken de, bir betik çizerken de aynı
biçimde uygulanır — çünkü hepsi noktanın üretildiği tek yolda çalışır.

## Ne yapar

`MOD`, **oturum kapsamındaki** ayarları yönetir: çizerken imlecin nereye oturacağını
belirleyen girdi yardımları. Nesne yakalama modları, dik mod, kutupsal izleme açısı ve
ızgaraya yakalama böyledir.

Bunlar çizim dosyasına **yazılmaz**, tercih dosyasına da **yazılmaz** ve programı
kapattığınızda kaybolur. Sebebi tek cümlede şudur: *bir girdi yardımı, çizimin verisi
değildir; çizerken tuttuğunuz cetveldir.* Cetvel masada kalır, çizim gider.

Üç kapsam ve üç komut vardır; hiçbiri diğerinin kapsamına giremez:

| Kapsam | Komut | Nerede yaşar |
|---|---|---|
| Proje | [`AYAR`](setting.md) | Çizim dosyasında; geri alınabilir, günlüğe girer |
| Uygulama | [`TERCİH`](preference.md) | Kullanıcı profilinde; makineye aittir |
| Oturum | `MOD` | Yalnızca bellekte; program kapanınca biter |

Üç kullanım biçimi vardır:

- **Argümansız** — bütün modları, değerleriyle ve varsayılan olup olmadıklarıyla listeler
- **Yalnızca ad** — o modun değerini, kimliğini, türünü, varsayılanını ve kaynağını yazar
- **Ad ve değer** — modu değiştirir ve önceki değerini söyler

## Adlar

| Ad | Tür |
|---|---|
| `MOD` | Türkçe, birincil |
| `MODE` | İngilizce karşılık |
| `MD` | Kısaltma |
| `core.mode` | Komut kimliği |

## Sözdizimi

```
MOD
MOD <ad>
MOD <ad> <deger>
MOD ad=<ad> deger=<deger>
MOD <ad> varsayilan
```

Mod adı yerine kimliği de yazılabilir: `MOD core.yakalama.dik_mod evet`.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Mod adı veya kimliği. Verilmezse bütün modlar listelenir |
| `deger` | Yeni değer. Verilmezse mod yalnızca okunur. `varsayilan` yazarsanız mod bildirilen varsayılanına döner |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

Bugün oturum kapsamında sekiz mod vardır:

| Mod | Tür | Varsayılan | Ne yapar |
|---|---|---|---|
| `yakalama_modları` | Bit maskesi | `0x10822F` | Etkin nesne yakalama modları |
| `dik_mod` | Evet/hayır | `hayır` | İmleci yatay ve düşey eksene kilitler |
| `yüzey_normali` | Evet/hayır | `hayır` | Çizgiyi başladığı **yüzeye** dik kilitler |
| `köşegen` | Evet/hayır | `hayır` | İmleci öncekinden 45°'nin katlarına kilitler |
| `kutupsal_açı` | µderece | `45000000` (45°) | Kutupsal izleme açı adımı |
| `ızgaraya_yakala` | Evet/hayır | `hayır` | Girilen noktayı en yakın ızgara kesişimine oturtur |
| `adım` | mm | `0` | İmlecin önceki noktaya uzaklığını adımın katına yuvarlar; `0` kapatır |
| `açı_kuralı` | Seçenek | `semt` | `@mesafe<açı` açısının nereden ve hangi yöne sayıldığı; kısa adı `kural` |

`köşegen`, tuval üzerinde **Ctrl** basılı tutularak da açılır; bıraktığınızda kapanır.
`DİKDÖRTGEN`'in ikinci köşesi böyle kilitlenince kare çıkar.

### Yüzey normali

`dik_mod` imleci **sayfanın** eksenlerine kilitler: yatay ya da düşey. `yüzey_normali`
imleci **nesnenin** eksenine yakalar: ilk noktanın oturduğu kenara tam dik olan
doğrultuya.

37°'lik bir parsel sınırından çekilen bir yapı yaklaşma mesafesi 127°'dir. Dik mod bunu
söyleyemez; gözle çizilen bir dik ise ölçüsü tapuya giden bir belgeye onda birlik hata
olarak yazılır. Yüzey normali bunu tam çizer.

**Bu bir kilit değil, bir yakalamadır.** Nişan aldığınız yön normale **20°'den fazla
uzaksa** yardım devreye girmez ve imleç sizindir; 20°'nin içindeyse nokta tam dik
doğrultuya oturur. Yani mod açıkken de her yöne çizebilir, her yöne ölçebilirsiniz —
dikey çekmek istediğinizde kabaca o yöne nişan almanız yeter, gerisini program tam
yapar.

Doğrultu, **ilk noktanın yakınındaki en yakın kenardan** okunur. Yakınlık ölçüsü
yakalama toleransıdır (`core.yakalama.tolerans`): erim içinde bir kenar yoksa yardım
sessizce devreye girmez. Aynı anda dik mod da açıksa **yüzey normali kazanır** — daha
özel olan, daha genel olanı ezer.

Kutupsal açı adımı yüzey normaliyle birlikte çalışır: normal doğrultuyu verir, adım o
doğrultu üzerinde ilerlemeyi böler.

**Karşı kenara tam dik iner.** Bir dik neredeyse hiç boşluğa çizilmez: bir sınırdan
**karşıdaki** sınıra gider ve orada biter — yapı yaklaşma mesafesi cephe hattında,
kesit duvardan duvara. Nişan koninin içindeyken imleç karşı kenara yaklaştığında nokta,
o kenarın imlece en yakın yerine değil, **normalin kenarı kestiği yere** oturur. Böylece
hem karşı kenarın üstündesinizdir hem de çizgi ilk yüzeye tam diktir.

Bu yüzden koninin içinde yüzey normali **nesne yakalamasının önündedir**: aksi hâlde
karşı kenara varıldığı anda uç nokta ya da en yakın kazanır ve çizgi tam da dik kalması
gereken yerde dikliğini kaybeder. Koninin **dışında** ise yüzey normali hiç devreye
girmez ve her yakalama her zamanki gibi çalışır — mod, çizimi yutmaz.

Normalin önüne geçtiği yer yalnızca burasıdır. Adım (`core.yakalama.adim`) bir kesişime
**uygulanmaz**: adım noktanın ışın üzerinde ne kadar ilerlediğini yuvarlar, kenara inen
bir noktanın bütün değeri ise tam orada durmasıdır.

Açı değerleri **mikro derece** cinsindendir: 45° = `45000000`. Ondalık sayı hiçbir ayarda
kabul edilmez; bildirilen birim yeterince incedir.

### Açı kuralı

`@100<45` yazdığınızda açının nereden başlayıp hangi yöne arttığını `açı_kuralı` söyler
(kısa adı `kural`):

| Değer | Sıfır | Artış | `@100<0` | `@100<100` (grad) |
|---|---|---|---|---|
| `semt` (varsayılan) | Kuzey | Saat yönünde | kuzeye 100 m | doğuya 100 m |
| `matematik` | Doğu | Saat yönünün tersine | doğuya 100 m | kuzeye 100 m |

Varsayılan **semt**tir: aletin okuduğu, poligon ve aplikasyon cetvelinin yazdığı açı
budur. Birimi ise proje ayarı `açı_birimi` verir (`AYAR açı_birimi derece`), ya da tek bir
koordinat için açının soneki: `@100<45g`, `@100<45d`, `@100<0.7r`.

```
MOD kural matematik
```

Kural yalnız **yazılan metni** etkiler: komut satırına, betik dizesine ya da yapay zekâ
önerisine yazılan `@mesafe<açı`. Komut günlüğü çözülmüş milimetreyi tuttuğu için eski
günlükler ve betikler her kuralda aynı çizimi verir; kural bu yüzden bir girdi yardımıdır ve
oturum kapsamındadır. `ÖLÇ`, `APLİKASYON` ve sürüklerken kılavuz üzerinde okunan açı da aynı
kuralla yazılır.

```
MOD kural varsayilan
```

Bütün koordinat biçimleri için bkz. [Komut satırı](komut-satiri.md).

### Yakalama modları bit maskesi

`yakalama_modları` bir bit maskesidir; istediğiniz modların değerlerini toplarsınız.

| Bit | Değer | Mod | Neye oturur |
|---|---|---|---|
| 0 | `1` | Uç nokta | Bir halkanın köşesine; yayın iki ucuna; elipsin dört eksen ucuna |
| 1 | `2` | Orta nokta | Bir kenarın ortasına |
| 2 | `4` | Merkez | Bir **eğrinin** çizildiği merkeze: dairenin, yayın, elipsin |
| 3 | `8` | Kesişim | İki kenarın gerçekten kesiştiği noktaya |
| 4 | `16` | Dik ayak | Önceki noktadan bir kenara indirilen dikin ayağına |
| 5 | `32` | En yakın | Bir kenarın imlece en yakın noktasına |
| 6 | `64` | Izgara | En yakın ızgara kesişimine — `ızgaraya_yakala` da bu biti açar |
| 7 | `128` | Kutupsal | Önceki noktadan çıkan en yakın kutupsal ışına |
| 10 | `1024` | Uzantı | Bir kenarın kendi doğrultusuna, kenarın **ötesinde** |
| 11 | `2048` | Paralel | Önceki noktadan çıkan, bir kenara **paralel** ışına |
| 12 | `4096` | Uzatılmış kesişim | İki kenarın uzatılsalar **buluşacakları** köşeye |
| 14 | `16384` | Kılavuz | Kendi koyduğunuz çizim kılavuzuna |
| 15 | `32768` | Ağırlık merkezi | Kapalı bir halkanın **alan** ağırlık merkezine |
| 17 | `131072` | Ekleme noktası | Bir nesnenin **yerleştirildiği** noktaya: blok referansının ekleme noktasına |
| 18 | `262144` | Çeyrek nokta | Bir eğrinin eksenleri kestiği **dört** noktaya: 0, 100, 200 ve 300 grad |
| 19 | `524288` | Teğet nokta | Son noktadan bir eğriye çizilen **teğetin** eğriye dokunduğu yere |
| 20 | `1048576` | İzleme | [`İZ`](tracking.md) ile işaretlenmiş noktaların yatay/düşey izlerine ve iki izin kesişimine |

Varsayılan `0x10822F` = uç nokta + orta nokta + merkez + kesişim + **en yakın** +
düğüm + ağırlık merkezi + **izleme**.

**İzleme kimseden bir şey almaz**, bu yüzden varsayılanda açıktır: işaret yoksa
kip hiçbir şey yapmaz. Kapalı olsaydı kimsenin bulamayacağı bir özellik olurdu.
Bir iz, gerçek olan her şeyin **altında** sıralanır — kullanıcının kurduğu iskele,
ölçülmüş bir noktayı elinden almaz.

**Eğriler çizildikleri gibi yakalanır, saklandıkları gibi değil.** Daire merkez
ve yarıçapla, yay merkez ve iki uçla, elips merkez ve iki eksen ucuyla saklanır;
yakalama bu tanım noktalarını değil ekrandaki eğriyi okur. Dairede ve yayda
`En yakın` ve `Dik ayak` tam hesaplanır. Elipste `Merkez` merkezi, `Uç nokta`
dört eksen ucunu verir; `En yakın`, `Dik ayak` ve `Kesişim` çizilen 128 kenarlı
yaklaşığın üzerine oturur (bir iki milimetre içinde). `Orta nokta`, `Uzantı` ve
`Paralel` elipste verilmez: bir yaklaşığın kirişinin ortası eğrinin geçmediği bir
yerdir, kirişin uzantısı kimsenin çizmediği bir doğrudur.

**En yakın** varsayılana sonradan katıldı. Kapalı olması "her zaman bir şey bulur,
aradığınız köşeyi gölgeler" gerekçesineydi; oysa bunu maske değil **öncelik tablosu**
çözüyor — en yakın, gerçek olan her şeyin altındadır, yani bir köşe, bir orta nokta, bir
merkez ve bir kesişim onu her zaman yener. Kapalı olmasının bedeli ise gerçekti: bir
sınırın **ortasına** getirilen imleç hiçbir şeye oturmuyor, ölçü pikselin düştüğü yerden
alınıyordu. "Şu çizgiye kaç metre" bir ölçümün en sık sorduğu sorudur.

**Çeyrek nokta bir eğrinin adı olan tek yeridir.** Bir baca kapağı çeyreklerinden
aplike edilir, bir yol geçiş eğrisi onlardan ölçülendirilir, bir borunun taban
kotu alt çeyreğindedir. Bu yüzden öncelikte **uç nokta ile aynı sıradadır**: bir
eğride çeyrek, bir çizgide köşenin olduğu kadar açıkça belirtilmiş bir noktadır —
eğrinin, çizimin yazıldığı ızgarayla buluştuğu yerdir. Bir yayda yalnız
**süpürülen** çeyrekler verilir: kesildiği çemberin çeyreği, çizilen şeyin
üzerinde bir nokta değildir.

**Teğet nokta kurulmuş bir noktadır** — uzantı ve paralel gibi — ve aynı sebeple
kurulmuştur: bir çizim başlamamışsa teğet diye bir şey yoktur. **İki** teğet
ayağı sunulur ve imlece yakın olan kazanır, yani hangisini kastettiğinizi
göstererek seçersiniz. Önceki nokta çemberin **içindeyse** teğet yoktur ve motor
bir nokta uydurmak yerine hiçbir şey söylemez. Öncelikte gerçek olan her şeyin
altındadır: bu motorun icat ettiği bir nokta, çizimin gerçekten içerdiği bir
noktayı kullanıcıdan asla alamaz.

**Merkez ile ağırlık merkezi ayrı iki şeydir**, ve CAD bunları hep ayrı tutmuştur.
`Merkez`, bir **eğrinin** çizildiği noktadır — dairenin ya da yayın merkezi; bir
röperin aplike edildiği nokta odur. `Ağırlık merkezi`, kapalı bir şeklin **alan**
ağırlık merkezidir — parselin ortası. Tek bir bit ikisini birden karşıladığı sürece
ikincisi çalışıyor, birincisi hiç çalışmıyordu.

**Bir eğri, sakladığı köşelerden ibaret değildir.** Bir daire merkezini ve
yarıçapı veren doğu yönünde bir tutamağı saklar; yakalama bu ikiliyi çizilmiş bir
**kenar** gibi yürüyordu, yani kimsenin çizmediği bir doğrunun ortasını, en yakın
noktasını ve tutamağını köşe diye öneriyordu. Artık daire ve yay kendi
geometrileriyle okunuyor: merkez merkez, yayın uçları uç, yayın ortası **yay
boyunca** yarıda, ve `En yakın` çemberin kendisi — kirişin değil, ve yayın
süpürmediği yere oturmadan.

Onaltılık de yazabilirsiniz: `MOD yakalama_modları 0x2F`.

`0` bütün nesne yakalamayı kapatır. Kısayolu **F3**'tür.

8. ve 9. bitler kullanılmaz. 8 sonuçta dik modu bildirir ve maskeye yazılmaz;
9 DÜĞÜM için ayrılmıştı ve şimdilik boştur — gerekçesi aşağıdadır.

### Çizimde olmayan, ama çizimin ima ettiği noktalar

Son üç mod, yakalanacak şey **çizili değilken** işe yarar. Kadastro ve imar işinin
günlük hâli budur:

- **Uzantı** — köşe taşı kaybolmuş bir sınırı, ayakta kalan kenarın kendi
  doğrultusundan yeniden kurarsınız. İstenen nokta kenarın üzerinde değil,
  ucundan ötededir; *En yakın* oraya erişemez.
- **Paralel** — çekme mesafesi, yol kenarı ve ifraz hattı böyle çizilir: "şu
  sınırla aynı doğrultuda, buradan başlayarak". Doğrultuyu hiçbir yerden okumanız
  gerekmez. Önceki noktadan uzaklık korunur, yani yönü verdikten sonra
  yazacağınız ölçülmüş uzunluk aynen oturur.
- **Uzatılmış kesişim** — iki sınır birbirine yetişmeden kesiliyorsa, uzatılsalar
  buluşacakları köşeyi verir. *Kesişim* burada hiçbir şey bulmaz, çünkü kenarlar
  gerçekten kesişmez; köşe yine de parselin ihtiyacı olan noktadır.

Bu üç mod, **çizimde gerçekten olan hiçbir noktayı yenemez.** Öncelik sıralamasında
*En yakın*'ın da altındadırlar: motorun kurduğu bir nokta, kullanıcının elindeki
gerçek bir köşeyi asla kapmaz. Bu yüzden üçünü de açık bırakmak güvenlidir.

Üçü de imlecin altında olmayan bir kenardan nokta ürettiği için, açıklık tek başına
o kenarı bulamaz. `uzantı_çarpanı` tercihi, açıklığın kaç katı ötesine bakılacağını
söyler (varsayılan `10`). `0` yazılırsa üç mod da maskede açık olsa bile çalışmaz.

**DÜĞÜM (nirengi/poligon noktası) neden yok.** Bu sürümde çizimde nokta nesnesi
tutulamıyor: açık halka en az iki tepe noktası ister, `İÇEAKTAR` nokta katmanını
"bu sürüm çizgi ve alan okur" diyerek atlar ve nokta çizen bir komut yoktur. Var
olmayan bir şeye oturan bir yakalama modu, programın tutmadığı bir sözdür. DÜĞÜM,
nokta nesneleriyle birlikte gelecektir — öncesinde değil.

### Hangi yardım önce uygulanır

Sıra sabittir ve bilerek böyledir:

1. **Nesne yakalama** — gerçek bir nesnenin gerçek bir noktası her şeyi yener
   (kurulmuş noktalar bunun en altındadır; yukarıya bakın)
2. **Dik mod / kutupsal izleme** — önceki noktadan gelen yön kilidi
3. **Izgara** — geriye kalan hâlde en yakın kafes kesişimi

Bir parselin köşesine oturmuş noktayı ızgaraya çekmek, ikisinden de olmayan bir yer
üretirdi; bu yüzden birinci adım tuttuğunda diğerleri çalışmaz.

Dik mod ve kutupsal izleme yalnızca **önceki bir nokta varken** iş görür: ilk nokta
kilitlenecek bir yöne sahip değildir.

İki istisna vardır, ikisi de kilidin anlamsız olduğu yerler:

- **Bir dikdörtgenin karşı köşesi** — `DİKDÖRTGEN`, seçim penceresi, `ESNET`
  penceresi, çıktı alanı — dik moda kilitlenmez: eksene kilitli bir karşı köşe eni ya
  da boyu olmayan bir dikdörtgen olurdu. Köşegen kilidi (kare çizmek için) orada da
  çalışır.
- **Bir şeyin ölçülmediği nokta** — `BUDA`/`UZAT`'ta atılacak parçayı gösteren
  tıklama, `ESNET`'in ötelemesinin başlangıcı, `HİZALA`'nın ikinci kaynağı — bir
  önceki noktaya göre verilmez; oraya yön kilidi uygulanmaz.

### Tolerans ve ekran

Yakalama arama yarıçapı `yakalama_toleransı`, seçme kutusu `seçim_toleransı`
tercihidir ve ikisi de **ekran pikselidir** (bkz. [`TERCİH`](preference.md)). Nişan
alan göz ekrana bakar; tolerans yakınlaştırmayla birlikte değişmelidir.

Bunun bir sonucu vardır: **ekranı olmayan bir istemcide nesne yakalama etkisizdir.**
Başsız çalışan bir betik, bir toplu iş ve bir günlük tekrar oynatması yazdıkları
koordinatı aynen çizerler. Bu bir ayrıcalık değil, aynı kuralın (yarıçap = piksel ×
ölçek) ekransız bağlamdaki sonucudur — ve günlüğü dürüst tutan şeydir: kaydedilmiş
bir nokta, o sırada var olmayan bir komşuya sonradan yapışamaz.

Uygulama açıkken çalışan bir betiğin ekranı vardır ve elle çizim ile aynı yakalamayı
alır.

## Örnekler

### Komut satırı

Bütün modları görün:

```
MOD
```

Dik modu açın — yalnız yatay ve düşey çizgi çizilir:

```
MOD dik_mod evet
```

Kutupsal izlemeyi 30 dereceye çekin:

```
MOD kutupsal_açı 30000000
```

Izgaraya yakalamayı açın:

```
MOD ızgaraya_yakala evet
```

Bir modun ne olduğunu sorun:

```
MOD yakalama_modları
```

Bir modu varsayılanına döndürün:

```
MOD dik_mod varsayilan
```

Kutupsal açıyı matematik kuralıyla okuyun — `@100<0` doğuya gider — ve sonra semte dönün:

```
MOD kural matematik
MOD kural semt
```

**Ayarlar** penceresi (menüde `Düzen > Ayarlar…`, kısayolu **Ctrl+,**) bildirilen her
ayarı gösterir. Pencerenin tamamı ayar kataloğundan **üretilir**: satırın adı ayarın
kendi birincil adı, alanı bildirilen tipinden, sınırları bildirilen aralığından,
üzerine gelince çıkan açıklaması bildirilen özetinden gelir. Kataloğa eklenen bir ayar
bu pencereye kendiliğinden düşer.

Sol sütun **konuya** göre bölünmüştür — Genel, Görünüm ve Tema, Çizim ve Yakalama,
Koordinat Sistemleri… — çünkü bir ayar konusuyla aranır: `çizim birimi`, öteki genel
şeylerin yanındadır. Sayfanın başlığının altındaki tek satır o sayfadaki ayarların
hangi kapsamda olduğunu söyler; karışıksa onu da söyler.

## Proje Ayarları penceresi

`Seçenekler` "bu program nasıl davransın" sorusunu cevaplar. **`Dosya ▸ Proje
Ayarları…`** ise başka bir soruyu: **"bu dosyanın içinde ne var"**. Ayrı bir
penceredir, çünkü ikinci soruyu soran biri genellikle dosyayı birine vermek
üzeredir — ve ikisi çoğu zaman aynı anda açık durur.

İki sayfası vardır:

| Sayfa | Ne var |
|---|---|
| **Ayarlar** | Çizimle birlikte giden ayarların **tamamı**, konularına göre gruplanmış. Her biri `Seçenekler`'de kendi konu sayfasında da durur |
| **Öznitelikler** | Çizimdeki **her** nesnenin taşıdığı sütunlar. Bunlar ayar değil, belgenin şemasıdır — bkz. [`SÜTUN`](column.md). Yalnız bir katmana ait sütunlar o katmanın özelliklerinden tanımlanır |

İki pencere de **tek katalogdan üretilir**: bir ayarı konusundan da, Proje
Ayarları'ndan da değiştirseniz aynı komut çalışır. İkinci bir liste yoktur.

Her satırın sağında değerin sizin mi yoksa programın mı olduğu (`ayarlanmış` /
`varsayılan`) ve varsayılana döndüren bir düğme vardır. Üstteki arama kutusu ad,
kimlik ve açıklama üzerinde birden arar.

Penceredeki her değişiklik komut yolundan geçer: kapsamına göre `AYAR`, `TERCİH` ya da
`MOD` komutu kurulup çalıştırılır. Yani transkriptte, günlükte ve yeniden oynatmada
pencereden yapılanla komut satırına yazılan arasında hiçbir fark yoktur.

Denemeyi bitirince açtığınız yardımları kapatın; oturum modları siz kapatana kadar
açık kalır:

```
MOD ızgaraya_yakala varsayilan
MOD kutupsal_açı varsayilan
MOD yakalama_modları varsayilan
```

### Arayüz

Komutu pencerenin altındaki **komut satırına** yazın; sonuç **Transkript** panelinde
görünür. Arayüzün ayrıcalığı yoktur: menüden yapılan da, komut satırından yazılan da aynı
komuttur.

**Görünüm** menüsündeki üç kalem ve kısayolları bu komutu çalıştırır; ikinci bir mod
listesi yoktur:

| Kalem | Kısayol | Gönderdiği komut |
|---|---|---|
| Nesne Yakalama | **F3** | `MOD yakalama_modları <maske>` |
| Dik Mod | **F8** | `MOD dik_mod evet` / `hayır` |
| Yüzey Normali | **F10** | `MOD yüzey_normali evet` / `hayır` |
| Izgaraya Yakala | **F9** | `MOD ızgaraya_yakala evet` / `hayır` |

Yüzey normalinin bir de **basılı tutma** yolu vardır: bir komut nokta beklerken tuval
üzerinde **Shift**'i basılı tutun, kilit tuttuğunuz sürece açık kalır, bıraktığınızda
kapanır. **F10** ise kilidi mandallar. İkisi de aynı ayarı yazar; hangisini
kullandığınızın komutun gördüğü değere etkisi yoktur.

Menü kalemlerinin işareti değerin kendisinden okunur: komut satırına
`MOD dik_mod evet` yazdığınızda **F8**'e basmışsınız gibi işaret gelir.

F3 yakalamayı kapatırken maskeyi hatırlar; yeniden açtığınızda seçtiğiniz modlar geri
gelir, varsayılana dönmez.

Bir komut nokta beklerken imlecin altında **yakalama işareti** belirir: her modun
kendi sembolü ve adı vardır — uç nokta kare, orta nokta üçgen, merkez daire, kesişim
çarpı, dik ayak dik açı işareti, en yakın kum saati, ızgara kafes, kutupsal ve dik mod
baklava. Kesikli kılavuz çizgi de yakalanan noktaya uzanır, çünkü çizgi oraya
düşecektir.

### Betik

```json
{
  "ad": "Dik çizim ortamı",
  "komutlar": [
    { "cmd": "core.mode", "args": { "ad": "core.yakalama.dik_mod",     "deger": "evet" } },
    { "cmd": "core.mode", "args": { "ad": "core.yakalama.izgara",      "deger": "evet" } },
    { "cmd": "core.mode", "args": { "ad": "core.yakalama.kutupsal_aci","deger": "30000000" } },
    { "cmd": "core.mode", "args": { "ad": "core.aci.kural",              "deger": "semt" } }
  ]
}
```

## Geri alma

`MOD` **geri alınamaz ve geri alınmamalıdır.** `GERİAL` çizimin verisini geri alır; bir
girdi yardımı çizimin verisi değildir. Aynı sebeple mod değişiklikleri komut günlüğüne de
yazılmaz: günlük, belgeye ne olduğunun kaydıdır.

Geri dönmek için modu varsayılanına döndürün ya da eski değerini yeniden yazın:

```
MOD dik_mod varsayilan
```

Komut, değiştirdiği her modun önceki değerini de yazar; not almak için oradan
okuyabilirsiniz.

## Betikten kullanım

`MOD` betiklenebilir. Bir kurulum betiğinin başında çizim ortamını hazırlamak için
kullanılır:

```json
[
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.dik_mod", "deger": "evet" } },
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.izgara",  "deger": "evet" } }
]
```

Modlar oturumla birlikte biter, bu yüzden bir betiğin açtığı dik mod bir sonraki
oturuma taşınmaz. Kalıcı olmasını istediğiniz şey bir mod değil, bir tercihtir; onlar
[`TERCİH`](preference.md) komutuna aittir.

`MOD` AI erişimine kapalıdır: bir öneri motoru kendi işini kolaylaştırmak için
kullanıcının çizim yardımlarını değiştiremez.

Bir betiğin çizdiği noktaların **hiç** yönlendirilmemesini istiyorsanız, betiğin
başında yakalamayı kapatın:

```json
[
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.modlar",  "deger": "0" } },
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.dik_mod", "deger": "hayır" } },
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.izgara",  "deger": "hayır" } },
  { "cmd": "core.line", "args": { "noktalar": [[485320150,4310220400],[485370150,4310250400]] } }
]
```

Kadastro koordinatını milimetresi milimetresine çizen bir betik için doğru alışkanlık
budur.

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bilinmeyen ayar: 'dikmod'. Beklenen: tanımlı bir ayar kimliği veya adı (23 tanımlı ayar).` | Mod adı yanlış yazılmış | Mesajın devamındaki `Bunu mu demek istediniz:` önerisine bakın veya `MOD` yazıp listeyi görün |
| `'core.crs.id' ayarı proje kapsamındadır; bu komut oturum ayarlarını yönetir.` | Proje ayarı `MOD` ile değiştirilmeye çalışılmış | [`AYAR`](setting.md) komutunu kullanın |
| `'core.arayuz.tema' ayarı uygulama kapsamındadır; bu komut oturum ayarlarını yönetir.` | Tercih `MOD` ile değiştirilmeye çalışılmış | [`TERCİH`](preference.md) komutunu kullanın |
| `'core.yakalama.dik_mod' ayarı evet/hayır bekliyor. Girilen: 'açık'` | Evet/hayır isteyen bir moda başka bir şey verilmiş | `evet`, `hayır`, `1` veya `0` yazın |
| `'core.yakalama.kutupsal_aci' için 400000000 değeri [1000, 360000000] aralığının dışında; 360000000 değerine kırpıldı.` | Değer bildirilen aralığın dışında | Hata değildir: değer aralığa kırpılır ve size söylenir |
| `'core.mode' daha fazla argüman almıyor. Fazlalık: 'evet'` | İkiden fazla argüman verilmiş | Boşluk içeren değerleri tırnak içine alın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
