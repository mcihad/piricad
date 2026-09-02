# BÖL — Çizgiyi İkiye Bölme

Bir yol kenarını kavşak noktasından, bir sınırı bir röperden ayırması gereken
herkes için; bu sayfayı bitirdiğinizde bölmeyi arayüzden, komut satırından ve
betikten yapmayı bileceksiniz.

## Ne yapar

`BÖL`, bir çizgiyi verdiğiniz noktadan **iki ayrı nesneye** ayırır.

**İlk yarı nesnenin kendisi olarak kalır**: kimliği, katmanı, stili ve
öznitelikleri onda durur. İkinci yarı **yeni bir nesnedir** ve yeni bir kimlik
alır. Bu, ifrazın yaptığının aynısıdır: bir parsel devam eder, biri yeni doğar.

Bölme noktası çizginin üzerinde olmak zorunda değildir; en yakın kenara düşürülür.

Bölme noktası **uçta olamaz** — sıfır uzunlukta bir parça çizgi değildir ve komut
bunu yapmak yerine söyler.

`BÖL` yalnız **açık çizgilerle** çalışır. Daire, yay, nokta ve kapalı alan
reddedilir.

## Adlar

| Ad | Tür |
|---|---|
| `BÖL` | Türkçe, birincil |
| `BOL` | ASCII karşılık |
| `SPLIT` | İngilizce karşılık |
| `BL` | Kısaltma |
| `core.split` | Komut kimliği |

## Sözdizimi

```text
BÖL nesne=<k> nokta=<n>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Bölünecek çizginin kimliği |
| `nokta` | Bölme noktası |

## Örnekler

### Komut satırı

```text
BÖL nesne=1 nokta=485330,4310200
```

```text
Çizgi ikiye bölündü.
```

### Arayüz

Çizgiyi seçin, `BÖL` yazın, bölme noktasını tıklayın. Yakalama açıkken nokta
mevcut köşelere ve kesişimlere oturur.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.split",
      "args": { "nesne": [1], "nokta": [485330000, 4310200000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`BÖL` tek bir geri alma adımıdır: [`GERİAL`](undo.md) iki yarıyı tekrar tek çizgi
yapar.

## Betikten kullanım

Betikten çağrıldığında `nesne` ve `nokta` verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bölünecek çizgi belirtilmedi. Örnek: BÖL nesne=1 nokta=30,0` | `nesne` verilmedi | Çizginin kimliğini yazın |
| `Bölme noktası çizginin ucunda; bölünecek bir şey kalmıyor.` | Nokta uçta | Çizginin içinde bir nokta verin |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Nesne N bir eğri ya da nokta; bu komut yalnız çizgilerle çalışır.` | Daire, yay ya da nokta verildi | Yalnız çizgi seçin |
| `Nesne N açık bir çizgi değil; bu komut yalnız açık çizgilerle çalışır.` | Kapalı alan verildi | Açık bir çizgi seçin |

## İlgili

- [`BUDA`](trim.md) · [`UZAT`](extend.md)
- [`ÇOKLUÇİZGİ`](polyline.md) · [`SEÇ`](select.md)
