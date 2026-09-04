# BÖL — Kesme Çizgisiyle Bölme

Bir yol kenarını kavşak noktasından, bir sınırı bir röperden ayırması gereken
herkes için; bu sayfayı bitirdiğinizde bölmeyi arayüzden, komut satırından ve
betikten yapmayı bileceksiniz.

## Ne yapar

`BÖL`, seçtiğiniz nesneleri **çizdiğiniz bir kesme çizgisiyle** ayırır. Kesme
çizgisini iki nokta tıklayarak çizersiniz; ikinci noktayı ararken kesme çizgisi
kılavuz olarak fareyi takip eder.

- **Çizgiler** kesme çizgisinin geçtiği yerden ikiye bölünür. İlk yarı nesnenin
  kendisi olarak kalır — kimliği, katmanı, stili ve öznitelikleri onda durur —
  ikinci yarı yeni bir nesnedir.
- **Alanlar** kesme çizgisinin iki yanına düşen iki parçaya ayrılır. Her iki
  parça da özgün nesnenin bütün öznitelik sütunlarını alır: bir şeyi ikiye
  bölmek onun **ne olduğunu** değiştirmez.

Kesme çizgisinin dokunmadığı nesneler olduğu gibi kalır ve komut kaç tanesini
kesmediğini söyler.

### Alan keserken çizgi uzatılır, çizgi keserken uzatılmaz

Bu ayrım kasıtlıdır. Bir **alanı** bölmek için kesme çizgisinin alanı baştan başa
geçmesi gerekir, bu yüzden çizdiğiniz parça iki yana sonsuza uzatılır. Bir
**çizgiyi** ise yalnız çizdiğiniz parçanın gerçekten kestiği yerde böler; yoksa
bir sınırın üstüne çektiğiniz kısa bir çizik, aynı doğrultudaki bütün seçili
çizgileri de keserdi.

## Bu komut ifraz değildir

`BÖL` genel bir geometri işlemidir. [`İFRAZ`](split_parcel.md) aynı kesmenin
üstüne kadastro kurallarını koyar: alan raporu, ada/parsel numaraları ve mevzuat.
İkisi de aynı geometriyi (`core::half_plane`) kullanır, ama parsel bölecekseniz
`İFRAZ` kullanın.

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
BÖL [nesne=<k>…] noktalar=<n1> noktalar=<n2>
BÖL nesne=<k> nokta=<n>          # eski biçim: tek çizgiyi bir noktadan böler
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Kesilecek nesneler; verilmezse etkin seçim |
| `noktalar` | Kesme çizgisinin iki noktası |
| `nokta` | **Eski biçim**: tek açık çizgiyi bu noktadan böler. `noktalar` verilirse yok sayılır |

## Örnekler

### Komut satırı

```text
BÖL nesne=1 noktalar=485330,4310190 noktalar=485330,4310260
```

```text
1 çizgi bölündü.
```

Eski biçim de çalışmaya devam eder:

```text
BÖL nesne=1 nokta=485330,4310200
```

```text
Çizgi ikiye bölündü.
```

### Arayüz

Sol araç kutusundaki **Böl** düğmesine basın, kesilecek nesneleri tıklayarak
seçin, **Enter**'a (ya da sağ tuşa) basın, sonra kesme çizgisinin iki ucunu
tıklayın. İkinci ucu ararken kesme çizgisi kılavuz olarak fareyi takip eder.
Yakalama açıkken uçlar mevcut köşelere ve kesişimlere oturur.

Nesneleri önceden seçtiyseniz doğrudan kesme çizgisini çizmeye geçer.

Seçim boşken de çalışır: düğmeye basın, komut satırı hangi nesneleri istediğini
yazar, tuvalden tıklayarak seçin ve **Enter**'a basın. Vazgeçmek için Esc.
Nesneleri önceden seçtiyseniz sorulmaz.

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
