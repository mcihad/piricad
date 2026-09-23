# UZAT — Ucu Sınıra Kadar Uzatma

Sınıra yetişmeyen bir çizgiyi ya da yayı oraya kadar uzatması gereken herkes için;
bu sayfayı bitirdiğinizde uzatmayı arayüzden, komut satırından ve betikten yapmayı
bileceksiniz.

## Ne yapar

`UZAT`, tıkladığınız yere **yakın olan ucu**, kendi yolunda ilerleterek ulaştığı
ilk sınıra kadar taşır:

- Bir **çizginin** ucu kendi doğrultusunda ilerler; çizginin yönü değişmez.
- Bir **yayın** ucu kendi çemberi boyunca ilerler; merkez ve yarıçap aynı kalır.
  Yay, kendi üstüne dolanacak kadar uzatılmaz.

Ulaşılan nokta sınırın **üzerindedir**: bir yaya ya da daireye uzatılan çizgi,
eğrinin gerçek kesişiminde durur, çizildiği kirişlerde değil.

`UZAT` açık çizgilerde ve yaylarda çalışır. Daire ve kapalı alanın ucu yoktur, bu
yüzden uzatılmazlar. Elips ve spline bugün uzatılmaz; [`BUDA`](trim.md)
sayfasındaki sebeple, yinelemeli çözümü getirecek kütüphaneyle (TODOS C-01)
uzatılabilecekler.

### Sınırlar

Sınırlar [`BUDA`](trim.md)'daki sırayla belirlenir: `sinir=` ile verilenler, yoksa
seçtikleriniz, o da yoksa tıkladığınız nesnenin yakınındaki her görünür çizgi, yay
ve daire (`hepsi=evet`).

## Adlar

| Ad | Tür |
|---|---|
| `UZAT` | Türkçe, birincil |
| `EXTEND` | İngilizce karşılık |
| `UZ` | Kısaltma |
| `core.extend` | Komut kimliği |

## Sözdizimi

```text
UZAT [sinir=<k> …] [hepsi=evet] [nesne=<k> …] nokta=<n> [<n> …]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nokta` | Uzatılacak her ucun yakınında bir nokta; birden çok verilebilir, sırayla işlenir |
| `nesne` | Her noktanın uzattığı nesne, aynı sırayla; verilmezse noktanın altındaki nesne |
| `sinir` | Ulaşılacak sınırlar; verilmezse seçim, o da yoksa yakındaki her nesne |
| `hepsi` | `evet`: tıklanan nesnenin yakınındaki her nesne sınırdır |

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

### Arayüz

Sol araç kutusundaki **Uzat** düğmesine basın ya da `UZAT` yazın. Komut satırı
`Uzatılacak uca tıklayın — Enter: bitir` der. Önceden nesne seçtiyseniz sınırlar
onlardır; seçim yoksa imlecin altındaki nesnenin yakınındaki her şey sınırdır.

İmleci bir ucun yakınında gezdirin: nesnenin uzatılmış hâli vurgu renginde,
**eklenecek uzantı** sınıra kadar kesikli çizilir. Tıklayın; komut bir sonraki ucu
bekler. Bitince **Enter**'a ya da sağ tuşa basın; **Esc** de o ana kadar
uzatılanları tutarak bitirir.

Tıklama bir konum değil, bir seçimdir: nesne yakalama, ızgara ve dik mod tıklamayı
kaydırmaz.

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

Bir `UZAT` çalışmasının bütün tıklamaları **tek** geri alma adımıdır;
[`GERİAL`](undo.md) hepsini birden geri alır.

## Betikten kullanım

Betikten `nokta` verilmelidir. `nesne` verilmezse her nokta **tam altındaki**
nesneyi uzatır — betiğin seçim açıklığı yoktur; güvenli yol `nesne`'yi vermektir.
Günlüğe sınırlar (ya da `hepsi`), uzatılan nesneler ve noktalar yazılır. Python'dan
`cad.extend(boundary=[2], object=[1], point=[[485350000, 4310200000]])` olarak
çağrılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bu uç, sınırlara uzatılarak ulaşamıyor: kesişme yok.` | Çizginin doğrultusu hiçbir sınırı kesmiyor | Doğrultunun kestiği bir sınır verin |
| `Bu yayın ucu, sınırlara çemberi boyunca uzatılarak ulaşamıyor: kesişme yok.` | Yayın çemberi, uzatılabileceği bölümde hiçbir sınırı kesmiyor | Çemberin kestiği bir sınır verin |
| `Kapalı bir şeklin ucu yok; uzatılacak bir şey yok.` | Daire tıklandı | Açık bir çizgi ya da yay seçin |
| `Nesne N kapalı bir alan; ucu olmayan bir şekil uzatılmaz.` | Kapalı alan tıklandı | Açık bir çizgi seçin |
| `Tıklanan yerde uzatılacak bir nesne yok. Bir çizginin ya da yayın ucuna tıklayın.` | Tıklamanın altında nesne yok; betikte nokta nesnenin tam üstünde değil | Nesnenin üstüne tıklayın ya da `nesne=` verin |
| `Nesne N bu komutun işleyebileceği bir tür değil; UZAT çizgi, yay ve dairelerde çalışır.` | Elips, spline, nokta, metin ya da delikli alan | Çizgi ya da yay seçin |
| `UZAT için ulaşılacak sınır yok: tıklanan nesnenin yakınında başka bir çizgi, yay ya da daire yok.` | Yakında ulaşılacak nesne yok | Bir sınır çizin ya da `sinir=` verin |
| `UZAT için ulaşılacak sınır yok: sınır olarak verilen nesneler tıklanan nesnenin kendisi ya da çizgi, yay veya daire değil.` | Verilen tek sınır, uzatılan nesnenin kendisi | Başka bir nesneyi sınır seçin |
| `UZAT: hiçbir uç gösterilmedi. Uzatılacak uca tıklayın ya da UZAT nokta=<nokta> yazın.` | Hiç tıklamadan Enter; betikte `nokta` yok | Bir uca tıklayın ya da `nokta` verin |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | `nesne` ile verilen kimlik yok | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Sınır nesnesi bulunamadı veya silinmiş: N` | `sinir` ile verilen kimlik yok | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `'K' katmanı kilitli; üzerindeki nesne düzenlenemez. Kilidi KATMAN ad=K kilitli=hayır ile açın.` | Nesne kilitli bir katmanda | Katmanın kilidini açın |

## İlgili

- [`BUDA`](trim.md) — uzatmak yerine sınırda keser
- [`UZUNLUK`](lengthen.md) — bir sınıra değil, verilen bir boya uzatır
