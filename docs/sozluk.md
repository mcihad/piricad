# Sözlük

Kılavuzda geçen her alan terimini ve KentOSCad terimini arayan herkes için; kılavuzun geri
kalanı bu sayfadaki tanımları kullanır.

## Haritacılık ve kadastro terimleri

**Aplikasyon** — Projede hesaplanmış bir noktanın veya sınırın araziye uygulanması, yani
kâğıttaki koordinatın zeminde işaretlenmesi.

**Azimut** — Bir doğrultunun kuzeyden saat yönünde ölçülen açısı. KentOSCad'de açının
varsayılan kuralıdır (`semt`): komut satırındaki `@mesafe<açı` böyle okunur, `ÖLÇ` ve
`APLİKASYON` böyle yazar. Bkz. [Komut satırı](komutlar/komut-satiri.md).

**Semt açısı** — Aletin sıfırlandığı bir bağlama doğrultusundan saat yönünde ölçülen açı;
bağlama kuzey ise azimuttur. `APLİKASYON` bağlama noktası verildiğinde bunu yazar.

**Grad** — Tam daireyi 400'e bölen açı birimi; dik açı 100 grad, 1 grad 0,9 derece.
Türkiye'de nirengi, poligon ve aplikasyon hesaplarının birimi ve KentOSCad'in varsayılan
açı birimidir (`açı_birimi`).

**Cins değişikliği** — Bir taşınmazın niteliğinin (arsa, tarla, bina vb.) tapu kütüğünde
değiştirilmesi işlemi.

**Düzenleme sınırı** — İmar uygulamasının hangi alanı kapsadığını belirleyen, uygulamaya
giren bütün parselleri çevreleyen sınır.

**DOP** — Düzenleme Ortaklık Payı. İmar uygulamasında yol, park, okul gibi umumi
hizmetler için parsellerden kesilen pay.

**İfraz** — Bir parselin iki veya daha fazla parsele bölünmesi.

**İhdas** — Kadastroda kayıtlı olmayan bir alanın tescil edilerek yeni parsel
oluşturulması.

**Gösterim** — Bir plan veya haritada bir arazi kullanımının, sınırın ya da tesisin
çizimde nasıl gösterileceğini belirleyen kural: rengi, çizgi kalınlığı, çizgi deseni,
taraması ve simgesi. Mekânsal Planlar Yapım Yönetmeliği'nin EK-1 ekleri plan gösterimlerini
tanımlar. Bkz. [Nesne stili ve gösterim kataloğu](komutlar/style.md).

**İrtifak** — Bir taşınmaz üzerinde başka bir taşınmaz veya kişi lehine kurulan sınırlı
ayni hak; geçit hakkı ve enerji nakil hattı irtifakı yaygın örnekleridir.

**Muhdesat** — Bir taşınmaz üzerindeki, taşınmazın kendisinden ayrı değerlendirilen yapı,
tesis veya ağaç gibi unsurlar.

**Nazım imar planı** — Arazi kullanım kararlarını ve yoğunlukları genel olarak belirleyen,
uygulama imar planına esas oluşturan üst ölçekli plan.

**Pafta** — Haritanın belirli bir ölçekte bölümlenmiş her bir parçası ve ona verilen ad.
Programın bastığı sayfa buna benzer ama bu değildir; ona **çıktı yerleşimi** denir.

**Çıktı yerleşimi** — Çizimin basılacağı sayfanın düzeni: kâğıt boyu, yönü, kenar
boşluğu ve üzerine yerleştirilen öğeler (harita çerçevesi, başlık, ölçek çubuğu, kuzey
oku, lejant, tablo). Çizimle birlikte kaydedilir ve tek `Ctrl+Z` ile geri alınır.
Bir paftayı gösterebilir, ama bir pafta değildir —
[`ÇIKTIYERLEŞİMİ`](komutlar/layout.md).

**Parselasyon** — İmar planına göre arazinin imar parsellerine ayrılması işlemi.

**Röper krokisi** — Bir noktanın çevredeki sabit ayrıntılara olan ölçülerini gösteren,
noktanın kaybolması hâlinde yeniden bulunmasını sağlayan kroki.

**TAKS** — Taban Alanı Katsayısı. Yapının parsele oturan taban alanının parsel alanına
oranı.

