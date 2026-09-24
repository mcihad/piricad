# ÖLÇÜDÜZENLE — Çizilmiş Ölçüyü Düzenleme

Bir ölçünün önüne `R` koyan, ardına ` m` yazan, bir boru boyuna tolerans ekleyen,
ondalık sayısını ya da birimini değiştiren, yazıyı ölçü çizgisinden kaldırıp yanına
koyan ya da ölçülen değerin yerine elle bir değer yazmak zorunda kalan herkes için;
bu sayfayı bitirdiğinizde bunları arayüzden, komut satırından ve betikten yapmayı ve
elle yazılmış bir değerin nasıl ayırt edildiğini bileceksiniz.

## Ne yapar

`ÖLÇÜDÜZENLE`, çizimde **duran** ölçülerin **yazılışını** değiştirir: önek, sonek,
tolerans, birim, ondalık sayısı, stil, yazının yeri ve yazının kendisi. Ölçünün
**ölçtüğü** değer değişmez; o, noktalardan hesaplanır.

**Ölçülen ile yazılan ayrı tutulur.** Yazıdaki `<>` ölçülen değerdir: `metin="<>
(tapu)"` her zaman ölçüyü yazar, köşe taşınınca yenisini. `<>` taşımayan bir yazı
(`metin="19,99"`) **elle yazılmış** sayılır ve program onu ölçülen değer diye
göstermez:

- tuvalde yazının altında uyarı renginde **elle yazılmış · ölçülen 20,00** durur;
- [`NESNEBİLGİ`](entity_info.md) `ölçtüğü 20,00, yazdığı "19,99" — ELLE YAZILMIŞ`
  der, yapılandırılmış cevapta `elle: true` ve ölçülen değer ayrı alanlardadır;
