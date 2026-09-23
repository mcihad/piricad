# KENARTÜRÜ — Kenarı Yaya ya da Düze Çevirme

Yola bakan parsel sınırını kurba uyduran, bir bordür dönüşünü yaya çeviren ya da
yanlışlıkla yay çizilmiş bir kenarı düzleştiren herkes için; bu sayfayı
bitirdiğinizde bir kenarın türünü arayüzden, komut satırından ve betikten
değiştirmeyi bileceksiniz.

## Ne yapar

`KENARTÜRÜ`, bir nesnenin **tek bir kenarının** türünü değiştirir:

- `tur=yay` — düz kenar, iki ucu yerinde kalarak **gösterdiğiniz noktadan geçen bir
  yaya** döner.
- `tur=duz` — yay kenar, iki ucu yerinde kalarak **düz kenara** döner.

`tur` verilmezse kenar **öbür türüne** döner: düz kenar yay olur, yay düz olur.

Nesne **aynı nesne olarak kalır**: kimliği, katmanı, stili, öznitelikleri ve ona
bağlı yazılar (örneğin [`UZUNLUKYAZ`](uzunluk_yaz.md) ile yazılmış kenar
uzunlukları) değişmez ve yazılar yeni kenarı izler. Türü gerekirse değişir ve bu
söylenir: bir parselin kenarı yay olunca parsel **yaylı çoklu çizgi** olur, son yayı
düzleşince yeniden düz çoklu çizgi olur. Yay kenar gerçek bir yaydır — merkezi ve
yarıçapı kesindir, kenarlardan örülmüş bir yaklaşım değildir.

Tek halkalı çizgi ve alanda, yayda ve yaylı çoklu çizgide çalışır. Delikli bir alanın
kenar türü değişmez, çünkü yaylı çoklu çizgi tek halkalıdır; komut bunu söyler.

Kenarlar **1'den başlayarak** numaralanır: 1. kenar 1. köşeden 2. köşeye gider;
kapalı bir alanda son kenar son köşeden ilk köşeye dönen kapanış kenarıdır.

## Adlar

| Ad | Tür |
|---|---|
| `KENARTÜRÜ` | Türkçe, birincil |
| `KENARTURU` | ASCII karşılık |
| `EDGEKIND` | İngilizce karşılık |
| `KNT` | Kısaltma |
| `core.edge_kind` | Komut kimliği |

## Sözdizimi

```text
KENARTÜRÜ nesne=<kimlik> kenar=<sıra> tur=yay nokta=<n>
KENARTÜRÜ nesne=<kimlik> kenar=<sıra> tur=duz
KENARTÜRÜ yer=<n> [tur=yay|duz] [nokta=<n>]
```

`kenar` yerine `yer` verilebilir: o noktaya en yakın kenar alınır; `nesne` de
verilmemişse o noktanın altındaki nesnenin.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Kenarı değişecek nesnenin kimliği |
| `kenar` | Değişecek kenarın sırası. İlk kenar `1`'dir |
| `yer` | Kenarı gösteren nokta: `kenar` verilmezse en yakın kenar, `nesne` de verilmezse altındaki nesne |
| `tur` | `yay` ya da `duz`; verilmezse kenarın öbür türü |
| `nokta` | `tur=yay` için yayın geçeceği nokta. Verilmezse arayüz sorar |

## Örnekler

### Komut satırı

Yeni bir çizimde, bir parselin güney kenarını dışa doğru 3 m'lik bir yaya çevirmek:

```text
ALAN 0,0 20,0 20,10 0,10
KENARTÜRÜ nesne=1 kenar=1 tur=yay nokta=10,-3
```

```text
Kenar yaya çevrildi (yarıçap 18,167 m); nesne yaylı çoklu çizgi oldu, kimliği ve öznitelikleri korundu.
```

Aynı kenarı yeniden düzleştirmek:

```text
KENARTÜRÜ nesne=1 kenar=1 tur=duz
```

```text
Kenar düzleştirildi; yay kalmadığı için düz çoklu çizgi oldu.
```

### Arayüz

Sol araç sütununda **köşe ailesinin** düğmesini basılı tutun ya da sağ tıklayın ve
**Kenar Türü**'nü seçin; aynı araç **Değiştir → Kenar Türü** menüsündedir.

1. Türü değişecek kenara tıklayın. Nesne de bu tıklamayla seçilir.
2. Kenar düzse yaya döner: imleci götürün, kenar imleçten geçen yay olarak vurgulu
   çizilir ve yanında `yarıçap X m` yazar. Tıklayın ya da noktayı yazın.
3. Kenar yaysa ilk tıklamada düzleşir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.area",
      "args": { "noktalar": [[0, 0], [20000, 0], [20000, 10000], [0, 10000]] } },
    { "cmd": "core.edge_kind",
      "args": { "nesne": [1], "kenar": 1, "tur": "yay", "nokta": [10000, -3000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** (`10000` = 10 m).

## Geri alma

`KENARTÜRÜ` tek bir geri alma adımıdır. [`GERİAL`](undo.md) kenarı ve nesnenin
türünü birlikte geri getirir: yaylı çoklu çizgiye dönmüş bir parsel yeniden düz çoklu
çizgi olur, aynı kimlikle.

## Betikten kullanım

Betikten çağrıldığında komut hiçbir şey sormaz: `nesne`, `kenar` (ya da `yer`),
`tur` ve `tur=yay` için `nokta` verilmelidir. Günlüğe `yer` değil, bulunan `kenar`
ile çözülmüş `tur` yazılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Kenarı değişecek nesne belirtilmedi. …` | Betik ne `nesne` ne `yer` verdi | `nesne=` ya da `yer=` verin |
| `Orada kenarı değişecek bir çizgi yok. …` | Tıklanan yerde nesne yok | Bir kenarın üstüne tıklayın |
| `Bir seferde tek nesnenin kenarı değişir; N nesne verildi.` | `nesne` birden çok kimlik aldı | Her nesne için ayrı çağırın |
| `Delikli bir alanın kenar türü değiştirilemez: …` | Nesne delikli bir alan | Deliği ayrı bir nesne olarak çizin |
| `Nesne N'in kenarı yok: …` | Daire dışı eğri, ölçü, blok ya da yazı | Çizgi, alan, yay ya da yaylı çoklu çizgi seçin |
| `Bu nesnenin N. kenarı yok; M kenarı var.` | Olmayan kenar numarası | 1 ile M arasında bir numara verin |
| `Yayın geçeceği nokta kenarın doğrultusunda; …` | Nokta kenarın üstünde ya da uzantısında | Noktayı kenarın bir yanında gösterin |
| `Bu kenar zaten düz.` | `tur=duz` düz kenara verildi | Yay olan bir kenar seçin |
| `Tam bir çember düzleştirilemez: …` | Dairenin tek kenarı düzleştirilmek istendi | — |
| `'TAPU' katmanı kilitli; üzerindeki nesne düzenlenemez. …` | Nesne kilitli katmanda | `KATMAN ad=TAPU kilitli=hayır` |

## İlgili

- [`YUVARLA`](fillet.md) — köşeyi teğet bir yayla yuvarlar
- [`KÖŞESİL`](vertex_delete.md) · [`KÖŞEEKLE`](vertex_insert.md) · [`KÖŞETAŞI`](vertex_move.md)
- [`GERİAL`](undo.md)
