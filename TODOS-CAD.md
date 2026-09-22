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
- [x] **P1b-7** Ortak: beşi de `Category::Draw`, `AiAccessible`, tek işlem tek undo; `yontem`, `yon`
  ve `dagitim` sözcük listeleri `Param::choice` ile bus'ta doğrulanıyor (R27).
- [x] **P1b-8** Docs: beş sayfa (`perp_offset`, `survey_polar`, `intersect_point`, `point_along`,
  `traverse`) + `docs/README.md` satırları + `make reference`.
- [x] **P1b-9** Test: **eşitlik kanıtı × 5 tamam** — `DİKAYAK` ve `ALIM` P1b'de yazıldı,
  `KESİŞİMNOKTA`, `ARANOKTA` ve `POLİGON` bu turda. `POLİGON` için ayrıca günlük replay'i, kesirli
  kenarlarla (42,315 m ve 56,72 m — günlükten 42 ve 56 olarak dönenler). Altın fikstür
  `tests/golden/senaryolar/poligon.txt`, tolerans kataloğu `ci-gate-catalogs.sh`'ten geçiyor,
  "kapanma aşıldı" reddi mevzuatı adıyla anıyor (`test_geodesy.cpp:818`).
- [x] **P1b-10 (plan dışı, kanıt yazılırken çıktı)** `POLİGON` **okumaları sormuyordu.** Açı ve
  kenar yalnız argümandan okunuyordu ve gerekçe "bir poligon ölçü karnesinden aktarılır, tıklanmaz"
  diye yazılıydı. Bu, sayıların NEREDEN geldiği için doğru, nasıl İÇERİ GİRDİĞİ için yanlış: araç
  kolonundan `POLİGON`'a basan kullanıcı iki bilinen noktayı veriyor, sonra "aci= ve kenar= gerekir"
  cevabını alıyordu — fareyle ulaşılabilen ama fareyle bitirilemeyen bir komut (5.15). Artık
  `ALIM`'ın kalıbıyla istasyon istasyon soruyor; argüman verildiyse hiç sorulmuyor, çünkü kararı
  veren şey argümanın boşluğu, `InputSource` değil (command.md P10).
- [x] **Günlükte anahtar SIRASI artık BİLDİRİLEN sıraya göre** — kendi değişikliğiyle yapıldı.
  `Args` ekleme sırasını tutuyor ve `to_json` onu basıyordu: `ÇOKGEN yontem=ic 0,0 6` ile
  `ÇOKGEN 0,0 6 yontem=ic` tek çağrının iki bayt dizisiydi. Üç istemciyi bir YAZIM sırasında
  anlaştırmak mümkün değil — `KILAVUZ yon=45g` ile başlayıp noktayı sonra soran bir araç gerçekten
  `yon`'u önce bağlıyor, yazılan satır konumsal noktayı öne koyuyor, betik JSON'un kendi sırasını
  veriyor. Bildirilen sıra, her istemcinin paylaştığı tek şey; kayıt onunla yazılıyor
  (`Args::reorder_like`, `Bus::journal_entry`). Dört altın fikstür yenilendi ve **değişimin yalnız
  anahtar sırası olduğu doğrulandı**: her satırın JSON'u eskisine eşit, hiçbir değer kıpırdamadı.
  Bir kanıt vakası bağı kuruyor ve beklenen sırayı `Registry`'den okuyor, elle yazmıyor.

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
  **`devam` yapıldı** ve erteleme sebebi yanlış çıktı: `Session`'a son segment yönü EKLEMEK
  gerekmedi. `SEÇ SON` zaten "en son oluşturulan nesne"yi belgeden okuyor, ve doğrultuyu aynı
  yerden okumak aynı soruyu aynı yere sormaktır — geri alma, replay ve yeniden yükleme ile ayrı
  tutulacak bir durum parçası daha olmuyor. Bir çizginin son kenarından ya da bir yayın ucundan
  (uç yarıçapına dik, modelin saat yönü tersi sıralamasıyla) teğet devam ediyor; merkez,
  başlangıçtaki dikle kirişin orta dikmesinin kesişimi. **Bitiş teğetin üzerindeyse** oradan devam
  eden şey bir yay değil bir doğrudur ve komut bunu söylüyor, sıfıra bölmüyor. Günlüğe **çözülmüş
  yay** yazılıyor (merkez/başlangıç/bitiş), yani replay BU yayı kuruyor — oynatıldığı belgede en
  yeni nesne ne olursa olsun (model.md P4). Dört test, altın fikstürde bir satır, `arc_draw.md`'de
  kendi bölümü.
- [x] **P2-3** `core.polygon_regular` — `ÇOKGEN`, `COKGEN`, `POLYGONREG`, `ÇKG`, `CKG`: `merkez`,
  `kenar_sayisi` (3–1024, bus doğruluyor), `yontem=ic|dis|kenar`, `yaricap` | `kenar_uzunlugu`, `aci`.
  Yeni sayfa. Altıgenin kenar=yarıçap kimliği testte: iki yöntem bayt bayt aynı çizimi veriyor.
- [x] **P2-4** `DİKDÖRTGEN yontem=3n` — bir kenarın iki köşesi ve karşı kenarın geçtiği nokta.
  Üçüncü nokta bir köşe DEĞİL, yalnız yüksekliği veriyor: kenarın normaline izdüşürülüyor, yani eli
  birkaç milimetre kayan kullanıcı paralelkenar değil dikdörtgen alıyor. (`aci=` ayrıca gerekmedi:
  `3n` döndürmenin kendisidir ve bir açıdan daha okunaklıdır.)
