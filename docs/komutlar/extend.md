# UZAT — Ucu Sınıra Kadar Uzatma

Sınıra yetişmeyen bir çizgiyi, yayı ya da elips yayını oraya kadar uzatması gereken
herkes için;
bu sayfayı bitirdiğinizde uzatmayı arayüzden, komut satırından ve betikten yapmayı
bileceksiniz.

## Ne yapar

`UZAT`, tıkladığınız yere **yakın olan ucu**, kendi yolunda ilerleterek ulaştığı
ilk sınıra kadar taşır:

- Bir **çizginin** ucu kendi doğrultusunda ilerler; çizginin yönü değişmez.
- Bir **yayın** ucu kendi çemberi boyunca ilerler; merkez ve yarıçap aynı kalır.
  Yay, kendi üstüne dolanacak kadar uzatılmaz.
- Bir **elips yayının** ucu kendi elipsi boyunca, elipsin kendi parametresinde
  ilerler; merkez ve eksenler aynı kalır. O da kendi üstüne dolanacak kadar
  uzatılmaz.

Ulaşılan nokta sınırın **üzerindedir**: bir yaya, daireye, elipse ya da spline'a
uzatılan çizgi, eğrinin gerçek kesişiminde durur, çizildiği kirişlerde değil.

`UZAT` açık çizgilerde, yaylarda ve elips yaylarında çalışır; elips ve spline sınır
da olur. Daire, tam elips ve kapalı alanın ucu yoktur, bu yüzden uzatılmazlar.
**Spline uzatılmaz**: eğri son düğümünde biter ve ötesi tanımsızdır; bir uzantı bu
programın yapmadığı bir tahmin olurdu. Spline'ı bir sınıra ulaştırmak için ucundan
bir çizgi çizin.

### Sınırlar

Sınırlar [`BUDA`](trim.md)'daki sırayla belirlenir: `sinir=` ile verilenler, yoksa
seçtikleriniz, o da yoksa tıkladığınız nesnenin yakınındaki her görünür çizgi, yay,
daire, elips ve spline (`hepsi=evet`).

### Uçları göstermenin iki yolu daha

- **Çitle** (`yontem=çit`): bir çit çizersiniz; çitin geçtiği her nesnenin, çite
  yakın olan ucu uzatılır. Bir çizgiyi iki ucunun yakınından geçen bir çit iki ucunu
  da uzatır. Bütün uzantılar Enter'dan önce tuvalde görünür ve hepsi tek geri alma
  adımıdır; hiçbir sınıra ulaşamayan uçların nesneleri atlanır ve sayısı söylenir.
- **Sınırları uzatarak** (`uzanti=evet`): ucun yoluna yetişmeyen bir sınır kendi
  yolunda uzatılmış sayılır; uç, sınırın o uzantısına kadar gider. Sınırın kendisi
  değişmez.

## Adlar

| Ad | Tür |
|---|---|
| `UZAT` | Türkçe, birincil |
| `EXTEND` | İngilizce karşılık |
| `UZ` | Kısaltma |
| `core.extend` | Komut kimliği |

## Sözdizimi

```text
UZAT [sinir=<k> …] [hepsi=evet] [uzanti=evet] [nesne=<k> …] nokta=<n> [<n> …]
UZAT [sinir=<k> …] [hepsi=evet] [uzanti=evet] [yontem=çit] cit=<n> <n> [<n> …]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nokta` | Uzatılacak her ucun yakınında bir nokta; birden çok verilebilir, sırayla işlenir |
| `nesne` | Her noktanın uzattığı nesne, aynı sırayla; verilmezse noktanın altındaki nesne |
| `sinir` | Ulaşılacak sınırlar; verilmezse seçim, o da yoksa yakındaki her nesne |
| `hepsi` | `evet`: tıklanan nesnenin yakınındaki her nesne sınırdır |
| `yontem` | `tıkla` (öntanımlı) ya da `çit`: uçlar çizilen bir çitle gösterilir |
| `cit` | Çitin köşeleri, en az iki; çitin geçtiği her nesnenin çite yakın ucu uzatılır |
| `uzanti` | `evet`: sınırlar kendi yolunda uzatılmış sayılır; yetişmeyen bir sınıra da ulaşılır |

