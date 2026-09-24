<!-- ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN. -->
<!-- Kaynak: kentos_kapsam. Her hücre bir komut GERÇEKTEN çalıştırılarak ölçülür. -->
<!-- Yeniden üret: make kapsam -->

# Destek Matrisi

Hangi düzenleme işleminin hangi nesne türünde ne yaptığını gösterir. Tablo elle
yazılmadı: her hücre için boş bir çizimde o tür, bir kullanıcının yazacağı komutla
oluşturulur, işlem aynı komut veri yolundan çalıştırılır ve **çıkan sonuç ölçülür**.
Bir davranış değiştiğinde bu sayfa da aynı değişiklikte değişir.

| İşaret | Anlamı |
|---|---|
| ✓ destekli | İşlem çalıştı ve sonuç o türden beklenen şey: kesilen yay yay kalır, taşınan nesne tam kayar |
| ◐ kısmi | İşlem çalıştı ama sonuç **türce yanlış**: eğri kirişe döndü, açık çizgi kapalı banda döndü, ölçü analitik değerden sapıyor |
| ✗ yok | Komut reddetti; ret cümlesi kanıttır |
| — uygulanamaz | İşlemin o türde anlamı yok: kapalı dairenin ucu uzatılmaz, tek noktada köşe yoktur |
| ⊘ ölçülemedi | Tür, bir kullanıcının yazabileceği hiçbir komutla oluşturulamadı |

Dosya alışverişi (DXF, DWG, GeoPackage) bu tabloda **yoktur**: sonucu derlemedeki
biçim kütüphanelerine bağlıdır ve ayrı bir matriste ölçülecektir. Burada ölçülen tek
gidiş-dönüş, her derlemenin yazıp okuduğu proje dosyasıdır.

## Özet

