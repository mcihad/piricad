# Şerit araçları

Bu liste KentOSCad şeridindeki araçları, şeritte göründükleri ad ve sırayla, sekme sekme ve sekme içinde grup grup sıralar.

## KentOS CAD menüsü

Sekme satırının en solundaki KentOS CAD düğmesi, dosyayla yapılan işleri toplayan uygulama menüsünü açar; sağ bölme varsayılan olarak son kullanılan belgeleri, Yazdır, Çıktı Yerleşimleri ve Diğer Komutlar satırlarının üstündeyken onların seçeneklerini gösterir.

### Komut arama

- **Komut ara…** -> Komut listesi sayfasını açar: bütün komutlar adları ve kısaltmalarıyla aranır (Ctrl+K)

### Sol sütun

- **Yeni** -> Boş bir çizim açar; ekrandaki çizimin yerine geçer, kaydedilmemiş değişiklik varsa önce kaydetmek isteyip istemediğinizi sorar (Ctrl+N)
- **Aç…** -> Bir KentOSCad proje dosyasını seçtirir, açar ve çizimin tamamına yakınlaşır (Ctrl+O)
- **Kaydet** -> Çizimi bağlı olduğu proje dosyasına yazar; çizim henüz bir dosyaya bağlı değilse dosya adı sorar (Ctrl+S)
- **Farklı Kaydet…** -> Çizimi yeni bir proje dosyasına yazar ve çizimi o dosyaya bağlar (Ctrl+Shift+S)
- **İçe Aktar…** -> Dış bir veri dosyasını (DXF, DWG, Shapefile, GeoPackage, Netcad NCZ) mevcut çizime ekler; tek işlemdir, bir öğe okunamazsa hepsi geri alınır
- **Dışa Aktar…** -> Çizimdeki görünür nesneleri dış bir veri biçimine (örneğin GeoPackage, DXF) yazar
- **Yazdır** -> Paftayı bir yazdırma profiliyle basar (Ctrl+P); üstüne gelince sağ bölme yazdırma profillerini ve çizimin çıktı yerleşimlerini listeler
- **Çıktı Yerleşimleri** -> Çizimin başlıklı, lejantlı pafta düzenlerini yönetir; sağ bölme yeni yerleşimi, yöneticiyi, şablonları ve çizimdeki her yerleşimi listeler
- **Proje Ayarları…** -> Çizimle birlikte giden ayarları ve projenin öznitelik sütunlarını gösteren pencereyi açar
- **Veritabanı…** -> PostGIS veritabanına bağlanır; katmanları tablo, projeleri kayıt olarak yazar (Ctrl+Shift+D)
- **Betik Çalıştır…** -> Bir betik dosyasındaki komutları sırayla çalıştırır; betiğin tamamı tek geri alma adımıdır (Ctrl+R)
- **Betiği Önizle…** -> Bir betiğin çizimde ne değiştireceğini, çizime dokunmadan söyler
- **Diğer Komutlar** -> Şeritte yeri olmayan dosya, betik ve sistem komutlarını listeler; liste komut kaydından üretilir
  - **Stil Aktar** -> Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar
  - **Nokta Listesi** -> Ölçülmüş nokta listesini okur ve yazar (nokta no, Y, X, Z, kod)
  - **Çıktı Yerleşimi** -> Çizimin çıktı yerleşimlerini yönetir: yeni yerleşim açar, siler, adlandırır ve kâğıdını değiştirir
  - **Çıktı Öğesi** -> Bir çıktı yerleşiminin öğelerini yönetir: harita çerçevesi, başlık, ölçek çubuğu, kuzey oku, lejant, resim, şekil ve tablo
  - **Çıktı Şablonu** -> Kurumun standart çıktı yerleşimlerini saklar ve uygular
  - **Python Çalıştır** -> Bir Python parçacığını komut veri yolu üzerinden çalıştırır; parçacığın tamamı tek geri alma adımıdır
  - **Yazdırma Profili** -> Yazdırma profillerini listeler, ekler, siler ya da birini varsayılan yapar
  - **Çizim Modları** -> Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur ve değiştirir
  - **Yapay Zeka Modeli** -> Yapay zekâ model sağlayıcılarını listeler, ekler, siler, birini varsayılan yapar ya da bağlantısını dener

### Sağ bölme

- **Son kullanılan belgeler** -> Son açılan ve kaydedilen çizimleri dosya adı, klasör ve zamanıyla listeler (en çok 10 belge; sayı ayarlardan değişir); birine tıklamak onu açar ve çizimin tamamına yakınlaşır

### Alt satır

- **Komut Listesi** -> Bütün komutları, adlarını ve kısaltmalarını gösteren komut listesi sayfasını açar (F1)
- **Hakkında** -> Sürüm, lisans ve kaynak kodu bilgisini gösterir
- **Seçenekler…** -> Bildirilen her ayarı kapsamına göre gösteren pencereyi açar; her değişiklik bir komut olarak işlenir (Ctrl+,)
- **Çıkış** -> KentOS CAD'i kapatır; kaydedilmemiş değişiklik varsa sorar

## Hızlı erişim

Sekme satırında, uygulama düğmesinin yanında durur ve her sekmeden erişilir; düğmeleri yalnız simgedir, adı ipucunda yazar.

### Hızlı erişim satırı

- **Yeni** -> Boş bir çizim açar (Ctrl+N)
- **Aç…** -> Bir KentOSCad proje dosyasını açar (Ctrl+O)
- **Kaydet** -> Çizimi bağlı olduğu proje dosyasına yazar (Ctrl+S)
- **Yazdır** -> Bölünmüş düğme: yüzü yazdırma çerçevesini açar, ikinci basış önizlemeye geçer (Ctrl+P); oku çizimin çıktı yerleşimlerini listeler
  - **Yeni Çıktı Yerleşimi…** -> Başlık, harita çerçevesi, ölçek çubuğu ve kuzey oku ile gelen yeni bir çıktı yerleşimi açar
  - **Çıktı Yerleşimi Yöneticisi…** -> Çizimdeki yerleşimleri listeler: aç, yeniden adlandır, çoğalt, sil (Ctrl+Shift+P)
  - **Şablonlar** -> Kurumun kayıtlı yerleşim şablonlarını içeren alt menü; kayıtlı şablon yoksa “(kayıtlı şablon yok)” yazar
  - **(yerleşim adı)** -> Çizimdeki her yerleşim için bir alt menü açılır: Tasarımcıyı Aç, Tuvalden Alan Seç…, PDF'e Aktar…
- **Geri Al** -> Son işlemi geri alır; çizerken yalnız son noktayı geri alır (Ctrl+Z)
- **Yinele** -> Geri alınan işlemi yineler

## Giriş

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Çizim

- **Çizgi** -> Noktaları sırayla birleştiren doğru parçaları çizer; her parça ayrı bir nesnedir
- **Çoklu Çizgi** -> Birden çok köşeli TEK bir çizgi nesnesi çizer; bütünü tek tıkla seçilir
- **Daire** -> Bölünmüş düğme: daire çizer; yüzü en son kullanılan yöntemi çalıştırır, oku dört yöntemi listeler
  - **Daire** -> Merkez ve çember üzerindeki bir noktayla daire çizer
  - **Daire — çapın iki ucu** -> Çapın iki ucunu gösterirsiniz; merkez ortalarıdır
  - **Daire — üç nokta** -> Çember üzerinde gösterdiğiniz üç noktadan daire çizer
  - **Daire — iki doğruya teğet** -> İki doğruya teğet, verdiğiniz yarıçaplı daire çizer; iki doğruyu, yarıçapı ve dairenin geleceği köşeyi gösterirsiniz
- **Yay** -> Bölünmüş düğme: yay çizer; yüzü en son kullanılan yöntemi çalıştırır, oku beş yöntemi listeler
  - **Yay** -> Merkez ve iki uç noktayla yay çizer; süpürme saat yönünün tersinedir
  - **Yay — üç nokta** -> Başlangıç, üzerinden geçtiği nokta ve bitişle yay çizer
  - **Yay — başlangıç, merkez, açı** -> Başlangıç noktası, merkez ve süpürme açısıyla yay çizer; açı ayarlardaki birim ve kuralla okunur
  - **Yay — başlangıç, bitiş, yarıçap** -> Başlangıç, bitiş ve yarıçapla yay çizer; iki çözümden hangisinin çizileceğini sol ya da sağ diyerek seçersiniz
  - **Yay — teğet devam** -> Son çizilen çizginin ya da yayın ucundan teğet devam eden yay çizer
- **Alan** -> Bölünmüş düğme: kapalı şekil çizer; yüzü en son kullanılanı çalıştırır, oku alanı, halkayı ve daire dilimini listeler
  - **Alan** -> Köşelerini verdiğiniz kapalı bir alan (parsel gibi) çizer; kapanış kenarını program ekler
  - **Halka** -> Merkez, iç ve dış yarıçaptan delikli halka çizer
  - **Daire Dilimi** -> Merkez ve iki kenardan kapalı daire dilimi çizer; süpürme saat yönünün tersinedir
- **Dikdörtgen** -> Bölünmüş düğme: dikdörtgen ve düzgün çokgen çizer; yüzü en son kullanılan yöntemi çalıştırır, oku beş yöntemi listeler
  - **Dikdörtgen** -> Karşılıklı iki köşeden yatay ve düşey eksene paralel dikdörtgen çizer; Ctrl basılıyken kare çizer
  - **Dikdörtgen — döndürülmüş** -> Bir kenarın iki köşesini ve yüksekliği veren üçüncü noktayı gösterirsiniz; eksene paralel olmak zorunda olmayan dikdörtgen çizer
  - **Düzgün Çokgen** -> Merkez ve kenar sayısından düzgün çokgen çizer; verdiğiniz yarıçap köşelerin üzerinde durduğu çemberin yarıçapıdır
  - **Çokgen — dıştan** -> Kenarlar çembere teğettir; verdiğiniz yarıçap iç çemberin yarıçapıdır
  - **Çokgen — kenardan** -> Kenar uzunluğundan düzgün çokgen çizer; yarıçap sorulmaz
- **Elips** -> Bölünmüş düğme (simge düğmesi): elips çizer; yüzü en son kullanılanı çalıştırır, oku iki yöntemi listeler
  - **Elips** -> Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir
  - **Elips — eksenin iki ucu** -> Eksenin iki ucunu gösterirsiniz (merkez ortalarıdır); üçüncü nokta ikinci ekseni verir
- **Nokta** -> Bölünmüş düğme (simge düğmesi): nokta koyma araçları ailesi; yüzü en son kullanılanı çalıştırır, oku beş aracı listeler
  - **Nokta** -> Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper
  - **Dik Ayak** -> Taban çizgisine göre ayak ve boy vererek nokta yerleştirir; boy, A'dan B'ye bakarken sağda pozitiftir
  - **Alım** -> İstasyondan okunan açı ve kenar çiftlerinden nokta hesaplar ve yerleştirir
  - **Kesişim Noktası** -> İki bilinen noktadan okunan iki doğrultunun (açının) kesişimine nokta koyar; kaybolan köşeyi geri kurar
  - **Ara Nokta** -> İki nokta arasındaki doğru üzerinde verdiğiniz oranda (0,5 tam ortadır) nokta koyar
- **↘ Çizim ve yakalama ayarları** -> Seçenekler penceresinin Çizim ve Yakalama bölümünü açar

### Değiştir

- **Taşı** -> Seçili nesneleri iki nokta arasındaki kadar taşır
- **Kopyala** -> Seçili nesnelerin kopyasını, başlangıçtan gösterdiğiniz her noktaya kadar öteleyerek koyar; her tıklama bir kopya daha koyar
- **Esnet** -> Pencere içinde kalan köşeleri taşır, dışında kalanları yerinde bırakır; aradaki kenarlar uzar ya da kısalır
- **Döndür** -> Bölünmüş düğme: yüzü en son kullanılan döndürme yöntemini çalıştırır
  - **Döndür** -> Seçili nesneleri bir merkez etrafında döndürür; açı yazılır ya da gösterilir, artı açı saat yönünün tersinedir
  - **Döndür — referansla** -> İki noktayla gösterilen doğrultuyu yeni doğrultuya döndürür; dönme açısı ikisinin farkıdır
- **Aynala** -> Bölünmüş düğme: yüzü en son kullanılan aynalama yöntemini çalıştırır
  - **Aynala** -> Seçili nesneleri iki noktadan geçen eksende yansıtır; yazılar okunur kalır
  - **Aynala — kopyalayarak** -> Özgün nesne yerinde kalır, aynalanmış kopyası çizilir
- **Ölçekle** -> Bölünmüş düğme: yüzü en son kullanılan ölçekleme yöntemini çalıştırır
  - **Ölçekle** -> Seçili nesneleri bir merkeze göre çarpan kadar büyütür ya da küçültür; alanlar çarpanın karesiyle değişir
  - **Ölçekle — referansla** -> İki noktayla gösterilen uzunluğu yeni uzunluğa getirir; çarpan ikisinin oranıdır
- **Buda** -> Bölünmüş düğme: budama ve uzatma yöntemleri; yüzü en son kullanılanı çalıştırır; yüzünde bir Uzat yöntemi varsa düğmenin adı Uzat görünür
  - **Buda** -> Tıkladığınız parçayı kesme sınırları arasından atar: çizgide, yayda, dairede, elipste ve spline'da; sınır olarak seçili nesneler, seçim yoksa yakındaki görünür nesneler kullanılır
  - **Buda — çitle** -> Çizdiğiniz çitin geçtiği bütün parçaları tek seferde budar; Enter uygular
  - **Buda — tıklanan kalsın** -> Tıkladığınız parça kalır, iki yanındaki kesimlerin dışında kalan parçalar gider
  - **Buda — sınırları uzatarak** -> Nesneye yetişmeyen bir sınır kendi doğrultusunda uzatılmış sayılır
  - **Uzat** -> Tıklanan ucu ulaştığı ilk sınıra kadar uzatır: çizginin ucunu doğrultusunda, yayın ucunu çemberi boyunca
  - **Uzat — çitle** -> Çizdiğiniz çitin yanından geçtiği bütün uçları sınıra uzatır; Enter uygular
  - **Uzat — sınırları uzatarak** -> Uca yetişmeyen bir sınır kendi doğrultusunda uzatılmış sayılır
