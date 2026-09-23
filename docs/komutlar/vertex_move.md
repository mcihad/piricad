# KÖŞETAŞI — Köşe Taşıma

Ölçüsü sonradan düzelen bir parsel köşesini, yanlış röperden geçmiş bir yol
kenarını ya da imar hattının kırık noktasını yerine oturtan herkes için; bu sayfayı
bitirdiğinizde bir nesnenin köşesini arayüzden, komut satırından ve betikten
taşımayı bileceksiniz.

## Ne yapar

`KÖŞETAŞI`, var olan bir nesnenin **tek bir köşesini** yeni bir yere taşır. Nesnenin
kendisi aynı nesne olarak kalır: kimliği, katmanı, stili, öznitelikleri ve varsa
yazısı değişmez. Değişen yalnız o köşenin koordinatıdır.

Bu ayrım önemlidir. Bir parseli silip yeniden çizmek **yeni bir nesne** üretir:
kimlik değişir, o kimliğe bağlı ada/parsel numarası ve bütün öznitelikler düşer.
`KÖŞETAŞI` bir düzeltmedir, yeniden çizim değil — bu yüzden komut günlüğünde de
düzeltme olarak görünür.

Taşınan köşe, açık bir çizginin ucu da olabilir kapalı bir alanın köşesi de. Alanda
kapanış kenarı köşeyle birlikte hareket eder; ayrıca bir şey yapmanız gerekmez.

Köşeler **1'den başlayarak** numaralanır ve numaralar nesnenin halkaları boyunca
sırayla ilerler: önce dış sınır, sonra varsa delikler. Dört köşeli bir parselin
köşeleri 1, 2, 3 ve 4'tür.

### Eğrilerin tutamakları

Köşesi olmayan nesnelerin de **tutamakları** vardır ve `KÖŞETAŞI` onları da taşır;
`kose` o zaman tutamağın sırasıdır. Bir tutamağı taşımak nesnenin türünü değiştirmez:
daire daire kalır, yalnız yarıçapı ya da yeri değişir.

| Nesne | Tutamaklar (sırayla) | Taşıyınca ne olur |
|---|---|---|
| Daire | 1 merkez · 2–5 doğu/kuzey/batı/güney çeyrek | merkez daireyi taşır; çeyrek yarıçapı kurar |
| Yay | 1 merkez · 2 başlangıç · 3 bitiş · 4 orta nokta | merkez yayı taşır; uç kendi yerine gider, **öbür uç yerinde kalır** ve yay yeni uçtan, orta noktadan ve öbür uçtan geçecek biçimde yeniden kurulur; orta nokta iki uç yerinde kalarak yayı yeni noktadan geçirir |
| Elips | 1 merkez · 2 birinci eksen ucu · 3 ikinci eksen ucu · 4–5 aynaları | eksen ucu ekseni çevirir ve uzatır; öteki eksen boyunu koruyarak dik kalır |
| Yaylı çoklu çizgi | köşeler · sonra her yayın orta noktası | köşe taşınınca ona değen yaylar şişkinliğini korur; yayın ortası yayı üç noktadan yeniden kurar, kirişin üstüne gelirse kenar düzleşir |
| Spline | kontrol noktaları | kontrol noktası yerine gider, eğri yeniden hesaplanır; dosyadan gelen uydurma noktaları artık eğriyi anlatmadığı için düşer |
| Tarama, lider | halka köşeleri | köşe yerine gider |
| Ölçü | tanım noktaları · son olarak yazı | tanım noktası taşınınca ölçü çizgisi, uzatma çizgileri ve yazı yeniden kurulur, rakam yeniden ölçülür; yazı tutamağı yalnız yazıyı kaydırır |
| Blok referansı | 1 ekleme noktası · 2 döndürme tutamağı | ekleme noktası referansı taşır; döndürme tutamağı referansı ekleme noktası çevresinde tutamağa doğru döndürür, ölçeği değişmez |
| Nokta | 1 nokta | nokta taşınır |

Yayın bir ucunu taşımak öbür ucu kıpırdatmaz: yaya bağlanan bir çizgi bağlı kalır.
Spline'ın tutamakları eğrinin üstünde değil, eğrinin çizildiği **kontrol
noktalarındadır**; spline seçiliyken kontrol noktaları kesikli ince bir çerçeveyle
birleştirilir. Blok referansının döndürme tutamağı, bloğun kendi yatay ekseni
üzerinde, çizdiği şeklin en uzak köşesi kadar dışarıdadır.

Bir eğrinin **arasına** köşe eklenemez: [`KÖŞEEKLE`](vertex_insert.md) yalnız çoklu
çizgi ve alan için çalışır.

## Adlar

