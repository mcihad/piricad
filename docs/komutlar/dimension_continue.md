# ZİNCİRÖLÇÜ — Zincir Ölçü

Bir cephenin açıklıklarını, bir yol boyunca parsellerin cephe genişliklerini ya da bir
duvarın parçalarını tek bir çizgi üzerinde art arda ölçmek isteyen herkes için; bu
sayfayı bitirdiğinizde zincir ölçüyü arayüzden, komut satırından ve betikten çizmeyi
bileceksiniz.

## Ne yapar

`ZİNCİRÖLÇÜ`, çizimin en son **doğrusal** ya da **hizalı** ölçüsünden (ya da
`temel=` ile verdiğiniz ölçüden) başlar: her yeni ölçü **bir öncekinin ikinci
noktasından** sizin gösterdiğiniz noktaya kadar ölçer ve **aynı ölçü çizgisi**
üzerine yazılır. Bitince toplamı söyler.

- Her parça **ayrı bir ölçüdür**: ayrı düzenlenir, ayrı silinir.
- Parçalar temel ölçünün **doğrultusundadır**. Hizalı bir ölçüden başlarsanız
  parçalar o doğrultuda doğrusal ölçülerdir; hepsi tek çizgide durur.
- Stil, ok, ondalık, birim, önek ve sonek temel ölçününkidir; elle yazılmış
  yazısı ise alınmaz.
- Köşelere tam düşen noktalar [bağlanır](dimension.md#bağlı-ölçü): köşe taşınınca
  parça yeniden ölçülür.
- Bir öncekiyle aynı yerde ölçülen nokta çizilmez, atlandığı söylenir.

Arayüzde her noktaya kadar sıradaki ölçü ve yazacağı değer imleçte önizlenir.

## Adlar

| Ad | Tür |
|---|---|
| `ZİNCİRÖLÇÜ` | Türkçe, birincil |
| `ZINCIROLCU` | ASCII karşılık |
| `DIMCONTINUE` | İngilizce karşılık |
| `ZÖ`, `ZO` | Kısaltma |
| `core.dimension_continue` | Komut kimliği |

## Sözdizimi

```text
ZİNCİRÖLÇÜ [temel=<kimlik>] noktalar=<nokta> <nokta> … [bagla=evet|hayır]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `temel` | Başlanacak doğrusal ya da hizalı ölçü. Verilmezse çizimin en son çizilen doğrusal ya da hizalı ölçüsü |
| `noktalar` | Zincirin sonraki noktaları; her biri bir öncekinden ölçülür. Arayüzde tek tek tıklanır, Enter bitirir |
| `bagla` | Köşelere tam düşen noktalar bağlansın mı; varsayılan `evet` |
| `katalog` | Stil kataloğu dosyası; varsayılan `TERCİH ölçü_stilleri` |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

5 m'lik ilk ölçüden sonra 7,50 ve 4,25 m:

<!-- örnek: yeni çizim -->
```
ÖLÇÜ tur=dogrusal birinci=0,0 ikinci=5,0 konum=2.5,-3
ZİNCİRÖLÇÜ noktalar=12.5,0 16.75,0
```

```text
Zincir ölçü: 2 ölçü eklendi (7,50 · 4,25); toplam 11,75.
```

### Arayüz

**Çizim ▸ Zincir Ölçü**, ya da araç kutusunda Ölçü ailesinin **Zincir Ölçü**
düğmesi. Önce bir doğrusal ya da hizalı ölçü çizin; sonra zincirin noktalarını
sırayla tıklayın — yakalama köşelere oturtur — ve **Enter** ile bitirin.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.dimension",
      "args": { "tur": "dogrusal", "birinci": [0, 0], "ikinci": [5000, 0], "konum": [2500, -3000] } },
    { "cmd": "core.dimension_continue", "args": { "noktalar": [[12500, 0], [16750, 0]] } }
  ]
}
```

### Üçü de aynı

Tıklanan, yazılan ve betikten verilen noktalar aynı ölçüleri bırakır; aynı belge,
aynı günlük (`tests/unit/test_dimtext.cpp`, `ZİNCİRÖLÇÜ KANIT`).

## Geri alma

Tek adımdır: [`GERİAL`](undo.md) zincirin bütün parçalarını birlikte kaldırır.

## Betikten kullanım

Betikten çağrıldığında komut hiçbir şey sormaz; `noktalar` verilmelidir. Günlüğe
çözülmüş `temel` (kimlik) ve noktalar yazılır; oynatma, sonradan çizilmiş başka bir
ölçüden değil, aynı temelden başlar.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Başlanacak doğrusal ya da hizalı bir ölçü yok: önce ÖLÇÜ çizin ya da temel= verin.` | Çizimde uygun ölçü yok | Önce [`ÖLÇÜ`](dimension.md) ile bir ölçü çizin |
| `Nesne N doğrusal ya da hizalı bir ölçü değil; zincir ve baz ölçü onlardan kurulur.` | `temel` başka bir nesneyi gösteriyor | Bir doğrusal ya da hizalı ölçünün kimliğini verin |
| `Zincire nokta verilmedi.` | Hiç nokta verilmedi ya da hepsi atlandı | Noktaları verin |

## İlgili

- [`BAZÖLÇÜ`](dimension_baseline.md) — her ölçü aynı ilk noktadan
- [`ÖLÇÜ`](dimension.md) — ilk ölçü
- [`ÖLÇÜDÜZENLE`](dimension_edit.md) — bir parçanın yazısını değiştirmek
