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

Yükün **ikinci düzeni** bunlara önek, sonek, tolerans (biçimi, üst ve alt sapma),
ölçünün kendi birimi (uzunlukta `0` çizimin birimi; açıda `0` derece — birimi olmayan
eski açılar böyle yazılmıştı — `1` grad, `2` derece, `3` radyan) ve **hangi pafta
ölçeği için boyutlandığı** ekler. İkinci düzen
yalnız bunlardan biri doluysa yazılır: bunları taşımayan ölçü, bu alanlar gelmeden
önceki baytlarıyla yazılır. Yeni çizilen her ölçü pafta ölçeğini taşır.

**Ölçülen ile yazılan ayrı saklanır.** Yükteki ölçülen değer hep noktalardan
hesaplanandır; yazı onun yazılışıdır. Elle yazılan metindeki `<>` ölçülen değerin
yeridir; `<>` taşımayan metin **elle yazılmış** sayılır ve program onu hiçbir yerde
ölçülen değer diye göstermez ([`ÖLÇÜDÜZENLE`](../komutlar/dimension_edit.md)).

## Nasıl çizilir

Resim saklanmaz, her seferinde tanımdan kurulur: tanım noktalarının ölçü çizgisi
üzerindeki ayakları, uzatma çizgileri, ölçü çizgisi (açısalda ve yay uzunluğunda
yay), uçlarda oklar (kapalı ok çizginin rengiyle dolu bir üçgendir; açık ok ve 45°
çentik çizgidir). Yazı, ölçü çizgisinin okunduğu yönde üstünde durur; hizalı ve
doğrusal ölçüde iki uzatma çizgisinin arasına sığmazsa ikincinin dışına çıkar, açı ve
yay uzunluğunda kola değmeyeceği yere kayar ya da kolun dışına çıkar; sığmayan oklar
dışarıdan içeri döner ([kurallar](../komutlar/dimension.md#paftadaki-çizimi)).
Yazı tam sayı aritmetiğiyle biçimlenir: 12 500 mm, metre, iki ondalık, virgül →
`12,50`; 90 000 000 mikroderece grad ile `100,00g`, derece ile `90,00°`. Aynı ölçü
her makinede aynı çizgilere ve aynı yazıya düşer.

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

Her `DIMENSION` kendi resmini adsız bir blokta (`*D1` …) taşır: çizgiler, dolu oklar
`SOLID`, yazı `TEXT`, katman 0 ve BYBLOCK; ölçü stili tablosu yazının yerini, ok türünü ve
açı birimini söyler ([ayrıntı](../komutlar/dimension.md#yay-uzunluğu-ölçüsü)). Geri
okunurken bu bloklar blok tanımı olarak alınmaz; ölçü tanım noktalarından yeniden kurulur.

DXF'te ölçünün yazısı (grup 1): ölçülen değeri okuyan program kendisi ölçsün diye
önekli, sonekli, toleranslı ya da şablonlu yazı `<>` ile gider (`R<>%%p0,05 m`);
elle yazılmış yazı yazıldığı gibi gider; kendi birimi olan ölçünün — grad yazan her
açı dahil — yazısı tam olarak yazılır, çünkü okuyan programın `<>`'si onu kendi
biriminde, açıyı kendi derecesinde ölçerdi. Birimi ve yazının `<>`'li kalıbı bu
programın kendi notunda (`KENTOSCAD` xdata grubunda `olcu.birim`, `olcu.yazi`) gider.
Geri okunduğunda `<>` taşıyan yazı ölçülen değer olarak kalır, elle yazılmış yazı elle
yazılmış olarak; tam yazılmış yazı da, başka bir program onu değiştirmediyse, kendi
biriminde yeniden **ölçülen** değerdir.

## Komutlar

```
ÖLÇÜ birinci=0,0 ikinci=12.5,0 konum=0,3
```

[ÖLÇÜ](../komutlar/dimension.md).

## Sınırlar

Yazı konumu komutta ölçü çizgisinin ortasıdır (sığmıyorsa dışında); yazı tutamağından
sürüklenen, `ÖLÇÜDÜZENLE yazi_yeri=` ile elle yerleştirilen ya da dosyadan elle taşınmış
gelen yazı yerinde kalır ve ölçü kaynağını izleyince onunla taşınır. Tolerans, ölçünün ondalığıyla yazılır; ayrı bir
tolerans ondalığı yoktur. Ölçek değiştirildiğinde (`ÖLÇEKLE`) ölçülen değer ve yazı da değişir; yazının
yüksekliği değişmez.
