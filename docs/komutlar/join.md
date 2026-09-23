# UÇUCA — Uç Uca Ekle

Parça parça sayısallaştırılmış bir yolu, bir dereyi ya da bir sınırı tek bir
nesne hâline getirecek herkes için.

## Ne yapar

Uçları birbirine **değen** çizgileri, yayları ve yaylı çoklu çizgileri tek bir
nesneye ekler. Sıra önemsizdir ve ters çizilmiş bir parça gerekirse çevrilir —
sayısallaştırılmış bir harita tam olarak böyle görünür.

**Yaylar yay kalır.** Yalnız doğru parçalarından oluşan bir zincir çoklu çizgi
olur; içinde yay olan bir zincir **yaylı çoklu çizgi** olur ve her yay kendi
merkeziyle, yarıçapıyla kalır — kirişine indirgenmez. Aynı çemberin uç uca iki
yayı tek bir yay olur. Uzunluk, eklenen parçaların uzunlukları toplamıdır.

### Kurallar

| Konu | Kural |
|---|---|
| **Yön** | Sonuç, seçilen **ilk** nesnenin yönündedir; öteki parçalar gerekirse çevrilir |
| **Katman, stil** | İlk nesnenin; başka katmandaki parça sayısı söylenir |
| **Öznitelikler** | İlk nesnenin değerleri kalır; değeri farklı olan sütunlar adıyla söylenir. `cakisma=reddet` ile katman ya da öznitelik farkı varsa hiç birleştirmez |
| **Z** | Model iki boyutludur; kot bir öznitelik olarak tutuluyorsa öznitelik kuralına tabidir |
| **Kimlik** | Sonuç ilk nesnenin türündeyse onun kimliğini taşır; tür değişirse (çizgi + yay → yaylı çoklu çizgi) yeni bir nesnedir. Her kaynağın hangi kimliğe dönüştüğü yapılandırılmış cevapta söylenir |
| **Kapalı şekil** | Daire, kapalı yaylı çoklu çizgi ve alan eklenmez — uçları yoktur |
| **Uçları buluşan zincir** | Açık kalır ve söylenir; kapalı alana çevirmek [`ÇİZGİDÜZENLE`](pedit.md) `islem=kapat`'ın işidir |

**`BİRLEŞTİR` ile karıştırmayın.** İkisi farklı sorudur:

| Komut | Ne yapar |
|---|---|
`UÇUCA` | Yalnız **çizgileri** ekler; toleransı **çağrının kendi** `tolerans=`'ı (varsayılan 1 mm) |
`BİRLEŞTİR` | Örtüşen **alanları** da birleştirir (poligon boolean); çizgide toleransı **projenin** düğüm toleransı (`AYAR düğüm_toleransı`, varsayılan 10 mm) |

İkisi de uçları birkaç milimetre kaçmış bir zinciri toparlar; fark **toleransın
nereden geldiğidir**. Tek bir zinciri bilerek daha gevşek toparlamak istiyorsanız
`UÇUCA tolerans=` o kararı o çağrıda verir ve projenin ayarına dokunmaz; bütün
ölçü işi aynı hassasiyetteyse düğüm toleransını bir kez ayarlayıp
[`BİRLEŞTİR`](combine.md) kullanmak doğrudur. Alanlarda yalnız `BİRLEŞTİR`
çalışır.

Adları karıştırmak, ikisini de kullanılmaz kılar; bu yüzden her iki sayfa
öbürünü adıyla anar.

## Tolerans

Uçların "değmiş" sayılması için en büyük açıklık `tolerans=` ile verilir,
**metre** cinsinden, varsayılan **1 mm**. Bu bir ölçünün özelliğidir, farenin
değil: kadastral bir çizim 1 mm ister, elle sayısallaştırılmış bir harita bir
metre isteyebilir.

**Tolerans gizlenmez ve hiçbir şey yerinden oynatılmaz.** Tam değen iki uç aynı
noktayı paylaşır; tolerans içindeki bir açıklık, parçaları kaydırmadan **bir doğru
parçasıyla kapatılır** ve komut kaç boşluk kapattığını ve en büyüğünü söyler.
Zincire değmeyen bir çizgi **olduğu gibi bırakılır** — yerine çekilmez.

