# Arayüz

KentOSCad penceresini yeni açan kullanıcı için; bu sayfayı bitirdiğinizde her panelin ne
işe yaradığını, nasıl taşınacağını ve fareyle klavyeyle neyin nasıl yapılacağını
bileceksiniz.

## Pencere düzeni

```text
┌ sistem başlık çubuğu (işletim sistemi çizer) ─────────────────────── ─ □ ✕ ┐
├ menü şeridi 34 px ────────────────────────────────────────────────────────┤
│ Dosya Düzen Görünüm Çizim Değiştir Harita Analiz Katman Pencere Yardım    │
│                      <belge adı>              [🔍 Komut ara… ⌘K]  (MK)    │
├ araç çubuğu 46 px ────────────────────────────────────────────────────────┤
│ 📄📂💾 │ ↶↷ │ ✂⧉📋 │ ➤✋🔍⛶ │ ⊞⊙📏 │ ▤🎨▦ │ 🖨⚙   ÖLÇEK 1:1 000  EPSG… │
├───────┬───────────────────────────────────────────┬───────────────────────┤
│ araç  │ ┌ doküman sekmeleri 30 px ──────────────┐ │ Öznitelikler │Geçmiş │
│ kutusu│ │ 📄 kadastro_ada_1284  ✕ │ 🌐 imar…    │ │ SEÇİLİ NESNE          │
│ 46 px │ ├───────────────────────────────────────┤ │ ⬠ Parsel 1284 / 21    │
│       │ │ ┌cetvel 20 px──────────────────────┐  │ │   POLYGON · fid 4128  │
│ 5 grup│ │ │            tuval                 │  │ ├───────────────────────┤
│ 19    │ │ │   ızgara · kuzey oku · ölçek     │  │ │ ⌄ KİMLİK              │
│ araç  │ │ └──────────────────────────────────┘  │ │   ada_no      1284    │
│       │ ├ komut satırı 28 px ───────────────────┤ ├─ Katmanlar ───────────┤
│ ▣ ▢   │ │ Komut: _PARSELBOL …                   │ │ 👁 ▪ Kadastro  1 482🔒│
├───────┴───────────────────────────────────────────┴───────────────────────┤
│ ⊕ Y 458 214.362  X 4 512 908.771 │IZGARA│YAKALAMA│DİK│…  PostGIS · 60 fps │
└ durum çubuğu 26 px ───────────────────────────────────────────────────────┘
```

Her bandın yüksekliği sabittir ve her platformda aynıdır: menü şeridi 34, araç çubuğu 46,
doküman sekmeleri 30, komut satırı 28, durum çubuğu 26 piksel. Sol araç kutusu 46,
sağ panel 312 piksel genişliğindedir. Bu ölçüler tasarım belgesinden gelir ve
Windows, macOS ve Linux'ta değişmez. **Pencere çerçevesi bu ölçülerin dışındadır:**
çerçeveyi, başlık çubuğunu ve pencere düğmelerini işletim sistemi çizer, dolayısıyla
onlar her masaüstünde o masaüstünün alışıldık görünümündedir.

İki ayrı araç yüzeyi vardır ve işleri farklıdır:

- **Araç kutusu** (sol, dikey, 46 px) — çizim ve düzenleme araçları, beş grup.
  Bir araca basınca komut başlar ve sizden girdi ister. En altta iki renk kutusu:
  çizim rengi ve dolgu rengi.
- **Araç çubuğu** (üst, yatay, 46 px) — eylemler, yedi grup: dosya · geri/yinele ·
  pano · gezinme · yardımcılar · pencereler · çıktı. Bir düğmeye basmak komutu
  hemen çalıştırır. Sağ ucunda iki salt-okunur okuma vardır: **ÖLÇEK** ve
  **KOORDİNAT SİSTEMİ**.

Bu ayrım AutoCAD ve QGIS'in ortak düzenidir.

## Pencere çerçevesi ve menü şeridi

Pencerenin çerçevesi, başlık çubuğu ve **kapat / küçült / büyüt** düğmeleri işletim
sistemine aittir. KentOSCad bunları kendisi çizmez: pencereyi kenarlarından tutup
boyutlandırmak, ekran kenarına yapıştırmak, sağ tıkla pencere menüsünü açmak ve
çift tıkla büyütmek masaüstünüzün kendi davranışıdır. Başlık çubuğunda
`<belge adı> — KentOSCad <sürüm>` yazar.

Onun hemen altındaki 34 px'lik **menü şeridi** uygulamanındır: solda on menü,
ortada açık belgenin adı ve sürümü, sağda **komut arama** ile kullanıcı baş harfi.

### Komut listesi — `Ctrl+K`

Kutucuğa tıklayın, **Ctrl+K**'ya basın, **Yardım > Komut Listesi**'ni (`F1`) seçin
ya da komut satırına `YARDIM` yazın: dördü de aynı sayfayı açar. Üstte süzgeç, solda
kategori başlıkları altında bütün komutlar, sağda imlecin üzerinde olduğu komutun
aldığı parametreler.

| Bölüm | Ne gösterir |
|---|---|
| Süzgeç | Yazdıkça süzer; ad, kısaltma, komut kimliği ve açıklama aranır |
| Sol liste | Çizim · Düzenleme · Görünüm · Katman · Dosya · Sorgu · İşlem · Betik · Sistem başlıkları altında komut adı, tek satır açıklaması ve sağ kenarda kısaltmaları |
| Sağ bölme | Komutun kategorisi, kimliği, kabul ettiği bütün yazımlar, açıklaması ve parametreleri — her parametrenin tipi, gerekliliği, aralığı, birimi ve varsa sözcük listesi |
| Alt satır | Komut sayısı ve tuşlar |

Liste `Registry`'den üretilir; yani bugün var olan her komut oradadır ve yarın eklenen
komut da hiçbir liste güncellenmeden orada olur.

Arama Türkçe katlamayla çalışır: `cizgi` yazınca `ÇİZGİ`, `olcek` yazınca `ÖLÇEK`
bulunur. `↑` `↓` gezinir, `Enter` seçili komutu komut satırına yazıp imleci sonuna
koyar — çünkü argümanı olan bir komutun argümanı yazılmalıdır — `Esc` kapatır.

### Komutun istediği şeyi vermek

Çalışan bir komut ne istediğini komut satırının solunda yazar. İstediği şeye göre
klavyenin nerede olacağı değişir:

| Komut ne istiyor | Nasıl verilir |
|---|---|
| **Nokta** | Tuvale tıklayın; ya da koordinatı komut satırına yazın (`485320,4310220`, `@50,30`, `@100<45`) |
| **Nesne** | Tuvalde seçin, sonra **Enter** |
| **Ad** (blok, katman, desen) ya da **sayı** | Odak kendiliğinden komut satırına geçer ve yazılacak yer hazır olur |

Cevabı belli bir kümeden olan istemler o kümeyi de gösterir: **Blok Ekle**
çizimdeki blokların adlarını, **Katman** ve **Etiket** katman adlarını,
**Katman Görünümü** alabileceği işlem sözcüklerini listeler. Listeden seçmek de
yazmak da olur.

