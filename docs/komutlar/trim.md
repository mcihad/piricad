# BUDA — Parçayı Kesme Sınırlarına Kadar Budama

Bir çizginin, yayın ya da dairenin sınırları aşan parçasını atması gereken herkes
için; bu sayfayı bitirdiğinizde budamayı arayüzden, komut satırından ve betikten
yapmayı bileceksiniz.

## Ne yapar

`BUDA`, **tıkladığınız parçayı** atar. Parça, tıkladığınız yerin iki yanındaki ilk
kesme sınırlarının arasında kalan bölümdür:

- İki yanında da sınır varsa **ortadaki parça** gider ve nesne ikiye ayrılır. İlk
  parça nesnenin kendisi olarak kalır — kimliği, katmanı, stili ve öznitelikleri
  onunla gider; ikinci parça aynı katmanda, aynı stil ve özniteliklerle yeni bir
  nesne olur.
- Yalnız bir yanında sınır varsa o yandaki **uç** gider.
- **Daire** ancak iki yerinden kesilince budanır: tek kesim bir çemberi açar ama
  ondan bir şey çıkarmaz. İki kesimin arasındaki parça gider, kalan bölüm bir
  **yay** olur.

Kalan her şey kendi türünde kalır. Budanan yay, aynı merkezli ve aynı yarıçaplı bir
yaydır; bir yaya ya da daireye budanan çizgi **eğrinin üzerinde** biter, eğrinin
ekranda çizildiği kirişlerin üzerinde değil. Kesişimler kapalı biçimde hesaplanır ve
milimetreye bir kez yuvarlanır, bu yüzden aynı işlem her bilgisayarda aynı noktayı
verir.

`BUDA` açık çizgilerde (`ÇİZGİ`, `ÇOKLUÇİZGİ`), yaylarda ve dairelerde çalışır.
Kapalı bir alanın (`ALAN`) bir parçası budanmaz; alanı ikiye ayırmak
[`BÖL`](split.md)'ün işidir. Elips ve spline bugün budanmaz: kesişimleri yinelemeli
bir çözüm ister, ve o çözümü getirecek kütüphaneyle birlikte (TODOS C-01)
budanabilecekler.

### Kesme sınırları

Hangi nesnelerin keseceği şu sırayla belirlenir:

1. `sinir=` ile verilen nesneler;
2. bunlar yoksa komuttan önce **seçtiğiniz** nesneler;
3. o da yoksa **hızlı budama**: tıkladığınız nesnenin yakınındaki her görünür çizgi,
   yay ve daire. `hepsi=evet` bunu açıkça ister.

Bir nesne kendi kendini kesmez. Her şeyi seçip `BUDA`'ya bastığınızda her nesne,
ötekiler tarafından kesilir.

### Parçayı göstermenin üç yolu daha

- **Çitle** (`yontem=çit`): tek tek tıklamak yerine bir **çit** çizersiniz; çitin
  geçtiği her nesnenin, geçtiği her parçası gider. Çitin bütün işi Enter'a basmadan
  önce tuvalde çizilir, ve hepsi tek geri alma adımıdır. Sınırların kesmediği ya da
  budanamayan (alan, elips…) nesneler atlanır ve sayısı söylenir.
- **Tıklanan kalsın** (`tut=evet`): tıkladığınız parça **kalır**, iki yanındaki
  kesimlerin dışında kalan her şey gider — iki yolun arasındaki bölümü bırakıp iki
  ucu birden atmak tek tıklamadır.
- **Sınırları uzatarak** (`uzanti=evet`): nesneye yetişmeyen bir sınır, kendi
  yolunda — çizgi doğrultusunda, yay çemberi boyunca — uzatılmış sayılır ve nesneyi
  orada keser. Sınırın kendisi değişmez; önizleme uzatılan bölümü noktalı çizer.

Önizlemede sınırların nesneyi kestiği **her yer** işaretlenir: kesen bir sınır `×`
ile, yalnız değen (teğet) bir sınır küçük bir halkayla. Teğet de keser; beklemediyseniz
halka bunu gösterir. İmlecin yanındaki yazı atılacak uzunluğu ve kesişim sayısını
söyler.

## Adlar

| Ad | Tür |
|---|---|
| `BUDA` | Türkçe, birincil |
| `TRIM` | İngilizce karşılık |
| `BD` | Kısaltma |
| `core.trim` | Komut kimliği |

## Sözdizimi

