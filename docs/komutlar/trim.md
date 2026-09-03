# BUDA — Çizgiyi Sınıra Kadar Budama

Sınırı aşan bir çizgiyi kestiği yere kadar kısaltması gereken herkes için; bu
sayfayı bitirdiğinizde budamayı arayüzden, komut satırından ve betikten yapmayı
bileceksiniz.

## Ne yapar

`BUDA`, bir çizginin ucunu, **kestiği sınır çizgisine** kadar geri çeker.

**Hangi ucun atılacağını verdiğiniz nokta söyler**: atmak istediğiniz parçanın
üzerine yakın bir yer gösterin. Bu, her çizim programının çalışma biçimidir —
gitmesini istediğiniz parçayı gösterirsiniz.

Kesişme **sınır çizgisinin üzerinde** olmalıdır, uzantısında değil. İki sınırın
uzatılsalardı buluşacakları yere budanmış bir çizgi, var olmayan bir yere budanmış
demektir.

`BUDA` yalnız **açık çizgilerle** çalışır. Yayı bir çizgiye budamak gerçek bir
işlemdir ama bu komut değildir: çember-doğru kesişimi ister, ve bir yayı kirişi
gibi işleyen bir komut yol kurbunu kirişin yaydan sapması kadar kaydırırdı. Bu
kaba bir çizim değil, **yanlış** bir çizimdir.

## Adlar

| Ad | Tür |
|---|---|
| `BUDA` | Türkçe, birincil |
| `TRIM` | İngilizce karşılık |
| `BD` | Kısaltma |
| `core.trim` | Komut kimliği |

## Sözdizimi

```text
BUDA nesne=<k> sinir=<k> nokta=<n>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Budanacak çizginin kimliği |
| `sinir` | Sınır çizgisinin kimliği |
| `nokta` | Atılacak parçanın üzerinde bir nokta |

## Örnekler

### Komut satırı

```text
BUDA nesne=1 sinir=2 nokta=485390,4310200
```

```text
Çizgi sınıra kadar budandı.
```

### Arayüz

**İki çizgiyi** seçin, sol araç kutusundaki **Buda** düğmesine basın, sonra atmak
istediğiniz parçayı tıklayın.

Hangisinin budanacağını **tıklama** söyler: tıkladığınız noktaya daha yakın olan
çizgi budanır, diğeri sınır olur. Seçim sırası kullanılmaz, çünkü seçim listesi
tıklama sırasını değil nesne kimliğini takip eder — "önce seçtiğim" demek "önce
çizdiğim" demek olurdu.

Komut satırından `BUDA nesne=1 sinir=2` yazarak ikisini açıkça da verebilirsiniz.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.trim",
      "args": { "nesne": [1], "sinir": [2], "nokta": [485390000, 4310200000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`BUDA` tek bir geri alma adımıdır; [`GERİAL`](undo.md) atılan parçayı geri getirir.

## Betikten kullanım

Betikten çağrıldığında `nesne`, `sinir` ve `nokta` verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bu uç sınır çizgisini kesmiyor; budanacak bir şey yok.` | Çizgi sınırı kesmiyor | Kesişen bir sınır verin ya da [`UZAT`](extend.md) kullanın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Nesne N bir eğri ya da nokta; bu komut yalnız çizgilerle çalışır.` | Daire, yay ya da nokta verildi | Yalnız çizgi seçin |
| `Nesne N açık bir çizgi değil; bu komut yalnız açık çizgilerle çalışır.` | Kapalı alan verildi | Açık bir çizgi seçin |

## İlgili

- [`UZAT`](extend.md) — kısaltmak yerine uzatır
- [`BÖL`](split.md) — bir noktadan ikiye ayırır