- [x] **P2-5** `KILAVUZ yon=<açı>`, `nokta=`, `tur=isin` — **ertelemenin gerektirdiği göç yapıldı**
  (CLAUDE.md 0.2a: veri göçü, refactor değil). Kendi commit'i:
  * `core::GuideStore` altı sütuna çıktı (eksen, koordinat, açı, geçtiği noktanın iki koordinatı,
    ışın); `core::GuideRow` dikişlerde değer olarak taşıyor. Model **alan kazandı**, anlam
    değiştirmedi — 0.2a'nın izin verdiği tek değişim biçimi.
  * Saklanan açı **matematik mikro-derece** (doğudan saat yönünün tersine, `atan2_udeg`'in birimi):
    saklanan sayı ile yakalama testi arasında dönüşüm yok. Kullanıcının yazdığı ve listenin
    yazdığı açı kenarda bir kez çevriliyor (`core::math_udeg_from_angle`, `core::direction_turns`).
  * Dosyada dört **isteğe bağlı** blok (`0x0092`–`0x0095`), **yalnız açılı kılavuz varsa** yazılıyor:
    yalnız cetvel kılavuzu taşıyan bir çizim eskisiyle bayt-özdeş. Açılı kılavuz taşıyan dosya
    `min_reader_version`'ı yükseltiyor (`io::kMinReaderVersionAngledGuide`), çünkü `kBlkGuideAxis`
    eski bir blok ve `2` değeri yeni — eski bir okuyucu o sütunu bozuk sayardı, yani doğru bir ret
    yanıltıcı bir gerekçeyle. İkisi de testli.
  * Yakalama: açılı kılavuza dik ayak; **ışının gerisinde noktanın kendisi**; iki cetvel kılavuzunun
    kesişimi açılıyı yener, tek cetvel kılavuzu yenmez (mesafeye bakılır). Dört snap testi.
  * Tuvalde çizim **ekran uzayında uzatılıyor**, dünyada değil: on bin kilometreyi dünyada koşup
    görünüme çevirmek hiçbir float'ın işine yaramayan bir ekran koordinatı veriyor ve kalınlaştırma
    hiç çizmiyordu — ilk denemede 45° kılavuz karede yoktu.
  * `model.md` R47–R47d eklendi; `docs/komutlar/guide.md` açı kuralı, doğru/ışın ve dosya biçimi
    bölümleriyle; **Çizim** menüsünde `Açılı Kılavuz` satırı; eşitlik kanıtı + günlük replay + birim
    değişse aynı yeri gösterme testi.
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
- [x] **ESNET (`core.stretch`)** — erteleme koşulu karşılandı, ama sebebini yeniden okuyarak:
      **pencere zaten süzgecin kendisidir.** İçindeki köşe gider, dışındaki kalır, hiçbir köşesi
      içinde olmayan nesneye hiç dokunulmaz. Bunun için hiçbir şeyin *seçilebilir* olması gerekmiyor,
      yani köşe-düzeyi seçim modeli kurmadan oldu. `pencere` (iki nokta) + `baslangic`/`bitis`
      öteleme + isteğe bağlı `nesneler`. Çokluçizginin pencereye giren bütün köşeleri **tek yazımda**
      gidiyor (yarısı taşınmış halka hiç denetleyiciye sunulmuyor); diğer her tür `core::move_grip`
      tablosundan geçiyor. **Her hedef anlık görüntüden hesaplanıyor**: dairenin merkezi zaten
      yarıçap kolunu taşıdığı için canlı geometriye bakan bir ikinci taşıma daireyi öteleme kadar
      büyütürdü — pencereye tamamen giren daire ötelenir, yalnız yarıçap kolu girerse boyutlanır
      (ikisi de testli). Günlüğe **çözülmüş kimlikler** yazılıyor, pencerenin şansı değil (model.md P4).
- [x] **KİLİTLİ KATMAN KUSURU (plan dışı, ESNET yazılırken çıktı).** Kilit yalnız her `add_*`
      üzerinde denetleniyordu ve başka hiçbir yerde: kilitli bir katman kullanıcının oraya **yeni**
      parsel çizmesini engelliyor, üzerinde **duran** her parseli TAŞI, KÖŞETAŞI, ESNET, DÖNDÜR,
      ÖLÇEKLE, PATLAT ve SİL ile yeniden şekillendirmeye ya da silmeye izin veriyordu. Kadastro
      çiziminde bu tam tersi: kilit, sayfadaki şey için vardır. Denetim `Document::editable()`'a
      girdi — her yerinde düzenlemenin zaten sorduğu tek soru — ve silme için
      `Transaction::erase_entity`'ye, çünkü `set_entity_alive` aynı zamanda silmenin **geri alma**
      yoludur ve oraya konan bir kilit denetimi, kullanıcı katmanı kilitlediği anda daha önceki
      silmeyi geri alma yığınında hapsederdi. `restore_geometry` ve öbür ters yollar bilerek
      denetimsiz: kilitlemek geçmişi dondurmaz (iki test bunu da çiviliyor).
- [x] **İŞARETLE ayrı bir komut olarak yazılmadı, ama ayırt edici maddesi geldi:** `BÖLÜMLE aralik=`
      zaten sabit aralıkla işaretliyordu; eksik olan `blok=` idi ve şimdi var. **Bir güzergâh boyunca
      direk, rögar, ağaç ya da bordür işareti dizmek** bu parametrenin bütün varlık sebebi — her
      istasyona tek tek `BLOKEKLE` yazmak aynı işi elle yapmaktır. `hizala=evet` bloğu üzerinde
      durduğu kenarın doğrultusuna çeviriyor (`atan2_udeg`, libm değil); varsayılan kapalı, çünkü
      bir rögar kapağı dönmek istemez. Blok **önceden tanımlı** olmalı: burada yeni tanım üretmek
      `BLOK`'un işini ikinci bir yerde yapmak olurdu (5.10). İkinci bir AD hâlâ yok ve olmayacak —
      aynı işin ikinci komutu, ikisini de kullanılmaz kılar.

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
- [x] **Geçici izleme (OTRACK)** — üç parçası da geldi, kendi commit'iyle:
      * **Oturumda işaretli nokta listesi**: `Bus::tracking_marks()` / `mark_tracking` /
        `clear_tracking`. En çok iki işaret; üçüncü en eskisinin yerini alıyor (üç işaret üçüncü bir
        eksen değil, yeni bir çift), aynı nokta iki kez işaretlenmiyor. Oturum durumu, belge değil
        (model.md R43): hash'lenmiyor, günlüğe düşmüyor, geri alınmıyor.
      * **Jest**: şeffaf `İZ` komutu (`core.tracking`; `IZ`, `TRACK`, `TRK`) ve tuvalde
        **Shift + sağ tık**. Jest komuttan geçiyor, doğrudan motora değil — fareyle konan işaret ile
        yazılan işaret tek şey olmalı (Article 1.1). İşaretlenen nokta **yakalanmış** noktadır,
        piksel değil.
      * **Motor alanı**: `SnapQuery::tracking` (span) + `tracking_reach`, yeni bit
        `core::SnapTracking`. **Kesişim tek izi yener** — iki işaret koyan kullanıcı onların
        belirlediği noktayı hedefler; tek iz her zaman bir eksende daha yakındır, yani yalnız
        mesafeye göre yarıştırılsa kesişim hiç yakalanamazdı (aynı sıralama `SnapGuide`'ın).
        Gerçek olan her şeyin altında.
      * Tuvalde izler ve işaret kareleri çiziliyor (`warn` mürekkebi, design.md §1.2 — aksan
        seçim/etkin araç/birincil eylem demek, iz bunların hiçbiri değil).
      * `xy(P,Q)` duruyor ve aynı noktayı veriyor: biri iki noktayı birlikte alıp kesişimi
        hesaplıyor, öbürü tek tek işaretleyip motora hesaplatıyor. Tek kural, iki yol.
- [x] **YAN YOLDA: yakalama maskesi ayarının ARALIĞI iki modu ulaşılmaz kılıyordu.** Aralık
      `0x3FFFF`'te (17. bit) duruyordu, oysa ÇEYREK (1<<18) ve TEĞET (1<<19) P4'te bildirilmişti:
      `MOD yakalama_modları` o değerleri reddediyor, F3 listesi iki modu açamıyordu. Aralık
      `SnapAllMask`'i kapsayacak şekilde genişletildi ve bir test bağı kuruldu — 21. biti ekleyen
      kişi o satırı da genişletmek zorunda.

## P5 — Ölçülendirme (tamamlandı)

- [x] **`ÖLÇÜ tur=koordinat`** (ordinat). **Model bunu baştan beri taşıyordu**:
      `DimensionType::Ordinate` bildirilmiş, kodlanmış, çözülmüş ve ÇİZİLMİŞTİ — ama `dimension_layout`
      ve `dimension_picks` onu `default: return false` ile geçiyordu ve hiçbir sözcük ona ulaşmıyordu.
      Yani bir Türk aplikasyon paftasının ordinat tablosu hiç çizilemiyordu. 5.15'in aynısı: kimsenin
      isteyemediği bir yetenek. Yerleşim, ölçü ve ters dönüşüm bağlandı.
      **Ekseni JEST belirliyor:** yazı yana çekilirse sağa, yukarı çekilirse yukarı değeri okunuyor —
      bir ordinat tablosu tam böyle kurulur ve kullanıcıya fazladan bir cevap maliyeti çıkmıyor.
      Başlangıcın batısındaki nokta EKSİ okuyor; işaret cevabın parçası.
- [x] **`ÖLÇÜ tur=yay`** (yay uzunluğu). `DimensionType::ArcLength` enum'un SONUNA eklendi — bu onu
      toplamalı kılan şey: bundan önce yazılmış bir dosya bu değeri hiç içermez, yani eski bir çizim
      aynen okunur (model.md: bir şekil bir durum KAZANABİLİR, bir durumu DEĞİŞTİREMEZ). r·θ, tek
      yuvarlamayla: 50 m yarıçaplı çeyrek yay boyunca **78,540 m**, kirişi 70,711 m — kirişi basan bir
      pafta yanlış boyda bordür sipariş ettirir.
      DXF'te R2007 öncesi karşılığı yok; dışa aktarımda **açısal** ölçü olarak yazılıyor (aynı üç
      nokta) ve uzunluk xdata'da gidiyor — bu programdan çıkıp geri girince aynen korunuyor, başka bir
      program bir açı görüp onu söylüyor, bir kiriş görüp ona inanmıyor.
- [x] **Ölçü stili kataloğu mekanizması** zaten vardı ve `ÖLÇÜ` onu okuyordu:
      `data/catalogs/dxf/olcu-stili.json` (ISO-25, STANDARD, MİMARİ), `core.olcu.stil_katalogu`
      ayarıyla ve `katalog=` parametresiyle. 5.13 karşılanmış: komutta tek bir yükseklik sabiti yok.
- [~] **Planın istediği BÖHHBÜY/MPYY pafta stili YOK ve uydurulmadı.** Plan "BÖHHBÜY/MPYY pafta
      metin yükseklikleri" diyor; kataloğun `source` alanı ise açıkça "yönetmelik değeri değildir"
      yazıyor — ISO 129-1 ve AutoCAD varsayılanları. Yönetmelikten gelen bir yükseklik uydurmak
      5.13'ün yasakladığı şeyin ta kendisidir, sadece dosyaya taşınmış hâli: resmî görünen bir sayı,
      olmayan bir sayıdan kötüdür. **Kalan:** yönetmelik metni + madde/ek atfı + bir harita
      mühendisinin onayı (Article 6.11) — poligon toleransları (`kapsam.onay: "BEKLİYOR"`) ile aynı
      sınıf ve aynı bekleyiş. Mekanizma hazır: satır eklendiği gün `ÖLÇÜ stil=BÖHHBÜY` çalışır ve
      tek satır C++ değişmez.
- [x] **Altın fikstür:** `tests/golden/senaryolar/olcu-turleri.txt` — yedi ölçü, ve iki `koordinat`
      satırı aynı iki noktadan iki farklı eksen okuyor. Kaydedilen sayılar 40000, 30000, 20000,
      **78540** ve **157080**; π bir çarpma ve bir yuvarlama içerir, yani bu satır farkın üç
      platformda da aynı milimetrede durduğunu söylüyor (§7.3).
- [x] **Docs:** `dimension.md`'ye iki tür satırı, iki bölüm (ordinat jesti ve yay-kiriş farkı, DXF
      notuyla), parametre ve hata satırları. `make reference`.

