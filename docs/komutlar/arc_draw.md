# YAY — Yay Çizme

Yol kurbunu, kavşak dönüşünü, dere kıvrımını ya da bir kadastro sınırının yay
şeklindeki kenarını çizen herkes için; bu sayfayı bitirdiğinizde yayı arayüzden,
komut satırından ve betikten çizmeyi bileceksiniz.

## Ne yapar

`YAY`, bir **merkez**, bir **başlangıç noktası** ve bir **bitiş noktası** alır ve
yay çizer. Yarıçapı merkez ile başlangıç noktası arasındaki uzaklık belirler.

> **Faz 0 durumu.** Yay çizilir, seçilir, kaydedilir ve geri alınır. Yakalama
> henüz yayın merkezine, uçlarına ve orta noktasına **ayrıca oturmaz**; genel
> yakalama kuralları geçerlidir. Yayın uzunluğu ve kiriş hesabı Faz 1'de gelecek.

### Süpürme her zaman saat yönünün tersine

Yay, başlangıç noktasından bitiş noktasına **saat yönünün tersine** süpürür. Bu
tek kural, yön denetiminin tamamıdır:

- Küçük yayı istiyorsanız uçları o sırayla verin.
- **Büyük yayı** istiyorsanız aynı iki ucu **ters sırayla** verin.

"Büyük yay" diye bir seçenek yoktur ve olmamalıdır: olsaydı aynı resmi anlatan iki
farklı kayıt olurdu, ve iki kayıt aynı şeyi anlatınca hangisinin doğru olduğu
sorusu ortaya çıkar. Aynı iki uç, ters sırayla, zaten dairenin öteki yayıdır.

Bitiş noktasının yarıçap üzerinde olması gerekmez; yalnız **yönü** okunur. Yay her
zaman başlangıç noktasının belirlediği yarıçapta çizilir.

### Yay da tanımıyla saklanır

[`DAİRE`](circle_draw.md) gibi yay da **tanımıyla** durur: merkez, tam yarıçap ve
ölçülen iki uç. Ekranda görünen çok kenarlı çizgi yalnız resimdir.

Yay hiçbir şeyi çevrelemez, dolayısıyla **alanı sıfırdır** — açık bir çizginin
alanının sıfır olmasıyla aynı sebeple. Kapalı bir yüzey istiyorsanız
[`ALAN`](area.md) ya da [`DAİRE`](circle_draw.md) kullanın.

Yayın **köşesi yoktur**: [`KÖŞETAŞI`](vertex_move.md), [`KÖŞEEKLE`](vertex_insert.md)
ve [`ALANAÇEVİR`](to_area.md) yayı reddeder.

## Adlar

| Ad | Tür |
|---|---|
| `YAY` | Türkçe, birincil |
| `ARC` | İngilizce karşılık |
| `YY` | Kısaltma |
| `core.arc_draw` | Komut kimliği |

## Dört yöntem

| `yontem` | Ne ister | Nerede kullanılır |
|---|---|---|
| `merkez` (varsayılan) | Merkez + iki uç | Günlük olan |
| `3n` | Yayın üzerindeki üç nokta | Ölçülmüş bir kavisin geri kurulması |
| `bma` | Başlangıç + merkez + **süpürme açısı** | Bir yol kurbunun plandaki hâli |
| `bby` | Başlangıç + bitiş + **yarıçap** + yön | Bir pah, bir birleşim kavsi |
| `devam` | Son çizilen çizginin ya da yayın ucundan **teğet** devam eder; yalnız bitiş noktası istenir |

**Süpürmenin işareti oturumun kuralındandır.** Varsayılan *semt* kuralında artı
bir süpürme **saat yönünde**dir — bir mühendisin bir süpürmeden kastettiği şey —
*matematik* kuralında ise saat yönünün tersine. Eksi bir süpürme öbür yöne döner
ve bir tura katlanmaz: −100 grad, 300 grad değildir.

`bby`'nin **iki** çözümü vardır: yarıçap iki noktayı iki yaydan biriyle
birleştirir, biri kirişin bir yanına biri öbür yanına kavis yapar. `yon=sol|sag`
hangisi olduğunu söyler. Yazıyla vermezseniz komut **sorar**: yayın hangi yandan
geçeceğini gösterirsiniz ve yay imlecin tarafına doğru kavislenerek önizlenir.
Yarıçap iki nokta arasının yarısından küçükse hiçbirini birleştirmez ve komut
bunu **yarıçapı yazdığınız anda** söyler — olmayan bir yayın hangi yanı
sorulmaz.

