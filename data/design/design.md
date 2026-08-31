# TERRACAD — Arayüz Tasarım Sistemi (v1.0)

Qt6 / QWidget tabanlı masaüstü CBS + CAD uygulaması. Bu doküman, `TERRACAD Masaüstü.dc.html` içindeki
4 ekranın (Ana Ekran, Stil Tasarımcısı, Öznitelik Tablosu, Ayarlar) tam görsel ve davranışsal
belirtimidir. Windows 11, macOS ve Linux (KDE/GNOME) üzerinde aynı görünmesi hedeflenir:
tüm çizim uygulama tarafından yapılır, yerel tema devre dışıdır (`Fusion` + tam QSS).

---

## 1. Tasarım ilkeleri

1. **Çizim alanı efendidir.** Kromun tamamı (menü + araç çubuğu + durum çubuğu) dikeyde 134 px'i
   geçmez. Renk, kontrast ve doygunluk yalnızca harita/çizim içeriğinde serbesttir; panellerde
   nötr gridir. Kullanıcının gözü ekranda hiçbir zaman arayüz tarafından çekilmez.
2. **Tek vurgu rengi.** Mavi (`--accent`) yalnızca üç şey demektir: *seçim*, *aktif araç*,
   *birincil eylem*. Turuncu (`--warn`) yalnızca *yakalama / düzenlenmiş veri / kilit* demektir.
   Bir ekranda bu ikisinden başka doygun renk arayüzde bulunmaz.
3. **Sayı = monospace.** Koordinat, alan, ölçek, tolerans, tarih, öznitelik değeri — hepsi
   IBM Plex Mono ve sağa hizalı. Kullanıcı rakamları okumaya değil, *karşılaştırmaya* gelir.
4. **Yoğun ama havalı.** Satır yüksekliği 26–30 px, dolgu 5–8 px. Yoğunluk bilgi için, boşluk
   gruplar arası ayrım için kullanılır. Dekoratif hiçbir öğe yok: gradyan yalnızca başlık
   çubuğunda 2 durak, gölge yalnızca yüzen pencerelerde.
5. **Her panel taşınabilir.** Hiçbir panel konumuna göre özel tasarlanmaz; her panel kendi
   başlığı, kendi minimum genişliği ve kendi kaydırma bağlamıyla bağımsızdır.

---

## 2. Renk jetonları

| Jeton | Değer | Kullanım |
|---|---|---|
| `--bg-app` | `#101215` | Uygulama dışı zemin, pencere arkası |
| `--bg-window` | `#1A1E22` | Pencere gövdesi, tablo tek satır |
| `--bg-panel` | `#1C2024` | Dock panelleri, araç kutusu |
| `--bg-raised` | `#1E2226` | Araç çubuğu, altlık çubukları, diyalog altlığı |
| `--bg-header` | `#20252A` | Tablo başlığı, grup başlığı |
| `--bg-titlebar` | `#262B30 → #20252A` | Başlık çubuğu (2 duraklı dikey gradyan) |
| `--bg-input` | `#171B1E` | Metin alanı, açılır liste, arama |
| `--bg-canvas` | `#23272B` | Çizim/harita tuvali |
| `--grid-minor` | `#2A2F34` | 20 px ızgara |
| `--grid-major` | `#31373D` | 100 px ızgara |
| `--line-hard` | `#14171A` | Bölgeler arası kesin ayraç (1 px) |
| `--line-soft` | `#22272B` | Satır ayracı |
| `--border` | `#363D43` | Girdi ve buton kenarı |
| `--text` | `#DFE5EA` | Birincil metin |
| `--text-dim` | `#8B949E` | Etiket, ikincil metin |
| `--text-faint` | `#6B747C` | Bölüm başlığı, ipucu, birim |
| `--accent` | `#2F9BD8` | Seçim, aktif araç, birincil buton |
| `--accent-hi` | `#6CC0EE` | Vurgu üzerindeki metin/ikon |
| `--accent-wash` | `rgba(47,155,216,.12)` | Seçili satır zemini |
| `--warn` | `#D98A2F` | Yakalama işaretçisi, düzenlenmiş hücre, kilit |
| `--warn-wash` | `rgba(217,138,47,.12)` | Kaydedilmemiş bir alanın zemini |
| `--ok` | `#4CAF7D` | Bağlantı durumu |
| `--danger` | `#D28484` | Yıkıcı eylemin ve geçersiz değerin mürekkebi |
| `--danger-edge` | `#874645` | Onun konturu, mürekkepten bir adım geri |
| `--danger-wash` | `rgba(210,132,132,.10)` | Geçersiz bir alanın zemini |
| `--accent-lift` | `#50ABDC` | Dolu bir vurgu yüzeyinin üst kenarı (birincil buton) |
| `--accent-edge` | `#3B799F` | Aktif ya da basılı bir denetimin konturu |

### Kabuk jetonları

Ana pencerenin kendi yüzeyleri. §7'nin bantları bunlarla boyanır; mockup'ta hepsi
harfi harfine geçer.