Sözle cevap veren komutlar (Katmanları Listele, Görünüm Bilgisi, Seçim Bilgisi,
Sorgula) cevabı sağ panelin **Geçmiş** sekmesine yazar ve o sekmeyi kendiliğinden
öne getirir.

## Menü çubuğu

On menü, hep bu sırayla: **Dosya · Düzen · Görünüm · Çizim · Değiştir · Harita ·
Analiz · Katman · Pencere · Yardım**. Sıra tasarımın parçasıdır: `Harita`'nın
nerede olduğunu öğrenen kullanıcı onu her platformda aynı yerde bulur.

Kısayol altçizgileri yalnızca **Alt** basılıyken görünür. Alt+D hâlâ Dosya'yı
açar; altçizgi, ekranı boş yere doldurmasın diye gizlidir.

| Menü | İçerik |
|---|---|
| **Dosya** | Yeni (**Ctrl+N**), Aç (**Ctrl+O**), Kaydet (**Ctrl+S**), Farklı Kaydet… (**Ctrl+Shift+S**), İçe/Dışa Aktar…, Yazdır, **Proje Ayarları…**, Veritabanı… (**Ctrl+Shift+D**), Betik Çalıştır… (**Ctrl+R**), Çıkış |
| **Düzen** | Geri Al (**Ctrl+Z**), Yinele (**Ctrl+Shift+Z**), Tümünü Seç (**Ctrl+A**), Seçimi Temizle (**Ctrl+Shift+A**), Ayarlar… (**Ctrl+,**) |
| **Görünüm** | Kapsama Yakınlaş (**Ctrl+0**), Yakınlaştır, Uzaklaştır, Nesne Yakalama (**F3**), Dik Mod (**F8**), Yüzey Normali (**F10**), Izgaraya Yakala (**F9**), Araç Çubuğu, Paneller, Koyu Tema, Geliştirici Bilgisi (**F12**) |
| **Çizim** | Çizgi, Çoklu Çizgi, Yay, Daire, Dikdörtgen, Nokta, Metin |
| **Değiştir** | Sil, Taşı, Kopyala, Döndür, Ofset |
| **Harita** | Sorgula, Ölç, Veritabanı… |
| **Analiz** | Öznitelik Tablosu (**F6**), Yapay Zekâ Önerisi |
| **Katman** | Katman, Katman Yöneticisi · Tümünü Göster, Gösterimi Ters Çevir |
| **Pencere** | Panellerin açık/kapalı durumu, Yerleşimi Sıfırla |
| **Yardım** | Komut Listesi (**F1**), Hakkında |

Menülerin sonundaki **Diğer komutlar** alt menüsü o kategorinin geri kalan
komutlarını taşır. Kendi yeri, kısayolu ve simgesi olan komutlar yukarıda
listelenir; **Diğer komutlar** ise komut kaydından üretilir, yani yeni eklenen
her komut kendiliğinden oraya düşer. Böylece bir komutun yalnız adını yazarak
erişilebildiği bir durum kalmaz: her komut ya bir düğmeden ya bir menüden
başlatılabilir.

Henüz yazılmamış bir özelliğin satırı (Kes, Panoya Kopyala, Yapıştır, Katman
Yöneticisi) tıklanınca ne yapacağını, hangi fazda geleceğini ve bugün onun
yerine ne kullanılacağını söyleyen bir pencere açar.

## Araç çubuğu

Yedi grup, aralarında 1 piksellik ayraçlar. Butonlar 30 × 30 piksel, adım 34.

| Grup | Düğmeler |
|---|---|
| **dosya** | Yeni · Aç · Kaydet |
| **geri/yinele** | Geri Al · Yinele |
| **pano** | Kes · Kopyala · Yapıştır (Faz 2) |
| **gezinme** | Seç · Kaydır · Yakınlaştır · Tümünü Göster |
| **yardımcılar** | Izgaraya Yakala · Nesne Yakalama · Ölç |
| **pencereler** | Katman Yöneticisi · Stil Tasarımcısı · Öznitelik Tablosu |
| **çıktı** | Yazdır · Ayarlar |

Sağ uçtaki iki okuma değiştirilemez, yalnızca okunur:

- **ÖLÇEK** — pafta ölçeği, `1 : 1 000` biçiminde. Bir kâğıt milimetresinin kaç
  zemin milimetresi taşıdığını söyler; yakınlaştırdıkça değişir.
- **KOORDİNAT SİSTEMİ** — `EPSG:5254 · ITRF96 / TM30` gibi. Çözümlenmemiş bir
  sistem "çözümlenmedi" yazar; bir mühendisin çizmeye başlamadan önce baktığı
  okuma budur.

## Araç kutusu

Beş grup. Aktif araç vurgu rengiyle işaretlenir.

**Bir araç elinizde kalır.** Çizgi'ye bastıysanız çizgi çizersiniz; bir çizgiyi **sağ
tıkla** bitirdiğinizde araç bırakılmaz, sıradaki çizgi için hazır bekler. Aynısı
Taşı, Alan Ölç, Ölçü ve diğer her araç için geçerlidir: **sol tuş başlatır, sağ tuş
bitirir, araç seçili kalır**. Aracı bırakmanın iki yolu vardır: **Esc** ya da
**Seç** okuna (veya başka bir araca) basmak. Nesne isteyen bir araç yeniden
hazırlanırken seçimi temizler ve sorusunu baştan sorar; bir önceki taşımanın
nesneleri elinizde kalmaz.

| Grup | Araçlar |
|---|---|
| **seçim** | Seç · Alan Seç · Kaydır |
| **oluşturma** | Çizgi ▸ · Dikdörtgen ▸ · Daire ▸ · Yay ▸ · Nokta · Metin · Blok Ekle ▸ · Ölçü ▸ *(▸ aileler)* |
| **düzenleme** | Böl/Buda · Birleştir (tevhit) · Parsel Böl (ifraz) · Taşı · Ofset |
| **ölçüm** | Uzunluk Ölç ▸ *(aile)* |
| **yardımcı** | Stil Kopyala · Topoloji Denetimi |

Üst araç çubuğunda **Kaydet**'in sağında **Yazdır** durur; yanındaki küçük ok
yazdırma profillerini listeler. İlk basış tuvalde [yazdırma alanı](yazdirma.md)
çerçevesini açar, ikincisi önizlemeye geçer.

### Araç aileleri

Sütun 46 piksel geniştir; on bir çizim aracını alt alta dizmek okunmayan bir liste
yapardı. Birbirinin yerine geçen araçlar **tek düğmede** toplanır; düğme en son
kullandığınız aracı gösterir, ailenin geri kalanı bir basış ötededir. Böyle bir
düğmenin sağ alt köşesinde küçük bir **köşe işareti** vardır.

Gruplama **ne çizdiğinize** göredir, kalemin düz gidip gitmediğine göre değil: bir
dikdörtgen bir çizgi türü değildir — alanı, çevresi ve dolgusu olan bir **yüzdür**, ve
yeri diğer yüz üreten aracın yanıdır. Aynı biçimde daire kapalıdır ve bir şeyi çevreler,
yay ise açık bir kenardır ve hiçbir şeyi çevrelemez.

