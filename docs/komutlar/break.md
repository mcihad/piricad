# KIR — Parça Çıkar

Bir çite kapı boşluğu, bir duvarın geçtiği yerde bir boru kesintisi ya da bir
sembolün oturacağı çentik açacak herkes için.

## Ne yapar

Çizgiden **iki nokta arasındaki parçayı çıkarır**. Geriye iki parça kalır ve
aralarında bir boşluk olur.

**`BÖL` ile karıştırmayın.** `BÖL` çizgiyi ikiye ayırır ve iki parçayı da tutar —
bir ifrazın ihtiyacı budur. `KIR` aradaki parçayı **atar**.

Tek nokta verilirse boşluk bırakmadan böler; bu, aynı fiilin sınır hâlidir ve
AutoCAD'in *break at point*'idir.

Tıklama sırası önemsizdir: bir el uzak ucu önce tıklar ve boşluk iki durumda da
aynı boşluktur.

## Adlar

| Ad | Tür |
|---|---|
| `KIR` | Türkçe, birincil |
| `BREAK` | İngilizce karşılık |
| `KR` | Kısaltma |
| `core.break` | Komut kimliği |

## Sözdizimi

```text
KIR nesne=<kimlik> birinci=<nokta> [ikinci=<nokta>]
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | seçim | 1 | Kırılacak çizgi |
| `birinci` | nokta | 1 | Çıkarılacak parçanın ilk noktası |
| `ikinci` | nokta | 0..1 | İkinci noktası; verilmezse boşluksuz böler |

## Örnekler

### Komut satırı

100 metrelik bir çizgiden 40–60 arasını çıkarmak:

```text
KIR nesne=1 birinci=40,0 ikinci=60,0
```

```text
Çizgi kırıldı; iki parça kaldı.
```

Boşluk bırakmadan bölmek:

```text
KIR nesne=1 birinci=40,0 ikinci=40,0
```

### Arayüz

**Değiştir > Kır**. Çizgiyi seçip Enter'a basın, sonra iki noktayı tıklayın.
Nokta yakalama açıkken kırılma yerini mevcut bir kesişime yakalayabilirsiniz.

### Betik

```json
{ "cmd": "core.break", "args": {
    "nesne": [1], "birinci": [40000, 0], "ikinci": [60000, 0] } }
```

## Geri alma

Tek işlem, tek **Ctrl+Z**: geri alma çizgiyi bütün hâline döndürür, iki
parçasından birini değil.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `KIR tek bir çizgiyle çalışır; N nesne seçildi.` | Birden çok nesne seçili | Tek çizgi seçin |
| `Kırılma noktası çizgi üzerinde bulunamadı.` | Nokta çizgiye uzak | Çizginin üstünü tıklayın |
| `Kırılma çizginin tamamını götürüyor; parça bırakmıyor.` | İki nokta çizginin iki ucunda | Silmek için `SİL` kullanın |
| `Nesne N açık bir çizgi değil.` | Kapalı alan ya da eğri | `ÇİZGİDÜZENLE islem=ac` ile açın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [BÖL](split.md) — ikiye ayırır, parça atmaz
- [BUDA](trim.md) — bir sınıra kadar kısaltır
- [UZUNLUK](lengthen.md) — bir ucu doğrultusunda hareket ettirir