## P6 — Pano (tamamlandı)

- [x] **Yük yerli biçimin kendisi**, planın dediği gibi "ikinci bir biçim yok" — ama JSON değil,
      çünkü bir belgenin JSON tarifi bu programda YOK ve uydurmak tam olarak planın yasakladığı
      ikinci biçim olurdu. Yük, `KAYDET`'in yazdığı proje dosyasının kendisi: aynı yazıcı yazıyor,
      aynı okuyucu okuyor, ve bu yüzden panoya katman adı, stil, çizgi tipi, öznitelik sütunları ve
      değerleri, blok tanımları ve koordinat sistemi birlikte gidiyor.
- [x] **Alt küme `Transaction::adopt_from`'a bir anahtar FİLTRESİ eklenerek çıkarılıyor.** İkinci bir
      kopyalayıcı yazmak, o kopyalayıcının çizgi tiplerini, resimleri, iç stilleri, katmanları,
      blokları ve öznitelik sütunlarını baştan öğrenmesi ve bir tür bunlardan birini kazandığı gün
      geride kalması demek olurdu (5.10). Filtre, bir ithalatın kullandığı fonksiyonun aynısı:
      doğru biçimde benimsenen bir tür doğru biçimde kopyalanıyor.
- [x] **İki yeni fiil `FileRequest`'e eklendi** (`ClipboardCopy`, `ClipboardPaste`), kendi seam'i
      açılmadı — `New`'in orada olma gerekçesiyle: bu programa bir çizim sokan ya da ondan çıkaran ne
      varsa tek bir seam, yani ters gitmesi için tek bir yer.
- [x] `core.copy_clip` (**PANOYAKOPYALA**, `PANOKOPYALA`, `COPYCLIP`, `PKP`) — çizimde hiçbir şeyi
      değiştirmiyor, geri alma adımı bırakmıyor; `Category::File`, çünkü yaptığı şey bir dosya yazmak.
- [x] `core.cut` (**KES**, `CUT`, `KS`) — kopyalama ve silme TEK işlem, dolayısıyla tek Ctrl+Z ikisini
      geri alıyor; ve geri alma panoyu BOŞALTMIYOR, ki bir kesmenin bütün amacı budur.
- [x] `core.paste` (**YAPIŞTIR**, `YAPISTIR`, `PASTE`, `YP`) — `nokta=` yükün sol alt köşesini oraya
      taşıyor (bir elin yapıştırmaktan anladığı şey), `yerinde=evet` her koordinatı olduğu gibi
      bırakıyor (aynı sistemdeki iki çizim arasında kopyalamanın istediği şey). Tek undo adımı.
- [x] **Pano nerede:** `dosya=` verilmezse kullanıcı başına ortak bir dosya
      (`kentoscad-pano.pcad`), yani bu programın iki penceresi aynı panoyu paylaşıyor ve bir çökme
      yükü kaybetmek yerine yerinde bırakıyor. `dosya=` betiğin ve başsız çalıştırmanın yolu ve
      AYNI yol (Article 1.2).
- [x] **`main_window.cpp` yer tutucuları gerçek oldu:** üçü de komutu çalıştırıyor, `Ctrl+X`/`C`/`V`
      kısayollarıyla ve **Düzen** menüsünde geri al/yinele'nin yanında. `tool-answerable` kapısının
      iddiası TERSİNE çevrildi: o üç satırın artık açıklama kutusu değil KOMUT çalıştırdığını
      denetliyor — bir komut geldikten sonra yerinde kalmış bir yer tutucuyu yakalayan iddia bu.
- [x] Üç sayfa, `docs/README.md` satırları, `make reference`, üç eşitlik/geri-alma testi
      (kopyala→yapıştır aynı içerik hash'i; KES tek adım ve pano dolu kalıyor; boş seçim ve boş pano
      sebebiyle reddediliyor).
- [x] **İŞLETİM SİSTEMİ PANOSU (`QClipboard`) bağlandı.** Kanca `io::FileService` üzerinde iki
      `std::function`: `on_clipboard_written` (yazılan dosyayı app okur ve
      `application/x-kentoscad-project` türüyle sisteme sunar) ve `on_clipboard_wanted` (yapıştırma
      öncesi app, sistemde bizim türümüzden bir yük varsa onu okuyucunun bakacağı yola yazar).
      `/src/io` Qt bağlamamaya devam ediyor (Article 3.2); Qt tarafı `Controller`'da, yani pencere
      katmanında. **Sistemde tutulan yük kazanır**: başka bir pencerede kopyalayan kullanıcı ONU
      bekler, bu sürecin temp dizininde bıraktığı eski yükü değil. Kullanıcı bir dosya adı verdiyse
      panoya dokunulmaz — istemediği bir yan etki olurdu.
      `KENTOS_CLIP_PROBE` + `os-clipboard` ctest'i uçtan uca kanıtlıyor: kopyaladıktan sonra
      **geçici dosya siliniyor**, yani geri gelen yük yalnız sistem panosundan gelebilir.

## P7 — Sorgu ve araç çubuğu artıkları

- [x] **P7-1** Araç çubuğu **Sorgula** → `core.query`. Erişilebilirlik işinde bağlandı; menüdeki
  `Faz 2` ölü satırı kaldırıldı.
- [x] **P7-2** `core.entity_info` — `NESNEBİLGİ`, `NESNEBILGI`, `OBJINFO`, `NB`: tür, katman, halka,
  köşe, çevre, alan, kapsam ve dolu öznitelik hücreleri; `Context::report` (R26); AI okuma aracı.
  Tür adı **tür tablosundan** okunur, komutun içindeki bir `switch`'ten değil (5.10).
- [x] **P7-3** `core.measure_angle` — `AÇIÖLÇ`, `ACIOLC`, `MEASUREANGLE`, `AÇÖ`: tepe ve iki kol
  noktası; süpürme ve tersi birlikte, P0 kuralı ve biriminde. **İki doğru biçimi yazılmadı**: iki
  doğru seçildiği anda üç noktaya indiriyor ve bir doğrunun ucunu imlecin altına koyan şey yakalama
  motorunun kendisi — ikinci bir giriş biçimi aynı hesabın ikinci yolu olurdu.
  Kısaltma `AÖ` **değil**: o `ALANÖLÇ`'ün.

**Yan yolda düzeltilen üç şey** (ikisi Article 1.2 kusuru):

- `Bus::finish` yapılandırılmış cevabı **taşımıyordu** — yalnız `dispatch` taşıyordu. Araç
  kolonundan kollanıp fareyle cevaplanan bir sorgu çağırana `report` boş dönüyordu: yazılan yolun
  sahip olduğu bir yeteneğe işaret edilen yol sahip değildi (5.15), hem de insanın kullandığı
  istemcide. Tek satır, `tests/unit/test_command.cpp` içinde kendi vakasıyla çivilendi.
- **Komut günlüğü paneli en yeni satıra kaymıyordu.** `appendPlainText` belgeye yazıyor,
  görünümü olduğu yerde bırakıyor; gizliyken yüz satır gelen bir sekme öne kırkıncı satırı
  göstererek çıkıyordu — yani sorgu çalışıyor, cevap veriyor ve cevap onu göstermek için yeni
  açılmış panelin dışında kalıyordu. Kuyruğu ancak kuyruk zaten görünürken takip eder: geri
  kaydırıp bir şey okuyan kullanıcının sayfasını program elinden almaz.
- `modifyTool` ile kurulan **okuma-amaçlı araçlar paneli açmıyordu** (`ALANÖLÇ` dâhil).
  Karar artık `Registry`'ye soruluyor (`MainWindow::answersInWords`), her çağrı yerinde tekrar
  edilen bir bayrağa değil — okuma-amaçlı olan yeni bir komut paneli kimse hatırlamadan alır.

---

## Plan metninin ikinci denetimi (2026-09-21)

- [x] **Mekanik tarama: planın adını verdiği 237 tanımlayıcı ağaçta arandı**, 225'i bulundu.
  Bulunmayan 12'nin hepsi hesaplı: `Bus::on_clipboard_request` (yerine mevcut `on_file_request`
  seam'i kullanıldı, sebebi CHANGELOG'da), `Category::Edit` (gerçek ad `Modify`), İŞARETLE'nin dört
  adı (bilerek yazılmadı), üç satır/dosya atfı, ve **üçü gerçek iş çıktı**:
- [x] **`ÇİZGİDÜZENLE islem=sadelestir` ELLE YAZILMIŞTI.** Plan "Sadeleştirme Clipper2
  `SimplifyPath`" diyor ve 5.16 çözülmüş bir problemi yeniden çözmeyi yasaklıyor. Eski hâli her
  köşeyi son TUTULAN köşeden sonrakine giden doğruya göre ölçüyordu — bu bir dik-uzaklık süzgeci ve
  sadeleştirme demek değil: bir köşenin kalıp kalmaması, komşularından hangisinin ondan önce
  kaldığına bağlı oluyor, yani aynı şekil yürüyüşün nereden başladığına göre başka iniyor.
  Sıra-bağımsız olmayan bir sadeleştirme, şeklin bir özelliği değildir. `core::simplify_ring`
  Clipper2'ye devrediyor; dört çekirdek testi (sıra bağımsızlığı testi eski hâli çürütüyor),
  kapalı halkada dikiş, sıfır tolerans ve "her şeyi yiyen tolerans şekli geri verir" hâli.