| Düğme | Ailesi | Ortak yanı |
|---|---|---|
| Çizgi | `ÇİZGİ` · `ÇOKLUÇİZGİ` · `SPLINE` | açık kenar dizisi: düz, kırık, pürüzsüz |
| Dikdörtgen | `DİKDÖRTGEN` · **döndürülmüş** · `ALAN` · `ÇOKGEN` · **dıştan** · **kenardan** · `TARAMA` | kapalı yüz; tarama desenli yüzdür |
| Daire | `DAİRE` · **çapın iki ucu** · **üç nokta** · **iki doğruya teğet** · `ELİPS` · **eksenin iki ucu** · `HALKA` | kapalı eğri |
| Yay | `YAY` · **üç nokta** · **başlangıç-merkez-açı** · **başlangıç-bitiş-yarıçap** · **teğet devam** · `DİLİM` | açık eğri ve ondan kesilen dilim |
| Nokta | `NOKTA` · `DİKAYAK` · `ALIM` · `KESİŞİMNOKTA` · `ARANOKTA` | tek nokta koymanın beş yolu |
| Blok Ekle | `BLOKEKLE` · `BLOK` | blok yerleştirmek ve tanımlamak |
| Ölçü | `ÖLÇÜ` · `LİDER` | açıklama: ölçü ve not oku |
| Uzunluk Ölç | `ÖLÇ` · `ALANÖLÇ` · `AÇIÖLÇ` · `KOORDİNAT` · `NESNEBİLGİ` | ölçme ve sorma |

### İnşa yöntemleri de birer araçtır

Kalın yazılan satırlar bir komutun **inşa yöntemleridir**: üç noktadan daire, iki
doğruya teğet daire, teğet devam eden yay, döndürülmüş dikdörtgen. Hepsi `yontem=`
ile yazılabilir, ama **yazmak zorunda değilsiniz** — ailede kendi satırları var.

Kartın sağ kolonu her satırın **tam olarak ne gönderdiğini** yazar
(`YAY yontem=3n`), yani kart aynı zamanda komut satırını öğretir. Bir yöntemi
fareyle bir kez kullanıp sonra yazmaya geçmek isteyen kullanıcı, yazacağı şeyi
zaten görmüş olur.

Aileyi açmanın üç yolu vardır: düğmeyi **basılı tutmak**, köşe işaretine **tıklamak**
ya da düğmeye **sağ tıklamak**. Kısa bir tıklama aileyi açmaz, düğmenin yüzündeki
aracı çalıştırır.

Kart açıldıktan sonra iki türlü seçebilirsiniz: parmağınızı **kaldırmadan** bir satırın
üstüne kayıp orada bırakmak, ya da **bırakıp** kartı okuduktan sonra tıklamak. Tuşu
bırakmak kartı kapatmaz — kart ancak bir satır seçilince, **Esc**'e basılınca ya da
dışına tıklayınca kapanır.

Açılan kart, her aracın adının yanına **komut adını** da yazar. Bu bilerek yapılmıştır:
düğmeyle bulduğunuz aracı yarın komut satırına yazabilesiniz diye. Kart ok tuşlarıyla
gezilir, **Enter** ile seçilir, **Esc** ile kapanır.

Ailedeki her araç ayrıca **Çiz** menüsünde kendi kalemiyle durur ve kendi adıyla
komut satırından çağrılabilir; aile düğmesi bir kısayoldur, tek yol değildir.

Faz 2'de gelecek araçlar pasiftir ve hangi fazda geleceklerini ipucunda yazarlar —
görünmez olmaları, yokmuş gibi davranmaktan daha kötü olurdu.

En altta iki kutu: üstteki **çizim rengi**, alttaki **dolgu rengi**. İçi boş bir
alt kutu "dolgu yok" demektir.

### Klavyeyle ve ekran okuyucuyla

Araç kutusu **tek bir Tab durağıdır**: Tab ile kolona gelirsiniz, içinde ok
tuşlarıyla gezersiniz. Her düğmeye ayrı bir Tab durağı verilseydi menü şeridiyle
tuval arasında otuz durak olurdu; araç paletlerinin ve araç çubuklarının her
yerdeki davranışı budur.

| Tuş | Ne yapar |
|---|---|
| **Tab** | Kolona girer. Ok halkası **o an çalışan** aracın üstünde belirir |
| **↑ / ↓** | Bir üstteki / alttaki araca geçer; pasif araçları atlar, uçtan başa döner |
| **Home / End** | İlk / son araca gider |
| **Boşluk** ya da **Enter** | Halkadaki aracı çalıştırır — farenin yaptığı işin aynısı |
| **→** | Bir aile düğmesinin kartını açar. Kart ok tuşlarıyla gezilir, **Enter** seçer, **Esc** kapatır |

Halka yalnız klavye odağında çizilir; fareyle bir araca basmak odağı tuvalden
almaz, çünkü komut çalışırken Esc'in ve ok tuşlarının yeri tuvaldir.

Ekran okuyucu (VoiceOver, NVDA, Orca) her düğmeyi **adıyla ve ipucuyla** okur ve
çalışan aracı "işaretli" diye söyler. Bir düğmeye erişilebilirlik katmanından
basmak — VoiceOver'da **Ctrl+Option+Boşluk** — aracı gerçekten **çalıştırır**.

> Bu, 0.1.0'a kadar böyle değildi: düğmeler `checkable` olduğu için macOS
> erişilebilirlik katmanı basışı bir "geçiş"e eşliyor, düğme yanıyor ve komut
> çalışmıyordu. Ekran okuyucu kullanan biri için her çizim aracı yanıp hiçbir şey
> yapmıyordu. Ayrıntı için değişiklik günlüğüne bakın.

Ailedeki her araç ayrıca **Çiz** menüsünde durur ve komut satırından adıyla
çağrılabilir; klavye yolu bunlarla da tamdır.

## Doküman sekmeleri

Tuvalin üstünde, 30 piksel. Her açık çizim bir sekmedir. Etkin sekme üst kenarındaki
**2 piksel vurgu çizgisiyle** işaretlenir ve zemini tuvalin zeminidir — altındaki
çizgi onun altında kesilir, böylece sekme gösterdiği çizime bağlanır. Sağ uçta
bölünmüş görünüm ve tam ekran düğmeleri bulunur.

Son sekmenin hemen ardında **+** düğmesi vardır: boş bir çizim başlatır ve
[`YENİ`](../komutlar/new.md) komutunu gönderir — `Dosya ▸ Yeni` ile ve **Ctrl+N**
ile aynı komuttur. Kaydedilmemiş değişikliğiniz varsa önce **Kaydet / Atla /
Vazgeç** sorusu gelir.

**Bugün tek sekme görünür** ve **kapatma işareti yoktur.** Tek çizimin kapatılacağı
bir yer yok: kapatmak programı belgesiz bırakırdı, ki öyle bir durumu yok. İşaret,
ikinci bir sekme var olabildiği gün geri gelir.

Birden çok çizimi **aynı anda** açmak henüz gelmedi: `YENİ` yeni bir sekme açmaz,
açık olanın yerine geçer.

## Harita alanı

Çizimin göründüğü yer.

