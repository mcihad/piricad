# BÖL — Nesneyi Parçalara Bölme

Bir yol kenarını kavşak noktasından, bir sınırı bir röperden, bir yayı eşit
parçalara ayırması gereken herkes için; bu sayfayı bitirdiğinizde bölmeyi
arayüzden, komut satırından ve betikten yapmayı bileceksiniz.

## Ne yapar

`BÖL`, seçtiğiniz nesneleri parçalara ayırır ve **bütün parçaları tutar** —
parça çıkaran komut [`KIR`](break.md)'dır. Nereden bölüneceğini beş yoldan
söylersiniz:

| Yöntem | Nereden böler |
|---|---|
| `cizgi` (öntanımlı) | Çizdiğiniz bir **kesme çizgisinin** geçtiği her yerden |
| `nokta` | Nesnenin üstüne tıkladığınız **noktalardan** |
| `kesisim` | Seçtiğiniz nesnelerin **birbirini kestiği** her yerden |
| `mesafe` | Nesnenin başından verdiğiniz **uzaklıktan** |
| `esit` | Verdiğiniz sayıda **eşit parçaya** |

`BÖL` çizgide, açık çoklu çizgide, **yayda**, **dairede** ve **yaylı çoklu
çizgide** çalışır. Her parça kendi türünde kalır: bir yaydan kesilen parçalar
yaydır, bir daire iki yaya ayrılır, yaylı bir sınırın yayları düzleşmez — parçalar
aynı çemberin yaylarıdır. Parçaların uzunlukları toplamı, kesim başına en çok bir
milimetrelik yuvarlamayla, kaynağın uzunluğudur.

**Her parça kaynağının katmanını, stilini ve bütün özniteliklerini taşır**: ikiye
bölünen bir yol iki yanında da o yoldur. İlk parça nesnenin kendisi olarak kalır —
kimliği onda durur — öteki parçalar yeni nesnelerdir. Bir daire bölününce bütün
parçalar yenidir, çünkü yay daireden başka bir türdür.

### Alanlar yalnız kesme çizgisiyle bölünür

Bir **alan** (kapalı alan, parsel) kesme çizgisinin iki yanına düşen iki alana
ayrılır ve iki parça da bütün öznitelik sütunlarını alır. Alanı kenarı boyunca
açmak onu iki çizgiye çevirirdi — bir parsel iki çizgi değildir — bu yüzden öteki
dört yöntem alanı söyleyerek reddeder.

Bir alanı bölmek için kesme çizgisinin alanı baştan başa geçmesi gerekir; bu
yüzden alan keserken çizdiğiniz parça iki yana sonsuza uzatılır. Bir çizgiyi ya
da eğriyi ise yalnız çizdiğiniz parçanın gerçekten kestiği yerlerden böler; yoksa
bir sınırın üstüne çektiğiniz kısa bir çizik, aynı doğrultudaki bütün seçili
çizgileri de keserdi.

### Kapalı şekiller

Bir daire ya da kapalı yaylı çoklu çizgi **tek noktadan bölünmez** — bir noktadan
açılan çember bir parça bile ayırmaz; en az iki nokta gerekir. `esit` kapalı
şekli başından (dairede doğudaki noktasından) başlayarak eşit yaylara böler.
`mesafe` kapalı şekilde çalışmaz, çünkü kapalı bir şeklin başı yoktur.

## Bu komut ifraz değildir

`BÖL` genel bir geometri işlemidir. [`İFRAZ`](split_parcel.md) aynı kesmenin
üstüne kadastro kurallarını koyar: alan raporu, ada/parsel numaraları ve mevzuat.
Parsel bölecekseniz `İFRAZ` kullanın.

## Adlar

| Ad | Tür |
|---|---|
| `BÖL` | Türkçe, birincil |
| `BOL` | ASCII karşılık |
| `SPLIT` | İngilizce karşılık |
| `BL` | Kısaltma |
| `core.split` | Komut kimliği |

## Sözdizimi