| Jeton | Değer | Kullanım |
|---|---|---|
| `--bg-shell-bar` | `#23282D → #1E2226` | Ana pencere başlık çubuğu (2 duraklı) |
| `--bg-strip` | `#191D21` | Sekme şeridi, durum çubuğu, panel başlığı — en koyu krom |
| `--bg-sunken` | `#1B1F23` | Komut satırı, tuval cetvelleri, zoom yığını |
| `--bg-tab-active` | `#22262A` | Seçili doküman sekmesi |
| `--window-edge` | `#2C3237` | Çerçevesiz pencerenin 1 px konturu |
| `--separator` | `#2E343A` | Araç çubuğu grupları arasındaki 1×22 çizgi |
| `--ruler-tick` | `#3A4147` | Cetveldeki bölme çizgisi |
| `--hover-chip` | `#2F353B` | İmlecin altındaki menü başlığı ya da çip |
| `--menu-text` | `#AEB6BD` | On menü başlığı, gövde metninden bir adım geri |
| `--dot` | `#3B4248` | Pencere düğmeleri; her platformda aynı çizilir |
| `--title-text` | `#767F87` | Başlık çubuğunun ortasındaki doküman adı |
| `--hint` | `#5F686F` | Arama alanındaki yer tutucu |
| `--hint-faint` | `#4C545B` | Yanındaki klavye kısayolu |
| `--readout` | `#D8DEE4` | Okunan ama düzenlenemeyen mono değer |
| `--readout-dim` | `#C4CCD3` | Aynısı bir adım geri: durum koordinatı, ölçek çubuğu |
| `--on-accent-dark` | `#0B1116` | Vurgu üzerine KOYU glif isteyen rozetin yazısı |

**Dördüncü anlam: DANGER.** §2'nin ilk dört anlamı — vurgu, uyarı, onay, sükûnet —
"bu geri alınamaz" ve "bu değer yanlış" cümlelerini söyleyemiyordu. `--danger`
yalnızca bu iki iş için vardır: yıkıcı bir eylemin konturu ve geçersiz bir alanın
konturu. Bir uyarı değildir; uyarı "dikkat et" der, bu "olmaz" der.

**İki vurgu kenarı.** Dolu bir vurgu yüzeyi (birincil buton) kendinden AÇIK bir üst
kenar taşır; aktif bir denetim (kip anahtarı, seçili segment) kendinden KOYU bir
kontur taşır. Tek jetonla ikisi yapılınca birincil buton düz, kip anahtarı basılı
görünüyordu.

**Seçili satır kalıbı** (tüm listelerde aynı): `background: --accent-wash` +
`box-shadow: inset 2px 0 0 --accent`. Hover: `#23282C`. Bu iki durum asla birbirine benzemez.

**Harita paleti** (arayüz paletinden ayrıdır, imar fonksiyonları): `#D8A45E` konut,
`#B06A58` ticaret, `#8F7BA8` sanayi, `#6F9A6A` yeşil alan, `#5F9BB8` eğitim, `#3F6F9E` sağlık,
`#A8B98A` tarım. Doygunlukları eşitlenmiş, koyu zeminde 4.5:1 üzerindedir.

---

## 3. Tipografi

- **IBM Plex Sans** — arayüz. 400 normal, 500 vurgulu satır/başlık, 600 pencere başlığı.
- **IBM Plex Mono** — tüm sayısal ve şema değerleri, komut satırı, cetvel, alan adları.

| Rol | Boyut / ağırlık / iz |
|---|---|
| Pencere ve diyalog başlığı | 13 px / 600 |
| Bölüm başlığı (sayfa içi) | 16 px / 600 |
| Menü ve sekme | 12.5 px / 400 |
| Panel başlığı | 11.5 px / 500 |
| Gövde, tablo hücresi | 11.5–12.5 px / 400 |
| Kolon başlığı, grup etiketi | 10–10.5 px / 600 / +0.7–1.2 px iz, BÜYÜK HARF |
| Cetvel, mikro etiket | 9–9.5 px / 400 mono |