| Etkileşim | Sonuç |
|---|---|
| Sol tık, komut nokta beklerken | Çalışan komuta bir nokta verir |
| Sol tık, komut nesne beklerken | Nesneyi seçime **ekler**; **Ctrl** ile çıkarır |
| Sol tık, komut yokken | İmlecin yakınındaki nesneyi seçer |
| Sol tuş basılı sürükle, komut yokken | Seçim kutusu çizer |
| **Shift** + tık/sürükle | Seçime ekler |
| **Ctrl** + tık/sürükle | Seçimden çıkarır |
| Sağ tık, komut nesne beklerken | Seçilenleri komuta verir (Enter ile aynı) |
| Sağ tık, komut nokta beklerken | Şekli olduğu yerde **bitirir**; araç elde kalır |
| Orta tuş basılı sürükle | Görünümü kaydırır |
| Fare tekerleği | İmlecin bulunduğu noktaya yakınlaştırır/uzaklaştırır |
| **Esc** | Çalışan komutu iptal eder ve aracı bırakır; komut yoksa seçimi temizler |

İmleç bir **CAD nişanıdır**: ortası boş bırakılmış yatay ve dikey iki çizgi, ortasında
da **seçim kutusu** — bir tıklamanın neyi tutacağını gösteren kare. Karenin kenarı
`Ayarlar > Uygulama > Seçim` altındaki toleransın iki katıdır; ne gösteriyorsa `SEÇ`
onu tutar. Komut bir **nokta** beklerken kare kaybolur ve nişan yalın artıya döner,
çünkü o anda tıklama bir koordinat bırakır, bir şey tutmaz. İşletim sisteminin ok
imleci tuvalin üstünde gizlidir; nişanın kendisi imleçtir. Koordinat durum çubuğunda
yazar.

Bir komut nokta beklerken imlecin altında **yapılacak şeklin hayaleti** durur, yalnız
bir kılavuz çizgi değil: `ÇİZGİ`'de sıradaki kenar, `ALAN` ve `TARAMA`'da o âna kadarki
halka, `DİKDÖRTGEN`'de yüz, `DAİRE`/`YAY`/`DİLİM`'de eğri, `ELİPS`'te üçüncü tıkla
oluşacak elips, `SPLINE`'da kontrol noktalarından geçen eğri, `ÖLÇÜ`'de uzatma çizgileri
ve oklarıyla ölçü, `BLOKEKLE`'de bloğun kendisi, `TAŞI` ve `KOPYALA`'da taşınan
nesnelerin kendileri. Hayalet, tıklamanın üreteceği geometriyi çizen aynı kodla
çizilir; ne görüyorsanız onu alırsınız.

Bir komut uzun bir işi ayrı iş parçacığına verdiğinde (bugün: [`İÇEAKTAR`](../komutlar/import.md)
dosyayı okurken) durum çubuğunun mesaj hücresi işin adını, altında kayan bir şeridi ve
yanında **Durdur** çipini gösterir. Pencere donmaz; **Durdur** ya da **Esc** işi keser ve
çizim değişmeden kalır.

### Harita üzerindeki yardımcılar

Çizimin üzerinde, çizime ait olmayan dört şey durur. Dördü de `Ayarlar > Uygulama >
Harita` ve `Cetvel` altından kapatılabilir; hiçbiri dosyaya girmez.

| Yardımcı | Ne söyler | Ayarı |
|---|---|---|
| **Cetvel** | Tuvalin üstünde ve solunda, zemin ölçüsünü rakamla | `cetvel_görünür`, `cetvel_kalınlığı`, `cetvel_birimi` |
| **Ölçek çubuğu** | Sol altta, o anki yakınlaştırmanın yuvarlak bir zemin uzunluğu karşılığını | `ölçek_çubuğu` |
| **Kuzey oku** | Sağ üstte, kuzeyin yönünü | `kuzey_oku` |
| **Koordinat göstergesi** | Sol altta, imlecin sağa/yukarı değerini | `koordinat_göstergesi` |

Koordinat göstergesi, bir yakalama tuttuğunda **yakalanmış** noktayı yazar; tıklamanın
üreteceği koordinat odur, imlecin durduğu ham nokta değil.

Cetvelin ve ölçek çubuğunun rakamları 1-2-5 merdivenine oturur (1, 2, 5, 10, 20, 50 …):
aralıkları 137 metre olan bir cetvelden kimse mesafe okuyamaz.

Nişan imleci `imleç` tercihiyle üç hâlde olabilir — tuvali baştan başa geçen çizgiler
(`tam_ekran`), kısa bir artı (`kısa`, uzunluğu `imleç_boyu` ile) ya da hiç (`yok`).
`yok` seçilirse işletim sisteminin ok imleci geri gelir. Seçim kutusu her iki nişanda
da ortada durur; boyu `Seçim > tolerans` tercihinden gelir.

### Seçim

Hiçbir komut çalışmıyorken sol fare tuşu seçim yapar. **Soldan sağa** sürüklerseniz
kutuya **tamamen giren** nesneler seçilir ve çerçeve düz çizilir; **sağdan sola**
sürüklerseniz kutuya **değen** her nesne seçilir ve çerçeve kesik çizilir. Bu, CAD
dünyasının kırk yıllık ayrımıdır ve KentOSCad'de de aynıdır.

Seçili nesneler kalın ve renkli çizilir. Seçim çizimin verisi değildir: dosyaya
yazılmaz, `GERİAL` ile geri alınmaz ve komut günlüğüne belge değişikliği olarak
düşmez.

Fareyle yaptığınız her seçim, komut satırına `SEÇ ...` yazmakla aynı komuttur.
Ayrıntı: [Nesne seçme](../komutlar/select.md).

### Nesne yakalama

Bir komut nokta beklerken imleç, yakınındaki gerçek geometriye **oturur**: bir köşeye,
bir kenarın ortasına, kapalı bir halkanın merkezine, iki kenarın kesişimine, önceki
noktadan indirilen dikin ayağına ya da en yakın kenar noktasına.

Hangi modun tuttuğu ekranda görünür: imlecin altında o moda ait bir işaret ve adı
belirir. Kesikli kılavuz çizgi de yakalanan noktaya uzanır, çünkü çizgi oraya
düşecektir.

**Geçici izleme** bir noktayı, çizimde hiçbir geometrinin bulunmadığı bir yere
koymanın yoludur: *şu köşeyle aynı hizada, bu köşeyle aynı doğrultuda*. Nokta
isteminde **Shift + sağ tık** ile bir köşeyi işaretlersiniz; işaretli her noktadan
yatay ve düşey bir kesikli **iz** geçer ve iki izin kesişimi yakalanır. İkinci
işaretten sonra kesişim, tek bir izi yener. Ayrıntısı
[`İZ`](../komutlar/tracking.md) sayfasındadır.

| Kısayol | Ne yapar |
|---|---|
| **F1** | Komut listesi sayfası — `Ctrl+K` ile aynı |
| **F3** | Nesne yakalamayı açar/kapatır |
| **Shift+F3** | **Yakalama modları listesini açar** — hangi modların açık olduğunu seçersiniz |
| **F8** | Dik modu açar/kapatır — imleci yatay ve düşey eksene kilitler |
| **F10** | Yüzey normalini açar/kapatır — imleci başlanan **kenara** dik kilitler |
| **Shift** (basılı) | Bir komut nokta beklerken yüzey normalini **tuttuğunuz sürece** açar |
| **Ctrl** (basılı) | Köşegen kilidi: imleci öncekinden 45°'nin katlarına kilitler |
| **F9** | Izgaraya yakalamayı açar/kapatır |
| **Del** ya da **⌫** | Seçili nesneleri siler (`SİL`). Mac klavyesinde ⌦ tuşu çoğu zaman yoktur; ⌫ (Backspace) da siler |
| **Shift + sağ tık** | Bir komut nokta beklerken imleçteki noktayı **izleme için işaretler** ([`İZ`](../komutlar/tracking.md)) |

