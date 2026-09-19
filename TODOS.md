# KentOSCad — Çıktı Yerleşimi, AI Surface ve MCP geliştirme planı

İnceleme tarihi: **19 Eylül 2026**. Ana referans: yerel `main`, **`86e290a`**.
Bu belge mevcut kodun incelemesini, QGIS 4.2 araştırmasını ve hedef ürün davranışını
birleştirir. İşaretlenmemiş maddeler yapılacak iştir; uygulanmış özellik iddiası değildir.
İnceleme statik kod ve Git geçmişi üzerinden yapılmıştır; uygulama çalıştırılarak
karşılaştırmalı QGIS testi veya performans ölçümü yapılmamıştır. Uzak depo fetch edilmemiştir.

**Hedef:** Kullanıcı, arayüzde yapabildiği bütün işleri AI penceresinden veya MCP
istemcisinden de tamamlayabilmeli. Çıktı Yerleşimi, QGIS 4.2 Print Layout / Atlas /
Report kapsamını karşılayacak derinlikte olmalı. AI'nin açıklama soruları ile işlem
onayları birbirinden bağımsız Settings tercihleri olmalı; otomatik çalışma gerçekten
işi bitirmeli, her adımda kullanıcıya geri dönmemeli.

## 1. İncelenen dallar ve mevcut çalışmalar

| Referans | Bulgular | Bu plandaki karşılığı |
|---|---|---|
| `main` — `86e290a` | Layout veri modeli, tasarımcı, çıktı, yönetici ve şablon kitaplığı var. AI ve MCP ortak katalog/öneri altyapısını kullanıyor. | Bütün mevcut durum değerlendirmelerinin tabanı. |
| `claude/pafta-duzeni` — `f865648` | Temel layout çalışması. Dalın ucu main'in atası; `158b4ab` birleştirmesiyle main'e alınmış. | Yeniden geliştirilecek veya yeniden merge edilecek bağımsız bir özellik dalı değil. |
| Main'deki `3975b3d`, `0148f18` | Sırasıyla layout yöneticisi/ana menü bağlantısı ve şablon kitaplığı. | Yönetici ve şablon sistemi “yok” kabul edilmemeli; genişletilmeli. |
| Açık dal `yerlesim-adlandirmasi` — `1713715` | “Pafta” yerine “Çıktı Yerleşimi”; komut adları, parametreler, yer tutucular, yardım ve format okuma yollarında değişiklikler. | Ürün dilinde Çıktı Yerleşimi kullanılmalı; uyumluluk ayrıca doğrulanmalı. |
| Açık çalışma ağacı | Layout başlığında alan düzenleme, öğe sıralamasında indeks kullanımı, bazı render/metin/tema değişiklikleri ve layout dışı düzenlemeler var. | Commit edilmemiş çalışma main'e mal edilmedi; mevcut değişiklikler korunmalı. |

Karşılaştırmada `git show main:<dosya>`, dal geçmişi ve ayrı commit/çalışma ağacı
diff'leri kullanıldı. Bu belgeyi hazırlarken dal değiştirilmedi veya merge yapılmadı.

- [x] **BR-01 / P0 — Adlandırma geçişinin uyumluluk sözleşmesini tamamla.** *(19 Eylül 2026)*
  `core.layout`, `core.layout_item`, `core.layout_template` kimlikleri sabit kaldı.
  Karar: **eski ad okunur, yeni ad yazılır.** `Param::was` alanı bir parametrenin emekli
  adını taşıyor; `bind_tokens` (komut satırı) ve `Bus::dispatch` (betik/günlük) onu güncel
  adın üstüne taşıyor, `Context::record` yalnız güncel adı yazıyor — yani programdan tek
  yazım çıkıyor ve tekrarın tekrarı aynı baytları veriyor. İki yazımı birlikte vermek
  reddediliyor (skaler bir argümanın sessizce sonuncuyu tutması `command.md` P15'in
  yasakladığı şey). `<pafta>` yer tutucusu çözülmeye devam ediyor: bir antede yazılmış
  yazı, programın sözcük değiştirmesiyle bozulmaz. Şablon JSON'u alan adları taşıdığı
  için etkilenmiyor (`layout_to_json`). **Kabul karşılandı:** `test_io.cpp` içinde üç
  vaka — eski komut satırı ile yeni komut satırı aynı `content_hash`, eski günlük satırı
  (`{"args":{"pafta":…}}`) ile yeni günlük satırı aynı `content_hash` **ve** bayt bayt
  aynı günlük, iki adın birlikte verilmesi reddediliyor.

## 2. Kodun bugün sundukları ve somut boşluklar

### 2.1. Layout

| Alan | Main'de görülen durum | Sonuç / yapılması gereken |
|---|---|---|
| Belge modeli | `src/core/include/kentos_cad/core/layout.hpp`: `Layout`, `LayoutPage`, `LayoutItem`, `LayoutStore`; kâğıt ölçüleri `Um`, arazi ölçüleri `Mm`; belge hash'i ve undo yaklaşımı var. | Doğru temel korunmalı; ayrı ve kopuk bir layout dosya modeli kurulmasın. |
| Öğe türleri | Map, Label, ScaleBar, NorthArrow, Legend, Picture, Shape, Table var. | Türün enum'da bulunması, özelliklerinin komut/UI/render üzerinden tam çalıştığı anlamına gelmiyor. |
| Çok sayfa | Modelde `pages` ve `item_pages` var. Tasarımcı `setSheet(name_, 0)` ile açılıyor; dönüşüm/sürükleme yollarında `pages.front()` kullanılıyor. | Veri modeli var, tamamlanmış çok sayfalı düzenleme akışı yok. Farklı boydaki sayfalar özellikle doğrulanmalı. |
| Hassasiyet | Model mikrometre saklıyor; `layout_item` konum/boyut parametreleri tam sayı mm, tasarımcı snap'i sabit 1 mm. | Modelin hassasiyeti kullanıcıya ve otomasyona ulaşmıyor. |
| Harita ilişkileri | Ölçek çubuğu ve metin çözümleme `first_map()` üzerinden ilerliyor. Öğeler arasında açık `linked_map_id` yok. | İki haritalı sayfada her öğenin hangi haritaya bağlı olduğu tanımlanmalı. |
| Katman süzme | Modelde harita `layers` alanı var; `layout_render.cpp::paint_map` bu listeyi render seçeneklerine aktarmıyor. | “Harita kendi katman listesini kullanır” kabulü mevcut render yolu için doğru değil. |
| Render | `paint_map` önce `QImage` üretip sayfaya basıyor. | PDF dosyasının var olması, harita içeriğinin vektörel olduğu anlamına gelmiyor. |
| Sayfa çıktısı | `print_service.cpp::printLayout` PDF ve yazıcı yolu sunuyor. Cihaz sayfa boyutu ilk sayfadan kuruluyor; sonraki sayfalarda yeni boyut atanmıyor. | Karma A4/A3 sayfalı çıktı için cihaz sayfa boyutu geçişi düzeltilmeli. |
| Veriyle çalışan içerik | Sabit yer tutucular ve temel katman öznitelik tablosu var. | Genel ifade bağlamı, atlas, rapor ağacı, tablo akışı ve grafik öğeleri eksik. |
| AI erişimi | `layout.cpp` içindeki üç layout komutunda `AiAccessible` yok; yorumlarda kâğıt/arazi koordinatı ayrımı gerekçe gösteriliyor. `core.print` ve `core.print_profile` da katalog dışında. | AI ve MCP bugün tam bir layout üretip dışa aktarma işini tamamlayamaz. |

### 2.2. AI Surface ve MCP