- nitelik panelinin **ÖLÇÜ** grubunda yazı satırı **ELLE** rozeti taşır;
- [bağlı](dimension.md#bağlı-ölçü) bir ölçü köşesi taşınınca yeniden ölçülür, ama
  elle yazılmış yazısı değişmez ve bu söylenir;
- elle yazılmış yazıya önek, sonek ya da tolerans **eklenmez**: elle yazılmış bir
  sayıyı ölçülmüş gibi süslemek olurdu.

`sifirla=metin` (ya da `metin="<>"`) yazıyı yeniden ölçüye bağlar.

**Verilmeyen hiçbir şey değişmez.** Yalnız `onek` verirseniz tolerans, birim ve yazı
olduğu gibi kalır.

## Adlar

| Ad | Tür |
|---|---|
| `ÖLÇÜDÜZENLE` | Türkçe, birincil |
| `OLCUDUZENLE` | ASCII karşılık |
| `DIMEDIT` | İngilizce karşılık |
| `ÖDZ`, `ODZ` | Kısaltma |
| `core.dimension_edit` | Komut kimliği |

## Sözdizimi

```text
ÖLÇÜDÜZENLE [nesneler=<kimlik> …] [metin=<yazı>] [onek=<yazı>] [sonek=<yazı>]
            [birim=cizim|mm|cm|m|km|grad|derece|radyan] [hassasiyet=<0..8>]
            [tolerans=<m>] [tolerans_ust=<m>] [tolerans_alt=<m>] [tolerans_bicim=simetrik|sapma|sinir]
            [stil=<ad>] [yazi_yeri=<nokta>] [sifirla=metin|onek|sonek|tolerans|birim|hassasiyet|yazi_yeri|hepsi …]
```

`nesneler` verilmezse **seçim** kullanılır; o da boşsa ölçüyü göstermeniz istenir.
Hiçbir şey verilmezse yazı sorulur ve ölçünün bugünkü yazısı önerilir (ölçülen
değerse `<>`). İçinde boşluk olan yazı tırnak içine alınır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Düzenlenecek ölçüler. Ölçü olmayan nesneler atlanır ve sayılır |
| `metin` | Yazı. `<>` ölçülen değerdir; `<>` taşımayan yazı elle yazılmış sayılır. `<>` ya da boş yazı ölçüye döndürür |
| `onek`, `sonek` | Değerin önüne ve ardına yazılanlar: `R`, `Ø`, `≈`, ` m`, ` (eski)` |
| `birim` | Değerin yazıldığı birim. Uzunlukta `cizim` çizimin birimini ([`AYAR çizim_birimi`](setting.md)) izler, ya da `mm`, `cm`, `m`, `km`; açı ölçüsünde `grad`, `derece`, `radyan`. `sifirla=birim` uzunluğu çizimin birimine, açıyı projenin [`açı_birimi`](setting.md) ayarına döndürür — yeni bir ölçünün yazacağına |
| `hassasiyet` | Ondalık basamak sayısı, 0–8. Tolerans da aynı ondalıkla yazılır |
| `tolerans` | Simetrik tolerans: `±` bu kadar. Uzunlukta **metre**, açıda **derece** olarak verilir ve ölçünün biriminde yazılır: grad yazan bir açıda `tolerans=0.9` → `±1,00g`. `tolerans=0` toleransı kaldırır |
| `tolerans_ust`, `tolerans_alt` | Sapma: `+üst/-alt`. İkisi de pozitif yazılır |
| `tolerans_bicim` | `simetrik`, `sapma` ya da `sinir`: iki sınır değer (`20,05/19,98`), ölçünün yerine |
| `stil` | Katalogdaki ölçü stili: ok, uzatma çizgileri, yazı boyu, ondalık ve ayraç ondan gelir; sonra verilenler onu değiştirir |
| `yazi_yeri` | Yazının yeri. Elle yerleştirilen yazı yerinde kalır, ölçü kaynağını izleyince onunla taşınır |
| `sifirla` | Stile döndürülecekler; birden çok kez yazılabilir. `hassasiyet` stilin ondalığına döner, `hepsi` hepsini döndürür |
| `katalog` | Stil kataloğu dosyası; varsayılan `TERCİH ölçü_stilleri` |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Bir yarıçapın önüne `R`, ardına ` m`, beş santimlik tolerans:

<!-- örnek: yeni çizim -->
```
DAİRE merkez=0,0 cevre=5,0
ÖLÇÜ tur=yaricap birinci=0,0 ikinci=5,0 konum=8,2
ÖLÇÜDÜZENLE nesneler=2 onek=R sonek=" m" tolerans=0.05
```

```text
Ölçü düzenlendi: 1 ölçü; yazısı "R5,00±0,05 m".
```

Tapu kaydındaki değeri yazmak zorundaysanız — elle yazılmış olarak kalır:

<!-- örnek: yeni çizim -->
```
ÇİZGİ 0,0 20,0
ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3
ÖLÇÜDÜZENLE nesneler=2 metin="19,99"
ÖLÇÜDÜZENLE nesneler=2 sifirla=metin
```

```text
Ölçü düzenlendi: 1 ölçü; yazısı "19,99" (elle yazılmış; ölçülen 20,00).
Ölçü düzenlendi: 1 ölçü; yazısı "20,00".
```

Ölçüyü koruyarak yanına not düşmek, santimetreyle ve sınır değerlerle:

<!-- örnek: yeni çizim -->
```
ÖLÇÜ birinci=0,0 ikinci=20,0 konum=10,-3
ÖLÇÜDÜZENLE nesneler=1 metin="<> (tapu)"
ÖLÇÜDÜZENLE nesneler=1 birim=cm hassasiyet=0
ÖLÇÜDÜZENLE nesneler=1 sifirla=hepsi tolerans_ust=0.05 tolerans_alt=0.02 tolerans_bicim=sinir
```

```text
Ölçü düzenlendi: 1 ölçü; yazısı "20,00 (tapu)".
Ölçü düzenlendi: 1 ölçü; yazısı "2000 (tapu)".
Ölçü düzenlendi: 1 ölçü; yazısı "20,05/19,98".
```

### Arayüz

**Açıklama ▸ Ölçü ▸ Ölçüyü Düzenle** (bir ölçü seçiliyken beliren **Ölçü** sekmesinde de
vardır): ölçüyü tıklayın, Enter'a basın, yazıyı yazın (`<>` ölçülen
değerdir). Öneki, soneki, birimi, ondalığı ve toleransı tek tek değiştirmek için
ölçüyü seçin: nitelik panelinin **ÖLÇÜ** grubunda her biri bir hücredir; hücreyi
düzenlemek bu komutun ilgili satırını çalıştırır.

