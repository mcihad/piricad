# SİL — Nesne Silme

Çiziminden nesne çıkarmak isteyen kullanıcı için; bu sayfayı bitirdiğinizde nesne
kimliğiyle silmeyi ve silmeyi geri almayı bileceksiniz.

## Ne yapar

Kimliği verilen nesneleri çizimden kaldırır. Birden fazla nesne tek komutta silinebilir
ve hepsi **tek bir geri alma adımı** olur.

Silme geri alınabilirdir: `GERİAL` nesneleri olduğu gibi geri getirir.

Verilen kimliklerden biri bile geçersizse **hiçbiri silinmez.** Yarım uygulanmış bir
silme kabul edilmez.

## Adlar

| Ad | Tür |
|---|---|
| `SİL` | Türkçe, birincil |
| `SIL` | Türkçe karaktersiz klavye için |
| `ERASE` | İngilizce karşılık |
| `E` | Kısaltma |
| `core.erase` | Komut kimliği |

## Sözdizimi

```
SİL nesneler=<kimlik>
SİL nesneler=<kimlik> nesneler=<kimlik> ...
```

## Parametreler

Tek parametresi vardır: **`nesneler`** — silinecek nesnelerin kimlikleri. En az bir
kimlik gerekir.

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

### Nesne kimliğini nereden bulursunuz

Bu sürümde grafik seçim aracı henüz yoktur; nesne kimliğini iki yerden okursunuz:

- **Komut Günlüğü** panelinden — nesneleri hangi sırayla yarattığınızı görürsünüz.
  Kimlikler sıfırdan başlar ve yaratılış sırasına göre artar.
- Bir betikten çiziyorsanız, kaçıncı segmenti yarattığınızı biliyorsunuzdur.

Fareyle seçim ve seçime göre silme Faz 2'de gelecek.

## Örnekler

### Komut satırı

Tek nesne sil:

```
SİL nesneler=0
```

Üç nesne birden sil:

```
SİL nesneler=0 nesneler=1 nesneler=2
```

Silmeyi geri al:

```
GERİAL
```

### Arayüz

Araç kutusundaki **Sil** düğmesi veya **Düzen > Sil** menüsü komut satırını
`SİL nesneler=` metniyle hazırlar ve odağı oraya taşır; kimliği yazıp **Enter**'a
basmanız yeterlidir. Transkriptte hatırlatma görürsünüz:

```text
SİL komutu nesne kimliği ister. Örnek:  SİL nesneler=0
```

Grafik seçim Faz 2'de geldiğinde bu düğme doğrudan seçili nesneleri silecek.

### Betik

```json
{
  "ad": "Yardımcı çizgileri temizle",
  "komutlar": [
    { "cmd": "core.erase", "args": { "nesneler": [12, 13, 14] } }
  ]
}
```

Betikte `nesneler` bir kimlik dizisidir.

## Geri alma

Silme tek bir geri alma adımıdır; kaç nesne sildiğinizden bağımsız olarak tek `GERİAL`
hepsini geri getirir.

```
GERİAL
```

Nesneler kimlikleriyle birlikte geri gelir, yani daha sonra tekrar silebilirsiniz.

Bkz. [Geri alma](undo.md).

## Betikten kullanım

`SİL` betiklenebilir ve AI erişimlidir.

Bir betik içinde silme yaparken, kimliklerin betiğin kendisinin yarattığı nesnelere ait
olduğundan emin olun. Kimlikler yaratılış sırasına göre verilir: betiğin ilk `core.line`
komutu iki nokta alırsa `0` kimlikli tek nesne, üç nokta alırsa `0` ve `1` kimlikli iki
nesne yaratır.

Betik çalışırken bir silme başarısız olursa **betiğin tamamı geri alınır.**

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Silinecek nesne belirtilmedi. Örnek: SİL nesneler=0` | `nesneler` boş geçilmiş | Bir kimlik verin |
| `Nesne bulunamadı veya zaten silinmiş: 99` | Kimlik yok ya da nesne zaten silinmiş | Kimliği denetleyin; hiçbir şey silinmedi |
| `Geçersiz nesne kimliği: -1` | Negatif kimlik verilmiş | Kimlikler sıfır veya daha büyüktür |
| `'core.erase': zorunlu 'nesneler' parametresi eksik. Beklenen: nesne seçimi` | Parametre hiç verilmemiş | `nesneler=` ile kimlik verin |
| `Bilinmeyen nesne kimliği: 99` | Betikten var olmayan kimlik gelmiş | Kimlikleri denetleyin |

Bulunamayan kimlik verdiğinizde komut hata döndürmez; transkripte açıklama yazar ve
çizime dokunmaz.

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