- [x] **`SEÇ ÇOKGENPENCERE`** eklendi: `ÇOKGEN` aynı zamanda bir ÇİZİM komutu (düzgün çokgen), yani
  birini çizip sonra `SEÇ ÇOKGEN` yazan kullanıcı tek sözcükle iki şey söylüyor. Planın kullandığı
  uzun yazım artık çalışıyor; ikisi de aynı kipe gidiyor ve uzun olan hangisi olduğunu söylüyor.
- [~] **`RubberShape::Offset` yapılmadı ve sebebi:** plan `DİKAYAK` için "taban çizgisi + dik ayak
  izi" lastik bandı istiyordu. Bir lastik bant İMLECİN koyacağı noktayı önizler; `ayak` ve `boy` ise
  `ctx.number` ile SORULUYOR ve bir sayı Enter'a basılana kadar yoktur — önizlenecek bir nokta yok.
  Ayağı tıklayarak vermek ayrı bir özellik olurdu (sayı isteminde nokta almak), ve o plan metninde
  yok. Bugünkü hâl: taban çizgisi lastik bantla veriliyor, sonra sayılar yazılıyor ve komut satırı
  odağı sayı istemlerinde kendiliğinden geliyor (bu oturumda düzeltildi).
- [x] **`POLİGON` istasyonlarını NUMARALIYOR.** Plan bunu dört sözcükle istiyordu ("noktalar
  `NOKTA` olarak, **numaralı**") ve numara hiç yazılmıyordu. Bir poligon istasyonu, ondan sonraki
  her detayın ölçüldüğü yerdir: numarası olmayan bir istasyon, bir mühendisin **atıfta
  bulunamadığı** bir noktadır. Sütun `nokta_no` — `NOKTALAR`'ın okuyup yazdığı ve `n(…)`'nin
  çözdüğü sütunun aynısı, ikinci bir sütun açılmadı (5.10). **Varsayılan, çizimdeki en büyük
  numaranın bir fazlası**: 1'den yeniden başlayan ikinci bir güzergâh iki istasyona tek ad verir ve
  `n(2)` o zaman aramanın önce ulaştığını gösterir. `R12` gibi AD taşıyanlar sayılmaz, sayaç
  aranır. **Çözülmüş `ilk_no` günlüğe yazılıyor** ve varsayılanı güvenli kılan şey bu: başka
  numaralı noktalar taşıyan bir belgeye oynatılan günlük aynı numaraları vermek zorunda
  (Article 1.4). Altın fikstür `ilk_no: 1` ve `ilk_no: 5` yazıyor — ikinci güzergâh dördün ardından
  sürüyor.
- [~] **Gözlem, iş değil:** altın fikstürlerin belge dökümü **öznitelik hücrelerini yazmıyor**.
  `icerik-ozeti` onları katlıyor, yani bit-özdeşlik kanıtlanıyor; ama fikstür kırıldığında diff
  "hangi hücre değişti" demiyor, yalnız "hash farklı" diyor. Köşeler için dökümün kendi yorumu tam
  bu gerekçeyi veriyor. Planın istediği bit-özdeşlik karşılandığı için burada bırakıldı.
- [x] **Planın 2. ilkesini koruyan kapı yazıldı** (`scripts/ci-gate-tek-yol.sh`). İlke şöyleydi:
  "Hiçbir komut `InputSource`'a bakmaz (command.md P10)." Denetimde **tutuyordu** — 71 komut
  gövdesinin hiçbiri hangi istemcinin sorduğuna bakmıyor — ama onu koruyan hiçbir şey yoktu. Bu
  kural bir bildirimle ölmez; **her seferinde bir `if` ile** ölür: her biri kendi başına makul
  ("arayüz zaten sordu, istemi atla") ve birlikte, tek ad taşıyan iki program — ve yalnız biri
  test edilmiş oluyor. Kapı ihlal enjekte edilerek doğrulandı (çıkış 1), temizken 0, ve yorum
  satırlarını kod saymıyor — kendi gerekçesini açıklamayı yasaklayan bir kapı olmasın.
  `command.md` P10 kapıyı ve "argümanın boşluğu karar verir" cümlesini adıyla yazıyor.

- [x] **İNŞA YÖNTEMLERİ ARAYÜZE GİRDİ (§2.6a).** P2 daire, yay, dikdörtgen, çokgen ve elipse
  klasik yöntemlerini verdi ve **hepsi yalnız `yontem=` yazılarak** erişilebiliyordu: bir el üç
  noktadan daire çizemiyordu. Komut bir düğmede olduğu için `probeReach` memnundu, yöntem değildi —
  5.15'in bir düzey aşağıdaki hâli. Her yöntem artık ailesinde kendi satırı: `MainWindow::methodTool`
  **tam satırı** `kToolCommand`'a koyuyor, yani kartın sağ kolonu `YAY yontem=3n` yazıyor ve kart
  aynı zamanda komut satırını öğretiyor. Aile üyesi 26'dan **37'ye** çıktı, `KENTOS_FLYOUT_PROBE`
  hepsini fareyle basıyor: 0 kusur. `arayuz.md`'de aile tablosu ve kendi bölümü.

Plan belgesinin her satırı TODOS'a karşı okundu; TODOS'a hiç girmemiş üç madde çıktı.

- [x] **`SEÇ tur=` süzgeci eklendi.** Plan "`SEÇ katman= tur=` süzgeçleri varsa docs'a, yoksa
  eklenir" diyordu: `katman` vardı, `tur` **yoktu**. Bir pafta üzerine atılan pencere parselleri,
  etiketleri, ölçüleri ve yol eksenini birlikte yakalar; "o penceredeki alanlar" sürekli sorulan
  şeydir. `katman=` bir KİP çünkü bir katman kendi başına bir küme adlandırır; `tur=` bir SÜZGEÇ
  çünkü bir tür bir kümeyi daraltır — ve her kiple (`ÇİT`, `ÖNCEKİ` dâhil) birlikte çalışıyor. Tür
  adı **nesne türleri tablosunun kendi adı** (`find_name`), yani ikinci bir tür listesi yok ve bir
  eklentinin türü eklendiği gün seçilebilir (5.10). Üç test, `select.md`'de kendi bölümü.
- [x] **Nokta fonksiyonu idempotens testi** — planın Test maddesinin adıyla istediği
  ("fonksiyon çıktısı tekrar girdi olarak aynı noktayı verir") ve olmayan test. Her fonksiyonun
  SABİT NOKTASI sınanıyor (`orta(P,P)=P`, `ara(A,B,0)=A`, `uzanti(A,B,0)=B`, `dik(A,B,0,0)=A`,
  `semt(S,θ,0)=S`, `xy(P,P)=P`, `ile(P,@0,0)=P`, `n(no)` her seferinde aynı), sonra çıktı geri
  besleniyor ve iç içe yazılıyor — bunlar iç içe yazıldığı için, kendi cevabıyla kayan bir fonksiyon
  her düzeyde bir milimetre kayardı. Yedi çağrı ayrıca iki yoldan (yazılan belirteç ve JSON dizesi)
  aynı noktayı veriyor.
- [~] **Ölçü stilinin BÖHHBÜY/MPYY yarısı**: aşağıda, P5'te.

## Kanıt denetimi (plan sonu, 2026-09-21)

- [x] **Eksik eşitlik kanıtları yazıldı.** Planın Doğrulama bölümü her yeni/değişen komut için bir
  `test_proof.cpp` vakası istiyor (Article 6.4). Denetimde eksik çıkanlar: `DAİRE yontem=3n`,
  `YAY yontem=bby`, `ÇOKGEN`, P3'ün yedi fiili (KIR, UÇUCA, UZUNLUK, PATLAT, HİZALA, BÖLÜMLE,
  ÇİZGİDÜZENLE), `SEÇ ÇİT`, `ÖLÇÜ tur=koordinat`, `PANOYAKOPYALA`/`YAPIŞTIR`, `İZ`. On yedi vaka.
  Yedi fiilin kanıtı tek şekilde yazıldı (`prove_verb`): yedi kez kopyalanmış bir kanıt, altısında
  kayan bir kanıttır.