| Tür | Seç | Yakala | Tutamaç | Taşı | Döndür | Ölçekle | Aynala | Kes (BUDA) | Uzat | Böl | Kır | Paralel (OFSET) | Yuvarla | Pah | Uç uca | Ölç | Proje dosyası |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| [Çizgi](#cizgi) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | ✓ | ✓ | ✓ |
| [Köşeli çoklu çizgi](#coklucizgi) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ◐ | ✓ | ✓ | ✓ | ✓ |
| [Yaylı çoklu çizgi (DXF şişkinliği)](#yayli) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ◐ | ✗ | ✗ | ✓ | ✓ | ✓ |
| [Delikli alan](#alan) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✗ | — | ✗ | ✗ | ✓ | ✗ | ✗ | — | ✓ | ✓ |
| [Çok parçalı alan](#cokparca) | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ | ⊘ |
| [Daire](#daire) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | ✗ | ✓ | ✓ | — | — | — | ✓ | ✓ |
| [Yay](#yay) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | ✓ | ✓ | ✓ |
| [Elips](#elips) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✗ | — | ✗ | ✗ | ◐ | — | — | — | ✓ | ✓ |
| [Spline](#spline) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✗ | ✗ | ✗ | ✗ | ◐ | — | — | ✗ | ✓ | ✓ |
| [Tarama](#tarama) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✗ | — | ✗ | ✗ | ✗ | ✗ | ✗ | — | ✓ | ✓ |
| [Nokta](#nokta) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | ✓ | ✓ |
| [Yazı](#yazi) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | ✓ | ✓ |
| [Blok referansı](#blok) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | ✓ | ✓ |
| [Ölçü](#olcu) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | — | — | — | — | — | — | — | — | ✓ | ✓ |
| [Kılavuz çizgi](#lider) | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ | ✗ | ✗ | ✗ | ✗ | ✗ | — | — | ✗ | ✓ | ✓ |

238 hücre: 154 destekli, 4 kısmi, 28 yok, 52 uygulanamaz. ⊘ işaretli satırların türü hiçbir komutla oluşturulamadığı için ölçülemedi;
nedeni o türün kanıt bölümündedir.

## Sessiz retler

Yok: reddeden her komut reddini hata olarak döndürüyor.

## Kanıt

### <a id="cizgi"></a>Çizgi

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | uç nokta, orta nokta, en yakın |
| Tutamaç | ✓ destekli | 2 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | ✓ destekli | kesildi: 50.000 m → 25.000 m; sonuç: ÇOKLUÇİZGİ |
| Uzat | ✓ destekli | uzadı: 50.000 m → 60.000 m; sonuç: ÇOKLUÇİZGİ |
| Böl | ✓ destekli | 2 parça, toplam uzunluk korundu; sonuç: ÇOKLUÇİZGİ, ÇOKLUÇİZGİ |
| Kır | ✓ destekli | aradaki parça çıktı: 50.000 m → 33.500 m; sonuç: ÇOKLUÇİZGİ, ÇOKLUÇİZGİ |
| Paralel (OFSET) | ✓ destekli | 2 paralel (ÇOKLUÇİZGİ), kaynak korundu |
| Yuvarla | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Pah | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Uç uca | ✓ destekli | iki parça tek ÇOKLUÇİZGİ oldu, uzunluk korundu |
| Ölç | ✓ destekli | çevre 50.000 m — analitik değerle aynı |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="coklucizgi"></a>Köşeli çoklu çizgi

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | uç nokta, orta nokta, en yakın |
| Tutamaç | ✓ destekli | 3 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | ✓ destekli | kesildi: 90.000 m → 65.000 m; sonuç: ÇOKLUÇİZGİ |
| Uzat | ✓ destekli | uzadı: 90.000 m → 100.000 m; sonuç: ÇOKLUÇİZGİ |
| Böl | ✓ destekli | 2 parça, toplam uzunluk korundu; sonuç: ÇOKLUÇİZGİ, ÇOKLUÇİZGİ |
| Kır | ✓ destekli | aradaki parça çıktı: 90.000 m → 60.300 m; sonuç: ÇOKLUÇİZGİ, ÇOKLUÇİZGİ |
| Paralel (OFSET) | ✓ destekli | 2 paralel (ÇOKLUÇİZGİ), kaynak korundu |
| Yuvarla | ◐ kısmi | 5 m yarıçapla yuvarlandı; yuvarlatma yayı kirişlerle (ÇOKLUÇİZGİ) |
| Pah | ✓ destekli | 5 m pah kırıldı; sonuç: ÇOKLUÇİZGİ |
| Uç uca | ✓ destekli | iki parça tek ÇOKLUÇİZGİ oldu, uzunluk korundu |
| Ölç | ✓ destekli | çevre 90.000 m — analitik değerle aynı |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="yayli"></a>Yaylı çoklu çizgi (DXF şişkinliği)

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | uç nokta, orta nokta, en yakın |
| Tutamaç | ✓ destekli | 4 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | ✓ destekli | kesildi: 108.540 m → 69.270 m; sonuç: YAYLIÇİZGİ |
| Uzat | ✓ destekli | uzadı: 108.540 m → 118.540 m; sonuç: YAYLIÇİZGİ |
| Böl | ✓ destekli | 2 parça, toplam uzunluk korundu; sonuç: YAY, YAYLIÇİZGİ |
| Kır | ✓ destekli | aradaki parça çıktı: 108.540 m → 72.719 m; sonuç: YAY, YAYLIÇİZGİ |
| Paralel (OFSET) | ◐ kısmi | YAYLIÇİZGİ paraleli kirişlerle (ÇOKLUÇİZGİ) üretildi |
| Yuvarla | ✗ yok | Nesne 1 bir eğri, yazı ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda çalışır. |
| Pah | ✗ yok | Nesne 1 bir eğri, yazı ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda çalışır. |
| Uç uca | ✓ destekli | iki parça tek YAYLIÇİZGİ oldu, uzunluk korundu |
| Ölç | ✓ destekli | çevre 108.540 m — analitik değerle aynı |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="alan"></a>Delikli alan

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | uç nokta, orta nokta, en yakın, ağırlık merkezi |
| Tutamaç | ✓ destekli | 8 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki, alan dört katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | ✗ yok | Nesne 1 bu komutun işleyebileceği bir tür değil; BUDA çizgi, yay ve dairelerde çalışır. |
| Uzat | — uygulanamaz | kapalı eğrinin ucu yok |
| Böl | ✗ yok | Nesne 1 bir alan; alan kenarı boyunca açılmaz. |
| Kır | ✗ yok | Nesne 1 kırılamıyor; KIR çizgi, yay, daire ve yaylı çoklu çizgide çalışır. |
| Paralel (OFSET) | ✓ destekli | 1 paralel (ÇOKLUÇİZGİ), kaynak korundu |
| Yuvarla | ✗ yok | Nesne 1 çok halkalı; köşe işlemleri tek halkalı nesnelerde çalışır. |
| Pah | ✗ yok | Nesne 1 çok halkalı; köşe işlemleri tek halkalı nesnelerde çalışır. |
| Uç uca | — uygulanamaz | uç uca eklenecek ucu yok |
| Ölç | ✓ destekli | çevre 440.000 m, alan 7600.000 m² — analitik değerle aynı |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="cokparca"></a>Çok parçalı alan

⊘ **Ölçülemedi: bu tür hiçbir komutla oluşturulamıyor.** Denenen yol: BİRLEŞTİR iki alanı tek nesne yapmadı: 2 kaldı

### <a id="daire"></a>Daire

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | merkez, en yakın, çeyrek nokta |
| Tutamaç | ✓ destekli | 5 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki, alan dört katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | ✓ destekli | kesildi: 62.832 m → 31.416 m; sonuç: YAY |
| Uzat | — uygulanamaz | kapalı eğrinin ucu yok |
| Böl | ✗ yok | Kapalı bir şekil tek noktada bölünmez; en az iki bölme noktası verin. |
| Kır | ✓ destekli | aradaki parça çıktı: 62.832 m → 42.260 m; sonuç: YAY |
| Paralel (OFSET) | ✓ destekli | yarıçap 2 m değişti, daire kaldı |
| Yuvarla | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Pah | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Uç uca | — uygulanamaz | uç uca eklenecek ucu yok |
| Ölç | ✓ destekli | çevre 62.832 m, alan 314.159 m² — analitik değerle aynı |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="yay"></a>Yay

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | uç nokta, orta nokta, merkez, en yakın, çeyrek nokta |
| Tutamaç | ✓ destekli | 4 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | ✓ destekli | kesildi: 78.540 m → 52.360 m; sonuç: YAY |
| Uzat | ✓ destekli | uzadı: 78.540 m → 88.671 m; sonuç: YAY |
| Böl | ✓ destekli | 2 parça, toplam uzunluk korundu; sonuç: YAY, YAY |
| Kır | ✓ destekli | aradaki parça çıktı: 78.540 m → 52.621 m; sonuç: YAY, YAY |
| Paralel (OFSET) | ✓ destekli | 1 paralel (YAY), kaynak korundu |
| Yuvarla | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Pah | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Uç uca | ✓ destekli | iki parça tek YAYLIÇİZGİ oldu, uzunluk korundu |
| Ölç | ✓ destekli | çevre 78.540 m — analitik değerle aynı |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="elips"></a>Elips

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | uç nokta, merkez, en yakın |
| Tutamaç | ✓ destekli | 5 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki, alan dört katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | ✗ yok | Nesne 1 bu komutun işleyebileceği bir tür değil; BUDA çizgi, yay ve dairelerde çalışır. |
| Uzat | — uygulanamaz | kapalı eğrinin ucu yok |
| Böl | ✗ yok | Nesne 1 bu yöntemle bölünemiyor; BÖL çizgi, yay, daire ve yaylı çoklu çizgide çalışır. |
| Kır | ✗ yok | Nesne 1 kırılamıyor; KIR çizgi, yay, daire ve yaylı çoklu çizgide çalışır. |
| Paralel (OFSET) | ◐ kısmi | ELİPS paraleli kirişlerle (ÇOKLUÇİZGİ) üretildi |
| Yuvarla | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Pah | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Uç uca | — uygulanamaz | uç uca eklenecek ucu yok |
| Ölç | ✓ destekli | çevre 230.131 m, alan 3141.593 m² — analitik değerle aynı |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="spline"></a>Spline

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | uç nokta, en yakın, düğüm |
| Tutamaç | ✓ destekli | 4 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | ✗ yok | Nesne 1 bu komutun işleyebileceği bir tür değil; BUDA çizgi, yay ve dairelerde çalışır. |
| Uzat | ✗ yok | Nesne 1 bu komutun işleyebileceği bir tür değil; UZAT çizgi, yay ve dairelerde çalışır. |
| Böl | ✗ yok | Nesne 1 bu yöntemle bölünemiyor; BÖL çizgi, yay, daire ve yaylı çoklu çizgide çalışır. |
| Kır | ✗ yok | Nesne 1 kırılamıyor; KIR çizgi, yay, daire ve yaylı çoklu çizgide çalışır. |
| Paralel (OFSET) | ◐ kısmi | SPLINE paraleli kirişlerle (ÇOKLUÇİZGİ) üretildi |
| Yuvarla | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Pah | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Uç uca | ✗ yok | Nesne 1 uç uca eklenemiyor; UÇUCA çizgi, yay ve yaylı çoklu çizgide çalışır. |
| Ölç | ✓ destekli | çevre 44.334 m (analitik değer tanımlı değil) |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="tarama"></a>Tarama

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | en yakın |
| Tutamaç | ✓ destekli | 4 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki, alan dört katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | ✗ yok | Nesne 1 bu komutun işleyebileceği bir tür değil; BUDA çizgi, yay ve dairelerde çalışır. |
| Uzat | — uygulanamaz | kapalı eğrinin ucu yok |
| Böl | ✗ yok | Nesne 1 bu yöntemle bölünemiyor; BÖL çizgi, yay, daire ve yaylı çoklu çizgide çalışır. |
| Kır | ✗ yok | Nesne 1 kırılamıyor; KIR çizgi, yay, daire ve yaylı çoklu çizgide çalışır. |
| Paralel (OFSET) | ✗ yok | Nesne 1: Bir taramanın paraleli olmaz; sınırının paralelini alın. |
| Yuvarla | ✗ yok | Nesne 1 bir eğri, yazı ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda çalışır. |
| Pah | ✗ yok | Nesne 1 bir eğri, yazı ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda çalışır. |
| Uç uca | — uygulanamaz | uç uca eklenecek ucu yok |
| Ölç | ✓ destekli | çevre 100.000 m, alan 600.000 m² — analitik değerle aynı |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="nokta"></a>Nokta

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | düğüm |
| Tutamaç | ✓ destekli | 1 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | — uygulanamaz | kesilecek bir eğri değil |
| Uzat | — uygulanamaz | ucu uzatılacak bir eğri değil |
| Böl | — uygulanamaz | bölünecek bir eğri değil |
| Kır | — uygulanamaz | kırılacak bir eğri değil |
| Paralel (OFSET) | — uygulanamaz | paraleli alınacak bir eğri değil |
| Yuvarla | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Pah | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Uç uca | — uygulanamaz | uç uca eklenecek ucu yok |
| Ölç | ✓ destekli | çevre 0.000 m (analitik değer tanımlı değil) |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="yazi"></a>Yazı

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | uç nokta, orta nokta, en yakın |
| Tutamaç | ✓ destekli | 2 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | — uygulanamaz | kesilecek bir eğri değil |
| Uzat | — uygulanamaz | ucu uzatılacak bir eğri değil |
| Böl | — uygulanamaz | bölünecek bir eğri değil |
| Kır | — uygulanamaz | kırılacak bir eğri değil |
| Paralel (OFSET) | — uygulanamaz | paraleli alınacak bir eğri değil |
| Yuvarla | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Pah | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Uç uca | — uygulanamaz | uç uca eklenecek ucu yok |
| Ölç | ✓ destekli | çevre 11.250 m (analitik değer tanımlı değil) |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="blok"></a>Blok referansı

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | en yakın, ekleme noktası |
| Tutamaç | ✓ destekli | 2 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | — uygulanamaz | kesilecek bir eğri değil |
| Uzat | — uygulanamaz | ucu uzatılacak bir eğri değil |
| Böl | — uygulanamaz | bölünecek bir eğri değil |
| Kır | — uygulanamaz | kırılacak bir eğri değil |
| Paralel (OFSET) | — uygulanamaz | paraleli alınacak bir eğri değil |
| Yuvarla | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Pah | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Uç uca | — uygulanamaz | uç uca eklenecek ucu yok |
| Ölç | ✓ destekli | çevre 0.000 m (analitik değer tanımlı değil) |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="olcu"></a>Ölçü

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | uç nokta, kesişim, en yakın |
| Tutamaç | ✓ destekli | 4 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | — uygulanamaz | kesilecek bir eğri değil |
| Uzat | — uygulanamaz | ucu uzatılacak bir eğri değil |
| Böl | — uygulanamaz | bölünecek bir eğri değil |
| Kır | — uygulanamaz | kırılacak bir eğri değil |
| Paralel (OFSET) | — uygulanamaz | paraleli alınacak bir eğri değil |
| Yuvarla | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Pah | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Uç uca | — uygulanamaz | uç uca eklenecek ucu yok |
| Ölç | ✓ destekli | çevre 0.000 m (analitik değer tanımlı değil) |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

### <a id="lider"></a>Kılavuz çizgi

| İşlem | Durum | Kanıt |
|---|---|---|
| Seç | ✓ destekli | pencere seçimi nesneyi aldı |
| Yakala | ✓ destekli | uç nokta, kesişim, en yakın |
| Tutamaç | ✓ destekli | 3 tutamaç; ilki taşındı, tür korundu |
| Taşı | ✓ destekli | kapsam tam (5, 3) m kaydı, tür ve ölçü korundu |
| Döndür | ✓ destekli | 90° döndü: en ve boy yer değiştirdi, çevre korundu |
| Ölçekle | ✓ destekli | ×2: kapsam iki katına çıktı |
| Aynala | ✓ destekli | dikey eksende aynalandı, tür ve çevre korundu |
| Kes (BUDA) | ✗ yok | Nesne 1 bu komutun işleyebileceği bir tür değil; BUDA çizgi, yay ve dairelerde çalışır. |
| Uzat | ✗ yok | Nesne 1 bu komutun işleyebileceği bir tür değil; UZAT çizgi, yay ve dairelerde çalışır. |
| Böl | ✗ yok | Nesne 1 bu yöntemle bölünemiyor; BÖL çizgi, yay, daire ve yaylı çoklu çizgide çalışır. |
| Kır | ✗ yok | Nesne 1 kırılamıyor; KIR çizgi, yay, daire ve yaylı çoklu çizgide çalışır. |
| Paralel (OFSET) | ✗ yok | Nesne 1: Bir açıklamanın (ölçü, kılavuz çizgi) paraleli olmaz; ölçtüğü çizginin paralelini alın. |
| Yuvarla | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Pah | — uygulanamaz | tek nesnede köşe yok; iki nesne arası köşe C-06'da |
| Uç uca | ✗ yok | Nesne 1 uç uca eklenemiyor; UÇUCA çizgi, yay ve yaylı çoklu çizgide çalışır. |
| Ölç | ✓ destekli | çevre 12.071 m (analitik değer tanımlı değil) |
| Proje dosyası | ✓ destekli | kaydet ve aç: tür, kapsam, çevre ve alan birebir aynı |

## Yüzeyler

Bütün istemciler aynı komut veri yolunu kullanır, dolayısıyla bir işlemin **sonucu**
yüzeyden yüzeye değişmez — bunu GUI = komut satırı = betik eşitlik kanıtı sınar. Değişen
tek şey işleme **ulaşılıp ulaşılamadığıdır**, ve o da komutun kendi bayraklarından okunur.

| İşlem | Komut | Komut satırı ve arayüz | Yapay zekâ ve MCP | Python ve JSON betik |
|---|---|---|---|---|
| Seç | `core.select` (`SEÇ`) | ✓ | ✗ bayrak yok | ✓ `cad.select` |
| Yakala | — | ✓ imleçle | — okuma aracı yok | — |
| Tutamaç | `core.vertex_move` (`KÖŞETAŞI`) | ✓ | ✓ | ✓ `cad.vertex_move` |
| Taşı | `core.move` (`TAŞI`) | ✓ | ✓ | ✓ `cad.move` |
| Döndür | `core.rotate` (`DÖNDÜR`) | ✓ | ✓ | ✓ `cad.rotate` |
| Ölçekle | `core.scale` (`ÖLÇEKLE`) | ✓ | ✓ | ✓ `cad.scale` |
| Aynala | `core.mirror` (`AYNALA`) | ✓ | ✓ | ✓ `cad.mirror` |
| Kes (BUDA) | `core.trim` (`BUDA`) | ✓ | ✓ | ✓ `cad.trim` |
| Uzat | `core.extend` (`UZAT`) | ✓ | ✓ | ✓ `cad.extend` |
| Böl | `core.split` (`BÖL`) | ✓ | ✓ | ✓ `cad.split` |
| Kır | `core.break` (`KIR`) | ✓ | ✓ | ✓ `cad.break` |
| Paralel (OFSET) | `core.offset` (`OFSET`) | ✓ | ✓ | ✓ `cad.offset` |
| Yuvarla | `core.fillet` (`YUVARLA`) | ✓ | ✓ | ✓ `cad.fillet` |
| Pah | `core.chamfer` (`PAH`) | ✓ | ✓ | ✓ `cad.chamfer` |
| Uç uca | `core.join` (`UÇUCA`) | ✓ | ✓ | ✓ `cad.join` |
| Ölç | `core.entity_info` (`NESNEBİLGİ`) | ✓ | ✓ | ✓ `cad.entity_info` |
| Proje dosyası | `core.saveas` (`FARKLIKAYDET`) | ✓ | ✗ bayrak yok | ✓ `cad.saveas` |
