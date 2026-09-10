# Nokta

## Nedir

Ölçülmüş tek bir yer: nirengi, poligon noktası, röper, detay noktası. Kadastro
paftasındaki her sınır böyle bir noktadan ölçülmüştür.

## Nasıl saklanır

Tek açık halkada **bir** tepe noktası. Yük taşımaz. Kot (yükseklik) bir öznitelik
sütunudur, geometrinin parçası değil ([NOKTALAR](../komutlar/points.md)).

## Nasıl çizilir

Tek köşe olarak sahneye verilir; kanvas onu ölçekten bağımsız bir işaretle çizer
(katmanın gösterimine göre nokta simgesi ya da sabit piksel boyunda işaret).

## Yakalama noktaları

| Mod | Nereye |
|---|---|
| Düğüm | Noktanın kendisine — köşeden **önce** gelir: bir röperle bir parsel köşesi bir milimetre yakınsa ölçülen olan kazanır |

Başka mod uygulanmaz: noktanın kenarı, ortası, dik ayağı yoktur.

## Ölçüler

Alanı ve çevresi sıfırdır. İki nokta arası uzaklık `ÖLÇ` ile alınır.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `4` (`core.point`) ve tek tepe noktası. DXF'te `POINT`,
GeoPackage ve Shapefile'da `Point` olur; `NOKTALAR` bir metin dosyasından kotlu
nokta okur.

## Komutlar

```
NOKTA 485300,4310200
```

[NOKTA](../komutlar/point_draw.md), [NOKTALAR](../komutlar/points.md).

## Sınırlar

Bir nokta döndürülemez (yönü yoktur) ve ölçeklenemez; taşınır ve silinir.