```text
BÖL [nesne=<k>…] noktalar=<n1> <n2>                  # kesme çizgisi
BÖL nesne=<k> yontem=nokta noktalar=<n> [<n> …]      # nesnenin üstündeki noktalar
BÖL nesne=<k> <k> … yontem=kesisim                   # birbirini kestikleri yerler
BÖL [nesne=<k>…] yontem=mesafe mesafe=<metre>
BÖL [nesne=<k>…] yontem=esit sayi=<parça>
BÖL nesne=<k> nokta=<n>                              # eski biçim: tek çizgi, tek nokta
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Kesilecek nesneler; verilmezse etkin seçim. `nokta` yönteminde tek nesne |
| `yontem` | `cizgi`, `nokta`, `kesisim`, `mesafe` ya da `esit`; verilmezse `cizgi` |
| `noktalar` | `cizgi`: kesme çizgisinin iki noktası · `nokta`: nesnenin üstündeki bölme noktaları |
| `mesafe` | `mesafe`: bölme noktasının nesnenin başından uzaklığı, metre |
| `sayi` | `esit`: kaç eşit parça, 2–10000 |
| `nokta` | **Eski biçim**: tek açık çizgiyi bu noktadan böler. `noktalar` verilirse yok sayılır |

## Örnekler

### Komut satırı

Bir çizgiyi kesme çizgisiyle bölmek:

```text
BÖL nesne=1 noktalar=485330,4310190 485330,4310260
```

```text
1 çizgi bölündü.
```

Çeyrek bir yayı (`1`, yarıçap 10 m) üç eşit yaya bölmek — parçalar yaydır ve
uzunlukları toplamı yayın kendisidir:

```text
BÖL nesne=1 yontem=esit sayi=3
```

```text
3 parça: 5,236 + 5,236 + 5,236 = 15,708 m.
```

Birbirini kesen üç çizgiyi (`1`, `2`, `3`) her kesişimden ayırmak:

```text
BÖL nesne=1 2 3 yontem=kesisim
```

```text
3 nesne bölündü, 7 parça çıktı.
```

20 metrelik bir çizgiyi başından 7 metrede bölmek:

```text
BÖL nesne=1 yontem=mesafe mesafe=7
```

```text
2 parça: 7,000 + 13,000 = 20,000 m.
```

Eski biçim de çalışmaya devam eder:

```text
BÖL nesne=1 nokta=485330,4310200
```

```text
Çizgi ikiye bölündü.
```

### Arayüz

Şeritte **Değiştir ▸ Kes ve Uzat ▸ Böl**'e basın, kesilecek nesneleri tıklayarak
seçin, **Enter**'a (ya da sağ tuşa) basın, sonra kesme çizgisinin iki ucunu
tıklayın. İkinci ucu ararken kesme çizgisi kılavuz olarak fareyi takip eder.
Yakalama açıkken uçlar mevcut köşelere ve kesişimlere oturur. Nesneleri önceden
seçtiyseniz doğrudan kesme çizgisine geçer.

Öteki yöntemler aynı düğmenin **okundaki** listededir:

- **Böl — noktalardan**: nesneyi seçin, sonra bölme noktalarını tıklayın. Nesne,
  verdiğiniz noktalardan ve imlecin durduğu yerden kesilmiş hâliyle, parçaları
  sırayla iki renkte çizilir; imlecin yanında `N parça · baştan X m` yazar. Yanlış
  bir noktayı **⌫** (ya da Ctrl+Z, `G`) geri alır. **Enter** böler.
- **Böl — kesişimlerden**: birbirini kesen nesneleri seçin, **Enter**.
- **Böl — eşit parçaya**: nesneleri seçin, parça sayısını yazın.
- **Böl — baştan uzaklıkla**: nesneleri seçin, uzaklığı metre olarak yazın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.arc_draw",
      "args": { "merkez": [0, 0], "baslangic": [10000, 0], "bitis": [0, 10000] } },
    { "cmd": "core.split",
      "args": { "nesne": [1], "yontem": "esit", "sayi": 3 } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`BÖL` tek bir geri alma adımıdır: [`GERİAL`](undo.md) bütün parçaları tekrar tek
nesne yapar, öznitelikleriyle birlikte.

## Betikten kullanım

Betikten `nesne` ile yöntemin istediği verilmelidir: `cizgi` için `noktalar` (iki
nokta), `nokta` için `noktalar`, `mesafe` için `mesafe`, `esit` için `sayi`.
Python'dan `cad.split(object=[1], method="esit", count=3)` olarak çağrılır.

**Hangi nesnenin neye dönüştüğü söylenir.** Komutun yapılandırılmış cevabı her
kaynak için bir kayıt taşır: `{"kaynak": 1, "sonuc": [1, 2, 3]}` — kaynağın
kimliği ve onu şimdi taşıyan nesnelerin kimlikleri, nesne boyunca sırayla. Bir
MCP istemcisi ya da betik, nesneleri kimlikle izliyorsa yeni parçaları buradan
bulur.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bölme noktası çizginin ucunda; bölünecek bir şey kalmıyor.` | Nokta uçta | Çizginin içinde bir nokta verin |
| `Kapalı bir şekil tek noktada bölünmez; en az iki bölme noktası verin.` | Daireye ya da kapalı şekle tek nokta verildi | İkinci bir nokta verin |
| `BÖL: bölme noktası verilmedi. Nesnenin üstünde bir noktaya tıklayın ya da BÖL yontem=nokta noktalar=<nokta> yazın.` | `nokta` yönteminde hiç nokta verilmeden Enter | Nesnenin üstüne tıklayın |
| `Nesne N bir alan; alan kenarı boyunca açılmaz. İkiye ayırmak için BÖL'ü kesme çizgisiyle kullanın (yontem=cizgi).` | `nokta`, `kesisim`, `mesafe` ya da `esit` bir alana verildi | Alanı kesme çizgisiyle bölün |
| `Nesne N bu yöntemle bölünemiyor; BÖL çizgi, yay, daire ve yaylı çoklu çizgide çalışır.` | Elips, spline, nokta, yazı ya da blok | Bölünebilir bir nesne seçin |
| `Nesne N bir eğri ya da nokta; BÖL çizgi, yay, daire, yaylı çoklu çizgi ve alanlarla çalışır.` | Kesme çizgisine elips, spline ya da nokta verildi | Bölünebilir bir nesne seçin |
| `Nesne N kapalı; başı olmayan bir şekil baştan uzaklıkla bölünmez. yontem=esit ya da yontem=nokta kullanın.` | `mesafe` kapalı bir şekle verildi | `esit` ya da `nokta` kullanın |
| `Nesne N X m uzunluğunda; bölme uzaklığı 0 ile X m arasında olmalı.` | Uzaklık nesnenin dışında | Nesnenin içinde bir uzaklık verin |
| `Parça sayısı 2 ile 10000 arasında olmalı; N verildi.` | `sayi` aralık dışında | 2–10000 arasında bir sayı verin |
| `Seçilen nesneler birbirini hiçbir yerde kesmiyor.` | `kesisim`, kesişmeyen nesnelerle | Birbirini kesen nesneler seçin |
| `Seçilen nesnelerin hiçbiri bölünemedi.` | `mesafe`/`esit` hiçbir nesneyi bölemedi | Nesneleri ve değeri denetleyin |
| `Kesme çizgisi seçilen nesnelerin hiçbirinden geçmiyor.` | Kesme çizgisi hiçbir nesneye değmiyor | Kesme çizgisini nesnelerin üstünden çizin |
| `Kesme çizgisinin iki ucu aynı nokta; bir doğrultu belirtmiyor.` | İki nokta aynı | Farklı iki nokta verin |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |

## İlgili

- [`KIR`](break.md) — iki nokta arasındaki parçayı çıkarır
- [`UÇUCA`](join.md) — uç uca değen parçaları birleştirir
- [`BUDA`](trim.md) · [`UZAT`](extend.md)
- [`BÖLÜMLE`](divide.md) — bölmeden, eşit aralıkla nokta ya da blok koyar
- [`ÇOKLUÇİZGİ`](polyline.md) · [`SEÇ`](select.md)