Burada **AI Surface**, yalnız sohbet balonu değil; modelin görebildiği bağlam,
keşfedebildiği araçlar, çalıştırabildiği komutlar, iş yürütme döngüsü ve sonuç
doğrulama yüzeyinin tamamıdır.

| Alan | Main'de görülen durum | Sonuç / yapılması gereken |
|---|---|---|
| Ortak katalog | `src/ai/src/catalog.cpp::build_catalog`, `Registry` içinden yalnız `AiAccessible` komutlarını alıyor. | İki ayrı elle tutulan AI/MCP araç listesi oluşturulmamalı. |
| Kapsama açığı | Statik `return CommandSpec{...}` taramasında 81 tanımın 59'unda `AiAccessible` var. Dinamik üretilen komutlar bu sayıya dahil değil. | Bu oran çalışma zamanı kapsam yüzdesi değildir; gerçek registry ve kullanıcı eylemleri üzerinden envanter gerekir. |
| Katalog dışında kalan örnekler | `core.open/save/saveas/import/export`, `core.undo/redo`, `core.column`, `core.database`, `core.select`, `core.setting/preference/mode`, layout ve print aileleri. | “Her şeyi yapabilme” için dosya, seçim, ayarlar, çıktı ve veri yönetimi de açılmalı. |
| Mutasyon sınıflaması | Araç etkisi `!Flags::NoEffect` üzerinden tek boolean; kategori `File` ise `open_world`. | Okuma, görünüm, belge düzenleme, disk yazma ve dış sistem etkisi ayrı modellenmeli. Layout'un `File` kategorisi tek başına dış dünya etkisi kanıtı değil. |
| Sabit onay | `Gate`, `Approval`, `Plan` ve `agent_preamble()` her yazmayı insan kararına bağlıyor. `catalog.cpp` açıklamaları da bunu sabit söylüyor. | Sorun yalnız model prompt'u değil; yürütme politikası ve sonuç sözleşmesi değişmeli. |
| Sohbet döngüsü | `chat_panel.cpp::finishTurn` yalnız okuma sonucu varsa ve yazma önerisi yoksa `sendRound()` çağırıyor. Kartın `settled` bağlantısı durum bildiriyor. | Uygulama sonrası sonuçları modele verip doğrulama/sonraki adıma geçme akışı tamamlanmalı. |
| Plan bütünlüğü | `fileWrites` hatalı adımları ayırırken geçerli adımları öneriye koyabiliyor; genel sonuç metni bütün yazma çağrılarına dolaştırılıyor. | Birbirine bağımlı planın parçası sessizce atılmamalı; her çağrı tek ve doğru sonuç almalı. |
| Öneri durumu | Sohbet metni `oneri_durumu` aracına yönlendiriyor; katalog adı eşlemesinde bu araç yok ve `core.suggestion` AI'ye açık değil. | Gerçek durum aracı/sonuç olayı sağlanmalı; üretilen metin yalnız mevcut araçları anmalı. |
| Plan ekleme | MCP protokol katmanı `_meta.plan` ile plana ekleme bekliyor; `AiService::propose` doğrudan `plans_.add(...)` çağırıyor. | Test double ile çalışan sözleşmenin gerçek masaüstü dispatcher'ında da uygulanması gerekir. |
| Revizyon | Plan revizyonu saklanıyor; `AiService::applyPlan` içinde uygulama öncesi plan revizyonu karşılaştırması görünmüyor. | Beklerken değişmiş belgeye eski niyet uygulanmadan tekrar doğrulanmalı. |
| MCP sürümü | Kod `2026-07-28`, `server/discover`, istek başına metadata ve `subscriptions/listen` kullanıyor. | Bu sürümde `initialize` bulunmaması tek başına kusur değildir; eski istemci uyumu ayrı hedef olmalı. |
| MCP taşıma | `mcp_service.cpp` SSE yanıtını tek seferde yazıp kapatıyor; `keep_open` aboneliklerinin bu sınırı kodda açıkça belirtilmiş. | Protokol birim testi yanında gerçek socket/Qt entegrasyonu, canlı bildirim ve iptal testleri gerekir. |
| Kaynaklar | MCP'de `llms.txt` / `llms-full.txt` kaynakları var. | Çizim, layout, seçim, işlem ve çıktı kaynakları ayrıca sunulmalı. |
| Settings | `core.ai.hassas`, `core.ai.dusunme_goster`, `core.ai.sorumlu`; MCP port/belirteç/otomatik başlatma ayarları var. | Açıklama sorusu ve işlem onayı politikaları henüz yok. |

## 3. QGIS 4.2 araştırması ve karşılaştırma hedefi

