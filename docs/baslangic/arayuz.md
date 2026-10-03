# Arayüz

PiriCAD penceresini yeni açan kullanıcı için; bu sayfayı bitirdiğinizde şeridin,
panellerin ve tuvalin ne işe yaradığını, nasıl taşınacağını ve fareyle klavyeyle neyin
nasıl yapılacağını bileceksiniz.

## Pencere düzeni

```text
┌ sistem başlık çubuğu (işletim sistemi çizer) ─────────────────────────── ─ □ ✕ ┐
├ şerit · sekme satırı 40 px ─────────────────────────────────────────────────────┤
│ [PiriCAD] Giriş  Çizim  Değiştir … Çıktı  Yazı   📄📂💾🖨▾ ↶↷ ⌃ [🔍 Komut ara…] │
├ şerit · paneller 91 px ─────────────────────────────────────────────────────────┤
│  ➤  │ ╱  ⌇  ◯▾  ◠▾ ▭▾ ⬠▾ │ ✥ Taşı  ↻ Döndür▾ ✂ Buda▾ ⌫ │ … │ [👁 ▪ 0      ▾] │ … │
│ Seç │         Çizim     ↘ │           Değiştir          │   │  Katmanlar     ↘ │   │
├─────────────────────────────────────────────────────────┬───────────────────────┤
│ ┌─────────────────────────────────────────────────────┐ │ Öznitelikler │ Geçmiş │
│ │                                                     │ │ SEÇİLİ NESNE          │
│ │ ┌cetvel 20 px──────────────────────────────────┐    │ │ ⬠ Parsel 1284 / 21    │
│ │ │                   tuval                      │    │ ├───────────────────────┤
│ │ │         ızgara · kuzey oku · ölçek           │    │ │ ⌄ KİMLİK              │
│ │ └──────────────────────────────────────────────┘    │ ├─ Katmanlar ───────────┤
│ ├ komut satırı 28 px ─────────────────────────────────┤ │ 👁 ▪ Kadastro  1 482🔒│
├─────────────────────────────────────────────────────────┴───────────────────────┤
│ ⊕ Y 458 214.362  X 4 512 908.771 │IZGARA│YAKALAMA│DİK│… 1 : 1 000 · EPSG:5254 … │
└ durum çubuğu 26 px ─────────────────────────────────────────────────────────────┘
```

Her bandın yüksekliği sabittir ve her platformda aynıdır: şerit 131 piksel (sekme satırı
40, paneller 91), komut satırı 28, durum çubuğu 26 piksel; sağ panel 312 piksel
genişliğindedir. Şeridin altında doğrudan tuval başlar. Bu ölçüler tasarım belgesinden gelir ve Windows,
macOS ve Linux'ta değişmez. **Pencere çerçevesi bu ölçülerin dışındadır:** çerçeveyi,
başlık çubuğunu ve pencere düğmelerini işletim sistemi çizer, dolayısıyla onlar her
masaüstünde o masaüstünün alışıldık görünümündedir. Başlık çubuğunda
`<belge adı> — PiriCAD <sürüm>` yazar.

## Şerit

Pencerenin üstündeki **şerit**, eski menü çubuğunun, araç çubuğunun ve sol araç
kutusunun üçünün birden yerini alır. Komutlar işe göre **sekmelere**, sekmelerin içinde
**panellere** ayrılmıştır; her panelin altında adı yazar. Düzen AutoCAD'in şerididir: bir
AutoCAD kullanıcısı `Çizgi`'yi, `Buda`'yı ve katman listesini aradığı yerde bulur.

Şeritteki her düğme bir komut çalıştırır; komut satırının, betiğin ve yapay zekânın
çalıştırdığı komutun aynısını (bkz. [Komut satırı](../komutlar/komut-satiri.md)).
Şeridin kendine ait bir yetkisi yoktur.

### Sekmeler

| Sekme | Paneller |
|---|---|
| **Giriş** | Seçim · Çizim · Değiştir · Açıklama · Katmanlar · Özellikler · Pano — her gün yapılan iş |
| **Çizim** | Seçim · Çizgi ve Eğri · Kapalı Şekil · Nokta ve Alım · Tarama · Blok |
| **Değiştir** | Seçim · Dönüştür · Çoğalt · Kes ve Uzat · Köşe ve Kenar · Birleştir ve Ayır · Alan İşlemleri · Sil ve Temizle |
| **Açıklama** | Seçim · Yazı · Ölçü · Etiket |
| **Kadastro** | Seçim · Parsel (İfraz, Alana Göre İfraz, Tevhit, Alanı Düzenle…, Alan Üret) · Yazım (Köşe Numarala, Uzunluk Yaz, Etiket) · Denetim (Topoloji Denetimi, Alan Ölç) |
| **Harita** | Seçim · Sorgu · Ölçüm · Jeodezi · Arazi · Veri · Dış Referans |
| **Analiz** | Seçim · Tablo · İşlem araçları · Denetim · Yapay zekâ |
| **Görünüm** | Seçim · Gezinme · Yardımcılar · Katmanlar · Pencereler · Tema |
| **Çıktı** | Seçim · Yazdır · Dosya |

**Bir panel bir iştir.** Çizim sekmesinde çizgiyle eğri bir panelde, kapalı şekiller
başka bir panelde, noktalar ve arazi alımı bir üçüncüsündedir; Değiştir sekmesinde nesnenin yerini
değiştirenler, çoğaltanlar, uçlarını kesip uzatanlar, köşelerini işleyenler, birleştirip
ayıranlar ve alanlarla işlem yapanlar ayrı panellerdedir. Bir aracı aradığınızda önce
hangi **işi** yaptığını düşünün; hangi sekmenin hangi paneline gireceği oradan çıkar.

**Giriş** sekmesi öteki sekmelerin kısa biçimidir: çizimin tamamı **Çizim**'de,
düzenlemenin tamamı **Değiştir**'dedir; Giriş en çok kullanılanları bir arada tutar.
Spline ve tarama bu yüzden **Çizim** sekmesindedir; **Stil Kopyala** Giriş'te
**Özellikler** panelinde, çizgi ve dolgu renk kutularının altındadır. Her sekme — nesne
seçince beliren düzenleyici sekmeleri de — 1440 piksel genişliğinde bir pencereye
kaydırmadan sığar.

**Her sekmenin ilk öğesi Seç aracıdır.** Hangi sekmede olursanız olun elinizdeki aracı
oradan bırakırsınız; okundaki listede **Alan Seç**, **Tümünü Seç** (**Ctrl+A**) ve
**Seçimi Temizle** (**Ctrl+Shift+A**) vardır.

Bir sekmenin sonundaki **Diğer komutlar** düğmesi o işin geri kalan komutlarını listeler.
Bu liste komut kaydından üretilir: yeni eklenen her komut kendiliğinden oraya düşer, bir
komutun yalnız adını yazarak erişilebildiği bir durum kalmaz.

Pencere bir sekmeyi göstermeye yetmeyecek kadar darsa sekmenin iki ucunda kaydırma okları
belirir; hiçbir düğme gizlenmez.

### Düğmeler

Şeritte **iki boy** düğme vardır ve boy bir anlam taşır; üçüncü bir boy yoktur:

| Boy | Ne için |
|---|---|
| **Büyük** — resim üstte, ad altta, panelin solunda | Panelin adını aldığı iş: Çizim panelinde Çizgi, Çoklu Çizgi, Alan, Daire, Yay; Dönüştür panelinde Taşı; Kes ve Uzat panelinde Buda ve Böl; Köşe ve Kenar panelinde Yuvarla |
| **Satır** — küçük resim ve ad, üçerli sütunlar | Panelin geri kalanı: Dikdörtgen, Elips, Nokta; Döndür, Ölçekle, Aynala; Kır, Uzunluk, Bölümle |