```text
BUDA [sinir=<k> …] [hepsi=evet] [tut=evet] [uzanti=evet] [nesne=<k> …] nokta=<n> [<n> …]
BUDA [sinir=<k> …] [hepsi=evet] [tut=evet] [uzanti=evet] [yontem=çit] cit=<n> <n> [<n> …]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nokta` | Atılacak her parçanın üzerinde bir nokta; birden çok verilebilir, sırayla işlenir |
| `nesne` | Her noktanın budadığı nesne, aynı sırayla; verilmezse noktanın altındaki nesne |
| `sinir` | Kesme sınırları; verilmezse seçim, o da yoksa hızlı budama |
| `hepsi` | `evet`: tıklanan nesnenin yakınındaki her nesne sınırdır |
| `yontem` | `tıkla` (öntanımlı) ya da `çit`: parçalar çizilen bir çitle gösterilir |
| `cit` | Çitin köşeleri, en az iki; çitin geçtiği her parça budanır |
| `tut` | `evet`: gösterilen parça kalır, iki yanındaki kesimlerin dışında kalan gider |
| `uzanti` | `evet`: sınırlar kendi yolunda uzatılmış sayılır; yetişmeyen bir sınır da keser |

## Örnekler

### Komut satırı

İki yolu kesen bir çizginin ortasını atmak — `2` ve `3` yollar, `1` çizgi:

```text
BUDA sinir=2 sinir=3 nesne=1 nokta=50,0
```

```text
1 parça budandı.
```

Aynı iki yolu kesen iki çizginin daha ortası, hızlı budamayla tek satırda:

```text
BUDA hepsi=evet nokta=50,10 50,-10
```

```text
2 parça budandı.
```

Aynı iki yolun arasını bırakıp iki ucu atmak — tıklanan parça kalır:

```text
BUDA tut=evet sinir=2 sinir=3 nesne=1 nokta=50,0
```

```text
2 parça budandı.
```

İki yolu kesen iki çizginin ortasını tek çitle atmak (çit, iki yolun arasından
geçer):

```text
BUDA cit=50,-5 50,15
```

```text
2 parça budandı (2 nesnede).
```

Çizgiye yetişmeyen bir sınırla budamak — `2`, y = 5'ten yukarı gider:

```text
BUDA uzanti=evet sinir=2 nesne=1 nokta=80,0
```

```text
1 parça budandı.
```

### Arayüz

Sol araç kutusundaki **Buda** düğmesine basın ya da `BUDA` yazın. Komut satırı
`Atılacak parçaya tıklayın — Enter: bitir` der.

- Önceden nesne seçtiyseniz sınırlar onlardır ve vurgulu kalırlar.
- Seçim yoksa hızlı budama çalışır: imlecin altındaki nesneyi yakınındaki her şey
  keser.

İmleci bir nesnenin üzerinde gezdirin: **atılacak parça** kırmızı ve kesikli,
**kalacak bölüm** vurgu renginde çizilir. Gördüğünüz, tıklamanın yapacağının
kendisidir; tuval önizlemeyi komutun kullandığı hesapla çizer. Tıklayın: parça gider
ve komut bir sonrakini bekler. İstediğiniz kadar parçayı art arda tıklayın, bitince
**Enter**'a ya da sağ tuşa basın.

Tıklama bir **konum değil, bir seçimdir**. Nesne yakalama, ızgara ve dik mod
tıklamayı kaydırmaz; bir kesişimin yakınına tıklamak parçayı kesişimin tam üstüne
çekip belirsizleştirmez.

**Esc** komutu Enter gibi bitirir: o ana kadar budananlar budanmış kalır. Hiç
tıklamadan Esc'e basarsanız hiçbir şey olmaz.

Öteki yollar aynı düğmenin ailesindedir: **Buda**'yı basılı tutun (ya da sağ tıklayın,
ya da köşesindeki küçük üçgene tıklayın) ve kartta seçin — **Buda — çitle**, **Buda —
tıklanan kalsın**, **Buda — sınırları uzatarak**. Hepsi **Değiştir** menüsünde de var.

- **Buda — çitle:** çitin köşelerini tıklayın; her köşeden sonra çitin şimdiye kadar
  alacağı bütün parçalar kırmızı ve kesikli görünür, imlecin yanında kaç parça
  budanacağı yazar. **Enter** uygular.
- **Buda — tıklanan kalsın:** komut satırı `Kalacak parçaya tıklayın` der; imlecin
  altındaki parça vurgulu kalır, dışındaki uçlar kırmızı görünür.