#### Hangi modlar açık

Durum çubuğundaki **OSNAP** çipine **sağ tıklayın** — ya da **Shift+F3** ile,
**Görünüm ▸ Yakalama Modları…** ile aynı listeyi açın. Her satır bir moddur ve
işaretlendiğinde o mod açılır:

| Mod | Neye oturur |
|---|---|
| **uç nokta** | Bir halkanın köşesi — parsel köşesi, bina köşesi |
| **orta nokta** | Bir kenarın tam ortası |
| **merkez** | Bir **eğrinin** çizildiği merkez: dairenin, yayın |
| **ağırlık merkezi** | Kapalı bir halkanın alan ağırlık merkezi — parselin ortası |
| **kesişim** | İki kenarın gerçekten kesiştiği yer |
| **dik ayak** | Önceki noktadan bir kenara indirilen dikin ayağı |
| **en yakın** | Kenarın imlece en yakın noktası — çizginin **herhangi bir** noktası |
| **düğüm** | Ölçülmüş tek nokta: nirengi, poligon noktası, röper |
| **ızgara** | En yakın ızgara kesişimi |
| **kutupsal** | Önceki noktadan çıkan kutupsal ışın |
| **uzantı** | Bir kenarın kendi ucundan öteye uzanan doğrusu |
| **paralel** | Önceki noktadan çıkan, bir kenara paralel ışın |
| **uzatılmış kesişim** | İki kenarın doğrularının kesişeceği yer — ikisi de oraya kadar uzanmasa bile |
| **kılavuz** | Cetvelden çektiğiniz yapı çizgisi; iki kılavuz kesişiyorsa kesişimi |

Varsayılan olarak açık olanlar: uç nokta, orta nokta, merkez, kesişim, **en yakın**,
düğüm ve ağırlık merkezi.

Son üçü **kurulmuş** noktalardır: çizimde öyle bir nokta yoktur, geometri onu ima
eder. Bu yüzden glifleri **açıktır** — içinde boşluk olan bir şekil — ve sıralamada
her gerçek köşenin **altındadırlar**: kurulmuş bir nokta, var olan bir köşeyi asla
elinden alamaz.

Aynı açıklıkta birden çok aday varsa sıra şudur: **düğüm → uç → kesişim → orta →
merkez → …**. Bir kadastro işinde her sınır bir röperden ölçüldüğü için düğüm en
üsttedir.

Listedeki her değişiklik `MOD yakalama_modları=<maske>` komutunu gönderir; yani
betikten de aynısını yaparsınız.

#### Adım — belli uzunluklarda çizmek

Bir çizgiyi 12 cm'nin katlarında bitirmek istiyorsanız **adım** kullanın:
`Shift+F3` listesinin altındaki **Adım…** satırından değeri girin, ya da

```text
MOD ad=adım deger=120
```

Açıkken imlecin bir önceki noktaya olan **uzaklığı** adımın katına yuvarlanır —
12, 24, 36 cm — ve yön serbest kalır. `0` kapatır.

Yön kilitleriyle **birlikte** çalışır ve sıralama şudur: dik mod ya da kutupsal
izleme **yönü** seçer, adım o yön üzerindeki **uzunluğu** seçer. Kutupsal ile
birlikte kullanınca kutupsal bir ızgara elde edersiniz: hem açı hem uzunluk
adımlı.

Adım, ızgaraya yakalama **değildir**. Izgara noktanın *nerede* olacağını sabitler
(zemine çakılı bir kafes); adım *ne kadar uzağa* gideceğini sabitler ve
başlangıcınız neredeyse oradan sayar.

Bir nesne yakalaması tuttuğunda adım devreye girmez: gerçek bir köşe, hesaplanmış
bir uzunluktan her zaman önceliklidir.

#### Sürüklerken okunan değerler

Kılavuz çizgi sürüklenirken üzerinde **uzunluk** ve **azimut** yazar:

```text
24,000 m  62,5000 grad
```

Açı varsayılan olarak **azimuttur**: kuzeyden saat yönünde ölçülür — aletten okuduğunuz
değer, matematik açısı değil. Birimi `açı_birimi` ayarına uyar ve varsayılanı **grad**'dır:
tam daire 400. `Seçenekler ▸ Genel ▸ Açı birimi` ile derece ya da radyana çevirirsiniz.
`MOD kural matematik` yazılmışsa açı doğudan saat yönünün tersine yazılır — komut satırına
yazdığınız `@mesafe<açı` ile aynı kural, tek ayardan (bkz.
[Oturum modları](../komutlar/mode.md)).

Durum çubuğundaki **DİNAMİK GİRDİ** anahtarı bu okumayı kapatır.

Tuvalde imlecin yanında çıkan bütün sayıların **boyu ayarlanabilir**: uzunluk, azimut,
koordinat göstergesi ve yakalama modunun adı. `Seçenekler ▸ Görünüm ve Tema ▸ İpucu
boyu` ya da:

```
TERCİH ipucu_boyu 18
```

Varsayılan **14 piksel**, aralık 9–28. Arayüz yazısından ayrı tutulur, çünkü bunlar
**el hareket ederken**, bir çizimin üstünde, çoğu zaman büyük bir ekrana uzaktan
bakılarak okunur; arayüz yazısı ise durup yakından okunmak için ölçülüdür.

#### Hassasiyet

Arama yarıçapı `yakalama_toleransı`, seçme kutusu `seçim_toleransı` tercihidir ve
ikisi de **ekran pikselidir**: nişan alan göz ekrana bakar, bu yüzden tolerans
yakınlaştırmayla birlikte değişir. İkisini de **Seçenekler ▸ Çizim ve Yakalama**
sayfasından değiştirirsiniz.

Modların tamamı ve bit maskesi: [Oturum modları](../komutlar/mode.md).

### Kılavuz ızgara

Arka plandaki kılavuz ızgara varsayılan olarak **uyarlanır**: yakınlaştırma düzeyine göre
1 / 2 / 5 x 10^n metre aralıklarından okunabilir olanı seçer. Her beşinci çizgi koyu
çizilir, böylece sayıları okumadan kaç aralık geçtiğinizi görebilirsiniz.

Izgaranın dördü de tercihtir ve [`TERCİH`](../komutlar/preference.md) ile değişir:

| Tercih | Ne yapar | Varsayılan |
|---|---|---|
| `ızgara` | Izgarayı açar/kapatır | `evet` |
| `ızgara_modu` | `uyarlanır` veya `sabit` | `uyarlanır` |
| `ızgara_adımı` | Sabit moddaki aralık, zeminde milimetre | `10000` (10 m) |
| `ana_çizgi` | Kaç ara çizgide bir koyu çizgi | `5` |

