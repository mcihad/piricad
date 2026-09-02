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
```

`nokta` verilmezse komut sizden ister ve o köşeden imlecinize bir kılavuz çizgi
uzatır. Nokta yazımı [`ÇİZGİ`](line.md) ile aynıdır: mutlak koordinat
(`485320,4310220`), göreli (`@50,30`) ve kutupsal (`@100<45`).

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Köşesi taşınacak nesnenin kimliği. [`SEÇ`](select.md)'in yazdığı kimliğin aynısı |
| `kose` | Taşınacak köşenin sırası. İlk köşe `1`'dir |
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

Nesneyi seçin; köşeleri küçük kare tutamaklarla işaretlenir. İmleç bir tutamağın
üzerine gelince tutamak vurgulanır. Basıp sürükleyin ve bırakın; sürüklerken
nesnenin alacağı yeni biçim kesikli çizgiyle gösterilir.

Sürüklerken yakalama açıksa köşe komşu nesnelerin köşelerine oturur ve yakalama
işareti nerede duracağını önceden gösterir — komşu parselin köşesine tam oturmak
için [`MOD`](mode.md) ile uç nokta yakalamasını açık tutun. Dik mod ve kutupsal
izleme, köşenin **eski yerinden** ölçer.

Tutamağa basıp kıpırdatmadan bırakırsanız hiçbir şey olmaz: bu bir tıklamadır,
taşıma değil, ve boşuna bir geri alma adımı üretmez.

Arayüzün ayrıcalığı yoktur: fareyle taşıdığınız köşe ile komut satırına yazdığınız
köşe aynı komuttur ve komut günlüğüne aynı satır olarak düşer.

### Betik

```json
{
  "komutlar": [
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

Betikten çağrıldığında komut hiçbir şey sormaz: `nesne`, `kose` ve `nokta` üçü de
verilmelidir. Eksik olan varsa komut bir açıklama yazar ve çizimi değiştirmez.

Bir betik içinde arka arkaya birden çok `KÖŞETAŞI` çağırabilirsiniz; her biri kendi
geri alma adımıdır. Hepsini tek adımda toplamak isterseniz betiği tek blok olarak
çalıştırın ([`BETİK`](script.md)).

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Düzenlenecek nesne belirtilmedi. Örnek: KÖŞETAŞI nesne=1 kose=2` | `nesne` verilmedi | Nesnenin kimliğini yazın; kimliği [`SEÇ`](select.md) gösterir |
| `Bir seferde tek nesne düzenlenir; N nesne verildi.` | `nesne` birden çok kimlik aldı | Her nesne için ayrı bir `KÖŞETAŞI` çağırın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar; doğru kimliği yazın |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik hiç var olmadı ya da nesne silindi | [`GERİAL`](undo.md) ile geri getirin veya doğru kimliği verin |
| `Köşe numarası belirtilmedi. İlk köşe 1'dir.` | `kose` verilmedi | Taşınacak köşenin sırasını yazın |
| `Tek bir köşe numarası beklenir; N değer verildi.` | `kose` birden çok değer aldı | Tek bir köşe numarası yazın |
| `Bu nesnenin N. köşesi yok; M köşesi var.` | Nesnede o sırada köşe yok | 1 ile M arasında bir numara verin |

Köşenin yeni yeri halkayı kendi üzerine katlarsa ya da bir deliği dış sınırın
dışına çıkarırsa geometri katmanı taşımayı reddeder ve sebebini yazar; bu durumda
köşe **hiç kıpırdamaz**, yarım uygulanmış bir taşıma olmaz.

## İlgili

- [`KÖŞEEKLE`](vertex_insert.md) — kenarın ortasına yeni köşe ekler
- [`SEÇ`](select.md) — nesne kimliklerini gösterir
- [`MOD`](mode.md) — yakalama modlarını açar ve kapatır
- [`GERİAL`](undo.md) · [`YİNELE`](redo.md)