| Ad | Tür |
|---|---|
| `KÖŞETAŞI` | Türkçe, birincil |
| `KOSETASI` | ASCII karşılık |
| `MOVEVERTEX` | İngilizce karşılık |
| `KT` | Kısaltma |
| `core.vertex_move` | Komut kimliği |

## Sözdizimi

```text
KÖŞETAŞI nesne=<kimlik> kose=<sıra>
KÖŞETAŞI nesne=<kimlik> kose=<sıra> nokta=<n>
KÖŞETAŞI yer=<n> nokta=<n>
```

`nokta` verilmezse komut sizden ister; imleç hareket ettikçe nesne, köşesi imleçte
olacak biçimde tuvalde vurgulu çizilir. `kose` yerine `yer` verilebilir: o noktaya
en yakın köşe alınır; `nesne` de verilmemişse o noktanın altındaki nesne. Nokta yazımı [`ÇİZGİ`](line.md) ile aynıdır: mutlak koordinat
(`485320,4310220`), göreli (`@50,30`) ve kutupsal (`@100<45`).

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Köşesi taşınacak nesnenin kimliği. [`SEÇ`](select.md)'in yazdığı kimliğin aynısı |
| `kose` | Taşınacak köşenin sırası. İlk köşe `1`'dir |
| `yer` | Köşeyi gösteren nokta: `kose` verilmezse en yakın köşe, `nesne` de verilmezse altındaki nesne. Günlüğe `yer` değil, bulunan `kose` yazılır |
| `nokta` | Köşenin yeni yeri. Verilmezse arayüz sorar |

Parametre adı `kose`, Türkçe harfsiz yazılır — bu programda bütün parametre adları
böyledir, çünkü komut satırına her klavyeden yazılabilmeleri gerekir.

## Örnekler

### Komut satırı

Önce hangi nesneyi düzelteceğinizi öğrenin:

```text
SEÇ
```

Sonra 1 numaralı nesnenin 2. köşesini taşıyın:

```text
KÖŞETAŞI nesne=1 kose=2 nokta=485360,4310200
```

Noktayı yazmadan bırakırsanız komut sorar ve tıklamanızı bekler:

```text
KÖŞETAŞI nesne=1 kose=2
```

### Arayüz

**Araçla.** Sol araç sütununda **köşe ailesinin** düğmesini basılı tutun ya da
sağ tıklayın ve **Köşe Taşı**'yı seçin (aynı araç **Değiştir → Köşe Taşı**
menüsündedir).

1. Taşınacak köşeye tıklayın. Nesne de bu tıklamayla seçilir; tek bir nesne
   seçiliyse onun en yakın köşesi alınır.
2. İmleci götürün: nesne, köşesi imleçte olacak biçimde vurgulu çizilir, iki
   komşu kenar imleci izler.
3. Yeni yere tıklayın ya da koordinatı yazın.

**Tutamakla.** Nesneyi seçin; köşeleri küçük kare tutamaklarla işaretlenir. Bir dairede merkez ve
dört çeyrek, bir yayda uçlar ve orta nokta, bir ölçüde tanım noktaları ve yazı
görünür; boyut ya da açı kuran tutamaklar (yarıçap, yay ortası, yazı, bloğun döndürme
tutamağı) kare değil **yuvarlak** çizilir. İmleç bir tutamağın üzerine gelince tutamak vurgulanır. Basıp sürükleyin ve
bırakın; sürüklerken nesnenin alacağı yeni biçim kesikli çizgiyle gösterilir — daire
sürüklenirken daire kalır, çünkü önizleme türün kendi çizimidir.

Sürüklerken yakalama açıksa köşe komşu nesnelerin köşelerine oturur ve yakalama
işareti nerede duracağını önceden gösterir — komşu parselin köşesine tam oturmak
için [`MOD`](mode.md) ile uç nokta yakalamasını açık tutun. Dik mod ve kutupsal
izleme, köşenin **eski yerinden** ölçer.

Tutamağa basıp kıpırdatmadan bırakırsanız hiçbir şey olmaz: bu bir tıklamadır,
taşıma değil, ve boşuna bir geri alma adımı üretmez.

Arayüzün ayrıcalığı yoktur: fareyle taşıdığınız köşe ile komut satırına yazdığınız
köşe aynı komuttur ve komut günlüğüne aynı satır olarak düşer.

### Betik

Betik önce 2. köşesi yanlış ölçülmüş bir parsel çizer, sonra o köşeyi doğru yerine
taşır:

