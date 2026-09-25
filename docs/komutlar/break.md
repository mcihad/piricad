# KIR — Parça Çıkar

Bir çite kapı boşluğu, bir duvarın geçtiği yerde bir boru kesintisi ya da bir
sembolün oturacağı çentik açacak herkes için.

## Ne yapar

Bir nesneden **iki nokta arasındaki parçayı çıkarır**. Geriye iki parça kalır ve
aralarında bir boşluk olur.

`KIR` çizgide, açık çoklu çizgide, **yayda**, **dairede**, **elipste**, **spline**'da
ve **yaylı çoklu çizgide** çalışır ve her parça kendi türünde kalır: bir yaydan çıkan
parça yaydır, kalanlar da yaydır; yaylı bir sınırın yayları düzleşmez; bir elipsten
aynı elipsin yayları, bir spline'dan kendi eğrisini çizen daha kısa spline'lar kalır.
Kalan parçalar kaynağın katmanını, stilini ve bütün özniteliklerini taşır.

- **Daire, tam elips ve kapalı yaylı çoklu çizgi**: birinci noktadan ikinciye,
  şeklin kendi yönünde — dairede ve elipste saat yönünün tersine — giden parça
  çıkar; geriye tek bir açık parça (dairede bir yay, elipste bir elips yayı) kalır.
  Kapalı bir şekil tek noktadan kırılmaz.
- **Alan kırılmaz**: bir parseli kenarı boyunca açmak onu iki çizgiye çevirirdi.
  Bilerek açmak için önce [`ÇİZGİDÜZENLE`](pedit.md) `islem=ac` kullanın.

**`BÖL` ile karıştırmayın.** `BÖL` çizgiyi ikiye ayırır ve iki parçayı da tutar —
bir ifrazın ihtiyacı budur. `KIR` aradaki parçayı **atar**.

Tek nokta verilirse açık bir nesneyi boşluk bırakmadan böler; bu, aynı fiilin sınır
hâlidir ve AutoCAD'in *break at point*'idir.

Açık bir nesnede tıklama sırası önemsizdir: bir el uzak ucu önce tıklar ve boşluk
iki durumda da aynı boşluktur. Kapalı bir şekilde sıra, hangi yandaki parçanın
gideceğini söyler.

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
| `nesne` | seçim | 1 | Kırılacak nesne: çizgi, yay, daire ya da yaylı çoklu çizgi |
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

Merkezi (0; 0), yarıçapı 10 m bir daireden doğudan kuzeye çeyreği çıkarmak —
geriye üç çeyreklik bir yay kalır:

```text
KIR nesne=1 birinci=10,0 ikinci=0,10
```

```text
Kapalı şekil kırıldı; açık bir parça kaldı.
```

### Elips

Boş bir çizimde, bir elipsten birinci eksen ucu (10; 0) ile ikinci eksen ucu (0; 5)
arasındaki çeyreği çıkarmak — geriye 270°'lik bir elips yayı kalır:

<!-- örnek: yeni çizim -->

```
ELİPS merkez=0,0 birinci=10,0 ikinci=0,5
KIR nesne=1 birinci=10,0 ikinci=0,5
```

### Arayüz

**Değiştir ▸ Kes ve Uzat ▸ Kır**. Nesneyi seçip Enter'a basın, sonra iki noktayı tıklayın.
Nokta yakalama açıkken kırılma yerini mevcut bir kesişime yakalayabilirsiniz.

İkinci nokta aranırken **gidecek parça işaretli çizilir**: birinci nokta ile imleç
arasında, nesne boyunca — yayda yayın kendisi olarak — kırmızı ve kesikli; kalacak
parçalar vurgulu. İmlecin yanında çıkacak parçanın **nesne boyunca** uzunluğu
yazar.
İkinci noktada tıklamak yerine **Enter**'a basarsanız çizgi birinci noktada, arası
açılmadan ikiye bölünür.

### Betik

```json
{ "cmd": "core.break", "args": {
    "nesne": [1], "birinci": [40000, 0], "ikinci": [60000, 0] } }
```

## Geri alma

Tek işlem, tek **Ctrl+Z**: geri alma çizgiyi bütün hâline döndürür, iki
parçasından birini değil.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır. Yapılandırılmış cevap, kaynağın hangi
kimliklere dönüştüğünü söyler: `{"kaynak": 1, "sonuc": [1, 2]}`. Bir daireden
kalan yay yeni bir nesnedir, çünkü yay daireden başka bir türdür.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `KIR tek bir çizgiyle çalışır; N nesne seçildi.` | Birden çok nesne seçili | Tek çizgi seçin |
| `Kırılma noktası çizgi üzerinde bulunamadı.` | Nokta çizgiye uzak | Çizginin üstünü tıklayın |
| `Kırılma çizginin tamamını götürüyor; parça bırakmıyor. Silmek için SİL kullanın.` | İki nokta çizginin iki ucunda | Silmek için `SİL` kullanın |
| `Kırılma şeklin tamamını götürüyor; parça bırakmıyor. Silmek için SİL kullanın.` | Kapalı şekilde iki nokta tüm şekli kapsıyor | Silmek için `SİL` kullanın |
| `Kırılma noktası çizginin ucunda; bölünecek bir şey kalmıyor.` | Tek nokta açık nesnenin ucunda | Nesnenin içinde bir nokta verin |
| `Kapalı bir şekil tek noktadan kırılmaz; iki ayrı nokta verin.` | Daireye ya da kapalı şekle tek nokta | İkinci bir nokta verin |
| `Nesne N bir alan; alan kırılmaz. Önce ÇİZGİDÜZENLE islem=ac ile açık çizgiye çevirin.` | Alan seçildi | `ÇİZGİDÜZENLE islem=ac` ile açın |
| `Nesne N kırılamıyor; KIR çizgi, yay, daire, elips, spline ve yaylı çoklu çizgide çalışır.` | Nokta, yazı ya da blok | Kırılabilir bir nesne seçin |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [BÖL](split.md) — ikiye ayırır, parça atmaz
- [BUDA](trim.md) — bir sınıra kadar kısaltır
- [UZUNLUK](lengthen.md) — bir ucu doğrultusunda hareket ettirir
