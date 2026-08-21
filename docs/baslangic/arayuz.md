# Arayüz

PiriCAD penceresini yeni açan kullanıcı için; bu sayfayı bitirdiğinizde her panelin ne
işe yaradığını, nasıl taşınacağını ve fareyle klavyeyle neyin nasıl yapılacağını
bileceksiniz.

## Pencere düzeni

```
┌ menü çubuğu ──────────────────────────────────────────────────────┐
│ ┌────┐ ┌──────────────────────────────┐ ┌─────────────────────┐ │
│ │Araç│ │                              │ │ Katmanlar │ Öznitel.│ │
│ │kutu│ │      Harita alanı            │ │      (sekmeli)      │ │
│ │ su │ │                              │ │                     │ │
│ │    │ ├──────────────────────────────┤ │                     │ │
│ │    │ │  Komut satırı                │ │                     │ │
│ └────┘ └──────────────────────────────┘ └─────────────────────┘ │
│ ┌ Transkript │ Komut Günlüğü ───────────────────────────────────┐│
└ durum çubuğu ─────────────────────────────────────────────────────┘
```

## Menü çubuğu

| Menü | İçerik |
|---|---|
| **Dosya** | Betik Çalıştır… (**Ctrl+R**), Çıkış |
| **Düzen** | Geri Al (**Ctrl+Z**), Yinele (**Ctrl+Shift+Z**), Sil |
| **Çizim** | Çizgi, Katman, Ölç |
| **Görünüm** | Kapsama Yakınlaş (**Ctrl+0**), Yakınlaştır, Uzaklaştır, Paneller, Koyu Tema, Geliştirici Bilgisi (**F12**) |
| **Yardım** | Komut Listesi, Hakkında |

**Görünüm > Paneller** her paneli tek tek gizleyip gösterir ve **Düzeni Sıfırla** ile
panelleri fabrika yerleşimine döndürür.

## Araç kutusu

Sol kenardaki dar sütun. Her düğme bir komut gönderir — düğmeye basmakla komut satırına
komutu yazmak arasında hiçbir fark yoktur.

| Araç | Gönderdiği komut | Not |
|---|---|---|
| Seç | — | Çalışan komutu iptal eder (**Esc**) |
| Çizgi | `ÇİZGİ` | [Çizgi çizme](../komutlar/line.md) |
| Katman | `KATMAN` | [Katman yönetimi](../komutlar/layer.md) |
| Ölç | — | Faz 2'de gelecek, şimdilik pasif |
| Sil | `SİL` | Komut satırını `SİL nesneler=` ile hazırlar |
| Geri Al | `GERİAL` | **Ctrl+Z** |
| Yinele | `YİNELE` | **Ctrl+Shift+Z** |
| Kaydır | — | Faz 2'de gelecek; orta fare tuşu her zaman kaydırır |
| Kapsama Yakınlaş | `YAKINLAŞ KAPSAM` | **Ctrl+0** |
| Yakınlaştır | `YAKINLAŞ ÇARPAN carpan=1.25` | **Ctrl++** |
| Uzaklaştır | `YAKINLAŞ ÇARPAN carpan=0.8` | **Ctrl+-** |
| Betik Çalıştır | `BETİK` | **Ctrl+R** |
| AI Asistan | — | Faz 3'te gelecek, şimdilik pasif |

Her düğmenin ipucu balonunda komut adı ve varsa kısayolu yazar.

**Araç kutusu taşınabilir.** Üstündeki tutamaktan sürükleyerek pencerenin sağ kenarına
taşıyabilir ya da pencereden koparıp serbest bir palet hâline getirebilirsiniz. Sağ
üstündeki küçük düğme de aynı işi yapar. Fabrika yerleşimine dönmek için
**Görünüm > Paneller > Düzeni Sıfırla**.

## Harita alanı

Çizimin göründüğü yer.

| Etkileşim | Sonuç |
|---|---|
| Sol tık | Çalışan komuta bir nokta verir |
| Sağ tık | Çalışan komutu iptal eder |
| Orta tuş basılı sürükle | Görünümü kaydırır |
| Fare tekerleği | İmlecin bulunduğu noktaya yakınlaştırır/uzaklaştırır |
| **Esc** | Çalışan komutu iptal eder |

İmleç konumu artı işaretiyle gösterilir ve koordinatı durum çubuğunda yazar. Bir komut
nokta beklerken son noktadan imlece kesikli bir kılavuz çizgi uzanır.

Arka plandaki kılavuz ızgara, yakınlaştırma düzeyine göre 1 / 2 / 5 × 10ⁿ metre
aralıklarına oturur.

**F12** geliştirici bilgisini açar: etkin çizim arka ucu, çizilen nesne ve tepe noktası
sayısı, görünüm dışında kaldığı için elenen nesne sayısı, kare süresi. Bu bir geliştirici
katmanıdır, günlük kullanımda kapalıdır.

Bu sürümde harita `QPainter` ile çizilir; GPU çizimi Faz 1'de devreye girecek
(`CLAUDE.md` Article 8.1). Transkript açılışta bunu hatırlatır.

## Komut satırı

Harita alanının hemen altındaki tek satırlık alan. PiriCAD'in birincil giriş yüzeyidir.

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
| X / Y | İmlecin harita koordinatı, metre, üç ondalık |
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
| Doğrudan yazmak | Komut satırına komut girmek |
| **Yukarı / Aşağı** | Komut geçmişi |
| **Esc** | Satırı temizler; satır boşsa komutu iptal eder |
| **Ctrl+Z** / **Ctrl+Shift+Z** | Geri al / yinele |
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
