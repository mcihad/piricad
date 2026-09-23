# Değişiklik Günlüğü

Bu proje [Semantik Sürümleme](https://semver.org/lang/tr/) kullanır.
Mevzuat kataloğu değişiklikleri, sebebi olan yönetmelik veya genelge adıyla
birlikte kaydedilir (CLAUDE.md Article 9).

## [Yayımlanmamış]

### Düzeltildi — geometri düzenlemesi öznitelikleri siliyordu

- **Taşınan, köşesi sürüklenen, budanan ya da uzatılan bir nesne bütün özniteliklerini
  kaybediyordu.** Öznitelik hücreleri geometri yuvasına göre tutuluyor ve her geometri
  düzenlemesi nesneye yeni bir yuva veriyor; yazı yeni yuvaya taşınıyordu, öznitelikler
  taşınmıyordu. Artık öznitelik satırı da taşınıyor; geri alma eski değerleri eski
  geometriyle birlikte geri getiriyor (`C-05: öznitelik bir geometri düzenlemesinde
  kaybolmaz` testi).

### Eklendi — UÇUCA yay zincirlerini birleştirir; kurallar ve boşluk açık (C-05)

- **`UÇUCA` yayları da ekliyor ve düzleştirmiyor**: çizgi + yay yaylı çoklu çizgi,
  aynı çemberin uç uca iki yayı tek yay olur; uzunluk korunur.
- **Hiçbir şey sessizce oynamıyor**: tolerans içindeki boşluk bir doğru parçasıyla
  kapatılıyor ve söyleniyor ("1 boşluk doğru parçasıyla kapatıldı; en büyüğü 4 mm
  (tolerans 10 mm)"); eskiden eklenen parçanın ilk köşesi atılıp zincire çekiliyordu.
- **Kurallar**: sonuç ilk nesnenin yönünde; katman, stil ve öznitelikler ilk
  nesneden, farklı olanlar adıyla söyleniyor; `cakisma=reddet` fark varsa hiç
  birleştirmiyor. Kapalı şekil eklenmiyor; uçları buluşan zincir söyleniyor.
  Kaynak→sonuç kimlik eşlemesi raporlanıyor.
- **`ÇİZGİDÜZENLE` yaylı çoklu çizgide**: `kapat`, `ac`, `ters` yayları koruyarak
  çalışıyor; `sadelestir` gerekçesiyle reddediliyor.
- Destek matrisi 152 → 154; `PROOF: UÇUCA çizgi ve yayı yaylı çoklu çizgi yapar`.

### Eklendi — KIR yayda, dairede ve yaylı çoklu çizgide (C-05)

- **`KIR` artık yalnız çizgide değil**: yaydan çıkan parça yaydır, kalanlar yaydır;
  bir daireden birinci noktadan ikinciye saat yönünün tersine giden parça çıkar ve
  geriye bir yay kalır; yaylı sınırın yayları düzleşmez. Kalan parçalar katmanı,
  stili ve öznitelikleri taşıyor; önizleme gidecek parçayı nesne boyunca çiziyor ve
  uzunluğunu nesne boyunca yazıyor. Alan kırılmıyor, nasıl açılacağı söyleniyor.
  Tek önizleme ve komut hesabı: `core::break_path` (eski `break_run` kalktı).
  Destek matrisi 149 → 152; `14q-kir-yay` karesi.

### Eklendi — BÖL her eğride ve beş yoldan böler; yaylar yay kalır (C-05)

- **`BÖL` artık yay, daire ve yaylı çoklu çizgide de çalışıyor**; her parça kendi
  türünde kalıyor: yaydan yaylar, daireden yaylar, yaylı sınırdan yaylı parçalar —
  yaylar düzleşmiyor ve parçaların uzunlukları toplamı kaynağınki.
- **Beş yöntem:** kesme çizgisi (`cizgi`, artık eğrileri her kesişiminde böler),
  nesnenin üstündeki noktalar (`nokta`; parçalar Enter'dan önce sırayla iki renkte
  görünür, ⌫ son noktayı geri alır), seçilenlerin birbirini kestiği yerler
  (`kesisim`), baştan uzaklık (`mesafe`) ve eşit parçalar (`esit`). Dördü araç
  kutusunda Buda ailesinde ve Değiştir menüsünde kendi araçlarıyla.
- **Her parça katmanı, stili ve bütün öznitelikleri taşıyor** (eskiden ikinci yarı
  öznitelikleri kaybediyordu); yapılandırılmış cevap her kaynak için hangi
  kimliklere dönüştüğünü söylüyor (`{"kaynak": 1, "sonuc": [1, 2, 3]}`).
- Kanıt: `tests/unit/test_split_join.cpp`, `test_curve_path.cpp` (yaylı çoklu çizgi
  yolu, ters yol, bölme uzunlukları, kayıt biçimi), `PROOF: BÖL yayı noktalarından`;
  destek matrisi 145 → 149 desteklenen hücre; `14p-bol-noktalardan` karesi.

### Düzeltildi — kılavuz tıklamanın çizeceği nesne; yöntem araç tekrarında korunuyor (C-02)

- **Hayalet, nesnenin kendisi.** Daire, yay, elips, çokgen, dikdörtgen, spline, çizgi
  ve alan kılavuzları artık tek bir hesaptan (`command::ghost_outline`) ve nesnenin
  belgede çizildiği sıklıkta çiziliyor; her yöntemde tıklamanın yazdığı nesnenin
  kılavuzla nokta nokta aynı olduğu her derlemede sınanıyor. Tuvaldeki beş ayrı
  yarıçap hesabı, elipsin ikinci ekseni, çokgenin dönüşü ve spline'ın örnekleme
  sabiti komutlarla ortak çekirdek fonksiyonlarına taşındı.
- **ÇOKGEN `aci=` verilince kılavuz dönmüyor.** Kılavuz imlece dönüyor, tıklama ise
  çokgeni `aci`'de çiziyordu; artık kılavuz da o açıda durur, fare yalnız boyunu
  değiştirir. İşaretlenen açı kaydedildiği gibi mikro-dereceye yuvarlanıp köşeler
  ondan hesaplanıyor; çizim ile günlükten oynatması milimetresine kadar aynı.
- **YAY `bma`'nın süpürmesi gösterilebiliyor.** Süpürme istemi eskiden ekranda hiçbir
  şey göstermeyen bir sayıydı; artık yay imlecin doğrultusuna kadar süpürülür, açı
  oturumun biriminde yanında yazar ve tıklama o açıyı verir.
- **Sabitlenen başvurular ekranda kalıyor.** DAİRE `ttr`'de birinci doğru ikinci
  çizilirken, iki doğru da yarıçap yazılırken görünüyor; YAY `bby`'de yarıçap
  yazılırken kiriş görünüyor. Kısmi elipsin kılavuzu bütün elipsi değil çizilecek
  parçayı gösteriyor.
- **Yazılan yöntem araç tekrarında korunuyor.** Komut satırına `DAİRE yontem=3n`
  yazıldığında bir sonraki daire merkez-çevre dairesine dönüyordu; artık yöntem ve
  ayarlar (`yaricap=`, `kenar_sayisi=`, `aci=`, `yon=` …) korunuyor, noktalar
  korunmuyor (`command::rearm_line`).
- Kanıt: `tests/unit/test_draw_methods.cpp` — 22 yöntemde hayalet = nesne (başsız ve
  görünümlü), işaretlenen = yazılan = oynatılan; `KENTOS_SHOT_DIR` kareleri
  `14l-yay-bma-supurme`, `14m-daire-ttr-yaricap`, `14n-daire-ttr-yon`,
  `14o-cokgen-aci`; gerçek pencere probunda yazılan yöntemin yeniden kurulması.

### Düzeltildi — çizerken yanlış son nokta çizimi kaybettirmeden geri alınıyor (C-02)

- **⌫, Ctrl+Z ve `G` yalnız son noktayı geri alıyor.** `ÇİZGİ`, `ÇOKLUÇİZGİ`, `ALAN` ve
  `SPLINE` nokta beklerken ⌫ (Backspace), Ctrl+Z, **Geri Al** düğmesi ya da komut satırına
  yazılan `G`/`GERİ`/`U` son noktayı geri alıyor; çizimin geri kalanı tuvalde kalıyor ve
  komut o noktayı yeniden istiyor. Eskiden ⌫ çalışmayı yazıp `SİL`'i başlatıyor, Ctrl+Z ve
  `U` ise çalışmayı yazıp **tamamını** geri alıyordu. Geri alınan nokta günlüğe yazılmıyor;
  arayüzden, komut satırından ve betikten aynı çizim ve bayt bayt aynı günlük satırı
  (`PROOF: geri alınan köşe hiçbir yolda kalmaz`).
- **`ÇİZGİ` çalışmayı bitince yazıyor.** Parçalar tıkladıkça tuvalde görünüyor, çalışma
  Enter, sağ tık ya da Esc ile bitince her biri ayrı nesne olarak yazılıyor —
  `ÇOKLUÇİZGİ` gibi. Böylece geri alınan bir parça kalıcı bir anahtar harcamıyor ve günlük
  aynı anahtarlarla yeniden oynuyor.
- **Çalışmanın kendi noktaları yakalanıyor.** Henüz belgede olmayan köşeler uç nokta,
  aralarındaki parçalar orta/yakın/dik/kesişim olarak yakalanıyor; bir sınır başladığı
  köşede kapatılabiliyor. Komutla birlikte yazılan noktalar (`ÇOKLUÇİZGİ 0,0 20,0 20,12`)
  çalışmanın kendi köşelerine çekilmiyor.
- **Tuvaldeki yakalama işareti tıklamanın gideceği noktayı gösteriyor.** Önizleme izleme
  işaretlerini hiç hesaba katmıyordu; artık komutun kullandığı işaretlerle ve çalışmayla
  aynı çağrıyı yapıyor.
- **Komut satırındayken Ctrl+Z çizimi geri alıyor.** Satır boşken Ctrl+Z satırın kendi
  metin geçmişine gidip son komutun metnini geri getiriyordu; artık `GERİAL`'e ulaşıyor.

### Düzeltildi — yapay zekanın onay ve soru ayarları uçtan uca çalışıyor (A-03)

- **Otomatik uygulama doğruyu söylüyor.** Onay politikası `otomatik` iken uygulanan bir
  öneri için sohbetteki model artık "Uygulanmadı" yerine `UYGULANDI` yanıtını alıyor ve
  işin sonraki adımına onay beklemeden geçiyor; MCP yanıtında `durum: uygulandi` ve
  `_meta` `cad.kentos/approval: policy-applied`; döküm satırı ve kart da "onay
  politikanızla uygulandı" diyor. Denetim kaydında `karar_veren: politika:otomatik` —
  otomatik bir uygulama artık insan tıklaması gibi yazılmıyor.
- **MCP önerileri kişinin önüne geliyor.** Bir MCP istemcisinin önerisi için programda
  hiç kart yoktu; öntanımlı politikada kimse onaylayamıyordu. Artık Yapay Zeka panelinde
  istemcinin adıyla ve Reddet/Uygula düğmeli kartla görünüyor; panel kapalıysa yerine
  oturtularak açılıyor.
- **Çok adımlı MCP işi kırılmıyor.** `_meta.plan` ile uzatılmak istenen öneri zaten
  uygulanmışsa yeni adım yeni bir öneri oluyor (eskiden ret dönüyordu).
- **Soru politikası modele söyleniyor.** Onay, soru ve üzerine yazma kuralları sohbetin
  sistem metnine ve MCP `server/discover` yanıtına aynı sözlerle ekleniyor
  (`ai::policy_rules`); yazan her aracın şemasında isteğe bağlı `varsayimlar` alanı var ve
  bildirilen varsayımlar kartta, yanıtta, dökümde ve denetim kaydında görünüyor.
- **Üzerine yazma algılanıyor.** Yerleşim şablonu kaydı ve PDF çıktısı var olan bir
  dosyaya denk gelirse `sor` onay istiyor (otomatik modda bile), `yeni_ad_uret` ` (2)`
  ekli bir ad veriyor ve söylüyor.
- **Kapsam dışı iş kart açmıyor**, sebebiyle reddediliyor; bekleyen önerinin kartı neden
  beklediğini söylüyor.
- **Ayarlar penceresi okunur seçenekler gösteriyor** ("Her değişiklikte onay iste" …);
  değer adları `TERCİH` ve günlükte aynı kalıyor (`SettingSpec::labels`).
- Sohbet paneli, yerleşim kaydı onu yüzen bırakmışsa her açılışta yerine oturuyor.
- Bir istemcinin kendi yetkisini genişletme denemesi denetim kaydına `karar: yetki_reddi`
  olarak yazılıyor.

### Eklendi — BUDA ve UZAT çitle, tıklananı tutarak ve sınırı uzatarak (C-04 tamam)

- **Çitle** (`yontem=çit`, `cit=`): çizilen bir çitin geçtiği her parça tek seferde
  budanıyor, ya da çite yakın her uç uzatılıyor. Bütün iş Enter'dan önce tuvalde
  görünüyor (`core::plan_fence` — komutun uyguladığı plan); tek geri alma adımı.
  Kesilemeyen ya da budanamayan nesneler atlanıp sayılıyor.
- **Tıklanan kalsın** (`tut=evet`): tıklanan parça kalıyor, iki yanındaki kesimlerin
  dışı gidiyor.
- **Sınırları uzatarak** (`uzanti=evet`): nesneye yetişmeyen bir sınır kendi yolunda
  — çizgi doğrultusunda, yay çemberi boyunca — uzatılmış sayılıyor; önizleme uzatılan
  bölümü noktalı çiziyor.
- **Adaylar:** önizleme sınırların nesneyi kestiği her yeri işaretliyor — kesen `×`,
  teğet halka — ve imlecin yanında atılacak uzunluğu, kesişim ve teğet sayısını
  yazıyor.
- Hepsi araç kutusunda **Buda** ailesinin kartında ve **Değiştir** menüsünde:
  Buda — çitle, Buda — tıklanan kalsın, Buda — sınırları uzatarak, Uzat — çitle,
  Uzat — sınırları uzatarak.
- Düzeltme: çizginin tam ucuna tıklamak uçtaki parçayı gösteriyor (destek matrisi bunu
  yakaladı).

### Düzeltildi — pencere dizüstü ekranına sığıyor; araç kutusu gerekirse ikinci sütuna geçiyor

Araç kutusu dikey bir kutu yerleşimiydi ve tam yüksekliği pencerenin **asgari**
yüksekliği oluyordu: pencere 1131 pikselden kısa olamıyordu. 900 piksellik bir ekranda
pencerenin altı — alttaki araçlar, Python paneli, durum çubuğu — ekranın dışında
kalıyor, görünmüyordu. Kutu artık araçlarını kendisi yerleştiriyor ve gövde kısa
kaldığında 45 piksellik ikinci (üçüncü…) bir sütuna devam ediyor; gruplar sığdıkça bütün
kalıyor. Pencerenin asgarisi 607 × 487'ye indi; 1280 × 720 ve 1440 × 860'ta, Python
paneli açıkken de, pencere ekrana sığıyor ve hiçbir araç gizlenmiyor. Python panelinin
asgarisi 140 piksele indi, açılış yüksekliği pencerenin üçte birini geçmiyor. Yeni
`window-fits` testi (`KENTOS_FIT_PROBE`) bunu her derlemede ölçüyor.

### Değişti — BUDA ve UZAT yaylarda, dairelerde ve çok sınırla çalışıyor (C-04)

- **Tıklanan parça gidiyor:** iki sınırın arasındaki orta parça da artık atılabiliyor;
  nesne ikiye ayrılıyor, ikinci parça aynı katman, stil ve özniteliklerle yeni nesne
  oluyor.
- **Eğriler:** yay bir çizgiyle budanınca yay olarak kalıyor; daire iki yerinden
  kesilince kalan bölüm yay oluyor; bir çizgi yaya ya da daireye eğrinin gerçek
  kesişiminde duruyor, kirişte değil. `UZAT` bir yayın ucunu kendi çemberi boyunca
  taşıyor. Kesişimler kapalı biçimde hesaplanıyor (`core::path_crossings`,
  `core::trim_curve`, `core::extend_curve`).
- **Sınırlar:** `sinir=` ile birden çok sınır; yoksa seçim; o da yoksa **hızlı
  budama** — tıklanan nesnenin yakınındaki her nesne keser (`hepsi=evet`).
- **Arayüz:** düğmeye basınca doğrudan parçaya tıklanıyor; art arda tıklamalar tek
  çalışmada ve **tek geri alma adımında** toplanıyor, Enter ya da Esc bitiriyor.
  Önizleme imlecin altındaki nesnenin atılacak parçasını (ya da eklenecek uzantısını)
  komutun kullandığı hesapla çiziyor. Tıklama bir seçim olduğu için yakalama ve ızgara
  onu kaydırmıyor.
- **Betik:** `nokta` birden çok nokta alıyor, `nesne` her noktanın nesnesini adlıyor;
  eski `BUDA nesne=1 sinir=2 nokta=…` satırları aynı sonucu veriyor.
- Kapalı alanın parçası hâlâ budanmıyor (`BÖL`); elips ve spline C-01'in yinelemeli
  çözümüyle gelecek.

### Değişti — ESNET, KIR, BUDA/UZAT ve HİZALA sonucu önceden gösteriyor

- **ESNET:** başlangıç noktası sorulurken pencere tuvalde kalıyor; bitiş noktasında
  pencerenin yakaladığı her nesne esnetilmiş hâliyle imleci izliyor. Önizleme komutun
  kullandığı çekirdek hesabın kendisi (`core::stretch_entity`); tutamaklar artık
  belgeye dokunmadan sırayla taşınıyor (`core::move_grips`).
- **KIR:** ikinci nokta aranırken gidecek parça kırmızı ve kesikli işaretleniyor.
- **BUDA / UZAT:** imleç çizginin üzerindeyken atılacak parça (ya da eklenecek
  uzantı) çiziliyor; iki çizgi seçiliyse hangisinin düzenleneceği imleçle birlikte
  değişiyor.
- **HİZALA:** ikinci nokta çifti artık arayüzden de soruluyor (Enter: yalnız taşı);
  nesneler ilk hedefte imleçle taşınıyor, ikinci hedefte imleçle dönüyor. **Hizala —
  ölçekleyerek** aracı `olcekle=evet`'i fareye açıyor.
- **TAŞI, KOPYALA, DÖNDÜR, ÖLÇEKLE, AYNALA:** hayalet artık komutun çözdüğü
  nesneleri gösteriyor; `TAŞI nesneler=5` gibi adıyla verilen nesnelerin de hayaleti
  çıkıyor (önce yalnız seçim çiziliyordu).

### Düzeltildi — dik mod dikdörtgen köşesini ve ölçülmeyen noktaları kilitlemiyor

Dik mod açıkken bir dikdörtgenin karşı köşesi eksene kilitleniyor, pencere bir çizgiye
dönüşüyordu: seçim penceresi, `DİKDÖRTGEN` ve `ESNET` penceresi hiç çizilemiyordu.
Önizlemesi olup bir önceki noktaya göre verilmeyen tıklamalar (BUDA'nın seçimi,
ESNET'in başlangıcı) da yanlışlıkla bir köşeden ya da çizimin sıfırından geçen bir
ışına kilitlenebiliyordu. Artık bu noktalara yön kilidi uygulanmıyor; tuvaldeki
yakalama işareti de aynı kuralı izliyor.

### Eklendi — OFSET öznitelikleri paralele aktarıyor (C-03 tamam)

[`OFSET`](docs/komutlar/offset.md) kaynağın öznitelik değerlerini paralele yazıyor —
`KOPYALA` ve `BÖL`'ün yaptığı gibi: bir yol ekseninin bordür çizgileri yolun adını
taşıyor. Paralel kaynağın kendisi değilse (parselin içine çizilen çekme hattı gibi)
`oznitelik=aktarma` onu boş bırakıyor; aynı ada/parsel numarasını taşıyan iki satır
oluşmuyor. Böylece C-03'ün bütün maddeleri — tek yanlı paralel, kapalı sınır ofseti,
ayrı tampon, taraf, mesafe, köşe, kaynak koruma ve öznitelik aktarımı — tanımlı ve
testli.

### Eklendi — RENK: renk kutuları boyuyor

Araç kutusunun altındaki iki renk kutusu "RENK komutu Faz 2" diyordu. Artık
[`RENK`](docs/komutlar/colour.md) (`core.colour`) seçili nesnelerin çizgi ve dolgu
rengini değiştiriyor: `#RRGGBB`, `#AARRGGBB`, dokuz renk adı (`kırmızı`, `mavi`, …),
`katman` (katmanın rengine dönüş) ve dolgu için `yok`. Kutular seçili nesnenin — seçim
yoksa etkin katmanın — renklerini gösteriyor; tıklamak renk örnekleri, **Başka bir
renk…**, **Katmanın rengi** ve **Dolgu yok** içeren bir menü açıyor. Seçim yoksa
komut boyanacak nesneleri soruyor. Dolgu, bir dolgu katmanıyla boyanıyor (düz bir
çizginin dolgu değeri çizilmiyordu); çok katmanlı sembollerde rengi kilitli katmanlar
korunuyor; özellikleri yeniden katmanınkiyle aynı olan nesne katmanını izlemeye
dönüyor. Komut **Değiştir → Renk** menüsünde ve Stil Kopyala ailesinde de var.

### Düzeltildi — YUVARLA dikdörtgen ve alan köşesinde çalışıyor

Kapalı bir şeklin (dikdörtgen, alan, kapalı çizgi) köşesi "Kapalı bir alanın köşesi
yuvarlatılamaz" diye reddediliyordu; fareyle en çok yuvarlatılan köşe buydu. Artık köşe
yerinde yuvarlatılıyor: şekil aynı nesne kalıyor (kimlik, öznitelik, bağlı yazılar),
alan olmaya devam ediyor ve yay, YAY'ın çizildiği noktalarla sınıra işleniyor; komut
gerçek yaydan en çok kaç mm saptığını söylüyor. Önizleme imlecin yanında
`yarıçap 5 m` yazıyor.

### Düzeltildi — İZ işaretleri artık kalıcı değil

İki izleme işareti bir kez konunca oturum boyunca, `YENİ`'den sonra bile, bütün
çizimlerin üstünde izlerini çiziyordu. İşaretler artık hizmet ettikleri komut
bitince siliniyor; hiçbir komut çalışmıyorken konanlar sıradaki komutla gidiyor ve
boşta Esc onları da siliyor.

### Düzeltildi — komut satırındaki seçenekler liste olarak geliyor

Seçenek sunan bir istemde (renk, katman adı, …) ilk seçenek satıra seçili yazılıyor,
soruyu gizliyor ve Enter'a basan kullanıcı vermediği bir cevabı veriyordu. Artık
seçenekler komutun sırasıyla, temaya uygun bir listede duruyor; bir seçeneğe tıklamak
cevap, yazmak listeyi daraltıyor; renk adlarının yanında renk örneği var. Liste
kapanınca klavye yeniden komut satırında. Böyle bir soruda tuvale tıklamak da artık boş
bir cevap sayılmıyor: soru açık kalıyor ve cevabın yazılması ya da seçilmesi isteniyor.

### Eklendi — TAMPON: iki yanlı GIS tamponu, ayrı bir araç olarak

[`TAMPON`](docs/komutlar/tampon.md) (`islem.tampon`, Araçlar ▸ Analiz ▸ Tampon bölge)
nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çiziyor: çizginin iki yanı,
noktanın çevresinde disk, alanın dışı (delikleri korunarak), eğrilerin çizildiği hâlinin
çevresi. Üst üste binen tamponlar tek alan oluyor (`birlestir=hayir` her birini ayrı
tutuyor); eksi mesafe alanları aşındırıyor. OFSET'in eskiden "paralel" diye çizdiği bant
buydu; artık ikisi ayrı araç. İşlem araçları eğrileri çizildiği hâliyle okuyabiliyor
(`InputEntity::drawn`) ve delikli alan üretebiliyor (`ToolOutput::faces`).

### Değişti — OFSET gerçek paralel çiziyor, tarafı imleçle gösteriliyor

OFSET açık bir çizginin iki yanını saran kapalı bir alan (bant) üretiyordu; o bir
paralel değil tampondu. Artık paralel kaynağıyla aynı türde:

- Açık çizginin paraleli **tek yanda açık bir çizgi**: `(0,0)→(10,0)` çizgisinin sol
  2 m paraleli `(0,2)→(10,2)`. Sol ve sağ çizim yönüne göredir.
- Alan delikleriyle alan, daire yarıçapı değişmiş daire, yay aynı merkezli yay
  olarak çıkıyor. Elips, spline ve yaylı çoklu çizginin paraleli çizildiği hâliyle
  alınıp çoklu çizgi oluyor ve komut gerçek eğriden ne kadar saptığını yazıyor.
- **Taraf gösteriliyor:** mesafe yazıldıktan sonra paralel imlecin olduğu yanda
  vurgulu çiziliyor, tıklama o yanı seçiyor. Betikte `taraf=sol|sag|dis|ic|iki`,
  `nokta=` ya da (tam bir satırda) mesafenin işareti.
- Paralel kaynağın katmanına ve stiline çiziliyor (`ozellik=aktif` ile etkin katmana);
  `kaynak=sil` kaynağı siliyor. Çöken sonuç nesne nesne söyleniyor.
- Nokta, yazı, ölçü, lider, tarama ve blok referansı artık sebebiyle reddediliyor.

### Değişti — ölçüm araçları: sürekli ölçüm, köşelerden alan, tuvalde kalan sonuç

- **ÖLÇ noktadan noktaya ölçüyor.** İkinci noktada durmuyor; her yeni nokta bir kenar
  ekliyor ve o kenarın uzunluğunu, açısını ve **toplamı** yazıyor. İmleç hareket
  ettikçe biten her kenarın üstünde uzunluğu, imlece giden kenarın yanında uzunluğu ve
  açısı, imlecin altında toplam yazıyor. Enter ya da sağ tık bitiriyor. Betikte
  `devam=` noktaları aynı işi yapıyor; yapılandırılmış sonuç `kenarlar_mm`,
  `toplam_mm`.
- **ALANÖLÇ köşelerden ölçüyor** (`yontem=nokta`, araç sütununda **Alan Ölç —
  köşelerden**): çizimde olmayan bir alanın köşeleri gösteriliyor, alan dolgulu
  çiziliyor ve alanı ile çevresi köşeler konuldukça yazılıyor. Açık bir çizgiye artık
  "alan: 0,00 m²" değil "kapalı değil, alanı yok; uzunluk: …" diyor.
- **Sonuç tuvalde kalıyor.** ÖLÇ, ALANÖLÇ, AÇIÖLÇ ve KOORDİNAT bittiklerinde ölçtükleri
  şeyi (hat ve kenar uzunlukları, alan, açı, koordinat) vurgulu çizili bırakıyor;
  çizim değişince ya da hiçbir komut çalışmıyorken Esc'e basınca siliniyor. Çizimin
  parçası değil, kaydedilmiyor, günlüğe girmiyor.
- **Bir nokta listesinin anahtarı ardından gelen koordinatları da topluyor:**
  `ALANÖLÇ noktalar=0,0 20,0 20,10`. Önceden ikinci ve sonraki koordinatlar ilk
  konumsal parametreye bağlanmaya çalışılıyordu; `KOPYALA … bitis=A B C` artık üç
  kopya yapıyor.
- Nokta dizisi bekleyen bir istemi Enter ya da sağ tıkla bitirmek artık komutu iptal
  etmiyor, "bu kadar" diyor: salt okunur bir ölçüm bittiğinde "İptal edildi" yazmıyor.
- Tuvaldeki sayılar (kılavuz okuması, ölçek çubuğu, imleç koordinatı) Türkçe ondalık
  virgülle yazılıyor; "10.77 m  124,2238 grad" gibi karışık yazım kalmadı.

### Düzeltildi — araç sütunu ve menüler komutlara tam bağlı; basılınca reddeden araç kalmadı

Araç sütunu ve menülerdeki 108 aracın her biri iki kez — önce seçip sonra basarak ve
hiçbir şey seçmeden basarak — sınandı. Seçim yokken basılınca soru sormak yerine hata
veren on yedi araç vardı; hepsi artık istediğini soruyor:

- **SİL, KES, PANOYAKOPYALA, TEVHİT, İFRAZ, ALANİFRAZ, ALANAÇEVİR, YAZIDÜZENLE,
  NESNENOKTALARI** nesne istiyor. Del tuşu ve **Sil** düğmesi seçim yokken komut
  satırına `SİL nesneler=` yazıp beklemiyor, silinecek nesneleri soruyor.
- **PAH, YUVARLA**: köşeye **tek tıklama** hem nesneyi hem köşeyi seçiyor. Mesafe ya
  da yarıçap **yazılabiliyor ya da gösterilebiliyor**; imleç hareket ettikçe kesilmiş
  köşe tuvalde çiziliyor. Hesap çekirdeğe (`core::cut_corner`) taşındı: önizleme ile
  komut aynı fonksiyonu çağırıyor.
- **KÖŞETAŞI, KÖŞEEKLE**: köşeye ya da kenara tıklamak nesneyi ve köşeyi seçiyor
  (yeni `yer` parametresi); yeni yer istenirken nesne, köşesi imleçte olacak biçimde
  çiziliyor.
- **HACİM** kotu (metre), **OTURT** kontrol çiftlerini tek tek, **DÖNÜŞTÜR** hedef
  sistemi (veri paketinin TM 3° dilimlerini önererek), **ARAÇARA** sözcüğü,
  **İŞŞABLONU** işlemi soruyor. Önceden menüden basılınca "zorunlu parametre eksik"
  diyorlardı.
- **KIR, UZUNLUK, BÖLÜMLE** birden çok nesne seçiliyken reddetmiyor, istediği nesneyi
  soruyor. **ÇİZGİDÜZENLE** tanınmayan bir işlemi hiçbir çizgiye dokunmadan
  reddediyor (önceden önce "2 çizgi düzenlendi ()" diyor, sonra geri alınıyordu).
- Bir sayı istemine **tıklamak** artık sessizce sıfır sayılmıyor; komut sayıyı
  yazmanızı söylüyor ve soruyu açık tutuyor.

Menüde kalıp sütunda olmayan araçlar sütuna aileler hâlinde girdi — sütun 20 düğmede
kaldı: **Sil**; **Buda** ailesi (Uzat, Kır, Uzunluk, Böl, Bölümle); **Pah** ailesi
(Yuvarla, Köşe Taşı, Köşe Ekle, Çizgi Düzenle); **Birleştir** ailesi (Uç Uca Ekle,
Alana Çevir, Patlat); **Taşı** ailesine Hizala ve Esnet; **Metin**'e Yazıyı Düzenle;
**Ölçü**'ye Etiket; **Nokta**'ya Poligon Hesabı; **Ölç**'e Aplikasyon; **Stil
Kopyala**'ya Katmana Taşı; çizgi ailesine açılı kılavuz. On bir aracın kendi simgesi
yoktu (Pah, Yuvarla ve Uzat Buda'nın makasını taşıyordu); hepsine çizildi.

Aracı yeniden kuran gecikmeli olay macOS'ta 800 ms bekleyebiliyordu ve arada
başlatılan komutu (örneğin yeni yazılan PAH'ı) iptal ediyordu; artık arada bir komut
başladıysa geri çekiliyor. POLİGON'un bağlama noktası ve okuma istemleri istasyonu
tuvalde çizili tutuyor.

### Düzeltildi — yapay zekâ artık çizebiliyor

Sohbet ve MCP ajanı hiçbir şey çizemiyordu, ve iki ayrı sebepten:

- **Hiçbir okuma aracı nokta tutamağı basmıyordu.** Bir konum yalnız tutamakla
  verilebiliyor, ama tutamaklar yalnız nesneler ve pencere için basılıyordu. Her çizim
  komutu nokta istediği için model tek bir çizgi bile öneremiyordu; bir nesneyi taşımak
  için bile iki nokta gerekiyordu. Artık `gorunum_bilgisi` ekranın ortasını, yeni
  [`nesne_noktalari`](docs/komutlar/object_points.md) (NESNENOKTALARI) bir nesnenin
  merkezini, köşelerini, uçlarını, kutusunu ve kenar ortalarını nokta tutamağı olarak
  veriyor.
- **Sohbet tutamağı çözmüyordu.** Sohbetin yazma adımları tutamağın *metnini*
  argümana yazıyordu; nokta bekleyen komut bir sözcük buluyordu. Sohbet ile MCP artık
  tek bir argüman derleyicisini paylaşıyor.

Bir ajan yeni köşeleri bir tutamaktan **ölçüyle** söyleyebiliyor:
`{"taban": "@….0", "dogu": 10000, "kuzey": 0}` tabanın 10 m doğusudur. "Ekranın ortasına
altıgen" ya da "parseli 5 m doğuya taşı" böyle ifade ediliyor. Koordinat yazmak hâlâ
reddediliyor; her konumun kaynağı — tutamak ve ölçü — öneri kartında ve denetim
kaydında (`konum_kaynagi`) duruyor. CLAUDE.md 5.8 bu okumaya göre güncellendi (bir ölçü
koordinat değildir; kaynağı §5.2'nin `ofset_hesapla`sı). Değerlendirme seti çizim
vakalarıyla 20 senaryoya çıktı.

Öneri kartındaki satır da düzeldi: noktalar saklandıkları milimetreyle yazılıyordu
(`ÇİZGİ 0,0 10000,10000`); komut satırı metre okuduğu için mühendis satırı yeniden
yazsa on kilometre öteye çizerdi. Artık kart komut satırının birimini yazıyor
(`ÇİZGİ 0,0 10,10`).

### Düzeltildi — Python konsolunda ipucu ve liste satırı örtmüyor

İmza ipucu ile tamamlama listesi pencerenin alt kenarındaki istemde birbirinin ve
yazılan satırın üstüne açılıyordu. Artık tek bir kuralla yerleşiyorlar: satırın yer olan
tarafında, ipucu satıra en yakın ve yazdıkça yerinde, liste onun ötesinde; hiçbiri
satırı örtmüyor. İstemin solunda satır numarası yerine `>>>` ve devam satırlarında
`...` duruyor. Ekran görüntüsü serisi artık üçünü tek karede çekiyor
(`1f-python-liste-ve-ipucu`) — ayrı ayrı çekilen karelerde üst üste binme hiç
görünmüyordu; `KENTOS_PYTHON_PROBE` üç dikdörtgenin kesişmediğini ölçüyor.

### Düzeltildi — reddedilen komut artık hata döner (TODOS F-01)

Bir komut işi reddettiğinde — daireyi budamak, olmayan bir nesneyi silmek, proje
ayarını `MOD` ile yazmak — nedenini transkripte yazıp çıkıyor ve komut veri yoluna
**başarı** bildiriyordu. Komut satırındaki kişi cümleyi okuyordu; betik, yapay zekâ,
MCP ve Python ise işin yapıldığını sanıyordu. Artık ret bir hatadır: aynı cümle hata
olarak döner, betik o satırda durur, komutun yaptığı her şey geri sarılır. Arayüzde
transkript ve durum satırı `Hata:` ile yazar. 47 dosyada 353 ret noktası
`Context::refuse` ile hata kanalına taşındı; rapor, liste, boş arama sonucu ve iptal
olduğu gibi kaldı. Sessizliğin sakladıkları:

- **Betikteki `GERİAL` betikten önceki işi siliyordu.** Betik bir şey çizdikten sonra
  çağrılan `core.undo`, betiğin kendi adımına uzanamadığı için kullanıcının betikten
  önceki son işini geri alıyordu; betik bitince yineleme yığını boşaldığından o iş geri
  gelmiyordu. Artık reddedilir. Henüz bir şey çizmemiş bir betikte ve Python
  konsolunda tek başına `cad.undo()` eskisi gibi çalışır.
- **Toplu işte başarısız bir komut, öncekilerin işini de geri sarıyordu** — günlük
  onları yapılmış gösterirken. `try/except` ile devam eden bir Python betiği yarım
  kalıyordu. Artık yalnız başarısız komutun kendi düzenlemeleri geri alınır.
- **Kılavuzdaki ~40 örnek hiç çalışmıyordu:** var olmayan nesneler, olmayan katmanlar,
  geri alınacak işi olmayan `GERİAL`. Kılavuz testi bütün sayfaları tek bir ortak
  çizimde koşuyor ve ret sessiz olduğu için "geçiyordu". Artık her sayfa kendi
  çiziminde, sırasıyla koşuyor; örnekler kendi nesnelerini çiziyor. PYTHON örnekleri
  gerçek yorumlayıcıyla koşuyor.
- **İki test hiçbir şey ölçmüyordu.** Yakalama testinin kurulum satırı (`AYAR` ile bir
  uygulama tercihi, üstelik sınır dışı) hiç çalışmamıştı ve ölçtüğü mesafe yakalansa da
  yakalanmasa da aynıydı; bir seçim testi olmayan `SEÇ HİÇBİRİ` kipiyle seçimi
  temizlediğini sanıyordu.

Davranış değişikliği: yanlış kapsamdaki ya da bilinmeyen bir ayarı yazmak
(`AYAR tema koyu`, `MOD koordinat_sistemi …`) ve boş yığında `GERİAL`/`YİNELE` artık
betiğe hata döner. [Destek matrisi](docs/nesneler/destek-matrisi.md) sessiz ret
listesini boş gösteriyor; `scripts/ci-gate-kapsam.sh` bir tane bile görürse kırmızı.

### Eklendi — ölçülen destek matrisi (TODOS F-01)

[Destek matrisi](docs/nesneler/destek-matrisi.md), hangi düzenleme işleminin hangi
nesne türünde ne yaptığını gösteriyor — ve **elle yazılmadı**. `kentos_kapsam` her
hücre için boş bir çizimde türü bir kullanıcının yazacağı komutla kuruyor, işlemi
aynı komut veri yolundan çalıştırıyor ve çıkan sonucu ölçüyor: tür korundu mu, uzunluk
azaldı mı, taşınan nesne tam kaydı mı, daire ölçüsü 2πr mi. `scripts/ci-gate-kapsam.sh`
tabloyu yeniden üretip karşılaştırıyor; bir davranış değişirse tablo da aynı commit'te
değişmek zorunda.

15 tür × 17 işlem, 238 ölçülen hücre: 138 destekli, 10 kısmi, 38 yok, 52 uygulanamaz.
Ölçüm, statik okumanın göremediğini buldu:

- **38 sessiz ret.** Komut işlemi reddediyor ama veri yoluna **başarı** bildiriyor:
  reddini transkripte yazıp çıkıyor. Kişi cümleyi okur; betik, yapay zekâ, MCP ve
  Python işlemin yapıldığını sanır. 28 komut dosyası bu deseni kullanıyor.
- **Paralel, tampon üretiyor.** `(0,0)→(50,0)` çizgisinin 2 m paraleli 200 m²'lik
  kapalı bir alan; daire paraleli kirişli çoklu çizgi; delikli alanın paralelinde delik
  ayrı bir nesneye dönüşüyor.
- **Yuvarlatma yayı kirişlerle** çiziliyor, gerçek yay değil.
- **Hiçbir komut çok parçalı alan üretmiyor.** `combine.cpp` başındaki yorum "iki
  ayrık parça tek nesne olur" diyor, kod her parçayı ayrı nesne yapıyor. Yaylı çoklu
  çizgiyi de hiçbir komut üretmiyor; tek yolu DXF.
- **Ölçü doğru:** daire, yay, elips ve yaylı çizginin çevresi analitik değerle
  milimetresine kadar aynı.


### Eklendi — `cad.Point`, `cad.Box`, `cad.viewport` ve gerçek bir editör

**Değer tipleri.** `cad.Point(east, north)` ve `cad.Box(...)` programın **kendi**
tipleridir, Python tarafında yeniden tanımlanmış kopyaları değil — `cad.Point`,
`core::Point2`'nin ta kendisi. Bir koordinat iki sayılık liste olarak da, `Point`
olarak da her yere verilebilir.

Eksen adları harf değil: bir paftada doğu değeri **Y** yazar, kodda ilk eksen
`x`'tir, yani ikisi birbirinin tersidir. Hangi harfi seçsek okuyucuların yarısı
tersini anlardı; API `east` ve `north` diyor.

**`cad.viewport`** pencerenin baktığı yeri değer olarak veriyor: `bbox()`,
`center()`, `scale()`, `mm_per_pixel()`, `size_px()`, `crs()`. Pencere yoksa
`exists()` `False` döner ve diğerleri hata verir — uydurulmuş bir dikdörtgen,
sonraki çizimi kimsenin bakmadığı bir yere koyardı. `GÖRÜNÜMBİLGİSİ` komutuyla
aynı kaynağı okur: biri insanın, öbürü betiğin okuduğu biçim.

**Editör baştan yazıldı.** Önceki hâli üç harf yazılmasını ve `cad` ile
başlamasını bekliyordu; yani `cad.` için hiçbir şey, çağrı içinde hiçbir şey,
betiğin kendi adları için hiçbir şey öneriyordu.

- **Tamamlama bağlama bakıyor:** `cad.` bütün komutları (yanında Türkçe adları),
  `cad.doc.` ve `cad.viewport.` o nesnenin çağrılarını, bir çağrının parantezi
  içi **o komutun anahtar kelimelerini** — zaten yazılanı tekrar önermeden.
  Dışarıda betiğin kendi tanımladığı adlar, Python anahtar sözcükleri ve
  yerleşikler. **Ctrl+Boşluk** her yerde açar.
- **İmza ipucu:** parantez içindeyken komutun bütün parametrelerini yazan,
  **o an yazılanı vurgulayan** ve altına Türkçe açıklamasını koyan bir şerit.
  Uzun imzalar satır kırıyor.
- Bir komut adı seçilince parantezler kendi açılıyor; anahtar kelimeler
  (`points=`) sözdiziminde ayrı renkte.
- Parametre tipleri `Coord` / `Coords` diye okunuyor. Önceki `list[list[int]]`
  yalnız çirkin değil **yanlıştı**: `core.circle_draw`'ın merkezi tek noktadır ve
  belge herkese `center=[[y, x]]` yazmasını söylüyordu.
- Yeni `python-editor` ctest'i istemi **gerçek tuş olaylarıyla** sürüyor ve ne
  önerildiğini denetliyor: `cad.` seksenden fazla aday veriyor mu, `cad.doc.`
  komut önermiyor mu, çağrı içinde `points=` çıkıyor mu, yazılmış anahtar tekrar
  önerilmiyor mu, betiğin kendi adı geliyor mu, çağrı bitince ipucu kapanıyor mu.


### Eklendi — Python konsolu ve `PYTHON` komutu

Pencerenin altında, komut satırının kardeşi bir panel: yazdığınızı `>>>` ile,
programın söylediğini altına yazar. **Pencere > Python Konsolu**.

- **`PYTHON` komutu** (`core.python`) panelin çalıştırdığı şeydir. Panel bir
  istemcidir, özel bir giriş değil: aynı parçacığı komut satırından ve JSON
  betikten de çalıştırırsınız (Article 1.2, 5.15).
- **Yapay zekâya kapalı ve kapalı kalacak** (yeni `CLAUDE.md` 5.24). Ajan komut
  önerir; komut önizlenebilir, doğrulanır, tek tek günlüğe yazılır. Bir parçacık
  çalışana kadar bunların hiçbiri değildir.
- **Sözdizimi renklendirmesi** `cad.` adlarını **canlı komut kaydından** tanır:
  `cad.line` renklenir, `cad.lien` renklenmez. Üç yeni mürekkep (`design.md` §2):
  yorum, sayı ve `def`/`class` ile tanımlanan ad.
- Enter gönderir, Shift+Enter satır ekler, Yukarı/Aşağı geçmişi getirir. Açık bir
  parantez varsa gönderim beklemeye alınır; bir `:` beklemeye ALMAZ, çünkü istem
  çok satırlıdır ve döngüyle gövdesi birlikte yazılır.

Yol boyunca düzeltilenler: yerleşim sürümü 6'ya çıktı (eski kayıtlı yerleşim yeni
paneli tanımadığı için konsol ekranın ortasında yüzen bir pencere olarak
açılıyordu); istemin yüksekliği tek sayfadan (`theme.cpp`) geliyor, çünkü oradaki
genel kural her `QPlainTextEdit`'in `max-height`'ını temizliyor ve widget'ın
kendi `setFixedHeight`'ı hiçbir şey yapmıyordu.


### Eklendi — `kentos.cad`: her komut bir Python fonksiyonu

`cad.run("ÇİZGİ 0,0 10,10")` yanına her komutun kendi fonksiyonu geldi:

```python
cad.line(points=[[485320150, 4310220400], [485370150, 4310250400]])
cad.circle_draw(center=[485400000, 4310230000], rim=[485410000, 4310230000])
```

Bu fonksiyonlar **elle yazılmadı; komut kaydından üretiliyor.** Programa bugün
eklenen bir komut bugün bir Python fonksiyonudur — güncellenecek ikinci bir
bağlama listesi yok (CLAUDE.md 5.10, 5.20).

- **Parametrelere İngilizce ad eklendi** (`Param::en`, `ToolParam::en`): 485
  bildirim, hepsi bildirim yerinde. Türkçe adın yanında, ona göre anahtarlanmış
  bir tabloda değil — çünkü `kenar`, `core.layout`'ta sayfa boşluğu,
  `geodesy.traverse`'de ölçülen kenar, `islem.alan_duzenle`'de kenar sırasıdır.
  Tek cevap veren bir tablo iki yerde yanılırdı.
- **Fonksiyon adı kimlikten türüyor:** `core.line` → `cad.line`. Kimliği Türkçe
  olan altı komut kendi İngilizce adını bildiriyor (`CommandSpec::python`):
  `islem.uzunluk_yaz` → `cad.label_length`.
- **Yalnız anahtar kelime, yalnız İngilizce.** Konumsal argüman yok: komutun
  parametreleri bir KÜME, komut satırının onlar için bir sırası hiç olmadı.
- **Okumalar `cad.doc` altına taşındı.** `cad`'in üst düzeyi üretime ait ve o
  küme kendiliğinden büyüyor; `KATMANLAR` ile `AYAR` gerçek komutlar olduğu için
  `cad.layers` ve `cad.setting` sessizce eziliyordu — çizimden katman adı isteyen
  bir betik komutun işlem sayısını alıyordu. Artık projeksiyon var olan bir adı
  ezmeyi reddediyor ve nedenini söylüyor.
- **Beşinci ve altıncı üretilmiş çıktı:** `docs/python/referans.md` (her komutun
  imzası, anahtarları, türleri) ve `docs/python/kentos_cad.pyi` (düzenleyici
  tamamlaması için tip taslağı). İkisi de Python KAPALIYKEN de üretiliyor —
  yapılandırmaya bağlı bir tazelik denetimi hiç çalışmamış demektir.
- **Yeni kapı `ci-gate-python-api.sh`:** İngilizce adı olmayan, geçerli bir Python
  tanımlayıcısı olmayan ya da Python'un ayrılmış sözcüğü olan bir parametre
  yapıyı kırar. Kapının göremediğini — bir komutun iki parametresinin aynı
  İngilizce sözcüğe düşmesi, iki komutun aynı fonksiyon adını istemesi — kurulu
  kaydı dolaşan bir birim testi yakalıyor.

Kural tadilleri: **CLAUDE.md 6.15** (bir komut Python karşılığı ve dökümanı
olmadan bitmez), **5.24** (Python çalıştırma yolu hiçbir ajan istemcisine
açılmaz; `core.script`'in `AiAccessible` taşımadığını bir test sabitliyor), ve
6.14 / 5.18 / 5.20 altı üretilmiş çıktıya göre güncellendi. `script.md`
R9/R9a/R11a/P15, `docs.md` R18a.


### Eklendi — gömülü Python 3.14: `kentos.cad` ve tek gömülü dil

`BETİK` artık `.py` uzantılı bir dosyayı gömülü CPython 3.14 ile çalıştırıyor
(`KENTOS_WITH_PYTHON=ON`, varsayılan kapalı). Değişken, döngü, koşul, fonksiyon —
ve JSON betiğinin her kuralı aynen geçerli: tek komut veri yolu, tek dilbilgisi,
tek işlem, tek geri alma adımı, aynı `{kind:"meta"}` günlük kaydı.

- **`cad.run("ÇİZGİ 0,0 10,10")` tek yazma yoludur.** Çizime dokunan başka
  bağlantı yok; doğrulama, geri alma ve günlük oraya bağlı. Okuma çağrıları
  (`cad.layers()`, `cad.entity_count()`, `cad.setting()`) yalnız **değer**
  döndürür — çizimin içine tutamak verilmez.
- **API'deki her ad İngilizce.** Program ve komutlar Türkçe kalır; bir Python
  modülü her Python kütüphanesinin yazıldığı dilde okunur.
- **`print` komut satırına düşer.** Pencereli bir uygulamanın terminali yoktur;
  ilerlemesini bildiren betik boşluğa bildirmiş olurdu.
- **İptal gerçekten durdurur.** Yorumlayıcı en geç 5 ms'de el değiştirir, durdurma
  isteği o an kesme olarak iner. Betik kesmeyi `except BaseException` ile yutsa
  bile çalıştırma iptal sayılır ve geri alınır.
- **Kum havuzu dürüstçe anlatıldı.** CPython kafese konamaz — `os`, `socket`,
  `ctypes` tek yorumlayıcının içindedir ve Python'un kendi belgeleri bir Python
  kum havuzunun başarılabilir olmadığını söyler. Seviye; bağlamaların kapsamını,
  yorumlayıcının yalıtımını (`PYTHONPATH`/`PYTHONHOME` okunmaz, betiğin dizini
  `sys.path`'e girmez), `tam` için istenen onayı ve günlüğe düşen kaydı yönetir.
  Veremeyeceği sözü vermeyen bir kural, veren bir kuraldan iyidir.

### Kaldırıldı — gömülü Lua

Lua 5.4 + sol2 ağaçtan silindi: `lua_runner.*`, `KENTOS_WITH_LUA`, sabitlenmiş
commit'ler, testi ve sayfası. **Bu, `kentoscad.md` §4.1'in iki betik dilli hâlini
geçersiz kılar.**

Eski gerekçe — "Python'u etiket ifadesi için her satırda çağıramazsınız" — hâlâ
doğru. Yanlış olan, ondan **ikinci bir betik dili** sonucunu çıkarmaktı. Nesne
başına çalışan bir ifadenin doğru yeri komut satırının kendi **derlenmiş ifade
motorudur**: deterministik, tahsissiz ve zaten programın tek dilbilgisi. İkinci
bir betik dili ise öğrenilecek ikinci bir söz dizimi, belgelenecek ikinci bir API
ve bakılacak ikinci bir sandbox demekti — kullanıcı karşılığında bir şey
kazanmadan.

`CLAUDE.md` Article 8.2 ve 8.3, `script.md` R1/R3/R5/R6/R7/P1/P12 ve `build.md`
R12/R23 aynı değişiklikte tadil edildi; her biri neyi geçersiz kıldığını yazıyor.


### Düzeltildi — içe aktarma penceresinde rütbe: en sessiz şey en önemliydi

Bu pencere bir "dosya seç" penceresi değil, bir **beyan denetimi**: bir DXF ne
koordinat sistemi ne birim taşır, ve bunları yanlış almak parseli yanlış yere
yanlış ölçekte koymak demek. Çıktı imzalanan bir belge. Buna göre bakıldığında
sıralama tersti.

- **Okuyucunun uyarıları** tek bir gri paragrafa düzleştirilmiş, önizlemenin
  ALTINDA, seviyeleri cümlenin önüne küçük harfle yazılmış hâlde duruyordu — yani
  "dosya koordinat sistemi bildirmiyor" nesne sayısıyla aynı ağırlıkta, sayfanın
  dibinde. Artık her biri bileşen setinin `Banner`'ı: seviyesi tonu seçiyor,
  ilk cümle başlık, gerisi ne yapılacağı. Çizimin **üstünde**, çünkü dosya
  hakkında; ve önizleme esneyen öge olduğu için dört uyarılı bir dosya onlara
  yerini veriyor. Küçük resim bir teselli, uyarı bir karardır.
- **Bir `Info` olgu sayılıyor** ve sessiz satırlara katılıyor ("okunan türler:
  ARC, CIRCLE, LINE"). Dört banner, iki önemlinin rütbesini götürüyordu.
- **Okunamayan dosya seçildiğinde pencere bunu orada söylüyor.** Yapı izin
  listesini biliyordu ve kullanmıyordu: var olan her dosya **İleri**'yi açıyor,
  ret bir sonraki sayfada geliyordu — kullanıcı akışa girdikten sonra.
- **Biçim listesi taranabilir oldu:** nokta ile birleştirilmiş cümleler yerine
  her satırın sonunda **okunur** / **okunur · yazılır** / **okunmaz** etiketi.
  Bu listeye bakan biri tek bir şey arar; artık gözüne o çarpıyor.
- **Sayfanın yarısı boş değil.** Bütün adımlara tek boy veriliyordu, yani bir
  yol alanı ve bir başvuru listesi, katman sayfasının önizleme + liste için
  istediği yüksekliği alıyordu. Pencere artık bulunduğu sayfanın boyunu alıyor;
  büyüyor, küçülmüyor (ileri geri gezerken pencere elin altında yeniden
  akmasın diye).
- **Cümle en üste taşındı.** "Dosya önce yalnızca okunur; çizime hiçbir şey
  eklenmez" sayfanın en son satırıydı — okuma başlamadan gizli duran bir ilerleme
  şeridinin altında. Pencereyi kullanmayı güvenli kılan tek söz, hakkında olduğu
  karardan sonra okunuyordu.
- **Metin bir sütuna alındı.** Her satır pencere genişliğinde, 170 karaktere
  yakın akıyordu; alan ölçülmüştü, yazı ölçülmemişti.

### Düzeltildi — varsayılan koordinat sistemi artık uyarı

`io.md` R20: "eksik ya da tanınmayan CRS bir hata olmalı, asla sessiz bir
varsayım değil." CRS taşıyabilen biçimlerde eksik CRS gerçekten hata. DXF
taşıyamadığı için varsayım sanksiyonlu — ama en yumuşak seviyeye (`Info`)
yazılıyordu, ki R20'nin yasakladığı sessizlik tam bu. `Severity`'nin kendi
tanımı da bunu söylüyor: `Warning` = "okuyucu bir şey varsaymak zorunda kaldı;
çizim yanlış olabilir."

**Birim varsayımına dokunulmadı** ve bu bilinçli: birim bildirmeyen bir DXF
neredeyse her zaman metre demektir, her içe aktarmada uyarı vermek uyarıyı
değersizleştirir, ve `test_io.cpp` bu kararı gerekçesiyle sabitliyor. Koordinat
sisteminin öyle bir varsayılanı yok. İki kararın farkı artık testte yazılı.

### Düzeltildi — yedi yakalama modu hiçbir işaret çizmiyordu

`map_canvas.cpp`'deki işaret `switch`'inin sonunda bir `default: break;` vardı —
ve o default sessiz bir delik: hiçbir dalı olmayan bir mod hiçbir şey çizmiyor,
yani yardımcı tutuyor, nokta kayıyor, işaret ise tutmadığını söylüyordu. Bu bir
kez `DÜĞÜM` için bulunup bir dal eklenerek kapatılmıştı, yani delik açık kalmıştı
— ve arkasından yedi mod içine düştü: **yüzey normali**, çeyrek, teğet, kılavuz,
ağırlık merkezi, iz ve adım.

İşaretler `render/snap_marker.hpp`'ye çıktı (Qt'siz) ve bir test **bütün modları
dolaşıp** işaretsiz olanda kırılıyor. Yedisinin işareti de yazıldı; yüzey
normalininki bir yüzey ve üzerinde duran **dik açı işareti** — kilidin yaptığı
şeyin tamamı, ve kullanıcının onu yaptığını göremediği tek şey. Ayrıca tanınmayan
bir bit bile bir işaret alıyor: motor noktayı oynattıysa bunu söyleyen bir şey
olmak zorunda. Yeni bir mod artık işaretsiz gönderilemiyor — derleme kırılır.

### Düzeltildi — AÇIÖLÇ ölçtüğü açıyı göstermiyordu

İki kol da tepeden imlece giden düz bir çizgi olarak önizleniyordu, yani ikinciyi
nişanlarken birincisi ekrandan kayboluyor ve komutun **tek ölçtüğü şey** — aradaki
açı — cevap döküme düşene kadar hiçbir yerde görünmüyordu.

Yeni `RubberShape::Angle` ile birinci kol yerinde duruyor, imlece ikinci kol
uzanıyor ve tepede süpürme bir **yay** olarak çiziliyor; yanında okuma yazıyor.
Yay, komutun bildirdiği süpürmenin kendisi: kısa olanı çizip uzun olanı yazmak
bu turda kaldırılan hatanın aynısı olurdu, o yüzden süpürme birinci koldan
ikinciye, oturumun kuralının yönünde okunuyor. Kol sırası kullanıcının seçimi.

Yay bir de piksel cinsinden sabit bir yarıçapta çiziliyor — 2 cm'lik bir kolla
40 m'lik bir kol arasındaki açı iki ucunda da okunabilsin diye.

### Eklendi — AÇIÖLÇ kendi ikonunu aldı

`ÖLÇ` ile aynı cetveli taşıyordu; artık iki kol ve aralarındaki yay.

### Düzeltildi — hayalet önizleme artık komutun kendi dönüşümü

Hayalet, fiil ne olursa olsun bir **öteleme**ydi: tuvalin yardımcısı `dx, dy`
alıyordu, yani önizleyebildiği tek şey bir kaydırmaydı. `TAŞI` ve `KOPYALA`'da
doğruydu; `DÖNDÜR`, `ÖLÇEKLE` ve `AYNALA`'da ise hiç yoktu — ve eskisi verilseydi
komut nesneleri döndürürken hayalet onları yana kaydırıyor gösterecekti. Yanlış
dönüşümü vaat eden bir önizleme, hiç önizleme olmamasından kötüdür: kullanıcı
onunla nişan alır.

`Xform` — dört dönüşümü tek yerde anlatan yapı — `core/transform.hpp`'ye çıktı.
Fiil belgeyi onunla yazıyor, tuval hayaleti onunla çiziyor: `core::transformed`
tek fonksiyon, yani ikisi **yapısal olarak** ayrışamaz. Tuvalin `addWorldRun`,
`addEmitRuns` ve `addGhost` yardımcıları artık öteleme değil dönüşüm alıyor.

Hangi dönüşümün önizlendiğini fiil söylüyor (`core::GhostSpec`, yükte), imleci
dönüşüme çeviren de tek fonksiyon (`core::ghost_xform`) — fiil cevabı alırken
onu çağırıyor, tuval her fare hareketinde. Hayaletin kusursuz olması bundan.

### Eklendi — DÖNDÜR ve ÖLÇEKLE fareyle

İkisinin de sayısı yalnız yazılabiliyordu. Artık verilmezse **gösterilir**:

- **`DÖNDÜR`**: imlecin merkeze göre doğrultusu açıdır (doğu 0, kuzey 90 derece)
  ve nesneler imlecin altında döner.
- **`ÖLÇEKLE`**: imlecin merkeze **metre** cinsinden uzaklığı çarpandır — iki
  metre dışarısı iki kat — ve nesneler imlecin altında büyür.

`aci=` ya da `carpan=` yazan için hiçbir şey değişmedi ve komut fazladan soru
sormuyor; eski günlük satırları aynen oynuyor. Türetilen sayı kayda `aci` ve
`carpan` olarak yazılıyor, jest yazılmıyor: bir soruya iki cevap olmaz.

### Eklendi — altı düzenleme fiili araç kutusuna girdi, ve yedi ikon ayrıldı

Kolonda yalnız `TAŞI` vardı; `KOPYALA`, `DÖNDÜR`, `ÖLÇEKLE`, `AYNALA` ve `DİZİ`
sadece `Değiştir` menüsündeydi — üçüne yeni verilen hayaleti bir menüden keşfetmek
mümkün değil (§2.6a). Altısı tek kartta.

İkonlar: `DÖNDÜR`, `ÖLÇEKLE` ve `AYNALA` aynı dönen oku taşıyordu, `KOPYALA` ile
`BLOKEKLE` aynı iki-çerçeveyi. Dört yeni glif (`Scale`, `Mirror`, `Array`,
`BlockInsert`) ile her fiil kendi markasını aldı.

### Düzeltildi — nokta araçları: sabitlenen referans ekranda kalıyor

Beş aracın dördünde tek bir ortak boşluk vardı. Bir komut önce bir **referans**
sabitliyor — taban çizgisi, istasyon, kazıklanacak doğru, bilinen noktalar — sonra
o referansa göre **sayı** soruyor. Referans çizimin nesnesi değil, komutun
hatırladığı noktalar; bu yüzden verildiği anda ekrandan kayboluyor ve okuma
görünmeyen bir şeye göre yazılıyordu.

Yeni `RubberShape::Fixed` sabitlenmiş olanı çiziyor ve imleci izleyen hiçbir şey
çizmiyor — fare o soruyu cevaplamadığı için doğru önizleme bu. İmleç tuvalin
dışındayken de çiziliyor, çünkü yazan bir el fareyi oraya bırakır.

- **DİKAYAK**: taban çizgisi duruyor, ve `ayak` yazıldığı anda **ayak noktası** da
  işaretleniyor — `boy` tam oradan ölçülür.
- **ALIM**: istasyon duruyor, `baglama` verildiyse bağlama doğrultusuyla birlikte.
- **ARANOKTA**: kazıklanan doğru duruyor.
- **KESİŞİMNOKTA**: bilinen noktalar duruyor.

Bunun için `ctx.number` ve `ctx.integer` da kılavuz alabiliyor artık: bir kılavuz
yalnız noktaya ait değil.

### Düzeltildi — KESİŞİMNOKTA `mesafe`: iki çözümden hangisi artık SORULUYOR

`bby`'dekinin aynısı. Komutun üstündeki not "kullanıcı hangisini istediğini
söyler" diyordu ama `yon` yalnız argümandan okunuyor ve yoksa `sol`
varsayılıyordu — iki kesişimden biri fareyle hiç seçilemiyordu. Bir sınırı yolun
yanlış tarafına koymanın yolu tam olarak bu.

Şimdi yeni `RubberShape::Candidates` ile iki çözüm de ekranda işaretleniyor,
imlece yakın olan halkayla vurgulanıyor ve tıkladığınız o oluyor; `yon` bulunan
yandan türetilip günlüğe yazılıyor, yani satır yeniden oynatıldığında aynı
noktayı veriyor.

### Eklendi — yöntemler karta girdi, ve nokta ailesinin beş aracı beş ikon

`KESİŞİMNOKTA`'nın üç yönteminden ve `ARANOKTA`'nın ikisinden yalnız varsayılanı
karttan erişilebiliyordu — yani iki tape ölçüsünden sınır kurma işi ancak yöntemi
yazarak yapılabiliyordu (§2.6a). Karta üç üye eklendi: **Kesişim — iki mesafeden**,
**Kesişim — iki doğrudan**, **Ara Nokta — mesafeden**.

İkonlar da ayrıldı: `NOKTA`, `KESİŞİMNOKTA` ve `ARANOKTA` üçü de sade noktayı
taşıyordu; `DİKAYAK` `ÖLÇÜ`'nün cetvelini, `ALIM` ise `LİDER`'in imlecini ödünç
almıştı. Dört yeni glif geldi ve ödünç alınan iki marka sahiplerine döndü.

### Düzeltildi — YAY: üç yöntem yayı önizlemiyordu, `bby` ise yanı hiç sormuyordu

Aynı hastalık, beşinin üçünde.

- **`3n` ve `devam` düz bir ÇİZGİ önizliyordu** — bir kavisin olmadığı tek şey.
  Artık `3n`'de üç noktadan geçen yay, `devam`'da ise son çizilenin ucundan
  **teğet** ayrılan yay gösteriliyor; kırık olup olmadığını tıklamadan önce
  görüyorsunuz. `devam`'da teğet doğrultusu kılavuza bir **nokta** olarak
  geçiyor, böylece kılavuz ile komut aynı iki noktadan aynı doğrultuyu okuyor,
  her biri kendi yuvarlamasını yapmıyor.
- **`bby`'nin yanı hiç sorulmuyordu.** Komut `yon`'u argümanından okuyup yoksa
  `sol` varsayıyordu — yani arayüzden **hep** biri çiziliyor, öteki fareyle
  erişilemez oluyordu; üstündeki yorumda "kullanıcı yanı gösterir" yazmasına
  rağmen öyle bir soru yoktu. Şimdi soruyor: imleci kirişin bir yanından öbürüne
  geçirdikçe yay taraf değiştiriyor. `yon=` yazan için hiçbir şey değişmedi ve
  komut fazladan soru sormuyor.
- **Yarıçap, yazıldığı anda denetleniyor.** Olmayan bir yayın hangi yanı
  sorulmaz: iki nokta arasının yarısından küçük bir yarıçap yan sorusundan önce
  reddediliyor.
- **Üç yapı `core`'a indi** (`core/arc.hpp`: `ArcBuild`, `ArcGuide`,
  `arc_from_guide`, `arc_radius_side`, `arc_by_radius`) ve komut da kılavuz da
  onları çağırıyor. Altın fikstür bayt-özdeş kaldı.

`bby`'nin yanını **kirişin hangi yanı** olduğuna göre seçmek bir düzeltme
değil, doğru ölçüt: "hangi merkez daha yakın" testi, bir pahın en sık çizildiği
durumda çöküyor — yarıçap tam yarım açıklık olduğunda iki merkez **çakışıyor**,
ama iki yay hâlâ iki ayrı yay (hangi ucun başlangıç olduğuyla ayrılıyorlar).
Kirişin ise yarıçap ne olursa olsun iki yanı var.

### Düzeltildi — dört araç başka bir aracın ikonunu taşıyordu

Rapor: "taşı ve esnet ikonları aynı." Bir kolon **etiket değil ikon** gösterir,
yani tek ikonu paylaşan iki araç elin uzandığı tek araçtır. Dördü de kendi
ikonunu aldı: `ESNET` (çerçeveden çekilen köşe — `TAŞI`'nın dört yön oku
değil), `ELİPS` (iki eksenli elips), `HALKA` (iç içe iki çember), `DİLİM`
(çemberden kesilmiş dilim). Kalan paylaşımlar aynı şeklin yöntem çeşitleridir
ve kartta etiketleriyle görünür.

### Düzeltildi — DAİRE'nin üç yöntemi çemberi önizlemiyordu

`2n` ve `3n` lastik bir **çizgi** gösteriyordu, `ttr` ise hiçbir şey. Dördünün
kılavuzu da artık tıklayınca oluşacak çemberin kendisi, ve çemberi kuracak olan
fonksiyondan geliyor (`core::circle_from_guide`).

- **`ttr`** en çok gereken yerdi: verilen yarıçapta iki doğruya teğet **dört**
  çember var, kullanıcı köşeyi göstererek seçiyor, ve hangisini aldığını
  tıkladıktan sonra öğreniyordu. Şimdi iki teğet doğru ve imlecin bulunduğu
  çeyrekteki pah çemberi önizleniyor.
- **Dört çözümü arayan kod `core`'a taşındı** (`core::tangent_circle_centre`) ve
  komutla kılavuz onu paylaşıyor — önizlemenin gösterdiği çember, çizilen
  çember.
- **Dört yapı tek evde**: `CircleBuild` ile `merkez`, `2n`, `3n` ve `ttr`
  `core::circle_from_guide`'a indi; komut kendi uzaklık yardımcısını bıraktı.
  Aritmetik birebir aynı, altın fikstürler değişmedi.

### Düzeltildi — halkanın iç çemberi ekranda kalıyor

Kullanıcının isteği: "halka çizilirken ilk çizilen halkanın kılavuz çizgileri
kalmalı ki görebilelim." Yapılan şey iki çemberin kendisi; yalnız en yenisi
gösterildiğinde ikinci tıklama birincisini silmiş gibi görünüyordu. İç çember
artık `rubber_chain` ile duruyor ve iki kılavuz birlikte çiziliyor. Aynı ilke
`DİLİM`e de uygulandı: sabitlenen ilk kenarın yarıçap çizgisi de duruyor, böylece
şekil çıplak bir yay değil dilim olarak okunuyor.

### Düzeltildi — çizimden sonra araç elde kalıyor, ve elde kalan araç aynı araç

Kullanıcının raporu: "çizim yaptıktan sonra varsayılan araç seçiliyor
tekrardan." Arkasında iki ayrı kusur vardı; ikisi de gerçek işletim sistemi
olaylarıyla ölçüldü.

**Enter bir nokta dizisini bitirmiyordu.** Sağ tık `finishInteractive()`
çağırıp aracı elde bırakıyor; Enter ise `supplyPickedObjects`/`acceptGuide`'a
düşüp boşa çıkıyordu. Böyle bir çalışmayı bitiren tek tuş Esc kalıyordu — ve Esc
aracı *bırakır*. Kullanıcı her çizgiden sonra araç kolonuna geri gidiyordu. Artık
Enter, nokta bekleyen bir çalışmayı sağ tıkla aynı şekilde bitiriyor
(`MapCanvas::finishPointRun`, tuvalden ve komut satırından — tek gövde, iki yol).
Bu aynı zamanda klavye eşitliği: yalnız fareyle erişilen bir yetenek
gönderilemez (5.15, ui.md R21).

**Tekrar kurulan araç, ailenin ilk üyesiydi.** Bir yöntem aracı tam satır taşıyor
(`ÇOKGEN yontem=dis`); tekrar mantığı bunu komut ADI diye çözüyor, bulamıyor ve
döngüye devam edip düz `ÇOKGEN`'i buluyordu — adı aynı komut kimliğine çözülen
üye. "Çokgen — dıştan"ı seçip bir tane çizdiğinizde sıradaki sessizce içten olan
oluyordu: seçtiğiniz araç varsayılanla değişiyordu. Eşleşme artık önce
`Controller::armedLine()`'a, sonra ilk sözcüğe bakıyor; `armedLine_` de sinyal
atıldıktan sonra temizleniyor, çünkü hangi aracın çalıştığını söyleyen tek şey
odur (beş düğme `core.arc_draw` gönderiyor).

`KENTOS_REALMOUSE_PROBE`'un 8. bölümü ikisini de tutuyor: Enter'dan sonra oturum
yeniden soruyor ve yanan düğme hâlâ ÇİZGİ; yöntem aracıyla bir çokgen çizildikten
sonra yanan düğme hâlâ `ÇOKGEN yontem=dis`.

### Düzeltildi — çokgen araçları komple elden geçti: kılavuz, soru sırası, işaretle boy verme

Kullanıcının raporu: "döndürülmüş dikdörtgen çiziyor ama çizerken kılavuz yok;
düzgün çokgen ile ilgili hiçbir aksiyon yok, hiçbir şey yapılamıyor, kılavuz da
yok; dıştan ve kenardan çokgen de aynı; kenar sayısını girebileceğim bir yer
yok." Dördü de gerçek işletim sistemi olaylarıyla yeniden üretildi ve düzeltildi.

**Kılavuz artık şeklin kendisi, ve şekli çizecek fonksiyondan geliyor.** Yeni
`core/polygon.hpp` iki çağıranın ortak cevabı: `ÇOKGEN` halkayı onunla kuruyor,
tuval kılavuzu onunla çiziyor. İkinci bir yoldan hesaplanan kılavuz, kolay
durumlarda komutla aynı çıkıp aritmetiğin ilginç olduğu yerde ayrışır — dıştan
bir çokgenin ağzı, normaline izdüşen bir üçüncü nokta — ve kullanıcı bir şekil
görüp başka bir şekil alır.

- **`DİKDÖRTGEN yontem=3n`** üçüncü noktayı zincirsiz bir `Ring` önizlemesiyle
  soruyordu: bir başlangıç ve bir imleç, yani iki nokta, yani çizilecek hiçbir
  şey. Araç doğru çiziyor ama yolda hiçbir şey göstermiyordu. Yeni
  `RubberShape::EdgeRectangle` kenarı alıp **döndürülmüş dikdörtgenin dört
  köşesini** çiziyor.
- **`ÇOKGEN`'in soru sırası değişti.** Eskiden merkez → kenar sayısı → boy;
  tıkladıktan sonra ekran boş kalıyor ve soru altta tek satır metin oluyordu.
  Şimdi **kenar sayısı en başta** soruluyor — ekranda bakacak bir şey yokken,
  soru tek olayken — odak kendiliğinden komut satırına geçiyor. Merkez
  konduktan sonra çokgen fareyi izliyor.
- **Boy artık işaret edilerek verilebiliyor.** `yaricap=` yazan için hiçbir şey
  değişmedi ve komut fazladan soru sormuyor. Elinde sayı olmayan ise yerini
  gösteriyor: `ic`'te bir köşenin, `dis`'te bir kenarın geçtiği nokta — tek
  tıklamada hem yarıçap hem dönüş, canlı önizlemeyle. `dis`'te imlecin altından
  **kenar** geçiyor (yarım adım), çünkü ölçü ağızdan veriliyor.
- **`yontem=kenar`** kenar uzunluğunu yazıyla alıyor (bir kenar uzunluğu
  merkezden işaret edilemez; imlecin uzaklığı yarıçaptır), sonra fare yalnız
  **çeviriyor** — yazılan boyda çokgen, imlecin verdiği açıda, önizlemeli.
- **Yeni `kose` parametresi**: işaret edilen nokta betikten de verilebilir.
  Ondan türeyen yarıçap ve açı `yaricap`/`aci` olarak günlüğe yazılıyor, nokta
  kendisi yazılmıyor — aynı soruya iki cevap olmaz (Article 1.4).

### Düzeltildi — boş bir argüman günlüğe `null` olarak yazılmıyor

`Empty` bir değer "hiçbir şey kabul edilmedi" demek; günlük onu `"kose":null`
diye yazıyordu, yani bir cevapsızlığı cevap olarak. Aynı işi yapan iki istemci
bu yüzden farklı satır yazıyordu. `Args::erase` geldi ve `journal_entry` boş
argümanı düşürüyor.

### Eklendi — gerçek işletim sistemi olaylarıyla sürülen probe

`KENTOS_OSCLICK_PROBE=<dizin>` pencereyi açık tutar, her araç düğmesinin ve
tuvalin ekran konumunu yazar, sonra dışarıdan gelen **gerçek** fare ve klavye
olaylarının ne yaptığını satır satır bildirir: hangi eylem tetiklendi, hangi
oturum başladı, hangi istem soruldu, nesne sayısı ne oldu, döküme ne düştü,
hangi kart açıldı. Sürücüsü `scripts/os-tikla.c` (CGEventPost) ve
`scripts/os-tikla.sh`; macOS ve Erişilebilirlik izni ister.

Bunun gerekçesi tek cümlede: **`osascript`'in `click at`'i fare değildir.**
Noktadaki öğeye erişilebilirlik "press" uygular; uygulamaya hiçbir
`MouseButtonPress` gelmez. Onunla yapılan bir deneme "araç kutusundaki hiçbir
şey çalışmıyor" sonucunu verir, çünkü hiçbir tıklama olmamıştır. Diğer bütün
probe'lar olayları süreç içinde üretir; bu, dışarıdan gelen olayın geçtiği yolu
— pencere sistemi, platform eklentisi, Qt'nin isabet testi — sınayan tek yoldur.

Altı şikâyetin hepsi bu yolla doğrulandı: `tetiklendi ÇİZGİ` → `nesne 1` →
`nesne 2` → "2 çizgi çizildi, her biri ayrı nesne"; YAY köşe işareti kartı
açıyor, seçilen üye `yanan YAY yontem=3n` olarak yanıyor ve sıradaki noktayı
soruyor; BLOKEKLE blok yokken açıklıyor; çizgiye tıklayıp **⌫** basınca
`1 nesne silindi.`

### Düzeltildi — ekran okuyucuyla basılan araç düğmesi artık komutu çalıştırıyor

Bir önceki maddede bulunan açık kapandı. Kolon düğmeleri `checkable`'dır, çünkü
kolonun işi hangi komutun çalıştığını söylemektir; Qt ise `checkable` bir düğmeyi
erişilebilirlik katmanına `QAccessible::CheckBox` diye verir ve ilk sunduğu eylem
`Toggle` olur — o da `QAbstractButton::toggle()`, yani `clicked` yaymadan sadece
işaret kutusunu çevirir. `QAction::triggered` yalnız `clicked`'den çıkar. Sonuç:
**düğme yanıyor, komut çalışmıyor.** Ekran okuyucu kullanan biri için her çizim
aracı yanıp hiçbir şey yapmıyordu.

Artık kolonun kendi düğme türü var (`ToolButton`) ve ona verilen erişilebilirlik
arayüzü (`ToolButtonAccessible`) **her eylemi `click()`'e bağlıyor** — farenin
gittiği yolun aynısı, yani hem komutu çalıştırır hem ışığı yerine koyar. Yanma
davranışının hiçbiri değişmedi: eylem hâlâ `checkable`, hâlâ `drawingTools_`
içinde ve `syncToolSelection` hâlâ onu işaretliyor. Ağaç da hâlâ "işaretli" diyor;
yanan düğmenin sesli karşılığı odur.

Gerçek macOS erişilebilirlik basışıyla doğrulandı. Öncesi tek satırdı —
`isaretlendi METİN`. Sonrası: `tetiklendi METİN` → `yanan METİN` →
`oturum core.text bekliyor=1` → `istem Yazının başlangıç noktası`.

Aynı partide **klavye yolu**: araç kutusu artık tek bir Tab durağı, içinde ok
tuşlarıyla gezilir, **Boşluk/Enter** aracı çalıştırır, **→** aile kartını açar
(kart zaten ok tuşlarına, Enter'a ve Esc'e cevap veriyordu; eksik olan onu
faresiz açmanın yoluydu). Odak halkası yalnız klavye odağında çizilir (`ui.md`
R31) ve düğmeler `Qt::NoFocus` kalır, çünkü fareyle bir araca basmak odağı
tuvalden almamalı. Her düğme `accessibleName` ve `accessibleDescription` taşıyor
(R22); aile düğmesi açıklamasında aile olduğunu da söylüyor, çünkü köşe işareti
bir resimdir ve ekran okuyucu onu göremez.

Kapıyı `tool-accessible` ctest'i tutuyor (`KENTOS_ACCESS_PROBE`): kolondaki her
düğme **erişilebilirlik eylem arayüzünden** sürülüyor ve iddia "bir şey oldu"
değil, **eylemin tetiklendiği**. Ayrım açığın kendisidir: özel `QActionGroup`
düğmeyi kendi başına yakar, yani yanan düğme komutun çalıştığının kanıtı hiç
olmamıştır. Düzeltme geri alınarak sınandı: eski davranışta 19 iddia kırmızı.

### Düzeltildi — araç kutusu gerçek kullanımda: ışık, kılavuz, alan, blok, silme

Mac'te gerçek fareyle çalışan bir kullanıcının raporu: "yeni çizim öğeleri
seçilince menüde seçili kalmıyor, çizerken kılavuz çizgileri gözükmüyor, alan
ölçme çalışmıyor, blok ekle anlamsız, nesneleri nasıl sileceğimi anlamadım,
çoklu çizgi ile çizgi aynı." Beşi de `KENTOS_REALMOUSE_PROBE` ile gerçek
pencerede kırmızı olarak yeniden üretildi, sonra yeşile döndü.

- **Yöntem araçları düğmesini yakmıyordu.** `YAY yontem=3n` gibi bir üye
  seçilince `syncToolSelection` düğmenin taşıdığı **tam satırı** komut adı
  diye çözmeye kalkıyor, bulamıyor ve hiçbir düğmeyi yakmıyordu; kullanıcı
  bunu "seçim bırakılıyor" diye görüyordu. `Controller` artık kurduğu satırı
  (`armedLine()`) tutuyor; eşleşme önce tam satıra, sonra ilk sözcüğe bakıyor.
  Işık ilk tıklamadan sonra da kalıyor ve lastik bant/kılavuz o araçta da
  çiziliyor.
- **`BLOKEKLE` çizimde blok yokken açıklıyor.** Eskiden boş bir seçici
  açılıyor ve ne istendiği anlaşılmıyordu. Şimdi önden söylüyor: blok bir kez
  çizilip çok kez yerleştirilen bir semboldür (rögar, direk, ağaç); önce
  nesneleri seçip `BLOK ad=<isim>` ile tanımlayın, `BLOKEKLE` onları
  yerleştirir.
- **Mac'te silme.** `SİL` artık **Del**'in yanında **⌫ (Backspace)** ile de
  çalışıyor — Mac klavyelerinin çoğunda Del tuşu yok. Komut satırına yazarken
  ⌫ karakter siler, nesne değil. `arayuz.md` tablosuna satır eklendi.
- **`ÇİZGİ` ile `ÇOKLUÇİZGİ` arasındaki fark** artık üç yerde söyleniyor:
  iki düğmenin ipucunda, `ÇİZGİ`'nin bitiş satırında ("2 çizgi çizildi, her
  biri ayrı nesne (tek nesne için ÇOKLUÇİZGİ)") ve `line.md`'deki karşılaştırma
  tablosunda. Üç tıklama iki komutta aynı görüntüyü verir; fark nesne
  sayısındadır ve bir parçaya tıklayınca ne seçildiğinde.
- **`ALANÖLÇ` fareyle** — probe bir alanın içine tıklayarak `alan: 200,00 m²
  çevre: 60,000 m` cevabını aldı; ölçüm çalışıyor, önceki sürüm cevabı
  dökümde en alta yazıyor ve döküm oraya kaymıyordu (bir önceki değişiklikte
  düzeltildi). Sorun kullanıcının elindeki eski derlemeydi.
- **Kart probe'unun "çalıştı" ölçütü** artık üç hâli sayıyor: soru bekleyen
  oturum, yanan düğme, **ya da dökümde bir cevap**. `BLOKEKLE`'nin önden reddi
  bu üçüncüsü; eski ölçüt onu "kart bir şey yapmadı" diye okuyordu.

### Düzeltildi — sadeleştirme elle yazılmıştı, artık Clipper2'nin

Planın adını verdiği 237 tanımlayıcı ağaçta tek tek arandı; 225'i vardı ve
bulunmayan 12'nin üçü gerçek iş çıktı. En önemlisi bu:

**`ÇİZGİDÜZENLE islem=sadelestir` kendi döngüsünü yürütüyordu.** Plan
"Sadeleştirme Clipper2 `SimplifyPath`" diyor ve CLAUDE.md 5.16 çözülmüş bir
problemi yeniden çözmeyi yasaklıyor — ama sebep kuraldan da somut:

Eski hâli her köşeyi, son **TUTULAN** köşeden sonrakine giden doğruya göre
ölçüyordu. Bu bir dik-uzaklık süzgeci ve sadeleştirme demek değil: bir köşenin
kalıp kalmaması, komşularından **hangisinin ondan önce kaldığına** bağlı oluyor,
yani aynı şekil yürüyüşün nereden başladığına göre başka iniyor. Sıra-bağımsız
olmayan bir sadeleştirme, şeklin bir özelliği değildir.

- `core::simplify_ring` Clipper2'ye devrediyor, kapalı/açık ayrımıyla: bir
  halkanın ilk ve son köşesi komşudur, açık bir dizinin uçları ise uçtur ve uç
  hiç atılmaz.
- **Her şeyi yiyen bir tolerans şekli geri verir**: çağıran bir yüzü
  inceltmek istedi, silmek istemedi.
- Dört çekirdek testi, ve **sıra bağımsızlığı testi eski hâli çürütüyor**: aynı
  testere dişi ileriden ve geriden aynı iniyor.

### Eklendi — SEÇ ÇOKGENPENCERE, tek anlamlı yazım

`ÇOKGEN` aynı zamanda bir **çizim** komutudur (düzgün çokgen), yani birini
çizip sonra `SEÇ ÇOKGEN` yazan kullanıcı tek sözcükle iki şey söylüyor. Planın
kullandığı uzun yazım artık çalışıyor; ikisi de aynı kipe gidiyor ve uzun olan
hangisi olduğunu söylüyor.

### Bilinsin — DİKAYAK'ın lastik bandı yapılmadı

Plan `DİKAYAK` için "taban çizgisi + dik ayak izi" lastik bandı istiyordu
(`RubberShape::Offset`). Bir lastik bant **imlecin** koyacağı noktayı önizler;
`ayak` ve `boy` ise `ctx.number` ile sorulur ve bir sayı Enter'a basılana kadar
**yoktur** — önizlenecek bir nokta yok. Ayağı tıklayarak vermek ayrı bir özellik
olurdu (sayı isteminde nokta almak) ve plan metninde o yok. Bugünkü hâl: taban
çizgisi lastik bantla veriliyor, sonra sayılar yazılıyor, ve komut satırı odağı
sayı istemlerinde kendiliğinden geliyor.

### Eklendi — POLİGON istasyonlarını numaralıyor

Plan bunu dört sözcükle istiyordu: *"noktalar `NOKTA` olarak, **numaralı**"*. Numara
hiç yazılmıyordu.

Bir poligon istasyonu, ondan sonraki **her detayın ölçüldüğü yerdir**: sonraki
komut `n(2)` der ve o istasyonu alır, nokta listesi onu numarasıyla yazar,
`APLİKASYON` numarayı geri okur. Numarası olmayan bir istasyon, bir mühendisin
atıfta bulunamadığı bir noktadır.

- Sütun **`nokta_no`** — [`NOKTALAR`](docs/komutlar/points.md)'ın okuyup yazdığı ve
  `n(…)` nokta fonksiyonunun çözdüğü sütunun aynısı. Aynı şeyi anlatan ikinci bir
  sütun açılmadı (5.10).
- **Varsayılan, çizimdeki en büyük numaranın bir fazlası.** Bir poligon bir işin
  tek ayağıdır ve önceki ayaklar numara kullanmış olur: 1'den yeniden başlayan
  ikinci bir güzergâh iki istasyona tek ad verir ve `n(2)` o zaman aramanın önce
  ulaştığını gösterir. `ilk_no=` ekibin kendi numaralamasını dayatır.
- `R12` ya da `NIR-3` gibi **ad** taşıyan noktalar sayılmaz: en büyük **sayaç**
  aranır, bir ad değil.
- **Çözülmüş `ilk_no` günlüğe yazılıyor**, ve varsayılanı güvenli kılan şey budur:
  başka numaralı noktalar taşıyan bir belgeye oynatılan bir günlük, bu
  istasyonları aynı şekilde numaralamak zorundadır (Article 1.4, model.md P4).
  Altın fikstür `ilk_no: 1` ve `ilk_no: 5` yazıyor.

### Eklendi — "tek yol" kapısı: hiçbir komut hangi istemcinin sorduğuna bakmaz

Planın ikinci ilkesi şöyleydi: *"Hiçbir komut `InputSource`'a bakmaz
(command.md P10)."* Denetimde **tutuyordu** — 71 komut gövdesinin hiçbiri
bakmıyor — ama onu koruyan hiçbir şey yoktu.

Bu kural bir bildirimle ölmez; **her seferinde bir `if` ile** ölür. Her biri
kendi başına makuldür ("arayüz zaten sordu, istemi atla") ve birlikte tek ad
taşıyan iki program olurlar — ve yalnız biri test edilmiş olur.

- `scripts/ci-gate-tek-yol.sh` komut GÖVDELERİNİ tarıyor:
  `/src/command/src/commands`, `/src/domain/*/src`, `/src/ai/src/commands`.
- **Bus bakabilir ve bakıyor**: kim sorduysa onu günlüğe yazıyor. Kim sorduğunu
  KAYDETMEK, kim sorduğuna göre DAVRANMANIN tam tersidir.
- Kapı **ihlal enjekte edilerek** doğrulandı: çıkış 1; temizken 0. Yorum
  satırlarını kod saymıyor — kendi gerekçesini açıklamayı yasaklayan bir kapı
  olmasın.
- Karar veren şeyin ne olduğu `command.md` P10'a yazıldı: **argümanın boşluğu**.
  `POLİGON` okumalarını `aci=` verilmediyse soruyor, verildiyse sormuyor; bu
  "etkileşimli miyim" değil "bana söylendi mi"dir, ve aynı gövde bir betiğe de
  bir ele de hangisi olduğunu bilmeden hizmet ediyor.

### Eklendi — YAY yontem=devam, ve inşa yöntemleri arayüze girdi

- **`YAY yontem=devam`** — planın kalan son inşa yöntemi. Bir yol geçiş eğrisi,
  bir bordür dönüşü ve zincirleme pahlar hep böyle çizilir: yay, en son çizilen
  çizginin ya da yayın ucundan **o ucun kendi doğrultusunda** ayrılır, böylece
  birleşme yerinde kırık olmaz. Bordürde bir kırık, yeniden dökülecek bir bordür
  demektir.
- **Erteleme sebebi yanlış çıktı.** Not "`Session`'a son segment yönü eklemek
  gerekiyor" diyordu; gerekmedi. `SEÇ SON` zaten "en son oluşturulan nesne"yi
  **belgeden** okuyor, ve doğrultuyu aynı yerden okumak aynı soruyu aynı yere
  sormaktır — geri alma, replay ve yeniden yükleme ile ayrı tutulacak bir durum
  parçası daha olmuyor.
- Bitiş noktası **teğetin üzerindeyse** oradan devam eden şey bir yay değil bir
  doğrudur ve komut bunu söylüyor, sıfıra bölmüyor. Günlüğe **çözülmüş yay**
  yazılıyor, yani replay BU yayı kuruyor — oynatıldığı belgede en yeni nesne ne
  olursa olsun (model.md P4).

### Eklendi — inşa yöntemleri artık birer araç (§2.6a)

P2 daire, yay, dikdörtgen, çokgen ve elipse klasik yöntemlerini verdi ve **hepsi
yalnız `yontem=` yazılarak** erişilebiliyordu: bir el üç noktadan daire
çizemiyordu. Komut bir düğmede olduğu için erişim probu memnundu ve **yöntem
değildi** — CLAUDE.md 5.15'in bir düzey aşağıdaki hâli.

- Her yöntem artık ailesinde **kendi satırı**: çapın iki ucu, üç nokta, iki
  doğruya teğet, başlangıç-merkez-açı, başlangıç-bitiş-yarıçap, teğet devam,
  döndürülmüş dikdörtgen, dıştan ve kenardan çokgen, eksenin iki ucu elips.
- **Tam satır düğmede duruyor**, yalnız ilk sözcük değil: kartın sağ kolonu
  `YAY yontem=3n` yazıyor, yani kart aynı zamanda komut satırını öğretiyor. Bir
  yöntemi fareyle bir kez kullanıp sonra yazmaya geçen kullanıcı, yazacağı şeyi
  zaten görmüş olur.
- Aile üyesi 26'dan **37'ye** çıktı ve `KENTOS_FLYOUT_PROBE` hepsini gerçek fare
  olaylarıyla basıyor: **0 kusur**.

### Eklendi — SEÇ tur= tür süzgeci, ve nokta fonksiyonu idempotens testi

Plan belgesinin her satırı ikinci kez okundu; TODOS'a hiç girmemiş üç madde
çıktı, ikisi iş.

- **`SEÇ tur=`** — plan "`SEÇ katman= tur=` süzgeçleri varsa docs'a, yoksa
  eklenir" diyordu: `katman` vardı, `tur` yoktu. Bir pafta üzerine atılan pencere
  parselleri, etiketleri, ölçüleri ve yol eksenini birlikte yakalar; *"o
  penceredeki alanlar"* bir harita mühendisinin sürekli sorduğu şeydir.
- **`katman=` bir kip, `tur=` bir süzgeç**, ve fark şudur: bir katman kendi başına
  bir küme adlandırır, bir tür ise bir kümeyi daraltır. Bu yüzden `tur=` her
  kiple — `ÇİT` ve `ÖNCEKİ` dâhil — birlikte çalışıyor.
- **Tür adı nesne türleri tablosunun kendi adıdır**: `DAİRE`, `daire`, `DAIRE` ve
  `CIRCLE` aynı türe gider, ikinci bir tür listesi yok, ve bir eklentinin
  tanımladığı tür eklendiği gün seçilebilir olur (5.10). Tanınmayan bir tür,
  tanınan türleri sayan bir retle karşılanıyor ve **seçime dokunulmuyor**.
- **Nokta fonksiyonu idempotens testi** — planın adıyla istediği ve olmayan test:
  "fonksiyon çıktısı tekrar girdi olarak aynı noktayı verir". Her fonksiyonun
  SABİT NOKTASI sınanıyor (`orta(P,P)=P`, `ara(A,B,0)=A`, `uzanti(A,B,0)=B`,
  `dik(A,B,0,0)=A`, `semt(S,θ,0)=S`, `xy(P,P)=P`, `ile(P,@0,0)=P`), sonra çıktı
  geri besleniyor ve iç içe yazılıyor. Bu fonksiyonlar iç içe yazılır —
  `orta(orta(A,B),n(1284))` bir mühendisin yazdığı satırdır — ve kendi cevabıyla
  kayan bir fonksiyon her düzeyde bir milimetre kayardı.

### Bilinsin — ölçü stilinin BÖHHBÜY/MPYY yarısı uydurulmadı

Plan P5 için "BÖHHBÜY/MPYY pafta metin yükseklikleri" istiyor. Katalog
mekanizması var ve `ÖLÇÜ` onu okuyor (5.13 karşılanmış: komutta tek bir yükseklik
sabiti yok), ama içindeki üç stil ISO 129-1 ve AutoCAD varsayılanları — dosyanın
`source` alanı bunu açıkça yazıyor.

Yönetmelikten geldiği söylenen bir yükseklik **uydurulmadı**: 5.13'ün yasakladığı
şey tam olarak budur, sadece dosyaya taşınmış hâli. Resmî görünen bir sayı,
olmayan bir sayıdan kötüdür. Kalan iş yönetmelik metni, madde/ek atfı ve bir
harita mühendisinin onayıdır (Article 6.11) — poligon toleranslarıyla aynı sınıf
ve aynı bekleyiş. Satır eklendiği gün `ÖLÇÜ stil=BÖHHBÜY` çalışır ve tek satır
C++ değişmez.

### Düzeltildi — DİKAYAK eşleşmeyen okuma dizisinde sessiz dönüyordu

Planın release listesi `DİKAYAK 0,0 100,0 30 -5` yazımını adıyla istiyordu. O
yazım **hiç çalışmıyordu ve hiçbir şey söylemiyordu**: iki dizi (`ayak`, `boy`)
arka arkaya bildirildiği için çıplak sayıların hepsi birincisine bağlanıyor,
ikincisi boş kalıyor, döngü ilk eksik yarıda kırılıyor ve komut geri dönüyordu.
Nokta yok, sebep yok — bir komutun verebileceği en kötü cevap, çünkü kullanıcının
düzeltecek bir şeyi yok.

- **Sayıları bölmek çözüm değil.** Hangisinin ayak hangisinin boy olduğuna karar
  vermek bir tahmindir, ve bir detayın taban çizgisinin hangi tarafına düştüğünü
  tahmin etmek bir yapının sınırın yanlış tarafına oturması demektir. Ret,
  eşleşme kuralını ve gelen iki sayıyı söylüyor, ve doğru yazımı gösteriyor.
- **Denetim tek bir nokta bile konmadan yapılıyor** (Article 1.6): üç `ayak` ile
  iki `boy` eskiden tam olan iki çifti koyup üçüncüyü **sessizce düşürüyordu** —
  yanlış aktarılmış bir karne satırının ölçüden kaybolma yolu.
- **`ALIM` aynı denetimi aldı.** Onun konumsal yazımı zaten bus'ın doğrulaması
  tarafından adıyla reddediliyordu (üçüncü parametresi bir nokta listesi:
  bağlama noktası), ama kendi eşleşmeyen dizisi aynı sessizliğe düşüyordu.
- **Esc sessiz kalıyor**, ki öbür yarısı bu: vazgeçmek bir hata değildir ve fikir
  değiştiren kullanıcıya bir şey söylenmez. İkisi de testli.

### Eklendi — günlük oynatma özelliği, ve bulduğu iki oynatılamaz komut

Article 6.4 her komut için bir günlük-oynatma vakası istiyor. Komut başına vaka
yerine **özellik** yazıldı: her altın senaryonun günlüğü boş bir belgeye yeniden
uygulanıyor ve aynı çizimi kurmak zorunda; ayrıca her günlük satırı
`to_json` → `from_json` turunu bayt bayt geçmek zorunda (§2.2). Senaryolar çizim
yüzeyinin büyük kısmını kullanıyor, ve bir özellik testi yazılmayı bekleyen bir
vaka değildir.

**İlk çalıştırmada iki komut oynatılamaz çıktı:**

- **`ELİPS yontem=eksen` kendi günlüğünden oynatılamıyordu.** Ortak kuyruk
  `birinci`'yi koşulsuz kaydediyor ve `eksen` yönteminde **birinci** eksen ucunu
  **ana** eksen ucuyla eziyordu — satır, iki ekseni ucunun aynı nokta olduğunu
  söyleyerek gidiyor ve oynatma "Eksenin iki ucu aynı nokta" diye reddediyordu.
  Oynatılamayan bir kayıt kayıt değildir (Article 1.4).
- **`ÖLÇÜ tur=acisal` kendi yazdığı sözcüğü okuyamıyordu.** Komut modelin kararlı
  adını kaydediyor (`acisal3`), ayrıştırıcısı ise `acisal` ve `angular`'dan
  başkasını tanımıyordu: açısal ölçü sessizce hiç oynatılmıyor ve altın senaryo
  bir nesne eksik kuruluyordu. `acisal3` ve `angular3` artık kabul ediliyor — ad
  tablosundan **türetilmedi**, çünkü `acisal` öbür açısal türün (beş noktalı
  `Angular`) kararlı adı ve türetmek bir sözcüğü iki türe bağlardı.

**`GERİAL` içeren senaryo muaf ve sebebi yazılı**: undo `ReadOnly` olduğu için
günlüğe düşmez — günlük belgeye ne YAPILDIĞINI tutar, undo ise yığının bir
hamlesidir, çizimin değil. Günlüğünden undo çıkarılmış bir oynatma, kullanıcının
geri aldığı düzenlemeyi yeniden uygulardı. Muafiyet senaryoyu tarayıp `core.undo`
arayarak veriliyor, elle listelenerek değil, ve yüksek sesle bildiriliyor.

### Eklendi — iptal özelliği: Esc her komutta boş delta bırakır

`Registry` üzerinde döngüyle ~64 etkileşimli komut: her biri Esc'te belgeyi ve
geri alma yığınını olduğu gibi bırakıyor. Esc bir CAD programında en çok basılan
tuştur — kullanıcı araca uzanır, yanlış araç olduğunu görür ve Esc'e basar. İlk
isteminden önce yazan bir komut her seferinde yarım bir düzenleme bırakır, hem de
**sessizce**, çünkü kimse değiştirmemeye karar verdiği çizime bakmaz.

Komut başına vaka değil, çünkü bir vaka yazılmayı bekler: döngü, yarın eklenen
bir komutu bildirildiği gün kapsıyor.

### Eklendi — yakalama idempotens testi, ve iki belgenin yanlış cümlesi

- **`snap(snap(p)) == snap(p)`**, planın adıyla istediği ve hiç var olmayan test.
  Bir noktayı ikinci kez çözmek göründüğünden sık olur: yeniden dispatch,
  günlük replay, bir betiğin bütün noktaları sırayla çözmesi, birkaç hedefli bir
  `KOPYALA`. Zaten yakalanmış bir noktayı oynatan bir mod, aynı komutun ikinci
  çalıştırmada başka bir çizim vermesi demektir — ve kayma bir milimetre olur,
  yani bir parsel kapanmayana kadar kimsenin görmediği boy.
- **Maske üzerinde döngüyle** yazıldı, mod başına vaka değil: motora eklenen bir
  mod bildirildiği gün kapsanıyor. Tutmayan bir mod **sayılıp bildiriliyor**;
  sessiz bir atlama, özelliği kanıtlanmamış bir modu kanıtlanmış saymaktır.
  `ekleme` tek istisna ve sebebi yazılı — hiçbir yerleşik tür henüz ekleme
  noktası yayınlamıyor, yani cevaplayacağı bir şey yok. **Tutan her mod
  idempotent çıktı.**
- **`UÇUCA` ile `BİRLEŞTİR` artık iki sayfada yan yana.** `join.md` "her iki sayfa
  öbürünü adıyla anar" diyordu ve `combine.md` `UÇUCA`'yı hiç anmıyordu.
- **Ve düzeltirken yazdığım ilk tablo da yanlıştı**: BİRLEŞTİR'in çizgide "yalnız
  tam değen uçları eklediğini" yazmışım, oysa **projenin** düğüm toleransını
  okuyor (varsayılan 10 mm). Fark toleransın nereden geldiğidir — `UÇUCA`'da
  çağrının kendi `tolerans=`'ı, `BİRLEŞTİR`'de projenin ayarı. Üç vakalı bir test
  bunu çiviliyor. İki belge yanlış bir cümleyi yayımlamak üzereydi ve testi
  yazmak onu ilk çalıştırmada çürüttü.

### Eklendi — inşa yöntemleri altın fikstürü ve F3 listesinin kanıtı

- **`tests/golden/senaryolar/insa-yontemleri.txt`** — plan `3n` daire için bir
  altın fikstür istiyordu; P2'nin **bütün** inşa yöntemleri girdi: 3n/2n/ttr
  daire, 3n/bby×2/bma×2 yay, ic/dis/kenar/açılı çokgen, 3n dikdörtgen, eksen
  elipsi. Her biri trigonometri içeriyor, yani her biri üç platformda bit-özdeş
  olmak zorunda (§7.3): bir paftaya çizilen çevrel çember yarım milimetre kayarsa
  o pafta iki makinede iki farklı belge olur.
- **Her sayı elle doğrulandı**: 3-4-5 üçgeninin çevrel çemberi (30, 40) ve r=50;
  altıgenin dış yarıçapı 30/cos30 = 34,641; sekizgenin çevresi tam 160 m; 50
  grad'lık üçgenin ilk köşesi kuzeyden 45° ile (321213, 621213); 3n dikdörtgenin
  alanı tam 2400 m²; elipsin alanı tam π·1000 m².
- **Fikstür üretilip okunduğu için bir kusur çıktı**: elipsin ikinci nokta
  parametresini ana eksenin üzerinde vermişim, komut sessizce hiçbir şey
  çizmemiş ve 14 nesne yerine 13 gelmişti. Bir altın fikstürü üretip bakmadan
  kabul etmek, yanlış bir beklentiyi üç platforma birden yaymaktır.
- **F3 yakalama listesi artık gerçek ikiliden doğrulanıyor**: `probeMenus`
  yakalama açılır menüsünü de yazıyor ve satır sayısını motorun mod sayısıyla
  karşılaştırıyor. 18 mod listeleniyor, **çeyrek nokta ve teğet nokta dâhil** —
  aralık düzeltilene kadar bu ikisinin bitleri ayardan yazılamıyordu.

### Eklendi — eksik eşitlik kanıtları, ve onların ortaya çıkardığı üç kusur

Planın **Doğrulama** bölümü "her yeni/değişen komut için `test_proof.cpp`
vakası" diyor (Article 6.4). Denetleyince P2'nin üç yöntemi, P3'ün yedi fiili,
P4'ün seçim kipleri, P5'in ordinat ölçüsü, P6'nın panosu ve `İZ` kanıtsızdı.
On yedi vaka yazıldı — ve **üç kusuru ortaya çıkardılar**, ki kanıtların
varlık sebebi tam olarak bu.

- **`SEÇ ÇİT` ve `SEÇ ÇOKGEN` fareyle tek nokta toplayabiliyordu.** Döngünün
  koşulu `while (supplied.empty())` idi, yani BİRİNCİ noktadan sonra çıkıyordu;
  komut sonra "Çit en az iki nokta ister" diye reddediyordu. Fareyle
  ulaşılabilen ama fareyle kullanılamayan bir kip (5.15) — ve `select.md` zaten
  "tıklamaya devam edersiniz, sağ tık bitirir" diye **yazıyordu**. Koşulun
  arkasındaki niyet doğruydu, yazımı değil: baştan nokta verilmiş bir betiğe
  sorulmamalı, çünkü sormak aynı noktaları ikinci kez topluyor ve kendi üzerine
  katlanmış bir çokgen hiçbir şeyi içermiyor. Niyet artık bir bayrakla yazılı.
- **`UZUNLUK` ve `BÖLÜMLE` okumalarını sormuyordu** — POLİGON'la aynı kusur.
  Araç kolonundan basınca nesneyi soruyor, sonra "delta= yazın" / "sayi= yazın"
  diye reddediyorlardı. Artık soruyorlar: `delta` ve `sayi`, yani elin sorduğu
  sorular. `yuzde`, `toplam` ve `aralik` yazılan yollar olarak kalıyor, çünkü bir
  sayı hangisi olduğunu söyleyemez. Argümanın boşluğu karar veriyor, `InputSource`
  değil (command.md P10).
- **Günlükte bir tam sayı, geldiği yola göre iki türlü yazılıyordu**: komut
  satırından `delta=25` → `25.0`, betikten `"delta": 25` → `25`. Aynı sayı, iki
  bayt dizisi. Kayıt artık **bildirilen türe** çevriliyor — sıra düzeltmesinin
  yanında, aynı satırda ve aynı sebeple. Yalnız sayısal bir parametredeki skaler
  değere dokunuluyor; bir sözcük, bir nokta, bir seçim ve bir liste geldiği gibi
  kalıyor, ve **hiçbir altın fikstür değişmedi**.

Yedi P3 fiilinin kanıtı **tek bir şekilde** yazıldı (`prove_verb`): yedi kez
kopyalanmış bir kanıt, altısında kayan bir kanıttır.

### Düzeltildi — günlük satırı artık yazım sırasına bağlı değil

Article 6.4'ün öbür yarısı. `Args` ekleme sırasını tutuyor ve `to_json` onu
basıyordu, yani `ÇOKGEN yontem=ic 0,0 6` ile `ÇOKGEN 0,0 6 yontem=ic` — bir
çağrının iki yazımı — **iki farklı günlük satırı** yazıyordu.

Üç istemciyi bir YAZIM sırasında anlaştırmak mümkün değil: `KILAVUZ yon=45g` ile
başlayıp noktayı sonra soran bir araç gerçekten `yon`'u önce bağlar, yazılan satır
ise konumsal noktayı öne koyar, betik ise JSON'un kendi sırasını verir. Üçünün
paylaştığı tek şey **bildirilen parametre sırasıdır**, ve kayıt artık onunla
yazılıyor (`Args::reorder_like`, `Bus::journal_entry`).

Dört altın fikstür yenilendi ve değişimin **yalnız anahtar sırası** olduğu
doğrulandı: her satırın JSON'u eskisine eşit, hiçbir değer kıpırdamadı. Bir kanıt
vakası bağı kuruyor ve beklenen sırayı `Registry`'den okuyor — elle yazılmış bir
sıra, taşınan bir parametreyle sessizce yanlışa düşerdi.

### Eklendi — geçici izleme (OTRACK): İZ ve izlerin kesişimi

Planın son maddesi. Bir çizimin kendi başına cevaplayamadığı en yaygın aplikasyon
sorusu: **şu köşeyle aynı hizada, bu köşeyle aynı doğrultuda bir nokta.** Cevabın
yerinde hiçbir geometri yoktur ve iki köşenin hiçbirinde oraya işaret eden bir şey
yoktur; nokta yalnızca o iki köşe var olduğu için vardır.

- **`İZ`** (`core.tracking`; `IZ`, `TRACK`, `TRK`) bir noktayı işaretler. İşaretli
  her noktadan yatay ve düşey bir iz geçer; iki işaretin izleri **kesişir**.
- **Şeffaf**, ve bütün tasarımı bu: işaret başka bir komutun ortasında konur —
  `ÇİZGİ`, sonra "Sonraki nokta:" isteminde `İZ 12,8`, sonra kesişime tıklamak.
  İşaretler `Bus` üzerinde durur, çünkü bir işaret onu koyan **komuttan uzun
  yaşar**.
- **Fareyle Shift + sağ tık**, nokta isteminde. İşaretlenen nokta **yakalanmış**
  noktadır, piksel değil: köşeyi tam almak jestin tamamıdır. Jest komuttan geçer,
  doğrudan motora değil — fareyle konan işaret ile yazılan işaret tek şey olmalı.
- **Kesişim tek izi yener.** İki işaret koyan kullanıcı onların belirlediği noktayı
  hedefler; tek bir iz her zaman bir eksende daha yakındır, yani ikisi yalnız
  mesafeye göre yarıştırılsa kesişim **hiç yakalanamazdı**. Aynı sıralama
  `KILAVUZ`'un ve aynı sebeple.
- **En çok iki işaret**: üçüncü en eskisinin yerini alır, çünkü üç işaret üçüncü bir
  eksen değil yeni bir çifttir. Aynı noktayı iki kez işaretlemek bir kez
  işaretlemektir — iki özdeş işaretin kesişimi işaretin kendisi olurdu.
- **Oturum durumu, belge değil** (model.md R43): hash'lenmiyor, günlüğe düşmüyor,
  geri alma adımı yemiyor. Bir işaret iskeledir.
- Varsayılan olarak **açık** ve kimseden bir şey almaz: işaret yoksa kip hiçbir şey
  yapmaz, ve bir iz gerçek olan her şeyin **altında** sıralanır.
- Yazılı karşılığı `xy(P,Q)` duruyor ve aynı noktayı verir: biri iki noktayı
  birlikte alıp kesişimi hesaplar, öbürü tek tek işaretleyip motora hesaplatır.
  Tek kural, iki yol (Article 1.2).

### Düzeltildi — iki yakalama modu ayardan ulaşılamıyordu

Yan yolda çıktı: `core.yakalama.modlar` ayarının **aralığı** `0x3FFFF`'te (17.
bit) duruyordu, oysa ÇEYREK (1<<18) ve TEĞET (1<<19) P4'te bildirilmişti. `MOD
yakalama_modları` o değerleri reddediyor, F3 listesi iki modu açamıyordu — yani
P4'ün eklediği iki mod, nasıl yazılırsa yazılsın ayardan **ulaşılamazdı**. Aralık
`SnapAllMask`'i kapsayacak şekilde genişletildi ve bir test bağı kuruldu: 21.
biti ekleyen kişi o satırı da genişletmek zorunda.

### Eklendi — işletim sistemi panosu ve BÖLÜMLE blok=

Planın kalan iki maddesi, ikisi de "kendi commit'ini hak ediyor" koşuluyla
bekliyordu.

- **`KES` / `PANOYAKOPYALA` / `YAPIŞTIR` artık işletim sistemi panosunu
  kullanıyor**, MIME türü `application/x-kentoscad-project`. Kanca
  `io::FileService` üzerinde iki `std::function`: biri yazılan yükü app'e verir
  (app okur ve sisteme sunar), öbürü yapıştırma öncesi app'e sorar (sistemde
  bizim türümüzden bir yük varsa okuyucunun bakacağı yola yazar). `/src/io` Qt
  bağlamamaya devam ediyor (Article 3.2) — Qt tarafı pencere katmanında,
  `Controller`'da.
- **Sistemde tutulan yük kazanır.** Başka bir pencerede kopyalayan kullanıcı
  ONU bekler, bu sürecin temp dizininde bıraktığı eski yükü değil. Kullanıcı bir
  dosya adı verdiyse panoya dokunulmaz: istemediği bir yan etki olurdu.
- **Uçtan uca kanıt** (`KENTOS_CLIP_PROBE`, `os-clipboard` ctest'i): iki parsel
  kopyalanıyor, **geçici dosya siliniyor**, yeni çizim açılıyor ve yapıştırma
  yine iki nesne getiriyor — yani yük yalnız sistem panosundan gelebilir. `/tests`
  Qt bağlamadığı için bu yarıyı başka hiçbir şey göremez.
- **`BÖLÜMLE blok=` ve `hizala=`.** Bir güzergâh boyunca direk, rögar, ağaç ya da
  bordür işareti dizmek bu parametrenin bütün varlık sebebi; her istasyona tek tek
  `BLOKEKLE` yazmak aynı işi elle yapmaktır. `hizala=evet` bloğu üzerinde durduğu
  kenarın doğrultusuna çevirir (`atan2_udeg`, libm değil), varsayılan kapalı —
  dik çizilmiş bir blok istenmedikçe dik kalır. Blok önceden tanımlı olmalı:
  burada yeni tanım üretmek `BLOK`'un işini ikinci bir yerde yapmak olurdu (5.10).
  Planın `İŞARETLE`si ayrı bir komut olarak **yazılmadı** ve yazılmayacak: aynı
  işin ikinci adı, ikisini de kullanılmaz kılar.

### Eklendi — açılı kılavuz: `KILAVUZ yon=<açı> nokta= tur=isin` (P2-5)

Planın ertelenmiş son maddesi, ertelemenin gerektirdiği **veri göçüyle** birlikte
(CLAUDE.md 0.2a: bir göçtür, refactor değil).

- **Model alan kazandı, anlam değiştirmedi.** `core::GuideStore` altı sütuna çıktı
  — eksen, koordinat, açı, geçtiği noktanın iki koordinatı, ışın — ve
  `core::GuideRow` bir satırı dikişlerde değer olarak taşıyor. Yan yana altı
  parametre, bir çağıranın enlemi boylamın yerine geçirme biçimidir.
- **Saklanan açı matematik mikro-derece**: doğudan saat yönünün tersine,
  `atan2_udeg`'in kendi birimi. Saklanan sayı ile yakalama testi arasında hiçbir
  dönüşüm yok; kullanıcının yazdığı ve listenin yazdığı açı **kenarda bir kez**
  çevriliyor. Bir yerde grad, öbüründe derece okuyan program yoktur.
- **Açı oturumun kuralıyla okunur** ve sonek onu geçersiz kılar: `yon=50`
  varsayılanda 50 grad, `yon=50d` 50 derece, `yon=0.7r` 0,7 radyan — `@mesafe<açı`
  ile aynı harf, aynı anlam. Günlüğe **birimiyle** yazılır: `50` yazan bir satır
  bugün bir yönü, `core.aci.birim` değiştikten sonra başkasını gösterirdi
  (Article 1.4, ve bir test bunu kanıtlıyor).
- **`tur=isin` tek yönlüdür** ve yakalama bunu bilir: ışının gerisinde çizgi
  yoktur, yani imleç noktanın arkasındayken **noktanın kendisine** oturur. Bir
  köşeden tek yöne çekilmiş kılavuzun elde verdiği şey budur.
- **Dosyada dört isteğe bağlı blok** (`0x0092`–`0x0095`), **yalnız açılı kılavuz
  varsa** yazılıyor: yalnız cetvel kılavuzu taşıyan bir çizim bu sürümden önceki
  yapının yazdığı baytların aynısını yazar. Açılı kılavuz taşıyan dosya
  `min_reader_version`'ı yükseltir, çünkü `kBlkGuideAxis` **eski** bir blok ve `2`
  değeri yeni — eski bir okuyucu o sütunu bozuk sayardı, yani doğru bir ret
  yanıltıcı bir gerekçeyle. Sürümü yükseltmek reddin ne olduğunu söylemesini
  sağlar. İki io testi her iki yarıyı da çiviliyor.
- **Tuvalde ekran uzayında uzatılıyor, dünyada değil.** On bin kilometreyi dünyada
  koşup görünüme çevirmek hiçbir float'ın taşıyamadığı bir ekran koordinatı verdi
  ve kalınlaştırma hiç çizmedi: ilk denemede 45° kılavuz karede yoktu. Yön
  dünyadan bir kez alınıyor, uçlar sınırlı bir piksel sayısı kadar uzatılıyor.
- `model.md` **R47–R47d** eklendi (kılavuz kuralları ilk kez yazılı hâle geldi);
  `docs/komutlar/guide.md` açı kuralı, doğru/ışın ve dosya biçimi bölümleriyle;
  **Çizim** menüsünde `Açılı Kılavuz` satırı.

### Düzeltildi — iki eski uyarı

Tam yeniden derlemede çıktı, ikisi de bu oturumun kendi satırlarından:
`file.cpp`'de pano fiilleri bir `switch`'te eksikti (`-Wswitch`) ve
`pick.cpp`'de `SEÇ SON` döngüsü `size_t`'i `EntityId`'ye daraltıyordu
(`-Wshorten-64-to-32`). Article 6.3 sıfır uyarı istiyor; ikisi de kapandı.

### Düzeltildi — POLİGON okumaları sormuyordu; ve P1b'nin eksik üç eşitlik kanıtı

- **`POLİGON` açı ve kenarı yalnız argümandan okuyordu.** Gerekçe kodun içinde
  yazılıydı: "bir poligon ölçü karnesinden aktarılır, tıklanmaz." Bu, sayıların
  NEREDEN geldiği için doğru ve nasıl İÇERİ GİRDİĞİ için yanlıştı — araç
  kolonundan `POLİGON`'a basan kullanıcı iki bilinen noktayı veriyor ve sonra
  "aci= ve kenar= gerekir" cevabını alıyordu. Fareyle ulaşılabilen ama fareyle
  bitirilemeyen bir komut, CLAUDE.md 5.15'in yasakladığı şeydir. Artık `ALIM`'ın
  kalıbıyla istasyon istasyon soruyor; sağ tık ya da `Esc` karneyi kapatır. Argüman
  verildiyse hiç sorulmaz, çünkü kararı veren şey **argümanın boş olup olmadığı**,
  `InputSource` değil (command.md P10).
- **P1b'nin eşitlik kanıtı beşe tamamlandı** (Article 6.4). `DİKAYAK` ve `ALIM`
  yazılıydı; `KESİŞİMNOKTA`, `ARANOKTA` ve `POLİGON` bu turda geldi. `POLİGON`
  için ayrıca günlük replay'i **kesirli kenarlarla** — 42,315 m ve 56,72 m, yani
  günlükten bir zamanlar 42 ve 56 olarak dönenler.
- **Bulundu, düzeltilmedi, sebebi yazılı:** günlükteki anahtar SIRASI istemciye
  değil yazım sırasına bağlı. `yontem` noktalardan önce yazılınca ve sonra
  yazılınca tek çağrının iki bayt dizisi oluyor; belge ve replay ikisinde de
  özdeş. Kaydı bildirilmiş parametre sırasına göre kanonikleştirmek saklanan her
  altın fikstürün günlük baytlarını değiştirir — kendi değişikliği olan bir karar,
  bir yan etki değil (TODOS-CAD).

### Eklendi — ESNET: pencere içindeki köşeleri taşıma

Planın P3 paketinde ertelenmiş son fiil. Yol genişlediğinde kenarındaki parselin
bir tarafını çekip öbür tarafını tapudaki yerinde bırakmak için.

- **Erteleme sebebi yeniden okunarak karşılandı.** Erteleme notu şöyle diyordu: bir
  köşe SEÇMEK, bu programda olmayan bir altyapı gerektiriyor — seçim nesne
  düzeyinde çalışıyor, köşe düzeyinde değil. Ama **pencere zaten süzgecin
  kendisidir**: içindeki köşe gider, dışındaki kalır, hiçbir köşesi içinde olmayan
  nesneye hiç dokunulmaz. Bunun için hiçbir şeyin seçilebilir olması gerekmiyor,
  yani köşe-düzeyi bir seçim modeli kurmadan oldu.
- **Çokluçizgi tek yazımda, diğer her tür tutamak tutamak.** Bir çokluçizginin
  köşeleri onun şeklidir ve pencereye giren hepsi tek `set_geometry` ile gider —
  yarısı taşınmış bir halka hiçbir zaman denetleyiciye sunulmaz. Bir dairenin,
  yayın, elipsin, ölçünün ve blok referansının sakladığı noktalar bir TANIMDIR ve
  `core::move_grip` tablosundan geçer; o tablo, o türün tutamağının ne anlama
  geldiğini bilen tek yerdir.
- **Her hedef, hiçbir şey kıpırdamadan önce hesaplanıyor.** Dairenin merkezi zaten
  yarıçap kolunu beraberinde taşıdığı için, canlı geometriye bakan bir ikinci taşıma
  kolu iki kez taşır ve daire öteleme kadar büyür. Anlık görüntüye göre ikinci
  taşıma kolu bulunduğu yere koyar: penceresine tamamen giren daire **ötelenir**,
  yalnız yarıçap kolu girerse **boyutlanır**. İkisi de testli, çünkü ikisi de aynı
  kuralın iki yarısı.
- **Günlüğe çözülmüş kimlikler yazılıyor**, pencerenin şansı değil: bir replay, bu
  çalıştırmanın esnettiğini esnetmelidir — oynatıldığı belgede aynı pencerenin
  altında başka şeyler durabilir (model.md P4).
- Arayüzde **Değiştir** menüsünde ve araç kolonunda, `TAŞI` ve `BUDA` ile aynı
  münhasır grupta: kollanıp beklerken kolon onu yakabiliyor.

### Düzeltildi — kilitli katman üzerindeki nesneyi korumuyordu

ESNET yazılırken çıktı ve plandan büyük: **kilit yalnız her `add_*` üzerinde
denetleniyordu ve başka hiçbir yerde.** Kilitli bir katman kullanıcının oraya YENİ
bir parsel çizmesini engelliyor, üzerinde DURAN her parseli `TAŞI`, `KÖŞETAŞI`,
`ESNET`, `DÖNDÜR`, `ÖLÇEKLE`, `PATLAT` ve `SİL` ile yeniden şekillendirmeye ya da
silmeye izin veriyordu. Kadastro çiziminde bu tam tersidir: **kilit, sayfada duran
şey için vardır.**

- Denetim `Document::editable()`'a girdi — her yerinde düzenlemenin zaten sorduğu
  tek soru, yani tek yer.
- Silme için `Transaction::erase_entity`'ye, çünkü `Document::set_entity_alive`
  aynı zamanda silmenin **geri alma** yoludur: oraya konan bir kilit denetimi,
  kullanıcı katmanı kilitlediği anda daha önceki bir silmeyi geri alma yığınında
  hapsederdi.
- `restore_geometry` ve öbür ters yollar bilerek denetimsiz kaldı: **kilitlemek
  geçmişi dondurmaz.** İki test bunu da çiviliyor.
- Reddin biçimi bu ağacın kendi kalıbı: komut işi yapamayacağını **kullanıcının
  dilinde söyler** ve hiçbir şeyi değiştirmez. Türkçe açıklanmış bir ret,
  programcıya yazılmış bir doğrulama mesajıyla ikiye katlanmaz (`Bus::finish`,
  `declined` yolu).

### Eklendi — iki soru: NESNEBİLGİ ve AÇIÖLÇ (P7)

Planın son paketi. İkisi de **soru**dur: hiçbir şeyi değiştirmez, geri alma adımı
yemez, günlüğe belge değişikliği olarak düşmez.

- **`NESNEBİLGİ`** (`core.entity_info`; `NESNEBILGI`, `OBJINFO`, `NB`) "bu nedir"
  sorusuna tek soruda cevap verir: tür, katman, halka ve köşe sayısı, çevre, alan,
  kapsam ve **dolu olan her öznitelik hücresi**. Bunların hepsi programda zaten
  vardı ve hiçbiri tek soruda alınamıyordu — alan `ALANÖLÇ`'te, katman panelde, ada
  numarası öznitelik tablosunda; bir bakış için üç yol.
- **Tür adı, tür tablosundan okunur.** Komutun içinde `kPolylineKind → "çizgi"`
  diye bir `switch` yazmak, türlerin ikinci listesi olurdu (5.10) ve bir eklentinin
  tanımladığı ilk türde sessizce "bilinmeyen" yazardı.
- **`AÇIÖLÇ`** (`core.measure_angle`; `ACIOLC`, `MEASUREANGLE`, `AÇÖ`) bir tepe ve
  iki kol noktası alır. `ÖLÇÜ tur=acisal` paftaya **ölçü nesnesi çizer**; bu komut
  elin şeritmetreyle sorup unuttuğu soruyu sorar.
- **Bir köşe iki açıdır** ve ikisi de yazılır. Süpürme oturumun kuralının kendi
  yönündedir; tersi yanında durur. Program hangisinin kastedildiğine sessizce karar
  vermez, çünkü bu karar bir sınırın hangi tarafının *iç* olduğuna karar vermektir.
- **Yazan sayı ile okunan sayı tek ayar çiftidir** (`core.aci.birim`,
  `core.aci.kural`): `@mesafe<açı` neyi anlıyorsa `AÇIÖLÇ` onu yazar.
- **Kısaltma `AÖ` değil, `AÇÖ`.** `AÖ` `ALANÖLÇ`'ün ve onu almak `ALANÖLÇ`'ü
  **programdan düşürüyordu**: çakışan bir kayıt yalnız bir günlük satırı yazıp devam
  ediyor. Derleme temiz, takım yeşil, `ALANÖLÇ` yok. Kayıt sayısı tripwire'ı
  (`tests/unit/test_command.cpp`) tam bunu yakaladı — 87'den 89'a çıkması gerekirken
  88'de kaldı.
- **İkisi de arayüzde**: **Harita** menüsünde `Sorgula` ve `Ölç` satırlarının yanında,
  ve araç kolonunun ölçüm ailesinde — yani fareyle kollanıp tuvalde cevaplanabiliyor
  (`KENTOS_FLYOUT_PROBE`: `Ölç` ailesi 5 üye, 0 kusur; `KENTOS_REACH_PROBE`: 115
  komuttan 115'i fareyle başlatılabiliyor).
- İkisi de `AiAccessible`: bir ajan neyin ne olduğunu sorabildiği için çizim
  hakkında kendisine anlatılmadan konuşabilir. Noktalar ajan yolunda yalnız araç
  sonucu tutamağıyla gelir (5.8).

### Düzeltildi — fareyle sorulan bir sorunun cevabı kayboluyordu

Üçü de P7 sırasında çıktı, ikisi Article 1.2 kusuru:

- **`Bus::finish` yapılandırılmış cevabı taşımıyordu**; yalnız `Bus::dispatch`
  taşıyordu. Araç kolonundan kollanıp fareyle cevaplanan bir sorgu çağırana `report`
  boş dönüyordu — yazılan yolun sahip olduğu bir yeteneğe işaret edilen yol sahip
  değildi (5.15), hem de insanın gerçekten kullandığı istemcide.
- **Komut günlüğü paneli en yeni satıra kaymıyordu.** `appendPlainText` belgeye
  yazar, görünümü olduğu yerde bırakır; gizliyken yüz satır gelen bir sekme öne
  kırkıncı satırı göstererek çıkıyordu. Yani sorgu çalışıyor, cevabı yazıyor ve cevap
  onu göstermek için yeni açılmış panelin dışında kalıyordu. Kuyruğu ancak kuyruk
  zaten görünürken takip eder — geri kaydırıp bir şey okuyan kullanıcının sayfasını
  program elinden almaz.
- **`modifyTool` ile kurulan okuma-amaçlı araçlar paneli hiç açmıyordu**, `ALANÖLÇ`
  dâhil. Karar artık `Registry`'ye soruluyor (`MainWindow::answersInWords`), her çağrı
  yerinde tekrar edilen bir bayrağa değil: okuma-amaçlı olan yeni bir komut, paneli
  kimse hatırlamadan alır.

### Eklendi — pano: KES, PANOYAKOPYALA, YAPIŞTIR (P6)

Araç çubuğundaki üç yer tutucu gerçek oldu.

- **Yük, çizimin kendi biçimidir.** `KAYDET`'in yazdığı proje dosyasının kendisi:
  aynı yazıcı yazar, aynı okuyucu okur. Kendine ait bir JSON taşıyan bir pano, bir
  belgenin ikinci tarifi olurdu ve bu programda her türü, her stili, her katmanı
  ve her öznitelik sütununu gidiş-dönüş taşıyan bir tarif zaten var. Bu yüzden
  panoya **her şey** gidiyor: katman adı, stil, çizgi tipi, öznitelikler, blok
  tanımları ve koordinat sistemi.
- **Alt küme, `Transaction::adopt_from`'a eklenen bir anahtar filtresiyle
  çıkarılıyor** — bir ithalatın kullandığı fonksiyonun aynısı. İkinci bir
  kopyalayıcı yazmak, onun çizgi tiplerini, resimleri, iç stilleri, katmanları,
  blokları ve sütunları baştan öğrenmesi ve bir tür bunlardan birini kazandığı gün
  geride kalması demek olurdu (5.10).
- **İki yeni fiil `FileRequest`'e eklendi**, kendi seam'i açılmadı: bu programa
  bir çizim sokan ya da ondan çıkaran ne varsa tek bir seam, yani ters gitmesi
  için tek bir yer.
- **KES tek işlemdir**: kopyalama ve silme birlikte geri döner, ve geri alma
  panoyu **boşaltmaz** — bir kesmenin bütün amacı budur.
- **YAPIŞTIR iki şekilde koyar**: `nokta=` yükün sol alt köşesini oraya taşır
  (bir elin yapıştırmaktan anladığı şey), `yerinde=evet` koordinatları olduğu gibi
  bırakır (aynı sistemdeki iki çizim arasında kopyalamanın istediği şey).
- **Pano nerede:** `dosya=` verilmezse kullanıcı başına ortak bir dosya, yani bu
  programın iki penceresi aynı panoyu paylaşıyor ve bir çökme yükü kaybetmek
  yerine yerinde bırakıyor. `dosya=` betiğin ve başsız çalıştırmanın yolu ve aynı
  yol.
- `Ctrl+X` / `Ctrl+C` / `Ctrl+V`, **Düzen** menüsünde geri al/yinele'nin yanında.
  `tool-answerable` kapısının iddiası **tersine** çevrildi: o üç satırın artık
  açıklama kutusu değil komut çalıştırdığını denetliyor — bir komut geldikten
  sonra yerinde kalmış bir yer tutucuyu yakalayan iddia bu.

**İşletim sistemi panosu (`QClipboard`) bağlanmadı ve sebebi planda yazılı:**
`/src/io` Qt bağlamaz, dolayısıyla baytları OS panosuna koymak `/src/app`'in işi
ve bir app-tarafı kanca daha istiyor. Bugün pano bu programın iki penceresi
arasında çalışıyor; başka bir uygulamaya kopyalamak o kancayı bekliyor.

### Eklendi — koordinat (ordinat) ve yay uzunluğu ölçüsü (P5)

- **`ÖLÇÜ tur=koordinat`** — ve model bunu **baştan beri taşıyordu**.
  `DimensionType::Ordinate` bildirilmiş, kodlanmış, çözülmüş ve çizilmişti; ama
  `dimension_layout` ile `dimension_picks` onu `default: return false` ile
  geçiyor ve hiçbir sözcük ona ulaşmıyordu. Bir Türk aplikasyon paftasının
  ordinat tablosu bu yüzden hiç çizilemiyordu — CLAUDE.md 5.15'in aynısı:
  kimsenin isteyemediği bir yetenek.
  **Ekseni jest belirliyor**: yazıyı noktadan yana çekerseniz sağa değerini,
  yukarı çekerseniz yukarı değerini okur. Bir ordinat tablosu tam böyle kurulur.
  Başlangıcın batısındaki bir nokta **eksi** okur; işaret cevabın parçasıdır,
  çünkü artı yazmak köşeyi paftanın öbür tarafına koymaktır.
- **`ÖLÇÜ tur=yay`** — yay **boyunca** uzunluk, kirişi değil. 50 m yarıçaplı bir
  çeyrek yay boyunca **78,540 m**, kirişi **70,711 m**'dir ve kirişi basan bir
  pafta yanlış boyda bordür sipariş ettirir. Bir yol geçiş eğrisi, bir bordür
  dönüşü ve bir boru dirseği hep tekerleğin katettiği mesafeyle ölçülendirilir.
  `DimensionType::ArcLength` enum'un **sonuna** eklendi ve bu onu toplamalı kılan
  şey: bundan önce yazılmış bir dosya bu değeri hiç içermez, yani eski bir çizim
  aynen okunur (model.md — bir şekil bir durum kazanabilir, bir durumu
  değiştiremez).
  DXF'te R2007 öncesi karşılığı yoktur; dışa aktarımda açısal ölçü olarak yazılır
  ve uzunluk xdata'da gider — bu programdan çıkıp geri girince aynen korunur,
  başka bir program bir açı görüp onu söyler, bir kiriş görüp ona inanmaz.
- **Altın fikstür** (`tests/golden/senaryolar/olcu-turleri.txt`): kaydedilen
  sayılar 40000, 30000, 20000, 78540 ve 157080. π bir çarpma ve bir yuvarlama
  içerir, yani bu satır farkın üç platformda da aynı milimetrede durduğunu
  söylüyor.
- Ölçü stili kataloğu **zaten vardı** ve `ÖLÇÜ` onu okuyordu
  (`data/catalogs/dxf/olcu-stili.json`: ISO-25, STANDARD, MİMARİ); bu maddede
  yapılacak bir şey kalmamıştı.

### Eklendi — ÇEYREK ve TEĞET yakalama, beş yeni seçim kipi (P4)

- **ÇEYREK**: bir eğrinin eksenleri kestiği dört nokta — 0, 100, 200, 300 grad.
  Bir eğrinin **adı olan tek yeri**: bir baca kapağı çeyreklerinden aplike
  edilir, bir borunun taban kotu alt çeyreğindedir. Yapıca tam — merkez ±
  yarıçap, trigonometri yok. Bir yayda yalnız **süpürülen** çeyrekler sunulur:
  kesildiği çemberin çeyreği, çizilen şeyin üzerinde bir nokta değildir.
  Öncelikte UÇ ile aynı sırada.
- **TEĞET**: son noktadan eğriye çizilen teğetin dokunduğu yer. **İki** ayak
  sunulur ve imlece yakın olan kazanır — iki teğetten birini sessizce seçmek,
  çizgiyi yanlış tarafa çekmektir. Önceki nokta çemberin içindeyse teğet yoktur
  ve motor bir nokta uydurmak yerine hiçbir şey söyler. `SnapConstructedMask`'e
  **konmadı**: teğet ayağı eğrinin ÜZERİNDEDİR, uzantı gibi geometrinin dışında
  değil, ve oraya koymak `reach` ayarlamamış her çağırıcı için modu kapatıyordu.
- F3 penceresi motor listesinden üretildiği için iki bit kendiliğinden orada
  (CLAUDE.md 5.10) — hiçbir tabloya elle eklenmedi.
- **Seçim kipleri: ÇOKGEN · ÇOKGENKESEN · ÇİT · ÖNCEKİ · SON.** Bir ada
  dikdörtgen değildir ve bir yol koridoru da değil: kutu ya kastedileni kaçırır
  ya komşuyu da alır, ve elle seçimden çıkarmak yanlış parselin içeride kaldığı
  yerdir. `ÇİT` çizdiği hattın kestiğini alır — yol boyunca bir bordür dizisi,
  arkasındaki binalar olmadan. Köşe sayısı sınırsız.
- **`ÖNCEKİ` silinmiş bir nesneyi geri getirmiyor.** `slot_of` silinmiş bir
  anahtarı hâlâ çözer — geri alma onun sayesinde çalışır — bu yüzden canlılık da
  soruluyor; yoksa bir sonraki `SİL` zaten gitmiş bir şeyi sildiğini bildirirdi.
  Bir adım derin ve bu bilinçli: bir seçim yığını kimsenin kafasında tutamadığı
  bir yığındır.
- Çekirdekte `pick_in_polygon` ve `pick_along_fence`; çokgenin kendi kutusu cull,
  çokgen testi yalnız ondan geçene koşuyor. Yedi yeni test, iki sayfa güncel.

**Geçici izleme (OTRACK) yapılmadı ve sebebi planda yazılı:** yazılı karşılığı
`xy(P,Q)` olarak P1a'da geldi; fare hâli oturumda geçici bir işaretli nokta
listesi, onu işaretleyecek bir jest ve `SnapQuery`'ye yeni bir alan istiyor —
yakalama motoruna alan eklemek bu paketin geri kalanı gibi tek dosyalık bir iş
değil.

### Eklendi — yedi düzenleme fiili (P3)

Bir drafter'ın sürekli kullandığı ve bu programın adı bile olmayan yedi fiil.

- **KIR** iki nokta arasındaki parçayı **çıkarır** — bir çite kapı boşluğu, bir
  duvarın geçtiği yerde bir kesinti. `BÖL` ikiye ayırır ve iki parçayı da tutar;
  bu aradakini atar. Tek nokta verilirse boşluksuz böler. Tıklama sırası
  önemsiz: bir el uzak ucu önce tıklar.
- **UÇUCA** uçları değen çizgileri tek çizgiye ekler, gerekeni çevirir ve her iki
  uçtan zincirler — sayısallaştırılmış bir harita tam olarak böyle görünür.
  `tolerans=` bir **parametre** ve seçim toleransı değil: ne kadar yakının
  "değmiş" sayıldığı ölçünün özelliğidir, farenin değil. `BİRLEŞTİR` poligon
  boolean'ıdır ve iki sayfa birbirini adıyla anıyor — adı karışan iki komut
  ikisini de kullanılmaz kılar.
- **UZUNLUK** bir ucu kendi doğrultusunda hareket ettirir: `delta=`, `yuzde=` ya
  da `toplam=`, tam olarak biri. Çok köşeli bir çizgide yalnız son parça değişir,
  ölçülmüş köşeler yerinde kalır.
- **PATLAT** çizgiyi kenarlara, alanı sınırına (kapanış kenarı dâhil), blok
  referansını bileşenlerine ayırır. Tanımında daire, yay ya da yazı olan bir blok
  **adıyla reddedilir**: bu türler bir yük taşır ve aynalı ya da eşit olmayan bir
  ölçekte artık daire, yay ya da yazı değildir; çizilmiş dış çizgisini koymak bir
  daireyi çevresi 2πr, alanı πr² olmayan bir 128-gen'e çevirir — ve bunlar tapuya
  giden sayılardır.
- **HİZALA** bir ya da iki nokta çiftiyle taşır, döndürür, `olcekle=evet` ile
  ölçekler. `OTURT` çok noktalı en küçük kareler Helmert'idir ve jeodezik bir
  işlemdir; bu çizim fiilidir. Yanlışını seçmek bir çizimin *neyi iddia ettiğini*
  sessizce değiştirir, bu yüzden iki sayfa birbirini anıyor.
- **BÖLÜMLE** bir nesne boyunca `sayi=k` eşit parça ya da `aralik=` kilometraj
  işareti koyar. Aralık bir **köşeyi geçebilir**: 60 metrelik istasyon bir L'nin
  ikinci ayağına düşerse, onu bulmak run boyunca okumak demektir ve komut bunu
  yapıyor. `ARANOKTA` iki noktayı böler; bu bir nesneyi böler.
- **ÇİZGİDÜZENLE**: `kapat`, `ac`, `ters`, `sadelestir`. Sadeleştirme bir köşenin
  komşuları arasındaki doğruya olan dik uzaklığına bakıyor ve **uçları hiç
  atmıyor** — onlar çizginin neye değdiği yerdir.
- Yedi sayfa, `docs/README.md` satırları, `make reference`, **Değiştir**
  menüsünde yedi giriş (§2.6a) ve yirmi iki sayısal test.

**Yapılmayan ikisi ve sebepleri planda yazılı:** `ESNET` köşe düzeyinde seçim
gerektiriyor ve bugünkü seçim nesne düzeyinde çalışıyor — P4'ün çokgen/çit
kipleri o altyapıyı getiriyor. `İŞARETLE` ise `BÖLÜMLE aralik=`'in tam olarak
yaptığı şey; ikinci bir ad ikinci bir komut demek olurdu.

### Eklendi — klasik çizim inşa yöntemleri (P2-1…P2-4, P2-6)

Bir daire, bir yay, bir dikdörtgen ve bir elips bir çizime birden çok şekilde
gelir ve şimdiye kadar her birinin yalnız bir yolu vardı.

- **DAİRE: `merkez` · `2n` · `3n` · `ttr`.** `3n` için `core::circumcircle`
  çekirdeğe eklendi ve `YAY yontem=3n` ile paylaşılıyor, yani bir daire ile
  ondan kesilen yay birbirine uyuyor. `ttr` iki doğruya teğet, verilen
  yarıçapla: **dört** çözüm var — iki doğrunun yaptığı her çeyrekte bir tane — ve
  hangisi istendiği sayıların içinde yok, bu yüzden kullanıcı köşeyi gösteriyor.
  Sessiz bir seçim pahı kavşağın yanlış köşesine koyardı. Üç doğrusal nokta
  reddediliyor: çevrel çemberi sonsuz yarıçaplıdır ve int64'e sığan en büyüğünü
  vermek yanlış bir cevabı cevap kılığına sokmaktır.
- **YAY: `merkez` · `3n` · `bma` · `bby`.** Süpürmenin işareti oturumun
  kuralından: semt'te artı bir süpürme SAAT YÖNÜNDE — bir mühendisin kastettiği
  şey — ve yay modelde saat yönünün TERSİNE saklandığı için uçlar ona göre takas
  ediliyor. Bunu yanlış yapmak biraz farklı bir yay değil, çemberin öbür üç
  çeyreğini çizer. Süpürme bir tura **katlanmıyor**: −100 grad öbür yöne döner,
  300 grad değildir. `bby`'nin iki çözümü `yon=sol|sag` ile ayrılıyor ve
  yarıçap iki nokta arasının yarısından küçükse komut bunu söylüyor.
- **ÇOKGEN (yeni).** Merkez ve kenar sayısından düzgün çokgen: `ic` (köşeler
  çemberin üzerinde), `dis` (kenarlar çembere teğet), `kenar` (kenar
  uzunluğundan). Köşeler `sin_cos_udeg`'den geliyor ve dört ana eksende tam
  sayıdır, yani 0°'de bir kare köşelerini milimetrenin üstüne koyuyor. Altıgenin
  kenar=yarıçap kimliği testte: iki yöntem bayt bayt aynı çizimi veriyor.
  Bir **çokgen**, bir *poligon* değil — o ad güzergâha ait.
- **DİKDÖRTGEN `yontem=3n`:** bir kenarın iki köşesi ve karşı kenarın geçtiği
  nokta. Izgaraya paralel olmayan her yapı için. Üçüncü nokta bir köşe **değil**,
  yalnız yüksekliği veriyor: kenarın normaline izdüşürülüyor, yani eli birkaç
  milimetre kayan bir kullanıcı paralelkenar değil dikdörtgen alıyor.
- **ELİPS `yontem=eksen`:** eksenin iki ucu, merkez ikisinin ortası — AutoCAD'in
  varsayılanı ve bir şerit metrenin ulaştığı şey. Aynı elips iki şekilde
  yazılıyor ve ikisi bayt bayt aynı çizimi veriyor.
- Yöntem parametreleri **konumsal argümanlardan sonra** bildirildi: `yontem`
  önde olsa `DAİRE 50,50 100,50`'nin ilk koordinatını yutardı — her sayfanın,
  betiğin ve günlüğün kullandığı biçim.
- Beş sayfa güncel (`ÇOKGEN` yeni), `make reference`, ve yirmi sekiz sayısal
  test (bilinen çember, çeyrek çember, kare, altıgen, 3-4-5 kenar, iki yarım
  daire).

**`KILAVUZ` açılı kılavuz (P2-5) ertelendi ve sebebi yazılı:** bugünkü
`core::Guide` yalnız {eksen, koordinat} taşıyor ve proje dosyasında iki paralel
dizi olarak saklanıyor. Açı ve geçtiği nokta eklemek bir **belge modeli**
değişikliğidir (CLAUDE.md 0.2a: bir veri göçüdür, refactor değil) ve biçim sürümü,
eski biçimi okuyan bir okuyucu ve gidiş-dönüş testi gerektirir. Komut düzeyinde
yarım yapmak, kaydedilip açılınca kaybolan bir kılavuz demek olurdu.

### Eklendi — POLİGON: her ölçünün üzerine oturduğu iskelet (P1b-5, P1b-6)

Poligon güzergâhını ölçü karnesinden koordinata çevirir: açı kapanmasını
hesaplar ve istasyonlara eşit dağıtır, düzeltilmiş semtlerle koordinatları
hesaplar, kenar kapanmasını hesaplar ve eşit ya da kenar orantılı (Bowditch)
dağıtır. Rapor yapılandırılmıştır (`Context::report`): sınıf, kaynak, onay
durumu, `[S]`, kapanma hataları ve her istasyon için semt, kenar, sağa, yukarı.

- **Kapanmanın kabul edilebilirliği mevzuat kararıdır ve bu dosya onu vermez.**
  Toleranslar `/data/catalogs/geodesy/poligon-toleranslari.json`'dan okunur
  (CLAUDE.md 5.13); aşan bir güzergâh **reddedilir**, ret yönetmeliği adıyla ve
  oranıyla söyler, ve reddedilen bir güzergâh çizime hiçbir şey bırakmaz
  (Article 1.6).
- **HARİTA MÜHENDİSİ ONAYI BEKLİYOR (CLAUDE.md 6.11).** Tolerans paketinin
  `kapsam.onay` alanı `BEKLİYOR`: değerler BÖHHBÜY'ün poligon bölümünden bir
  harita mühendisi tarafından teyit edilmemiştir. Komut **her retinde** bunu
  yazar ve rapor `onay` alanında taşır, yani imzalanmamış bir sayı kural gibi
  görünmüyor. Kod, şema, `data/LICENCES.md` izin satırı, kapılar ve testler
  tamamdır; eksik olan yalnız o onaydır ve paket o gelene kadar bir üretim işinin
  kabulü için kullanılmaz.
- **`POLİGON` `ALAN`'dan alındı.** Bir yanlış çeviriydi: Türkçe haritacılıkta
  *poligon* bir güzergâhtır, `ALAN`'ın çizdiği şekil ise *çokgen*'dir. `ALAN`,
  `AREA` ve `AL` değişmedi ve günlükler komut kimliğini sakladığı için replay
  etkilenmedi.
- **Altın fikstür** (`tests/golden/senaryolar/poligon.txt`): kare bir güzergâh ve
  20 mm'lik bir kapanmanın kenar orantılı dağıtımı — düzeltme payları 5, 10, 15,
  20 mm. Dağıtım bir bölme içerir ve bölmenin yuvarlaması üç platformda aynı
  olmak zorundadır (§7.3).
- Arayüz: **Çizim > Poligon Hesabı** ve **Harita > Poligon Hesabı**. Sayfası,
  `docs/README.md` satırı, `make reference`, beş test.

### Düzeltildi — günlük kesirli bir kenarı kırpıyordu

`Value::from_json` her sayısal diziyi kimlik listesi olarak okuyup **tam sayıya
kırpıyordu**. `"kenar":[42.315, 56.720]` günlükten 42 ve 56 olarak dönüyordu:
replay başka bir poligon çiziyor, başka bir içerik hash'i veriyor ve imza yanlış
çizimin üstüne düşüyordu. Bir kenar milimetresine kadar ölçülür ve yalnız
metresini tutan bir günlük günlük değildir (Article 1.4).

İçinde kesir olan bir dizi artık `NumberList` olarak okunuyor; tam sayı dizisi
eskisi gibi kimlik listesi kalıyor, çünkü her zaman o anlama geldi ve bir altın
fikstür ona bağlı — bir `Number` parametresi kimlik listesini kendi dizisi olarak
okuduğu için iki yönde de kayıp yok.

### Düzeltildi — bir domain komutunun ad çakışması hiçbir yerde yakalanmıyordu

`test_command.cpp`'nin tripwire'ı yalnız `/src/command` kaydını sayar; bir domain
modülünü **göremez**, çünkü `/src/command` bir domain modülüne bağımlı olamaz
(Article 3.2). `geodesy.traverse` bu yüzden `POLİGON` `ALAN`'ın eşadıyken
kaydolamıyor, başarısız kayıt yalnız bir log satırı yazıp devam ettiği için
program traverse olmadan — temiz derlemeyle ve yeşil bir suite ile —
başlıyordu.

Yeni kapı (`test_geodesy.cpp`, "kayıt: bütün kayıtlar birlikte") uygulamanın
kaydettiği her şeyi birlikte kaydediyor, bildirilen her adın onu bildiren komuta
çözüldüğünü denetliyor ve dört domain komutunu **adıyla** arıyor — bir sayı bir
şeyin değiştiğini söyler, bir ad neyin eksik olduğunu söyler. Kapı eski koda
karşı denendi ve onda "kayıtta yok: geodesy.traverse" diye düşüyor.

### Eklendi — KESİŞİMNOKTA ve ARANOKTA: kaybolan köşe ve kazık dizisi (P1b-3, P1b-4)

`KESİŞİMNOKTA` taş taşı gitmiş bir köşeyi üç yoldan geri kurar: iki doğrultunun
kesişimi, iki uzaklığın kesişimi, ya da iki sınırın uzatılmış hâlinin kesişimi.
`ARANOKTA` bir doğru üzerinde oran, uzaklık ya da `sayi=k` ile eşit bölme
yaparak nokta koyar — bir yol ekseni üzerindeki istasyon kazıkları.

- **İki uzaklığın iki çözümü sessizce seçilmez.** `yon=sol|sag` istenir; biri
  sessizce seçilse bu bir sınırı yolun yanlış tarafına koymanın yolu olurdu.
  Test iki yönün aynadaki iki farklı köşeyi verdiğini doğruluyor.
- **Ret cümleleri rakamı söyler** (R19): ulaşmayan iki uzaklık, iki yarıçapı ve
  merkezler arası mesafeyi birden yazar, yani hangi ölçünün yanlış olduğu bir
  bakışta görülür.
- **Ortak zemin:** üç kesişim ve iki ara-nokta yapısı `command/construct.hpp`'ye
  taşındı ve `kes()`, `ara()`, `uzanti()` nokta fonksiyonlarıyla paylaşılıyor.
  Aynı cevap, aynı çözüm seçimi ve aynı ret cümlesi, iş yazılmış da olsa
  tıklanmış da olsa (5.10, Article 1.2). Yan fayda: `ara()`nın metre biçimi artık
  oran biçiminin yuvarlamasından geçiyor, yani 100 m'lik bir doğruda `0.2` ile
  `20 m` aynı milimetreye düşüyor — önce birbirinin bir milimetre yakınındaydı.
- `sayi=k` uçları **tekrar koymaz**: onlar zaten oradadır ve tekrar konması bir
  taşın üstünde iki nokta bırakırdı.
- Arayüz aynı değişiklikte, iki sayfa, `docs/README.md` satırları,
  `make reference`, ve dokuz sayısal test (bilinen bir karenin merkezi üç
  yöntemden de aynı milimetre).

### Eklendi — ALIM: bir istasyonun ölçü karnesi çizime böyle girer (P1b-2)

Bilinen bir noktada duran alet her detay için bir **açı** ve bir **kenar** okur.
`ALIM` bu iki sütunu koordinata çevirir: istasyon, isteğe bağlı bağlama noktası,
sonra sırayla açı–kenar çiftleri.

- **`APLİKASYON`un tersi**, ve ikisi aynı aritmetiği paylaşıyor: semt hesabı
  `core::polar_offset_turns` olarak `core/angle.hpp`'ye kondu. Aplike edilen bir
  nokta ile geri okunan aynı nokta artık aynı milimetreye düşüyor, iki ayrı
  yuvarlamaya değil (5.10).
- **Bağlama noktası açının anlamını değiştirir**: verilmezse açı semt açısıdır,
  verilirse bağlamadan itibaren okunmuştur — bir aletin gerçekten okuduğu şey.
  Hangisi olduğu her koordinatı değiştirdiği için komut hangisini kullandığını
  yazıyor.
- **Kısaltma `AL` değil `ALM`.** `AL` `ALAN`'ın kısaltmasıdır. Onu kapmak ALAN'ı
  gölgelemiyor, **düşürüyordu**: başarısız bir kayıt yalnız bir log satırı yazıp
  devam eder, yani derleme temiz kalır, suite yeşil kalır ve parsel çizen komut
  yok olur. `registry: bildirilen her komut GERÇEKTEN kaydedilmiş` testi tam bu
  yüzden var ve bunu yakaladı.
- Arayüz aynı değişiklikte: **Çizim > Alım** ve araç kutusunda Nokta ailesinin
  üçüncü üyesi. Sayfası, `docs/README.md` satırı, `make reference`, eşitlik
  kanıtı ve dört sayısal test (dört ana yön, bağlamalı okuma, eksi kenar reddi,
  kapalı çokgen).

### Eklendi — DİKAYAK: şerit metreyle alınan detay çizime böyle girer (P1b-1)

İki bilinen noktadan geçen bir taban çizgisi ve o çizgiye göre okunan her detay:
taban üzerinde kaç metre ilerlediğiniz (**ayak**) ve oradan dik olarak kaç metre
çıktığınız (**boy**). Bir Türk ölçü karnesinde bir duvar, bir bordür, bir direk
ya da bir bina köşesi tam bu iki sayıyla yazılır.

- **Boy'un işareti: A→B yönünde sol pozitif** — Netcad'in işaretiyle aynı, komut
  sayfasında yazılı ve iki yönde de testli.
- **Hesap paylaşılıyor.** `core::perpendicular_offset`: aynı yapıyı hem tek
  gramerdeki `dik(A,B,ayak,boy)` hem bu komut çağırır. Bir işaret kuralının iki
  kopyası, ikisinden birinin aynalanmasının yoludur (5.10).
- **Arayüz aynı değişiklikte** (TODOS-CAD §2.6a): **Çizim > Dik Ayak** ve araç
  kutusunda Nokta ailesinin ikinci üyesi.
- Sayfası, `docs/README.md` satırı, `make reference`, eşitlik kanıtı (GUI = komut
  satırı = betik, aynı belge + bayt bayt aynı günlük), günlük replay kanıtı, iptal
  ve aynı-iki-nokta reddi testleri.

### Düzeltildi — sayı dizisi diye bir şey yoktu: ikinci okuma sessizce düşüyordu

`DİKAYAK`ın `ayak`/`boy` çiftleri girdi katmanında dört yerde birden kayboluyordu
ve hepsi aynı sebeptendi: **bir `Number` parametresi liste olabiliyordu ama
`Value`'nun sayı dizisi yoktu.** Nokta, metin, kimlik ve seçim birikiyordu;
sayı **değiştiriyordu** — yani `.claude/command.md` P15'in kaçırdığı tür.

- `Value::Kind::NumberList` ve `Value::numbers()` / `as_numbers()`. Enum'un
  SONUNA eklendi (günlük türü ada göre okur).
- `bind_tokens` sayı dizisini biriktiriyor. Öncesinde `ayak=10 boy=5 ayak=30
  boy=-5` SON çifti tutuyor ve ekibin iki okuduğu yere bir nokta koyuyordu.
- `record_awaited` sayı dizisini günlüğe **dizi** olarak yazıyor. Öncesinde son
  okumayı yazıyordu: çizime iki detay giriyor, günlükten biri çıkıyor, replay
  başka bir çizim üretiyordu (Article 6.4).
- `next_of` diziyi istek başına bir okuma tüketiyor — nokta dizisinin yaptığının
  aynısı.
- `Validator` dizinin sayısını sayıyor ve arity'si birden fazlaya izin veren bir
  `Number`a dizi geldiğinde kabul ediyor.
- İki okumalık bir dizi JSON'da bir noktadan ayırt edilemez (`[10, 30]`), bu yüzden
  `bus.cpp` onu spec elde olduğunda geri çeviriyor — tamsayı listesi için zaten
  yapılanın aynısı.
- `ai::render_line` bir sayı dizisini tekrarlanan anahtar olarak yazıyor, yani
  yazdığı satır tuttuğu şeye geri ayrıştırılıyor (Article 1.4).

### Düzeltildi — ölçüm satırı durum çubuğunun sağındaki hücrelerin üstüne biniyordu

Kullanıcının bildirdiği kusur: *"sadece mesafe ölçme çalışıyor o da bozuk"*.
Bozuk olan ölçüm değil, cevabının indiği yerdi.

Durum şeridinin sağ ucunda üç hücre var — çizim arka ucu, veritabanı, ajan
dinleyicisi — ve yardımcı çiplerin yanındaki mesaj bunların bıraktığı boşluğu
alıyor. Boşluk üçün yalnız İKİSİNİ düşüyordu: uzun bir satır, dinleyici
hücresinin altına uzanan bir kutuya kırpılıyor ve ikisi üst üste çiziliyordu.
`ÖLÇ` programın en uzun satırlarından birini yazar —
`Mesafe: 58,941 m  ΔY: 57,000 m  ΔX: 15,000 m  Açı: 83,6183 grad
(kuzeyden saat yönünde)` — bu yüzden bozuk diye bildirilen araç ölçüm aracı oldu.

- Üç hücrenin genişliği çizimden önce bir kez ölçülüyor ve mesajın sağ kenarı
  üçünü de düşüyor. Aynı hata "Durdur" dalında da vardı; o da düzeldi.
- **Yeni kapı `status-strip`** (`KENTOS_STRIP_PROBE`), ve kanıtı PİKSEL: şerit
  boş mesajla, gerçek ölçüm satırıyla ve onun üç katı uzunlukta bir satırla
  çiziliyor; sağdaki hücrelerin bandı üç karede bayt bayt aynı olmak zorunda.
  Probe eski aritmetiğe karşı denendi ve onda **düşüyor** — yani bir dilek değil,
  bir kapı.

### Düzeltildi — ağır bir tıklama aracı hiç çalıştırmıyordu (çizim araçları)

Kullanıcının bildirdiği kusur: *"çizim asla çalışmıyor, üçü de hem de"*,
*"mouse ile çalışmıyor kesinlikle"*, *"mouse takip eden ne kılavuz var ne de
başka bir şey"*. "Üçü de" tam olarak Çizgi ailesinin üç üyesi: ÇİZGİ,
ÇOKLUÇİZGİ, SPLINE — hepsi TEK bir aile düğmesinin arkasında.

Sebep `kHoldMs = 280`. Bir tıklama bir basış ve bir bırakıştır ve elin tıklaması
anlık değildir: bir araç düğmesine yapılan kararlı bir tıklama rahatlıkla 280
ms'yi geçiyordu. Geçtiğinde araç değil **kart** açılıyor, ve açık bir Qt popup'ı
fareyi yakaladığı için aynı düğmeye yapılan sonraki basış yalnız kartı kapatıp
yutuluyordu. Ağır tıklama → kart. Tekrar tıkla → kart kapanır, hiçbir şey
çalışmaz. Tekrar tıkla → kart. Tıklamaları hep ağır olan bir kullanıcı o araca
hiç ulaşamıyordu.

- **`kHoldMs` 280 → 500 ms**: her platformun uzlaştığı uzun-basış süresi.
- **Ağır tıklama yine tıklamadır.** Düğmeden hiç ayrılmadan bırakıldıysa
  kullanıcı ARACA uzanmıştır ve eli sadece bir zamanlayıcıdan yavaştır: kart
  kapanır ve yüzdeki araç çalışır. Yalnız düğmeden AYRILAN el — ya da köşe
  işaretiyle veya sağ tıkla kartı isteyen el — aileye uzanmış sayılır.
- **Köşe işaretinin hedefi büyütüldü** (5 px çizim, 14 px hedef): 46 px'lik bir
  düğmede 5 piksellik hedef kimsenin tutamadığı hedeftir.
- **Yeni kapı `real-mouse`** (`KENTOS_REALMOUSE_PROBE`). Diğer bütün probe'lar
  olaylarını doğrudan kastettikleri öğeye gönderiyordu; bu, gerçek bir tıklamanın
  önce geçtiği iki şeyi atlıyor: imlecin altındaki öğeyi seçen isabet testi ve
  basış-bırakış aralığı. Kusur tam olarak atladıkları yerdeydi. Yeni probe 40,
  150, 260, 320, 500, 700 ve 1000 ms'lik tıklamaların hepsinin aracı
  çalıştırdığını, tuvalin üstünde tıklamayı yiyen başka bir öğe olmadığını,
  ilk tıklamanın geçtiğini, kılavuzun fareyi izlediğini ve çizilen çizginin
  belgeye girdiğini doğruluyor. Kılavuzun EKRANDA olduğu ancak gerçek pencerede
  sınanır ve probe bunu açıkça **BEKLEMEDE** diye yazar: offscreen platform bir
  `QRhiWidget`'a `QRhi` vermez, hiç çizmez, dolayısıyla sahneyi de kurmaz.
- **Yeni kapı `tool-flyouts`** (`KENTOS_FLYOUT_PROBE`): on bir araç yalnız aile
  kartının arkasında yaşıyor ve hiçbiri bir testte hiç basılmamıştı. Yedi aile,
  on sekiz üye; kart üç jestin hepsiyle açılıyor (basılı tutma, sağ tık, köşe
  işareti) ve her üye kartından seçilince çalışıyor.

### Düzeltildi — 97 komutun 33'ü fareyle başlatılamıyordu

Kullanıcının bildirdiği kusur: *"toolbox üzerindeki araçlar da mouse ile
çalıştıramadım mesela blok blokekle, ölçüm araçları ve diğerleri çok kötü ve
çalışmıyorlar"*. Ölçüldü, tahmin edilmedi (`KENTOS_TOOL_PROBE`): **97 komutun
33'ü yalnız adını yazarak başlatılabiliyordu.** Fare kullanıcısının o komutları
hiç yoktu.

CLAUDE.md 5.15 fareyle erişilebilen tek yollu bir özelliği yasaklar; aynadaki
hâli — yalnız klavyeden erişilebilen komut — Article 1.2'nin (GUI eşit istemci,
daha yoksulu değil) ihlaliydi. Menüler elle tutuluyordu, ki o da 5.10'un
yasakladığı ikinci komut listesidir.

- **Menüler artık `Registry`'den tamamlanıyor.** Küratörlü girişlerin altına, her
  kategorinin menüsüne, tek bir **Diğer komutlar** satırı olarak. Sonraki komut
  menüsüne kendiliğinden gelir. Son satırı olan menüde (Dosya'nın `Çıkış`'ı)
  kuyruk onun ÜSTÜNE girer.
- **`CommandSpec::title`** — insan okuyacağı Türkçe etiket. Ad tek kelimedir
  (`ÇIKTIYERLEŞİMİ`) ve ondan üretilen menü Türkçe değildir; `ToolSpec` bu alanı
  zaten taşıyordu. 92 komutun hepsi dolduruldu. Etiket menüye, üretilmiş
  referansın yeni **Adı** sütununa ve ajan kataloğunun MCP `title` alanına gidiyor.
- **Klavye, yazılacak yere taşınıyor.** `BLOK`, `BLOKEKLE`, `KATMAN`, `KATMANAT`,
  `ETİKET` ilk olarak bir AD soruyor. İstem durum satırına yazılıyor, odak tuvalde
  kalıyordu: tuşlar hiçbir yere gitmiyordu ve düğme ölü görünüyordu. Yazı, sayı ya
  da tamsayı isteminde komut satırı açılıp odaklanıyor; nokta ve nesne isteminde
  odak tuvalde kalıyor, çünkü cevap oradan geliyor.
- **`Prompt::choices`** — cevabı bilinen bir kümeden olan istem o kümeyi sunuyor:
  çizimdeki bloklar, katmanlar, fiil listeleri. Komut bilir, kabuk sunar. Kısıt
  değil öneri: yeni bir bloğun adı tanımı gereği listede olamaz.
- **Sözle cevap veren komut cevabını görünür kılıyor.** Sorgu komutları transkripte
  yazar ve o panel kapalı başlar, yani `Katmanları Listele`'ye basmak ölü bir satıra
  basmakla aynı görünüyordu. Menü satırı artık **Geçmiş** sekmesini önce açıyor.
  (Bu arada `transcriptDock_` üyesinin hiç atanmadığı ve iki tema döngüsünün onu
  sessizce atladığı ortaya çıktı; kaldırıldı — transkript `propertyDock_`'un bir
  sekmesi.)
- **Henüz olmayan özellik kendini anlatıyor.** KES, PANOYA KOPYALA, YAPIŞTIR ve
  KATMAN YÖNETİCİSİ `setEnabled(false)` idi: tıklanınca hiçbir şey olmuyordu, yani
  bozuk görünüyorlardı. Artık ne yapacağını, hangi fazda geleceğini ve bugün ne
  kullanılacağını söyleyen bir kutu açıyorlar.
- **`SORGULA`** gerçek komutu çalıştırıyor; yanında duran `Faz 2` ölü satır kalktı.
  Küratörlü girişler eklendi: KILAVUZ, ETİKET, EŞYÜKSELTİ (Çizim); APLİKASYON,
  HACİM, OTURT, DÖNÜŞTÜR, ALANÖLÇ, KOORDİNAT (Harita).
- **Üç yeni kapı**, hepsi gerçek ikiliyi çalıştırır çünkü `/tests` Qt bağlamaz:
  `tool-reach` (97/97), `tool-answerable` (istem geldi, odak doğru yerde, seçenekler
  sunuldu, cevap geçti, henüz-yok satırları kendini anlatıyor, sorgu cevabı görünür),
  `menu-bar` (on bir menü açılıyor, boş değil, ekranı aşmıyor).

### Değişti — komut listesi: doksan sekiz transkript satırı ve bir taşan pencere yerine bir sayfa

`YARDIM` bütün komutları transkripte döküyordu — her komut için adı, bütün
eşadları ve açıklaması, doksan sekiz satır. Transkript akan bir konuşmadır, yani
bu döküm kullanıcının o ana kadar yaptığı her şeyi görünmez yere itiyor ve kaydırma
konumunu da beraberinde götürüyordu. **Yardım > Komut Listesi** ise üretilmiş
referansın tamamını bir `QMessageBox`'a koyuyordu; bir `QMessageBox`'ın metni
kaydırma alanı taşımaz, dolayısıyla pencere ekranın altından taşıyor ve kaydırmanın
yolu olmuyordu. Kullanıcının ifadesiyle: *"scroll olmuyor ve aşağı doğru uzayıp
gitmiş"*, *"çok geç açılıyor"*, *"berbat"*.

Bir liste TEK bir soruyu yanıtlar — ne var — ve bunun cevabı okunacak bir sayfadır,
geriye kaydırılacak bir konuşma değil.

- **Sayfa.** 980×560; solda dokuz kategori başlığı altında 97 komut (adı, tek satır
  açıklaması, sağ kenarda kısaltmaları), sağda imlecin üzerindeki komutun kategorisi,
  kimliği, bütün yazımları ve **parametre listesi**: her parametrenin tipi,
  gerekliliği, aralığı, birimi ve varsa sözcük listesi. Liste kaydırılır, yatay
  kaydırma çubuğu yoktur, yükseklik sabittir.
- **Dört yol, bir sayfa.** `Ctrl+K`, **Yardım > Komut Listesi**, yeni **F1** ve komut
  satırına yazılan `YARDIM` aynı komutu çalıştırır. Menü artık `YARDIM` satırını veri
  yoluna gönderiyor; kendine ait bir yolu yok (Article 1.2).
- **Metin her istemciye yine yazılıyor,** ama kategori başına bir satır: on satır,
  doksan sekiz değil. Betik, başsız çalıştırma ve ajan için komut ayrıca
  `Context::report` ile bütün kümeyi veri olarak döndürüyor — kimlik, adlar, kategori
  ve özet — yani makine artık yüz echo satırını kazımak yerine tek cevap okuyor
  (command.md R26).
- **Dikiş, dal değil.** `Bus::on_help_page`, `on_print_request` ile aynı kalıpta:
  komut NEYİ göstereceğini bilir, pencereler hakkında hiçbir şey bilmez ve hiçbir
  komut `InputSource`'a bakmaz (command.md P10).
- `Registry::markdown_reference()` kaldırıldı: tek tüketicisi o pencereydi ve komut
  listesinin üçüncü bir sunumuydu.
- **Kanıt:** `tests/unit/test_command.cpp` metnin kategorilere göre yazıldığını,
  satır sayısının komut sayısıyla büyümediğini, dikişin doğru adla çağrıldığını ve
  bilinmeyen bir adda hiç çağrılmadığını sınıyor. `/tests` Qt bağlamadığı için
  pencerenin yüksekliğini ve kaydırma çubuğunu göremez: yeni `help-page` ctest'i
  (`KENTOS_HELP_PROBE`) gerçek ikiliyi çalıştırıp `YARDIM`'ı menünün yolundan
  gönderiyor ve sayfanın açıldığını, 97 komutun listede olduğunu, listenin
  kaydırıldığını, yüksekliğin ekranı aşmadığını ve `YARDIM komut=ÖLÇÜ`'nün sayfayı
  ÖLÇÜ üzerinde açtığını doğruluyor.

### Eklendi — nokta fonksiyonları: koordinatı yazmak yerine nasıl bulunduğunu yazmak

Bir harita mühendisi araziden koordinatla dönmez; iki bilinen noktadan ölçülmüş
bir dikle, bir semt ve kenarla, iki şerit mesafesiyle döner. Bugüne kadar bu
inşalar elde ya da hesap makinesinde yapılıp sonuç yazılıyordu. Artık **bir
koordinatın yazıldığı her yere** yazılabiliyorlar — komut satırına, betiğe,
çalışan bir komutun istemine — çünkü hepsi tek gramerin (`command/parser.hpp`)
parçası (TODOS-CAD P1a, CLAUDE.md 5.11).

- **On iki biçim, tek tablo:** `son`, `n(1284)`, `orta(A,B)`, `ile(P,@dx,dy)`,
  `dik(A,B,ayak,boy)`, `semt(S,açı,kenar)`, `kes(A,açı1,B,açı2)`,
  `kes(A,r1,B,r2,sol|sağ|yon=<nokta>)`, `kes(A,B,C,D)`,
  `ara(A,B,oran)` / `ara(A,B,mesafe m)`, `uzanti(A,B,mesafe)`, `xy(P,Q)`. Adlar
  `turkish_fold_key` ile eşleşir: `ORTA`, `orta`, `uzantı`, `uzanti` aynı şey.
- **Yeni komut yok.** Fonksiyon, dispatch'ten ÖNCE tek bir `Point2`'ye çözülür;
  komut, doğrulayıcı ve günlük yalnız çözülmüş noktayı görür, dolayısıyla eski
  günlükler ve betikler aynen oynar (Article 1.4).
- **Argüman kuralı belgelendi:** argümanlar virgülle ayrılır ve koordinat da
  virgülle yazılır, bu yüzden mutlak bir nokta **iki** argüman yeri harcar.
  Argüman listesi sayılmaz, **imzaya karşı eşlenir**; `kes`'in üç biçimi böyle
  ayrılır ve iki biçim birden uyarsa çağrı reddedilir.
- **`dik`'in işareti: sol pozitif** (Netcad ile aynı), komut sayfasında yazılı ve
  iki yönde de testli. `kes(A,r1,B,r2,…)` iki çözümlüdür ve **sessiz seçim
  yoktur**: `sol`, `sağ` ya da `yon=<yakın nokta>`. Çemberler kesişmiyorsa hata
  iki yarıçapı ve merkezler arası mesafeyi birlikte söyler (command.md R19).
  Yakın nokta `yon=` ile yazılır, çünkü çıplak bir koordinat orada
  `kes(A,B,C,D)` okumasından ayırt edilemez.
- **Paralel doğrultu tam sayıda yakalanır:** iki açı mikro-derece cinsinden
  karşılaştırılır, yuvarlanmış bir ışın ucunun çapraz çarpımıyla değil — yoksa
  tam ters iki doğrultu kesişimi Ay'ın ötesinde bir nokta verirdi.
- **`n(1284)` çizimi arar ama gramer çizimi tanımaz:** arama, çağıranın verdiği
  bir `ResolveContext::named_point` işlevidir (`core::ImageResolver` kalıbı).
  `Bus::resolve_context()` tek yerde kurar, koordinat çözen üç dikiş de onu
  kullanır; çizimi olmayan bir çağıran boş verir ve `n()` bunu söyleyerek
  reddeder — yapısı gereği etkisiz, istemcinin kontrol etmesiyle değil.
- `core::circle_intersection` (`core/pick.hpp`) — iki çemberin kesişimi, sol ve
  sağ çözümüyle ve neden buluşmadıklarını söyleyen `CircleMeet`'iyle. P2'nin
  `ttr` dairesi ve P1b'nin `KESİŞİMNOKTA yontem=mesafe`'si de bunu kullanacak.
- Belgeler: `komut-satiri.md`'ye "Nokta fonksiyonları" bölümü (tablo, argüman
  kuralı, işaret kuralı, çizim örnekleri), `ilk-adimlar.md`'ye dik ayak örneği,
  `betik/README.md`'ye metin koordinat satırı, `sozluk.md`'ye iki terim,
  `sorun-giderme.md`'ye dokuz hata. Testler: `test_command.cpp`'de bilinen
  üçgenlerle `Mm` eşitliği, idempotens ve hata metinleri; `test_proof.cpp`'de
  arayüz = komut satırı = betik eşitlik kanıtı ve günlük replay'i;
  `nokta-fonksiyonlari` altın senaryosu; fuzz korpusuna üç tohum (17).
### Düzeltildi — aşırı bir koordinat tanımsız davranıştı, artık doymuş bir değer

`mm_round` (`core/units.hpp`) ve `udeg_from_angle` (`core/angle.hpp`) ikisi de,
çağıranın sınırlamadığı bir `double`'ı `static_cast<std::int64_t>` ile
çeviriyordu. int64 aralığının dışındaki bir değer için bu çevrim **tanımsız
davranıştır** — doyuran bir komut değil — ve ardından gelen yarım adım
(`truncated + 1`) da taşar.

Depodaki girdiden erişilebilirdi. `tests/fuzz/tohum/komut/14-asiri-sayi.txt`
`@(2^1000),0` yazar; `^` işleci 1,07e301 döndürür ve `mm_from_metres` onu
1,07e304 mm'ye büyütür. AArch64'te çevrim doyuruyor, ardından gelen adım taşıp
sarıyordu, ve sonuç **-9223372036854775808** — yani `kMmInvalid` — oluyordu:
dünyanın ucuna kaçmış bir koordinat, "değer yok" işaretini taşıyarak geliyordu.
`RingGeometry::append` böyle bir tepe noktasını "kaynak veride okunamayan bir
koordinat var" diye reddeder; oysa o koordinat okunamamış değil, temsil
edilemeyecek kadar büyüktü.

- **Doyma, taşma değil.** Her iki yardımcı da `±kMmSaturated` — 2^63 − 1024,
  yani aynı zamanda tam bir `double` olan en büyük `Mm` — yanıtlar. `constexpr`
  olmaları ve yarıyı sıfırdan uzağa yuvarlama sözleşmesi aynen korunur: aralık
  içindeki hiçbir değerin sonucu değişmedi, bit bile.
- **Doyma simetriktir, çünkü `kMmInvalid` INT64_MIN'dir.** `min()`'e doymak,
  temsil edilemeyecek kadar büyük bir sayıyı hiç okunamamış bir sayıya
  çevirirdi. Cevabın işareti de sorunun işaretiyle aynı kalır.
- **Kapı üç dal değil, iki seçim.** `mm_round` teğetleyicilerde tepe noktası
  başına çağrılıyor (`arc.cpp`, `circle.cpp`, `ellipse.cpp`, `transform.cpp`,
  `snap.cpp`), yani Article 7 hemen yanıbaşında. Üç erken `return` ile —
  bariz yazım — M serisi bir çekirdekte çağrı başına +4,2 ns; aşağıdaki iki
  seçimle +0,10 ns. Karşılaştırma yönü (`v < …`) NaN'ı üçüncü bir teste gerek
  kalmadan aynı yola sokar.
- **NaN da `+kMmSaturated` yanıtlar, ve doğru cevap zaten bu.** Sıfır ilkeli
  görünür ve tuzaktır: sıfır BAŞLANGIÇ NOKTASIDIR, TUREF'te herhangi bir
  parselden bin kilometre uzakta (`Box2`'nin kendi notu bunu anlatır). NaN'a
  sıfır demek, bir köşeyi oraya sessizce koymaktır; doymuş demek onu aralık
  dışına koyar ve depo reddeder.
- **Doymak kabul etmek değildir.** 2^63 − 1024 mm, `kMmCoordinateLimit`'in dört
  katıdır; `RingGeometry::append` onu hak ettiği aralık mesajıyla reddeder.
  Kapı o reddi *erişilebilir* kılar — tanımsız davranış bir ret değildir.
- `udeg_from_angle` çevrimin ikinci bir kopyasını tutuyordu; artık tek
  yuvarlama yardımcısına iniyor (core.md R20).
- **Tohum korpusu bu yolu artık gerçekten oynatıyor.** `14-asiri-sayi.txt`'nin
  tek satırı ilk belirtecinde — `@1e308<1e308g`, sayı dilbilgisinde üs biçimi
  yok — `parse_line`'ı düşürüyordu, ve replay satırın tamamını atladığı için
  aynı satırdaki `@(2^1000),0` hiç çözülmüyordu. Dört satır eklendi: mutlak
  koordinat iki işarette, ve `@0<(2^1000)` üç birim sonekiyle.
- Testler: `test_core.cpp`'de ±1e308, ±inf, NaN, int64 sınırının iki yanındaki
  komşu değerler, `kMmInvalid`'den ayrı durduğu, ve `constexpr` katlama.
### Düzeltildi — nesne seçimine yazılan koordinat sessizce yutuluyordu

`bind_tokens` bir koordinat belirtecini, parametrenin türüne **bakmadan** önce
noktaya çeviriyordu. Sayı, tam sayı, metin ve evet/hayır bir kat sonra
`Validator::check_against_spec` tarafından kurtarılıyordu; **nesne seçimi**
kurtarılmıyordu: bir noktanın `as_ids()`'i boştur, saklanan boş kimlik listesi de
`arity.min == 0` olan bir seçime "verilmemiş" diye okunur. Argüman yere düşüyor
ve komut başarı bildiriyordu — `.claude/command.md` P15'in tam olarak yasakladığı
şey.

- **`SİL nesneler=1,2` hiçbir şeyi silmiyordu**, `KOPYALA … bitis=485620,4310200
  485640,4310200 485660,4310200` ise üç kopya istenirken **bir** kopya yapıp
  başarı yazıyordu: sondaki çıplak koordinatlar, arity'si dolmayan ilk parametre
  olan `nesneler`'e konumdan bağlanıp yutuluyordu. İkisi de artık reddediliyor.
- **Koordinat yalnız nokta istenen yerde koordinattır.** Kısa devre `Point` ve
  `nokta listesi` ile sınırlandı; kalan her tür, `value_from_token`'ın kendi
  hata iletisine düşer. Çok değerli bir parametrede hata, işe yarayan biçimi de
  söylüyor: `Birden çok değer için anahtarı yineleyin: nesneler=1 nesneler=2`.
- **Virgül kimlik ayıracı değil.** `1,2` tek gramerde bir koordinattır ve
  `1,2,3` koordinat olarak da okunamaz (`classify` reddeder) — yani virgüllü
  biçim ikiden sonra zaten çalışmıyordu. Liste komut satırında anahtar
  yinelenerek, betikte dizi olarak yazılır: `"nesneler": [1, 2, 3]`.
- **Arayüzün üç çağrı yeri düzeltildi.** Öznitelik tablosunun satır seçimi ve
  satır silmesi ile Dışa Aktar penceresinin köşe listesi kimlikleri virgülle
  birleştiriyordu; yani iki satır seçmek hiçbir şey seçmiyor, üç satır silmek
  ayrıştırma hatası veriyordu. Üçü de artık anahtarı yineliyor.
- Betiğin `{"nesneler": [1, 2]}` yolu (`Bus::dispatch`'teki tür onarımı) olduğu
  gibi duruyor ve arayüz = komut satırı = betik eşitliği testle bağlandı.
### Düzeltildi — `NOKTA` günlüğe tek nokta yazıyordu; üçünü de yazıyor

`NOKTA 1,1 2,2 3,3` çizime üç nokta koyuyor, komut günlüğüne ise yalnız
sonuncusunu yazıyordu. Günlüğü yeniden oynatmak üç noktalık belgeyi geri
getirmiyordu — her komuttan istenen şey (CLAUDE.md 6.4) `NOKTA` için tutmuyordu.

Sebep gövdede değil, bekleyicideydi. Bekleyici çözdüğü her değeri sorulduğu
parametrenin altına kaydeder, `noktalar` ise **tek bir listedir**: döngüyle
çözülen her nokta bir öncekinin yerine geçiyordu. `ÇİZGİ` bunu gizliyordu, çünkü
gövdesi kendi vektörünü tutup sonunda bir kez kaydeder — noktalarını döngüyle
çözen sekiz komutun yedisi aynı defteri elde tutuyor, sekizincisi tutmuyordu.

Artık defteri bekleyici tutuyor (`Session::record_awaited`): şartnamesinde nokta
**dizisi** olarak bildirilmiş bir parametrede, dizinin ilk değeri istemcinin
baştan verdiği listenin yerine geçer, sonrakiler onu uzatır. Gövdesinde tam
cevap olan bir komut yine `ctx.record` çağırır ve yine kazanır; en sonda çalışır.
Kayıtlı hiçbir günlük ve betik değişmez: değişen tek satır, `NOKTA`'nın bugüne
kadar eksik yazdığı satırdır.

- Eşitlik kanıtı `tests/unit/test_proof.cpp`'de: `NOKTA` arayüzden, komut
  satırından ve betikten aynı belgeyi ve aynı günlüğü bırakır, günlük üç noktayı
  da geri oynatır, tek noktalık çalıştırma da liste olarak kaydedilir.
- `nokta-dizileri` altın senaryosu belgeyi ve günlüğü yan yana kilitler:
  `NOKTA`, `ÇİZGİ`, `ÇOKLUÇİZGİ`, `ALAN`, `SPLINE`, `LİDER` ve `KOPYALA`'nın
  `bitis`'i — bugün noktalarını döngüyle çözen komutların hepsi.

### Değişti — `@mesafe<açı` artık semt açısı okur: kuzeyden saat yönüne, grad

Kutupsal koordinatın açısı bugüne kadar sabit bir matematik kuralıyla — derece,
doğudan saat yönünün tersine — okunuyordu ve `açı_birimi` ayarı (varsayılanı
`grad`) bu yolda hiç okunmuyordu. Bir harita mühendisinin aletten okuduğu açı
semt açısıdır; `@100<45` artık 45 grad kuzeyden saat yönüne demektir
(TODOS-CAD P0).

- **`core.aci.kural` oturum modu** (`semt` varsayılan · `matematik`; adları
  `açı_kuralı`, `aci_kurali`, `anglerule`, `kural`). `MOD kural matematik`
  eski yöne, `AYAR açı_birimi derece` eski birime döner. Komut günlüğü çözülmüş
  koordinatı tuttuğu için kayıtlı hiçbir oturum ve betik değişmez; yalnız canlı
  metnin — komut satırı, betik dizesi, öneri — anlamı değişti.
- **Kutupsal çözüm iki ayarı okur.** `@100<0` semt'te kuzeye, matematik'te doğuya
  gider; `@100<50` (grad) köşegen. Ayarlar ayrıştırıcıya `core::AngleConvention`
  olarak geçer, global okunmaz. Trigonometri `core/trig.hpp`'nin belirlenimci
  `sin_cos_udeg`'i: kutupsal biçim libm'e giden son yoldu, artık üç platformda
  bit-özdeştir (§7.3).
- **Açık birim soneki:** `@100<45g`, `@100<45d`, `@100<0.7r` — büyük harf de
  olur, parantezli ifadeden sonra da (`@100<(40+5)g`). Yanlış harf adıyla
  reddedilir, sessizce sonek sayılmaz.
- **`ÖLÇ`, `APLİKASYON` ve sürüklerken okunan açı** aynı iki ayarla yazılır:
  `40,9666 grad (kuzeyden saat yönünde)`. Üç ayrı biçimleyici tek
  `core::angle_text`'e indi. Tuval, proje kapsamındaki `açı_birimi`'ni uygulama
  deposundan okuyordu ve hep varsayılanı görüyordu — düzeltildi; `MOD kural` ve
  `AYAR açı_birimi` okumayı anında yeniler.
- **Betik dizesinde koordinat.** `"noktalar": ["0,0", "@100<50"]` komut
  satırıyla aynı gramerle, aynı kuralla okunur (metre). Eşitlik kanıtı: arayüz =
  komut satırı = betik, aynı belge ve aynı günlük; günlük başka kuralda aynen
  oynar.
- Gramer fuzz hedefi `kentos_fuzz_komut` ve 14 tohumluk korpus (her yapıda
  yeniden oynatılır); `koordinat-bicimleri` altın senaryosu iki kural ve üç
  birimle yeniden kaydedildi.

### Eklendi — `YENİ`: yeni çizim başlatmanın bir yolu var

`Dosya ▸ Yeni` bugüne kadar sönük bir Faz 1 yer tutucusuydu ve `YENİ` diye bir
komut yoktu; yani programda boş bir sayfaya geçmenin hiçbir yolu yoktu.

- **`core.new` (`YENİ` · `YENI` · `NEW`).** Ekrandaki çizimin yerine boş bir çizim
  koyar. `AÇ` ile aynı yer değiştirme, eksik olan tek şey okunacak dosya:
  `FileRequest::Verb::New` ile aynı seamden geçer ve `io::FileService` yürütür.
  Belgeyi, geri alma yığınını, aktif katmanı, seçimi, bağlı olunan dosya yolunu,
  proje ayarlarını ve görünümü sıfırlar; uygulama tercihlerine, sembol
  kitaplığına, yazdırma profillerine ve oturum modlarına dokunmaz. Geri
  alınamaz (`UndoPolicy::None`) ve `AÇ` gibi yapay zekâya kapalıdır.
- **Soruyu pencere sorar, komut değil.** Kaydedilmemiş çalışma varsa
  `Dosya ▸ Yeni`, `Ctrl+N` ve şeritteki `+` **Kaydet / Atla / Vazgeç** diye sorar;
  komut satırı ve betik sorusuz geçer, tıpkı `AÇ` gibi. Bir komut gövdesi fare,
  klavye, betik ve yapay zekâ için aynı çalışır ve toplu bir çalıştırmada kipli
  soruyu yanıtlayacak kimse yoktur. Pencerenin bu üç seçenekli sorusu artık tek
  kopyadır (`MainWindow::confirmDiscard`); kapatma da onu kullanır.
- **Sekme şeridine `+` geldi** ve bunu ancak komut gerçek olduğu gün yapabildi.
  Bir önceki sürümde kapatma işareti tam da bu yüzden kaldırılmıştı: "Faz 2'de
  gelecek" diyen bir düğme, pencerenin tutamadığı bir sözdür. `+`, `DocumentTabs`
  üzerinde `newRequested()` olarak çıkar ve `closeRequested`/`splitRequested` ile
  aynı hattan geçer.

### Düzeltildi — belge yer değiştirince eski belgenin geri alma adımı kalıyordu

Bir betik tek bir birleşik geri alma adımıdır (§2.5). Betiğin ortasında bir `AÇ`
çalıştığında, o ana kadar biriken **eski** belgeye ait ters işlemler açık toplu
işte duruyordu ve `end_batch` onları yeni belgenin üzerine bir geri alma adımı
olarak yığına itiyordu — kadastro çiziminde bu, komşu parselin üzerine yazmaktır
(model.md R5). Yer değiştirmenin unuttuğu liste artık tek yerde:
`Bus::document_replaced()` yığını, aktif katmanı ve seçimi temizler, açık toplu
işin işlemlerini *geri sarmadan* düşürür ve toplu işi yeni belgeye göre yeniden
temellendirir — böylece betik çalışmaya devam eder ve kalanı yine tek adımda
birleşir. `AÇ`, `YENİ` ve `VERİTABANI projeac` bu tek çağrıdan geçer; yan etkisi
olarak `AÇ` artık seçimi de temizliyor (eski belgenin anahtarları yeni belgede
başka nesnelere çözülürdü).

### Değişti — doküman sekmesi artık sekme gibi görünüyor

- **`design.md` §7'nin istediği 2 px vurgu çizgisi hiç çizilmiyordu.** Şerit onu
  "aktif sekmede 2 px `--accent` üst çizgi" diye tarif ediyor; kod yalnız zemini
  biraz açıyordu, yani etkin sekme etkisizden bir gri tonuyla ayrılıyordu. Hangi
  çizimin gösterildiğini söyleyen tek işaret buydu.
- **Etkin sekme tuvale bağlandı.** Zemini artık tuvalin zemini ve şeridin ayağındaki
  çizgi onun altında kesiliyor, böylece ikisi tek bir şekil. Sürekli bir çizginin
  üstünde duran kutu, içinde ad yazan bir etiket gibi okunuyordu.
- **Son sekmeden sonraki başıboş dikey çizgi kaldırıldı.** Ayraç her sekmeden sonra
  çiziliyordu; tek çizimli bir pencerede bu, iki yanında hiçbir şey olmayan boş
  şeritte duran bir çizgi demekti.
- **Tek sekmede kapatma işareti yok.** Basınca "birden çok çizim Faz 2'de gelecek"
  diyordu — yani pencerenin belge hakkında söylediği tek şey, tutamadığı bir sözdü.
  Tek çizimin kapatılacağı bir yer yok. İşaret, ikinci bir sekme var olabildiği gün
  kendiliğinden geri gelir (`DocumentTabs::closable`).

*(Bu bölümün "yeni proje hâlâ açılamıyor" notu artık geçerli değil: komut da,
düğme de yukarıdaki `YENİ` girdisiyle geldi — söylendiği gibi, aynı değişiklikte.)*


### Değişti — Enter artık yalnız onaylıyor (öznitelik tablosu ve nesne müfettişi)

Enter, değeri aldıktan sonra **sonraki alanı** da açıyordu — defter kalıbı: yaz,
Enter, yaz, Enter. Kullanıcı bunu geri istedi ve haklı: bir değeri onaylamak ile
nereye gideceğine karar vermek iki ayrı karardır, ikincisini ok tuşları ve fare
zaten daha iyi söylüyor. Enter yazılanı onaylıyor ve duruyor.

Kaldırılanlar: `AttributePanel::commitAndAdvance` ve `nextEditable`,
`AttributeTable::advanceFrom`, ve bunları birbirine bağlayan
`FieldDelegate::advanced` sinyali.

**"Durmak" bedava değildi.** Bir hücreyi onaylamak komut çalıştırıyor, belge
değişiyor ve tablo kendini sıfırlıyor — bu da görünümü hiç geçerli hücresi olmayan
bir hâlde bırakıyor. İlerletme her seferinde yeni bir hücre seçtiği için bu
görünmüyordu; ilerletme kalkınca imleç onaydan sonra tamamen kayboldu ve ok tuşları
başlayacak yer bulamadı. Tablo artık imlecin bulunduğu hücreyi sıfırlamadan **önce**
saklıyor ve sonrasında geri koyuyor — sonradan okumak görünümün kendi kendine
gittiği yeri, yani ilk hücreyi okumak demekti.

`ci-gate-tablo-giris.sh` üç satırı da yeni davranışa göre doğruluyor: yazılan hücre
neresiyse imleç orada kalıyor.


### Düzeltildi — sağ sütun bozulmuştu: kayıtlı yerleşim, dev sekmeler, sıfırlama

Üç kusur bir araya gelince sağ taraf komple gitti. Sürükleme çalışır hâle geldiği
an ortaya çıktılar.

- **Probe koşuları kabuğun durumunu kaydediyordu.** Her probe GERÇEK kabuğu
  sürüyor ve kabuk çıkarken tercihlerini, pencere geometrisini ve dock yerleşimini
  yazıyor. Paneli yüzdürüp sürükleyen yeni sürükleme probe'u, bıraktığı düzeni
  kullanıcının kendi düzeninin üstüne kaydetti; programı sonra açan kişi sağ
  sütunu testin bıraktığı hâlde buldu. `QStandardPaths::setTestModeEnabled` bunu
  kapatmıyor: macOS'ta `QSettings` CFPreferences üzerinden yazıyor ve test kipi
  orayı yönlendirmiyor. Tek güvenilir koruma yazmamak — probe koşusu artık hiçbir
  şey kaydetmiyor. Ayrıca probe, dock düzenini bulduğu gibi geri koyuyor.
- **Qt'nin kendi dock sekme çubuğu.** `AllowTabbedDocks` açıktı; iki panel
  sekmeleşince Qt kendi sekme şeridini onların başlıklarının ÜSTÜNE ekliyordu —
  iki sıra sekme, biri kimsenin tasarlamadığı. Kullanıcının "üstte kocaman
  sekmeler, anlamsız" dediği şey buydu. Her panel zaten kendi `PanelHeader`'ını
  taşıyor; `AllowTabbedDocks` kaldırıldı. `design.md` §6'nın "sekme olarak
  birleştir" hedefi duruyor, ama o panelin kendi dilinde çizilmiş sekmeler
  demektir, Qt'nin şeridi değil; çizilene kadar bir paneli diğerinin üstüne
  bırakmak onu yanına ya da altına yerleştirir.
- **`Yerleşimi Sıfırla` yerleşimi düzeltmiyordu.** Dört dock'un ikisini geri
  koyuyor, onları sekmeleştiriyor, sohbeti ve günlüğü tamamen unutuyordu — yani
  kabuk bozuk göründüğünde başvurulacak tek komut onu bozuk BIRAKIYORDU. Artık
  yapıcının kurduğu yerleşimin aynısını kuruyor.

`kLayoutVersion` 5'e çıkarıldı: sekmeleşmiş hâlde kaydedilmiş bir durum artık
reddediliyor, yani etkilenen herkes bir sonraki açılışta düzgün yerleşimi geri
alıyor — kimsenin ayar dosyasına dokunmadan.


### Düzeltildi — yüzen paneller sürüklenemiyordu

Panel başlıkları dock'ların başlık çubuğudur (`setTitleBarWidget`). `QDockWidget`
sürüklemeyi başlık alanına gelen bir fare basışından başlatır; `PanelHeader` ise o
basışı her durumda **tüketiyordu**. Yerleşik panelde bu, paneli başka bir kenara
taşıyan sürüklemeyi; yüzen panelde ise pencereyi taşımanın tek yolunu götürüyordu.

Basış artık sahibine iade ediliyor. Yalnız bunu yapmak yetmezdi: `design.md` §6 her
başlığa bir `drag_indicator` (**tutamak**) koyuyor, kod onu çiziyor ve **hiçbir şeye
bağlamamıştı** — Öznitelikler paneli üç sekme ve dört işaret taşıdığı için aralarında
tutulacak boşluk da kalmıyordu. Tutamak artık §6'nın söylediği şey: üzerindeki basış
doğrudan dock'a gidiyor, imleç de üzerinde açık ele dönüyor.

Yol boyunca iki hata daha çıktı:

- **İşaretler sekmeyi yenmiyordu.** İkisi aynı şeridin üzerine çiziliyor, işaretler
  en son gidiyor; ama isabet sınaması ikisini birden bildiriyordu. Tutamağa basmak
  bu yüzden işaret dalını atlayıp sekme dalına düşüyor ve sürükleme yerine sekme
  değiştiriyordu.
- **Basış, hover durumundan karar veriyordu.** `mousePressEvent`, `mouseMoveEvent`'in
  bıraktığı `hotTab_`/`hotButton_` değerlerini okuyordu; öncesinde hareket olmayan bir
  basış — imlecin altında yeni beliren bir panele tıklamak gibi — bayat değerle karar
  veriyordu. Artık basışın kendi konumundan sınanıyor.

`KENTOS_LAYOUT_PROBE` paneli yüzdürüp tutamağından gerçekten sürüklüyor ve yerinin
değiştiğini doğruluyor; ayrıca sekmelerin hâlâ geçiş yaptığını. Probe'un ilk hâli,
tutamak yerine başlığın ortasına — yani bir sekmenin üzerine — bastığı için panel
kıpırdamadığı hâlde geçiyordu.


### Değişti — rafın hizalamaları ve seçim şeridi

Rafta üç şey üç ayrı yerden başlıyordu: bölüm başlığı 10 pikselden, gösterim
görseli 13'ten, adı 57'den. Hiçbiri diğeriyle hizalı değildi — okurun
adlandırameden "özensiz" diye gördüğü türden bir ıska. Görsel artık kendi
sütununda, ince çerçeveli bir **numune kartı** olarak duruyor; başlık ve ad dâhil
raftaki bütün yazı kartların bittiği yerden başlıyor, yani tek bir sol kenarı
paylaşıyor ve ekin bölümleri girinti olarak okunuyor.

Seçim rengi de kâğıdı boyuyordu: seçili satırın görseli, çevresindeki on bir
satırdan farklı renkte bir zemin üzerinde duruyordu. Satır artık elle çiziliyor ve
vurgu kartın kenarında duruyor.

### Değişti — `Seçileni kullan` bir şeye ait oldu

Düğme üç yer değiştirdi ve ilk ikisi aynı sebeple yanlıştı: **yalnızdı**. Sağ alt
köşede, aynı satırın sol ucundaki kısa sayımdan bin piksel uzakta; sola alınınca
da listenin kendi kenarını aşarak, iki satırlık bir künyenin karşısında ortalanmış
ve hiçbir şey seçili değilken yanık ve işlevsiz duruyordu.

İhtiyacı olan şey daha iyi bir köşe değil, ait olacağı bir nesneydi. Seçili
satırın künyesi ile o satırı uygulayan düğme tek bir nesnedir: **seçim şeridi**
rafın ayağına yapışık durur, solunda yönetmelik/ek/madde/tarih, sağında düğme, ve
bir satır seçilene kadar hiç yoktur. Hiçbir şey havada durmuyor, hiçbir şey
beklerken sönük değil.

Düğmedeki ✓ de kaldırıldı: tik "yapıldı" demektir ve burada henüz yapılmış bir şey
yok — basıldıktan SONRA karşılaşılan işarettir.


### Değişti — Katman Simgeleyici: raf artık ekin kendisi gibi okunuyor

Kullanıcının hükmü "yerleşimler berbat" idi ve üç ayrı yerleşim hatası vardı.

- **Üst şeritteki kanyon.** `design.md` §8 bu şeridi *dört kontrol* olarak tarif
  ediyor; kod araya bir `addStretch` koymuştu. Dördü birden görünürken bu §8'in
  şeridi gibi okunuyordu, ama Tek Sembol'de DEĞER ile RENK SKALASI gizli olduğu
  için geriye kalan iki kontrol bin yüz pikselin iki ucunda kalıyordu — tek bir
  banda benzemeyen bir bant. Esneme kaldırıldı.
- **Öksüz geometri sekmeleri.** Raf başlığıyla simgeleyici şeridi arasında,
  ikisine de ait olmayan bir bant hâlinde duruyorlardı. Sembolün *ne olduğuna*
  dair üçüncü karardır; artık o kararların verildiği şeritte, birimle aynı
  bileşenle (`Segment`). Sekme olarak okunmalarını sağlamak için yazılmış bütün
  aparat da gitti: onları hiçbir şeye benzetmeyen şey çizim değil, yerdi.
- **Raf gruplandı.** Yüz on üç düz satır yerine, ekin kendi bölümleri başlık
  olarak bir kez yazılıyor (`EK-1a / SINIRLAR / İDARİ SINIRLAR`) ve altında o
  bölümün gösterimleri duruyor. Satırların sağ ucundaki dört yüz piksellik boşluk
  böylece kendiliğinden kapandı ve uzun adlar artık kesilmiyor.

### Düzeltildi — koyu temada siyah gösterimler görünmüyordu

Raftaki gösterim görselleri pencerenin kendi girdi rengini zemin alıyordu; koyu
temada bu `#171B1E`, ve yayımlanmış bir gösterim çoğunlukla siyah bir çizgidir.
`SINIRLAR` bölümünün on bir satırı koyu temada okunamıyordu.

Bir gösterim bu pencerenin kromu değil, imzalanacak bir paftaya basılacak
mürekkeptir ve o pafta beyazdır. Görseller artık her temada **beyaz kâğıt**
üzerinde çiziliyor.

### Değişti — `Seçileni kullan` uyguladığı şeyin yanına taşındı

Bin yüz piksel genişliğinde bir sütunun sağ alt köşesinde tek başına duruyordu,
aynı satırın sol ucunda kısa bir sayım ve arada hiçbir şey vardı — üst şeridin
kanyonunun aynısı. Artık rafın **sol altında**, gözün seçim yaparken zaten
bulunduğu yerde; yanında seçili satırın künyesi — yönetmelik, ek, madde, yayım
tarihi — aynı satırda duruyor. Bir satır seçilene kadar sönük: eskiden her zaman
yanıyor ve boş seçimle basıldığında sessizce hiçbir şey yapmıyordu.

Gösterim sayımı da arama kutusunun yanına, onu değiştiren iki kontrolün yanına
taşındı; böylece rafın altındaki üç bant tek bir satıra indi. Boş raf artık ne
yapılacağını söylüyor.


### Değişti — yerleşim tasarımcısı: kâğıt kahraman, panel bir ad taşıyor

Kullanıcının hükmü hâlâ "kötü" idi. Sebebi de dürüstçe şuydu: pencerede
birbirinin aynı ağırlıkta yirmi altı kutu vardı ve hiçbiri diğerinden önemli
görünmüyordu.

- **Sağ sütunun başlığı artık bir AD.** `ÖZELLİKLER` yerine bakılan şeyin adı,
  cümle düzeninde, gövdeden bir punto büyük — bu penceredeki tek büyük tipografi,
  ve sütuna bir tepe veren şey. Altında türü, kimliği ve ölçüsü. Her üretilmiş
  panelin ilk uzandığı "aralıklı BÜYÜK HARF etiket" kalıbı böylece bir eksildi.
- **Öğenin ayarları iki bölüme ayrıldı:** *Yerleştirme* (nerede durduğu) ve
  *İçerik* (ne gösterdiği). İkincisi türün adını tekrarlamaz — üstteki başlık
  zaten söylüyor. Kuzey okunun İçerik bölümü yoktur, çünkü ayarı yoktur.
- **Sayfa şeridi ve sıra düğmeleri kutusuz oldu** (`Ghost`): altı 32 pikselik
  kutu, sayfanın kendisiyle göz için yarışıyordu. Bunlar satır içi eylem, ki
  `Ghost`'un tanımı bu.
- **Kâğıt büyüdü**: iki sütun daraldı, tuvalin nefes payı yarıya indi.
- **Havanın yeri değişti**: sol sütunda listeyle ekleme çubuğunun ARASINDA açılan
  boşluk artık en altta toplanıyor.
- İki anahtarın altındaki açıklamalar kaldırıldı: `Çerçeve` ve `Kilit` etiketli
  bir anahtar ne yaptığını söyler.

Denenip **kesilen**: kâğıdın altına, cetvellerle aynı adımda bir kesim altlığı.
Fikir konuya aitti ama yaşayacak yeri yoktu — kâğıt tuvali neredeyse doldurduğu
için altlık yalnız birkaç piksellik şeritlerde görünüyordu.


### Düzeltildi — tasarımcı, haritanın içini çıktıda olmayan bir griyle dolduruyordu

Kullanıcının sorusu: "harita arkaplanı neden gri oluyor çıktı yerleşiminde".

Tuval önce kâğıdın gölgesini çiziyor, sonra `paint_layout_page`'i çağırıyordu —
PDF'in, yazıcının ve resim dışa aktarmanın geçtiği **aynı** fonksiyon. Gölgenin
fırçası hâlâ takılıydı ve o fonksiyon kendisine verilen boyacıyla çiziyor, yani
sayfadaki her parsel o fırçayla doluyordu: beyaz kâğıt üzerinde %10 siyah, yani
harita çerçevesinin içindeki soluk gri. Aynı yerleşimin PDF'inde o dolgu yok.

Önizlemenin, imzalanacak bir belgede var olmayan bir dolgu göstermesiydi. Düzeltme
kapsamla: gölge `save()`/`restore()` arasına alındı — döngüden sonra konacak yalın
bir `setBrush(Qt::NoBrush)`, bu satırın üstüne eklenecek ilk yeni çizime kadar
dayanırdı.

`KENTOS_LAYOUT_PROBE` artık kâğıdın ne kadarının dokunulmadan kaldığını ölçüyor:
boyacı geri verildiğinde %94, verilmediğinde %49.


### Düzeltildi — yerleşim tasarımcısı: üst üste binen panel ve yanlış yazdırma yolu

İki hata, ikisi de kullanıcının bildirdiği hâliyle:

- **Sağdaki anahtar seçilince düzen bozuluyordu.** Özellik paneli her yeniden
  kurulduğunda eski satırlar düzenden çıkarılıp `deleteLater` ile siliniyordu; ama
  `deleteLater` olay döngüsü dönene kadar widget'ı panelin **görünür** bir çocuğu
  bırakır ve o aralıkta widget en son bulunduğu koordinatlarda çizmeye devam eder.
  Yani her yeniden kurulum yeni satırları eskilerin **üstüne** boyuyordu. Düzen yanlış
  kurulmuyordu; önceki panel hiç gitmemişti.
- **Yerleşim için başlatılan çerçeve normal yazdırmaya gidiyordu.** Bir yerleşim seçip
  sahneden alan seçtikten sonra yazıcı ikonuna basmak çerçeveyi yakalıyor ama neden
  açıldığını unutuyordu. Çerçeve artık ne için açıldıysa oraya teslim ediliyor; kâğıt
  profilinin açıkça seçilmesi fikir değişikliği sayılıp yerleşimi temizliyor.
- **Cetvel fırçayı geri bırakmıyordu** (bu turda eklenen cetvelin ilk hâli): seçili
  kutu cetvelin şerit rengiyle doluyordu — açık temada zor fark edilir, koyu temada
  harita çerçevesinin yerinde siyah bir delik.

### Değişti — yerleşim tasarımcısı baştan düzenlendi

Kullanıcının tarifi "çok kısır ve kötü" idi, ve sebebi bir üslup meselesi değildi:
`ÇIKTIÖĞE` on yedi öğe ayarı, `ÇIKTIYERLEŞİMİ` dokuz sayfa ayarı kabul ederken panel
öğe ayarlarının dokuzunu, sayfa ayarlarının **hiçbirini** gösteriyordu.

- **Kâğıdın üstünde ve solunda milimetre cetveli.** Seçili öğenin kapladığı açıklık iki
  cetvelde de vurgulanır ve sürükleme boyunca onunla hareket eder — dört sayıyı tek tek
  okumak yerine tek bakışta görülür. Adım ölçeğe göre seçilir, A0'da da A5'te de okunur.
- **Sağ sütun asla boş kalmaz.** Seçim yokken sayfanın kendi ayarları açılır: kâğıt,
  yön, kenar boşluğu, çözünürlük, yerleşim adı. Dördü de bu pencereden hiç
  ulaşılamıyordu.
- **Öğenin bütün ayarları erişilebilir oldu:** durduğu sayfa, çizim sırası (`sira`),
  adı (`yeni_ad`), ızgara aralığı, harita çerçevesinin çizdiği katmanlar, tablonun
  sütunları ve satır sınırı, ve ölçek çubuğu / kuzey oku / lejant / grafiğin hangi
  haritaya bağlı olduğu.
- **`tur=grafik` ekleme çubuğuna girdi.** Komut kabul ediyordu, fare ile istenemiyordu
  (CLAUDE.md 5.15).
- **Öğe listesi bir içindekiler tablosu oldu:** tür glifi, öğenin **adı**, ve kutunun
  ölçüsü. Kimlik ipucuna taşındı; komut satırının andığı kimlik sağdaki **Ad**
  alanından değiştirilir.
- **Ekleme çubuğu etiketlendi:** iki sütun, dokuz sözcük — sekiz simge karesi yerine.
- **Sayfa bölümü bir şeride indi:** `‹ 2 / 7 ›` artı üç sayfa fiili; başlıklı bir form
  alanı ve üç satır yerine.
- **Yeniden kurulan satırlar temayı alıyor.** `DialogFrame` çocukları bir kez gezer;
  sonradan kurulan her denetim `Themed`'in varsayılanını, yani **koyu**yu tutuyordu —
  ızgara açılır listesi açık temada siyah çiziliyordu.
- Aynı gerçek üç yerde yazılıyordu (sayfa boyu, sayfa sayısı, öğe sayısı); her biri tek
  yerde kaldı ve tuvalin altındaki satır yalnız jestleri anlatıyor.

### Düzeltildi — sayfa ayarları birbirini siliyordu

`ÇIKTIYERLEŞİMİ islem=sayfa` kendisine **verilmeyen** her argümanı varsayılana
düşürür: `kagit` yoksa A4, `yon` yoksa dikey, `kenar` yoksa 10. Yani yalnız
`yon=yatay` yazan bir satır A3'ü yan çevirmez — onu A4'e çevirir ve kenar boşluğunu
da sıfırlar. Tasarımcının yeni sayfa alanları her biri tek argüman gönderseydi
dördü birbirini bozardı ("kenar boşluğunu değiştirdim, kâğıdım küçüldü"). Panel
artık sayfayı **olması gereken hâliyle** bütün yazıyor; dokunulan alan duranı
geçersiz kılar. `KENTOS_LAYOUT_PROBE` tek alanı değiştirip diğer üçünün yerinde
durduğunu doğruluyor.

Komut satırında aynı tuzak duruyor ve ayrı bir değişikliğin konusu: `islem=sayfa`
değiştiren bir fiildir, kuran değil, ve verilmeyen argümanın duranı koruması
beklenir.

### Eklendi — `ÇIKTIYERLEŞİMİ islem=sayfa` artık `dpi` kabul ediyor

Çözünürlük yalnız `islem=ekle` sırasında seçilebiliyordu. Model onu taşıyor, `YAZDIR`
onu okuyor, ve **hiçbir istemci** — komut satırı, betik, tasarımcı ya da ajan — sonradan
değiştiremiyordu: varsayılan 300 dpi ile kurulmuş bir yerleşim orada kalıyordu.


### Değişti — onay modeli: tek yol yerine iki yol (TODOS S-05, S-04; §5.2.1 tadili)

**Bu bir kural değişikliğidir ve kullanıcının kararıyla yapıldı.** Zincir, hiyerarşinin
gerektirdiği sırayla ve tek değişiklikte taşındı; aradan biri atlansaydı anayasa ya
niyetin kaynağıyla ya da kendi rulebook'uyla çelişirdi (CLAUDE.md Article 0.2/0.4).

- **`kentoscad.md` §5.2.1**: "Otomatik uygulama yok" → **"Onaysız uygulama yok"**. AI'nın
  ürettiği bir dizi çizime iki yoldan ulaşır: önizleme + onay, ya da kullanıcının
  **önceden, kendisi için, bilerek** kurduğu onay politikası. **§5.2.4 değişmedi:** AI
  imza atamaz — politika, kullanıcının imzasının kapsamını önceden tarif etmesidir,
  imzasının yerine geçmesi değil.
- **`CLAUDE.md` 5.7** yeniden yazıldı ve neyi geçersiz kıldığını adıyla yazıyor
  (Article 0.5). Yeni **5.23**: hiçbir çağıran kendi yetkisini genişletemez.
- **`.claude/ai.md`** R3, P1, P15, R24.
- **`ci-gate-ai.sh`**: onay fabrikası **iki kapalı çağırana** açıldı — kart ve politika
  yolu — ve **üçüncüsünde hâlâ kırıyor**.
- **Kod**: `ai::decide_by_policy` ve `AiService::applyByPolicy`.

**Öntanımlı davranış değişmedi.** `her_degisiklikte` hâlâ fallback, yükseltmede de öyle
kalıyor: ayarı hiç ellememiş bir kullanıcı için her şey aynı.

**Ve değişmeyenler asıl mesele:** kapsam dışı iş hiçbir modda yürümüyor (`Deny` onayla
açılmıyor); girdisi eksik plan yürümüyor; onay kartta okunan adımlara bağlı; her karar
denetim kaydına hangi politikanın verdiğiyle yazılıyor; ve politikayı **yalnız
kullanıcı** değiştirebiliyor. İkinci yolu bir "güven kipi" değil bir **izin** yapan şey
bu sonuncusu.

**Yol üstünde bulunan kusur:** MCP cevabı her hâlde "Çizim değişmedi" diyordu. Politika
uygulamışsa bu, istemciye üzerinde çalıştığı çizim hakkında düpedüz yalandır. Cevap artık
duruma göre konuşuyor ve her hâlde ekliyor: **uygulayan sen değilsin.**

### Güvenlik — onay, kartta okunan satırlara bağlandı (TODOS S-04)

- Onay yalnız önerinin **kimliğine** bağlıydı. Kart çizildikten sonra öneriyi açan
  istemci ona bir adım daha ekleyebiliyor — bir diziyi tek geri alma adımında
  toplamanın yolu budur ve bu hafta eklendi — dolayısıyla **iki satır gösteren bir
  kart üç satır uygulayabilirdi**, ve denetim kaydı mühendisin üçünü de
  onayladığını yazardı.
- `Plan::content_fingerprint` (adımların komut kimlikleri ve **çözülmüş
  argümanları**; satırlar değil, çünkü satır okunan şeydir, argüman çalışan şey) ve
  `Approval::content`. Kart ne çizdiyse onu taşıyor.
- Farklı bir öneri **reddediliyor, kırpılmıyor**: dürüst cevap önerinin şu anki
  hâlini gösteren yeni bir karttır; ön eki sessizce uygulamak başka türlü bir yalan
  olurdu. Ret bir karar değil, öneri beklemeye devam ediyor.
- İddiada bulunmayan bir çağıran sessizce güvenilmiyor — yalnızca iddiada
  bulunmuyor ve denetim çalışmıyor; kart her zaman iddiada bulunuyor.

### Eklendi — grafik öğesi (TODOS L-09, kısmi)

- Yeni yerleşim öğesi `grafik`: bir katmanın bir öznitelik sütununa göre **nesne
  sayısı**, çubuk grafik olarak. Kaç parsel `Arsa`, kaç parsel `Tarla` — bir imar
  ya da kadastro paftasının gerçekten gösterdiği özet, ve ölçülen alana ihtiyaç
  duymadığı için **hukuki alan sanılamayacak** tek özet.
- Çubuklar **katmanın kendi rengini** alıyor: grafik ile haritadaki katman aynı şey
  olarak okunsun diye. İlgisiz renklerde bir grafik, okuyucunun öğrenmesi gereken
  ikinci bir lejanttır.
- Sayı çubuğun üstünde yazılı: bir eksenden tahmin edilmesi gereken çubuk, yanlış
  tahmin edilecek çubuktur. Değeri olmayan nesneler `(boş)` çubuğunda toplanıyor,
  atılmıyorlar.
- **Kaynağı kullanılamayan bir grafik boş çizmiyor, sebebini yazıyor** — kâğıdın
  üstüne ve `trouble` listesine, yani `islem=denetle` sonucuna ve dışa aktarma
  uyarılarına. L-09 bunu açıkça istiyor, ve imzalanan bir sayfadaki boş bir
  dikdörtgen aylar sonra kimsenin cevaplayamayacağı bir sorudur.
- `LayoutItemKind::Chart` **enum'un sonuna** eklendi: önceki değerler numaralarını
  koruyor ve eski bir yapının yazdığı çizim hâlâ okunuyor (io.md R10).
- `sutunlar=` artık tabloya **ve grafiğe** veriliyor; ret mesajı ikisini de
  adlandırıyor.
- `KENTOS_LAYOUT_PROBE` ikisini de sınıyor: kaynağı olan grafik gerçekten çubuk
  çiziyor mu, kaynağı olmayan gerçekten sebebini söylüyor mu.
- **Probe'un kendi kusuru da düzeltildi:** açık bir etkileşimli komut varken
  pencerenin komut satırına yazılan satır o komuta GİRDİ olur — tasarım böyle — ve
  bu, iki alandan birini yutup grafiğe yarım veri bırakıyordu. Kurulum artık veri
  yolundan geçiyor.

### Eklendi — rapor modeli: ada başına bölüm ve toplam (TODOS L-11, kısmi)

- **Bir atlas ve bir rapor farklı şekillerdir** ve biri diğerinin işini yapamaz.
  Atlas düz bir döngüdür: aynı sayfa, her nesne için bir kez. Rapor
  hiyerarşidir — ada 1284 kendi başlığını ve toplamlarını alır, sonra her parseli
  için bir sayfa gelir. Döngü bunu anlatamaz çünkü **bölüm diye bir kavramı
  yoktur**.
- `core::Report` ve `core::report_groups`: bölüm değeri, üyeler, adet, kutu alanı
  toplamı ve bölümün kendi kapsamı. Üyeler `atlas_targets`'tan geliyor — adlar,
  öznitelikler, belirlenimli sıra ve yinelenen ad ekleri orada karara bağlanıyor;
  ikinci bir üye listesi "bu sayfa neyi kapsıyor" sorusuna ikinci bir cevap olurdu
  (5.10).
- **`kutu_alani_mm2` ölçülen alan değildir** ve adı bunu söylüyor. Ölçülen alan
  cadastre alanının cevabıdır; bir raporun üstündeki toplamın hukuki alan
  sanılması, imzalanan bir belgede yapılabilecek en pahalı karışıklıktır.
- **Grup değeri olmayan nesne kendi bölümünü oluşturuyor**, atılmıyor: ada
  numarası girilmemiş bir parsel kurulmakta olan bir çizimde olağandır, ve onu
  sessizce atlayan bir rapor eksik veriyle imzalanan bir rapordur.
- `ÇIKTIYERLEŞİMİ islem=rapor` bölümlemeyi **okur ve bildirir**, hiçbir şey
  basmaz: yüz sayfa yazılmadan önce bölümleme görülsün diye.

### Düzeltildi — `dosya=cikti.png` bir PDF yazıp adını `cikti.png` koyuyordu (TODOS L-13)

- Ve **"tamam" diyordu**. Sonra o dosyayı açmaya çalışan her şey — tarayıcı, rapor,
  e-posta eki — başarısız oluyordu ve hiçbir yerde sebebi yazmıyordu. Doğru adla
  yazılmış yanlış bir dosya, hiç yazılmamış bir dosyadan kötüdür.
- **Uzantı isteğin kendisidir.** Yazılabilen biçim yazılıyor, yazılamayan **adıyla
  reddediliyor** ve ne yazılabildiği sayılıyor. "Desteklenmeyen özellik sessiz düz
  PDF'e indirgenmez" L-13'ün kendi cümlesi, ve sessizce yeniden adlandırmak bu
  indirgemenin en kötü biçimi: dosya işe yaramış gibi görünüyor.

### Eklendi — raster, SVG ve world file (TODOS L-13, kısmi)

- **`.png` / `.tif`**: sayfa istenen dpi'de raster olarak yazılıyor, beyaz zeminle
  (saydam bir PNG koyu arka planda siyah üstüne siyah yazı olur) ve doğru fiziksel
  çözünürlük etiketiyle.
- **`.svg`**: vektör, **sayfa başına bir dosya** — bir SVG bir çizim tutar, çok
  sayfalı bir yerleşim tek dosya olamaz. Adlar sonuçta yazılı, keşfedilmeye
  bırakılmıyor.
- **World file** (`.pgw` / `.tfw`) raster çıktının yanına, sayfada hedeflenmiş bir
  harita çerçevesi varsa. Altı satır, her CBS okur, kütüphane gerektirmez ve bir
  görüntü kodlayıcısı tarafından sessizce düşürülemez.
- World file **sayfanın tamamını** tanımlıyor ama çerçevenin sayfadaki yerini hesaba
  katıyor: katmasaydı lejantı koordinatlandırmış olurdu. Ölçek **iki yönde de aynı**,
  çünkü çerçeve pencereyi germiyor sığdırıyor — ilk yazdığım hâli iki ayrı ölçek
  üretiyordu, yani hiç çizilmeyen bir resmi tarif ediyordu.
- Hedeflenmiş çerçeve yoksa world file **yazılmıyor** ve sonuç bunu söylüyor:
  görüntünün zeminde bir yeri yok.
- `KENTOS_PRINT_PROBE` dördünü de gerçek dosyada sınıyor: PNG gerçekten PNG mi,
  world file'ın afini çerçevenin ortasını hedeflenen pencerenin ortasına düşürüyor
  mu, desteklenmeyen biçim gerçekten **hiç dosya bırakmıyor** mu, SVG gerçekten SVG
  mi. Afin sınaması ilk seferde kırıldı ve **hatalı olan sınamaydı**: bir world
  file 0 numaralı pikselin MERKEZİNİ adlandırır, köşesini değil.

### Düzeltildi — atlas nişanı belge özetine girmiyordu (içerik karması)

- `Document::content_hash` yerleşimin **atlas bloğunu** katlamıyordu. Atlas kaç
  sayfanın basılacağına ve her birinin neyi göstereceğine karar verir; iki ayrı
  katmana nişanlanmış iki çizim iki ayrı belge basar, dolayısıyla aynı parmak izini
  taşıyamazlar. `linked_map`'i bu katlamaya koyan gerekçenin aynısı.
- **Değerlendirme koşucusu buldu**, kodu okuyarak değil: bir atlas senaryosu
  çalıştı, sayfa sayısı değişti ve içerik özeti hiç kıpırdamadı.

### Eklendi — Türkçe değerlendirme seti, koşucusu ve sayacı (TODOS A-08, kısmi)

- `tests/ai-eval` bir README'den ibaretti — ama CLAUDE.md Article 8.9 "başlangıç
  seti harness'ı ve saklanan temeliyle birlikte geliyor" ve "`make check` vaka
  sayısını bildiriyor" diyordu. **Hiçbiri yoktu**; madde, ağacın sahip olmadığı bir
  düzeneği anlatıyordu. Bugün düzelttiğim dördüncü "olmayan şeyi anlatan metin".
- **Set** (`senaryolar.json`, 16 vaka, 28 terim): gerçek Türkçe istek + beklenen
  komut dizisi.
- **Koşucu** (`test_ai_eval.cpp`) iki soruyu ayrı sorar. (1) Dizi hâlâ geçerli mi:
  her adım **canlı kütüğe** karşı ayrıştırılıp çözülüyor, yani cevap anahtarı
  programla birlikte çürümüyor — yazdığım ilk on altı vakadaki **beş hatayı** aynı
  gün yakaladı. (2) Üç istemci aynı şeyi mi üretiyor: aynı çözülmüş çağrı komut
  satırından, JSON betiğinden ve ajan yolundan geçiyor; içerik özeti ve günlük
  satırları birebir aynı olmalı (A-08 kabulü). Argümanlar yol üstünde
  `Value::to_json`/`from_json` turunu da yapıyor, yani Article 1.4 de sınanmış
  oluyor.
- **Modelin doğruluğu burada ölçülmez ve ölçülemez**: hiçbir test canlı bir
  sağlayıcıya bağlanmaz (ai.md P10). Set doğru cevabı tanımlar.
- **`scripts/ci-gate-eval.sh`** her koşuda vaka sayısını ve hedefe kalanı yazıyor —
  Article 8.9'un sapmaya bağladığı koşul buydu. Küçük sette **kırmıyor**: kıran bir
  kapı yalnızca devre dışı bırakılır, ve sapma zaten onaylı.

### Eklendi — denetim kaydı hangi iznin hangi işi yürüttüğünü söylüyor (TODOS S-06, kısmi)

- İki alan: **`karar_veren`** (kararı ne verdi) ve **`onay_politikasi`** (karar
  anında yürürlükte olan kural).
- `karar_veren` bugün her satırda `insan` yazıyor, çünkü bu sürümde kararı yalnız
  bir insan verebilir. **Yine de yazılıyor**, ve sebebi tam olarak bu: hiçbir şey
  söylemeyen bir kayıt, kuralın değiştiği günden sonra yazılmış bir kayıttan
  ayırt edilemez — S-06'nın "otomatik işlem insan tıklaması gibi yazılmasın"
  kuralı o zaman geriye dönük olarak denetlenemez olurdu. İnsan hâli, tek hâl
  olduğu sırada kendini söylemeli.
- `onay_politikasi` **onayla birlikte taşınıyor**, kayıt yazılırken bakılmıyor:
  ayar tıklama ile satırın diske inmesi arasında değişebilir, ve bir kaydın
  koruması gereken şey kararın **altında verildiği** kuraldır.
- Kayıt satırının hiçbir gizli değer taşımadığı da sınanıyor (5.21).
- Yol üstünde: `core.ai.onay_politikasi`'nın yanındaki bir yorum, yeni kurulumun
  "first-run path" ile `riskli_islemlerde`'ye taşındığını söylüyordu. Ağaçta öyle
  bir yol **yok**; yorum, programın sahip olmadığı bir mekanizmayı tarif ediyor ve
  sonraki okuyucuyu onu aramaya gönderiyordu.

### Düzeltildi — onay politikası ayarı programın yapmadığı şeyi vaat ediyordu (TODOS S-03, kısmi)

- Ayarın özeti `otomatik` için "yetki kapsamı içindeki ve girdileri tam olan iş
  **onay beklemeden yürür**" diyordu. Bu bu sürümde **yanlış**: CLAUDE.md 5.7
  onaysız uygulamayı yasaklıyor ve `ai::Gate` bunu ancak öneri kartının
  üretebildiği bir `ai::Approval` isteyerek uyguluyor. `otomatik`'i seçen kişiye
  program yine her seferinde soruyordu — ona aksini söylemiş bir kontrol
  tarafından.
- **Üstelik iki metin birbiriyle çelişiyordu.** Ajanlara verilen yönerge
  (`default_instructions`) "Bunu atlayan bir yol, bir başlık ya da bir AYAR
  yoktur" diyor. Ayar ise tam o ayarın atladığını söylüyordu.
- Özet artık ne olduğunu söylüyor: `otomatik` **henüz yürürlükte değil**, hangi
  mod seçili olursa olsun her değişiklik kartta bekliyor. Değer duruyor — politika
  motoru yazıldı ve sınandı, kuralın değiştiği gün kullanılacak — ama programın
  bugün yapmadığı bir davranış olarak anlatılmıyor (11.8).
- Test bunu **olumsuz** biçimde tutuyor, yani yeniden yazılınca da geçerli: özet,
  program katılım isterken katılımsız yürütme vaat edemez. Kuralın değiştiği gün
  bu test de aynı commit'te değişir — ki maksat budur.

### Düzeltildi — "iptal edildi" mesajı uygulanmış işleri gizliyordu (TODOS A-06, kısmi)

- **Dur**, "İptal edildi. Çizimde hiçbir şey değişmedi." diyordu. Bu, o konuşmada
  bir öneriyi uygulamış biri için **yanlıştı** — ve bir öneriyi uyguladıktan hemen
  sonra Dur'a basmak en olası andır. İptal edilen TUR hiçbir şey değiştirmez;
  **iş** epey şey değiştirmiş olabilir, ve uygulanmış işi gizleyen bir mesaj hiç
  mesaj olmamasından kötüdür.
- Mesaj artık bu konuşmada uygulanmış önerileri adıyla sayıyor ve Ctrl+Z'yi
  gösteriyor. Hangisinin uygulandığı **öneri defterinden** okunuyor, bir bayraktan
  değil: kişi bir kartı uygulayıp geri almış, sonra başkasını uygulamış olabilir.
- **Tur sınırı artık bir ayar** (`core.ai.tur_siniri`, 1–50, öntanımlı 8): bir
  modelin okumakta ne kadar ısrar edebileceği, bu makinedeki kişinin kararıdır,
  bir C++ sabitinin değil. **Yetki ayarı** olarak işaretlendi — sınıra çarpıp onu
  yükselten bir model, reddedildiği turları kendine vermiş olurdu (S-04).
- Bu kararı **testin kendisi zorladı**: `core.ai.` altına eklenen sekizinci ayar,
  yetki mi tercih mi olduğu söylenene kadar yapıyı kırdı. Kapı yazıldığı gün işe
  yaradı.

### Sağlamlaştırıldı — aynı desen için ağaç tarandı

- `aimAt`'ı çökerten desen — belgeye işaret eden bir imleci veri yolu çağrısının
  üstünden taşımak — bütün `/src/app`, `/src/command`, `/src/ai` ve
  `/src/processing` için tarandı. İki yer daha bulundu ve ikisi de **bugün
  zararsızdı**: `panels.cpp`'de `SEÇ` katman tablosunu yeniden yazmıyor,
  `mcp.cpp`'de bir plan kaydı komut kütüğünü değiştirmiyor. İkisi de kopyaya
  çevrildi, çünkü "bugün zararsız" tam olarak `aimAt`'ın dayandığı şeydi.

### Düzeltildi — çıktı tasarımcısında serbest bırakılmış belleğe erişim (çökme)

- `LayoutDesigner::aimAt` belgeye **işaret eden** bir `const LayoutItem*` tutuyor,
  sonra `ÇIKTIÖĞE islem=ayarla` çalıştırıyor — bu komut yerleşimin öğe dizisini
  yeniden yazıyor — ve ardından o işaretçiden `map->id` okuyordu. `strlen`
  çöp bir adreste patlıyordu.
- **Gerçek uygulamada da çöküyordu**, testte değil yalnızca: kullanıcı çıktı
  tasarımcısında harita çerçevesini nişanladığında. Ad artık belge kıpırdamadan
  önce kopyalanıyor.
- Haftalardır "seyrek düşen test" (`layout-designer`) sanılan şey buydu. Probe bir
  döngüde koşturulunca çıkış kodu **139** çıktı — SIGSEGV — ve macOS'un çökme
  raporu satırı adıyla verdi. Düzeltmeden önce 12. ve 5. koşuda çöküyordu; sonra
  **60 ardışık koşu temiz**.

### Düzeltildi — onaydan sonra iş devam ediyor (TODOS A-04, kısmi)

- **Konuşma ilk kartta bitiyordu.** "Şu adanın paftasını çıkar" katman kurmayı,
  nesne yerleştirmeyi, yerleşim kurmayı ve PDF basmayı gerektirir; sohbet
  bunların **ilkini** öneriyor, kullanıcı Uygula'ya basıyor ve **hiçbir şey
  olmuyordu**. Kalan her adım yeniden istenmek zorundaydı — üstelik modelin artık
  ipini kaybettiği bir konuşmada.
- Artık karardan sonra modele ne olduğu anlatılıyor — önerinin durumu, çizimin
  yeni sürümü, yazılan dosyalar, uyarılar — ve değişikliği **doğrulaması**
  isteniyor. Sonucu okumadan "düzelttim" demek, A-05'in adını koyduğu kusur.
- **Bu otomatik uygulama değil**: burada hiçbir şey uygulanmıyor, kararı kişi
  zaten verdi; devam eden **konuşma**. Modelin bundan sonra isteyeceği her çizim
  adımı yine öneri ve yine kart (5.7).
- **Ret tur harcamıyor**: konuşmaya yazılıyor, model o anda söz almıyor. Az önce
  hayır demiş birine cevap vermek, kararla tartışmaktır.
- **Tur sınırı bir onayla sıfırlanmıyor**: turunu tüketmiş bir iş orada duruyor ve
  durduğunu söylüyor.
- **Bilinmeyen bir araç adı artık cevapsız kalmıyor.** Atlanıyordu, yani modelin
  çağrı kimliği yanıtsız kalıyordu — ve cevapsız bir araç çağrısı alan sağlayıcı
  sonraki turun tamamını reddeder; kullanıcı bunu konuşmanın sebepsiz ölmesi
  olarak görür. Cevap aracın yokluğunu söylüyor ve `araclari_ara`yı gösteriyor.
- **Her çağrı kimliği tam bir sonuç alıyor**: okuma/bilinmeyen `runReadTools`'un,
  yazma `fileWrites`'ın; iki küme ayrık ve hepsini kapsıyor. İkinci bir sonuç,
  hiç sonuç olmamasıyla aynı kusurdur — sağlayıcı hangisine inanacağını bilemez.
- `KENTOS_CHAT_PROBE` bunu sınıyor ve kusur geri konularak doğrulandı.

### Eklendi — lejant artık haritanın kendi sembollerini çiziyor (TODOS L-07, kısmi)

- **Anahtar, düz renk kutusu değil.** Her katmanın yanında o katmanın **gerçek
  sembolü** duruyor ve haritayı çizen **aynı** boru hattından geçiyor
  (`app::paint_symbol` → `render::Backend`): taramalı bir katman anahtarında da
  taramalı, kesik çizgili bir sınır anahtarında da kesik çizgili. "Parseller
  turuncudur" diyen bir anahtarın anlattığı katman tarama + kesik sınır +
  işaretçi ile çiziliyorsa, o anahtar hiç anahtar olmamasından kötüdür — bu
  imzalanan bir belge.
- **Ve vektör olarak gidiyor.** İlk hâli `symbol_preview`'in resmini sayfaya
  basıyordu; PDF her satır için bir raster kazandı (25.504 → 9.906 bayt fark).
  Bu, harita çerçevesinin düzeltildiği kusurun aynısı (L-12): bir sembolün
  fotoğrafı ölçülemez, seçilemez, çözünürlüğe bağlıdır.
- **Kapı da sıkılaştırıldı.** `KENTOS_LAYOUT_PROBE` raster aramasında bir delik
  vardı: küçük bir blit XObject olmaz, içerik akışında kısaltılmış anahtarlarla
  `BI … ID … EI` olur — ve lejant çipleri tam o boyutta. Artık `/BPC` de
  aranıyor, ve anahtarın **bir şey çizdiği** sayfayı gerçekten render edip
  katmanın rengini arayarak doğrulanıyor: hiçbir şey çizmeyen bir anahtar, raster
  sınamasını yanlış sebeple geçerdi. Kusur geri konularak sınandı; kapı ikisini de
  yakalıyor.
- **Taşınan projede logo kaybolmuyor** (`LayoutFacts::project_dir`): göreli bir
  resim yolu artık **projenin klasörüne** göre çözülüyor, programın çalışma
  dizinine göre değil. Göreli yol taşınabilir olmanın tek yoluydu ve tam da bu
  yüzden çalışmıyordu.

### Düzeltildi — önizleme aylardır hiçbir şey bildirmiyordu ve temiz görünüyordu (TODOS A-05, L-15)

- MCP'nin `kentoscad://yerlesim/denetim` kaynağı `ÇIKTIYERLEŞİMİ islem=denetle`
  komutunu okuma kapısından çalıştırıyor — ve o kapı **komut başına** bir bayrağa
  bakıyordu. Yerleşim komutu sayfa da eklediği için reddediliyordu, sonuç da her
  sayfa için **boş bir `satirlar` listesi**: yani "bu sayfada sorun yok". Bir
  önizlemenin asla kazara veremeyeceği tek cevap.
- Kapı artık **bu çağrının** ne yapacağına bakıyor: `command::effect_of(spec, args)`
  fiili argümanlardan okuyor ve tanımsız bir fiilde komutun bütününe düşüyor —
  yani eksik fiil tablosu olan bir komut geçmiyor, reddediliyor (C-02).
  Görünümü gezmek etkisiz sayılıyor, CLAUDE.md 2.10'un kendi sözleriyle.
- Ret artık **söyleniyor**: `hata` alanı. Yutulmuş bir ret, temiz bir sayfa gibi
  okunuyordu.
- `KENTOS_MCP_PROBE` bunu gerçek sokette sınıyor: taze bir sayfanın hedeflenmemiş
  harita çerçevesi vardır, dolayısıyla liste boş olamaz.

### Eklendi — ne neyin üstünde (TODOS A-05)

- **`core::layout_overlaps`**: hangi öğe hangisinin üstünde, hangi sayfada,
  altındakinin yüzde kaçını kapatıyor, ve **gizliyor mu** (üstteki öğenin zemini
  mat mı). En çok kapanan üstte, sıralama belirlenimli.
- **Çakışma sorun ilan edilmiyor** ve bu bilinçli: çoğu çakışma tasarımın kendisi —
  başlık, ölçek çubuğu ve kuzey oku harita çerçevesinin üstünde durur. Bunları
  kusur saymak doğru kurulmuş her sayfada boşuna alarm vermek olurdu.
- **Hiç istenmeyen tek hâl sorun listesinde**: bir öğe mat bir öğenin altında
  tamamen kalıyorsa hiç görünmez, sayfa da basılır ve bitmiş görünür.
- "Lejant haritanın üstüne binmiş" insanın söylediği bir cümledir; buna cevap
  verebilmek için programın neyin neyi kapattığını söyleyebilmesi gerekiyordu.
  `islem=denetle` artık `sorunlar` ve `ust_uste_binen` alanlarını hem insana hem
  makineye veriyor.

### Eklendi — uzun işler: nerede kaldı, ve aynı isteği iki kez sormak (TODOS M-06)

- **`PlanState::Running` (`uygulaniyor`)**: bir karar değil, kararla sonuç
  arasındaki süre. Dört yüz parsellik bir atlas dakikalar sürer ve o sırada
  durumu soran istemciye `beklemede` demek yanlıştı — "hâlâ bir insanı bekliyor"
  demek olur ve ajanı kullanıcıya "neden hâlâ tıklamadınız" diye sormaya
  gönderir. Cevap artık **biten adım** ve **toplam adım** sayılarını taşıyor;
  yüzde değil, çünkü adım kişinin onayladığı ve istemcinin adlandırabildiği birim.
- **Yarım bir çıktı bitmiş görünmüyor.** Yazılan dosyalar, yeni sürüm ve uyarılar
  ancak toplu iş kapandıktan sonra yazılıyor; `uygulaniyor` hâlindeki cevap
  bunları hiç taşımıyor ve "henüz bitmedi" diye **sözle** söylüyor. İlerleme her
  adım **bittiğinde** sayılıyor, başladığında değil.
- **Idempotency anahtarı** (`_meta.idempotency` / `cad.kentos/idempotency`):
  bağlantısı kopan bir istemci çağrının ulaşıp ulaşmadığını bilemez ve yeniden
  denemekten başka bir şey yapamaz. Anahtarsız bir deneme ikinci bir öneri
  açıyordu — bilgisayar başındaki kişinin ekranında tek iş için iki aynı kart.
  Artık ekrandaki önerinin kimliği dönüyor ve cevap "zaten açıktı — aynı istek"
  diyor, ki istemci bunu "açıldı" diye okumasın.
- Anahtar **istemciye özel** (`PlanStore::find_by_key`): iki ajan aynı sözcüğü iki
  ayrı iş için kullanabilir ve hiçbiri anahtar tahmin ederek bir başkasının
  önerisine ulaşamaz (M-07).
- `PlanStore::begin_apply` ve `settle` ayrı: biri kararın **yürütüldüğünü**, diğeri
  kararın kendisini kaydediyor. Çalışan bir planın ikinci kez uygulanması
  reddediliyor; `Running`'den karara geçmek serbest, çünkü `Running` bir karar
  değil.
- `ÖNERİ islem=durum` aynı iki sayıyı ve biten iş için yazılan dosyaları söylüyor:
  komut satırı ile protokol tek bir şey anlatıyor (Article 1.2).

### Güvenlik — bir istemci kendi iznini genişletemez (TODOS S-04, kısmi)

- **`SettingSpec::authority`**: bir ayarın tercih mi yetki mi olduğu, ayarın
  **kendi alanında** yazılı. Yedi ayar yetki: üç onay/soru/üzerine-yazma
  politikası, projenin hassaslık işareti ve üç MCP dinleyici ayarı. Liste yapay
  zeka katmanında tutulmuyor — orada tutulan bir liste ikinci bir listedir ve
  bir yıl sonra politika ayarı ekleyen kişi onu düzenlemesi gerektiğini bilmez.
- **`ai::escalates` / `ai::escalation_refusal`**: bir çağrının çağıranın kendi
  yetkisini genişletip genişletmediği. `AYAR`/`TERCİH` bir yetki ayarına **yazarsa**
  (okumak değil), ve `MCPSUNUCU` ile `YAPAYZEKAMODELİ` hangi argümanla olursa
  olsun. Ret, ayarı ve kimin değiştirebileceğini **adıyla** söylüyor: "yetkiniz
  yok" bir ajanı dolaştırır, bu deneme bitirir.
- Üç kapıda birden denetleniyor: okuma kapısında (`run_read_only`), öneri
  kaydedilirken (bir genişletme bir önerinin **adımı** olarak da kaçırılamaz —
  kartta çizim işini okuyan kişi "ve bu arada onay politikasını kapat"ı onaylamış
  olmaz) ve protokol katmanında, ki istemci cevabı **şimdi** alsın.
- **Yeni bir yetki ayarı işaretsiz eklenemez**: `core.ai.` ve `core.mcp.`
  içindeki her ayar, testte gerekçesiyle tercih olarak adlandırılmadıkça yetki
  sayılıyor. Sekizinci ayar yazıldığı gün test kırılıyor ve yazan kişi hangi
  cinsten olduğuna karar veriyor.
- Okumak genişletme değil: bir ajan kendi politikasını okuyabiliyor. Okuyamasaydı
  kendi davranışını açıklayamazdı — denetim kaydının amacının tam tersi.

### Eklendi — kataloğu aramak ve bir işin sırasını bilmek (TODOS M-09)

- **`ARAÇARA`** (`core.tool_search`): ajan araç kataloğunda ad ve özete göre arar,
  Türkçe katlamayla (`olcek` → `ÖLÇEKLE`). **Arama hiçbir aracı gizlemez** — bir
  araç yüzeyinde aramanın tek gerçek tehlikesi budur: tam görünen ama süzülmüş bir
  liste, ajanı görmediği araçların var olmadığı sonucuna götürür. Her cevap üç sayı
  taşıyor (kaç eşleşti, kaçı gösterildi, katalogda kaç var) ve tam listenin
  `tools/list` olduğunu söylüyor. Hiç eşleşme yoksa bunu **sözle** söylüyor; boş bir
  dizi cevap gibi okunur. `sinir` yalnız gösterileni kesiyor, sayımı değil.
- **`İŞŞABLONU`** (`core.job_template`): atlas, kadastro kontrolü ve parsel raporu
  işlerinin komut satırları, **sırasıyla**. Bir atlas altı komuttur ve sıra tahmin
  edilemeyen kısımdır — denetlemeyi basmadan **önce** yapmak ile sonra yapmak
  arasındaki fark, kopuk bir bağı ekranda görmek ile imzalanmış bir PDF'de görmek
  arasındaki farktır.
- **Hiçbir şablon kendini çalıştırmaz.** Satırları veriyor; satırlar olağan araç
  yüzeyinden gidiyor ve yazan her adım yine önizlemeli bir öneri oluyor (5.7).
  Doldurulmamış bir yer tutucu **olduğu gibi kalıyor**: boşaltmak `ad=""` gibi
  bitmiş görünen ama bitmemiş bir satır üretirdi.
- Şablonlar **veri**: `data/catalogs/ai/is-sablonlari.json` + şeması. Daha iyi bir
  sıra bir veri yayımı, yeniden derleme değil (3.5). Her şablonun paketten ayrı
  **kendi sürümü** var: bir işi güncellemek, diğerlerinin değiştiğini söylememeli.
- **Her adım canlı komut kütüğüne karşı sınanıyor** (`test_ai_tools.cpp`): şablon
  örnek değerleriyle çözülüyor, satır ayrıştırılıyor, komut çözülüyor ve her
  `anahtar=` o komutun tanımlı parametresi mi diye bakılıyor. Bu test yazıldığı anda
  yazdığım üç hatayı buldu — `ÖZNİTELİKŞEMASI` argüman almıyor ve tablo öğesinin
  katmanı `metin=` ile veriliyor. Satırları çalışmayan bir şablon, hiç şablon
  olmamasından kötüdür.
- `command::read_catalog_text`: `/src/ai` sans-IO olduğu için (ai.md P10) baytlar
  katalog okumanın zaten yapıldığı yerde okunuyor, şeklin bilindiği yerde ayrıştırılıyor.
- **`prompts` ilan edilmiyor ve bu bilinçli**: `prompts/get` bir modele verilecek
  mesajlar döndürür; bir iş şablonu ise kişinin uyguladığı bir öneriye dönüşecek
  komut satırlarıdır. Olmayan bir kabiliyeti ilan etmek, sunmamaktan kötüdür.

### Eklendi — bağlantıyı yönetmek: kim konuştu, kimi durdurmalı (TODOS M-08)

- **İstemci defteri** (`ai::ClientLedger`, AGPL): konuşmuş her ajan için ad, çağrı,
  ret, öneri sayısı, son yöntem, **son hata** ve son görülme zamanı. Bu bir oturum
  listesi değildir ve olamaz — `2026-07-28` durumsuzdur, yani "şu anda bağlı olan"
  diye okunabilecek bir soket tablosu yoktur. Dürüst cevap "kim konuştu ve ne zaman".
- **Tek bir istemcinin yetkisi kaldırılabiliyor** (`MCPSUNUCU islem=iptal ad=…`,
  ve Ayarlar'daki satır düğmesi). Belirteci yenilemek **kör bir araçtır**: o sırada
  birlikte çalıştığınız ajanı da kapatır. Bu keskin olanı — yalnız adı verilen
  istemci durur, belirteç yerinde kalır, karar `islem=izin` ile geri alınır.
  Durdurulan istemci `403` ve `-32001` alır ve **sebebini okur**: belirteci hâlâ
  geçerli olduğu için, yalnız "yasak" diyen bir cevap onu sonsuza kadar yeniden
  denemeye iterdi.
- **`MCPSUNUCU islem=sina`**: motorun kendi kendini yoklaması değil — gerçek bir
  soket, ayar sayfasındaki gerçek adres, gerçek bir `server/discover`. Portun bağlı
  olduğunu, belirtecin doğru olduğunu ve o portta başka bir program olmadığını
  sınar. `401`, cevapsızlık ve tanınmayan cevap ayrı ayrı anlatılıyor.
- **`MCPSUNUCU islem=istemciler`** listeyi komut satırına da veriyor: tablo bir
  ayrıcalık değil (Article 1.2).
- `HttpRequestView::received_at`: motor **sans-IO** olduğu için saati yok, "son
  görülme" ancak taşımanın söylediği olabilir.
- Ayarlar ▸ MCP Sunucusu sayfası: **İSTEMCİLER** tablosu, iki satır düğmesi,
  **Bağlantıyı sına** ve her istemcinin aynı kapsamda çalıştığını yazan bir satır.
- Ekrana bakarak düzeltilen iki kusur: boş tablo bir sözle değiştirildi (boş bir
  ızgara "bozuk" diye okunuyordu) ve sütun başlıkları artık kırpılmıyor
  ("İstem…", "Çağ…", "Son görül…"). Tablo satır sayısıyla büyüyor, iki ile altı
  satır arasında.
- **`McpService::clientsChanged`**: dinleyicinin durumu ile istemci listesi farklı
  hızlarda değişiyor. Bu sinyal olmadan açık bir ayar sayfası, sayfa kurulduğu
  andaki hâli gösteriyordu — taze bir başlatmada hiç dolmayan boş bir tablo.
- Belirteç yenilendiğinde defter sıfırlanıyor: her ad eski belirtecin parmak izini
  taşıyor, yenilemeden sonra hiçbiri bir daha sunulamaz.
- Defter sınırlı ama **yetkisizliği unutmuyor**: taşma hâlinde en eski **yetkili**
  kayıt düşüyor, böylece ad uydurarak taşırmak bir kararı geri almanın yolu olmuyor.
- `KENTOS_MCP_PROBE` üç madde daha sınıyor: defterin gerçek sokette dolduğunu,
  `403`'ün yalnız adı verilen istemciye geldiğini ve belirtecin yerinde kaldığını.

### Güvenlik — her istemci kendi alanında çalışır (TODOS M-07)

- **Tutamak defteri artık istemci başına.** `ai::HandleScopes`: bir ada bir defter.
  Bugüne kadar tek bir defter vardı ve `HandleStore::next_id` bir **sayaç** olduğu
  için, bir istemcinin ikinci tutamağının kimliği bir başkasının ikinci tutamağının
  kimliğiyle aynıydı — yani bir ajan, hiç okumadığı bir geometrinin tutamağını
  tahmin etmeden eline geçirebiliyordu. Deftere yazılmış olan yorum ("her oturumun
  kendi sayacı") artık doğru.
- **Bir önerinin sahibi var.** `PlanStore::owned_by`, `find_for`, `append_for`. Bir
  istemci yalnız kendi açtığı öneriyi okuyabilir, genişletebilir ve akışını
  kapatarak geri çektirebilir. Önemli olan madde **genişletme**: öneri kimliği
  istemcinin cevabında geçtiği için gizli değildir, ve adım ekleyebilen ikinci bir
  istemci, mühendisin kartta **okumadığı** bir satırı onun onayıyla uygulatırdı;
  denetim kaydı da o adım için yanlış istemciyi gösterirdi (ai.md R6, R8).
- **"Sizin değil" ile "yok" aynı cevabı alır.** Ayrı bir ret, bir istemciye bir
  başkasının öneri kimliğini doğrulayan bir kâhin olurdu.
- **Bilgisayar başındaki kişi kapsamsızdır.** Boş istemci adı operatördür: öneri
  panosu kimin açtığına bakmadan hepsini listeler, çünkü uygulayan odur.
- `ai::Dispatcher`'ın dört kapısı artık **kimin sorduğunu** alıyor
  (`run_read_only`, `plan_state`, `withdraw`, `handles`); `StreamPlan` istemci adını
  taşıyor, böylece kapanan bir akış yalnız kendi önerisini geri çekiyor.
- `AiService::propose` **adım ekleme sözleşmesini gerçekten uyguluyor**: `mcp.cpp`
  bunu anlatıyordu ama uygulama her çağrıda yeni öneri açıyordu.
- Sohbet de bir istemcidir: tutamakları ve önerileri `sohbet · <profil>` adıyla,
  tek bir yerden (`ChatPanel::requesterLabel`) geliyor.

### Eklendi — kâğıt ve zemin sayıları şemada ayrıldı, yerleşim ajana açıldı (TODOS A-03, A-02)

- **`Param::unit`**: bir sayının neyle ölçüldüğü. Yapısal ayrım zaten vardı — bir
  `Point` parametresi yalnız **tutamak** kabul ediyor, bir `Integer` ise çağıranın
  yazabileceği bir sayı — eksik olan, şemanın hangisinin hangisi olduğunu
  **söylememesiydi**. "integer 0..10000" diyen bir şema ajanı tahmine bırakır, ve
  tahmin eden ajan kâğıt alanına zemin koordinatı yazar.
- Yerleşim komutlarının `x`, `y`, `genislik`, `yukseklik`, `yazi`, `kenar` alanları
  `[kâğıt mm]`, `pencere` ise `[ZEMİN koordinatı — kâğıt değil]` diyor. Birim
  açıklamanın başında, çünkü yanlış yapılan şey o.
- **`core.layout`, `core.layout_item`, `core.layout_template` ve `core.print`
  ajana açıldı.** Ajan kapsamı **%74'ten %78'e** çıktı (93 komutun 73'ü).


### Eklendi — çıktı yerleşimi öğelerinin kalıcı kimliği (TODOS L-01)

- `LayoutItemKey` ve `LayoutPageKey`. Yerleşimin **kendi** sayacından basılıyor,
  çünkü bir yerleşim şablon dosyasında belgesiz yolculuk ediyor.
- **Oturum kimliği, saklanan kimlik değil.** Dosya adlarla konuşuyor — okunabilir
  ve diff'lenebilir kalsın diye — anahtarlar okunurken basılıyor. Evin kendi
  cevabı bu: `Document::content_hash` zaten "key assignment is an allocation
  detail" diyor. Anahtarlar ne katlamada ne eşitlikte; ikisi de elle yazıldı ki
  dışarıda kalmaları bilerek olsun.
- **`ÇIKTIÖĞE islem=ad`**: bir öğeyi yeniden adlandırmak ona bağlı olanları
  koparmıyor, ve dosyaya hedefin **yeni** adı yazılıyor. Kullanımdaki bir ada
  yeniden adlandırma reddediliyor: bir sözcüğe iki öğenin cevap vermesi, yanlış
  kutunun düzenlenmesidir.

### Düzeltildi — harita bağı parmak izine girmiyordu

- `linked_map` katlamada yoktu. Bağ hangi haritanın ölçeğinin basılacağını
  belirliyor, yani içeriktir; farklı sayı basan iki çizim aynı `content_hash`'i
  taşıyamaz.


### Düzeltildi — çıktı yerleşiminin haritası PDF'e fotoğraf olarak gidiyordu (TODOS L-12)

- Harita bir `QImage`'a çizilip sayfaya yapıştırılıyordu. PDF'te **tek bir
  fotoğraftı**: 1:1000 bir parsel sınırı piksel olarak varıyordu — ölçülemez,
  seçilemez, çözünürlüğe bağlı — imzalanıp teslim edilen bir belgede.
- Koddaki gerekçe gerçekti: render boru hattı hedefinin tamamına sahip, arka
  planı dolduruyor ve hiçbir şeyi kırpmıyor; doğrudan sayfaya çizmek çerçevenin
  altındakini siler ve dışına taşardı. **İkisi de yanıtlandı, etrafından
  dolaşılmadı**: `FrameContext::target_is_painter` ile arka uç çağıranın
  boyacısına çiziyor ve durumunu bulduğu gibi geri veriyor; kırpma çerçeve, arka
  plan sıfır alfa ("orada olanı bırak").
- **Kanıt dosyadan okundu**: aynı yerleşimin PDF'i **435.594 bayttan 8.402 bayta**
  indi, `/Subtype /Image` sayısı **sıfır**, 34 vektör çizgi operatörü var. Basılan
  görüntü bire bir aynı. `layout-designer` probe'u rasterı her koşumda denetliyor.
- Ölçü ayrıca sınandı: 1:1000'de 100 mm'lik bir çerçeve tam olarak 100 000 mm
  zemin gösteriyor. `Mm` sabit noktalı olduğu için bu aritmetik kesin, ölçülen
  değil.


### Düzeltildi — dosyadan okunan çıktı yerleşimi araç çubuğunun listesinde çıkmıyordu

- Bir yerleşim kaydedilip proje tekrar açıldığında `Dosya ▸ Çıktı Yerleşimleri`
  altında görünüyor, **araç çubuğundaki yazdırma düğmesinin yanındaki listede
  görünmüyordu**.
- Sebep: o liste üç yerden yeniden kuruluyordu — açılışta bir kez, bir yazdırma
  **profili** değişince, ve yerleşim kuran tek menü girdisinden. Proje açmak
  bunların hiçbiri değil. Komut satırından, betikten, şablondan ve MCP'den kurulan
  yerleşimler de aynı sebeple görünmüyordu.
- Artık `aboutToShow`'a bağlı, yani her açılışta belgeye soruyor — `Dosya ▸ Çıktı
  Yerleşimleri` menüsünün baştan beri yaptığı şey, ve o menünün doğru olmasının
  sebebi. Çağrı yerlerini kovalamak, biri yeni bir yerleşim kurma yolu eklediğinde
  yine yanlış olacak bir listedir.
- `layout-designer` probe'u üç yolu da yürüyor: menüden kurulan, komut satırından
  kurulan ve **kaydedilip tekrar açılan**. Düzeltme kapatılınca üçü de kırılıyor.


### Eklendi — öğeler kendi haritasına bağlanıyor (TODOS L-05)

- Bir ölçek çubuğu bir haritanın ölçeğini, bir `<olcek>` yer tutucusu onun
  paydasını söyler. Tek haritalı sayfada soru yok; **iki harita çerçeveli sayfada
  vardır**, ve program bunu "ilk bulduğunu al" diye cevaplıyordu: 1:1000 ve 1:5000
  iki çerçeve varken bir haritanın sayısı öbürünün altına basılıyordu.
- `core::LayoutItem::linked_map` ve `ÇIKTIÖĞE islem=ayarla harita=<ad>`.
  Verilmezse ilk harita — tek haritalı sayfanın doğru cevabı ve bu alandan önce
  yazılmış her yerleşimin söylediği şey. `harita=ilk` bağı kaldırır.
- **Kopuk bağ sessizce ilk haritaya dönmüyor.** `Layout::map_for` null veriyor,
  `link_is_broken` doğru diyor ve `paint_layout_page` bunu `trouble` listesine
  yazıyor — imzalanan bir belgede başka bir haritanın ölçeğini sessizce yazan bir
  ölçek çubuğu yanlış bir sayıdır. Bu liste preflight raporunun tohumu (L-15).
- Dosya biçimi: dört bayt kaydın **`reserved` dizisinden** alındı, yani
  `LayoutItemRecord` hâlâ 160 bayt ve bu alandan önce yazılmış bir dosya sıfırlarla
  okunup her öğeyi ilk haritaya bağlıyor — yazıldığında ne demek istediyse o
  (io.md R10). Sürüm artışı yok.


### Düzeltildi — harita çerçevesinin katman listesi render'a hiç ulaşmıyordu

- `core::LayoutItem::layers` modelde baştan beri duruyordu ve `paint_map` onu
  hiçbir render seçeneğine aktarmıyordu: "bu harita şu katmanları çizer",
  kullanıcının ayarlayabildiği ve sayfanın yok saydığı bir alandı.
- `render::SceneOptions::layer_allowed` eklendi — **katman başına bir bayt**,
  ad listesi değil: sahne bunu nesne başına bir kez okuyor ve orada bir ad
  karşılaştırması kare bütçesinin içine girerdi (§10.1).
- Maske **daraltıyor, genişletmiyor**: belgenin gizlediği bir katman ne olursa
  olsun gizli kalıyor, çünkü `EntityTable::visible` zaten konuşmuştur (model.md R7).
  Boş olması "görünür bütün katmanlar" demek — tuvalin cevabı ve ilk harita
  çerçevesinin istediği.


### Eklendi — çok sayfalı çıktı yerleşimi (TODOS L-02)

- **Tasarımcıda bir SAYFA bölümü**: hangi sayfada olunduğunu söyleyen bir alan,
  "N sayfadan biri · 420×297 mm" yardım satırı ve üç düğme (ekle, çoğalt, sil).
  Düğmeler kendi mantığını taşımıyor; bir elin yazacağı komut satırını yazıyorlar.
- Öğe listesi **yalnız gösterilen sayfanın** öğelerini taşıyor. Her sayfanın
  öğesini gösteren bir liste, önündeki kâğıtta olmayan bir şeyi seçtirirdi.
- **`ÇIKTIÖĞE islem=tasi sayfa=<n>`** bir öğeyi sayfalar arasında taşıyor. Bu
  olmadan iki sayfalı bir yerleşim kurulabiliyor ama aralarında hiçbir şey
  taşınamıyordu; bir anteti taşımanın tek yolu silip yenisini yapmak olurdu, ki o
  aynı antet değildir.

- **Dört yeni fiil**: `ÇIKTIYERLEŞİMİ islem=sayfaekle | sayfasil | sayfacogalt |
  sayfatasi`, ve `islem=sayfa` artık `sayfa=<n>` alıyor. Bir yerleşim bir A4 dikey
  kapakla bir A3 yatay harita sayfasını aynı belgede tutabiliyor. Sayfa numaraları
  **1'den** başlıyor: sayfanın üstünde yazan ve insanın söylediği o.
- Her fiil `item_pages`'i onarıyor. **Silinen sayfa öğelerini de götürüyor** ve kaç
  öğe gittiğini söylüyor; **çoğaltma öğeleri taze adlarla kopyalıyor**; **taşıma
  öğeleri sayfasıyla birlikte götürüyor**. Son sayfa silinemiyor.

### Düzeltildi — karma sayfalı PDF her sayfayı ilk sayfanın boyunda yazıyordu

- **Ve tasarımcıda ikinci sayfada sürükleme kayıyordu.** `pageRect()` sayfa
  kutusunu aktif sayfadan ölçüyordu ama `deviceFrom`, `paperFrom` ve `dragged`
  `pages.front()` ile ölçekliyordu: ikinci sayfası farklı boyda olan bir
  yerleşimde her kutu olduğu yerden başka bir yere çiziliyor ve sürükleniyordu.
  Dördü de tek bir `activePage()`'e bağlandı.

- `print_service` cihaz sayfa boyutunu bir kez `pages.front()`'tan alıyor ve bir
  daha atamıyordu. Karma bir A4/A3 yerleşimin **her sayfası A4 yazılıyordu**:
  ikinci sayfanın içeriği A4 kutusuna A3 ölçüsünde çizilip kâğıttan taşıyordu.
  Boyut artık her `newPage()`'ten önce, o sayfanın kendi ölçüsünden atanıyor.
- Çıktı mesajı karma sayfalı bir yerleşim için "karma sayfa boyu" diyor; olmayan
  tek bir ölçüyü rapor etmiyor.
- `print-pdf` probe'u bunu **dosyadan okuyarak** doğruluyor: PDF'te iki MediaBox
  var, `595x842` ve `1191x842`.


### Eklendi — etki sözleşmesi (TODOS C-02)

- **`command::Effect`**: yedi bit — `sorgu`, `gorunum`, `belge_duzenleme`,
  `dosya_okuma`, `dosya_yazma`, `dis_yazma`, `ayar_degisikligi`. `Flags` "hangi
  istemci ulaşabilir" sorusunu cevaplar; bu, bir politikanın sormak zorunda olduğu
  farklı soruyu: **geriye ne değişmiş kalıyor, ve nerede.** İkisi tek bir boolean'dı
  (`!NoEffect`) ve o boolean bir yerleşimi listelemekle basmayı ayırt edemiyordu.
- **Karma fiiller argümandan türetiliyor.** `ÇIKTIYERLEŞİMİ islem=listele` yalnız
  okur; `islem=sil` belgeyi düzenler. Beş sözcüğü komutun en kötü hâline çökertmek,
  bir okumaya onay sordurmak olurdu — `.claude/ai.md` R3 bunu yasaklıyor. Fiil
  verilmemişse cevap yine en kötü hâl, çünkü etkileşimli çalıştırma fiili
  doğrulamadan sonra sorar.
- **`core.save`, `core.saveas`, `core.export` `ReadOnly` taşır ve dosyanın üstüne
  yazar.** O bayrak "işlem yolunu atlar" demek, "zararsız" değil; artık üçü de
  `dosya_yazma` diyor. `core.print` `dis_yazma` diyor: bir yazıcı geri alma yığını
  değildir.
- Bildirilmemiş bir komutun etkisi kategorisinden okunuyor. **İlk geri düşüş
  yanlıştı**: `ReadOnly && !Dosya → ayar_degisikligi` diyordu ve `core.zoom` ile
  `core.pan`'i "ayar değiştirir" yapıyordu. Envanterin dağılımına bakınca görüldü.


### Düzeltildi — çıktı yerleşimi sayfasında üç görünür kusur

Basılan sayfanın kendisine bakarak bulundu; üçü de kâğıda çıkan kusurlardı.

- **Tablo ve lejant saydam geliyordu.** Haritanın ızgara çizgileri ve parsel
  sınırları tablonun satırlarının içinden geçiyordu. İkisi de tasarımı gereği
  haritanın üstünde durur; basılı bir öznitelik tablosu opaktır, lejant kutusu da.
  Artık `core::default_item` ikisine de arka plan ve çerçeve veriyor.
- **İki yerde iki ayrı varsayılan vardı**: `default_layout` sayfanın dört öğesini
  kendi seçimleriyle kuruyordu, `ÇIKTIÖĞE islem=ekle` kendi seçimleriyle. Elle
  eklenen tablonun saydam çıkmasının sebebi buydu. Tek cevap: `core::default_item`.
- **Ölçek çubuğunun birimi son taksimat etiketinin üstüne basılıyordu.** Taksimat
  çubuğun ucunda ORTALANMIŞ çiziliyor, yani "80"in yarısı ucun sağına taşıyor;
  birim ise ucun yalnız dört piksel sağından başlıyordu. Sonuç "80m 1:550"nin
  kendi üstüne binmesiydi.
- **Izgara koordinat etiketleri hiç çizilmiyordu.** İki etiket geçişi de
  `grid != Cross` ile korunuyordu, oysa yeni bir haritanın varsayılanı tam olarak
  `Cross` + `Outside` — yani varsayılan sayfa, çizilmeyeceği kararlaştırılmış
  sayılar istiyordu. Etiket artık ızgaranın stiline bağlı değil: kenarlarda
  koordinatı olan bir artı ızgarası, bir kadastro paftasının tam olarak kendisidir.


### Eklendi — capability envanteri ve ajan kapsamı kapısı (TODOS C-01, komut tarafı)

- **`kentos_envanter`**: canlı registry'lerden alınan makine okunur envanter. Her
  komut için kimlik, adlar, kategori, geri alma politikası, tam parametre şeması
  (tür, arity, seçenek listesi, sayısal aralık, emekli ad) ve altı bayrağın her biri
  ayrı ayrı. Kimliğe göre sıralı, yani iki makine aynı baytları üretir.
- **Ağacı grep'leyen sayım yanlıştı.** `return CommandSpec{...}` taraması **81**
  komut görüyordu; program **93** kaydediyor. Aradaki fark, bir modülün döngüyle ya
  da yardımcıyla kaydettiği komutlar — ve o tarama bir parametrenin arity'sini,
  sözcük listesini ya da aralığını zaten göremez.
- **Gerçek ajan kapsamı: 93'ten 69'u açık (%74), 24'ü kapalı.**
  `tests/support/ai-kapsam.json` yirmi dördünün her birinin gerekçesini ve onu
  açacak iş paketini taşıyor. Dördü bilerek kalıcı: `core.select` (değişen bir vurgu
  sonraki SİL'in neyi sildiğini değiştirir), `core.ai_provider` ve `core.mcp` (model
  kendi erişim yolunu açamaz), `core.script` (diskteki herhangi bir dosyayı komut
  dizisi olarak çalıştırmak, doğrulanmış araç yüzeyinin etrafından dolaşmaktır).
- **`scripts/ci-gate-envanter.sh`**: gerekçesiz kapalı bir komut, ölmüş bir satır ve
  açıldığı hâlde duran bir satır kapıyı kırıyor. Üretici derlenmemişse atlamıyor,
  kırıyor — ölçemeyen bir kapı sessizce geçemez.


### Düzeltildi — adlandırmanın uyumluluk sözleşmesi (TODOS BR-01)

- **Eski ad okunur, yeni ad yazılır.** `pafta=` → `yerlesim=` yeniden adlandırması
  diskteki veriyi geçersiz kılamaz: bir komut çağrısı veridir (Article 1.4) ve bir
  argümanın adı günlük satırının içine yazılır. `Param::was` bir parametrenin emekli
  adını taşıyor; `bind_tokens` komut satırında, `Bus::dispatch` betik ve günlük
  yolunda onu güncel adın üstüne taşıyor. Programdan **tek yazım** çıkıyor, yani
  tekrarın tekrarı aynı baytları veriyor.
- **İki yazımı birlikte vermek reddediliyor.** Skaler bir argümanda sessizce
  sonuncuyu tutmak, `command.md` P15'in "bir argümanı yere düşürme" yasağıdır.
- **`<pafta>` yer tutucusu çözülmeye devam ediyor.** Yeniden adlandırmadan önce bir
  antede yazılmış yazı, programın bir sözcük hakkında fikir değiştirmesiyle bozulmaz.
- Şablon dosyaları etkilenmedi: `layout_to_json` alan adları yazıyor, parametre adı değil.


### Düzeltildi — clang-tidy kapısı gerçekten çalışıyor

- **`make check` clang-tidy'yi atlıyordu.** Makefile `command -v clang-tidy` diye
  soruyordu; Homebrew'un LLVM'i keg-only olduğu için cevap "kurulu değil" oluyor ve
  kapı SKIPPED yazıp geçiyordu — oysa araç iki dizin ötede duruyordu. Soru artık
  `scripts/tidy.sh --var`'a soruluyor; aracı arayan kod zaten oradaydı (Article 10:
  Makefile'da yapı mantığı olmaz). **Atlayan bir kapı hiç çalışmamış kapıdır** (6.2).
- Kapı açılınca ağaçta **3719 bulgu** çıktı, hepsi temizlendi. İçlerinden gerçek olanlar:
  - **Üç yerde işaretçi sıralanıyordu** (`command/registry.cpp`, `core/settings.cpp`,
    `processing/registry.cpp`). Karşılaştırıcılar anahtara bakıyordu, ama adres kabı
    sıralamak bu programın bit-aynı çıktı iddiasının (Article 2.5, §7.3) yanında
    durulacak bir şekil değil. Üçü de **indeks sıralamaya** çevrildi.
  - `app/main.cpp` sınama sürüşünde `check()` rapor ediyor ama dönmüyordu; bir sonraki
    satır az önce şikâyet ettiği null'ı dereference ediyordu.
  - `app/layout_designer.cpp` kullanıcıya **yanlış yer tutucuyu** söylüyordu.
  - `Banner::addAction` `QWidget::addAction`'ı gizliyordu → `addButton`.
    `GridDelegate::setTheme` sanal olmayan tabanı gizliyordu → taban `virtual` oldu.
  - `io/dxf_reader.cpp`'de `PendingInsert` her vektör büyümesinde koca bir DXF
    varlığını kopyalıyordu; artık işaretçinin arkasında.
  - `kentos_docgen`'in `main`'i istisna sızdırıyordu: yarım yazılmış bir referans
    `ci-gate-docs.sh`'in karşılaştıracağı şeydir. Artık sınırda yakalanıyor.
  - `core::LayoutItem` nesne başına **39 bayt dolgu** harcıyordu (en iyisi 7).
- `.clang-tidy`: `bugprone-signed-bitwise` pozitif tamsayı sabitlerini yok sayıyor;
  kontrol bit işlemindeki **işaretli değişkeni** yakalamak için var, `1u << 0` biçimindeki
  bayrak sözcüklerini değil — ağaçta 3571 kez. İşaretli bir `int` işlenen hâlâ yapıyı kırar.
- `scripts/tidy.sh` artık bulguyu **konumuna göre** süzüyor: `/src` dışındaki bir bulgu
  üzerinde çalışabileceğimiz bir bulgu değil. Betiğin kendi yorumu bunu zaten söylüyordu;
  clang-tidy'nin `HeaderFilterRegex`'i çözümleyici bulgularına ulaşmadığı için (libpqxx'in
  `result_iter`'ı canlı örnek) politika koda taşındı.


### Eklendi — Çıktı yerleşimi (sayfa sistemi)

- **Bu sayfaya "pafta" DENMEZ, "çıktı yerleşimi" denir.** Bu ülkede pafta, kadastronun
  böldüğü harita sayfasıdır — `29-30-K` gibi bir adı olan, paylaşılan bir paftalama
  sisteminin karesi. Bastığınız kâğıt ise bambaşka bir şeydir ve ikisini aynı sözcükle
  anmak, bir haritacıya iki farklı işi aynı adla söylemektir. Komut adları
  `ÇIKTIYERLEŞİMİ`, `ÇIKTIÖĞE` ve `ÇIKTIŞABLON`; parametre `yerlesim=`; yer tutucu
  `<yerlesim>`. **Komut kimlikleri (`core.layout`, `core.layout_item`,
  `core.layout_template`) değişmedi**, çünkü onlar kaydedilmiş belgelere ve günlüğe
  yazılır; bir yeniden adlandırma eski bir dosyayı okunamaz hâle getiremez.
- **Çıktı yerleşimi çizimin içindedir** (`core/layout.hpp`): kâğıt boyu, yönü, kenar boşluğu ve
  üzerine yerleştirilmiş öğeler. Yazdırma profilinden farkı budur — profil bu
  bilgisayarın ayarıdır, yerleşim teslim edilen işin parçasıdır: dosyayla gider, çizimin
  parmak izine girer ve her düzenlemesi tek `Ctrl+Z` ile geri alınır. Konumlar **kâğıt
  mikrometresi** (`int32`, model.md R20) ve sayfanın sol **ÜST** köşesinden ölçülür;
  zemin/kâğıt çevrimi yalnız harita öğesinin içinde yapılır.
- **Sekiz öğe türü**: harita çerçevesi (kendi kapsamı, ölçeği ve **koordinat
  ızgarası** ile), metin, ölçek çubuğu, kuzey oku, lejant, resim, şekil, tablo. Metin
  öğesinin `<yerlesim>`, `<olcek>`, `<tarih>`, `<crs>`, `<proje>`, `<kagit>` yer
  tutucuları **çizim anında** çözülür ve asla çözülmüş hâlde saklanmaz — ölçek
  değiştiğinde yeniden bastığınız yerleşim yeni ölçeği yazar.
- **İki yeni komut**: **`ÇIKTIYERLEŞİMİ`** (`islem=listele|ekle|sil|ad|sayfa`) sayfaları,
  **`ÇIKTIÖĞE`** (`islem=listele|ekle|sil|tasi|ayarla`) üzerindeki öğeleri yönetir.
  Yeni bir yerleşim boş değildir: harita, başlık, ölçek çubuğu ve kuzey oku ile gelir.
  Kâğıdı değiştirmek öğeleri **yeniden ölçeklemez** — üstten 20 mm'deki başlık A3'te
  de üstten 20 mm'dedir. Her çağrı tek işlem, tek geri alma adımıdır; liste bütün
  hâlinde geri yüklenir, çünkü bir öğe silindiğinde dizinler kayar.
- **Bildirilen ölçek kazanır**: `olcek=1000` verildiğinde pencere çerçevenin kâğıt
  boyundan hesaplanır, kapsamın merkezine oturur. Bu yüzden 1:1000 bir yerleşimin
  kâğıdını büyütmek daha ÇOK zemin gösterir; bir harita ölçeğinin anlamı budur.
  `olcek=0` ise pencere neredeyse odur ve ölçek ondan çıkar. Tek fonksiyon
  (`core::map_scale` / `core::map_window`), dolayısıyla çizilen harita, ölçülen
  ölçek çubuğu ve basılan `<olcek>` asla birbirinden ayrılamaz.
- **Yerleşimi çizen tek boyacı** (`app/layout_render.hpp`): tasarımcının sayfası, baskı
  önizlemesi ve dışa aktarılan PDF aynı fonksiyondan geçer. Harita çerçevesi tuvalin
  kendi render boru hattından (`render::build_scene`) çizilir, yani yerleşimdeki çizgi
  kalınlıkları ekrandakiyle aynı kuralla hesaplanır. Izgara aralığı verilmezse 1-2-5
  basamaklarından seçilir: 37 metrelik aralık kimsenin koordinat okuyamayacağı bir
  ızgaradır.
- **Dosya biçimi**: dört yeni **isteğe bağlı** blok (`kBlkLayouts`, `…Pages`,
  `…Items`, `…Names`). Yerleşimi olmayan bir çizim tek bayt ödemez ve dosyası
  yerleşimlerden önceki hâliyle bayt bayt aynıdır (io.md R10), dolayısıyla sürüm
  yükseltmesi değildir. Bilinmeyen bir öğe türü **reddedilir**, sessizce metne
  çevrilmez: ileri sürümden gelen bir yerleşimi kaydetmek veri kaybı olurdu.
- ISO 216 kâğıt tablosu `/src/io`'dan **`/src/core`'a taşındı**: A4'ün 210×297 olması
  bir dosya biçimi değil geometrik olgudur, ve hem yazdırma profillerinin hem yerleşim
  komutlarının okuduğu tek tablo olması gerekir (CLAUDE.md 5.10).

- **Çıktı yerleşimi tasarımcısı** (`app/layout_designer.hpp`): solda çizim sırasına göre öğe
  listesi, ortada sayfa, sağda seçili öğenin özellikleri, altta **PDF'e aktar** ve
  **Yazdır**. Öğeye tıklamak seçer, sürüklemek taşır, köşe tutamağı boyutlandırır, ok
  tuşları birer milimetre (Shift ile on) kaydırır. **Her jest bırakıldığında tek bir
  `ÇIKTIÖĞE` satırı yazar** — sürükleme boyunca değil: sayfanın bir ucundan öbürüne
  taşınan kutu tek `Ctrl+Z` ile döner, dört yüz adımda değil. Kendi düzenleme yolu
  yoktur; pencerenin yaptığı her şey komut günlüğünde durur ve bir betiğin
  yazabileceği satırlardır (CLAUDE.md 1.1, 1.2).
- **İstenen akış tamam**: araç çubuğundaki yazdırma okunun listesinde profillerin
  altında **çizimin yerleşimleri** durur. Bir yerleşim seçilince tuval o yerleşimin **harita
  çerçevesinin en-boy oranında** bir seçme çerçevesi açar — kâğıdın değil, haritanın
  oranında, çünkü çerçevelenen şey haritanın göstereceği alandır — ve alan bırakılınca
  **tasarımcı açılır, harita o alana bakıyor olur**. Aynı listede **Yeni çıktı yerleşimi…** var.
- **`YAZDIR yerlesim=<ad>`**: yerleşim kendi kâğıdını, kenarını, sayfalarını ve harita
  penceresini taşıdığı için `pencere`, `merkez`, `olcek` ve `profil` ile birlikte
  verilmez — birlikte verilirse **reddedilir**, sessizce biri kazanmaz. Çok sayfalı
  yerleşim çok sayfalı PDF olur.
- Yeni bütünleşme sınaması **`layout-designer`**: tasarımcıyı bir el gibi sürer —
  öğe seç, sürükle, metin yaz, ızgara ve ölçek ayarla, öğe ekle — sonra tek `GERİAL`in
  son jesti geri aldığını ve yerleşimin PDF olarak yazıldığını doğrular.

- **Öznitelik tablosu doluyor**: başlıklar katmanın şemasından, değerler çizimden.
  Kutuya kaç satır sığıyorsa o kadarı yazılır ve **sığmayanlar sayılarak bildirilir**
  (`… 14 satır daha sığmadı`) — sessizce ilk on biri gösteren bir tablo, eksiksiz
  sanılarak dosyalanan bir tablodur.
### Düzeltildi — Anahtar okuması pencereyi donduruyordu

- **`SecretStore::read` GUI iş parçasından çıktı** (`app/secret_resolver.hpp`).
  `AiTransport::send` bir sohbet isteğinden hemen önce, daha tek bayt çıkmadan,
  platformun anahtar deposunu **GUI iş parçasında** sorguluyordu. Üç platformun üçü de
  istediği kadar bekleyebilir: macOS'ta `SecItemCopyMatching` Security çerçevesine
  girip kullanıcı izin verene kadar bloke olur — klavyede kimse yoksa öylece durur ve
  başsız bir koşu `SecurityServer::ClientSession::decrypt` içinde **iki dakikadan
  fazla** ölçüldü; libsecret kilitli bir anahtarlığa D-Bus turu bekler. Pencere bu süre
  boyunca donuyordu, ki `.claude/ai.md` R18 uygulamanın kullanılabilir kalmasını şart
  koşar ve P8 UI iş parçasında bloke beklemeyi doğrudan yasaklar.
- Arama artık kendi iş parçasında yapılıyor ve çözülen anahtar **oturum boyunca
  bellekte** tutuluyor — dosyaya, `SettingSpec`'e, komut argümanına, günlüğe, denetim
  kaydına ya da hata mesajına değil (CLAUDE.md 5.21 bu listeyi sayar ve bu sınıf ona
  yeni bir yer eklemez). Bellek sayesinde sohbetin ikinci turu anahtar deposuna hiç
  dokunmuyor ve bir kez izin vermeyen kullanıcı her cümlede yeniden sorgulanmıyor.
- **İzin penceresi kişinin beklediği anda** çıkıyor: sohbet seçicisinden model
  seçildiğinde istenmiş bir penceredir, üç cümle sonra çıkanı kimse açıklayamaz.
  Kullanıcının az önce yazdığı anahtar ise işletim sistemine hiç sorulmadan hatırlanıyor.
- Aynı kusurun sınamadaki yüzü de kapatıldı: sohbet probe'u paneli anahtarsız yerel bir
  uca yönlendiriyor, yani bir geliştiricinin kendi makinesindeki gerçek bulut profiline
  ve gerçek anahtar deposuna hiçbir yoldan ulaşamıyor.

- **Ana menüye girdi**: **`Dosya ▸ Çıktı Yerleşimleri`** — QGIS'in `Project ▸ Layouts`'unun
  durduğu yer, ve aynı sebeple `Dosya` altında: yerleşim belgeye aittir, dosyayla gider ve
  imzalanan işin parçasıdır. Menü her açılışta çizimden yeniden kurulur, yani komut
  satırından eklenen bir yerleşim orada olur. Her yerleşimin kendi alt menüsü var:
  **Tasarımcıyı Aç**, **Tuvalden Alan Seç…**, **PDF'e Aktar…**.
- **Çıktı Yerleşimi Yöneticisi** (`Ctrl+Shift+P`, `app/layout_manager.hpp`): çizimdeki yerleşimleri
  kâğıdı, yönü ve öğe sayısıyla listeler; açar, yeniden adlandırır, siler ve
  **çoğaltır**. Çoğaltma tek bir "kopyala" fiili değildir — bir `ÇIKTIYERLEŞİMİ islem=ekle` ve
  her öğe için bir `ÇIKTIÖĞE`, hepsi tek toplu iş: günlükte gerçekte ne kurulduğu
  görünür ve tek `Ctrl+Z` geri alır.
- Duman testi (`windows-open`) artık Çıktı Yerleşimi Yöneticisi'ni ve tasarımcıyı da açıyor;
  `layout-designer` sınaması menüyü gezip girişlerin bağlı olduğunu doğruluyor.

- **Çıktı yerleşimi şablon kitaplığı** (**`ÇIKTIŞABLON`**, `app/layout_templates.hpp`): kurumun
  standart sayfası — antedi, lejant kutusu, ızgara ayarı — her işte kullanılır,
  dolayısıyla tek bir işin dosyasında duramaz. Şablonlar kullanıcı profilinde bir
  klasörde, **her biri kendi JSON dosyasında** durur; tek bir büyük dosya değil, çünkü
  bir şablon kendi başına bir belgedir: postalanır, sürüm denetimine konur, antet
  değişince yamalanır. `Dosya ▸ Çıktı Yerleşimleri ▸ Şablonlar` uygular ve kaydeder.
- **Şablon düzeni taşır, zemini taşımaz.** Uygulanan yerleşimin harita çerçevesi
  hedefsiz gelir: Trabzon'daki bir çizimin koordinatlarını Ankara'daki bir sayfaya
  taşımak, şablonun yerleşimi yanlış yere hedeflemesidir. `uygula` sıradan bir yerleşim
  düzenlemesidir ve tek `Ctrl+Z` ile kalkar; `kaydet` ve `sil` çizime dokunmaz.
- Şablon adındaki dosya adı olamayacak karakterler `_` ile değiştirilir, reddedilmez —
  kurum sayfasına `18. madde / askı` demek isteyen engellenmez, ama yazılan dosya
  klasörün dışına çıkamaz.

### Eklendi — Gömülü MCP sunucusu, yapay zeka sohbeti ve üretilmiş `llms.txt`

- **Yedi yeni komut** (`src/ai/src/commands/`). Beşi hiçbir şeyi değiştirmeyen okuma
  aracıdır (`ReadOnly | NoEffect`) ve bir ajan onları onay beklemeden çalıştırır:
  **`KATMANLAR`** (katmanlar, nesne sayıları, görünürlük, kilit, baskı, grup; aktif
  katman ve CRS), **`ÖZNİTELİKŞEMASI`** (sütunlar, tipleri, etiketleri, katalogları,
  zorunluluk), **`SORGULA`** (`katman` / `alan` / `deger` / `sinir`; sayı kırpılmaz,
  yalnız bildirim sınırlanır), **`SEÇİMBİLGİSİ`** (o anki seçim ve çizim sürümü),
  **`GÖRÜNÜMBİLGİSİ`** (görünen dikdörtgen, merkez, ölçek, ekran boyu, CRS; pencere
  yoksa açıkça söyler). Kalan ikisi arayüzün kendi düğmelerinin veri yoluna ulaşma
  yoludur: **`ÖNERİ`** (`islem=uygula|reddet|durum|listele`) ve **`MCPSUNUCU`**
  (`islem=baslat|durdur|durum|belirtec`, `port`). Her okuma aracı **iki kere** yanıt
  verir: insana bir satır Türkçe, istemciye yapılandırılmış veri.
  `SORGULA`'nın koşulu bir ifade değil **bildirilmiş parametrelerdir**, çünkü bu
  programda tek bir ayrıştırıcı vardır (CLAUDE.md 5.11).
- **MCP sunucusu** (`src/ai/src/mcp.cpp`, AGPL-3.0-or-later — bkz. `/NOTICE`): yalnız
  yerel döngü (`127.0.0.1`, `::1`), tek uç nokta, yalnız `POST`, varsayılan olarak
  **belirteç zorunlu**. Belirteç `Authorization: Bearer` başlığında ya da
  `/mcp/<belirteç>` yolunda kabul edilir; yol biçimi bilinçli bir yerel kolaylıktır ve
  belirteç hiçbir günlüğe girmez — kayda sekiz haneli bir parmak izi yazılır.
  Karşılaştırma sabit zamanlıdır. Belirteçsiz uç nokta yalnız ayar kasıtlı
  temizlendiğinde açılır ve durum çubuğu o zaman **KORUMASIZ** der. `Origin` her şeyden
  önce denetlenir (DNS yeniden bağlama; tanınmayan köken `403`).
- **Protokol yalnız `2026-07-28`**: oturumsuz, `initialize` el sıkışması yok, `GET`
  akışı yok. Her istek sürümünü hem başlıkta hem `_meta` içinde söyler ve ikisi aynı
  olmak zorundadır. Eski istemci, konuşulan sürümü adıyla söyleyen bir retle karşılanır.
  Karşılanan yöntemler: `server/discover`, `tools/list`, `tools/call`,
  `resources/list`, `resources/read`, `subscriptions/listen`.
- **Okuyan araç çalışır, yazan araç önerir.** Çizimi ya da diski değiştiren bir araç
  çağrısı **uygulanmaz**: bir öneri kaydı açar ve istemciye kimliği, durumu ve
  uygulanacak **komut satırlarının tamamını** hemen döndürür. Cevap bunu Türkçe olarak da
  söyler — yazdığının bir insanı beklediğini bilmeyen bir ajan boş sonucu başarısızlık
  sayıp yeniden dener. Bir istemci `_meta` içindeki `plan` alanıyla adımları tek
  öneride toplayabilir; akışı kapatmak iptaldir ve öneriyi geri çeker.
- **Onay zorlanamaz** (`src/ai/gate.hpp`): `ai::Approval`'ın yapıcısı özeldir, tek
  üreticisi `Gate::approve`'dur ve onun tek çağıranının öneri kartı olduğu
  `scripts/ci-gate-ai.sh` ile denetlenir. `ÖNERİ islem=uygula` komut satırından karar
  **vermez**; kararı taşır. Onaylanan öneri tek bir toplu iştir: tek `Ctrl+Z`, aradaki
  ret hepsini geri sarar.
- **Koordinat uydurulamaz** (`src/ai/handles.hpp`): ajana sunulan şemada nokta, nokta
  listesi ve nesne seçimi **tutamak dizesidir** (`@` + 16 onaltılık hane, istenirse
  `.N`), dolayısıyla sayı **ifade edilemez** ve ret argümanlar hâlâ JSON iken gerçekleşir.
  Tutamağı yalnız okuma araçları üretir, alındığı çizim sürümünü taşır ve eskiyen tutamak
  sessizce kullanılmaz — reddedilir. İstemciye koordinat listesi söylenmez; tek istisna,
  ekranın kendi köşelerini taşıyan pencere tutamağıdır.
- **Denetim kaydı** (`src/ai/audit.hpp`): kullanıcı yapılandırma dizininde aya bir JSONL
  dosyası (`denetim/YYYY-AA.jsonl`), `sürüm` alanı başta, her kararda diske boşaltılır.
  Ne istendiği, model, uç nokta, isteyen, komut satırları, karar, kararı veren kişi,
  UTC zaman, sonuç ve oluşan **kalıcı nesne anahtarları**. Reddedilen öneriler ve
  **koordinat reddi** de yazılır; hiçbir anahtar ya da belirteç yazılmaz.
- **`YAPAYZEKAMODELİ`** (`core.ai_provider`, `islem=listele|ekle|sil|varsayilan|dene`):
  sağlayıcı profillerini komut satırından, betikten ve ayar sayfasından aynı cümleyle
  yönetir — hangi modelin çalıştığı bir pencereye değil komut veri yoluna aittir
  (CLAUDE.md 5.15). **Anahtar bu komutun parametresi değildir**: `anahtar_ref` yalnız
  anahtarı tutan kaydın adını taşır, dolayısıyla anahtar `Args`'a, günlüğün
  `{cmd, args}` satırına, dökümüne ya da bir hata mesajına hiç ulaşamaz; anahtara
  benzeyen bir "anahtar adı" reddedilir. Komut **AI erişimine kapalıdır**: hangi
  modelin çalıştığını değiştirebilen bir model, kendisini sınırlayan çerçeveyi
  düzenliyor olurdu.
- **Profiller artık kalıcı ve düzenlenebilir** (`app/provider_service.hpp`,
  `app/secret_store.hpp`): profiller kullanıcı profilindeki `ai-modelleri.json`
  dosyasında sürüm alanıyla tutulur (okunamayan bir dosya **bildirilir** ve o oturumda
  yerleşik set kullanılır, kullanıcının dosyası olduğu gibi bırakılır);
  `Seçenekler ▸ Yapay Zeka Modelleri` sayfası profilleri tabloda gösterir — bağlam
  penceresinin yanında **kimin söylediği** yazar — ve her düzenleme
  `YAPAYZEKAMODELİ` satırı olarak veri yolundan geçer. **Anahtar bunun tek
  istisnasıdır ve ters yönde:** bir komuta dönüşmez, çünkü komut argümanları günlüğe
  yazılır. Anahtar işletim sisteminin deposuna gider — macOS Anahtar Zinciri
  (Security framework), Windows kimlik deposu, Linux'ta libsecret; hepsi sistem
  API'si, dolayısıyla yeni bağımlılık yok — ve **her yapıda** açık olan ikinci yol
  ortam değişkenidir: anahtar adı bir değişkeni adlandırır, değişken kullanım anında
  okunur ve hiçbir yere yazılmaz (`KENTOS_WITH_KEYCHAIN` kapalıysa kaydetme **reddedilir
  ve nedenini söyler**). `islem=dene` gerçek bir istek gönderir ve cevabı geldiğinde
  komut dökümüne yazar: bir modelden cevap beklerken pencere donmaz (ai.md R18, P8),
  ve hassasiyet işareti ile eksik anahtar istek gönderilmeden **önce** söylenir.
- **Model sağlayıcıları** (`src/ai/provider.hpp`): bir sağlayıcı **veridir**, lehçe
  **koddur** — `openai_chat`, `openai_responses`, `anthropic_messages`,
  `ollama_native`. Profil adres, yol, model, kimlik başlığı, ek başlık ve gövde, akış,
  düşünme ayarı, çıktı sınırı, sıcaklık, bağlam penceresi ve **anahtar adını** taşır;
  anahtarın kendisi işletim sisteminin anahtar zincirindedir ve bir API anahtarına
  benzeyen bir "anahtar adı" reddedilir. Yeni kurulumda on beş profil gelir ve
  **varsayılan yerel olan Ollama'dır**, çünkü kadastro verisi çoğu zaman kurumdan
  çıkamaz. Hassasiyet işareti bir **tiple** zorlanır: `permit_for` dışında hiçbir yerden
  uç nokta izni üretilemez, izin adresin makinesinden okunur ve hassas projede MCP
  dinleyicisi başlamaz.
- **Bağlam penceresi bir profil alanıdır**, gömülü bir sayı değil: sağlayıcıların çoğu
  bildirmez, bu yüzden sayı "kim söyledi" işaretiyle (bilinmiyor / yerleşik / kullanıcı /
  bildirilen) birlikte durur. Jeton tahmini dört **bayta** birdir, yani Türkçede
  yüksekten sayar; sağlayıcının bildirdiği sayı geldiğinde tahmin atılır, ortalanmaz.
- **`docs/llms.txt` ve `docs/llms-full.txt` üretilmiş dosyalardır** (`kentos_docgen`,
  `make reference`): birimler, eksen adları, Türkçe adlandırma, tutamak kuralı, uygulama
  kuralı ve araçların kullanım sırası. Aynı metin bağlanan istemciye
  `kentoscad://llms.txt` kaynağı ve `llms_txt` aracı olarak da sunulur. Üretilmiş
  referans gibi elle düzenlenmezler; `scripts/ci-gate-docs.sh` tazeliklerini diff ile
  denetler ve üretici derlenmemişse **atlamaz, kırar**.
- Kılavuza yeni bir bölüm: **Yapay zeka** (`docs/yapay-zeka/`) — ajanlar ve dört kural,
  MCP sunucusu, onay ve denetim, panel, model sağlayıcıları, lisans ve ağ yükümlülüğü;
  yedi komutun kendi sayfaları; sözlüğe MCP, ajan, öneri, tutamak, denetim kaydı, lehçe
  ve bağlam penceresi terimleri.
- **Sağlayıcı listesi artık veri** (`data/catalogs/ai/saglayicilar.json`, `ai/provider_catalog.hpp`):
  41 uç nokta ve 124 model kimliği — adres, kimlik başlığı, lehçe, bağlam penceresi ve
  hangi modelin düşündüğü. Bir model kimliği haftalar içinde eskir; artık bir satırın
  düzeltilmesi yeter, yeniden derleme gerekmez. `ProviderProfiles::builtin()`'in on beş
  C++ sabiti **silindi**: başlangıç profilleri, pencere şablonları ve model listeleri
  artık aynı dosyanın üç görünümüdür (CLAUDE.md 5.10). Katalog bulunamazsa program
  bunu söyler ve boş listeyle açılır — derlemeye gömülü bayat bir kopyaya düşmez.
- **Model profili penceresi** (`app/provider_dialog.hpp`): tablonun altındaki beş alanlık
  satır kaldırıldı. Satır bir profilin on dört alanından beşini ifade edebiliyordu ve
  düşürdüğü dokuzu — çıktı sınırı, sıcaklık, bağlam penceresi, düşünme kipi, sağlayıcının
  zorunlu başlıkları — uç noktanın cevap verip vermeyeceğine karar verenlerdi. Pencere
  hepsini taşır; **Düzenle** ve satıra çift tıklama aynı pencereyi mevcut profille açar.
- **Model artık seçilir, yazılmaz.** Şablonu seçtiğinizde model açılır listesi o
  sağlayıcının kendi modelleriyle dolar — yanlarında bağlam penceresi ve düşünüp
  düşünmediği — ve **Modelleri getir** uç noktaya sorup gelen listeyi yerine koyar
  (`/models`, Ollama için `/api/tags`). Kutu yazılabilir kalır: kurum içi bir sunucunun
  sunduğu ad hiçbir katalogda olmayabilir. Liste profilin KENDİ adresine sorulur, şablonun
  adresine değil.
- **Sohbet paneli** (`app/chat_panel.hpp`): araç çubuğundaki konuşma ikonu ya da
  **Ctrl+Shift+A** sağ tarafta bir panel açar. Akış canlı yazılır; ayarda **düşünmeyi
  göster** açıksa düşünme metni yanıtın üstünde katlanabilir bir blokta durur, kapalıysa
  üç noktalı gösterge ve geçen saniye çıkar. Dosya eklenir (metin, görüntü, PDF; en çok
  8 MB), bağlam sayacı kullanımı ve **hangi tür sayı olduğunu** yazar, **Dur** akışı
  keser ve yarı kalmış tur konuşmaya hiç girmez. Model bir okuma aracı çağırırsa panel
  onu hemen çalıştırır ve sonucu — ürettiği tutamaklarla — modele geri verir; en çok
  sekiz tur, sonra durur ve nedenini söyler. Yazan çağrıların tümü **turun tek
  önerisinde** toplanır ve yanıtın kendi balonunda bir **öneri kartı** olarak görünür.
  Panel modelin konuşacağı adresi kendi seçmez: izin `permit_for`'dan gelir, yani hassas
  projede bulut ucu istek gönderilmeden reddedilir.
- **Öneri kartı** (`app/suggestion_card.hpp`): kesikli çerçeve — çizimin parçası
  olmadığını bir şekil söyler, renk değil — `ÖNERİ` rozeti, uygulanacak komut
  satırlarının tamamı, **koordinatların geldiği tutamaklar**, isteyen ve model,
  çizim öneriden sonra değiştiyse bir uyarı şeridi, ve iki düğme: ikincil **Reddet**,
  birincil **Uygula**. Düğmenin sözcüğü `Uygula`'dır; `Onayla` kullanılmaz, çünkü onay
  sözcüğü ruhsatlı mühendisin imzasına ayrılmıştır (ai.md P4). Kararı veren kişinin adı
  yeni **`core.ai.sorumlu`** ayarından gelir, boşsa işletim sisteminin kullanıcı adı
  "işletim sistemi kullanıcısı" işaretiyle yazılır — denetim kaydı kimin onayladığını
  adlandırmak zorundadır (ai.md R8).
- **Beş yeni bileşen** (`app/widgets.hpp`): **döküm** (yalnız sonunda durursanız sonu
  izler), **ileti balonu** (dört konuşmacı, model balonu kurulurken `ÖNERİ` rozetini
  takar), **düşünme göstergesi**, **ek pençesi** (boyut etiketin parçasıdır) ve
  **bağlam ölçeri** (pencere bilinmiyorsa çubuk hiç çizilmez). Hepsi canlı standartta
  (`KENTOS_WIDGETS_PROBE`), `scripts/ci-gate-bilesenler.sh` envanterinde ve
  `docs/baslangic/bilesenler.md`'de (CLAUDE.md 6.13).
- **Durum çubuğunda dinleyici hücresi**: kapalıyken içi boş halka ve `MCP kapalı`,
  açıkken dolu nokta ve `MCP 8765`, belirteçsizken dolu üçgen ve **`MCP 8765
  KORUMASIZ`** — durum renkle değil şekille de söylenir (ui.md R31). Hücreye tıklamak
  menü üyesinin çalıştırdığı `MCPSUNUCU` satırını çalıştırır. **Analiz** menüsüne
  duruma göre sözcüğünü değiştiren `MCP Sunucusunu Başlat/Durdur` ve
  `MCP Belirteci Üret` girişleri eklendi.
- **İki yeni bütünleşme sınaması**: `mcp-server` gerçek dinleyiciyi geçici bir portta
  açıp protokolü konuşur ve yazan aracın **hiçbir şey uygulamadığını** doğrular;
  `chat` kayıtlı bir akışı sohbet paneline sürer ve çözülen bir araç çağrısının balona,
  karta ve — kart basıldıktan sonra — çizime dönüştüğünü doğrular. İkisi de ağa
  çıkmaz: hiçbir sınama canlı bir sağlayıcıyı aramaz (ai.md P10).
- **Henüz yok, ve kılavuzda gelecek zamanla yazılı:** `mevzuat_ara` (mevzuat derlemi
  dolmadan gelemez — madde numarası ve yayım tarihi taşımayan cevap bastırılır),
  bir önerinin **sonucunun** tuvalde hayalet önizlemesi (bugün vurgulanan şey
  girdilerdir) ve kullanıcının ölçüm listesini onaylayarak tutamağa çevirmesi.

### Eklendi — Yazdırma, PDF ve yazdırma profilleri

- **Yazdırma profilleri** (`io/print_profiles.hpp`): adlandırılmış kâğıt — kâğıt boyu
  (A5–A0 ya da `ozel`), yön, çözünürlük, kenar boşluğu. Biri varsayılandır. Kullanıcı
  profilinde JSON olarak tutulur (`yazdirma-profilleri.json`); çizime yazılmaz,
  günlüğe girmez, geri alınmaz. Yeni kurulumda altı profil gelir.
  **`YAZDIRMAPROFİLİ`** (`islem=listele|ekle|sil|varsayilan`) yönetir; `Seçenekler ▸
  Plot ve Çıktı` sayfasının başındaki tablo da aynı listeye bakar ve her düzenlemesini
  bu komutla yapar.
- **`YAZDIR`**: çizimin bir penceresini profilin kâğıdına yerleştirip PDF dosyasına
  yazar ya da yazıcıya gönderir. Alan iki biçimde verilir: **`merkez` + `olcek`**
  (paftanın dili; `olcek` yoksa projenin plan ölçeği) ya da iki köşe (`pencere`).
  Kâğıdın en-boy oranına **büyütülerek** oturtulur, çizim gerilmez. Kâğıt, yön, dpi
  ve kenar boşluğu satırdan geçersiz kılınabilir. Çizim ekrandaki boru hattından
  geçer: aynı semboloji, aynı kalınlıklar, profilin çözünürlüğünde.
- **PDF şifreleme** (qpdf, `KENTOS_WITH_QPDF`, yeni bağımlılık — Apache-2.0, `/NOTICE`):
  AES-256, açma ve sahip şifresi, `yazdirilabilir` / `kopyalanabilir` /
  `degistirilebilir` izinleri, `baslik` ve `yazar` alanları. **Şifreler günlüğe
  yazılmaz**: yazdırma salt okunur bir komuttur ve günlüğe hiç girmez — yeniden
  oynatılan bir yazdırma birinin PDF'ini yeniden yazardı (aynı karar `AÇ` ve
  `KAYDET` için de geçerli).
- **Yazdırma alanı çerçevesi**: araç çubuğundaki Yazdır'ın ilk basışı tuvalin
  ortasında kâğıt oranında bir çerçeve açar, dışını griler, köşelerine L işaretleri
  ve **merkezine + işareti** ile o noktanın `Sağa (Y)` / `Yukarı (X)` değerlerini
  koyar. Harita altında kayar (sol tuşla sürükleme), tekerlek yaklaştırır; çerçeve
  ekranda aynı boyda kalır. İkinci basış ya da **Enter** görüntüyü yakalayıp
  **önizleme penceresini** açar; **Esc** ya da sağ tık vazgeçer.
- **Önizleme penceresi**: solda kâğıdın kendisi (aynı çizim borusundan geçmiş),
  sağda profil, **elle yazılabilen ölçek** (çerçevenin ölçeği 1/200 gibi yuvarlak bir
  değere yuvarlanarak gelir) ve **merkez**, çıktı yeri, PDF alanları ve şifreleme.
  Altta gönderilecek komut satırı yazılı. Kâğıdın ölçüleri **yalnız profilde** durur:
  yön iki yerde sorulmaz.
- Araç çubuğunda Yazdır **Kaydet'in sağında**; yanındaki küçük ok profilleri listeler
  (varsayılan `●` ile) ve **Profilleri Yönet…** ayarlar sayfasını açar.
- Bileşen setine **sahneden seçme girdisi** eklendi (`FieldKind::Point` / `Object`,
  bkz. aşağıdaki madde) ve **gizli girdi** (`FieldSpec::secret`): şifre kutusundaki
  karakterler nokta görünür.
- `KENTOS_PRINT_PROBE` ve `print-pdf` ctest'i: gerçek ikili bir parseli A3 yatay bir
  profile basar, çıkan PDF'in sayfa ölçüsünü (1191×842 pt), yazar alanını ve
  şifrelenmiş olmasını doğrular; çerçevenin fare hareketini de sürer. Probe iki
  dosyayı **önce siler**: eski bir çıktı, hiçbir bayt yazılmasa da bütün
  denetimleri geçirir — bir kez tam bunu yaptı.

### Düzeltildi — Ağaç clang-format 18'e geri hizalandı

- 15 dosya clang-format **23** ile biçimlendirilmiş hâlde duruyordu ve CI'ın kullandığı
  **18** bunları ihlal sayıyordu; `make format-check` bu yüzden kırmızıydı (yazdırma
  değişikliğiyle ilgisi yoktu). Ağaç 18 ile yeniden biçimlendirildi; değişikliklerin
  tamamı tasarlanmış ilklendiricilerin ve satır sonu yorumlarının hizasıdır. Tek
  istisna: `mapped_file.cpp`'de `struct stat st{}` artık `= {}` ile yazılıyor, çünkü
  clang-format onu bir yapı TANIMININ başı sanıp süslü parantezi alt satıra indiriyordu.

### Eklendi — Katman görünürlüğü: çoklu seçim ve Görünüm menüsü

- **`KATMANGÖRÜNÜM`** (`KGÖ`, `LAYERVIEW`): bir katmanı gösterir (`islem=goster`),
  gizler (`gizle`), **yalnız** onu bırakır (`yalniz`), **hepsini** gösterir (`tumu`) ya
  da gösterimi **ters çevirir** (`tersine`). `KATMAN`'ın yanında ayrı bir komut olmasının
  sebebi iki şeyi YAPMAMASI: aktif katmanı değiştirmez (kırk katmanı gizlemek bir katman
  seçmek değildir) ve olmayan katmanı yaratmaz. Bir çağrı bir geri alma adımıdır.
- **Katmanlar panelinde çoklu seçim**: Ctrl ile tek tek, Shift ile aralık. Vurgulamak
  hâlâ bir düzenleme değildir — aktif katmanı değiştirmez (model.md R43).
- **Sağ tuş menüsünde `Görünüm` alt menüsü**: göster, gizle, yalnız bunu göster, tümünü
  göster, gösterimi ters çevir. **Seçimin tamamına** uygulanır; seçili olmayan bir satıra
  sağ tıklarsanız yalnız o satıra. Başlıklar kaç katman seçili olduğunu yazar. Eski
  tek satırlık `Gizle`/`Göster` girişi bu menünün içine taşındı.
- **Menü çubuğunda `Katman ▸ Tümünü Göster` ve `Gösterimi Ters Çevir`**: bir satıra bağlı
  olmadıkları için orada da var — bütün katmanlarını gizlemiş birinin sağ tıklayacak
  satırı kalmaz.
- Panelin gözü de artık `KATMANGÖRÜNÜM` gönderiyor, yani göze basmak katmanı aktif
  yapmıyor.
- `Controller::runLines`: bir jest, birden çok satır, **tek geri alma adımı**
  (`Bus::begin_batch`). On bir katmanı gizlemek on bir komuttur ve tek Ctrl+Z'dir; aradan
  biri reddedilirse hiçbiri uygulanmaz (Article 1.6).
- `scripts/ci-gate-katman-menu.sh` menüyü artık **gerçekten** ölçüyor: ekran yoksa
  offscreen platformla açıyor (eskiden "BEKLEMEDE" deyip geçiyordu, yani CI'da ve
  macOS'ta hiç çalışmıyordu). Yeni beklenen satırlar: alt menünün şekli, iki katmanın
  birlikte gizlenmesi ve tek geri almayla geri gelmesi.

### Düzeltildi — Çerçevede seçilen alan ile kâğıda giden alan aynı değildi

- Önizleme penceresi, çerçevenin ölçeğini **pafta ölçeğine yuvarlayıp** onu
  yazdırıyordu: 1/184 → 1/200, yani kâğıda çerçevede görülenden **beşte bir fazla
  zemin** giriyordu; en kötü durumda (1/101 → 1/200) alan neredeyse iki katına
  çıkıyordu. Artık **çerçevenin alanı kâğıda giden alandır**: pencere ölçek ve
  merkeze dokunulmadıkça çerçevenin iki köşesini olduğu gibi kullanır ve komut
  satırını da `pencere=… pencere=…` olarak gönderir.
- Pafta ölçeği kaybolmadı, **düğme oldu**: ölçeğin yanındaki **Yuvarla** bir üst
  pafta ölçeğine çıkarır (ölçek zaten yuvarlaksa görünmez), o anda satır
  `merkez=… olcek=…` olur ve büyüyen alan solda kâğıtta görülür. Ölçek ve merkez
  alanları, elle yazılmadıkça çerçevenin değerlerini **bildirir**; profil
  değişince yeniden hesaplanır.
- `KENTOS_PRINT_PROBE` çerçeveyi sürdükten sonra önizlemenin göndereceği satırı da
  **okuyor**: iki köşe var mı, ölçeğe çevrilmiş mi, gönderilen alanın merkezi ve
  boyu çerçevenin mi (%1 tolerans, çünkü kâğıdın en-boy oranına oturtmak bir
  kenarı kıl payı oynatır). Eski davranış geri konduğunda probe üç ayrı satırla
  düşüyor — denendi.

### Düzeltildi — `make check` IWYU aşamasında çöküyordu

- IWYU 0.26, libc++'ın `std::find` içindeki SIMD hızlı yolunu (`__simd_vector`,
  clang'ın `ext_vector_type` uzantısı) tanımadığı için **kendisi çöküyordu**:
  `iwyu.cc:1967: Assertion failed: TODO(csilvers): for objc and clang lang
  extensions`. Makefile'a bu, hiçbir şey anlatmayan `Error 250` (SIGABRT) olarak
  geliyor ve `make check`'i düşürüyordu. 258 dosyadan yalnız biri — `style.cpp`,
  64 bitlik tam sayılar üzerinde `std::find` çağıran sıradan bir derleme birimi —
  yetiyordu; bu değişiklikle ilgisi yoktu.
- `scripts/run-iwyu.sh` çözümlemeyi artık `-D__OPTIMIZE_SIZE__=1` ile çalıştırıyor:
  bu, libc++'ın kendi okuduğu anahtardır (`_LIBCPP_VECTORIZE_ALGORITHMS` 0 olur) ve
  skaler yol derlenir, böylece IWYU o türle hiç karşılaşmaz. Bayrak **yalnız
  çözümlemeye** girer, derlemeye giremez: `make build` bu betiği çağırmaz. Gate
  gevşetilmedi — hiçbir çıkış kodu yutulmuyor. `-Os` işe yaramıyor, çünkü IWYU
  eniyileme bayraklarını makro türetilmeden önce atıyor. IWYU `ExtVectorType`'ı
  ele alan bir sürüm çıkarınca kaldırılacak.

### Düzeltildi — `canvas-edits` sınaması çöküyordu (yazdırmadan önce de)

- Tuval sınaması (`KENTOS_EDIT_PROBE`, `canvas-edits`) bu değişiklikten **önce de**
  çöküyordu: probe `ALAN`'ı dört köşeli bir satırla çağırıyor, komut beşinci köşeyi
  beklerken parklanıyor, sonra `SEÇ nesneler=1` var olmayan nesneyi arıyor ve boş
  tablonun sıfırıncı satırı okunuyordu. Probe artık şekli **sağ tuşla bitirip Esc ile
  aracı bırakıyor**; ayrıca son bölümü belgelenmiş araç modeline hizalandı (sağ tuş
  bitirir ve araç elde kalır, Esc bırakır) — eskiden Esc'ten sonra aracın yanık
  kalmasını bekliyordu, ki bu modal araçlardan önceki modeldi.

### Düzeltildi — Sürüklenen harita bırakılmıyordu

- Yazdırma çerçevesi açıkken haritayı sol tuşla sürükleyip bırakmak haritayı
  bırakmıyordu: tuval yalnız **orta** tuş için `panning_` bayrağını temizliyordu, sol
  tuşla başlayan kaydırma bırakıldıktan sonra da fareyi izlemeye devam ediyordu.
  `print-pdf` testi bu davranışı sürüyor: düzeltme geri alınınca test kırılıyor.

### Eklendi — Bağlı nesneler ve sahneden seçme girdisi

- **Bağlı nesneler** (`core/attach.hpp`): bir nesne başka bir nesnenin bir köşesini ya da
  kenarını izler. `UZUNLUKYAZ`'ın yazdığı uzunluklar kenarlarına, `KÖŞENUMARALA`'nın
  yazdığı numaralar köşelerine bağlı doğar (`bagla=evet` varsayılan; `hayır` serbest yazı).
  Kaynağı değiştiren komut — `TAŞI`, `DÖNDÜR`, `ÖLÇEKLE`, `KÖŞETAŞI`, `KÖŞEEKLE`,
  `ALANDÜZENLE`, tutamak — bittiğinde bağlı yazılar **aynı işlem ve aynı geri alma adımı
  içinde** yeniden yerleşir; uzunluk yazısının sayısı yenilenir; köşe sayısı değişince
  en yakın kenara/köşeye yeniden bağlanır; kaynak silinince bağlı yazılar da silinir ve
  bu söylenir. Elle taşınan yazının **el payı** saklanır, kaynak sonra taşınsa da korunur.
  Günlükte yalnız verilen komut vardır; yeniden oynatma aynı sonucu verir. Bağlar
  `.pcad` dosyasında `0x0089` bloğunda, kalıcı anahtarlarla saklanır; bağı olmayan çizimin
  dosyası değişmez. `content_hash()` bağları katlar (bağ yokken değişmez).
- **`BAĞLA`** (`islem.bagla`): kapsamdaki yazıları seçilen nesnenin en yakın kenarına ya
  da köşesine bağlar (`bag=kenar|kose`); yazı yerinden oynamaz; `tur=uzunluk` ile sözü
  kenarın uzunluğu olur. **`BAĞÇÖZ`** (`islem.bag_coz`): bağı çözer. İkisi de Araçlar
  panelinde **Etiketleme** altında.
- İşlem araçları bir **nesne parametresi** bildirebilir (`ToolParam::object`); çalıştırıcı
  o nesnenin kopyasını `ToolInput::references` olarak araca verir. Yerinde değiştirme
  çıktısı artık yazıyı, bağı ve bağ çözmeyi de taşır.
- **Sahneden seçme girdisi** (bileşen seti, `FieldKind::Point` / `FieldKind::Object`):
  Araçlar kartındaki nokta ve nesne alanlarının yanında bir nişan düğmesi; basınca işaretçi
  seçim işaretçisine döner, durum satırı ne istendiğini söyler, tuvaldeki tık alanı
  doldurur (nokta seçerken köşeler yakalanır; birden çok nesnede "Hangisi?" listesi).
  Klavyeden **F4** / **Alt+↓** başlatır, **Esc** vazgeçer; değer elle de yazılır. Canlı
  bileşen standardında, kapıda ve `docs/baslangic/bilesenler.md`'de.

### Düzeltildi — macOS'ta tuval boştu

- macOS'ta çizim, ızgara ve cetveller **hiç çizilmiyordu**: `TitleBar` içindeki
  `QMenuBar` yerel menü çubuğu varsayılanıyla kurulduğundan Qt ana pencerenin yerel
  penceresini canvas var olmadan yaratıyor (`QMenuBarPrivate::handleReparent` →
  `createWinId`), QRhi ile birleştirme kararı o anda "hayır" olarak donuyor ve
  `QRhiWidget` her karede "No QRhi" diyordu. Uygulama `Qt::AA_DontUseNativeMenuBar`
  ile açılıyor (kabuk menülerini zaten kendi çiziyor), menü çubuğu ebeveynsiz kurulup
  sonra bağlanıyor, ve duman testi (`KENTOS_SMOKE`) tuvalin GPU bağlamını aldığını
  doğruluyor. Linux'ta yerel menü çubuğu olmadığı için sorun görünmüyordu.
- macOS derlemesi: `io::VectorReport::layer_names` ve `ProbeReport::layers` çiftleri
  `std::size_t` oldu; `std::uint64_t` ile `std::size_t` bu platformda farklı türler ve
  atama derlenmiyordu.

### Eklendi — İşlem araçları (Processing)

- **`kentos_processing`** modülü ve `ProcessingTool` arayüzü (`.claude/processing.md`):
  bir araç adını, açıklamasını, uygulandığı geometri türlerini, parametrelerini ve çıktı
  biçimini bildirir; her araç kayıttan **üretilen bir komuttur** (`İşlem` kategorisi).
  Dört ortak parametre: `nesneler`, `kapsam` (secili/gorunum/proje), `pencere`, `katman`.
- **Asenkron çalışma**: iş kopya üzerinde ayrı iş parçacığında koşar (`Job`), durum çubuğu
  yüzde gösterir (`Job::permille`), **Durdur** hiçbir şey yazılmadan keser; sonuç tek
  işlemde, tek geri alma adımında, istenen katmana yazılır. Günlüğe uygulanan nesnelerin
  kimlikleri ve bütün parametreler yazılır; yeniden oynatma aynı sonucu verir.
- **Araçlar paneli**: sağ panelde Öznitelikler ve Geçmiş'in yanında üçüncü sekme; ağaç,
  süzgeç, kart (türler, kapsam, parametre alanları, çıktı katmanı, gönderilecek komut
  satırı, Çalıştır). **Analiz ▸ İşlem Araçları** menüsü aynı kayıttan üretilir.
- İlk iki araç: **`UZUNLUKYAZ`** (kenar uzunluklarını kenara paralel, istenen birim ve
  biçimde yazar) ve **`KÖŞENUMARALA`** (köşeleri seçilen köşeden başlayarak, istenen yönde
  ve biçimde — `A00001` — numaralar, dışa yazar).
- **`ALANDÜZENLE`**: kapalı bir alanı istenen alana getirir — her yandan eşit
  daraltıp genişleterek (`mod=hepsi`, komut satırı ve toplu iş), bir kenarı kaydırarak
  (`kenar`) ya da bir köşeyi çekerek (`kose`). Arayüzde kenar/köşe tıklanır, hayalet
  fareyi izler ve hedefe yaklaşınca oturur, **Enter** hedefi kabul eder; özgün sınır o
  ana kadar durur. Aritmetik `core/area_edit.hpp`'de, milimetrede ve belirlenimci.
  İşlem araçları artık bir **etkileşimli faz** (`ProcessingTool::interact`) ve
  **yerinde değiştirme** çıktısı (`OutputShape::InPlace`) bildirebilir.
- Araçlar panelinde bir araca ikinci kez tıklamak da kartı (ya da pencereyi) açar; kart
  aracın bu çizimdeki son değerleriyle açılır (`AYAR son_değerler`, `core.islem.hatirla`);
  seçili tek alanın alanı kartta yazar; açık temada açılır listenin seçili satırı okunur.
- Araçlar ağacında her araç ve grup ikonlu; `TERCİH araç_penceresi` (`core.islem.pencere`)
  açıkken bir araca tıklamak kartı kendi penceresinde açar. Kart panelin zeminine oturur,
  alanlar bileşen standardının kutularıyla belirgindir.

### Değiştirildi — Araç modeli, nişan ve tutamaklar

- **Araç elde kalır.** Her modal araç (çizim, düzenleme, ölçme) bitince yeniden
  hazırlanır: **sol tuş başlatır, sağ tuş bitirir, araç seçili kalır**; Esc ya da Seç oku
  bırakır. Nesne isteyen bir araç yeniden hazırlanırken seçimi temizler. `ALANÖLÇ` artık
  nesnesi yoksa sorar (`Interactive`).
- **Sağ tık bitirir**, iptal etmez. Komut nesne beklerken bir tık seçime **ekler**
  (`BİRLEŞTİR` iki nesneyi tıkla alabilir); Ctrl çıkarır.
- **CAD nişanı**: ortası boş artı ve ortasında seçim toleransı boyunda **seçim kutusu**;
  nokta beklenirken yalın artı. İşletim sistemi imleci tuvalde gizli.
- **Hayaletler**: `ELİPS`, `SPLINE`, `TARAMA`, `ÖLÇÜ` (uzatma çizgileri ve oklarıyla),
  `BLOKEKLE` (bloğun kendisi), `TAŞI` ve `KOPYALA` (nesnelerin kendileri) imlecin altında
  yapılacak şekli çizer; hepsi türün kendi çizim koduyla.
- **`KOPYALA` her `bitis` noktasına bir kopya** koyar (`bitis` artık nokta listesi).
- **Tutamaklar her türde**: `core/grips.hpp` daire (merkez + dört çeyrek), yay (merkez,
  uçlar, orta), elips (merkez, eksen uçları ve aynaları), yaylı çizgi (köşeler + yay
  ortaları; şişkinlik korunur), spline/tarama/lider köşeleri, ölçü (tanım noktaları +
  yazı; çizgi ve rakam yeniden kurulur), blok referansı (ekleme noktası). `KÖŞETAŞI` bunları
  taşır, tuval tutamaklarını çizer ve sürüklerken türün kendi önizlemesini gösterir.
  `KÖŞEEKLE` eğrileri reddetmeye devam eder.
- **NORMAL** çipi durum çubuğunda (`core.yakalama.yuzey_normali`).
- **Düzeltildi:** geometrisi değiştirilen nesnenin yazısı yeni yuvaya taşınır. `KÖŞETAŞI`
  yazılı bir nesnenin (METİN, ölçü) köşesini taşıyınca yazı eski yuvada kalıp kayboluyordu.
- **Düzeltildi:** bir komut nokta beklerken yazılan başka bir komut önce bekleyeni bitirir;
  önceden bekleyenin açık işlemi içinde çalışıyor ve hiçbir şey çizmiyordu.
- `core::dimension_layout` / `dimension_picks` / `dimension_baseline`: ölçünün yerleşimi
  komut, tutamak ve önizleme için tek yerde.

### Düzeltildi — DXF birimi ve daire/yay okuma

- **Birimde otorite `AYAR çizim_birimi`.** Dosyanın `$INSUNITS` başlığı artık yalnız
  karşılaştırılıp söylenir; farklıysa `uyarı:` satırı düzeltme komutunu verir. Türk
  kadastro DXF'leri sayılar metre iken başlıkta milimetre der; başlığa inanmak koca bir
  ilçeyi sekiz metreye sığdırıp her daireyi lekeye çeviriyordu.
- **Daire ve yay uydurma** üç noktalı çevrel merkezden tüm köşelerin en küçük kareler
  uydurmasına geçti; kısa kaldırım yayları artık yay olarak gelir.
- **Elips yakalama** çizilen eğriye oturur (MERKEZ, dört eksen ucu UÇ; YAKIN/DİK/KESİŞİM);
  tanım çizgilerine oturmaz. Seçim vurgusu da elipsi elips olarak çizer.
- Kayıp sayaçları yalnız içe alınan katmanları sayar.

### Eklendi — Yeni nesne türleri (Sprint 5, C1–C5)

- **Altı yeni tür**, her biri kendi `KindSpec`'i, yükü ve `docs/nesneler` sayfasıyla:
  yaylı çoklu çizgi (`core.arc_polyline`, 6), spline (`core.spline`, 7), tarama
  (`core.hatch`, 8), blok referansı (`core.block_reference`, 9), ölçü (`core.dimension`,
  10), lider (`core.leader`, 11); elips (5) kısmi elips için 24 baytlık yük taşır. Her yük
  8 baytlık düzen başlığıyla başlar (model.md R9a).
- **Komutlar**: `SPLINE`, `TARAMA`, `BLOK`, `BLOKEKLE`, `ÖLÇÜ`, `LİDER`; `ELİPS
  baslangic= bitis=`. `TAŞI`/`DÖNDÜR`/`ÖLÇEKLE`/`AYNALA` yükü de dönüştürür (yay
  merkezleri, tarama açısı, blok dönüşümü, ölçü değeri).
- **Kataloglar** `data/catalogs/dxf/`: `tarama-desenleri.json` (SOLID, ANSI31–37, LINE,
  NET, DOTS, EARTH, GRASS), `olcu-stili.json` (ISO-25, STANDARD, MIMARI); yolları
  `TERCİH desen_kataloğu` ve `ölçü_stilleri`.
- **Çizim**: `EmitBuffer` koşuları stil, katman ve yazı taşır; blok referansı üyelerini her
  karede tam sayı aritmetiğiyle yerleştirir (ByBlock ve `0` katmanı referansa uyar);
  yazılı bir tür (ölçü) hem yazısını hem çizgilerini çizer.
- **DXF**: şişkinlik → yaylı çoklu çizgi, `SPLINE` → spline, `HATCH` → tarama (aileler
  katalogdan), `BLOCK`/`INSERT` → blok tanımı ve referansı (açılmaz), `DIMENSION` →
  ölçü (DIMSTYLE ölçüleriyle), `LEADER` → lider, kısmi `ELLIPSE` → kısmi elips; yazıcı
  her birini kendi varlığıyla geri yazar (`BLOCK`, `DIMSTYLE` tablosu dahil).
  `blok_adi`/`tarama_deseni` köprü sütunları kalktı.
- Altın senaryo `nesne-turleri.txt`; fixture `22-lider.dxf`.
- DXF renk 7 ("beyaz/siyah") siyah okunur ve siyah 7 olarak yazılır: katman tablosu olmayan
  bir dosyanın çizgileri beyaz zeminde görünmez geliyordu.
- **Araç kutusu ve Çizim menüsü**: SPLINE çizgi ailesine, TARAMA yüz ailesine girdi; iki
  yeni aile: Blok Ekle (`BLOKEKLE` · `BLOK`) ve Ölçü (`ÖLÇÜ` · `LİDER`).
- **Durum çubuğu anahtarları çalışıyor**: YAKALAMA, DİK, POLAR ve OSNAP var olmayan ayar
  kimliklerine bağlıydı ve tıklandığında yalnız "bir ayara bağlı değil" diyordu; şimdi her
  biri kapsamının komutunu gönderir (`MOD`/`TERCİH`), OSNAP ve POLAR yakalama maskesini
  okur ve yazar. **KALINLIK** yeni `çizgi_kalınlığı` tercihidir: kapalıyken çizgiler kıl
  çizgi olur, kalınlık nesnede kalır.

### Eklendi — DXF libdxfrw ile (Sprint 4)

- **libdxfrw** (GPL-2.0-or-later, LibreCAD, `92d7466e`) `KENTOS_WITH_DXFRW` arkasında,
  kaynak indirilebilen her yapıda açık; kendi CMake'i CMake 4'te derlenmediği için
  kaynaklarından bizim hedefimiz olarak kurulur (Article 2.5 kayan nokta bayraklarıyla).
- **Okuyucu** (`import_dxf`): daire, yay, elips gerçek eğri; LWPOLYLINE/POLYLINE
  (şişkinlik düşürülerek), SPLINE (de Boor, düşürülerek), INSERT açılımı (ölçek, dönme,
  ayna, dizi, iç içe 16 kat, ByBlock renk, `blok_adi`), HATCH (dolu → dolgu; desen →
  `tarama_deseni`), SOLID/TRACE/3DFACE alan, TEXT/MTEXT (kod süzgeci, hiza), katman
  rengi/kalınlığı/durumu, nesne stili, `$INSUNITS` karşılaştırma, kod sayfası uyarısı,
  Z → `kot`, tutamak → `kaynak_kimlik`, XDATA → yabancı veri, 44 satırlık tanı sözlüğü.
- **Yazıcı** (`export_dxf`): her tür kendi varlığı; katmanlar renk/kalınlık/görünürlük/kilit;
  öznitelikler `KENTOSCAD` XDATA'sı olarak (`ad#tür=değer`) ve geri okunur; yabancı XDATA
  geldiği gibi; `.prj` yan dosyası; `DIŞAAKTAR surum=2000…2018`.
- GDAL'ın DXF sürücüsü yalnız libdxfrw kapalı derlenmiş yapılarda kullanılır.
- Fixture'lar 18–21 (şişkinlik, XDATA, blok dönüşümü, aynalı OCS) ve tam gidiş-dönüş testi.

### Değişti — İçe aktarma arayüzü dondurmuyor (Sprint 3)

- **İki fazlı içe aktarma.** Okuma bir `Job` olarak ayrı iş parçacığında bir karalama
  belgeye yapılır (`read_into_scratch`), sonra `Transaction::adopt_from` ile komutun tek
  işlemi içinde gerçek belgeye kopyalanır (io.md P3, R17). Arayüz = komut satırı = betik:
  üçü aynı belgeyi ve günlüğü üretir; kanıt testi eklendi.
- Durum çubuğunda iş etiketi, kayan şerit ve **Durdur** çipi; Esc de durdurur. Sihirbazın
  `Okumayı durdur` düğmesi artık pencereyi kapatmıyor, okumayı durduruyor.
- Komut satırına yazılan etkileşimli komutlar düğme gibi başlar (`begin_interactive`),
  günlükte kaynağı `CommandLine` olarak kalır.
- `Transaction::adopt_from` genel: her tür (bilinmeyen dahil), yük, katman görünümü,
  stil, yazı, öznitelik, yabancı veri ve blok tanımları kopyalanır.

### Eklendi — CAD çekirdeği C0 (Sprint 2)

- **Tür yükü** (model.md R9a): `RingGeometry` yuva başına bayt dizisi taşır; dosyada
  0x0080–0x0083 blokları; `Document::add_kind` / `set_kind_payload`; `KindSpec::validate`
  ve `key_points`; içerik özeti türü ve yükü katlar.
- **Tanınmayan tür korunur** (R26): okuyucu her türü kabul eder, elips dosyaya gidip
  geri gelir, bilinmeyen tür görünür ve düzenlenemez, yeniden kaydedilince bayt bayt aynı.
- **Yabancı veri** (`ForeignTable`, R26a) ve **blok tablosu** iskeleti (R45,
  `FlagInBlock`); dosya blokları 0x0084–0x0088; panelde `ek_veri` sayısı.
- **`entity_outline`** ve çok koşulu `EmitBuffer` (delik bayrağı): sahne, seçim, yakalama
  ve kanvas tek yoldan geçer.
- **Belirlenimcilik araçları**: `atan2_udeg`, `rotate_udeg`, `mul_div_round`,
  `circular_segment_area`; yayın uzunluğu artık `std::atan2` kullanmaz; `transform.cpp`'deki
  ikinci trigonometri silindi.
- Yakalama modu **EKLEME** (bit 17); maske 16 bite kırpılmıyor.
- `docs/nesneler/` sayfaları ve üretilmiş `docs/nesneler/referans.md`.

### Düzeltildi — `make check` baştan sona yeşil

- **Sıfır uyarı.** `painter_backend`, `symbol_preview`, `theme` ve
  `settings_dialog`'daki on `-Wdouble-promotion` / `-Wunused-lambda-capture`
  uyarısı kapatıldı. Anayasa 6.3 uyarısız bir yapı istiyor; kimsenin okumadığı
  uyarılarla dolu bir yapı, içindeki gerçek bulguyu da gizler.
- **Biçim.** Yeniden adlandırma tanımlayıcıları kısalttığı için clang-format'ın
  sarma noktaları kaydı ve ağaç 519 ihlale çıktı; `make format` ile sıfırlandı.
  **Not:** clang-format sürümü hiçbir yerde sabitlenmiş değil ve CI'da bir format
  işi yok — bu ağaç 23.1.0 ile biçimlendirildi. Farklı bir sürümle çalışan bir
  katkıcı gereksiz churn görür; sürümü sabitlemek ayrı bir iş.
- **IWYU macOS'ta çalışıyor.** `iwyu_tool.py` SDK yolunu bilmediği için ilk
  dosyada `'type_traits' file not found` ile düşüyordu — bozuk bir checkout gibi
  okunan bir toolchain iletisi. `scripts/run-iwyu.sh` `xcrun --show-sdk-path`
  ekliyor; Article 10 gereği Makefile'da OS koşulu değil, betikte.

### Eklendi — HACİM: kazı ve dolgu

- Kotlu noktalardan kurulan yüzeyi bir kotla karşılaştırıp kazı ve dolgu
  hacimlerini veriyor: saha düzenlemesinin, yol platformunun, havuzun keşif
  hesabı.
- **Netlenmiyor.** 500 m³ kazı ve 500 m³ dolgusu olan bir saha bir haftalık makine
  işidir; net hacmi sıfır olduğu için hiçbir şeyin kıpırdamadığı bir saha hiç iş
  değildir. Makineler ayrı rakamlara göre tutulur. Fark yine yazılıyor — ama en
  sonda ve adı konarak.
- Düzlemin **kestiği** üçgen ikiye ayrılıyor: bir üçgeni bütün hâlde tek tarafa
  saymak, dolguyu kazı sütununa yazmak olurdu.
- Ara çarpımlar 128 bitte: bir sahanın alanı ile derinliğinin çarpımı int64'ü
  kolayca aşar.
- Belge: [`earthwork.md`](docs/komutlar/earthwork.md).

### Eklendi — EŞYÜKSELTİ ve yüzey modülü

Yeni modül `/src/domain/surface` (Article 3.1 zaten öngörüyordu).

- **`EŞYÜKSELTİ`** — kotlu noktalardan eş yükselti eğrileri. Bir ekip nivelman
  yapıp birkaç yüz nokta getirir; paftaya giren o noktalar değil, tasarımcının
  araziyi okuduğu bu eğrilerdir. Eğriler kendi katmanına düşüyor ve her biri kendi
  `kot` özniteliğini taşıyor.
- **Kotu olmayan nokta kullanılmıyor**, sıfır sayılmıyor: bir yamacın ortasındaki
  deniz seviyesi noktası etrafındaki bütün eğrileri aşağı çekerdi.
- **Üçgenleme saklanmıyor.** Ara üründür; saklamak programın tamamına — kırpma,
  seçme, alan, dışa aktarma — kimsenin çizmediği bir şey için tür öğretmek olurdu.
- **CDT bağlandı** (MPL 2.0, 21fae3ba'da sabit, `/NOTICE`'ta). Bir nivelman
  ızgarası **her yerde** eş çemberlidir ve elle yazılmış bir Delaunay'ın çapraz
  üçgen ürettiği yer tam orasıdır. CDT, 5.4'ün zaten zorunlu kıldığı Shewchuk
  yüklemleriyle çalışıyor. Kendi CMakeLists'i CMake 4'ün desteklemediği bir
  minimum bildirdiği için kaynağı alınıp include dizini doğrudan kullanılıyor —
  deponun Lua/sol2/stb için zaten kullandığı kalıp.
- **Zincirleme toleranssız**: bir kotun bir kenarı kestiği yer, kenarın iki
  ucundan sabit sırayla hesaplanıyor, yani kenarı paylaşan iki üçgen aynı
  milimetreyi buluyor ve parçalar tolerans olmadan birleşiyor. Tolerans gerektiren
  bir zincirleme, paftanın basacağı kıl payı boşluklar bırakırdı.
- Kapalı görünen ama **alanı sıfır** olan eğri (bir kot sırası nivelman noktalarına
  tam denk geldiğinde olur) alan değil çizgi olarak çiziliyor.
- Belge: [`contour.md`](docs/komutlar/contour.md).

### Eklendi — DÖNÜŞTÜR: koordinat sistemi dönüşümü

- Çizimin tamamını bir sistemden diğerine taşıyor ve belgenin CRS etiketini de
  yeniliyor. Türkiye'de günlük bir iş: arşiv ED50 dolu, her yeni pafta TUREF, bir
  belediyenin katmanları yan paftadan farklı dilimde olabiliyor.
- Dönüşümü **PROJ** yapıyor (§9). Bir datum kaymasını yeniden yazmak, bir sınırın
  kimse fark etmeden yarım metre kaymasının yoludur.
- **Coğrafi ucu reddediyor.** Çizim geometrisi tam sayı milimetre; `29,830716°`
  en yakın "milimetreye" yuvarlandığında nokta yüz metre kayar.
- **Etiket koordinatları izliyor**: sayıları taşınmış ama CRS'i eski sistemi
  söyleyen bir çizim, hiç dönüştürülmemiş olandan kötüdür — aşağıdaki her okuyucu
  etikete güvenir.
- Tek geri alma adımı: her köşe taşınır ya da hiçbiri.
- Belge: [`reproject.md`](docs/komutlar/reproject.md).

### Eklendi — ALANİFRAZ: alana göre parsel ayırma

- "Bu parselden yola paralel 400 m² ayır" — bir harita mühendisinden gerçekte
  istenen ifraz. `İFRAZ` söylendiği yerden keser; buna cevap verilir ve kesimi o
  bulur.
- **Kesim verilen yöne paralel kayar** ve arama bu yüzden güvenilir: sabit yönlü
  bir çizgi kaydıkça arkasında kalan alan tek yönlü büyür, dolayısıyla ikiye
  bölme tek bir cevaba yakınsar. Bir nokta etrafında dönen kesimin böyle bir
  garantisi yoktur ve bazen başka bir geçerli cevap bulan bir çözücü, üzerine
  tapu kaydı hesaplanacak bir şey değildir.
- **Elde edileni raporluyor, isteneni değil.** Sınır milimetre ızgarasına oturur;
  istenen sayıyı yazan bir komut tapuya gidecek bir sayı hakkında yalan söylerdi.
  Tolerans (varsayılan 0,01 m²) aşılırsa ifraz **yapılmıyor**.
- **Mevzuat imzası bekliyor** (6.11): hangi toleransla kabul edileceği ve
  ada/parsel numarasının hangi parçada kalacağı mevzuata ait sorulardır.
- Belge: [`split_area.md`](docs/komutlar/split_area.md).

### Eklendi — Netcad iş akışı: NOKTALAR ve APLİKASYON

Hedef kitle Netcad kullanan harita mühendisleri; bu iki komut onların günlük
işinin iki ucu.

- **`NOKTALAR`** — ölçü nokta listesi okuma ve yazma. Sahadan dönen numaralı
  liste çizime nokta olarak giriyor; numara, kot ve kod öznitelik oluyor.
  **Y sağa, X yukarıdır** (Türkiye'de liste `no, Y, X, Z` yazılır); `eksen=XY`
  söylenmek zorunda, tahmin edilmiyor. Koordinatlar basamak basamak okunuyor,
  `strtod` ile değil. Ayraç dosyadan anlaşılıyor, Türkçe ondalık virgülü
  okunuyor, bozuk satır sessizce atlanmıyor.
- **`APLİKASYON`** — istasyondan her noktaya mesafe ve açı listesi. `baglama`
  verilirse açılar ondan ölçülüyor (**semt açısı**), verilmezse kuzeyden
  (**azimut**) — ve rapor hangisi olduğunu yazıyor. Alet göreli açı okurken
  azimut vermek, operatörü tripodun başında kafadan çevirmeye zorlardı.
  Birim `açı_birimi` tercihine uyuyor, varsayılan grad.
- Belgeler: [`points.md`](docs/komutlar/points.md),
  [`stakeout.md`](docs/komutlar/stakeout.md).

### Değişti — tanımlayıcılar da KentOSCad oldu (Faz 0/2)

Görünen ad daha önce değişmişti; bu adım kodun içindeki adları taşıdı. Tamamen
mekanik, tek commit, açık dal yokken.

| Eski | Yeni |
|---|---|
| `namespace piricad` | `namespace kentos` |
| `#include "piricad/…"` | `#include "kentos_cad/…"` |
| `piricad_core`, `piricad_app`… | `kentos_core`, `kentos_app`… |
| `PIRICAD_*` makro ve ortam değişkenleri | `KENTOS_*` |
| `piricad` çalıştırılabiliri | `kentos_cad` (macOS'ta `KentOSCad.app`) |
| `piricad.md` | `kentoscad.md` |
| `cmake/PiriCAD*.cmake` | `cmake/KentOSCad*.cmake` |
| `piricad_test.hpp`, `piricad_tr.ts` | `kentos_test.hpp`, `kentos_tr.ts` |

**Üç şey bilerek DEĞİŞMEDİ** ve `CLAUDE.md` 0.5a bunu anayasaya yazdı, çünkü
sonradan gelen biri "işi bitirmek" isteyebilir:

- **Proje dosyasının 8 baytlık imzası `PIRICAD\x1A`.** O ana kadar yazılmış her
  dosyada duruyor; değiştirmek yeniden adlandırma kılığında bir veri kaybı olurdu.
  Doğrulandı: yeni yapıyla yazılan bir `.pcad` hâlâ `PIRICAD\x1A` ile başlıyor.
- **İçerik karması tohumları `fnv1a("piricad.core.*")`.** Her golden fixture'a,
  her günlük parmak izine ve eşitlik kanıtına katlanmış durumdalar.
- **`core.*` ayar kimlikleri ve bütün komut kimlikleri.** Kaydedilmiş belgelerin
  ve günlüğün içindeler; bir tekrar onları adlarıyla çözüyor.

Depo adresi `github.com/mcihad/kentos_cad`, `vcpkg.json` adı `kentos-cad`.

### Değişti — uygulamanın adı KentOSCad oldu (görünen ad)

- Pencere başlığı, hakkında kutusu, dosya süzgeçleri ve 29 belge sayfası yeni adı
  taşıyor. `QApplication` kimliği `KentOSCad`, alan adı `kentoscad.org`.
- **Kullanıcı verisi taşınıyor, kaybolmuyor.** Qt her kullanıcı yolunu uygulama
  adından türetir; ad değişince ayar dosyası, stil kütüphanesinin yazdığı yapılandırma
  dizini ve otomatik kaydın veri dizini başka yere düşerdi. `migrate_user_data()`
  eski yolları Qt'ye *sordurup* (tahmin etmeden, üç platformun yerleşimi farklı)
  yenisine taşıyor; hedef zaten varsa dokunmuyor.
- İsim uzayı, `#include` yolları, CMake hedefleri ve `KENTOS_*` makroları bu adımda
  **değişmedi**; onlar tek mekanik değişiklik olarak ayrı iniyor.

### Eklendi — ELİPS

- Merkez ve iki eksenden elips. **Tanımıyla saklanıyor** (`core.ellipse` türü,
  kimlik 5): merkez ve iki eksen ucu, beş sayı. Çizilen çok kenarlı hat yalnız
  görüntü — hattı saklayan bir çizim, biri yakınlaştırdığı anda şeklin kimliğini
  kaybederdi.
- Eksen uçları **nokta** olarak saklanıyor, uzunluk ve açı olarak değil: iki eksen
  vektörü dönüklüğü zaten taşıyor, yani hiçbir yerde açı saklanmıyor ve okunmuyor.
- Çizilen hat `DAİRE`'nin **aynı** birim çember tablosundan geliyor, iki eksen
  boyunca ölçeklenerek: çarpma ve toplamadan başka işlem yok, üç platformda aynı
  köşeler (§7.3). İkinci bir tablo kurmak, iki eğrinin çeyreklerin nerede olduğu
  konusunda ayrışması demekti.
- **İkinci eksen birinciye dik alınıyor.** Serbest bırakılsaydı kullanıcı elips
  olmayan kaydırılmış bir şekil çizebilirdi ve kayıtta hiçbir elipsin sahip
  olmadığı iki eksen dururdu.
- Döndürülmüş elipsin **kapsam kutusu** üç saklanan köşenin kutusu değil: o üçgen
  şeklin içinde kalır ve o kadar küçük bir kutu, elipsi görüşün kenarında düşürür.
- Belge: [`docs/komutlar/ellipse_draw.md`](docs/komutlar/ellipse_draw.md).

### Eklendi — kadastro: TEVHİT, İFRAZ, TOPOLOJİ (Faz 8)

Yeni modül `/src/domain/cadastre` (Article 3.1 zaten öngörüyordu). Üç komut da
alan modülünden kaydediliyor: `/src/command` bir alan modülüne bağımlı olamaz.

- **`TEVHİT`** — komşu parselleri tek parselde birleştirir; dikiş kalkar, alan
  korunur. Bitişik olmayan parselleri **reddeder**: iki ayrı parçayı çizip "tek
  parsel" demek TKGM'nin reddedeceği bir kayıt üretirdi.
  - **Öznitelikleri uydurmuyor.** Bütün girdilerde aynı olan sütun korunuyor;
    ayrışan sütun BOŞ geliyor ve komut hangilerini boşalttığını yazıyor. Birinci
    parselin malikini seçmek bir tapu kaydı uydurmak olurdu. Ayrışanı dolduran
    kural mevzuata aittir, `/data`'ya ve **uzman imzasına** (6.11) tabidir.
- **`İFRAZ`** — bir parseli düz ayırma çizgisiyle ikiye böler. Çizgi parselin
  dışına uzatılıp iki yarı düzlem kesiştiriliyor: **hiçbir alan kaybolmuyor**, bir
  bant çıkarılmıyor. Her parçanın ve toplamın alanı, ifrazdan önceki alanla
  birlikte yazılıyor.
  - Kırıklı çizgiyle ayırma **yaklaşık yapılmıyor**: bir parselden eksilen birkaç
    santimetrekare, birinin sahip olduğu birkaç santimetrekaredir.
  - Alana göre ifraz yok: yineleme ve tolerans gerektirir, ikisi de mevzuat
    kararıdır.
- **`TOPOLOJİ`** — kendini kesen sınır, sıfır alan ve örtüşen parselleri
  raporlar. **Hiçbir şeyi düzeltmez**: sınır ölçülmüş veridir, bir kusurun ne
  anlama geldiğine mühendis karar verir. Ortak sınır örtüşme sayılmıyor (bir
  santimetrekare pay), yoksa gerçek örtüşmeler okunmayan bir raporun altında
  kalırdı. Ne kadarının denetlendiğini her zaman yazıyor.
- **Poligon boolean çekirdeğe eklendi** (`core/offset.hpp`): birleşim, fark,
  kesişim ve halka alanı, Clipper2 üzerinden. Delikler **çift-tek** kuralıyla
  okunuyor: bir DXF'ten ya da GML'den gelen delik dışıyla aynı yöne sarılmış
  olabilir ve biçim aksini vaat etmez.
- Belgeler: [`merge.md`](docs/komutlar/merge.md),
  [`split_parcel.md`](docs/komutlar/split_parcel.md),
  [`topology.md`](docs/komutlar/topology.md).

### Eklendi — yerel koordinat ve OTURT (Faz 7)

- **`OTURT`** — yerel ölçülmüş bir çizimi yayımlanmış kontrol noktalarıyla
  haritaya taşır. Ekibin istasyonu kurup ona 0,0 dediği iş için: çizimin kendi
  içinde her mesafesi ve açısı doğrudur, olmadığı tek şey haritanın üzerinde
  olmaktır.
- **2B Helmert, afin değil.** Öteler, döndürür, tek ölçekle büyütür; asla
  kaydırmaz. Bir ölçünün iç geometrisi veridir — kendi çizgileri arasındaki
  açılar ölçülmüş gerçeklerdir — ve kaydırabilen bir dönüşüm, kontrolü ölçüye
  değil ölçüyü kontrole uydururdu.
- **Ölçek kilitlenebilir** (`olcek_kilitli=evet`): kalibre şeritle çalışan bir
  ekibin mesafeleri yeniden ölçeklenmez, böylece bir kontrol hatası çizimdeki her
  uzunluğa sessizce dağılmaz. Kilitliyken artıklar büyür — büyümesi gerekir.
- **Artıklar, RMS ve en büyük artık** raporlanıyor; ölçüm uygulanacak dönüşümle,
  yuvarlaması dahil yapılıyor, yani rapor çizimin alamayacağı bir uyum vaat
  etmiyor.
- **Determinizm**: kapalı biçim en küçük kareler, yalnız +, −, ×, ÷ ve `sqrt`.
  Dönüklük hiçbir zaman açıya dönüşmüyor — (a, b) çifti olarak kalıyor — ve
  noktalar tam sayıda kendi ağırlık merkezine indirgeniyor, yani çift duyarlık
  yalnız küçük farkları görüyor (§7.3).
- **Tek geri alma adımı**: her köşe taşınır ya da hiçbiri. Yarı taşınmış bir
  kadastro paftası Article 1.6'nın adını koyduğu hatadır ve burada her yerden
  kötüdür, çünkü iki yarısı da makul görünür.
- **`YEREL` sistem**: durum çubuğu ve `KOORDİNAT` "haritaya oturtulmadı" diyor.
  Bir yerel okuma kendi sahası hakkında doğrudur ve başkasına hiçbir şey ifade
  etmez; tapuya yazılabilecek bir çift gibi görünmemeli.
- **Varsayılan dilim TUREF/TM36 oldu** (K1). Golden fixture'lar bilinçli olarak
  yeniden üretildi; diff yalnız CRS satırını ve ondan türeyen içerik özetini
  içeriyor, hiçbir geometri değişmedi.
- Alan komutları ayrı kaydediliyor (`register_geodesy_commands`): `/src/command`
  bir alan modülüne bağımlı olamaz (Article 3.2), bu yüzden yerleşik liste
  OTURT'u adlandıramaz.
- Belge: [`docs/komutlar/fit.md`](docs/komutlar/fit.md). **Golden değerleri
  jeodezi uzmanı imzası bekliyor** (6.11); aritmetiği ve determinizmi sınanmıştır.

### Eklendi — cetvel kılavuzları (Faz 6)

- **`KILAVUZ`** — yatay ya da düşey sonsuz yapı çizgisi. Üst cetvelden aşağı,
  sol cetvelden sağa **sürükleyerek** bırakılır; cetvele geri bırakmak vazgeçmektir.
  Bırakma bir `KILAVUZ` komutu gönderiyor, yani betik de aynısını koyuyor.
- **Kılavuz bir nesne değil.** Geometrisi, stili, katmanı, özniteliği yok;
  seçime girmiyor, alan hesabında sayılmıyor, kapsamı büyütmüyor. Belgenin
  mobilyası: dosyayla gidiyor, dosyayla geliyor. Varlık tablosuna konsaydı bir
  yapı çizgisinin tapuya karışması kaçınılmaz olurdu.
- **Yeni yakalama kipi: `kılavuz`.** İki kılavuz kesişiyorsa imleç kesişime
  oturuyor — bir noktayı aplike etmenin yolu budur — tek kılavuza yakınsa üzerinde
  kayıyor. Sıralamada gerçek her köşenin **altında**: kullanıcının kendi çizdiği
  bir çizgi, ölçülmüş bir noktayı elinden alamaz.
- Dosya biçimine iki **isteğe bağlı** blok eklendi (`0x0039`, `0x003A`).
  Kılavuzu olmayan bir çizim hiç blok yazmıyor, yani eski dosyalar okunur kalıyor
  ve golden fixture'lar aynı boyutta (io.md R10).
- Silme **yerini söyleyerek** yapılıyor, sıra numarasıyla değil: bir kılavuzu
  silmek sonrakilerin sırasını kaydırır.
- Belge: [`docs/komutlar/guide.md`](docs/komutlar/guide.md).

### Eklendi — DİLİM ve HALKA

- **`DİLİM`** — merkez ve iki kenardan daire dilimi: kavşak dolgusu, görüş konisi,
  etki sektörü. Süpürme `YAY`'daki gibi saat yönünün tersine.
- **`HALKA`** — merkez, iç ve dış yarıçaptan delikli halka: kuyu koruma bandı,
  sabit genişlikte tampon. Deliği gerçek delik — alan hesabında sayılmıyor. İç ve
  dış noktanın sırası önemsiz.
- İkisi de **alan olarak** saklanıyor, tanımıyla değil: `DAİRE` ve `YAY` iki
  sayıdan çıktığı için tanımıyla saklanır, bir dilimin sınırı ise iki düz yarıçap
  ve bir eğridir. Model'e bunun için tür eklemek, programın başka hiçbir yerde
  tanıması gerekmeyen bir şekli tanımak zorunda bırakırdı.
- Eğrileri `YAY` ve `DAİRE`'nin deterministik bölmesinden geliyor, yani üstüne
  çizilen bir yay tıpatıp aynı köşelere oturuyor (§7.3).
- Belgeler: [`sector.md`](docs/komutlar/sector.md), [`annulus.md`](docs/komutlar/annulus.md).

### Eklendi — OFSET, KAYDIR ve çalışan Alan Seç

- **`OFSET`** — seçili nesnelerin paraleli. Yol şeridi eksenden, çekme mesafesi
  parsel sınırından, koruma bandı dere ekseninden bununla çıkar. Aslını yerinde
  bırakır: sınır ölçülmüş olandır. Bir `OFSET` kaç paralel üretirse üretsin tek
  geri alma adımıdır.
  - **Clipper2 bağlandı** (BSL-1.0, 1.4.0'da sabit) ve `/NOTICE`'ta "bağlı"
    listesine taşındı. `kentos_core`'a PRIVATE bağlı; başlıkları yalnız
    `core/offset.cpp`'ye ulaşıyor. Elle yazılmış ofset ters köşede yanlış, içbükey
    girdide kendini kesen, keskin köşede sınırsız sivrilen sonuç verir (5.16).
  - Tam sayı girip tam sayı çıkıyor: Clipper2'nin `Path64` yolu int64, `Mm` de
    int64 — dönüşüm yok, yuvarlama yok, üç platformda aynı cevap.
  - Kapanan şekli **yok** sayıyor, ters dönmüş halka üretmiyor; bel veren bir
    şekli ikiye bölebiliyor ve ikisini de çiziyor.
- **`KAYDIR`** — görünümü iki noktayla öteler, ölçek değişmez. Saydam komut:
  çizim sürerken araya girip devam edebiliyor. Orta tuşu olmayan aygıtlar için
  düğme ve komut vardı, komut yoktu.
- **`SEÇ` etkileşimli oldu**: pencere köşelerini artık kendisi soruyor. Kutuyu
  argüman olarak alabiliyor ama isteyemiyordu, bu yüzden "Alan Seç" düğmesi
  gönderecek bir şey bulamayıp devre dışı çıkıyordu.
- Belgeler: [`offset.md`](docs/komutlar/offset.md), [`pan.md`](docs/komutlar/pan.md).

### Eklendi — öznitelikler sağ panelden düzenleniyor

- Panel boyanan bir tablo olmaktan çıkıp **düzenleme yüzeyi** oldu: arkasında bir
  komut olan her satır çift tıklama, `Enter`, `F2` ya da `Space` ile düzenlenir.
  Metin kutusu, evet/hayır çevirme, renk seçici ve kapalı küme listesi — satırın
  tipine göre.
- **Panelin belgeye özel bir yolu yok**: her düzenleme bir komut satırı kurup veri
  yoluna veriyor (`ÖZNİTELİK`, `KATMAN`, `STİL`, `AYAR`). Günlüğe yazılıyor, tek
  `GERİAL` ile kalkıyor, betikten aynısı yapılabiliyor (Article 1.1, 5.9).
- Nesne **kalıcı kimliğiyle** adlandırılıyor, slotuyla değil — bir slot sonraki
  düzenlemede başka nesneye düşebilir ve günlük tekrarı yanlış parsele yazardı.
- Panel klavyeyle tam kullanılabilir (ui.md R21); düzenlenebilir değer okuma
  mürekkebiyle, düzenlenemeyen bir adım soluk yazılıyor.

### Düzeltildi — panel yanlış nesnenin özniteliğini gösteriyordu

- Öznitelik sütunu **geometri slotuna** göre indekslidir (`Document::set_attribute`
  `entities_.slot[e]` yazar); panel ise sütunu **varlık slotuyla** okuyordu. İkisi
  yalnız nesneler sırayla oluşturulmuşsa ve hiçbiri düzenlenmemişse aynıdır — yani
  panel yeni bir çizimde doğru, gerçek bir çizimde başka bir parselin değerini
  gösteriyordu. Okuma `Document::attribute` üzerinden yapılıyor artık; eşlemeyi
  bilen tek okuyucu odur.

### Eklendi — dinamik girdi ve çizim adımı

- **Kılavuz artık ölçüsünü yazıyor**: sürüklenen lastik bandın üzerinde uzunluk ve
  azimut. Azimut kuzeyden saat yönündedir — aletten okunan değer — ve birimi
  `açı_birimi` tercihine uyar; varsayılan **grad**, çünkü Türkiye'de nirengi,
  poligon ve aplikasyon hesapları gradla yürür.
- **ADIM kilidi** (`core.yakalama.adim`): imlecin bir önceki noktaya olan
  uzaklığını verilen değerin katına yuvarlar. 12 cm dendiyse çizgi 12, 24, 36 cm'de
  durur. Yön kilitleriyle birlikte çalışır — dik mod/kutupsal yönü, adım uzunluğu
  seçer — ve gerçek bir nesne yakalaması her zaman adımın önündedir.
  `Shift+F3 ▸ Adım…` ya da `MOD ad=adım deger=120`.
- Adımın aritmetiği tam sayıdır (`segment_length` + tam bölme), yani üç platformda
  bit-birebir aynıdır (§7.3).
- **`core.arayuz.dinamik_girdi` ayarı eklendi.** Durum çubuğundaki DİNAMİK GİRDİ
  çipi bir yıldır var olmayan bir ayara bağlıydı: tıklandığında "bu yardımcı henüz
  bir ayara bağlı değil" diyordu.

### Eklendi — nesne yakalama modları arayüzden seçilebiliyor

- Yakalama motoru on üç kip taşıyor ve kabuk üçünü gösteriyordu: F3 "herhangi
  biri", F8 dik mod, F9 ızgara. KESİŞİM, DİK AYAK, EN YAKIN, DÜĞÜM, UZANTI,
  PARALEL ve UZATILMIŞ KESİŞİM yazılmış, sınanmış ve programdan erişilemez
  durumdaydı.
- **OSNAP kip listesi**: durum çubuğundaki OSNAP çipine sağ tık, `Shift+F3`, ya da
  `Görünüm ▸ Yakalama Modları…`. Her satır `core::snap_mode_label`'dan gelir —
  motora eklenen bir kip listede kendiliğinden belirir (5.10) — ve her değişiklik
  `MOD ad=yakalama_modları` komutunu gönderir.
- **DÜĞÜM'ün glifi eklendi.** Kip `default`'a düşüyor ve hiçbir şey çizmiyordu:
  bir röpere yakalanıyordunuz, işaretçi tutmadığını söylüyordu. Kadastroda her
  sınır bir röperden ölçüldüğü için bu kip sıralamada en üsttedir.
- Belge: [`docs/baslangic/arayuz.md`](docs/baslangic/arayuz.md) — on iki kipin
  tamamı, öncelik sırası ve hassasiyet ayarı.

### Eklendi — KOORDİNAT komutu

- `KOORDİNAT` / `KOORDINAT` / `COORDINATE` / `KRD`: tıklanan noktanın sağa ve yukarı
  değerini belgenin koordinat sisteminde yazar. Salt okunur — geri alma yığınına
  girmez. Araç kutusundaki devre dışı "Koordinat Oku" düğmesinin yerine geçti.
- Belge: [`docs/komutlar/coordinate.md`](docs/komutlar/coordinate.md).

### Düzeltildi — çalıştığı hâlde kullanıcıya ulaşmayan iki komut

- **METİN artık tuvale yazıyor.** Komut çapayı alıp `ctx.text` ile yazı istiyordu;
  istem alttaki komut satırının yer tutucusuna düşüyor, odak tuvalde kalıyordu, ve
  kabuk metin cevabı verecek bir yola sahip değildi — komut süresiz bekliyordu.
  `Controller::supplyText` eklendi ve tuval, çapa tıklamasının hemen ardından
  tıklanan yerde bir yazı kutusu açıyor. Enter yazar, Esc vazgeçer.
- **Komut çıktısı durum çubuğunda.** Her komut `ctx.echo` ile konuşur; bu yalnız
  kullanıcının çoğu zaman kapalı tuttuğu `Geçmiş` sekmesine düşüyordu. ÖLÇ ölçüyor,
  sonucu kimsenin bakmadığı yere yazıyordu — "ölçüm araçları çalışmıyor" bu.
- **ÖLÇ ve KOORDİNAT modal araç oldu**: çalışırken araç kutusunda yanıyorlar.
- **YAY araç kutusuna eklendi.** Tam bir çizim aracı olarak kurulmuş ama sütuna
  konmamıştı; programın çizebildiği tek eğri yalnız adı yazılarak ulaşılabiliyordu.

### Değişti — stil tasarımcısı, `design.md` §8'e hizalandı

- **Özellikler dört başlık altında ve sol etiketli**: KATMAN · DOLGU · KENAR ·
  GEOMETRİ · GÖRÜNÜRLÜK. On dört satırlık düz liste, her satırda etiketi üstte
  taşıyor ve 756 px'lik pencerede üç satır gösterip gerisini kaydırıyordu.
  Şimdi etiket 110 px'lik sol sütunda, değer yanında; yaygın tiplerde hiçbir
  şey kaydırılmıyor. Boş kalan başlık gösterilmiyor.
- **Ön izleme ile sembol katmanları yan yana**, mockup'ın çizdiği gibi. Üst
  üste dururken ikisi 372 px alıyordu; şimdi listenin boyu kadar.
- **Birim alanın içinde**: `Çizgi kalınlığı (µm)` → `Kalınlık` + `µm` soneki,
  `Açı (°)` → `Açı` + `°`. Etiketler kısaldı, hiçbiri sarmıyor.
- Üst şeridin başlıkları 16 px'ten §8'in 10.5 px büyük harfine indi; şerit
  bir sekme değil, iki kontrol gibi okunuyor.
- Renk alanı en az 200 px boyanmayı bırakıp sütununun genişliğini alıyor;
  açılır kutular en uzun öğelerine değil sütuna göre daralıyor. İkisi de sağ
  kenardan taşan satırların sebebiydi.
- Belge: [`docs/baslangic/stil-tasarimcisi.md`](docs/baslangic/stil-tasarimcisi.md).

### Düzeltildi — macOS'ta `make run` çalışmıyordu

- CMake macOS'ta `.app` paketi üretir ve ikili `bin/kentos_cad.app/Contents/MacOS/`
  altına iner; Makefile, `/docs` örnekleri ve gate'ler ise `bin/kentos_cad`'ı arar.
  Derleme artık macOS'ta o yola göreli bir sembolik bağ bırakıyor; üç tüketici
  de değişmeden çalışıyor.
- `libpq` Homebrew'da keg-only olduğundan `find_package(PostgreSQL)` onu
  bulamıyor, PostGIS sessizce kapanıyordu; `brew --prefix libpq` ipucu eklendi.
- `qgis_backend.cpp` `KENTOS_WITH_QGIS` kapalıyken de derleniyordu; QGIS
  başlıkları olmayan her makinede derleme kırılıyordu.

### Eklendi — gömülü Lua betik motoru (`KENTOS_WITH_LUA`)

- **`kentos::script::LuaRunner`**, sol2 üzerinden gömülü Lua 5.4. JSON
  çalıştırıcısının yerine geçmez, yanına gelir: `BETİK` hangi motorun çalışacağını
  dosya uzantısından seçer (`.lua` → Lua, gerisi → JSON).
- **Tek yazma yolu `h.komut(...)`**, `Bus::execute_line` üzerinden — komut
  satırıyla aynı ayrıştırıcı, aynı doğrulama, aynı geri alma, aynı günlük.
  Okuma bağlantıları yalnız değer döndürür; çizime hiçbir tutamak verilmez.
- **Kum havuzu üç seviye.** `güvenli` seviyede `io`, `os`, `package`, `debug`,
  `require`, `dofile`, `loadfile` ve `load` hiç açılmaz. `proje` seviyesinde
  dosya erişimi proje dizinine hapsedilir ve yol `weakly_canonical` ile çözülerek
  denetlenir, metin öneki karşılaştırılarak değil. `tam` yalnız o betiğin metnine
  verilmiş onayla çalışır.
- **İptal edilebilir**: `std::stop_token`, 10 000 komutta bir yoklanan bir Lua
  hook'uyla; sonsuz döngü de durur.
- **Bir betik = bir geri alma adımı**; herhangi bir satırın hatası bütün bloğu
  geri alır.
- Lua 5.4.8 ve sol2 3.5.0 sabitlenmiş commit'lerden indirilir; makinede kurulu
  olmaları gerekmez. İkisi de MIT, `/NOTICE`'a işlendi.
- Belge: [`docs/betik/lua.md`](docs/betik/lua.md).

### Eklendi — günlüğün ikinci satır türü: `{kind:"meta"}`

- `Journal::append_meta()` — komut olmayan kayıtlar için (`.claude/command.md`
  R20). İlk kullanıcısı betik çalıştırmalarının kum havuzu seviyesi ve `tam`
  onayıdır (`.claude/script.md` R11, R12); eklenti kimliği ve kullanıcı kararı
  aynı satır türünü kullanacak.
- Tekrar oynatma bu satırları **atlar**, çünkü neyin yapıldığını değil neye izin
  verildiğini anlatırlar. `canonical()` de dışarıda bırakır: üç istemcinin
  bayt-birebir günlük kanıtı (CLAUDE.md 6.4) yalnız betiğin yazdığı bir satır
  yüzünden bozulmamalıdır.
- JSON çalıştırıcısı da artık bu satırı yazıyor; önceden hiçbir konak yazmıyordu.

### Düzeltildi — QRhi tuvalinde çizim çıkmıyordu

Üç kusur, üçü de ekran görüntüsünden görünmeyen cinsten; kare ikiye bölünerek
bulundu (`KENTOS_RHI_DEBUG`).

- **`firstInstance` taşınabilir değil.** Örneklenmiş çizimlerde çizgi grupları
  `cb->draw(..., firstInstance)` ile ayrılıyordu; bu `QRhi::BaseInstance`
  gerektirir ve OpenGL ES ile ARB_base_instance'sız GL'de yoktur — üstelik
  eksikse çizim reddedilmez, sessizce yanlış olur. Karenin ilk çizgi grubu
  çiziliyor, sonrakiler kayboluyordu: ızgara yarım, kuzey oku ve ölçek çubuğu
  yok. Artık vertex buffer bayt kaydırmasıyla bağlanıyor; hiçbir GPU özelliği
  gerektirmiyor.
- **Belge çizgilerinin kaydırması örnek indeksiydi**, bayt değil. Grid'den sonra
  tampon yarım örnekten okunuyor, çizim ekran dışına düşüyordu. Belge tek başına
  çizdirilince görünüyor olması kusuru bire bir işaret etti.
- **Stil tasarımcısı GPU yapısında çöküyordu**: sembol önizlemeleri `QImage`'a
  çiziyor ama `make_canvas_backend()` GPU arka ucunu döndürünce `QPaintDevice*`
  işaretçisi `RhiFrameTarget*` diye okunuyordu. Önizlemelerin artık kendi
  fabrikası var (`make_preview_backend`).

### Düzeltildi — kare dökümü GPU tuvalini boş gösteriyordu

`QWidget::grab()` arka tampon üzerinden yürür; `QRhiWidget`'ın karesi orada
değil, GPU'dadır. `KENTOS_FRAME_DUMP` ve `KENTOS_SHOT_DIR` bu yüzden doğru
çizen bir tuvali boş gösteriyordu. `MapCanvas::grabCanvas()` kareyi kendi
yüzeyinden alıyor ve pencere görüntüsüne yerleştiriliyor.

### Düzeltildi — kayıtlı dock yerleşimi kabuğu bozuk gösteriyordu

`kLayoutVersion` 4'e çıktı. Tuval `QRhiWidget` tabanına geçince merkez pencere
sınıf değiştirdi ve çevresindeki dockların kayıtlı boyutları anlamsız kaldı:
Öznitelikler paneli otuz piksellik boş bir şerit olarak geri yükleniyordu, yani
sanki bütün kabuk dağılmış gibi görünüyordu. Bayat olan **durumdu**, çizici
değil — bunu ayırt etmek bir öğleden sonra aldı, çünkü kayıtlı yerleşim yeniden
derlemeden sağ çıkar ve yeni bir hata gibi görünür.

### Eklendi — GPU tuvalinde metin: SDF atlası (`KENTOS_WITH_TEXT`)

- `render::TextAtlas` — FreeType konturu → msdfgen çok kanallı mesafe alanı →
  stb_rect_pack ile tek dokuya; HarfBuzz `tr` diliyle şekillendirme
  (`.claude/render.md` R8). Qt'siz, `/src/render` içinde: `/tests` Qt bağlamaz,
  dolayısıyla arka uç kurulamayan bir süitte bile `İ` ile `I`'nın ayrı glif
  olduğu doğrulanabiliyor.
- Ölçek bağımsız: bir 48 px hücre 8 pikselde de 200'de de keskin çıkar, çünkü
  saklanan şey piksel değil **kontur**. Üç kanalın medyanı köşeleri korur; tek
  kanallı bir alan her köşeyi yuvarlar.
- Cetvel sayıları, ölçek çubuğunun rakamları, kuzey okunun `K` harfi ve çizimin
  kendi başlıkları artık GPU'da. Döndürülmüş taban çizgisi, çok satırlı
  TAKS/KAKS etiketi ve dört çapa QPainter arka ucuyla aynı kuralları izler.
- 7 test: beş yüzün açılması, `ÇİĞDEM`'in altı harfinin altı glif olması (bayt
  sayan bir çizici dokuz üretirdi), `İ` ≠ `I`, boşluğun kalem ilerletip
  çizmemesi, mono yüzün gerçekten eşaralıklı olması, alanın gradyan taşıması,
  aynı kelimenin atlası ikinci kez büyütmemesi.
- Shader hedefleri GLES 3.0 / GL 3.3'e çekildi: qsb'nin varsayılanı ESSL 100 ile
  başlar ve orada ne `textureSize` ne türev vardır.

### Eklendi — QRhi GPU canvas'ının ilk dilimi (`KENTOS_WITH_RHI`)

- `render::Backend`'in GPU uygulaması: poligon dolguları (stencil ile tek-çift
  kuralı, üçgenleyici bağımlılığı olmadan), shader'da genişletilen çizgiler
  (`render.md` R5) ve ızgara/seçim/imleç katmanı. Shader paketleri derleme
  anında `qsb` ile pişirilir; çalışma anında hiçbir shader derlenmez (P11).
- **Metin ve yayımlanmış raster semboller bu dilimde çizilmez.** `handles()` bir
  beyaz listedir ve tanımadığı katman türünü sahiplenmek yerine reddeder — QGIS
  arka ucunun üç raster türünü sessizce düşürmesi bu yüzden bir kapıya bağlandı.
- `MapCanvas` seçeneğe göre `QRhiWidget` ya da `QWidget` tabanlıdır; arada kalan
  her şey aynıdır.
- `scripts/doctor.sh` artık `qsb`'yi Qt'nin kendi dizinlerinde arıyor. Qt
  araçlarını hiçbir platformda PATH'e koymaz, dolayısıyla eski yoklama kurulu
  olan bir makinede "MISSING" diyordu.

### Eklendi — MPYY gösterimlerinin vektör paketi tamamlandı

Sebep: **Mekânsal Planlar Yapım Yönetmeliği (MPYY), EK-1 Gösterimler**
(RG-22/1/2026-33145; EK-1b için RG-14/6/2014-29030).

- **`data/catalogs/mpyy-vektor` 30 satırdan 467 satıra çıktı.** Yönetmeliğin 476
  gösteriminin tamamı ele alındı: 467'si çizildi, 9'u gerekçesiyle atlandı.
  Toplam 1 574 sembol katmanı ve 98 SVG çizim, 431 KB.
- **Sayılar ölçüldü, tahmin edilmedi.** Her kırpma gömülü DPI'sıyla mikrometreye
  çevrildi; çizgi kalınlığı, tekrar adımı, işaretçi aralığı, kesik/boşluk yapısı
  ve mürekkep rengi izdüşüm ölçümünden okundu. Eğik taramalarda projeksiyon adımı
  dik aralığın √2 katıdır; ölçüm bunu böler.
- **17 satırda kaynak kusuru bulundu ve yazıldı.** Yönetmelik ekinin bazı
  hücrelerine gösterim yerine program ekran görüntüsü konmuş; AYRIK, BİTİŞİK,
  BLOK DÜZEN ve KAT ADEDİ satırları aynı düz siyah lekeyle basılmış. Kusur
  satırın `kaynak_kusuru` alanına gerekçesiyle geçti.
- **Dokuz satır çizilmedi** çünkü ekin kendisi o satırların bütün gösterim
  sütunlarını boş basmıştır; gerekçeleri `ATLANANLAR.md` dosyasında.
- **`scripts/ci-gate-mpyy-vektor.py`** paketi ve iş listesini denetler; `ctest`
  ve `make check` içinden çalışır. İş listesinde işaretli ama pakette olmayan bir
  satır kusurdur.

### Eklendi — DİKDÖRTGEN komutu, poligon aracı ve köşegen kilidi

- **`DİKDÖRTGEN` (`core.rectangle`).** Karşılıklı iki köşeden dört köşeli kapalı
  alan çizer; kalan iki köşeyi program hesaplar. Elle tıklanan dört köşe
  "neredeyse" diktir, ve imzalanan bir paftada neredeyse dik bir kusurdur.
  Günlüğe iki köşe yazılır, türetilen dördü değil.
- **Araç kutusunda poligon ve dikdörtgen artık çalışıyor.** İkisi de yer
  kaplayan ama devre dışı birer `placeholder`'dı; `ALAN` komutu ise baştan beri
  vardı ve hiçbir düğmeye bağlı değildi.
- **`core.yakalama.kosegen` — köşegen kilidi.** İmleci öncekinden 45°'nin
  katlarına kilitler; dikdörtgenin ikinci köşesi böyle kilitlenince **kare**
  çıkar. Çizerken **Ctrl** basılı tutmak bu modu basılı tutar, `MOD köşegen=evet`
  aynı anahtarı yazarak açar — Ctrl bir fare hüneri değil, bir modun kısayolu
  (Article 5.15). Motorda yeni bir kısıt yok: 45° adımlı kutupsal izlemedir.
- **Kılavuz artık çizilecek şekli gösteriyor.** `Prompt` bir `RubberShape`
  taşıyor; dikdörtgen çizilirken tuval köşegeni değil **dörtgeni** önizliyor.
  Çizgi olarak önizlenen bir dikdörtgen, ne çizileceğini tıklamadan önce
  söylemez — kilit basılıyken kareyi görmekle çizdikten sonra öğrenmek arasındaki
  fark budur.

### Düzeltildi — alan gösterimleri sembolojide seçilebiliyor

İki ayrı kök neden, ikisi de aynı sonucu veriyordu: bir parsel katmanı için alan
dolgusu seçilemiyordu.

- **Geometri sekmesi katmanın değil sembolün şeklinden seçiliyordu.** Tek
  konturlu bir parsel katmanı "çizgi" sayılıp Çizgi sekmesinde açılıyor, galeri
  de çizgi gösterimlerini listeliyordu. Sekme artık katmanın **nesnelerinden**
  okunuyor: kapalı halkası olan katman alan katmanıdır.
- **Bildirilen katman yığını sınıflandırılmıyordu.** Raf, bir satırın alan mı
  çizgi mi olduğunu yalnız resimli paketin alanlarına (`tarama`, `çizgi_tipi`,
  `sembol`, alan renk kodu) bakarak karar veriyordu. Vektör paketinin 476
  satırının hiçbirinde bunlar yok — hepsi `else` dalına düşüp çizgi oluyordu.
  Artık yığın ne çiziyorsa o: dolduran katman alan, konturlayan çizgi, yalnız
  glif basan nokta. Alan sekmesi 335 gösterim listeliyor.

### Eklendi — gösterimin öteki adı da aynı satıra çıkıyor

- **`takma_adlar`.** Bir gösterim satırı, yönetmeliğin aynı kullanım için
  kullandığı öteki yazımları taşıyabiliyor; `STİL sinifla=` bir özniteliği
  kimliğe, ada **ve** takma ada göre çözüyor. MPYY EK-1e'nin 379 detay kartından
  40'ı ekteki gösterimden başka yazılmıştır (`KRUVAZİYER LİMANI` / `KRUVAZİYER
  LİMAN`); detay kataloğundan etiketlenmiş veri o satırlarda hiçbir şeye
  eşleşmiyor ve parsel varsayılan renkte, hatasız çiziliyordu.
- **13 satıra takma ad yazıldı.** Her biri yönetmeliğin kendi öteki yazımıdır.
  Bir değerin hangi gösterime ait olduğuna dair **karar** gerektiren hiçbir
  eşleme yazılmadı: yoğunluk kademeleri, EK-1e'de iki kartın birleştiği satırlar
  ve birden çok gösterime yakın duran kartlar `UZMANA.md` dosyasında gerekçesiyle
  duruyor ve imza bekliyor (Article 6.11).
- Takma ad bir gösterimin kendi adını gölgeleyemez; test bunu paketin tamamında
  sınıyor.

### Ölçüldü — çizim arka uçları ve desen dolgusunun bedeli

`KENTOS_FRAME_TIMES=<n>` eklendi: tuvali n kez boyar, kare maliyetlerinin
ortancasını yazar ve sahne kurulumunu çizimden ayırır. `KENTOS_FRAME_DUMP` ile
aynı kategoride geliştirici kancasıdır — kullanıcıya bakan bir özellik değildir.

576 parselli bir yaprakta (`tests/bench/sahne/`), aynı yakınlıkta:

| Sahne | QGIS | Dahili |
|---|---|---|
| MPYY gösterimli (desen dolgusu) | 57,8 ms | 58,7 ms |
| Düz dolgu | 1,23 ms | 1,07 ms |
| Sahne kurulumu | 0,065 ms | 0,067 ms |

İki sonuç:

1. **Arka uç seçimi performansla belirlenmiyor**; ikisi desenli sahnede yüzde bir
   içinde. Dahili olan üstelik deseni yanlış çiziyor — aynı satırda %88,8 mürekkep
   basıp ormanı siyah bloğa çeviriyor, QGIS %5,0 basıyor. QGIS varsayılan kalır.
2. **Asıl darboğaz desen dolgusu.** Aynı parsellerde düz dolgu 1,2 ms, desenli
   58 ms — elli kat. 576 parselde §10.1'in 16 ms bütçesi 3,6 kat aşılıyor ve
   bütçe 5 milyon poligon için konmuştu. Bu, GPU tuvalinden önce cevaplanacak
   soru: maliyet parsel başına yeniden döşemede, ve orası CPU'da da düzelebilir.

### Düzeltildi — katman özellikleri paneli form standardına çekildi

- **Etiket girdinin üstüne alındı** (design.md 16.1). Panel 352 px'lik bir sütunda
  sabit 86 px'lik sol etiket kullanıyordu: uzun etiketler iki satıra sarıp satır
  yüksekliğini bozuyor, girdi ile birim kutusu kalanı paylaşamıyor ve ikisi de
  kaydırma çubuğunun altına giriyordu.
- **Renk artık bir alan.** 30 px'lik araç düğmesine iliştirilmiş 40x18 örnek
  yerine, komşusuyla aynı genişlikte, değerin kendisiyle dolu ve onaltılığı
  üstüne yazılı bir alan. Yazı rengi parlaklığa göre seçilir. Biçim yaprağıyla
  değil **boyanarak** yapılır: bu programda tek yaprak vardır.
- **Ön izleme başlığı taşmıyordu artık.** Başlık katman adını tekrar etmiyor —
  pencerenin kendi başlığı zaten yazıyor — ve açıklama notu başlığın altına indi.
- Sütun bütçesi yeniden paylaşıldı: ön izleme ve katman ağacı kısaldı, özellikler
  iki alan yerine dördünü birden gösteriyor.

### Düzeltildi — bildirilen katman bir görseli çağırabiliyor

- **`katmanlar` içindeki `gorsel` alanı okunmuyordu.** Şema onu sayıyordu, C++
  ayrıştırıcısı sessizce atıyordu: `gorsel-cizgi`, `gorsel-isaretci` ve
  `gorsel-dolgu` katmanları resimsiz kalıyor, hiçbir şey çizmiyordu. Artık
  paketin `gorseller` tablosundan çözülüyor ve satır uygulanırken kimliğe
  dönüştürülüyor — böylece bir sınır çizgisi sayılarla yazılıp üzerine çark
  basılabiliyor.
- **Desen dolguları kendi alanını glif rengiyle boyuyordu.** `nokta-desen-dolgu`
  ve `cizgi-desen-dolgu`, `dolgu_renk` ile bütün yüzeyi doldurup glifleri onun
  içinde görünmez bırakıyordu; MPYY'nin orman ve mezarlık gösterimleri düz siyah
  blok olarak çiziliyordu. Alanı boyamak `dolgu` katmanının işidir.

### Eklendi — PostGIS veritabanı desteği

- **`VERİTABANI` komutu (`core.database`).** Bir PostGIS sunucusuna bağlanır;
  `baglan`, `kes`, `tablolar`, `katmanyaz`, `projekaydet`, `projeac`, `projeler`
  ve `projesil` işlemleri. Diğer her şey gibi komut yolundan geçer: farede olan
  betikte de vardır (Article 1.1, 1.2).
- **Katman → mekansal tablo.** `katmanyaz`, katmanı nesne başına bir satır, çizimin
  SRID'iyle bir `geom` sütunu ve tanımlı her öznitelik için bir sütun olacak
  şekilde yazar. QGIS, `ogr2ogr` ve düz `SELECT` okur. Satırlar `COPY` ile yazılır
  ve sonrasında GIST dizini kurulur; tablo eklenmez, **yerine yazılır**.
- **Proje → kayıt.** `projekaydet`, çizimin tamamını `.pcad` baytları olarak
  saklar — stil tablosu, gömülü gösterim resimleri, katman ağacı, ayarlar ve
  koordinat sistemi dâhil. Gidiş-dönüş `content_hash()` ile sınanır
  (`tests/unit/test_database.cpp`).
- **`io::PostgisStore` ve `io::DatabaseService`.** libpqxx 7.9.2 (BSD-3),
  `KENTOS_WITH_POSTGIS` arkasında, commit SHA ile sabitlenmiş. libpqxx başlıkları
  yalnız `src/io/src/postgis.cpp` içinde, açık başlıkta pimpl arkasında (io.md
  R2/P2). Bütün yazma tek işlemde; yarıda kalan bir yazma yoktur (Article 1.6).
- **Dosya > Veritabanı… penceresi (`Ctrl+Shift+D`).** Modsuz; bağlantı alanları,
  mekansal tablolar ve kayıtlı projeler. Her düğme bir `VERİTABANI …` satırı kurup
  komut yolundan çalıştırır; pencereden belgeye başka yol yoktur.
- **Parola iki bağlantı biçiminden de siliniyor.** `command::redact_conninfo`
  tektir ve hem günlüğe yazan komut hem de ekranda gösteren pencere onu kullanır.
  İlk hâli yalnız `password=...` alanını biliyordu; libpq'nun URI biçiminde
  (`postgresql://kullanici:PAROLA@sunucu/db`) parola başka yerdedir ve olduğu
  gibi günlüğe düşerdi.
- **Dört uygulama ayarı:** `veritabani_sunucu`, `veritabani_port`,
  `veritabani_adi`, `veritabani_kullanici`. **Parola ayarı yoktur ve olmayacaktır**
  — ayar dosyası düz metindir. libpq'nun `~/.pgpass` ve `PGPASSWORD` mekanizmaları
  kullanılır; komut satırına yazılan parola günlüğe `password=***` olarak düşer.
- **[Kullanım kılavuzu](docs/komutlar/database.md).** Kurulum, parola, tablo
  şeması, arayüz, betik ve her hata mesajı.
- **Bu sürümde katman yazılır, okunmaz.** Proje için asimetri yok (`projekaydet`
  ile yazılan `projeac` ile aynen döner); katman için var. Tabloyu katman olarak
  okuyan `katmanoku`, Madde 2.9'un "read, write and edit" cümlesinin kalan yarısı
  olarak Faz 1'e kaldı ve kılavuzda gelecek zamanla yazıldı (Madde 11.8).

### Düzeltildi — stil tasarımcısı çöküyordu ve stili katmana yazmıyordu

- **Yarı kurulmuş satır kendi kendini düzenliyordu.** `refresh()`, sembol katmanı
  listesini kurarken satırı önce listeye ekleyip sonra dolduruyordu. Her `setText`,
  `setIcon`, `setData` çağrısı ayrı bir `itemChanged` yayıyor; ilki, satırın yığın
  sırası daha yazılmadan geliyordu. İşleyici bunu kullanıcı düzenlemesi sanıp
  `layers[0]`'ı okuyor, henüz kurulmamış onay kutusunu **kapalı** görüyor ve
  sembolün ilk katmanını sessizce kapatıyordu — Uygula'nın "katman varsayılanına
  döndü" demesinin sebebi buydu. Ardından `refresh()`'i yeniden çağırıyor, oradaki
  `clear()` dış döngünün elindeki satırı siliyor ve döngü **silinmiş belleğe**
  yazmaya devam ediyordu: `make run` segfault'u. ASan raporu `heap-use-after-free`
  olarak doğruladı.
  Satırlar artık listeden **bağımsız** kurulup bütün hâlde ekleniyor, liste kurulum
  boyunca sessiz ve `refresh()` kendi içinden çağrılamıyor.
- **Koruma bayrağı artık iç içe geçiyor.** `loadSelected()` işini `loading_ = false`
  ile bitiriyordu; `refresh()` onu `true` yapıp `loadSelected()`'ı çağırdığı için
  koruma, çağıranın ortasında düşüyordu. Bayrak artık **önceki değeri** geri koyan
  bir kapsam nesnesiyle tutuluyor.
- **Aynı renk düğmesi iki form satırına konuyordu.** `stroke_`, hem "Yazı rengi"
  hem "Çizgi rengi" olarak ekleniyordu; bir widget'ı tek `QFormLayout`'un iki
  gözüne koymak Qt'de tanımsızdır ve iki satır görünürlük konusunda birbiriyle
  çelişiyordu — `yazi-isaretci` katmanında renk düğmesi hiç görünmüyordu. Tek satır
  kaldı; tipe göre adı değişiyor.
- **Seçili öğesi olmayan bir açılır kutu -1 bildirir** ve bu sayı bir satır sonra
  tabloya indis olarak giriyordu. Artık kırpılıyor.

### Değiştirildi — stil tasarımcısının görünümü

- **Üst sekmeler sekmeye benziyor.** Pencere genişliğine yayılmış üç düz gri çubuk,
  ilk kararı taşıyan denetim için kötü bir görünümdü ("orada sekme olduğu bile belli
  değil"). Sekmeler artık etiketleri kadar geniş, kenarlıklı, geometrisinin küçük
  bir simgesini taşıyor ve seçili olan altındaki ön izleme panosuna **yapışıyor**.
- **Ön izleme, gösterildiği genişlikte çiziliyor.** Pencere kurulurken ölçülen 160
  piksellik etikete çizilip sonra esnetilmiyor; etiketin kendi boyut değişimi
  izleniyor.
- **Zemin birimli semboller artık görünüyor.** Ön izleme sabit 40 mm/piksel
  çalışma ölçeğindeydi; MPYY yapılaşma koşulunun 26 m'lik dairesi bu ölçekte 650
  piksel oluyor ve 44 piksellik kutuyu tamamen ıskalıyordu — satır boş çiziliyor ve
  bozuk gibi duruyordu. Ölçek artık yalnız **gevşiyor**: kâğıt birimli her ön
  izleme piksel piksel aynı kaldı.
- **Raf boşken sol taraf üç boş kutu göstermiyor,** tek cümle gösteriyor. Sembol
  katmanı listesi kaydırmadan beş satır alıyor.

### Eklendi — her ölçünün kendi birimi

- **`STİL` komutuna `boyut_birim`, `aralik_birim`, `aralik_y_birim` ve
  `kaydirma_birim`.** Tek bir sembol katmanı iki ayrımı birden taşır: bir il sınırı
  işaretçisinin **çapı** paftaya (2,6 mm), **aralığı** zemine (15 m) aittir.
  Komutun tek bir `birim`i vardı; tasarımcı ise her ölçünün yanında bir birim
  kutusu gösteriyordu ve çıkışta dördünü birine indiriyordu — yani ekranda yazan
  sembol ile çizime yazılan sembol farklı olabiliyordu. `birim`, kendi birimini
  söylemeyen ölçüler için varsayılan olarak duruyor. Tanınmayan bir birim adı
  sessizce `birim`'e düşmez; komut hangi parametrenin hatalı olduğunu adıyla
  söyleyip durur ve çizime dokunmaz.

### Eklendi — ayar sistemi, harita yardımcıları ve zengin nesne yakalama

**Yakalama: çizimde olmayan, ama çizimin ima ettiği noktalar.** Üç yeni mod, üçü de
kadastro ve imar işinin günlük hâli için:

- **UZANTI** (`1024`) — bir kenarın kendi doğrultusu, kenarın ötesinde. Köşe taşı
  kaybolmuş bir sınır, ayakta kalan kenardan yeniden kurulur; istenen nokta kenarın
  üzerinde değildir ve YAKIN oraya erişemez.
- **PARALEL** (`2048`) — önceki noktadan çıkan, bir kenara paralel ışın. Çekme
  mesafesi, yol kenarı ve ifraz hattı böyle çizilir. Önceki noktadan uzaklık
  korunur, yani yönden sonra yazılan ölçülmüş uzunluk aynen oturur.
- **UZATILMIŞ KESİŞİM** (`4096`) — iki kenarın uzatılsalar buluşacakları köşe.
  KESİŞİM burada hiçbir şey bulmaz, çünkü kenarlar gerçekten kesişmez.

Üçü de öncelik sıralamasında **YAKIN'ın da altındadır**: motorun kurduğu bir nokta,
kullanıcının elindeki gerçek bir köşeyi asla kapmaz — bu yüzden üçünü de açık
bırakmak güvenlidir. Üçü de imlecin altında olmayan bir kenardan nokta ürettiği için
`uzantı_çarpanı` tercihi açıklığın kaç katı ötesine bakılacağını söyler; `0` yazılırsa
maskede açık olsalar bile çalışmazlar — motorun `grid_step` ve `polar_step` için zaten
tuttuğu sözleşmenin aynısı. İşaretleri **açık** biçimlerdir (uçları birleşmeyen
şekiller), böylece kurulmuş bir nokta bir bakışta gerçek bir köşe sanılmaz.

`core::line_intersection` ve `core::closest_point_on_line` eklendi; `segment_intersection`
artık birincinin üzerine yazılıyor, yani iki kod yolu bozuk girdide ayrışamaz.

**DÜĞÜM yazıldı ve aynı gün geri alındı.** Bu belge modeli tek noktalı nesne tutamıyor:
açık halka en az iki tepe ister (model.md R9-R12), `İÇEAKTAR` nokta katmanını "bu sürüm
çizgi ve alan okur" diyerek atlıyor ve nokta çizen komut yok. Var olmayan bir şeye oturan
yakalama modu, programın tutmadığı bir sözdür. 9. bit boş bırakıldı ve `test_snap.cpp`'de
sebebini sabitleyen bir vaka var: nokta nesneleri geldiğinde önce o vaka değişir.

**Yirmi iki yeni ayar.** Uygulama kapsamında: yakalama işaretinin boyu, rengi, ipucu ve
uzantı çarpanı; ızgara rengi, ana çizgi rengi ve ikinci eksen adımı; cetvelin
görünürlüğü, kalınlığı ve birimi; ölçek çubuğu, kuzey oku, koordinat göstergesi, imleç
biçimi ve boyu, yakınlaştırma adımı, ters tekerlek; seçim ve vurgu renkleri. Proje
kapsamında üç tane, üçü de belgenin kendi sayılarının nasıl okunacağını söylediği için:
**plan ölçeği** (1:N), **açı birimi** (varsayılan GRAD — Türkiye'de nirengi, poligon ve
aplikasyon hesapları grad ile yürür) ve **alan birimi** (metrekare / dekar / hektar).
`tuval_arkaplanı` da birimini `0xAARRGGBB` olarak bildiriyor artık, yani renk olduğunu
kendisi söylüyor.

**Harita yardımcıları.** Tuvale cetvel (üstte ve solda, 1-2-5 merdivenine oturan
rakamlarla), ölçek çubuğu, kuzey oku ve koordinat göstergesi eklendi. Gösterge, bir
yakalama tuttuğunda **yakalanmış** noktayı yazar: tıklamanın üreteceği koordinat odur.
Nişan imleci artık tam ekran, kısa ya da kapalı olabiliyor; tekerlek adımı yüzde olarak
ayarlanıyor ve ters çevrilebiliyor.

**Ayarlar penceresi (`Düzen > Ayarlar…`, Ctrl+,).** Tamamı ayar kataloğundan üretilir:
satırın adı ayarın birincil adı, alanı bildirilen tipinden, sınırları aralığından,
ipucu özetinden. Kataloğa eklenen bir ayar pencereye kendiliğinden düşer — CLAUDE.md
5.10'un komut listesine koyduğu kuralın aynısı, aynı gerekçeyle: kendi kopyasını taşıyan
bir pencere, kataloğla er geç ayrışacak ikinci bir listedir. Üç sekme üç kapsamdır ve
her sekme kapsamının ne demek olduğunu kullanıcının kendi diliyle yazar. Her satırda
değerin kimin olduğu (`ayarlanmış` / `varsayılan`) ve varsayılana döndüren bir düğme
var; birimi `0xAARRGGBB` olan ayarlar renk seçici alır.

Satır etiketleri **okunmak için** yazılır, yazılmak için değil: `ızgara_adımı`
bildirimi pencerede `Izgara adımı` olur — ayarın kendi adı, alt çizgisi boşluğa
çevrilmiş ve ilk harfi Türkçe kurallarıyla büyütülmüş (`ızgara` → `Izgara`,
`imleç` → `İmleç`; ASCII bir sınıflandırıcı ikisini de yanlış yapar, CLAUDE.md 5.6).
Etiket ayrıca yazılmaz, addan **türetilir** — ikisi ayrışamasın diye. Yazılacak ad ve
makine kimliği ipucunda durur, çünkü pencerenin ikinci bir işi vardır: burada bir
ayarı bulan kişi onu ayarlayan satırı da yazabilmelidir. Arama kutusu etiketi, her
takma adı, kimliği ve açıklamayı birden tarar.

Grup başlıkları da Türkçedir. Kimlikler ASCII olduğu için `core.cizim` başlığı `Cizim`
diye okunurdu; başlıklar bir tablodadır, tıpkı yakalama işaretinin şekli gibi
(`map_canvas.cpp`) — ikisi de üründe verilmiş kararlardır ve türetilecekleri bir
bildirim yoktur. Tabloda satırı olmayan bir grup yine de okunur bir başlık alır.

`ızgara_görünür` ve `ızgara_dikey_adımı` artık birincil adlar; eski `ızgara` ve
`ızgara_adımı_y` takma ad olarak duruyor, yani yazılmış hiçbir betik bozulmadı.

Penceredeki her değişiklik
kapsamına göre `AYAR`, `TERCİH` ya da `MOD` komutu kurup çalıştırır — transkript, günlük
ve yeniden oynatma pencereden yapılanı komut satırından yazılandan ayırt edemez.

### Eklendi — EK-1a arazi kullanımı gösterimleri, ve satırın kendi resim boyutu

**Altı gösterim çizildi:** ORMAN ALANI (üçgen), ZEYTİNLİK (daire), MERA (artı), DOĞAL
KARAKTERİ KORUNACAK ve DOĞAL VE EKOLOJİK YAPISI KORUNACAK (çim demeti SVG), EKOLOJİK
ÖNEME SAHİP (mercan SVG). İlk üçü mevcut işaretçilerle, son üçü elle çizilmiş SVG ile.

- **Satır artık kendi resminin boyutunu bildirebiliyor** (`boyut`, `tarama_boyut`,
  `sembol_boyut`, `tarama_aralik`). Boyut kodda sabitti ve her satır aynısını alıyordu.
  Bu, taranmış bir kırpma için doğru — boyutu bir şey ifade etmez; **çizilmiş** bir
  sembol için yanlış, çünkü orada boyut çizimin parçasıdır.
- **`gorsel-dolgu` artık aralık okuyor.** Bir fırça resmini uç uca döşer, ki taranmış
  bir tarama için doğrudur: kırpma zaten ekin bastığı aralığı içerir. Çizilmiş bir glif
  için yanlıştır — kendi kutusunu doldurur, uç uca döşenince desen katı bir hasıra
  döner. Resim artık daha büyük saydam bir hücreye yerleştiriliyor ve döşenen o hücre.

**QGIS çeviri hatası:** `Cross` ↔ `Cross2` ters eşlenmişti. QGIS'te `Cross` dik artı,
`Cross2` döndürülmüş çarpı; adların benzerliğine göre eşlemek MERA ALANI'nın artı
ızgarasını çarpı ızgarası olarak çizdiriyordu.

Rafta şu an: 419 resimli, **30 vektör yığın**, 28 düz.

### Düzeltildi — stil düzenleyicisinde bir gösterim seçince form eksik kalıyordu

Galeriden bir satır seçilince form "bozuluyordu": `Görsel işaretçi` katmanı yalnız Boyut
ve Saydamlık gösteriyor, Açı satırı hiç görünmüyordu.

Sebep, iki listenin birbirinden habersiz olması. Düzenleyici hangi satırı göstereceğine
yanlarında elle yazılmış bir tipler tablosundan karar veriyor; boyayıcı hangi özelliği
okuyacağına kendi `switch`inden. İkisini eşleşik tutan hiçbir şey yoktu ve ayrıştılar:
`gorsel-dolgu` fırçasını `angle_udeg` ile döndürüyor ve pencerede o satır yoktu — yani
**boyayıcının okuduğu bir değere kullanıcı ne bakabiliyor ne değiştirebiliyordu.**

- Tablo, boyayıcının gerçekten ne okuduğuna **bakılarak** düzeltildi: `gorsel-dolgu`,
  `gorsel-cizgi`, `gorsel-isaretci` ve `merkez-isaretci` artık açı satırını görüyor.
- **`faz`** satırı da eklendi. Motora geçen tur eklenmişti ama pencerede yoktu.
- **`ci-gate-designer.sh`** eklendi: boyayıcının her katman tipi için okuduğu özellik,
  düzenleyicinin o özelliği yöneten listesinde de olmak zorunda. Kapının yakaladığı, bir
  tip listeden geçici olarak çıkarılıp doğrulandı.

### Düzeltildi — arayüz vektör paketi yüklemiyordu, hep resim çiziyordu

Bir önceki turda motor tarafını düzelttim ve "476 satırın 464'ünde iki motor aynı
çiziyor" dedim. Ölçüm doğruydu ama **yanlış soruya** cevap veriyordu: ikisi de RESİM
çiziyordu. Kullanıcının gördüğü buydu.

Sebep: `core.stil.kutuphane` tek bir paket adı alıyor ve arayüz yalnız onu yüklüyordu.
O paket yönetmeliğin kendi paketi — **476 satırının 439'u resimli, sıfırı vektör.**
Elle çizdiğim vektör satırları ayrı bir pakette duruyordu ve rafa hiç girmiyordu.

- **`core.stil.vektor` ayarı eklendi.** Arayüz iki paketi SIRAYLA yüklüyor: önce
  yönetmeliğin resimli paketi, sonra vektör paketi. Raf her kimlikten bir satır tutar
  ve aynı kimliği yeniden bildiren paket öncekinin yerine geçer — yani vektörü çizilmiş
  bir gösterim vektör olarak, çizilmemiş olan ekin resmiyle görünür ve **hiçbiri
  eksilmez**.
- Tek ayara iki yol sığdırmayı denedim ve **olmadı**: bir metin ayarı 48 bayt alır, iki
  yol 82 bayt. `text_value` sessizce boş bir değer döndürüyor, ayar da "evet/hayır" tipi
  sanılıp kayıt sırasında reddediliyordu. `builtin_setting_failures()` bunu söyledi.
  Ayrı ayar doğru çözüm.

Ölçüldü: açılışta rafta **422 resimli, 27 vektör yığın, 28 düz** satır var. Vektörlerin
azlığı beklenen — 476 satırın 12'si çizildi.

### Düzeltildi — QGIS motoru raster gösterimleri sessizce atlıyordu

Semboloji "komple bozuk, sadece çizgi çiziyor ve sadece rengi değişiyor" hâline geldi.
Sebebi: `QgisBackend::handles()` **çizemediklerini** listeliyor, gerisini "evet"
sayıyordu. Yani hiç öğretilmemiş her katman tipi sessizce sahipleniliyor ve sessizce
atlanıyordu. Üç raster tipi — `gorsel-cizgi`, `gorsel-dolgu`, `gorsel-isaretci` — o
kapıdan çıktı, ki MPYY'nin yayımladığının neredeyse tamamı bunlar: taramalı bir lekesi
düz renk, yayımlanmış bir çizgi tipi düpedüz çizgi olarak çıkıyordu.

Liste artık **beyaz liste**: motorun çizebildikleri sayılıyor, gerisi `default`'a düşüp
reddediliyor ve kare onu çizebilen motora gidiyor. Bir beyaz liste bu şekilde
başarısız olamaz — çevirisi yazılmamış bir tip sahiplenilemez.

Ölçüldü: paketin **476 satırının 464'ünde iki motor birebir aynı mürekkebi koyuyor**,
ayrılan satır yok.

`ci-gate-backends.sh` genişletildi: `handles()`'ın `default` dalının **reddetmesi**
şart. Kapının yakaladığı, dal geçici olarak "kabul et"e çevrilip doğrulandı.

### Düzeltildi — QGIS motoru bindirmeyi hiç çizmiyordu

Izgara, cetvel, ölçek çubuğu, kuzey oku, yakalama işareti, nişan imleci, seçim kutusu
ve çizimin kendi yazıları **tuvalden tamamen kayboldu** — QGIS motoru varsayılan olduğu
anda. Çizim duruyordu, etrafındaki her şey gitmişti.

Sebep: `QgisBackend::render`, bindirmeden yalnız arkaplan rengini okuyordu. Bunları
yazarken atladım ve hiçbir şey fark etmedi, çünkü **bindirmenin hiç testi yoktu**.

- `paint_frame_aids()` ortak fonksiyon oldu ve iki motor da onu çağırıyor. İkinci bir
  kopya, birbiriyle uyumlu tutulacak ikinci bir liste demekti (5.10'un itirazı).
- **`ci-gate-backends.sh`** eklendi: `render::Backend` uygulayan her dosyayı **bularak**
  (listeleyerek değil — listeye eklenmeyi unutulan bir motor, kimsenin denetlemediği bir
  motordur) bindirmeyi çizip çizmediğine bakıyor. Kapının gerçekten yakaladığı, çağrı
  geçici olarak silinip doğrulandı.

Testte değil kapıda, çünkü `/tests` Qt bağlamıyor ve bir arka uç tanımı gereği Qt'dir
(Madde 3.4 `kentos_render`'ı Qt'siz tutuyor, bu yüzden iki motor da `/src/app` içinde).

### Eklendi — işaretçi çizgide FAZ

`faz`, çizgi boyunca ilk işaretçiye kadar olan mesafe. Verilmezse aralığın yarısı, ki
eski davranış budur — yazılmış hiçbir çizim başka türlü çizilmiyor.

Bunun ne işe yaradığı MPYY'nin kendi ekinin ilk sayfasında iki kez görünüyor.
**ETAPLAMA SINIRI** dolu ve boş daireyi sırayla dizer: aynı aralıkta iki işaretçi
çizgisi, ikincisi yarım adım ileride. **ÜLKE SINIRI** kalın bir çubuğun iki **ucuna**
dik birer tik koyar: aynı aralıkta iki tarak çizgisi, biri çubuğun başında öteki
sonunda. Faz olmadan iki katman da aynı yere düşüyor ve sembol söylediğinin yarısını
kaybediyordu — bir önceki turda ikisi de "eksik" diye işaretlenmişti, artık çiziliyorlar.

Faz `SymbolLayer`'da, dosyada (`kBlkSymbolLayerPhase`, isteğe bağlı blok — faz
kullanmayan bir çizim bayt birebir eskisi gibi yazılıyor), `STİL faz=` parametresinde ve
katalog satırında.

İki kayıp daha bulundu ve kapatıldı: faz `pass_of`'a hiç ulaşmıyordu (biçimlendirme
sonrası satır kaydığı için düzenlemem tutmamış), ve QGIS işaretçisinde dolgusu sıfır
olan bir daire çizgi rengiyle doldurulyordu — dolu/boş ayrımı kayboluyordu. `STİL`'in
`has_pictures` denetimi de `has_symbol` oldu: bildirilmiş katmanı olup resmi olmayan bir
satır düz renk yoluna düşüyordu.

### Eklendi — MPYY yapılaşma koşulu gösterimi, parselin kendi sayılarıyla

Yönetmeliğin bastığı gösterim: içinden yatay bir çizgi geçen çember, üstte **kat alanı
katsayısı**, altta **taban alanı katsayısı**. İkisi de parselin özniteliğidir, yani aynı
sembolü taşıyan iki parsel farklı sayılar gösterir.

- **`ETİKET kaydirma=`** eklendi. Bu olmadan iki sayı da nesnenin ortasına, yani
  aralarındaki çizginin üstüne düşüyordu — resmi çekince görülüyor. Her sayının kendi
  `ETİKET` satırı ve kendi kaydırması var; sabit kelimeleri yazan iki `yazi-isaretci`
  katmanının kaydırma taşımasıyla aynı biçim.
- **İş bilinçli olarak ikiye bölünüyor.** Sembol çemberi ve çizgiyi çizer — bunlar
  hiçbir parsel hakkında bir şey söylemez, bu yüzden kaç parsel taşırsa taşısın stil
  sütununda **tek kayıttır** (testte doğrulanıyor). `ETİKET` sayıları yazar ve her sayı
  sıradan bir yazı nesnesi olur: taşınır, yeniden stillenir, kendi katmanında kapatılır,
  `.pcad` ve DXF'e olduğu gibi gider.
- **Sembolün kendisi özniteliği okumuyor** ve okumayacak: `.claude/model.md` R29
  "öznitelik sütunları çerçeve yolunda asla okunmaz" ve P7 "çerçeve yolunda asla ifade
  değerlendirilmez". Kare başına nesne başına bir sütun araması, 16 ms bütçesinin içine
  bir tablo araması koymak demektir. Karşılığında kaybedilen şey söylenmelidir:
  **etiket, sonradan değişen bir özniteliği takip etmez**; komutu yeniden çalıştırmak
  onları tazeler.

### Düzeltildi — veri paketleri dağıtımla birlikte gitmiyordu

Kurulum yalnız ikili dosyayı kuruyordu. MPYY gösterimleri, TM3 dilim tablosu ve `/data`
altındaki her şey — yani bu programı bir çizim düzenleyicisi değil bir Türkiye planlama
programı yapan şeyler — derlendiği makine dışında **hiçbir yerde** yoktu. Üstelik
`core.stil.kutuphane` ayarının varsayılanı `data/catalogs/...` diye **göreli** bir yol
ve göreli yol çalışma dizinine göre çözülür; yani tam da bulunmayacağı yere.

- `install(DIRECTORY data/ ...)` eklendi; paket `share/piricad/data` altına gidiyor.
- **`app::data_root()`** sırayla bakıyor: `$KENTOS_DATA`, `<exe>/../share/piricad/data`,
  `<exe>/data`, sonra yapılandırıldığı kaynak ağacı. Bir dizin ancak içinde gerçekten
  `catalogs` varsa kabul ediliyor — yarım kurulmuş bir ağacı bulmuş saymak, taze bir
  makinede sessizce boş raf demektir. Derleme zamanında gömülü bir yol değil, çünkü
  paket başka makinede kurulur, taşınır, taşınabilir dizinden çalıştırılır.
- Kurulup ilgisiz bir dizinden çalıştırılarak denendi.

### Eklendi — QGIS semboloji motoru bağlandı

Madde 2.7 ve 5.16: olgun, mükemmel, çok platformlu bir kütüphane kullanılır, yeniden
yazılmaz. Semboloji motoru tam olarak böyle bir şeydir ve QGIS'inki bu alandaki en iyi
özgür motordur — gerçek yerleşim kurallarıyla işaretçi çizgileri, kendi kaydırma ve
dönüklüğü olan çizgi/nokta desen dolguları, parametre yerine koymalı SVG semboller,
gradyan, shapeburst. `painter_backend.cpp`'deki elle yazılmış hâl bunların hepsinde
daha kötü.

**Bağlamama gerekçesi ölçülmeden yazılmıştı; ölçtüm:** `libqgis_core.so` 45 MB ve 246
paylaşımlı nesne, `QgsApplication::initQgis()` **soğuk 517 ms, sıcak 44 ms**. Madde
7'nin iki saniyelik açılış bütçesinin rahat içinde — eski itirazın "bunu kırar" dediği
sayı buydu. Lisans da engel değil: QGIS **GPL-2.0-or-later**, GPLv3 ile uyumlu (yalnız
GPL-2.0-**only** olsaydı Madde 5.5 gereği reddedilirdi).

- **`app::QgisBackend`**, `render::Backend` arayüzünün arkasında. Dikiş orası ve başka
  yer değil: Madde 3.4 `kentos_render`'ı Qt'siz tutuyor, QGIS ise Qt — bu yüzden dosya
  `QPainter` arka ucunun yanında `/src/app` içinde, tam da Madde 8.5'in tarif ettiği
  gibi. Kabuğun altındaki hiçbir katman QGIS'in var olduğunu öğrenmiyor.
- **Çizim listesi sözleşme olarak kalıyor.** Her `PassStyle`, aynı anlama gelen QGIS
  sembol katmanına çevriliyor; geometri, `QPainter` arka ucunun aldığı ekran uzayı
  yığınlarının aynısı. İki motor aynı belgeyi aynı sayılardan çiziyor — karşılaştırmayı
  mümkün kılan şey bu. `KENTOS_BACKEND=dahili` ile yan yana bakılabiliyor.
- **Sistemden alınıyor, vcpkg'den değil:** QGIS altında GDAL, PROJ, GEOS ve SpatiaLite
  olan bir masaüstü yığını; onu manifestten kurmak QGIS'i kurmak olurdu.
- **QGIS başlıkları `SYSTEM` olarak dâhil ediliyor.** Bu bir susturma değil (CLAUDE.md
  5.14): derleyiciye hangi başlıkların *bizim* olduğunu söylüyor. QGIS'in kendi
  başlıklarındaki dönüşüm ve gölgeleme uyarıları bu depodaki hiçbir düzenlemeyle
  giderilemez, ve bir uyarı duvarı kendi kodumuzdaki gerçek bir bulgunun kaydırılıp
  geçilme biçimidir.

Çeviride bir hata çıktı ve ölçümle yakalandı: kesik deseni sayıları çizgi kalınlığının
katıdır, QGIS'in `setCustomDashVector`'ü ise verildiği birimde uzunluk ister. Ham
sayıları piksel diye vermek sekiz kalınlıklık çizgiyi sekiz piksel çiziyordu — iki motor
yan yana konunca görülüyor.

### Eklendi — gösterimler SVG olabiliyor

- **`ImageStore` SVG tanıyor**, uzantıdan değil imzadan: `<svg` kökü aranıyor, prolog ve
  yorum toleranslı, ilk 1 KB'la sınırlı (düşmanca bir dosya megabaytlarca yorumla
  gelmesin).
- **Boyayıcı SVG'yi `QSvgRenderer` ile çiziyor** — QGIS'in de SVG için kullandığı motor.
  **Çizileceği boyutta** rasterleştiriliyor ve önbellek anahtarı o boyutu da içeriyor:
  rasterin tek çözünürlükte açılıp her yakınlaştırmada yeniden örneklenmesi, mevzuatın
  keskin çizdiği çizgiyi her seferinde biraz daha yumuşatan şeydi.
- **`data/catalogs/mpyy-vektor/` paketi kuruldu.** Çıkarılan pakete karıştırılmadı:
  elle çizilen şey kaynaktan yeniden üretilemez ve `ci-gate-mpyy` bunu haklı olarak
  denetliyor. İzin belgesi satırı **türetilmiş eser** diyor, her satır `belirsiz: true`
  ve `cizim-yorumu` ile işaretli, ve uzman onayı olmadan pakete bir plan uygulanmıyor.
  İlk dört sembol çizildi ve tuvalde doğrulandı.

**Otomatik izleme denendi ve bırakıldı.** Tarama karolarını Radon izdüşümü ve bağlı
bileşen analiziyle okuyup açı/aralık/kalınlık çıkarmayı denedim; okumayı geri çizip
aslıyla yan yana koyunca **altı örnekten ikisi** doğru çıktı. Kalınlık 104 piksel
okunup siyah blok çiziliyor, JPEG'de parçalanmış daire konturu 2×1 glif sanılıyor,
seyrek bir sembol 3 piksellik kafes okunuyor. Aile sınıflandırması (tarama / nokta
deseni) altıda beş doğru ama **sayılar katalog kalitesinde değil** — ve yanlış bir açı
imzalanan bir plana yanlış gösterim yazar. Çıkarıcının kendi doktrini uydurmayı
yasaklıyor; bu yüzden semboller **çiziliyor**, izlenmiyor.

### Eklendi — çizgi tipi artık bir desen, resim değil

MPYY il sınırını bir çizgi, bir boşluk, bir nokta ve bir boşluk olarak basar. Bu dört
sayıdır; program onu JPEG kırpması olarak taşıyordu ve bir resmin veremediği her şeyi
kaybediyordu — yeniden renklendirilemez, yeniden ölçeklenirken yeniden örneklenir,
DWG/DXF/GML'e çizgi tipi olarak yazılamaz ve hepsinden önemlisi **köşe dönemez**.
Damgalanan resim katı bir dikdörtgendir; bir kenarın açısına döner ve her kıvrımın
dışında kama biçiminde bir boşluk bırakır.

- **`core::DashStore`.** Çizimin taşıdığı çizgi tipleri, içerikle tekilleştirilmiş —
  `ImageStore` ile birebir aynı biçim ve aynı gerekçe: desenler **çizimin içinde
  gider**. `Appearance.dash` zaten "desen tablosuna indeks" diye bildirilmişti; eksik
  olan tablonun kendisiydi. Yalnız katalog paketinde yaşayan bir tablo, paketin kurulu
  olmadığı bir bilgisayarda paftanın başka çizilmesi demekti — pafta hukuki bir belge.
- **Birim, çizginin kendi kalınlığıdır.** Desen kalınlığın katı olarak saklanır; bu, tek
  bir tanımın 0,2 mm'de de 1,0 mm'de de doğru kalmasını sağlar ve `QPen::setDashPattern`
  zaten bu birimi ister. Ekin bastığı örnek de bunu söyler.
- **`STİL desen=` parametresi bağlandı.** Bildirilmiş ama kullanılmıyordu.
  `desen="8 1 1 1"` kesik-noktalı, `desen=sürekli` düz. Tek sayıda parça, sekizden çok
  parça ve sayı olmayan bir sözcük **reddedilir** — hiçbiri sessizce düz çizgiye
  dönmez, çünkü düz çizilen bir sınır paftada farklı bir hukuki beyandır.
- **Dosya formatına `kBlkDashes` bloğu.** İsteğe bağlı olduğu için sürüm yükseltmesi
  değil (io.md R10): çizgi tipleri var olmadan yazılmış her dosya boş tabloyla okunur ve
  içindeki her çizgi düz kalır — zaten öyleydi.
- **Desenli çizgi düz uçla çizilir.** Qt ucu her çizgi parçasına uygular; yuvarlak uçta
  her parça iki ucundan yarım kalınlık uzar ve bir kalınlık genişliğindeki boşluk tamamen
  kapanır. Yayımlanmış kesik-noktalı bir sınır **düz çizgi olarak** çıkıyordu. Bildirilen
  uç biçimi çizginin iki gerçek ucunu anlatır, içindeki her parçayı değil.

Beş yeni test: desenin çizime yazılması, tekilleştirme, `sürekli`, bozuk desenin
reddi ve çizime dokunmaması, dosya gidiş-dönüşü ve desensiz eski dosyanın okunması.

### Düzeltildi — damgalanan gösterimlerde kâğıt lekesi ve köşe deliği

- **JPEG'in kâğıdı artık çözümleme anında saydamlaşıyor.** Damgalar çarpma kipiyle
  çiziliyordu; çarpma beyazı olduğu gibi bırakır ama JPEG'in beyazı 255 değil ~250'dir
  ve her çizginin çevresinde halkalanma vardır — ekrana ulaşan şey, her damganın altında
  **soluk gri bir kutu** oldu. Alfa artık pikselin kendisinden geliyor:
  `alfa = 255 - min(r,g,b)`. **Süreklidir**, yani hangi grinin mürekkep olduğuna dair bir
  karar vermez — eşiklemeye yapılan haklı itiraz buydu. En küçük kanala bakması, doygun
  bir rengin opak kalmasını sağlar: MPYY'nin kırmızı sınır noktaları yarı-koyu sayılmak
  yerine tam güçte kırmızı kalır. Damga artık yalnız koyulaştırmıyor, **boyuyor** — koyu
  bir dolgu üzerine beyaz bir glif bunu gerektirir.
- **Damgalar halkanın tamamı boyunca, yay uzunluğuyla yürüyor.** Önceki hâl her kenarı
  ayrı yürüyor, iki ucunda yarım damgalık pay bırakıyor ve fazı her köşede sıfırlıyordu.
  Üçü de tek başına savunulabilirdi; birlikte, kullanıcının bildirdiği resmi ürettiler:
  **her parselin her köşesinde bir delik**, kenardan kenara değişen bir aralık, ve
  kenarları bir damgadan kısa olan bir sınırda **hiçbir şey**. Artık adım bütün koşu için
  bir kez seçiliyor, köşe yürüyüş için özel bir yer değil.

**Kalan kusur bitmap'in kendisindedir.** Altmış piksel eninde katı bir dikdörtgen köşe
dönemez; damga bir kenarın açısına göre döner ve dönüşün dışında bir kama boşluk kalır.
QGIS'in raster çizgi sembollerinde de aynı sınır vardır — QGIS kesik-noktalı çizgi için
raster kullanmaz, vektör kesik deseni kullanır ve köşeyi gerçek bir birleşimle döner.
`Appearance.dash` alanı çizimde **vardır ve bağlı değildir**: boyayıcı, ölçülü segment
uzunlukları yerine sabit bir `Qt::PenStyle` dizisini vekil olarak kullanıyor. Eksik olan
yarı budur.

### Düzeltildi — MPYY gösterimleri artık mevzuatın bastığı gibi çiziliyor

- **Kâğıt milimetresi bir ekran pikseli sayılıyordu.** `render/scene.cpp` içindeki
  `kPixelsPerPaperMm = 1.0` — dosyanın kendi yorumunda "PLACEHOLDER" diye
  işaretliydi. Mevzuatın 8 mm bastığı bir gösterim tuvale **8 piksel** olarak
  geliyordu; 96 dpi'da 8 mm otuz pikseldir. Kâğıt birimli her şey yaklaşık dört kat
  küçüktü. Çizgi kalınlığı da aynı yerden geliyordu: 0,5 mm'lik bir sınır yarım
  piksel istiyor, tabandan 1'e yuvarlanıyordu — yönetmeliğin 0,2 / 0,5 / 1,0 mm
  ayrımı tek bir saç teline çöküyordu. Çözünürlük artık `SceneOptions`'tan geliyor
  ve `MapCanvas` onu bulunduğu ekrandan okuyor; QGIS de render bağlamının DPI'ını
  aynı şekilde kullanır.
- **Tarama karosunun beyaz kâğıdı, satırın dolgu rengini siliyordu.**
  `drawRasterFill`, diğer iki raster yolunun (`drawRasterAlong`,
  `drawRasterCentres`) kullandığı çarpma kipini kullanmıyor, düz doku fırçasıyla
  boyuyordu. MPYY görselleri JPEG olduğu için alfası yoktur ve opak beyaz üstünde
  gelir; sonuç, MEVCUT KONUT ALANI'nın kahverengi yerine bembeyaz çıkmasıydı.
  **Paketin 476 satırından 277'si bu yoldan geçiyor.**

### Değiştirildi — ön izleme sembolü kutuya sığdırıyor

- **Tek yakınlaştırma, iki birim ailesine birden.** Ön izleme yalnız küçültür: kutuya
  zaten sığan bir sembol tuvaldeki ölçeğiyle çizilir, ki bir örneklik ancak o zaman
  çizim hakkında bir söz olur. İkisine birden, çünkü kâğıt ve zemin ölçülerini
  karıştıran bir sembolün oranları yalnız birini küçültmekle bozulurdu.
- **Görselin en-boy oranı hesaba katılıyor.** Damgalanan bir çizgi tipi bildirdiği
  boyut kadar değil, kendi resmi kadar geniştir (MPYY sınır görselleri iki-bire
  yakın) ve bir damganın parçaya sığıp sığmadığına **eni** karar verir;
  `render::distribute_along`, damgadan kısa bir kenara hiç damga koymaz. Oran
  `QImageReader` ile yalnız başlıktan okunur, çözme maliyeti yoktur.
- **Dar bir örneklikte zikzak düzleşiyor.** Zikzak, desenin köşede ne yaptığını
  göstermek için vardır ve büyük ön izlemede yerini hak eder; 44 piksellik bir liste
  simgesinde ise koşuyu üç güdük parçaya bölüyor ve hiçbiri yayımlanmış bir çizgi
  tipinin tek damgasını taşıyamıyordu — simge boş çıkıyordu. 120 pikselin altında
  örneklik köşeyi değil deseni gösteriyor.

### Düzeltildi — iki yüzlü parsel artık delikli parsel sanılmıyor

- **Çok parçalı yüz `MULTIPOLYGON` olarak yazılıyor.** Nesnenin halkaları düz bir
  listedir ve onları gruplayan şey rolleridir. Kodun ilk hâli listeyi "ilk halka
  sınır, gerisi delik" diye okuyordu; **yolla ikiye bölünmüş bir parselin ikinci
  yüzü delik oluyordu**. Sonuç, alanları yanlış olan ve buna rağmen `ST_IsValid`
  dâhil hiçbir denetimin şikâyet etmediği bir tablo. Böyle bir parsel KentOSCad'e
  `İÇEAKTAR` ile, TKGM'den gelen bir GeoPackage'ın `MULTIPOLYGON` kaydı olarak
  girer; yani hata canlıydı. Açık halkalar için `MULTILINESTRING` de aynı anda
  eklendi.
- **`io::entity_ewkb` açık başlığa çıkarıldı.** Sunucu gerektirmeyen saf aritmetik
  olduğu için PostGIS kapalı derlenmiş bir yapıda da derleniyor ve sınanıyor —
  doğru olması gereken parça, çalışan bir veritabanı isteyen bir testle
  korunamaz.
- **`KENTOS_WITH_POSTGIS=OFF` yapısı derlenmiyordu.** `postgis.cpp` koşulsuz
  olarak `<pqxx/pqxx>` içeriyordu. Artık `vector.cpp`'nin GDAL için kullandığı
  kalıpta: bağlantı yarısı korumalı, kodlama yarısı her yapıda derleniyor,
  `PostgisStore` her giriş noktasında desteğin kapalı olduğunu söylüyor.

### Düzeltildi — tırnak içindeki değer artık gerçekten değişmez

- **Tırnaklı bir değer ikinci kez ayrıştırılıyordu.** `hedef="host=localhost
  dbname=x"` yazıldığında ayrıştırıcı, tırnakların kaybolduğunu unutup değeri
  kendi `=` işaretinden yeniden bölüyor ve komut "metin bekliyor" diyerek
  reddediyordu. Aynı hata `=` içeren her yol, katman adı ve biçim dizesini de
  vururdu. Tek dilbilgisi (CLAUDE.md 5.11) artık tırnaklı bir değeri **harfi
  harfine** alıyor: kendi `=` işaretinden bölünmez, virgül taşıyor diye koordinat
  sanılmaz, sayıya benziyor diye sayıya çevrilmez.
- **Tırnak sınırlar, tür değiştirmez.** `ÖLÇEK "500"` artık `ÖLÇEK 500` ile aynı
  şeyi, `gorunur="evet"` de `gorunur=evet` ile aynı şeyi yapıyor. Sayı,
  ayrıştırıcının kendi ifade değerlendiricisiyle okunuyor — ikinci bir sayı
  ayrıştırması eklenmedi.

### Eklendi — dosya açma ve kaydetme

- **`kentos_io` modülü.** Biçim okuma-yazmanın tamamı `/src/io` altında; Qt yok,
  GDAL başlıkları yalnız `.cpp` dosyalarında, dışa açılan başlıklarda yalnız core
  ve command tipleri (`.claude/io.md` R1–R3, P2).
- **Yerel proje biçimi `.pcad`.** Sütunlu (SoA), 8 bayt hizalı, `u64` konumla
  adreslenen, belleğe eşlenebilir tek dosya. Koordinatların tamamı `int64`
  milimetre; dosyada hiçbir yerde ondalıklı sayı yok (io.md R5, R7; model.md R21).
- **Sürümleme ve ileri uyumluluk.** İlk 32 baytta imza, yazan sürüm ve gereken en
  düşük okuyucu sürümü. Tanınmayan blok uzunluğuna bakılarak atlanır ve ölümcül
  değildir; okunamayacak kadar yeni bir dosya, gereken sürümü söyleyerek
  reddedilir ve yarım yüklenmez (io.md R8, R9, R10).
- **Kalıcı kimlikler korunuyor.** Nesne ve katman anahtarları, silinmiş nesnelerin
  satırları dahil dosyaya yazılır ve okunurken birebir doğrulanır. Anahtar
  boşlukları sıkıştırılmaz: emekli bir anahtarın başka bir parsele verilmesi
  "bu parsel hangisiydi?" sorusunu cevapsız bırakırdı (model.md R4, P5).
- **Proje ayarları dosyayla gidiyor.** Proje kapsamlı ayarlar `.pcad` içinde
  taşınır ve belgenin parmak izinin parçasıdır; uygulama ve oturum kapsamlıları
  dosyaya girmez (model.md R39, R40).
- **Kesintiye dayanıklı kaydetme.** Önce yanına geçici dosya yazılır, ancak son
  bayt diske indikten sonra yerine konur. Yarıda kesilen bir kaydetme bir önceki
  kaydı bozmaz.
- **Güvenilmeyen girdi savunması.** Dosyadaki her uzunluk, konum, sayaç ve çapraz
  dizin gerçek dosya boyutuna karşı denetlenir; taşan toplama, çakışan blok,
  yuva dışı gösterim ve sıra dışı anahtar reddedilir (io.md R18, P6).
- **Beş dosya komutu.** `AÇ`, `KAYDET`, `FARKLIKAYDET`, `İÇEAKTAR`, `DIŞAAKTAR` —
  `Registry`'de kayıtlı, başsız çalışabilen, arayüz-komut satırı-betik eşitliği
  sınanan komutlar. Dosya seçme penceresi yalnız argümanı toplar (Article 1.2).
- **GDAL/OGR ile DXF ve GeoPackage.** `KENTOS_WITH_GDAL` arkasında; sürücüler
  `cmake/KentOSCadGdalDrivers.cmake` içindeki açık izin listesinden gelir, tam
  sürücü kümesi asla açılmaz (io.md P7). `/vsicurl` gibi sanal dosya sistemi
  yolları reddedilir (P14). Kapalıyken komutlar hangi paketin gerektiğini söyler,
  sessizce başarılı olmaz.
- **Etiketsiz koordinat reddediliyor.** Koordinat sistemi bildirmeyen veri kümesi
  içe aktarılmaz; DXF'in yeri olmadığı için `.prj` yardımcı dosyası yazılır ve
  okunur (io.md R20).
- **libFuzzer koşumları ve tohum korpusu.** `kentos_fuzz_proje` ve
  `kentos_fuzz_dxf`, ASan + UBSan altında; tohumlar Clang olmayan yapılarda da
  `kentos_tests` tarafından aynı okuyucudan geçirilir (io.md R19, CLAUDE.md 6.7).
- **Belgeler.** `docs/veri/proje-dosyasi.md`, `docs/veri/dis-formatlar.md` ve beş
  komut sayfası; sözlük ve sorun giderme genişletildi.

### Eklendi — seçim ve nesne yakalama motoru

- **Nesne yakalama.** Uç nokta, orta nokta, merkez, kesişim, dik ayak, en yakın,
  ızgara ve kutupsal; hepsi `core.yakalama.modlar` bit maskesinden sürülüyor.
  `kentos_cad/core/snap.hpp` istemciyi bilmez: bir nişan `Point2`, bir tolerans
  mesafedir.
- **Yardımlar tek yolda uygulanıyor.** `co_await ctx.point(...)` ne fareyi ne
  betiği tanır; yakalama, dik mod ve kutupsal izleme `InputAwaiter` içinde,
  değerin kaynağı sorulmadan çalışır (kentoscad.md §2.4, `CLAUDE.md` 1.2).
- **Seçim.** `EntityKey` kümesi, oturum kapsamında; `content_hash()`'e dokunmaz,
  geri alınmaz, belge değişikliği olarak günlüğe girmez (`model.md` R43, R44).
- **`SEÇ` komutu.** Tümü, kimlik, pencere, kesen, yön okuyan kutu ve tek nokta;
  ekle/çıkar/tersine işlemleri. Fareyle çizilen kutu ile komut satırına yazılan
  `SEÇ KUTU` aynı komuttur.
- **Tuvalde geri bildirim.** Her yakalama modu için ayrı işaret ve adı, seçili
  nesne vurgusu, pencere/kesen kutusunun ayırt edilebilir çerçevesi.
- **Kısayollar.** **F3** nesne yakalama, **F8** dik mod, **F9** ızgaraya yakalama,
  **Ctrl+A** / **Ctrl+Shift+A** tümünü seç / seçimi temizle. Her biri `MOD` veya
  `SEÇ` gönderir; ikinci bir mod listesi yok.

### Düzeltildi

- **`SİL` artık kalıcı anahtar konuşuyor.** `nesneler` parametresi yoğun slot
  yerine `EntityKey` alıyor (`model.md` R5/P4): günlüğe giren bir slot, tekrar
  oynatıldığında komşu parsele düşerdi. Kimlikler `1`'den başlar. Argümansız
  `SİL` etkin seçimi siler.
- **Liste parametreleri artık birikiyor.** `SİL nesneler=1 nesneler=2` iki nesneyi
  siliyor; önceden ikinci değer birinciyi sessizce eziyordu (`command.md` P15).
- **İki elemanlı JSON dizisi.** `{"nesneler": [1, 2]}` artık kimlik çifti olarak
  okunuyor; ayrımı komut bildirimi yapıyor, JSON'un biçimi değil.

### Eklendi — Faz 0 iskeleti

- **Komut veri yolu.** `Bus` → doğrulama → `Transaction` → `Journal`. Arayüz,
  komut satırı, betik, AI ve toplu iş eşit istemciler; hiçbirinin ayrıcalığı yok.
- **`Task<T>` coroutine tipi.** Etkileşimli komutlar elle yazılmış durum makinesi
  değil, düz coroutine akışı (kentoscad.md §2.4).
- **Tek kaynaklı komut tanımı.** `KENTOS_COMMAND` makrosu ve `Registry`; komut
  satırı yardımı, AI araç şeması ve dokümantasyon buradan üretiliyor.
- **Tek gramer.** `kentos_cad/command/parser.hpp` hem komut satırını hem betiği
  ayrıştırır: mutlak, göreli (`@50,30`), kutupsal (`@100<45`) ve satır içi ifade
  (`@(100*3),0`).
- **Sabit-nokta koordinat.** İç depoda `int64` milimetre; platformlar arası
  bit-birebir sonuç.
- **Geri alma / yineleme.** Ters-işlem günlüğü; bir komut = bir adım, bir betik
  bloğu = tek birleşik adım.
- **Komut günlüğü.** JSONL, ayrı thread'de asenkron yazım, oynatılabilir.
- **Sekiz çekirdek komut.** ÇİZGİ, SİL, KATMAN, YAKINLAŞ, GERİAL, YİNELE, BETİK,
  YARDIM.
- **Qt 6 kabuğu.** Harita canvas'ı, komut satırı widget'ı, katman paneli, komut
  günlüğü paneli, transkript, durum çubuğu.
- **Faz 0 kanıtı.** Aynı `ÇİZGİ` komutu arayüzden, komut satırından ve JSON
  betiğinden çalıştırıldığında tıpatıp aynı dokümanı ve tıpatıp aynı günlüğü
  üretiyor (kentoscad.md §16.5). `tests/unit/test_proof.cpp`.

### Eklendi — stil / gösterim motoru ve MPYY gösterim paketi

- **Stil kademesi çalışır hâlde.** `model.md` R13–R19'un tarif ettiği çözüm artık
  gerçek: görünüm çerçeve başında türetilmiyor, komut işlem içinde çözüyor,
  `StyleTable`'a intern ediyor ve nesne başına tek bir `StyleId` yazıyor. Çizici
  bir `u32` okuyor, kural işletmiyor.
- **`STİL` komutu (`core.style`).** Bir katmandaki nesnelerin stilini stil
  kataloğu paketinden veya doğrudan verilen renk/kalınlık/dolgu/sıra
  değerlerinden yazar; `sifirla=evet` ile katman varsayılanına döndürür. Arayüz,
  komut satırı ve betikten aynı belgeyi ve aynı günlüğü üretiyor
  (`tests/unit/test_style_rule.cpp`).
- **Bildirimsel kural değerlendirici** (`kentos_cad/core/style_rule.hpp`). Kural dili
  bilerek kapalı: eşitlik, küme üyeliği, tam sayı aralığı, varlık. İfade, öncelik,
  olumsuzlama ve aritmetik yok — projede tek gramer `command/parser.hpp`'dir
  (CLAUDE.md 5.11). Kurallar dosya sırasına göre denenir, ilk uyan kazanır; sıra
  paketin içerik özetinin parçasıdır.
- **Ölçek penceresi.** Satır ve kural bazında `1:N` payda aralığı, iki ucu dahil,
  `0` = sınırsız. Ölçeğe bağlı gösterim nesne başına değil, tablo başına çözülür
  (`model.md` R16).
- **Kâğıt mikrometresi.** Katalogdaki `kalinlik_um` doğrudan `Appearance::width_um`
  alanına gidiyor; piksel hiçbir yerde saklanmıyor (`model.md` R20).
- **MPYY plan gösterim paketi** — `data/catalogs/mpyy/plan-gosterim.json` ve
  şeması `data/catalogs/schema/plan-gosterim.schema.json`. Kaynak: Mekânsal
  Planlar Yapım Yönetmeliği, EK-1 Gösterimler (EK-1a/1b/1c/1ç/1d + EK-1e Detay
  Kataloğu), yayım 14.06.2014.
- **Paketin gösterim satırları 0.1.0'da BİLEREK BOŞTU.** Paket künyesi, şeması,
  plan türü eşlemesi, çizgi ve tarama sembol tabloları tamdı; `stiller` ve
  `kurallar` dizileri boştu, çünkü EK-1 gösterim kodları, renkleri ve çizgi
  kalınlıkları resmî ek metninden birebir okunmadan girilmez. Bu boşluk aşağıdaki
  0.2.0 kaydıyla kapandı; `kurallar` hâlâ ve bilerek boştur.
- **Determinizm sınandı.** Aynı katalog + aynı belge = aynı `StyleId` dizisi ve
  aynı `content_hash()`; aynı görünüm iki kez istendiğinde stil tablosu
  büyümüyor. Golden senaryosu `tests/golden/senaryolar/stil.txt`.
- **Belge.** [`docs/komutlar/style.md`](docs/komutlar/style.md), sekiz bölüm,
  üç istemci yolu, gösterim satırlarının eksikliği ilk paragrafta ve gelecek
  zamanla yazılı (Article 11.8).

### Eklendi — MPYY gösterim ekleri veri paketi (`mpyy` katalogları 0.1.0 → 0.2.0)

Sebebi olan mevzuat: **Mekânsal Planlar Yapım Yönetmeliği**, EK-1 Gösterimler ve
EK-2 asgari altyapı standartları tablosu. EK-1a, EK-1c, EK-1ç, EK-1d ve EK-1e
metinlerinde **(Değişik:RG-22/1/2026-33145)** damgası vardır; paket bu hâli esas
alır. EK-1b'de değişiklik damgası **yoktur**, yönetmeliğin **RG-14/6/2014-29030**
sayılı ilk hâli esas alınmıştır ve bu tespit paketin `source` alanında yazılıdır.
EK-2 **(Değişik:RG-17/5/2017-30069)** ile değişik hâldedir.

- **`data/catalogs/mpyy/plan-gosterim.json` 0.2.0** — 476 gösterim satırı: EK-1a
  Ortak Gösterimler 89, EK-1b Mekânsal Strateji Planı 30, EK-1c Çevre Düzeni Planı
  37, EK-1ç Nazım İmar Planı 115, EK-1d Uygulama İmar Planı 205. 297 satırda alan
  renk kodu, 48 satırda `ŞEFFAF` hükmü, 8 satırda çizgi rengi, 17 satırda simge
  rengi çözüldü.
- **`data/catalogs/mpyy/detay-katalogu.json` 0.2.0** — EK-1e Detay Kataloğu'nun
  379 detay kartı; 338 kartta renk, 311 kartta plan türü başına çizgi kalınlığı
  (kâğıt mikrometresi, 1000 = 1 mm) çözüldü. Şeması
  `data/catalogs/schema/detay-katalogu.schema.json`.
- **`data/catalogs/mpyy/asgari-standartlar.json` 0.2.0** — EK-2'nin 33 altyapı
  kalemi, 4 nüfus grubu ve 13 maddelik açıklama bloğu. m²/kişi değerleri binde tam
  sayı olarak saklanır (0.5 → 500); kayan nokta saklanmaz (CLAUDE.md 2.4). Şeması
  `data/catalogs/schema/asgari-standartlar.schema.json`.
- **608 sembol görseli** `data/catalogs/mpyy/semboller/` altında, dosya adı içerik
  SHA-256'sının ilk 16 basamağı. Aynı sembol kaç satırda geçerse geçsin tek
  dosyadır; toplam 6,6 MB, en büyüğü 287 KB — `data.md` R15'in 10 MB dosya ve
  250 MB ağaç sınırlarının altında, LFS gerekmez.
- **`plan-gosterim.schema.json` `schema_version` 1 → 2.** Yalnız ALAN EKLENDİ; hiçbir
  alanın anlamı değişmedi (CLAUDE.md 0.2a). Yeni alanlar satırın kaynak izini
  taşır: `sutunlar` (ham hücreler), `gorsel`, `sutun_metinleri`, `bolum`, `grup`,
  `renk_secenekleri`, `simge_renk`, `dolgu.seffaf`, `dolgu.saydamlik_yuzde`,
  `belirsiz` / `belirsiz_nedeni` ve paket düzeyinde `gorseller` tablosu.
- **`scripts/mpyy-cikar.py`.** Katalogları resmî ek dosyalarından üretir; yalnız
  Python standart kütüphanesi. Elle düzenlenmiş bir katalog kabul edilmez: aynı
  kaynaktan iki koşum bayt birebir aynı JSON'u verir.
- **Hiçbir değer uydurulmadı.** Okunamayan renk, çözülemeyen satır ve belirsiz ad
  `belirsiz: true` ve bir gerekçe koduyla işaretlendi: 14 gösterim satırı, 15 detay
  kartı, 6 standart kalemi. Gerekçeler `kapsam.eksikler` bloklarında sayılıdır.
- **`kurallar` hâlâ boş.** Hangi nesnenin hangi gösterim satırını alacağı ek
  metninden okunamaz; plan türü ve öznitelik şemasıyla birlikte uzman kararıdır.
- **Uzman onayı BEKLİYOR.** Üç katalogun da `kapsam.onay` alanı `BEKLİYOR`
  yazıyor; harita mühendisi / şehir plancısı imzası olmadan bu paket bir plana
  uygulanmaz (CLAUDE.md 6.11).
- **`scripts/ci-gate-mpyy.sh`.** Katalog özetini `tests/golden/mpyy/beklenen.txt`
  ile karşılaştırır, üç dosyanın SHA-256'sını sabitler ve kaynak ekler mevcutsa
  çıkarımı yeniden koşup bayt birebir karşılaştırır.

### Eklendi — kullanıcı dokümantasyonu

- **Kati kural.** Kullanıcının yapabildiği her şeyin `/docs` altında, Markdown
  biçiminde, Türkçe ve yayımlanabilir kalitede bir sayfası olacak — komut sistemi
  dahil. Belgelenmemiş özellik yayımlanmamış sayılır (`CLAUDE.md` Article 11,
  5.16–5.17, 6.12; `.claude/docs.md`).
- **Kılavuz.** Kurulum, ilk adımlar, arayüz turu, komut sistemi, komut satırı,
  sekiz komut sayfası, betik yazma, komut günlüğü, koordinat sistemleri, sözlük ve
  sorun giderme.
- **Üretilmiş komut referansı.** `kentos_docgen` komut kaydından
  `docs/komutlar/referans.md` üretir; elle düzenlenirse CI kapısı fark eder
  (`make reference`).
- **`scripts/ci-gate-docs.sh`.** Belgesiz komut, eksik zorunlu bölüm, dizine
  bağlanmamış sayfa, ölü bağlantı, TODO kalıntısı ve bayat referans derlemeyi kırar.
- **`tests/unit/test_docs.cpp`.** Kılavuzdaki her komut satırını ve her JSON betiğini
  doğrudan Markdown'dan okuyup çalıştırır. Örneklerin ikinci bir kopyası yoktur.

### Düzeltildi

- Kullanıcıya görünen bütün hata mesajları Türkçeleştirildi; doğrulama, ayrıştırıcı,
  doküman ve kayıt katmanlarında İngilizce metin kalmamıştı. Yeni bir test İngilizce
  sızıntısını yakalıyor.
- Komut satırında anahtar sonrası tırnaklı değer (`KATMAN ad="YOL KENARI"`)
  ayrıştırılamıyordu; kılavuz örneğini çalıştıran test bunu ortaya çıkardı.
- BÖHHBÜY TM 3° dilim tablosu C++ içinden `data/crs/tm3-dilimleri.json` dosyasına
  taşındı — mevzuat verisi koda gömülmez (`CLAUDE.md` 5.13).

### Eklendi — performans ve determinizm altyapısı (Faz 0)

- **Benchmark kapısı.** `make bench` §10.1 bütçelerini ölçer ve aşılırsa derlemeyi
  kırar. `make bench-baseline` makineye özel temel değer kaydeder. Regresyon,
  hem %10'u hem de ölçümün kendi yayılımını aşmak zorundadır — sıfıra yakın bir
  ölçümde göreli eşik tek başına gürültüyü regresyon sanır.
- **Ölçülemeyen bütçeler listelenir.** DWG, LAZ, topoloji, soğuk açılış, boş proje
  RAM'i ve tuş gecikmesi `BEKLEMEDE` olarak sebebiyle raporlanır; hiçbir zaman
  "geçti" saymaz. Sessizce kaybolan bütçenin sahibi olmaz.
- **Golden data altyapısı.** `tests/golden/senaryolar` altındaki senaryolar
  oynatılır ve belgenin deterministik dökümüyle karşılaştırılır. Fark hangi tepe
  noktasının kaydığını söyler, yalnız "özet değişti" demez.
- **Jitter testi (§11 Faz 0).** 30. dilim TM3 koordinatlarında 1000 ardışık
  milimetrenin float'ta yalnız ~32 farklı değere çöktüğü, `ViewTransform`'un
  origin offset'iyle bin ayrı değer kaldığı ölçülerek kanıtlandı.
- **Mekânsal indeks.** `core::SpatialIndex` — §10.5'in tarif ettiği toplu
  yüklenen STR R-tree. Kaba kuvvetle karşılaştıran altı testi var.
- **Nesne başına önbelleklenmiş sınır kutusu.** Ayrı bir SoA bloğu; eleme artık
  tepe noktalarına hiç dokunmuyor.

### Düzeltildi — ölçümün ortaya çıkardıkları

- **5M poligonda kare süresi 41 ms → 0,003 ms.** 16 ms bütçesi artık beş bin kat
  payla karşılanıyor. Aynı düzenlemeden sonraki kare de bütçe içinde: bir çizgi
  çizmek katmanı yeniden paketlemiyor.
- **Toplu yüklemede O(n²).** `add_polyline` her çağrıda tam boyutla `reserve`
  ediyor, vektörün geometrik büyümesini bozuyordu. 1M parsel kurulumu 2 dakikadan
  63 ms'ye indi.
- **Arayüzde üç ayrı O(n) tarama.** Canlı nesne sayısı ve katman başına nesne
  sayısı artık artımlı tutuluyor; katman ve öznitelik panelleri her doküman
  değişiminde bütün nesneleri dolaşmıyor.

### Bilinen sapmalar

Üçü de CLAUDE.md Article 8'de kayıtlı ve kaldırma koşulu yazılı:
canvas `QPainter` (qsb yok), Qt dışında bağımlılık yok, betik motoru JSON.