### Kılavuz: her yöntemde yayın kendisi

| `yontem` | Kılavuz neyi gösterir |
|---|---|
| `merkez` | İkinci tıklamaya kadar bütün çember (seçtiğiniz şey yarıçap), sonra yayın kendisi |
| `3n` | İlk iki nokta çizgiyle, üçüncüsünde üç noktadan geçen yay |
| `devam` | Son çizilenin ucundan **teğet** ayrılan yay — kırık olup olmadığını tıklamadan görürsünüz |
| `bby` | Yazılan yarıçapta, imlecin tarafına kavislenen yay |
| `bma` | Süpürme bir sayıdır ve yazılır; fareyle verilen hâli `merkez` yöntemidir |

Kılavuz, yayı kuracak olan fonksiyonun kendisinden gelir
(`core::arc_from_guide`), yani gördüğünüz yay oluşacak yaydır. `3n` ve `devam`
eskiden düz bir **çizgi** gösteriyordu — bir kavisin olmadığı tek şey.

## Sözdizimi

```text
YAY <merkez> <baslangic> <bitis>
YAY yontem=3n baslangic=<n> uzerinden=<n> bitis=<n>
YAY merkez=<n> baslangic=<n> yontem=bma supurme=<açı>
YAY yontem=bby baslangic=<n> bitis=<n> yaricap=<m> [yon=sol|sag]
YAY yontem=devam bitis=<n>
```

Nokta yazımı [`ÇİZGİ`](line.md) ile aynıdır: mutlak koordinat (`485320,4310220`),
göreli (`@50,30`) ve kutupsal (`@100<45`).

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `merkez` | Yayın merkezi |
| `baslangic` | Yayın başladığı nokta. Merkezle arasındaki uzaklık yarıçaptır |
| `bitis` | Yayın biteceği yön. Yalnız yönü okunur, uzaklığı değil |
| `yon` | `bby`: yayın hangi yandan geçeceği — `sol` ya da `sag` |
| `yon_nokta` | `bby`: yanın gösterildiği nokta; `yon` verilmişse sorulmaz |

## Örnekler

### Komut satırı

Doğudan kuzeye çeyrek yay — yarıçap 30 metre:

```text
YAY merkez=485300,4310200 baslangic=485330,4310200 bitis=485300,4310230
```

Aynı iki uç ters sırayla: dairenin öteki yayı, yani 270 derecelik olan:

```text
YAY merkez=485300,4310200 baslangic=485300,4310230 bitis=485330,4310200
```

Başlangıç kutupsal koordinatla — merkezden 50 metre kuzeye (`@50<0`: semt açısı sıfır,
kuzey) — bitiş yönü mutlak koordinatla, merkezin tam doğusu: yarıçapı tam 50 metre olan
100 gradlık (90°) kurp:

```text
YAY 485300,4310200 @50<0 485350,4310200
```

### Arayüz

Sol paletteki **yay** aracına basın ya da komut satırına `YAY` yazın. Üç tıklama:
merkez, başlangıç, bitiş.

İkinci tıklamaya kadar kılavuz **bütün çemberi** gösterir — çünkü o anda
seçtiğiniz şey yarıçaptır, ve bir yarıçap bir çemberdir. Başlangıç noktasını
tıkladıktan sonra kılavuz **yayın kendisine** döner ve imleciniz döndükçe süpürme
büyür: bırakacağınız yay tam olarak gördüğünüz yaydır.

Yakalama açıkken üç nokta da mevcut nesnelere oturur ([`MOD`](mode.md)).

Öteki dört yöntem aynı düğmenin **kartında**: düğmeyi basılı tutun ya da
köşesindeki işarete tıklayın. `bby`'de yarıçapı yazarken iki uç arasındaki kiriş
ekranda kalır; yarıçapı yazdıktan sonra komut yayın hangi yandan geçeceğini sorar,
ve imleci kirişin bir yanından öbürüne geçirdikçe yay taraf değiştirir. Aynı
karttaki **Daire Dilimi** ayrı bir şekildir ve artık kendi ikonunu taşır.

`bma`'da merkezi ve başlangıcı verdikten sonra komut **süpürme açısını** ister
(`Süpürme açısı — yazın ya da gösterin`). Açıyı yazabilir ya da **gösterebilirsiniz**:
yay, başlangıçtan imlecinizin doğrultusuna kadar oturumun açı yönünde — öntanımlı
`semt`'te saat yönünde — süpürülür ve süpürme imlecin yanında oturumun biriminde
yazar. Tıkladığınızda o açı, yazmışsınız gibi kaydedilir. Doğudaki bir noktaya
kuzeyden bakan bir başlangıçla tıklamak `100 grad` demektir.

