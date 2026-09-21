# ÇOKGEN — Düzgün Çokgen

Bir cıvata dairesi, bir baca bileziği, bir pilon temeli ya da bir döşeme deseni
çizecek herkes için; bu sayfayı bitirdiğinizde düzgün bir çokgeni üç şekilde
tanımlamayı bileceksiniz.

## Ne yapar

Merkez ve kenar sayısından düzgün bir çokgen çizer. Boyutu üç şekilde verilir,
çünkü bir plan paftasında üç şekilde yazılır:

| `yontem` | Verilen sayı | Nerede kullanılır |
|---|---|---|
| `ic` (varsayılan) | Köşelerin üzerinde olduğu çemberin yarıçapı | Cıvata dairesi, baca bileziği, pilon temeli |
| `dis` | Kenarlara teğet çemberin yarıçapı | Somun, bordür pahı — ölçüsü *ağızdan* verilen her şey |
| `kenar` | Kenar uzunluğu | Döşeme deseni, parke dizilimi |

`aci` ilk köşenin merkeze göre doğrultusudur; oturumun açı birimi ve kuralıyla
okunur (varsayılan grad, kuzeyden saat yönünde). Verilmezse 0'dır, yani ilk köşe
kuzeydedir.

Köşeler `core::sin_cos_udeg`'den gelir ve dört ana eksende tam sayıdır: 0°'de
bir kare köşelerini milimetrenin üstüne koyar, bir milimetre yanına değil (§7.3).

## Adlar

| Ad | Tür |
|---|---|
| `ÇOKGEN` | Türkçe, birincil |
| `COKGEN` | ASCII katlanmış Türkçe |
| `POLYGONREG` | İngilizce karşılık |
| `ÇKG` / `CKG` | Kısaltma |
| `core.polygon_regular` | Komut kimliği |

**Bir *çokgen*, bir *poligon* değil.** Türkçe haritacılıkta poligon bir
güzergâhtır ve o ad [POLİGON](traverse.md) komutuna aittir.

## Sözdizimi

```text
ÇOKGEN <merkez> <kenar_sayisi> yaricap=<m> [yontem=ic|dis] [aci=<açı>]
ÇOKGEN <merkez> <kenar_sayisi> yontem=kenar kenar_uzunlugu=<m> [aci=<açı>]
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `merkez` | nokta | 1 | Çokgenin merkezi |
| `kenar_sayisi` | tamsayı | 1 | 3–1024 |
| `yontem` | sözcük | 0..1 | `ic` (varsayılan), `dis`, `kenar` |
| `yaricap` | sayı | 0..1 | `ic`/`dis` yönteminin yarıçapı (m) |
| `kenar_uzunlugu` | sayı | 0..1 | `kenar` yönteminin uzunluğu (m) |
| `aci` | sayı | 0..1 | İlk köşenin doğrultusu; varsayılan 0 |

## Örnekler

### Komut satırı

50 metrelik bir çemberin üzerinde bir kare — köşeleri eksenlerde, tam
milimetrede:

```text
ÇOKGEN 0,0 4 yaricap=50
```

```text
4 kenarlı çokgen çizildi.
```

Altıgenin kenarı yarıçapına eşittir, yani bu ikisi **aynı** altıgendir:

```text
ÇOKGEN 0,0 6 yaricap=10
ÇOKGEN 0,0 6 yontem=kenar kenar_uzunlugu=10
```

Ağzı 50 metre olan bir kare (dıştan) köşelerine 70,711 metre uzanır:

```text
ÇOKGEN 0,0 4 yontem=dis yaricap=50
```

50 grad döndürülmüş bir kare:

```text
ÇOKGEN 0,0 4 yaricap=50 aci=50
```

### Arayüz

**Çizim > Düzgün Çokgen** menüsünden ya da araç kutusundaki **Dikdörtgen**
düğmesini basılı tutup açılan karttan **Düzgün Çokgen**'i seçin. Merkezi
tıklayın; komut kenar sayısını ve ardından yarıçapı (ya da `yontem=kenar`
verdiyseniz kenar uzunluğunu) ister ve odak kendiliğinden komut satırına geçer.

### Betik

```json
{ "cmd": "core.polygon_regular", "args": {
    "merkez": [0, 0], "kenar_sayisi": 6,
    "yontem": "kenar", "kenar_uzunlugu": 10 } }
```

## Geri alma

Tek çokgen, tek işlem, tek **Ctrl+Z**.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır. `kenar_sayisi` 3–1024 aralığı bus'ta,
gövde çalışmadan önce denetlenir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `'core.polygon_regular': 'kenar_sayisi' parametresi 3 ile 1024 arasında olmalı` | Kenar sayısı aralık dışında | 3–1024 arası bir sayı verin |
| `Yarıçap ya da kenar uzunluğu sıfır ya da eksi olamaz.` | Ölçü sıfır ya da eksi | Artı bir değer verin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [DİKDÖRTGEN](rectangle.md) — dört köşeli, eksenlere paralel ya da döndürülmüş
- [ALAN](area.md) — köşelerini tek tek verdiğiniz kapalı alan
- [DAİRE](circle_draw.md) — dört yöntemle daire
