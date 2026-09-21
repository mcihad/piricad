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

- [ ] **P2-1** `DAİRE yontem=merkez|2n|3n|ttr` — `3n` için `pick.hpp`'ye `circumcircle`; `ttr` iki nesne +
  yarıçap, en yakın teğet çifti; teğetlik `|d − r| < 1 Mm` testi.
- [ ] **P2-2** `YAY yontem=merkez|3n|bma|bby|devam` — `devam` için `Session`'a son segment yönü.
- [ ] **P2-3** `core.polygon_regular` — `ÇOKGEN`, `COKGEN`, `POLYGONREG`, `ÇKG`: `merkez`, `kenar_sayisi`
  (3–1024), `yontem=ic|dis|kenar`, `yaricap` | `kenar_uzunlugu`, `aci`. Yeni sayfa.
- [ ] **P2-4** `DİKDÖRTGEN aci=`, `yontem=3n`.
- [ ] **P2-5** `KILAVUZ yon=<açı>`, `nokta=`, `tur=isin`.
- [ ] **P2-6** `ELİPS yontem=merkez|eksen` (mevcut argümanlar aynı kalır).
- [ ] **P2-7** Docs güncel (sözdizimi + örnekler), eşitlik kanıtı her yöntem için.

## P3 — Düzenleme fiilleri

- [ ] **P3-1** `core.break` — `KIR`, `BREAK`, `KR`: bir noktada ya da iki nokta arası; parça silinir.
- [ ] **P3-2** `core.join` — `UÇUCA`, `UCUCA`, `JOIN`, `UÇ`: uç uca değenler tek çokluçizgi; `tolerans=` Mm
  (varsayılan 1); `BİRLEŞTİR`(boolean) ile farkı docs'ta yan yana.
- [ ] **P3-3** `core.stretch` — `ESNET`, `STRETCH`, `ES`: KESEN pencere içindeki köşeler taşınır.
- [ ] **P3-4** `core.lengthen` — `UZUNLUK`, `LENGTHEN`, `UZN`: `delta=|yuzde=|toplam=|dinamik`.
- [ ] **P3-5** `core.explode` — `PATLAT`, `EXPLODE`, `PT`: blok → bileşen, çokluçizgi/alan → çizgiler; tek undo.
- [ ] **P3-6** `core.align` — `HİZALA`, `HIZALA`, `ALIGN`, `HZ`: 1–2 nokta çifti, `olcekle=evet`; `OTURT`
  (Helmert) ile farkı docs'ta.
- [ ] **P3-7** `core.divide` / `core.measure_along` — `BÖLÜMLE`/`İŞARETLE`: eşit parça / sabit aralık;
  `nokta` ya da `blok=`.
- [ ] **P3-8** `core.pedit` — `ÇİZGİDÜZENLE`, `PEDIT`, `ÇD`: `islem=kapat|ac|ters|kalinlik|sadelestir`;
  sadeleştirme Clipper2 `SimplifyPath`.
- [ ] **P3-9** Kilitli katman reddi `TAŞI` kalıbıyla; eşitlik + iptal + `Value` testleri.

## P4 — Yakalama ve seçim

- [ ] **P4-1** `SnapQuadrant` (ÇEYREK: 0/100/200/300 grad) ve `SnapTangent` (TEĞET) bitleri; `test_snap.cpp`
  idempotens; `MOD`, F3 menüsü, ipucu tek listeden (5.10).
- [ ] **P4-2** Geçici izleme (OTRACK): `SnapQuery::tracking` 1–2 nokta; istemde `İZ` sözcüğü (transparent,
  R18) ve `Shift+sağ tık`; `xy(P,Q)` fonksiyonunun fare hâli.
- [ ] **P4-3** Seçim: `ÇİT`, `ÇOKGENPENCERE`/`ÇOKGENKESEN`, `ÖNCEKİ` (Session saklar), `SON`; `pick.hpp`
  `pick_in_polygon`/`pick_along_fence`.
- [ ] **P4-4** Docs: `arayuz.md` "Harita alanı" ve yakalama; `secim` sayfası.

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