Türkçe için `İ/ı/ş/ğ` render kontrolü zorunlu: büyük harfli etiketlerde `text-transform`
kullanılmaz, metin doğrudan büyük yazılır (Qt'de `toUpper()` locale hatası riski).

---

## 4. Ölçü ve ritim

Temel birim **2 px**, bileşen ritmi **4 px**.

| Öğe | Ölçü |
|---|---|
| Başlık çubuğu | 34–38 px |
| Araç çubuğu | 46 px (ikon 20 px, buton 30×30, boşluk 4 px) |
| Sol araç kutusu | 46 px genişlik (buton 32×32) |
| Doküman sekmesi | 30 px |
| Panel sekme başlığı | 29 px |
| Tablo başlığı / satırı | 30 / 26 px |
| Komut satırı | 28 px |
| Durum çubuğu | 26 px |
| Sağ dock varsayılan | 312 px (min 240, max 520) |
| Diyalog | Stil Tasarımcısı 1280×756 · Ayarlar 1180×740 |
| Köşe yarıçapı | Pencere 6–7 px · buton/girdi 3–4 px · başka hiçbir yerde yok |
| Gölge | Yalnızca yüzen pencere: `0 40px 90px -20px rgba(0,0,0,.8)` |

Dokunma/tıklama hedefi masaüstünde min 24×24 px, birincil butonlarda 32 px yükseklik.

---

## 5. İkon sistemi

Tek kaynak: **çizgi ikon seti, 20 px kutu, 1.6 px kontur, yuvarlatılmamış uç.** Referans
uygulamada Material Symbols Outlined (weight 400, FILL 0) kullanılır; üretimde aynı metriklerde
SVG seti gömülür (`QIcon` + `.svgz`, 1x/2x otomatik).

- Araç çubuğu ikonu 20 px, araç kutusu 19 px, panel başlığı 14 px, satır içi 13–15 px.
- Renk: pasif `--text-dim`, hover `#FFFFFF`, aktif `--accent-hi`.
- **Alt araç göstergesi:** araç kutusu butonunun sağ alt köşesinde 4 px'lik üçgen
  (`#7D868D`) — basılı tutunca yan açılır menü (Adobe kalıbı).
- Renk kuyusu: 22×22 px, 1 px açık kenar; ön/arka plan çifti araç kutusunun altında.

---

## 6. Docking sistemi

Qt `QMainWindow` + `QDockWidget` üzerine kurulur; görsel davranış tamamen özelleştirilir.

**Panel başlığı (dock title bar).** 29 px, sola dayalı 14 px ikon + 11.5 px ad, sağda
`drag_indicator` (tutamak), `remove` (daralt), `close_fullscreen` (yüzdür), `close`.
Aynı alana yerleşen iki panel üstte **sekme** olur (aktif sekme: `--bg-panel` zemin +
2 px üst `--accent` çizgi).

**Sürükleme geri bildirimi.**
- Sürükleme başlarken panel %70 opaklıkta bir hayalete dönüşür, imleç tutamağa kilitlenir.
- Geçerli hedef bölge `rgba(47,155,216,.14)` ile doldurulur, 1 px `--accent` kenarlıkla
  çerçevelenir; hedefte 4 yön + merkez (sekme olarak birleştir) rozeti gösterilir.
- Bölme çizgisi (splitter) 5 px, ortada 26×1 px tutamak izi; hover'da `--accent`.
- Serbest bırakılan panel `QDockWidget` olarak yüzer: kendi 7 px yarıçaplı gövdesi ve
  pencere gölgesi olur, başlık çubuğu aynı kalır.

**Kurallar.** Her panelin minimum genişliği 240 px, minimum yüksekliği 120 px'tir.
Tuval asla dock olamaz. Yerleşim `QSettings` içinde `saveState()/restoreState()` ile
profil başına saklanır; Görünüm ▸ Yerleşim menüsünden *Kadastro Üretim*, *Harita Kartografya*,
*Analiz* ön ayarları ve *Yerleşimi sıfırla* sunulur.

---

## 7. Ekran 1 — Ana Ekran

Dikey sıra: başlık çubuğu → araç çubuğu → gövde → durum çubuğu.

**Başlık çubuğu (34 px).** Solda pencere düğmeleri (macOS'ta yerel, Windows/Linux'ta aynı
ölçüde çizilir), ardından 10 menü: Dosya, Düzen, Görünüm, Çizim, Değiştir, Harita, Analiz,
Katman, Pencere, Yardım. Ortada doküman adı + sürüm. Sağda komut arama (`⌘K` / `Ctrl+K`) ve
kullanıcı baş harfi.

**Araç çubuğu (46 px).** 1 px ayraçlarla 7 grup: dosya · geri/yinele · pano · gezinme
(seç, kaydır, yakınlaş, tümünü göster) · yardımcılar (ızgara, yakalama, ölçüm) ·
pencereler (katman, stil, tablo) · çıktı. Sağ uçta iki salt-okunur okuma: **ÖLÇEK** `1 : 1 000`
ve **KOORDİNAT SİSTEMİ** `EPSG:5254 · ITRF96 / TM30`. Araç çubuğu taşarsa son grup
`»` taşma menüsüne girer, asla satır kırmaz.

**Sol araç kutusu (46 px).** 5 grup, toplam 20 araç:
seçim (nesne, alan, kaydır) · oluşturma (çizgi, polyline, poligon, dikdörtgen, daire/yay, nokta, metin) ·
düzenleme (böl/trim, birleştir, parsel böl, taşı/döndür, ofset) · ölçüm (uzunluk, alan, koordinat) ·
yardımcı (stil kopyala, topoloji denetimi). Aktif araç: `--accent-wash` zemin +
`inset 0 0 0 1px #3F7FA5`.

**Tuval.** Üstte doküman sekmeleri (30 px; aktif sekmede 2 px `--accent` üst çizgi ve
kapatma ikonu, sağ uçta bölünmüş görünüm düğmeleri). Tuvalin kendisi: 20 px + 100 px iki
katmanlı ızgara, üst ve sol kenarda 20 px cetvel (mono 8.5 px, 100 px'te bölme çizgisi).
Tuval üstü öğeler:
- **Seçili nesne:** `--accent` konturu + %14 dolgu + her köşede 9×9 px koyu tutamak.
- **Yakalama ipucu:** imlecin sağ altında koyu balon, turuncu `adjust` ikonu,
  `UÇ NOKTA · 458 214.362 , 4 512 908.771`.
- **Dinamik ölçüm:** kesikli `--accent` çizgi + `Δ 231.480 m` etiketi.
- **Kuzey oku** (74 px daire), **ölçek çubuğu** (180 px, 4 bölme, 0 / 100 / 200 m),
  sağ üstte 28 px zoom yığını (`+`, `−`, `fit`).
- İmleç: tuvalde tam ekran artı imleç (crosshair), 1 px, `#E8EEF3`.

**Sağ dock (312 px).** Üstte *Öznitelikler | Geçmiş* sekmeleri. Öznitelikler: seçili nesne
kartı (tip ikonu + ad + `POLYGON · fid 4128 · 4 köşe`), ardından katlanabilir gruplar —
KİMLİK, GEOMETRİ, MÜLKİYET, İMAR. Satır ızgarası `112px | 1fr`: solda alan adı, sağda mono
değer. Rozetler: `HESAP` (türetilmiş, salt okunur, mavi), `BOŞ` (eksik zorunlu alan, turuncu).
Altta 5 px splitter, sonra *Katmanlar* paneli (268 px): göz · renk örneği · ad · nesne sayısı ·
kilit; iç içe girinti 14 px (ada sınırları ve yapılar kadastro altında). Altlıkta
`9 katman · 2 düzenlenebilir`.

**Komut satırı (28 px).** `Komut:` + aktif komut (`_PARSELBOL`) + istem metni +
yanıp sönen `--accent` imleç. AutoCAD/Netcad kullanıcısı için klavye yolu birinci sınıftır:
her araç kutusu aracının bir komut adı vardır ve durum çubuğu ipucunda gösterilir.

**Durum çubuğu (26 px).** Solda mono `X / Y / Z` okuması; sonra tıklanabilir kip anahtarları
(IZGARA, YAKALAMA, DİK, POLAR, OSNAP, DİNAMİK GİRDİ, KALINLIK) — açık olan `--accent-wash`
zeminli; sağda veri kaynağı durumu (`cloud_done`, yeşil) ve `60 fps · 128 MB`.

---

## 8. Ekran 2 — Stil Tasarımcısı (Katman Özellikleri)

QGIS'in katman özellikleri diyalogunun mantığı, TERRACAD dilinde. 1280×756 modal,
tuval üzerinde ızgaralı zemin ve derin gölge ile yüzer.

**Sol dikey sekme şeridi (186 px).** Bilgi, Kaynak, **Simgeleyici**, Etiketler, 3B Görünüm,
Şeffaflık, Ölçek, Öznitelik Formu, Geçerlilik, Eylemler, Bağlantılar, Sürüm.
Aktif sekme: `inset 2px 0 0 --accent`.

**Üst simgeleyici şeridi.** Dört kontrol, her biri 10 px büyük harfli etiketiyle:
SİMGELEYİCİ (`Kategorize Edilmiş`), DEĞER (ifade alanı — mono, `function` ikonu ile
ifade düzenleyiciye geçiş), RENK SKALASI (gradyan önizlemeli açılır), SEMBOL BOYUT BİRİMİ
(segment: Milimetre / Harita birimi / Piksel).

**Kategori tablosu (sol, esnek).** Kolonlar `34 | 74 | 1fr | 1fr | 86`:
onay kutusu · sembol önizleme (58×17, %20 dolgu + 1.5 px kontur) · değer (mono) ·
gösterim adı (düzenlenebilir) · nesne sayısı. `‹diğer›` satırı gri ve en altta.
Altlık: **Sınıflandır** (birincil), Ekle, Sil, Tümünü sil, *Diğer değerleri birleştir*
onayı, `Gelişmiş ▾`.

**Sembol düzenleyici (sağ, 352 px).** Üstte 96×96 dama zeminli canlı önizleme ve
**Sembol Katmanları** listesi (Basit dolgu / Çizgi dolgu 45° / Basit kenar çizgisi) +
ekle, sil, çoğalt, yukarı, aşağı. Altında gruplanmış özellik listesi —
DOLGU (renk + opaklık, dolgu stili, karışım modu), KENAR (renk, kalınlık mm, çizgi stili,
birleşim stili), GEOMETRİ (ofset X/Y, döndürme), GÖRÜNÜRLÜK (ölçek aralığı, katman şeffaflığı).
Satır ızgarası `110px | 1fr | 22px`; üçüncü kolondaki `data_object` ikonu her özelliğin
**veriye bağlı geçersiz kılma** düğmesidir (ifadeyle sürülen özellik olduğunda `--accent-hi`
renge döner).

**Altlık (48 px).** Solda `Stil ▾` (kopyala/yapıştır/kaydet/yükle) ve
*Sembolü kütüphaneye kaydet*; sağda İptal · Uygula · **Tamam**. Uygula tuvali kapatmadan
canlı yeniler.

---

## 9. Ekran 3 — Öznitelik Tablosu (DataGrid)

Yüzer ya da alta dock edilebilir tam pencere. Başlıkta katman adı ve mono özet:
`1 482 nesne · 3 seçili · 1 düzenlendi`.

**Araç çubuğu (42 px).** Beş grup: düzenleme kipi (kalem, açıkken `--accent-wash`), kaydet,
geri/yinele · kayıt ekle/sil/kopyala · seçim (tümü, kaldır, tersle, seçiliye yakınlaş) ·
analiz (filtre, **alan hesaplayıcı**, istatistikler, kolon düzeni) · çıktı (dışa aktar, yazdır).
Sağda Tablo / Form segmenti.

**İfade çubuğu.** Tam genişlik mono girdi, sözdizimi renkli: alan `#8ECDF2`,
operatör `#E0A55E`, metin sabiti `#9DC78A`, mantıksal `#A48FD0`. Yanında **Filtrele**
(birincil), `Kaydet ▾` (adlandırılmış filtre), ve tablo içi arama.

**Izgara.** Kolon genişlikleri sabit ve sürüklenerek değişir; ilk kolon 46 px satır numarası
(sağa hizalı, `--text-faint`). Kural seti:
- Sayısal kolon → mono, sağa hizalı. Metin kolon → sans, sola hizalı, taşarsa `…`.
- Zebra: tek satır `#1D2125`, çift `#1A1E22`. Hover `#23282C`.
- Seçili satır: `--accent-wash` + `inset 2px 0 0 --accent`, metin `#EAF1F6`.
- **Kaydedilmemiş hücre:** turuncu metin, `rgba(217,138,47,.12)` zemin, 1 px turuncu iç kontur.
- NULL değer: `—`, `--text-faint`.
- Sıralanan kolon başlığı `--accent-hi` + yön oku; başlıklar mono 10.5 px.
- Satır yüksekliği 26 px; 1 482 satır `QAbstractTableModel` + `QTableView` ile sanal.

**Altlık (30 px).** Sayfalama (`1 – 18 / 1 482`), *Yalnızca seçiliyi göster*,
*Haritayla eşitle*, sağda toplu ölçüler: `Σ alan_m2 = 5 128 402.16 m²`, `x̄ = 3 460.46 m²`.

**Alan İstatistikleri paneli (268 px, dock edilebilir).** Seçili kolonun histogramı
(14 kova, mavi gradyan çubuk) ve 9 satırlık ölçü listesi — nesne sayısı, geçerli değer, NULL,
min, maks, ortalama, ortanca, standart sapma, toplam.

---

## 10. Ekran 4 — Ayarlar (Seçenekler)

1180×740 modal. İki kolon: kategori listesi (232 px) + içerik.

**Sol liste.** Üstte arama; 12 kategori: Genel, Görünüm ve Tema, **Çizim ve Yakalama**,
Koordinat Sistemleri, Veri Kaynakları, Etiketleme, Kısayollar, Plot ve Çıktı, Eklentiler,
Performans ve GPU, Klasörler ve Şablonlar, Ağ ve Kimlik. Varsayılandan sapan kategoride
sağda 6 px turuncu nokta. Altta etkin profil: `Kadastro Üretim`.

**İçerik.** Sayfa başlığı 16/600 + tek satır açıklama (maks 640 px, `text-wrap: pretty`).
Ayar satırı ızgarası `1fr | 250px`: solda ad + 11 px ipucu, sağda kontrol. Satırlar 1 px
`--line-soft` ile ayrılır; bölüm başlıkları 10.5 px büyük harf + esneyen ince çizgi.

Kontrol tipleri: **anahtar** (38×20 px, açık `--accent`, kapalı `#20252A`),
**sayı** (mono değer + gri birim: piksel, derece, m², basamak), **açılır liste** (250 px,
sağda `expand_more`), **renk** (34×13 örnek + hex + ad), **kısayol** (mono, `Ctrl + Shift + B`).

Örnek sayfa dört bölüm içerir: YAKALAMA (mod, tolerans, köşe/orta nokta/kesişim, işaretçi
rengi), DİNAMİK GİRDİ (girdi kutusu, açı adımı, koordinat gösterimi, ondalık hassasiyet),
TOPOLOJİ DENETİMİ (kaydetmede denetim, çakışma toleransı, ortak kenar düzenleme),
KISAYOLLAR (parsel böl, alan hesapla, stil tasarımcısı).

**Altlık (52 px).** Solda *Varsayılanlara dön*, *Profili dışa aktar*; sağda
İptal · Uygula · **Tamam**. Yeniden başlatma gerektiren ayar, satırın altında turuncu
`Yeniden başlatma gerekir` ipucu alır.

---

## 11. Etkileşim durumları

| Durum | Kural |
|---|---|
| Hover (ikon buton) | zemin `#2B3137`, ikon `#FFFFFF`, 90 ms |
| Hover (satır) | zemin `#23282C`, geçiş yok (liste taramasında titreme olmaması için) |
| Aktif / basılı | zemin `--accent-wash`, ikon `--accent-hi` |
| Odak (klavye) | 1 px `--accent` dış çerçeve + 2 px offset; fare tıklamasında gösterilmez |
| Devre dışı | opaklık %38, imleç `default` |
| Salt okunur değer | `--text-dim`, zemin yok, kopyalanabilir |
| Yükleniyor | başlıkta 2 px belirsiz `--accent` şerit; içerik yerinde kalır |
| Sürükleme | kaynak %70 opak, hedef `--accent` çerçeve + %14 dolgu |

Animasyon bütçesi: 90–140 ms, `ease-out`. Panel açılış/kapanışı ve tuval hiçbir zaman
animasyonlu değildir.

---

## 12. Qt6 uygulama notları

- `QApplication::setStyle("Fusion")` + tek `terracad.qss`. Yerel tema hiçbir platformda
  miras alınmaz; bu, Windows/macOS/Linux'ta birebir aynı sonucu verir.
- Renk jetonları `QPalette` + QSS değişken üretimi olarak tek kaynaktan (`tokens.json`)
  derlenir; açık tema aynı yapıdan ikinci bir eşleme ile üretilir.
- Tuval: `QGraphicsView` (OpenGL viewport) ya da `QOpenGLWidget`; ızgara, cetvel ve
  tutamaklar ekran uzayında çizilir, dolayısıyla zoom'dan bağımsız 1 px kalır.
  Yüksek DPI: `Qt::HighDpiScaleFactorRoundingPolicy::PassThrough`, ikonlar SVG.
- Panel yerleşimi: `QMainWindow::saveState()` → `QSettings` (profil başına anahtar).
- Tablo: `QTableView` + `QAbstractTableModel`, `setUniformRowHeights(true)`,
  sıralama/filtre `QSortFilterProxyModel`; 1 M satıra kadar akıcı kalması için hücre
  delegeleri hafif tutulur (`QStyledItemDelegate`, özel boyama yalnızca rozetlerde).
- Komut satırı: `QLineEdit` + kendi tamamlayıcısı; her `QAction` bir komut adına bağlanır,
  böylece klavye ve menü tek kaynaktan beslenir.
- Metin ölçekleme: 12.5 px temel yazı tipi `QFont::setPointSizeF` ile değil piksel olarak
  ayarlanır (platformlar arası tutarlılık), kullanıcı ölçeği %100–%150 arası ayardan gelir.

## 13. Erişilebilirlik

- Gövde metni koyu zeminde en az 7:1, ikincil metin 4.6:1 kontrast sağlar.
- Hiçbir bilgi yalnızca renkle verilmez: seçim ayrıca sol iç çizgi, düzenlenmiş hücre ayrıca
  iç kontur, kilit ayrıca ikon ile işaretlenir.
- Tüm araçlar klavyeden erişilebilir; `Tab` sırası panel → içerik → altlık düzenindedir.
- Yakalama ve ölçüm etiketleri en az 11 px mono; harita paleti renk körlüğü için
  doygunluk değil *açıklık* farkıyla ayrışacak şekilde seçilmiştir.

---

## 14. Ölçüm, doğrulama ve Qt'nin sınırları

Bu bölüm tasarımı değiştirmez; **uygulamanın referansla nasıl karşılaştırıldığını**
ve karşılaştırmanın nerede sınıra dayandığını kayda geçirir. Buradaki her sayı
`Screenshots/ana_ekran.png` üzerinden ölçülmüştür.

### 14.1 Referans ekran görüntüsü 1:1'dir

`ana_ekran.png` 1898 × 1080'dir ama ölçeklenmemiştir: pencerenin kendisi
**1880 × 1058** içerik kutusudur ve görüntünün içinde **(9, 7)** noktasından
başlar. Çevresindeki 1 px kenarlık `#30353A`, dışındaki alan pencere gölgesidir.
Doğrulama bu kutuyu kırparak yapılır.

### 14.2 Renkler yakalama sırasında açılmıştır — kaynak mockup'tır

Ekran görüntüsündeki koyu tonlar, kanal başına yaklaşık **+5…+7** açıktır:
tuval `#282B2F` görünür, oysa jeton `#23272B`'dir; panel `#222529` görünür, jeton
`#1C2024`'tür. Bu bir tasarım kararı değil, yakalama sırasındaki renk profili
dönüşümüdür.

Bu yüzden **geometri ekran görüntüsünden, renk `TERRACAD Masaüstü.dc.html`
mockup'ından** alınır. §2'deki 20 jetonun tamamı mockup'ta harfi harfine geçer;
`scripts/ci-gate-tokens.sh` bunu her yapıda doğrular.

### 14.3 Her bant `border-box`'tır

Bir bandın bildirilen yüksekliği **alt çizgisini içerir**:

| Bant | İçerik | Çizgi | Toplam |
|---|---|---|---|
| Başlık çubuğu | 33 px gradyan | 1 px `--line-hard` | **34** |
| Araç çubuğu | 45 px | 1 px | **46** |
| Doküman sekmeleri | 29 px | 1 px | **30** |
| Panel başlığı | 28 px | 1 px | **29** |
| Komut satırı | 27 px | 1 px (üstte) | **28** |
| Durum çubuğu | 25 px | 1 px (üstte) | **26** |

İçerik satırları, referansta: 0, 33, 34, 79, 80, 109, 110 … 1004, 1005, 1032, 1033.
Çizgiyi bildirilen yüksekliğin *üstüne* eklemek, altındaki her bandı bir piksel
aşağı kaydırır.

### 14.4 Yatay ölçüler

- Sol araç kutusu **46 px** (45 içerik + 1 px sağ çizgi); buton **32 × 32**,
  adım **34** (2 px boşluk), grup ayracı **24 × 1**, üstünde ve altında 7 px hava.
  Alttaki renk kutuları **22 × 22**, aralarında 3 px.
- Araç çubuğu butonu **30 × 30**, adım **34** (4 px boşluk); grup ayracı 1 px,
  iki yanında 5 px kenar boşluğu — yani iki grup arasındaki adım **49**. Çubuğun
  iki ucunda 8 px iç boşluk.
- Sağ dok **312 px**.
- Menü şeridi: `Dosya`'nın ilk mürekkebi x = **78**, `Yardım`'ın sonu x = **649**.

### 14.5 Qt'nin yarım pikseli yoktur

§3 yazı boyutlarının bir kısmı yarım pikseldir (12.5, 11.5, 10.5, 9.5). Tarayıcı
bunları gerçekten o boyutta çizer; **Qt çizemez** — `QFont::setPixelSize` tam sayı
alır ve `setPointSizeF` ile verilen kesirli boyut da FreeType tarafından tam
ppem'e yuvarlanır (96 DPI'da 12.5 px → 13 px, ölçüldü).

Sonuç, on menü başlığından oluşan şeritte **573 px'e karşı 572 px**'tir: on başlık
boyunca toplam **bir piksel**. Bu, Qt'nin metin çiziciyle ulaşılabilecek en yakın
değerdir; kapatmak için harf aralığını kurcalamak, her dizgede farklı bir hata
üretirdi. Kayıt altına alınmıştır, gizlenmemiştir.

### 14.6 Stil sayfasının ulaşamadığı ölçüler

Aşağıdakiler `QStyle::pixelMetric` ile verilir, çünkü QSS'in bunlara sözü geçmez
(`ShellStyle`, `theme.cpp`):

| Ölçü | Değer | Neden |
|---|---|---|
| `PM_DockWidgetTitleMargin` | 0 | Fusion 2 px ekler, 29 px panel başlığı 31 olur |
| `PM_DockWidgetFrameWidth` | 0 | aynı |
| `PM_DockWidgetSeparatorExtent` | 1 | §6'nın 1 px bölme çizgisi |
| `PM_MenuBarHMargin` / `PanelWidth` | 0 | ilk menü başlığını 6 px sağa iterdi |
| `PM_MenuBarItemSpacing` | 0 | başlıklar arası boşluk `::item` dolgusundan gelir |
| `SH_UnderlineShortcut` | Alt basılıyken 1 | referansta altçizgi yok; klavye yolu duruyor |

Ayrıca `QDockWidget::title` için **stil kuralı yazılmaz**: bir kural yazıldığı anda
Qt panel başlığının yüksekliğini widget'tan değil kuraldan hesaplar.

### 14.7 Doğrulama araçları

- `scripts/ci-gate-tokens.sh` — her jeton mockup'ta geçiyor mu, ve `kDark`/`kLight`
  alan sırası `tokens.hpp` ile aynı mı (`Tokens` konumsal ilklendirilir).
- `scripts/ci-gate-theme.sh` — Fusion zorunlu, tek stil sayfası, `tokens.cpp`
  dışında renk sabiti yok.

---

## 15. Girdi bileşenleri — tasarım standardı

Kaynak: `bileşen_standardı.png`. Bu bölüm **bütün formlar için bağlayıcıdır**;
yeni bir pencere buradaki türlerin dışına çıkmaz.

**Üç yükseklik:** 24 px (segment, çip), 30 px (girdi, buton), 36 px (büyük).
**Yarıçap:** 4 px. **Kenar:** 1 px `--border`.

### 15.1 Butonlar — hiyerarşi

> **Bir ekranda yalnızca tek birincil buton bulunur.** İki birincil taşıyan bir
> ekran, kullanıcıya hangisine basacağı hakkında hiçbir şey söylememiştir.

| Tür | Görünüm | Ne zaman |
|---|---|---|
| **Birincil** | `--accent` dolgu, 1 px `--accent-lift` kenar, `--on-accent` yazı | Diyalogu kapatan tek onay eylemi |
| **İkincil** | Saydam, 1 px `--border`, `--text-dim` yazı | Yıkıcı olmayan ikinci eylem; iptal, uygula |
| **Hayalet** | Kenarsız, zeminsiz, `--text-dim` | Düşük öncelik; araç çubuğu ve satır içi eylemler |
| **Yıkıcı** | 1 px `--danger-edge` kenar, `--danger` yazı, çöp ikonu | Geri alınamayan eylem. **Her zaman onay ister** |
| **Kip anahtarı** | Basılıyken `--accent-wash` zemin + `--accent-edge` kenar | Açık/kapalı durum taşır |
| **İkon** | 32 × 32, glif 16 px, yalnızca ipucu | Etiketi olmayan eylem |

Her tür için **devre dışı** hâli vardır: yazı `--text-faint`, kenar `--line-soft`,
zemin yok. Birincilin devre dışısı `--accent-wash` zemin taşır — dolu kalır ama
söner, çünkü yeri korunmalıdır.

### 15.2 Metin ve sayı girdileri — yedi durum

30 px, 4 px yarıçap, 1 px kenar. Her durum **farklı** bir şey söyler:

| Durum | Kenar | Zemin | Yazı | Anlamı |
|---|---|---|---|---|
| Varsayılan | `--border` | `--bg-input` | `--text` | Değer neyse o |
| **Odaklı** | `--accent` + 2 px halka | `--bg-input` | `--text` | İmleç burada |
| **Değiştirilmiş** | `--warn` | `--warn-wash` | `--warn` | Düzenlendi, kaydedilmedi |
| **Hatalı** | `--danger-edge` | `--danger-wash` | `--danger` | Değer kabul edilemez |
| Salt okunur | yok | saydam | `--text-dim` | Okunur, yazılamaz |
| Devre dışı | yok | saydam | `--text-faint` | Erişilebilir değil |
| **Türetilmiş** | `--accent-edge` | `--bg-input` | `--accent-hi` | Hesaplanır; düzenlemek anlamsız |

**`Değiştirilmiş` ile `hatalı` asla aynı renkte olmaz.** Warn "bunu sen
değiştirdin" der, danger "bu yanlış" der. Tek renkle yapılan bir pencere,
kullanıcıya kendi düzenlemesini geçersiz diye göstermiş olur.

Girdinin **solunda** bir ikon (birim tipi, `fx`, kilit), **sağında** birim eki
(`m`, `m²`, `kat`, `piksel`) durabilir; ikisi de `--text-faint`. Etiketin sağına
küçük bir rozet konur: `HESAP`, `ZORUNLU`, `SALT OKUNUR`, `KAYDEDİLMEDİ`.

Hatalı bir alanın **altında** tek satır neden yazar; alan adıyla aynı hizada,
`--danger`.

### 15.2b Renk alanı

Bir renk, formun bir **alanıdır**; yanına iliştirilmiş bir örnek değil. Bu yüzden
diğer girdilerle aynı genişlikte, aynı yükseklikte durur, değerin kendisiyle
dolar ve onaltılık karşılığı üstüne yazılır.

| | |
|---|---|
| Ölçü | Sütun genişliği × 22 px — komşusu olan açılır kutuyla aynı |
| Yüzü | Değerin kendisi. Kenarı, değerin `darker(140)` hâli |
| Yazısı | `#RRGGBB` (saydamsa `#AARRGGBB`), tek aralıklı, ortalanmış |
| Yazı rengi | Parlaklığa göre siyah ya da beyaz — on altı milyon zeminde de okunur |
| Dolgusuz | Kesikli kenar, boş yüz, ortada `dolgusuz`. Beyaz **değil**: beyaz bir plan renkidir |

Değer kullanıcının verisidir, temanın değil: jetondan gelmez ve gelemez. Yüzü
**boyanır**, biçim yaprağıyla verilmez — bu programda tek bir yaprak vardır (§2).

### 15.3 Seçim bileşenleri

| Bileşen | Ölçü | Durumlar |
|---|---|---|
| **Onay kutusu** | 14 × 14, yarıçap 3 | işaretli (tik), belirsiz (tire), boş, devre dışı |
| **Radyo** | 14 px daire | seçili (4 px `--accent` halka), boş, devre dışı |
| **Anahtar** | 38 × 20, yarıçap 10 | açık (`--accent`, topuz sağda), kapalı (topuz solda), devre dışı |
| **Segment** | 24 px, bitişik hücreler | seçili hücre `--accent-wash` + `--accent-edge` |
| **Kaydırıcı** | 4 px yol, 12 px topuz | dolu kısım `--accent` |
| **Etiket çipi** | 22 px, yarıçap 11 | `--warn` konturlu; taşanlar `+N` çipinde toplanır |

Hiçbiri durumunu **yalnız renkle** söylemez (§13): onay kutusunda tik ya da tire,
radyoda nokta, anahtarda topuzun tarafı — her birinde bir **şekil** vardır.

---

## 16. Form yerleşimi — standart

Kaynak: `form_örnek.png`, `form_örnek_2.png`. Bir kaydın tek sayfada düzenlendiği
her pencere bu yerleşimi kullanır.

### 16.1 Izgara

**Dört sütun, her biri 312 px, aralarında 26 px oluk.** Bir alan bir sütun kaplar;
uzun bir metin alanı iki sütuna yayılır (650 px). Girdi 30 px, satır adımı 63 px —
yani etiket, 4 px boşluk, girdi, 29 px nefes.

Alan etiketi girdinin **üstündedir**, solunda değil: dört sütunlu bir ızgarada sol
etiket, kullanılabilir genişliğin yarısını yer.

Zorunlu alan adının sonuna `*` konur. Etiketin sağ ucuna, satırın sağına dayalı,
küçük bir rozet: `HESAP`, `SALT OKUNUR`.

### 16.2 Bölümler

Her grup bir **başlık** taşır (`--text-faint`, 10.5 px, büyük harf, izli) ve
başlığın sağ ucunda o grubun **kaynağını** yazan tek satırlık soluk bir not:
`TAKBİS'ten çekildi · 14.03.2019`, `Türetilmiş alanlar geometriden hesaplanır`,
`1/1000 Uygulama İmar Planı · Rev. 2024/3`. Bu not bir süs değildir: bir alanın
nereden geldiği, değerinin kendisi kadar bilgidir.

### 16.3 Pencerenin parçaları

| Yer | Ne |
|---|---|
| Başlık | Simge · ad · `fid` (mono, soluk) · değişiklik rozeti (`✎ 3 ALAN DEĞİŞTİ`) |
| Araç satırı | Düzenle · kaydet · geri/yinele │ ekle · çoğalt · sil │ … Sağda: kayıt sayfalayıcı ve `Tablo \| Form` |
| Sol kenar | **FORM BÖLÜMLERİ**, 35 px satır; sağ ucunda sayı ya da ⚠ rozeti. Altta **TAMAMLANMA %78** çubuğu ve `2 zorunlu alan eksik` |
| Sekme satırı | Genel · Mülkiyet · İmar · Yapı(•) · Ekler |
| Kayıt kartı | Tip ikonu · ad · tek satır tanım. Sağda üç okuma: ALAN · ÇEVRE · DURUM |
| Alt tablo | Kendi başlığı, `+ Satır ekle`, satır başına silme işareti, **Toplam** satırı |
| Uyarı şeridi | ⚠ · `Geçerlilik denetimi: 2 uyarı` · açıklama · `Ayrıntı` |
| Altlık | `Son değişiklik: … · 3 dk önce` … `Değişiklikleri geri al` · `İptal` · **`Kaydet`** |
| Sağ panel | **Denetim ve Değişiklikler**: bekleyen değişiklikler (eski → yeni), geçerlilik kuralları (✓ / ⓘ / ⚠), kayıt geçmişi |

### 16.4 Bekleyen değişiklik nasıl gösterilir

`tapu_alani` · ~~`3 482.64`~~ → `3 480.00`

Eski değer **üstü çizili** ve soluk, yeni değer `--warn`. Kullanıcı neyi
değiştirdiğini kaydetmeden önce görebilmelidir; "3 alan değişti" tek başına bir
bilgi değildir.

### 16.5 Geçerlilik kuralları üç ayrı şey söyler

| İşaret | Renk | Anlamı |
|---|---|---|
| ✓ | `--ok` | Kural sağlanıyor |
| ⓘ | `--danger` | Kural **ihlal edildi**; kayıt bu hâliyle geçersiz |
| ⚠ | `--warn` | Şüpheli ama engelleyici değil |

Üçü de metinle birlikte yazılır; işaret tek başına anlam taşımaz (§13).
