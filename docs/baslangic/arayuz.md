# Arayüz

PiriCAD penceresini yeni açan kullanıcı için; bu sayfayı bitirdiğinizde her panelin ne
işe yaradığını, nasıl taşınacağını ve fareyle klavyeyle neyin nasıl yapılacağını
bileceksiniz.

## Pencere düzeni

```text
┌ menü çubuğu ──────────────────────────────────────────────────────┐
│ Dosya │ Düzen │ Görünüm │ Katman [aktif katman ▾] │ CBS          │
├───────────────────────────────────────────────────────────────────┤
│ ┌────┐ ┌──────────────────────────────┐ ┌─────────────────────┐ │
│ │Araç│ │                              │ │ Katmanlar │ Öznitel.│ │
│ │kutu│ │      Harita alanı            │ │      (sekmeli)      │ │
│ │ su │ │                              │ │                     │ │
│ │    │ ├──────────────────────────────┤ │                     │ │
│ │    │ │  Komut satırı (gizli)        │ │                     │ │
│ └────┘ └──────────────────────────────┘ └─────────────────────┘ │
│ ┌ Transkript │ Komut Günlüğü ───────────────────────────────────┐│
└ durum çubuğu ─────────────────────────────────────────────────────┘
```

İki ayrı araç yüzeyi vardır ve işleri farklıdır:

- **Araç kutusu** (sol, dikey) — çizim ve düzenleme araçları. Bir araca basınca komut
  başlar ve sizden girdi ister.
- **Araç çubukları** (üst, yatay) — eylemler: dosya, geri alma, görünüm, katman ve
  CBS. Bir düğmeye basmak komutu hemen çalıştırır.

Bu ayrım AutoCAD ve QGIS'in ortak düzenidir.

## Menü çubuğu

| Menü | İçerik |
|---|---|
| **Dosya** | Aç (**Ctrl+O**), Kaydet (**Ctrl+S**), Farklı Kaydet… (**Ctrl+Shift+S**), İçe Aktar…, Dışa Aktar…, Betik Çalıştır… (**Ctrl+R**), Çıkış; Yeni ve Yazdır sonraki fazlarda |
| **Düzen** | Geri Al (**Ctrl+Z**), Yinele (**Ctrl+Shift+Z**), Sil, Tümünü Seç (**Ctrl+A**), Seçimi Temizle (**Ctrl+Shift+A**) |
| **Çizim** | Çizgi, Çoklu Çizgi, Yay, Daire, Dikdörtgen, Nokta, Metin, Katman, Katman Yöneticisi |
| **Görünüm** | Kapsama Yakınlaş (**Ctrl+0**), Yakınlaştır, Uzaklaştır, Nesne Yakalama (**F3**), Dik Mod (**F8**), Izgaraya Yakala (**F9**), Araç Çubukları, Paneller, Koyu Tema, Geliştirici Bilgisi (**F12**) |
| **CBS** | Sorgula, Öznitelik Tablosu, Ölç, AI Asistan — hepsi sonraki fazlarda |
| **Yardım** | Komut Listesi, Hakkında |

**Görünüm > Araç Çubukları** her araç çubuğunu, **Görünüm > Paneller** her paneli tek
tek gizleyip gösterir. **Düzeni Sıfırla** fabrika yerleşimine döndürür.

## Araç çubukları

Menü çubuğunun altında beş araç çubuğu vardır. Hepsi taşınabilir ve
**Görünüm > Araç Çubukları** menüsünden tek tek gizlenebilir.

### Dosya

| Düğme | Komut | Durum |
|---|---|---|
| Aç | `AÇ` | **Ctrl+O** |
| Kaydet | `KAYDET` | **Ctrl+S** — çizim henüz bir dosyaya bağlı değilse Farklı Kaydet penceresini açar |
| Farklı Kaydet | `FARKLIKAYDET` | **Ctrl+Shift+S**, menüde |
| İçe Aktar | `İÇEAKTAR` | DXF ve GeoPackage okur |
| Dışa Aktar | `DIŞAAKTAR` | DXF ve GeoPackage yazar |
| Betik Çalıştır | `BETİK` | **Ctrl+R** |
| Yeni, Yazdır | — | Faz 1–2'de gelecek, şimdilik pasif |

Dosya seçme pencereleri yalnızca komutun argümanını toplar: aynı işi komut
satırından ve betikten de yapabilirsiniz, ve üçü de aynı komuta gider.
Bkz. [Proje dosyası açma](../komutlar/open.md),
[Çizimi kaydetme](../komutlar/save.md),
[Dış veri alma](../komutlar/import.md).

### Düzen

| Düğme | Komut | Durum |
|---|---|---|
| Geri Al | `GERİAL` | **Ctrl+Z** |
| Yinele | `YİNELE` | **Ctrl+Shift+Z** |
| Sil | `SİL` | Seçili nesneleri siler; seçim boşsa komut satırını hazırlar |
| Taşı, Kopyala, Döndür, Ofset | — | Faz 2'de gelecek, şimdilik pasif |

