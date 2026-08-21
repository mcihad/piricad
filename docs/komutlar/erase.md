# SİL — Nesne Silme

Çiziminden nesne çıkarmak isteyen kullanıcı için; bu sayfayı bitirdiğinizde seçerek
ve nesne kimliğiyle silmeyi, silmeyi geri almayı bileceksiniz.

## Ne yapar

Etkin seçimdeki ya da kimliği verilen nesneleri çizimden kaldırır. Birden fazla nesne
tek komutta silinebilir ve hepsi **tek bir geri alma adımı** olur.

Silme geri alınabilirdir: `GERİAL` nesneleri olduğu gibi geri getirir.

Verilen kimliklerden biri bile geçersizse **hiçbiri silinmez.** Yarım uygulanmış bir
silme kabul edilmez.

Silinen nesneler seçimden de düşer. Silinen bir nesnenin kimliği emekliye ayrılır ve
hiçbir zaman başka bir nesneye verilmez — "bu parsel hangisiydi?" hukuki bir sorudur
ve cevapsız kalamaz.

## Adlar

| Ad | Tür |
|---|---|
| `SİL` | Türkçe, birincil |
| `SIL` | Türkçe karaktersiz klavye için |
| `ERASE` | İngilizce karşılık |
| `E` | Kısaltma |
| `core.erase` | Komut kimliği |

## Sözdizimi

```text
SİL
SİL nesneler=<kimlik>
SİL nesneler=<kimlik> nesneler=<kimlik> ...
```

Argümansız `SİL`, [`SEÇ`](select.md) ile belirlediğiniz etkin seçimi siler.

## Parametreler

Tek parametresi vardır: **`nesneler`** — silinecek nesnelerin kimlikleri. Verilmezse
etkin seçim kullanılır.

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

### Nesne kimliğini nereden bulursunuz

Kimlikler **1'den başlar** ve yaratılış sırasına göre artar. Hiçbir kimlik yeniden
kullanılmaz: bir nesne silinince kimliği emekli olur.

Kimliği üç yerden okursunuz:

- [`SEÇ`](select.md) komutundan — seçtiğiniz nesnelerin kimliklerini transkripte yazar
- **Komut Günlüğü** panelinden — nesneleri hangi sırayla yarattığınızı görürsünüz
- Bir betikten çiziyorsanız, kaçıncı segmenti yarattığınızı biliyorsunuzdur

En pratik yolu kimlik okumamaktır: nesneyi seçip argümansız `SİL` yazın.

## Örnekler

### Komut satırı

Önce silinecek bir şey çizin:

```
ÇİZGİ 0,0 10,0
```

Seçip silin — CAD'de olağan sıra budur:

```
SEÇ NOKTA 5,0 tolerans=1
SİL
```

Tek nesneyi kimliğiyle silin:

```
SİL nesneler=1
```

Üç nesne birden silin:

```
SİL nesneler=1 nesneler=2 nesneler=3
```

Silmeyi geri alın:

```
GERİAL
```

### Arayüz

Nesneleri fareyle seçin (tek tık ya da kutu sürükleyin), sonra araç kutusundaki veya
**Düzen** araç çubuğundaki **Sil** düğmesine basın — ya da **Düzen > Sil** menüsünü
kullanın. Seçim boşsa transkriptte hatırlatma görürsünüz:

```text
Silinecek nesne seçili değil. Nesneleri seçin ya da SİL nesneler=1 yazın.
```

Seçim yapmayı [`SEÇ`](select.md) sayfası anlatır.

### Betik

```json
{
  "ad": "Yardımcı çizgileri temizle",
  "komutlar": [
    { "cmd": "core.line",  "args": { "noktalar": [[0,0],[10000,0]] } },
    { "cmd": "core.line",  "args": { "noktalar": [[0,5000],[10000,5000]] } },
    { "cmd": "core.erase", "args": { "nesneler": [1, 2] } }
  ]
}
```

Betikte `nesneler` bir kimlik dizisidir. Boş bırakılırsa etkin seçim silinir.

İki kimlikli bir dizi — `[1, 2]` — JSON'da bir noktayla aynı görünür. `nesneler`
parametresinde kimlik olarak okunur; ayrımı komutun bildirimi yapar, dosyanın biçimi
değil.

## Geri alma

Silme tek bir geri alma adımıdır; kaç nesne sildiğinizden bağımsız olarak tek `GERİAL`
hepsini geri getirir.

```
GERİAL
```

Nesneler kimlikleriyle birlikte geri gelir, yani daha sonra tekrar silebilirsiniz.
Seçim geri gelmez: seçim çizimin verisi değildir.

Bkz. [Geri alma](undo.md).

## Betikten kullanım

`SİL` betiklenebilir ve AI erişimlidir.

Bir betik içinde silme yaparken, kimliklerin betiğin kendisinin yarattığı nesnelere ait
olduğundan emin olun. Kimlikler yaratılış sırasına göre verilir: betiğin ilk `core.line`
komutu iki nokta alırsa `1` kimlikli tek nesne, üç nokta alırsa `1` ve `2` kimlikli iki
nesne yaratır.

Daha sağlamı, kimlik saymak yerine seçmektir:

```json
[
  { "cmd": "core.line",   "args": { "noktalar": [[0,0],[10000,0]] } },
  { "cmd": "core.select", "args": { "mod": "PENCERE", "noktalar": [[-1000,-1000],[11000,1000]] } },
  { "cmd": "core.erase",  "args": {} }
]
```

Betik çalışırken bir silme başarısız olursa **betiğin tamamı geri alınır.**

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Silinecek nesne belirtilmedi ve seçim boş. Örnek: SİL nesneler=1` | `nesneler` verilmemiş ve seçim boş | Önce [`SEÇ`](select.md) ile seçin ya da bir kimlik verin |
| `Nesne bulunamadı veya zaten silinmiş: 99` | Kimlik yok ya da nesne zaten silinmiş | Kimliği denetleyin; hiçbir şey silinmedi |
| `Geçersiz nesne kimliği: 0. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik verilmiş | Kimlikler `1`'den başlar |
| `'core.erase': 'nesneler' parametresi nesne seçimi bekliyor, başka türde bir değer geldi.` | Kimlik yerine metin gelmiş | Tam sayı kimlik verin |

Bulunamayan kimlik verdiğinizde komut hata döndürmez; transkripte açıklama yazar ve
çizime dokunmaz.

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
