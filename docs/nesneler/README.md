# Nesne türleri

Çizimdeki her nesnenin bir **türü** vardır: çoklu çizgi, daire, yay, nokta, elips,
yaylı çoklu çizgi, spline, tarama, blok referansı, ölçü, lider.
Tür, nesnenin iki şeyini birden söyler: dosyaya hangi **sayıların** yazıldığını ve
o sayılardan ekranda hangi **biçimin** çizildiğini. Bir daire dosyada iki nokta
olarak durur — merkezi ve yarıçapı — ama ekranda 128 kenarlı, pürüzsüz bir çember
olarak çizilir; alanı π·r² olarak hesaplanır, çizilen çokgenin alanı olarak değil.

Bu sayfa türlerin ortak kurallarını anlatır. Türlerin tablosu
[referans](referans.md) sayfasında çekirdeğin kaydından üretilir; her türün kendi
sayfası vardır ve aynı iskeletle yazılır: **Nedir · Nasıl saklanır · Nasıl çizilir ·
Yakalama noktaları · Ölçüler · Dosya ve dış biçimler · Komutlar · Sınırlar.**

## Sayılar ve biçim ayrıdır

Her tür halkalar hâlinde **tepe noktaları** saklar ([koordinatlar](../veri/koordinat-sistemleri.md)
milimetre cinsinden tam sayıdır). Bir türün halkaları bazen çizilen biçimin ta
kendisidir (çoklu çizgi), bazen yalnız **tanımıdır** (dairenin merkezi ve yarıçapı,
elipsin merkezi ve iki eksen ucu). Çizim, yakalama ve seçim her zaman **biçime**
bakar, tanım noktalarına değil: bir elipsin kenarına yakalanırsınız, merkezden eksen
ucuna giden görünmez çizgiye değil.

Halkaların söyleyemediğini tür **yük** olarak saklar: sabit genişlikli tam sayı
alanlardan oluşan bir bayt dizisi, yalnız o türün kendisinin okuduğu. İlk beş tür yük
taşımaz; kısmi elips (başlangıç ve bitiş açısı), yaylı çoklu çizgi (yayların merkezi
ve yarıçapı), spline (derece, düğümler, ağırlıklar), tarama (desen, açı, ölçek, çizgi
aileleri), blok referansı (yerleştirme dönüşümü), ölçü (tür, ölçülen değer, stil
ölçüleri) ve lider (ok) taşır. Her yük bir düzen sürümüyle başlar; daha yeni bir
düzeni bu sürüm tanımazsa nesne tanınmayan tür gibi korunur.

## Belirlenimcilik

Bir türün çizilen biçimi ve ölçüleri her bilgisayarda **bit bit aynı** hesaplanır:
Linux, Windows ve macOS aynı daireyi aynı 128 köşeyle çizer, aynı yayın uzunluğunu
aynı milimetreye yuvarlar. Bunun için trigonometri kütüphaneden değil, yalnız toplama,
çıkarma, çarpma, bölme ve karekökle çalışan kendi rutinlerinden gelir. Bir kadastro
belgesinin alanı hangi makinede hesaplandığına bağlı olamaz.

## Tanınmayan tür

Daha yeni bir KentOSCad'in ya da bir eklentinin yazdığı bir türü bu sürüm tanımıyorsa
nesne **kaybolmaz**: halkaları ve yükü bayt bayt korunur, halkaları ekranda çizilir,
kapsamda ve dizinde yer alır. Yalnız düzenlenemez — taşımaya, kırpmaya ya da yükünü
değiştirmeye kalkışan komut şunu söyler:

```text
Bu yapının tanımadığı türdeki nesne düzenlenemez; olduğu gibi korunur.
```

Dosya yeniden kaydedilince nesne geldiği baytlarla gider. Öznitelik paneli türü
numarasıyla gösterir.

## Yabancı veri

Bir DXF'ten gelen nesne, başka bir programın ona bağladığı **ek veriyi** (XDATA)
taşıyabilir. KentOSCad bunu okuyamaz ama kaybetmez: baytlar nesnenin yanında
etiketiyle saklanır, dosyaya yazılır, dışa aktarımda geri verilir. Öznitelik paneli
yalnız sayısını gösterir (`ek_veri: 2 kayıt`); hiçbir komut içeriğini değiştirmez.

## Bloklar

Bir kez çizilip çok kez yerleştirilen sembol — rögar kapağı, kuzey oku, antet — bir
**blok tanımıdır**. Tanımın nesneleri çizimde durur ama kendi başlarına çizilmez,
seçilmez, düzenlenmez; onları yerleştiren [**blok referansı**](blokreferansi.md)
çizer. Blok tanımları dosyaya adıyla, açıklamasıyla, taban noktasıyla ve üyeleriyle
yazılır. [`BLOK`](../komutlar/block.md) seçilen nesnelerden tanım yapar,
[`BLOKEKLE`](../komutlar/insert.md) yerleştirir; bir DXF'in blokları yapısıyla
gelir ve gider.
