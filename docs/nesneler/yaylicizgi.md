# Yaylı çoklu çizgi

## Nedir

Kenarları düz **ya da yay** olabilen çoklu çizgi ya da alan: kaldırım kenarı, yol
sınırı, köşesi yuvarlatılmış bina. DXF'in şişkinlikli `LWPOLYLINE`'ı budur; bugüne
kadar yayları düz parçalara bölünerek okunuyordu, artık yay olarak saklanır.

## Nasıl saklanır

Halka, **köşelerdir** — düz çoklu çizgi gibi tek açık ya da kapalı halka. Hangi
kenarın büküldüğü ve nasıl büküldüğü **yükte** durur: her yay kenar için kenar
numarası, yayın **merkezi**, tam **yarıçapı** ve yönü (saat yönünün tersine mi).
Şişkinlik sayısı (bulge) saklanmaz; DXF'ten okunurken merkeze ve yarıçapa
çevrilir, yazılırken geri hesaplanır. Yay bir tanımdır, resmi değil; `YAY`
nesnesiyle aynı kural. İsteğe bağlı sabit kalınlık da yükte korunur (DXF gidiş-dönüşü
için; çizimde kullanılmaz).

## Nasıl çizilir

Köşeler sırayla; her yay kenar `YAY`'ın çizdiği aynı belirlenimci rutinle
(kirişin tekrarlı bölünmesi, trigonometri yok) köşeleri arasına açılır. Saat yönündeki
yay aynı noktaların tersten yürünmesidir.

## Yakalama noktaları

| Mod | Nereye |
|---|---|
| Uç nokta | Her köşeye |
| Orta nokta | Düz kenarın ortasına; yay kenarın **yay boyunca** ortasına |
| Merkez | Her yay kenarın merkezine |
| En yakın, dik ayak, kesişim | Çizilen kenarların üzerine |

## Ölçüler

**Alan** tamdır: köşelerin çokgeninin alanı, artı dışa bükülen her yayın **daire
parçası** (r²(θ − sin θ)/2), eksi içe bükülenlerinki. **Çevre** düz kenarların
uzunluğu ve her yayın r·θ'sı; θ tam mikroderece olarak hesaplanır. Bir yarım daireyle
büküllen 10 m'lik karenin alanı 100 + 12,5π m² olarak milimetrekaresine kadar doğru
çıkar.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `6` (`core.arc_polyline`), köşeler halka olarak, yaylar
yük olarak. DXF okurken şişkinliği olan `LWPOLYLINE` ve `POLYLINE` bu tür olur;
yazarken her yay kenar şişkinliğe geri çevrilip `LWPOLYLINE` olarak gider. GeoPackage'a
çizilen biçimiyle çizgi ya da çokgen olarak yazılır.

## Komutlar

Bu türü çizen komut henüz yoktur; `ÇOKLUÇİZGİ`'ye yay kenar eklemek Faz 2'nin
sonraki işidir. Tür DXF içe aktarımıyla gelir; `TAŞI`, `DÖNDÜR`, `ÖLÇEKLE` ve `AYNALA`
yayları köşelerle birlikte taşır (aynalamada yön çevrilir), `SİL` ve `GERİAL` her
nesnede olduğu gibi çalışır.

```
İÇEAKTAR dosya="yol.dxf"
```

[İÇEAKTAR](../komutlar/import.md), [TAŞI](../komutlar/move.md).

## Sınırlar

Kenar başına değişen kalınlık okunmaz ve söylenir. Bir halkadan çok halkalı (delikli)
yaylı alan bu sürümde yoktur; delikli bir taramanın yaylı sınırı çizgi parçalarına
bölünür.