## Örnekler

### Komut satırı

`1` çizgisini `2` sınırına kadar uzatmak — nokta, uzatılacak uca yakın:

```text
UZAT nesne=1 sinir=2 nokta=50,0
```

```text
1 uç sınıra uzatıldı.
```

Doğudan kuzeye çeyrek bir yayın (`1`) ucunu, çemberi boyunca x = −5'teki çizgiye
(`2`) taşımak; yay 120°'de, (−5; 8,660) noktasında durur:

```text
UZAT nesne=1 sinir=2 nokta=1,9.9
```

```text
1 uç sınıra uzatıldı.
```

İki çizginin (`1`, `2`) sağ uçlarını tek çitle `3` sınırına uzatmak:

```text
UZAT sinir=3 cit=35,-5 45,15
```

```text
2 uç sınıra uzatıldı (2 nesnede).
```

Ucun yoluna yetişmeyen bir sınıra (`4`, y = 0'dan yukarı gider) uzatmak — `3`
çizgisi y = −10 boyunca uzanır:

```text
UZAT uzanti=evet sinir=4 nesne=3 nokta=10,-10
```

```text
1 uç sınıra uzatıldı.
```

### Elips yayı

Boş bir çizimde, elipsin dörtte biri olan bir yay ile x = 55 doğrusu: yayın (60; 5)'teki
ucu elipsi boyunca doğruya kadar ilerler ve elipsin 120°'sinde, (55; 4,330)'da durur.

<!-- örnek: yeni çizim -->

```
ELİPS merkez=60,0 birinci=70,0 ikinci=60,5 baslangic=0 bitis=90
ÇİZGİ 55,-10 55,10
UZAT sinir=2 nesne=1 nokta=60,5
```

### Arayüz

Şeritte **Değiştir ▸ Kes ve Uzat ▸ Uzat**'a basın ya da `UZAT` yazın. Komut satırı
`Uzatılacak uca tıklayın — Enter: bitir` der. Önceden nesne seçtiyseniz sınırlar
onlardır; seçim yoksa imlecin altındaki nesnenin yakınındaki her şey sınırdır.

İmleci bir ucun yakınında gezdirin: nesnenin uzatılmış hâli vurgu renginde,
**eklenecek uzantı** sınıra kadar kesikli çizilir. Tıklayın; komut bir sonraki ucu
bekler. Bitince **Enter**'a ya da sağ tuşa basın; **Esc** de o ana kadar
uzatılanları tutarak bitirir.

Tıklama bir konum değil, bir seçimdir: nesne yakalama, ızgara ve dik mod tıklamayı
kaydırmaz.

**Uzat — çitle** ve **Uzat — sınırları uzatarak** **Uzat** düğmesinin okundadır (**Giriş ▸
Değiştir** panelinde **Buda** düğmesinin okunda da). Çitte her köşeden sonra çitin şimdiye kadar
uzatacağı bütün uçlar kesikli görünür; **Enter** uygular.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.line",
      "args": { "noktalar": [[485300000, 4310200000], [485350000, 4310200000]] } },
    { "cmd": "core.line",
      "args": { "noktalar": [[485380000, 4310190000], [485380000, 4310210000]] } },
    { "cmd": "core.extend",
      "args": { "sinir": [2], "nesne": [1], "nokta": [[485350000, 4310200000]] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

Bir `UZAT` çalışmasının bütün tıklamaları — ya da çitin uzattığı bütün uçlar —
**tek** geri alma adımıdır; [`GERİAL`](undo.md) hepsini birden geri alır.

## Betikten kullanım

Betikten `nokta` verilmelidir. `nesne` verilmezse her nokta **tam altındaki**
nesneyi uzatır — betiğin seçim açıklığı yoktur; güvenli yol `nesne`'yi vermektir.
Günlüğe sınırlar (ya da `hepsi`), uzatılan nesneler ve noktalar yazılır; çitle
yapılan uzatmada çit (`yontem=çit`, `cit`). Python'dan
`cad.extend(boundary=[2], object=[1], point=[[485350000, 4310200000]])` olarak
çağrılır; çitle `fence=[[…], […]]`, sınırları uzatarak `carry_edges=True`.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bu uç, sınırlara uzatılarak ulaşamıyor: kesişme yok.` | Çizginin doğrultusu hiçbir sınırı kesmiyor | Doğrultunun kestiği bir sınır verin |
| `Bu yayın ucu, sınırlara çemberi boyunca uzatılarak ulaşamıyor: kesişme yok.` | Yayın çemberi, uzatılabileceği bölümde hiçbir sınırı kesmiyor | Çemberin kestiği bir sınır verin |
| `Bu elips yayının ucu, sınırlara elipsi boyunca uzatılarak ulaşamıyor: kesişme yok.` | Elips, yayın uzatılabileceği bölümde hiçbir sınırı kesmiyor | Elipsin kestiği bir sınır verin |
| `Spline'ın ucu uzatılamaz: eğri son düğümünde biter, ötesi tanımsızdır. Ucundan sınıra bir çizgi çizin.` | Bir spline'ın ucu tıklandı | Spline'ın ucundan sınıra bir çizgi çizin |
| `Kapalı bir şeklin ucu yok; uzatılacak bir şey yok.` | Daire ya da tam elips tıklandı | Açık bir çizgi, yay ya da elips yayı seçin |
| `Nesne N kapalı bir alan; ucu olmayan bir şekil uzatılmaz.` | Kapalı alan tıklandı | Açık bir çizgi seçin |
| `Tıklanan yerde uzatılacak bir nesne yok. Bir çizginin, yayın ya da elips yayının ucuna tıklayın.` | Tıklamanın altında nesne yok; betikte nokta nesnenin tam üstünde değil | Nesnenin üstüne tıklayın ya da `nesne=` verin |
| `Nesne N bu komutun işleyebileceği bir tür değil; UZAT çizgi, yay, daire, elips ve spline'da çalışır.` | Nokta, metin, blok ya da delikli alan | Çizgi, yay ya da elips yayı seçin |
| `UZAT için ulaşılacak sınır yok: tıklanan nesnenin yakınında başka bir çizgi, yay, daire, elips ya da spline yok.` | Yakında ulaşılacak nesne yok | Bir sınır çizin ya da `sinir=` verin |
| `UZAT için ulaşılacak sınır yok: sınır olarak verilen nesneler tıklanan nesnenin kendisi ya da çizgi, yay, daire, elips veya spline değil.` | Verilen tek sınır, uzatılan nesnenin kendisi | Başka bir nesneyi sınır seçin |
| `UZAT: hiçbir uç gösterilmedi. Uzatılacak uca tıklayın ya da UZAT nokta=<nokta> yazın.` | Hiç tıklamadan Enter; betikte `nokta` yok | Bir uca tıklayın ya da `nokta` verin |
| `UZAT: çit en az iki noktadan oluşur. Çitin köşelerini tıklayın ya da UZAT cit=<nokta> <nokta> yazın.` | Çitin tek köşesi var | Çite en az bir köşe daha verin |
| `Çit, sınırlara uzatılabilecek bir uçtan geçmiyor.` | Çit hiçbir nesneden geçmiyor ya da geçtiği nesnelerin hiçbir ucu bir sınıra ulaşmıyor | Çiti uzatılacak uçların yakınından çizin |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | `nesne` ile verilen kimlik yok | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Sınır nesnesi bulunamadı veya silinmiş: N` | `sinir` ile verilen kimlik yok | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `'K' katmanı kilitli; üzerindeki nesne düzenlenemez. Kilidi KATMAN ad=K kilitli=hayır ile açın.` | Nesne kilitli bir katmanda | Katmanın kilidini açın |

## İlgili

- [`BUDA`](trim.md) — uzatmak yerine sınırda keser
- [`UZUNLUK`](lengthen.md) — bir sınıra değil, verilen bir boya uzatır