Araç kalıcıdır: bir yayı bitirdiğinizde `YAY` **aynı yöntemle** yeniden kurulur;
komut satırına `YAY yontem=bby yon=sag` yazdıysanız bir sonraki yay da öyledir.
Aracı bırakmak için **Esc**'e basın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.arc_draw",
      "args": { "merkez": [485300000, 4310200000],
                "baslangic": [485330000, 4310200000],
                "bitis": [485300000, 4310230000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Teğet devam (`yontem=devam`)

Bir yol geçiş eğrisi, bir bordür dönüşü ve zincirleme pahlar hep böyle çizilir:
yay, **en son çizilen** çizginin ya da yayın ucundan, o ucun **kendi
doğrultusunda** ayrılır — böylece birleşme yerinde kırık olmaz. Bordürde bir
kırık, yeniden dökülecek bir bordür demektir.

```text
ÇOKLUÇİZGİ 0,0 100,0
YAY yontem=devam bitis=150,50
```

Yalnız **bitiş noktası** istenir; başlangıç ve teğet çizimden okunur. Merkez,
başlangıçtaki dike ile kirişin orta dikmesinin kesiştiği yerdir.

**Kaynak çizimin kendisidir**, hatırlanan bir oturum alanı değil: `SEÇ SON` da
"en son oluşturulan nesne"yi belgeden okur, ve doğrultuyu aynı yerden okumak aynı
soruyu aynı yere sormaktır. Geri alma, replay ve yeniden yükleme ile ayrı tutulacak
bir durum parçası daha olmuyor.

Bir yayın ucunda teğet, uç yarıçapına diktir; modelin yayı saat yönünün tersine
sakladığı için (`core/arc.hpp`) uçtaki hareket, yarıçapın aynı yöne çeyrek tur
döndürülmüş hâlidir.

**Bitiş noktası teğetin üzerindeyse** oradan devam eden şey bir yay değil bir
doğrudur, ve komut bunu söyler — sıfıra bölmez. [`ÇİZGİ`](line.md) kullanın ya da
yanda bir nokta seçin.

Günlüğe **çözülmüş yay** yazılır (merkez, başlangıç, bitiş): bir replay BU yayı
kurmalıdır, oynatıldığı belgede en yeni nesne ne olursa olsun.

## Geri alma

`YAY` tek bir geri alma adımıdır. [`GERİAL`](undo.md) yayı kaldırır,
[`YİNELE`](redo.md) geri getirir.

## Betikten kullanım

Betikten çağrıldığında `merkez`, `baslangic` ve `bitis` üçü de verilmelidir; komut
hiçbir şey sormaz.

Komut günlüğüne **üç nokta** yazılır, yarıçap ve açı değil: yarıçap merkez ile
başlangıcın *anlamıdır*, kaydedilen ise çalıştırmanın kendisidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Başlangıç noktası merkezle aynı yerde; yarıçap sıfır olamaz.` | Merkez ile başlangıç çakışık | Başlangıcı merkezden uzağa verin |
| `Bitiş noktası merkezle aynı yerde; yayın nereye kadar gideceği belirsiz.` | Bitiş merkezle çakışık; yön okunamıyor | Bitişi merkezden uzağa verin |
| `Yay yarıçapı sıfırdan büyük olmalı: N` | Yarıçap sıfır ya da negatif geldi | Geçerli bir başlangıç noktası verin |
| `'<katman>' katmanı kilitli.` | Etkin katman kilitli | [`KATMAN`](layer.md) ile kilidi açın |
| `Nesne N bir eğri; eğrinin köşesi yoktur. ...` | Yaya [`KÖŞETAŞI`](vertex_move.md) uygulandı | Yayı silip yeniden çizin |
| `Nesne N bir eğri; eğri çizgi gibi birleştirilemez.` | Yaya [`ALANAÇEVİR`](to_area.md) uygulandı | Yayı seçimden çıkarın |

## İlgili

- [`DAİRE`](circle_draw.md) — tam daire çizer
- [`ÇİZGİ`](line.md) · [`ALAN`](area.md) · [`DİKDÖRTGEN`](rectangle.md)
- [`MOD`](mode.md) — yakalama modları
- [`SİL`](erase.md) · [`GERİAL`](undo.md)