Kadastro paftasında metrekare defteriyle çakışan sabit bir ağ isterseniz:

```
TERCIH ızgara_modu sabit
TERCIH ızgara_adımı 10000
```

Ekrandaki aralık 2 pikselin altına düşerse ızgara o ölçekte çizilmez; aksi hâlde ekran
düz bir renge dönerdi.

Izgara ekranda görünür, **paftaya basılmaz**: bir görünüm yardımıdır, çizimin verisi
değildir. Bu yüzden çizim dosyasına da yazılmaz.

**F12** geliştirici bilgisini açar: etkin çizim arka ucu, çizilen nesne ve tepe noktası
sayısı, görünüm dışında kaldığı için elenen nesne sayısı, kare süresi. Bu bir geliştirici
katmanıdır, günlük kullanımda kapalıdır.

Bu sürümde harita `QPainter` ile çizilir; GPU çizimi Faz 1'de devreye girecek
(`CLAUDE.md` Article 8.1). Transkript açılışta bunu hatırlatır.

## Komut satırı

Tuvalin altında, 28 piksellik bir şerit. Solunda değişmeyen bir **`Komut:`** yazısı
vardır; sağında ne yazdığınız ve çalışan komutun ne beklediği görünür.

Her zaman açıktır — bir CAD kullanıcısının eli oraya kendiliğinden gider.
**Görünüm ▸ Paneller ▸ Komut Satırı** ile gizlenebilir.

- **Yukarı / Aşağı** — geçmiş
- **Tab** — tamamlama; adlar `Registry`'den gelir
- **Esc** — çalışan **komutu** iptal eder, yazıyı silmez

Bir komut nokta, sayı ya da yazı beklerken yazdığınız satır **o komutun cevabıdır**:
`10,20`, `@5<45`, `12,5` ya da bir yazı. İlk sözcük bir komut adıysa satır cevap
değil komuttur: `YAKINLAŞ KAPSAM` gibi saydam bir komut bekleyenin yanında çalışır,
diğer her komut bekleyeni önce sağ tık gibi bitirir ve sonra kendisi başlar.

Ayrıntı için bkz. [Komut satırı](../komutlar/komut-satiri.md).

## Sağ panel

312 piksel genişliğinde ve iki panel taşır. Her panelin başlığı 29 pikseldir ve
başlıkta sekmelerle birlikte üç işaret bulunur: **tutamak** (sürükle), **daralt**
ve **yüzdür**.

**Paneli taşımak.** Tutamaktan basılı tutup sürükleyin: imleç tutamağın üzerinde
açık ele döner. Panel yerleşik durumdayken bu onu başka bir kenara ya da başka bir
panelin yanına taşır; **yüzdür** ile pencereye dönüştüğünde de aynı tutamak o
pencereyi taşır. Başlıkta sekmelerin bittiği yerde boşluk varsa oradan da
sürükleyebilirsiniz; sekmelerin ve diğer işaretlerin üzeri sürükleme alanı değildir,
çünkü oradaki basış başka bir şey yapar.

### Araçlar

Üçüncü sekme: [işlem araçları](../islem/README.md), gruplar hâlinde bir ağaçta. Üstteki
kutu ada göre süzer. Bir satır seçilince altında aracın kartı açılır: açıklaması,
uygulandığı türler (çip olarak), **Kapsam** (Seçili · Görünüm · Proje), parametre
alanları, çıktı katmanı, gönderilecek **komut satırı** ve **Çalıştır**. Görünüm kapsamı
seçilince görünümün iki köşesi satıra `pencere=` olarak yazılır; kartta okuduğunuz
satırı komut satırına yazmak aynı işi yapar. Aynı araçlar **Analiz ▸ İşlem Araçları**
menüsünde de durur. `TERCİH araç_penceresi evet` ile kart panelin içinde değil kendi
penceresinde açılır. Bir **nokta** ya da **nesne** alanı tuvalden doldurulur: yanındaki
nişan düğmesi işaretçiyi seçim işaretçisine çevirir, tıkladığınız yer ya da nesne alana
yazılır ([Bileşenler](bilesenler.md)).

### Öznitelikler

Üstte **seçili nesne kartı**: tip ikonu, adı ve bir satır tanım — türü, kalıcı
kimliği, köşe sayısı ve katmanı: `ALAN · fid 4128 · 4 köşe · Kadastro Parselleri`.
Hiçbir şey seçili değilse belgenin kendi bilgileri yazılır — boşalan bir panel bozulmuş
görünür.

Altında katlanabilir gruplar ve `112 px | 1fr` ızgarasında satırlar: solda alan adı,
sağda değer. Değer tek aralıklı yazıyla yazılır, çünkü değer veridir ve veri tek
aralıklı okunur.

İki rozet vardır ve ikisi ayrı şey söyler:

- **HESAP** (turuncu) — bu sayı hesaplanmıştır, elle yazılmaz.
- **BOŞ** (soluk) — bu hücre henüz doldurulmamıştır.

Katmanlar panelinde bir katman seçtiğinizde bu panel o katmanın özelliklerini
gösterir: kabuğun **tek** özellik yüzeyi vardır, iki tane değil.

#### Çizimde bir nesne seçtiğinizde

Panel o nesneyle ilgili bildiği her şeyi dört grupta yazar. **NESNE** açık gelir,
diğerleri kapalı — çünkü ilk sorulan "bu nedir", "kaç metrekare" değildir. Bir
grubu açtığınızda **açık kalır**: sonraki parseli seçtiğinizde kapanmaz.

| Grup | Ne yazar |
|---|---|
| **NESNE** | `kimlik` (kalıcı, değişmez) · `tur` · `kose_sayisi` (hesap) · `halka_sayisi` (delikli ya da çok parçalı nesnede) · `katman` · `stil` · `gorunur` |
| **GEOMETRİ** | `kose` · `halka` · `cevre` ya da `uzunluk` · `alan` |
| **KAPSAM** | `saga_min/max` · `yukari_min/max` · `genislik` · `yukseklik` |
| **ÖZNİTELİKLER** | Belgede tanımlı her sütun ve nesnenin o sütundaki değeri |

Nesne yazı taşıyorsa bir de **METİN** grubu gelir: içerik ve yükseklik.

Tek nesne seçiliyken panele **sağ tıklayın** — klavyede **Menü** tuşu ya da
**Shift+F10** — nesnenin menüsü açılır: **Koordinatları dışa aktar…**
([Dışa Aktar](disa-aktarma.md) penceresi, `NOKTALAR … yon=yaz nesneler=…`),
**Koordinatları kopyala** (aynı satırlar panoya: `nesne.köşe;Y;X;katman`),
**Öznitelik tablosunu aç** ve **Katman özellikleri…**.

`tur` satırı **ne olduğunu** yazar, nasıl saklandığını değil: kapalı bir halka
`ALAN`, deliği varsa `ALAN (delikli)`, açık bir halka `ÇOKLUÇİZGİ` olur. İkisi de
belgede aynı türde durur (`core.polyline`), ama bir parselin karşısında
"ÇOKLUÇİZGİ" yazması doğru cevap değildir.

