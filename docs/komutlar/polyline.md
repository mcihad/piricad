# ÇOKLUÇİZGİ — Tek Nesne Olarak Çoklu Çizgi

Yol kenarı, dere hattı ya da imar hattı gibi çok kırıklı bir hattı **tek nesne**
olarak çizen herkes için; bu sayfayı bitirdiğinizde çoklu çizgiyi arayüzden,
komut satırından ve betikten çizmeyi bileceksiniz.

## Ne yapar

`ÇOKLUÇİZGİ`, verdiğiniz bütün noktalardan **tek bir çizgi nesnesi** üretir.

[`ÇİZGİ`](line.md) ile farkı budur ve önemlidir. `ÇİZGİ` her doğru parçasını
**ayrı nesne** olarak yazar; `ÇOKLUÇİZGİ` hepsini **bir** nesne yapar.

On dört parçadan oluşan bir yol kenarı on dört ayrı nesneyse tek seferde
seçilemez, tek stil verilemez, tek öznitelik satırı taşıyamaz ve dışa aktarmada
tek öznitelik olarak çıkmaz. GeoPackage'daki bir LineString çok köşeli **tek**
geometridir; totalstation çıktısı da öyle gelir.

İki komut da durur, çünkü iki niyet de gerçektir: birbiriyle ilgisiz çizgiler
çizmek ile bir hat çizmek farklı işlerdir.

Çoklu çizgi **açık**tır: ilk ve son nokta birleşmez, dolayısıyla bir alan
çevrelemez ve yüzölçümü yoktur. Kapalı bir yüzey için [`ALAN`](area.md), var olan
çizgileri kapatmak için [`ALANAÇEVİR`](to_area.md) kullanın.

## Adlar

| Ad | Tür |
|---|---|
| `ÇOKLUÇİZGİ` | Türkçe, birincil |
| `COKLUCIZGI` | ASCII karşılık |
| `POLYLINE` | İngilizce karşılık |
| `ÇÇ`, `PL` | Kısaltma |
| `core.polyline` | Komut kimliği |

## Sözdizimi

```text
ÇOKLUÇİZGİ
ÇOKLUÇİZGİ <n1> <n2> <n3> …
ÇOKLUÇİZGİ noktalar=<n1> noktalar=<n2> …
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Çoklu çizginin köşe noktaları. En az iki nokta; hepsi tek nesne olur |

## Örnekler

### Komut satırı

```text
ÇOKLUÇİZGİ 485300,4310200 485360,4310200 485360,4310245 485400,4310245
```

```text
4 noktalı tek çoklu çizgi çizildi.
```

### Arayüz

Sol paletteki **çoklu çizgi** aracına basın ya da `ÇOKLUÇİZGİ` yazın. Köşeleri
sırayla tıklayın, bitirmek için **Esc**.

Çizerken o ana kadar verdiğiniz **bütün** köşeler kesikli kılavuzla birbirine
bağlanır, yani hattın alacağı biçimi tıklarken görürsünüz.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.polyline",
      "args": { "noktalar": [[485300000, 4310200000],
                             [485360000, 4310200000],
                             [485360000, 4310245000]] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`ÇOKLUÇİZGİ` tek bir geri alma adımıdır; kaç köşesi olursa olsun tek
[`GERİAL`](undo.md) hattın tamamını kaldırır. `ÇİZGİ` de tek adımdır ama
ürettiği bütün parçaları birden kaldırır.

## Betikten kullanım

Betikten çağrıldığında `noktalar` verilmelidir; en az iki nokta gerekir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `'noktalar' en az 2 değer ister, N geldi` | Tek nokta verildi | En az iki nokta verin |
| `'<katman>' katmanı kilitli.` | Etkin katman kilitli | [`KATMAN`](layer.md) ile kilidi açın |

## İlgili

- [`ÇİZGİ`](line.md) — her parçayı ayrı nesne yapar
- [`ALAN`](area.md) — kapalı yüzey çizer
- [`ALANAÇEVİR`](to_area.md) — var olan çizgileri kapalı alana çevirir
- [`KÖŞETAŞI`](vertex_move.md) · [`KÖŞEEKLE`](vertex_insert.md) — köşelerini düzeltir