`Düzen` menüsünün altında ayrıca **Ayarlar…** (**Ctrl+,**) vardır: bildirilen her ayarı
kapsamına göre gösteren pencereyi açar. Ayrıntısı [AYAR](../komutlar/setting.md)
sayfasındadır.

### Görünüm

| Düğme | Komut | Durum |
|---|---|---|
| Kaydır | — | Faz 2; orta fare tuşu her zaman kaydırır |
| Kapsama Yakınlaş | `YAKINLAŞ KAPSAM` | **Ctrl+0** |
| Yakınlaştır | `YAKINLAŞ ÇARPAN carpan=1.25` | **Ctrl++** |
| Uzaklaştır | `YAKINLAŞ ÇARPAN carpan=0.8` | **Ctrl+-** |
| Nesne Yakalama | `MOD yakalama_modları ...` | **F3**, **Görünüm** menüsünde |

### Katman

| Düğme | Komut | Durum |
|---|---|---|
| Katman Yöneticisi | — | Faz 1'de gelecek, şimdilik pasif |
| Katman | `KATMAN` | Katman adını sorar |
| **Aktif katman listesi** | `KATMAN ad="..."` | Çalışıyor |

Aktif katman listesi CAD'in imza denetimidir: renk kutucuğuyla birlikte katmanları
gösterir, seçtiğiniz katman aktif olur. Listeden seçim yapmak `KATMAN` komutunu
gönderir — yani günlüğe yazılır ve **Ctrl+Z** ile geri alınabilir.

### CBS

| Düğme | Komut | Durum |
|---|---|---|
| Sorgula, Öznitelik Tablosu, Ölç | — | Faz 2'de gelecek, şimdilik pasif |
| AI Asistan | — | Faz 3'te gelecek, şimdilik pasif |

Pasif düğmelerin ipucu balonu hangi fazda geleceğini yazar. PiriCAD sessizce hiçbir şey
yapmayan düğme göstermez.

## Araç kutusu

Sol kenardaki palet. Çizim ve düzenleme araçlarını taşır. Her düğme bir komut
gönderir — düğmeye basmakla komutu yazmak arasında hiçbir fark yoktur.

**Araçlar paletin genişliğine göre dizilir:** soldan sağa doldurur, satır dolunca alta
geçer. Paleti dar bıraktığınızda tek sütun olur, kenarından tutup genişlettiğinizde iki,
üç, dört sütunlu bir ızgaraya dönüşür. Grup ayraçları satırın tamamını kaplar, böylece
hangi araçların birlikte olduğu her genişlikte okunur kalır.

| Araç | Gönderdiği komut | Durum |
|---|---|---|
| Seç | — | Çalışan komutu iptal eder (**Esc**); komut yokken fare zaten seçim yapar |
| Çizgi | `ÇİZGİ` | [Çizgi çizme](../komutlar/line.md) |
| Çoklu Çizgi, Yay, Daire, Dikdörtgen, Nokta, Metin | — | Faz 2'de gelecek |
| Sil | `SİL` | [Nesne silme](../komutlar/erase.md) |
| Taşı, Kopyala, Döndür, Ofset | — | Faz 2'de gelecek |
| Ölç, Sorgula | — | Faz 2'de gelecek |

Her düğmenin ipucu balonunda komut adı ve varsa kısayolu yazar.

**Araç kutusu taşınabilir.** Üstündeki tutamaktan sürükleyerek pencerenin sağ kenarına
taşıyabilir ya da pencereden koparıp serbest bir palet hâline getirebilirsiniz.
Fabrika yerleşimine dönmek için **Görünüm > Paneller > Düzeni Sıfırla**.

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

Harita alanının hemen altındaki tek satırlık alan. **Bu sürümde varsayılan olarak
gizlidir**; işi araç çubukları devraldı.

Açmak için **Ctrl+9** veya **Görünüm > Paneller > Komut Satırı**. Kapatmak için aynı
kısayol.

Gizli olması hiçbir şeyi kaldırmaz: komut satırından yazabildiğiniz her komut betikten
de çalışır ve araç çubuğu düğmeleri de aynı komutları gönderir.

- Komut adı yazarken satır içi tamamlama önerir
- **Yukarı/Aşağı ok** komut geçmişinde gezinir
- Bir komut girdi beklerken buraya koordinat yazabilirsiniz
- **Esc** önce satırı temizler, satır zaten boşsa çalışan komutu iptal eder

Tam kullanım: [Komut satırı](../komutlar/komut-satiri.md).

## Katmanlar paneli

Sağ üstteki **Katmanlar** sekmesi. Sütunlar:

| Sütun | İçerik |
|---|---|
| Katman | Renk kutucuğu ve ad. Aktif katman kalın yazılır |
| Gör. | `●` görünür, `○` gizli |
| Kilit | `🔒` kilitli, `–` açık |
| Nesne | Katmandaki canlı nesne sayısı |

