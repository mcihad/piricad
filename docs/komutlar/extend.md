# UZAT — Çizgiyi Sınıra Uzatma

Sınıra yetişmeyen bir çizgiyi oraya kadar uzatması gereken herkes için; bu sayfayı
bitirdiğinizde uzatmayı arayüzden, komut satırından ve betikten yapmayı
bileceksiniz.

## Ne yapar

`UZAT`, bir çizginin ucunu, **kendi doğrultusunda**, verilen sınır çizgisine
ulaşana kadar ileri taşır.

**Hangi ucun uzayacağını verdiğiniz nokta söyler**: uzatmak istediğiniz uca yakın
bir yer gösterin.

Çizginin yönü değişmez; yalnız uç noktası kendi doğrultusu üzerinde ileri gider.
Kesişme sınır çizgisinin **üzerinde** olmalıdır.

`UZAT` yalnız **açık çizgilerle** çalışır ([`BUDA`](trim.md) sayfasındaki sebeple).

## Adlar

| Ad | Tür |
|---|---|
| `UZAT` | Türkçe, birincil |
| `EXTEND` | İngilizce karşılık |
| `UZ` | Kısaltma |
| `core.extend` | Komut kimliği |

## Sözdizimi

```text
UZAT nesne=<k> sinir=<k> nokta=<n>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Uzatılacak çizginin kimliği |
| `sinir` | Sınır çizgisinin kimliği |
| `nokta` | Uzatılacak ucun yakınında bir nokta |

## Örnekler

### Komut satırı

```text
UZAT nesne=1 sinir=2 nokta=485350,4310200
```

```text
Çizgi sınıra uzatıldı.
```

### Arayüz

`UZAT` yazın, uzatılacak çizgiyi ve sınırı verin, sonra uzatmak istediğiniz uca
yakın bir yeri tıklayın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.extend",
      "args": { "nesne": [1], "sinir": [2], "nokta": [485350000, 4310200000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`UZAT` tek bir geri alma adımıdır.

## Betikten kullanım

Betikten çağrıldığında `nesne`, `sinir` ve `nokta` verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bu uç, sınır çizgisine uzatılarak ulaşamıyor: kesişme yok.` | Doğrultu sınırı kesmiyor | Doğrultunun kestiği bir sınır verin |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Nesne N bir eğri ya da nokta; bu komut yalnız çizgilerle çalışır.` | Daire, yay ya da nokta verildi | Yalnız çizgi seçin |
| `Nesne N açık bir çizgi değil; bu komut yalnız açık çizgilerle çalışır.` | Kapalı alan verildi | Açık bir çizgi seçin |

## İlgili

- [`BUDA`](trim.md) — uzatmak yerine kısaltır
- [`BÖL`](split.md)
