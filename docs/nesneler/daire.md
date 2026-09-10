# Daire

## Nedir

Merkezi ve yarıçapıyla tanımlı tam çember. Ada içi ağaç, rögar, dönel kavşak adası,
koruma bandı.

## Nasıl saklanır

Tek açık halkada **iki** tepe noktası: merkez ve merkezin tam doğusunda, yarıçap
uzaklığında bir tutamak. Yarıçap iki X'in farkıdır — tam sayı çıkarma, yuvarlama
yok. Çizilen 128 köşe dosyaya yazılmaz. Yük taşımaz.

## Nasıl çizilir

Merkez etrafında 128 kenarlı düzgün çokgen, doğudan başlayıp saat yönünün tersine.
En büyük sapma yarıçapın binde 0,3'üdür: 300 m'lik bir dairede bir milimetrenin
onda birinden az. Köşeler kütüphane trigonometrisiyle değil, her platformda aynı
sonucu veren kendi rutinle üretilir.

## Yakalama noktaları

| Mod | Nereye |
|---|---|
| Merkez | Merkeze |
| En yakın | Çemberin imlece en yakın noktasına — merkezden yarıçap boyunca **tam** hesaplanır, çokgene değil |
| Dik ayak | Dik kilidi çemberin normali boyunca, yani yarıçap doğrultusunda çalışır |

Uç nokta ve orta nokta verilmez: dairenin köşesi de ucu da yoktur. Saklanan tutamak
hiçbir modda sunulmaz.

## Ölçüler

**Alan** π·r², **çevre** 2·π·r; ikisi de tanımdan hesaplanır ve milimetreye
yuvarlanır. Çizilen çokgenin alanı kullanılmaz.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `2` (`core.circle`) ve iki tepe noktası. DXF `CIRCLE`
merkezi ve yarıçapıyla gelir ve gider. GeoPackage'a `tur=core.circle` alanıyla
çokgen olarak yazılır ve geri okunurken daire olur.

## Komutlar

```
DAİRE merkez=485300,4310200 cevre=485325,4310200
```

[DAİRE](../komutlar/circle_draw.md). Merkez ve yarıçap `ÖLÇ` ile okunur.

## Sınırlar

Yarıçap sıfır olamaz. Daire ölçeklenebilir ve taşınabilir; eşit olmayan ölçek
(yalnız bir eksende) onu elipse çevirmez, reddedilir.
