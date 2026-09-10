# Spline

## Nedir

Kontrol noktaları, derece ve düğümlerle tanımlı pürüzsüz eğri (NURBS): dere yatağı,
serbest çizilmiş sınır, peyzaj hattı. DXF `SPLINE` budur.

## Nasıl saklanır

Halka 0 **kontrol noktalarıdır**; kaynak dosya verdiyse halka 1 uydurma noktalarıdır.
Derece (1–15), düğümler ve ağırlıklar **yükte** durur; düğümler ve ağırlıklar
nano-sabit noktalı tam sayı olarak saklanır (10⁻⁹), hiçbir alan kayan nokta değildir.
Düğüm yoksa eğri **düzgün, uçları bağlı** düğümlerle çizilir: ilk kontrol noktasında
başlar, sonuncuda biter.

## Nasıl çizilir

De Boor algoritması: her düğüm aralığında 16 nokta, yalnız toplama, çıkarma, çarpma
ve bölme, ilk kontrol noktasına ötelenmiş çerçevede. Aynı spline üç işletim sisteminde
aynı milimetrelere düşer. Kapalı bayrağı açıksa son noktadan ilkine düz kapanır.

## Yakalama noktaları

| Mod | Nereye |
|---|---|
| Uç nokta | Açık eğrinin iki ucuna |
| Düğüm | Her kontrol noktasına ve uydurma noktasına |
| En yakın, dik ayak, kesişim | Çizilen eğrinin üzerine |

Kontrol noktaları eğrinin üzerinde değildir; bu yüzden **uç nokta** değil **düğüm**
olarak verilir.

## Ölçüler

**Çevre** çizilen eğrinin uzunluğudur. **Alan** yalnız kapalı spline'da, çizilen
biçimin kapattığı alandır — bir yaklaşıktır ve burada söylenir; kadastral alan
hesabında spline kullanmayın.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `7` (`core.spline`). DXF `SPLINE` derecesi, düğümleri,
ağırlıkları ve uydurma noktalarıyla gelir ve aynı şekilde gider. Yalnız uydurma noktası
taşıyan bir spline'ın uydurma noktaları kontrol noktası sayılır ve bu `düşürme:` ile
söylenir. GeoPackage'a çizilen biçimiyle çizgi olarak yazılır.

## Komutlar

```
SPLINE noktalar=0,0 10,20 20,20 30,0 derece=3
```

[SPLINE](../komutlar/spline.md).

## Sınırlar

Ağırlıklı (rasyonel) spline dosyadan okunur ve çizilir; komut ağırlık vermez.
Periyodik spline periyodik olarak işaretlenir ama kapalı bir eğri gibi çizilir.