Resmî 4.2 kullanıcı kılavuzu ve 4.2 değişiklik günlüğü esas alındı. QGIS yol haritası
inceleme tarihinde 4.2.2'yi mevcut sürüm olarak gösteriyor. Bu belge 4.2 ailesinin
özelliklerini hedefler; geliştirme dalındaki özellikleri 4.2 özelliği saymaz.
[QGIS sürüm yol haritası](https://qgis.org/resources/roadmap/)

### 3.1. Genel layout kabiliyeti ile 4.2 yeniliklerini ayır

QGIS 4.2 kılavuzunda layout yöneticisi, sayfalar, kılavuzlar, öğe ve undo panelleri;
harita, 3B harita, metin, lejant, ölçek, tablo, resim/kuzey oku, yükseklik profili,
HTML, şekil ve grafik öğeleri bulunuyor. Atlas ve raporlar da aynı çıktı iş akışının
parçası. Bunlar topluca “4.2'de yeni geldi” diye sunulmamalı.
[Layout genel bakış](https://docs.qgis.org/4.2/en/docs/user_manual/print_layout/overview_layout.html),
[Öğe türleri](https://docs.qgis.org/4.2/en/docs/user_manual/print_layout/layout_items/index.html)

**4.2 değişiklik günlüğünün özellikle eklediği üç çıktı yerleşimi özelliği:**

1. Grafik kategorilerini ve renklerini kaynak vektör katmanının sembolojisinden üretme;
   sayım veya Y ifadesi üzerinden toplama.
2. Resim öğesini şekil türündeki başka bir öğeyle kırpma.
3. GeoPDF çıktısında katman ağacının grup, sıra, ad ve görünürlük yapısını taşıma.
   QGIS tarafında kilitli katmanları olmayan harita gereksinimi ve karşılıklı dışlayan
   gruplar gibi sınırlamalar var; “her durumda tam katman eşitliği” varsayılmamalı.

Kaynak: [QGIS 4.2 — Print Layouts yenilikleri](https://qgis.org/project/visual-changelogs/visualchangelog42/#print-layouts).

### 3.2. Yetenek eşleme matrisi

| QGIS 4.2 referansı | KentOSCad hedefi | İş paketleri |
|---|---|---|
| Sayfa, kılavuz, öğe yönetimi, undo | Profesyonel çok sayfalı düzenleyici; hassas yerleştirme ve toplu işlemler | L-01, L-02, L-03 |
| Harita extent/ölçek/dönüş, katman kilidi/tema, grid ve overview | Aynı belgede birbirinden bağımsız harita çerçeveleri | L-04, L-05 |
| Ortak öğe özellikleri ve veriyle tanımlanan özellikler | Tutarlı özellik sistemi ve tipli ifade bağlamı | L-03, L-06 |
| Lejant, ölçek, kuzey oku, resim | Haritaya açık bağlantı ve gerçek kartografik temsil | L-05, L-07 |
| Öznitelik/manuel tablo, HTML çerçeveleri, rapor | Sayfalara akan veri ve rapor hiyerarşisi | L-08, L-11 |
| Grafik, yükseklik profili, 3B harita | Analiz çıktılarının yerleşime bağlanması | L-09 |
| Atlas | Kapsama nesnesi başına tekrarlanabilir pafta üretimi | L-10 |
| PDF, SVG, resim, coğrafi referanslı çıktı | Vektör kalitesi, gerçek ölçü, katmanlı GeoPDF ve toplu export | L-12, L-13 |
| Şablon ve processing ile çıktı | Sürümlü şablon, komut/API üzerinden baştan sona üretim | L-14, A-02, M-03 |

Detay referansları:
[Harita](https://docs.qgis.org/4.2/en/docs/user_manual/print_layout/layout_items/layout_map.html),
[Ortak öğe özellikleri](https://docs.qgis.org/4.2/en/docs/user_manual/print_layout/layout_items/layout_items_options.html),
[Tablolar](https://docs.qgis.org/4.2/en/docs/user_manual/print_layout/layout_items/layout_tables.html),
[Resim ve kuzey oku](https://docs.qgis.org/4.2/en/docs/user_manual/print_layout/layout_items/layout_image.html),
[Grafikler](https://docs.qgis.org/4.2/en/docs/user_manual/print_layout/layout_items/layout_chart.html),
[Çıktı ve atlas](https://docs.qgis.org/4.2/en/docs/user_manual/print_layout/create_output.html),
[Raporlar](https://docs.qgis.org/4.2/en/docs/user_manual/print_layout/create_reports.html),
[Processing kartografya araçları](https://docs.qgis.org/4.2/en/docs/user_manual/processing_algs/qgis/cartography.html).

Aşağıdaki tasarım KentOSCad için öneridir. QGIS'in C++ sınıf hiyerarşisini kopyalama
veya QGIS proje/şablon biçimleriyle kendiliğinden uyum iddiası taşımaz. “QGIS kadar
güçlü” ifadesi bu matristeki işlerin uygulamada tamamlanabilmesiyle ölçülmelidir.

## 4. Ortak mimari: GUI, AI ve MCP aynı işi yapmalı

```text
GUI / komut satırı / AI sohbeti / MCP
                 |
    Registry + tipli girdi/sonuç sözleşmesi
                 |
  Bağlam ve hedef çözümleme -> Plan -> Etki analizi
                 |
   Settings + kullanıcı/istemci kapsamı -> PolicyDecision
                 |
      Doğrula -> Önizle -> Gerekiyorsa onay
                 |
     Komut veri yolu -> Uygula -> Sonucu doğrula
                 |
     Journal + audit + kaynak/çıktı + UI olayı
```

Önizleme otomatik modda da üretilebilen bir veri/artefakt olmalı; kullanıcının her
seferinde tıklamasını gerektiren bir bariyer olmamalı. Yetkilendirme, geometri
doğrulaması ve işlem bütünlüğü birbirinden ayrı kalmalı.

- [~] **C-01 / P0 — Capability envanteri ve kapsam kapısı.** *(komut tarafı 19 Eylül 2026)*
  **Yapıldı:** `kentos_envanter` (`src/command/tools/envanter.cpp`) canlı registry'leri
  yürüyor — builtin + processing + üç domain + ai — ve her komut için kimlik, adlar,
  kategori, geri alma, **parametre şeması** (tür, arity, seçenek listesi, aralık,
  emekli ad) ve altı bayrağın her birini ayrı ayrı JSON'a yazıyor. Ağacı grep'leyen
  statik sayım **81** komut görüyordu; program **93** komut kaydediyor.
  **Gerçek kapsam: 93 komuttan 69'u ajana açık (%74), 24'ü kapalı.**
  `tests/support/ai-kapsam.json` bu 24'ün her birinin gerekçesini ve onu açacak iş
  paketini taşıyor (20 plana bağlı, 4 bilerek kalıcı: `core.select`, `core.ai_provider`,
  `core.mcp`, `core.script`). `scripts/ci-gate-envanter.sh` canlı envanteri bu listeyle
  karşılaştırıyor: gerekçesiz kapalı bir komut, ölmüş bir satır ve açıldığı hâlde
  duran bir satır kapıyı kırıyor. Üretici derlenmemişse **atlamıyor, kırıyor**.
  **Kalan:** menü/panel/etkileşimli araç tarafı — Qt gerektirdiği için ayrı bir uygulama
  probe'u olacak; "her satırda test" sütunu; MCP erişim sütunu (M-03 ile birlikte).
- [x] **C-02 / P0 — Etki sözleşmesi.** *(19 Eylül 2026)*
  `command::Effect` yedi biti taşıyor: `sorgu`, `gorunum`, `belge_duzenleme`,
  `dosya_okuma`, `dosya_yazma`, `dis_yazma`, `ayar_degisikligi`. `Flags` "hangi
  istemci ulaşabilir" sorusunu cevaplıyor; `Effect` bir politikanın sormak zorunda
  olduğu **farklı** soruyu: geriye ne değişmiş kalıyor, ve nerede.
  **Karma fiiller argümandan türetiliyor**: `CommandSpec::effect_verb` + `verb_effects`,
  ve `effect_of(spec, args)` verilen sözcüğün etkisini döndürüyor —
  `ÇIKTIYERLEŞİMİ islem=listele` yalnız `sorgu`, `islem=sil` `belge_duzenleme`.
  Fiil verilmemişse ya da tanınmayan bir sözcükse cevap **en kötü hâl**, çünkü
  etkileşimli bir çalıştırma fiili doğrulamadan sonra sorar.
  `Effect::None` "kimse söylemedi" demek ve `effect_of` onu asla döndürmüyor:
  bildirilmemiş bir komut kategorisinden okunuyor. **İlk geri düşüş yanlıştı** —
  `ReadOnly && !Dosya → ayar_degisikligi` diyordu ve `core.zoom` ile `core.pan`'i
  "ayar değiştirir" yapıyordu; kategoriye çevrildi.
  **Kabul karşılandı:** üç test — listelemek onay doğurmaz, `core.save` `ReadOnly`
  taşımasına rağmen `dosya_yazma` der, `core.print` `dis_yazma` der (bir yazıcı geri
  alma yığını değildir), ve 93 komutun hiçbiri bildirilmemiş kalmaz.
  Envanter her komutun etkisini ve fiil başına etkisini yazıyor.
  **Kalan:** bu sözleşmeyi TÜKETEN politika motoru S-01'dir.
- [ ] **C-03 / P0 — Plan ve sonuç sözleşmesi.** Plan; belge kimliği/revizyonu, adım
  bağımlılıkları, hedef nesneler, varsayımlar, etki özeti, önizleme ve policy sürümü
  taşısın. Sonuç; `status`, değişen kimlikler, yeni revizyon, uyarılar, doğrulama sonucu,
  undo referansı ve çıktı URI'ları döndürsün. **Kabul:** “planlandı”, “onay bekliyor”,
  “uygulandı” ve “çıktı üretildi” farklı makine durumlarıdır.
- [ ] **C-04 / P0 — Revizyon ve tekrar çalıştırma.** Commit öncesi belge kimliği,
  revizyon ve referanslar yeniden doğrulansın; plan değişirse önceki onay yeni plana
  taşınmasın. İstemci kimliğiyle kapsamlanmış idempotency anahtarı ekle. **Kabul:**
  bağlantı yeniden denemesi aynı çizgiyi veya aynı PDF'yi ikinci kez üretmez;
  `revision_conflict` yanlış nesnede işlem yapmak yerine taze plan gerektirir.
- [ ] **C-05 / P1 — Belge işlemi ile dış etkiyi ayır.** Bir belge değişikliği grubu
  tek undo ve atomik rollback sunsun. Dosya çıktısı geçici hedefe yazılıp doğrulansın,
  ardından atomik yayımlansın; yazıcıya gönderme ve uzak sistem yazma ayrı sonuçlarla
  izlenmeli. **Kabul:** Ctrl+Z'nin dışarı gönderilmiş çıktıyı geri aldığı iddia edilmez;
  kısmi dış başarısızlıklar ve tekrar denenebilir adımlar açıkça döner.

## 5. Çıktı Yerleşimi iş paketleri

### 5.1. Model, tasarımcı ve haritalar

- [ ] **L-01 / P0 — Kalıcı kimlik ve genişletilebilir model.** Layout, sayfa ve öğe
  için yeniden adlandırmadan etkilenmeyen kimlik; açık `page_id`, grup ilişkisi,
  referans harita ve öğe bağlantıları ekle. Değer tabanlı, Qt'siz core ve sabit nokta
  modeli korunsun. Eski `item_pages` için migration tanımlansın. **Kabul:** ekle/sil/
  yeniden sırala, kaydet/aç ve undo/redo sonrası bütün bağlantılar aynı hedefi bulur.
- [ ] **L-02 / P0 — Gerçek çok sayfalı düzenleme.** Sayfa ekle/sil/çoğalt/sırala;
  sayfa başına boyut, yön ve kenar; sayfa seçici ve bütün sayfaları görme; öğeyi
  sayfalar arasında taşıma. Koordinat dönüşümleri aktif sayfayı kullansın; PDF/yazıcı
  cihazı her sayfada doğru boyuta geçsin. **Kabul:** A4 dikey + A3 yatay aynı belgede
  düzenlenir ve doğru MediaBox/boyutlarla çıkar; ikinci sayfada sürükleme kaymaz.
- [ ] **L-03 / P1 — Profesyonel öğe düzenleme.** Çoklu seçim, grup/çöz, kopyala/
  yapıştır/çoğalt, hizala/dağıt, eş boyutlandır, referans noktası, döndürme, z sırası,
  görünürlük, baskıdan hariç tutma, kilit, cetvel, kılavuz ve ayarlanabilir snap ekle.
  Kâğıt ölçüleri birimi açık ondalık mm olarak girilebilsin, içeride `Um` saklansın.
  **Kabul:** 0,35 mm konum ve 0,18 mm çizgi kalınlığı UI/komut/AI/MCP'de aynı değere
  gider; tek sürükleme veya toplu hizalama tek undo oluşturur.
- [ ] **L-04 / P0 — Bağımsız harita çerçevesi.** Extent, merkez, ölçek, içerik
  dönüşü, katman listesi/sırası, stil anlık görüntüsü veya tema takibi ve çerçeveye
  özel CRS tanımla. Harita içeriğini kaydırma ile kâğıttaki kutuyu taşıma ayrı araçlar
  olsun. `paint_map` gerçek katman filtresini kullansın. **Kabul:** aynı sayfadaki
  1:1000 ve 1:5000 haritalar farklı katmanlarla çizilir; ana tuval görünürlüğündeki
  değişiklik kilitlenmiş çerçeveyi etkilemez. CRS dönüşüm desteği eksikse hata açık olur.
- [ ] **L-05 / P0 — Haritaya bağlı kartografik öğeler.** `linked_map_id` ile ölçek
  çubuğu, kuzey oku, lejant, overview ve dinamik metni ilişkilendir. Harita içerik
  dönüşü ile öğe kutusu dönüşünü ayır. Grid'in CRS'i, aralıkları, çizgi/çentik,
  kenar etiketleri ve sayı formatı düzenlenebilsin. **Kabul:** ikinci haritanın ölçeği
  değişince yalnız ona bağlı ölçek/metin güncellenir; silinen bağlantı preflight'ta
  raporlanır, sessizce ilk haritaya dönmez.

### 5.2. Veriyle çalışan öğeler

- [ ] **L-06 / P1 — İfade ve değişken altyapısı.** Proje, layout, sayfa, harita,
  seçili/atlas nesnesi ve rapor grubu bağlamlarını tipli sun. Konum, boyut, görünürlük,
  renk, metin, dosya adı ve ölçek özellikleri sabit değer veya ifadeye bağlanabilsin.
  Birim, null, tarih/yerel ayar, hata ve döngü davranışını tanımla. İfadeler keyfî
  dosya/ağ/kod yürütme sağlamasın. **Kabul:** ada/parsel başlığı ile alan toplamı
  önizleme/export'ta aynı snapshot'tan hesaplanır; eksik alan sessiz boş metin olmaz.
- [ ] **L-07 / P1 — Tam öğe özellikleri.** Lejantta gerçek semboloji, gruplar,
  filtre, manuel ad/sıra ve kolon düzeni; ölçek çubuğunda birim/segment/etiket;
  metinde font, satır aralığı ve taşma; resimde SVG/raster, en-boy oranı ve kaynağı
  paketleme ekle. QGIS 4.2 karşılığı olarak resim bir şekil öğesiyle kırpılabilsin.
  **Kabul:** katman adı listesi yerine baskıdaki sembollerle eşleşen lejant çıkar;
  taşınan projede logo kaybolmaz; Türkçe karakterler korunur.
- [ ] **L-08 / P1 — Tablo ve çok çerçeveli akış.** Alan/ifade kolonları, filtre,
  sıralama, toplama, koşullu biçim, kolon genişliği, yinelenen başlık ve sayfaya
  devam destekle. Manuel tablo ve sınırlı HTML/zengin metin içeriği aynı akış
  sözleşmesine otursun. **Kabul:** 500 satırlı tablo bütün satırları sayfalara taşır;
  satırların sığmadığı durumda sessiz kesme yerine devam veya açık taşma raporu olur.
- [ ] **L-09 / P2 — Grafik, profil ve 3B öğeleri.** Çubuk/çizgi/pasta grafiklerini
  alan/ifade/filtreyle bağla; QGIS 4.2 karşılığı olarak kategori ve renkleri katman
  sembolojisinden türet. Yükseklik profilinde güzergâh ve yüzey kaynağı; 3B haritada
  kamera, ölçekli çıktı ve sahne snapshot'ı tanımla. Bunlar mevcut yüzey/3B motorunun
  yeterliliğine bağımlı paketlerdir. **Kabul:** temel veri değişince grafik/profil
  kontrollü yenilenir; desteklenmeyen kaynak için boş resimle başarı bildirilmez.

### 5.3. Atlas, rapor, çıktı ve şablon

- [ ] **L-10 / P1 — Atlas.** Kapsama katmanı/seçimi, filtre, sıralama, sayfa adı,
  dosya adı ifadesi; sabit ölçek, ön tanımlı ölçek veya kenar payıyla kapsama;
  güncel atlas nesnesini gösterme/vurgulama/kırpma; ileri/geri önizleme ekle.
  Tek PDF ve nesne başına dosya destekle. **Kabul:** 100 parsel tek istekle sıralı,
  benzersiz adlandırılmış çıktı verir; sıfır sonuç, boş geometri, dosya adı çakışması
  ve iptal deterministik davranır. Snapshot ve manifest yeniden üretimi mümkün kılar.
- [ ] **L-11 / P2 — Rapor motoru.** Statik kapak, grup başlığı/altlığı, nesne
  bölümü ve toplamları iç içe tanımla; ada → parsel hiyerarşisiyle çalış. Atlasın
  “her nesne için aynı şablon” döngüsünden ayrı rapor modeli kur. **Kabul:** ada
  bazında başlık ve toplam, parsel bazında harita/tablo içeren rapor elle sayfa
  çoğaltılmadan üretilir; boş bölümlerin davranışı ayarlanabilir.
- [ ] **L-12 / P0 — Render ve çıktı doğruluğu.** Önizleme/PDF/SVG/raster/yazıcı
  aynı ölçü ve stil çözümünü kullansın. Desteklenen çizgi/metin/semboller vektör
  çıksın; raster gerektiren öğeler ayrı işaretlensin. Font gömme/ikame, metni metin
  veya kontur çıkarma ve sayfa aralığı seçenekleri tanımlansın. **Kabul:** 1:1000'de
  100 m çizgi kâğıtta 100 mm'dir; raster-only veri dışında harita PDF'de tek görüntü
  değildir. Önerilen test toleransı 0,1 mm; fiziksel baskıda “sayfaya sığdır” kapatılır.
- [ ] **L-13 / P2 — Coğrafi referans ve üretim çıktısı.** Referans harita üzerinden
  world file/coğrafi referanslı PDF; katmanlı GeoPDF, grup/sıra/ad/görünürlük eşlemesi;
  SVG ve PNG/TIFF çıktı, DPI ve renk profili seçenekleri ekle. Backend desteğini
  capability olarak bildir. **Kabul:** koordinat eşlemesi bağımsız okuyucuda doğrulanır;
  iç içe katman ağacı referans projeyle karşılaştırılır; desteklenmeyen özellik
  sessiz düz PDF'ye indirgenmez. GeoPDF bağımlılık/lisans kararı ayrı teknik incelemedir.
- [ ] **L-14 / P1 — Taşınabilir şablonlar.** Var olan kitaplığı; sürüm, küçük
  önizleme, kurum/proje değişkenleri, font/resim bağımlılıkları, katman/alan eşleme
  ve içe/dışa aktarma ile genişlet. Şablona arazi extent'i yanlışlıkla taşınmasın.
  QGIS `.qpt` içe aktarma istenirse ayrı dönüştürücü ve destek matrisi gerekir.
  **Kabul:** kurum şablonu başka projede katman eşlemesinden sonra aynı yerleşimi kurar;
  eksik bağımlılık listelenir; eski şablonlar migration testinden geçer.
- [ ] **L-15 / P1 — Preflight ve büyük işler.** Sayfa taşması, eksik font/resim,
  geçersiz ifade, kopuk harita bağlantısı, boş extent, tablo kesilmesi ve yetersiz
  raster çözünürlüğünü export öncesi raporla. Sayfa bazında render/cache, iptal,
  ilerleme, bellek sınırı ve UI dışı ağır hesap kullan. **Kabul:** 100 sayfalık atlas
  UI'yi kilitlemez; iptal edilen çıktı tamamlanmış diye yayımlanmaz. Referans makine
  ve fixture sabitlenip ilk önizleme/tepe bellek/iptal gecikmesi ölçülür; ölçümden
  önce performans başarısı iddia edilmez.

## 6. AI Surface: isteği tamamlayan çalışma ortamı

### 6.1. “Her şeyi yapabilme” kapsamı

| Kullanıcı işi | AI ve MCP'de gerekli yol |
|---|---|
| Proje aç/yeni/kaydet/farklı kaydet, import/export | Dosya hedefi, seçenekler, sonuç URI'sı, overwrite politikası ve işlem sonucu |
| Katman/stil/semboloji/etiket/tema | Oku, oluştur, düzenle, sırala, görünürlük ve stilleri uygula |
| Çizim/düzenleme/seçim/snap/ölçüm/undo | GUI tıklaması istemeyen tipli girdi, nesne referansı ve geometrik araç sonucu |
| Öznitelik sorgusu/kolon/hesap/toplu güncelleme | Şema keşfi, filtreli okuma, değişim önizlemesi ve toplu transaction |
| CRS/jeodezi/kadastro/imar/yüzey/processing | Algoritma keşfi, parametre doldurma, iş başlatma, iptal ve sonucu denetleme |
| Layout/atlas/rapor/yazdırma | Bölüm 5'teki her özellik için komut ve okunabilir sonuç |
| Görünüm/panel/aktif layout/araç | Anlamsal görünüm ve çalışma bağlamı kontrolü; mouse otomasyonu gerekmez |
| Settings/sağlayıcı/MCP/veritabanı | Normal ayar CRUD; gizli değer yerine credential referansı; yetki kapsamı açık yönetim işlemleri |

Üründe henüz olmayan bir yetenek “AI destekli” gösterilmez. Önce ortak komut ve
motor kabiliyeti eklenir; GUI, AI ve MCP aynı sürümde bu kabiliyeti kullanır.

- [ ] **A-01 / P0 — Tam ve tipli bağlam.** Aktif belge/layout/sayfa, seçim,
  katmanlar, CRS/birimler, görünüm, şablonlar, kullanılabilir algoritmalar, yetenekler
  ve etkili politikayı sorgulanabilir sun. Büyük veriyi sayfalı/sınırlandırılmış
  sorgula; bütün geometriyi prompt'a dökme. **Kabul:** “bunu A3'e yerleştir” ifadesi
  tek geçerli seçim ve layout bağlamında tekrar nesne seçtirmeden çözülür.
- [ ] **A-02 / P0 — Eksik komutları erişime aç.** C-01 envanterindeki dosya,
  layout, print, seçim, ayarlar, undo ve diğer aileleri C-02 etkileriyle aç. Eksik
  parametrede GUI tıklaması bekleyen coroutine'ler otomasyon yolunda tipli eksik
  girdi hatası versin. **Kabul:** parametreleri tam bir işlem pencere/modal açmadan
  hem sohbet hem MCP'den biter; yardım ve şemalar Registry'den üretilir.
- [ ] **A-03 / P0 — Kâğıt ve arazi koordinatını ayır.** Şemada `paper_length`,
  `ground_point`, `extent_ref`, `entity_ref` gibi anlamsal türler ve birimler tanımla.
  Kâğıtta 20 mm konum model tarafından üretilebilir; arazi geometrisi araç sonucuna
  veya açık kullanıcı verisine dayanmalı. Kullanıcının verdiği koordinat/CSV,
  CRS ve birimi doğrulanarak provenance taşıyan handle'a dönüşsün. **Kabul:** AI
  layout öğesini yerleştirir ve kullanıcının verdiği ölçüyü işler; arazi koordinatı
  uydurmaz. Her sayı için genel bir handle zorunluluğu getirilmez.
- [ ] **A-04 / P0 — Kalıcı iş yürütme döngüsü.** Bağlam → plan → doğrulama →
  policy → uygulama → sonuç okuma → gerekirse düzeltme → tamamlanma döngüsünü
  `ChatPanel` widget'ından bağımsız servis yap. Onay sonrası aynı iş kendiliğinden
  devam etsin. Araç bağımlılıklarını sırala; eksik plan adımını sessiz atlama.
  **Kabul:** katman oluştur → nesne ekle → layout kur → PDF çıkar → dosyayı doğrula
  tek kullanıcı talebinden tamamlanır; her tool-call kimliği doğru tek sonuç alır.
- [ ] **A-05 / P1 — Görsel ve yapısal önizleme.** Çizim/layout snapshot'ı, değişim
  özeti, etkilenen nesne sayısı ve preflight raporunu sohbet içinde göster; destekleyen
  modele görsel girdi sun. Çok modlu olmayan model yapısal raporla çalışabilsin.
  **Kabul:** “lejant haritanın üstüne binmiş” talebinde çakışma saptanır, düzeltilir
  ve yeni önizlemeyle doğrulanır; yalnız metinle “düzelttim” denmez.
- [ ] **A-06 / P1 — İş sürekliliği ve anlaşılır UI.** İş/adım durumu, varsayımlar,
  uygulanan değişiklikler, çıktı bağlantıları, durdur/devam/geri al ve gerektiğinde
  tek toplu soru kartı göster. Oturum geçmişi ve çalışma özeti kalıcı olsun. İş/tur/
  süre/maliyet sınırları Settings'te olsun; sınıra gelince sonuç kaybolmasın.
  **Kabul:** sağlayıcı hatasında tamamlanmış adımlar yeniden uygulanmaz; devam
  doğru checkpoint'ten olur; “iptal edildi” mesajı gerçekten uygulanmış işleri gizlemez.
- [ ] **A-07 / P1 — Sağlayıcı ve hata dayanıklılığı.** Mevcut lehçeler için tool-call,
  streaming, görsel, context ve structured-output kabiliyetlerini bildir. Yeniden
  deneme, rate limit ve zaman aşımını tek iş durumu üzerinden yönet. **Kabul:**
  parçalanmış akış/bozuk argüman/araç reddi modele tipli sonuç olarak döner;
  aynı hataya sınırsız tur harcanmaz, iptal tüm alt işleri sonlandırır.
- [ ] **A-08 / P1 — Türkçe uçtan uca değerlendirme.** Katalog sayısına ek olarak
  gerçek iş senaryoları, gereksiz soru sayısı, gereksiz onay sayısı, tamamlanma,
  doğruluk ve tekrar yürütme başarısı ölç. **Kabul:** bölüm 9'daki ortak fixture'lar
  GUI, sohbet ve MCP için aynı beklenen proje/çıktı durumunu üretir.

## 7. Settings: soru sıklığı ve onay ayrı tercihler olmalı

### 7.1. Kullanıcıya sunulacak davranış

Settings → **Yapay Zeka ve Otomasyon → Çalışma Davranışı** altında iki bağımsız
kontrol bulunmalı. Aşağıdaki adlar ve kimlikler **önerilen yeni sözleşmedir**.

| Ayar | Değerler | Önerilen varsayılan |
|---|---|---|
| `core.ai.onay_politikasi` | `her_degisiklikte`, `riskli_islemlerde`, `otomatik` | `riskli_islemlerde` |
| `core.ai.soru_politikasi` | `etkili_belirsizlikte_sor`, `yalniz_zorunlu`, `varsayimla_ilerle` | `yalniz_zorunlu` |
| `core.ai.is_butcesi.*` | Azami tur, süre, varsa sağlayıcı maliyet sınırı | Ölçümle belirlenen ürün varsayılanları |
| `core.ai.cikti_dizini` | Yeni çıktılar için varsayılan hedef | Kullanıcının seçtiği çalışma dizini |
| `core.ai.uzerine_yazma` | `sor`, `yeni_ad_uret`, `izin_ver` | `yeni_ad_uret` |
| MCP istemci profili | Aynı politikayı devral veya istemciye özel daha dar/geniş açık yetki; proje/dizin/işlem kapsamı | Genel politikayı devral, verilen erişim kapsamı içinde |

MCP profilleri tek global setting içine düzleştirilmemeli; kimliği doğrulanmış
istemci/principal anahtarıyla saklanmalı. İstemcinin kendi bildirdiği ad yetki kanıtı değildir.

**Onay modları:**

| İşlem | Her değişiklikte | Riskli işlemlerde | Otomatik |
|---|---|---|---|
| Bağlam/sorgu/önizleme | Doğrudan | Doğrudan | Doğrudan |
| Görünüm/aktif sayfa değiştirme | Doğrudan | Doğrudan | Doğrudan |
| Geri alınabilir belge/layout düzenleme | Plan başına tek onay | Doğrudan | Doğrudan |
| İzinli dizinde yeni dosya | Planın dış etki özetiyle onay | Doğrudan | Doğrudan |
| Üzerine yazma veya yıkıcı dış işlem | Onay | Onay; `yeni_ad_uret` ile önlenebilir | Açık kapsam ve overwrite tercihi izin veriyorsa doğrudan |
| Yetki kapsamı dışındaki işlem | Yetki gerekli sonucu | Yetki gerekli sonucu | Yetki gerekli sonucu; model kendine yetki veremez |

`otomatik`, izinli ve girdileri yeterli işi tekrar onay istemeden yürütmelidir.
Her aracın arkasına gizli bir “son onay” eklenmemeli. Önizleme, audit, doğrulama,
undo ve durdurma bu modda da çalışır. İnsan imzası gerektiren bir ürün işlemi varsa
otomatik uygulama ile imza durumu birbirine karıştırılmamalıdır.

**Soru politikası:**

- `etkili_belirsizlikte_sor`: Sonucu anlamlı değiştirecek belirsizlikleri tek kartta
  topla; her küçük tercihte konuşmayı durdurma.
- `yalniz_zorunlu`: Önce araçlarla bilgi topla; proje/kurum varsayılanı ve mevcut
  seçim yeterliyse ilerle. Gerçekten eksik zorunlu bilgi için bir soru sor.
- `varsayimla_ilerle`: Geri alınabilir ve makul varsayımları kısa durum mesajıyla
  kaydet ve uygula. Kaynak CRS'i bilinmeyen ölçüyü, erişim parolasını veya hangi
  dosyanın silineceğini uydurma. Çözülemeyen zorunlu girdi tek `input_required`
  durumuyla raporlansın; sonsuz soru/yeniden deneme döngüsü oluşmasın.

“A3 yatay hazırla” için kâğıt/yön yeniden sorulmaz. “PDF al” için kayıtlı çıktı
dizini ve çakışmasız ad varsa dosya adı sorulmaz. “Seçilileri sil” gibi açık talep
otomatik modda yeniden onaya çevrilmez. “Bunu taşı” için iki eşit olasılıklı nesne
ve hiç seçim yoksa önce bağlam çözülür; çözülemiyorsa yalnız nesne sorulur.

### 7.2. Politikanın uygulanması ve eski kurallardan geçiş

- [ ] **S-01 / P0 — Ortak policy engine.** Modelden bağımsız bir karar motoru;
  etki, hedef, kullanıcı tercihi, istemci kapsamı ve iş kimliğinden `allow`,
  `approval_required`, `input_required`, `deny` üretsin. Gate bu kararı uygulasın;
  hem otomatik hem elle onaylanan plan aynı Bus doğrulamasından geçsin. **Kabul:**
  sohbet, MCP ve tekrar bağlanma aynı girdilerde aynı kararı verir.
- [ ] **S-02 / P0 — Settings UI, kapsam ve saklama.** Uygulama/kullanıcı düzeyinde
  sakla; sıradan proje dosyası açılması güveni yükseltmesin. Etkin profil ve kaynağı
  sohbet başlığında/MCP keşfinde görülsün. Kullanıcı değişikliği yeni adımlara hemen
  uygulansın; bekleyen planlar tekrar değerlendirilsin. **Kabul:** uygulama yeniden
  açılınca tercihler korunur, başka proje dosyası bunları sessizce değiştiremez.
- [ ] **S-03 / P0 — Prompt ve gerçek yürütmeyi eşle.** `agent_preamble`, araç
  açıklamaları, MCP `instructions`, hata metinleri ve öneri kartları etkili
  policy'den üretilsin. Yalnız prompt'a “sorma” yazmak yeterli sayılmasın.
  **Kabul:** otomatik modda “mühendis uygulayana kadar bekler” gibi çelişkili
  metin yok; modeller araç başarısını bekleyip iş akışına devam eder.
- [ ] **S-04 / P0 — İzinleri yeniden sorma ve yetki yükseltme kontrolü.** Bir
  onay planın içeriği/revizyonu/etki kapsamına bağlansın; aynı iş için onay
  tekrarlanmasın. Model genel ayarları yönetebilsin; kendi onay politikasını veya
  erişim kapsamını genişletme işlemi ayrı, açık kullanıcı talebine dayanmalı.
  **Kabul:** model araç hatasını çözmek için kendiliğinden `otomatik` açamaz;
  kullanıcının önceden verdiği otomatik yetki de her komutta yeniden sorulmaz.
- [ ] **S-05 / P0 — Kural, test ve doküman migration'ı.** Mevcut `CLAUDE.md`
  2.8/2.10/5.7, `.claude/ai.md` R2/R3 ve P1/P15, `plan.hpp`, `gate.hpp`,
  `dispatcher.hpp` yorumları ve `scripts/ci-gate-ai.sh` zorunlu insan onayı/tek
  fabrika çağırıcısı varsayımlarını taşıyor. Bu kullanıcı talebi yeni ürün yönünü
  belirliyor: uygulama sırasında bu metinler ve testler policy tabanlı modele
  birlikte geçirilmeli; kuraldan habersiz bir bypass eklenmemeli. `docs/yapay-zeka/onay.md`,
  MCP/sohbet kılavuzları ve üretilen `docs/llms*.txt` aynı değişimde güncellensin.
  **Kabul:** CI hâlâ yetkisiz uygulamayı yakalar; yetkili otomatik uygulamayı
  “yasak onay çağırıcısı” diye reddetmez. Üretilen dosyalar elle değiştirilmez.
- [ ] **S-06 / P1 — Audit ve ayar migration'ı.** Karar kaynağı `human`/`policy`,
  etkili policy sürümü, kapsam, varsayım ve sonuç kaydedilsin. Otomatik işlem insan
  tıklaması gibi yazılmasın. Mevcut kurulumlar ilk yükseltmede `her_degisiklikte`
  davranışını korusun; Settings'ten tek seçimle diğer modlara geçebilsin. Yeni
  kurulum varsayılanı tabloda önerilen olsun. **Kabul:** kayıtlar hangi iznin hangi
  işi yürüttüğünü açıklayabilir; token/parola ve gereksiz hassas payload içermez.

## 8. MCP: dış ajan için tam uygulama arayüzü

### 8.1. Protokol kararları

Mevcut kodun hedefi **MCP 2026-07-28**. Resmî sürümleme sayfası bu revizyonda istek
başına sürüm/yetenek metadata'sını ve `server/discover` yolunu tanımlar; eski
`2025-11-25` ve öncesi `initialize` oturumlarıyla farkını açıklar. Gerekliyse iki
dönemi destekleyen adaptör eklenmeli; modern protokol eski handshake'e zorlanmamalı.
[MCP sürümleme ve uyumluluk](https://modelcontextprotocol.io/specification/2026-07-28/basic/versioning)

Streamable HTTP'de yanıt akışının kapanması ile iptal ilişkisi, stdio'daki bildirim
mekaniğinden farklıdır. Tamamlanmış bir yanıtın normal kapanması, kalıcı işi yanlışlıkla
iptal etmemeli. Protokol taşıma davranışı ile uygulamadaki plan/iş ömrü ayrı tasarlanmalı.
[MCP taşıma sözleşmesi](https://modelcontextprotocol.io/specification/2026-07-28/basic/transports)

Araçlar, kaynaklar ve opsiyonel uzantılar farklı işlevlerdir. Uygulama iş kimliğinin
bulunması tek başına MCP Tasks uzantısının desteklendiği anlamına gelmez. Yalnız
uygulanan ve müzakere edilen yetenekler ilan edilmelidir.
[MCP 2026-07-28](https://modelcontextprotocol.io/specification/2026-07-28),
[Tools](https://modelcontextprotocol.io/specification/2026-07-28/server/tools),
[Resources](https://modelcontextprotocol.io/specification/2026-07-28/server/resources)

- [ ] **M-01 / P0 — Gerçek istemci uyumu.** Modern discover, metadata/header
  doğrulaması, yetenek bildirimi ve anlaşılır sürüm hataları korunup gerçek
  istemcilerle test edilsin. Hedef istemci listesi ve sürümleri kaydedilsin;
  ihtiyaç varsa legacy adaptör eklensin. **Kabul:** en az iki bağımsız istemci
  discover → araç keşfi → sorgu → izinli yazma → sonuç doğrulama akışını tamamlar.
- [ ] **M-02 / P0 — Canlı taşıma ve yaşam döngüsü.** `mcp_service.cpp` içinde
  açık SSE responder yaşamı, keepalive, disconnect, backpressure ve kapanma
  temizliğini uygula. Gerçekte taşınmayan bildirim kabiliyetini ilan etme.
  **Kabul:** açık abonelik katalog değişimini alır; bağlantı kopması doğru işi
  etkiler; tamamlanan `tools/call` yanıtının kapanması öneriyi geri çekmez.
- [ ] **M-03 / P0 — Sohbetle aynı tam araç yüzeyi.** C-01/A-02 kapsamının tamamı
  MCP'de bulunsun. Girdi ve çıktı şemaları, birimler, enum'lar, yan etkiler ve
  policy bilgisi katalogdan gelsin. Özel bir sınırsız `execute` aracıyla doğrulama
  atlanmasın. **Kabul:** layout oluşturma, düzenleme, export ve dosya sonucu
  okuma GUI'de onay penceresine mecbur kalmadan seçilen politikayla çalışır.
- [ ] **M-04 / P0 — Plan/iş işlemlerini tamamla.** Durum oku, plan önizle,
  policy kapsamında uygula, iptal et ve sonucu al işlemleri tanımla. `_meta.plan`
  ekleme davranışı gerçek `AiService` ile uyumlu olsun. Otomatik modda dönen
  durum gerçek `applied/running/completed` sonucu olsun; hep `pending` dönmesin.
  **Kabul:** dış ajan insanın sohbet mesajı göndermesini beklemeden işini takip eder;
  onay gereken modda tek plan kararıyla devam eder, başka istemcinin planına ekleyemez.
- [ ] **M-05 / P1 — İş ve çıktı kaynakları.** Belge özeti, layer schema, layout
  ağacı, seçili öğeler, önizleme, preflight, iş sonucu ve artefaktları kaynak
  olarak sun. Örnek uygulama URI'ları: `kentoscad://documents/{id}/summary`,
  `kentoscad://layouts/{id}/preview`, `kentoscad://jobs/{id}/result`. Bunlar yeni
  uygulama tasarımıdır, mevcut endpoint iddiası değildir. **Kabul:** ajan layout'u
  yalnız oluşturmaz; son durumunu ve ürettiği dosyanın doğrulama raporunu okuyabilir.
- [ ] **M-06 / P1 — Uzun işler ve olaylar.** Atlas, import ve processing işlerini
  `job_id`, ilerleme, iptal, hata ve sonuçla izle. Desteklenen Tasks uzantısı ayrıca
  uygulanıp müzakere edilirse kullan; diğer istemciler için uygulama araçlarıyla
  durum sorgulama fallback'i sun. **Kabul:** reconnect sonrasında durum bulunur,
  aynı istek tekrarı yeni iş açmaz; kısmi çıktı tamamlanmış görünmez.
- [ ] **M-07 / P1 — İstemci kapsamı ve veri izolasyonu.** Mevcut loopback/belirteç
  temelini koru; belge, dosya dizini ve işlem kapsamını kimliği doğrulanmış istemciye
  bağla. Handle, plan ve kaynak erişimini bu kapsama göre denetle. UI thread'ine
  belge erişimini mevcut dispatcher ilkesiyle taşı. **Kabul:** iki istemci birbirinin
  handle/planını kullanamaz; aynı belgeyi düzenleyen işler revizyon kontrolünden geçer.
- [ ] **M-08 / P1 — Kullanılabilir bağlantı yönetimi.** Settings'te sunucu durumu,
  adres, bağlı istemciler, etkin policy/kapsam, token yenile/iptal ve son hatalar
  bulunsun. Mümkün olan istemcide Bearer kimlik doğrulamasını tercih et; mevcut
  token'lı URL desteğinin log/ekran paylaşımı riskini azalt. **Kabul:** kullanıcı
  bağlantıyı test eder, tek istemcinin yetkisini kaldırır; gizli değer modele veya
  audit metnine taşınmaz.
- [ ] **M-09 / P2 — Keşif ve iş şablonları.** Büyük katalog için uygulama düzeyinde
  arama/alan filtresi; atlas, kadastro kontrolü ve rapor gibi işler için sürümlü
  iş şablonları sun. `prompts` veya Skills uzantısı ancak gerçek destek varsa
  ilan edilsin. **Kabul:** istemci tam katalog yolunu da kullanabilir; arama sonucu
  gerekli araçları gizlemez; protokolün desteklemediği pagination alanları uydurulmaz.

## 9. Ölçülebilir uçtan uca kabul senaryoları

Aşağıdaki senaryolar özellik bitiş ölçütüdür. Mevcut birim testleri korunmalı;
eksik masaüstü/taşıma/çıktı entegrasyon testleri bunları tamamlamalı.

| No | İstek / kurulum | Geçme koşulu |
|---|---|---|
| E-01 | “Seçili parselleri A3 yatay, 1:1000, lejant ve kuzey oklu yerleştir; çalışma dizinine PDF çıkar.” | Otomatik + yalnız zorunlu modunda yeterli fixture için **0 soru, 0 onay**; layout ve doğrulanmış PDF mevcut. |
| E-02 | Aynı isteği MCP'den gönder. | GUI'ye tıklamadan aynı anlamsal proje durumu; çıktı kaynağı, ölçek ve sayfa ölçüleri eşit. |
| E-03 | Aynı sayfada farklı tema/ölçekli iki harita. | Lejant/ölçek/kuzey doğru haritaya bağlı; birinin değişimi diğerini bozmaz. |
| E-04 | A4 dikey ve A3 yatay sayfaları düzenle, sırasını değiştir, kaydet/aç. | Öğeler doğru sayfada; sürükleme ve PDF sayfa boyutları doğru; undo/redo çalışır. |
| E-05 | “100 parsel için ada/parsel adında atlas PDF'leri üret.” | 100 doğru hedef/başlık; sıralama ve dosya adları deterministik; boş geometri/çakışma raporu var. |
| E-06 | 500 satırlı tablo, grup toplamları ve dinamik grafik içeren rapor. | Veri eksilmeden sayfalara akar; toplamlar doğrulanır; grafik renkleri katman sembolojisiyle eşleşir. |
| E-07 | Her değişiklikte onay modunda 12 adımlı belge düzenlemesi. | **Tek plan onayı**, tek undo; uygulama sonrası doğrulama otomatik devam eder. |
| E-08 | Planın ortasında geçersiz nesne/parametre. | Bağımlı plan sessiz kısaltılmaz; belge hash'i aynı kalır; başarısız çağrı başarılı diye işaretlenmez. |
| E-09 | Plan beklerken belgeyi değiştir; eski handle veya planı uygula. | Yanlış hedefte işlem yok; revizyon çatışması, taze bağlam ve gerekiyorsa yeni kapsam onayı. |
| E-10 | Dosya zaten var; `yeni_ad_uret` seçili. | Tekrar soru sormadan benzersiz ad; var olan dosya değişmez; gerçek yol sonuçta döner. |
| E-11 | MCP bağlantısı kesilsin ve istek aynı anahtarla tekrar gelsin. | Çift mutasyon yok; işin son durumu bulunur; başka istemcinin işi etkilenmez. |
| E-12 | “Bunu taşı”; seçim yok ve iki eşit aday var. | Araçlarla çözüm aranır; zorunluysa tek hedef sorusu; gereksiz işlem onayıyla karıştırılmaz. |
| E-13 | Kullanıcı koordinat CSV'siyle geometri oluştur; yanlış CRS de dene. | Doğru birim/CRS/provenance ile işlenir; belirsiz CRS uydurulmaz; layout mm'si arazi koordinatı sanılmaz. |
| E-14 | Ayar değiştir, yeniden başlat, başka proje aç, ikinci MCP istemcisini bağla. | Soru/onay tercihi korunur; istemci kapsamları doğru; proje güveni yükseltmez. |
| E-15 | Önizlemede taşma/eksik logo/font ve export iptali. | Sorunlar kaynak/öğe kimliğiyle raporlanır; yarım dosya başarı olarak sunulmaz. |
| E-16 | QGIS 4.2 ile referans karşılaştırması. | Aynı veri/CRS/font/ölçekle çok sayfa, atlas, rapor, tablo, grafik, resim kırpma ve GeoPDF fixture'ları karşılaştırılır. |

**Karşılaştırma yöntemi:** Geometri/ölçek/bağlantı/alan toplamları yapısal testle;
yerleşim taşması ve görsel kalite render karşılaştırmasıyla; PDF sayfa kutuları,
fontlar, vektör/raster içeriği ve GeoPDF koordinatları bağımsız okuyucuyla doğrulanır.
Farklı render motorlarının tüm PDF baytlarının aynı olması beklenmez. Proje/journal
determinizmi ile görsel tolerans ayrı test edilir. QGIS fixture'ında kullanılan tam
4.2.x sürümü ve bütün export seçenekleri kaydedilir.

## 10. Uygulama sırası ve bitiş kapıları

| Aşama | Kapsam | Bağımlılık ve çıkış koşulu |
|---|---|---|
| **P0 — Temel doğruluk ve erişim** | BR-01; C-01–04; L-01/02/04/05/12; A-01–04; S-01–05; M-01–04 | Önce capability/etki/policy sözleşmesi. Ardından layout/print erişimi ve agent döngüsü. E-01–04, E-07–09 geçer; mevcut veri okunur. |
| **P1 — Günlük üretim** | C-05; L-03/06–08/10/14/15; A-05–08; S-06; M-05–08 | P0 üzerinde atomik dışa aktarma, atlas, görsel doğrulama ve kalıcı işler. E-05, E-10–15 geçer; UI/taşıma entegrasyonu doğrulanır. |
| **P2 — QGIS kapsamının tamamlanması** | L-09/11/13; M-09 | İfade/akış motoru, render backend'i, yüzey/3B ve export yeteneklerine bağlı. E-06/E-16 ve tüm eşleme matrisi geçer. |

Öncelik, kapsamı düşürmek için kullanılmamalı. P0 sonunda “tam QGIS eşitliği” veya
“uygulamanın her şeyi AI'ye açık” denmez; yalnız biten envanter satırları raporlanır.

Uygulama sahipliği mevcut modül sınırlarına göre dağıtılmalı:

- **Core/command:** layout modeli, migration, birimler, etki/şema sözleşmesi,
  Registry, transaction/journal ve tüm yeni komutlar.
- **App/render/io:** tasarımcı, önizleme/export, çok sayfa, kaynak paketleme,
  Settings UI ve gerçek Qt MCP taşıması.
- **AI:** policy, plan ve iş yürütücüsü, katalog, provenance, provider davranışı,
  MCP protokolü ve yapısal sonuçlar; Qt bağımlılığı eklenmeden.
- **Test/docs:** registry kapsam testi, eski dosya/journal fixture'ları, Türkçe
  değerlendirme, istemci entegrasyonu, QGIS referans çıktıları ve üretilmiş kılavuzlar.

**Tamamlanma tanımı:** Ürün envanterindeki her iş GUI, AI ve MCP'den aynı kurallarla
tamamlanıyor; kullanıcı Settings'ten onay/soru davranışını seçebiliyor; otomatik
modda yeterli girdili işler tekrar soru/onay istemiyor; layout karşılaştırma matrisi
gerçek dosyalar ve ölçülebilir testlerle karşılanıyor. Sadece yeni panel, araç adı,
enum değeri veya prompt eklenmesi tamamlanma sayılmaz.