- **Yuvarla** -> Bölünmüş düğme: köşe yuvarlama ve pah kırma yöntemleri; yüzü en son kullanılanı çalıştırır; yüzünde bir Pah yöntemi varsa düğmenin adı Pah görünür
  - **Yuvarla** -> Bir köşeyi ya da iki nesne arasındaki köşeyi verdiğiniz yarıçapta teğet yayla yuvarlatır; 0 keskin köşe kurar
  - **Yuvarla — bütün köşeler** -> Çizgiye ya da alana tıklarsınız; bütün köşelerini aynı yarıçapla yuvarlatır
  - **Pah** -> Bir köşeyi ya da iki çizgi arasındaki köşeyi düz bir kenarla keser (pah kırar); mesafe yazılır ya da gösterilir
  - **Pah — bütün köşeler** -> Çizgiye ya da alana tıklarsınız; bütün köşelerine aynı mesafeyle pah kırar
- **Dizi** -> Bölünmüş düğme: nesneleri çoğaltır; yüzü en son kullanılan yöntemi çalıştırır, oku üç yöntemi listeler
  - **Dizi** -> Seçili nesneleri satır ve sütunlardan oluşan bir ızgara olarak çoğaltır
  - **Dizi — kutupsal** -> Seçili nesneleri bir merkez etrafında döndürerek çoğaltır
  - **Dizi — yol boyunca** -> Seçili nesneleri bir çizgi ya da yay boyunca eşit aralıkla dizer, her kopyayı yolun doğrultusuna döndürür
- **Sil** -> Seçili nesneleri siler (Del); geri alınabilir (simge düğmesi)
- **Patlat** -> Çizgiyi kenarlara, alanı sınırına, yaylı çizgiyi çizgi ve yaylarına, bloğu bileşenlerine ayırır (simge düğmesi)
- **Ofset** -> Seçili nesnelerin verdiğiniz mesafede, gösterdiğiniz tarafta paralelini çizer; özgün nesne yerinde kalır (simge düğmesi)
- **Stil Kopyala** -> Bir nesnenin görünümünü (stilini) seçili nesnelere uygular; geometri ve öznitelikler değişmez (simge düğmesi)

### Açıklama

- **Metin** -> Bölünmüş düğme: yüzü en son kullanılanı çalıştırır, oku iki aracı listeler
  - **Metin** -> Çizime tek ya da çok satırlı yazı nesnesi yazar; yükseklik, hizalama ve satır aralığı verilebilir
  - **Yazıyı Düzenle** -> Çizimde duran yazının metnini, yüksekliğini, hizalamasını ya da satır aralığını değiştirir
- **Ölçü** -> Bölünmüş düğme: yedi ölçü türünden birini çizer; yüzü en son çizilen türü çalıştırır; ölçü stili ayarlardaki varsayılan stilden gelir
  - **Hizalı Ölçü** -> İki nokta arasını, aralarındaki doğru boyunca ölçüp yazısı ve oklarıyla çizer
  - **Doğrusal Ölçü** -> Yatay ya da düşey uzaklığı ölçer: ölçü çizgisini üste çekmek yatay, yana çekmek düşey ölçer
  - **Açı Ölçüsü** -> Tepe noktasını ve iki kolun ucunu gösterirsiniz; yayın içinden geçtiği açıyı, ayarlardaki açı biriminde (grad, derece ya da radyan) ölçü çizimi olarak çizer
  - **Yay Uzunluğu Ölçüsü** -> Yaya tıklarsınız: yayın kirişini değil, boyunca uzunluğunu ölçer
  - **Yarıçap Ölçüsü** -> Daireye ya da yaya tıklarsınız; yarıçapı R önekiyle yazar, ölçü çizgisi yazıya doğru uzanır
  - **Çap Ölçüsü** -> Daireye ya da yaya tıklarsınız; çapı Ø önekiyle yazar, çap çizgisi yazıya doğru döner
  - **Koordinat Ölçüsü** -> Başlangıç ve ölçülecek noktayı, sonra yazının yerini gösterirsiniz; yazıyı yana çekmek sağa, yukarı çekmek yukarı değerini okur
