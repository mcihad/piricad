# KESİŞİMNOKTA — Kesişimden Nokta

Taş taşı kaybolmuş bir köşeyi geri koyacak herkes için; bu sayfayı bitirdiğinizde
bir köşeyi üç yoldan yeniden kurmayı bileceksiniz.

## Ne yapar

Kaybolan bir köşe üç şekilde geri kurulabilir ve `KESİŞİMNOKTA` üçünü de yapar:

| `yontem` | Ne verir | Ne ister |
|---|---|---|
| `dogrultu` (varsayılan) | İki bilinen noktadan okunan iki doğrultunun kesişimi | iki nokta, iki açı |
| `mesafe` | İki bilinen noktadan ölçülen iki uzaklığın kesişimi | iki nokta, iki uzaklık, `yon` |
| `dogru` | İki doğrunun — sınırların uzatılmış hâlinin — kesişimi | dört nokta |

`dogru` doğruları **sonsuz** kabul eder: iki sınırın kesişeceği köşeyi verir,
parçalar birbirine değmese de. Bir ifrazda taş taşı gitmiş köşe tam budur.

### İki uzaklığın iki çözümü vardır

`mesafe` yöntemi iki çember kesiştirir ve iki çemberin iki kesişimi olur.
Hangisini istediğinizi **`yon`** söyler: `sol` ya da `sag`, birinci→ikinci
yönüne göre. Sessizce biri seçilmez — bir sınırı yolun yanlış tarafına koymanın
yolu tam olarak budur.

Hesap, komut satırındaki [`kes(...)`](komut-satiri.md#nokta-fonksiyonları) nokta
fonksiyonuyla **aynı** hesaptır: aynı cevap, aynı çözüm seçimi ve aynı Türkçe
ret cümlesi, iş yazılmış da olsa tıklanmış da olsa.

## Adlar

| Ad | Tür |
|---|---|
| `KESİŞİMNOKTA` | Türkçe, birincil |
| `KESISIMNOKTA` | ASCII katlanmış Türkçe |
| `INTERSECTPT` | İngilizce karşılık |
| `KSN` | Kısaltma |
| `core.intersect_point` | Komut kimliği |

## Sözdizimi

```text
KESİŞİMNOKTA [yontem=dogrultu] birinci=<nokta> birinci_aci=<açı> ikinci=<nokta> ikinci_aci=<açı>
KESİŞİMNOKTA yontem=mesafe birinci=<nokta> birinci_mesafe=<m> ikinci=<nokta> ikinci_mesafe=<m> [yon=sol|sag]
KESİŞİMNOKTA yontem=dogru birinci=<nokta> ikinci=<nokta> ucuncu=<nokta> dorduncu=<nokta>
```

## Parametreler

Üretilmiş [komut referansına](referans.md) bakın; hangi parametrenin istendiği
`yontem`e bağlıdır ve arayüzde sırayla sorulur.

## Örnekler

### Komut satırı

100 metrelik bir karenin merkezini üç yoldan bulmak — üçü de (50, 50) verir:

```text
KESİŞİMNOKTA yontem=dogru birinci=0,0 ikinci=100,100 ucuncu=100,0 dorduncu=0,100
KESİŞİMNOKTA yontem=mesafe birinci=0,0 birinci_mesafe=70.710678 ikinci=100,0 ikinci_mesafe=70.710678 yon=sol
KESİŞİMNOKTA yontem=dogrultu birinci=0,0 birinci_aci=50 ikinci=100,0 ikinci_aci=350
```

```text
Kesişim noktası yerleştirildi.
```

### Arayüz

**Çizim > Kesişim Noktası** menüsünden ya da araç kutusundaki **Nokta** düğmesini
basılı tutup karttan seçin. Yöntem `yontem=` ile verilir; verilmezse
`dogrultu`dur. Komut ne istediğini sırayla sorar ve yazı isteyen her istemde odak
komut satırına geçer.

### Betik

```json
{ "cmd": "core.intersect_point", "args": {
    "yontem": "mesafe",
    "birinci": [0, 0], "birinci_mesafe": 70.710678,
    "ikinci": [100000, 0], "ikinci_mesafe": 70.710678,
    "yon": "sol" } }
```

### Üçü de aynı

Aynı kesişim, üç istemciden aynı noktayı ve aynı günlüğü bırakır.

## Geri alma

Tek nokta, tek işlem, tek **Ctrl+Z**.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır. `yontem` ve `yon` birer sözcük
listesidir; bus onları gövde çalışmadan önce doğrular.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `kes(): iki doğrultu paralel, kesişmiyorlar. Açılar: …` | İki doğrultu aynı ya da tam ters | Açılardan birini kontrol edin |
| `kes(): çemberler birbirine ulaşmıyor. Yarıçaplar … merkezler arası …` | İki uzaklığın toplamı merkezler arası mesafeden küçük | Hangi ölçünün yanlış olduğunu rakamlardan görün |
| `kes(): bir çember ötekinin tamamen içinde. …` | Uzaklık farkı merkezler arasından büyük | Aynı |
| `kes(): iki merkez aynı nokta, kesişim tek bir nokta değil.` | İki bilinen nokta aynı | Ayrı iki nokta verin |
| `kes(): iki doğru paralel, kesişmiyorlar.` | `dogru` yönteminde iki doğru paralel | Noktaları kontrol edin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [Komut satırı](komut-satiri.md) — `kes(...)` nokta fonksiyonu, aynı hesabın yazılı hâli
- [ARANOKTA](point_along.md) — doğru üzerinde ara nokta
- [DİKAYAK](perp_offset.md) — taban çizgisine göre alım
- [ALIM](survey_polar.md) — açı ve kenarla nokta