- [x] **Kanıtlar üç kusur ortaya çıkardı** — varlık sebepleri tam olarak bu:
  * `SEÇ ÇİT` ve `SEÇ ÇOKGEN` **fareyle tek nokta** toplayabiliyordu (`while (supplied.empty())`
    birinci noktadan sonra çıkıyor). `select.md` doğru davranışı zaten yazıyordu; kod yanlıştı.
  * `UZUNLUK` ve `BÖLÜMLE` okumalarını **sormuyordu** (POLİGON'la aynı kusur, 5.15).
  * Günlükte bir tam sayı **geldiği yola göre** `25` ya da `25.0` yazılıyordu; kayıt artık bildirilen
    türe çevriliyor. Hiçbir altın fikstür değişmedi.
- [x] **`3n` daire altın fikstürü yazıldı** — ve planın istediğinden genişi:
  `tests/golden/senaryolar/insa-yontemleri.txt` P2'nin BÜTÜN inşa yöntemlerini tutuyor (3n/2n/ttr
  daire, 3n/bby×2/bma×2 yay, ic/dis/kenar/açılı çokgen, 3n dikdörtgen, eksen elipsi). Her biri
  trigonometri içeriyor, yani her biri bit-özdeş olmak zorunda (§7.3). **Her sayı elle
  doğrulandı**: 3-4-5 üçgeninin çevrel çemberi (30, 40) r=50; altıgenin dış yarıçapı
  30/cos30 = 34,641; sekizgenin çevresi tam 160 m; 50 grad'lık üçgenin ilk köşesi kuzeyden
  45° ile (321213, 621213); 3n dikdörtgenin alanı tam 2400 m²; elipsin alanı tam π·1000 m².
  **Fikstür üretilip OKUNDUĞU için bir kusur çıktı**: elipsin ikinci nokta parametresini ana
  eksenin üzerinde vermişim, komut sessizce hiçbir şey çizmemiş ve 14 nesne yerine 13 gelmiş.
- [x] **F3 yakalama listesi gerçek ikiliden doğrulandı** (release listesi maddesi 3): `probeMenus`
  artık yakalama açılır menüsünü de yazıyor ve satır sayısını motorun mod sayısıyla karşılaştırıyor.
  18 mod listeleniyor, **çeyrek nokta ve teğet nokta dâhil** — aralık düzeltilene kadar bitleri
  ayardan yazılamıyordu. "Tutar" yarısı `test_snap.cpp`'de.
- [x] **Yakalama idempotens testi yazıldı** (`snap(snap(p)) == snap(p)`) — planın Birim maddesinin
  adıyla istediği ve **hiç var olmayan** test. Maske üzerinde döngüyle yazıldı, mod başına vaka
  değil: motora eklenen bir mod bildirildiği gün kapsanıyor (5.10). Onaltı hedef ve bir sahne, her
  modun tutacağı bir şey içeriyor; **tutmayan bir mod sayılıp bildiriliyor**, çünkü sessiz bir atlama
  o modun özelliği kanıtlanmadan kanıtlanmış sayılması demektir. `ekleme` tek istisna ve sebebi
  yazılı: hiçbir yerleşik tür henüz ekleme noktası yayınlamıyor (`snap.hpp`), yani cevaplayacağı bir
  şey yok. **Tutan her mod idempotent çıktı.**
- [x] **`UÇUCA` ile `BİRLEŞTİR` iki sayfada yan yana anlatıldı** (planın "Bilinmesi gerekenler"
  maddesi). `join.md` "her iki sayfa öbürünü adıyla anar" diyordu ve `combine.md` `UÇUCA`'yı hiç
  anmıyordu — cümle bir yönde yanlıştı. **Ve düzeltirken yazdığım ilk tablo da yanlıştı**:
  BİRLEŞTİR'in çizgide "yalnız tam değen uçları eklediğini" yazmışım, oysa PROJENİN düğüm
  toleransını okuyor (varsayılan 10 mm). Fark **toleransın nereden geldiğidir** — `UÇUCA`'da
  çağrının kendi `tolerans=`'ı (1 mm), `BİRLEŞTİR`'de projenin ayarı. Üç vakalı bir test bunu
  çiviliyor; iki belge yanlış bir cümleyi yayımlamak üzereydi.
- [x] **Günlük oynatma ve `Value` gidiş-dönüşü, komut başına değil ÖZELLİK olarak** yazıldı
  (`test_golden.cpp`): her senaryonun günlüğü boş bir belgeye yeniden uygulanıyor ve aynı çizimi
  kurmak zorunda, ve her günlük satırı `to_json` → `from_json` turunu bayt bayt geçmek zorunda.
  Komut başına vaka yerine senaryolar üzerinde döngü, çünkü altın senaryolar çizim yüzeyinin
  büyük kısmını kullanıyor ve bir özellik testi yazılmayı bekleyen bir vaka değildir.
  **İlk çalıştırmada iki kusur buldu** (aşağıda). `GERİAL` içeren senaryo muaf ve sebebi yazılı:
  undo `ReadOnly` olduğu için günlüğe düşmüyor — günlük belgeye ne YAPILDIĞINI tutar, undo ise
  yığının hamlesidir. Muafiyet senaryoyu tarayarak veriliyor, elle listelenerek değil.
- [x] **İptal özelliği** (`her etkileşimli komut iptal edilince boş geri alma deltası bırakır`):
  `Registry` üzerinde döngü, ~64 etkileşimli komut, hepsi Esc'te belgeyi ve yığını olduğu gibi
  bırakıyor. Esc bir CAD programında en çok basılan tuştur; ilk isteminden önce yazan bir komut
  her seferinde yarım bir düzenleme bırakır, hem de sessizce — çünkü kimse değiştirmemeye karar
  verdiği çizime bakmaz.
- [x] **Oynatma özelliğinin bulduğu iki kusur:**
  * `ELİPS yontem=eksen` **kendi günlüğünden oynatılamıyordu.** Ortak kuyruk `birinci`'yi koşulsuz
    kaydediyor ve `eksen` altında BİRİNCİ eksen ucunu ANA eksen ucuyla eziyordu; satır "iki uç aynı
    nokta" diyerek gidiyor ve oynatma reddediyordu. Kuyruk artık `eksen` altında ezmiyor.
  * `ÖLÇÜ tur=acisal` **kendi yazdığı sözcüğü okuyamıyordu**: komut modelin kararlı adını
    (`acisal3`) kaydediyor, ayrıştırıcısı ise onu tanımıyordu. Açısal ölçü sessizce hiç
    oynatılmıyor ve altın senaryo bir nesne eksik kuruluyordu. `acisal3`/`angular3` kabul ediliyor;
    ad tablosundan TÜRETİLMEDİ, çünkü `acisal` öbür açısal türün (beş noktalı `Angular`) kararlı
    adı ve türetmek bir sözcüğü iki türe bağlardı.
- [x] **Release listesinin `DİKAYAK 0,0 100,0 30 -5` yazımı hiç çalışmıyordu — ve SESSİZ
  dönüyordu.** İki dizi (`ayak`, `boy`) arka arkaya bildirildiği için çıplak sayıların hepsi
  birincisine bağlanıyor, ikincisi boş kalıyor, döngü ilk eksik yarıda kırılıyor ve komut hiçbir şey
  söylemeden geri dönüyordu — bir komutun verebileceği en kötü cevap, çünkü kullanıcının
  düzeltecek bir şeyi yok. Sayıları bölmek **çözüm değil**: hangisinin ayak hangisinin boy olduğuna
  karar vermek bir tahmindir ve bir detayın taban çizgisinin hangi tarafına düştüğünü tahmin etmek,
  bir yapının sınırın yanlış tarafına oturması demektir. Onun yerine ret, eşleşme kuralını ve iki
  sayıyı söylüyor.
- [x] **Ve denetim TEK BİR POINT BİLE KOYULMADAN yapılıyor** (Article 1.6): üç `ayak` ile iki `boy`
  eskiden tam olan iki çifti koyup üçüncüyü sessizce düşürüyordu — yanlış aktarılmış bir karne
  satırının ölçüden kaybolma yolu. `ALIM` aynı denetimi aldı; onun konumsal yazımı ise zaten
  bus'ın doğrulaması tarafından adıyla reddediliyor (`baglama` bir nokta listesi). İki sayfa da
  kuralı yazıyor.
- [x] **Fuzz korpusu güncel**: P0'ın açı sonekleri (`04-kutupsal-sonekli`, `08-bozuk` içinde
  `@100<45x`, `@100<45gg`) ve P1a'nın nokta fonksiyonları (`15`, `16`, `17`) tohumda.

## Gerçek kullanım raporu — Mac, gerçek fare (2026-09-22)

Kullanıcının kendi Mac'inde derleyip çizerken bildirdiği altı şikâyet. Her biri önce
`KENTOS_REALMOUSE_PROBE` ile gerçek pencerede kırmızı üretildi, sonra düzeltildi; probe artık
yedi bölümle bunları sürekli tutuyor.

- [x] **"Yeni çizim öğeleri seçilince menüde seçili kalmıyor / seçim bırakılıyor."** Kök neden
  `syncToolSelection`: yöntem araçlarının düğmesi `YAY yontem=3n` gibi TAM SATIR taşıyor, senkron
  bu satırı komut adı diye `Registry`'de arıyor, bulamıyor, hiçbir düğmeyi yakmıyordu. `Controller`
  kurduğu satırı `armedLine()` olarak tutuyor; eşleşme önce tam satır, sonra ilk sözcük. Işık ilk
  tıklamadan sonra da duruyor.
