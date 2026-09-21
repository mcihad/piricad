# KentOSCad — Çizim, yakalama ve CAD araçları planı

Plan tarihi: **21 Eylül 2026**. Ana referans: yerel `main`, **`d294809`**.
Bu belge mevcut çizim/yakalama/düzenleme yüzeyinin incelemesini, Netcad ve AutoCAD araç setleriyle
karşılaştırmayı ve sıralı iş paketlerini birleştirir. İşaretlenmemiş maddeler yapılacak iştir;
uygulanmış özellik iddiası değildir. İşaretler `TODOS.md` §0 ile aynıdır: `[ ]` yapılacak, `[~]`
kısmen (altında **Yapıldı**/**Kalan**), `[x]` bitti, `[!]` karar bekliyor.

**Hedef:** bir harita mühendisinin Netcad'de her gün yaptığı alım girdisini ve bir AutoCAD
kullanıcısının refleks olarak aradığı inşa/düzenleme fiillerini, hepsini **komut** olarak
(CLAUDE.md 1.1), Türkçe adlarıyla (2.6), AI'nin öğrenebileceği bir yüzey hâlinde (2.8) getirmek.

## 0. Verilen kararlar

| Karar | Seçim | Sonucu |
|---|---|---|
| Açı kuralı | **Semt + grad varsayılan.** `@100<45` = 45 grad, kuzeyden saat yönüne | `core.aci.birim` (var, varsayılan grad) okunur; yeni `core.aci.kural` = `semt` \| `matematik`. Günlük **çözülmüş** argüman tutar (`journal.hpp:38`) → eski kayıtlar aynen oynar. Etkilenen yalnız canlı metin: komut satırı, betik dizeleri, AI. |
| Öncelik | **Netcad alım girdisi önce** | P0 → P1 → P2 → … sırası bağlayıcı. P2–P7 birbirinden bağımsız. |
| Tasarım | Alım yöntemleri **nokta fonksiyonu** olarak tek gramere girer, artı her birinin bir çizim komutu olur | Bir kez tanımlanır, her nokta isteminde çalışır (Article 1.2, 5.11). |

## 1. Mevcut zemin — yeniden yazma, üstüne kur

| Ne | Nerede | Kullan |
|---|---|---|
| Yakalama motoru, 18 mod (UÇ ORTA MERKEZ AĞIRLIK KESİŞİM DİK YAKIN DÜĞÜM UZANTI PARALEL UZATILMIŞ-KESİŞİM KILAVUZ EKLEME + ızgara/kutupsal/dik/adım/normal) | `src/core/include/kentos_cad/core/snap.hpp`, `core/src/snap.cpp` | Yeni bit = enum + `snap_mode_id/label` + `SnapObjectMask`; idempotens şart |
| Yardımcıların uygulandığı tek yol | `command/aids.hpp`, `context.hpp` `InputAwaiter<Point2>::await_resume` | Nokta fonksiyonları buradan ÖNCE, ayrıştırma anında çözülür |
| Tek gramer: mutlak, `@dx,dy`, `@d<a`, ifadeler, `anahtar=değer(iç içe)` | `src/command/src/parser.cpp` (`Token::Kind::Absolute/Relative/Polar/KeyValue`, `ExprParser`; `Polar` çözümü `parser.cpp:821-824`) | Fonksiyon sözdizimi `KeyValue`'nun iç içe mekanizmasını genişletir |
| Geometri yardımcıları | `core/pick.hpp`: `line_intersection`, `closest_point_on_line`, `segment_intersection`, `ring_contains` | Elle yeniden yazma yok |
| Lastik bant | `command/input.hpp` `RubberShape{Line,Rectangle,Ring,Circle,Arc,Ellipse,Curve,Dimension,Block}`, `context.hpp` `PointOptions` | |
| Oturum ayarları | `src/core/src/settings.cpp`: `core.yakalama.*`, `core.aci.birim` (grad/derece/radyan), `core.cizim.birim`, `core.arayuz.dinamik_girdi`; `MOD` komutu (`settings.cpp` `run_mode`, Session kapsamı) | `core.aci.kural` buraya |
| Eşitlik kanıtı kalıbı | `tests/unit/test_proof.cpp` (`Rig`, `execute_line`, `script::JsonRunner`, günlük replay) | Her yeni komut bir vaka |
| Yakalama testleri | `tests/unit/test_snap.cpp` | |
| Geodezi alanı | `src/domain/geodesy/` (`stakeout_command.cpp` semt hesabı, `helmert`, `fit`, `reproject`) | Poligon hesabı buraya, `geodesy.` kimliğiyle (domain.md R5, R15) |
| Nokta varlığı | `core.point_draw` (`NOKTA`), `core.points` (`NOKTALAR`: no, Y, X, [Z], [kod]), `SnapNode` | Alım `n(1284)` ile buna bağlanır |
| Seçim kipleri | `pick.cpp` `{PENCERE,KESEN,HEPSİ}`, `core/pick.hpp` `PickMode`, `pick_in_box` | ÇİT/ÇOKGEN buraya |
| Yer tutucu araçlar | `main_window.cpp` `placeholder(...)`: KES, Panoya Kopyala, YAPIŞTIR, Katman Yöneticisi, Sorgula | P6, P7 |
| Komut sayfası iskeleti | `docs/komutlar/line.md`: Ne yapar · Adlar · Sözdizimi · Parametreler · Örnekler (Komut satırı / Arayüz / Betik / Üçü de aynı) · Geri alma · Betikten kullanım · Hatalar | Her yeni sayfa |
| Gramer belgesi | `docs/komutlar/komut-satiri.md`, `docs/baslangic/ilk-adimlar.md` §3 | Nokta fonksiyonları ve açı kuralı buraya |

## 2. Her paket için bağlayıcı ilkeler

1. **Tek gramer** (5.11, command.md R16/R17): ikinci ayrıştırıcı, ifade motoru ya da "alım modu" yok.
2. **Tek yol** (command.md P10): fonksiyonlar dispatch'ten önce `Point2`'ye; `Bus` çözülmüş noktayı
   günlükler; hiçbir komut `InputSource`'a bakmaz.
3. **Kütüphane varken elle yazma yok** (Article 9, 5.16): Clipper2 ofset/boolean/sadeleştirme, CDT
   üçgenleme, `pick.hpp` kesişim.
4. **`Mm` int64**; trigonometri `double` yerel, sonuç yuvarlanır; iki platformda farklı bit = hata (2.5).
5. **Mevzuat sabiti C++'a yazılmaz** (5.13): poligon toleransları, ölçü metin yükseklikleri `/data/catalogs`.
6. **Türkçe önce** (R7), `AiAccessible` — bu komutlar AI'nin eğitim yüzeyi (2.8).
6a. **Araç eklenirken arayüzü de eklenir.** Kullanıcının talimatı: *"araçları eklerken arayüz ile
   kullanımlarını ve ilave araçları da arayüze eklemen gerekiyor"*. Yeni bir komut yalnız komut
   satırından çalışıyorsa paket bitmemiştir. Her pakette, o paketin komutları için:
   menü girişi (**Çizim** / **Değiştir** / **Harita** / **Analiz**), gereken yerde araç çubuğu
   düğmesi ve `Glyph`, F-tuşu ya da kısayol gerekiyorsa o, yeni yakalama bitinin **F3 menüsünde**
   ve `MOD` listesinde görünmesi, yeni seçim kipinin durum çubuğu ipucunda görünmesi, ve
   `docs/komutlar/<slug>.md`'nin **Arayüz** bölümünün gerçekten yazılmış olması. Bu 5.15'in
   (fareyle erişilemeyen özellik yok) ve 1.2'nin (GUI eşit istemci) uygulamasıdır: bir jest
   komutu çağırır, komut jesti taklit etmez. Gerekli bir bileşen setin dışındaysa `widgets.cpp`
   /`fields.cpp`'ye girer ve 6.13'e göre canlı standarda, kapı envanterine ve
   `docs/baslangic/bilesenler.md`'ye aynı değişiklikte eklenir.
7. **Bitti** = command.md DoD + Article 6: `KENTOS_COMMAND` tek beyan · `make reference` + dört üretilmiş
   dosya aynı commit'te (6.14) · `docs/komutlar/<slug>.md` sekiz bölüm + `docs/README.md` satırı (6.12) ·
   `test_proof.cpp` eşitlik kanıtı + günlük replay (6.4) · iptal testi (boş undo deltası) · `Value`
   gidiş-dönüş · gramer değiştiyse fuzz korpusu (6.7) · CHANGELOG · `make check` yeşil (clang-tidy ağaç
   genelinde önceden kırmızı: `dxf_reader.cpp`, `flow_layout.cpp` — kendi satırlarını süz) ·
   clang-format **18** (`/opt/homebrew/opt/llvm@18/bin` PATH'te).

---

## PU — Arayüzden erişim: fareyle çalışmayan araçlar (tamamlandı, 2026-09-21)

**Neden bir paket:** kullanıcı araç kutusundaki araçları fareyle çalıştıramadı — *"toolbox
üzerindeki araçlar da mouse ile çalıştıramadım mesela blok blokekle, ölçüm araçları ve diğerleri
çok kötü ve çalışmıyorlar"*. Ölçüldü, tahmin edilmedi (`KENTOS_TOOL_PROBE`): **97 komutun 33'ü
yalnız adını yazarak başlatılabiliyordu.** Fare kullanıcısının o komutları hiç yoktu. Bu 5.15'in
aynadaki hâli ve 1.2'nin (GUI eşit istemci) ihlali; ayrıca elle tutulan bir menü tablosu 5.10'un
yasakladığı ikinci komut listesidir.

- [x] **Menüler `Registry`'den tamamlanıyor** (`MainWindow::completeMenusFromRegistry`): küratörlü
      girişlerin altına, kategorisinin menüsüne, tek bir **Diğer komutlar** satırı olarak. Sonraki
      paketin komutu menüsüne kendiliğinden gelir — kimseye söylemek gerekmez. `Çıkış` gibi son
      satırı olan menüde kuyruk onun ÜSTÜNE girer.
- [x] **`CommandSpec::title`** — insan okuyacağı Türkçe etiket (`Blok Ekle`, `Çıktı Yerleşimi`).
      Ad tek kelimedir (`ÇIKTIYERLEŞİMİ`); ondan üretilen menü Türkçe değildir. `ToolSpec` bunu
      zaten taşıyordu. 92 komutun hepsi doldu; `test_command.cpp` boş başlığı reddediyor.
      Etiket menüye, üretilmiş referansa ve ajan kataloğunun MCP `title` alanına gidiyor.
- [x] **Küratörlü girişler**: KILAVUZ, ETİKET, EŞYÜKSELTİ (Çizim); APLİKASYON, HACİM, OTURT,
      DÖNÜŞTÜR, ALANÖLÇ, KOORDİNAT (Harita). `SORGULA` artık gerçek komutu çalıştırıyor — yanında
      duran `Faz 2` ölü satır kaldırıldı.
- [x] **Yazı isteyen komut, klavyeyi yazılacak yere taşıyor.** `BLOK`, `BLOKEKLE`, `KATMAN`,
      `KATMANAT`, `ETİKET` ilk olarak bir AD soruyor; istem durum satırına yazılıyor ve odak
      tuvalde kalıyordu, yani tuşlar hiçbir yere gitmiyordu. `onPromptChanged` artık `Text`,
      `Number`, `Integer` isteminde komut satırını açıp odaklıyor; `Point`/`Selection`'da
      dokunmuyor (Esc, lastik bant ve oklar tuvalde).
- [x] **`Prompt::choices`** — cevabı bilinen kümeden olan istem o kümeyi sunuyor: çizimdeki
      bloklar, katmanlar, fiil listeleri. Komut bilir (belgeyi tutar), kabuk sunar
      (`CommandLine::offerChoices`). Kısıt değil, öneri: yeni bir bloğun adı tanımı gereği listede
      olamaz.
- [x] **Sözle cevap veren komut cevabını görünür kılıyor.** Sorgu komutları transkripte yazıyor ve
      o panel kapalı başlıyor: `Katmanları Listele`'ye basmak ölü bir satıra basmakla aynı
      görünüyordu. Menü satırı artık `Geçmiş` sekmesini önce açıyor. (Bu arada `transcriptDock_`
      üyesinin hiç atanmadığı ve iki tema döngüsünün onu sessizce atladığı ortaya çıktı — kaldırıldı.)
- [x] **Henüz olmayan özellik kendini anlatıyor.** KES, PANOYA KOPYALA, YAPIŞTIR, KATMAN YÖNETİCİSİ
      `setEnabled(false)` idi; tıklanınca hiçbir şey olmuyordu. Artık ne yapacağını, hangi fazda
      geleceğini ve bugün ne kullanılacağını söyleyen bir kutu açıyor.
- [x] **Kapılar** (üçü de gerçek ikiliyi çalıştırır; `/tests` Qt bağlamaz):
      `tool-reach` (97/97 fareyle başlatılabiliyor), `tool-answerable` (istem geldi, odak doğru
      yerde, seçenekler sunuldu, cevap geçti), `menu-bar` (on bir menü açılıyor, boş değil, ekranı
      aşmıyor).

**Sonraki paketler için bağlayıcı:** §2.6a. Bir komut eklendiğinde menü girişi kendiliğinden gelir,
ama `title`, küratörlü yer, gereken düğme ve komut sayfasının **Arayüz** bölümü paketin işidir.

---

## P0 — Açı kuralı: semt ve grad (önkoşul, tek commit)

- [x] **P0-1** `core.aci.kural` oturum ayarı (`semt` varsayılan | `matematik`), `settings.cpp`'de
  `core.aci.birim`'in yanına aynı kalıpla; `MOD kural semt` ile değişir.
- [x] **P0-2** `parser.cpp` `Kind::Polar` çözümü iki ayarı okur: birim (grad `×π/200`, derece `×π/180`,
  radyan), kural `semt` → `x = d·sin θ, y = d·cos θ`; `matematik` → mevcut. Çözüm `Parser`'a bir
  `AngleConvention` bağlamı olarak geçer — global okunmaz.
- [x] **P0-3** Açık birim soneki: `@100<45g`, `@100<45d`, `@100<0.7r`. Sonek yoksa ayar geçer.
- [x] **P0-4** Kutupsal izleme göstergesi, `ÖLÇ`/`KOORDİNAT`/`APLİKASYON` çıktıları aynı iki ayara göre yazar.
- [x] **P0-5** Docs: `komut-satiri.md`, `ilk-adimlar.md` §3 örneği yeni anlamıyla; `MOD kural matematik`
  yolu gösterilir.
- [x] **P0-6** Test: dört çeyrek × her birim × her kural × sonek; `@100<0` semt'te kuzey, matematik'te doğu;
  fuzz korpusu; eşitlik kanıtı (`ÇİZGİ 0,0 @100<50` komut satırı = betik).

## P1a — Nokta fonksiyonları (tek gramer, her nokta isteminde)

Sözdizimi `ad(arg, …)`; argüman: nokta (mutlak, `@`, fonksiyon, `son`, `n(no)`), sayı (ifade), açı
(P0 kuralı). Hepsi saf; sonuç `Point2`, dispatch'ten önce.

**Argüman ayırıcısı koordinat ayırıcısıyla aynı karakterdir.** `orta(0,0,100,0)` dört sayı değil
iki noktadır, `dik(0,0,100,0,30,-5)` iki nokta ve iki sayıdır. Bu yüzden argümanlar sayılmaz,
**imzaya karşı eşlenir**: mutlak yazılan bir nokta iki alan, `@100<50` / `son` / iç içe bir çağrı
tek alan harcar. Bir ada birden çok biçim düşüyorsa (`kes`) hepsi denenir ve tam olarak biri
uymak zorundadır; ikisi birden uyarsa çağrı reddedilir.

- [x] **P1a-1** `Token::Kind::Call` ve çözücü (`parser.cpp`), `parser.hpp` gramer yorumu (R17 listesi
  güncellenir), `context.cpp`.
- [x] **P1a-2** `son` — son verilen nokta (`@`'ın tabanı). **Çıplak `son` yalnız argüman listesinde koordinattır**; tek başına bir istemde `son()` ya da `@0,0` yazılır, çünkü `KATMAN son` bir katman adıdır ve her `son`'u noktaya çevirmek o yazımı elden alırdı.
- [x] **P1a-3** `n(1284)` — 1284 numaralı ölçü noktası (`NOKTALAR` → `SnapNode`); yoksa "1284 numaralı
  nokta yok".
- [x] **P1a-4** `orta(A,B)` — iki nokta ortası.
- [x] **P1a-5** `ile(P, @dx,dy)` / `ile(P, @d<a)` — P tabanlı göreli.
- [x] **P1a-6** `dik(A,B,ayak,boy)` — **dik ayak / dik boy**: AB üzerinde A'dan `ayak`, sola pozitif `boy`
  dik. İşaret kuralı belgelenir (sol +, Netcad ile aynı).
- [x] **P1a-7** `semt(S,açı,kenar)` — istasyondan semt + kenar (P0 kuralı; sonek alır).
- [x] **P1a-8** `kes(A,açı1,B,açı2)` — iki doğrultu (`line_intersection`; paralelse hata adlarıyla).
- [x] **P1a-9** `kes(A,r1,B,r2,yön)` — iki mesafe, iki çözüm; `yön`: `sol`|`sağ` ya da yakın nokta;
  kesişmiyorsa hata mesafeleri söyler. Sessiz seçim yok. **Yakın nokta `yon=` ile yazılır:**
  çıplak bir koordinat orada `kes(A,B,C,D)` okumasıyla aynı alan sayısını harcar ve iki okumadan
  birini sıraya bakarak seçmek tam da bu maddenin yasakladığı şeydir; `sol`/`sağ` çıplak yazılır,
  çünkü sözcük alanını başka hiçbir biçim tüketemez.
- [x] **P1a-10** `kes(A,B,C,D)` — iki doğru.
- [x] **P1a-11** `ara(A,B,t)` / `ara(A,B,d m)` — oran ya da metre.
- [x] **P1a-12** `uzanti(A,B,d)` — B'den öteye.
- [x] **P1a-13** `xy(P,Q)` — P'nin sağa, Q'nun yukarı değeri (`.x/.y` süzgeci).
- [x] **P1a-14** Hata metinleri beklenen/verileni söyler (R19).
- [x] **P1a-15** Docs: `komut-satiri.md` "Nokta fonksiyonları" bölümü (tablo + çizim örnekleri);
  `ilk-adimlar.md`'ye dik ayak örneği.
- [x] **P1a-16** Test: `test_command.cpp` her fonksiyon için bilinen üçgenle `Mm` eşitliği; idempotens;
  fuzz korpusu (iç içe fonksiyon, paralel doğrultu, sonekli açı).

## P1b — Alım komutları (fonksiyonların etkileşimli, çizen hâli)

- [x] **P1b-1** `core.perp_offset` — `DİKAYAK`, `DIKAYAK`, `PERPOFFSET`, `DA`: taban A,B; tekrar `ayak boy`
  çiftleri → `NOKTA`; `cizgi=evet` ile `ÇOKLUÇİZGİ`; sağ tık/Esc bitirir; lastik bant taban çizgisinde.
  Hesap `core::perpendicular_offset`'e taşındı ve `dik()` nokta fonksiyonuyla **paylaşılıyor** — işaret
  kuralının ikinci kopyası yok. Arayüz: **Çizim > Dik Ayak** ve araç kutusunda Nokta ailesinde (§2.6a).
  **Girdi katmanında üç boşluk kapandı:** `Value::Kind::NumberList` eklendi (sayı dizisi yoktu),
  `bind_tokens` sayı dizisini biriktiriyor (tek liste-biçimli tür `değiştiriyordu` — command.md P15),
  `record_awaited` sayı dizisini günlüğe dizi olarak yazıyor (son çifti yazıyordu, yani replay başka
  bir çizim üretiyordu), `next_of` diziyi istek başına bir okuma tüketiyor, ve iki okumalık bir dizinin
  JSON'da nokta gibi görünmesi `bus.cpp`'de tamir ediliyor (tamsayı listesi için zaten yapılanın aynısı).
- [x] **P1b-2** `core.survey_polar` — `ALIM`, `SURVEY`, **`ALM`**: istasyon, isteğe bağlı `baglama=`
  (açının sıfırı); tekrar `aci kenar` → nokta; `cizgi=evet` ile birleştirir.
  **`AL` değil `ALM`:** `AL` `ALAN`'ın kısaltmasıdır ve onu kapmak ALAN'ı gölgelemiyor, **düşürüyordu** —
  başarısız kayıt yalnız bir log satırı yazar, yani derleme temiz, suite yeşil ve parsel çizen komut
  yok olur. Tripwire testi tam bunun için var. Bu madde planın `AL` yazımını geçersiz kılar.
  Semt hesabı `core::polar_offset_turns` olarak `core/angle.hpp`'ye kondu ve `APLİKASYON` ile
  paylaşılıyor: aplike edilen nokta ile geri okunan nokta aynı milimetreye düşer. Arayüz: **Çizim >
  Alım** ve araç kutusunda Nokta ailesinin üçüncü üyesi (§2.6a).
- [x] **P1b-3** `core.intersect_point` — `KESİŞİMNOKTA`, `KESISIMNOKTA`, `INTERSECTPT`, `KSN`:
  `yontem=dogrultu|mesafe|dogru`; iki-mesafe çözümü `yon=sol|sag` ile, sessiz seçim yok.
- [x] **P1b-4** `core.point_along` — `ARANOKTA`, `POINTALONG`, `ARN`: AB + oran/mesafe (dizi olarak);
  `sayi=k` k eşit parçaya bölen k−1 nokta (uçları tekrar koymaz).
- [x] **P1b-3/4 ortak zemin:** üç kesişim ve iki ara-nokta yapısı `command/construct.hpp`'ye taşındı ve
  `kes()`, `ara()`, `uzanti()` nokta fonksiyonlarıyla **paylaşılıyor** — aynı cevap, aynı çözüm seçimi,
  aynı Türkçe ret cümlesi, iş yazılmış da olsa tıklanmış da olsa. `ara()`nın metre biçimi artık oran
  biçiminin yuvarlamasından geçiyor, yani `0.2` ile `20 m` aynı milimetre. Arayüz: **Çizim > Kesişim
  Noktası** ve **Çizim > Ara Nokta**, ikisi de Nokta ailesinde (§2.6a).
- [x] **P1b-5** `geodesy.traverse` — `POLİGON`, `POLIGON`, `TRAVERSE`, `PLG`
  (`src/domain/geodesy/src/traverse_command.cpp`): bilinen başlangıç/bitiş, kırılma açısı + kenar →
  koordinatlar; açı ve kenar kapanma hataları; `dagitim=esit|kenar`; `Context::report` yapılandırılmış
  rapor; noktalar numaralı `NOKTA`, katman `POLİGON`.
- [~] **P1b-6** `/data/catalogs/geodesy/poligon-toleranslari.json` — BÖHHBÜY kaynaklı, data.md başlık
  bloğu, şema (`schema/poligon-toleranslari.schema.json`), `data/LICENCES.md` izin satırı; aşan kapanma
  `Error` + yönetmelik adı (domain.md R23). **HARİTA MÜHENDİSİ ONAYI HÂLÂ BEKLİYOR (6.11).** Paketin
  `kapsam.onay` alanı `BEKLİYOR` ve komut HER RETİNDE bunu yazıyor; rapor da `onay` alanında taşıyor.
  Kod, şema, izin satırı, kapı ve testler tamam — eksik olan yalnız bir harita mühendisinin BÖHHBÜY'ün
  poligon bölümünden değerleri teyit etmesi. O onay gelene kadar paket bir üretim işinin kabulü için
  kullanılmaz ve bu, imzalanmamış bir sayının kural gibi görünmesini engelliyor.
- [x] **P1b-7** Ortak: `Category::Draw`, `AiAccessible`, tek işlem tek undo; `Param::choice` listeleri
  (`yontem`, `yon`, `sinif`, `dagitim`) bus'ta doğrulanıyor.
- [x] **P1b-8** Docs: beş sayfa (`perp_offset`, `survey_polar`, `intersect_point`, `point_along`,
  `traverse`) + `docs/README.md` satırları + `make reference`.
- [x] **P1b-9** Test: eşitlik kanıtı × 2 (DİKAYAK, ALIM) + günlük replay × 2; `POLİGON` için
  `/tests/golden/senaryolar/poligon.txt` — kare güzergâh ve kenar orantılı dağıtım, bit-özdeş;
  tolerans kataloğu `ci-gate-catalogs.sh` ve `ci-gate-data-permits.sh`'ten geçiyor; "kapanma aşıldı"
  hatasının yönetmelik adını, oranı ve onay uyarısını içerdiği doğrulanıyor.
- [x] **P1b-10 (plan dışı, yolda çıktı)** Çapraz-kayıt ad çakışması kapısı
  (`test_geodesy.cpp`, "kayıt: bütün kayıtlar birlikte"). `test_command.cpp`'nin tripwire'ı yalnız
  `/src/command` kaydını sayar — bir domain modülünü göremez (Article 3.2) — ve `geodesy.traverse`
  `POLİGON` `ALAN`'ın eşadıyken kaydolamıyordu. Başarısız kayıt yalnız log yazıp devam ettiği için
  program traverse olmadan, temiz derlemeyle ve yeşil suite ile başlıyordu. Yeni kapı eski koda karşı
  denendi ve onda düşüyor. `POLİGON` `ALAN`'dan alındı: Türkçe haritacılıkta *poligon* güzergâhtır,
  `ALAN`'ın çizdiği şekil *çokgen*'dir.
- [x] **P1b-11 (plan dışı, yolda çıktı)** Günlükte kesirli sayı kaybı. `Value::from_json` her sayısal
  diziyi kimlik listesi okuyup **kırpıyordu**: `"kenar":[42.315, 56.720]` günlükten 42 ve 56 olarak
  dönüyor, yani replay başka bir poligon çiziyordu (Article 1.4). İçinde kesir olan dizi artık
  `NumberList`; tam sayı dizisi eskisi gibi kimlik listesi kalıyor (altın fikstür ona bağlı).
- [ ] **P1b-7** Ortak: `Category::Draw`, `AiAccessible`, tek işlem tek undo; `Param::choice` listeleri bus'ta
  (R27).
- [ ] **P1b-8** Docs: beş sayfa + `docs/README.md` "Çizim" satırları; `make reference`.
- [ ] **P1b-9** Test: eşitlik kanıtı × 5; `POLİGON` `/tests/golden` — ders kitabı poligonu, üç platformda
  bit-özdeş (6.5); tolerans kataloğu `ci-gate-catalogs.sh`; "kapanma aşıldı" hatasında madde adı.

## P2 — Klasik çizim inşa yöntemleri

- [x] **P2-1** `DAİRE yontem=merkez|2n|3n|ttr` — `core::circumcircle` çekirdeğe eklendi ve `YAY
  yontem=3n` ile paylaşılıyor. `ttr` iki DOĞRU + yarıçap + gösterilen köşe: dört çözüm hesaplanıyor
  (her iki doğrunun iki yana ofseti) ve en yakını alınıyor — sessiz seçim yok, çünkü sessiz bir seçim
  pahı kavşağın yanlış köşesine koyar. Üç doğrusal nokta reddediliyor.
  **Not:** plan "iki nesne + yarıçap" diyordu; uygulama iki DOĞRU alıyor (dört nokta). Nesneden teğet,
  nesnenin türüne göre teğet çözümü gerektirir ve P3'ün `PATLAT`/nesne sorgusu altyapısıyla gelir.
- [x] **P2-2** `YAY yontem=merkez|3n|bma|bby`. Süpürmenin işareti oturumun kuralından: semt'te artı
  süpürme SAAT YÖNÜNDE, ve yay modelde saat yönünün TERSİNE saklandığı için uçlar ona göre takas
  ediliyor — yanlışı biraz farklı bir yay değil, çemberin öbür üç çeyreği olurdu. Süpürme bir tura
  KATLANMIYOR: −100 grad, 300 grad değildir. `bby`'nin iki çözümü `yon=sol|sag` ile.
  **`devam` yapılmadı:** `Session`'a son segment yönünü eklemek gerekiyor ve bu oturum durumu
  değişikliği; P2-5 ile birlikte kendi commit'ini hak ediyor.
- [x] **P2-3** `core.polygon_regular` — `ÇOKGEN`, `COKGEN`, `POLYGONREG`, `ÇKG`, `CKG`: `merkez`,
  `kenar_sayisi` (3–1024, bus doğruluyor), `yontem=ic|dis|kenar`, `yaricap` | `kenar_uzunlugu`, `aci`.
  Yeni sayfa. Altıgenin kenar=yarıçap kimliği testte: iki yöntem bayt bayt aynı çizimi veriyor.
- [x] **P2-4** `DİKDÖRTGEN yontem=3n` — bir kenarın iki köşesi ve karşı kenarın geçtiği nokta.
  Üçüncü nokta bir köşe DEĞİL, yalnız yüksekliği veriyor: kenarın normaline izdüşürülüyor, yani eli
  birkaç milimetre kayan kullanıcı paralelkenar değil dikdörtgen alıyor. (`aci=` ayrıca gerekmedi:
  `3n` döndürmenin kendisidir ve bir açıdan daha okunaklıdır.)
- [ ] **P2-5** `KILAVUZ yon=<açı>`, `nokta=`, `tur=isin` — **ERTELENDİ, sebebi yazılı.** Bugünkü
  `core::Guide` yalnız {eksen, koordinat} taşıyor ve proje dosyasında iki paralel dizi olarak
  saklanıyor (`project_writer.cpp`). Açı ve geçtiği nokta eklemek BELGE MODELİ değişikliğidir
  (CLAUDE.md 0.2a: "bir veri göçüdür, refactor değil"): `model.md`, `io/format.hpp` sürüm artışı, eski
  biçimi okuyan bir okuyucu ve gidiş-dönüş testi gerektirir. Komut düzeyinde yarım yapmak, kaydedilip
  açılınca kaybolan bir kılavuz demek olurdu. Kendi commit'ini hak ediyor.
- [x] **P2-6** `ELİPS yontem=merkez|eksen` — ve `eksen` kozmetik bir ad değil: eksenin İKİ UCU
  (AutoCAD'in varsayılanı), merkez ikisinin ortası. Aynı elips iki şekilde yazılıyor ve ikisi bayt
  bayt aynı çizimi veriyor (testte).
- [x] **P2-7** Docs: `ÇOKGEN` yeni sayfa; `DAİRE`, `YAY`, `DİKDÖRTGEN`, `ELİPS` sayfalarına "yöntemler"
  bölümü ve yeni sözdizimi; `make reference`. Her yöntem için sayısal test (bilinen çember, çeyrek
  çember, kare, altıgen, 3-4-5 kenar); arayüz: **Çizim > Düzgün Çokgen** ve Dikdörtgen ailesi.

## P3 — Düzenleme fiilleri (tamamlandı)

- [x] `core.break` — **KIR**, `BREAK`, `KR`: iki nokta arasındaki parçayı çıkarır; tek nokta
      boşluksuz böler (AutoCAD'in *break at point*'i). Tıklama sırası önemsiz. `BÖL`'den farkı
      sayfada: BÖL parça atmaz.
- [x] `core.join` — **UÇUCA**, `UCUCA`, `JOIN`, `UÇE`: uçları değen çizgileri tek çizgiye ekler,
      gerekeni çevirir, her iki uçtan zincirler. `tolerans=` metre cinsinden bir PARAMETRE
      (varsayılan 1 mm) ve seçim toleransı değil: ne kadar yakının "değmiş" sayıldığı ölçünün
      özelliğidir, farenin değil. Zincire değmeyen çizgi olduğu gibi bırakılıyor. `BİRLEŞTİR` ile
      yan yana anlatıldı — adı karışan iki komut ikisini de kullanılmaz kılar.
- [x] `core.lengthen` — **UZUNLUK**, `LENGTHEN`, `UZN`: `delta=` | `yuzde=` | `toplam=`, tam olarak
      biri (ret cümlesi şimdiki uzunluğu söyler), `uc=son|bas`. Çok köşeli çizgide yalnız son parça
      değişiyor: ölçülmüş köşeler yerinde kalıyor.
- [x] `core.explode` — **PATLAT**, `EXPLODE`, `PTL`: çizgi→kenarlar, alan→sınırı (kapanış kenarı
      dâhil), blok referansı→bileşenleri, dizinin her kopyası için. Tanımında daire/yay/yazı olan
      blok ADIYLA REDDEDİLİYOR: bu türler yük taşır ve aynalı ya da eşit olmayan ölçekte artık
      daire/yay değildir; çizilmiş dış çizgisini koymak bir daireyi alanı πr² olmayan bir 128-gen'e
      çevirir ve bu tapuya giden sayıdır. Faz 2'nin `BLOKDÜZENLE`'si gerçek cevaptır.
- [x] `core.align` — **HİZALA**, `HIZALA`, `ALIGN`, `HZL`: bir çift taşır, iki çift döndürür,
      `olcekle=evet` uzunluk oranıyla ölçekler. Dönüş `sin_cos_udeg`'den, libm'den değil.
      `OTURT` ile farkı iki sayfada: OTURT çok noktalı en küçük kareler Helmert'idir.
- [x] `core.divide` — **BÖLÜMLE**, `BOLUMLE`, `DIVIDE`, `BLM`: `sayi=k` (k−1 nokta) ya da
      `aralik=` (kilometraj), tam olarak biri. Aralık bir KÖŞEYİ GEÇEBİLİR: istasyon run boyunca
      okunuyor, tek kenar boyunca değil. `ARANOKTA` iki noktayı böler, bu bir NESNEYİ böler.
- [x] `core.pedit` — **ÇİZGİDÜZENLE**, `CIZGIDUZENLE`, `PEDIT`, `ÇZD`, `CZD`:
      `islem=kapat|ac|ters|sadelestir`. Sadeleştirme dik uzaklığa bakıyor ve UÇLARI hiç atmıyor.
- [x] Hepsi `Category::Modify`, `AiAccessible`, tek işlem tek undo; kilitli katman reddi; yedi sayfa
      + `docs/README.md` satırları + `make reference`; arayüzde **Değiştir** menüsünde yedi giriş
      (§2.6a); yirmi iki sayısal test.
- [ ] **ESNET (`core.stretch`) yapılmadı ve sebebi:** KESEN pencere içindeki köşelerin taşınması,
      bir pencerenin İÇİNDEKİ köşeleri seçmeyi gerektiriyor — bugünkü seçim nesne düzeyinde çalışıyor,
      köşe düzeyinde değil. P4'ün çokgen/çit seçim kipleri bu altyapıyı getiriyor; ESNET onun üstüne
      oturur ve o pakette yapılacak.
- [ ] **İŞARETLE (`core.measure_along`) yapılmadı:** `BÖLÜMLE aralik=` tam olarak onun işini yapıyor.
      İkinci bir ad ikinci bir komut demek olurdu ve `blok=` ile blok yerleştirme (planın ayırt edici
      maddesi) P6'nın pano altyapısıyla birlikte gelecek.

## P4 — Yakalama ve seçim (büyük kısmı tamamlandı)

- [x] **Yakalama bitleri: ÇEYREK ve TEĞET.** `SnapQuadrant` (bit 18) bir eğrinin eksenleri kestiği
      dört noktayı verir — 0, 100, 200, 300 grad — ve **yapıca tamdır**: merkez ± yarıçap, trigonometri
      yok. Bir yayda yalnız SÜPÜRÜLEN çeyrekler sunulur. Öncelikte UÇ ile aynı sırada: bir eğride
      çeyrek, bir çizgide köşenin olduğu kadar belirtilmiş bir noktadır.
      `SnapTangent` (bit 19) son noktadan eğriye teğetin ayağını verir; **iki** ayak sunulur ve
      imlece yakın olan kazanır. `SnapConstructedMask`'e KONMADI ve sebebi kodda yazılı: teğet ayağı
      eğrinin ÜZERİNDEDİR, uzantı gibi geometrinin dışında değil — oraya koymak `reach` ayarlamamış
      her çağırıcı için modu kapatıyordu. Öncelikte gerçek olan her şeyin altında.
      F3 penceresi motor listesinden üretildiği için ikisi kendiliğinden orada (5.10).
- [x] **Seçim kipleri: ÇOKGEN, ÇOKGENKESEN, ÇİT, ÖNCEKİ, SON.** Çekirdekte
      `core::pick_in_polygon` ve `core::pick_along_fence`; çokgenin kendi kutusu cull, çokgen testi
      yalnız ondan geçene koşuyor (§10.1). Köşe sayısı sınırsız: arayüzde sağ tık/Esc bitirir, komut
      satırında `noktalar=` tekrarlanır. `ÖNCEKİ` bir adım derin ve `Bus::previous_selection`'da;
      silinmiş bir nesneyi GERİ GETİRMİYOR — `slot_of` silinmiş bir anahtarı hâlâ çözer (geri alma
      onun sayesinde çalışır), bu yüzden canlılık da soruluyor. `ÇİT` tek bir noktayı almıyor: bir
      hattın bir röperden geçmesi bir elin çizebileceği bir şey değil.
- [x] **Docs:** `mode.md`'ye iki bit satırı ve ikisinin gerekçesi; `select.md`'ye beş kip tablosu ve
      incelikleri. `make reference`.
- [x] **Test:** `test_snap.cpp`'ye dört vaka (dört çeyrek + idempotens, yayda süpürülmeyen çeyrek
      reddi, 3-4-5 üçgeniyle teğet ayağı, teğetin gerçek bir köşeyi almaması); `test_command.cpp`'ye
      üç vaka (çokgenin kutunun alamayacağını alması, çitin kestiğini alması, ÖNCEKİ/SON).
      Bit sayısı tripwire'ı 15 → 17.
- [ ] **Geçici izleme (OTRACK) yapılmadı ve sebebi:** yazılı karşılığı `xy(P,Q)` olarak P1a'da
      geldi; fare hâli ise ÜÇ parça istiyor — oturumda geçici bir işaretli nokta listesi,
      nokta isteminde onu işaretleyecek bir jest (şeffaf bir sözcük ya da `Shift+sağ tık`), ve
      `SnapQuery`'ye o listeyi okuyan bir alan. Yakalama motoruna bir alan eklemek ve tuvale geçici
      bir durum koymak, bu paketin geri kalanı gibi tek dosyalık bir iş değil; kendi commit'ini
      hak ediyor ve `xy(P,Q)` bugün aynı noktayı yazarak veriyor.

## P5 — Ölçülendirme

- [ ] **P5-1** `ÖLÇÜ tur=koordinat`, `tur=yay`.
- [ ] **P5-2** `/data/catalogs/olcu-stilleri.json` — BÖHHBÜY/MPYY metin yükseklikleri, ok/çentik, birim;
  `ÖLÇÜ stil=` bunu okur (5.13).
- [ ] **P5-3** Golden: bilinen iki nokta → ölçü metni.

## P6 — Pano: KES / PANOYAKOPYALA / YAPIŞTIR

- [ ] **P6-1** Yük: seçili varlıkların yerel JSON'u (`io` yazıcısıyla aynı şema), MIME
  `application/x-kentoscad+json`; `QClipboard` `/src/app`'te; `Bus::on_clipboard_request` seam'i
  (`on_file_request` kalıbı).
- [ ] **P6-2** `core.copy_clip` (`PANOYAKOPYALA`), `core.cut` (`KES` = kopyala + `SİL`, tek undo),
  `core.paste` (`YAPIŞTIR nokta=` taban; `dosya=` aynı JSON dosyadan — betik yolu, Article 1.2).
- [ ] **P6-3** `main_window.cpp` yer tutucuları gerçek eyleme (`actOpen_` kalıbı).
- [ ] **P6-4** Test: kopyala→yapıştır aynı geometri, yeni `EntityId`; eksik katman kararı docs'ta.

## P7 — Sorgu ve araç çubuğu artıkları

- [ ] **P7-1** Araç çubuğu **Sorgula** → `core.query`.
- [ ] **P7-2** `core.entity_info` — `NESNEBİLGİ`, `OBJINFO`, `NB`: tür, katman, köşe, uzunluk, alan, `fid`;
  `Context::report` (R26); AI okuma aracı.
- [ ] **P7-3** `core.measure_angle` — `AÇIÖLÇ`: iki doğru ya da üç nokta; P0 kuralıyla yazar.

---

## Doğrulama (her pakette)

- **Birim:** `test_command.cpp` (gramer, fonksiyonlar — bilinen üçgenler `Mm`'de), `test_snap.cpp`
  (`snap(snap(p)) == snap(p)`), `test_geodesy.cpp` (poligon, kapanma dağıtımı).
- **Eşitlik kanıtı:** `test_proof.cpp` — GUI (`InputSource` fare) = komut satırı = JSON betik → özdeş
  `Document`, bayt-özdeş günlük; replay altın belgeyi verir (6.4, test.md R3).
- **Golden:** `POLİGON`, `3n` daire, ölçü metni — üç platformda bit-özdeş (6.5).
- **Fuzz:** parser değişti → `tests/fuzz` harness ve tohum korpusu (6.7, R9).
- **Kapılar:** `ci-gate-docs.sh`, `ci-gate-catalogs.sh`, `ci-gate-hardcoded-thresholds.sh`,
  `ci-gate-i18n.sh`, `ci-gate-comments.sh`.
- **Elle (release listesi):** dik ayak fareyle ve `DİKAYAK 0,0 100,0 30 -5` yazarak aynı nokta;
  `@100<45` semt'te kuzeydoğuya 40.5°; F3'te ÇEYREK/TEĞET tutar; `ÇİT` ile seçim; KES→YAPIŞTIR.

## Bilinmesi gerekenler

- Açı kuralı değişimi **canlı metni** etkiler; günlükler değil. Dokümanın her `@d<a` örneği güncellenir;
  betik yazarlarına `MOD kural matematik` ve `g/d/r` sonekleri belgelenir.
- `ttr` ve iki-mesafe kesişimi **çok çözümlü**: seçim kuralı belgelenir ve testlenir; sessiz seçim yok.
- Poligon toleransları mevzuat verisidir: `package_version`, `source` (BÖHHBÜY madde), `published`; harita
  mühendisi onayıyla girer (6.11).
- `BİRLEŞTİR` boolean birleşimdir; `UÇUCA` gelince ikisi docs'ta yan yana anlatılır.
