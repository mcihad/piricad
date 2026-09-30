# Yazdırma ve PDF

Bir paftayı kâğıda ya da PDF'e çıkarmak isteyen herkes için; bu sayfayı
bitirdiğinizde yazdırma alanını ekranda seçmeyi, ölçeği ve merkezi vermeyi,
profilleri ayarlamayı ve şifreli PDF almayı bileceksiniz.

## İki adım: önce alanı seçin, sonra yazdırın

Hızlı erişim satırındaki **Yazdır** simgesi (yazıcı; **Ctrl+P**, Kaydet'in sağında) ve
şeritteki **Çıktı ▸ Yazdır ▸ Yazdır** tek başına yazdırmaz; önce **nereyi**
yazdıracağınızı sorar. Yazıcının sağındaki ok çizimin çıktı yerleşimlerini listeler.

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

Pencerenin büyük bölümünü **kâğıdın kendisi** kaplar: aynı semboloji, aynı çizgi
kalınlıkları, profilin çözünürlüğünde. Ekranda gördüğünüz çıkan kâğıttır. Kesik çizgi
profilin **kenar payını**, mavi **+** işareti kâğıdın **merkezini** gösterir; ikisi de
yalnız ekrandadır, kâğıda basılmaz. Kâğıdın altındaki satır ölçeği, kâğıdı ve kâğıdın
zeminde kapladığı yeri tek cümlede söyler: `1:1316 ölçekte A4 dikey kâğıt, zeminde
250.0 × 364.5 m`.

Sağdaki sütun seçimleri yapılış sırasıyla gruplar:

- **Kâğıt** — **Profil** listesi kâğıdı seçer: hangi boy, hangi yön, hangi
  çözünürlük, hangi kenar payı. Altında seçilen profilin ne dediği yazılıdır.
  Kâğıdın bütün ölçüleri **yalnız profilde** durur; **Profilleri düzenle…**
  `Seçenekler ▸ Plot ve Çıktı`'yı açar.
- **Ölçek ve konum**
  - **Ölçek 1 :** — **Çerçevenin kendi ölçeğiyle gelir**, yuvarlanmadan: kâğıda
    giren alan, çerçevede gördüğünüz alanın tam olarak kendisidir. Ölçek bir pafta
    ölçeği değilse altında **1:2000 ölçeğine yuvarla** gibi bir düğme çıkar; basınca
    ölçek bir üstteki pafta ölçeğine çıkar (1, 2, 2,5 ya da 5 çarpı 10'un bir kuvveti:
    1/200, 1/500, 1/1000, 1/2500…). Bunu siz istersiniz, pencere kendi başına yapmaz,
    çünkü ölçeği büyütmek kâğıda çerçevenin dışını da sokar. İstediğiniz değeri elle
    de yazabilirsiniz: 1/1000 bir karardır, çerçevenin rastgele düştüğü yer değil.
  - **Merkez (Y, X)** — kâğıdın ortalandığı nokta, `Y,X` metre olarak. Yanındaki nişan
    düğmesi önizlemeyi kapatıp çerçeveyi yeniden açar, yani "yeniden nişan alayım"
    demektir.
  - **Zeminde** — kâğıdın zeminde kapladığı alan, metre olarak.

  Altındaki satır alanı hangisinin belirlediğini söyler: **Çerçevenin tuttuğu alan
  basılır.** ya da **Alanı merkez ve ölçek belirliyor.** İkinci durumda
  **Çerçeveye dön** düğmesi yazdığınız ölçeği ve merkezi bırakır; kâğıda yeniden
  çerçevenin alanı gider.
- **Hedef** — **PDF** ya da **Yazıcı**. PDF'te **Dosya**, yazıcıda **Yazıcı** satırı
  görünür.
- **Belge bilgileri** ve **Koruma** — yalnız PDF'te görünür ve kapalı gelir, çünkü çoğu
  çıktı onlara ihtiyaç duymaz. Başlığın yanındaki not, grup kapalıyken içinde ne
  olduğunu söyler (`boş`, başlığın kendisi, `şifresiz`, `şifreli`); sağdaki ok grubu
  açar ve kapatır.

Altta, pencerenin bütün genişliğinde **Komut** şeridi durur: pencerenin göndereceği
satır. Pencerede ne görüyorsanız komut satırına yazılacak olan odur; kopyalayıp bir
betiğe koyabilirsiniz ([`YAZDIR`](../komutlar/print.md)). **Yazdır** düğmesi bu satırı
gönderir; PDF'e bir dosya adı verilene kadar satır boştur ve düğme kapalı durur.

Bu satır da **alanın hangi biçimde gittiğini** söyler. Ölçeğe ve merkeze
dokunmadıysanız satır çerçevenin iki köşesini taşır — `pencere=… pencere=…` — yani
kâğıda giden alan çerçevenin alanıdır. Ölçek ya da merkez yazdığınız (veya yuvarlama
düğmesine bastığınız) anda satır `merkez=… olcek=…` olur: artık alanı o ikisi
belirler, çerçeve geride kalır. Soldaki kâğıt her iki durumda da gerçekten gidecek
olanı gösterir.

## PDF

**Dosya** alanına yolu yazın ya da yanındaki klasör düğmesiyle (**Gözat…**) seçin.
**Belge bilgileri** grubundaki **Başlık** ve **Yazar** PDF'in kendi alanlarına yazılır.

**Koruma** grubu iki şifre ve üç izinden oluşur:

| Alan | Ne yapar |
|---|---|
| **Açma şifresi** | PDF'i açmak için istenir. Boşsa dosya şifresizdir |
| **Sahip şifresi** | İzinleri değiştirmek için istenir; boşsa açma şifresiyle aynıdır |
| **Yazdırılabilir / Kopyalanabilir / Değiştirilebilir** | Sahip şifresini bilmeyen bir okuyucunun yapabilecekleri |

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
varsayılandır ve hızlı erişimdeki Yazdır onu kullanır. Ekleme, silme ve varsayılan
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
| **Boşluk** | Odaktaki grup okunda grubu açar ya da kapatır |
| **F4** / **Alt+↓** | Merkez alanında çerçeveyi yeniden açar |

## İlgili

- [`YAZDIR`](../komutlar/print.md) — komutun bütün parametreleri
- [`YAZDIRMAPROFİLİ`](../komutlar/print_profile.md) — profiller
- [Dışa Aktar penceresi](disa-aktarma.md) — çizimi başka bir biçime yazma
- [Arayüz](arayuz.md) — şerit, hızlı erişim ve durum çubuğu
