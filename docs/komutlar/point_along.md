# ARANOKTA — Doğru Üzerinde Ara Nokta

Bir yol ekseni üzerine istasyon kazığı koyacak ya da bir kenarı bölecek herkes
için; bu sayfayı bitirdiğinizde iki nokta arasındaki doğru üzerinde nokta
üretmeyi bileceksiniz.

## Ne yapar

İki nokta bir doğru belirler. `ARANOKTA` o doğru üzerinde nokta koyar, üç
şekilde:

| Nasıl | Ne verir |
|---|---|
| `deger=<oran>` | Oran kadar ilerideki nokta; `0.5` orta nokta, `1.25` ucun ötesi |
| `yontem=mesafe deger=<m>` | İlk noktadan o kadar metre ileride |
| `sayi=<k>` | Doğruyu k eşit parçaya bölen **k−1** nokta |

`sayi=k` uçları tekrar koymaz: onlar zaten oradadır ve tekrar konması bir taşın
üstünde iki nokta bırakırdı.

`deger` birden çok verilebilir — bir çırpıda bir kazık dizisi.

Oran ve mesafe **aynı yuvarlamadan** geçer, yani `deger=0.2` ile 100 metrelik
bir doğruda `yontem=mesafe deger=20` aynı milimetreye düşer.

## Adlar

| Ad | Tür |
|---|---|
| `ARANOKTA` | Türkçe, birincil |
| `POINTALONG` | İngilizce karşılık |
| `ARN` | Kısaltma |
| `core.point_along` | Komut kimliği |

## Sözdizimi

```text
ARANOKTA <A> <B> deger=<oran> [deger=<oran> …]
ARANOKTA <A> <B> yontem=mesafe deger=<m> [deger=<m> …]
ARANOKTA <A> <B> sayi=<k>
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `birinci` | nokta | 1 | Doğrunun ilk noktası |
| `ikinci` | nokta | 1 | Doğrunun ikinci noktası |
| `yontem` | sözcük | 0..1 | `oran` (varsayılan) ya da `mesafe` |
| `deger` | sayı | 0..n | Oran ya da uzaklık; birden çok verilebilir |
| `sayi` | tamsayı | 0..1 | 2–1000: doğruyu bu kadar eşit parçaya böler |

## Örnekler

### Komut satırı

Çeyrek, orta ve üç çeyrek:

```text
ARANOKTA 0,0 100,0 deger=0.25 deger=0.5 deger=0.75
```

```text
3 ara nokta yerleştirildi.
```

20 metre ileride bir kazık:

```text
ARANOKTA 0,0 100,0 yontem=mesafe deger=20
```

Yol eksenini dört eşit parçaya bölen üç kazık:

```text
ARANOKTA 0,0 100,0 sayi=4
```

```text
3 ara nokta yerleştirildi (4 eşit parça).
```

### Arayüz

Şeritte **Çizim ▸ Nokta ve Alım ▸ Ara Nokta**'ya basın (okunda **Ara Nokta — mesafeden**
de vardır) ya da **Giriş ▸ Çizim** panelindeki **Nokta** düğmesinin okundan seçin. İki noktayı tıklayın; aradaki kılavuz fareyi izler.
Sonra komut satırı oran (ya da `yontem=mesafe` verdiyseniz uzaklık) ister ve
odak kendiliğinden oraya geçer. Sağ tık ya da **Esc** bitirir.

**Kazıklanan doğru ekranda kalır** okumaları yazarken. O doğru çizimin nesnesi
değil, komutun hatırladığı iki noktadır; eskiden verildiği anda ekrandan
kayboluyordu.

### Betik

```json
{ "cmd": "core.point_along", "args": {
    "birinci": [0, 0], "ikinci": [100000, 0], "sayi": 4 } }
```

## Geri alma

Bir çağrı tek bir işlemdir: kaç nokta koyduysa tek bir **Ctrl+Z** ile birlikte
gider.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır. `deger` bir **dizi** olarak verilir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `İki nokta aynı; üzerinde ara nokta bulunacak doğru yok.` | A ile B aynı | Ayrı iki nokta verin |
| `ara(): A ve B aynı nokta, üzerinde mesafe ölçülecek doğru yok.` | Aynı, `mesafe` yönteminde | Aynı |
| `'core.point_along': 'sayi' parametresi 2 ile 1000 arasında olmalı` | Bölme sayısı aralık dışında | 2–1000 arası bir sayı verin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [Komut satırı](komut-satiri.md) — `ara(A,B,oran)` ve `uzanti(A,B,mesafe)` nokta fonksiyonları
- [KESİŞİMNOKTA](intersect_point.md) — kesişimden nokta
- [BÖLÜMLE](../README.md) — nesneyi eşit parçalara bölmek (P3)