`alan` yalnızca kapalı bir şekilde çıkar ve **türün kendi hesabıdır**: bir daire
πr² bildirir, çizildiği çokgenin alanını değil. Bir yay hiçbir şey çevrelemediği
için alan yazmaz. [`ALANÖLÇ`](../komutlar/measure_area.md) ile aynı hesap.

`stil` satırı `katmandan` yazıyorsa nesnenin kendi stili yoktur, katmanınkiyle
çizilir — bu, "stili yok" demekten farklıdır ve nesneye uygulanan bir
[`STİL`](../komutlar/style.md) komutunun neden bir şey değiştirmediğini açıklar.

**Birden çok nesne seçtiyseniz** panel tek tek satır yazmaz; **SEÇİM** grubunda
adet, ortak katman (karışıksa `karışık`), toplam uzunluk ve toplam alan verilir.
Nesne nesne okumak için [öznitelik tablosunu](../veri/oznitelik-tablosu.md)
(**F6**) kullanın — 312 piksellik bir panel ikinci bir tablo değildir.

#### Değer düzenleme

Arkasında bir komut olan her satır **buradan düzenlenir**. Düzenlenebilir bir
değer normal mürekkeple, düzenlenemeyen bir adım soluk yazılır — tıklamadan önce
hangisinin değişebileceğini görürsünüz.

| Yol | Ne yapar |
|---|---|
| Çift tıklama | Satırın düzenleyicisini açar |
| **↑ ↓** | Satırlar arasında gezer |
| **Enter** / **F2** | Seçili satırı açar |
| **Space** | evet/hayır satırını çevirir |
| **Enter** (düzenlerken) | Değeri alır; imleç aynı satırda kalır |
| **Esc** (düzenlerken) | Vazgeçer, değeri değiştirmez |
| Başka bir yere tıklamak | **Enter ile aynı**: değer alınır |

Son satır bilerek öyledir. Bir değeri yazıp sonraki satıra tıklayan kişi işini
bitirmiştir; **Enter**'a basmadı diye yazdığını atmak, gözüyle gördüğü emeği çöpe
atmaktır.

**Enter yalnız onaylar.** Bir süre sonraki satırı da açıyordu — defter gibi: yaz,
Enter, yaz, Enter — ama bu geri alındı. Bir değeri onaylamak ile nereye gideceğine
karar vermek iki ayrı karardır; ikincisini ok tuşları ve fare zaten daha iyi söylüyor.
Sonraki satıra **↓** ile inip **Enter** ya da **F2** ile açarsınız.

Aynısı **öznitelik tablosu** için de geçerlidir: Enter hücreyi onaylar ve imleç
onayladığınız hücrede kalır.

#### Düzenleyici, satırın kendisidir

Düzenleyici, değerin çizildiği **dikdörtgenin tamamını** kaplar: kendi çerçevesi,
kendi kenar boşluğu, kendi köşe yuvarlaması yoktur. Ekranda değişen tek şey değerin
artık seçilebilir olmasıdır — panelin üstüne bir bileşen konmuş hissi vermez.

Ve **ne düzenlediğini bilir**. Sütunun bildirilmiş türü hangi düzenleyicinin
açılacağını belirler:

| Sütun türü | Açılan |
|---|---|
| `metin`, `kod` | Metin kutusu |
| `tam_sayi` | Yalnız rakam alan kutu |
| `uzunluk` | Aynısı, sonunda `mm` birimi |
| `ondalik` | Bildirilen basamak kadar ondalık alan kutu; hem `,` hem `.` kabul eder |
| `evet_hayir` | İki kelimelik segment: **evet** / **hayır** |
| `tarih` | `YYYY-AA-GG` kutusu ve yanında takvim |

Takvim, kutunun **hâlihazırda taşıdığı güne** açılır — her seferinde bugüne değil.
Ay okları `‹ ›`, yıl okları `«  »`; ok tuşlarıyla gün gün, **PgUp/PgDn** ile ay ay
gezilir, **Enter** seçer, **Delete** hücreyi boşaltır, **Esc** kapatır. Altta
**Bugün** ve **Temizle**.

Gün adları ve ay adları `QLocale(Türkçe)`'den gelir; programda hiçbir yerde Türkçe
metin bir tablodan ya da `<cctype>`'tan üretilmez.

Bunlar tek bir bileşen setidir ve program boyunca aynıdır: metin, sayı, ondalık,
evet/hayır, liste, çoklu seçim, tarih, aralık ve renk. Katman Özellikleri'ndeki
**Öznitelikler** sayfasının formu da aynı bileşenleri kullanır.

Önceden her satır aynı metin kutusunu açıyordu: bir tarih de, bir evet/hayır da
serbest metin olarak yazılıyor ve yanlış olduğu ancak komut reddettiğinde anlaşılıyordu.

| Satır | Gönderdiği komut |
|---|---|
| Öznitelik hücresi | `ÖZNİTELİK ad=<sütun> nesne=<kimlik> deger=<değer>` |
| `gorunur`, `kilitli` | `KATMAN ad=<katman> gorunur=evet` |
| `renk` | `KATMAN ad=<katman> renk=0xAARRGGBB` |
| `kalinlik` | `STİL katman=<katman> kalinlik=<µm>` |
| `grup` | `KATMAN ad=<katman> grup=<yol>` |
| `aktif_katman` | `KATMAN ad=<katman>` |
| `koordinat_sistemi` | `AYAR ad=koordinat_sistemi deger=<sistem>` |

Panelin belgeye giden **özel bir yolu yoktur**: her düzenleme bir komut satırı
kurar ve veri yoluna verir. Yani hücreden yaptığınız değişiklik komut günlüğünde
görünür, `GERİAL` ile tek adımda kalkar ve aynısını betikten de yaparsınız.

Nesne **kalıcı kimliğiyle** adlandırılır, bulunduğu sırayla değil: kimlik çizim
boyunca sabittir, sıra bir depolama ayrıntısıdır ve sonraki bir düzenlemede
değişebilir.

Değer kabul edilmezse — bir sayı sütununa harf yazmak gibi — komut reddeder,
hiçbir şey değişmez ve sebebi durum çubuğunda yazar.

### Geçmiş

Oturumda ne olduğunun metin dökümü. Komut günlüğünün kendisi için
**Pencere ▸ Komut Günlüğü**'nü açın; bkz. [Komut günlüğü](../mimari/gunluk.md).

## Katmanlar paneli

Sağ panelin altında, kendi 29 piksellik başlığıyla. Başlığın sağ ucunda panelin iki
işareti vardır: **＋** yeni katman adı sorar ve `KATMAN ad=…` çalıştırır; **süzgeç**
listenin üstünde bir arama kutusu açar — yazdıkça adı uymayan katmanlar gizlenir, Türkçe
büyük-küçük harf gözetilmez; aynı işaret kutuyu kapatır ve hepsini geri getirir. Onların
yanında dock işaretleri (taşıma, katlama, ayırma) durur.

Her satır tek bir satırdır ve sütun başlığı yoktur:

| Kısım | Ne yapar |
|---|---|
| 👁 **göz** | Katmanı gösterir/gizler |
| ▪ **renk** | Katmanın çizim rengi |
| **ad** | Aktif katman kalın yazılır |
| **sayı** | Katmandaki nesne sayısı |
| 🔒 **kilit** | Kilitli katman turuncu; açık olan soluk |

