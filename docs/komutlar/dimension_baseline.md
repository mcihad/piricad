# BAZÖLÇÜ — Baz Ölçü

Bir binanın köşelerini bir parsel köşesinden, kilometre taşlarını bir başlangıç
noktasından ya da bir kesitin kotlarını tek bir referanstan ölçmek isteyen herkes için;
bu sayfayı bitirdiğinizde baz ölçüyü arayüzden, komut satırından ve betikten çizmeyi
bileceksiniz.

## Ne yapar

`BAZÖLÇÜ`, çizimin en son **doğrusal** ya da **hizalı** ölçüsünden (ya da `temel=`
ile verdiğiniz ölçüden) başlar: her yeni ölçü o ölçünün **ilk noktasından** sizin
gösterdiğiniz noktaya kadar ölçer. Ölçü çizgileri **üst üste** dizilir: her biri bir
öncekinin, ölçülen noktalardan uzak yanına, stilin **baz aralığı** kadar ötesine
konur.

- Baz aralığı stilin kâğıttaki değeridir ([`ÖLÇÜSTİLİ`](dimension_style.md)):
  `ISO-25` için 3,75 mm, 1/1000 paftada zeminde 3,75 m. Stilde verilmemişse yazı
  yüksekliğinin 1,5 katıdır.
- Her ölçü **ayrı bir ölçüdür**; stil, ok, ondalık, birim, önek ve sonek temel
  ölçününkidir, doğrultusu da.
- Köşelere tam düşen noktalar [bağlanır](dimension.md#bağlı-ölçü).

## Adlar

| Ad | Tür |
|---|---|
| `BAZÖLÇÜ` | Türkçe, birincil |
| `BAZOLCU` | ASCII karşılık |
| `DIMBASELINE` | İngilizce karşılık |
| `BÖ`, `BO` | Kısaltma |
| `core.dimension_baseline` | Komut kimliği |

## Sözdizimi

```text
BAZÖLÇÜ [temel=<kimlik>] noktalar=<nokta> <nokta> … [bagla=evet|hayır]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `temel` | Başlanacak doğrusal ya da hizalı ölçü. Verilmezse çizimin en son çizilen doğrusal ya da hizalı ölçüsü |
| `noktalar` | Tabandan ölçülecek noktalar; her biri temel ölçünün ilk noktasından ölçülür |
| `bagla` | Köşelere tam düşen noktalar bağlansın mı; varsayılan `evet` |
| `katalog` | Stil kataloğu dosyası; varsayılan `TERCİH ölçü_stilleri` |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Aynı başlangıçtan 5, 12,50 ve 16,75 m:

<!-- örnek: yeni çizim -->
```
ÖLÇÜ tur=dogrusal birinci=0,0 ikinci=5,0 konum=2.5,-3
BAZÖLÇÜ noktalar=12.5,0 16.75,0
```

```text
Baz ölçü: 2 ölçü eklendi (12,50 · 16,75).
```

İkinci ölçünün çizgisi 3,75 m, üçüncüsününki 7,50 m daha aşağıdadır.

### Arayüz

**Açıklama ▸ Ölçü ▸ Baz Ölçü** (bir ölçü seçiliyken beliren **Ölçü** sekmesinde de vardır).
Önce bir doğrusal ya da hizalı ölçü çizin; sonra ölçülecek noktaları sırayla tıklayın
ve **Enter** ile bitirin. İmleçte sıradaki ölçü, gideceği çizgide önizlenir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.dimension",
      "args": { "tur": "dogrusal", "birinci": [0, 0], "ikinci": [5000, 0], "konum": [2500, -3000] } },
    { "cmd": "core.dimension_baseline", "args": { "noktalar": [[12500, 0], [16750, 0]] } }
  ]
}
```

## Geri alma

Tek adımdır: [`GERİAL`](undo.md) eklenen bütün ölçüleri birlikte kaldırır.

## Betikten kullanım

Betikten çağrıldığında komut hiçbir şey sormaz; `noktalar` verilmelidir. Günlüğe
çözülmüş `temel` ve noktalar yazılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Başlanacak doğrusal ya da hizalı bir ölçü yok: önce ÖLÇÜ çizin ya da temel= verin.` | Çizimde uygun ölçü yok | Önce [`ÖLÇÜ`](dimension.md) ile bir ölçü çizin |
| `Nesne N doğrusal ya da hizalı bir ölçü değil; zincir ve baz ölçü onlardan kurulur.` | `temel` başka bir nesneyi gösteriyor | Bir doğrusal ya da hizalı ölçünün kimliğini verin |
| `Tabandan ölçülecek nokta verilmedi.` | Hiç nokta verilmedi ya da hepsi atlandı | Noktaları verin |

## İlgili

- [`ZİNCİRÖLÇÜ`](dimension_continue.md) — her ölçü bir öncekinin ucundan
- [`ÖLÇÜSTİLİ`](dimension_style.md) — stillerin baz aralığı
- [`ÖLÇÜ`](dimension.md) — ilk ölçü
