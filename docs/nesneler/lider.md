# Lider

## Nedir

Bir noktayı gösteren, ucunda ok olan çizgi; yanına konan yazıyla birlikte bir nesneye
not düşer. DXF `LEADER` budur.

## Nasıl saklanır

Tek açık halka: okun ucundan yazının yanına köşeler. Yükte okun olup olmadığı, ok
boyu (zemin milimetresi) ve kaynağın spline olarak çizdiğine dair bayrak durur. Yazı
liderin parçası **değildir**: `LİDER metin=` verilirse son köşenin yanına ayrı bir
yazı nesnesi konur, her CAD biçiminin yaptığı gibi.

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
ayrıca gelir. GeoPackage'a çizgi olarak yazılır.

## Komutlar

```
LİDER noktalar=0,0 3,3 6,3 metin=Rögar
```

[LİDER](../komutlar/leader.md).

## Sınırlar

Spline lider düz kenarlarla çizilir; bayrağı korunur ve DXF'e geri yazılır.