**KAKS** — Kat Alanı Katsayısı, emsal. Yapının bütün katlarının toplam alanının parsel
alanına oranı.

**Tevhit** — İki veya daha fazla parselin birleştirilerek tek parsel hâline getirilmesi.

**Uygulama imar planı** — Yapılaşma koşullarını parsel düzeyinde belirleyen, ruhsata esas
alt ölçekli plan.

**Yola terk** — Bir parselin bir kısmının yol olarak kamuya bedelsiz bırakılması.

**Kamulaştırma** — Kamu yararı için özel mülkiyetteki taşınmazın bedeli ödenerek idareye
geçirilmesi.

## Kurum, mevzuat ve standart kısaltmaları

**BÖHHBÜY** — Büyük Ölçekli Harita ve Harita Bilgileri Üretim Yönetmeliği. Jeodezik
altlık, ölçüm ve detay üretim kurallarını belirler.

**MPYY** — Mekânsal Planlar Yapım Yönetmeliği. Plan gösterimlerini ve plan yapım
esaslarını belirler.

**TUCBS** — Türkiye Ulusal Coğrafi Bilgi Sistemi. Kurumlar arası coğrafi veri
paylaşımının ulusal çerçevesi.

**TKGM** — Tapu ve Kadastro Genel Müdürlüğü.

**MEGSİS** — Mekânsal Gayrimenkul Sistemi. TKGM'nin parsel sorgulama ve paylaşım servisi.

**TAKBİS** — Tapu ve Kadastro Bilgi Sistemi.

**LİHKAB** — Lisanslı Harita Kadastro Mühendisleri ve Büroları.

**PlanGML** — İmar planlarının sayısal değişimi için kullanılan GML tabanlı veri biçimi.

**e-Plan** — Mekânsal planların elektronik ortamda sunulduğu otomasyon sistemi.

**TUSAGA-Aktif / CORS-TR** — Türkiye Ulusal Sabit GNSS Ağı. Gerçek zamanlı hassas konum
düzeltmesi sağlar.

## Koordinat ve datum terimleri

**TUREF** — Türkiye Ulusal Referans Çerçevesi. Ülkenin güncel yatay datumu.

**ITRF96** — Uluslararası Yersel Referans Çerçevesi'nin 1996 gerçekleştirmesi; TUREF bu
çerçeveye dayanır.

**ED50** — European Datum 1950. Türkiye'de eski üretimde yaygın kullanılmış datum.

**TM 3°** — Üç derece genişliğinde dilimlere ayrılmış Transverse Mercator projeksiyonu.
Türkiye'de büyük ölçekli harita üretiminin standart projeksiyonudur.

**Orta meridyen** — Bir dilimin ölçek bozulmasının en az olduğu boylam; dilim buna göre
adlandırılır (TM30 = 30° orta meridyen).

**Sağa değer (Y)** — Doğu-batı yönündeki koordinat bileşeni. TM 3° dilimlerinde 500 000 m
yalancı doğu eklenmiştir, bu yüzden Türkiye'de tipik olarak 300 000–700 000 m arasındadır.

**Yukarı değer (X)** — Kuzey-güney yönündeki koordinat bileşeni; ekvatordan itibaren ölçülür,
Türkiye'de tipik olarak 4 000 000–4 700 000 m arasındadır. Türk konvansiyonunda X yukarı
değerdir, matematikteki kullanımın tersine.

**Jeoit** — Ortalama deniz seviyesini temsil eden eşpotansiyelli yüzey; elipsoit
yüksekliğinden ortometrik yüksekliğe geçmek için gerekir.

Ayrıntı: [Koordinat sistemleri](veri/koordinat-sistemleri.md).

## KentOSCad terimleri

**Yazdırma profili** — Adlandırılmış bir kâğıt: kâğıt boyu, yön (dikey/yatay),
çözünürlük ve kenar boşluğu. Bir tanesi varsayılandır ve Yazdır düğmesi onu kullanır.
Kullanıcıya aittir, çizim dosyasıyla gitmez.

**Yazdırma alanı** — Yazdırılacak yerin tuvalde seçildiği, kâğıt oranındaki çerçeve.
Ekranda hep aynı boydadır; harita altında kayar ve yaklaştıkça kâğıda daha az yer
girer.

