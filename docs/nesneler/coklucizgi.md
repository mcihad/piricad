# Çoklu çizgi ve alan

## Nedir

Çizimin temel nesnesi: art arda tepe noktalarından oluşan bir **çizgi** ya da kapalı
bir **alan**. Bir parsel sınırı, bir yol ekseni, bir bina tabanı bu türdür. Alan
boşluk (iç halka) ve birden çok parça taşıyabilir: yola terk ve irtifak bunu rutin
olarak üretir.

## Nasıl saklanır

Halkalar hâlinde. Her halka **açık** (çizgi), **dış** (alan sınırı) ya da **iç**
(boşluk) rolündedir ve bir parça numarası taşır. Kapanış noktası yinelenmez: dış ve
iç halkanın son köşesinden ilk köşesine giden kenar dosyada durmaz, programca
kapatılır. Halka sırası parça numarasına göre artan, her dış halkanın ardından kendi
iç halkaları gelir; bu sıra içerik özetinin parçasıdır.

Bir çizgi en az iki, bir alan halkası en az üç köşe ister. Bu tür yük taşımaz.

## Nasıl çizilir

Saklandığı gibi: köşeler ekrana taşınır, kenarlar aralarına çekilir. Alanın dolgusu
dış halkadan iç halkalar çıkarılarak boyanır. Uzaklaşmış görünümde bir kenar
ekranda bir pikselden kısaysa köşe atlanır; dolguda hiçbir köşe atlanmaz, çünkü
sınırından taşan bir plan lekesi kaba değil yanlış bir çizimdir.

## Yakalama noktaları

| Mod | Nereye |
|---|---|
| Uç nokta | Her köşeye |
| Orta nokta | Her kenarın ortasına |
| Kesişim | İki kenarın gerçekten kesiştiği yere |
| Dik ayak | Önceki noktadan kenara indirilen dikin ayağına |
| En yakın | Kenarın imlece en yakın noktasına |
| Ağırlık merkezi | Kapalı halkanın alan ağırlık merkezine |
| Uzantı, paralel, uzatılmış kesişim | Kenarın doğrultusundan türetilen noktalara |

## Ölçüler

**Alan**, dış halkalar toplanıp iç halkalar çıkarılarak, ayakkabı bağı formülüyle ve
her halkanın ilk köşesine ötelenerek tam sayı milimetrekare olarak hesaplanır; açık
halkanın alanı sıfırdır. **Çevre**, kenarların uzunluk toplamıdır; kapalı halkada
kapanış kenarı dahildir. `ALANÖLÇ` ve `ÖLÇ` bu sayıları verir.

## Dosya ve dış biçimler

Proje dosyasına tepe noktası, halka ve yuva sütunları olarak yazılır
([proje dosyası](../veri/proje-dosyasi.md)). DXF'te açık halka `LWPOLYLINE`, alan kapalı
`LWPOLYLINE` olarak yazılır (delik kendi kapalı `LWPOLYLINE`'ı); okurken kapalılık
dosyanın bayrağından alınır ve şişkinliği olan bir çizgi
[yaylı çoklu çizgi](yaylicizgi.md) olur. GeoPackage ve Shapefile'da `LineString`
ve `Polygon` olur ([dış biçimler](../veri/dis-formatlar.md)).

## Komutlar

```
ÇİZGİ 485300,4310200 485360,4310200
ÇOKLUÇİZGİ 485300,4310200 485360,4310200 485360,4310230
ALAN 485300,4310200 485360,4310200 485360,4310230 485300,4310230
```

[ÇİZGİ](../komutlar/line.md), [ÇOKLUÇİZGİ](../komutlar/polyline.md),
[ALAN](../komutlar/area.md); köşe düzenleme için [KÖŞETAŞI](../komutlar/vertex_move.md).

## Sınırlar

Kenarlar düz çizgidir: yay kenarlı bir sınır (DXF'teki şişkinlik) bugün düz
parçalara bölünür ve içe aktarma bunu `düşürme:` satırıyla söyler. Yaylı çoklu çizgi
türü Faz 2'de gelecek.
