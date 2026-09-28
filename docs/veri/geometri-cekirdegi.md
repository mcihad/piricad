# Geometri çekirdeği: OpenCASCADE

Bir parselin köşesi yuvarlatıldığında, bir yolun kenarı kavis yaptığında ya da bir
tampon bölge çizildiğinde çizimde **yay** vardır. Bu sayfa, KentOS CAD'in bu tür
geometriyi hangi çekirdekle hesapladığını, sonuçların neden her bilgisayarda aynı
milimetreyi verdiğini ve bunun hangi işlemlere hangi aşamada geldiğini anlatır.

## OpenCASCADE nedir, neden kullanılır

KentOS CAD'in geometri çekirdeği **OpenCASCADE Technology**'dir (OCCT). OCCT, CAD
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

Çekirdek bu sürümde programa bağlı ve sınanmış durumda. Kullanıcıya görünen işlemler
aşama aşama ona geçecek:

| Aşama | İşlem | Ne değişecek |
|---|---|---|
| O-2 | [`YUVARLA`](../komutlar/fillet.md), [`PAH`](../komutlar/chamfer.md) | Kapalı bir alanın köşesi de kısa kenarlarla değil **gerçek yayla** yuvarlanacak; yuvarlanmış bir parselin öteki köşeleri de yuvarlanabilecek; elips ve spline içeren eğri çiftleri de yuvarlanabilecek |
| O-3 | [`İFRAZ`](../komutlar/split_parcel.md), [`ALANİFRAZ`](../komutlar/split_area.md), [`TEVHİT`](../komutlar/merge.md), `BİRLEŞTİR`, `TAMPON` | Yaylı kenarlı parseller bölünürken ve birleşirken yaylar yay olarak kalacak. Düz kenarlı yüz binlerce parselde hızlı yol olarak Clipper2 kalacak |
| O-4 | [`OFSET`](../komutlar/offset.md) | Yaylı bir yolun ofseti de yaylı olacak; dış köşeler gerçek yayla dönecek |
| O-5 | `BUDA`, `UZAT`, `BÖL`, `KIR` | Elips ve spline kesişimleri çekirdekten gelecek |

Bir aşama gelene dek o işlem bugünkü yoluyla çalışmaya devam eder. Yaylı kenarlı bir
parsel bugün İFRAZ'a yayı çizildiği hâliyle, yani kısa kenarlarla girer.

## Kurulum

Çekirdek programın bir parçasıdır ve kaynaktan derlerken **zorunludur**. Nasıl
kurulacağı [Kurulum](../baslangic/kurulum.md) sayfasındadır. Eksikse derleme hangi
paketin eksik olduğunu söyleyerek durur.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bu yapıda geometri çekirdeği (OpenCASCADE) yok; KENTOS_WITH_OCCT=ON ile derleyin.` | Program çekirdeksiz derlenmiş (`-DKENTOS_WITH_OCCT=OFF`) | OCCT'yi kurup programı yeniden derleyin |
| `Geometri çekirdeği bu alan işlemini tamamlayamadı; sınırlardan biri kendini kesiyor ya da açık olabilir.` | Verilen sınırlardan biri geçerli bir yüz değil | Sınırı [`TOPOLOJİ`](../komutlar/topology.md) ile denetleyin |
| `Elips ve spline kenarları geometri çekirdeğine bu aşamada verilmiyor; yalnız doğru ve yay kenarları.` | İşlem bir elips ya da spline kenarı içeriyor | O-5 aşamasına kadar bu kenarları çizgiye çevirerek (`PATLAT`) işleyin |