**Bağlı nesne** — Başka bir nesneyi (kaynağını) izleyen nesne: kaynağın bir köşesine ya da
kenarına bağlıdır, kaynak taşınınca onunla yerleşir, gerekiyorsa sözü yenilenir. Bugün
kenar uzunluğu ve köşe numarası yazıları böyledir; `BAĞLA` kurar, `BAĞÇÖZ` çözer.

**Komut** — Çizimin durumunu değiştiren her işlem. Arayüz düğmesi, komut satırı, betik ve
AI aynı komutları çağırır. Bkz. [Komut sistemi](komutlar/README.md).

**Komut veri yolu** — Bütün komutların geçtiği ortak yol. Doğrulama, işlem ve günlük
burada çalışır; hiçbir istemci onu atlayamaz.

**İstemci** — Komut gönderen taraf: arayüz, komut satırı, betik, AI, toplu iş veya test.
Hiçbirinin diğerine üstünlüğü yoktur.

**İşlem** — Bir komutun yaptığı bütün değişikliklerin oluşturduğu bölünmez bütün. Ya
tamamı uygulanır ya hiçbiri.

**Geri alma adımı** — `GERİAL` ile bir kerede geri alınan iş. Bir komut bir adımdır; bir
betiğin tamamı da bir adımdır.

**Komut günlüğü** — Çalıştırılan her komutun JSON kaydı. Geri alma, makro ve oturum
kurtarmanın ortak kaynağı. Bkz. [Komut günlüğü](mimari/gunluk.md).

**Katman** — Nesnelerin gruplandığı, birlikte gizlenip kilitlenebilen ve ortak renge sahip
küme. Bkz. [Katman yönetimi](komutlar/layer.md).

**Aktif katman** — Yeni çizilen nesnelerin gideceği katman. Durum çubuğunda yazar.

**Nesne** — Çizimdeki tek bir geometri parçası; bugün bir doğru parçası. Her nesnenin
**1'den** başlayan kalıcı bir kimliği vardır; kimlik hiçbir zaman yeniden kullanılmaz.

**Seçim** — Üzerinde işlem yapılacak nesneler kümesi. Çizimin verisi değildir: dosyaya
yazılmaz, geri alınmaz, içerik özetine girmez. Bkz. [Nesne seçme](komutlar/select.md).

**Pencere seçim** — Kutunun içinde **tamamen** kalan nesneleri alan seçim. Fareyle
soldan sağa sürüklenir.

**Kesen seçim** — Kutuya **değen** her nesneyi alan seçim. Fareyle sağdan sola
sürüklenir.

**Nesne yakalama** — Girilen noktayı yakınındaki gerçek geometriye oturtan girdi
yardımı: uç nokta, orta nokta, merkez, kesişim, dik ayak, en yakın. Bkz.
[Oturum modları](komutlar/mode.md).

**Yakalama toleransı** — Nesne yakalamanın arama yarıçapı, **ekran pikseli**. Zemin
metresi değildir: nişan alan göz ekrana bakar.

**Dik mod** — İmleci önceki noktadan geçen yatay ve düşey eksene kilitleyen girdi
yardımı. Kısayolu **F8**.

**Kutupsal izleme** — İmleci önceki noktadan çıkan, belirli açı adımlarındaki ışınlara
oturtan girdi yardımı.

