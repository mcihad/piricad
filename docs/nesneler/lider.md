# Kılavuz çizgi

## Nedir

Bir noktayı gösteren, ucunda ok olan çizgi; yanına konan yazıyla birlikte bir nesneye
not düşer. DXF `LEADER` budur.

## Nasıl saklanır

Tek açık halka: okun ucundan yazının yanına köşeler. Yükte okun olup olmadığı, ok
boyu (zemin milimetresi) ve kaynağın spline olarak çizdiğine dair bayrak durur. Yazı
kılavuz çizginin parçası **değildir**: son köşenin yanına ayrı bir yazı nesnesi konur, her
CAD biçiminin yaptığı gibi — ama kılavuzun ucuna **bağlıdır** (bağ türü `uc`): ucu izler,
son parça yön değiştirince tarafını ve yaslanışını değiştirir, kılavuz silinince silinir
([LİDER](../komutlar/leader.md)). Bağ proje dosyasına yazılır.

## Nasıl çizilir

Köşeler sırayla ve ilk köşede, ilk kenarın doğrultusunda dolu üçgen ok. Ok boyu
ölçü stilinden gelir.

## Yakalama noktaları

| Mod | Nereye |
|---|---|
| Uç nokta | Her köşeye |
| Orta nokta, en yakın, dik ayak, kesişim | Kenarların üzerine |

## Ölçüler

**Çevre** çizginin uzunluğudur (ok sayılmaz); alanı yoktur.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `11` (`core.leader`). DXF `LEADER` köşeleri ve ok
bayrağıyla gelir ve gider; dosyanın bağladığı yazı kendi `MTEXT`/`TEXT` nesnesi olarak
ayrıca gelir. DXF `MULTILEADER`'ın her kılavuz çizgisi de bir kılavuz çizgi olarak gelir —
okun ucundan inişin sonuna, oku boyuyla — ve tek satırlı yazısı kılavuzun ucuna bağlanır
([ayrıntı](../veri/dis-formatlar.md)). GeoPackage'a çizgi olarak yazılır.

## Komutlar

```
LİDER noktalar=0,0 3,3 6,3 metin=Rögar
```

[LİDER](../komutlar/leader.md).

## Sınırlar

Spline kılavuz çizgi düz kenarlarla çizilir; bayrağı korunur ve DXF'e geri yazılır.
