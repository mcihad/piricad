# KÖŞEEKLE — Kenara Köşe Ekleme

Düz çizilmiş bir sınıra sonradan kırık nokta eklemesi gereken herkes için — yola
verilen bir çıkıntı, dereye göre kırılan bir parsel kenarı, imar hattının ölçüden
sonra çıkan bir dönüşü; bu sayfayı bitirdiğinizde bir kenarın ortasına arayüzden,
komut satırından ve betikten köşe eklemeyi bileceksiniz.

## Ne yapar

`KÖŞEEKLE`, var olan bir nesnenin **iki köşesi arasına yeni bir köşe** koyar. Bir
kenar böylece ikiye ayrılır ve nesne bir köşe daha kazanır.

Nesne aynı nesne olarak kalır: kimliği, katmanı, stili ve öznitelikleri değişmez.
Bu, silip yeniden çizmekten farkıdır — bir parselin ada/parsel numarası kenarına
köşe eklendi diye düşmez.

Yeni köşenin **nereye** gireceğini `kose` söyler: verdiğiniz köşeden **sonraki**
kenarın ortasına girer. `kose=1` demek "1. köşe ile 2. köşe arasına" demektir.

Kapalı bir alanda son köşenin kenarı **kapanış kenarıdır** — son köşeden ilk köşeye
dönen kenar. Ona da köşe eklenebilir: dört köşeli bir parselde `kose=4`, 4. köşe ile
1. köşe arasına yeni bir köşe koyar. Kapanış kenarı diğerlerinden farksız bir
kenardır ve bir parselin ona da kırık nokta gerekmesi sık görülür.

Açık bir çizgide durum başkadır: son köşe bir **uçtur** ve ondan çıkan kenar yoktur,
çünkü açık bir çizginin ilk ve son noktası birleşmez. Son köşe verilirse komut
reddeder ve ne yapmanız gerektiğini yazar.

Köşeler 1'den başlayarak numaralanır ve numaralar nesnenin halkaları boyunca
sırayla ilerler: önce dış sınır, sonra varsa delikler.

## Adlar

| Ad | Tür |
|---|---|
| `KÖŞEEKLE` | Türkçe, birincil |
| `KOSEEKLE` | ASCII karşılık |
| `ADDVERTEX` | İngilizce karşılık |
| `KE` | Kısaltma |
| `core.vertex_insert` | Komut kimliği |

## Sözdizimi

```text
KÖŞEEKLE nesne=<kimlik> kose=<sıra>
KÖŞEEKLE nesne=<kimlik> kose=<sıra> nokta=<n>
```

`nokta` verilmezse komut sizden ister ve kenarın başladığı köşeden imlecinize bir
kılavuz çizgi uzatır. Nokta yazımı [`ÇİZGİ`](line.md) ile aynıdır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Köşe eklenecek nesnenin kimliği. [`SEÇ`](select.md)'in yazdığı kimliğin aynısı |
| `kose` | Yeni köşenin ardına geleceği köşe. İlk köşe `1`'dir |
| `nokta` | Yeni köşenin yeri. Verilmezse arayüz sorar |

Parametre adı `kose`, Türkçe harfsiz yazılır — bu programda bütün parametre adları
böyledir, çünkü komut satırına her klavyeden yazılabilmeleri gerekir.

## Örnekler

### Komut satırı

1 numaralı nesnenin 1. ve 2. köşesi arasına köşe ekleyin:

```text
KÖŞEEKLE nesne=1 kose=1 nokta=485330,4310195
```

Dört köşeli bir parselin **kapanış kenarına** köşe ekleyin:

```text
KÖŞEEKLE nesne=1 kose=4 nokta=485295,4310222
```

Noktayı yazmadan bırakırsanız komut sorar ve tıklamanızı bekler:

```text
KÖŞEEKLE nesne=1 kose=1
```

### Arayüz