Her panelde büyük düğmeler solda, satırlar onların sağında ve soldan sağa, sütun sütun
okunur. Yalnız resimli, adsız düğme yoktur: her aracın adı yazar. İki yer bunun dışındadır
ve ikisi de bilinen bir kalıptır: **Katmanlar** panelinde katman kutusunun altındaki altı
simge (AutoCAD'deki katman şeridi gibi) ve sekme satırının sağındaki hızlı erişim simgeleri.

Simgeler renklidir ve renk her simgede aynı şeyi söyler: mavi komutun çizdiği ya da
değiştirdiği şekil, kırmızı kestiği ya da sildiği, turuncu yazdığı, sarı veri ve katman,
yeşil eklediği ya da birleştirdiği.

**Bir çizim aracı elinizde kalır.** Çizgi'ye bastıysanız çizgi çizersiniz; bir çizgiyi
**sağ tıkla** bitirdiğinizde araç bırakılmaz, sıradaki çizgi için hazır bekler. Alan,
Alan Ölç ve Ölçü de böyledir: **sol tuş başlatır, sağ tuş şekli bitirir, araç seçili
kalır**. Henüz hiç nokta vermediğiniz bir araçta sağ tık aracı bırakır.

**Sağ tık ve Esc bırakır.** Bir düzenleme sürerken (Yuvarla köşeyi, Taşı taban noktasını
sorarken) ya da hiçbir komut çalışmazken **sağ tık** veya **Esc**, süren düzenlemeyi **ve
seçimi** bırakır; el yeniden **Seç** aracındadır (Netcad'deki gibi). Aracı bırakmanın bir
yolu da **Seç**'e (veya başka bir araca) basmaktır. Çalışan aracın düğmesi basılı görünür.

**Bıraktığınız aracı geri almak.** Araç bırakıldıktan sonra boş komut satırında **Enter**
ya da **Boşluk** onu yöntemiyle yeniden başlatır. Netcad'deki gibi çizim alanında boş bir
yere tıklamanın da yinelemesini isterseniz `TERCİH son_komut tik` yazın; ayrıntısı
[Son komutu yinelemek](../komutlar/komut-satiri.md#son-komutu-yinelemek)'te.

### Aileler: bölünmüş düğmeler

Birbirinin yerine geçen araçlar tek bir **bölünmüş düğmede** durur: düğmenin yüzü en son
kullandığınız üyeyi çalıştırır, yanındaki **▾** ok ailenin bütününü listeler. Giriş'teki
**Daire** düğmesinin okunda merkezden, çapın iki ucundan, üç noktadan ve iki doğruya teğet
daire vardır; **Buda**'nın okunda çitle budama, tıklananı tutma, sınırı uzatarak budama ve
**Uzat**'ın üç biçimi vardır.

| Düğme | Ailesi |
|---|---|
| Daire | `DAİRE` · çapın iki ucu · üç nokta · iki doğruya teğet |
| Yay | `YAY` · üç nokta · başlangıç-merkez-açı · başlangıç-bitiş-yarıçap · teğet devam |
| Dikdörtgen | `DİKDÖRTGEN` · döndürülmüş |
| Çokgen | `ÇOKGEN` · dıştan · kenardan |
| Elips | `ELİPS` · eksenin iki ucu |
| Kesişim, Ara Nokta | `KESİŞİMNOKTA` · iki mesafeden · iki doğrudan / `ARANOKTA` · mesafeyle |
| Tarama | `TARAMA` · içine tıklayarak · seçilenler dışarıda |
| Kılavuz | yatay (`KILAVUZ yon=yatay`) · düşey (`yon=düşey`) · açılı (`yon=45g`) · kılavuzları listele (`KILAVUZ`) |
| Döndür, Aynala, Ölçekle | komut ve referansla / kopyalayarak biçimi |
| Hizala | `HİZALA` · ölçekleyerek |
| Dizi | `DİZİ` · kutupsal · yol boyunca |
| Buda | `BUDA` · çitle · tıklanan kalsın · sınırları uzatarak · `UZAT` · çitle · uzatarak |
| Böl | `BÖL` (kesme çizgisiyle) · noktalardan · kesişimlerden · eşit parçaya · baştan uzaklıkla |
| Yuvarla | `YUVARLA` · bütün köşeler · `PAH` · bütün köşeler |
| Ölç, Alan Ölç | `ÖLÇ` · ilk nokta sabit · `PRİZMA` / `ALANÖLÇ` · köşelerden · içine tıklayarak |
| Ölçü | Hizalı · Doğrusal · Açı · Yay Uzunluğu · Yarıçap · Çap · Koordinat — yedi `ÖLÇÜ tur=` |
| Temizle | `TEMİZLE` (bulur) · `TEMİZLE islem=onar` |
| Panoya Kopyala | `PANOYAKOPYALA` · taban noktasıyla |
| Kırp | `BLOKKIRP` · çokgenle · nesneyle |

Bir ailede **yalnız aynı işi yapanlar** durur: Daire'nin dört yolu, Buda ile Uzat gibi her
CAD'in tek araç saydığı çift, Yuvarla ile Pah. Halka bir alan çizmenin yolu değildir, ayrı
bir komuttur; bu yüzden kendi düğmesi vardır.

Listedeki bir inşa yöntemi — üç noktadan daire, teğet devam eden yay — komut satırında
`yontem=` ile yazılır, ama yazmak zorunda değilsiniz: ailede kendi satırı vardır ve
düğmenin ipucu tam olarak ne gönderdiğini (`YAY yontem=3n`) yazar. **Araç, yöntemiyle
birlikte elde kalır**: üç noktalı daireyi bitirince araç yine üç noktalı daire için bekler.

**Kılavuz, tıklamanın çizeceği nesnedir.** Her çizim yönteminde imleci gezdirirken görünen
kesikli şekil belgeye yazılacak nesnenin kendisidir: aynı hesapla çizilir.

### Canlı kutular: katman ve renk

Giriş sekmesinin **Katmanlar** ve **Özellikler** panelleri çizimi okur; AutoCAD'deki gibi
çalışırlar:

- **Katman listesi** — seçim yokken yeni nesnelerin çizileceği **etkin katmanı** gösterir;
  listeden başka bir katman seçmek onu etkin yapar ([`KATMAN ad=…`](../komutlar/layer.md)).
  Seçim varken **seçilen nesnelerin katmanını** gösterir (farklı katmanlardalarsa
  "farklı katmanlar" yazar) ve listeden bir katman seçmek nesneleri oraya taşır
  ([`KATMANAT`](../komutlar/set_layer.md)). Her satırda katmanın rengi, gizliyse kapalı göz,
  kilitliyse kilit işareti vardır.
- Listenin altındaki simgeler: **Etkin Yap** (seçili nesnenin katmanı etkin olur), **Etkin
  Katmana Taşı**, **Gizle**, **Yalnız Bu**, **Tümünü Göster**, **Kilitle / Aç**.
  **Görünüm ▸ Katmanlar** panelinde **Gösterimi Ters Çevir** ve **Stil Tasarımcısı** de
  vardır; yeni katmanı Katmanlar panelinin başlığındaki **+** açar.
- **Çizgi** ve **Dolgu** kutuları seçili nesnenin — seçim yoksa etkin katmanın — renklerini
  gösterir; nesne kendi rengini taşımıyorsa **Katmandan** yazar. Kutuyu açmak
  [`RENK`](../komutlar/colour.md) komutunun renklerini, başka bir renk seçmeyi, katmanın
  rengine dönmeyi ve dolgu için **Dolgu yok**'u sunar.

**Açıklama** sekmesindeki **Yükseklik** ve **Stil** kutuları yeni bir yazının yüksekliğini
ve yeni bir ölçünün stilini ([`AYAR metin_yüksekliği`, `AYAR ölçü_stili`](../komutlar/setting.md)),
**Çıktı** sekmesindeki **Ölçek** kutusu pafta ölçeğini (`AYAR plan_ölçeği`) okur ve yazar.
Bir kutuya değer yazıp **Enter**'a basmak da listeden seçmek de aynı komutu çalıştırır.

### Galeriler

**Çizim ▸ Tarama** panelindeki galeri tarama desenlerini **göründükleri gibi** gösterir:
her kutu desen kataloğundaki çizgi ailelerinden çizilir. Bir desene basmak
[`TARAMA`](../komutlar/hatch.md) komutunu o desenle başlatır ve sınırı sorar; galerinin
sağındaki okla bütün desenler açılır.

### Panel başlatıcıları (↘)

Bazı panellerin adının sağ alt köşesinde küçük bir **↘** vardır: panelin gündelik kısmını
gösterdiği şeyin tam penceresini açar.

| Panel | ↘ açar |
|---|---|
| Giriş ▸ Çizim | Seçenekler ▸ Çizim ve Yakalama |
| Giriş ▸ Açıklama, Açıklama ▸ Ölçü | Seçenekler ▸ Plot ve Çıktı (ölçü stili, pafta ölçeği) |
| Giriş ▸ Katmanlar | Katmanlar paneli |
| Giriş ▸ Özellikler | Stil Tasarımcısı |
| Harita ▸ Jeodezi | Seçenekler ▸ Koordinat Sistemleri |
| Analiz ▸ Yapay zekâ | Seçenekler ▸ Yapay Zeka Modelleri |
| Görünüm ▸ Yardımcılar | Yakalama modları |
| Görünüm ▸ Tema | Seçenekler ▸ Görünüm ve Tema |
| Çıktı ▸ Yazdır | Seçenekler ▸ Plot ve Çıktı (yazdırma profilleri) |
| Alan ▸ Ölç ve Yaz | Araçlar panelinde numaralama ve uzunluk yazma ayarları |

### Düzenleyici sekmeleri

Bir nesne seçtiğinizde onu düzenlemeye yarayan sekme sekme satırının sonunda belirir;
üstünde ince **mavi bir şerit** taşır — mavi bu programda seçim demektir. Seçim boşalınca
sekme de kaybolur.

| Sekme | Ne seçilince | İçinde |
|---|---|---|
| **Yazı** | yazı | **Düzenle**: Yazıyı Düzenle, Stil Kopyala · **Biçim**: Yükseklik ve Aralık kutuları · **Hizalama**: dokuz hizalama (3 × 3) · **Bağ**: Bağla, Bağı Çöz |
| **Ölçü** | ölçü | **Düzenle**: Ölçüyü Düzenle, Stile Döndür, Pafta Ölçeğine Uyarla · **Stil ve Değer**: Stil, Ondalık, Birim kutuları · **Devam**: Zincir Ölçü, Baz Ölçü |
| **Tarama** | tarama | **Desen**: desen galerisi · **Özellikler**: Açı, Ölçek, Çapraz · **Adalar**: Normal, Yalnız dış, Adasız · **Düzenle**: Taramayı Düzenle, Alan Ölç, Nesne Bilgisi |
| **Alan** | kapalı alan (parsel), kenarı yaylı olsa da | **Köşe ve Kenar**: Yuvarla (Pah ile), Köşe Taşı, Köşe Ekle, Köşe Sil, Kenar Türü, Çizgi Düzenle · **Parsel**: İfraz, Böl, Alana Göre İfraz, Tevhit, Alanı Düzenle… · **Alan İşlemleri**: Birleşim, Kesişim, Fark, Simetrik Fark · **Dönüştür**: Ofset, Tampon…, Tarama, Patlat · **Ölç ve Yaz**: Alan Ölç, Nesne Bilgisi, Topoloji Denetimi, Köşe Numarala, Uzunluk Yaz |
| **Çizgi** | açık çizgi ya da çoklu çizgi | **Kes ve Uzat**: Buda (Uzat ile), Böl, Kır, Uzunluk, Bölümle · **Köşe ve Kenar**: Yuvarla (Pah ile), Köşe Taşı, Köşe Ekle, Köşe Sil, Kenar Türü, Çizgi Düzenle · **Dönüştür**: Ofset, Alana Çevir, Uç Uca Ekle, Tampon…, Patlat · **Ölç ve Yaz**: Nesne Bilgisi, Uzunluk Yaz, Köşe Numarala |
| **Eğri** | daire, yay, elips ya da spline | **Kes ve Uzat**: Buda (Uzat ile), Böl, Kır, Uzunluk, Bölümle · **Köşe ve Birleştir**: Yuvarla (Pah ile), Uç Uca Ekle · **Alan İşlemleri**: Birleşim, Kesişim, Fark, Simetrik Fark · **Dönüştür**: Ofset, Tampon…, Tarama · **Ölç**: Alan Ölç, Nesne Bilgisi |
| **Blok** | blok | **Blok**: Bloğu Düzenle, Taban Noktası, Patlat, Nesne Bilgisi · **Kırpma**: Kırp (çokgenle, nesneyle), Kırpma Sınırını Çiz, Kırpmayı Kaldır ([BLOKKIRP](../komutlar/block_clip.md)) |

**Her düzenleyici sekme aynı iskelettedir:** Seç · **Nesne** · seçilen türe özgü paneller ·
Kapat. **Nesne** paneli — Taşı, Kopyala, Döndür, Ölçekle, Aynala ve Sil — her sekmede aynı
yerde, ikincidir: bir parsel, bir yazı ya da bir blok biçimlendirilmekten çok taşınır ve
silinir; sekme öne geldiğinde bunun için Giriş'e dönmeniz gerekmez.

**Bir sekmede yalnız seçilen türün işlediği araçlar bulunur.** Her komut hangi nesnelerde
çalıştığını bildirir ([Soluk araçlar](#soluk-araçlar)); sekme o komutlardan kurulur ve
seçimle ilgisi olmayanı taşımaz. **Koordinat Oku** ve **Bul ve Değiştir…** nesne seçmeden
çalışır, bu yüzden Harita ve Açıklama sekmelerindedir; **Sınır Bul** tıklanan bir bölgeyi
arar, **Blok Ekle** başka bir blok yerleştirir — ikisi de Çizim sekmesindedir.

Bu sekmedeki her şey **seçili nesnelerde** çalışır: Yazı sekmesinde yüksekliği 3,50 m
seçmek seçili yazılarda `YAZIDÜZENLE yukseklik=3500` çalıştırır. Kutular seçilen ilk
nesnenin değerini gösterir. Seçim **yalnız bir türdense** o türün sekmesi kendiliğinden
**öne gelir**: bir parsel seçince **Alan**, bir çizgi seçince **Çizgi**, bir daire seçince
**Eğri** açılır. Seçim birden çok türü karıştırıyorsa her türün sekmesi belirir ama
hiçbiri öne gelmez. Seçim boşalınca önceki sekmeye dönülür. Bir komut sizden bir şey
isterken (TAŞI'nın nesneleri gibi) düzenleyici sekmeleri görünmez; her birinin en
sağındaki **Seçimi Bırak** seçimi boşaltır.

### Soru sürerken: Seçim sekmesi

Bir komut sizden **nesne** isterken — TAŞI'nın, SİL'in, KOPYALA'nın sorusu — sekme
satırının sonunda **Seçim** sekmesi belirir ve soru cevaplanınca kaybolur. Sekme
**öne gelmez**: bulunduğunuz sekmede kalırsınız, gerektiğinde tıklarsınız.

| Panel | Düğmeler |
|---|---|
| **Seçim Kipi** | Pencere, Kesen, Çokgen, Çokgen Kesen, Çit, Daire, Dışında, İçeren, Geçen |
| **Küme** | Tümü, Önceki, Son, Temizle, Tersine Çevir |
| **Süzgeç** | **Tür** kutusu: yalnız bu türdeki nesneler |
| **Bitir** | **Seçimi Ver** — Enter ve sağ tıkla aynı |

**Her düğme bir satır yazar.** Tıklama istemeyen bir kip — Tümü, Önceki, Son — hemen
çalışır. Tıklama isteyen bir kip satırı **komut satırına başlatır** ve tuvalde yaptığınız
tıklamalar koordinat olarak satıra yazılır: Pencere iki köşeden, İçeren tek tıklamadan
sonra kendi çalışır; Çokgen ve Çit'i Enter bitirir. Sekmenin bulduğu, seçmekte olduğunuz
nesnelere **eklenir** (`islem=EKLE`), Tür kutusu seçiliyse satıra `tur=` da yazılır:

```text
SEÇ ÇİT islem=EKLE 485310.000,4310250.000 485330.000,4310190.000
```

Satırı kendiniz yazsaydınız olacak olan neyse o olur; komut günlüğü de aynı satırı
tutar. Kiplerin ne aldığı [SEÇ](../komutlar/select.md) sayfasındadır. Esc satırı ve
tıklama beklemeyi bırakır. Tıkladığınız noktalar tuvalde kesik bir izle, imlece kadar
çizilir.

### Soru sürerken: Nokta Girişi sekmesi

Bir komut sizden **nokta** isterken — ÇİZGİ'nin, DAİRE'nin, TAŞI'nın taban noktası —
**Nokta Girişi** sekmesi belirir (Netcad'in Nokta Seçim Araçları ve Koordinat Hesap
Makinası). Seçim sekmesi gibi öne gelmez ve soru bitince kaybolur.

| Panel | Düğmeler |
|---|---|
| **Yakalama** | Her yakalama modu bir anahtardır: uç nokta, orta nokta, merkez, kesişim… `MOD yakalama_modları` yazar, basılı hâli ayardan okunur |
| **Hesap** | Her [nokta fonksiyonu](../komutlar/komut-satiri.md#nokta-fonksiyonları) bir düğmedir: Son Nokta, Numaralı Nokta, Orta Nokta, Göreli, Dik Ayak, Semt ve Kenar, Kesişim, Ara Nokta, Uzantı, X ve Y, Boyunca |
| **Katman** | **Katmanı nesneden al**: basıp bir nesneye tıklayın, komutun çizdikleri o nesnenin katmanına gider; etkin katman değişmez. Kendi katmanına çizmeyen bir komutta soluktur |
| **Satır** | **Gönder** (Enter) · **Vazgeç** (Esc) |

**Hesap düğmesi fonksiyonu satıra başlatır**, tıkladığınız noktalar içine yazılır, sayıları
siz yazarsınız: **Dik Ayak**'a basıp taban çizgisinin iki ucuna tıklayın, `,30,5` yazın ve
Enter'a ya da **Gönder**'e basın — açık parantez kendiliğinden kapanır:

```text
dik(485300.000,4310200.000,485380.000,4310200.000,30,5)
```

**Boyunca**'da `nesne(` yazıldıktan sonraki tıklama noktayı değil, **tıkladığınız nesnenin
kimliğini** yazar. Yakalama anahtarları ve **F3** çalışan komutu bölmez: `MOD` şeffaftır.
**Katmanı nesneden al** komut satırına `katman="YOL"` yazmakla aynıdır; ikisi de
[Çizimin katmanı](../komutlar/komut-satiri.md#çizimin-katmanı-katman)'nda anlatılır.

### Soluk araçlar

Bir araç seçtiğiniz nesnelerde işe yaramıyorsa **soluk** görünür ve basılmaz. Bir yazı
seçiliyken **Yuvarla**, **Pah**, **Ofset** ve **Patlat** soluktur, çünkü bir yazının
yuvarlanacak köşesi ya da paraleli yoktur; **Taşı**, **Kopyala** ve **Sil** açıktır. Bir
çizgi seçiliyken **İfraz** soluktur, çünkü ifraz bir alanı böler. Soluk bir düğmenin
üstünde beklerseniz ipucu aracın nerede çalıştığını yazar:

```text
Seçimde bu komutun işlemediği nesne var: YUVARLA yalnız çizgi, alan, eğri üzerinde çalışır.
```

Kural komut kaydından gelir: her komut hangi nesnelerde çalıştığını bildirir ve
[komut başvurusu](../komutlar/referans.md) bunu her komutun altında **Uygulandığı
nesneler** satırında yazar. Komut satırından yazılan bir komut da aynı nesneleri kabul
eder; başka bir nesne verilirse nedenini söyleyerek reddeder.

- **Komutlar** seçimin **tamamına** uygulanır. Seçimde işlemedikleri tek bir nesne varsa
  soluktur: bir parsel ve bir yazı birlikte seçiliyken Yuvarla basılmaz.
- **Araçlar paneli işlemleri** (Köşe Numarala, Uzunluk Yaz, Tampon…) uygun olmayan
  nesneleri atlar ve kaçını atladığını söyler; bu yüzden seçimde uygun **bir** nesne
  varsa açıktır.
- **Seçim yoksa** her araç açıktır: araç nesnesini size sorar.
- **Buda** ve **Uzat** hiçbir seçimde soluklaşmaz: onlar için seçim **kesme sınırıdır**.
  Bir parseli seçip Buda'ya basarsanız parsel sınır olur ve tıkladığınız çizgi parçaları
  parselin kenarına kadar budanır.
- Seçimi kullanmayan yöntemler de soluklaşmaz: **Alan Ölç — köşelerden** köşelere
  tıklatır, **Bloğu Kaydet** açık düzenlemenin nesnelerini kaydeder.
- Bir ailenin **yöntemleri** kendi nesnelerini ayrıca bildirebilir. **Böl** bir alanı
  yalnız kesme çizgisiyle böler; **noktalardan**, **kesişimlerden**, **eşit parçaya** ve
  **baştan uzaklıkla** bir kenar boyunca yürür ve yalnız çizgi ile eğride çalışır. Bir
  parsel seçiliyken bu dört satır soluktur, Böl'ün kendisi açıktır. Komut başvurusu bunu
  **Uygulandığı nesneler** satırında yöntem yöntem yazar.

Bir aile düğmesinin yüzündeki araç soluk, okundaki bir üyesi açıksa düğme basılabilir
kalır; basınca üyelerin listesi açılır.

### İpuçları

İmleci bir düğmenin üstünde bekletince ipucu açılır: aracın adı ve kısayolu, ne yaptığı,
komut satırında nasıl yazıldığı (`DAİRE · CIRCLE · DR` gibi, bir yöntem ise tam satır) ve
bir aile düğmesiyse okundaki öteki üyeler. İpucu komut kaydından üretilir; komut
satırına yazacağınız ad her zaman oradaki addır.

### Şeridi daraltmak

Bir sekmeye **çift tıklamak** ya da sekme satırının sağındaki **⌃** düğmesi şeridi sekme
satırına indirir; tuval o kadar büyür. Daraltılmış şeritte bir sekmeye tıklamak panelleri
geçici olarak açar. Aynı düğme (⌄) ya da bir sekmeye yeniden çift tıklamak şeridi geri açar.

### Klavyeyle ve ekran okuyucuyla

| Tuş | Ne yapar |
|---|---|
| **Tab** / **Shift+Tab** | Şeritteki düğmeler arasında gezer |
| **Boşluk** ya da **Enter** | Odaktaki düğmeye basar — farenin yaptığının aynısı |
| **↓** ya da **F4** | Bölünmüş bir düğmenin listesini açar; liste ok tuşlarıyla gezilir, **Enter** seçer, **Esc** kapatır |
| **Ctrl+K** | Komut listesini açar; listedeyken **Ctrl+D** komutu favorilere ekler ya da çıkarır |

Fareyle bir düğmeye basmak klavye odağını tuvalden almaz, çünkü komut çalışırken Esc'in ve
ok tuşlarının yeri tuvaldir. Her düğmenin kısayolu — **Ctrl+H** (macOS'ta
**Cmd+Option+F**) Bul ve Değiştir, **F3** Nesne Yakalama gibi — hangi sekme açık olursa
olsun çalışır.

Ekran okuyucu (VoiceOver, NVDA, Orca) her düğmeyi **adıyla ve ipucuyla** okur, çalışan
aracı "işaretli" diye söyler ve bölünmüş düğmede iki eylem sunar: **Bas** yüzdeki aracı
çalıştırır, **Menüyü göster** listeyi açar. Erişilebilirlik katmanından basmak — VoiceOver'da
**Ctrl+Option+Boşluk** — aracı gerçekten **çalıştırır**.

## PiriCAD menüsü

Sekme satırının en solundaki **PiriCAD** düğmesi uygulama menüsünü açar. Solda dosyayla
yapılan işler büyük satırlar halinde, her birinin altında ne yaptığı yazar: **Yeni**
(**Ctrl+N**), **Aç…** (**Ctrl+O**), **Kaydet** (**Ctrl+S**), **Farklı Kaydet…**
(**Ctrl+Shift+S**), **İçe Aktar…**, **Dışa Aktar…**, **Yazdır** (**Ctrl+P**), **Çıktı
Yerleşimleri**, **Proje Ayarları…**, **Veritabanı…** (**Ctrl+Shift+D**), **Betik
Çalıştır…** (**Ctrl+R**) ve şeritte yeri olmayan komutlar için **Diğer Komutlar**.

Sağ bölme **son kullanılan belgeleri** listeler: dosyanın adı, klasörü ve ne zaman
açıldığı ya da kaydedildiği. Birine tıklamak onu [`AÇ`](../komutlar/open.md) ile açar ve
kapsama yakınlaşır. Listede kaç belge tutulacağını
[`TERCİH son_dosya_sayısı`](../komutlar/preference.md) belirler (varsayılan 10); liste bu
bilgisayara aittir, çizimle birlikte gitmez.

İmleç ya da klavye **Yazdır**'ın üstündeyken sağ bölme yazdırma profillerini ve çizimin
yerleşimlerini, **Çıktı Yerleşimleri**'nin üstündeyken yerleşimleri, şablonları ve
yöneticiyi gösterir. En üstteki **Komut ara…** kutusu komut listesini açar. En altta
**Komut Listesi** (**F1**), **Hakkında**, **Seçenekler…** (**Ctrl+,**; macOS'ta **Cmd+,**) ve
**Çıkış** durur.

Menü klavyeyle de gezilir: **↑ ↓** satırlar arasında, **→** sağ bölmeye, **←** geri,
**Enter** seçer, **Esc** kapatır.

## Hızlı erişim satırı ve sağ köşe

Sekme satırının sağında her sekmeden erişilen düğmeler durur: **Yeni**, **Aç**, **Kaydet**,
**Yazdır** — yazıcının hemen sağındaki **▾** ok çizimin çıktı yerleşimlerini, yeni yerleşimi,
yerleşim yöneticisini ve şablonları açar — sonra **Geri Al** (**Ctrl+Z**) ve **Yinele**
(**Ctrl+Shift+Z**). Onların sağında şeridi daraltan **⌃**, komut arama kutusu ve kullanıcı
baş harfleri vardır.

Pafta ölçeği ve koordinat sistemi **durum çubuğunun** sağ ucunda okunur:
`1 : 1 000 · EPSG:5254 · ITRF96 / TM30` gibi. Çözümlenmemiş bir sistem "çözümlenmedi" yazar;
bir mühendisin çizmeye başlamadan önce baktığı okuma budur.

### Komut listesi — `Ctrl+K`

Sekme satırının sağındaki **Komut ara…** kutucuğuna tıklayın, **Ctrl+K**'ya basın, **PiriCAD** menüsünün altındaki **Komut Listesi**'ni (`F1`) seçin
ya da komut satırına `YARDIM` yazın: dördü de aynı sayfayı açar. Üstte süzgeç, solda
kategori başlıkları altında bütün komutlar, sağda imlecin üzerinde olduğu komutun
aldığı parametreler.

| Bölüm | Ne gösterir |
|---|---|
| Süzgeç | Yazdıkça süzer ve sıralar; ad, kısaltma, başka programdaki adı, komut kimliği ve açıklama aranır |
| Sol liste | Üstte **Favoriler** ve **Son kullanılanlar** (varsa), altında Çizim · Düzenleme · Görünüm · Katman · Dosya · Sorgu · İşlem · Betik · Sistem başlıkları altında komut adı, tek satır açıklaması ve sağ kenarda kısaltmaları |
| Sağ bölme | Komutun kategorisi, kimliği, kabul ettiği bütün yazımlar, açıklaması ve parametreleri — her parametrenin tipi, gerekliliği, aralığı, birimi ve varsa sözcük listesi |
| Alt satır | Komut sayısı ve tuşlar |

Liste `Registry`'den üretilir; yani bugün var olan her komut oradadır ve yarın eklenen
komut da hiçbir liste güncellenmeden orada olur.

#### Favoriler ve son kullanılanlar

Bir şey yazmadan açtığınızda liste, kategorilerden önce **sizin** komutlarınızla başlar:

- **Favoriler** — yıldızladığınız komutlar, yıldızladığınız sırayla. Bir komutu yıldızlamak
  için imleci üzerine getirip **Ctrl+D**'ye basın (imleç süzgeçte kalır) ya da satırın solundaki
  yıldıza tıklayın. Aynı tuş yıldızı kaldırır. İmleç yıldızladığınız komutun üzerinde kalır,
  böylece art arda on komutu yıldızlamak listenin başına dönmek demek değildir.
- **Son kullanılanlar** — en son kendi elinizle başlattığınız en çok sekiz komut, en yenisi
  üstte; yıldızlıysa Favoriler'de olduğundan burada tekrar yazılmaz. Komut satırına yazdığınız,
  şerit düğmesine bastığınız ve bu listeden seçtiğiniz komutlar sayılır. **Sayılmayanlar:**
  görünüm değiştirenler (`YAKINLAŞ`, `KAYDIR`), `GERİAL` ve `YİNELE`, bir **betiğin** ya da
  yapay zekânın çalıştırdığı komutlar ve programın kendi kurulum işleri — kısa bir listenin
  işi, sizin ne aradığınızı göstermektir.

Altındaki kategori listesi eskisi gibi eksiksizdir: bir komut hem Favoriler'de hem kendi
kategorisinde görünür. Bir şey yazmaya başlayınca bu iki bölüm kalkar ve liste, aşağıdaki
gibi, en iyi eşleşen üstte olacak biçimde sıralanır.

#### Komutun adını bilmiyorsanız: cümle yazın

Süzgeç yalnız komut adı aramaz; **ne yapmak istediğinizi** yazabilirsiniz. Türkçe ekler,
fiil ve edat ayıklanır, kalan sözcükler komutların adında, başka programdaki adında,
açıklamasında ve parametre adlarında aranır:

| Yazdığınız | Üstte çıkan |
|---|---|
| `çizgiyi paralel kaydır` | OFSET |
| `köşeyi yuvarla` | YUVARLA |
| `iki çizgiyi birleştir` | BİRLEŞTİR |
| `nesneyi çoğalt` | KOPYALA, DİZİ |
| `alanı hesapla` | ALANÖLÇ |
| `parseli ifraz et` | İFRAZ |

Kurallar:

- **Önce tam eşleşme.** Yazdığınız metin bir komut adının, kısaltmasının ya da başka
  programdaki adının içindeyse sıralama eskisi gibidir: `kaydır` hâlâ önce KAYDIR'ı, hemen
  altında TAŞI'yı (Netcad'in adıyla) gösterir. Cümle yalnız hiçbir komut adında geçmediği
  zaman sözcüklerine ayrılır.
- **Ad, nesneden önce gelir.** `iki çizgiyi birleştir` içinde BİRLEŞTİR adın kendisidir;
  `çizgiyi` ise ek almış, yani üzerinde çalışılan şeydir. Komutlar yalın emir kipinde
  (`birleştir`, `yuvarla`) aranır, bu yüzden adın kendisiyle eşleşen sözcük ek almış
  sözcükten önce sayılır.
- **Yarısı yeter.** Sözcüklerin en az yarısı bir komutta karşılık bulmalıdır; `çiz` gibi hiçbir
  komutun adı olmayan fiiller sonucu bozmaz. Hiçbir komut karşılık vermiyorsa liste boştur ve
  alt satır bunu söyler; tahmin yürütülmez. (Örneğin `arazi kesiti çıkar`: kesit aracı henüz
  yok.)
- Yazım Türkçe katlanır: `cizgiyi` ile `çizgiyi` aynıdır.

Eşleşme bir komutun açıklamasına dayanıyorsa komutun kendisini değil, açıklamasını
düzeltmek gerekir: arama, komutun kayıtta bildirdiğinden başkasını bilmez.

İki liste de **bu bilgisayara** aittir ve çizime yazılmaz: çizimi bir başka büroya
gönderdiğinizde sizin sık kullandığınız araçlar onunla gitmez.

Arama Türkçe katlamayla çalışır: `cizgi` yazınca `ÇİZGİ`, `olcek` yazınca `ÖLÇEK`
bulunur. `↑` `↓` gezinir, `Enter` seçili komutu komut satırına yazıp imleci sonuna
koyar — çünkü argümanı olan bir komutun argümanı yazılmalıdır — `Esc` kapatır.

Bir şey yazınca kategori başlıkları kalkar ve liste **en iyi eşleşen üstte** olacak
biçimde sıralanır:

| Sıra | Eşleşme | Örnek |
|---|---|---|
| 1 | Yazılan, komutun adlarından biri | `kaydır` → `KAYDIR` |
| 2 | Yazılan, komutun başka bir programdaki adı | `kaydır` → `TAŞI` (Netcad'de Kaydır) |
| 3 | Adlardan biri yazılanla başlıyor | `dikd` → `DİKDÖRTGEN` |
| 4 | Başka programdaki adı yazılanla başlıyor | `ku` → `DİKDÖRTGEN` (Netcad'de Kutu) |
| 5 | Yazılan bir adın ya da komut kimliğinin içinde geçiyor | `ortge` → `DİKDÖRTGEN` |
| 6 | Yazılan başka programdaki adın içinde geçiyor | |
| 7 | Yazılan komutun başlığında ya da açıklamasında geçiyor | `tutulan` → `KAYDIR` |

Aynı sıradakiler kategori sırasıyla gelir.

**Netcad'den gelenler için.** Netcad'in adıyla aramak da bulur: `kutu` yazınca
`DİKDÖRTGEN`, `tabaka` yazınca `KATMAN`, `cetvel` yazınca `ÖLÇ` en üstte çıkar ve satırın
ikinci satırında `Netcad adı: KUTU` gibi hangi sözcüğün eşleştiği yazar. Bu adlar
**komut satırında komut başlatmaz**: bir komut soru sorarken yazılan `KUTU` o sorunun
yanıtıdır — bir yazı, bir katman adı — ve boş komut satırına yazılınca program
karşılığını söyler (`KUTU (Netcad) burada DİKDÖRTGEN komutudur.`). Netcad'deki adıyla
**yazılabilen** komutlar (`PARALEL`, `ÇİZDİR`, `LİMİTBUL`…) her komutun sayfasındaki
Adlar tablosundadır; aramada bulunan ama yazılamayanların tam listesi üretilmiş
[komut referansının](../komutlar/referans.md) sonunda, **Başka programlardaki adlar**
başlığı altındadır.

### Komutun istediği şeyi vermek

Çalışan bir komut ne istediğini komut satırının solunda yazar. İstediği şeye göre
klavyenin nerede olacağı değişir:

| Komut ne istiyor | Nasıl verilir |
|---|---|
| **Nokta** | Tuvale tıklayın; ya da koordinatı komut satırına yazın (`485320,4310220`, `@50,30`, `@100<45`) |
| **Nesne** | Tuvalde seçin, sonra **Enter** ya da **sağ tık** (hiçbir şey seçmeden sağ tık komutu bırakır) |
| **Ad** (blok, katman, desen) ya da **sayı** | Odak kendiliğinden komut satırına geçer ve yazılacak yer hazır olur; yazmaya hemen başlayabilirsiniz |
| **Mesafe** (pah mesafesi, yuvarlatma yarıçapı) | İstem "yazın ya da gösterin" der: sayıyı yazın **ya da** tuvale tıklayın — istemin başladığı noktadan tıklanan yere olan uzaklık cevaptır. İmleç hareket ettikçe sonuç tuvalde çizilir |

Bir **sayı** isteyen öteki istemlerde tuvale tıklamak bir cevap değildir: komut
"… bir sayı bekliyor; tıklamak yerine komut satırına yazın." der ve soruyu açık
tutar. (Tıklamanın sessizce sıfır sayıldığı eski davranış yoktur.)

Uzunluk soran bir istemde sayıyı **birimiyle** yazabilirsiniz: `12.5 m`, `1250 cm` ve
`12500mm` aynı cevaptır; sorunun parantez içinde söylediği birime (çoğunlukla metre) kendisi
çevrilir. Bir tek sayı soran istemde ondalık ayırıcı **virgül** de olabilir (`12,5`, `1.250,5`);
tuvale tıklayarak da cevaplanan mesafe istemlerinde tek başına yazılan `5,5` ise bir koordinattır,
orada `5.5` ya da `5,5 m` yazın. Uzunluk sormayan bir istemde (adet, açı, ölçek) birim yazarsanız
komut bunu söyler ve soruyu açık tutar.
Ayrıntı: [Birimli sayılar](../komutlar/komut-satiri.md#birimli-sayılar).

Nesneyle çalışan bir araca **hiçbir şey seçmeden** basmak da bir hata değildir:
araç hangi nesneleri istediğini sorar. Tek nesneyle çalışan bir araca birden çok
nesne seçiliyken basarsanız reddetmez; "2 nesne seçili; bu araç bir seferde 1
nesneyle çalışır" diyerek istediği nesneyi sorar.

**Tek nesne isteyen bir soru tek tıkla cevaplanır.** Kır, Uzunluk, Bölümle ve Stil
Kopyala'nın kaynak sorusu bir nesne ister; tıkladığınız nesne cevaptır, **Enter**'a
basmanız gerekmez. Birden çok nesne isteyen sorular (Taşı'nın nesneleri gibi) seçime
eklemeye devam eder ve **Enter** ya da sağ tıkla biter.

**Stil Kopyala** önce **kaynak** nesneyi, sonra stili alacak nesneleri sorar. Bir nesne
seçiliyken basarsanız seçili nesne kaynak olur ve yalnız hedefler sorulur.

Bir çizim aracının **ilk noktası** sorulurken **Enter**'a basmak (ya da komut satırını
boş göndermek) aracı hatasız bırakır; henüz çizilmiş bir şey olmadığı için geri alınacak
bir şey de kalmaz.

Cevabı belli bir kümeden olan istemler o kümeyi de gösterir: **Blok Ekle**
çizimdeki blokların adlarını, **Katman** ve **Etiket** katman adlarını,
**Katman Görünümü** alabileceği işlem sözcüklerini listeler. Listeden seçmek de
yazmak da olur.

Sözle cevap veren komutlar (Katmanları Listele, Görünüm Bilgisi, Seçim Bilgisi,
Sorgula) cevabı sağ panelin **Geçmiş** sekmesine yazar ve o sekmeyi kendiliğinden
öne getirir.

Değerini bir formda isteyen araçlar — şeritte adı `…` ile biten **Tampon…**, **Alanı
Düzenle…** gibi — **Araçlar** panelinde açılır ve klavye formun **ilk alanına** geçer,
içindeki değer seçili olur: yazdığınız onun yerine geçer. Her değerinin bir varsayılanı
olan işlemler — **Köşe Numarala**, **Uzunluk Yaz**, **Alan Üret**, **Bağla**, **Bağı
Çöz** — şeritten basınca **doğrudan çalışır**: seçim varsa seçime, yoksa sorduğu nesnelere,
varsayılan değerlerle. Değerleri değiştirmek için aynı işlemi **Araçlar** panelinden açın.

## Tek belge, sekmesiz

Tuvalin üstünde belge sekmesi yoktur: bir anda **tek çizim** açıktır ve adı pencerenin
başlık çubuğunda yazar (`ada-112.pcad — PiriCAD 0.1.0`). Sekme şeridi, tek sekmesiyle
tuvalden 30 piksel alıyordu; birden çok çizimi aynı anda açmak geldiğinde (Faz 2) geri
gelecek. Bugün [`YENİ`](../komutlar/new.md) açık çizimin yerine boş bir çizim koyar;
kaydedilmemiş değişikliğiniz varsa önce **Kaydet / Atla / Vazgeç** sorusu gelir.

## Harita alanı

Çizimin göründüğü yer.

| Etkileşim | Sonuç |
|---|---|
| Sol tık, komut nokta beklerken | Çalışan komuta bir nokta verir |
| Sol tık, komut nesne beklerken | Nesneyi seçime **ekler**; **Ctrl** ile çıkarır |
| Sol tık, komut yokken | İmlecin yakınındaki nesneyi seçer |
| Sol tık birden çok nesnenin üstüne | İlkini seçer; çerçeveli listeden istediğiniz satırı doğrudan seçebilir veya tür/katman/kimlikle arayabilirsiniz. **Boşluk** sıradakine, **Home/End** ilk/son adaya geçer; **Enter** tutar, **Esc** geri alır ([SEÇ](../komutlar/select.md#aynı-noktada-birden-çok-nesne-varsa)) |
| **Çift tık**, komut yokken | Nesneyi tek başına seçer ve düzenleyicisini açar: yazıda [`YAZIDÜZENLE`](../komutlar/edittext.md), ölçüde [`ÖLÇÜDÜZENLE`](../komutlar/dimension_edit.md) — ikisinde de metin, yazının üstündeki kutuda, şimdiki hâliyle seçili durur; **Enter** yazar, **Esc** vazgeçer —, taramada [`TARAMADÜZENLE`](../komutlar/hatch_edit.md); başka her nesnede **Öznitelikler** paneli öne gelir |
| Sol tuş basılı sürükle, komut yokken | Seçim kutusu çizer |
| **Shift** + tık/sürükle | Seçime ekler |
| **Ctrl** + tık/sürükle | Seçimden çıkarır |
| Sağ tık, komut yokken | **Seçimi bırakır** (tuvaldeki ölçü ve izleme işaretleriyle birlikte); el **Seç** aracındadır |
| Sağ tık, komut nesne beklerken | Seçilenleri komuta verir (Enter ile aynı); hiçbir şey seçilmediyse komutu bırakır |
| Sağ tık, nokta dizisi sürerken (Çizgi, Alan, Ölç… en az bir nokta verilmişken) | Şekli olduğu yerde **bitirir**; araç elde kalır. Henüz nokta yoksa aracı bırakır |
| Sağ tık, bir düzenleme sürerken (köşe, taban noktası, yarıçap istenirken) | **Düzenlemeyi ve seçimi bırakır**; el **Seç** aracındadır |
| Orta tuş basılı sürükle | Görünümü kaydırır |
| Fare tekerleği | İmlecin bulunduğu noktaya yakınlaştırır/uzaklaştırır |
| **Esc** | Çalışan komutu **ve seçimi** bırakır (süren bir dizi o ana kadar çizdiğini tutar); komut yoksa seçimi ve tuvaldeki işaretleri temizler. El **Seç** aracındadır. Bir köşe tutamağını sürüklerken sürüklemeyi bırakır |

İmleç bir **CAD nişanıdır**: ortası boş bırakılmış yatay ve dikey iki çizgi, ortasında
da **seçim kutusu** — bir tıklamanın neyi tutacağını gösteren kare. Karenin kenarı
`seçim_toleransı` tercihinin (**Seçenekler ▸ Çizim ve Yakalama**) iki katıdır; ne gösteriyorsa `SEÇ`
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

### Harita üzerindeki yardımcılar

Çizimin üzerinde, çizime ait olmayan dört şey durur. Dördü de **Seçenekler ▸ Görünüm ve
Tema** sayfasından kapatılabilir; hiçbiri dosyaya girmez.

| Yardımcı | Ne söyler | Ayarı |
|---|---|---|
| **Cetvel** | Tuvalin üstünde ve solunda, zemin ölçüsünü rakamla; yakınlaştıkça bölmeleri ayırt edecek kadar ondalıkla (`20,1  20,2`) | `cetvel_görünür`, `cetvel_kalınlığı`, `cetvel_birimi` |
| **Ölçek çubuğu** | Sol altta, o anki yakınlaştırmanın yuvarlak bir zemin uzunluğu karşılığını | `ölçek_çubuğu` |
| **Kuzey oku** | Sağ üstte, kuzeyin yönünü | `kuzey_oku` |
| **Koordinat göstergesi** | Sol altta, imlecin sağa (Y) ve yukarı (X) değerini, durum çubuğuyla aynı harflerle: `Y 485320,15   X 4310220,40` | `koordinat_göstergesi` |

Koordinat göstergesi, bir yakalama tuttuğunda **yakalanmış** noktayı yazar; tıklamanın
üreteceği koordinat odur, imlecin durduğu ham nokta değil.

Cetvelin ve ölçek çubuğunun rakamları 1-2-5 merdivenine oturur (1, 2, 5, 10, 20, 50 …):
aralıkları 137 metre olan bir cetvelden kimse mesafe okuyamaz.

Nişan imleci `imleç` tercihiyle üç hâlde olabilir — tuvali baştan başa geçen çizgiler
(`tam_ekran`), kısa bir artı (`kısa`, uzunluğu `imleç_boyu` ile) ya da hiç (`yok`).
`yok` seçilirse işletim sisteminin ok imleci geri gelir. Seçim kutusu her iki nişanda
da ortada durur; boyu `seçim_toleransı` tercihinden (**Seçenekler ▸ Çizim ve Yakalama**) gelir.

### Seçim

Hiçbir komut çalışmıyorken sol fare tuşu seçim yapar. **Soldan sağa** sürüklerseniz
kutuya **tamamen giren** nesneler seçilir ve çerçeve düz çizilir; **sağdan sola**
sürüklerseniz kutuya **değen** her nesne seçilir ve çerçeve kesik çizilir. Bu, CAD
dünyasının kırk yıllık ayrımıdır ve PiriCAD'de de aynıdır.

Seçili nesneler kalın ve renkli çizilir. [Kırpılmış](../komutlar/block_clip.md) bir blok
ya da dış referans seçiliyken kırpma sınırı da ince, kesikli çizilir — yalnız ekranda;
yazdırılmaz. Seçim çizimin verisi değildir: dosyaya yazılmaz, `GERİAL` ile geri alınmaz
ve komut günlüğüne belge değişikliği olarak düşmez.

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

**Her yardımcı kendi işaretini çizer.** Bir yakalama tuttuğunda imlecin altında o
moda ait bir şekil ve adı görünür: uç noktada kare, ortada üçgen, merkezde halka,
kesişimde çarpı, yüzey normalinde bir yüzey ve üzerinde duran **dik açı işareti**,
çeyrekte çeyrek yay, teğette eğriye yatan bir çizgi, izde küçük bir kutu ve
kuyrukları. Yedi mod bir zamanlar hiçbir şey çizmiyordu — yakalama tutuyor, nokta
kayıyor, işaret ise tutmadığını söylüyordu. Şimdi her modun işareti var ve bir
test bunu modların **hepsini dolaşarak** güvence altına alıyor, yani yeni bir mod
işaretsiz gönderilemiyor.
| **Ctrl** (basılı) | Köşegen kilidi: imleci öncekinden 45°'nin katlarına kilitler |
| **F9** | Izgaraya yakalamayı açar/kapatır |
| **Del** ya da **⌫** | Seçili nesneleri siler (`SİL`). Mac klavyesinde ⌦ tuşu çoğu zaman yoktur; ⌫ (Backspace) da siler |
| **⌫** (çizerken) | Çizgi, çoklu çizgi, alan ya da spline nokta beklerken yalnız **son noktayı** geri alır; hiçbir nesne silinmez |
| **Shift + sağ tık** | Bir komut nokta beklerken imleçteki noktayı **izleme için işaretler** ([`İZ`](../komutlar/tracking.md)) |

#### Hangi modlar açık

Durum çubuğundaki **OSNAP** çipine **sağ tıklayın** — ya da **Shift+F3** ile,
**Görünüm ▸ Yardımcılar ▸ Yakalama Modları…** ile aynı listeyi açın. Her satır bir moddur ve
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

Yakalama **fareyle nişan aldığınız** noktaya uygulanır. Komut satırına yazdığınız bir
koordinat yazdığınız yere düşer; yakınlaştırma ne kadar uzak olursa olsun yakındaki bir
köşeye çekilmez. Ayrıntı: [Komut satırı](../komutlar/komut-satiri.md).

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

### Tuvalin üstündeki şerit: bir çıkış yolu ya da bir sonraki adım

Tuvalin üst kenarında bazen tek düğmeli bir şerit belirir:

| Şerit | Ne zaman | Örnek |
|---|---|---|
| Uyarı renginde | Bir komut **reddedildi** ve reddin bir çıkış yolu var | Dış referanstaki bir nesne düzenlenmek istendi: **Yerel Kopya** |
| Vurgu renginde | Bir komut **bitti** ve bir sonraki adımı öneriyor | `ALANÖLÇ` köşelerden bitti: **Alan olarak çiz**; `KAPSAMDENETİM` kopuk nesne buldu: **Seç** |

Düğme, komutun transkripte yazdığı satırı çalıştırır — **Alan olarak çiz** aynı köşelerle
`ALAN …` satırını; çizim o satırın kendisiyle, tek geri alma adımıyla ve günlükte kendi
satırıyla değişir. Düğmeye basmak zorunlu değildir: şerit, bir sonraki komut bittiğinde
kalkar. Komut satırında çalışan el aynı satırı transkriptten okur; betik ve yapay zekâ da
komutun cevabında aynı satırı alır.

## Komut satırı

Tuvalin altında, 28 piksellik bir şerit. Solunda değişmeyen bir **`Komut:`** yazısı
vardır; sağında ne yazdığınız ve çalışan komutun ne beklediği görünür.

Her zaman açıktır — bir CAD kullanıcısının eli oraya kendiliğinden gider.
**Görünüm ▸ Pencereler ▸ Paneller ▸ Komut Satırı** ya da **Ctrl+9** ile gizlenebilir.

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
satırı komut satırına yazmak aynı işi yapar. Aynı araçlar şeritte **Analiz ▸ İşlem
araçları** düğmesinin listesinde de durur; Kadastro ve Açıklama sekmelerindeki araç
düğmeleri de bu kartı açar. Kapalı bir alan seçiliyken beliren **Alan** sekmesindeki
**Köşe Numarala** ve **Uzunluk Yaz** ise kartı açmadan, seçili alanlarda varsayılan
ayarlarla hemen çalışır; ayarları o panelin **↘** başlatıcısı gösterir. `TERCİH
araç_penceresi evet` ile kart panelin içinde değil kendi
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

Rozetler ayrı şeyler söyler:

- **HESAP** (turuncu) — bu sayı hesaplanmıştır, elle yazılmaz.
- **BOŞ** (soluk) — bu hücre henüz doldurulmamıştır.
- **ELLE** — bir ölçünün yazısı ölçülen değer değil, elle yazılmış bir değerdir.
- **KOPUK** — bir ölçünün ölçtüğü nesneyle bağı kopmuştur.
- **UYARLA** — ölçü, plan ölçeğinden başka bir pafta ölçeği için boyutludur
  ([`ÖLÇÜYENİLE`](../komutlar/dimension_refresh.md)).

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

Seçilen bir **tarama** ise en üstte **TARAMA** grubu açık gelir: `desen`, `aci`,
`olcek` (kendi deseninizde `aralik`), `cift`, `adalar` ve bağlıysa `sinir` (kopuksa
**KOPUK**). Düzenlenebilir her hücre bir [`TARAMADÜZENLE`](../komutlar/hatch_edit.md)
satırıdır.

Seçilen bir **ölçü** ise en üstte **ÖLÇÜ** grubu açık gelir, NESNE onun altında
kapalı: `olculen` (noktalardan hesaplanan değer, HESAP), `yazi` (paftada yazan;
elle yazılmışsa **ELLE**), `metin` (`<>` ölçülen değerdir), `onek`, `sonek`,
`birim`, `hassasiyet`, `tolerans`, `stil`, `pafta_olcegi` ve bağlıysa `baglar`.
Düzenlenebilir her hücre bir [`ÖLÇÜDÜZENLE`](../komutlar/dimension_edit.md)
satırıdır: hücrede yazdığınız ile komut satırına yazdığınız aynı değişikliktir, tek
adımda geri alınır. `metin` hücresi ölçülen değer için `<>` gösterir, böylece
hücreye girip değiştirmeden çıkmak ölçülen değeri elle yazılmış bir değere
çevirmez.

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
**Görünüm ▸ Pencereler ▸ Paneller ▸ Komut Günlüğü**'nü açın; bkz. [Komut günlüğü](../mimari/gunluk.md).

## Katmanlar paneli

Sağ panelin altında, kendi 29 piksellik başlığıyla ve iki sekmesiyle: **Katmanlar** ve
**Dış Referanslar** (aşağıda). Panel dar olduğunda, açık olmayan sekme yalnız simgesiyle
görünür; üstüne gelince adı yazar. Başlığın sağ ucunda panelin iki
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

Gruplanmış katmanlar (bir DXF'ten gelen `PLAN > SINIRLAR`, bir dış referansın `altlik`
grubu) bir **grup satırının** altında durur. Grup satırının gözü, altındaki katmanlardan
biri görünürken açıktır; ona tıklamak altındaki **bütün** katmanları gizler — hepsi
gizliyse gösterir — ve tek Ctrl+Z geri getirir. Grubun kendi kilidi yoktur; kilit simgesi
yalnız altındaki katmanların hepsi kilitliyse görünür.

Seçili satır vurgu yıkaması ve sol kenarında 2 piksellik vurgu çizgisi taşır. Bu
kalıp programdaki **her** listede aynıdır; üzerine gelme ise düz bir gridir ve
seçime hiç benzemez — "farenin nerede olduğu" ile "neyin seçili olduğu" birbirine
karışmaz.

Altta bir sayaç: kaç katman var ve kaçı düzenlenebilir.

**Birden fazla katman seçebilirsiniz**: Ctrl ile tek tek, Shift ile aralık. Vurgulamak
bir düzenleme değildir — aktif katmanı değiştirmez.

Sağ tuş menüsü, tıkladığınız satırın komutlarını sunar: **Tümünü seç**, **Katmana
yakınlaş** (`YAKINLAŞ KATMAN`), **Öznitelik tablosu**, **Aktif katman yap**, **Özniteliklerden etiketle…**, **Görünüm** alt menüsü,
kilit, **Gruba taşı…** ve en altta **Katman Özellikleri…**.

**Görünüm** alt menüsü gösterme ve gizlemeye dairdir: göster, gizle, yalnız bunu göster,
tümünü göster, gösterimi ters çevir. Seçimin **tamamına** uygulanır — seçili olmayan bir
satıra sağ tıklarsanız yalnız o satıra — ve kaç katman seçili olduğu giriş başlığında
yazar. Onbir katman tek bir Ctrl+Z ile geri gelir.

Her giriş bir komutla gider: görünürlük [`KATMANGÖRÜNÜM`](../komutlar/layer_visibility.md),
geri kalanı [`KATMAN`](../komutlar/layer.md), `SEÇ` ve `ETİKET`. Panelin yapıp komut
satırının yapamadığı bir şey yoktur.

### Dış Referanslar sekmesi

Çizime bağlı [dış referansların](../komutlar/xref.md) listesi. Bir dosya bağladığınızda
sekme kendiliğinden öne gelir; başlıktaki **＋** burada yeni bir dosya bağlar. Her satır
iki satırdır: üstte göz, ad ve durumu; altta dosyanın yeri (proje klasörünün içindeyse
ona göre) ve kaç nesne, kaç referans olduğu.

| Durum | Anlamı |
|---|---|
| `YÜKLÜ` | Dosyasından okundu, çiziliyor |
| `DEĞİŞTİ` | Dosya, çizim açıkken başka yerde kaydedildi; çizim önceki hâlini gösteriyor |
| `BOŞALTILDI` | Bir kenara kondu; referansı yerinde, boş |
| `BULUNAMADI` | Dosyası kayıtlı yerinde de proje klasöründe de yok |
| `BOŞ` | Dosyası var ama ondan çizime bir şey gelmedi |

Bir dosya başka bir programda ya da başka bir PiriCAD penceresinde kaydedildiğinde
program bunu fark eder: satır `DEĞİŞTİ` olur, sekmenin üstünde **Kaynak dosya değişti**
bandı ve durum çubuğunda bir satır belirir. Banttaki **Yenile** değişen dosyaları yeniden
okur.

Satırın altındaki düğmeler seçili dış referansa uygulanır: **Yenile**, **Boşalt** (boşaltılmış
olanda **Yükle**), **Yol…** (dosyanın yeni yerini seçtirir), **Bağla** ve **Kaldır**; sağ
tuş menüsü aynı adımları sunar. Satırın gözü dış referansın katmanlarını gizler ve
gösterir. Her adım bir `DIŞREFERANS` ya da `KATMANGÖRÜNÜM` satırıdır ve tek Ctrl+Z ile
geri alınır.

## Durum çubuğu

26 piksel, pencerenin tamamını kaplar — tuvalin ve sağ panelin de altından geçer.

| Bölüm | Ne yazar |
|---|---|
| solda | İmlecin koordinatı: `Y <sağa değer>  X <yukarı değer>` |
| ortada | Yardımcı anahtarları: **IZGARA · YAKALAMA · DİK · POLAR · OSNAP · DİNAMİK GİRDİ · KALINLIK** |
| sağda | Pafta ölçeği ve koordinat sistemi (`1 : 1 000 · EPSG:5254 · ITRF96 / TM30`), yapay zekâ sunucusunun durumu, veritabanı durumu ve çizim motoru |

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
ötekilerde Seçenekler penceresinin ilgili sayfası.

### Uzun işler

Bazı komutların işi saniyeler sürer ve pencereyi dondurmadan ayrı bir iş parçacığında
koşar: [`İÇEAKTAR`](../komutlar/import.md) dosyayı okurken,
[`DIŞAAKTAR`](../komutlar/export.md) dosyayı yazarken, [`TOPOLOJİ`](../komutlar/topology.md)
paftayı denetlerken, [`EŞYÜKSELTİ`](../komutlar/contour.md) ve
[`HACİM`](../komutlar/earthwork.md) yüzeyi üçgenlerken ve **Araçlar** panelindeki
[işlem araçları](../islem/README.md) çalışırken. Üretilmiş [komut
referansında](../komutlar/referans.md) bu komutların bayraklarında **uzun iş** yazar.

İş sürerken durum çubuğunun mesaj hücresi işin adını ve ilerlemesini gösterir —
`Topoloji denetimi · %40`, `Dışa aktarılıyor: ada.gpkg · %75` —, altında kayan bir şerit
programın çalıştığını söyler ve yanında **Durdur** çipi durur. Pencere dar olduğu için
yer kalmazsa iş hücresi, iş bitene dek yardımcı çiplerin (IZGARA, YAKALAMA…) yerini
alır; Durdur her zaman görünür.

| Ne | Sonuç |
|---|---|
| **Durdur** çipi ya da **Esc** | İşi keser. Çizim, dosya ve komut günlüğü işten önceki hâlinde kalır; komut neyin durduğunu söyler, örneğin `Topoloji denetimi durduruldu; sonuç verilmedi, çizim değişmedi.` |
| **Enter** ya da sağ tık | İşi **durdurmaz**; iş kendi bitince biter |
| İş sürerken çizimi değiştiren bir komut (yazılan, düğmeden, betikten ya da bir yapay zekâ istemcisinden) | Çalışmaz: `Bir iş sürüyor (Topoloji denetimi); o bitene dek çizim değiştirilmez. Bitmesini bekleyin ya da durum çubuğundaki Durdur'a basın (Esc).` İş bitince aynı komutu yeniden verin |
| İş sürerken çizimi okuyan bir komut (ölçme, sorgulama) ve görünüm (yakınlaştırma, kaydırma) | Çalışır |
| İş sürerken gelen bir yapay zekâ önerisi | Kartı açılır ama önizlenmez ve **Uygula** iş bitene dek beklemesini söyler; öneri bozulmaz, iş bitince önizlenir (bkz. [Onay](../yapay-zeka/onay.md)) |

**Neden çizim kilitli?** İş, çizimi başladığı anki hâliyle okur. Altında çizim
değişseydi bir dışa aktarım yarısı eski yarısı yeni bir dosya yazabilir, bir denetim
yarısı başka bir paftayı denetleyebilir, durdurulan bir iş de sonradan eklenenleri
geri alırdı. Kilit yalnız iş sürerken vardır ve kimse için ayrıcalık tanımaz: arayüz,
komut satırı, betik ve yapay zekâ aynı cevabı alır.

## Tema

Şeritte **Görünüm ▸ Tema ▸ Koyu Tema** ile ya da **Seçenekler ▸ Görünüm ve Tema ▸ tema** ile
değiştirilir. Gece ve gündüz olmak üzere iki tema vardır ve ikisi de aynı yapıdan
üretilir: aynı jetonlar, farklı değerler. Bir panelin gündüz temasında yeri
değişmez, yalnızca rengi değişir.

Tema tercihi profilinizde saklanır ve program açıldığında geri gelir.

### Neden her platformda aynı görünüyor

Şerit, paneller ve düğmeler işletim sisteminden alınmaz; uygulama kendisi çizer —
yalnız pencere çerçevesi ve başlık çubuğu işletim sisteminindir. Yazı tipleri (**IBM Plex Sans** ve **IBM Plex Mono**) programla
birlikte gelir, sistemde kurulu olmaları gerekmez. Sonuç: Windows, macOS ve
Linux'ta aynı pencere, aynı ölçüler, aynı renkler.

## Klavyeyle tam kullanım

PiriCAD faresiz tam çalışabilir olacak şekilde tasarlanır. Bugün klavyeyle
yapabilecekleriniz:

| Tuş | İşlev |
|---|---|
| **Ctrl+K** | Komut listesi — favorileriniz ve son kullandıklarınız üstte, altında kategorilere ayrılmış, yazdıkça süzülen sayfa |
| **Ctrl+9** | Komut satırını açar veya kapatır |
| Komut satırına yazmak | Komut girmek |
| **Yukarı / Aşağı** | Komut geçmişi |
| **Esc** | Satırı temizler; satır boşsa komutu iptal eder |
| **Enter** ya da **Boşluk** (boş satırda) | Komut çalışmıyorken son komutu yöntemiyle yeniden başlatır ([Son komutu yinelemek](../komutlar/komut-satiri.md#son-komutu-yinelemek)) |
| **Ctrl+Z** / **Ctrl+Shift+Z** | Geri al / yinele. Çizgi, çoklu çizgi, alan ya da spline çizerken Ctrl+Z yalnız son noktayı geri alır |
| **⌫** | Seçili nesneleri siler; çizerken yalnız son noktayı geri alır. Komut satırı boşken de çalışır |
| **Ctrl+A** / **Ctrl+Shift+A** | Tümünü seç / seçimi temizle |
| **Ctrl+H** (macOS'ta **Cmd+Option+F**) | [Bul ve Değiştir](../komutlar/find_replace.md): yazılarda bul, önizle, hepsini değiştir. macOS'ta Cmd+H programı gizlediği için orada başka tuştur |
| **F3** / **F8** / **F10** / **F9** | Nesne yakalama / dik mod / yüzey normali / ızgaraya yakalama |
| **Ctrl+0** | Kapsama yakınlaş |
| **Alt+C** | Önceki görünüm — Netcad'deki gibi (`YAKINLAŞ ÖNCEKİ`, otuz adım) |
| **Alt+Z** | Pencereyle yakınlaş: tuvalde sürüklediğiniz ya da iki köşesine tıkladığınız pencereye (`YAKINLAŞ PENCERE`); çalışan komutu bozmaz |
| **Ctrl++** / **Ctrl+-** | Yakınlaştır / uzaklaştır |
| **Ctrl+R** | Betik çalıştır |
| **F1** | [Komut listesi](../komutlar/help.md) — `Ctrl+K` ile aynı sayfa |
| **F6** | [Öznitelik tablosu](../veri/oznitelik-tablosu.md) |
| **F12** | Geliştirici bilgisi |
| **Ctrl+Shift+K** | [Yapay Zeka](../yapay-zeka/sohbet.md) paneli — komut paletinin **Ctrl+K**'sinin yanında |
| **Tab** / **Boşluk** / **↓** | Şeritte gezer, düğmeye basar, bölünmüş düğmenin listesini açar (bkz. [Şerit](#klavyeyle-ve-ekran-okuyucuyla)) |

Koordinatlar komut satırından girilebildiği için çizim de tamamen klavyeyle yapılabilir.

Tek harfli genel kısayol bilinçli olarak yoktur: komut satırına `Ç` yazarken tuşun
komuta kaçmaması gerekir. Kısaltmalar komut satırına **yazılır**, kısayol tuşu değildir.

Şeridin ekran okuyucu desteği [yukarıda](#klavyeyle-ve-ekran-okuyucuyla) anlatılır; bütün
pencerelerde tamamlanması Faz 1'de gelecek.

## Sırada ne var

- [Komut sistemi](../komutlar/README.md)
- [Komut satırı](../komutlar/komut-satiri.md)
- [Nesne seçme](../komutlar/select.md)
- [Oturum modları](../komutlar/mode.md)
- [Öznitelik tablosu](../veri/oznitelik-tablosu.md)