- [x] **"Çizerken kılavuz çizgileri gözükmüyor."** Aynı kök: yanmayan araçta lastik bant da
  kuruluyordu ama kullanıcı aracın düştüğünü sanıp bırakıyordu. Probe yöntem aracında ilk
  tıklamadan sonra kılavuzun (`buildTracking` + lastik bant) var olduğunu doğruluyor.
- [x] **"Alan ölçme çalışmıyor."** Probe alanın İÇİNE tıklıyor (`ring_contains`), `alan: 200,00 m²
  çevre: 60,000 m` cevabını alıyor. Komut doğru; kullanıcının derlemesi dökümün en alta kaymadığı
  önceki sürümdü. Bu sürümde döküm yeni satırı izliyor.
- [x] **"Blok ekle ne işe yarıyor bilmiyorum."** `BLOKEKLE` çizimde tanımlı blok yokken önden
  açıklıyor: ne olduğu, nasıl tanımlanır (`BLOK ad=`), sonra ne yapar. Boş seçici açılmıyor.
- [x] **"Sahnedeki objeleri Mac'te nasıl sileceğimi anlamadım."** `SİL` = **Del** ya da **⌫**;
  komut satırı odaktayken ⌫ karakter siler, nesne silmez (probe iki hâli de sınıyor). Araç ipucu
  ve `arayuz.md` tablosu yazıyor.
- [x] **"Çoklu çizgi aracı ile çizgi aracı aynı."** Fark üç yerde: iki ipucu, `ÇİZGİ`'nin bitiş
  satırı ("2 çizgi çizildi, her biri ayrı nesne (tek nesne için ÇOKLUÇİZGİ)"), `line.md`'de
  karşılaştırma tablosu.
- [x] **Kart probe'u** (`tool-flyouts` ctest) "çalıştı"yı üç hâlle tanımlıyor: bekleyen oturum,
  yanan düğme ya da dökümde cevap. `BLOKEKLE`'nin reddi üçüncüsü.
- [x] **İŞLETİM SİSTEMİ DÜZEYİNDE TIKLAMA YAPILDI ve altı şikâyetin hepsi gerçek fare/klavye
  olaylarıyla doğrulandı.** Kullanıcı erişilebilirlik iznini verdi; `KENTOS_OSCLICK_PROBE` ile
  pencere açık tutuluyor, her düğmenin ekran konumu yazılıyor, sonra dışarıdan gerçek olaylar
  sürülüyor ve probe ne olduğunu satır satır bildiriyor (tetiklenen eylem, oturum, istem, nesne
  sayısı, döküm satırı, açılan kart). Gerçek olaylarla alınan kayıt:
  `tetiklendi ÇİZGİ` → `istem İlk nokta` → `nesne 1` → `nesne 2` →
  `2 çizgi çizildi, her biri ayrı nesne (tek nesne için ÇOKLUÇİZGİ).`;
  YAY köşe işareti → `kart 6 satır` → 2. üye → `tetiklendi YAY yontem=3n` →
  **`yanan YAY yontem=3n`** ve ilk noktadan sonra `istem Yayın üzerinden geçtiği nokta`;
  `tetiklendi BLOKEKLE` → blok yokken tam açıklama; çizginin üstüne tıklama →
  `1 nesne bulundu` → **⌫** → `nesne 0` → `1 nesne silindi.`; 500 ms basılı tutma kartı açıyor.
- [x] **`osascript` GERÇEK FARE DEĞİLDİR — ve bu tek başına bir bulgu.** System Events'in
  `click at` komutu fare olayı göndermiyor; noktadaki öğeye erişilebilirlik "press" uyguluyor.
  Uygulamanın olay akışı izlendiğinde hiçbir `MouseButtonPress` gelmediği, buna rağmen araç
  eyleminin işaretlendiği görüldü. Bu yüzden sürücü `scripts/os-tikla.c` (CGEventPost) oldu:
  fiziksel farenin geçtiği yol. Bir sonraki okuyucu `osascript` ile "araçlar çalışmıyor" sonucuna
  varmasın diye yazılıyor.
- [x] **ERİŞİLEBİLİRLİK AÇIĞI (bulundu ve kapandı).** Araç kolonundaki düğmeler `checkable`
  olduğu için macOS erişilebilirlik katmanı onları "geçiş kutusu" sayıyordu: AXPress eylemi
  `setChecked` yapıyor, `triggered` çıkmıyor, **komut çalışmıyordu**. Kanıt: gerçek fareyle
  `olay bas QToolButton/ÇİZGİ` + `tetiklendi ÇİZGİ` çıkarken, erişilebilirlik basışında yalnız
  `isaretlendi METİN` çıkıyordu. Seçilen yol ikisinden birincisi: kolonun kendi düğme türü
  (`ToolButton`) ve ona verilen erişilebilirlik arayüzü (`ToolButtonAccessible`) **her eylemi
  `click()`'e** bağlıyor — farenin yolu. Yanma `checkable` üstünde kaldı, çünkü ağacın
  "işaretli" demesi yanan düğmenin sesli karşılığıdır. Aynı partide klavye yolu da geldi: kolon
  tek Tab durağı, ok tuşları gezer, Boşluk/Enter çalıştırır, → aile kartını açar. Gerçek macOS
  basışıyla doğrulandı (`tetiklendi METİN` → `oturum core.text bekliyor=1`), kapısı
  `tool-accessible` ctest'i (`KENTOS_ACCESS_PROBE`), eski davranışta 19 iddia kırmızı.
- [ ] **Aile kartının kendisi erişilebilirlik ağacında yok.** `ToolFlyout` elle çizilir, yani
  satırları birer widget değil; ekran okuyucu kartı açabilir (→ ya da köşe işareti) ama
  içindeki on üyeyi okuyamaz. `ui.md` R22'nin "her elle çizilen widget `QAccessibleInterface`
  uygular" şartı. Üyelerin hepsi **Çiz** menüsünde ve komut satırında olduğu için 5.15 ihlali
  değil; kartın kendi arayüzü ayrı bir iştir ve kendi testini ister. Aynı soru `MapCanvas`,
  `ColourChips` ve `DataGrid` için de açık.
- [~] **İşletim sistemi düzeyinde tıklama** (Cocoa'nın gerçek olayları) ile aynı probe: Qt'nin
  sentezlediği olaylar ile gerçek olaylar arasında bir fark varsa yalnız orada görünür. Bu makinede
  yapılamadı: `osascript`'in erişilebilirlik (assistive access) izni yok (`-1719`), `cliclick` ve
  `pyobjc/Quartz` kurulu değil. İzin bir sistem güvenlik ayarıdır ve yalnız kullanıcı verir: Sistem
  Ayarları → Gizlilik ve Güvenlik → Erişilebilirlik'te terminale izin verilir. **İzin verildi ve
  yapıldı; üstteki iki satır sonucu yazıyor.** `osascript`'in `click at`'i yetmedi (erişilebilirlik
  basışı, fare olayı değil); sürücü `scripts/os-tikla.c` + `scripts/os-tikla.sh`.

## Çokgen araçları elden geçti — gerçek fareyle (2026-09-22)

Kullanıcının ikinci raporu: "döndürülmüş dikdörtgen çiziyor ama kılavuz yok; düzgün çokgende hiçbir
aksiyon yok; dıştan/kenardan aynı; kenar sayısını girecek yer yok; polygon araçlarını komple elden
geçir." Hepsi `KENTOS_OSCLICK_PROBE` + `scripts/os-tikla.sh` ile gerçek olaylarla doğrulandı.

- [x] **Ortak geometri `core/polygon.hpp`'ye çıktı**: `regular_polygon_corners`,
  `polygon_circumradius`/`polygon_measurement` (birbirinin tersi), `polygon_half_step_turns`,
  `edge_rectangle_corners`, `PolygonGuide` yükü. Komut ve tuval kılavuzu **tek** cevabı paylaşıyor;
  `circle.hpp`'nin `circle_outline` için zaten uyduğu kural (CLAUDE.md 5.16, 5.10).
- [x] **`DİKDÖRTGEN yontem=3n` kılavuzu**: `RubberShape::EdgeRectangle`, zincir {ilk, ikinci},
  imleç yüksekliği veriyor; dört köşe çiziliyor. Eski hâli zincirsiz `Ring`'di — iki nokta, çizilecek
  hiçbir şey yok.
- [x] **`ÇOKGEN` soru sırası**: kenar sayısı → merkez → (boy/yön). Kenar sayısı ilk sırada, çünkü
  düğmeye basıldığı anda ekranda bakacak bir şey yok ve odak komut satırına geçiyor.
- [x] **Boy işaret edilerek**: `ic` köşe, `dis` kenar (yarım adım), `kenar` yazıyla boy + fareyle
  dönüş. `RubberShape::Polygon` + `PolygonGuide` yükü (kenar sayısı, yöntem, sabit yarıçap).
- [x] **Yeni `kose` parametresi** (nokta, 0..1); türeyen `yaricap`/`aci` günlüğe yazılıyor, `kose`
  yazılmıyor. `yaricap` argümanla geldiyse komut hiçbir şey sormuyor — eski günlük satırları aynen
  oynuyor (`insa-yontemleri.txt` altın fikstürü değişmedi).
- [x] **`Args::erase` ve günlükten boş argümanın düşmesi**: `"kose":null` bir cevapsızlığı cevap
  olarak yazıyordu ve aynı işi yapan iki istemciyi ayırıyordu.
- [x] **Testler**: `test_geometry.cpp`'de dört yeni vaka (üç yöntemin tek yarıçapa inmesi ve
  terslerinin birbirini götürmesi, köşeler ve dönüş yönü iki kuralda, kenar üstüne dikdörtgen 3-4-5,
  kılavuz yükünün gidip gelmesi ve bozuk yükün reddi); `test_command.cpp`'de soru sırası, `dis`
  yarım adımı, `kenar`ın sabit boyu, argümanla gelen boyun soru sormaması, ESC'te boş undo;
  `DİKDÖRTGEN 3n` önizleme zinciri; `test_proof.cpp`'de ÇOKGEN eşitlik kanıtı işaret edilen jeste
  göre yazıldı (üç istemci aynı işi yapıyor: `aci=0` üçünde de var).
