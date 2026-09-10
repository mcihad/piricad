# Yay

## Nedir

Bir çemberin parçası: kaldırım dönüşü, yol kurbu, kavşak köşesi.

## Nasıl saklanır

Tek açık halkada **dört** tepe noktası: merkez, merkezin tam doğusunda yarıçap
tutamağı, başlangıç ucu, bitiş ucu. Süpürme her zaman başlangıçtan bitişe **saat
yönünün tersine**dir; saat yönünde bir yay uçları değişerek saklanır. Uçlar
milimetreye yuvarlandığı için çember üzerine yalnız bir milimetre içinde oturur. Yük
taşımaz.

## Nasıl çizilir

Başlangıçtan bitişe bir dizi köşe, kapanış kenarı yok. Köşeler iki birim vektör
arasındaki kirişin art arda ikiye bölünmesiyle üretilir — yalnız toplama, çarpma ve
karekök — bu yüzden her platformda aynıdır. Yarım turdan uzun bir süpürme önce
çeyrek turluk parçalara kesilir.

## Yakalama noktaları

| Mod | Nereye |
|---|---|
| Merkez | Merkeze |
| Uç nokta | İki uca |
| Orta nokta | Yayın **boyunca** ortasına — kirişin ortasına değil |
| En yakın | Yayın imlece en yakın noktasına, tam hesaplanır; yayın örtmediği kısım verilmez |
| Dik ayak | Yarıçap doğrultusunda |

## Ölçüler

**Uzunluk** r·θ: süpürme açısı uçların yönlerinden mikroderece cinsinden tam sayı
olarak bulunur (`atan2` kütüphane çağrısı yok), yarıçapla çarpılır, milimetreye
yuvarlanır. Uçları çakışan bir yay tam tur sayılır. **Alan** yayın kirişle
kapattığı dairesel dilimdir.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `3` (`core.arc`) ve dört tepe noktası. DXF `ARC` merkezi,
yarıçapı ve iki açısıyla gelir ve gider; aynalı bir düzlemdeki (normal −Z) yayın yönü
çizim düzleminde doğru çıkar. GeoPackage'a çizgi olarak yazılır ve geri okunurken yay
olur.

## Komutlar

```
YAY merkez=485300,4310200 baslangic=485340,4310200 bitis=485300,4310240
```

[YAY](../komutlar/arc_draw.md).

## Sınırlar

Yarıçap sıfır olamaz. Uçlar merkezden farklı uzaklıkta verilmişse yarıçap
başlangıç ucundan alınır, bitiş yalnız yönü söyler.
