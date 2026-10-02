# Geometri çekirdeği: OpenCASCADE

Bir parselin köşesi yuvarlatıldığında, bir yolun kenarı kavis yaptığında ya da bir
tampon bölge çizildiğinde çizimde **yay** vardır. Bu sayfa, PiriCAD'in bu tür
geometriyi hangi çekirdekle hesapladığını, sonuçların neden her bilgisayarda aynı
milimetreyi verdiğini ve bunun hangi işlemlere hangi aşamada geldiğini anlatır.

## OpenCASCADE nedir, neden kullanılır

PiriCAD'in geometri çekirdeği **OpenCASCADE Technology**'dir (OCCT). OCCT, CAD
yazılımlarında kullanılan olgun ve açık kaynaklı bir geometri çekirdeğidir.
Doğrularla olduğu kadar **yaylarla, elipslerle ve spline'larla** da tam hesap
yapar.

Önceki boolean aracı (Clipper2) yalnız çokgen tanıyordu. Bir yay ona ancak kısa
doğrulardan (kirişlerden) oluşan bir çizgi olarak verilebiliyordu ve sonuç da öyle
dönüyordu. OCCT yayı çemberiyle birlikte alır ve sonucu yine **yay** olarak, aynı
merkez ve aynı yarıçapla verir. Köşesi 2 m yarıçapla yuvarlatılmış bir parsel
ikiye bölündüğünde, yuvarlak köşe hangi parçada kalıyorsa o parçada yine 2 m
yarıçaplı bir yaydır; alanı da yayın kendisinden hesaplanır.

**Bu program OpenCASCADE Technology yazılımının sağladığı olanakları kullanır ve
onlar üzerine kuruludur.** OCCT, GNU LGPL 2.1 ve Open CASCADE istisnasıyla
dağıtılır. Ayrıntılar ve lisans bulgusu kaynak ağacındaki `NOTICE` dosyasındadır.

## Sonuçlar neden her bilgisayarda aynı

Çizim her koordinatı **tam sayı milimetre** olarak saklar (bkz. [Sayısal doğruluk ve
toleranslar](hassasiyet.md)). Çekirdek kendi içinde ondalıklı sayılarla çalışır.
Bir işlemin sonucu çizime dönerken:

- her nokta **bir kez**, en yakın milimetreye yuvarlanır;
- bir yayın yönü ve açısı çekirdeğin iç değerlerinden okunmaz, yuvarlanmış uçlarından
  ve merkezinden yeniden hesaplanır;
- kapalı bir sınır her zaman saat yönünün tersine, bir delik saat yönünde döner ve
  her biri en alttaki, sonra en soldaki köşesinden başlar; parçalar da bu köşeye
  göre sıralanır.

Böylece aynı işlem aynı girdiden **Linux'ta, Windows'ta ve macOS'ta aynı
milimetreleri** verir. Kadastro ve imar çıktısı resmî belge olduğu için bu bir
tercih değil, zorunluluktur. Her sürümün testleri bunu üç işletim sisteminde
ayrı ayrı denetler.

## Hangi işlem ne zaman çekirdeğe geçiyor

Çekirdek programa bağlı ve sınanmış durumda. Kullanıcıya görünen işlemler aşama aşama
ona geçiyor:

| Aşama | İşlem | Durum |
|---|---|---|
| O-2 | [`YUVARLA`](../komutlar/fillet.md), [`PAH`](../komutlar/chamfer.md) | **Bu sürümde.** Açık çizginin de alanın da köşesi **gerçek yayla**, aynı nesnenin yay kenarı olarak yuvarlanır; yuvarlanmış bir parselin öteki köşeleri de yuvarlanır, bir kenarı yay olan köşede yeni yay iki kenara da teğettir |
| O-3 | [`İFRAZ`](../komutlar/split_parcel.md), [`ALANİFRAZ`](../komutlar/split_area.md), [`TEVHİT`](../komutlar/merge.md) | **Bu sürümde:** yaylı kenarlı bir parsel bölünürken ve birleşirken yay, merkezi ve yarıçapıyla yay olarak kalır. Düz kenarlı parseller eskisi gibi hızlı yoldan (Clipper2) işlenir |
| O-3 | [`BİRLEŞTİR`](../komutlar/combine.md) | **Bu sürümde:** yaylı kenarlı alanların birleşimi yayı aynı merkez ve yarıçapla korur; yaylı çizgiler uç uca eklenirken yay yay kalır |
| O-3 | [`BİRLEŞİM`](../komutlar/area_union.md), [`KESİŞİM`](../komutlar/area_intersection.md), [`FARK`](../komutlar/area_difference.md), [`SİMETRİKFARK`](../komutlar/area_symdifference.md) | **Bu sürümde:** Değiştir, Alan ve Eğri sekmelerinin **Alan İşlemleri** grubunda. Düz/yaylı kapalı alan ve daire çekirdekten; simetrik farkın ara sonuçları yuvarlanmadan birleştirilir. Düz delikler korunur; eğrili ve delikli sonuç mevcut modele kaydedilemediği için işlem bütünüyle reddedilir |
| O-3 | [`TAMPON`](../komutlar/tampon.md) | **Bu sürümde:** yuvarlak köşe ve uçlu tampon gerçek yaylıdır — noktanın çevresi daire alan, bandın uçları yarım daire; çizginin her parçasının bandı alınıp birleştirilir. 256 köşeden uzun çizgi, köşeli/pahlı köşe, düz/kare uç ve elips/spline kaynak Clipper2 ile |
| O-4 | [`OFSET`](../komutlar/offset.md) | **Bu sürümde:** yaylı çoklu çizginin ve köşesi yuvarlanmış parselin paraleli yaylı çoklu çizgidir — her yay aynı merkezli, yarıçapı mesafe kadar değişmiş; `kose=YUVARLAK` dış köşeyi gerçek yayla, `kose=PAH` yaylı çizgide köşeyi düz kirişle döner. Düz kenarlı şeklin keskin ve pahlı köşeli paraleli eskisi gibi Clipper2 ile; delikli alanın yuvarlak köşeleri kısa kenarlarla (komut söyler) |
| O-5 | `BUDA`, `UZAT`, `BÖL`, `KIR`, `YUVARLA` | **Bu sürümde:** elips ve spline kesişimleri OCCT'nin 2D eğri çözücüsünden; kısmi/ters elips, rasyonel spline, teğet ve ortak parça ayrı sonuçlanır. İki nesne arasındaki [`YUVARLA`](../komutlar/fillet.md) elips ya da spline içeren köşede teğet yayı OCCT'nin 2B yuvarlamasından (`ChFi2d`) alır: elips elips, spline spline kalıp teğet noktasında kesilir, karşıdaki çizgi gerekirse uzar; yaylı köşe yine tek bir gerçek yaydır. Elips ve spline arasında [`PAH`](../komutlar/chamfer.md) kırılmaz |

Bir aşama gelene dek o işlem bugünkü yoluyla çalışmaya devam eder.

## Yaylı kenarlı alan

Köşesi yuvarlanmış bir parsel, [`KENARTÜRÜ`](../komutlar/edge_kind.md) ile kenarı yaya
çevrilmiş bir alan ya da DXF'ten "bulge" ile gelen bir çoklu çizgi **yaylı kenarlı bir
alandır**. Program onu her yerde alan olarak görür: şeritte **Alan** sekmesini açar,
[işlem araçları](../islem/README.md) onu alan olarak işler, alanı yayın kendisinden
hesaplanır. Yay kenarına [Uzunluk Yaz](../komutlar/uzunluk_yaz.md) **yayın boyunu**, yayın
ortasına yazar; kenarı sonradan yaya çevrilen bir kenarın bağlı yazısı da yayın boyunu
söyler. [Alanı Düzenle](../komutlar/alan_duzenle.md) yaylı kenarlı alanı bu sürümde atlar
ve bunu söyler.

## Kurulum

Çekirdek programın bir parçasıdır ve kaynaktan derlerken **zorunludur**. Nasıl
kurulacağı [Kurulum](../baslangic/kurulum.md) sayfasındadır. Eksikse derleme hangi
paketin eksik olduğunu söyleyerek durur.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bu yapıda geometri çekirdeği (OpenCASCADE) yok; PIRICAD_WITH_OCCT=ON ile derleyin.` | Program çekirdeksiz derlenmiş (`-DPIRICAD_WITH_OCCT=OFF`) | OCCT'yi kurup programı yeniden derleyin |
| `OpenCASCADE alan işlemini tamamlayamadı; kaynaklar değiştirilmedi.` | Çekirdek boolean işlemini tamamlayamadı | Sınırı [`TOPOLOJİ`](../komutlar/topology.md) ile denetleyin |
| `Elips ve spline kenarları geometri çekirdeğine bu aşamada verilmiyor; yalnız doğru ve yay kenarları.` | İşlem bir elips ya da spline kenarı içeriyor | O-5 aşamasına kadar bu kenarları çizgiye çevirerek (`PATLAT`) işleyin |
| `Sonuç eğrili bir sınır ve delik içeriyor; bu alan biçimi henüz kaydedilemiyor. Kaynaklar değiştirilmedi.` | Sınır veya deliği eğrili bir sonuç mevcut belge modeline sığmıyor | Boşluğu çevreleyen parçaları ayrı ayrı işleyin; sonuç sessizce düzleştirilmez |