**Nokta fonksiyonu** — Bir koordinatı yazmak yerine nasıl bulunduğunu yazmaya yarayan
gramer: `orta(A,B)`, `dik(A,B,ayak,boy)`, `kes(...)`, `n(1284)`. Komut çalışmadan önce
tek bir noktaya çözülür ve komut satırında, betikte, çalışan bir komutun isteminde aynı
şeyi yapar. Bkz. [Komut satırı](komutlar/komut-satiri.md#nokta-fonksiyonları).

**Dik ayak · dik boy** — Bir tabana (AB doğrusuna) göre bir noktanın yerini söyleyen
ikili: **ayak** tabanda A'dan kaç metre gidildiği, **boy** oradan kaç metre dik
çıkıldığı. A'dan B'ye bakarken **sol pozitif, sağ negatiftir**. Cephe alımının
alfabesidir; `dik(A,B,ayak,boy)` ile yazılır.

**Açı kuralı** — Bir açının nereden ve hangi yöne sayıldığını söyleyen oturum modu
(`açı_kuralı`, kısa adı `kural`): `semt` kuzeyden saat yönüne (varsayılan), `matematik`
doğudan saat yönünün tersine. Yalnız yazılan metni etkiler; komut günlüğü çözülmüş
koordinatı tutar. Bkz. [Oturum modları](komutlar/mode.md).

**Girdi yardımı** — Çizerken imlecin nereye oturacağını belirleyen, çizimin verisi
olmayan ayar: nesne yakalama, dik mod, kutupsal izleme, ızgaraya yakalama.

**Kapsam** — Çizimdeki görünür nesnelerin tamamını çevreleyen dikdörtgen. `YAKINLAŞ
KAPSAM` görünümü buna sığdırır.

**Proje dosyası** — KentOSCad'in kendi kayıt biçimi, uzantısı `.pcad`. Çizimi kayıpsız
taşır: geometri, katman, stil, nesne anahtarları ve proje ayarları.
Bkz. [KentOSCad proje dosyası](veri/proje-dosyasi.md).

**Nesne anahtarı** — Bir nesnenin kalıcı kimliği. Kaydetmeden, yeniden yüklemeden ve
sıralamadan etkilenmez, silinse bile başka bir nesneye verilmez. "Bu parsel hangisiydi?"
sorusunun cevabı budur.

**İçe aktarma** — Dış bir veri dosyasının var olan çizime eklenmesi. Açmaktan farkı,
ekrandaki çizimin yerine geçmemesidir. Bkz. [Dış veri alma](komutlar/import.md).

**Dışa aktarma** — Çizimin dış bir veri biçimine yazılması. Kayıplıdır: öznitelik ve stil
aktarılmaz. Bkz. [Dış biçime yazma](komutlar/export.md).

**Sürücü** — Bir dış veri biçimini okuyup yazan bileşen; `DXF` ve `GPKG` gibi bir adı
vardır. KentOSCad yalnızca izin verilen sürücüleri açar.
Bkz. [Dış veri biçimleri](veri/dis-formatlar.md).

**`.prj` dosyası** — Bir veri dosyasının koordinat sistemini yanında taşıyan metin
dosyası. DXF'in kendi içinde koordinat sistemi için yeri olmadığından gereklidir.

**Şeffaf komut** — Başka bir komut çalışırken araya girebilen komut. `YAKINLAŞ` böyledir.

**Salt okunur komut** — Çizimi değiştirmeyen, bu yüzden geri alma yığınına girmeyen
komut. `YAKINLAŞ`, `GERİAL`, `YİNELE`, `YARDIM`.

**Toplu iş** — Çok sayıda komutun tek doğrulama geçişi ve tek geri alma adımı olarak
yürütülmesi. Betikler böyle çalışır.

**Kum havuzu** — Bir betiğin dosya sistemine ve ağa erişim sınırı: `güvenli`, `proje`,
`tam`. Bkz. [Betik yazma](betik/README.md).

**Betik** — Komutları sırayla çalıştıran JSON dosyası.

**Öznitelik** — Bir katmanın veya dokümanın sayısal olmayan özellikleri; **Öznitelikler**
panelinde görünür.

**Transkript** — Komutların kullanıcıya yazdığı mesajların akışı.

**Stil** — Bir nesnenin çizilirken kullanılacak görünümü: çizgi rengi, kâğıt kalınlığı,
çizgi deseni, dolgu rengi, tarama, simge ve çizim sırası. Her nesne tek bir stil numarası
taşır; görünüm çizim anında hesaplanmaz. Bkz. [Nesne stili](komutlar/style.md).

**Stil kataloğu** — Gösterim satırlarını ve bu satırları nesnelere bağlayan eşleme
kurallarını taşıyan veri paketi. `data/catalogs/` altında durur; mevzuat değişikliği
paketin güncellenmesidir, programın yeniden derlenmesi değil.

**Ölçek paydası** — `1:N` gösteriminde `N`. Uzaklaştıkça büyür: 1:25000, 1:1000'den daha
uzak bir görünümdür. Ölçeğe bağlı gösterimler bu sayıya göre seçilir.

**Kâğıt mikrometresi** — Çizgi kalınlığının saklandığı birim; 1000 mikrometre paftada
1 mm eder. Piksel değildir, çünkü piksel karşılığı ölçek ve ekran çözünürlüğüyle değişir.

**Bileşen seti** — KentOSCad pencerelerinin kurulduğu ortak düğme, girdi ve seçim
denetimleri ailesi. Her denetim 24, 30 ya da 36 piksel boyundadır ve aynı köşe, kenar ve
renk kurallarıyla çizilir. Bkz. [Bileşenler](baslangic/bilesenler.md).

**Birincil düğme** — Bir pencerenin asıl onayı olan mavi dolgulu düğme: `Tamam`,
`Bağlan`, `Filtrele`. Bir ekranda yalnız bir tane bulunur.

**Hayalet düğme** — Zemini ve kenarı olmayan, yalnız yazıdan ibaret düşük öncelikli
düğme: `Yardım`, `Tümü`, `Yenile`.

**Yıkıcı düğme** — Geri alınamayan bir eylemi başlatan kırmızı kenarlı düğme: `Sil`,
`Projeyi Sil`. Her zaman onay ister.

**Kip anahtarı** — Basılı durarak bir kipi açık tutan düğme; `Düzenleme` gibi. Basılıyken
mavi kenar ve dolgu alır.

**Segment** — Yan yana duran, yalnız biri seçili olabilen iki–dört seçenek: `Tablo | Form`,
`Milimetre | Harita birimi | Piksel`. Hiçbirinin seçili olmaması, seçeneğin anlattığı
şeylerin aynı fikirde olmadığı anlamına gelir.

**Çip** — Tam yuvarlak küçük etiket; seçilebilir olabilir. Sığmayan çipler `+2` gibi bir
sayı çipiyle toplanır.

**Rozet** — Bir değerin ya da etiketin yanına düşen 14 piksellik küçük etiket: `HESAP`,
`BOŞ`, `ZORUNLU`, `SABİT`, `ELLE`, `KOPUK`, `UYARLA`. Rengi ne dediğini söyler, yazısı
her zaman rengin yanındadır.

**Türetilmiş değer** — Programın başka değerlerden hesapladığı, elle girilmeyen değer.
Girdi kutusunda `fx` işareti, etiketinde `HESAP` rozeti taşır.

**Nesne türü** — Bir nesnenin dosyaya hangi sayılarla yazıldığını ve ekranda hangi biçimin
çizildiğini birlikte söyleyen sınıfı: çoklu çizgi, daire, yay, nokta, elips. Bkz.
[Nesne türleri](nesneler/README.md).

**Tür yükü** — Bir nesne türünün halkalarının söyleyemediğini taşıyan, yalnız o türün
okuduğu bayt dizisi. Kısmi elips, yaylı çoklu çizgi, spline, tarama, blok referansı,
ölçü ve lider taşır; çoklu çizgi, daire, yay, nokta ve tam elips taşımaz.

**Yabancı veri** — Başka bir programın nesneye bağladığı, KentOSCad'in okumadığı ama
kaybetmediği baytlar; DXF'te XDATA. Panelde yalnız sayısı görünür.

**Blok** — Bir kez çizilip çok kez yerleştirilen sembolün tanımı: rögar kapağı, kuzey oku,
antet. Tanımın nesneleri kendi başına çizilmez ve düzenlenmez.

**Blok referansı** — Bir bloğu belli bir noktaya, ölçekle ve açıyla yerleştiren nesne
(`BLOKEKLE`); DXF `INSERT`.

**Ekleme noktası** — Bir blok referansının yerleştirildiği nokta; yakalama modu
`EKLEME`.

**Şişkinlik** — DXF'te bir çoklu çizgi kenarının yay olduğunu söyleyen sayı (bulge):
kirişin yarısına oranla yayın yüksekliği. Okunurken yayın merkezine ve yarıçapına
çevrilir ve yaylı çoklu çizgi türünde saklanır.

**Spline** — Kontrol noktaları, derece ve düğümlerle tanımlı pürüzsüz eğri (NURBS);
`SPLINE` komutu ve DXF `SPLINE`.

**Tarama** — Kapalı bir sınırı dolu ya da çizgi deseniyle dolduran nesne (DXF `HATCH`);
desenler katalogdan adla gelir.

**Ölçü ve lider** — Uzunluğu ya da açıyı yazısı ve oklarıyla gösteren nesne (`ÖLÇÜ`,
DXF `DIMENSION`) ile bir noktayı gösteren oklu çizgi (`LİDER`, DXF `LEADER`).

**Bağlı ölçü** — Noktaları ölçtüğü nesnenin köşesine, merkezine ya da yay ucuna bağlı
ölçü: nesne değişince yeniden ölçülür, nesne silinince bağı kopar ve bu görünür. Bkz.
[ÖLÇÜ](komutlar/dimension.md#bağlı-ölçü).

**Elle yazılmış ölçü** — Yazısı ölçülen değer değil, elle yazılmış bir değer olan ölçü;
yazısında ölçülen değerin yeri olan `<>` yoktur. Program onu hiçbir yerde ölçülen değer
diye göstermez. Bkz. [ÖLÇÜDÜZENLE](komutlar/dimension_edit.md).

**Pafta ölçeği (ölçünün)** — Bir ölçünün ok ve yazı boylarının hangi pafta ölçeği için
zemine indirildiği; başka bir ölçekte kâğıtta aynı boyda kalmak için
[ÖLÇÜYENİLE](komutlar/dimension_refresh.md) ile uyarlanır.

**İşlem aracı** — Kapsamındaki (seçim, görünüm ya da proje) bütün nesnelere aynı işi tek
seferde uygulayan komut; QGIS'in Processing araçlarının karşılığı. Hangi geometri
türlerine uygulandığını bildirir, ayrı iş parçacığında koşar, durdurulabilir ve sonucunu
seçilen katmana yazar (`UZUNLUKYAZ`, `KÖŞENUMARALA`). Bkz. [İşlem araçları](islem/README.md).


**Ajan** — Programa kendi başına bağlanıp araçlarını çağıran yapay zeka istemcisi. Okuma
araçlarını doğrudan çalıştırır; çizimi değiştiren bir araç çağrısı bir öneriye dönüşür ve
bilgisayar başındaki mühendisi bekler. Bkz. [Yapay zeka ve ajanlar](yapay-zeka/README.md).

**MCP** — Model Context Protocol: bir yapay zeka modelinin ya da ajanının bir programın
yeteneklerine araç olarak erişmesi için kullanılan açık protokol. KentOSCad yalnız
yerel döngüyü dinleyen bir MCP sunucusu gömer. Bkz. [MCP sunucusu](yapay-zeka/mcp-sunucusu.md).

**Öneri** — Bir yapay zeka istemcisinin çizimde yapılmasını istediği işin kaydı: bir
kimlik, bir durum ve uygulanacak komut satırları. Açıldığında hiçbir şey uygulanmaz;
onaylanırsa tamamı tek bir işlem ve tek geri alma adımı olur. Bkz. [`ÖNERİ`](komutlar/suggestion.md).

**Tutamak** — Bir okuma aracının döndürdüğü sonuca verilen kimlik: `@` ve on altı
onaltılık hane, istenirse listenin bir elemanı için `.N`. Bir ajan nokta, nokta listesi
ya da nesne seçimi isteyen bir parametreye yalnız tutamak yazabilir; koordinat
yazamaz. Tutamak alındığı çizim sürümüne bağlıdır ve çizim değişirse reddedilir.
Bkz. [Onay ve denetim](yapay-zeka/onay.md).

**Denetim kaydı** — Her yapay zeka kararının — uygula, reddet, geri çekme ve koordinat
reddi — kullanıcı yapılandırma dizinine yazılan JSONL kaydı: ne istendiği, hangi model,
hangi uç nokta, hangi komut satırları, kimin karar verdiği ve ne zaman. "Bu sınırı buraya
kim koydu?" sorusunun cevabı budur. İçine hiçbir anahtar ya da belirteç yazılmaz.

**Lehçe** — Bir model uç noktasının konuştuğu telli dil. KentOSCad dört tane konuşur:
`openai_chat`, `openai_responses`, `anthropic_messages`, `ollama_native`. Yeni bir
satıcı bunlardan birini konuşuyorsa eklenmesi bir kayıt yazmaktır.
Bkz. [Model sağlayıcıları](yapay-zeka/modeller.md).

**Bağlam penceresi** — Bir modelin bir turda tutabildiği en fazla jeton sayısı. Programa
gömülemez, çünkü sağlayıcıların çoğu bunu bildirmez; bu yüzden sayı, kimin söylediğini
belirten bir işaretle birlikte tutulur: bilinmiyor, yerleşik, kullanıcı ya da bildirilen.