**Çift tıklama davranışı:**

- **Gör.** sütununa çift tıklamak görünürlüğü ters çevirir — `KATMAN ad="..." gorunur=...` komutunu gönderir
- **Kilit** sütununa çift tıklamak kilidi ters çevirir — `KATMAN ad="..." kilitli=...` gönderir
- Diğer sütunlara çift tıklamak katmanı aktif yapar — `KATMAN ad="..."` gönderir

Yani panelden yaptığınız her değişiklik komut veri yolundan geçer, günlüğe yazılır ve
**Ctrl+Z** ile geri alınabilir.

## Öznitelikler paneli

**Katmanlar** ile aynı sekme grubundaki ikinci sekme. Seçili katmanın ve dokümanın
özelliklerini gösterir.

| Grup | Alanlar |
|---|---|
| Doküman | Koordinat sistemi, katman sayısı, nesne sayısı, aktif katman, sürüm numarası |
| Kapsam | X min/max, Y min/max, genişlik × yükseklik (metre). Çizim boşsa "boş çizim" |
| Katman — *ad* | Görünür, kilitli, renk (`#AARRGGBB`), çizgi kalınlığı, nesne sayısı, tepe noktası sayısı |
| Oturum | Komut sayısı, günlük satırı sayısı, geri alma/yineleme derinliği, render arka ucu |

Katman grubu yalnızca **Katmanlar** panelinden bir katman seçtiğinizde görünür.

**Her iki panel de taşınabilir.** Sekmeyi sürükleyerek sol kenara taşıyabilir, alt
bölgeye indirebilir veya pencereden koparabilirsiniz.

## Transkript ve Komut Günlüğü

Alttaki sekme grubu.

**Transkript** komutların size söylediklerini gösterir: hangi katmanın aktif olduğu, kaç
nesne silindiğinin, hataların. Komut satırına yazdığınız her şey `> ` önekiyle burada
tekrarlanır.

**Komut Günlüğü** aynı oturumu makine biçiminde gösterir — her komut bir JSON satırı.
Ayrıntı: [Komut günlüğü](../mimari/gunluk.md).

## Durum çubuğu

| Bölme | İçerik |
|---|---|
| Sol | Çalışan komutun isteği, komut yoksa `Hazır` |
| Katman | Aktif katman adı |
| Sağa (Y) / Yukarı (X) | İmlecin harita koordinatı, metre, üç ondalık. Türk haritacılık konvansiyonu: **Y sağa değer, X yukarı değer** |
| Ölçek | Bir ekran pikselinin kaç metreye karşılık geldiği |
| Sağ | Dokümanın koordinat sistemi, örneğin `TUREF/TM30` |

## Tema

**Görünüm > Koyu Tema** gündüz ve gece teması arasında geçer. Varsayılan gündüz
temasıdır. Seçiminiz kaydedilir ve programı yeniden açtığınızda korunur; pencere boyutu
ve panel yerleşimi de öyle.

## Klavyeyle tam kullanım

PiriCAD faresiz tam çalışabilir olacak şekilde tasarlanır. Bugün klavyeyle
yapabilecekleriniz:

| Tuş | İşlev |
|---|---|
| **Ctrl+9** | Komut satırını açar veya kapatır |
| Komut satırı açıkken yazmak | Komut girmek |
| **Yukarı / Aşağı** | Komut geçmişi |
| **Esc** | Satırı temizler; satır boşsa komutu iptal eder |
| **Ctrl+Z** / **Ctrl+Shift+Z** | Geri al / yinele |
| **Ctrl+A** / **Ctrl+Shift+A** | Tümünü seç / seçimi temizle |
| **F3** / **F8** / **F9** | Nesne yakalama / dik mod / ızgaraya yakalama |
| **Ctrl+0** | Kapsama yakınlaş |
| **Ctrl++** / **Ctrl+-** | Yakınlaştır / uzaklaştır |
| **Ctrl+R** | Betik çalıştır |
| **F12** | Geliştirici bilgisi |
| **Alt+D / Alt+Z / Alt+G / Alt+Y** | Menüleri açar |

Koordinatlar komut satırından girilebildiği için çizim de tamamen klavyeyle yapılabilir.

Tek harfli genel kısayol bilinçli olarak yoktur: komut satırına `Ç` yazarken tuşun
komuta kaçmaması gerekir. Kısaltmalar komut satırına **yazılır**, kısayol tuşu değildir.

Ekran okuyucu desteği (NVDA, VoiceOver, Orca) Faz 1'de tamamlanacak.

## Sırada ne var

- [Komut sistemi](../komutlar/README.md)
- [Komut satırı](../komutlar/komut-satiri.md)
- [Nesne seçme](../komutlar/select.md)
- [Oturum modları](../komutlar/mode.md)