Araç yalnız ölçü sorduğu için, ölçünün üst üste bindiği bir kenara tıklamak "hangisi?"
diye sormadan ölçüyü seçer; altta birden çok ölçü varsa seçim listesi açılır.

[`YAZIDÜZENLE`](edittext.md) bir ölçüye uygulanırsa aynı yola gider: yazıyı ölçünün
kendi modeline yazar; elle yazılan değer elle yazılmış olarak işaretlenir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.dimension",
      "args": { "birinci": [0, 0], "ikinci": [20000, 0], "konum": [10000, -3000] } },
    { "cmd": "core.dimension_edit",
      "args": { "nesneler": [1], "onek": "≈", "sonek": " m", "hassasiyet": 3 } }
  ]
}
```

### Üçü de aynı

Arayüzdeki soru, komut satırındaki `metin=` ve betikteki `"metin"` aynı değeri
bırakır; aynı belge, aynı günlük satırı (`tests/unit/test_dimtext.cpp`,
`ÖLÇÜDÜZENLE KANIT`).

## Geri alma

Tek adımdır: [`GERİAL`](undo.md) bütün ölçüleri düzenlemeden önceki hâline döndürür.

## Betikten kullanım

Betikten çağrıldığında komut hiçbir şey sormaz; hiçbir şey verilmemişse ne
verileceğini söyleyerek reddeder. Günlüğe `nesneler` ve verilen her parametre
yazılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Seçimde ölçü yok; ÖLÇÜDÜZENLE yalnız ölçüleri düzenler.` | Seçilenlerin hiçbiri ölçü değil | Ölçüyü seçin ya da `nesneler=` verin |
| `Nesne bulunamadı veya silinmiş: N` | `nesneler` içinde olmayan bir kimlik var | Kimlikleri [`SEÇ`](select.md) ile denetleyin |
| `Ölçü N kilitli katmanda; düzenlenemez.` | Ölçünün katmanı kilitli | [`KATMAN`](layer.md) ile kilidi açın |
| `Tolerans pozitif yazılır; aşağı sapma tolerans_alt= ile verilir.` | Negatif tolerans verildi | Alt sapmayı `tolerans_alt=` ile pozitif yazın |
| `'grad' bir açı birimi; bu ölçü bir uzunluk yazar: cizim, mm, cm, m ya da km.` | Bir uzunluk ölçüsüne açı birimi verildi | Uzunluk birimlerinden birini yazın |
| `'m' bir uzunluk birimi; açı ölçüsü grad, derece ya da radyan yazar.` | Bir açı ölçüsüne uzunluk birimi verildi | `grad`, `derece` ya da `radyan` yazın |
| `Tanınmayan ölçü stili: '…'. Katalogdaki stiller: …` | `stil` katalogda yok | Listelenen stillerden birini yazın |
| `Değiştirilecek bir şey verilmedi: …` | Betikten hiçbir parametre verilmedi | Değiştireceğiniz alanı verin |

## İlgili

- [`ÖLÇÜ`](dimension.md) — ölçü çizmek; aynı önek, sonek, birim ve tolerans parametreleri
- [`ÖLÇÜYENİLE`](dimension_refresh.md) — ölçüleri başka bir pafta ölçeğine uyarlamak
- [`NESNEBİLGİ`](entity_info.md) — ölçülen ve yazılan değer yan yana
- [Ölçü nesnesi](../nesneler/olcu.md)
