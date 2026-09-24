# Ölçü

## Nedir

Bir uzunluğu, yarıçapı, çapı ya da açıyı yazısı, ölçü çizgisi, uzatma çizgileri ve
oklarıyla gösteren nesne. DXF `DIMENSION` budur.

## Nasıl saklanır

Halka 0 yazının **taban çizgisidir** (yazı metin tablosunda, ortalanmış); halka 1
**tanım noktalarıdır**, türe göre sırayla: hizalı ve doğrusal ölçüde iki nokta ve ölçü
çizgisinin yeri; yarıçapta merkez ve çember üstü nokta; çapta iki karşı nokta; açısalda
tepe, iki kol ucu ve yayın geçtiği nokta; koordinat ölçüsünde başlangıç, ölçülen nokta
ve yazının yeri; yay uzunluğunda merkez, başlangıç, bitiş ve yazının yeri. Yükte tür, ok biçimi, doğrusal ölçünün açısı,
**ölçülen değer** (milimetre ya da mikroderece), stilin ölçüleri **zemin
milimetresine** indirilmiş hâliyle (ok boyu, uzatma fazlası, uzatma boşluğu, yazı
boşluğu), ondalık sayısı, ondalık ayracı, stil adı ve varsa elle yazılan metin durur.

## Nasıl çizilir

Resim saklanmaz, her seferinde tanımdan kurulur: tanım noktalarının ölçü çizgisi
üzerindeki ayakları, uzatma çizgileri, ölçü çizgisi (açısalda yay), uçlarda oklar
(dolu üçgen, açık ok ya da 45° çentik). Yazı tam sayı aritmetiğiyle biçimlenir:
12 500 mm, metre, iki ondalık, virgül → `12,50`; 90 000 000 mikroderece → `90,00°`.
Aynı ölçü her makinede aynı çizgilere ve aynı yazıya düşer.

## Bağlar

Bir tanım noktası başka bir nesnenin bir köşesine, bir dairenin ya da yayın merkezine,
bir yayın ucuna ya da çemberin üstündeki bir açıya **bağlı** olabilir
([Bağlı ölçü](../komutlar/dimension.md#bağlı-ölçü)). Bağlar ölçünün halkalarında değil,
ayrı bir **bağ tablosunda** durur: her bağ hangi tanım noktasının, hangi nesnenin (kalıcı
anahtarıyla) hangi özelliğine bağlı olduğunu ve bağın kopuk olup olmadığını tutar.
Kaynak değişince ölçü komutun sonunda, aynı işlemin içinde yeniden kurulur; her karede
değil. Bağlı ölçüsü olmayan bir çizim bu tablo için hiçbir şey ödemez: parmak izi de
dosyası da bağlardan önceki hâliyle aynıdır.

## Yakalama noktaları

| Mod | Nereye |
|---|---|
| Uç nokta | Her tanım noktasına |
| En yakın, dik ayak, kesişim | Çizilen çizgilerin ve yazının taban çizgisinin üzerine |

## Ölçüler

Ölçü nesnesinin kendi alanı ve çevresi sıfırdır; **gösterdiği** değer yükünde
durur ve yazısında okunur.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `10` (`core.dimension`), iki halka ve yük. DXF
`DIMENSION` türüyle (hizalı, doğrusal, yarıçap, çap, açısal, ordinat) gelir; stilin
ölçüleri dosyanın `DIMSTYLE` tablosundan, dosyada yoksa ISO-25'ten alınır ve söylenir;
dosyanın elle yazdığı metin korunur. Yazarken kendi türüyle `DIMENSION` olur ve stili
`DIMSTYLE` tablosuna yazılır. GeoPackage'a çizilen çizgiler ve yazı gider.

Bağlar proje dosyasında kendi bloğunda (`0x008E`, bağ başına 32 bayt: ölçünün ve
kaynağın anahtarı, özellik, halka, sıra, kopukluk) durur; blok yalnız bağlı bir ölçü
varsa yazılır. Dosyada olmayan bir nesneye işaret eden bağ okunurken **kopuk** sayılır.
DXF'e bağ yazılmaz; DXF'ten gelen ölçü bağsızdır.

## Komutlar

```
ÖLÇÜ birinci=0,0 ikinci=12.5,0 konum=0,3
```

[ÖLÇÜ](../komutlar/dimension.md).

## Sınırlar

Yazı konumu komutta ölçü çizgisinin ortasıdır; elle taşınmış yazı dosyadan geldiği yerde
kalır. Ölçek değiştirildiğinde (`ÖLÇEKLE`) ölçülen değer ve yazı da değişir; yazının
yüksekliği değişmez.
