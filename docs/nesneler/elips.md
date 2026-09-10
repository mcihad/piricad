# Elips

## Nedir

Merkezi ve iki ekseniyle tanımlı kapalı eğri: dönel kavşak, çim adası, süs havuzu.

## Nasıl saklanır

Tek açık halkada **üç** tepe noktası: merkez, birinci eksenin ucu, ikinci eksenin
ucu. Uçlar birer noktadır, uzunluk ve açı değil: dönmeyi vektörler taşır, hiçbir açı
saklanmaz ve okunurken trigonometri gerekmez. İki eksen de sıfır uzunlukta olamaz.
Tam elips yük taşımaz; **kısmi elips** (elips yayı) yükünde başlangıç ve bitiş
parametresini taşır — birinci eksenden saat yönünün tersine, mikroderece.

## Nasıl çizilir

128 köşeli kapalı çokgen, birinci eksenden başlayıp saat yönünün tersine. Köşeler
birim çember köşelerinin iki eksen vektörüyle ölçeklenmesidir; daireyle aynı
belirlenimci rutin. Kısmi elips başlangıçtan bitişe açık bir yay olarak, en çok turun
1/128'i adımlarla, iki ucu tam olarak çizilir.

## Yakalama noktaları

| Mod | Nereye |
|---|---|
| Merkez | Merkeze |
| Uç nokta | Dört eksen ucuna: komuta verilen iki uç ve merkeze göre aynaları; kısmi elipste yayın iki ucuna |
| En yakın, dik ayak, kesişim | Çizilen 128-genin üzerine — gerçek eğriye bir iki milimetre içinde |

Orta nokta, uzantı ve paralel verilmez: bir yaklaşığın kirişinin ortası eğrinin
geçmediği bir yerdir. Merkezden eksen ucuna giden tanım çizgilerine **hiçbir modda**
yakalanmaz; o çizgiler çizimde yoktur.

## Ölçüler

**Alan** π·|a×b|: iki eksen vektörünün vektörel çarpımı, dönmüş elipste de doğru.
**Çevre** Ramanujan'ın ikinci yaklaşımıyla hesaplanır — elipsin çevresi kapalı
biçimde yazılamaz — ve milyarda birkaç parça içinde doğrudur, milimetreye
yuvarlanır.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `5` (`core.ellipse`) ve üç tepe noktası, kısmi elipste
24 baytlık yük; dosyaya gidip elips olarak geri gelir. DXF `ELLIPSE` merkezi,
eksenleri ve parametre açılarıyla gelir ve gider: tam elips tam, kısmi elips kısmi.
GeoPackage'a `tur=core.ellipse` alanıyla çokgen olarak yazılır.

## Komutlar

```
ELİPS merkez=485300,4310200 birinci=485360,4310200 ikinci=485300,4310230
ELİPS merkez=485400,4310200 birinci=485460,4310200 ikinci=485400,4310230 baslangic=0 bitis=90
```

[ELİPS](../komutlar/ellipse_draw.md).

## Sınırlar

Kısmi elipsin alanı sıfırdır (açık eğri) ve çevresi çizilen yayın uzunluğudur; tam
elipsin kapalı biçimi yoktur.