- [x] **Gerçek olaylarla görsel doğrulama**: altıgen (`ic`), sekizgen (`dis`, kenar imlecin altından
  geçiyor), beşgen (`kenar`, yazılan 8 m sabit kalıp imleçle dönüyor), döndürülmüş dikdörtgen (dört
  köşe fareyi izliyor). Dördünün karesi de alındı.
- [x] **Belgeler**: `polygon_regular.md`'ye "Fareyle: boyu göstererek" bölümü, `kose` satırı, yeni
  arayüz sırası ve yeni hata; `rectangle.md`'ye iki yöntemin kılavuzu ve döndürülmüş dikdörtgenin
  arayüz adımları; `make reference` (dört üretilmiş dosya, 6.14).

## Araç çizimden sonra elde kalıyor (2026-09-22)

Rapor: "çizim yaptıktan sonra varsayılan araç seçiliyor tekrardan." İki ayrı kusur.

- [x] **Enter nokta dizisini bitirmiyordu.** Sağ tık `finishInteractive()`, Enter ise hiçbir şey.
  Böyle bir çalışmayı bitiren tek tuş Esc'ti ve Esc aracı bırakır. `MapCanvas::finishPointRun()`
  geldi; tuvalden ve komut satırından çağrılıyor, yalnız `ParamKind::Point` isteminde çalışıyor
  (seçim `supplyPickedObjects`'in, istenen alan `acceptGuide`'ın, ad/sayı yazmanın).
- [x] **Tekrar, ailenin ilk üyesini kuruyordu.** `onInteractiveFinished` tam satırı komut adı diye
  çözüyor, bulamıyor, döngü devam edip düz üyeyi buluyordu. Artık önce `armedLine()` ile tam satır,
  sonra ilk sözcük; `armedLine_` sinyalden sonra temizleniyor. `rearm()` ortak gövde oldu.
- [x] **Probe 8. bölüm** (`KENTOS_REALMOUSE_PROBE`): Enter'dan sonra oturum yeniden soruyor, yanan
  düğme hâlâ ÇİZGİ; yöntem aracıyla çizimden sonra yanan düğme hâlâ `ÇOKGEN yontem=dis`. Offscreen
  ve gerçek pencerede 0 kusur.
- [ ] **Tekrar her seferinde kenar sayısını yeniden soruyor.** Şimdi araç elde kaldığı için görünür
  oldu: beş altıgen çizmek "6" yazmayı beş kez istiyor. AutoCAD son değeri varsayılan olarak sunar
  (`<4>`), bizde böyle bir mekanizma yok. Dürüst yolu bir oturum ayarı olmalı (`core.aci.birim`
  kalıbı): komut, argüman gelmediğinde ayarı okur, betik açıkça verdiğinde ayar karışmaz ve günlük
  zaten çözülmüş değeri tutar. Karar kullanıcının: bir betiğin `kenar_sayisi`'nı atlaması ayardan
  okumak anlamına gelecek mi. Bu satır o kararı bekliyor.

## Daire, elips, halka ve ikon ayrımı (2026-09-22)

Rapor: "taşı ve esnet ikonları aynı toolbox üzerinde, aynı düzenlemeleri daire ve elips için de
yapalım" + "halka çizilirken ilk çizilen halkanın kılavuz çizgileri kalmalı."

- [x] **Dört yeni ikon**: `Stretch`, `Ellipse`, `Annulus`, `Sector`. Kolon etiket değil ikon
  gösterir; `ESNET` `TAŞI`'nın okunu, `ELİPS`/`HALKA`/`DİLİM` ise `DAİRE`'nin çemberini taşıyordu.
  Denetim yeniden çalıştırıldı: kalan paylaşımlar yalnız aynı şeklin yöntem çeşitleri.
- [x] **`core/circle.hpp` büyüdü**: `tangent_circle_centre` (dört çözümden en yakını),
  `CircleBuild` (merkez/2n/3n/ttr), `CircleGuide` + yükü, `circle_from_guide`. Komut dört yöntemi de
  buradan kuruyor ve kendi `span` kopyasını bıraktı; aritmetik birebir, altın fikstür değişmedi.
- [x] **`RubberShape::CircleBuild`**: `2n` (zincir: ilk uç), `3n` (zincir: ilk iki nokta), `ttr`
  (zincir: iki doğrunun dört noktası, yük: yarıçap). Tuval dalı tek; çemberi `circle_outline` çiziyor.
- [x] **HALKA'nın iç çemberi** `rubber_chain` ile duruyor; `Circle` dalı zincir doluysa sabitlenmiş
  çemberi de çiziyor. `DİLİM`de aynı zincir ilk kenarın yarıçap çizgisini veriyor.
- [x] **ELİPS'e dokunulmadı, çünkü önizlemesi zaten doğruydu**: `merkez`de birinci eksen çizgi,
  üçüncü tıklamada elipsin kendisi; `eksen`de merkez iki ucun ortası. Gerçek fareyle doğrulandı.
- [x] **Testler**: `test_geometry.cpp`'de üç yeni vaka (dört çeyrekte teğet merkez, dört yapı ve
  reddedilen hâlleri, yük gidip gelmesi); `test_command.cpp`'de her yöntemin kılavuz yükü ve
  `ttr`'nin işaret edilen çeyreği çizdiği, HALKA'nın zinciri. 1000 birim testi, 57 ctest.
- [x] **Gerçek olaylarla görsel**: `2n`, `3n`, `ttr` ve HALKA kareleri alındı.
- [x] **Belgeler**: `circle_draw.md`'ye "Kılavuz: her yöntemde çemberin kendisi" tablosu ve kart
  adımları; `annulus.md`'ye iç çemberin kalması.

## Yay yöntemleri (2026-09-22)

Rapor: "şimdi yay hatalarını düzelt, aynı sorunlar."

- [x] **`core/arc.hpp` büyüdü**: `ArcBuild` (3n / teğet / yarıçap), `ArcGuide` + yükü,
  `arc_from_guide`, `arc_radius_side`, `arc_by_radius`. Komut ve kılavuz aynı fonksiyonları
  çağırıyor; `insa-yontemleri.txt` bayt-özdeş kaldı, yani `devam`'ın geometrisi değişmedi.
- [x] **`RubberShape::ArcBuild`**: `3n` (zincir: başlangıç + üzerinden), `devam` (zincir: başlangıç
  + teğet üzerinde bir nokta, 1 km ötede — iki taraf aynı doğrultuyu okusun diye), `bby` (zincir:
  iki uç, yük: yarıçap). Tuval dalı tek; `arc_outline` çiziyor.
- [x] **`bby` yanı soruyor** (`yon_nokta`, yeni nokta parametresi). Eskiden `yon` yalnız argümandan
  okunuyor ve `sol` varsayılıyordu: arayüzden öteki yay erişilemezdi. `yon` verilmişse soru yok —
  eski günlük satırları aynen oynuyor.
- [x] **Yan ölçütü kirişin hangi yanı**, en yakın merkez değil: yarıçap tam yarım açıklıkken iki
  merkez çakışıyor ve o test iki yayı ayırt edemiyor (test bunu tutuyor).
- [x] **Yarıçap denetimi yan sorusundan önce.** Olmayan bir yayın yanı sorulmaz.
- [x] **`bma`'ya dokunulmadı**: süpürme bir sayıdır ve fareyle verilen hâli `merkez` yöntemidir.
  İkinci bir pointed yöntem eklemek `merkez`i tekrarlamak olurdu.
- [x] **Testler**: `test_geometry.cpp`'de iki yeni vaka (üç yapı ve reddedilen hâlleri, yan ölçütü,
  yük gidip gelmesi); `test_command.cpp`'de beş bölüm (3n zinciri ve yarım çember, devam'ın teğet
  zinciri, bby'nin sorduğu yan ve kuzey yayı, `yon` verilince soru sormaması, küçük yarıçabın anında
  reddi). 1003 birim testi, 57 ctest.