```json
{
  "komutlar": [
    { "cmd": "core.area",
      "args": { "noktalar": [[485300000,4310200000],[485359500,4310200400],
                             [485360000,4310245000],[485300000,4310245000]] } },
    { "cmd": "core.vertex_move",
      "args": { "nesne": [1], "kose": 2, "nokta": [485360000, 4310200000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte
ham depolama birimi kullanılır (`485300000` = 485 300 m). Ayrıntısı
[Betik yazma](../betik/README.md) sayfasındadır.

## Geri alma

`KÖŞETAŞI` tek bir geri alma adımıdır. [`GERİAL`](undo.md) köşeyi tam olarak eski
koordinatına döndürür — yakınına değil, aynısına: program eski geometriyi
saklamaya devam eder ve geri alma onu yeniden hesaplamaz, yerine koyar.

[`YİNELE`](redo.md) taşımayı geri getirir.

## Betikten kullanım

Betikten çağrıldığında komut hiçbir şey sormaz: `nesne` ve `kose` (ya da ikisinin
yerine `yer`) ile `nokta` verilmelidir. Eksik olan varsa komut bir açıklama yazar ve çizimi değiştirmez.

Bir betik içinde arka arkaya birden çok `KÖŞETAŞI` çağırabilirsiniz; her biri kendi
geri alma adımıdır. Hepsini tek adımda toplamak isterseniz betiği tek blok olarak
çalıştırın ([`BETİK`](script.md)).

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Düzenlenecek nesne belirtilmedi. Örnek: KÖŞETAŞI nesne=1 kose=2` | Betik ne `nesne` ne `yer` verdi | Nesnenin kimliğini ya da köşeyi gösteren `yer=` noktasını yazın |
| `Orada köşesi taşınacak bir nesne yok. ...` | Tıklanan yerde nesne yok | Bir nesnenin köşesine tıklayın |
| `Bir seferde tek nesne düzenlenir; N nesne verildi.` | `nesne` birden çok kimlik aldı | Her nesne için ayrı bir `KÖŞETAŞI` çağırın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar; doğru kimliği yazın |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik hiç var olmadı ya da nesne silindi | [`GERİAL`](undo.md) ile geri getirin veya doğru kimliği verin |
| `Köşe numarası belirtilmedi. İlk köşe 1'dir.` | Betik ne `kose` ne `yer` verdi | Taşınacak köşenin sırasını ya da `yer=` noktasını yazın |
| `Tek bir köşe numarası beklenir; N değer verildi.` | `kose` birden çok değer aldı | Tek bir köşe numarası yazın |
| `Bu nesnenin N. köşesi yok; M köşesi var.` | Nesnede o sırada köşe yok | 1 ile M arasında bir numara verin |
| `Bu nesnenin N. tutamağı yok; M tutamağı var.` | Eğride o sırada tutamak yok | Yukarıdaki tabloya göre 1 ile M arasında bir numara verin |
| `Yarıçap sıfır: tutamak merkezin üstünde. …` | Dairenin çeyrek tutamağı merkeze bırakıldı | Merkezden uzak bir nokta verin |
| `Yayın iki ucu aynı noktaya düşüyor. …` | Yayın ucu öbür ucun üstüne bırakıldı | Öbür uçtan uzak bir nokta verin |
| `Üç nokta aynı doğru üzerinde; yay düzleşir. …` | Yayın ucu ya da ortası iki ucu birleştiren doğrunun üstüne bırakıldı | Noktayı o doğrunun dışına bırakın |
| `Döndürme tutamağı ekleme noktasının üstünde; …` | Bloğun döndürme tutamağı ekleme noktasına bırakıldı | Açıyı gösteren bir yer seçin |
| `Birinci eksen sıfır: …` / `İkinci eksen sıfır: …` | Elipsin ekseni sıfıra indi | Merkezden ya da birinci eksenden uzak bir nokta verin |
| `Ölçü bu noktayla kurulamıyor: …` | İki nokta çakıştı ya da açının tepesi kolun ucuna geldi | Noktayı başka yere bırakın |
| `Blok tanımındaki nesne doğrudan düzenlenemez …` | Nesne bir blok tanımının üyesi | Referansın ekleme noktasını taşıyın |

Köşenin yeni yeri halkayı kendi üzerine katlarsa ya da bir deliği dış sınırın
dışına çıkarırsa geometri katmanı taşımayı reddeder ve sebebini yazar; bu durumda
köşe **hiç kıpırdamaz**, yarım uygulanmış bir taşıma olmaz.

## İlgili

- [`KÖŞEEKLE`](vertex_insert.md) — kenarın ortasına yeni köşe ekler
- [`SEÇ`](select.md) — nesne kimliklerini gösterir
- [`MOD`](mode.md) — yakalama modlarını açar ve kapatır
- [`GERİAL`](undo.md) · [`YİNELE`](redo.md)
