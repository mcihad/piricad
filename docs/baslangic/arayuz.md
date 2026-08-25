# Arayüz

PiriCAD penceresini yeni açan kullanıcı için; bu sayfayı bitirdiğinizde her panelin ne
işe yaradığını, nasıl taşınacağını ve fareyle klavyeyle neyin nasıl yapılacağını
bileceksiniz.

## Pencere düzeni

```text
┌ başlık çubuğu 34 px ──────────────────────────────────────────────────────┐
│ ● ● ●  Dosya Düzen Görünüm Çizim Değiştir Harita Analiz Katman Pencere    │
│        Yardım        <belge adı>              [🔍 Komut ara… ⌘K]  (MK)    │
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

Her bandın yüksekliği sabittir ve her platformda aynıdır: başlık 34, araç çubuğu 46,
doküman sekmeleri 30, komut satırı 28, durum çubuğu 26 piksel. Sol araç kutusu 46,
sağ panel 312 piksel genişliğindedir. Bu ölçüler tasarım belgesinden gelir ve
Windows, macOS ve Linux'ta değişmez — pencere çerçevesi de dahil olmak üzere
uygulamanın kendi çizdiği bir çerçevedir, işletim sisteminin değil.

İki ayrı araç yüzeyi vardır ve işleri farklıdır:

- **Araç kutusu** (sol, dikey, 46 px) — çizim ve düzenleme araçları, beş grup.
  Bir araca basınca komut başlar ve sizden girdi ister. En altta iki renk kutusu:
  çizim rengi ve dolgu rengi.
- **Araç çubuğu** (üst, yatay, 46 px) — eylemler, yedi grup: dosya · geri/yinele ·
  pano · gezinme · yardımcılar · pencereler · çıktı. Bir düğmeye basmak komutu
  hemen çalıştırır. Sağ ucunda iki salt-okunur okuma vardır: **ÖLÇEK** ve
  **KOORDİNAT SİSTEMİ**.

Bu ayrım AutoCAD ve QGIS'in ortak düzenidir.

## Başlık çubuğu

Solda üç pencere düğmesi: kapat, küçült, büyüt. Bunlar **çizilir**, işletim
sisteminden alınmaz — aynı program her yerde aynı görünsün diye.

Ortada açık belgenin adı ve sürüm. Sağda **komut arama** ve kullanıcı baş harfi.

### Komut arama — `Ctrl+K`

Kutucuğa tıklayın ya da **Ctrl+K**'ya basın: bir arama penceresi açılır ve
yazdıkça komutları süzer. Liste `Registry`'den üretilir; yani bugün var olan her
komut oradadır ve yarın eklenen komut da hiçbir liste güncellenmeden orada olur.

Arama Türkçe katlamayla çalışır: `cizgi` yazınca `ÇİZGİ`, `olcek` yazınca `ÖLÇEK`
bulunur. Seçtiğiniz komut komut satırına yazılır, imleç sonuna gelir — çünkü
argümanı olan bir komutun argümanı yazılmalıdır.

## Menü çubuğu

On menü, hep bu sırayla: **Dosya · Düzen · Görünüm · Çizim · Değiştir · Harita ·
Analiz · Katman · Pencere · Yardım**. Sıra tasarımın parçasıdır: `Harita`'nın
nerede olduğunu öğrenen kullanıcı onu her platformda aynı yerde bulur.

Kısayol altçizgileri yalnızca **Alt** basılıyken görünür. Alt+D hâlâ Dosya'yı
açar; altçizgi, ekranı boş yere doldurmasın diye gizlidir.

| Menü | İçerik |
|---|---|
| **Dosya** | Yeni, Aç (**Ctrl+O**), Kaydet (**Ctrl+S**), Farklı Kaydet… (**Ctrl+Shift+S**), İçe/Dışa Aktar…, Yazdır, Veritabanı… (**Ctrl+Shift+D**), Betik Çalıştır… (**Ctrl+R**), Çıkış |
| **Düzen** | Geri Al (**Ctrl+Z**), Yinele (**Ctrl+Shift+Z**), Tümünü Seç (**Ctrl+A**), Seçimi Temizle (**Ctrl+Shift+A**), Ayarlar… |
| **Görünüm** | Kapsama Yakınlaş (**Ctrl+0**), Yakınlaştır, Uzaklaştır, Nesne Yakalama (**F3**), Dik Mod (**F8**), Izgaraya Yakala (**F9**), Araç Çubuğu, Paneller, Koyu Tema, Geliştirici Bilgisi (**F12**) |
| **Çizim** | Çizgi, Çoklu Çizgi, Yay, Daire, Dikdörtgen, Nokta, Metin |
| **Değiştir** | Sil, Taşı, Kopyala, Döndür, Ofset |
| **Harita** | Sorgula, Ölç, Veritabanı… |
| **Analiz** | Öznitelik Tablosu (**F6**), Yapay Zekâ Önerisi |
| **Katman** | Katman, Katman Yöneticisi |
| **Pencere** | Panellerin açık/kapalı durumu, Yerleşimi Sıfırla |
| **Yardım** | Komut Listesi, Hakkında |

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

Beş grup, toplam 19 araç. Aktif araç vurgu rengiyle işaretlenir.

| Grup | Araçlar |
|---|---|
| **seçim** | Seç · Alan Seç · Kaydır |
| **oluşturma** | Çoklu Çizgi · Poligon · Dikdörtgen · Daire · Nokta · Metin |
| **düzenleme** | Böl/Buda · Birleştir (tevhit) · Parsel Böl (ifraz) · Taşı · Ofset |
| **ölçüm** | Uzunluk Ölç · Alan Ölç · Koordinat Oku |
| **yardımcı** | Stil Kopyala · Topoloji Denetimi |

Faz 2'de gelecek araçlar pasiftir ve hangi fazda geleceklerini ipucunda yazarlar —
görünmez olmaları, yokmuş gibi davranmaktan daha kötü olurdu.

En altta iki kutu: üstteki **çizim rengi**, alttaki **dolgu rengi**. İçi boş bir
alt kutu "dolgu yok" demektir.

## Doküman sekmeleri

Tuvalin üstünde, 30 piksel. Her açık çizim bir sekmedir; etkin sekmenin kapatma
işareti vardır. Sağ uçta bölünmüş görünüm ve tam ekran düğmeleri bulunur.

Birden çok çizimi aynı anda açmak Faz 2'de gelecek; bugün tek sekme görünür.

## Harita alanı

Çizimin göründüğü yer.

| Etkileşim | Sonuç |
|---|---|
| Sol tık, komut çalışırken | Çalışan komuta bir nokta verir |
| Sol tık, komut yokken | İmlecin yakınındaki nesneyi seçer |
| Sol tuş basılı sürükle, komut yokken | Seçim kutusu çizer |
| **Shift** + tık/sürükle | Seçime ekler |
| **Ctrl** + tık/sürükle | Seçimden çıkarır |
| Sağ tık | Çalışan komutu iptal eder |
| Orta tuş basılı sürükle | Görünümü kaydırır |
| Fare tekerleği | İmlecin bulunduğu noktaya yakınlaştırır/uzaklaştırır |
| **Esc** | Çalışan komutu iptal eder; komut yoksa seçimi temizler |

İmleç konumu artı işaretiyle gösterilir ve koordinatı durum çubuğunda yazar. Bir komut
nokta beklerken son noktadan imlece kesikli bir kılavuz çizgi uzanır.

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

### Seçim

Hiçbir komut çalışmıyorken sol fare tuşu seçim yapar. **Soldan sağa** sürüklerseniz
kutuya **tamamen giren** nesneler seçilir ve çerçeve düz çizilir; **sağdan sola**
sürüklerseniz kutuya **değen** her nesne seçilir ve çerçeve kesik çizilir. Bu, CAD
dünyasının kırk yıllık ayrımıdır ve PiriCAD'de de aynıdır.

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

| Kısayol | Ne yapar |
|---|---|
| **F3** | Nesne yakalamayı açar/kapatır |
| **F8** | Dik modu açar/kapatır — imleci yatay ve düşey eksene kilitler |
| **F9** | Izgaraya yakalamayı açar/kapatır |

Arama yarıçapı `yakalama_toleransı`, seçme kutusu `seçim_toleransı` tercihidir ve
ikisi de **ekran pikselidir**: nişan alan göz ekrana bakar, bu yüzden tolerans
yakınlaştırmayla birlikte değişir.

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

Ayrıntı için bkz. [Komut satırı](../komutlar/komut-satiri.md).

## Sağ panel

312 piksel genişliğinde ve iki panel taşır. Her panelin başlığı 29 pikseldir ve
başlıkta sekmelerle birlikte üç işaret bulunur: **tutamak** (sürükle), **daralt**
ve **yüzdür**.

### Öznitelikler

Üstte **seçili nesne kartı**: tip ikonu, adı ve bir satır tanım. Hiçbir şey seçili
değilse belgenin kendi bilgileri yazılır — boşalan bir panel bozulmuş görünür.

Altında katlanabilir gruplar ve `112 px | 1fr` ızgarasında satırlar: solda alan adı,
sağda değer. Değer tek aralıklı yazıyla yazılır, çünkü değer veridir ve veri tek
aralıklı okunur.

İki rozet vardır ve ikisi ayrı şey söyler:

- **HESAP** (turuncu) — bu sayı hesaplanmıştır, elle yazılmaz.
- **BOŞ** (soluk) — bu hücre henüz doldurulmamıştır.

Katmanlar panelinde bir katman seçtiğinizde bu panel o katmanın özelliklerini
gösterir: kabuğun **tek** özellik yüzeyi vardır, iki tane değil.

### Geçmiş

Oturumda ne olduğunun metin dökümü. Komut günlüğünün kendisi için
**Pencere ▸ Komut Günlüğü**'nü açın; bkz. [Komut günlüğü](../mimari/gunluk.md).

## Katmanlar paneli

Sağ panelin altında, kendi 29 piksellik başlığıyla. Her satır tek bir satırdır ve
sütun başlığı yoktur:

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

Sağ tuş menüsü katman komutlarını sunar: adlandır, renk, kilit, stil düzenle. Hepsi
`KATMAN` komutuyla gider.

## Durum çubuğu

26 piksel, pencerenin tamamını kaplar — araç kutusunun ve sağ panelin de altından
geçer.

| Bölüm | Ne yazar |
|---|---|
| solda | İmlecin koordinatı: `Y <sağa değer>  X <yukarı değer>` |
| ortada | Yardımcı anahtarları: **IZGARA · YAKALAMA · DİK · POLAR · OSNAP · DİNAMİK GİRDİ · KALINLIK** |
| sağda | Veritabanı durumu ve çizim motoru |

Anahtarlara tıklamak o ayarı yazar — ve bir **komut** gönderir. Yani F7 ile tıklamak
aynı şeydir ve ikisi de günlüğe aynı satırı yazar. Açık bir anahtar iki şeyle
işaretlenir: zemini açılır **ve** yazısı vurgu rengine döner; renk körü bir
kullanıcı için tek başına renk yeterli değildir.

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

PiriCAD faresiz tam çalışabilir olacak şekilde tasarlanır. Bugün klavyeyle
yapabilecekleriniz:

| Tuş | İşlev |
|---|---|
| **Ctrl+K** | Komut arama — yazdıkça süzülen komut listesi |
| **Ctrl+9** | Komut satırını açar veya kapatır |
| Komut satırına yazmak | Komut girmek |
| **Yukarı / Aşağı** | Komut geçmişi |
| **Esc** | Satırı temizler; satır boşsa komutu iptal eder |
| **Ctrl+Z** / **Ctrl+Shift+Z** | Geri al / yinele |
| **Ctrl+A** / **Ctrl+Shift+A** | Tümünü seç / seçimi temizle |
| **F3** / **F8** / **F9** | Nesne yakalama / dik mod / ızgaraya yakalama |
| **Ctrl+0** | Kapsama yakınlaş |
| **Ctrl++** / **Ctrl+-** | Yakınlaştır / uzaklaştır |
| **Ctrl+R** | Betik çalıştır |
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
