# KentOSCad - Türkiye Odaklı CBS + CAD Harita Yazılımı — Teknik Referans ve Yol Haritası

**Sürüm 2** · Ağustos 2026

**Kararlar:** GPLv3 açık kaynak · C++20 taban · Qt 6 arayüz · **komut merkezli mimari** · gömülü script motoru · AI destekli çizim · Linux / Windows / macOS · BÖHHBÜY, MPYY, TUCBS, TKGM uyumu.

> **Lisans uyarısı:** Lisans notları yön göstericidir. Bağımlılığı sürüme sabitlediğinizde o sürümün `LICENSE` dosyasını okuyup SBOM'a kaydedin.

---

## İçindekiler

1. [Temel Lisans Kararı](#1-temel-lisans-kararı-gplv3)
2. [Mimari Omurga: Komut Merkezli Tasarım](#2-mimari-omurga-komut-merkezli-tasarım)
3. [Komut Satırı](#3-komut-satırı)
4. [Script Motoru](#4-script-motoru)
5. [AI Katmanı](#5-ai-katmanı)
6. [Arayüz Teknolojisi: Qt mi, Başkası mı?](#6-arayüz-teknolojisi-qt-mi-başkası-mı)
7. [Dil, Derleyici, Standart](#7-dil-derleyici-standart)
8. [Derleme ve Bağımlılık Yönetimi](#8-derleme-ve-bağımlılık-yönetimi)
9. [Kütüphane Listesi](#9-kütüphane-listesi)
10. [Performans](#10-performans-somut-teknikler)
11. [Yol Haritası](#11-yol-haritası)
12. [Türkiye Mevzuatı Kontrol Listesi](#12-türkiye-mevzuatı--gereksinim-kontrol-listesi)
13. ["Dünya Standardı" Somut Kontrol Listesi](#13-dünya-standardı--somut-kontrol-listesi)
14. [Ekip ve Süreç](#14-ekip-ve-süreç)
15. [Riskler](#15-riskler-ve-karşı-önlemler)
16. [İlk 30 Gün](#16-ilk-30-gün--yapılacaklar)

---

## 1. Temel Lisans Kararı: GPLv3

Bu kararı ilk gün verin; kütüphane setinin tamamını belirliyor.

| Bağımlılık                                                                      | Lisansı    | GPLv2-only ile           | GPLv3 ile |
| ------------------------------------------------------------------------------- | ---------- | ------------------------ | --------- |
| CGAL (çekirdek)                                                                 | GPLv3+     | ❌                        | ✅         |
| LibreDWG                                                                        | GPLv3+     | ❌                        | ✅         |
| Qt Charts / DataVisualization                                                   | GPLv3      | ❌                        | ✅         |
| Apache 2.0 lisanslı her şey (oneTBB, OpenCV, Crashpad, OpenSSL 3, ONNX Runtime) | Apache 2.0 | ❌ patent maddesi çakışır | ✅         |

**Karar: GPLv3-or-later.** Sunucu/web bileşeni planlıyorsanız **AGPLv3**.

**Yan kararlar:** Katkı kabul etmeden önce DCO (basit) veya CLA (esnek ama caydırıcı) seçin — CLA olmadan ileride lisans değiştiremezsiniz. İsim ve logoyu tescil ettirin; kod GPL olsa da marka sizin kalır.

---

## 2. Mimari Omurga: Komut Merkezli Tasarım

Bu, projenin en doğru kararı. Doğru uygularsanız script motoru, komut satırı, AI, makro kaydı, undo/redo, regresyon testi ve gelecekteki işbirliği özelliği **aynı altyapıdan bedavaya gelir**. Yanlış uygularsanız hepsini ayrı ayrı yazarsınız.

### 2.1 Tek Kural

> **Uygulamanın durumunu değiştiren her şey bir komuttur. Arayüz, komut veri yolunun sadece bir istemcisidir.**

```
   GUI butonu ─┐
   Komut satırı ─┤
   Script ─────┼──►  KOMUT VERİ YOLU  ──►  Doğrulama ──►  İşlem (Transaction) ──►  Doküman
   AI ─────────┤         (Command Bus)                          │
   Toplu iş ───┘                                                └──►  Günlük (Journal)
```

Bu diyagramdaki hiçbir istemcinin ayrıcalığı yok. GUI butonu, `ÇİZGİ` komutunu tam olarak script'in çağırdığı gibi çağırır.

### 2.2 Komut = Veri, Fonksiyon Değil

Komut çağrısı **serileştirilebilir** olmalı. Bu tek karar şunları açar:

| Özellik                           | Nasıl bedavaya geliyor                             |
| --------------------------------- | -------------------------------------------------- |
| Undo/redo                         | Komut günlüğünde ileri/geri yürüme                 |
| Makro kaydı                       | Günlüğü dosyaya yaz                                |
| Script                            | Günlüğe komut üret                                 |
| AI                                | Günlüğe komut üret (aynı yol)                      |
| Regresyon testi                   | Günlüğü oynat, çıktıyı golden data ile karşılaştır |
| Çökme kurtarma                    | Son kayıttan itibaren günlüğü oynat                |
| Uzak API / toplu işleme           | Günlüğü ağdan al                                   |
| İleride çok kullanıcılı düzenleme | Komut akışı zaten CRDT'ye yakın                    |

```json
{
  "cmd": "core.line",
  "args": { "noktalar": [[485320.150, 4310220.400], [485380.000, 4310250.000]] },
  "crs": "TUREF/TM30",
  "katman": "SINIR"
}
```

### 2.3 Komut Tanımı — Tek Kaynak

Her komut tek bir yerde tanımlanır; komut satırı yardımı, script bağlaması, AI araç şeması ve dokümantasyon bu tanımdan **üretilir**. Elle senkronize edilen ikinci bir liste olmamalı.

```cpp
COMMAND(line) {
    .id        = "core.line",
    .names     = { "ÇİZGİ", "LINE", "Ç", "L" },     // TR + EN + kısaltmalar
    .kategori  = Kategori::Cizim,
    .params    = { Param::points("noktalar", Arity::atLeast(2)) },
    .undo      = UndoPolicy::TekIslem,
    .flags     = Flags::Etkilesimli | Flags::Scriptable | Flags::AiErisimli,
    .ozet      = "İki veya daha fazla nokta arasında doğru parçaları çizer.",
};
```

`Flags::AiErisimli` tek satır. AI'nın araç kataloğu bu bayrağa göre otomatik üretilir. Yeni komut yazdığınızda AI onu **otomatik olarak** öğrenir.

### 2.4 Etkileşimli Komutlar: C++20 Coroutine Kullanın

CAD komutları doğası gereği "sor–bekle–sor" akışıdır. Klasik çözüm elle yazılmış durum makinesidir ve okunmaz. Coroutine ile aynı akış düz kod olur:

```cpp
CommandTask LineCommand::run(CommandContext& ctx) {
    auto p1 = co_await ctx.nokta("İlk nokta");
    if (!p1) co_return;                       // ESC

    auto onceki = *p1;
    while (auto p2 = co_await ctx.nokta("Sonraki nokta", RubberBand{onceki})) {
        ctx.uygula(CreateLine{onceki, *p2});  // undo kaydına yazılır
        onceki = *p2;
    }
    co_return;
}
```

`ctx.nokta()` girdinin **nereden geldiğini bilmez**: fare tıklaması, klavyeden koordinat, script'in sıradaki argümanı veya AI'nın ürettiği değer. Aynı komut kodu dört bağlamda da çalışır. Bu, mimarinin en kritik detayı.

### 2.5 İşlem (Transaction) Sınırı

- Bir komut = bir undo adımı (varsayılan)
- Bir script bloğu veya AI önerisi = tek bir birleşik undo adımı
- İşlem sırasında doğrulama hatası → tam geri alma, kısmi uygulama yok
- Kadastro/imar verisinde yarım uygulanmış işlem kabul edilemez

### 2.6 Doğrulama Katmanı — Girdi Kaynağından Bağımsız

Topoloji kontrolü, mevzuat kuralları ve geometri geçerliliği **komut veri yolunda** çalışır, arayüzde değil. AI'ın veya script'in kuralları atlaması mümkün olmamalı. Kural motorunun kendisi veri odaklıdır (bkz. §9.9).

---

## 3. Komut Satırı

AutoCAD'in komut satırı 40 yıllık rafinasyondur; kullanıcılar oradan gelecek. Küçümsemeyin — bu ayrı bir mühendislik işidir.

**Gerekenler:**

- Çift dilli komut adları (TR birincil, EN eşdeğer) + kısaltma çözümü
- Yazarken satır içi otomatik tamamlama ve parametre ipucu
- Geçmiş (yukarı ok), arama (Ctrl+R), transkript penceresi
- Komut ortasında koordinat girişi: mutlak `485320,4310220`, göreli `@50,30`, kutupsal `@100<45`
- Komut ortasında değiştirici: `LINE` çalışırken `ORTA` (osnap geçici geçersiz kılma)
- Kullanıcı tanımlı takma adlar (`alias.json`)
- İfade değerlendirme: `@(100*3),0` gibi satır içi hesap
- Şeffaf komutlar: `ZOOM`, `PAN` başka komut çalışırken araya girebilmeli
- Hata mesajları eyleme dönük olmalı: "Geçersiz nokta" değil, "Beklenen: 2 sayı veya nesne yakalama. Girilen: 'abc'"

**Uygulama notu:** Komut satırı ayrıştırıcısı, script motorunun ayrıştırıcısıyla **aynı gramer** olmalı. İki ayrı ayrıştırıcı yazarsanız davranış farkları kaçınılmaz olarak ortaya çıkar.

---

## 4. Script Motoru

### 4.1 İki Katmanlı Yaklaşım (Öneri)

Tek bir dil her ihtiyacı karşılamıyor. İki katman kullanın:

| Katman                    | Dil                                | Nerede                                                                                   | Neden                                                                 |
| ------------------------- | ---------------------------------- | ---------------------------------------------------------------------------------------- | --------------------------------------------------------------------- |
| **Gömülü / sıcak yol**    | **Lua (sol2)** veya **QuickJS-ng** | İfade değerlendirici, stil kuralları, etiket ifadeleri, alan hesaplayıcı, hafif makrolar | ~200 KB, milisaniyede binlerce çağrı, tam sandbox, ek kurulum yok     |
| **Ekosistem / otomasyon** | **Python (pybind11)**              | Eklentiler, toplu işleme, veri boru hatları, bilimsel analiz                             | CBS dünyasının ortak dili. QGIS/ArcPy bilen herkes ilk günden üretken |

**Neden ikisi de:** Python'u etiket ifadesi için her satırda çağıramazsınız — GIL ve çağrı maliyeti öldürür. Lua'yı da NumPy/GDAL/scikit-learn ekosistemi yerine koyamazsınız.

### 4.2 Python Entegrasyon Notları

- **Opsiyonel modül olarak dağıtın.** Çekirdek uygulama Python olmadan da çalışsın. Kurulum boyutunu 150 MB şişirmeyin.
- Uzun süren script'i **ayrı thread veya ayrı süreçte** çalıştırın; UI donmasın, iptal edilebilsin (`stop_token`).
- Sürüm sabitleyin (örn. CPython 3.12) ve gömülü olarak dağıtın. Sistem Python'una asla güvenmeyin — Linux dağıtımları arasındaki farklar destek kâbusudur.
- `pip` ekosistemine izin veriyorsanız izole bir sanal ortamda tutun.

### 4.3 Script API Tasarımı

```python
import harita as h

kat = h.katman("PARSEL")
buyuk = kat.sorgu("alan > 5000")

for p in buyuk:
    h.komut("OFSET", nesne=p, mesafe=-3.0)   # komut veri yoluna gider

h.disa_aktar("cikti.gpkg", katmanlar=["PARSEL"], crs="TUREF/TM30")
```

**İlkeler:**
- Script çekirdek nesnelere doğrudan pointer almaz. Her mutasyon `h.komut()` üzerinden geçer — undo, doğrulama ve günlük otomatik çalışır.
- Okuma API'si zengin ve doğrudan olabilir (performans için).
- Sandbox seviyeleri: `güvenli` (dosya sistemi/ağ yok), `proje` (proje dizini), `tam` (kullanıcı onaylı).
- İmzalı eklenti deposu. İmzasız eklenti bariz uyarıyla yüklensin.

---

## 5. AI Katmanı

### 5.1 Temel İlke

> **AI geometriye dokunmaz. AI komut üretir.**

Komut merkezli mimariyi kurduysanız AI entegrasyonu neredeyse ücretsizdir: araç şeması komut kataloğundan üretilir, üretilen komutlar aynı doğrulama ve undo yolundan geçer.

```
Kullanıcı: "Yola cepheli parsellerde 5 metre çekme mesafesi oluştur"
   │
   ▼
[Bağlam toplama]  ── okuma araçları ──►  katman özeti, seçim, aktif CRS, mevzuat bağlamı
   │
   ▼
[Model]  ──►  komut dizisi önerisi
   │
   ▼
[Doğrulama]  ── şema + parametre + topoloji + mevzuat kuralı
   │
   ▼
[Önizleme]  ── kullanıcı çizimde vurgulanmış sonucu görür
   │
   ▼
[Onay]  ──►  tek işlem olarak uygula  ──►  denetim kaydı
```

### 5.2 Kesinlikle Uyulacak Kurallar

1. **Otomatik uygulama yok.** AI'nın ürettiği her komut dizisi önizlenir ve kullanıcı onaylar. Kadastro ve imar çıktısı hukuki belgedir.
2. **Tek undo adımı.** Onaylanan AI önerisinin tamamı tek `Ctrl+Z` ile geri alınabilmeli.
3. **Denetim kaydı zorunlu.** Prompt, model kimliği ve sürümü, üretilen komutlar, kullanıcı kararı, zaman damgası. "Bu çizgi neden burada?" sorusuna cevap verebilmelisiniz.
4. **Sorumluluk sınırı net.** BÖHHBÜY'e göre üretim kontrolü harita/geomatik mühendisinin sorumluluğundadır. AI imza atamaz. Arayüz bunu belirsiz bırakmamalı — AI çıktısı "öneri" olarak etiketlenmeli.
5. **Koordinat halüsinasyonuna karşı sertlik.** Model asla ham koordinat uydurmamalı. Konum her zaman bir araçtan gelmeli (`nesne_seç`, `kesişim_bul`, `ofset_hesapla`), modelin metninden değil.

### 5.3 Bağlam Yönetimi

Çizimi prompt'a **dökmeyin**. 500 bin parselin koordinatı bağlam penceresine sığmaz ve sığsa bile faydasızdır.

Bunun yerine modele **sorgulanabilir okuma araçları** verin:

| Araç                            | Döndürdüğü                                |
| ------------------------------- | ----------------------------------------- |
| `katmanlari_listele()`          | Ad, geometri tipi, nesne sayısı, CRS      |
| `oznitelik_semasi(katman)`      | Alan adları ve tipleri                    |
| `sorgula(katman, ifade, limit)` | Eşleşen nesne kimlikleri ve özet          |
| `secimi_al()`                   | Kullanıcının aktif seçimi                 |
| `gorunum_bilgisi()`             | Ölçek, kapsam, aktif CRS                  |
| `mevzuat_ara(sorgu)`            | RAG ile mevzuat metninden ilgili maddeler |

### 5.4 Yerel Model Desteği — Pazarlanabilir Bir Zorunluluk

Kadastro ve imar verisi hassastır; birçok kurum bunu yurt dışındaki bir API'ye gönderemez. **Yerel model desteği tercih değil, gereklilik.**

| Bileşen                     | Kütüphane                       | Lisans                  |
| --------------------------- | ------------------------------- | ----------------------- |
| Yerel LLM çalıştırma        | **llama.cpp**                   | MIT                     |
| Yerel model sunucusu        | **Ollama** / **vLLM**           | MIT / Apache 2.0        |
| ONNX model çalıştırma       | **ONNX Runtime**                | MIT                     |
| Vektör arama (RAG)          | **sqlite-vec** veya **usearch** | Apache 2.0 / Apache 2.0 |
| Bulut sağlayıcı soyutlaması | `libcurl` + JSON                | —                       |

Mimaride sağlayıcıyı soyutlayın: aynı arayüz arkasında yerel llama.cpp, kurum içi vLLM sunucusu veya bulut API'si. Kurumsal kullanıcı hangisini seçeceğine kendisi karar versin.

### 5.5 Ayırt Edici Özellik: Mevzuat RAG'ı

Bu, uluslararası hiçbir CBS'nin yapamayacağı şey ve ürünün en güçlü satış argümanı olabilir.

BÖHHBÜY, MPYY (ekleri ve detay katalogları dahil), Planlı Alanlar İmar Yönetmeliği, 3194 sayılı Kanun, TUCBS tanımlama dokümanları ve TKGM genelgelerinden oluşan bir korpus kurun; gömme (embedding) indeksi çıkarın ve şu tür soruları cevaplayabilir hale getirin:

- "Bu plandaki eğitim alanı asgari büyüklüğü yönetmeliğe uygun mu?"
- "Bu gösterim Bakanlıkça ilan edilmiş mi, hangi tarihte eklendi?"
- "18. madde uygulamasında DOP oranı üst sınırı nedir?"

**Kritik:** Cevap her zaman **kaynak madde referansı** ile verilmeli. Madde numarası ve yayım tarihi olmadan verilen cevap bu alanda değersizdir, hatta tehlikelidir.

### 5.6 Türkçe Alan Dili

- Komut eşlemesi Türkçe olmalı: "üç metre içeri kaydır" → `OFSET mesafe=-3`
- Alan terimleri sözlüğü: ifraz, tevhit, ihdas, DOP, TAKS, KAKS, nazım, uygulama imar planı, muhdesat, irtifak
- Model seçiminde Türkçe performansını ayrıca test edin; İngilizce kıyaslarda iyi olan model Türkçe alan terminolojisinde zayıf olabilir
- Değerlendirme seti: 200-300 gerçek Türkçe kullanıcı isteği ve beklenen komut dizisi. Regresyon takibi için CI'da çalıştırın

---

## 6. Arayüz Teknolojisi: Qt mi, Başkası mı?

Doğrudan cevap: **Qt 6'da kalın.** Ama nasıl kullanacağınıza dair net bir görüş var.

### 6.1 Alternatiflerin Değerlendirmesi

| Seçenek                       | Artı                                                                                                              | Eksi                                                                                                                     | Bu proje için                  |
| ----------------------------- | ----------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ | ------------------------------ |
| **Qt 6 Widgets + QRhiWidget** | Olgun, yerleşik, tam erişilebilirlik, tam i18n, yüksek DPI, çoklu monitör, yazdırma, yerel menüler, dev ekosistem | Widget çizimi CPU tarafında; stil biraz eski görünümlü                                                                   | ✅ **Seçim**                    |
| Qt Quick / QML (tam uygulama) | GPU kompozisyon, akıcı animasyon, modern görünüm                                                                  | Yoğun araç çubuğu / diyalog / tablo ağırlıklı masaüstü uygulamasında acı verir; olgun docking yok; erişilebilirlik zayıf | Kısmi kullanım                 |
| **Dear ImGui**                | Aşırı hızlı, geliştirmesi çok kolay                                                                               | Yerel değil, erişilebilirlik yok, IME/Türkçe girdi zayıf, ekran okuyucu yok, kurumsal masaüstü ürün için uygun değil     | ⚠️ Sadece hata ayıklama katmanı |
| **Slint**                     | Modern, GPU, C++ API, GPL seçeneği var                                                                            | Genç, karmaşık widget seti eksik, ekosistem küçük                                                                        | ❌ Henüz değil                  |
| Electron / Tauri + web        | Kolay işe alım, zengin UI ekosistemi                                                                              | CAD canvas'ı için yine yerel katman gerekir; bellek ve gecikme maliyeti; dosya sistemi entegrasyonu zayıf                | ❌                              |
| wxWidgets                     | Yerel görünüm                                                                                                     | Ekosistem ve modern grafik desteği zayıf                                                                                 | ❌                              |

### 6.2 Neden Qt

- CAD/CBS uygulamasının ihtiyaç duyduğu sıkıcı ama zorunlu şeylerin **hepsi** Qt'de var: docking, yüksek DPI, çoklu monitör, yerel dosya diyalogları, yazdırma/plot, erişilebilirlik, IME, sağdan-sola, tarih/sayı yerelleştirme.
- `QRhi` ile Vulkan / Metal / D3D12 / OpenGL soyutlaması hazır gelir; ayrı bir render kütüphanesi bağımlılığından kurtulursunuz.
- `QRhiWidget` (6.7+) klasik widget kabuğunun içine GPU canvas'ı gömer — tam ihtiyacınız olan şey.
- GPL olduğunuz için Qt ücretsiz.
- Türkiye'de Qt/C++ geliştirici bulunabilir.
- Kanıt: QGIS, FreeCAD, KiCad, Krita ve pek çok ticari CAD ürünü Qt üzerinde çalışıyor. Bu kategoride başka olgun seçenek fiilen yok.

### 6.3 Nasıl Kullanmalı — Karma Yaklaşım

```
┌──────────────────────────────────────────────────┐
│  Qt Widgets kabuğu                               │
│  menü · araç çubukları · docking · tablolar      │
│  ┌────────────────────────┐ ┌──────────────────┐ │
│  │  QRhiWidget            │ │ QQuickWidget     │ │
│  │  GPU harita canvas'ı   │ │ AI sohbet paneli │ │
│  │  (sizin pipeline'ınız) │ │ özellik paneli   │ │
│  └────────────────────────┘ └──────────────────┘ │
│  Komut satırı widget'ı                           │
└──────────────────────────────────────────────────┘
```

- **Kabuk: Qt Widgets.** Yoğun masaüstü uygulaması için doğru araç.
- **Canvas: QRhiWidget** içinde kendi GPU pipeline'ınız. Qt'nin çizim sistemi buraya karışmaz.
- **Docking: Qt Advanced Docking System** (LGPL). Qt'nin yerleşik `QDockWidget`'ı ciddi bir CAD uygulaması için yetersizdir — sekmeli gruplar, kayan pencereler, perspektif kaydetme gerekir.
- **QML'i seçici kullanın:** AI sohbet paneli, karşılama ekranı, animasyonlu özellik panelleri `QQuickWidget` ile. Tüm kabuğu QML'de kurmayın.
- **Dear ImGui'yi canvas içinde hata ayıklama katmanı olarak** ekleyin: render istatistikleri, komut denetleyicisi, profil çubukları. Kullanıcıya açılmaz, geliştirici için paha biçilmezdir.

---

## 7. Dil, Derleyici, Standart

### 7.1 C++ Sürümü

**Taban: C++20.** C++23 özelliklerini `#if __cpp_lib_...` ile koşullayın.

| C++20 özelliği                | Bu projede nerede                                          |
| ----------------------------- | ---------------------------------------------------------- |
| **Coroutines**                | **Etkileşimli komut akışları (§2.4) — en kritik kullanım** |
| `concepts`                    | Geometri/koordinat şablonlarında okunur hata mesajları     |
| `ranges`                      | Kopyasız feature/vertex iterasyonu                         |
| `std::span`                   | SoA geometri dizilerini sahiplenmeden geçirme              |
| `std::jthread` + `stop_token` | İptal edilebilir uzun hesaplar ve script'ler               |
| `constexpr` genişlemesi       | Sembol/stil tablolarını derleme zamanında kurma            |
| `<bit>`                       | Quadtree / Morton kodları                                  |
| `operator<=>`                 | Koordinat ve kimlik tipleri                                |

C++23'ten değerliler: `std::expected` (yoksa `tl::expected`), `std::mdspan` (raster/DEM için birebir), `std::print`, "deducing this", `std::flat_map`.

**C++20 Modules: şimdilik hayır.** Üç derleyicide davranış hâlâ tutarsız, IDE desteği eksik, başlık tabanlı bağımlılıklarla (GDAL, Qt) karışım sorunlu. Header + PCH ile başlayın.

**Coroutine kütüphanesi:** Çıplak C++20 coroutine'leri düşük seviyelidir. `cppcoro` (MIT, bakımı yavaş) veya kendi minimal `Task<T>`/`Generator<T>` tipinizi yazın — komut altyapısı için 300 satır yeterlidir ve tam kontrol sağlar. Öneri: kendiniz yazın.

### 7.2 Derleyici Tabanı

| Platform     | Derleyici  | Minimum                |
| ------------ | ---------- | ---------------------- |
| Windows      | MSVC       | VS 2022 17.10+ (19.40) |
| Linux        | GCC        | 13+ (tercihen 14)      |
| Linux (alt.) | Clang      | 17+                    |
| macOS        | AppleClang | Xcode 15.3+            |

**Kural:** Her platformda tek derleyici, tek standart kütüphane. Bağımlılıkları da aynı toolchain ile derleyin.

### 7.3 Derleme Bayrakları — Kritik

```
MSVC:  /O2 /Oi /Gy /GL /fp:precise /DNDEBUG
GCC:   -O2 -fno-fast-math -ffp-contract=off -flto=thin -fvisibility=hidden
Clang: -O2 -fno-fast-math -ffp-contract=off -flto=thin -fvisibility=hidden
```

**Asla: `-ffast-math` / `/fp:fast`.** NaN kontrollerini, toplama sırasını ve robust predicate doğruluğunu bozar.

**`-ffp-contract=off` neden şart:** FMA komutları ara sonucu yuvarlamaz; aynı kod x86 ve Apple Silicon'da farklı sonuç verir. Resmî belge üreten yazılımda alan, kesişim ve dengeleme sonuçları platformlar arası bit-birebir eşit olmalıdır.

---

## 8. Derleme ve Bağımlılık Yönetimi

- **CMake 3.28+** — `CMakePresets.json` kullanın
- **Ninja** — üç platformda aynı, Make'ten belirgin hızlı
- **vcpkg (manifest mode)** — `vcpkg.json` + baseline ile sürüm sabitleme. GDAL, PROJ, GEOS, Qt, Boost, CGAL portlu. Alternatif: Conan 2
- **ccache / sccache** — CI süresini yarıya indirir
- **CPack** — MSI, DMG, DEB, RPM

### Depo Yapısı

```
/cmake
/src
  /core           # Qt'siz: geometri, topoloji, koordinat, indeks
  /command        # komut veri yolu, kayıt, ayrıştırıcı, günlük, işlem
  /io             # GDAL sarmalayıcı, kendi format, DWG/DXF
  /render         # GPU pipeline (QRhi)
  /script         # Lua/QuickJS host + Python bağlamaları
  /ai             # sağlayıcı soyutlaması, araç şeması üretimi, RAG
  /domain
    /geodesy /cadastre /planning /surface
  /app            # Qt UI
  /plugin-api     # kararlı C ABI
/data
  /catalogs       # BÖHHBÜY kodları, MPYY gösterimleri  (VERİ, kod değil)
  /crs            # jeoit gridleri, dönüşüm parametreleri
  /corpus         # mevzuat metinleri + gömme indeksi
/tests
  /unit /golden /bench /fuzz /journal /ai-eval
/packaging
```

**CI kapısı:** `/src/core` içinde `#include <Q` görülürse build kırılsın. `/src/domain` içinde doğrudan geometri mutasyonu (komut dışı) görülürse build kırılsın.

---

## 9. Kütüphane Listesi

### 9.1 Çekirdek Altyapı

| Kütüphane                            | Lisans     | Ne için                                     |
| ------------------------------------ | ---------- | ------------------------------------------- |
| **fmt**                              | MIT        | Formatlama                                  |
| **spdlog**                           | MIT        | Asenkron loglama                            |
| **Boost** (Geometry, PFR, Container) | BSL-1.0    | R-tree indeks, yardımcı yapılar             |
| **Eigen**                            | MPL 2.0    | Lineer cebir, dengeleme, dönüşüm matrisleri |
| **Taskflow**                         | MIT        | Görev grafiği, iş çalma zamanlayıcı         |
| **oneTBB** (alt.)                    | Apache 2.0 | Paralel algoritmalar                        |
| **mimalloc**                         | MIT        | Çok thread'de belirgin kazanç               |
| **moodycamel::ConcurrentQueue**      | BSD-2/BSL  | Kilitsiz kuyruk (render ↔ IO)               |
| **glaze** / **nlohmann/json**        | MIT        | JSON (glaze belirgin hızlı)                 |
| **simdjson**                         | Apache 2.0 | Büyük GeoJSON okuma                         |
| **xxHash**                           | BSD-2      | Hızlı hash                                  |
| **zstd**                             | BSD/GPLv2  | Sıkıştırma                                  |
| **magic_enum**                       | MIT        | Enum ↔ string                               |
| **xsimd**                            | BSD-3      | Taşınabilir SIMD                            |

### 9.2 Geometri ve CBS Çekirdeği

| Kütüphane               | Lisans        | Ne için                                                       | Not                                                                    |
| ----------------------- | ------------- | ------------------------------------------------------------- | ---------------------------------------------------------------------- |
| **GDAL/OGR**            | MIT           | 200+ format                                                   | Sürücü setini kırpın, ikili boyut ciddi düşer                          |
| **PROJ**                | MIT           | Koordinat dönüşümü                                            | TUREF, TM 3°, ED50, jeoit gridleri                                     |
| **GEOS**                | LGPL 2.1      | Overlay, buffer, predicate                                    | Olgun                                                                  |
| **CGAL**                | GPLv3+        | Kesin aritmetik, Delaunay, arrangement, **straight skeleton** | GPL olduğunuz için ücretsiz. Straight skeleton = çekme mesafesi/offset |
| **Clipper2**            | BSL-1.0       | Poligon boolean + offset                                      | GEOS'tan hızlı, tam sayı aritmetiği                                    |
| **CDT**                 | MPL 2.0       | Kısıtlı Delaunay                                              | TIN, kırıklık hatları                                                  |
| **earcut.hpp**          | ISC           | Poligon üçgenleme                                             | Render pipeline'ı                                                      |
| **libspatialindex**     | MIT           | Disk tabanlı R-tree                                           | Bellek içi için `boost::geometry::index::rtree` daha hızlı             |
| **GeographicLib**       | MIT           | Jeodezik problemler, TM, jeoit                                | PROJ'un yapmadığı ince hesaplar                                        |
| **Shewchuk predicates** | Public domain | Robust orientation/incircle                                   | Tek dosya, mutlaka alın                                                |

> ⛔ **Triangle (Shewchuk üçgenleyicisi) KULLANMAYIN.** Ticari kullanım kısıtlı, GPL ile uyumsuz. Yerine CDT veya CGAL. (`predicates.c` ayrıdır ve serbesttir.)

### 9.3 Veri Depolama

| Kütüphane                   | Lisans                     | Not                                             |
| --------------------------- | -------------------------- | ----------------------------------------------- |
| **SQLite**                  | Public domain              | GeoPackage, proje dosyası, cache, komut günlüğü |
| **libpqxx**                 | BSD-3                      | PostGIS                                         |
| **SpatiaLite**              | MPL 1.1 / GPLv2 / LGPL 2.1 | Üçlü lisans — GPL modunda kullanın              |
| **DuckDB + spatial**        | MIT                        | Analitik sorgular, GeoParquet                   |
| **Apache Arrow / GeoArrow** | Apache 2.0                 | Kolonsal bellek, sıfır kopya                    |
| **protozero**               | BSD-2                      | MVT vector tile                                 |

**Kendi iç formatınız:** En büyük uzun vadeli performans kaldıracı. `mmap`'lenebilir, kolonsal, önceden LOD hesaplanmış, R-tree'si gömülü tek dosya.

### 9.4 Render ve Arayüz

| Kütüphane                            | Lisans          | Not                                          |
| ------------------------------------ | --------------- | -------------------------------------------- |
| **Qt 6** (Core, Widgets, Quick, RHI) | LGPLv3 / GPLv2+ | `QRhiWidget` ile GPU canvas                  |
| **Qt Advanced Docking System**       | LGPLv2.1        | CAD için zorunlu — yerleşik docking yetersiz |
| **Dear ImGui**                       | MIT             | Sadece geliştirici hata ayıklama katmanı     |
| **FreeType**                         | FTL / GPLv2     | Font rasterization                           |
| **HarfBuzz**                         | MIT             | Metin şekillendirme (Türkçe için şart)       |
| **ICU**                              | Unicode-3.0     | Sıralama, normalizasyon, yerelleştirme       |
| **msdfgen**                          | MIT             | SDF atlas — ölçek bağımsız etiket/sembol     |
| **libtess2**                         | SGI-B 2.0       | Karmaşık tessellation                        |
| **lunasvg**                          | MIT             | SVG → path (MPYY sembolleri)                 |
| **stb_rect_pack**                    | MIT/PD          | Atlas paketleme                              |

### 9.5 Script ve Eklenti

| Kütüphane              | Lisans      | Not                                |
| ---------------------- | ----------- | ---------------------------------- |
| **sol2 + Lua**         | MIT         | Gömülü sıcak yol script'i          |
| **QuickJS-ng** (alt.)  | MIT         | JS tercih ederseniz                |
| **pybind11 + CPython** | BSD-3 / PSF | Ekosistem katmanı, opsiyonel modül |

### 9.6 AI

| Kütüphane                    | Lisans     | Not                                        |
| ---------------------------- | ---------- | ------------------------------------------ |
| **llama.cpp**                | MIT        | Yerel LLM — veri egemenliği için zorunlu   |
| **ONNX Runtime**             | MIT        | Gömme (embedding) modelleri, sınıflandırma |
| **sqlite-vec** / **usearch** | Apache 2.0 | Vektör arama (mevzuat RAG'ı)               |
| **libcurl** + JSON           | curl / MIT | Bulut sağlayıcı istemcisi                  |

### 9.7 Hesap ve Dengeleme

`Eigen` (MPL 2.0), `Ceres Solver` (BSD-3 — GNSS baz dengeleme, doğrusal olmayan en küçük kareler).
**SuiteSparse** ⚠️ modül modül karışık lisans: AMD/COLAMD BSD-3, CHOLMOD kısmen LGPL, UMFPACK GPL. Hangi modülü linklediğinizi bilin.

### 9.8 CAD Formatları

| Kütüphane        | Lisans               | Kapsam                           |
| ---------------- | -------------------- | -------------------------------- |
| **libdxfrw**     | GPLv2+               | DXF tam, DWG kısmi               |
| **LibreDWG**     | GPLv3+               | DWG okuma iyi, **yazma sınırlı** |
| **OpenCASCADE**  | LGPL 2.1 + exception | 3B katı, STEP/IGES               |
| **IfcOpenShell** | LGPLv3               | IFC / BIM                        |

> ⚠️ **ODA Drawings SDK kullanamazsınız** — kapalı kaynak, GPL ile uyumsuz. DWG uyumunuz LibreDWG kalitesiyle sınırlı. **Projenin en büyük teknik riski.** Faz 0'da gerçek dosyalarla ölçün.

### 9.9 XML ve Veri Değişimi

`libxml2` (MIT) — **XSD şema doğrulama, PlanGML için kritik**. `Xerces-C` (Apache 2.0) daha güçlü XSD, daha ağır. `pugixml` (MIT) hızlı okuma.

PlanGML doğrulamasını **kendi içinizde** yapın; kullanıcı e-Plan'a yükleyip reddedilene kadar beklemesin.

### 9.10 Nokta Bulutu ve 3B

`PDAL` (BSD-3), `laz-perf` (Apache 2.0 — LASzip'in LGPL'inden kaçınmak için), `PCL` (BSD-3), `Open3D` (MIT), `meshoptimizer` (MIT), `Draco` (Apache 2.0), `libcitygml` (LGPL 2.1).

### 9.11 Test, Profil, Kalite

| Araç                                 | Lisans     | Not                                   |
| ------------------------------------ | ---------- | ------------------------------------- |
| **doctest** / **Catch2**             | MIT / BSL  | doctest derleme süresi çok daha iyi   |
| **Google Benchmark**                 | Apache 2.0 | Mikro benchmark                       |
| **Tracy Profiler**                   | BSD-3      | Frame-level profiling — vazgeçilmez   |
| **ASan / UBSan / TSan**              | —          | Ayrı CI job'ları                      |
| **libFuzzer / AFL++**                | —          | DXF, GML, LAS parser'larını fuzzlayın |
| **clang-tidy / clang-format / IWYU** | —          | CI kapısı                             |
| **Crashpad**                         | Apache 2.0 | Çökme raporlama                       |
| **CycloneDX**                        | Apache 2.0 | SBOM — GPL uyumu için zorunlu         |

---

## 10. Performans: Somut Teknikler

### 10.1 Önce Hedefi Sayısallaştırın

| Senaryo                                   | Hedef                  |
| ----------------------------------------- | ---------------------- |
| 5M poligonlu kadastro katmanında pan/zoom | ≤ 16 ms frame (60 fps) |
| 200 MB DWG açılış                         | ≤ 3 sn                 |
| 50M nokta LAZ ilk görüntüleme             | ≤ 5 sn                 |
| 100k parselde topolojik doğrulama         | ≤ 2 sn                 |
| Uygulama soğuk açılış                     | ≤ 2 sn                 |
| **Komut satırı tuş → ekran gecikmesi**    | **≤ 30 ms**            |
| **Script'ten komut çağrı maliyeti**       | **≤ 10 µs**            |
| Boş projede RAM                           | ≤ 300 MB               |

CI'da benchmark kapısı kurun; %10'dan fazla regresyon build'i kırsın.

### 10.2 Veri Düzeni

- **Struct-of-Arrays** — `std::vector<Point>` değil, ayrı `xs`, `ys` dizileri. SIMD ve cache lokalitesinin ön koşulu
- **Arena allocator** — katman başına arena, yıkım tek seferde
- **32-bit indeks** — pointer yerine indeks; yapı boyutu yarıya iner
- **Sabit-nokta koordinat** — iç depoda `int64` milimetre. Kesin aritmetik, deterministik karşılaştırma, yarı yer. Kadastro için milimetre fazlasıyla yeterli
- **Sıcak/soğuk ayrımı** — geometri ve öznitelikler ayrı bloklar; çizim öznitelik okumaz
- **mmap** — kendi formatınızı belleğe eşleyin, OS sayfa cache'i sizin için çalışsın

### 10.3 Render

- **Origin offset (jitter tuzağı).** TUREF-TM3'te koordinatlar 7 basamaklı; `double`'ı doğrudan `float` vertex'e yazarsanız metrelik titreme görürsünüz. Görünüm merkezine göre offset alıp `float`'a orada çevirin
- **Önceden hesaplanmış LOD** — Douglas-Peucker ile 4-5 seviye, quadtree tile'larına yazılı
- **GPU'da çizgi genişletme** — instanced quad + vertex shader'da screen-space genişletme; kalınlık, kesikli desen, uç tipi shader'da
- **Kalıcı eşlenmiş buffer** — persistent mapped ring buffer + fence
- **Draw call birleştirme** — sembolleri tek atlasta topla; hedef: karmaşık paftada < 100 draw call/frame
- **SDF etiket atlası** — ölçek bağımsız, tek texture
- **Etiket yerleşimi ayrı thread'de** — sonucu atomik takas et, UI beklemesin
- **Ayrı render thread** — UI thread asla geometri işlemesin, asla disk okumasın
- **Frustum culling + quadtree** — görünmeyen tile'ı hiç açma
- **Raster:** COG + overview piramitleri, asenkron tile yükleme, LRU cache

### 10.4 Komut ve Script Yolunun Performansı

Bu, komut merkezli mimarinin özel maliyetidir; ihmal edilirse tüm avantajı yer.

- **Komut gönderimi tahsissiz olmalı.** Sıcak yolda `std::function` ve `shared_ptr` kullanmayın; argümanları küçük bir POD union veya arena'ya yazın
- **Toplu iş modu.** Script 100 bin nesne yaratıyorsa her komut için ayrı doğrulama + ayrı undo kaydı yapmayın. `TOPLU_BASLA` / `TOPLU_BITIR` ile tek işlem, tek doğrulama geçişi
- **Günlük yazımı asenkron.** Komut günlüğü ayrı thread'de diske yazılsın; kullanıcı beklemesin
- **Script sıcak yolu için Lua.** Etiket ifadesi her nesne için çalışır; Python'un çağrı maliyeti burada kabul edilemez
- **AI çağrıları tamamen asenkron.** Model yanıtı beklenirken uygulama tam işlevsel kalmalı, iptal edilebilmeli

### 10.5 Hesaplama ve Ölçüm

- **Bulk-load STR R-tree** — tek tek insert değil
- **Paralellik:** Taskflow; thread başına ayrı arena, `alignas(64)` ile false sharing'e dikkat
- **SIMD:** bbox testi, koordinat dönüşümü, Douglas-Peucker mesafe hesabı
- **Erken çıkış:** önce bbox, sonra prepared geometry, en son tam kesişim
- **Tracy** ile her frame'i işaretleyin; tahminle optimizasyon yapmayın
- **Golden data regresyon** — üç platformda bit-birebir eşleşme (bkz. §7.3)

---

## 11. Yol Haritası

### Faz 0 — Teknik Doğrulama (0-4 ay)
**Ürün kodu yazmayın, riskleri öldürün.**

- [ ] Render prototipi: 5M poligon, üç platformda 16 ms doğrulaması
- [ ] `double` jitter testi: 30. dilim TM3 koordinatlarıyla zoom
- [ ] **Komut altyapısı iskeleti:** 3 örnek komut, coroutine akışı, günlük, undo, aynı komutun GUI + komut satırı + script'ten çalıştığının kanıtı
- [ ] PROJ ile TUREF dönüşüm doğruluğunun TKGM referans verisiyle karşılaştırması
- [ ] **DWG kapsam testi:** LibreDWG + libdxfrw ile 50+ gerçek müşteri dosyası
- [ ] AI fizibilite: yerel modelin Türkçe alan terminolojisinde komut üretme başarısı (50 örnekle hızlı ölçüm)
- [ ] CMake + vcpkg + üç platformlu CI, benchmark harness, golden data altyapısı

**Çıktı:** Yapılabilirlik kararı ve revize tahmin.

### Faz 1 — Çekirdek + Komut Altyapısı (4-12 ay)
Komut veri yolu ve kaydı · coroutine etkileşim modeli · günlük/undo/redo · **komut satırı widget'ı** · veri modeli · katman yönetimi · stil motoru · düzenleme + snap + topolojik doğrulama · GDAL I/O · PostGIS/GPKG · proje dosya formatı · Qt kabuk · QGIS proje/stil okuma (göç yolu).

> Komut altyapısı Faz 1'de olmalı, sonraya bırakılamaz. Sonradan eklenmesi tüm kodun yeniden yazılması demektir.

### Faz 2 — CAD + Haritacılık + Script (12-20 ay) → **İlk sürüm**
Tam çizim/düzenleme · ölçülendirme · blok/xref · tarama · DXF/DWG çift yönlü · **Lua ifade motoru** · **Python eklenti API'si** · makro kaydı ve oynatma · jeodezik hesap ve dengeleme · total station / GNSS aktarımı · BÖHHBÜY nesne kataloğu · paftalama ve çıktı.

### Faz 3 — İmar/Planlama + AI (18-28 ay) → **Ticari değerin merkezi**
MPYY gösterim kütüphanesi · plan çizim ve otomatik kontrol · PlanGML export + XSD doğrulama · e-Plan dosya seti · 3194/18. madde parselasyon ve DOP · TKGM MEGSİS entegrasyonu · **AI komut üretimi (önizleme + onay)** · **mevzuat RAG'ı** · yerel model desteği.

### Faz 4 — Yüzey ve Analiz (24-36 ay)
TIN/DEM · eşyükselti · kübaj · boykesit-enkesit · güzergah · raster analiz · nokta bulutu · 3B görselleştirme.

### Faz 5 — Ekosistem
Eklenti paket deposu · sunucu bileşeni · komut günlüğü tabanlı işbirliği · arazi için mobil uygulama · eğitim ve sertifikasyon.

---

## 12. Türkiye Mevzuatı — Gereksinim Kontrol Listesi

### Jeodezik (BÖHHBÜY)
- [ ] TUREF/ITRF96, epok ve hız alanı yönetimi
- [ ] TM 3° dilimleri (27°, 30°, 33°, 36°, 39°, 42°, 45°), ölçek 1.0
- [ ] ED50 / UTM 6° ve ITRF↔ED50 bölgesel dönüşüm
- [ ] Güncel Türkiye Jeoit Modeli ile Helmert ortometrik yükseklik
- [ ] TUSAGA-Aktif / CORS-TR, RINEX, NTRIP
- [ ] Poligon, nirengi, GNSS baz dengelemesi; hata elipsleri
- [ ] Aplikasyon ve röper krokisi
- [ ] Pafta bölümleme ve isimlendirme
- [ ] BÖHHBÜY detay kodları ve nesne kataloğu
- [ ] Kontrol işleri belge ve çizelgeleri

### Kadastro (TKGM)
- [ ] MEGSİS / TAKBİS servis entegrasyonu
- [ ] İfraz, tevhit, yola terk, ihdas, irtifak, cins değişikliği
- [ ] Tescil bildirimi, alan hesap cetvelleri, kontrol raporları
- [ ] LİHKAB iş akışları

### İmar ve Planlama
- [ ] MPYY EK-1a/1b/1c/1ç/1d gösterimleri + EK-1e Detay Kataloğu (güncellenebilir veri paketi)
- [ ] PlanGML üretimi + XSD doğrulama + uyumsuz alan kullanımı raporu
- [ ] e-Plan Otomasyon dosya seti (PlanGML, vektör, GeoTIFF, KML, plan notu, rapor)
- [ ] 3194/18. madde: düzenleme sınırı, DOP, dağıtım cetvelleri
- [ ] Kamulaştırma (2942)
- [ ] Planlı Alanlar İmar Yönetmeliği kontrolleri (TAKS/KAKS, çekme mesafeleri)
- [ ] Değer artış payı

### Veri ve Kurumsal
- [ ] TUCBS veri temaları uyumu (32 tema / 53 alt tema)
- [ ] ISO 19115/19139 metaveri
- [ ] OGC: WMS, WMTS, WFS-T, WCS, CSW, OGC API Features
- [ ] **Coğrafi Veri İzin Belgesi** süreci (hukuk ekibinin ilk işi)

### AI'ya Özgü
- [ ] Denetim kaydı: prompt, model sürümü, üretilen komutlar, kullanıcı kararı
- [ ] Veri egemenliği: hassas veride yerel model zorunlu kılınabilmeli
- [ ] AI çıktısının "öneri" olarak açıkça etiketlenmesi
- [ ] Mevzuat cevaplarında madde referansı ve yayım tarihi zorunluluğu

---

## 13. "Dünya Standardı" — Somut Kontrol Listesi

Bu ifade ölçülebilir olmalı. Aşağıdakiler pazarlama değil, kabul kriteri:

**Kullanıcı deneyimi**
- [ ] Her işlem klavyeden erişilebilir; fare olmadan tam çalışabilirlik
- [ ] Ekran okuyucu desteği (NVDA, VoiceOver, Orca)
- [ ] Otomatik kayıt + çökme kurtarma (komut günlüğünden geri yükleme)
- [ ] Yüksek DPI, karışık DPI çoklu monitör, koyu tema
- [ ] Tam TR + EN yerelleştirme; Türkçe `i`/`I` dönüşümü için `QLocale` (asla `std::toupper`)

**Mühendislik**
- [ ] Semantik sürümleme ve yayınlanmış API kararlılık politikası
- [ ] Dosya formatı sürümlenmesi + ileri uyumluluk (eski sürüm yeni dosyayı reddederken açıklayıcı mesaj versin)
- [ ] Yeniden üretilebilir derlemeler (reproducible builds)
- [ ] SBOM her yayında üretiliyor
- [ ] Kod imzalama: Windows EV, Apple Developer ID + notarization
- [ ] Güvenlik açığı bildirim politikası ve yanıt SLA'sı
- [ ] Parser'lar sürekli fuzzlanıyor (dosya okuma en büyük saldırı yüzeyi)
- [ ] Çökmesiz oturum oranı ölçülüyor, hedef > %99.5

**Topluluk (açık kaynak olduğunuz için)**
- [ ] Açık issue takibi, RFC süreci, katkı rehberi, Code of Conduct
- [ ] Genel yol haritası ve düzenli yayın takvimi
- [ ] Doxygen (API) + MkDocs (kullanıcı) dokümantasyonu, ikisi de CI'da yayınlanıyor
- [ ] Örnek veri setleri ve öğretici içerik

---

## 14. Ekip ve Süreç

### Kadro (10-14 kişi)

| Rol                                  | Kişi |
| ------------------------------------ | ---- |
| Çekirdek / grafik C++                | 2-3  |
| CBS / geometri                       | 2    |
| **Komut altyapısı + script/eklenti** | 1-2  |
| **AI entegrasyonu**                  | 1    |
| **Harita mühendisi (domain)**        | 1-2  |
| **Şehir plancısı (domain)**          | 1    |
| UI/UX                                | 1-2  |
| QA / test                            | 1-2  |
| DevOps                               | 1    |

Domain uzmanı kadroda olmadan mevzuat uyumu yazılımcı tahminine kalır ve tutmaz — benzer projeleri en sık batıran hata budur.

### Altyapı
- CI matrisi: 3 OS × (Debug + Release), ASan ayrı job. PR başına < 20 dk
- Paketleme: MSI (WiX), DMG, AppImage + `.deb`/`.rpm` + Flatpak
- **Komut günlüğü regresyon paketi:** kaydedilmiş gerçek oturumlar, her gece oynatılıp çıktı karşılaştırılıyor — bu, CAD yazılımı için en değerli test türüdür
- **AI değerlendirme paketi:** 200-300 Türkçe istek → beklenen komut dizisi, CI'da başarı oranı takibi

---

## 15. Riskler ve Karşı Önlemler

| Risk                                        | Etki           | Önlem                                                                                          |
| ------------------------------------------- | -------------- | ---------------------------------------------------------------------------------------------- |
| **DWG uyumu yetersiz**                      | Yüksek         | Faz 0'da ölçün. Yetersizse DXF'i birinci sınıf yapın, LibreDWG'ye upstream katkı verin         |
| Performans hedefine ulaşılamıyor            | Yüksek         | Faz 0 prototipi bunun için. Tutmuyorsa mimari değişir, ürün kodu değil                         |
| **Komut altyapısı sonradan ekleniyor**      | **Çok yüksek** | Faz 1'de kurun. Sonradan eklemek yeniden yazmaktır                                             |
| **AI halüsinasyonu hukuki belgeye sızıyor** | **Çok yüksek** | Otomatik uygulama yok, önizleme + onay, koordinat asla modelden gelmesin, denetim kaydı        |
| Mevzuat sık değişiyor                       | Orta           | Veri odaklı katalog mimarisi                                                                   |
| Kapsam sonsuza genişliyor                   | Yüksek         | Faz 2'yi satılabilir minimum olarak kilitleyin                                                 |
| GPL nedeniyle gelir modeli belirsiz         | Orta           | Destek + eğitim + entegrasyon + kurumsal sürüm; kamu ihalelerinde açık kaynak avantaj olabilir |
| NetCAD ekosistemi                           | Yüksek         | Veri göçü ve kamu referansı planı, ürün planı kadar ciddiye alınmalı                           |

---

## 16. İlk 30 Gün — Yapılacaklar

1. `LICENSE` (GPLv3) + `NOTICE` + DCO/CLA kararı yazılsın
2. `CMakePresets.json` + `vcpkg.json`, üç platformda "hello triangle" derlensin
3. GitHub Actions matrisi ayağa kalksın (3 OS, ccache, artefakt)
4. Tracy entegre edilmiş boş `QRhiWidget` penceresi
5. **Komut altyapısı iskeleti:** `Task<T>` coroutine tipi (~300 satır), `COMMAND` makrosu, komut kaydı, günlük. `ÇİZGİ` komutu hem butondan hem komut satırından hem de bir JSON dosyasından çalışsın — bu, Faz 0'ın en önemli kanıtı
6. Gerçek müşteri DWG dosyalarını toplayın (Faz 0 testinin girdisi)
7. PROJ'u TUREF-TM3 ile doğrulayan test, TKGM'den referans koordinat
8. Yerel bir modelle 50 Türkçe istek üzerinde hızlı AI fizibilitesi
9. Bir harita mühendisi ve bir şehir plancısıyla gereksinim toplantısı; §12 listesini birlikte önceliklendirin

---

*Ağustos 2026. Mevzuat referansları ve kütüphane sürüm/lisans bilgileri kullanım anında doğrulanmalıdır.*