Nesneyi seçin; köşeleri küçük kare tutamaklarla işaretlenir. İmleci bir **kenarın**
üzerine götürdüğünüzde, kenarın üstünde yuvarlak bir işaret belirir: yeni köşenin
oluşacağı yer orasıdır. Kare tutamak var olan bir köşeyi taşır, yuvarlak işaret yeni
bir köşe yapar — ikisi bu yüzden farklı görünür.

Kenara basıp sürükleyin: bastığınız yerde yeni bir köşe oluşur ve imlecinizle
birlikte gelir, bıraktığınız yerde durur. Köşeye çok yakın bastığınızda köşe
kazanır, yani yanlışlıkla var olan bir köşenin dibine ikinci bir köşe koymazsınız.

Sürüklerken yakalama açıksa yeni köşe komşu nesnelere oturur —
[`MOD`](mode.md) ile hangi yakalamaların açık olduğunu ayarlayabilirsiniz.

Arayüzün ayrıcalığı yoktur: fareyle eklediğiniz köşe ile komut satırına yazdığınız
köşe aynı komuttur ve komut günlüğüne aynı satır olarak düşer.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.vertex_insert",
      "args": { "nesne": [1], "kose": 1, "nokta": [485330000, 4310195000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte
ham depolama birimi kullanılır (`485300000` = 485 300 m). Ayrıntısı
[Betik yazma](../betik/README.md) sayfasındadır.

## Geri alma

`KÖŞEEKLE` tek bir geri alma adımıdır. [`GERİAL`](undo.md) eklenen köşeyi kaldırır
ve kenarı tam olarak eski haline döndürür. [`YİNELE`](redo.md) köşeyi geri getirir.

## Betikten kullanım

Betikten çağrıldığında komut hiçbir şey sormaz: `nesne`, `kose` ve `nokta` üçü de
verilmelidir. Eksik olan varsa komut bir açıklama yazar ve çizimi değiştirmez.

Arka arkaya köşe eklerken **numaraların kaydığına** dikkat edin: bir köşe eklendikten
sonra ondan sonraki bütün köşelerin sırası bir artar. Aynı kenara iki köşe eklemek
için ikinci çağrıda `kose` değerini bir artırın.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Düzenlenecek nesne belirtilmedi. Örnek: KÖŞETAŞI nesne=1 kose=2` | `nesne` verilmedi | Nesnenin kimliğini yazın; kimliği [`SEÇ`](select.md) gösterir |
| `Bir seferde tek nesne düzenlenir; N nesne verildi.` | `nesne` birden çok kimlik aldı | Her nesne için ayrı bir `KÖŞEEKLE` çağırın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar; doğru kimliği yazın |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik hiç var olmadı ya da nesne silindi | [`GERİAL`](undo.md) ile geri getirin veya doğru kimliği verin |
| `Köşe numarası belirtilmedi. İlk köşe 1'dir.` | `kose` verilmedi | Kenarın başladığı köşenin sırasını yazın |
| `Tek bir köşe numarası beklenir; N değer verildi.` | `kose` birden çok değer aldı | Tek bir köşe numarası yazın |
| `Bu nesnenin N. köşesi yok; M köşesi var.` | Nesnede o sırada köşe yok | 1 ile M arasında bir numara verin |
| `Son köşeden sonra kenar yok: açık bir çizgide N. köşe uçtur. Araya köşe eklemek için ondan önceki bir köşe verin.` | Açık bir çizginin son köşesi verildi | Bir önceki köşeyi verin; çizgiyi uzatmak istiyorsanız [`ÇİZGİ`](line.md) kullanın |

Yeni köşe halkayı kendi üzerine katlarsa ya da bir deliği dış sınırın dışına
çıkarırsa geometri katmanı eklemeyi reddeder ve sebebini yazar; bu durumda nesne
**hiç değişmez**, yarım uygulanmış bir ekleme olmaz.

## İlgili

- [`KÖŞETAŞI`](vertex_move.md) — var olan bir köşeyi taşır
- [`SEÇ`](select.md) — nesne kimliklerini gösterir
- [`MOD`](mode.md) — yakalama modlarını açar ve kapatır
- [`GERİAL`](undo.md) · [`YİNELE`](redo.md)
