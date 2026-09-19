# Yazdırma ve PDF

Bir paftayı kâğıda ya da PDF'e çıkarmak isteyen herkes için; bu sayfayı
bitirdiğinizde yazdırma alanını ekranda seçmeyi, ölçeği ve merkezi vermeyi,
profilleri ayarlamayı ve şifreli PDF almayı bileceksiniz.

## İki adım: önce alanı seçin, sonra yazdırın

Araç çubuğundaki **Yazdır** simgesi (Ctrl+P, Kaydet'in sağında) tek başına
yazdırmaz; önce **nereyi** yazdıracağınızı sorar.

**Birinci basış** tuvalin ortasında bir **yazdırma çerçevesi** açar. Çerçeve
varsayılan profilin kâğıdı kadar bir dikdörtgendir, köşelerinde **L** işaretleri
vardır ve dışındaki her yer grileşir: içinde kalan, kâğıda gidecek olandır.

Çerçeve açıkken harita her zamanki gibi davranır:

| Hareket | Ne olur |
|---|---|
| Sol tuşla sürüklemek | Harita çerçevenin altında kayar |
| Tekerlek | Yaklaşır, uzaklaşır — çerçeveye daha az ya da daha çok yer girer |
| **Yazdır** (ya da **Enter**) | O görüntüyü yakalar, önizleme penceresini açar |
| **Esc** ya da **sağ tık** | Çerçeveyi kapatır, hiçbir şey yazdırmaz |

Çerçeve **ekranda hep aynı boydadır**; çünkü çerçeve kâğıttır, çizimdeki bir
dikdörtgen değil. Yaklaştıkça kâğıda daha az yer girer — yani ölçek büyür.

Ortasındaki **+** işareti kâğıdın **merkezini** gösterir; yanındaki iki sayı o
noktanın `Sağa (Y)` ve `Yukarı (X)` değerleridir. Merkezi tam bir noktaya oturtmak
istiyorsanız bu sayıları önizleme penceresinde elle de yazabilirsiniz.

**İkinci basış** önizlemeyi açar.

## Önizleme penceresi

Solda **kâğıdın kendisi** durur: aynı semboloji, aynı çizgi kalınlıkları, profilin
çözünürlüğünde. Ekranda gördüğünüz çıkan kâğıttır. Altında kâğıdın ölçüleri ve o
kâğıdın zeminde kapladığı yer yazılıdır.

Sağda dört şey vardır:

- **Profil** — hangi kâğıt, hangi yön, hangi çözünürlük, hangi kenar boşluğu.
  Kâğıdın bütün ölçüleri **yalnız burada** durur; altındaki **Profilleri yönet…**
  bağlantısı `Seçenekler ▸ Plot ve Çıktı`'yı açar.
- **Ölçek** — `1 : N`. **Çerçevenin kendi ölçeğiyle gelir**, yuvarlanmadan: kâğıda
  giren alan, çerçevede gördüğünüz alanın tam olarak kendisidir. Yanındaki
  **Yuvarla** düğmesi ölçeği bir pafta ölçeğine çıkarır (1/184 → 1/200) — bunu
  siz istersiniz, pencere kendi başına yapmaz, çünkü ölçeği büyütmek kâğıda
  çerçevenin dışını da sokar. Düğme, ölçek zaten yuvarlaksa görünmez. İstediğiniz
  değeri elle de yazabilirsiniz: 1/1000 bir karardır, çerçevenin rastgele düştüğü
  yer değil.
- **Merkez** — kâğıdın ortalandığı nokta, `Y,X` metre olarak. Yanındaki nişan
  düğmesi önizlemeyi kapatıp çerçeveyi yeniden açar, yani "yeniden nişan alayım"
  demektir.
- **Çıktı** — **PDF** ya da **Yazıcı**.

Altta pencerenin göndereceği **komut satırı** yazılıdır. Kartta ne görüyorsanız
komut satırına yazılacak olan odur; kopyalayıp bir betiğe koyabilirsiniz
([`YAZDIR`](../komutlar/print.md)).

Bu satır aynı zamanda **alanın hangi biçimde gittiğini** söyler. Ölçeğe ve merkeze
dokunmadıysanız satır çerçevenin iki köşesini taşır — `pencere=… pencere=…` — yani
kâğıda giden alan çerçevenin alanıdır. Ölçek ya da merkez yazdığınız (veya
**Yuvarla**'ya bastığınız) anda satır `merkez=… olcek=…` olur: artık alanı o ikisi
belirler, çerçeve geride kalır. Sol taraftaki kâğıt her iki durumda da gerçekten
gidecek olanı gösterir.

## PDF

**Dosya** alanına yolu yazın ya da **Gözat…** ile seçin. **Başlık** ve **Yazar**
PDF'in kendi alanlarına yazılır.

**Şifreleme** iki şifre ve üç izinden oluşur:

| Alan | Ne yapar |
|---|---|
| **Şifre** | PDF'i açmak için istenir. Boşsa dosya şifresizdir |
| **Sahip şifresi** | İzinleri değiştirmek için istenir; boşsa açma şifresiyle aynıdır |
| **Yazdır / Kopyala / Değiştir** | Sahip şifresini bilmeyen bir okuyucunun yapabilecekleri |

Şifreleme **AES-256** ile yapılır. İzinler yalnız bir şifre verildiğinde anlam
taşır, bu yüzden şifre kutuları boşken kapalı durur.

Şifreler **komut günlüğüne yazılmaz.** Yazdırma komutu günlüğe hiç girmez: günlük
çökme kurtarmada yeniden oynatılır ve yeniden oynatılan bir yazdırma kimsenin
istemediği bir anda bir PDF'i yeniden yazardı. Şifreyi komut satırına yazarsanız o
satır oturumun komut geçmişinde kalır; temiz yol önizleme penceresidir.

> Şifreleme `qpdf` ile yapılır. `KENTOS_WITH_QPDF` kapalı derlenmiş bir yapıda şifre
> kutuları "bu yapıda yok" der ve PDF şifresiz yazılır.

## Yazıcı

**Yazıcı** seçildiğinde listede sistemin tanıdığı yazıcılar görünür; **(sistem
varsayılanı)** makinenin kendi varsayılanına gönderir. Kâğıt boyu, yön ve
çözünürlük profilden gider — yazıcının kendi penceresi açılmaz, çünkü kâğıdı
profil söyler.

## Profiller

Bir **profil** adlandırılmış bir kâğıttır: kâğıt boyu, yön, çözünürlük, kenar
boşluğu. `Seçenekler ▸ Plot ve Çıktı` sayfasının başındaki tabloda durur; `●` olan
varsayılandır ve araç çubuğundaki Yazdır onu kullanır. Ekleme, silme ve varsayılan
yapma [`YAZDIRMAPROFİLİ`](../komutlar/print_profile.md) sayfasında anlatılır.

Yazdır simgesinin yanındaki küçük ok profilleri listeler: birine basmak **o
profille** çerçeve açar. A3 yatay bir profil seçtiyseniz çerçeve de yatay açılır.

Profiller kullanıcı profilinizde bir dosyada tutulur; çizimle birlikte gitmez.
Dosyanın yolu ayarlar sayfasının altında yazılıdır.

## Klavye

| Tuş | Ne yapar |
|---|---|
| **Ctrl+P** | Yazdırma çerçevesini açar; ikinci kez önizlemeye geçer |
| **Enter** | Çerçeve açıkken görüntüyü yakalar |
| **Esc** | Çerçeveyi kapatır; önizleme penceresini kapatır |
| **Tab** | Önizlemede alanlar arasında gezer |
| **F4** / **Alt+↓** | Merkez alanında çerçeveyi yeniden açar |

## İlgili

- [`YAZDIR`](../komutlar/print.md) — komutun bütün parametreleri
- [`YAZDIRMAPROFİLİ`](../komutlar/print_profile.md) — profiller
- [Dışa Aktar penceresi](disa-aktarma.md) — çizimi başka bir biçime yazma
- [Arayüz](arayuz.md) — araç çubuğu ve durum çubuğu