## Adlar

| Ad | Tür |
|---|---|
| `UÇUCA` | Türkçe, birincil |
| `UCUCA` | ASCII katlanmış Türkçe |
| `JOIN` | İngilizce karşılık |
| `UÇE` | Kısaltma |
| `core.join` | Komut kimliği |

## Sözdizimi

```text
UÇUCA nesne=<kimlik> nesne=<kimlik> [nesne=… …] [tolerans=<m>] [cakisma=ilk|reddet]
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | seçim | 0..n | Uç uca eklenecek çizgiler, yaylar ve yaylı çoklu çizgiler; ilki yönü ve değerleri verir |
| `tolerans` | sayı | 0..1 | Uçların değmiş sayılması için en büyük açıklık (m); varsayılan 0,001. Aradaki boşluk doğru parçasıyla kapatılır |
| `cakisma` | sözcük | 0..1 | `ilk` (varsayılan): katman ve öznitelikler ilk nesneden · `reddet`: fark varsa birleştirmez |

## Örnekler

### Komut satırı

Üç parçayı — biri ters çizilmiş — tek çizgiye eklemek:

```text
UÇUCA nesne=1 nesne=2 nesne=3
```

```text
3 çizgi tek bir çizgiye eklendi (4 köşe).
```

5 metrelik bir açıklığı kapatmak — açıklık bir doğru parçasıyla kapanır:

```text
UÇUCA nesne=1 nesne=2 tolerans=5
```

```text
2 çizgi tek bir çizgiye eklendi (4 köşe).
  1 boşluk doğru parçasıyla kapatıldı; en büyüğü 5000 mm (tolerans 5000 mm).
```

Bir çizgiyle bir yayı (`1`, `2`) eklemek — sonuç yaylı çoklu çizgidir, yay yay
kalır:

```text
UÇUCA nesne=1 2
```

Katmanları ya da öznitelikleri farklıysa hiç birleştirmemek:

```text
UÇUCA nesne=1 2 cakisma=reddet
```

### Arayüz

**Değiştir > Uç Uca Ekle**. Çizgileri seçip Enter'a basın. Önce seçtiğiniz nesne
yönü, katmanı ve öznitelikleri verir; transkript, kapatılan boşlukları ve farklı
olan katman ve öznitelikleri yazar.

### Betik

```json
{ "cmd": "core.join", "args": { "nesne": [1, 2, 3], "tolerans": 0.005 } }
```

## Geri alma

Tek işlem, tek **Ctrl+Z**.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır; her parametre için tip ve adet üretilmiş
[komut referansındadır](referans.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `UÇUCA en az iki çizgi ister; N geldi.` | Tek nesne seçili | En az iki çizgi seçin |
| `Seçilen çizgilerin uçları birbirine değmiyor (tolerans N mm). Uçları yakalama açıkken yeniden çizin ya da tolerans= ile büyütün.` | Açıklık toleransı aşıyor | `tolerans=` ile büyütün ya da uçları yakalama açıkken yeniden çizin |
| `Nesne N kapalı; ucu olmayan bir şekil uç uca eklenmez.` | Daire, kapalı şekil ya da alan | Açık bir nesne seçin; alanı `ÇİZGİDÜZENLE islem=ac` ile açın |
| `Nesne N uç uca eklenemiyor; UÇUCA çizgi, yay ve yaylı çoklu çizgide çalışır.` | Elips, spline, nokta, yazı ya da blok | Eklenebilir bir nesne seçin |
| `UÇUCA: birleşecek nesneler farklı — katman, ad. İlk nesnenin değerleriyle birleştirmek için cakisma=ilk verin.` | `cakisma=reddet` ve katman ya da öznitelik farkı | Farkları giderin ya da `cakisma=ilk` verin |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [BİRLEŞTİR](combine.md) — alanları tek alan yapar, bu ise çizgileri
- [ÇİZGİDÜZENLE](pedit.md) — kapat / aç / ters / sadeleştir
- [KIR](break.md) — parça çıkarır