- **Kılavuz Çizgi** -> Bir noktayı gösteren oklu kılavuz çizgi çizer, istenirse son köşesinin yanına yazı koyar
- **Etiket** -> Katmandaki nesneleri özniteliklerinden ve ölçülerinden okuyarak etiketler; etiket yazısı nesneyi izler
- **Bul ve Değiştir…** -> Yazılarda bir sözcüğü bulur, önizler ve hepsinde birden değiştirir; tek geri alma adımıdır (Ctrl+H; macOS'ta Cmd+Option+F)
- **↘ Ölçü stili, pafta ölçeği ve yazdırma ayarları** -> Seçenekler penceresinin Plot ve Çıktı bölümünü açar

### Katmanlar

- **Katmanlar** -> Katmanlar panelini açar: her katmanın görünürlüğü, kilidi, rengi ve stili
- **Katman listesi** -> Seçim yokken etkin katmanı gösterir, başka bir katman seçmek onu etkin yapar; seçim varken seçili nesnelerin katmanını gösterir ve başka bir katman seçmek nesneleri oraya taşır; değerler çizimdeki katmanlardır (satırlar rengini, gizli ya da kilitli olduğunu da gösterir)
- **Etkin Yap** -> Seçili nesnenin katmanını etkin katman yapar (simge düğmesi)
- **Etkin Katmana Taşı** -> Seçili nesneleri etkin katmana taşır (simge düğmesi)
- **Gizle** -> Seçili nesnenin katmanını — seçim yoksa etkin katmanı — gizler (simge düğmesi)
- **Yalnız Bu** -> Yalnız seçili nesnenin katmanı — seçim yoksa etkin katman — görünür kalır, diğerleri gizlenir (simge düğmesi)
- **Tümünü Göster** -> Gizli bütün katmanları geri getirir (simge düğmesi)
- **Kilitle / Aç** -> Seçili nesnenin katmanını — seçim yoksa etkin katmanı — kilitler ya da kilidini açar; kilitli katmanın nesneleri görünür ama seçilemez (simge düğmesi)
- **↘ Katmanlar paneli** -> Katmanlar panelini açar

### Özellikler

- **Çizgi** -> Seçili nesnelerin (seçim yoksa etkin katmanın) çizgi rengini gösterir; renk katmandan geliyorsa “Katmandan” yazar. Açınca renk menüsü gelir; değerler şunlardır: siyah, kırmızı, sarı, yeşil, camgöbeği, mavi, macenta, gri, beyaz
  - **Hazır renkler** -> Dokuz hazır renk karesi: siyah, kırmızı, sarı, yeşil, camgöbeği, mavi, macenta, gri, beyaz; seçim varsa seçilenleri boyar, yoksa rengi seçip sonra boyanacak nesnelere tıklarsınız
  - **Başka bir renk…** -> Saydamlık dahil herhangi bir rengi seçtiğiniz renk penceresini açar
  - **Katmanın rengi** -> Seçili nesneleri kendi katmanlarının rengine döndürür
- **Dolgu** -> Seçili nesnelerin (seçim yoksa etkin katmanın) dolgu rengini gösterir; renk katmandan geliyorsa “Katmandan” yazar. Açınca renk menüsü gelir; değerler şunlardır: siyah, kırmızı, sarı, yeşil, camgöbeği, mavi, macenta, gri, beyaz
  - **Hazır renkler** -> Dokuz hazır renk karesi: siyah, kırmızı, sarı, yeşil, camgöbeği, mavi, macenta, gri, beyaz; seçim varsa seçilenleri boyar, yoksa rengi seçip sonra boyanacak nesnelere tıklarsınız
  - **Başka bir renk…** -> Saydamlık dahil herhangi bir rengi seçtiğiniz renk penceresini açar
  - **Katmanın dolgusu** -> Seçili nesneleri kendi katmanlarının dolgusuna döndürür
  - **Dolgu yok** -> Seçili nesnelerin dolgusunu kaldırır
- **↘ Stil Tasarımcısı — etkin katmanın bütün stili** -> Stil Tasarımcısı penceresini açar: katmanın nasıl çizileceğini fare ile tasarlarsınız

### Pano

- **Yapıştır** -> Panodaki nesneleri tıkladığınız yere koyar (Ctrl+V); katmanlarıyla, stilleriyle ve öznitelikleriyle gelir
- **Kes** -> Seçili nesneleri panoya alır ve çizimden siler; tek geri alma adımıdır (Ctrl+X) (simge düğmesi)
- **Panoya Kopyala** -> Seçili nesneleri çizimin kendi biçiminde panoya yazar (Ctrl+C) (simge düğmesi)
- **Taban Noktasıyla Kopyala** -> Nesneleri seçip taban noktasını gösterirsiniz; yapıştırırken o nokta gösterdiğiniz yere gelir (simge düğmesi)

## Çizim

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Çizgi

- **Çizgi** -> Noktaları sırayla birleştiren doğru parçaları çizer; her parça ayrı bir nesnedir
- **Çoklu Çizgi** -> Birden çok köşeli TEK bir çizgi nesnesi çizer; bütünü tek tıkla seçilir
- **Spline** -> Kontrol noktalarından pürüzsüz eğri (NURBS) çizer
- **Kılavuz** -> Bölünmüş düğme: sonsuz yapı çizgisi (kılavuz) koyar; kılavuz basılmaz, imleç ona oturur; yüzü en son kullanılanı çalıştırır
  - **Yatay Kılavuz** -> Tıkladığınız noktadan geçen yatay kılavuz (yapı çizgisi) koyar; cetvelden sürüklemek de koyar
  - **Düşey Kılavuz** -> Tıkladığınız noktadan geçen düşey kılavuz koyar
  - **Açılı Cetvel Kılavuzu** -> Tıkladığınız noktadan geçen açılı kılavuz koyar; düğme 45g (45 grad) açıyla başlar, açı ayarlardaki birim ve kuralla okunur
  - **Kılavuzları Listele** -> Çizimdeki kılavuzları listeler

### Şekil

- **Daire** -> Bölünmüş düğme: daire çizer; yüzü en son kullanılan yöntemi çalıştırır, oku dört yöntemi listeler
  - **Daire** -> Merkez ve çember üzerindeki bir noktayla daire çizer
  - **Daire — çapın iki ucu** -> Çapın iki ucunu gösterirsiniz; merkez ortalarıdır
  - **Daire — üç nokta** -> Çember üzerinde gösterdiğiniz üç noktadan daire çizer
  - **Daire — iki doğruya teğet** -> İki doğruya teğet, verdiğiniz yarıçaplı daire çizer; iki doğruyu, yarıçapı ve dairenin geleceği köşeyi gösterirsiniz
- **Yay** -> Bölünmüş düğme: yay çizer; yüzü en son kullanılan yöntemi çalıştırır, oku beş yöntemi listeler
  - **Yay** -> Merkez ve iki uç noktayla yay çizer; süpürme saat yönünün tersinedir
  - **Yay — üç nokta** -> Başlangıç, üzerinden geçtiği nokta ve bitişle yay çizer
  - **Yay — başlangıç, merkez, açı** -> Başlangıç noktası, merkez ve süpürme açısıyla yay çizer; açı ayarlardaki birim ve kuralla okunur
  - **Yay — başlangıç, bitiş, yarıçap** -> Başlangıç, bitiş ve yarıçapla yay çizer; iki çözümden hangisinin çizileceğini sol ya da sağ diyerek seçersiniz
  - **Yay — teğet devam** -> Son çizilen çizginin ya da yayın ucundan teğet devam eden yay çizer
- **Alan** -> Köşelerini verdiğiniz kapalı bir alan (parsel gibi) çizer; kapanış kenarını program ekler
- **Dikdörtgen** -> Bölünmüş düğme: yüzü en son kullanılan yöntemi çalıştırır, oku iki yöntemi listeler
  - **Dikdörtgen** -> Karşılıklı iki köşeden yatay ve düşey eksene paralel dikdörtgen çizer; Ctrl basılıyken kare çizer
  - **Dikdörtgen — döndürülmüş** -> Bir kenarın iki köşesini ve yüksekliği veren üçüncü noktayı gösterirsiniz; eksene paralel olmak zorunda olmayan dikdörtgen çizer
- **Çokgen** -> Bölünmüş düğme: düzgün çokgen çizer; yüzü en son kullanılan yöntemi çalıştırır, oku üç yöntemi listeler
  - **Düzgün Çokgen** -> Merkez ve kenar sayısından düzgün çokgen çizer; verdiğiniz yarıçap köşelerin üzerinde durduğu çemberin yarıçapıdır
  - **Çokgen — dıştan** -> Kenarlar çembere teğettir; verdiğiniz yarıçap iç çemberin yarıçapıdır
  - **Çokgen — kenardan** -> Kenar uzunluğundan düzgün çokgen çizer; yarıçap sorulmaz
- **Elips** -> Bölünmüş düğme: elips çizer; yüzü en son kullanılanı çalıştırır, oku iki yöntemi listeler
  - **Elips** -> Merkez ve iki eksenden elips çizer; ikinci eksen birincisine diktir
  - **Elips — eksenin iki ucu** -> Eksenin iki ucunu gösterirsiniz (merkez ortalarıdır); üçüncü nokta ikinci ekseni verir
- **Daire Dilimi** -> Merkez ve iki kenardan kapalı daire dilimi çizer; süpürme saat yönünün tersinedir
- **Halka** -> Merkez, iç ve dış yarıçaptan delikli halka çizer

### Nokta ve Alım

- **Nokta** -> Ölçülmüş nokta yerleştirir: nirengi, poligon noktası, röper
- **Alım** -> İstasyondan okunan açı ve kenar çiftlerinden nokta hesaplar ve yerleştirir
- **Dik Ayak** -> Taban çizgisine göre ayak ve boy vererek nokta yerleştirir; boy, A'dan B'ye bakarken sağda pozitiftir
- **Kesişim** -> Bölünmüş düğme: kaybolan bir köşeyi geri kurar; yüzü en son kullanılan yöntemi çalıştırır, oku üç yöntemi listeler
  - **Kesişim Noktası** -> İki bilinen noktadan okunan iki doğrultunun (açının) kesişimine nokta koyar; kaybolan köşeyi geri kurar
  - **Kesişim — iki mesafeden** -> İki bilinen noktadan ölçülen iki uzaklığın kesişimine nokta koyar; iki çözümden birini gösterirsiniz
  - **Kesişim — iki doğrudan** -> İki doğrunun (her birinden iki nokta) uzatılmış hâllerinin kesişimine nokta koyar
- **Ara Nokta** -> Bölünmüş düğme: yüzü en son kullanılan yöntemi çalıştırır, oku iki yöntemi listeler
  - **Ara Nokta** -> İki nokta arasındaki doğru üzerinde verdiğiniz oranda (0,5 tam ortadır) nokta koyar
  - **Ara Nokta — mesafeden** -> Oran yerine ilk noktadan metre cinsinden uzaklıkla nokta koyar

### Tarama

- **Tarama** -> Bölünmüş düğme: tarama yöntemleri; yüzü en son kullanılanı çalıştırır, oku üç yöntemi listeler
  - **Tarama** -> Kapalı nesnelerin içini katalogdaki bir desenle tarar; tarama sınırına bağlıdır, sınır değişince güncellenir
  - **Tarama — içine tıklayarak** -> Bölgenin içine tıklarsınız: çevreleyen çizgilerin kapattığı alan taranır, içindeki kapalı çizgiler boş kalır
  - **Tarama — seçilenler dışarıda** -> Önce taramadan boş kalacak yazı, blok ya da noktaları seçersiniz, sonra bölgenin içine tıklarsınız; seçilenler taramada boş kalır
- **Desenler** -> Tarama desenlerini göründükleri gibi gösteren galeri (üç desen görünür, ok hepsini açar); birine basmak taramayı o desenle başlatır; değerler şunlardır: SOLID, ANSI31, ANSI32, ANSI33, ANSI34, ANSI37, LINE, NET, DOTS, EARTH, GRASS
  - **SOLID** -> Dolu dolgu; basınca tarama bu desenle başlar ve sınırı sorar
  - **ANSI31** -> 45° tek yönlü çizgiler, 3,175 mm aralık (demir, tuğla, taş); basınca tarama bu desenle başlar ve sınırı sorar
  - **ANSI32** -> 45° çift çizgi, 9,525 mm aralık (çelik); basınca tarama bu desenle başlar ve sınırı sorar
  - **ANSI33** -> 45° çizgi ve kesikli çizgi, 6,35 mm aralık (bronz, pirinç, bakır); basınca tarama bu desenle başlar ve sınırı sorar
  - **ANSI34** -> 45° dört çizgi, 19,05 mm aralık (plastik, kauçuk); basınca tarama bu desenle başlar ve sınırı sorar
  - **ANSI37** -> 45° ve 135° çapraz çizgiler, 3,175 mm aralık (kurşun, çinko, magnezyum); basınca tarama bu desenle başlar ve sınırı sorar
  - **LINE** -> Yatay paralel çizgiler, 3,175 mm aralık; basınca tarama bu desenle başlar ve sınırı sorar
  - **NET** -> Yatay ve düşey kare ağ, 3,175 mm aralık; basınca tarama bu desenle başlar ve sınırı sorar
  - **DOTS** -> Nokta deseni, 1,5875 mm aralık; basınca tarama bu desenle başlar ve sınırı sorar
  - **EARTH** -> Toprak: 45° ve 135° kesikli çizgiler, 6,35 mm aralık; basınca tarama bu desenle başlar ve sınırı sorar
  - **GRASS** -> Çim: 45° ve 135° kısa çizgiler, 44,45 mm aralık; basınca tarama bu desenle başlar ve sınırı sorar
- **Taramayı Düzenle** -> Çizilmiş taramanın desenini, açısını, ölçeğini, aralığını, çapraz çizimini ve ada kuralını değiştirir; bağı ve sınırı korunur
- **Sınır Bul** -> İçine tıkladığınız kapalı bölgenin sınırını yeni bir alan olarak çıkarır; içerideki adalar delik olur, kapanmıyorsa açık uçları gösterir

### Blok

- **Blok Ekle** -> Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir
- **Kitaplıktan Ekle** -> Bir proje, DXF ya da DWG dosyasından seçtiğiniz bloğu bu çizime getirir ve yerleştirir
- **Blok** -> Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar
- **Kırp** -> Bölünmüş düğme: blok ya da dış referansı kırpar; yüzü en son kullanılan yöntemi çalıştırır, oku üç yöntemi listeler
  - **Kırp** -> Blok ya da dış referansı iki köşeli bir dikdörtgenle kırpar: içi çizilir, dışı çizilmez ve yakalanmaz
  - **Çokgenle Kırp** -> Kırpma sınırını köşe köşe çizerek kırpar; Enter sınırı kapatır
  - **Nesneyle Kırp** -> Çizimdeki kapalı bir çizgi, alan, daire ya da elipsle kırpar

### Diğer komutlar

- **Diğer** -> Şeritte düğmesi olmayan çizim komutlarını listeler; liste komut kaydından üretilir
  - **Çift Çizgi** -> Bir eksen çizer; eksenin solunda ve sağında, her biri kendi genişliğinde birer paralel çizgi çizer; köşeler keskin, yuvarlak ya da pahlı olabilir
  - **Dördüncü Köşe** -> Üç köşeden dördüncü köşeyi hesaplar ve dört köşeli kapalı bir alan çizer; istenirse üçüncü köşeyi dik açıya çeker ve sapmayı söyler

## Değiştir

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Dönüştür

- **Taşı** -> Seçili nesneleri iki nokta arasındaki kadar taşır
- **Kopyala** -> Seçili nesnelerin kopyasını, başlangıçtan gösterdiğiniz her noktaya kadar öteleyerek koyar; her tıklama bir kopya daha koyar
- **Döndür** -> Bölünmüş düğme: yüzü en son kullanılan döndürme yöntemini çalıştırır
  - **Döndür** -> Seçili nesneleri bir merkez etrafında döndürür; açı yazılır ya da gösterilir, artı açı saat yönünün tersinedir
  - **Döndür — referansla** -> İki noktayla gösterilen doğrultuyu yeni doğrultuya döndürür; dönme açısı ikisinin farkıdır
- **Ölçekle** -> Bölünmüş düğme: yüzü en son kullanılan ölçekleme yöntemini çalıştırır
  - **Ölçekle** -> Seçili nesneleri bir merkeze göre çarpan kadar büyütür ya da küçültür; alanlar çarpanın karesiyle değişir
  - **Ölçekle — referansla** -> İki noktayla gösterilen uzunluğu yeni uzunluğa getirir; çarpan ikisinin oranıdır
- **Aynala** -> Bölünmüş düğme: yüzü en son kullanılan aynalama yöntemini çalıştırır
  - **Aynala** -> Seçili nesneleri iki noktadan geçen eksende yansıtır; yazılar okunur kalır
  - **Aynala — kopyalayarak** -> Özgün nesne yerinde kalır, aynalanmış kopyası çizilir
- **Hizala** -> Bölünmüş düğme: nesneleri nokta çiftleriyle hizalar; yüzü en son kullanılanı çalıştırır, oku iki yöntemi listeler
  - **Hizala** -> Bir ya da iki nokta çiftiyle nesneleri taşır, döndürür ve istenirse ölçekler
  - **Hizala — ölçekleyerek** -> İki nokta çiftiyle taşır, döndürür ve ikinci çiftin uzunluğuna göre ölçekler
- **Esnet** -> Pencere içinde kalan köşeleri taşır, dışında kalanları yerinde bırakır; aradaki kenarlar uzar ya da kısalır

### Dizi ve Ofset

- **Dizi** -> Bölünmüş düğme: nesneleri çoğaltır; yüzü en son kullanılan yöntemi çalıştırır, oku üç yöntemi listeler
  - **Dizi** -> Seçili nesneleri satır ve sütunlardan oluşan bir ızgara olarak çoğaltır
  - **Dizi — kutupsal** -> Seçili nesneleri bir merkez etrafında döndürerek çoğaltır
  - **Dizi — yol boyunca** -> Seçili nesneleri bir çizgi ya da yay boyunca eşit aralıkla dizer, her kopyayı yolun doğrultusuna döndürür
- **Ofset** -> Seçili nesnelerin verdiğiniz mesafede, gösterdiğiniz tarafta paralelini çizer; özgün nesne yerinde kalır

### Kes ve Uzat

- **Buda** -> Bölünmüş düğme: budama yöntemleri; yüzü en son kullanılanı çalıştırır, oku dört yöntemi listeler
  - **Buda** -> Tıkladığınız parçayı kesme sınırları arasından atar: çizgide, yayda, dairede, elipste ve spline'da; sınır olarak seçili nesneler, seçim yoksa yakındaki görünür nesneler kullanılır
  - **Buda — çitle** -> Çizdiğiniz çitin geçtiği bütün parçaları tek seferde budar; Enter uygular
  - **Buda — tıklanan kalsın** -> Tıkladığınız parça kalır, iki yanındaki kesimlerin dışında kalan parçalar gider
  - **Buda — sınırları uzatarak** -> Nesneye yetişmeyen bir sınır kendi doğrultusunda uzatılmış sayılır
- **Uzat** -> Bölünmüş düğme: uzatma yöntemleri; yüzü en son kullanılanı çalıştırır, oku üç yöntemi listeler
  - **Uzat** -> Tıklanan ucu ulaştığı ilk sınıra kadar uzatır: çizginin ucunu doğrultusunda, yayın ucunu çemberi boyunca
  - **Uzat — çitle** -> Çizdiğiniz çitin yanından geçtiği bütün uçları sınıra uzatır; Enter uygular
  - **Uzat — sınırları uzatarak** -> Uca yetişmeyen bir sınır kendi doğrultusunda uzatılmış sayılır
- **Kır** -> İki nokta arasındaki parçayı çıkarır; tek nokta verilirse açık nesneyi boşluk bırakmadan böler
- **Uzunluk** -> Çizginin bir ucunu kendi doğrultusunda hareket ettirerek uzunluğunu değiştirir
- **Böl** -> Bölünmüş düğme: nesneleri parçalara ayırır ve bütün parçaları tutar; yüzü en son kullanılan yöntemi çalıştırır, oku beş yöntemi listeler
  - **Böl** -> Çizdiğiniz kesme çizgisiyle böler: çizgi, yay, daire, yaylı çoklu çizgi ve alan; bütün parçaları tutar
  - **Böl — noktalardan** -> Nesnenin üstüne tıkladığınız noktalardan böler; parçalar Enter'dan önce görünür, ⌫ son noktayı geri alır
  - **Böl — kesişimlerden** -> Seçtiğiniz nesneleri birbirini kestikleri her yerden böler
  - **Böl — eşit parçaya** -> Seçtiğiniz nesneleri verdiğiniz sayıda eşit parçaya böler
  - **Böl — baştan uzaklıkla** -> Seçtiğiniz nesneleri başından verdiğiniz uzaklıkta böler
- **Bölümle** -> Bir nesne boyunca eşit parçalara bölerek ya da sabit aralıkla nokta (ya da blok) yerleştirir

### Köşe

- **Yuvarla** -> Bölünmüş düğme: köşe yuvarlama; yüzü en son kullanılanı çalıştırır, oku iki yöntemi listeler
  - **Yuvarla** -> Bir köşeyi ya da iki nesne arasındaki köşeyi verdiğiniz yarıçapta teğet yayla yuvarlatır; 0 keskin köşe kurar
  - **Yuvarla — bütün köşeler** -> Çizgiye ya da alana tıklarsınız; bütün köşelerini aynı yarıçapla yuvarlatır
- **Pah** -> Bölünmüş düğme: pah kırma; yüzü en son kullanılanı çalıştırır, oku iki yöntemi listeler
  - **Pah** -> Bir köşeyi ya da iki çizgi arasındaki köşeyi düz bir kenarla keser (pah kırar); mesafe yazılır ya da gösterilir
  - **Pah — bütün köşeler** -> Çizgiye ya da alana tıklarsınız; bütün köşelerine aynı mesafeyle pah kırar
- **Köşe Taşı** -> Köşeye tıklarsınız, yeni yerini gösterirsiniz; kenarlar imleci izler
- **Köşe Ekle** -> Kenara tıklarsınız, yeni köşenin yerini gösterirsiniz
- **Köşe Sil** -> Köşeye tıklarsınız; köşede buluşan iki kenar tek kenar olur; seçili parsellerin ortak köşesi ikisinden birden silinir
- **Kenar Türü** -> Kenara tıklarsınız: düz kenar gösterdiğiniz noktadan geçen yaya, yay düz kenara döner; nesnenin kimliği korunur
- **Çizgi Düzenle** -> Çizgiyi kapatır, açar, yönünü çevirir ya da yakın köşelerini atarak sadeleştirir
- **Alanı Düzenle…** -> Kapalı bir alanı istenen alana getirir (hedef alanı Araçlar panelinde yazarsınız): kenarları eşit daraltıp genişleterek, bir kenarı kaydırarak ya da bir köşeyi çekerek; şekil bozulmaz

### Birleştir

- **Birleştir** -> Seçili alanları tek alanda birleştirir, uç uca değen çizgileri tek çizgi yapar
- **Uç Uca Ekle** -> Uçları değen çizgileri, yayları ve yaylı çoklu çizgileri tek nesneye ekler; yaylar yay kalır
- **Alana Çevir** -> Uç uca değen çizgilerden tek bir kapalı alan kurar ve kaynak çizgileri siler
- **Patlat** -> Çizgiyi kenarlara, alanı sınırına, yaylı çizgiyi çizgi ve yaylarına, bloğu bileşenlerine ayırır

### Sil ve Temizle

- **Sil** -> Seçili nesneleri siler (Del); geri alınabilir
- **Temizle — bul** -> Yinelenen, boş ve tekrarlanan köşeli nesneleri bulur, seçer ve işaretler; hiçbir şeyi değiştirmez
- **Temizle — onar** -> Yinelenenleri ve boş nesneleri siler, tekrarlanan köşeleri çıkarır; değişen alanları önce ve sonra söyler, tek adımda geri alınır

### Diğer komutlar

- **Diğer** -> Şeritte düğmesi olmayan düzenleme komutlarını listeler; liste komut kaydından üretilir
  - **Öznitelik** -> Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar
  - **Sütun** -> Öznitelik sütunu tanımlar, düzenler, siler; argümansız çalışınca sütunları listeler

## Açıklama

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Yazı

- **Metin** -> Çizime tek ya da çok satırlı yazı nesnesi yazar; yükseklik, hizalama ve satır aralığı verilebilir
- **Yazıyı Düzenle** -> Çizimde duran yazının metnini, yüksekliğini, hizalamasını ya da satır aralığını değiştirir
- **Bul ve Değiştir…** -> Yazılarda bir sözcüğü bulur, önizler ve hepsinde birden değiştirir; tek geri alma adımıdır (Ctrl+H; macOS'ta Cmd+Option+F)
- **Yükseklik** -> Yeni bir yazının yüksekliğini belirler (zeminde metre; projenin metin yüksekliği ayarıdır); kutuya başka bir sayı da yazılabilir; değerler şunlardır: 0,50 m, 1,00 m, 1,50 m, 2,00 m, 2,50 m, 3,00 m, 3,50 m, 5,00 m, 7,00 m, 10,00 m

### Ölçü

- **Ölçü** -> Bölünmüş düğme: yedi ölçü türünden birini çizer; yüzü en son çizilen türü çalıştırır; ölçü stili ayarlardaki varsayılan stilden gelir
  - **Hizalı Ölçü** -> İki nokta arasını, aralarındaki doğru boyunca ölçüp yazısı ve oklarıyla çizer
  - **Doğrusal Ölçü** -> Yatay ya da düşey uzaklığı ölçer: ölçü çizgisini üste çekmek yatay, yana çekmek düşey ölçer
  - **Açı Ölçüsü** -> Tepe noktasını ve iki kolun ucunu gösterirsiniz; yayın içinden geçtiği açıyı, ayarlardaki açı biriminde (grad, derece ya da radyan) ölçü çizimi olarak çizer
  - **Yay Uzunluğu Ölçüsü** -> Yaya tıklarsınız: yayın kirişini değil, boyunca uzunluğunu ölçer
  - **Yarıçap Ölçüsü** -> Daireye ya da yaya tıklarsınız; yarıçapı R önekiyle yazar, ölçü çizgisi yazıya doğru uzanır
  - **Çap Ölçüsü** -> Daireye ya da yaya tıklarsınız; çapı Ø önekiyle yazar, çap çizgisi yazıya doğru döner
  - **Koordinat Ölçüsü** -> Başlangıç ve ölçülecek noktayı, sonra yazının yerini gösterirsiniz; yazıyı yana çekmek sağa, yukarı çekmek yukarı değerini okur
- **Zincir Ölçü** -> Son ölçünün ucundan aynı çizgide art arda ölçer; noktaları tıklarsınız, Enter bitirir ve toplamı söyler
- **Baz Ölçü** -> Son ölçünün ilk noktasından ölçer, ölçü çizgilerini stilin aralığıyla üst üste dizer; Enter bitirir
- **Ölçüyü Düzenle** -> Çizilmiş ölçünün yazısını, önek ve sonekini, toleransını, birimini, ondalığını, stilini ya da yazı yerini değiştirir; ölçülen değer değişmez
- **Stil** -> Ölçü stili kutusu: ölçü, zincir ölçü, baz ölçü ve kılavuz çizgi çizilirken stil verilmezse kullanılacak varsayılan ölçü stilini belirler (ok, uzatma çizgileri, yazı ve ondalıklar stilden gelir); değerler şunlardır: ISO-25, ISO-18, ISO-35, STANDARD, MIMARI
  - **ISO-25** -> ISO metrik varsayılan: kapalı ok 2,5 mm, uzatma fazlası 1,25 mm, iki ondalık, virgül; varsayılan yapılır
  - **ISO-18** -> ISO 3098-1 dizisinin 1,8 mm yazısı: sık paftada kapalı ok 1,8 mm, iki ondalık, virgül; varsayılan yapılır
  - **ISO-35** -> ISO 3098-1 dizisinin 3,5 mm yazısı: seyrek ya da uzaktan okunacak paftada kapalı ok 3,5 mm, iki ondalık, virgül; varsayılan yapılır
  - **STANDARD** -> AutoCAD STANDARD: kapalı ok 1,8 mm, dört ondalık, nokta; varsayılan yapılır
  - **MIMARI** -> Mimari çizgi ucu: 45° çentik, iki ondalık, virgül; varsayılan yapılır
- **Ölçü Stilleri** -> Ölçü stillerini kâğıttaki ve bu paftadaki boylarıyla listeler; hangisinin varsayılan olduğunu söyler
- **Pafta Ölçeğine Uyarla** -> Çizimdeki bütün ölçüleri plan ölçeğine uyarlar: oklar, uzatma çizgileri ve yazılar kâğıtta stilin boyunda kalır
- **↘ Ölçü stili ve pafta ölçeği ayarları** -> Seçenekler penceresinin Plot ve Çıktı bölümünü açar

### Etiket

- **Etiket** -> Katmandaki nesneleri özniteliklerinden ve ölçülerinden okuyarak etiketler; etiket yazısı nesneyi izler
- **Kılavuz Çizgi** -> Bir noktayı gösteren oklu kılavuz çizgi çizer, istenirse son köşesinin yanına yazı koyar
- **Bağla** -> Kapsamdaki yazıları seçtiğiniz çizgi ya da alanın en yakın kenarına, köşesine ya da ortasına bağlar; nesne taşınınca yazı izler
- **Bağı Çöz** -> Yazıların bağını çözer: yazı yerinde kalır, bağlı olduğu nesne bundan sonra tek başına taşınır
- **Uzunluk Yaz** -> Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel ve kenara bağlı bir yazı olarak yazar

## Kadastro

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Parsel

- **İfraz** -> Bir parseli düz bir ayırma çizgisiyle ikiye böler; alan kaybolmaz, parçaların ve toplamın alanı yazılır
- **Alana Göre İfraz** -> Parselden verilen yöne paralel, istenen alanda bir parça ayırır
- **Tevhit** -> Komşu parselleri tek parselde birleştirir; bitişik olmayan parselleri reddeder

### Yazım

- **Etiket** -> Katmandaki nesneleri özniteliklerinden ve ölçülerinden okuyarak etiketler; etiket yazısı nesneyi izler
- **Köşe Numarala** -> Her alanın köşelerini seçtiğiniz köşeden başlayarak sırayla numaralar ve numarayı köşenin dışına yazar; numara köşesine bağlıdır
- **Uzunluk Yaz** -> Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel ve kenara bağlı bir yazı olarak yazar
- **Alan Üret** -> Çizgilerin kapattığı her gözü ayrı bir alan olarak çizer; içerideki adalar delik olur, açık uçlar sayılıp gösterilir

### Denetim

- **Topoloji Denetimi** -> Kendini kesen sınır, sıfır alan, örtüşen parsel, yinelenen ve boş nesne, tekrarlanan köşe ve çizgi ağındaki boşlukları raporlar; hiçbir şeyi düzeltmez
- **Alan Ölç** -> Seçili nesnelerin alanını ve çevresini yazar; sonuç tuvalde kalır
- **Tampon…** -> Nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer; mesafeyi Araçlar panelinde yazarsınız
- **Bağımlılıklar** -> Tampon, üretilen alan, sınır ve eş yükselti eğrileri gibi sonuçların kaynaklarına göre güncel olup olmadığını söyler

## Harita

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Sorgu

- **Sorgula** -> Katman ve öznitelik koşuluna uyan nesneleri sayar ve anahtarlarını bildirir; seçim yapmaz, çizime dokunmaz
- **Nesne Bilgisi** -> Seçtiğiniz nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve özniteliklerini söyler
- **Koordinat Oku** -> Tıkladığınız noktanın sağa ve yukarı değerini çizimin koordinat sisteminde yazar

### Ölçüm

- **Ölç** -> Bölünmüş düğme: mesafe, koordinat farkı ve açı ölçer; yüzü en son kullanılanı çalıştırır, oku üç aracı listeler
  - **Ölç** -> Noktadan noktaya her kenarı, açısını ve toplam uzunluğu yazar; Enter bitirir, sonuç tuvalde kalır
  - **Ölç — ilk nokta sabit** -> Her nokta bir öncekinden değil ilk noktadan ölçülür (bir köşenin çevresindeki yapılara uzaklıklar); Enter bitirir
  - **Prizma** -> İki noktalı bir tabana göre noktaların dik ayağını ve dik boyunu okur; boy sağda pozitiftir
- **Alan Ölç** -> Bölünmüş düğme: alan ve çevre ölçer; yüzü en son kullanılan yöntemi çalıştırır, oku üç yöntemi listeler
  - **Alan Ölç** -> Seçili nesnelerin alanını ve çevresini yazar; sonuç tuvalde kalır
  - **Alan Ölç — köşelerden** -> Çizimde olmayan bir alanı ölçer: köşelere tıklarsınız, alan ve çevre imleçle birlikte yazılır, Enter bitirir
  - **Alan Ölç — içine tıklayarak** -> Bölgenin içine tıklarsınız: çevreleyen çizgilerin kapattığı alan ölçülür, içindeki kapalı çizgiler ada olarak düşülür
- **Açı Ölç** -> Tepe ve iki kol noktasıyla açıyı ölçer, ayarlardaki açı biriminde ve kuralıyla yazar; çizime bir şey eklemez

### Jeodezi

- **Poligon Hesabı** -> Kırılma açısı ve kenarlardan poligon koordinatlarını hesaplar, kapanma hatalarını dağıtır ve mevzuat toleransına karşı denetler
- **Aplikasyon** -> İstasyondan her noktaya mesafe ve açı listesi çıkarır
- **Oturt (Helmert)** -> Yerel ölçülmüş çizimi ortak kontrol noktalarıyla haritaya oturtur (2B Helmert dönüşümü)
- **Dönüştür** -> Çizimin tamamını bir koordinat sisteminden diğerine dönüştürür
- **↘ Koordinat sistemi ayarları** -> Seçenekler penceresinin Koordinat Sistemleri bölümünü açar

### Arazi

- **Eşyükselti** -> Kotlu noktalardan eş yükselti eğrileri çizer
- **Hacim** -> Kotlu noktalardan kurulan yüzeyi verilen bir kotla karşılaştırıp kazı ve dolgu hacmini hesaplar

### Veri

- **Veritabanı…** -> PostGIS veritabanına bağlanır; katmanları tablo, projeleri kayıt olarak yazar (Ctrl+Shift+D)
- **İçe Aktar…** -> Dış bir veri dosyasını (DXF, DWG, Shapefile, GeoPackage, Netcad NCZ) mevcut çizime ekler; tek işlemdir, bir öğe okunamazsa hepsi geri alınır
- **Dışa Aktar…** -> Çizimdeki görünür nesneleri dış bir veri biçimine (örneğin GeoPackage, DXF) yazar
- **Dış Referans** -> Bir proje, DXF, DWG, Netcad NCZ ya da CBS dosyasını dış referans olarak bağlar: yerinde çizilir ve yakalanır, düzenlenmez, dosyası değişince yenilenir
- **Dış Referansları Yenile** -> Bağlı dış referansları dosyalarından yeniden okur
- **Yerel Kopya** -> Bir dış referanstaki nesnelerin düzenlenebilir kopyalarını bu çizime alır; bağlantı yerinde kalır
- **Kırp** -> Blok ya da dış referansı iki köşeli bir dikdörtgenle kırpar: içi çizilir, dışı çizilmez ve yakalanmaz

### Diğer komutlar

- **Diğer** -> Şeritte düğmesi olmayan sorgu komutlarını listeler; liste komut kaydından üretilir
  - **Önizle** -> Komut satırlarının çizimde ne değiştireceğini çizime dokunmadan söyler: çalıştırır, sayar ve bütünüyle geri alır
  - **Geçici İzleme** -> Geçici izleme için nokta işaretler; iki işaretin izleri kesişir
  - **Katmanları Listele** -> Katmanları, nesne sayılarını, görünürlük ve kilit durumlarını listeler
  - **Öznitelik Şeması** -> Çizimde tanımlı öznitelik sütunlarını ve tiplerini listeler
  - **Seçim Bilgisi** -> Kullanıcının o anki seçimini bildirir: kaç nesne ve hangi anahtarlar
  - **Nesne Noktaları** -> Nesnelerin merkezini, köşelerini, uçlarını, kutusunu ya da kenar ortalarını bildirir
  - **Görünüm Bilgisi** -> Ekranda görünen alanın köşe koordinatlarını, merkezini, ölçeğini ve koordinat sistemini bildirir
  - **Bağlam** -> Üzerinde çalışılan her şeyi tek çağrıda özetler: belge sürümü, koordinat sistemi, kapsam, katmanlar, çıktı yerleşimleri, seçili nesneler ve görünüm
  - **Araç Ara** -> Yapay zekâ araç kataloğunda ad ve özete göre arar
  - **İş Şablonu** -> Sık yapılan işlerin — atlas, kadastro kontrolü, parsel raporu — komut satırlarını sırasıyla verir; hiçbirini çalıştırmaz

## Analiz

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Tablo

- **Öznitelik Tablosu** -> Etkin katmanın satırlarını ve sütunlarını öznitelik tablosu penceresinde açar (F6)

### İşlem araçları

- **İşlem Araçları** -> İşlem araçlarının listesini açar (liste işlem kaydından üretilir); adı … ile biten araç önce gereken sayıyı sormak için Araçlar panelini açar, ötekiler seçili nesnelerde öntanımlı ayarlarla hemen çalışır
  - **Tampon bölge…** -> Nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer; mesafeyi Araçlar panelinde yazarsınız
  - **Alanı düzenle…** -> Kapalı bir alanı istenen alana getirir (hedef alanı Araçlar panelinde yazarsınız): kenarları eşit daraltıp genişleterek, bir kenarı kaydırarak ya da bir köşeyi çekerek; şekil bozulmaz
  - **Kenar uzunluklarını yaz** -> Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel ve kenara bağlı bir yazı olarak yazar
  - **Köşeleri numarala** -> Her alanın köşelerini seçtiğiniz köşeden başlayarak sırayla numaralar ve numarayı köşenin dışına yazar; numara köşesine bağlıdır
  - **Yazının bağını çöz** -> Yazıların bağını çözer: yazı yerinde kalır, bağlı olduğu nesne bundan sonra tek başına taşınır
  - **Yazıyı nesneye bağla** -> Kapsamdaki yazıları seçtiğiniz çizgi ya da alanın en yakın kenarına, köşesine ya da ortasına bağlar; nesne taşınınca yazı izler
  - **Çizgilerden alan üret** -> Çizgilerin kapattığı her gözü ayrı bir alan olarak çizer; içerideki adalar delik olur, açık uçlar sayılıp gösterilir
- **Araçlar Paneli** -> Sağ paneldeki Araçlar sekmesini açar
- **Tampon…** -> Nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer; mesafeyi Araçlar panelinde yazarsınız
- **Alan Üret** -> Çizgilerin kapattığı her gözü ayrı bir alan olarak çizer; içerideki adalar delik olur, açık uçlar sayılıp gösterilir

### Denetim

- **Bağımlılıklar** -> Tampon, üretilen alan, sınır ve eş yükselti eğrileri gibi sonuçların kaynaklarına göre güncel olup olmadığını söyler
- **Güncelle** -> Kaynağının gerisinde kalan bağlı yazıları, ölçüleri ve taramaları yetiştirir; güncel olmayan sonuçları yeniden hesaplar
- **Topoloji Denetimi** -> Kendini kesen sınır, sıfır alan, örtüşen parsel, yinelenen ve boş nesne, tekrarlanan köşe ve çizgi ağındaki boşlukları raporlar; hiçbir şeyi düzeltmez
- **Kapsam Denetimi** -> Çizimin çoğunluğundan kopuk nesneleri (0,0'a düşmüş nokta, başka dilimden gelmiş blok gibi) bulur ve işaretler; hiçbirini taşımaz

### Yapay zekâ

- **Yapay Zeka** -> Yapay zekâ sohbet panelini açar: model komut önerir, ne zaman uygulanacağını onay politikanız belirler (Ctrl+Shift+K)
- **MCP Sunucusunu Başlat** -> Yapay zekâ ajanlarının bağlanacağı yerel MCP sunucusunu açar; sunucu açıkken düğmenin adı “MCP Sunucusunu Durdur” olur ve sunucuyu kapatır
- **MCP Belirteci Üret** -> Yeni bir erişim belirteci üretir; eskisi geçersiz olur
- **↘ Yapay zeka modelleri** -> Seçenekler penceresinin Yapay Zeka Modelleri bölümünü açar

## Görünüm

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Gezinme

- **Kapsama Yakınlaş** -> Çizimin tamamını pencereye sığdırır (Ctrl+0)
- **Pencere** -> Görünümü, sürüklediğiniz ya da iki köşesine tıkladığınız pencereye yakınlaştırır (Alt+Z); düğme üzerinde kısa adı yazar, tam adı Pencereyle Yakınlaş
- **Seçime** -> Seçili nesneleri görünüme sığdırır; düğme üzerinde kısa adı yazar, tam adı Seçime Yakınlaş
- **Kaydır** -> Tuttuğunuz bir noktayı başka bir yere taşır; ölçek değişmez (orta fare tuşuyla sürüklemek her zaman çalışır)
- **Önceki** -> Görünüm geçmişinde bir önceki görünüme döner (Alt+C); düğme üzerinde kısa adı yazar, tam adı Önceki Görünüm
- **Sonraki** -> Geri dönülen görünümden bir adım ileri gider; düğme üzerinde kısa adı yazar, tam adı Sonraki Görünüm
- **Yakınlaştır** -> Görünümü yakınlaştırır (çarpan 1,25) (simge düğmesi)
- **Uzaklaştır** -> Görünümü uzaklaştırır (çarpan 0,8) (simge düğmesi)

### Yardımcılar

- **Nesne Yakalama** -> Nesne yakalamayı açar ya da kapatır (F3); açılınca en son açık olan modlar geri gelir
- **Yakalama Modları…** -> Nesne yakalama modlarının listesini açar; işaretli olanlar açıktır (Shift+F3)
  - **uç nokta** -> Bir halkanın köşesine ya da yayın ucuna oturur — parsel köşesi, bina köşesi (işaretlenince açık olur)
  - **orta nokta** -> Bir kenarın (ya da yayın) tam ortasına oturur (işaretlenince açık olur)
  - **merkez** -> Bir eğrinin çizildiği merkeze oturur: dairenin ve yayın merkezi (işaretlenince açık olur)
  - **ağırlık merkezi** -> Kapalı bir halkanın alan ağırlık merkezine — parselin ortasına — oturur (işaretlenince açık olur)
  - **kesişim** -> İki kenarın gerçekten kesiştiği yere oturur (işaretlenince açık olur)
  - **dik ayak** -> Önceki noktadan bir kenara indirilen dikin ayağına oturur (işaretlenince açık olur)
  - **en yakın** -> Kenarın imlece en yakın noktasına — çizginin herhangi bir noktasına — oturur (işaretlenince açık olur)
  - **düğüm** -> Ölçülmüş tek noktaya (nirengi, poligon noktası, röper) oturur (işaretlenince açık olur)
  - **ızgara** -> En yakın ızgara kesişimine oturur (işaretlenince açık olur)
  - **kutupsal** -> Önceki noktadan çıkan en yakın kutupsal ışına oturur (işaretlenince açık olur)
  - **uzantı** -> Bir kenarın kendi ucundan öteye uzanan doğrusuna oturur (işaretlenince açık olur)
  - **paralel** -> Önceki noktadan çıkan, bir kenara paralel ışına oturur (işaretlenince açık olur)
  - **uzatılmış kesişim** -> İki kenarın doğrularının kesişeceği yere oturur; ikisi de oraya kadar uzanmasa bile (işaretlenince açık olur)
  - **kılavuz** -> Cetvelden çektiğiniz kılavuza, iki kılavuz kesişiyorsa kesişimine oturur (işaretlenince açık olur)
  - **ekleme noktası** -> Bir nesnenin yerleştirildiği noktaya — blok referansının ekleme noktası gibi — oturur (işaretlenince açık olur)
  - **çeyrek nokta** -> Daire, yay ve elipsin eksenleri kestiği dört çeyrek noktasına oturur (işaretlenince açık olur)
  - **teğet nokta** -> Son noktadan bir eğriye çizilen teğetin eğriye değdiği noktaya oturur (işaretlenince açık olur)
  - **izleme** -> İşaretlediğiniz bir noktadan geçen yatay ve düşey ize oturur; iki işaretin izleri kesişir (işaretlenince açık olur)
  - **Hepsi** -> Bütün yakalama modlarını açar
  - **Hiçbiri** -> Bütün yakalama modlarını kapatır
  - **Adım: (değer) m…** -> Adım uzunluğunu sorar (henüz verilmemişse satırda “Adım: yok…” yazar): imlecin bir önceki noktaya uzaklığı bu değerin katlarında durur; 0 kapatır
- **Dik Mod** -> Açıkken imleci yatay ve düşey eksene kilitler (F8)
- **Yüzey Normali** -> Açıkken çizgiyi başladığı yüzeye (kenara) dik kilitler (F10; tuval üzerinde Shift basılıyken de çalışır)
- **Izgaraya Yakala** -> Açıkken noktayı en yakın ızgara kesişimine oturtur (F9)
- **↘ Yakalama modları** -> Yakalama Modları listesini açar

### Katmanlar

- **Katmanlar** -> Katmanlar panelini açar: her katmanın görünürlüğü, kilidi, rengi ve stili
- **Gizle** -> Seçili nesnenin katmanını — seçim yoksa etkin katmanı — gizler
- **Yalnız Bu** -> Yalnız seçili nesnenin katmanı — seçim yoksa etkin katman — görünür kalır, diğerleri gizlenir
- **Tümünü Göster** -> Gizli bütün katmanları geri getirir
- **Gösterimi Ters Çevir** -> Görünen katmanları gizler, gizli olanları gösterir
- **Stil Tasarımcısı** -> Stil Tasarımcısı penceresini açar: katmanın nasıl çizileceğini fare ile tasarlarsınız

### Pencereler

- **Katmanlar** -> Katmanlar panelini gösterir ya da gizler (işaretliyken açıktır)
- **Öznitelikler** -> Sağ paneli (Öznitelikler, Geçmiş ve Araçlar sekmeleri) gösterir ya da gizler (işaretliyken açıktır)
- **Yapay Zeka** -> Yapay zekâ sohbet panelini gösterir ya da gizler (işaretliyken açıktır)
- **Komut Günlüğü** -> Çalışan her komutun JSON olarak yazıldığı günlük panelini gösterir ya da gizler (işaretliyken açıktır)
- **Python Konsolu** -> Python konsolu panelini gösterir ya da gizler (işaretliyken açıktır)
- **Komut Satırı** -> Komut satırını gösterir ya da gizler (Ctrl+9; işaretliyken açıktır)
- **Yerleşimi Sıfırla** -> Panelleri yüzen durumdan çıkarıp program ilk açıldığındaki yuvalarına ve boyutlarına döndürür; Öznitelikler, Katmanlar ve Yapay Zeka panelleri açılır

### Tema

- **Koyu Tema** -> Koyu ve açık tema arasında geçer; düğme işaretliyken koyu tema kullanılır
- **Geliştirici Bilgisi** -> Kare süresi, çizilen nesne sayısı ve arka uç bilgisini tuvalde gösteren geliştirici katmanını açar ya da kapatır (F12) (simge düğmesi)
- **↘ Görünüm ve tema ayarları** -> Seçenekler penceresinin Görünüm ve Tema bölümünü açar

### Diğer komutlar

- **Diğer** -> Şeritte düğmesi olmayan görünüm ve katman komutlarını listeler; liste komut kaydından üretilir
  - **Katman Görünümü** -> Katmanların görünürlüğünü toptan değiştirir: bir katmanı gösterir ya da gizler, yalnız onu bırakır, hepsini gösterir veya görünürlüğü ters çevirir
  - **Stil** -> Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar
  - **Sembol** -> MPYY gösterim rafını yükler, ağacında gezer ve içinde arar

## Çıktı

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Yazdır

- **Yazdır** -> Bölünmüş düğme: yüzü yazdırma çerçevesini tuvalde açar, ikinci basış önizlemeyi açar ve PDF ya da yazıcıya gönderir (Ctrl+P); oku yazdırma profillerini ve yerleşimleri listeler
  - **Yazdırma profilleri** -> Menünün ilk satırları yazdırma profilleridir; varsayılan profil ● ile işaretlidir, birini seçmek o kâğıtla yazdırma çerçevesini açar; yeni kurulumdaki değerler şunlardır: A4 Dikey (varsayılan), A4 Yatay, A3 Yatay, A2 Yatay, A1 Yatay, A0 Yatay
  - **Çıktı Yerleşimleri** -> Çizimde yerleşim varsa başlık satırıdır; altındaki her yerleşim adını seçmek tuvalden alan seçtirir ve tasarımcıyı açar
  - **Yeni çıktı yerleşimi…** -> Başlık, harita çerçevesi, ölçek çubuğu ve kuzey oku ile gelen yeni bir çıktı yerleşimi açar
  - **Profilleri Yönet…** -> Seçenekler penceresinin Plot ve Çıktı bölümünü açar
- **Yerleşimler** -> Çizimin çıktı yerleşimlerinin (başlıklı, lejantlı pafta düzenleri) menüsünü açar
  - **Yeni Çıktı Yerleşimi…** -> Başlık, harita çerçevesi, ölçek çubuğu ve kuzey oku ile gelen yeni bir çıktı yerleşimi açar
  - **Çıktı Yerleşimi Yöneticisi…** -> Çizimdeki yerleşimleri listeler: aç, yeniden adlandır, çoğalt, sil (Ctrl+Shift+P)
  - **Şablonlar** -> Kurumun kayıtlı yerleşim şablonlarını içeren alt menü; bir şablonu seçmek bu çizimde o şablondan yerleşim kurar, “Yerleşimi Şablon Olarak Kaydet…” geçerli yerleşimi şablon olarak saklar; kayıtlı şablon yoksa “(kayıtlı şablon yok)” yazar
  - **(yerleşim adı)** -> Çizimdeki her yerleşim için bir alt menü: Tasarımcıyı Aç, Tuvalden Alan Seç… (haritanın bakacağı alanı tuvalden çerçeveler), PDF'e Aktar…; yerleşim yoksa “(çizimde çıktı yerleşimi yok)” yazar
- **Ölçek** -> Pafta ölçeğini belirler: kâğıtta bildirilen her boyun zeminde ne kadar yer tuttuğunu söyler (projenin plan ölçeği ayarıdır); kutuya 1:1000, 1/1000 ya da 1000 yazılabilir; değerler şunlardır: 1:500, 1:1000, 1:2000, 1:5000, 1:10000, 1:25000
- **↘ Yazdırma profilleri** -> Seçenekler penceresinin Plot ve Çıktı bölümünü açar

### Dosya

- **Dışa Aktar…** -> Çizimdeki görünür nesneleri dış bir veri biçimine (örneğin GeoPackage, DXF) yazar
- **Kaydet** -> Çizimi bağlı olduğu proje dosyasına yazar (Ctrl+S)
- **Farklı Kaydet…** -> Çizimi yeni bir proje dosyasına yazar ve çizimi o dosyaya bağlar (Ctrl+Shift+S)

## Yazı (bağlamsal sekme)

Bir yazı nesnesi seçilince sekme satırının sonunda belirir, seçim boşalınca kapanır; yalnız yazılar seçiliyse öne gelir.

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Düzenle

- **Yazıyı Düzenle** -> Çizimde duran yazının metnini, yüksekliğini, hizalamasını ya da satır aralığını değiştirir
- **Bul ve Değiştir…** -> Yazılarda bir sözcüğü bulur, önizler ve hepsinde birden değiştirir; tek geri alma adımıdır (Ctrl+H; macOS'ta Cmd+Option+F)
- **Stil Kopyala** -> Bir nesnenin görünümünü (stilini) seçili nesnelere uygular; geometri ve öznitelikler değişmez

### Biçim

- **Yükseklik** -> Seçili yazıların yüksekliğini değiştirir (zeminde metre); kutu ilk seçili yazının değerini gösterir, başka bir sayı da yazılabilir; değerler şunlardır: 0,50 m, 1,00 m, 1,50 m, 2,00 m, 2,50 m, 3,00 m, 3,50 m, 5,00 m, 7,00 m, 10,00 m
- **Aralık** -> Seçili yazıların satır aralığını değiştirir; değerler şunlardır: 1,00, 1,15, 1,50, 2,00, 2,50, 3,00

### Hizalama

- **Hizalama ızgarası (3 × 3)** -> Seçili yazıların yerleştirme noktasını seçtiğiniz 3 × 3 simge ızgarası; basılı olan ilk seçili yazınınkidir; değerler şunlardır: Üst sol, Üst orta, Üst sağ, Orta sol, Merkez, Orta sağ, Taban sol, Taban orta, Taban sağ
  - **Üst sol** -> Yazının yerleştirme noktası yazının sol üst köşesindedir
  - **Üst orta** -> Yerleştirme noktası yazının üst kenarının ortasındadır
  - **Üst sağ** -> Yerleştirme noktası yazının sağ üst köşesindedir
  - **Orta sol** -> Yerleştirme noktası yazının sol kenarının ortasındadır
  - **Merkez** -> Yerleştirme noktası yazının tam ortasındadır
  - **Orta sağ** -> Yerleştirme noktası yazının sağ kenarının ortasındadır
  - **Taban sol** -> Yerleştirme noktası taban çizgisinin sol ucundadır
  - **Taban orta** -> Yerleştirme noktası taban çizgisinin ortasındadır
  - **Taban sağ** -> Yerleştirme noktası taban çizgisinin sağ ucundadır

### Bağ

- **Bağla** -> Kapsamdaki yazıları seçtiğiniz çizgi ya da alanın en yakın kenarına, köşesine ya da ortasına bağlar; nesne taşınınca yazı izler
- **Bağı Çöz** -> Yazıların bağını çözer: yazı yerinde kalır, bağlı olduğu nesne bundan sonra tek başına taşınır

### Kapat

- **Seçimi Bırak** -> Seçimi boşaltır; bu sekme de kapanır (Esc)

## Ölçü (bağlamsal sekme)

Bir ölçü nesnesi seçilince sekme satırının sonunda belirir, seçim boşalınca kapanır; yalnız ölçüler seçiliyse öne gelir.

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Düzenle

- **Ölçüyü Düzenle** -> Çizilmiş ölçünün yazısını, önek ve sonekini, toleransını, birimini, ondalığını, stilini ya da yazı yerini değiştirir; ölçülen değer değişmez
- **Stile Döndür** -> Seçili ölçülerin yazısını, önekini, sonekini, toleransını, birimini, ondalığını ve yazı yerini stile döndürür
- **Pafta Ölçeğine Uyarla** -> Seçili ölçüleri plan ölçeğine uyarlar: oklar, uzatma çizgileri ve yazılar kâğıtta stilin boyunda kalır

### Stil ve Değer

- **Stil** -> Seçili ölçülerin stilini değiştirir; kutu ilk seçili ölçünün stilini gösterir; değerler şunlardır: ISO-25, ISO-18, ISO-35, STANDARD, MIMARI
  - **ISO-25** -> ISO metrik varsayılan: kapalı ok 2,5 mm, uzatma fazlası 1,25 mm, iki ondalık, virgül; seçili ölçülerin stili bu olur
  - **ISO-18** -> ISO 3098-1 dizisinin 1,8 mm yazısı: sık paftada kapalı ok 1,8 mm, iki ondalık, virgül; seçili ölçülerin stili bu olur
  - **ISO-35** -> ISO 3098-1 dizisinin 3,5 mm yazısı: seyrek ya da uzaktan okunacak paftada kapalı ok 3,5 mm, iki ondalık, virgül; seçili ölçülerin stili bu olur
  - **STANDARD** -> AutoCAD STANDARD: kapalı ok 1,8 mm, dört ondalık, nokta; seçili ölçülerin stili bu olur
  - **MIMARI** -> Mimari çizgi ucu: 45° çentik, iki ondalık, virgül; seçili ölçülerin stili bu olur
- **Ondalık** -> Seçili ölçülerin yazısındaki ondalık basamak sayısını belirler; değerler şunlardır: 0, 0,0, 0,00, 0,000, 0,0000
- **Birim** -> Seçili ölçülerin yazıldığı birimi belirler; değerler şunlardır: çizimin (çizimin kendi birimi), mm, cm, m, km

### Devam

- **Zincir Ölçü** -> Son ölçünün ucundan aynı çizgide art arda ölçer; noktaları tıklarsınız, Enter bitirir ve toplamı söyler
- **Baz Ölçü** -> Son ölçünün ilk noktasından ölçer, ölçü çizgilerini stilin aralığıyla üst üste dizer; Enter bitirir

### Kapat

- **Seçimi Bırak** -> Seçimi boşaltır; bu sekme de kapanır (Esc)

## Tarama (bağlamsal sekme)

Bir tarama nesnesi seçilince sekme satırının sonunda belirir, seçim boşalınca kapanır; yalnız taramalar seçiliyse öne gelir.

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Desen

- **Desenler** -> Seçili taramanın desenini değiştiren galeri; kullanılan desen işaretli görünür; değerler şunlardır: SOLID, ANSI31, ANSI32, ANSI33, ANSI34, ANSI37, LINE, NET, DOTS, EARTH, GRASS
  - **SOLID** -> Dolu dolgu; seçili taramanın desenini bu yapar
  - **ANSI31** -> 45° tek yönlü çizgiler, 3,175 mm aralık (demir, tuğla, taş); seçili taramanın desenini bu yapar
  - **ANSI32** -> 45° çift çizgi, 9,525 mm aralık (çelik); seçili taramanın desenini bu yapar
  - **ANSI33** -> 45° çizgi ve kesikli çizgi, 6,35 mm aralık (bronz, pirinç, bakır); seçili taramanın desenini bu yapar
  - **ANSI34** -> 45° dört çizgi, 19,05 mm aralık (plastik, kauçuk); seçili taramanın desenini bu yapar
  - **ANSI37** -> 45° ve 135° çapraz çizgiler, 3,175 mm aralık (kurşun, çinko, magnezyum); seçili taramanın desenini bu yapar
  - **LINE** -> Yatay paralel çizgiler, 3,175 mm aralık; seçili taramanın desenini bu yapar
  - **NET** -> Yatay ve düşey kare ağ, 3,175 mm aralık; seçili taramanın desenini bu yapar
  - **DOTS** -> Nokta deseni, 1,5875 mm aralık; seçili taramanın desenini bu yapar
  - **EARTH** -> Toprak: 45° ve 135° kesikli çizgiler, 6,35 mm aralık; seçili taramanın desenini bu yapar
  - **GRASS** -> Çim: 45° ve 135° kısa çizgiler, 44,45 mm aralık; seçili taramanın desenini bu yapar

### Özellikler

- **Açı** -> Seçili taramaların desen açısını derece olarak belirler; kutuya başka bir sayı da yazılabilir; değerler şunlardır: 0°, 15°, 30°, 45°, 60°, 90°, 135°
- **Ölçek** -> Seçili taramaların desen ölçeğini belirler; kutuya başka bir sayı da yazılabilir; değerler şunlardır: 0,5; 1; 2; 5; 10; 100; 500; 1.000
- **Çapraz** -> Açıkken desen bir de dik açıyla çizilir (çapraz tarama); anahtardır, işaretliyken açıktır

### Adalar

- **Normal** -> İç içe sınırlar sırayla delik ve dolu olur: adanın içi boş, onun içindeki ada yine taralı
- **Yalnız dış** -> Yalnız en dıştaki alan taranır; ilk ada ve içindekiler boş kalır
- **Adasız** -> Adalar yok sayılır; dış sınırın bütün içi taranır

### Sınır

- **Sınır Bul** -> İçine tıkladığınız kapalı bölgenin sınırını yeni bir alan olarak çıkarır; içerideki adalar delik olur, kapanmıyorsa açık uçları gösterir
- **Taramayı Düzenle** -> Çizilmiş taramanın desenini, açısını, ölçeğini, aralığını, çapraz çizimini ve ada kuralını değiştirir; bağı ve sınırı korunur

### Kapat

- **Seçimi Bırak** -> Seçimi boşaltır; bu sekme de kapanır (Esc)

## Alan (bağlamsal sekme)

Kapalı bir alan (parsel) seçilince, kenarı yaylı olsa da, sekme satırının sonunda belirir ve yalnız alanlar seçiliyse öne gelir.

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Ölç ve Yaz

- **Alan Ölç** -> Seçili nesnelerin alanını ve çevresini yazar; sonuç tuvalde kalır
- **Nesne Bilgisi** -> Seçtiğiniz nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve özniteliklerini söyler
- **Koordinat Oku** -> Tıkladığınız noktanın sağa ve yukarı değerini çizimin koordinat sisteminde yazar
- **Köşe Numarala** -> Her alanın köşelerini seçtiğiniz köşeden başlayarak sırayla numaralar ve numarayı köşenin dışına yazar; numara köşesine bağlıdır
- **Uzunluk Yaz** -> Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel ve kenara bağlı bir yazı olarak yazar
- **↘ Numaralama ve uzunluk yazma ayarları — Araçlar paneli** -> Araçlar panelinde köşe numaralama aracının ayarlarını açar

### Kadastro

- **İfraz** -> Bir parseli düz bir ayırma çizgisiyle ikiye böler; alan kaybolmaz, parçaların ve toplamın alanı yazılır
- **Alana Göre İfraz** -> Parselden verilen yöne paralel, istenen alanda bir parça ayırır
- **Tevhit** -> Komşu parselleri tek parselde birleştirir; bitişik olmayan parselleri reddeder
- **Topoloji Denetimi** -> Kendini kesen sınır, sıfır alan, örtüşen parsel, yinelenen ve boş nesne, tekrarlanan köşe ve çizgi ağındaki boşlukları raporlar; hiçbir şeyi düzeltmez

### Kes ve Köşe

- **Böl** -> Bölünmüş düğme: parseli böler; yüzü en son kullanılan yöntemi çalıştırır, oku beş yöntemi listeler (alanı yalnız kesme çizgisiyle böler)
  - **Böl** -> Çizdiğiniz kesme çizgisiyle böler: çizgi, yay, daire, yaylı çoklu çizgi ve alan; bütün parçaları tutar
  - **Böl — noktalardan** -> Nesnenin üstüne tıkladığınız noktalardan böler; parçalar Enter'dan önce görünür, ⌫ son noktayı geri alır
  - **Böl — kesişimlerden** -> Seçtiğiniz nesneleri birbirini kestikleri her yerden böler
  - **Böl — eşit parçaya** -> Seçtiğiniz nesneleri verdiğiniz sayıda eşit parçaya böler
  - **Böl — baştan uzaklıkla** -> Seçtiğiniz nesneleri başından verdiğiniz uzaklıkta böler
- **Yuvarla** -> Bölünmüş düğme: köşe yuvarlama; yüzü en son kullanılanı çalıştırır, oku iki yöntemi listeler
  - **Yuvarla** -> Bir köşeyi ya da iki nesne arasındaki köşeyi verdiğiniz yarıçapta teğet yayla yuvarlatır; 0 keskin köşe kurar
  - **Yuvarla — bütün köşeler** -> Çizgiye ya da alana tıklarsınız; bütün köşelerini aynı yarıçapla yuvarlatır
- **Pah** -> Bölünmüş düğme: pah kırma; yüzü en son kullanılanı çalıştırır, oku iki yöntemi listeler
  - **Pah** -> Bir köşeyi ya da iki çizgi arasındaki köşeyi düz bir kenarla keser (pah kırar); mesafe yazılır ya da gösterilir
  - **Pah — bütün köşeler** -> Çizgiye ya da alana tıklarsınız; bütün köşelerine aynı mesafeyle pah kırar
- **Köşe Taşı** -> Köşeye tıklarsınız, yeni yerini gösterirsiniz; kenarlar imleci izler
- **Köşe Ekle** -> Kenara tıklarsınız, yeni köşenin yerini gösterirsiniz
- **Köşe Sil** -> Köşeye tıklarsınız; köşede buluşan iki kenar tek kenar olur; seçili parsellerin ortak köşesi ikisinden birden silinir
- **Kenar Türü** -> Kenara tıklarsınız: düz kenar gösterdiğiniz noktadan geçen yaya, yay düz kenara döner; nesnenin kimliği korunur

### Düzenle

- **Tarama** -> Kapalı nesnelerin içini katalogdaki bir desenle tarar; tarama sınırına bağlıdır, sınır değişince güncellenir
- **Ofset** -> Seçili nesnelerin verdiğiniz mesafede, gösterdiğiniz tarafta paralelini çizer; özgün nesne yerinde kalır
- **Tampon…** -> Nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer; mesafeyi Araçlar panelinde yazarsınız
- **Alanı Düzenle…** -> Kapalı bir alanı istenen alana getirir (hedef alanı Araçlar panelinde yazarsınız): kenarları eşit daraltıp genişleterek, bir kenarı kaydırarak ya da bir köşeyi çekerek; şekil bozulmaz
- **Patlat** -> Çizgiyi kenarlara, alanı sınırına, yaylı çizgiyi çizgi ve yaylarına, bloğu bileşenlerine ayırır

### Nesne

- **Taşı** -> Seçili nesneleri iki nokta arasındaki kadar taşır
- **Kopyala** -> Seçili nesnelerin kopyasını, başlangıçtan gösterdiğiniz her noktaya kadar öteleyerek koyar; her tıklama bir kopya daha koyar
- **Döndür** -> Bölünmüş düğme: yüzü en son kullanılan döndürme yöntemini çalıştırır
  - **Döndür** -> Seçili nesneleri bir merkez etrafında döndürür; açı yazılır ya da gösterilir, artı açı saat yönünün tersinedir
  - **Döndür — referansla** -> İki noktayla gösterilen doğrultuyu yeni doğrultuya döndürür; dönme açısı ikisinin farkıdır
- **Ölçekle** -> Bölünmüş düğme: yüzü en son kullanılan ölçekleme yöntemini çalıştırır
  - **Ölçekle** -> Seçili nesneleri bir merkeze göre çarpan kadar büyütür ya da küçültür; alanlar çarpanın karesiyle değişir
  - **Ölçekle — referansla** -> İki noktayla gösterilen uzunluğu yeni uzunluğa getirir; çarpan ikisinin oranıdır
- **Aynala** -> Bölünmüş düğme: yüzü en son kullanılan aynalama yöntemini çalıştırır
  - **Aynala** -> Seçili nesneleri iki noktadan geçen eksende yansıtır; yazılar okunur kalır
  - **Aynala — kopyalayarak** -> Özgün nesne yerinde kalır, aynalanmış kopyası çizilir
- **Sil** -> Seçili nesneleri siler (Del); geri alınabilir

### Kapat

- **Seçimi Bırak** -> Seçimi boşaltır; bu sekme de kapanır (Esc)

## Çizgi (bağlamsal sekme)

Açık bir çizgi ya da çoklu çizgi seçilince sekme satırının sonunda belirir ve yalnız çizgiler seçiliyse öne gelir.

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Kes ve Uzat

- **Buda** -> Bölünmüş düğme: budama yöntemleri; yüzü en son kullanılanı çalıştırır, oku dört yöntemi listeler
  - **Buda** -> Tıkladığınız parçayı kesme sınırları arasından atar: çizgide, yayda, dairede, elipste ve spline'da; sınır olarak seçili nesneler, seçim yoksa yakındaki görünür nesneler kullanılır
  - **Buda — çitle** -> Çizdiğiniz çitin geçtiği bütün parçaları tek seferde budar; Enter uygular
  - **Buda — tıklanan kalsın** -> Tıkladığınız parça kalır, iki yanındaki kesimlerin dışında kalan parçalar gider
  - **Buda — sınırları uzatarak** -> Nesneye yetişmeyen bir sınır kendi doğrultusunda uzatılmış sayılır
- **Uzat** -> Bölünmüş düğme: uzatma yöntemleri; yüzü en son kullanılanı çalıştırır, oku üç yöntemi listeler
  - **Uzat** -> Tıklanan ucu ulaştığı ilk sınıra kadar uzatır: çizginin ucunu doğrultusunda, yayın ucunu çemberi boyunca
  - **Uzat — çitle** -> Çizdiğiniz çitin yanından geçtiği bütün uçları sınıra uzatır; Enter uygular
  - **Uzat — sınırları uzatarak** -> Uca yetişmeyen bir sınır kendi doğrultusunda uzatılmış sayılır
- **Kır** -> İki nokta arasındaki parçayı çıkarır; tek nokta verilirse açık nesneyi boşluk bırakmadan böler
- **Uzunluk** -> Çizginin bir ucunu kendi doğrultusunda hareket ettirerek uzunluğunu değiştirir
- **Böl** -> Bölünmüş düğme: çizgiyi parçalara ayırır; yüzü en son kullanılan yöntemi çalıştırır, oku beş yöntemi listeler
  - **Böl** -> Çizdiğiniz kesme çizgisiyle böler: çizgi, yay, daire, yaylı çoklu çizgi ve alan; bütün parçaları tutar
  - **Böl — noktalardan** -> Nesnenin üstüne tıkladığınız noktalardan böler; parçalar Enter'dan önce görünür, ⌫ son noktayı geri alır
  - **Böl — kesişimlerden** -> Seçtiğiniz nesneleri birbirini kestikleri her yerden böler
  - **Böl — eşit parçaya** -> Seçtiğiniz nesneleri verdiğiniz sayıda eşit parçaya böler
  - **Böl — baştan uzaklıkla** -> Seçtiğiniz nesneleri başından verdiğiniz uzaklıkta böler

### Köşe

- **Yuvarla** -> Bölünmüş düğme: köşe yuvarlama; yüzü en son kullanılanı çalıştırır, oku iki yöntemi listeler
  - **Yuvarla** -> Bir köşeyi ya da iki nesne arasındaki köşeyi verdiğiniz yarıçapta teğet yayla yuvarlatır; 0 keskin köşe kurar
  - **Yuvarla — bütün köşeler** -> Çizgiye ya da alana tıklarsınız; bütün köşelerini aynı yarıçapla yuvarlatır
- **Pah** -> Bölünmüş düğme: pah kırma; yüzü en son kullanılanı çalıştırır, oku iki yöntemi listeler
  - **Pah** -> Bir köşeyi ya da iki çizgi arasındaki köşeyi düz bir kenarla keser (pah kırar); mesafe yazılır ya da gösterilir
  - **Pah — bütün köşeler** -> Çizgiye ya da alana tıklarsınız; bütün köşelerine aynı mesafeyle pah kırar
- **Köşe Taşı** -> Köşeye tıklarsınız, yeni yerini gösterirsiniz; kenarlar imleci izler
- **Köşe Ekle** -> Kenara tıklarsınız, yeni köşenin yerini gösterirsiniz
- **Köşe Sil** -> Köşeye tıklarsınız; köşede buluşan iki kenar tek kenar olur; seçili parsellerin ortak köşesi ikisinden birden silinir

### Dönüştür

- **Alana Çevir** -> Uç uca değen çizgilerden tek bir kapalı alan kurar ve kaynak çizgileri siler
- **Uç Uca Ekle** -> Uçları değen çizgileri, yayları ve yaylı çoklu çizgileri tek nesneye ekler; yaylar yay kalır
- **Çizgi Düzenle** -> Çizgiyi kapatır, açar, yönünü çevirir ya da yakın köşelerini atarak sadeleştirir
- **Kenar Türü** -> Kenara tıklarsınız: düz kenar gösterdiğiniz noktadan geçen yaya, yay düz kenara döner; nesnenin kimliği korunur
- **Patlat** -> Çizgiyi kenarlara, alanı sınırına, yaylı çizgiyi çizgi ve yaylarına, bloğu bileşenlerine ayırır
- **Bölümle** -> Bir nesne boyunca eşit parçalara bölerek ya da sabit aralıkla nokta (ya da blok) yerleştirir

### Ofset ve Yaz

- **Ofset** -> Seçili nesnelerin verdiğiniz mesafede, gösterdiğiniz tarafta paralelini çizer; özgün nesne yerinde kalır
- **Uzunluk Yaz** -> Kapsamdaki her çizginin ve alanın her kenarına uzunluğunu, kenara paralel ve kenara bağlı bir yazı olarak yazar
- **Tampon…** -> Nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer; mesafeyi Araçlar panelinde yazarsınız
- **Nesne Bilgisi** -> Seçtiğiniz nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve özniteliklerini söyler

### Nesne

- **Taşı** -> Seçili nesneleri iki nokta arasındaki kadar taşır
- **Kopyala** -> Seçili nesnelerin kopyasını, başlangıçtan gösterdiğiniz her noktaya kadar öteleyerek koyar; her tıklama bir kopya daha koyar
- **Döndür** -> Bölünmüş düğme: yüzü en son kullanılan döndürme yöntemini çalıştırır
  - **Döndür** -> Seçili nesneleri bir merkez etrafında döndürür; açı yazılır ya da gösterilir, artı açı saat yönünün tersinedir
  - **Döndür — referansla** -> İki noktayla gösterilen doğrultuyu yeni doğrultuya döndürür; dönme açısı ikisinin farkıdır
- **Ölçekle** -> Bölünmüş düğme: yüzü en son kullanılan ölçekleme yöntemini çalıştırır
  - **Ölçekle** -> Seçili nesneleri bir merkeze göre çarpan kadar büyütür ya da küçültür; alanlar çarpanın karesiyle değişir
  - **Ölçekle — referansla** -> İki noktayla gösterilen uzunluğu yeni uzunluğa getirir; çarpan ikisinin oranıdır
- **Aynala** -> Bölünmüş düğme: yüzü en son kullanılan aynalama yöntemini çalıştırır
  - **Aynala** -> Seçili nesneleri iki noktadan geçen eksende yansıtır; yazılar okunur kalır
  - **Aynala — kopyalayarak** -> Özgün nesne yerinde kalır, aynalanmış kopyası çizilir
- **Sil** -> Seçili nesneleri siler (Del); geri alınabilir

### Kapat

- **Seçimi Bırak** -> Seçimi boşaltır; bu sekme de kapanır (Esc)

## Eğri (bağlamsal sekme)

Bir daire, yay, elips ya da spline seçilince sekme satırının sonunda belirir ve yalnız eğriler seçiliyse öne gelir.

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Ölç

- **Alan Ölç** -> Seçili nesnelerin alanını ve çevresini yazar; sonuç tuvalde kalır; yayda ve açık eğride alan yoktur, uzunluğu yazar
- **Nesne Bilgisi** -> Seçtiğiniz nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve özniteliklerini söyler
- **Koordinat Oku** -> Tıkladığınız noktanın sağa ve yukarı değerini çizimin koordinat sisteminde yazar

### Kes ve Uzat

- **Buda** -> Bölünmüş düğme: budama yöntemleri; yüzü en son kullanılanı çalıştırır, oku dört yöntemi listeler
  - **Buda** -> Tıkladığınız parçayı kesme sınırları arasından atar: çizgide, yayda, dairede, elipste ve spline'da; sınır olarak seçili nesneler, seçim yoksa yakındaki görünür nesneler kullanılır
  - **Buda — çitle** -> Çizdiğiniz çitin geçtiği bütün parçaları tek seferde budar; Enter uygular
  - **Buda — tıklanan kalsın** -> Tıkladığınız parça kalır, iki yanındaki kesimlerin dışında kalan parçalar gider
  - **Buda — sınırları uzatarak** -> Nesneye yetişmeyen bir sınır kendi doğrultusunda uzatılmış sayılır
- **Uzat** -> Bölünmüş düğme: uzatma yöntemleri; yüzü en son kullanılanı çalıştırır, oku üç yöntemi listeler
  - **Uzat** -> Tıklanan ucu ulaştığı ilk sınıra kadar uzatır: çizginin ucunu doğrultusunda, yayın ucunu çemberi boyunca
  - **Uzat — çitle** -> Çizdiğiniz çitin yanından geçtiği bütün uçları sınıra uzatır; Enter uygular
  - **Uzat — sınırları uzatarak** -> Uca yetişmeyen bir sınır kendi doğrultusunda uzatılmış sayılır
- **Kır** -> İki nokta arasındaki parçayı çıkarır; tek nokta verilirse açık nesneyi boşluk bırakmadan böler
- **Uzunluk** -> Çizginin bir ucunu kendi doğrultusunda hareket ettirerek uzunluğunu değiştirir
- **Böl** -> Bölünmüş düğme: eğriyi parçalara ayırır; yüzü en son kullanılan yöntemi çalıştırır, oku beş yöntemi listeler
  - **Böl** -> Çizdiğiniz kesme çizgisiyle böler: çizgi, yay, daire, yaylı çoklu çizgi ve alan; bütün parçaları tutar
  - **Böl — noktalardan** -> Nesnenin üstüne tıkladığınız noktalardan böler; parçalar Enter'dan önce görünür, ⌫ son noktayı geri alır
  - **Böl — kesişimlerden** -> Seçtiğiniz nesneleri birbirini kestikleri her yerden böler
  - **Böl — eşit parçaya** -> Seçtiğiniz nesneleri verdiğiniz sayıda eşit parçaya böler
  - **Böl — baştan uzaklıkla** -> Seçtiğiniz nesneleri başından verdiğiniz uzaklıkta böler

### Dönüştür

- **Ofset** -> Seçili nesnelerin verdiğiniz mesafede, gösterdiğiniz tarafta paralelini çizer; özgün nesne yerinde kalır
- **Tarama** -> Kapalı nesnelerin içini katalogdaki bir desenle tarar; tarama sınırına bağlıdır, sınır değişince güncellenir
- **Bölümle** -> Bir nesne boyunca eşit parçalara bölerek ya da sabit aralıkla nokta (ya da blok) yerleştirir
- **Tampon…** -> Nesnelerin verilen mesafe içindeki bütün zeminini alan olarak çizer; mesafeyi Araçlar panelinde yazarsınız

### Nesne

- **Taşı** -> Seçili nesneleri iki nokta arasındaki kadar taşır
- **Kopyala** -> Seçili nesnelerin kopyasını, başlangıçtan gösterdiğiniz her noktaya kadar öteleyerek koyar; her tıklama bir kopya daha koyar
- **Döndür** -> Bölünmüş düğme: yüzü en son kullanılan döndürme yöntemini çalıştırır
  - **Döndür** -> Seçili nesneleri bir merkez etrafında döndürür; açı yazılır ya da gösterilir, artı açı saat yönünün tersinedir
  - **Döndür — referansla** -> İki noktayla gösterilen doğrultuyu yeni doğrultuya döndürür; dönme açısı ikisinin farkıdır
- **Ölçekle** -> Bölünmüş düğme: yüzü en son kullanılan ölçekleme yöntemini çalıştırır
  - **Ölçekle** -> Seçili nesneleri bir merkeze göre çarpan kadar büyütür ya da küçültür; alanlar çarpanın karesiyle değişir
  - **Ölçekle — referansla** -> İki noktayla gösterilen uzunluğu yeni uzunluğa getirir; çarpan ikisinin oranıdır
- **Aynala** -> Bölünmüş düğme: yüzü en son kullanılan aynalama yöntemini çalıştırır
  - **Aynala** -> Seçili nesneleri iki noktadan geçen eksende yansıtır; yazılar okunur kalır
  - **Aynala — kopyalayarak** -> Özgün nesne yerinde kalır, aynalanmış kopyası çizilir
- **Sil** -> Seçili nesneleri siler (Del); geri alınabilir

### Kapat

- **Seçimi Bırak** -> Seçimi boşaltır; bu sekme de kapanır (Esc)

## Blok (bağlamsal sekme)

Bir blok referansı seçilince sekme satırının sonunda belirir ve yalnız bloklar seçiliyse öne gelir.

### Seçim

- **Seç** -> Seçim aracını eline alır: elindeki aracı bırakır, çalışan komutu iptal eder (Esc); okunda diğer seçme yolları vardır
  - **Alan Seç** -> İki köşe tıklanır; soldan sağa çizilen kutu içinde kalanları, sağdan sola çizilen kutu değdiklerini de seçer
  - **Tümünü Seç** -> Görünür bütün nesneleri seçer (Ctrl+A)
  - **Seçimi Temizle** -> Seçimi boşaltır (Ctrl+Shift+A)

### Blok

- **Bloğu Düzenle** -> Seçili bloğun tanımını düzenlemeye açar; Bloğu Kaydet ile bütün referanslar yeni biçimi çizer
- **Taban Noktası** -> Seçili bloğun taban noktasını taşır; referanslar çizildikleri yerde kalır, bundan sonra Blok Ekle bloğu yeni noktadan yerleştirir
- **Patlat** -> Çizgiyi kenarlara, alanı sınırına, yaylı çizgiyi çizgi ve yaylarına, bloğu bileşenlerine ayırır
- **Blok Ekle** -> Tanımlı bir bloğu bir noktaya ölçek, açı ve diziyle yerleştirir
- **Blok** -> Seçilen nesnelerden adlı bir blok tanımlar ve yerlerine bir referans koyar
- **Nesne Bilgisi** -> Seçtiğiniz nesnenin türünü, katmanını, köşe sayısını, çevresini, alanını ve özniteliklerini söyler
- **Dış Referansları Yenile** -> Bağlı dış referansları dosyalarından yeniden okur

### Kırpma

- **Kırp** -> Blok ya da dış referansı iki köşeli bir dikdörtgenle kırpar: içi çizilir, dışı çizilmez ve yakalanmaz
- **Çokgenle Kırp** -> Kırpma sınırını köşe köşe çizerek kırpar; Enter sınırı kapatır
- **Nesneyle Kırp** -> Çizimdeki kapalı bir çizgi, alan, daire ya da elipsle kırpar
- **Kırpma Sınırını Çiz** -> Kırpma sınırını etkin katmana kapalı çizgi olarak çizer
- **Kırpmayı Kaldır** -> Kırpma sınırını kaldırır; referans yeniden bütün çizilir

### Kapat

- **Seçimi Bırak** -> Seçimi boşaltır; bu sekme de kapanır (Esc)

## Blok Düzenleme (bağlamsal sekme)

Bir blok düzenlemeye açıkken — Bloğu Düzenle ile ya da bloğa çift tıklayarak — seçimden bağımsız olarak belirir; adı “Blok” görünür.

### Düzenlemeyi Bitir

- **Bloğu Kaydet** -> Bloğun tanımını düzenlenen nesnelerden yeniden kurar; bütün referanslar yeni biçimi çizer
- **Vazgeç** -> Açılan nesneleri kaldırır, blok tanımı değişmez

### Bloğun İçinde

- **Çizgi** -> Noktaları sırayla birleştiren doğru parçaları çizer; her parça ayrı bir nesnedir
- **Daire** -> Merkez ve çember üzerindeki bir noktayla daire çizer
- **Taşı** -> Seçili nesneleri iki nokta arasındaki kadar taşır
- **Sil** -> Seçili nesneleri siler (Del); geri alınabilir

## Seçim (bağlamsal sekme)

Bir komut nesne isterken (taşımak, silmek, kopyalamak için) sekme satırının sonunda belirir ve soru cevaplanınca kaybolur; öne gelmez. Her düğme komut satırına bir seçim satırı yazar; bulunanlar seçilmekte olan nesnelere eklenir.

### Seçim Kipi

- **Pencere** -> Tamamen içinde kalan nesneleri seçer; iki köşe tıklanır
- **Kesen** -> Kutuya değen her şeyi seçer; iki köşe tıklanır
- **Çokgen** -> Çokgenin tamamen içindekileri seçer; köşeler tıklanır, Enter bitirir
- **Çokgen Kesen** -> Çokgenin değdiği her şeyi seçer; köşeler tıklanır, Enter bitirir
- **Çit** -> Çizilen hattın kestiği her şeyi seçer; noktalar tıklanır, Enter bitirir
- **Daire** -> Dairenin tamamen içindekileri seçer; önce merkez, sonra çevre tıklanır
- **Dışında** -> Kutuya hiç değmeyenleri seçer; iki köşe tıklanır
- **İçeren** -> Noktayı içeren en küçük alanı seçer; alanın içine tıklanır
- **Geçen** -> Noktadan geçen çizgileri seçer; noktaya tıklanır

### Küme

- **Tümü** -> Görünür bütün nesneleri seçer
- **Önceki** -> Bundan önceki seçimi geri getirir
- **Son** -> En son çizilen nesneyi seçer
- **Temizle** -> Seçimi boşaltır
- **Tersine Çevir** -> Seçili olanları çıkarır, seçili olmayanları seçer

### Süzgeç

- **Tür** -> Bu sekmenin yazdığı her seçim satırını seçilen türdeki nesnelerle sınırlar; değerler şunlardır: Her tür, Çokluçizgi, Daire, Yay, Nokta, Elips, Yaylıçizgi, Spline, Tarama, Blokreferansı, Ölçü, Lider

### Bitir

- **Seçimi Ver** -> Seçilenleri soran komuta verir; Enter ya da sağ tıkla aynıdır

## Nokta Girişi (bağlamsal sekme)

Bir komut nokta isterken (çizginin, dairenin, taşımanın taban noktası gibi) sekme satırının sonunda belirir ve soru bitince kaybolur; öne gelmez.

### Yakalama

- **Uç nokta** -> Bir halkanın köşesine ya da yayın ucuna oturur — parsel köşesi, bina köşesi; anahtardır, işaretliyken açıktır
- **Orta nokta** -> Bir kenarın (ya da yayın) tam ortasına oturur; anahtardır, işaretliyken açıktır
- **Merkez** -> Bir eğrinin çizildiği merkeze oturur: dairenin ve yayın merkezi; anahtardır, işaretliyken açıktır
- **Ağırlık merkezi** -> Kapalı bir halkanın alan ağırlık merkezine — parselin ortasına — oturur; anahtardır, işaretliyken açıktır
- **Kesişim** -> İki kenarın gerçekten kesiştiği yere oturur; anahtardır, işaretliyken açıktır
- **Dik ayak** -> Önceki noktadan bir kenara indirilen dikin ayağına oturur; anahtardır, işaretliyken açıktır
- **En yakın** -> Kenarın imlece en yakın noktasına — çizginin herhangi bir noktasına — oturur; anahtardır, işaretliyken açıktır
- **Düğüm** -> Ölçülmüş tek noktaya (nirengi, poligon noktası, röper) oturur; anahtardır, işaretliyken açıktır
- **Izgara** -> En yakın ızgara kesişimine oturur; anahtardır, işaretliyken açıktır
- **Kutupsal** -> Önceki noktadan çıkan en yakın kutupsal ışına oturur; anahtardır, işaretliyken açıktır
- **Uzantı** -> Bir kenarın kendi ucundan öteye uzanan doğrusuna oturur; anahtardır, işaretliyken açıktır
- **Paralel** -> Önceki noktadan çıkan, bir kenara paralel ışına oturur; anahtardır, işaretliyken açıktır
- **Uzatılmış kesişim** -> İki kenarın doğrularının kesişeceği yere oturur; ikisi de oraya kadar uzanmasa bile; anahtardır, işaretliyken açıktır
- **Kılavuz** -> Cetvelden çektiğiniz kılavuza, iki kılavuz kesişiyorsa kesişimine oturur; anahtardır, işaretliyken açıktır
- **Ekleme noktası** -> Bir nesnenin yerleştirildiği noktaya — blok referansının ekleme noktası gibi — oturur; anahtardır, işaretliyken açıktır
- **Çeyrek nokta** -> Daire, yay ve elipsin eksenleri kestiği dört çeyrek noktasına oturur; anahtardır, işaretliyken açıktır
- **Teğet nokta** -> Son noktadan bir eğriye çizilen teğetin eğriye değdiği noktaya oturur; anahtardır, işaretliyken açıktır
- **İzleme** -> İşaretlediğiniz bir noktadan geçen yatay ve düşey ize oturur; iki işaretin izleri kesişir; anahtardır, işaretliyken açıktır

### Hesap

- **Son Nokta** -> Bir önceki noktayı yazar: `son()`
- **Numaralı Nokta** -> Çizimdeki numaralı ölçü noktasının koordinatını yazar: `n(nokta_no)`
- **Orta Nokta** -> A ile B'nin tam ortasındaki noktayı verir: `orta(A,B)`
- **Göreli** -> P noktasından ölçülen göreli noktayı verir: `ile(P,@dx,dy)`
- **Dik Ayak** -> AB doğrultusunda A'dan ayak metre ilerleyip oradan boy metre dik gider (sağ pozitif): `dik(A,B,ayak,boy)`
- **Semt ve Kenar** -> S istasyonundan verilen açı semtinde kenar metre ötedeki noktayı verir: `semt(S,açı,kenar)`
- **Kesişim** -> İki doğrultunun, iki uzaklığın ya da iki doğrunun kesişimini verir: `kes(A,açı1,B,açı2)` · `kes(A,r1,B,r2,sol|sağ)` · `kes(A,B,C,D)`
- **Ara Nokta** -> AB doğrusu üzerinde oranla ya da metreyle nokta verir: `ara(A,B,oran)` · `ara(A,B,mesafe m)`
- **Uzantı** -> AB doğrultusunda B'den mesafe metre ötedeki noktayı verir: `uzanti(A,B,mesafe)`
- **X ve Y** -> P noktasının sağa değerini, Q noktasının yukarı değerini birleştirir: `xy(P,Q)`
- **Boyunca** -> Kimliği verilen çizgi, yay ya da daire boyunca ilk noktasından mesafe metre, oradan sapma metre yana (sağ pozitif) gider: `boyunca(nesne(kimlik),mesafe,sapma)`

### Katman

- **Katmanı nesneden al** -> Bir nesneye tıklarsınız: bu komutun çizdikleri onun katmanına gider, etkin katman değişmez

### Satır

- **Gönder** -> Kurulan satırı soran komuta verir (Enter); açık parantezler kendiliğinden kapanır
- **Vazgeç** -> Kurulan satırı siler, soru sürer (Esc)

## Sağ köşe

Sekme satırının en sağında, hızlı erişim satırından ayrı durur.

### Sekme satırının sağ ucu

- **Şeridi daralt** -> Şeridi sekme satırına indirir (bir sekmeye çift tıklamak da aynısını yapar); daraltılmışken adı Şeridi aç olur ve şeridi geri açar
- **Komut ara…** -> Komut listesi sayfasını açar: bütün komutlar adları ve kısaltmalarıyla aranır (Ctrl+K)
- **Kullanıcı baş harfleri** -> Yuvarlak bir rozette kullanıcının baş harflerini gösterir (davranışı kodda belirsiz: baş harfler hiç atanmıyor, varsayılan PC kalır; tıklanınca bir şey yapmaz)
