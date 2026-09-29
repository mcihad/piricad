# KentOSCad — Netcad planı

> Plan tarihi: **28 Eylül 2026**. Bu belge Netcad araştırmasını KentOSCad işine çevirir: Netcad'deki
> hangi aracın bizde olduğu, hangisinin eksik ya da yarım olduğu ve eksikleri **KentOSCad'in kendi
> yoluyla** — daha sade, daha zarif, komut merkezli ve 3B'ye evrilebilir biçimde — nasıl yapacağımız.
> Uygulanmış özellik iddiası değildir; işaretsiz her madde yapılacak iştir. Biçim `TODOS.md` ile
> aynıdır: `[ ]` yapılacak, `[x]` bitti, `[!]` karar bekliyor.

**Neden bu plan.** Kullanıcının sözüyle: en büyük referansımız Netcad; AutoCAD gibi değil Netcad gibi
**harita odaklı** bir CAD geliştiriyoruz, altyapı **3B'ye evrilecek**; Netcad'deki araçları daha estetik,
zarif ve hoş biçimde yapacağız.

**Kaynaklar ve kanıt kuralı.**

| Taraf | Kaynak | Kural |
|---|---|---|
| Netcad | Resmî yardım sitesi `wiki.netcad.com.tr` sayfaları (Confluence REST dökümleri; bağlantı biçimi `https://wiki.netcad.com.tr/pages/viewpage.action?pageId=<id>`), sayfa ağaçları (şerit, temel, CBS, Hesap/Netçap/Netmap, NC6, yardım) ve önceki alt ajan raporu | Tablodaki her satırın `pageId`'si okunmuş ham sayfadır. Önceki raporun ekran görüntüsüne dayanan görsel iddiaları (renk kodları, piksel boyları, 8.6 sekme sırası) ve açık menü içerikleri doğrulanmadı; plan onlara dayanmaz (§10) |
| KentOSCad | `docs/komutlar/referans.md` (komut kaydından üretilmiş, **139 komut**), `src/app/src/main_window_ribbon.cpp` (şerit), `docs/baslangic/arayuz.md`, `docs/nesneler/destek-matrisi.md`, `docs/islem/README.md`, `docs/llms.txt`, `TODOS.md`, `TODOS-CAD.md`, `CLAUDE.md` ve `.claude/*.md` | Bir şey ancak referansta komut olarak ya da şerit kodunda/arayüz belgesinde panel olarak varsa "var" sayıldı |

**TODOS ile ilişki.** Bu plan `TODOS.md`'deki hiçbir maddeyi yinelemez. Yeni işler **N-01…N-29** kimliğini
alır (§3). Zaten `TODOS.md`'de olan bir işe Netcad araştırmasının eklediği somut şeyler §4'te o maddenin
kimliğiyle (U-01, G-10, S-01, T-01 …) yazılır; kimlik değişmez. `TODOS-CAD.md`'deki önceki Netcad/AutoCAD
karşılaştırmasının P0–P7 paketleri tamamlanmıştır (açı kuralı, nokta fonksiyonları, alım komutları, inşa
yöntemleri, düzenleme fiilleri, ölçü, pano) ve burada yeniden açılmaz.

**Mevzuat sınırı.** İFRAZ, ALANİFRAZ, TEVHİT ve mevzuata bağlı her şey (parselasyon, 18. madde, DOP, plan
notları, TAKS/KAKS, tescil belgeleri, tolerans ve tecvizler, pafta adlandırma kuralı, BÖHHBÜY gösterim
ölçüleri) için bu plan yalnız Netcad karşılığını ve arayüz/geometri açığını yazar; davranışı **tarif etmez**.
Böyle her satır **[M]** ile işaretlidir: *kullanıcı tarif edecek*. Kod yalnız veriyi yorumlar (CLAUDE.md 5.13,
domain.md R1).

## 1. Özet

### "Netcad gibi, daha zarif" ne demek

1. **Harita önce, çizgi sonra.** Netcad'de iş ölçü noktasından başlar: adı, kodu, kotu olan nokta, ona bağlı
   çizgiler, noktanın adıyla çizim. KentOSCad'de nokta bugün yalnız bir yerdir; ad ve kot öznitelikte durur.
   Nokta ve kot birinci sınıf olacak (N-05, S-01).
2. **Her nokta isteminde hesap.** Netcad'in "Koordinat Hesap Makinası" bizde zaten tek gramerde yaşıyor
   (nokta fonksiyonları: `n()`, `dik()`, `kes()`, `ara()`…); eksik olan onun **eli**: istem sürerken beliren
   bir bağlam sekmesi, her düğmenin komut satırına yazdığı satır (N-04).
3. **Kroki ve pafta.** Röleve, cephe, koordinat yazımı, karelaj, pafta indeksi. Netcad bunları düz yazı
   olarak bırakır; KentOSCad'de hepsi **kaynağına bağlıdır** — kaynak değişince izler ya da "güncel değil"
   der (bağlar ve sonuçlar, TODOS F-04).
4. **Arazi bir nesnedir.** Netsurf'ün üçgen modeli, eğrileri, hacmi ve kesitleri bizde kalıcı bir yüzeyin
   **sonuçları** olacak (T-01…T-03); kotlu geometri için önce veri modeli (N-05).
5. **Düzenleme tek yüzeyde.** Netcad'in "Düzenle" çarkı → KentOSCad'in tutamakları, nesne sekmeleri ve istem
   seçenekleri; modal pencere yok (N-09).
6. **Zarafet somut kurallardır** (§6): diyalog yerine tek satır ve tek kart; renk yalnız anlam; önizleme =
   sonuç; sessiz dönüştürme yok; her tıklamanın yazılı karşılığı; varsayılanlar pafta ölçeğinden; yönetmelik
   değeri kodda değil veride.
7. **3B'ye evrim yeniden yazmadan.** Bugünkü 2B depoya isteğe bağlı tepe kotu, sonra yüzey türü, en sonda
   ayrı bir 3B görünüm; kesme (cull) bloğu ve 2B çizim hattı hiç değişmeden (§7).

### Sayılar

| | Satır | Araç (yaklaşık) |
|---|---|---|
| Eşlenen | **277** | **~380** (bir satır `·` ile ayrılan birden çok aracı toplar) |
| ✓ var | 72 | ~91 |
| ◐ kısmi | 97 | ~128 |
| ✗ yok | 99 | ~140 |
| — kapsam dışı / bilerek yok | 9 | ~21 |
| [M] mevzuata bağlı (kullanıcı tarif edecek) | 30 | — |