- [x] **Gerçek olaylarla görsel**: `3n`, `devam` (teğet ayrılış), `bby` (yan seçimi) kareleri.
- [x] **Belgeler**: `arc_draw.md`'ye "Kılavuz: her yöntemde yayın kendisi" tablosu, `yon`/`yon_nokta`
  satırları, kart adımları; `make reference` (dört üretilmiş dosya, 6.14).

## Nokta araçları (2026-09-22)

Rapor: "daha sonra da nokta araçları."

- [x] **Ortak boşluk: sayı sorulurken referans kayboluyordu.** Yeni `RubberShape::Fixed` sabitlenmiş
  olanı çiziyor, imleci izleyen hiçbir şey çizmiyor, ve imleç tuvalin dışındayken de çiziliyor
  (yazan el fareyi orada bırakır). DİKAYAK'ta taban + `ayak` sonrası ayak noktası; ALIM'da istasyon
  + bağlama; ARANOKTA'da doğru; KESİŞİMNOKTA'da bilinen noktalar.
- [x] **`ctx.number` / `ctx.integer` kılavuz alabiliyor.** Bir kılavuz yalnız noktaya ait değil —
  istemin kendisine ait.
- [x] **`KESİŞİMNOKTA yontem=mesafe` yanı soruyor** (yeni `yon_nokta` parametresi, yeni
  `RubberShape::Candidates`): iki çözüm işaretli, imlece yakın olan vurgulu, `yon` türetilip
  günlüğe yazılıyor. `yon` verilmişse soru yok — eski satırlar aynen oynuyor.
- [x] **Yöntemler karta girdi** (§2.6a): Kesişim — iki mesafeden / iki doğrudan, Ara Nokta —
  mesafeden. Kart artık 40 üye; `tool-flyouts` probe'u hepsini çalıştırıyor.
- [x] **Dört yeni ikon**: `PointIntersect`, `PointAlong`, `PerpOffset`, `Survey`. `ÖLÇÜ` cetvelini,
  `LİDER` imlecini geri aldı.
- [x] **DİKAYAK'ın lastik bandı** — bu satır "Bilinsin" listesinde bekliyordu ve kapandı: taban
  çizgisi ve ayak noktası okumalar yazılırken duruyor. Sayı istemine imleç izleyen bir bant
  takılmadı, çünkü o soruyu fare cevaplamıyor.
- [x] **Testler**: `test_command.cpp`'de iki bölüm (üç aracın `Fixed` zinciri ve ayak noktasının
  eklenmesi; `mesafe`'nin iki adayı, kuzey/güney seçimi ve `yon` verilince soru sormaması).
  1005 birim testi, 57 ctest.
- [x] **Gerçek olaylarla görsel**: DİKAYAK'ın duran tabanı ve `mesafe`'nin iki işaretli adayı.
- [x] **Belgeler**: dört sayfaya "ekranda kalır" bölümleri, `intersect_point.md`'ye sorulan yan ve
  `yon_nokta` satırı; `make reference`.

## Düzenleme fiilleri ve hayalet önizleme (2026-09-22)

Rapor: "senin dediklerini yapalım ama hayalet önizleme kusursuz olmalı."

- [x] **`Xform` `core/transform.hpp`'ye çıktı** (`core::Xform`, `core::transformed`). Fiil belgeyi,
  tuval hayaleti aynı fonksiyonla yazıyor; ayrışma yapısal olarak imkânsız. Eskiden hayalet
  `dx, dy` alıyordu, yani önizleyebildiği tek şey bir kaydırmaydı.
- [x] **`addWorldRun` / `addEmitRuns` / `addGhost` dönüşüm alıyor**, öteleme değil. Varsayılan
  birim dönüşüm, yani hayalet dışındaki bütün çağıranlar değişmedi.
- [x] **`core::GhostSpec` + `core::ghost_xform`**: fiil hangi dönüşümün önizlendiğini yükte söylüyor,
  imleci dönüşüme çeviren tek fonksiyon hem fiil hem tuval tarafından çağrılıyor. Dört tür:
  Translate (TAŞI/KOPYALA), Rotate (DÖNDÜR), Scale (ÖLÇEKLE), Mirror (AYNALA).
- [x] **DÖNDÜR ve ÖLÇEKLE fareyle**: `aci_nokta` ve `carpan_nokta` (yeni nokta parametreleri).
  Açı imlecin doğrultusu, çarpan imlecin metre cinsinden uzaklığı — AutoCAD'in sürükleme kuralı.
  Sayı argümanla geldiyse soru yok.
- [x] **AYNALA aynalanmış hayalet** gösteriyor; eskiden yalnız eksen çizgisiydi, yani komutun tek
  derdi olan şey olup bittikten sonra görülüyordu.
- [x] **Altı fiil araç kutusunda** (§2.6a): TAŞI'nın kartında KOPYALA, DÖNDÜR, ÖLÇEKLE, AYNALA, DİZİ.
  Kart artık 9 aile 46 üye, hepsi probe'dan geçiyor.
- [x] **Dört yeni ikon**: `Scale`, `Mirror`, `Array`, `BlockInsert`. `Rotate` yalnız DÖNDÜR'ün,
  `Copy` yalnız KOPYALA'nın.
- [x] **Testler**: `test_geometry.cpp`'de üç vaka (dört dönüşüm tek fonksiyondan; imlecin dönüşüme
  çevrilmesi — kuzeye bakmak çeyrek tur, 3-4-5 ile çarpan 5; yük gidip gelmesi);
  `test_command.cpp`'de beş bölüm (her fiilin kendi hayalet türünü adlandırması, işaret edilen
  açının çeyrek tur vermesi, işaret edilen çarpanın iki kat yapması, sayı verilince soru
  sormaması). 1009 birim testi, 57 ctest.
- [x] **Gerçek olaylarla görsel**: dönen hayalet (57 grad), büyüyen hayalet, aynalanmış hayalet.
- [ ] **DİZİ'nin hayaleti yok.** Satır/sütun sayıları ve aralıkları yazıyla veriliyor, yani imlecin
  tamamlayacağı bir şey yok; `GhostSpec::copies` alanı ötelemeli tekrar için hazır duruyor ama
  aralığı fareyle veren bir yol eklenmedi. Eklenirse `DİZİ` de kopyalarını önizler.

## Doğrulama (her pakette)

- **Birim:** `test_command.cpp` (gramer, fonksiyonlar — bilinen üçgenler `Mm`'de), `test_snap.cpp`
  (`snap(snap(p)) == snap(p)`), `test_geodesy.cpp` (poligon, kapanma dağıtımı).
- **Eşitlik kanıtı:** `test_proof.cpp` — GUI (`InputSource` fare) = komut satırı = JSON betik → özdeş
  `Document`, bayt-özdeş günlük; replay altın belgeyi verir (6.4, test.md R3).
- **Golden:** `POLİGON`, `3n` daire, ölçü metni — üç platformda bit-özdeş (6.5).
- **Fuzz:** parser değişti → `tests/fuzz` harness ve tohum korpusu (6.7, R9).
- **Kapılar:** `ci-gate-docs.sh`, `ci-gate-catalogs.sh`, `ci-gate-hardcoded-thresholds.sh`,
  `ci-gate-i18n.sh`, `ci-gate-comments.sh`.
- **Elle (release listesi) — beşi de artık OTOMATİK kapsanıyor, elle değil:**
  * dik ayak fareyle ve yazarak aynı nokta → `PROOF: DİKAYAK …` (ve planın `30 -5` yazımının
    çalışmadığı, sessiz döndüğü orada bulundu ve düzeltildi);
  * `@100<45` semt'te kuzeydoğuya 40,5° → `test_command.cpp:232` ve `komut-satiri.md`;
  * F3'te ÇEYREK/TEĞET görünür → `probeMenus` gerçek ikiliden 18 modu yazıyor; tutar →
    `test_snap.cpp` (ayrıca her mod idempotent);
  * `ÇİT` ile seçim → `PROOF: SEÇ ÇİT …` (ve fareyle tek nokta toplayabildiği orada bulundu);
  * KES→YAPIŞTIR → `PANO:` vakaları, `PROOF: PANOYAKOPYALA …` ve `os-clipboard` ctest'i.

## Bilinmesi gerekenler

- Açı kuralı değişimi **canlı metni** etkiler; günlükler değil. Dokümanın her `@d<a` örneği güncellenir;
  betik yazarlarına `MOD kural matematik` ve `g/d/r` sonekleri belgelenir.
- `ttr` ve iki-mesafe kesişimi **çok çözümlü**: seçim kuralı belgelenir ve testlenir; sessiz seçim yok.
- Poligon toleransları mevzuat verisidir: `package_version`, `source` (BÖHHBÜY madde), `published`; harita
  mühendisi onayıyla girer (6.11).
- `BİRLEŞTİR` boolean birleşimdir; `UÇUCA` gelince ikisi docs'ta yan yana anlatılır.