- **Buda — sınırları uzatarak:** yetişmeyen bir sınırın uzatılan bölümü noktalı
  çizilir; kesim onun ucunda işaretlenir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.line",
      "args": { "noktalar": [[485300000, 4310200000], [485400000, 4310200000]] } },
    { "cmd": "core.line",
      "args": { "noktalar": [[485330000, 4310190000], [485330000, 4310210000]] } },
    { "cmd": "core.line",
      "args": { "noktalar": [[485370000, 4310190000], [485370000, 4310210000]] } },
    { "cmd": "core.trim",
      "args": { "sinir": [2, 3], "nesne": [1], "nokta": [[485350000, 4310200000]] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

Bir `BUDA` çalışmasının bütün tıklamaları — ya da çitin aldığı bütün parçalar —
**tek** geri alma adımıdır; [`GERİAL`](undo.md) atılan parçaların hepsini birden geri
getirir. Esc ile bitirilen çalışma da tek adımdır.

## Betikten kullanım

Betikten `nokta` verilmelidir; betiğin tıklayacak bir eli yoktur. `nesne`
verilmezse her nokta **tam altındaki** nesneyi budar: betiğin ekranı, dolayısıyla
seçim açıklığı yoktur ve nokta nesnenin üzerinde olmalıdır. Güvenli yol `nesne`'yi
vermektir.

Günlüğe sınırlar (ya da `hepsi`), budanan nesneler ve noktalar yazılır; yeniden
oynatılan satır aynı parçaları atar. Çitle yapılan budamada nesneler değil **çit**
yazılır (`yontem=çit`, `cit`): oynatılan satır çiti aynı çizime yeniden çizer ve
aynı parçaları alır. `hepsi` ile yazılan bir satır, oynatıldığı
çizimdeki yakın nesneleri yeniden okur — kaydedildiği çizimde yaptığını yapar.
Python'dan `cad.trim(boundary=[2, 3], object=[1], point=[[485350000, 4310200000]])`
olarak çağrılır; çitle `cad.trim(fence=[[…], […]])`, tutarak `keep=True`, sınırları
uzatarak `carry_edges=True`.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bu nesneyi sınırlardan hiçbiri kesmiyor; atılacak bir parça yok.` | Sınırlar nesneyi kesmiyor | Kesen bir sınır verin ya da [`UZAT`](extend.md) ile sınıra ulaştırın |
| `Kapalı bir şekil ancak iki yerinden kesilince budanır; sınırlar onu N yerinden kesiyor.` | Daire tek yerden kesiliyor ya da hiç kesilmiyor | Daireyi iki yerden kesen sınırlar verin |
| `Tıklanan parça bütün nesne; budanınca geriye bir şey kalmıyor. Silmek için SİL kullanın.` | Kesişim nesnenin tam ucunda, tıklanan parça nesnenin tamamı | Silmek istiyorsanız [`SİL`](erase.md) |
| `Tıklanan nokta bir kesişimin tam üstünde; hangi parçanın atılacağını söylemek için parçanın içine tıklayın.` | Verilen nokta kesişimin kendisi | Parçanın içinden bir nokta verin |
| `Tıklanan yerde budanacak bir nesne yok. Bir çizginin, yayın ya da dairenin atılacak parçasına tıklayın.` | Tıklamanın altında nesne yok; betikte nokta nesnenin tam üstünde değil | Nesnenin üstüne tıklayın ya da `nesne=` verin |
| `Nesne N kapalı bir alan; alanın bir parçası budanmaz. Alanı ikiye ayırmak için BÖL kullanın.` | Kapalı bir alan tıklandı | [`BÖL`](split.md) kullanın |
| `Nesne N bu komutun işleyebileceği bir tür değil; BUDA çizgi, yay ve dairelerde çalışır.` | Elips, spline, nokta, metin ya da delikli alan | Çizgi, yay ya da daire seçin |
| `BUDA için kesecek sınır yok: tıklanan nesnenin yakınında başka bir çizgi, yay ya da daire yok.` | Hızlı budamada yakında kesecek nesne yok | Bir sınır çizin ya da `sinir=` verin |
| `BUDA için kesecek sınır yok: sınır olarak verilen nesneler tıklanan nesnenin kendisi ya da çizgi, yay veya daire değil.` | Seçili ya da verilen tek sınır, budanan nesnenin kendisi | Başka bir nesneyi sınır seçin |
| `BUDA: hiçbir parça gösterilmedi. Atılacak parçaya tıklayın ya da BUDA nokta=<nokta> yazın.` | Hiç tıklamadan Enter; betikte `nokta` yok | Bir parçaya tıklayın ya da `nokta` verin |
| `Tıklanan parçanın dışında atılacak bir şey yok.` | `tut=evet` ile tıklanan parça nesnenin tamamı | Tutmak yerine budayın ya da nesneyi kesen bir sınır ekleyin |
| `BUDA: çit en az iki noktadan oluşur. Çitin köşelerini tıklayın ya da BUDA cit=<nokta> <nokta> yazın.` | Çitin tek köşesi var | Çite en az bir köşe daha verin |
| `Çit, sınırların kestiği bir parçadan geçmiyor.` | Çit hiçbir nesneden geçmiyor ya da geçtiği nesneleri hiçbir sınır kesmiyor | Çiti atılacak parçaların üzerinden çizin |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | `nesne` ile verilen kimlik yok | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Sınır nesnesi bulunamadı veya silinmiş: N` | `sinir` ile verilen kimlik yok | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `'K' katmanı kilitli; üzerindeki nesne düzenlenemez. Kilidi KATMAN ad=K kilitli=hayır ile açın.` | Nesne kilitli bir katmanda | Katmanın kilidini açın |

## İlgili

- [`UZAT`](extend.md) — kısaltmak yerine sınıra kadar uzatır
- [`KIR`](break.md) — iki nokta arasını sınır olmadan atar
- [`BÖL`](split.md) — bir kesme çizgisiyle ikiye ayırır