Temel CAD düzenlemesi güçlü (Düzenle sekmesinin 21 satırından 16'sı ✓); açıklar **nokta ve kot**, **arazi**
(Netsurf'ün 15 satırında hiç ✓ yok), **kroki/pafta yazımı**, **konumsal analiz** ve **nokta girişinin eli**
etrafında toplanıyor.

### En önemli on açık

| # | Açık | Netcad karşılığı | İş |
|---|---|---|---|
| 1 | Tepe noktasında kot (Z) — 3B'nin temeli | Nokta Z'si, Kotları Modelden Al, Kot Sıfırla, eğik mesafe | N-05 |
| 2 | Kalıcı arazi modeli ve düzeltmesi | Üçgen Oluştur, Model Düzelt, Model Kontrolü | T-01 (§4) |
| 3 | Alan kesişimi, alan çıkarımı, çok parçalı alan | Alan ▾: Kesişim, Birleştir, Böl, Çıkart; Çoklu Doğru Birleştir | N-08 |
| 4 | Adı, kodu, kotu olan nokta ve Nokta Editörü | Nokta, Ardışıl Nokta At, Nokta Editörü, Otomatik Nokta Üret | S-01 (§4) |
| 5 | İstem sürerken hesap paleti ve seçim süzgeci; çizerken izleme | Nokta Seçim Araçları, Seçim Süzgeci, Obje İzle | N-04, N-06 |
| 6 | Kroki yazımı: dik düş, kutur, cephe, koordinat | Detaylar ▸ Röleve, Ölçme; Koordinat Yaz | N-15, N-16 |
| 7 | Pafta indeksi, pafta ara, noktanın paftası | Pafta Editörü, Pafta Ara, XYZ Sor | N-17 [M] |
| 8 | Overlay, İçindekinden/Çevreleyenden Bilgi Al, Kolon Doldur | Analiz ▸ Konumsal Analizler | G-10, G-03 (§4) |
| 9 | Önceki görünüm, pencereyle yakınlaş, içine tıklayarak alan | Önceki Pencere, Pencere Büyüt, Alan Seçim Aracı | N-01, N-03 |
| 10 | Kotlu alım, geriden kestirme, total station verisi | Eğik Kenar–Düşey Açı, Takeometri, Geriden Kestirme | S-02, S-01 (§4) |

### Paketler

| | P0 | P1 | P2 | Toplam |
|---|---|---|---|---|
| Yeni paketler (N-xx, §3) | 6 | 15 | 8 | **29** |
| Netcad katkısı alan TODOS maddeleri (§4) | — | — | — | **37** |

P0: N-01 gezinme, N-02 sorgu ekleri, N-03 içine tıklayarak alan, N-04 bağlam sekmeleri, N-05 tepe kotu,
N-08 alan işlemleri. Önerilen sıra ve kilometre taşları §8'de.

## 2. Eşleme tablosu

Her satır bir Netcad aracıdır (ya da `·` ile ayrılmış kardeş araçlar); sütunlar: Netcad'deki adı, resmî
yardım sayfasının `pageId`'si, KentOSCad'deki karşılığı (komut kimliği ve Türkçe adı ya da panel), durum,
kısa not ve işin nereye gittiği (N-xx §3'te, harf-rakam kimlikler `TODOS.md`'de).

| İşaret | Anlamı |
|---|---|
| ✓ | KentOSCad'de aynı işi gören komut ya da panel var (`docs/komutlar/referans.md` ya da şerit kodu) |
| ◐ | Var, ama Netcad'in önemli bir seçeneği ya da yolu eksik |
| ✗ | Yok |
| — | Kapsam dışı ya da bilerek yok (sebebi notta) |
| [M] | Mevzuata bağlı: davranışı kullanıcı tarif edecek; bu plan tarif etmez |

KHM = Koordinat Hesap Makinası (Netcad'in nokta istemi paleti, 217386679).

### 2.1 Pencere, uygulama menüsü, paneller

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| Yeni (şablon galerisi, favoriler) | 217387999 | `core.new` YENİ | ◐ | Boş çizim açar; proje şablonu ve galeri yok → U-06 |
| Aç (NCZ, DWG/DXF/DGN, SHP/KML/MDB/SQLite/XLS, GML, NCN/XYZ, LAS/LAZ, raster, GPX/NMEA, ZIP, NCY) | 217387997 | `core.open` AÇ (.pcad), `core.import` İÇEAKTAR | ◐ | DXF, GPKG, SHP (okuma), NCZ (okuma; pafta indeksi gerçek biçimiyle — 29 Eylül), DWG (derlemeye bağlı); ötekiler yok → I-01, I-02 |
| Projeye Ekle · Ayrı Projeler Olarak Aç | 217387997 | `core.import`, `core.xref` DIŞREFERANS | ◐ | Aynı anda tek belge açık (arayuz.md "Tek belge, sekmesiz") |
| Dosya Ekle Yerleştir (`??` alanlarını sorar) | 217388012 | `core.insert` BLOKEKLE `dosya=` `deger=` | ✓ | Blok alanları `deger=` ile, eksikse sorulur |
| Başka Projeden Özellik Ekle | 217388012 | — | ✗ | Katman/stil/blok tanımını başka projeden alma yok → U-06 |
| Kaydet · otomatik kayıt hatırlatması · `.NCY` yedekleri | 217388025 | `core.save` KAYDET | ◐ | Zaman damgalı yedek ve otomatik kayıt yok → I-05 |
| Şablon Olarak Kaydet | 217388025 | `core.layout_template` ÇIKTIŞABLON | ◐ | Yalnız çıktı yerleşimi şablonu; proje şablonu yok → U-06 |
| Seçilen Objeleri Kaydet | 217388025 | — | ✗ | → N-22 |
| Rulo Şeklinde Kaydet (şerit harita) | 217388025, 217385136 | — | ✗ | → N-26 |
| GeoTIFF Kaydet | 217388025 | `core.print` / `core.layout` PNG·TIFF + world file | ◐ | Coğrafi etiketli TIFF değil → L-04 |
| Katalog · Katalog Ara | 217385465, 217386588 | — | ✗ | Dosya kataloğu; paket yok (U-06/I-05 sonrası değerlendirilir) |
| Gönder › KML/KMZ | 217388033 | `core.export` DIŞAAKTAR | ✗ | KML yazımı izin listesinde yok → I-01 |
| Gönder › E-Posta · Teknik Desteğe Gönder | 217388033 | — | — | Bilerek yok: belge makineden kendiliğinden çıkmaz |
| Gönder › GPS (GPX/NMEA) | 217388033 | — | ✗ | → I-01 |
| Gönder › 3D+ | 217388033 | — | ✗ | → N-29 |
| Özellikler › Proje Ölçeği (yazı/sembol boyunu uyarla) | 217388073 | `AYAR plan_ölçeği`, `core.dimension_refresh` ÖLÇÜYENİLE | ◐ | Ölçüler uyarlanır; yazı ve blok boyu uyarlanmaz → U-04 |
| Özellikler › Projeksiyon | 217388073, 217387143 | `AYAR koordinat_sistemi`, `core.reproject` DÖNÜŞTÜR | ✓ | |
| Özellikler › 2. projeksiyon | 217388073 | — | ✗ | → G-01 |
| Özellikler › Proje Değişkenleri | 217388073 | `ÇIKTIÖĞE metin=` yer tutucuları (`<olcek>`, `<tarih>`, `<crs>`) | ◐ | Kullanıcı tanımlı proje değişkeni yok → L-02 |
| Proje Düzenle › Kullanılmayan Tanımları Temizle | 217388066 | — | ✗ | → N-22 |
| Proje Düzenle › Çift Objeleri · Çift Koordinatlı Objeleri Ayıkla | 217388066 | `core.cleanup` TEMİZLE | ✓ | Yineleneni bulur/onarır; "aynı ad" ölçütü yok |
| Proje Düzenle › Kot Sıfırla | 217388066 | — | ✗ | → N-05, N-22 |
| Çizdir (Ctrl+P; F5/F6 ölçek, F7/F8 döndür; CIZPEN kaydı) | 217388055 | `core.print` YAZDIR, yazdırma çerçevesi | ◐ | Çerçeve döndürme ve basılan pencerelerin kaydı yok → N-26 |
| Seçilen Pencereleri Çizdir · Seçilenleri Çizdir | 217388055, 217388037 | `core.layout` ÇIKTIYERLEŞİMİ atlas | ◐ | Yalnız seçili nesneyi basmak yok → N-26 |
| Genel Ayarlar | 217388138, 217388230 | Seçenekler (`TERCİH`, `AYAR`) | ✓ | |
| Hızlı Erişim Çubuğu (her komut eklenebilir) | 217387702 | hızlı erişim satırı | ◐ | Düğmeler sabit → U-01 (favoriler) |
| Katman Yöneticisi (CAD + Referanslar) | 217387696 | Katmanlar paneli + Dış Referanslar sekmesi | ✓ | |
| CAD Katmanlar (@ana tabaka, süzgeç, sıralama, kolon seç, kopyala/birleştir, Tabloya Aktar) | 217387722 | Katmanlar paneli (grup, süzgeç, sayaç) | ◐ | Katman kopyala/birleştir ve çizim sırasıyla sıralama yok → U-05 |
| Semboloji (hat tipi, font) | 217387071 | Stil Tasarımcısı, `core.style` STİL | ✓ | |
| Sayısallaştırma Sihirbazı (kalemler, BÖHYY menüleri) | 217387068 | — | ✗ | → G-04 |
| Ara › Netcad Arama Motoru (CAD ve spatial veride; kart/ızgara sonuç, süz, yazdır) | 217387074 | `core.query` SORGULA, `core.find_replace` BULDEĞİŞTİR, tabloda ara | ◐ | Tek arama yüzü ve bulgu listesi yok → N-21 |
| Ara › Sembol/Blok Ara | 217386605 | `core.symbol` SEMBOL `ara=`, `BLOKEKLE dosya=` | ◐ | Gösterim rafında arar; kitaplıkta arayıp yerleştirme tek adım değil → G-04 |
| Ara › Netcad Komut Satırı (`REGEN`, `SET ZOOM`, `PROJECT …`) | 217386603 | komut satırı (Türkçe adlar, `Registry`) | ✓ | İngilizce makro fiilleri yerine kayıttan üretilen Türkçe adlar |
| Ara › Komut Ara (Git, Çalıştır) | 217386600 | Ctrl+K komut listesi | ◐ | "Git" (komutu şeritte göster) yok → N-20 |
| Ara › Geocoding (çevrimiçi sağlayıcılar) | 217386598 | — | ✗ | Çevrimiçi ve anahtarlı; Açık soru 13 |
| Ara › GeoRSS | 217386594 | — | — | Kapsam dışı |
| Ara › Pafta Ara (Git, Oluştur) | 217386592 | — | ✗ | → N-17 [M] adlandırma kuralı |
| Mesaj Paneli (Bilgi/Uyarı/Hata; Yaz) | 217386585 | Geçmiş sekmesi, komut günlüğü | ◐ | Türe göre süzme ve bulgudan nesneye gitme yok → N-21 |
| Durum Çubuğu (aktif tabaka, hat tipi, ölçek, koordinat, dinamik bilgi) | 217386631 | durum çubuğu + şeritteki katman kutusu | ✓ | |
| Çizim Alanı › sol tık son işlemi tekrarlar | 217386629, 217387057 | — | ✗ | → U-01 katkısı (son komutu yinele) |
| Çizim Alanı › akıllı nesneye tıkla-düzenle | 217386629 | çift tık düzenleyicisi | ✓ | |

### 2.2 Nokta girişi ve seçim (Netcad "Genel Özellikler")

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| "Nokta Seçim Araçları" bağlam sekmesi | 217386613 | komut satırı + yakalama modları | ◐ | Bağlam sekmesi yok → N-04 |
| Nokta Yakalama (Nokta, Son Nokta, Kesişim, Orta, Uygulama Noktası, En Yakın, Karelaj, Eksene Dik, Otomatik) | 217387852 | `core.mode` MOD yakalama modları, F3/F8/F9/F10 | ✓ | KentOSCad'inki daha geniş (teğet, çeyrek, uzantı, paralel, izleme) |
| KHM › Koordinat (K) | 217386679 | mutlak ve göreli koordinat yazımı | ✓ | |
| KHM › Nokta Adı (süzgeçle, sıralı, atlama adımlı) | 217386679 | `n(nokta_no)` | ◐ | Tek nokta; ad aralığıyla nokta dizisi yok → N-04 |
| KHM › Nokta Bulutu | 217386679 | — | ✗ | → T-05 |
| KHM › Mesafe–Açı/Eğim, açıortay | 217386679 | `@d<a`, `semt()` | ◐ | Eğim % ve açıortay yok → U-02 |
| KHM › dX–dY | 217386679 | `@dx,dy` | ✓ | |
| KHM › Tabaka (katmanı nesneden al) | 217386679 | — | ✗ | → N-04 |
| KHM › Çizgi İzle · Obje İzle | 217386679 | — | ✗ | → N-06 |
| KHM › Kesişim (4 nokta) · 2 Noktalı Kesişim | 217386679 | `kes(A,B,C,D)`, `kes(A,r1,B,r2,yön)` | ✓ | |
| KHM › Dik-çık · Dik Düş · Teğet | 217386679 | dik ayak ve teğet yakalama | ✓ | |
| KHM › Dik-abs (dik ayak / dik boy) | 217386679 | `dik(A,B,ayak,boy)` | ✓ | İşaret kuralı Netcad'inki: sağ pozitif (29 Eylül; Açık soru 3) |
| KHM › Paralel Nokta (hat + paralel mesafe + uçtan sapma) | 217386679 | paralel yakalama, `ile()` | ◐ | Tek adımda yok → N-04 (`boyunca()`) |
| KHM › Hat Üzerinde a/b | 217386679 | `ara(A,B,oran)` | ✓ | |
| KHM › Obje Üzerinde (başlangıca mesafe + sapma) | 217386679 | — | ✗ | → N-04 (`boyunca()`) |
| KHM › GPS | 217386679 | — | ✗ | → N-28 |
| KHM › KM (güzergah kilometresi + sapma) | 217386679 | — | ✗ | → T-04 |
| Çizim Hesap Araçları (Açı, Sapma, Eğim, dX/dY, Koordinat, Referans Noktası; rakamla giriş) | 217386622, 217387846 | sürüklerken uzunluk + semt, DİNAMİK GİRDİ, `core.tracking` İZ | ◐ | Sapma açısı, eğim ve değer kilidi yok → U-02 |
| Obje Seçim Aracı (Boşluk ile alttakini ya da listeyi seç) | 217387890, 217388230 | `SEÇ mod=NOKTA sira=`, "Hangisi?" listesi | ◐ | Tuvalde aday döngüsü yok → U-03 |
| Hızlı Seçim › Pencere · Alan · Daire | 217387890 | `core.select` SEÇ PENCERE/KESEN/ÇOKGEN/ÇOKGENKESEN | ◐ | Daireyle seçim yok → N-04 |
| Hızlı Seçim › Kesenler · İçindekiler · Üzerindekiler · Dışındakiler | 217387890 | `SEÇ` KESEN/PENCERE | ◐ | "Dışındakiler" ve "Üzerindekiler" yok → N-04 |
| Hızlı Seçim › Doğruyu Kesenler · Noktayı İçerenler · Noktadan Geçenler | 217387890 | `SEÇ mod=ÇİT`, `mod=NOKTA` | ◐ | İçeren alan (iç içe) ve geçen yok → N-03, N-04 |
| Hızlı Seçim › Tümünü Seç · Son Seçilenler · Tersine Çevir | 217387890 | `SEÇ TÜMÜ/ÖNCEKİ/SON`, `islem=TERSİNE` | ✓ | |
| Hızlı Seçim › Bölgeden Kırp · Grup Seç | 217387890 | — | ✗ | Grup kavramı yok; paket yok (kırpma G-10) |
| Obje Tipleri süzgeci | 217387890 | `SEÇ tur=` | ✓ | Arayüz yüzü → N-04 |
| İleri Seçim Süzgeçleri (tabaka, metin, sembol, renk, uzunluk, boy, Z, kod; objeden kriter al) | 217387176 | öznitelik süzme ifadesi, `core.query` SORGULA | ◐ | Geometri ölçütleri ve "nesneden ölçüt al" yok → U-03, G-07 |
| Alan Seçim Aracı (içine tıkla, F2 Çevir, F3 Seç, F4 Gelişmiş, "/" iç içe) | 217387115 | `core.boundary` SINIR (yalnız komut olarak) | ◐ | Girdi türü olarak yok → N-03 |
| Süzgeç kuralları (`A*`, `-A*`, `~`, `#`, `500..521`, `1..10,2`, `+ & %`) | 217391827 | SQL biçimli süzme ifadesi | ◐ | Ad deseni ve aralık yok → U-03 katkısı |
| Projeksiyon Ayarları (EPSG, WKT, yerel + afin düzeltme) | 217387143 | Koordinat Sistemleri ayarı, `core.fit` OTURT | ◐ | Afin düzeltmeli yerel sistem tanımı yok → G-01 |

### 2.3 Giriş sekmesi

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| Çizgi (Ctrl+L) | 217385138 | `core.line` ÇİZGİ | ✓ | |
| Paralel Çizgi (eksenin sağına/soluna genişlik) | 217385136 | `core.offset` OFSET `taraf=iki` (sonradan) | ◐ | Çizerken iki yan yok → N-11 |
| 2 Daireye Teğet Doğru | 217385134 | teğet yakalama (tek uçta) | ◐ | İki eğriye birden teğet doğru yok → N-10 |
| Çoklu Doğru (Ctrl+C; bitişte kapalı/taralı/ad) | 217385133 | `core.polyline` ÇOKLUÇİZGİ | ✓ | Bitiş sorusu yok (bilinçli: ALAN ve TARAMA ayrı) |
| Noktadan Geçen Eğri (T-spline) | 217385130 | `core.spline` SPLINE (kontrol noktalı) | ◐ | Noktalardan geçen eğri yok → N-10 |
| Yumuşatılmış Eğri (B-spline) | 217385101 | `core.spline` SPLINE | ✓ | |
| Çoklu Doğru Birleştir (tek çok parçalı nesne) | 217385099 | — | ✗ | Çok parçalı nesne üreten komut yok (destek matrisi ⊘) → N-08 |
| Çoklu Doğru Parçası Çıkar | 217385096 | — | ✗ | → N-08 |
| Alan (alan, çevre, köşe; 2. projeksiyonda alan) | 217385092 | `core.area` ALAN | ✓ | 2. sistemde alan → G-01 |
| Kutu (ad, kâğıt boyu, dY/dX, açı; yerel pafta) | 217385090 | `core.rectangle` DİKDÖRTGEN | ◐ | Sayısal en/boy/açı ve kâğıt boyu yok → N-13 |
| Alan Kesişimi | 217385090 | — | ✗ | Çekirdekte `kernel_boolean` var, komutu yok → N-08 |
| Alan Birleştir (kesişmeyenler tek çok parçalı; delik korunur) | 217385090 | `core.combine` BİRLEŞTİR | ◐ | Ayrık parçalar ayrı nesne kalır → N-08 |
| Alan Böl (çizgisel obje ya da alanla; kot kesenden) | 217385090 | `core.split` BÖL | ◐ | Yalnız iki noktalı kesme çizgisi → N-08 |
| Alan Çıkart | 217385090 | — | ✗ | → N-08 |
| Nokta (Ctrl+N; N/Y/X/Z/K; ad artımı; aynı yerde düzeltme) | 217385088 | `core.point_draw` NOKTA | ◐ | Ad, kod, kot parametresi yok → S-01 katkısı, N-05 |
| Ardışıl Nokta At (başlangıç no, sabit Z, modelden Z) | 217385086 | — | ✗ | → S-01 katkısı |
| Daire (merkez; Y ile yarıçap) | 217385122 | `core.circle_draw` DAİRE | ✓ | |
| Daire: Serbest (teğet nesnelere, alternatifler, sonuçları kır) | 217385120 | `DAİRE yontem=ttr` | ◐ | Yalnız iki doğruya teğet → N-10 |
| Elips · Elips: Serbest (kısmi elips) | 217385117, 217385115 | `core.ellipse_draw` ELİPS | ✓ | |
| Yay (3 nokta) · Yay: Merkez | 217385113, 217385110 | `core.arc_draw` YAY | ✓ | |
| Yay Serbest (ardışık teğet yay/çizgi, D ile geçiş) | 217385108 | `YAY` teğet devam | ◐ | Tek nesnede çizgi↔yay zinciri yok → N-10 |
| Yazı (boy, açı, sıkışma, fon, `++`, uygulama noktası) | 217385103 | `core.text` METİN (YAZI) | ◐ | Sıkıştırma, zemin, ardışık artırma yok → N-12 |
| Metin Düzenleyici (ASCII rapor; CKS↔Excel) | 217385188 | — | — | Netcad rapor biçimine özgü; raporlar çıktı yerleşiminde |
| Metin Dosya Yükle | 217385186 | — | ✗ | → N-12 |
| Sembol (kitaplık, ölçeğe göre boy, 2 noktayla) | 217385184, 217386605 | `core.style` STİL gösterimi, `core.insert` BLOKEKLE, `core.symbol` SEMBOL | ◐ | Sembol katman stilinden gelir; iki noktayla döndürerek yerleştirme yok → N-13 |
| Resim (fotoğraf) | 217385183 | `ÇIKTIÖĞE tur=resim` | ◐ | Yalnız çıktı yerleşiminde → G-08 |
| Blok (yeni, yenile, adlandır, sil, yerleştir, 2 noktadan, ayrıştır) | 217385182 | `core.block` BLOK, `core.insert` BLOKEKLE, `core.block_edit` BLOKDÜZENLE, `core.explode` PATLAT | ◐ | Adlandır/sil ve 2 noktayla yerleştirme yok → N-13 |
| Zengin Metin (tablo, resim) | 217385179 | çok satırlı METİN | ◐ | Tablolu/resimli metin yok; paket yok (L-02) |
| Excel Dosyası Yükle (çizime tablo) | 226664216 | `ÇIKTIÖĞE tur=tablo` (öznitelikten) | ◐ | Dış tablodan çizime tablo yok; paket yok |
| Obje Özellikleri (Genel/Görünüm/Nesne/Gönder; Değişiklikleri Uygula) | 217385175 | Öznitelikler paneli | ✓ | Satır satır hemen uygular (bilinçli fark) |
| Alan Sor (Ctrl+A; 2. proj.; Tamam alan üretir) | 217385205 | `core.measure_area` ALANÖLÇ | ◐ | İçine tıklayarak ölçme ve sonucu alan yapma yok → N-02, N-03 |
| XYZ Sor (2. proj., pafta adı, paftayı ekle, kopyala) | 217385203 | `core.coordinate` KOORDİNAT | ◐ | Pafta adı ve 2. sistem yok → N-17, G-01 |
| Cetvel (ardışık / ilk nokta sabit; semt, eğim, dX, dY) | 217385201 | `core.measure` ÖLÇ | ◐ | İlk nokta sabit kipi ve eğim yok → N-02 |
| Prizma (hatta göre dik ayak/dik boy sorgusu) | 217385199 | — | ✗ | → N-02 |
| Nokta Bulutu Alan Sor · XYZ Sor | 217391842, 217391838 | — | ✗ | → T-05 |
| Sil (sayı + onay) | 217385194 | `core.erase` SİL | ✓ | Onay `core.duzenleme.silme_onayi` ile |
| Hızlı Sil | 217385192 | seç + Del/⌫ | ✓ | |
| Çoklu Doğru Köşesi Sil | 217385190 | `core.vertex_delete` KÖŞESİL | ✓ | |
| Kaydır (referans → yeni yer; projeler arası) | 217385158 | `core.move` TAŞI | ✓ | Ad çakışması: KAYDIR = görünüm kaydırma → §5.4 |
| Hızlı Kaydır · Kopyala Kaydır | 217385158 | tutamak sürükleme, `core.copy` KOPYALA | ✓ | |
| Hizala (referans nesneye göre sol/sağ/üst/alt/orta) | 217385156 | — (`HİZALA` başka iş) | ✗ | → N-14, §5.4 |
| Biçim Boya (sınıf, tabaka, renk, kalınlık, hat tipi) | 217385154 | `core.match_style` STİLKOPYALA | ◐ | Katman ve sınıf aktarılmaz → U-04 katkısı |
| Düzenle (Edit) çarkı: nesneye/tutamağa göre seçenekler, harf tuşları | 217385152, 217394337 | tutamaklar + nesne bağlam sekmeleri | ◐ | Tutamakta seçenek listesi yok → N-09 |
| Edit › Tek Obje / Dokunan / Noktaları Dokunan | 217385152 | `core.vertex_move` KÖŞETAŞI `kaynak=` (ortak köşe) | ◐ | Dokunanları kendiliğinden bulma yok → G-05, N-09 |
| Edit › Paralel Kaydır (kenarı komşularıyla) | 217385152 | `islem.alan_duzenle` ALANDÜZENLE (alan hedefli) | ◐ | Mesafeyle kenar kaydırma yok → N-09 |
| Edit › Uzat · Yarıçap Değiştir · Yay Yap | 217385152 | `core.lengthen` UZUNLUK, tutamaklar, `core.break` KIR | ✓ | |
| Edit › Teğet Yap | 217385152 | — | ✗ | → N-10 |
| Obje Uzat (pencereyle toplu) | 217385151 | `core.stretch` ESNET, `UZAT` çitle | ✓ | |
| Limit Bul (katmana göre; Shift ile bozanları ayır) | 217385147 | `core.zoom` YAKINLAŞ KAPSAM | ◐ | Katman kapsamı ve kapsam bozanı bulma yok → N-01 |
| Yeniden Çiz (`*`) | 217385144 | — | — | Tuval iz bırakmıyor; gerekmez |
| Önceki Pencere (Alt+C, 30 adım) | 217385173 | — | ✗ | → N-01 |
| Pencere Büyüt · Küçült (Alt+Z) | 217385171 | fare tekerleği | ◐ | Pencereyle yakınlaştırma yok → N-01 |

### 2.4 Düzenle sekmesi

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| Toplu Obje Değiştir (ortak özellikler; uzunluk/yarıçapta aritmetik) | 217385435 | `KATMANAT`, `RENK`, `STİL`; panelde SEÇİM özeti | ◐ | → U-04 katkısı |
| Tabaka Değiştir (+ Kopyalama Modu) | 217385432 | `core.set_layer` KATMANAT, şeritteki katman kutusu | ✓ | Kopyalayarak başka katmana yok |
| Tabaka Kapat · Aktif · Kilitle · Aç (gösterilen nesnenin) | 217388026, 217388017, 217388020, 217388045 | Giriş ▸ Katmanlar: Etkin Yap, Gizle, Yalnız Bu, Kilitle/Aç | ✓ | Seçili nesnenin katmanına uygulanır |
| Bul Değiştir (joker kuralları) | 217385430 | `core.find_replace` BULDEĞİŞTİR | ◐ | `*` önek/sonek kalıbı yok → N-12 |
| Ayrıştır (kiriş dik boyu; iç/dış alanlara) | 217385428 | `core.explode` PATLAT | ✓ | Çok parçalıyı parçalara ayırmak → N-08 |
| Uzat/Kes (tek geçişte; Y ile ters) | 217385426 | `core.trim` BUDA / `core.extend` UZAT aileleri | ◐ | Tek geçişte "kısaysa uzat, uzunsa kes" yok → N-09 |
| Uzat | 217385425 | `core.extend` UZAT | ✓ | |
| Kes (kesilen daire yay olur) | 217385423 | `core.trim` BUDA | ✓ | Ad çakışması: KES = panoya kes → §5.4 |
| Serbest Kes | 217385422 | `core.break` KIR | ✓ | |
| Kır (başa/sona mesafeyle) | 217385397 | `KIR`, `BÖL yontem=mesafe` | ✓ | |
| Paralel (köşe yöntemleri, uç biçimi, tek taraf, asıl çizgiyi sil, kot modelden) | 217385395 | `core.offset` OFSET | ✓ | "Ucuna bağla" köşesi ve kot modelden yok → N-11, C-16 |
| Birleştir (Ucuna Bağla / Tek Obje Yap) | 217385394 | `YUVARLA` (0 yarıçap), `core.join` UÇUCA | ✓ | |
| Obje Böl (uzunluğa / sayıya göre) | 217385391 | `BÖL yontem=esit`, `core.divide` BÖLÜMLE | ◐ | Sabit uzunlukta ardışık bölme yok → N-09 |
| Çoklu Doğruya Çevir (otomatik takip, tabaka öncelikli, süzgeç) | 217385389 | `core.join` UÇUCA | ◐ | Kavşakta yön seçerek izleme yok → N-06 |
| Çizgi Yönünü Değiştir | 217385387 | `ÇİZGİDÜZENLE islem=ters` | ✓ | |
| Köşe Yuvarlat (yarıçap; referans daireden; ayrık nesneler) | 217385386 | `core.fillet` YUVARLA | ✓ | Referans daireden yarıçap almak yok |
| 2 Noktadan Dönüşüm | 217385410 | `core.align` HİZALA | ✓ | |
| Aynala | 217385408 | `core.mirror` AYNALA | ✓ | |
| Döndür (Z ile ikinci referans) | 217385406 | `core.rotate` DÖNDÜR `yontem=referans` | ✓ | |
| Ölçekle | 217385404 | `core.scale` ÖLÇEKLE | ✓ | |
| XY Yönünde Ölçekle | 217385402 | `ÖLÇEKLE carpan_y=` | ✓ | |

### 2.5 Analiz sekmesi

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| Overlay | 217385482 | — | ✗ | → G-10 |
| Çevreleyenden Bilgi Al (ilk değer, sayı, toplam, ortalama; tampon) | 217385480 | — | ✗ | → G-10 |
| İçindekinden Bilgi Al | 217385478 | — | ✗ | → G-10 |
| Kolon Doldur (makro ya da değer) | 217385476 | tablodaki "alan hesaplayıcı" (henüz soluk) | ✗ | → G-03 |
| Tampon (baş stili; normal/birleşim) | 217385474 | `islem.tampon` TAMPON | ✓ | |
| Çoklu Tampon | 217385472 | — | ✗ | → G-10 |
| Voronoi Analizi · Yoğunluk Analizi (NC6) | 217388320, 217388318 | — | ✗ | → G-10, G-11 |
| Analist · Network · Koridor · NetHydro · Zaman Gezgini | 217385364, 217384826, 217388954, 217391602, 217391165 | — | — | Uzmanlık modülleri; bu planın dışı |

### 2.6 Araçlar sekmesi

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| Topoloji › Birleştir (ortak özelliğe göre, yakınlıkla) | 217385491 | — | ✗ | Dissolve → G-10 |
| Topoloji › Çizgileri Basitleştir | 217385489 | `ÇİZGİDÜZENLE islem=sadelestir` | ◐ | Nesne başına; ortak sınırı koruyan toplu hâli yok → G-05 |
| Topoloji › Düzelt (Kesişim) | 217385487 | `BÖL yontem=kesisim` | ✓ | Tolerans parametresi yok |
| Topoloji › Düzelt (Son Nokta) | 217385486 | `core.topology` TOPOLOJİ (yalnız rapor) | ◐ | Uçları toleransla buluşturan onarım yok → G-05 |
| Topoloji › Düzelt (Uzat-Kes) | 217385453 | — | ✗ | → G-05 |
| Topoloji › Otomatik Alan Kapat | 217385451 | `islem.alan_uret` ALANÜRET | ✓ | Kendiliğinden kapatma bilerek kapalı (`bosluk=`) |
| CAD'e Çevir Etiket Üret | 217385447 | `core.label` ETİKET | ✓ | Tek model: CAD/CBS ayrımı yok |
| Geometri Kontrol (tekrarlanan nokta, kesişen halka, geçersiz Z) | 217385444 | `core.topology` TOPOLOJİ, `core.cleanup` TEMİZLE | ✓ | Z denetimi N-05 ile gelir |
| Geometri Toplamları | 217385442 | panelde SEÇİM toplamları, tablo istatistikleri | ✓ | |
| Netcad MDB'ye Çevir | 217385437 | — | — | Özel biçim; GPKG karşılar |
| Otomatik Register (taranmış paftaları toplu oturtma) | 217386764 | — | ✗ | → G-08 |
| Veri Karşılaştır | 217385463 | — | ✗ | → N-23 |
| Veri Aktar · Hızlı Veri Aktar (anahtarla güncelle/ekle) | 217385459, 217385461 | `core.database` VERİTABANI `katmanyaz` | ◐ | Tabloyu bütün yazar; anahtarla satır güncellemesi yok → I-04, N-23 |
| Veritabanı Yönetimi (bağlantılar, SQL komutu) | 217385457, 217385553 | `core.database` VERİTABANI (PostGIS) | ◐ | SQL penceresi yok; öteki VTYS'ler bilerek yok (Article 2.9) |
| Tablo İlişkileri ve Kural Tanımla (1-1, 1-N; kurallar) | 217385499 | — | ✗ | → G-03, G-04 |
| Tablo Kolonu Değer İfadeleri (alan, çevre, merkez, Z min/max, kullanıcı, tarih…) | 217385510 | `BAĞLA bicim={#alan}`, `ETİKET` | ◐ | Hesaplanan sütun yok → G-03 |
| LRS (doğrusal referans) | 217385513 | — | ✗ | → T-04 |
| Obje Aktar (CAD → GIS) | 217386768 | `core.database` VERİTABANI `katmanyaz` | ✓ | Tek model |
| Mimar (iş akışı tasarımcısı) | 217385530, 217385528 | JSON/Python betik, `core.job_template` İŞŞABLONU | ◐ | Görsel model tasarımcısı yok → G-12 |
| Makro Düzenleyici | 217385635 | `core.python` PYTHON (konsol ve editör) | ✓ | |
| Makro Menü Düzenleyici (makroyu şeride ekle) | 217385711 | — | ✗ | → A-06 |
| Karo Oluşturucu | 217388118 | — | ✗ | Paket yok (sunucu tarafı işi) |
| Yön Bulucu | 217388170 | — | ✗ | Paket yok (düşük değer) |
| OGC Katalog Web Servisleri İstemcisi | 217388154 | — | ✗ | → G-09 |

### 2.7 Detaylar sekmesi

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| Kartezyen Dizi | 217385833 | `core.array` DİZİ | ✓ | |
| Obje Üzerinde Dizi (aralık, başlama, sapma; `??KM`/`??U`/`??X`/`??Y`/`??N`) | 217385830 | `DİZİ mod=YOL`, `core.divide` BÖLÜMLE | ◐ | Eksenden sapma ve km/mesafe yazısı yok → N-18 |
| Karelaj Üret (grid sembolü ya da nokta; Z DEM'den; ad/kod) | 217385826 | `ÇIKTIÖĞE izgara=arti` (yalnız çıktıda) | ◐ | Çizimde karelaj nesnesi yok → N-18 |
| Alan Taramaları (Tara / Çoklu Tara; içteki yazılar hariç) | 217385786 | `core.hatch` TARAMA | ✓ | İçteki yazıyı taramadan ayırmak → N-03 |
| Bina Taraması · Bina Kenar Taraması | 217385784, 217385781 | `STİL tip=tarak-cizgi` | ◐ | Stil ile çizilir; ölçüleri BÖHHBÜY verisi [M] → G-06 |
| Merdiven Taraması | 217385779 | — | ✗ | → N-19 |
| Cephe Yaz | 217385773 | `islem.uzunluk_yaz` UZUNLUKYAZ | ✓ | |
| Alinmana Cephe Yaz | 217385077 | — | ✗ | → N-16 |
| Ara Mesafe Yaz | 217385076 | — | ✗ | → N-16 |
| Uzunluk Yazdır (toplam uzunluk; eğik mesafe) | 217385770 | `islem.uzunluk_yaz` (kenar kenar) | ◐ | Toplam uzunluk ve eğik mesafe yok → N-15, N-05 |
| Yatay · Dikey · Paralel Ölçülendir · Paralel Ölçülendir (Yay) | 217385809, 217385805, 217385803, 217385801 | `core.dimension` ÖLÇÜ (`dogrusal`, `hizali`, `yay`) | ✓ | |
| Açı Ölçülendir · Yarıçap Ölçülendir | 217385799, 217385796 | `ÖLÇÜ tur=acisal`, `tur=yaricap` | ✓ | |
| Dik Düş (röleve: dik boy ve ayak yazılır) | 217385075 | — (`DİKAYAK` yalnız nokta koyar) | ✗ | → N-16 |
| Paralele Dik Hat | 217385073 | — | ✗ | → N-16 |
| Poligon Hattı · Poligon Kaydır · Poligon Hattı (Yön) | 217385071, 217385069, 217385067 | — | ✗ | → N-16 |
| Kutur Bağla | 217385789 | `core.dimension` ÖLÇÜ | ◐ | Kutur yazı biçimi yok → N-16 |
| Koordinat Yaz (NC6) | 217388750 | `ÖLÇÜ tur=koordinat` | ◐ | Köşelere toplu Y/X yazımı yok → N-15 |
| Etkileşimli Etiket (şablon, `??` değişkenleri) | 217385084 | `core.label` ETİKET, `islem.bagla` BAĞLA `bicim=` | ◐ | Koordinat değişkeni ve şablon kitaplığı yok → N-15, G-07 |
| Etiketleri Üret (dinamik → kalıcı) | 217385083 | `core.label` ETİKET (doğrudan nesne üretir) | ✓ | Dinamik etiket ayrı iş → G-07 |
| Grid (akıllı pafta çerçevesi, kenar yazıları) | 217385081 | `ÇIKTIÖĞE izgara=` | ◐ | Çıktıda var; çizimde akıllı nesne değil → N-18, L-01 |
| Lejant (dinamik; MPYY şablonları) | 217385079 | `ÇIKTIÖĞE tur=lejant` | ✓ | MPYY şablonları [M] → L-01 |
| Ölçek Çubuğu | 217385078 | `ÇIKTIÖĞE tur=olcek` | ✓ | |

### 2.8 Görünüm sekmesi

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| 2 Pencere Aç (farklı ölçekte iki görünüm) | 217385050 | — | ✗ | → N-24 |
| Ekran Sakla (resim / pano) | 217385050 | — | ✗ | → N-24 |
| Özelleştirilebilir araç çubuğu (8.6) | 217385050 | — | — | Şerit tek yüzeydir (ui.md R46) |
| Arka Plan Rengini Seç | 217385048 | Tema (gece/gündüz) | ◐ | Serbest zemin rengi yok (bilinçli: jetonlar) |
| Renk Modu (gri/tek renk) · Taramalar Renkli · Saydamlığı Kapat | 217385048 | — | ✗ | → N-24 |
| Hızlı Eğri Çiz | 217385043 | — | ✗ | → T-02 |
| Alan Taramaları · Alan Sınırları (aç/kapat) | 217385043 | — | ✗ | → N-24 |
| Balastrolar | 217385043 | `STİL yerlesim=ilk/son` işaretçisi | ◐ | Çizgi dairede kırpılmaz; ölçüleri BÖHHBÜY [M] → G-06 |
| Obje İpuçları (üzerine gelince) | 217385043 | — | ✗ | → N-24 |
| Notlar (durumlu, yanıtlı inceleme notları) | 217385043 | — | ✗ | → N-25 |
| Etiket Ayarları (nokta adı/kot/kod, alan adı, çizgi kotu; dinamik) | 217385037 | — | ✗ | → G-07 |
| Kalın Obje Çizimi | 217385035 | durum çubuğunda KALINLIK | ✓ | |
| Kalemleri Kullan (kalem tablosu) | 217385035 | — | ✗ | → G-06 |

### 2.9 Hesap modülü

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| Nokta Editörü (süzgeç, F3/F4/F5, sırala, çift ayıkla, adlandır, sıralı no, kolon işlemleri, modelden kot, ondalık yuvarla, bağlı çizgiler) | 217389312 | öznitelik tablosu (F6), `core.points` NOKTALAR | ◐ | → S-01 katkısı, N-07 |
| Pafta Editörü (ülke/mevzi indeksi, tek tek ve otomatik paftala, `.PAF`) | 217389338 | — | ✗ | → N-17 [M] adlandırma kuralı |
| Dönüşümler › Projeksiyon | 217384998 | `core.reproject` DÖNÜŞTÜR | ✓ | |
| Dönüşümler › N Noktadan Helmert (uyuşum testi, M0) | 217384998 | `core.fit` OTURT (artıklar, RMS) | ◐ | Nokta çıkarınca M0'ın değişimi yok → S-02 katkısı |
| Dönüşümler › N Noktadan Afin · Helmert Matrisinden | 217384998 | — | ✗ | → S-02 katkısı |
| Kesişim 4 Nokta · Kesişim 2 Kenar | 217389329, 217389339 | `core.intersect_point` KESİŞİMNOKTA | ✓ | Nokta adı/kotu → S-01 |
| Yan Nokta Hesabı | 217389335 | `core.perp_offset` DİKAYAK | ✓ | İşaret kuralı aynı: sağ pozitif (29 Eylül; Açık soru 3) |
| Eğik Kenar–Düşey Açı · Yatay Kenar Kot Farkı · Yatay Kenar Düşey Açı · Takeometrik (etkileşimli) | 217389337, 217389328, 217389333, 217389336 | `core.survey_polar` ALIM (2B) | ◐ | Düşey açı, alet/reflektör yüksekliği ve kot yok → S-02, N-05 |
| Takeometrik Hesap (MIR) · Yatay Kenar (YDE; total station oku) · Prizmatik Hesap (PRZ) | 217389341, 217389332, 217389285, 217389278 | — | ✗ | → S-01 katkısı (saha verisi) |
| Netveri Koordinat Editörü (total station oku/yaz, koordine özet) | 217389281 | `core.points` NOKTALAR (metin listesi) | ◐ | Alet biçimleri yok → S-01 |
| Enkesit Editörü | 217389314 | — | ✗ | → T-03 |
| Poligon Hesabı (güzergah, dallanma, kurum ve güzergah türü) | 217389326 | `geodesy.traverse` POLİGON (`sinif=` katalogdan) | ◐ | Tek güzergah; ağ ve karne yok → S-02 [M] tolerans |
| Karne Editörü (rasat, Gauss, özet; indirgemeler) | 217389311 | — | ✗ | → S-02 [M] |
| Kanava Çizimleri | 217389334 | `POLİGON cizgi=evet` | ◐ | Kanava biçimi yok → S-02 |
| Otomatik Nokta Üret (köşelere sıralı nokta) | 217389316 | `islem.kose_numarala` KÖŞENUMARALA (yazı), `core.object_points` | ◐ | Köşeye nokta nesnesi yok → S-01 katkısı |
| Zemine İndirgeme | 217389283 | — | ✗ | → S-02 |
| Geriden Kestirme | 217394096 | — | ✗ | → S-02 katkısı |
| İleriden Kestirme | 217389331 | `KESİŞİMNOKTA yontem=dogrultu`, `kes(A,a1,B,a2)` | ✓ | |
| Koordine Özet (ekrana/dosyaya; 2. proj.) | 217394043 | `NOKTALAR yon=yaz`, `ÇIKTIÖĞE tur=tablo` | ◐ | Çizimde koordinat tablosu yok → N-15 |
| Alan Çıktıları (özet, cepheli, koordine özetli, yanılma/tecviz) | 217389310 | `core.layout` rapor/atlas | ◐ | Biçim ve tecviz [M] → L-03, S-03 |
| Aplikasyon Çıktıları (koordinat, kot, açı, mesafe, semt) | 217389330 | `core.stakeout` APLİKASYON | ✓ | Kot yok → N-05 |
| Prizmatik Aplikasyon (+ çizim) | 217389282 | — | ✗ | → S-02 katkısı |
| Hesap Genel Parametreler | 217394846 | Seçenekler | ✓ | |

### 2.10 Eski Komutlar ve Gps

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| 4.Köşeyi Oluştur | 217385354 | — | ✗ | → N-13 |
| Bina Oluştur (iki köşe + derinlik) | 217385353 | `DİKDÖRTGEN yontem=3n` | ◐ | Sayısal derinlik yok → N-13 |
| Şev Grupla | 217385098 | — | — | Eski komut |
| Gps: Başlat · Aplikasyon Modu · Koordinat Al · Sürekli Koordinat Al · Ayarlar · Konumu Göster | 217386907, 217388181, 217388164, 217388151, 217388155, 217388146, 217388140 | — | ✗ | → N-28 |

### 2.11 Netsurf modülü

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| Şev Tara (bozuk, dere içi, normal, tümsek; höyük; kokurdan) | 217385372 | — | ✗ | → N-19 [M] aralıklar |
| 2 Doğru Arasını Tara | 217385376 | — | ✗ | → N-19 |
| Şevi Tekrar Tara · Ölçek Değiştir · Şev Kenar Bul | 217385335, 217385322, 229905063 | — | ✗ | → N-19 |
| Üçgen Oluştur (Z ve kenar filtresi, kırık hat, dış sınır, delik, içbükey sınır) | 217385374 | `EŞYÜKSELTİ`/`HACİM` içinde geçici üçgenleme | ◐ | Kalıcı yüzey yok → T-01 |
| Eğrilerden Üçgen · Karelaj (grid DEM) · Voronoi | 217385374 | — | ✗ | → T-01, G-10 |
| Üçgen Analizi (kot farkı, kot düzenle, hatalı kotları bul) | 229905074 | — | ✗ | → T-01 |
| Model Birleştir (sınır bul, model kes, model noktaları, platform ekle, boşluk doldur) | 229905076 | — | ✗ | → T-01 |
| Model Düzelt (kırık hatla, nokta ekle/sil, üçgen ekle/döndür) | 217385375 | — | ✗ | → T-01 |
| Model Kontrolü (yırtık bul, onar) | 329154828 | — | ✗ | → T-01 |
| Hacim Hesapla (enkesitten TCK, ortalama alan, prizmoidal, DSİ; iki yüzey prizmatik; tabana göre) | 217392988 | `core.earthwork` HACİM (tek kota göre) | ◐ | İki yüzey ve enkesit yöntemleri yok → T-03 [M] kurum yöntemi |
| Eğri Geçir (Zmin/Zmax/aralık, 25 m ayır, basitleştir) · Eğrilere Kot Yaz | 217385338 | `core.contour` EŞYÜKSELTİ | ◐ | Ana/ara eğri, kot yazısı, bölge yok → T-02 |
| Eğri Temizle · Eğri Temizle (Oto) · Eğri Alanı Hesapla | 217385356, 217385370, 217387758 | — | ✗ | → T-02 |
| Güzergah Tanımla (km; elemanlar/someler) | 217385340 | — | ✗ | → T-04 |
| Enkesit Al · Profil Çizimi · Hızlı Profil · Enkesitten Kübaj/Plankote | 217393227 | — | ✗ | → T-03 |
| LandXML Oku · Rasterdan Üçgen/Nokta · SHP'den Kot/Model · E00 Kot | 229905436, 229905438, 229905441, 229905443, 229905445 | — | ✗ | → T-01, G-11, I-01 |

### 2.12 Netmap, Netçap, Netkamu, Nettop (Alanları Basitleştir dışında hepsi [M])

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| Yeni Proje · Proje Parametreleri · Tapu Sözel Verileri · Veritabanı Düzenle | 217385905, 217385864, 217385881, 217386112 | — | ✗ | [M] → S-03 |
| Ada · Parsel · Parsel Köşe Noktası · Yapı · Mahalle · İrtifak Hakkı · Yer Kontrol Noktası | 217385845, 217385856, 217385846, 217385870, 217385871, 217385910, 217385904 | `core.area` ALAN + öznitelik | ◐ | Anlamlı sınıf kalemi yok → G-04 [M] |
| Parsel Editörü (işlemler; sağ tuş: malik, pay, geçmiş) | 217385857, 217384907, 217384906 | öznitelik tablosu | ◐ | [M] → S-03 |
| Alan Düzeltme (serbest, klavyeden, paralel, köşe; tapu alanı) | 217385878 | `islem.alan_duzenle` ALANDÜZENLE | ◐ | Geometri var; tapu alanı ve pasif parsel [M] → S-03 |
| Alanları Basitleştir | 217386113 | `ÇİZGİDÜZENLE islem=sadelestir` | ◐ | Ortak sınırı koruyan toplu hâli → G-05 |
| Yol Dengelemesi | 217385903 | — | ✗ | Geometri N-27; kural [M] |
| Birleştir (tevhit) | 217385906 | `core.merge` TEVHİT | ◐ | [M] kullanıcı tarif edecek |
| Ayır / İfraz (dik, paralel, serbest, sabit noktadan, cephe-açı, paralel mesafe, iç alan, kalan) · Klasik İfraz | 217385885, 217385862 | `core.split_parcel` İFRAZ, `core.split_area` ALANİFRAZ | ◐ | Yöntemlerin çoğu yok [M] → S-03 |
| Cins Değişikliği · Yola Terk · Yoldan İhdas | 217386114, 217385854, 217385873 | — | ✗ | [M] → S-03 |
| Ada Etrafında Adlandır · Otomatik Köşe No Yaz · Nokta Tabakalandır | 217385887, 217385877, 217385868 | `islem.kose_numarala` KÖŞENUMARALA | ◐ | Adlandırma kuralı [M] → S-03 |
| 2B Ayır · 2B/Orman Uygulama Sınırı · İmar Uygulama Sınırı | 217385860, 217385858, 217385849, 217388200 | — | ✗ | [M] → S-03 |
| Belge ve Raporlar (krokiler, beyannameler; TKGM 2025/4) | 217385879 | çıktı yerleşimi + atlas | ◐ | [M] → L-03, S-03 |
| Dağıtım (18. madde) ve "Dağıtım" bağlam sekmesi | 217385208, 217393460 | — | ✗ | [M] → S-04 |
| NETÇAP (çap verisi, çizelge, belge) | 217385899, 217385943 | — | ✗ | [M] → S-03 |
| NETKAMU (kamulaştırma sınırı, güzergah, beyanname, ABC) | 217384961 | — | ✗ | [M] → S-04 |
| NETTOP (toplulaştırma, endeks, dağıtım, DOP) | 217384999 | — | ✗ | [M] → S-04 |

### 2.13 Planet, Netpro, 3D+ ve öteki modüller

| Netcad aracı | pageId | KentOSCad karşılığı | Durum | Not |
|---|---|---|---|---|
| Yol Çiz (Aksis / Paralel) · Yaya Yolu · Bisiklet Yolu · Kalınlık Ver | 320014438, 320018799 | — | ✗ | Geometri → N-27; genişlik ve kalınlık [M] |
| Kavşak Oluştur · Ada/Kaldırım Köşesi · Köşe Düzelt · Refüj Kapat · Yol Yuvarlat | 320014486, 320015541, 320015550, 320015556, 320015562 | `core.fillet` YUVARLA, `core.chamfer` PAH | ◐ | Yol ağına toplu araç yok → N-27 [M] |
| Semboller (yerleşim, yapılaşma, yoğunluk, plan notu, alan sembollerini ve fonksiyon adlarını üret) | 320014255 | `STİL` + MPYY gösterim kataloğu, `SEMBOL` | ◐ | [M] → S-04 |
| Hesap (alan dağılımı, donatı, nüfus, DOP, yürüme mesafesi) | 320014255 | — | ✗ | [M] → S-04 |
| PlanGML'e Aktar · PlanGML'den Aktar | 320020586 | — | ✗ | [M] → S-04 (domain.md R18) |
| 3D Simülasyon | 320016527 | — | ✗ | → N-29 |
| NETPRO (yatay/düşey güzergah, enkesit, kübaj, kavşak; dinamik proje) | 217386111 | — | ✗ | → T-04 |
| NETCAD/3D+ (3B görüntüleme, doku, 3B'de ölçüm) | 217389385 | — | ✗ | → N-29 |
| DRONET · EPlanet · YAPINET · Water · Atıksu · Mine · EXCANET · RASVEK | ağaç: 217394193, 217394243, 217394244 | — | — | İncelenmedi; bu planın dışı |

## 3. İş paketleri

Her paket bir commit'lik iş olacak biçimde kesildi; sıra, her paketin öncekinin üstüne kurulduğu sıradır
(§8). Her pakette CLAUDE.md'nin ortak şartları geçerlidir ve bir daha yazılmaz: komut `KENTOS_COMMAND` ile
bir kez bildirilir, adlar R7 sırasında (Türkçe, ASCII, İngilizce, kısaltma) ve çapraz kayıt çakışma
kapısından geçer; her parametrenin İngilizce adı (`Param::en`, 6.15); `docs/komutlar/<slug>.md` sekiz
bölümüyle ve `docs/README.md`'den bağlı (5.17, docs.md R7–R8); arayüz = komut satırı = JSON betik eşitlik
kanıtı ve günlük tekrarı (6.4, test.md R3); iptal testi boş geri alma farkıyla; `make reference` ile altı
üretilmiş belge aynı commit'te (6.14); sayısal değişiklikte altın fikstür üç platformda aynı (6.5);
yönetmelik değeri yalnız `/data`'da (5.13); `tr()` ve iki `.ts` dosyası (6.9). Bir işlem aracı
`processing.md` R1–R17'ye uyar. Şerit yeri, yeni düğmenin §5'teki düzende duracağı yerdir.

### Aşama A — Netcad eli: gezinme, sorgu, girdi (P0)

- [x] **N-01 · P0 — Görünüm gezinme: pencereyle yakınlaş, önceki/sonraki görünüm, seçime ve katmana yakınlaş, kapsamı bozanları bul.**
  **Durum (29 Eylül 2026):** dört commit — görünüm geçmişi (12b363b), PENCERE/MERKEZ ve Alt+Z (4be34c4), SEÇİM/KATMAN ve "Katmana yakınlaş" (c633be2), KAPSAMDENETİM. İncelemenin düzeltmeleri uygulandı: PENCERE ve MERKEZ yeni kancadan (`Bus::on_view_move`, `ViewMove`/`ViewMoved`) geçer; YAKINLAŞ soru sormaz, köşeleri tuval hareketi toplar; geçmiş Qt'siz (`render::ViewHistory`) ve tekerlek dizisini tek adıma katlar; Alt+Z ve Alt+C boştu, atandı; KAPSAMDENETİM **raporlar ve işaretler, seçmez** — seçen ve taşıyan satırı yazar, çalıştırmak kullanıcıya kalır. Kopukluk kuralı bir mesafe eşiği değil, boş bant: yavaş seyrekleşen çizimde yanlış alarm vermez. Kabuldeki "altın çizim" bir birim sınamasıdır (`test_detached.cpp`, 10 000 parsel + 3): altın dosyalar belgeyi yazar, salt okunur bir denetimin cevabını değil.
  **Netcad:** Pencere Büyüt/Küçült (Alt+Z) 217385171; Önceki Pencere (Alt+C, 30 adım) 217385173; Limit Bul katman menüsünden, Shift+Limit Bul kapsamı bozan nesneleri "HATALI" tabakasına taşır 217385147; komut satırında `SET ZOOM LIMITS`, `SET ZOOM <Y,X dy,dx>` 217386603.
  **Bugün:** `core.zoom` YAKINLAŞ yalnız `KAPSAM | ÇARPAN | SIFIRLA`; görünüm geçmişi yok (`zoom.md`, `pan.md` "önceki görünüme dönmek için KAPSAM ya da SIFIRLA").
  **Tasarım:** `YAKINLAŞ`'a kipler: `PENCERE` (iki köşe), `ÖNCEKİ` / `SONRAKİ` (oturumun görünüm geçmişi, 30 adım; görünüm durumudur — belgeye, geri alma yığınına ve günlüğe belge satırı olarak girmez, model.md R43), `SEÇİM`, `KATMAN katman=`, `MERKEZ merkez= olcek=`. Yeni salt okunur `core.extent_check` — `KAPSAMDENETİM`, `KAPSAMDENETIM`, `EXTENTCHECK`, `KPD`: çizimin çoğunluğundan kopuk nesneleri (yanlış sistemle gelmiş, sıfıra düşmüş) **bulur, seçer, işaretler; taşımaz** — Netcad'in HATALI tabakasına kendiliğinden taşıması yerine karar kullanıcıda (`KATMANAT` ile tek adım). Eşik bir `SettingSpec`.
  **Şerit:** Görünüm ▸ Gezinme: Pencere, Önceki, Sonraki, Seçime; Giriş ▸ Görüntü (N-20); Katmanlar panelinde satır menüsüne "Katmana yakınlaş". Kısayollar Alt+Z (pencere), Alt+C (önceki) — boş oldukları doğrulanarak.
  **Kabul:** `YAKINLAŞ PENCERE pencere=… pencere=…` beklenen görünümü verir; 31 görünüm değişikliğinden sonra `ÖNCEKİ` 30 adım geri, `SONRAKİ` ileri gider; hiçbir adım `content_hash` ya da günlükte belge satırı üretmez; 10 000 parsel ve 3 uzak nesnelik altın çizimde `KAPSAMDENETİM` tam 3 nesne bildirir ve hiçbirini değiştirmez; üç istemci aynı `report`u verir; `zoom.md` güncellenir, `extent_check.md` yazılır.
  **Bağımlılık:** yok.
  **3B:** Geçmiş kaydı bugün 2B pencere; kamera durumu (N-29) eklenebilecek biçimde tutulur.

- [x] **N-02 · P0 — Sorgu ekleri: sabit ilk noktalı ölçüm, dik ayak/dik boy sorgusu, ölçülen alanı alan olarak çizmek.**
  **Durum (29 Eylül 2026):** PRİZMA (c5cd302), `ÖLÇ sabit=evet` (4bf70d1) ve "Alan olarak çiz" yapıldı. İncelemenin dediği düğme yeri yeni bir yolla açıldı: komut bittiğinde bir sonraki adımı önerir (`command::Offer`, `Context::offer`, `DispatchResult::offer`), kabuk onu tuvalin üstünde tek düğmeli bir şeritte gösterir ve düğme satırı baştan sona tek seferde çalıştırır (`Controller::runWhole`; yazılmış bir ALAN beşinci köşeyi beklemez). KAPSAMDENETİM de aynı yoldan **Seç** sunar. İki sapma: (1) istemde `S` ile ardışık↔sabit geçişi yapılmadı — `S` SEÇ'in kısaltması (istemde yazılan komut adı soruyu kapatır) ve istemin seçenek taşıyan bir yapısı yok; önce istem seçenekleri gerekir. (2) Giriş ▸ Sorgu paneli eklenmedi: Giriş 1382 piksel, dört düğmelik bir panel onu 1440'ın üstüne çıkarır (docs/baslangic/arayuz.md'nin sığma kuralı); sorgular Harita ▸ Sorgu ve Ölçüm'de, Netcad adları (`ALANSOR`, `XYZSOR`, aramada `CETVEL`) ise komut satırında. Giriş'in yerleşimi değişecekse bu bir karar ister.
  **Netcad:** Cetvel "İlk Nokta Sabit" (Alt ile geçiş), semt, eğim, dX/dY, kopyala 217385201; Prizma: iki noktalı hatta göre dik ayak ve dik boy, noktanın adı 217385199; Alan Sor tamamlanınca alan nesnesi üretir 217385205.
  **Tasarım:** `core.measure` ÖLÇ'e `sabit=evet` (her yeni nokta ilk noktaya ölçülür; istemde `S` seçeneğiyle ardışık↔sabit geçişi). Yeni salt okunur `core.station_offset` — `PRİZMA`, `PRIZMA`, `STATIONOFFSET`, `PRZ`: `baslangic`, `bitis` (taban), `noktalar` → her nokta için dik ayak, dik boy, taban uzunluğu ve varsa `nokta_no`; işaret kuralı `dik()` ile **aynı** (Açık soru 3); yapılandırılmış `report` ve tuvalde ölçü işareti (`command/measure_mark.hpp`). ALANÖLÇ salt okunur kalır: sonuç satırındaki **Alan olarak çiz** düğmesi aynı köşelerle `ALAN noktalar=…` gönderir (komut satırından ve betikten de aynı satır yazılır — istemci ayrıcalığı yok).
  **Şerit:** Giriş ▸ Sorgu paneli (Nesne Bilgisi, Alan Ölç ▾, Koordinat Oku, Ölç ▾ [sabit · prizma]) — Netcad kullanıcısı Alan Sor, XYZ Sor ve Cetvel'i Giriş'te arar (217385177).
  **Kabul:** taban (0,0)→(100,0), nokta (30,5): ayak 30,000 m, boy işaret kuralına göre ±5,000 m; üç istemci aynı `report`; `ÖLÇ sabit=evet` ile iki kenarın ikisi de ilk noktadan ölçülür; "Alan olarak çiz" tek geri alma adımı ve günlükte düz bir `core.area` satırı; Python adı `station_offset`.
  **Bağımlılık:** yok; eğim sütunu N-05'ten sonra.
  **3B:** Eğim (%) ve eğik mesafe tepe kotuyla gelir; `report` şeması `egim` alanını şimdiden taşır (kot yoksa "kot yok", sıfır değil).

- [x] **N-03 · P0 — İçine tıklayarak alan: bölge girdisi ve "noktayı içeren" seçimi.**
  **Durum (29 Eylül 2026):** `ALANÖLÇ yontem=ic` yapıldı; incelemenin düzeltmeleriyle: `ctx.region` bir bekleyici yapılmadı, bölge `command::ask_region` ile sorulur (SINIR ile ALANÖLÇ aynı yolu, aynı ret sözlerini paylaşır; parametreler `yontem=ic nokta= ada= bosluk=`); bölge tıklaması yakalanmaz (`Prompt::aids`, `command::aids_for` — tuvalin işareti ile sonucun ayrışamadığı tek yer). `SEÇ İÇEREN` yapıldı (`core::pick_containing`: seçimin kendi iç kuralı, türün alanı, küçükten büyüğe; nokta SEÇ'in kendi `noktalar=`'ıyla verilir, ikinci bir nokta parametresi açılmadı; şeritteki yeri N-04'ün Seçim sekmesi). `TARAMA yontem=ic` yapıldı (bölgenin yüzü tarama halkaları olur, adaları delik; çizgilere bağlanmaz, çünkü bağ sınırı kapalı nesnelerden yeniden kurar — satırda ve sayfada söylenir); Çizim ▸ Tarama bir aile oldu. **Ertelenen:** içteki yazı ve sembollerin taramadan boş kalması — kendiliğinden mi, seçerek mi, Açık soru 18.
  **Netcad:** Alan Seçim Aracı: tıklanan noktayı çevreleyen alanı kendiliğinden çevirir; F2 Çevir (köşeleri göster), F3 Seç (var olan alan), F4 Gelişmiş (8.6; kesişen, çok kırıklı geometride iç alan) 217387115; Alan Sor ve Alan Taramaları bu araçla çalışır 217385205, 217385786; Boşluk ve `/` iç içe alanları küçükten büyüğe listeler 217387890.
  **Bugün:** `core.boundary` SINIR aynı hesabı bir **komut** olarak yapıyor (CGAL düzenlemesi, `core/planar.hpp`; adalar delik, yaylar yay); başka komutlar onu girdi olarak kullanamıyor.
  **Tasarım:** Tek girdi türü `ctx.region(...)` — cevap üç biçimde gelir: köşeler (F2), var olan kapalı nesne (F3), **iç nokta** (F4; SINIR'ın kodu). Komut gövdesi hangisinin geldiğine göre dallanmaz (command.md P10); günlüğe **çözülmüş sınır** yazılır, tıklanan nokta değil — yeniden oynatma el istemez. İlk kullanıcılar: `ALANÖLÇ yontem=ic nokta=`, `TARAMA ic=` (içteki yazı ve semboller ada kuralıyla dışarıda kalır — Netcad'in "Diğer Objeler Seç"i). `core.select` SEÇ'e `mod=İÇEREN nokta=`: noktayı içeren kapalı nesneler **küçükten büyüğe** (parsel ⊂ ada ⊂ mahalle), `sira=` ile hangisi.
  **Şerit:** Nokta Girişi bağlam sekmesinde "İçine tıkla" (N-04); Alan Ölç ailesine "içine tıklayarak"; Tarama ailesine "içine tıklayarak".
  **Kabul:** dört çizgiyle kapanan bölgede `ALANÖLÇ yontem=ic` = aynı köşelerle `yontem=nokta` (mm² eşit); delikli bölgede delik düşülür; açık uçlu bölgede ret cümlesi boşluğu SINIR'la aynı sözle söyler; iç içe üç alanda `SEÇ mod=İÇEREN sira=1` en küçüğü seçer; `test_proof.cpp` vakası; sayfalar.
  **Bağımlılık:** C-09 (kapandı), U-03.
  **3B:** Bölge planda çözülür; kotlu sınırda (N-05) kaynak köşeler kotunu korur, kesişimden doğan köşeye kenar boyunca doğrusal kot — kural `core.md`'ye yazılır.

- [ ] **N-04 · P0 — Bağlam sekmeleri: "Nokta Girişi" ve "Seçim" (Koordinat Hesap Makinası ve Seçim Süzgeci'nin karşılığı).**
  **Durum (29 Eylül 2026):** dört dilime bölündü. **N-04a yapıldı:** `SEÇ DAİRE | DIŞINDA | GEÇEN` (`core::pick_in_circle`, `pick_outside_box`, `pick_through`; DIŞINDA kutuyu yüzünde taşıyan alanı dışarıda saymaz, GEÇEN yüzü saymaz). **N-04b yapıldı:** `boyunca(nesne(k),mesafe[,sapma])` (`core::point_along`: çift duyarlıkta, tek yuvarlama; `Bus::object_path`, `ResolveContext::object_path`), fuzz tohumu ve koşumu. **N-04c yapıldı:** Seçim sekmesi (`kPromptSelectContextId`), düğmeler `command::select_modes()` tablosundan (SEÇ'in kip çözümleyicisi de aynı tablodan; `LAST`'in `NESNE`'ye düşmesi düzeldi); tıklama isteyen kipler **satır kurma** ile (`CommandLine::beginCompose`: tuval tıklaması satıra koordinat, `nesne(` sonrasında kimlik yazar; ui.md R48a); sekme öne gelmez (öne gelmesi çizim sekmesini elden alırdı). Eksik: kurulan satırın noktaları tuvalde iz olarak çizilmiyor, yalnız satırda — N-04d ile. Sırada: N-04d Nokta Girişi sekmesi — hesap paleti fonksiyon tablosundan üretilir (5.10), "Katmanı nesneden al"ın bugün komutu yok (çizim komutlarında `katman=` yok), önce karar.
  **Netcad:** Bir işlem nokta isteyince "Nokta Seçim Araçları" sekmesi açılır: yakalama modları; Koordinat Hesap Makinası (Koordinat, Nokta Adı, Mesafe–Açı/Eğim, dX–dY, Tabaka, Çizgi/Obje İzle, Kesişim, 2 Noktalı Kesişim, Dik-çık, Dik Düş, Teğet, Dik-abs, Paralel Nokta, Hat Üzerinde a/b, Obje Üzerinde, GPS, KM); Çizim Hesap Araçları 217386613, 217387852, 217386679, 217386622. Nesne isteyince "Seçim Süzgeci": Pencere/Alan/Daire; Kesenler/İçindekiler/Üzerindekiler/Dışındakiler; Doğruyu Kesenler, Noktayı İçerenler, Noktadan Geçenler; Tümü, Son Seçilenler; Bölgeden Kırp, Tersine, Grup; nesne tipleri 217387890.
  **Tasarım (KentOSCad yolu):** İki bağlam kategorisi, **yalnız istem sürerken** görünür ve istem bitince kaybolur (ui.md R48'e ek: bugün istem sürerken hiçbir düzenleyici sekmesi görünmüyor). **Her düğme bir satır yazar**, kendi yetkisi yoktur:
  - *Nokta Girişi:* yakalama anahtarları (`MOD yakalama_modları`); **hesap paleti** — her düğme bir nokta fonksiyonunu adım adım kurar ve komut satırına yazar (`dik(` → A'ya tıkla → B'ye tıkla → ayak ve boy yaz → `)`): `n()`, `orta()`, `dik()`, `kes()` (üç biçim), `ara()`, `uzanti()`, `semt()`, `xy()`. Yeni fonksiyonlar: `boyunca(nesne(<kimlik>), mesafe, sapma)` (Netcad Obje Üzerinde ve Paralel Nokta; çizgi, yay, daire ve yaylı çizgi boyunca, C-01 eğri sorgusuyla) ve nesne başvurusu `nesne(<kimlik>)` (çözümleme `n()` gibi çağıranın verdiği aramayla, command.md R17a); "İçine tıkla" (N-03); "Katmanı nesneden al" (komutun `katman=` parametresini tıklanan nesnenin katmanıyla doldurur; etkin katman değişmez; parametresi olmayan komutta soluk).
  - *Seçim:* `SEÇ` kiplerinin düğmeleri ve yeni kipler `DAİRE` (merkez + çevre), `DIŞINDA` (pencerenin dışı), `GEÇEN` (noktadan geçen), `İÇEREN` (N-03); tür çipleri (`tur=`); `islem=TERSİNE`. Bölgeden kırp ve grup bu pakette yok (grup kavramı yok; kırpma G-10).
  **Şerit:** iki bağlam kategorisi; renk bloğu yok, yalnız 3 px vurgu başlığı (design.md §2); 1440 px'e sığar; Tab zinciri; her düğmenin ipucu yazacağı satırı gösterir.
  **Kabul:** şerit ve araç probları iki sekmeyi bulur ve 0 kusur; `ÇİZGİ` sürerken paletten kurulan `dik(...)` ile elle yazılan aynı günlük satırını üretir; `boyunca()` 20 m yarıçaplı yayda 25 m, 3 m sapma el hesabıyla mm eşit; `SEÇ mod=DAİRE|DIŞINDA|GEÇEN` birer testle; yeni fonksiyonlar `komut-satiri.md` tablosunda ve ayrıştırıcı fuzz korpusunda (6.7); ui.md değişikliği aynı commit'te.
  **Bağımlılık:** N-03; U-02 (dinamik girdi alanları bu sekmeye yerleşir), U-03.
  **3B:** Palet okuması `Y · X · Z` gösterecek biçimde tasarlanır; N-05'ten sonra `boyunca()` isteğe bağlı kot farkı (`dz`) alır.

### Aşama B — Kotlu ölçü noktası ve 3B temeli

- [ ] **N-05 · P0 — Tepe noktasında kot (Z): depolama kararı, dosya bloğu ve göç (C-16'nın veri ön koşulu).** `[!]` bakımcı kararı bekler.
  **Netcad:** Z her yerde: nokta seçiminde model varsa Z enterpolasyonla okunur 217385088, 217385140; Paralel "Kotları Modelden Al" 217385395; Alan Birleştir/Çıkart "Kot Değerlerini Koru", Alan Böl "Kotları Kesenden Al" 217385090; Geometri Kontrol "geçersiz yükseklik" 217385444; Uzunluk Yazdır "Eğik Mesafe" 217385770; Cetvel eğim 217385201; Proje Düzenle "Kot Sıfırla" 217388066; Nokta Editörü "Çizgi Kotlarını Güncelle" 217389312.
  **Bugün:** `Point2` yalnız `x, y` (`core/units.hpp`); kot yalnız noktalarda `kot` öznitelik sütunu; EŞYÜKSELTİ ve HACİM onu okur (`contour_command.cpp`, `earthwork_command.cpp`).
  **Öneri:** `RingGeometry` yanında **isteğe bağlı tepe kotu sütunu** — tepe başına `Mm z`, "kot yok" ayrı bir durumdur (0 değil). model.md'nin "alan eklenir, anlam değişmez" kuralına uyar ama bir **veri göçüdür** (0.2a): R9a'daki gibi yalnız varsa yazılan dosya bloğu, `content_hash()`'e yalnız varsa katılır (kotsuz çizimin baytı, parmak izi ve bütün altın fikstürler aynı kalır); **kesme bloğuna girmez** (R6–R8 değişmez). `kot` sütunundan tek seferlik göç (eski dosya açılınca kotlar tepeye taşınır ve sayısı söylenir). io: DXF 3B çoklu çizgi ve POINT z, GPKG Z. core: plan uzunluğu ile 3B uzunluk ayrı işlevler; XY taşıma, döndürme, aynalama Z'yi korur (C-16 kabulü). Yeni komut `core.set_elevation` — `KOTVER`, `SETELEVATION`, `KTV`: `nesneler`, `yontem=sabit|artir|sifirla|oznitelik`, `deger=` (mm); `yuzey` yöntemi T-01'den sonra.
  **Şerit:** Değiştir ▸ Kot grubu (Kot Ver ▾: sabit · artır · sıfırla); Öznitelikler panelinde `Z` satırı ve 3B uzunluk.
  **Kabul:** kotsuz belge kaydet/aç: dosya baytları ve `content_hash` bu değişiklikten önceki sürümle aynı; kotlu nokta ve 3B çoklu çizgi kaydet/aç birebir; `kot` sütunlu eski dosya açılınca kotlar tepeye taşınır ve rapor edilir; DXF ve GPKG gidiş-dönüşünde Z korunur; TAŞI/DÖNDÜR/AYNALA Z'yi değiştirmez; `KOTVER yontem=sifirla` tek geri alma adımı; `model.md`, `io.md`, `core.md` kuralları ve dosya sürümü (`min_reader_version`) kararı aynı commit'te; altın `kot-tepe.txt`.
  **Bağımlılık:** Açık soru 2; sonra C-16, S-01, T-01.
  **3B:** 3B'nin temelidir; sonraki her kotlu iş bu sütunu okur.

- [ ] **N-06 · P1 — Çizerken izle (Obje İzle / Çizgi İzle; Çoklu Doğruya Çevir'in takibi).**
  **Netcad:** Çizgi İzle: iki nokta arasında çizgileri kendiliğinden sayısallaştırır; Obje İzle: başlangıçtan sonra CAD/GIS nesnelerinin üzerinde gezinince rota oluşur 217386679; Çoklu Doğruya Çevir: bağlı çizgileri kavşağa kadar izler, tabaka öncelikli 217385389.
  **Tasarım:** Nokta Girişi sekmesinde **İzle**: bir nokta dizisi sürerken (ÇOKLUÇİZGİ, ALAN, TARAMA sınırı, ALANÖLÇ köşeleri) imleç bir nesnenin üstündeyken sonraki tık, önceki noktadan tıklanana **nesnenin kendi yolunu** ekler; kavşakta en kısa kol, `Tab` öbür kol. Yazılı karşılığı yeni **liste fonksiyonu** `izle(nesne(<kimlik>), A, B)` — yalnız nokta listesi parametrelerinde, çözülünce köşe dizisine açılır (command.md R17a'nın "bir noktaya çözülür" kuralına liste karşılığı eklenir). Yolda yay varsa sonuç yaylı çoklu çizgi olmalıdır — kiriş yok; ÇOKLUÇİZGİ bugün yalnız nokta aldığı için ilk dilimde yaylı iz **reddedilir ve söylenir**, ikinci dilim N-10'un yaylı çizim girdisini kullanır.
  **Kabul:** ada sınırı izlenerek çizilen parsel kenarının köşeleri kaynağın köşeleriyle bayt bayt aynı; iki kollu kavşakta `Tab` kolu değiştirir; `izle()` yazılan satır ile fareyle izleme aynı günlük satırı; yaylı izde ret cümlesi; fuzz korpusu.
  **Bağımlılık:** N-04, C-01; ikinci dilim N-10.
  **3B:** İzlenen köşeler kaynak kotlarını taşır (N-05).

- [ ] **N-07 · P1 — Noktaya bağlı çizgi (köşe bağı): ölçü noktası taşınınca ona oturan köşeler izler.**
  **Netcad:** "Bir noktanın koordinatının değişmesi durumunda bağlı olan tüm çizgiler de değişecektir" 217385088; Nokta Editörü çıkışında "Bağlı Çizgileri Güncelle", "Çizgi Kotlarını Güncelle", "Nokta Kotlarını Güncelle (üçgen model)" 217389312.
  **Tasarım:** `core/ties.hpp`'ye beşinci bağ türü `TieKind::Vertex` ("köşe"): bir çizginin ya da alanın köşesi bir `core.point`'e **anahtarla** bağlıdır (model.md R1); bağ komut sonunda çözülür, kare başına değil (R46a deseni): nokta taşınınca ya da kotu değişince bağlı köşeler aynı adımda ve aynı geri alma kaydında yeni yere gider. Kurulması: `AYAR nokta_bagi evet` (proje kapsamı, R40) açıkken DÜĞÜM yakalamasıyla verilen köşe bağlanır; var olan çizim için `core.tie_vertices` — `NOKTAYABAĞLA`, `NOKTAYABAGLA`, `TIETOPOINTS`, `NYB`: çakışan köşe–nokta çiftlerini F-03 düğüm toleransıyla bağlar, `islem=coz` çözer. Kilitli katmandaki izleyici R46f gibi "gerisinde" kalır; `BAĞIMLILIK` yeni türü listeler; dosyada yalnız varsa yazılan kendi bloğu.
  **Kabul:** noktası taşınan üç parselin ortak köşesi üçünde de aynı mm'ye gider ve tek GERİAL hepsini geri getirir; kilitli katmanda "gerisinde" durumu; nokta silinince bağ "kopuk", köşe yerinde; kaydet/aç; DXF'e bağsız gider; `test_dependency.cpp`; `veri/bagimliliklar.md` güncellenir.
  **Bağımlılık:** F-04, S-01 (nokta kimliği), N-05.
  **3B:** Bağ kotu da taşır ("Çizgi Kotlarını Güncelle" varsayılan açık).

### Aşama C — CAD geometrisinde Netcad eşitliği

- [ ] **N-08 · P0 — Alan kesişimi, alan çıkarımı ve çok parçalı alan.**
  **Netcad:** Alan ▾: Alan Kesişimi, Alan Birleştir (kesişmeyenleri çok parçalı yapar, delikleri korur), Alan Böl (çizgisel nesne ya da alanla), Alan Çıkart (birden çok sonuç) 217385090; Çoklu Doğru Birleştir / Parçası Çıkar 217385099, 217385096; Ayrıştır "İç/Dış Alanlara" 217385428; 8.6 sürüm notlarında bu işlemler geliştirilmiş 217386522.
  **Bugün:** `kernel_boolean` (birleşim, kesişim, fark) çekirdekte var (O-1) ama komutu yok; `BİRLEŞTİR` ayrık parçaları ayrı nesne bırakıyor; hiçbir komut çok parçalı alan üretmiyor (destek matrisinde "Çok parçalı alan" satırı ⊘).
  **Tasarım:** `core.area_intersect` — `ALANKESİŞİMİ`, `ALANKESISIMI`, `INTERSECTAREAS`, `AKŞ`, `AKS`; `core.area_subtract` — `ALANÇIKAR`, `ALANCIKAR`, `SUBTRACTAREAS`, `AÇR`, `ACR` (`nesne` kesilen, `kesen` çıkarılanlar, `kesen=koru|sil`); `BİRLEŞTİR parca=tek|ayri` (ayrık parçalar tek kimlikli çok parçalı nesne); `BÖL kesen=<nesne>` (var olan çizgi ya da alanla); `PATLAT yontem=parca` (çok parçalıyı parçalarına, deliği ayrı alana). Hepsi yayı koruyan çekirdekten (O-3), düz kenarda Clipper2 hızlı yolu; öznitelik aktarımı `cakisma=ilk|reddet` (UÇUCA'daki gibi); sonuçlar köken taşır (F-02).
  **Şerit:** Değiştir ▸ Birleştir paneline "Alan ▾" ailesi (Birleştir · Kesişim · Çıkar · Kesenle Böl); Alan bağlam sekmesinde aynı aile.
  **Kabul:** destek matrisinde çok parçalı satır ⊘'den ölçülen hücrelere geçer; yuvarlak köşeli parselle kesişimde yay aynı merkez ve yarıçapla (O-1 testi gibi); |A∪B| + |A∩B| = |A| + |B| mm² içinde; iki ayrık alan `parca=tek` ile tek kimlik; üç istemci eşit; altın `alan-boolean.txt`; sayfalar.
  **Bağımlılık:** O-3; G-10 aynı çekirdeği katman düzeyinde kullanacak.
  **3B:** Yeni köşelere kot: `kot=kaynak|kesen` (Netcad'in "Kot Değerlerini Koru" ve "Kotları Kesenden Al"ı), kenar boyunca doğrusal; N-05'ten sonra.

- [ ] **N-09 · P1 — Tutamak seçenekleri ve kenar kaydırma (Düzenle çarkının karşılığı); tek geçişte uzat-kes; aralıkla bölme.**
  **Netcad:** Düzenle: nesneye ve tutulan yere göre seçenekler — S nokta/çizgi/merkez kaydır, K kaydır, D döndür/ölçekle, Del, − nokta/segment sil, + nokta ekle, C, P paralel kaydır, U uzat, R yarıçap, A yay yap, T teğet yap, J; komşuluk: Tek Obje / Dokunan / Noktaları Dokunan; Boşluk ile ek nesne; R referans noktası 217385152, 217394337. Uzat/Kes tek geçişte 217385426; Obje Böl uzunluğa göre 217385391.
  **Tasarım:** Sıcak tutamak istemi ("tıkla, götür, tıkla", `vertex_move.md`) bir **seçenek listesi** taşır: `[Köşe (S) · Kaydır (K) · Döndür (D) · Paralel (P) · Uzat (U) · Yarıçap (R) · Köşe ekle (+) · Köşe sil (−)]`; harf + Enter ya da sözcük seçeneği seçer — genel tek harf kısayolu değil, istem seçeneğidir (arayuz.md kuralı korunur). Her seçenek var olan bir komutun satırını kurar (`KÖŞETAŞI`, `TAŞI`, `DÖNDÜR`, `UZUNLUK`, `KÖŞEEKLE`, `KÖŞESİL`); tek yeni komut `core.edge_offset` — `KENARKAYDIR`, `OFFSETEDGE`, `KNK`: bir kenarı kendine paralel `mesafe` kadar kaydırır, komşu kenarlar kendi doğrultularında uzar/kısalır (Netcad P; Netmap Alan Düzeltme "Paralel"); `komsu=tek|dokunan` (dokunan: aynı köşeyi paylaşan komşu parselin kenarı da gider — G-05'in ortak sınır düzenlemesinin ilk dilimi). `BUDA`'ya `yontem=uzatkes` (sınıra göre kısaysa uzat, uzunsa kes; taraf ters seçeneği). `BÖL yontem=aralik aralik=<m>` (baştan sabit uzunlukta ardışık parçalar).
  **Kabul:** kenarı 2 m kaydırılan dikdörtgenin alanı el hesabıyla, kimlik ve öznitelikler aynı; `komsu=dokunan` ile ortak kenarlı iki parsel arasında boşluk ya da bindirme yok (TOPOLOJİ temiz); tutamak isteminde `P`+Enter ile `KENARKAYDIR` satırı aynı günlük; `BUDA yontem=uzatkes` iki sınır arasındaki kısa çizgiyi uzatır, uzunu keser; 23 m çizgi `BÖL yontem=aralik aralik=5` → 5,5,5,5,3; gerçek fare probu bölümü.
  **Bağımlılık:** C-07, G-05, N-04.
  **3B:** Kaydırılan kenarın yeni köşe kotları komşu kenar boyunca enterpolasyonla (N-05).

- [ ] **N-10 · P1 — Eğri inşaları: iki eğriye teğet doğru, teğet daireler, teğet yap, noktalardan geçen eğri, yaylı çoklu çizgi çizmek.**
  **Netcad:** 2 Daireye Teğet Doğru (dört çözüm, taraf yaklaşık gösterilir) 217385134; Daire: Serbest (teğet nesnelere, alternatifler, sonuçları kır) 217385120; Edit Teğet Yap 217385152; Noktadan Geçen Eğri 217385130; Yay Serbest (ardışık teğet yay ve çizgi, D ile geçiş) 217385108.
  **Tasarım:** Olgun kitaplık (Article 2.7, 2.11): teğet problemleri **OCCT `Geom2dGcc`** çözücüleriyle, uydurma eğri **OCCT `GeomAPI_Interpolate`** ile; sonuç bir kez mm'ye yuvarlanır. `ÇİZGİ yontem=teget2 birinci= ikinci= yon=` (dört çözümün yöne en yakını; önizleme dördünü soluk, seçileni kesikli gösterir); `DAİRE yontem=ttr` doğrudan yay ve daireye genişler, yeni `yontem=ttt` (üç teğet) ve `kir=evet` (teğet noktalar arasında kaynakları budar); `core.make_tangent` — `TEĞETYAP`, `TEGETYAP`, `MAKETANGENT`, `TĞY`, `TGY` (daire ya da yayı yarıçapını koruyup merkezini taşıyarak bir nesneye teğet yapar); `SPLINE yontem=gecen` (verilen her noktadan geçen); `ÇOKLUÇİZGİ` çizilirken istemde `Yay`/`Doğru` seçeneği — sonuç yaylı çoklu çizgi, yazılı karşılığı yeni `kenarlar=` parametresi (her kenarın ara noktası).
  **Kabul:** iki dış daireye dört teğet el hesabıyla mm; üç doğruya teğet iç çember el hesabı; teğet noktası `teğet` yakalamasıyla aynı nokta; uydurma eğri her noktadan geçer; yaylı çizim ile `kenarlar=` satırı aynı günlük; DXF şişkinlik gidiş-dönüşü; üç platform aynı mm (O-6).
  **Bağımlılık:** O-5, O-6, C-01.
  **3B:** Plan inşalarıdır; uç kotlarından enterpolasyon (N-05); düşey kurp T-04'ün işi.

- [ ] **N-11 · P1 — Çift çizgi: eksen çizerken iki yanda paralel (Paralel Çizgi).**
  **Netcad:** Paralel Çizgi: eksenin sağına ve soluna genişlik, köşe kesişimleri kendiliğinden çözülür, bir yana 0 verince tek yan 217385136; Paralel'in "Ucuna Bağla" köşe yöntemi (kenar uzunlukları korunur) 217385395.
  **Tasarım:** `core.double_line` — `ÇİFTÇİZGİ`, `CIFTCIZGI`, `DOUBLELINE`, `ÇFÇ`, `CFC`: `noktalar` (eksen), `sol`, `sag` (m; 0 = o yan yok), `kose=keskin|yuvarlak|pah`, `eksen=ciz|cizme`, `uclar=acik|kapali`, `katman_sol`, `katman_sag`. `core::entity_parallel` ve `kernel_offset` yeniden kullanılır — ikinci bir paralel hesabı yazılmaz; önizleme sonucun kendisidir. OFSET'e `kose=uc` ("ucuna bağla") eklenir.
  **Kabul:** L eksenin 2 m ve 3 m paralelleri el hesabıyla, köşeler kesişimde; `sol=0` tek yan; `kose=yuvarlak` gerçek yay (O-4); OFSET `kose=uc` vakası; üç istemci.
  **Bağımlılık:** O-4.
  **3B:** Eksen kotluysa paraleller aynı kotu taşır (N-05); şevli yol T-04.

- [ ] **N-12 · P1 — Yazı ekleri: sıkıştırma, zemin maskesi, ardışık artırma, okunur yön, metin dosyası, joker bul-değiştir.**
  **Netcad:** Yazı: Sıkışma (genişlik çarpanı), Fon, `++` ardışık artırma, uygulama noktası 217385103; Metin Dosya Yükle 217385186; komut satırında `MAKE TEXTS READABLE` 217386603; Bul Değiştir joker kuralları 217385430.
  **Tasarım:** METİN ve YAZIDÜZENLE'ye `genislik_carpani` (genişlik yine tek ölçüden, `core::text_width` — C-18), `fon=evet` (çizim arka uçlarında yazının arkasına zemin rengi; G-07'deki maske işiyle aynı yol), `artir=evet` (bir sonraki yerleştirmede sondaki sayı 1 artar: `101`→`102`, `101/12`→`101/13`). `core.readable_text` — `OKUNURYAP`, `MAKEREADABLE`, `OKY` (ters okunan yazıları 180° çevirir, bağlı yazıda bağ korunur). `core.place_text_file` — `METİNDOSYASI`, `METINDOSYASI`, `PLACETEXTFILE`, `MTD` (UTF-8 metin dosyasını çok satırlı tek yazı olarak koyar; güvenilmeyen girdi, io.md; fuzz). BULDEĞİŞTİR'e `desen=evet` (`*` önek/sonek kalıbı) — tek ayrıştırıcının içinde, yeni gramer değil (5.11).
  **Kabul:** `genislik_carpani=0.8` seçim kutusu, ekran ve PDF aynı genişlik; `fon` ekranda ve PDF'te; `artir` üç yerleştirmede 101, 102, 103; `OKUNURYAP` 200°'lik yazıyı 20°'ye çevirir, bağ kalır; bozuk UTF-8 dosyada ret; fuzz korpusu.
  **Bağımlılık:** C-12, C-18, G-07.
  **3B:** Yok.

- [ ] **N-13 · P1 — Ölçü krokisinden hızlı çizim: dördüncü köşe, derinlikle bina, ölçülü kutu, iki noktayla blok.**
  **Netcad:** 4.Köşeyi Oluştur (üç ölçülü köşeden dördüncü) 217385354; Bina Oluştur (iki köşe + derinlik; + sağ, − sol) 217385353; Kutu (ad, kâğıt boyu, dY/dX, açı) 217385090; Sembol ve Blok "2 Nokta ile" 217385184, 217385182.
  **Tasarım:** `core.fourth_corner` — `DÖRDÜNCÜKÖŞE`, `DORDUNCUKOSE`, `FOURTHCORNER`, `DKÖ`, `DKO` (üç köşeden paralelkenar; `dik=evet` dik açıyı dayatır ve sapmayı söyler); `DİKDÖRTGEN yontem=derinlik derinlik=<m>` (işaret kuralı `dik()` ile aynı — Açık soru 3); `DİKDÖRTGEN yontem=olcu en= boy= aci=` ve `kagit=A4|A3|…` (kâğıt ölçüsü × plan ölçeği = zemin boyu); `BLOKEKLE yontem=2n` (iki noktayla ölçek ve açı).
  **Kabul:** (0,0), (10,0), (10,6) → (0,6); 3 cm dik açı sapmasında sapma yazılır; A3 × 1:1000 kutu 420 × 297 m; `yontem=2n` bloğun eni iki nokta arasına oturur; üç istemci.
  **Bağımlılık:** C-02; Açık soru 3.
  **3B:** Üç köşe kotluysa dördüncüye düzlem kotu (N-05).

- [ ] **N-14 · P2 — Yasla: nesneleri bir referans nesnenin kenarına ya da ortasına hizalamak (Netcad Hizala).**
  **Netcad:** Hizala: sola, sağa, yukarı, aşağı, yatay ortaya, düşey ortaya — referans nesneye göre 217385156.
  **Tasarım:** `HİZALA` bizde nokta çiftiyle hizalamadır (AutoCAD ALIGN; Netcad'in "2 Noktadan Dönüşüm"ü) — ad çakışması yüzünden yeni ad: `core.align_box` — `YASLA`, `ALIGNBOX`, `YSL` (dizgideki "sola yasla"); `nesneler`, `referans`, `kenar=sol|sag|ust|alt|yatay_orta|dusey_orta`; ölçüt sınır kutusu değil çizilen biçim (`core::curve_outline`).
  **Kabul:** üç yazı sola yaslanınca en batı noktaları referansın en batı noktasında; tek geri alma adımı; aramada "hizala" ikisini de bulur (§5.4).
  **Bağımlılık:** yok.
  **3B:** Yok.

### Aşama D — Kroki, pafta ve harita tamamlama

- [ ] **N-15 · P1 — Koordinat ve uzunluk yazımı: köşelere Y/X, toplam uzunluk, çizimde koordinat özeti.**
  **Netcad:** Koordinat Yaz (NC6) 217388750; Etkileşimli Etiket `??KOOR_Y`, `??KOOR_X`, `Koordinat_Grid` 217385084; Uzunluk Yazdır "Toplam Uzunluk", "Çoklu Doğruları Parçalama", "Eğik Mesafe" 217385770; Koordine Özet (ekrana tablo) 217394043.
  **Tasarım:** işlem aracı `islem.koordinat_yaz` — `KOORDİNATYAZ`, `KOORDINATYAZ`, `LABELCOORDINATES`, `KRY`: her köşeye ya da noktaya `bicim="Y={y}  X={x}"`, `ondalik`, `taraf=otomatik|sol|sag|dis|ic`, `yukseklik` (0 = pafta ölçeğinde 2,5 mm, processing.md R10), `bagla=evet` (köşeye bağlı, köşe taşınınca yenilenir — model.md R46). `islem.uzunluk_yaz`'a `toplam=evet` (her nesneye toplam uzunluğu); N-05'ten sonra `egik=evet`. `islem.koordinat_ozet` — `KOORDİNATÖZET`, `KOORDINATOZET`, `COORDINATETABLE`, `KÖZ`, `KOZ`: seçili nokta ve köşelerin no · Y · X · (Z) tablosunu **çizime** sıradan yazı ve çizgilerle kurar (processing.md R9: yeni tür icat edilmez) ve kaynaklarının SONUCU olur — kaynak değişince "güncel değil".
  **Kabul:** dört köşeli parselde dört bağlı koordinat yazısı; köşe taşınınca yazı ve değer yenilenir; `toplam=evet` üç kenarlı çizgiye tek yazı = kenarların toplamı; özet tablosu nokta taşınınca `BAĞIMLILIK`'ta "güncel değil", `yenile` yeniden kurar; `docs/islem/README.md` satırları.
  **Bağımlılık:** F-04, G-07, S-01.
  **3B:** `{z}` alanı N-05 ile; özette isteğe bağlı Z sütunu.

- [ ] **N-16 · P1 — Röleve yazımı: dik düş, kutur, ara mesafe, alinmana cephe, poligon hattı, paralele dik hat.**
  **Netcad:** Detaylar ▸ Röleve: Dik Düş (dik boy ve başlangıca mesafe yazılır) 217385075, Paralele Dik Hat (iki paralel hat arasında oklu dik) 217385073, Poligon Hattı (uç değerli taban) 217385071, Poligon Kaydır (okunaklılık için kaydırılmış gösterim, gerçek değerler) 217385069, Poligon Hattı (Yön) 217385067, Kutur Bağla (bağsız nesneler arası mesafe) 217385789; Ölçme: Alinmana Cephe Yaz 217385077, Ara Mesafe Yaz 217385076; ayarlar: dik yazı boyu, alinman kriteri (5 cm), sıfırları kaldır 217388206.
  **Tasarım:** Bir aile, altı küçük komut; yazıların hepsi kaynağına **bağlı** (R46) ve pafta ölçeğinden boyutlu (R10); tabana dik ölçüler `dik()` ile aynı işaret kuralı. `core.perp_label` — `DİKDÜŞ`, `DIKDUS`, `PERPLABEL`, `DDŞ`, `DDS` (taban + noktalar → dik çizgi, dik boy ve ayak yazısı); `core.baseline` — `POLİGONHATTI`, `POLIGONHATTI`, `BASELINE`, `PHT` (iki noktalı taban, uç değerleri; `kaydir=` yalnız gösterimi öteler, değerler gerçek); `core.tie_label` — `KUTUR`, `TIELABEL`, `KTR`; `core.gap_label` — `ARAMESAFE`, `GAPLABEL`, `ARM` (iki hat arası, `aralik=`); `islem.alinman_cephe` — `ALİNMANCEPHE`, `ALINMANCEPHE`, `FRONTAGECHAINAGE`, `ACY` (`ALC` değil: `ALÇ` ALANAÇEVİR'in, katlanınca çakışır; alinman üzerindeki noktaların ardışık mesafeleri; alinman kriteri bir `SettingSpec`); `core.width_mark` — `DİKHAT`, `DIKHAT`, `WIDTHMARK`, `DKH` (paralele dik oklu hat). Kroki **biçimi ve şablonu** resmî bir belgedir: [M] kullanıcı tarif edecek; bu paket yalnız çizim araçlarını yapar.
  **Kabul:** her komut için el hesaplı altın senaryo (ör. taban (0,0)→(40,0), nokta (12.4, 3.1): ayak 12,40, boy 3,10); nokta taşınınca yazı yenilenir; `POLİGONHATTI kaydir=` gösterimi öteler, yazı değişmez; alinmana 4 cm uzaklıktaki nokta alinmanda sayılır, 6 cm'deki sayılmaz; altı komut sayfası ve bir "Röleve" rehber sayfası.
  **Bağımlılık:** N-15, S-01.
  **3B:** Röleve plan belgesidir; `{z}` isteğe bağlı.

- [ ] **N-17 · P1 — Pafta sistemi: pafta indeksi, pafta bul/git/oluştur, noktanın paftası, paftalara göre çıktı.** [M]
  **Netcad:** Pafta Editörü (ülke ve mevzi pafta indeksi, tek tek ve otomatik paftala, pafta değişkenleri, `.PAF`) 217389338; Pafta Ara (Git, Oluştur) 217386592; XYZ Sor'da noktanın paftası ve komşu pafta 217385203; Kutu ile yerel pafta 217385090; raporlarda pafta adı 217389281.
  **Tasarım:** Pafta bölümleme ve **adlandırma kuralı veri paketidir** (`data/catalogs/pafta/…`, data.md başlığı, `source` = ilgili yönetmelik maddesi): [M] kullanıcı tarif edecek; kod yalnız yorumlar (domain.md R1, R15). Komut `geodesy.sheet` — `PAFTA`, `MAPSHEET`, `PFT`: `islem=bul ad=` (adı sınırına çevirir; `git` / `olustur`), `islem=ad nokta=` (noktanın her ölçekteki paftası), `islem=indeks bolge= olcek= sistem=ulke|yerel baslangic=` (bölgeyi kesen paftaları `PAFTA` katmanına kutu olarak, adları öznitelikte), `islem=komsu`. KOORDİNAT'ın raporuna pafta adı (katalog yüklüyse). "Otomatik paftala" = `ÇIKTIYERLEŞİMİ islem=atlas katman=PAFTA` (L-03) — ayrı bir paftalama motoru yazılmaz.
  **Kabul:** katalog yokken komut adıyla reddeder (sessiz varsayılan yok, domain.md P11); katalogla gelen örnek adlar için ad → kutu → ad gidiş-dönüşü birebir; bilinen bir bölgede indeks beklenen pafta sayısını verir; atlas her paftaya bir sayfa basar; katalog şeması `schema/` altında.
  **Bağımlılık:** G-01, L-03; katalog kullanıcıdan (Açık soru 7).
  **3B:** Yok.

- [ ] **N-18 · P1 — Karelaj ve kilometre yazısı: çizimde grid kesişimleri, yol boyunca km/mesafe işaretleri.**
  **Netcad:** Karelaj Üret (belirli aralıkta grid sembolü ya da nokta, GRID tabakası, ad ve kod, Z DEM'den) 217385826; Obje Üzerinde Dizi `??KM`, `??U`, `??X`, `??Y`, `??N`, eksenden sapma 217385830; Grid akıllı çerçevesi 217385081.
  **Tasarım:** işlem aracı `islem.karelaj` — `KARELAJ`, `GRIDMARKS`, `KRJ`: `bolge` (kapsam ya da pafta kutusu), `aralik` (m), `bicim=arti|nokta|blok`, `yazi=evet` (kenarda koordinat); sonuç kaynağının SONUCUdur. İşlem aracı `islem.kilometre_yaz` — `KMYAZ`, `CHAINAGE`, `KMY`: yol boyunca `aralik`la işaret ve `bicim="{km}"` (`0+125,40` biçimi; `{mesafe}`, `{y}`, `{x}`), `baslangic_km`, `sapma`. `DİZİ mod=YOL`'a `sapma=`. Çıktıdaki `izgara=` aynı aralık hesabını kullanır (tek kod).
  **Kabul:** 100 × 100 m bölgede 10 m aralıkla 121 artı; 25 m'de bir `0+025` biçimli yazılar; yol değişince yazılar "güncel değil"; üç istemci.
  **Bağımlılık:** F-04, L-01; kotlu karelaj T-01'den sonra.
  **3B:** Karelaj noktaları yüzeyden kot alır (T-01) — Netcad'in DEM'den kotlu karelajı.

- [ ] **N-19 · P1 — Şev ve iki çizgi arası tarama: şev, höyük, kokurdan, merdiven.** [M] aralık değerleri
  **Netcad:** Şev Tara (bozuk, dere içi, normal, tümsek; tarama hattını belirle; yarım oran 0,50) 217385372, Höyük ve Kokurdan taraması aynı sayfada; 2 Doğru Arasını Tara 217385376; Şevi Tekrar Tara, Ölçek Değiştir, Şev Kenar Bul 217385335, 217385322, 229905063; Merdiven Taraması 217385779.
  **Tasarım:** Şev taraması bir stil değil bir **üretimdir**: iki kaynağa (şev üstü, şev altı) bağlı SONUÇ (model.md R46e) — kaynak değişince "güncel değil", `BAĞIMLILIK islem=yenile` yeniden üretir (Netcad'in "Şevi Tekrar Tara"sı kendiliğinden gelir). `surface.slope_hatch` — `ŞEVTARA`, `SEVTARA`, `SLOPEHATCH`, `ŞVT`, `SVT`: `ust`, `alt`, `tip=normal|bozuk|dere|tumsek|hoyuk|kokurdan`, `aralik`, `oran`; `core.between_hatch` — `ARATARA`, `BETWEENHATCH`, `ART`: iki çizgi arasında düzgün aralıklı çizgiler (merdiven, iki doğru arası). Aralık ve oranın ölçeğe göre değerleri (Netcad 1/1000'de 2 m, 1/500'de 1 m der ve BÖHHBÜY'e atıf yapar) [M] katalogdan, kullanıcı tarif edecek; kodda yalnız parametre.
  **Kabul:** iki paralel doğru arasında `ARATARA aralik=1` beklenen sayıda çizgi; şev üstü taşınınca tarama "güncel değil", yenileyince yeni biçim; höyük tipi tepeden ışınsal; katalog yokken `aralik` zorunlu; altın senaryo.
  **Bağımlılık:** F-04, G-06.
  **3B:** Kotlu şev üst/alt çizgileri (N-05) T-01'de kırık hat olur; tarama yüzeyden bağımsız kalır.

- [ ] **N-20 · P1 — Şerit düzeni: Netcad alışkanlığıyla yeniden gruplama ve "şeritte göster".**
  **Netcad:** Sekmeler Giriş · Düzenle · Analiz · Araçlar · Detaylar · Görünüm (+ Hesap, Netsurf, Netmap… modül sekmeleri) 217387132; Giriş'te Çizim · Sorgu · Düzenleme · Görüntü 217385142; Komut Ara ▸ **Git**: komutun şeritteki yerini gösterir 217386600.
  **Tasarım:** §5'teki tabloya göre: Giriş'e Sorgu ve Görüntü, Pano Değiştir'e; Harita sekmesi ikiye — **Ölçme** (Hesap karşılığı) ve **Arazi** (Netsurf karşılığı); Veri grubu Çıktı'ya; Açıklama'ya Röleve; Analiz'e Konumsal ve Topoloji. Ctrl+K komut listesinde **Şeritte göster** (`Ctrl+Enter`): seçili komutun düğmesinin sekmesini açar ve düğmeyi vurgular; şeritte yeri olmayan komutta "Diğer komutlar" listesini açar. Sekme adı değişiklikleri Açık soru 4'ün kararına bağlı.
  **Kabul:** her sekme 1440 px'e sığar, şerit ≤ 134 px (U-07 kabulü korunur); menü, araç, erişim probları 0 kusur; `arayuz.md` ve her komut sayfasının "Arayüz" bölümü yeni yollarla (docs gate yeşil); "Şeritte göster" her komutta bir düğme bulur ya da "Diğer komutlar"ı açar.
  **Bağımlılık:** U-07 (kapandı), U-01; N-15…N-19 geldikten sonra, tek seferde.
  **3B:** Arazi sekmesi 3B görünümün (N-29) giriş yeri olur.

### Aşama E — CBS, proje ve uzmanlık

- [ ] **N-21 · P1 — Proje araması: yazı, nokta adı, öznitelik ve katman adında tek arama; Bulgular paneli.**
  **Netcad:** Netcad Arama Motoru (CAD ve spatial veride, sonuç kart/ızgara, süz, yazdır, CAD/GIS sayıları) 217387074; Mesaj Paneli (bilgi/uyarı/hata, yaz) 217386585.
  **Tasarım:** salt okunur `core.find_objects` — `NESNEARA`, `FINDOBJECTS`, `NSA`: `metin`, `alanlar=yazi|nokta_no|oznitelik|katman`, `tur=`, `katman=`, `buyuk_kucuk`, `tam_kelime`, `sinir`; yapılandırılmış `report` (anahtar, tür, katman, eşleşen alan, konum); ajana açık okuma aracı. Sağ panelde **Bulgular** sekmesi: satıra tıkla → yakınlaş ve seç (`SEÇ` satırı); TOPOLOJİ, TEMİZLE ve KAPSAMDENETİM bulguları da aynı panelde (command.md R13h: önce say, sonra listele).
  **Kabul:** 10 000 nesnede "1284" araması yazı, nokta_no ve öznitelik eşleşmelerini ayrı sayar; Türkçe katlama (`İSTANBUL` = `istanbul`); tıklamayla seçim `SEÇ` satırı; TOPOLOJİ bulguları aynı panelde.
  **Bağımlılık:** U-01, G-03.
  **3B:** Yok.

- [ ] **N-22 · P1 — Proje düzenle: kullanılmayan tanımları temizle, seçileni ayrı dosyaya kaydet, kot sıfırla.**
  **Netcad:** Proje Düzenle: Kullanılmayan Tanımları Temizle, Çift Objeleri Ayıkla, Kot Sıfırla 217388066; Kaydet ▸ Seçilen Objeleri Kaydet 217388025.
  **Tasarım:** `core.purge` — `KULLANILMAYANLAR`, `PURGE`, `KLN`: boş katmanlar, kullanılmayan stil, blok ve desenler; `islem=bul|temizle`. Blok tablosu yalnız eklenir (model.md R45) ve geri alma tablo satırlarını kesmez (R4a) — temizlemenin tablo işaretiyle mi yoksa kayıt sırasında mı yapılacağı bakımcı kararıdır (paketin ilk adımı). `DIŞAAKTAR nesneler=` ve `FARKLIKAYDET nesneler=` (yalnız seçili nesneler ve kullandıkları tanımlar). Kot sıfırlama `KOTVER yontem=sifirla` (N-05).
  **Kabul:** kullanılmayan 3 katman ve 2 blok bulunur, temizlenince dosya küçülür ve sorunsuz açılır; seçili 12 nesnelik kayıt açılınca 12 nesne ve kullandıkları katman/stil gelir.
  **Bağımlılık:** N-05, I-05.
  **3B:** Kot sıfırlama N-05'ten.

- [ ] **N-23 · P2 — Veri karşılaştır ve aktar: iki katmanı anahtar sütunla karşılaştır, farkları raporla, seçilenleri aktar.**
  **Netcad:** Veri Karşılaştır (kaynak/hedef tablo, ortak kolon, farklı kayıtlar; bu değeri kullan, satırı/tümünü/geometriyi/sözeli aktar) 217385463; Veri Aktar (güncelle/ekle, kaynak ve hedef anahtarı, geometri kopyala, varsayılan değer, look-up, otomatik eşleştir) 217385459.
  **Tasarım:** `core.compare_layers` — `KARŞILAŞTIR`, `KARSILASTIR`, `COMPARELAYERS`, `KRŞ`, `KRS`: `kaynak`, `hedef` (katman ya da PostGIS tablosu), `anahtar`, `alanlar`, `geometri=evet`; rapor dört küme (yalnız kaynakta, yalnız hedefte, değeri farklı, geometrisi farklı); `aktar=deger|geometri|hepsi` seçilen satırlar için tek işlem; PostGIS'e yazım I-04'ün işlem sözleşmesiyle.
  **Kabul:** iki GPKG katmanında 3 değer farkı, 1 eksik, 1 geometri farkı bulunur; aktarım tek geri alma adımı; PostGIS'te tek `pqxx::work`.
  **Bağımlılık:** G-02, G-03, I-04.
  **3B:** Geometri karşılaştırması Z'yi de kapsar (N-05 sonrası, ayrı eşik).

- [ ] **N-24 · P2 — Görünüm kipleri: gri/tek renk önizleme, dolguyu/sınırı gizle, nesne ipucu, ikinci görünüm.**
  **Netcad:** Renk Modu (renkli/gri/tek renk), Taramalar Renkli, Saydamlığı Kapat 217385048; Alan Taramaları ve Alan Sınırları aç/kapat, Obje İpuçları 217385043; 2 Pencere Aç, Ekran Sakla 217385050.
  **Tasarım:** görünüm durumu (model.md R43; belgeye girmez): `TERCİH renk_kipi=renkli|gri|tek`, `dolgu_goster`, `sinir_goster`, `nesne_ipucu` (imleç altında tür · katman · kimlik · uzunluk/alan; `core::pick` ile, yeni bir seçim yolu değil); durum çubuğunda KALINLIK gibi çipler. Ekran görüntüsü: önce `YAZDIR` ile görünümün PNG'ye yazılıp yazılamadığı doğrulanır; yazılabiliyorsa ayrı komut açılmaz. İkinci görünüm (aynı belge, farklı ölçek) ayrı dock tuvalidir ve render iş parçacığı kararıyla (render.md R10) birlikte ele alınır.
  **Kabul:** gri kipte tuval ve yazdırma önizlemesi aynı; ipucu kare bütçesini (16 ms) aşmaz; kipler çizimin parmak izini değiştirmez.
  **Bağımlılık:** G-06; ikinci görünüm için render.md R10.
  **3B:** İkinci görünüm N-29'da 3B görünüm olarak da açılır.

- [ ] **N-25 · P2 — İnceleme notları: koordinata bağlı, durumlu, yanıtlı notlar.**
  **Netcad:** Notlar (Açık, Devam Etmekte, Çözüldü, Çözülmedi, Kapalı; kimler düzenleyebilir; yanıtla; haritada göster; ilişkili nesneler; Excel'e aktar) 217385043.
  **Tasarım:** Önce model kararı: not bir **belge eşyası** mı (kılavuz gibi, model.md R47 — seçime ve dışa aktarıma girmez) yoksa kendi katmanında nesne mi (Açık soru 16). Komut `core.note` — `NOT`, `NOTE`, `NT`: `islem=ekle|yanitla|durum|sil|listele`, `nokta`, `metin`, `durum`, `nesneler` (ilişkili); yazar oturum kullanıcısıdır; yapay zekânın yazdığı not her durumda "öneri" etiketlidir (ui.md R42). CSV ve GPKG'ye dışa aktarım.
  **Kabul:** ekle/yanıtla/durum değiştir birer geri alma adımı; kaydet/aç; "Açık" süzgeci; haritada göster yakınlaşır.
  **Bağımlılık:** F-02; model kararı.
  **3B:** Not konumu kot taşıyabilir (N-05).

- [ ] **N-26 · P2 — Çizdir ekleri: çerçeveyi döndürme, yalnız seçileni basma, basılan pencereleri kaydetme, eksen boyunca şerit pafta.**
  **Netcad:** Çizdir: F5/F6 ölçek, F7/F8 döndür, ok tuşlarıyla kaydırma, basılan pencereler CIZPEN katmanına kutu olarak (kenarlaştırma için) 217388055; Seçilenleri Çizdir 217388037; Rulo Şeklinde Kaydet (RULO tabakası, şerit harita) 217388025, 217385136.
  **Tasarım:** `YAZDIR aci=` (çerçeve dönüşü; ui.md R33–R37: yakalanan alan basılan alandır), `YAZDIR nesneler=` (yalnız seçili), `YAZDIR kaydet=evet` (basılan pencereyi `YAZDIRMA_PENCERELERİ` katmanına kutu olarak; bir sonraki basımda yakalanır ve kenarlaşır); şerit pafta = `ÇIKTIYERLEŞİMİ islem=atlas yol=<çizgi> aralik= genislik=` (eksen boyunca dönük sayfalar; L-03 atlasının bir kapsam türü).
  **Kabul:** 30° döndürülmüş çerçevenin PDF'i önizlemeyle aynı; `nesneler=` ile başka nesne basılmaz; ardışık iki basımın kutuları kenarda çakışır; 2 km'lik yolda beklenen sayfa sayısı.
  **Bağımlılık:** L-01, L-03, L-04.
  **3B:** Yok.

- [ ] **N-27 · P2 — Yol ve kavşak çizimi (Planet geometrisi): eksenden yol, kaldırım ve refüj; kavşak köşesi; refüj kapatma; yol yuvarlatma.** [M] genişlik ve kalınlık kuralları
  **Netcad:** Yol Çiz (Aksis / Paralel): sağ ve sol genişlik, kaldırım, refüj, tutma noktası, kırıklarda kendiliğinden düzeltme 320014438, 320018799; Kavşak Oluştur (kırma mesafeleri, yarıçap, "Oto") 320014486; Ada/Kaldırım Köşesi (dik ve yatay mesafeyle kır, yarıçapla yuvarlat) 320015541; Refüj Kapat 320015556; Yol Yuvarlat 320015562; Köşe Düzelt 320015550; Netmap Yol Dengelemesi 217385903.
  **Tasarım:** Geometri kodda, **kurallar veride**: `planning.road` — `YOLÇİZ`, `YOLCIZ`, `DRAWROAD`, `YÇZ`, `YCZ` (N-11'in çift çizgisi üstüne: eksen + `yol`, `kaldirim`, `refuj` genişlikleri, tutma noktası, katmanlar); `planning.junction` — `KAVŞAK`, `KAVSAK`, `JUNCTION`, `KVŞ`, `KVS` (kesişen yol kenarlarında `kir_dik`, `kir_yatay`, `yaricap`; YUVARLA ve PAH çekirdeğiyle); `planning.close_median` — `REFÜJKAPAT`, `REFUJKAPAT`, `CLOSEMEDIAN`, `RFK`. Plan kademesine göre genişlik, çizgi kalınlığı ve köşe değerleri (Netcad: UİP'te ada kenarı kalınlığı yol genişliğinin onda biri, ÇDP'de sabit yol gösterimleri) [M] kullanıcı tarif edecek; kodda sabit yok (5.13).
  **Kabul:** iki yolun kesişiminde dört köşe kırılmış ve yuvarlanmış, kenarlar kesilmiş, tek geri alma adımı; katalog yokken genişliksiz çağrı ret; üç istemci; altın senaryo.
  **Bağımlılık:** N-11, C-06, S-04 (kural tarifi).
  **3B:** Kotlu eksen (N-05) enkesit ve şevde T-04'e devreder.

- [ ] **N-28 · P2 — Canlı GNSS konumu ve saha aplikasyonu (NMEA).**
  **Netcad:** Gps sekmesi: Başlat, Koordinat Al, Sürekli Koordinat Al (zaman ve mesafe süzgeci), Aplikasyon Modu, Konumu Göster; seri port ve hız ayarları 217386907, 217388146, 217388155, 217388164.
  **Tasarım:** Olgun kitaplık: Qt Positioning (NMEA kaynağı) ve Qt Serial Port, yalnız `/src/app`'te (Qt'siz hedefler kuralı, Article 3.4). Konum bir **girdi kaynağıdır**: NOKTA isteminin "GNSS'ten al" cevabı; komut gövdesi kaynağa göre dallanmaz (command.md P10). `core.gnss` — `GNSS`, `GNS`: `islem=baglan|kes|durum|kaydet`, sürekli kayıtta `zaman` ve `mesafe` süzgeci; aplikasyon kipinde seçili noktaya kalan dY, dX ve mesafe tuvalde. WGS 84'ten belge sistemine dönüşüm G-01 hattından (PROJ).
  **Kabul:** kayıtlı bir NMEA dosyasıyla (cihazsız) 10 m süzgeçli sürekli kayıt beklenen nokta sayısını verir; dönüşüm hangi sistemden hangisine olduğunu söyler; bağlantı kopunca arayüz donmaz; `build.md` ve `NOTICE` kaydı.
  **Bağımlılık:** G-01.
  **3B:** GNSS yüksekliği elipsoidaldir; ortometrik yükseklik jeoit ızgarasıyla (domain.md R13); Z'ye yazarken hangi yükseklik olduğu kayda geçer (N-05).

- [ ] **N-29 · P2 — 3B görünüm (3D+ karşılığı): yüzey, kotlu çizgiler ve yükseltilmiş yapılar; görüntüleme ve ölçüm.**
  **Netcad:** NETCAD/3D+ (projeyi 3B'ye aktarma, 2B–3B dinamik etkileşim, doku kaplama, 3B'de yakalama ve ölçüm, çok pencere) 217389385; Planet 3D Simülasyon 320016527; 3B ekranda paralel, ölçekleme ve ölçüm 217385395, 217385404, 217385201.
  **Tasarım:** Ayrı bir dock tuvali ve QRhi 3B hattı; 2B sahne kurucusuna ve kesme bloğuna dokunulmaz, 3B sahne kendi verisini kurar. Kaynaklar: yüzey (T-01), kotlu çizgi ve noktalar (N-05), yapılar `yukseklik`/`kat` özniteliğinden **ekstrüzyonla** (OCCT `BRepPrimAPI_MakePrism`, yalnız görüntü; belge 2.5B kalır). Origin offset kuralı (render.md R2) Z'ye genişler; seçim 2B ile aynı kimlikte (T-05 kabulüyle aynı ilke); ilk sürümde 3B düzenleme yok — ölçüm (3B uzunluk) ve seçim var. Ortofoto kaplama G-08 rasterıyla.
  **Kabul:** 100 bin üçgenlik yüzey ve 5 bin yapıda kare süresi ölçülür ve bütçe yazılı sabitlenir; 2B'de seçilen nesne 3B'de vurgulu; 3B ölçüm el hesabıyla (plan uzunluğu + kot farkı); sayfa.
  **Bağımlılık:** N-05, T-01, G-08, render.md R10.
  **3B:** Paketin kendisi.

## 4. Mevcut TODOS maddelerine Netcad'in eklediği

Bu maddeler `TODOS.md`'de zaten var; burada yeni kimlik açılmaz. Her satır, o maddenin kabulüne eklenmesi
önerilen **Netcad kaynaklı somut** işlerdir. Harfli dilimler (S-01·a gibi) yalnız bu plandaki sırayı
anlatmak içindir; `TODOS.md`'deki kimlik değişmez. S-01'in Netcad dilimi (a–e) P0'a çekilmeli: Netcad
kullanıcısı için nokta her işin başıdır.

| TODOS | Netcad kaynağı (pageId) | Eklenecekler |
|---|---|---|
| **U-01** araç keşfi, komut satırı | 217386600, 217386629, 217387057 | (1) Çakışmasız **eş adlar** (§5.3): OFSET'e `PARALEL`, PATLAT'a `AYRIŞTIR`, STİLKOPYALA'ya `BİÇİMBOYA`, YUVARLA'ya `KÖŞEYUVARLAT`, ÇOKLUÇİZGİ'ye `ÇOKLUDOĞRU`, ALANÖLÇ'e `ALANSOR`, KOORDİNAT'a `XYZSOR`, ÖLÇ'e `CETVEL`, YAZDIR'a `ÇİZDİR`, KATMAN'a `TABAKA`, KATMANAT'a `TABAKADEĞİŞTİR`, YAKINLAŞ'a `LİMİTBUL`, DİKDÖRTGEN'e `KUTU`, BÖL'e `OBJEBÖL`, DİKAYAK'a `YANNOKTA`. (2) Çakışan terimler için **yalnız aramada** görünen "bilinen adlar" alanı (`CommandSpec`'e ek alan, ayrıştırıcı okumaz; 6.14) — "kaydır" araması TAŞI'yı "bilinen adı: Kaydır" diye üstte gösterir. (3) Komut Ara ▸ Git = "Şeritte göster" (N-20). (4) **Son komutu yinele**: boş komut satırında Enter ya da Boşluk son komutu yöntemiyle yeniden başlatır; Netcad'in "sol tık tekrarlar"ı isteğe bağlı tercih (`TERCİH son_komut enter`, `tik` ya da `kapali`, varsayılan `enter`; tık seçimle çakıştığı için varsayılan değil). **Durum (29 Eylül 2026):** (1), (2) ve (4) M1'de yapıldı; `tik`'te nesneye tık yine seçer; seçim yokken boş yere tık yineler ve komut noktayla başlıyorsa ilk noktası olur, seçim varken seçimi bırakır. (3) N-20'yi bekliyor |
| **U-02** dinamik sayısal giriş | 217386622, 217387846 | Çizim Hesap Araçları: referans eksene göre **açı**, önceki kenara göre **sapma**, **eğim %**, dX/dY, koordinat; "yakalanacak açı/eğim değerleri" listesi (yakalanınca vurgu); rakama basınca giriş (bizde odak komut satırında, R53); Referans Noktası (bizde İZ); Netcad'in Shift+1…5 geçişleri bizde istem seçeneği |
| **U-03** yakalama ve seçim | 217387890, 217388230, 217391827 | Boşlukla alttaki nesne ve aday listesi; `/` ile iç içe alanlar (N-03 `SEÇ mod=İÇEREN`); "seçilince bir kez yanıp sönsün"; süzgeç kurallarının (`A*`, `-A*`, `~`, `#3,569`, `500..521`, `1..10,2`, `+ & %`) tek ayrıştırıcıya **fonksiyon olarak** girmesi (`desen()`, `aralik()`; 5.11 — ikinci gramer yok) |
| **U-04** tek özellik paneli | 217385435, 217385154, 217385175 | Toplu Obje Değiştir: karışık seçimde ortak özellikler (Genel / Görünüm / Nesne), uzunluk ve yarıçapta **aritmetik** (+, −, ½); Biçim Boya'nın katman ve sınıf aktarımı (STİLKOPYALA'ya `katman=evet`, `oznitelik=`); proje ölçeği değişince yazı ve blok boylarını uyarlama seçeneği (217388073) |
| **U-05** katman ve çalışma alanı | 217387722 | Katman süzgecinde `A*`, `-A*`, `A*+B*`; sıralama (A–Z, kilitliler önce, basılabilirler önce, çizim sırasına göre); katmanları birleştir ve kopyala; "Tabloya Aktar"; gösterilecek kolonlar (nokta adı, kot, kod); katmana yakınlaş (N-01) |
| **U-06** işe dayalı başlangıç | 217387999 | Yeni ▸ şablon galerisi: kategori, favori, projeksiyon, katmanlar, sayısallaştırma kalemleri, proje değişkenleri, altlık; "Başka Projeden Özellik Ekle" (217388012) |
| **G-01** CRS ve dönüşüm | 217387143, 217388073 | **İkinci sistem okuması**: KOORDİNAT, ÖLÇ, ALANÖLÇ, nokta tablosu ve raporlarda ikinci sistemde değer; WKT ile sistem girişi; yerel sistem + afin düzeltme (DNS dosyası) |
| **G-03** öznitelik tablosu, alan hesaplayıcı | 217385476, 217385510, 217385499 | Kolon Doldur (ifade ya da sabit değer); hesaplanan sütunlar (birincil anahtar, bilgisayar ve kullanıcı adı, tarih/saat, renk, tür, katman, alan, çevre, uzunluk, merkez, başlangıç/bitiş, min/max X-Y-Z, açı, semt, nokta sayısı, dış/iç halka sayısı, geometri doğrulama kodu); tablo ilişkileri (1-1, 1-N, N-1, N-N, gösterim ifadesi) |
| **G-04** sayısallaştırma kalemleri | 217387068 | Kalem = sınıf/katman, renk, çizgi tipi, kalınlık, dolgu, yumuşatma, merkez sembolü, tarama, kot yazdırma; **"alan kapat moduyla başla"** (içine tıkla, N-03); sütun ön değerleri; **"Menüye Gönder"** (kalemler şeritte bir sekme olur); "İlişkilendir" (var olan nesneyi sınıfa bağlar); BÖHYY menüleri [M] |
| **G-05** topoloji denetimi ve düzenleme | 217385486, 217385453, 217385487, 217385489, 217386113, 217385491, 217385152 | Düzelt (Son Nokta) toleransla uç buluşturma; Düzelt (Uzat-Kes) eksik uzatma; toleranslı Düzelt (Kesişim); ortak sınırı koruyan Çizgileri/Alanları Basitleştir; Düzenle'nin Dokunan / Noktaları Dokunan kipleri (ilk dilimi N-09 `komsu=dokunan`) |
| **G-06** semboloji ve kartografya | 217385043, 217385387, 217385784, 217385781, 217385035, 217387722 | **Balastro** (uç dairesi, çizgi dairede kırpılır); tek taraflı çizgi tipleri (duvar, tel çit — çizim yönü mülkiyet yönünü gösterir); bina ve bina kenar taraması `tarak-cizgi` ile; kalem tablosu; katman başına renk kipi; **BÖHHBÜY gösterim kataloğu** (bugün `/data`'da yalnız MPYY var) [M] |
| **G-07** etiketleme ve ifade | 217385037, 217385084, 217385083 | Görünümde **dinamik etiketler**: nokta adı, nokta kotu, nokta kodu, alan adı, çizgi kotu (proje bazlı, mm ya da piksel boy, `/` öncesini göster, nokta adını kısalt); şablonlu etiket (`??KOOR_Y,N,10,3`: alan genişliği ve ondalık); "Etiketleri Üret" (dinamikten kalıcıya); yazı zemini maskesi (N-12 ile ortak) |
| **G-08** raster ve oturtma | 217386764, 217385183 | Otomatik Register: ölçek ve pafta adından köşeler, grid noktalarını kullan, toplu, kopyalarla çalış; çizime fotoğraf/resim yerleştirme |
| **G-09** OGC ve servisler | 217388154 | OGC katalog istemcisi; TUCBS coğrafi veri servis havuzundan "referans olarak ekle" |
| **G-10** vektör mekânsal analiz | 217385482, 217385480, 217385478, 217385472, 217385374, 217385491 | **Overlay** (iki katman, öznitelikler birleşir, sonuçtan çıkar); **İçindekinden / Çevreleyenden Bilgi Al** (ilk değer, kayıt sayısı, toplam, ortalama; tampon değeri); Çoklu Tampon; Voronoi; Topoloji ▸ Birleştir (ortak değere göre dissolve, yakınlık); nesne düzeyindeki boolean N-08'den |
| **G-11** raster–vektör | 217385826, 229905438, 217388318 | Karelaj noktalarına DEM'den kot; rasterdan üçgen/nokta üret; yoğunluk analizi |
| **G-12** işlem modeli | 217385528, 217385530 | Mimar operatör aileleri: veri kaynağı, veri güncelle, konumsal analiz, topolojik düzeltme, dosya, GPS, görüntü işleme, geliştirici (komut / makro / iş akışı çalıştır) |
| **I-01** format matrisi | 217387997, 229905436 | Netcad'in açtıkları: KML/KMZ, GML, ITF (Interlis), NMEA/GPX, LAS/LAZ, XLS/XLSX, SQLite, ZIP içinden açma, DGN, raster (TIFF, IMG, DT0, HGT, VRT), LandXML |
| **I-02** Netcad'den geçiş | 217389312, 217389338, 217384998, 217389311, 217389341, 217385340, 217387068, 217385184 | Taşınacak Netcad dosya türleri: NCZ (proje), NCN (nokta), CKS (rapor), PAF (pafta indeksi), DNS (dönüşüm noktaları), NCK/PO5 (karne/poligon), MIR/YDE/PRZ (takeometri), KTB/KSE/LIS (güzergah/enkesit), ODF/ODFP (sayısallaştırma menüsü), NCS/NCB (sembol). Önce metin biçimliler (NCN, CKS, DNS); NCZ için yol I-02'deki karar |
| **I-04** canlı PostGIS | 217385459 | Veri Aktar: kaynak/hedef anahtarıyla güncelle ya da ekle, geometri kopyala, varsayılan değer, otomatik eşleştir (N-23 ile) |
| **I-05** taşınabilir, kurtarılabilir proje | 217388025, 217388073 | Otomatik kayıt hatırlatması; `ad_(YYYY_MM_DD HH_MM_SS)` biçiminde zaman damgalı yedek ve tutulacak yedek sayısı; proje özelliklerinden yedeği açma ya da referans olarak ekleme |
| **S-01** ölçü noktası, saha verisi | 217385088, 217385086, 217389312, 217389316, 217389332, 217389281, 217391827 | **a** `NOKTA ad= kod= kot=` ve ad artımı (`101`→`102`, `101/12`→`101/13`); tıklanan yerde nokta varsa yakalayıp düzeltme sunar; önekten sembol (`P.*`, `N.*`, `B.*` hangi sınıf) [M]. **b** Ardışıl nokta: başlangıç no, sabit Z, yüzeyden Z. **c** Nokta tablosu: Y/X/Z düzenleme, ekle/düzenle/sil (Netcad F3/F4/F5), sırala, çift noktaları ayıkla (ilk ya da ortalama), yeniden adlandır (`P.` ekle/kaldır), sıralı numara, kolon işlemleri, ondalık yuvarla, tabakalandır. **d** `NOKTALAR` yüklemede çift politikası: yenisi / eskisi / ortalama / bulunamayanları ekle / yalnız Z güncelle. **e** Köşelere nokta nesnesi (klasik, kenar sıralı, başlangıçtan; mevcutları koru). **f** Total station ham verisi (WILD/LEICA, TOPCON, ZEISS, SOKKIA, GEODIMETER; io'da, fuzz'lı). **g** Nokta kodundan otomatik çizgi |
| **S-02** jeodezik hesap, aplikasyon | 217389337, 217389328, 217389336, 217394096, 217384998, 217389283, 217389326, 217389311, 217389334, 217389282, 217389332 | **Kotlu alım**: ALIM'a `duseyaci`, `egik`, `alet`, `reflektor`, takeometride alt/orta/üst kıl (N-05 sonrası); **Geriden Kestirme** (üç bilinen noktaya açılar); N noktadan **Afin**, Helmert matrisinden dönüşüm; OTURT'ta uyuşum testi (nokta çıkarınca m0); Zemine İndirgeme; poligon **ağı** (güzergah tanımla, dallanma, ana/ara) ve **karne** (rasat → Gauss → özet; deniz/elipsoit yüzeyi ve Gauss–Krüger indirgemeleri); kanava çizimi; Prizmatik Aplikasyon (+ çizim); "400 − açı" (ters yön) seçeneği. Kurum ve yönetmelik toleransları, indirgeme sabitleri [M] |
| **S-03** parsel düzenleme [M] | 217385885, 217385862, 217385878, 217385857, 217385887, 217385879 | Yalnız geometri ve arayüz açıkları; davranış kullanıcı tarif edecek: ifrazın **sabit noktadan** (dönen ayırma çizgisiyle alan), **cephe + açı**, **dik** (kenara dik, alanla), **iç alan**, **serbest çoklu çizgiyle** yöntemleri; "bu parsel yeniden bölünecek" akışı; alan düzeltmede anlık alan göstergesi ve hassasiyet adımı; parsel tablosu (Netcad Parsel Editörü); Netmap sınıfları (Ada, Parsel, PKN, Yapı, Mahalle, İrtifak, YKN) G-04 kalemleri olarak |
| **S-04** planlama, dağıtım [M] | 320014255, 217385208, 217393460, 217384961, 217384999 | Planet (plan kademesi, alan dağılımı, donatı, nüfus, DOP, yürüme mesafesi, semboller, plan notu, PlanGML), Dağıtım (18. madde; dağıtım editörü, otomatik dağıtım, alan dengeleme), Netkamu, Nettop — hepsi kullanıcı tarif edecek; geometri araçları N-27 |
| **T-01** kalıcı yüzey | 217385374, 217385375, 229905074, 229905076, 329154828, 229905436, 229905438 | Üçgen Oluştur: Z filtresi, kenar uzunluğu filtresi (dışındakiler ayrı katmana), kısa üçgenleri yok et, kırık hat, dış sınır, delik, **içbükey sınır** (yakınlık kriteri, önizleme); eğrilerden üçgen; karelaj (grid DEM); **Model Düzelt** (kırık hatla, nokta ekle/çoklu/sil, üçgen ekle, **üçgen döndür**); Üçgen Analizi (kot farkı, kot düzenle, hatalı kotları bul: komşu ortalaması, en yakın kot); Model Birleştir, Sınır, Kes, Noktalar, **Platform Ekle**, Boşluk Doldur; **Model Kontrolü** (yırtık bul, onar); LandXML, raster ve SHP'den model. Kısıtlı Delaunay için CDT zaten ağaçta |
| **T-02** eş yükselti | 217385338, 217385356, 217385370, 217387758, 217385043 | Eğri Geçir: Zmin/Zmax/aralık, süzgeçle tek eğri, basitleştir, "25 metreleri ayır" (ana/ara eğri katmanları), kota göre renk; **Eğrilere Kot Yaz** (periyodik ya da bir çizginin kestiği yerde; artan eğim yönünde okunur); Eğri Temizle (oto); Eğri Alanı Hesapla; **Hızlı Eğri** (kaydedilmeyen, hata bulmaya yarayan dinamik eğri) |
| **T-03** boykesit, enkesit, hacim | 217392988, 217393227, 217389314 | Hacim: enkesitten (TCK, ortalama alan, prizmoidal, DSİ — zorunlu olanlar [M]), iki yüzey arası prizmatik, tabana göre (bölgede, kazı/dolgu ayrı model); Enkesit Al (aralık, örnekleme, genişlik; detay noktalarında, modelin kesildiği yerde, kırık izdüşümlerinde, ekseni kesen çizgilerde; sabit kotlu ya da katmandan yüzey ekle); Profil (yatay/düşey ölçek, kıyas kotu, çok model); Hızlı Profil; enkesitten kübaj/plankote; enkesit tablosu (km, eksene mesafe: sol −, sağ +, kot, kod) |
| **T-04** güzergah ve koridor | 217385340, 217386679, 217385513, 217386111 | Güzergah Tanımla (elemanlar: doğru, yay, klotoid; someler), km yakalama (km + sapma: sağ +, sol −), doğrusal referans (başlangıca mesafe + sapma ile nokta ve çizgi), Netpro'nun dinamik projesi (yatay/düşey, enkesit, kübaj, kavşak; biri değişince bağlılar güncellenir — bizde bağ ve SONUÇ ilkesi) |
| **T-05** nokta bulutu | 217391838, 217391842, 217386679 | XYZ ve Alan Sor (sınıf, RGB, en yüksek/düşük/ortalama/ortanca kot, arama yarıçapı); nokta bulutu üzerinde sayısallaştırma (eksen profilleri, görünüşü kilitle, min/max/ortalama/ortanca kottan yakalama); kitaplıklar kentoscad.md §9.10 (PDAL, laz-perf) |
| **C-16** 2.5B CAD | 217385395, 217385090, 217385404 | N-05'teki Z kullanım listesi; OFSET ve ÇİFTÇİZGİ'de kotları yüzeyden alma; alan işlemlerinde "kot koru / kesenden al"; 3B'de paralel yönü (X/Y/Z) ve en/boy/yükseklik ölçekleme — belge 2.5B kalır, katı modelleme ayrı kapsamdır |
| **L-01** yerleşimi doğrula | 217385081, 217385079, 217385078 | Grid (çerçeve, kenar yazıları, iki sistemde grid); Lejant (kapalı katmanları göster, yalnız görünenler, genişleme yönü, şablon kaydet/yükle; MPYY 2014 UİP/NİP/MSP/ÇDP şablonları [M]); ölçek çubuğu (bölüm sayısı, ilk bölümü ikiye böl) |
| **L-02** veri güdümlü yerleşim | 217388073, 217385179 | Kullanıcı tanımlı proje değişkenleri (yer tutucu olarak); tablolu ve resimli zengin metin öğesi |
| **L-03** atlas ve raporlar [M] | 217389310, 217385879 | Alan Çıktıları biçimleri (özet, noktalı özet, cepheli, koordine özetli, ikinci sistemde, yanılma/tecviz) [M]; Belge ve Raporlar krokileri (ayırma çapı, fen klasörü, ölçü krokisi, durum haritası, ihdas; TKGM 2025/4) [M]; kroki seçenekleri (komşuların içini temizle, komşu adları, parsel kenarından kes + genişleme, köşelere daire, karelaj, cephe yaz) |
| **L-04** PDF, raster teslim | 217388025, 217388055 | GeoTIFF Kaydet; çıktıda tarama renklerini MPYY RGB kodlarına göre ölçekleme [M] |
| **A-06** iş tarifleri, Python | 217385711, 217385635 | Makro Menü Düzenleyici: kullanıcının betiğini şeride sekme, grup ve simgeyle ekleme (komut kaydına kullanıcı komutu olarak; ikinci komut listesi değil); korumalı/lisanslı makro dağıtımı kapsam dışı |
| **O-3** yayı koruyan alan işlemleri | 217385090 | Alan işlemlerinde kot seçenekleri ve çok parçalı sonuç N-08 ile birlikte |

## 5. Şerit ve isimlendirme

Amaç: Netcad'den gelen elin aradığı yerde bulması — ama bizim şeridimizle. Kopyalanmayanlar: Netcad'in
sekme görüntüsü, renkli bağlam başlıkları, simgeleri ve ürün adları. Alınanlar: araç kümeleri, işin sırası
ve Türkçe terimler.

### 5.1 Sekme eşlemesi

| Netcad sekmesi (grupları) | KentOSCad bugün | Öneri |
|---|---|---|
| **Giriş** (Çizim · Sorgu · Düzenleme · Görüntü) 217385142 | **Giriş** (Seçim · Çizim · Değiştir · Açıklama · Katmanlar · Özellikler · Pano) | Sorgu ve Görüntü gruplarını ekle; Pano'yu Değiştir sekmesine taşı (Ctrl+C/V/X her yerde çalışıyor) |
| **Düzenle** (Düzenleme · Dönüşüm) 217385414 | **Değiştir** (Dönüştür · Dizi ve Ofset · Kes ve Uzat · Köşe · Birleştir · Sil ve Temizle) | İçerik uyumlu; Birleştir paneline Alan ▾ (N-08), yeni Kot grubu (N-05); sekme adı Açık soru 4 |
| **Analiz** (Konumsal Analizler; modül grupları) 217385399 | **Analiz** (Tablo · İşlem araçları · Denetim · Yapay zekâ) | "Konumsal" (Tampon, Overlay, Bilgi Al, Birleştir) ve "Topoloji" (denetim + düzeltmeler) panelleri |
| **Araçlar** (Topolojik Düzeltmeler · Veri Araçları · Veritabanı · Geliştirme …) 217385494 | dağınık: Değiştir ▸ Sil ve Temizle, Kadastro ▸ Denetim, Harita ▸ Veri, Analiz | Ayrı "Araçlar" sekmesi **açılmaz**: bizde "Araçlar" işlem araçları panelinin adı, ikinci anlamı karışıklık doğurur; topoloji Analiz'e, veri Çıktı'ya |
| **Detaylar** (Dizi · Tarama · Ölçme · Röleve · Etiket · Çizim) 217385838 | **Açıklama** (Yazı · Ölçü · Etiket) + Çizim ▸ Tarama + Değiştir ▸ Dizi + Çıktı | Açıklama'ya Röleve paneli (N-16), Etiket paneline Koordinat Yaz, Km Yaz, Karelaj; ad "Açıklama" kalır — haritacılıkta "detay" arazi ayrıntısıdır, sekme adında karışır |
| **Görünüm** (Pencere · Çizim Yöntemi · Görünüm Ayarları · Etiket Ayarları · Kalınlık) 217385053 | **Görünüm** (Gezinme · Yardımcılar · Katmanlar · Pencereler · Tema) | "Göster" paneli: renk kipi, dolgu/sınır, nesne ipucu (N-24), dinamik etiketler (G-07) |
| **Hesap** (Editörler · Hesap Araçları · Raporlar) 217384952 | Harita ▸ Jeodezi, Çizim ▸ Nokta ve Alım | **Ölçme** sekmesi: nokta, alım, kesişim, dik ayak, poligon, aplikasyon, oturt, dönüştür, pafta, nokta tablosu |
| **Netsurf** (Şev · Arazi Model · Eğri · Enkesit · Model Üret) 217384995 | Harita ▸ Arazi (Eşyükselti, Hacim) | **Arazi** sekmesi: yüzey, model düzelt, eşyükselti, hacim, profil/enkesit, şev |
| **Netmap / Çap / Netkamu / Nettop** | **Kadastro** (Parsel · Yazım · Denetim) | Kadastro kalır; içeriği [M] kullanıcının tarifinden sonra |
| **Planet** | — | "Plan" sekmesi S-04'ün tarifinden sonra |
| **Gps** | — | Ölçme ▸ GNSS grubu (N-28) |
| **Eski Komutlar** | — | Açılmaz: bizde eski komut yok |
| Bağlam: **Nokta Seçim Araçları**, **Seçim Süzgeci**, **Dağıtım** | Bağlam: Yazı · Ölçü · Tarama · Alan · Çizgi · Eğri · Blok · Blok Düzenleme | **Nokta Girişi** ve **Seçim** bağlam sekmeleri (N-04); Dağıtım S-04 ile |

### 5.2 Somut yeniden gruplama

| # | Değişiklik | Neden |
|---|---|---|
| 1 | Giriş ▸ **Sorgu** (Nesne Bilgisi, Alan Ölç ▾, Koordinat Oku, Ölç ▾) | Netcad eli Alan Sor, XYZ Sor ve Cetvel'i Giriş'te arar (217385177); bugün Harita ▸ Sorgu ve Ölçüm'de |
| 2 | Giriş ▸ **Görüntü** (Kapsam, Pencere, Önceki) | Limit Bul, Pencere Büyüt, Önceki Pencere Netcad'de Giriş'tedir (217385149) |
| 3 | Pano → Değiştir sekmesi | 1440 px bütçesi; kısayollar zaten her sekmede çalışıyor |
| 4 | Harita → **Ölçme** + **Arazi** | Hesap ve Netsurf iki ayrı kas hafızası; Harita bugün beş ayrı işi taşıyor |
| 5 | Veri grubu (Veritabanı, İçe/Dışa Aktar, Dış Referans) → Çıktı ▸ Dosya'nın yanında "Veri" | Harita sekmesinde veri alışverişi aranmıyor |
| 6 | Açıklama ▸ **Röleve**; Etiket'e Koordinat Yaz, Km Yaz, Karelaj | Netcad'in Detaylar ▸ Röleve/Ölçme/Çizim kümesi |
| 7 | Kadastro ▸ Yazım'a Koordinat Yaz; Kadastro ▸ Denetim'den Tampon'u çıkar | Tampon bir analiz aracıdır; kadastro denetimi değildir |
| 8 | Değiştir ▸ Birleştir'e **Alan ▾** ailesi | Netcad'in Alan ▾ (Kesişim, Birleştir, Böl, Çıkart) kümesi |
| 9 | Değiştir ▸ **Kot** grubu | N-05 |
| 10 | Analiz ▸ **Konumsal** ve **Topoloji** | Netcad Analiz ▸ Konumsal ve Araçlar ▸ Topolojik Düzeltmeler |

### 5.3 Eklenecek eş adlar (çakışmasız)

`.names` listesinde ikinci Türkçe ad önceden var (`METİN` + `YAZI`, `SEMBOL` + `SEMBOLLER`); aynı yol.
Her ad çapraz kayıt çakışma kapısından geçer ve 6.14 gereği altı belge yeniden üretilir.

| Netcad adı | Eklenecek ad (ASCII) | Komut |
|---|---|---|
| Paralel | `PARALEL` | `core.offset` (U-01 kabulü bunu zaten istiyor) |
| Ayrıştır | `AYRIŞTIR` (`AYRISTIR`) | `core.explode` |
| Biçim Boya | `BİÇİMBOYA` (`BICIMBOYA`) | `core.match_style` |
| Köşe Yuvarlat | `KÖŞEYUVARLAT` (`KOSEYUVARLAT`) | `core.fillet` |
| Çoklu Doğru | `ÇOKLUDOĞRU` (`COKLUDOGRU`) | `core.polyline` |
| Alan Sor | `ALANSOR` | `core.measure_area` |
| XYZ Sor | `XYZSOR` | `core.coordinate` |
| Cetvel | `CETVEL` | `core.measure` |
| Çizdir | `ÇİZDİR` (`CIZDIR`) | `core.print` |
| Tabaka | `TABAKA` | `core.layer` |
| Tabaka Değiştir | `TABAKADEĞİŞTİR` (`TABAKADEGISTIR`) | `core.set_layer` |
| Limit Bul | `LİMİTBUL` (`LIMITBUL`) | `core.zoom` (varsayılan kip KAPSAM) |
| Kutu | `KUTU` | `core.rectangle` (`SEÇ mod=KUTU` bir kip sözcüğüdür, komut adı değil) |
| Obje Böl | `OBJEBÖL` (`OBJEBOL`) | `core.split` |
| Yan Nokta Hesabı | `YANNOKTA` | `core.perp_offset` |

**Durum (29 Eylül 2026).** On iki ad normal ad olarak eklendi (995b701). `CETVEL`, `TABAKA`
ve `KUTU` komut adı **olmadı**: istemde yazılan ilk sözcük kayıttaysa bekleyen komut
kapanır ve üçü de yazı ya da katman adı olarak yazılır. Kullanıcının kararıyla yalnız
aramada bulunurlar (`CommandSpec::known_as`, f154275); Netcad'in `KAYDIR`'ı da orada,
`TAŞI`'nın bilinen adı olarak.

### 5.4 Çakışmalar — karar gerekir

| Terim | Netcad'de | KentOSCad'de | Risk | Öneri |
|---|---|---|---|---|
| **KES** | budama (trim) 217385423 | `core.cut`: seçimi panoya alır ve **siler** | Netcad eli budamak isterken seçili nesneler silinir (geri alınır, ama şaşırtıcı ve tehlikeli) | En önemli çakışma. (a) KES kalır, istemde ve aramada uyarı; (b) `core.cut`'ın birincil adı `PANOYAKES` olur, `KES` ne pano ne budama olarak kullanılmaz (geçiş süresi boyunca uyarıyla) — Açık soru 4 |
| **KAYDIR** | nesneyi taşı 217385158 | `core.pan`: görünümü kaydırır | Zararsız ama şaşırtıcı | Ad kalır; aramada TAŞI "bilinen adı: Kaydır" diye üstte |
| **HİZALA** | referans nesneye göre yaslama 217385156 | `core.align`: nokta çiftiyle hizalama (Netcad'in 2 Noktadan Dönüşüm'ü) | Beklenen iş gelmez | N-14 `YASLA`; aramada "hizala" ikisini de getirir |
| **BİRLEŞTİR** | uçları birleştir / tek obje yap 217385394 | `core.combine`: alan birleşimi + değen çizgileri tek çizgi | Kısmen örtüşür | Kalır; `combine.md`'de UÇUCA ve YUVARLA r=0 ile yan yana anlatılır |
| **Düzenle** | sekme adı ve "Düzenle (Edit)" çarkı | "Değiştir" sekmesi; bağlam sekmelerinde "Düzenle" paneli; `ÇİZGİDÜZENLE` | Ad karışıklığı | Açık soru 4 |
| **Ctrl+C / Ctrl+N / Ctrl+A** | Çoklu Doğru / Nokta / Alan Sor 217385133, 217385088, 217385205 | Kopyala / Yeni / Tümünü seç | İşletim sistemi kısayolları | İşletim sistemi kısayolları korunur; isteğe bağlı bir "Netcad kısayol profili" — Açık soru 5 |
| **F tuşları** | yakalama anahtarları; yardım metni F4'ü (nokta yakala) doğruluyor 217387852, öteki atamalar doğrulanamadı | F3 yakalama, F8 dik, F9 ızgaraya yakala, F10 yüzey normali | Netcad eli F tuşlarında başka iş bekler | KentOSCad düzeni kalır; profil Açık soru 5'in parçası |
| **Alt+Z · Alt+C** | pencere büyüt · önceki pencere | atanmamış görünüyor (doğrulanmalı) | — | N-01'e atanır |
| **G · T · `*`** | Görünüm sekmesi · katman yöneticisi · yeniden çiz | tek harfli genel kısayol **bilerek** yok | — | Alınmaz: komut satırına yazılan harf komuta kaçmamalı (arayuz.md) |

### 5.5 Terimler

| Netcad terimi | KentOSCad terimi | Not |
|---|---|---|
| obje | nesne | Kılavuz "nesne" der; aramada "obje" de bulunur |
| tabaka / katman | katman | Netcad CAD katmanına "tabaka", referansa "katman" der; bizde tek kavram, `TABAKA` eş ad |
| çoklu doğru | çoklu çizgi | eş ad |
| alan sor · XYZ sor · cetvel | alan ölç · koordinat oku · ölç | eş adlar |
| limit bul · pencere büyüt · önceki pencere | kapsama yakınlaş · pencereyle yakınlaş · önceki görünüm | N-01 |
| çizdir | yazdır | eş ad |
| paralel | ofset (paralel) | eş ad |
| karelaj | karelaj (çizimde, basılır) ≠ ızgara (ekran yardımı, basılmaz) | İkisi ayrı şeydir; kılavuz ikisini ayırır |
| kurp | yay | sözlüğe |
| alinman · cephe · kutur · röleve · kroki | aynı | `docs/sozluk.md`'ye eklenir |
| dik ayak / dik boy | aynı | **işaret kuralı** aynı: sağ pozitif (Açık soru 3, 29 Eylül) |
| semt | semt (azimut) | aynı |
| tecviz / yanılma sınırı | tolerans (katalogdan) | [M] |
| sayısallaştırma kalemi | kalem | G-04 |
| akıllı obje (Grid, Lejant, Ölçek Çubuğu) | çıktı öğesi / bağlı nesne | Açık soru 15 |

## 6. Estetik ve kullanım ilkeleri

Her ilke denetlenebilir yazıldı. "Netcad'de" sütunu araştırmada gözlenen davranıştır; "KentOSCad'in yolu"
bilinçli olarak farklılaştığımız yeri söyler.

| # | İlke | Netcad'de | KentOSCad'in yolu | Nasıl denetlenir |
|---|---|---|---|---|
| 1 | **Diyalog değil satır** | Her araç bir özellik penceresiyle açılır (Paralel, Alan Düzeltme, Karelaj, analizler) | Değerler komut satırında ya da Araçlar kartında; kart göndereceği satırı gösterir (processing.md R13) | Yeni araçta ham `QDialog` yok; `ci-gate-bilesenler.sh` |
| 2 | **"Değişiklikleri Uygula" yok** | Obje Özellikleri ve Toplu Obje Değiştir değişikliği topluca uygular 217385175 | Her hücre bir komut; Enter onaylar, başka yere tıklamak da (arayuz.md) | Panel probu |
| 3 | **Modal sonuç yok** | Çizim bitince "Çoklu Doğru/Alan Özellikleri", noktada "Nokta Bilgisi" penceresi 217385133, 217385088 | Sonuç satırı (alan, çevre, köşe) ve Öznitelikler paneli; nokta adı ve kotu istemde yazılır | Gerçek fare probu: çizim sırasında pencere açılmaz |
| 4 | **Her tıklamanın yazılı karşılığı** | Palet ve çark yalnız fareyle | Palet düğmesi, tutamak seçeneği, bağlam sekmesi komut satırına satır yazar; ipucu o satırı gösterir (5.15, ui.md P7) | Eşitlik kanıtı + "düğme → satır" probu |
| 5 | **Önizleme = sonuç** | Teğet ve daire alternatifleri "sanal" gösterilir | Çözümler soluk hayalet, seçilen kesikli; aynı hesap | `test_preview.cpp` her yeni araç için |
| 6 | **Renk yalnız anlam** | Bağlam sekmelerine sarı başlık, çarka mavi disk | 3 px vurgu başlığı; mavi = seçim/aktif/birincil, turuncu = yakalama/kilit (design.md §1–2) | Tek stil sayfası (ui.md R49); bileşen kapısı |
| 7 | **Simge dili bizim** | Tek renkli çizgi simgeler ve vurgu rengi | Rol renkli yol simgeleri (`icons.cpp`): şekil mavi, kesilen kırmızı, yazılan turuncu, veri sarı, eklenen yeşil. Netcad simgesi, adı ve ekran düzeni kopyalanmaz. Röleve: taban mürekkep + ölçü turuncu; şev tara: şekil mavi | Simge ayrışma probu (U-01'de 42 simge ayrıştı) |
| 8 | **Varsayılan pafta ölçeğinden** | Yardım metni "1/1000'de 2 mm" gibi kurallar anlatır | Kâğıt ölçüsü × `plan_ölçeği` (processing.md R10); yönetmelik değeri katalogda [M] | `ci-gate-hardcoded-thresholds.sh` |
| 9 | **Katman adı sabit değil** | Çıktılar PKN, KÖŞENO, GRID, SEVALT, CIZPEN, HATALI gibi tabakalara kendiliğinden gider | Çıktı katmanı bir parametredir, varsayılanı kartta görünür; resmî sınıf katmanları katalogdan | Her üretici araçta `katman=` |
| 10 | **Sessiz dönüştürme yok** | Keserken ve paralel alırken yumuşatılmış eğri ve spiral çoklu doğruya döner 217385423, 217385395 | Tür korunur ya da sapma söylenir (TODOS §1 ilkesi) | Destek matrisi ◐ hücreleri |
| 11 | **Yazı kaynağına bağlı** | Cephe, koordinat, röleve yazıları düz yazıdır | Yazı kaynağını izler ya da "güncel değil" der (model.md R46, R46e) | `BAĞIMLILIK` |
| 12 | **Tek işaret kuralı** | Sağ pozitif (Yan Nokta, Bina Oluştur, Enkesit, KM) | Dik boy, sapma, kesit ofseti, derinlik ve OFSET tarafı tek kuraldan (Açık soru 3) | `komut-satiri.md` işaret bölümü ve testler |
| 13 | **Tek harf yalnız istemde** | Çark ve istemlerde harfler (K, D, P, U, R, Y, G, Z, O, J) | Harfler istem seçeneğidir; genel tek harf kısayolu yok | Kısayol probu (her tuş tek eylem) |
| 14 | **Sağ tık ve Esc bırakır** | Nokta seçimini sağ tuş ya da Esc bitirir 217386629 | Zaten kural (ui.md R52); yeni araçlarda korunur | R52 probları |
| 15 | **Sayı yazınca komut satırı** | Rakama basınca küçük bir giriş kutusu açılır 217386622 | Odak komut satırında (ui.md R53); değer tek yerde | İstem probu |
| 16 | **Birim değerin yanında** | Pencerelerde birim etikette | Birim değerin yanında ve parametre açıklamasında (`llms.txt` kuralı) | Docs kapısı |
| 17 | **Sığma** | Şerit geniş; modül sekmeleri çoğalır | Her sekme 1440 px, şerit ≤ 134 px (U-07) | `KENTOS_FIT_PROBE` |
| 18 | **Klavye ve ekran okuyucu** | — | Her düğme Tab zincirinde, `accessibleName` (ui.md R21–R22, 6.9) | `KENTOS_ACCESS_PROBE` |
| 19 | **Varsayılana dönüş görünür** | Çizim Hesap Araçları ayarlarında "Varsayılanları Geri Yükle" ve "Varsayılan Yap" 217387846; Mimar'da "Varsayılanları Geri Sil" 217385530 | Araçlar kartı son değerleri günlükten getirir; karta Ghost rolünde "Varsayılanlar" düğmesi | Kart probu |
| 20 | **Bulguya gidilir** | Geometri Kontrol hataları Mesajlar'da, "Yaklaş" ile 217385444 | Bulgular paneli; satır → yakınlaş ve seç (N-21) | TOPOLOJİ probu |

## 7. 3B'ye evrim

### 7.1 Netcad'in 3B ve arazi yetenekleri → KentOSCad işi

| Netcad | KentOSCad işi | Önce gereken veri |
|---|---|---|
| Nokta seçiminde Z'nin modelden okunması 217385088 | Nokta istemi yüzeyden kot alır — yalnız nişanlanan noktada (command.md R9a), yazılan noktada değil | N-05 + T-01 |
| Kotları Modelden Al (Paralel), Kot Değerlerini Koru (Alan) 217385395, 217385090 | OFSET, ÇİFTÇİZGİ, alan işlemlerinde `kot=yuzey`, `kot=kaynak` ya da `kot=kesen` | N-05 (+ T-01) |
| Kot Sıfırla, geçersiz Z denetimi 217388066, 217385444 | `KOTVER`, TOPOLOJİ'de Z denetimi | N-05 |
| Eğik mesafe, eğim, 3B cetvel 217385770, 217385201 | ÖLÇ, UZUNLUKYAZ, PRİZMA'da 3B uzunluk ve eğim | N-05 |
| Üçgen model, model düzelt, model kontrolü 217385374, 217385375, 329154828 | Kalıcı yüzey türü (T-01) | N-05 + yeni tür (üçgen dizini payload'ı, R9a) |
| Eğri geçir, hızlı eğri 217385338 | T-02; eğriler yüzeyin SONUCU | T-01 |
| Hacim: iki yüzey, enkesit, tabana göre 217392988 | T-03 | T-01 |
| Güzergah, km, enkesit, profil 217385340, 217393227 | T-03, T-04; güzergah kaynak, kesit ve profil sonuç | N-05 + güzergah türü |
| Şev taraması, platform ekle 217385372, 229905076 | N-19 (plan), T-01 kırık hat, T-04 şev | N-05 |
| Nokta bulutu sorgu ve sayısallaştırma 217391838, 217386679 | T-05 (PDAL, laz-perf) | Ayrı depo; belgeye kopyalanmaz, referanstır |
| 3D+ görüntüleme, doku, simülasyon 217389385, 320016527 | N-29 | N-05, T-01, G-08 |
| Bina kütleleri (kalınlık = yükseklik) 229905076 | Ekstrüzyon görüntüsü (OCCT); CityGML sonra (libcitygml, kentoscad.md §9.10) | `yukseklik` özniteliği; katı model belgeye girmez |

### 7.2 Veri modelinin önce ihtiyaç duydukları (sırayla)

1. **İsteğe bağlı tepe kotu sütunu** (N-05). Kesme bloğu değişmez; kotsuz belgenin baytı değişmez.
2. **Yüksekliğin türü.** `Crs` zaten jeoit modeli ve düşey datum taşıyor (model.md R36); kot yazan her
   komut hangi yüksekliği yazdığını kaydeder (ortometrik ya da elipsoidal), dönüşüm jeoit ızgarasıyla
   (domain.md R13).
3. **Kot hesap kuralları `core.md`'de.** Plan uzunluğu ve 3B uzunluk ayrı işlevler; enterpolasyon tam sayı
   mm ve belirlenmiş sırayla (§7.3 determinizm); XY dönüşümleri Z'ye dokunmaz.
4. **Yüzey türü** (T-01): üçgenler kaynak noktalara anahtarla bağlı (bağ ve köken), kendi payload'ı (R9a),
   kırık hat ve sınır kısıtları CDT ile.
5. **Girdi yardımında Z:** yakalanan köşe kotunu getirir; yeni "yüzeye yakala" kipi.
6. **Güzergah türü** (T-04): elemanlar (doğru, yay, klotoid) ve km başlangıcı; kesitler sonuç.
7. **3B sahne kurucusu ayrı** (N-29): 2B kesme ve çizim hattı olduğu gibi kalır.

### 7.3 Sınırlar

- Belge **2.5B** kalır: katı modelleme ayrı kapsamdır (C-16'nın notu). OCCT'nin 3B yetenekleri yalnız
  görüntü ve ileride açıkça kararlaştırılmış işler için.
- İlk 3B görünümde düzenleme yoktur; seçim ve ölçüm vardır.
- Nokta bulutu belgeye kopyalanmaz; dış kaynaktır (T-05).

## 8. Sıra ve kilometre taşları

Her paket bir commit; kutu yalnız kabul kanıtıyla kapanır (TODOS §4 sözleşmesi). [M] işleri kullanıcı tarif
etmeden başlamaz.

| Kilometre taşı | İçerik (sırayla) | Çıkış kanıtı |
|---|---|---|
| **M1 — Netcad eli** (P0 hızlı kazanımlar) | U-01 Netcad dilimi (eş adlar, bilinen adlar, son komutu yinele) → N-01 → N-02 → N-03 → N-04 | Netcad kullanan bir mühendis on günlük işi (liste Q-05'te) ad aramadan yapar; problar 0 kusur |
| **M2 — Kotlu nokta** | Açık soru 2 kararı → N-05 → S-01·a–e → N-06 → N-07 | Nokta listesi oku → nokta adıyla çiz → noktayı düzelt → bağlı çizgiler izler → kaydet/aç kot korunur (altın senaryo) |
| **M3 — CAD eşitliği** | O-3 → N-08 → N-09 → O-5, O-6 → N-10 → N-11 → N-12 → N-13 → (N-14) | Destek matrisinde çok parçalı satır ölçülür, ◐ sayısı düşer; E1, E2 senaryoları |
| **M4 — Kroki ve pafta** | N-15 → N-16 → N-18 → N-19 → (katalog gelince) N-17 → N-20 | Bir ölçü krokisi ve pafta çıktısı uçtan uca (kroki biçimi [M] kullanıcıdan) |
| **M5 — Arazi (2.5B)** | C-16 → T-01 Netcad dilimi → T-02 → T-03 | E9 senaryosu: Üçgen Oluştur + Model Düzelt + Eğri Geçir + Hacim akışı |
| **M6 — CBS ve proje** | G-03 dilimi → G-10 dilimi → G-05 dilimi → G-04 dilimi → N-21 → N-22 → N-23 | E6 senaryosu: Overlay + Bilgi Al + Kolon Doldur + Bulgular |
| **M7 — Uzmanlık ve 3B** | S-02 dilimi → T-04 → T-05 → N-24 → N-25 → N-26 → N-28 → N-29 → (tariften sonra) S-03, S-04, N-27 | E10 senaryosu; 3B görünüm bütçesi ölçülüp yazıldı |

## 9. Açık sorular

1. **Hangi Netcad modülleri önce?** Hesap, Netsurf, Netmap, Planet, Netpro, Netkamu, Nettop, Çap, 3D+ —
   sıra M4–M7'yi belirler.
2. **Tepe kotu sütunu (N-05):** model.md'ye alan eklenmesi, dosya sürümü (`min_reader_version`) ve `kot`
   özniteliğinden göç onaylanıyor mu?
3. **Dik boy işaret kuralı.** Netcad yardımı dört ayrı sayfada **sağ pozitif, sol negatif** diyor: Yan Nokta
   Hesabı ("dik boy sol tarafta kalıyorsa değeri eksi girilmelidir", 217389335), Bina Oluştur ("+ sağa, −
   sola", 217385353), Enkesit Editörü ("sol taraf negatif, sağ taraf pozitif", 217389314), KM yakalama
   (217386679). KentOSCad'de `dik()` ve `DİKAYAK` **sol pozitif** ve `docs/komutlar/komut-satiri.md`
   ("Dik ayak ve dik boy — işaret kuralı") ile `TODOS-CAD.md` P1a-6 bunu "Netcad'deki kuralın aynısı"
   diye yazıyor — araştırma bunu doğrulamıyor. Seçenekler: (a) Netcad kuralına geçmek, (b) kuralı koruyup
   belgeyi düzeltmek, (c) proje ayarı. Günlük çözülmüş noktayı tuttuğu için eski günlükler etkilenmez;
   etkilenen canlı metin, betik dizeleri ve yapay zekâdır. N-02, N-13, N-16 ve `boyunca()` bu karara bağlı.
   **Karar (29 Eylül 2026): (a) — Netcad gibi sağ pozitif.** `core::perpendicular_offset`, `dik()`,
   `DİKAYAK` ve belgeler değişti; N-02, N-13, N-16 ve `boyunca()` bu kuralla yazılır.
4. **Ad çakışmaları:** `KES` (bizde panoya kesip siler, Netcad'de budar) ne olsun? "Değiştir" sekmesi
   "Düzenle" olsun mu? `KAYDIR` görünümde kalsın mı?
5. **Netcad kısayol profili** (Ctrl+C = Çoklu Doğru, Ctrl+N = Nokta, Ctrl+A = Alan Sor, F tuşlarındaki yakalama anahtarları — yalnız F4 doğrulandı)
   isteğe bağlı olarak sunulsun mu?
6. Kılavuzda **"Netcad'den gelenler için"** adlı, Netcad adını anan bir karşılaştırma sayfası olsun mu, yoksa
   yalnız nötr "bilinen adlar" mı?
7. **Pafta bölümleme ve adlandırma kataloğu** (N-17): kaynağı ve uzman onayı sizden mi gelecek?
8. **BÖHHBÜY gösterim kataloğu** (bina taraması, şev aralıkları, balastro, tek taraflı çizgiler, nokta
   önekleri): kim hazırlayacak, hangi sürüm?
9. **Hacim yöntemleri** (TCK, DSİ, ortalama alan, prizmoidal): hangileri zorunlu, rapor biçimi ne?
10. **Netcad dosyaları:** hangi biçimler önce (NCN, CKS, PAF, DNS, NCK/PO5, MIR/YDE/PRZ, KTB/KSE, NCZ)?
    Elinizde örnek dosya var mı (I-02)?
11. **Total station ham verisi:** hangi markalar önce?
12. **Canlı GNSS** masaüstünde gerekli mi (N-28)?
13. **Çevrimiçi geocoding** (sağlayıcı anahtarı, gizlilik): isteniyor mu?
14. **3B görünüm** (N-29): 2B üretim işlerinden önce mi sonra mı?
15. Grid, lejant ve ölçek çubuğu **çizimde akıllı nesne** olarak da istenir mi, yoksa çıktı yerleşiminde
    kalmaları yeterli mi?
16. **İnceleme notları** (N-25): belge eşyası mı, katman nesnesi mi?
17. **İFRAZ, ALANİFRAZ, TEVHİT** ve Netmap'in öteki işlemleri: tarifi ne zaman? (Kullanıcı "şimdi kalsın"
    demişti; S-03 ve S-04 o zamana kadar yalnız bu plandaki arayüz/geometri listesini taşır.)
18. **Taramanın altında kalan yazı ve semboller (N-03).** `TARAMA yontem=ic` bölgenin içindeki kapalı
    çizgileri boş bırakıyor; yazıları, blokları ve noktaları bırakmıyor. Seçenekler: (a) AutoCAD gibi
    **kendiliğinden** — bölgedeki her yazının kutusu ve her sembolün çerçevesi bir pay bırakılarak delik
    olur; (b) Netcad gibi **seçerek** — "Diğer Objeler Seç"in karşılığı `disarida=<nesneler>`, yalnız
    gösterilenler boş kalır; (c) ikisi, (a) öntanımlı ve `yazilar=hayır` ile kapatılır. Pay (metre ya da
    yazı yüksekliğinin katı) katalog değeri mi, parametre mi?

## 10. Kaynak notları

- **Okunan Netcad sayfaları:** eşleme tablosunda `pageId`'si geçen her sayfa ham dökümden okundu. Dökümde
  olmayan 32 sayfa (Netsurf'ün arazi modeli, eğri, enkesit ve hacim sayfaları; Planet'in yol, kavşak, köşe,
  refüj, 3D Simülasyon ve PlanGML sayfaları; NETCAD/3D+ ve NETPRO ana sayfaları) resmî REST adresinden
  alınıp araştırma klasörüne kaydedildi.
- **Doğrulanamayanlar:** önceki raporun ekran görüntüsüne dayanan görsel iddiaları (8.6 sekme sırası, renk
  kodları, 32/16 piksel boylar, açık menü içerikleri) ve üçüncü taraf kısayol listesi; F5–F11 yakalama
  tuşlarının varsayılanları (yalnız F4 yardım metninde geçiyor); Dronet, EPlanet, Yapınet, Water, Atıksu,
  Mine sekmelerinin içeriği; NETPRO, NETÇAP, NETKAMU ve NETTOP'un alt sayfaları (yalnız dizin sayfaları
  okundu); Karo Oluşturucu ve Katalog'un ayrıntıları. Plan bunlara dayanmaz.
- **KentOSCad'de bulunan iki belge tutarsızlığı** (874ce64'te, 28 Eylül 2026'da düzeltildi): `docs/komutlar/komut-satiri.md`
  işaret kuralı bölümü Netcad'in kuralını yanlış aktarıyordu (Açık soru 3; kural 29 Eylül'de Netcad'inkine
  geçti) ve aynı bölüm "Aynı işin fareyle yapılan hâli P1b'de `DİKAYAK` komutu olarak gelecek" diyordu.