Seçili satır vurgu yıkaması ve sol kenarında 2 piksellik vurgu çizgisi taşır. Bu
kalıp programdaki **her** listede aynıdır; üzerine gelme ise düz bir gridir ve
seçime hiç benzemez — "farenin nerede olduğu" ile "neyin seçili olduğu" birbirine
karışmaz.

Altta bir sayaç: kaç katman var ve kaçı düzenlenebilir.

**Birden fazla katman seçebilirsiniz**: Ctrl ile tek tek, Shift ile aralık. Vurgulamak
bir düzenleme değildir — aktif katmanı değiştirmez.

Sağ tuş menüsü, tıkladığınız satırın komutlarını sunar: **Tümünü seç**, **Öznitelik
tablosu**, **Aktif katman yap**, **Özniteliklerden etiketle…**, **Görünüm** alt menüsü,
kilit, **Gruba taşı…** ve en altta **Katman Özellikleri…**.

**Görünüm** alt menüsü gösterme ve gizlemeye dairdir: göster, gizle, yalnız bunu göster,
tümünü göster, gösterimi ters çevir. Seçimin **tamamına** uygulanır — seçili olmayan bir
satıra sağ tıklarsanız yalnız o satıra — ve kaç katman seçili olduğu giriş başlığında
yazar. Onbir katman tek bir Ctrl+Z ile geri gelir.

Her giriş bir komutla gider: görünürlük [`KATMANGÖRÜNÜM`](../komutlar/layer_visibility.md),
geri kalanı [`KATMAN`](../komutlar/layer.md), `SEÇ` ve `ETİKET`. Panelin yapıp komut
satırının yapamadığı bir şey yoktur.

## Durum çubuğu

26 piksel, pencerenin tamamını kaplar — araç kutusunun ve sağ panelin de altından
geçer.

| Bölüm | Ne yazar |
|---|---|
| solda | İmlecin koordinatı: `Y <sağa değer>  X <yukarı değer>` |
| ortada | Yardımcı anahtarları: **IZGARA · YAKALAMA · DİK · POLAR · OSNAP · DİNAMİK GİRDİ · KALINLIK** |
| sağda | Veritabanı durumu ve çizim motoru |

Anahtarlara tıklamak o ayarı yazar — ve bir **komut** gönderir. Yani F8 ile DİK'e
tıklamak aynı şeydir ve ikisi de günlüğe aynı satırı yazar. Açık bir anahtar iki
şeyle işaretlenir: zemini açılır **ve** yazısı vurgu rengine döner; renk körü bir
kullanıcı için tek başına renk yeterli değildir.

| Anahtar | Gönderdiği komut | Ne yapar |
|---|---|---|
| **IZGARA** | `TERCİH core.izgara.gorunur` | Kılavuz ızgarayı çizer/gizler |
| **YAKALAMA** | `MOD ızgaraya_yakala` | Noktayı en yakın ızgara kesişimine oturtur (F9) |
| **DİK** | `MOD dik_mod` | İmleci yatay ve düşey eksene kilitler (F8) |
| **POLAR** | `MOD yakalama_modları` (kutupsal biti) | Önceki noktadan çıkan kutupsal ışınlara yakalar |
| **OSNAP** | `MOD yakalama_modları` | Nesne yakalamayı açar/kapatır; kapatınca maske hatırlanır, açınca geri gelir (F3). Sağ tık mod listesini açar |
| **DİNAMİK GİRDİ** | `TERCİH core.arayuz.dinamik_girdi` | İmlecin yanındaki koordinat ve uzunluk okumasını açar/kapatır |
| **KALINLIK** | `TERCİH çizgi_kalınlığı` | Çizgi kalınlıklarını paftadaki ölçüsüyle çizer; kapalıyken her çizgi tek piksel kıl çizgidir. Kalınlık nesnede ve çıktıda durur, yalnız ekran değişir |

Sağ tıklamak anahtarın ayarını açar: OSNAP ve POLAR'da yakalama modları listesi,
ötekilerde Ayarlar penceresinin ilgili sayfası.

## Tema

**Görünüm ▸ Koyu Tema** ile ya da **Ayarlar ▸ Görünüm ve Tema ▸ tema** ile
değiştirilir. Gece ve gündüz olmak üzere iki tema vardır ve ikisi de aynı yapıdan
üretilir: aynı jetonlar, farklı değerler. Bir panelin gündüz temasında yeri
değişmez, yalnızca rengi değişir.

Tema tercihi profilinizde saklanır ve program açıldığında geri gelir.

### Neden her platformda aynı görünüyor

Pencere çerçevesi, menü çubuğu ve düğmeler işletim sisteminden alınmaz; uygulama
kendisi çizer. Yazı tipleri (**IBM Plex Sans** ve **IBM Plex Mono**) programla
birlikte gelir, sistemde kurulu olmaları gerekmez. Sonuç: Windows, macOS ve
Linux'ta aynı pencere, aynı ölçüler, aynı renkler.

## Klavyeyle tam kullanım

KentOSCad faresiz tam çalışabilir olacak şekilde tasarlanır. Bugün klavyeyle
yapabilecekleriniz:

| Tuş | İşlev |
|---|---|
| **Ctrl+K** | Komut listesi — kategorilere ayrılmış, yazdıkça süzülen sayfa |
| **Ctrl+9** | Komut satırını açar veya kapatır |
| Komut satırına yazmak | Komut girmek |
| **Yukarı / Aşağı** | Komut geçmişi |
| **Esc** | Satırı temizler; satır boşsa komutu iptal eder |
| **Ctrl+Z** / **Ctrl+Shift+Z** | Geri al / yinele |
| **Ctrl+A** / **Ctrl+Shift+A** | Tümünü seç / seçimi temizle |
| **F3** / **F8** / **F10** / **F9** | Nesne yakalama / dik mod / yüzey normali / ızgaraya yakalama |
| **Ctrl+0** | Kapsama yakınlaş |
| **Ctrl++** / **Ctrl+-** | Yakınlaştır / uzaklaştır |
| **Ctrl+R** | Betik çalıştır |
| **F1** | [Komut listesi](../komutlar/help.md) — `Ctrl+K` ile aynı sayfa |
| **F6** | [Öznitelik tablosu](../veri/oznitelik-tablosu.md) |
| **F12** | Geliştirici bilgisi |
| **Alt** + menünün altçizgili harfi | Menüleri açar; altçizgiler yalnızca Alt basılıyken görünür |

Koordinatlar komut satırından girilebildiği için çizim de tamamen klavyeyle yapılabilir.

Tek harfli genel kısayol bilinçli olarak yoktur: komut satırına `Ç` yazarken tuşun
komuta kaçmaması gerekir. Kısaltmalar komut satırına **yazılır**, kısayol tuşu değildir.

Ekran okuyucu desteği (NVDA, VoiceOver, Orca) Faz 1'de tamamlanacak.

## Sırada ne var

- [Komut sistemi](../komutlar/README.md)
- [Komut satırı](../komutlar/komut-satiri.md)
- [Nesne seçme](../komutlar/select.md)
- [Oturum modları](../komutlar/mode.md)
- [Öznitelik tablosu](../veri/oznitelik-tablosu.md)
