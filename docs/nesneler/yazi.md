# Yazı

## Nedir

Paftadaki ada ve parsel numarası, plan notu, lejant açıklaması: kendi konumu, boyu ve
dönüklüğü olan, çizilmiş bir nesne. Tek satırlı da olabilir, satırları kırılan bir not
da. DXF `TEXT` ve `MTEXT` budur.

## Nasıl saklanır

**Geometrisi taban çizgisidir:** iki köşeli bir açık çizgi. İlk köşe yazının
**noktasıdır** (hizalama bu noktanın yazının neresinde durduğunu söyler), ikinci köşe
yönünü verir — dönüklük saklanan bir açı değil, bu doğru parçasının yönüdür.
Uzunluğu yazının kutusudur: seçme ve ekrana girme onunla yapılır; satırları kırılan
bir yazıda satırların kırıldığı genişliktir.

Yanında, yazı tablosunda dört şey durur:

| Alan | Ne |
|---|---|
| Metin | Yazının kendisi; `\n` satır sonudur, boş satır kalır |
| Yükseklik | Büyük harfin boyu, **zemin milimetresi** |
| Hizalama | Dokuz noktadan biri: `sol`, `orta`, `sag`, `orta_sol`, `merkez`, `orta_sag`, `ust_sol`, `ust_orta`, `ust_sag` |
| Satır düzeni | Satır aralığı (tek aralığın katı, 0,25–4) ve satırların taban çizgisinin uzunluğunda kırılıp kırılmadığı |

## Nasıl çizilir

Programla gelen **IBM Plex Sans** yüzüyle; ekranda, PDF'te ve yazıcıda **aynı
kurallarla ve aynı ölçüyle**, çünkü üçü de satırları tek bir yerleşim işlevine
sorar. Yüzde olmayan bir harf üçünde de yüzün kendi boş kutusudur; başka bir yazı
tipinden alınmaz ([METİN](../komutlar/text.md)):

- Satırlar arası **yüksekliğin 5/3'ü** kadardır, satır aralığıyla çarpılır — DXF
  MTEXT'in "3'e 5" aralığı.
- Hizalamanın **sütunu** her satırı kendi başına sola, ortaya ya da sağa yaslar;
  **sırası** noktayı ilk satırın büyük harflerinin üstüne, yazının ortasına ya da son
  satırın tabanına koyar. Tek satırlı yazıda bu üç sıra DXF TEXT'in üst, orta ve
  taban hizalarıdır.
- Kırılan bir yazıda bir sonraki kelime satırı taban çizgisinin uzunluğundan öteye
  taşıyacaksa satır ondan önceki boşlukta kırılır; tek başına daha uzun bir kelime
  kendi satırında kalır.
- Ekranda 3 pikselden kısa kalan yazı çizilmez: okunmayan bir yazı paftayı lekeler.

## Yakalama noktaları

| Mod | Nereye |
|---|---|
| Uç nokta | Taban çizgisinin iki ucuna — ilk uç yazının noktasıdır |

## Ölçüler

Alanı yoktur; **çevre** taban çizgisinin uzunluğudur.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `1` (çoklu çizgi) ve yazı bloğunun bir satırı: metin,
yükseklik, hizalama, satır aralığı ve kırılma bayrağı. Satır düzeni eklenmeden önce
yazılmış dosyalar o baytlarda sıfır taşır ve tek aralık okunur.

**DXF'e:**

- Tek satırlı ve satır düzeni varsayılan olan yazı **TEXT** olur; hizalama grup 72
  (sol, orta, sağ) ve 73 (taban, orta, üst) ile, nokta grup 11'de.
- Birden çok satırlı, satır aralığı ya da genişliği olan yazı **MTEXT** olur: hizalama
  bağlantı noktası (71: üst sıra 1–3, orta 4–6, alt 7–9), satırlar `\P` paragrafları,
  satır aralığı 44 (tam aralık, 73 = 2), genişlik 41 (kırılmayan yazıda 0), yön X ekseni
  (11/21/31). Ters bölü ve süslü parantez kaçışlıdır.

**DXF'ten:** MTEXT'in paragrafları satır olarak, bağlantı noktası dokuz hizadan biri
olarak, satır aralığı ve genişliği olduğu gibi gelir; biçim kodları (renk, yazı tipi,
alt çizgi, boy) atılır, yığılmış kesir `\S1/2;` "1/2" olur. TEXT'in üst, orta ve taban
hizaları olduğu gibi gelir. Alt hizası (73 = 1, harflerin inişi), yaslanmış ve
sığdırılmış yazı (72 = 3 ve 5, iki noktası iki ucudur) en yakın hizayla — başından,
yönüyle — çizilir ve bu transkriptte söylenir.

GeoPackage'a nokta ve `yazi`, `yukseklik_mm`, `aci`, `hizalama` alanlarıyla gider.

## Nesnesini izleyen yazı

Bir yazı bir nesneye [bağlanabilir](../islem/bagli-nesneler.md): kenarına, köşesine ya da
ortasına. Bağlı yazı nesneyle birlikte yer değiştirir; sözü bir kalıpsa (`{ada}`,
`{#alan} m²`) nesnenin köşesi çekildiğinde ya da bir sütunu değiştiğinde o işlemin içinde
yeniden yazılır. [ETİKET](../komutlar/label.md) yazdığı etiketleri böyle bağlar,
[BAĞLA](../komutlar/bagla.md) var olan bir yazıyı bağlar.

## Komutlar

```
METİN noktalar=0,0 yazi="PLAN NOTLARI\nYapı yaklaşma 5 m" yukseklik=2000 hizalama=ust_sol satir_araligi=1.5
```

[METİN](../komutlar/text.md) çizer, [YAZIDÜZENLE](../komutlar/edittext.md) değiştirir,
[ETİKET](../komutlar/label.md) öznitelikten yazar.

## Sınırlar

Yazı tipi seçilemez; bütün yazılar aynı yüzle çizilir. Bir başka program MTEXT'in
genişliğe göre kırılan satırlarını kendi yazı tipinin ölçüsüyle kırar; satır sonları
açıkça yazılmış (`\n`) bir yazı her yerde aynı satırlara ayrılır.
