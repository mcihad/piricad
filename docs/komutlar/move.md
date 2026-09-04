# TAŞI — Nesne Taşıma

Bir bloğu doğru istasyonuna oturtan, yanlış yere düşmüş bir çizimi yerine
taşıyan herkes için; bu sayfayı bitirdiğinizde seçili nesneleri arayüzden, komut
satırından ve betikten taşımayı bileceksiniz.

## Ne yapar

`TAŞI`, seçili nesneleri **iki nokta arasındaki kadar** öteler. Nesneler aynı
nesne olarak kalır: kimlikleri, katmanları, stilleri, öznitelikleri ve yazıları
değişmez — değişen yalnız koordinatlarıdır.

Taşıma miktarı iki noktayla verilir, tek bir öteleme vektörüyle değil. Sebebi
pratiktir: bir parselin köşesini komşusunun köşesine oturtmak istediğinizde iki
noktayı yakalamayla tıklarsınız ve program farkı kendisi hesaplar.

Her tür taşınır. Daire dairelik, yay yaylık kalır: dönüşüm nesnenin **tanımına**
uygulanır, ham tepe noktalarına değil — dairenin merkezi ile yarıçap tutamağını
birbirinden bağımsız oynatmak daire olmayan bir kayıt bırakırdı.

## Adlar

| Ad | Tür |
|---|---|
| `TAŞI` | Türkçe, birincil |
| `TASI` | ASCII karşılık |
| `MOVE` | İngilizce karşılık |
| `TŞ` | Kısaltma |
| `core.move` | Komut kimliği |

## Sözdizimi

```text
TAŞI
TAŞI nesneler=<k1> nesneler=<k2> …
TAŞI nesneler=<k> baslangic=<n> bitis=<n>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Taşınacak nesnelerin kimlikleri. Verilmezse etkin seçim |
| `baslangic` | Taşımanın başlangıç noktası |
| `bitis` | Taşımanın bitiş noktası |

## Örnekler

### Komut satırı

Seçili nesneleri 100 metre doğuya, 50 metre kuzeye taşıyın:

```text
SEÇ
TAŞI baslangic=0,0 bitis=100,50
```

Kimlik vererek, göreli koordinatla:

```text
TAŞI nesneler=1 nesneler=2 baslangic=0,0 bitis=@25,0
```

### Arayüz

Nesneleri seçin, sol araç kutusundaki **Taşı** düğmesine basın, sonra iki noktayı
tıklayın.
İki tıklama arasında kesikli bir kılavuz uzanır. Yakalama açıkken iki nokta da
mevcut nesnelere oturur — bir köşeyi komşu parselin köşesine tam oturtmak için
[`MOD`](mode.md) ile uç nokta yakalamasını açık tutun.

Seçim boşken de çalışır: düğmeye basın, komut satırı hangi nesneleri istediğini
yazar, tuvalden tıklayarak seçin ve **Enter**'a basın. Vazgeçmek için Esc.
Nesneleri önceden seçtiyseniz sorulmaz.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.move",
      "args": { "nesneler": [1, 2],
                "baslangic": [0, 0], "bitis": [100000, 50000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`TAŞI` tek bir geri alma adımıdır; kaç nesne taşıdığınızdan bağımsız olarak tek
[`GERİAL`](undo.md) hepsini yerine döndürür.

## Betikten kullanım

Betikten çağrıldığında `nesneler`, `baslangic` ve `bitis` verilmelidir; betik
çalışırken "etkin seçim" diye bir şey olmayabilir ve bir betiğin ekranda ne
seçili olduğuna bağlı olması, aynı betiğin iki çalıştırmada iki farklı sonuç
vermesi demektir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne belirtilmedi ve seçim boş. Örnek: ...` | Ne kimlik verildi ne seçim var | Nesneleri seçin ya da kimliklerini yazın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |

## İlgili

- [`KOPYALA`](copy.md) — taşımak yerine çoğaltır
- [`DÖNDÜR`](rotate.md) · [`ÖLÇEKLE`](scale.md) · [`AYNALA`](mirror.md)
- [`SEÇ`](select.md) · [`MOD`](mode.md) · [`GERİAL`](undo.md)
