# ALANÜRET — Çizgilerden Alan Üretme

Parselleri, adaları ya da imar bölgelerini yalnız sınır çizgileri olarak alan — bir
DXF'ten, sayısallaştırmadan ya da eski bir paftadan — ve her birini tek tek gerçek bir
alana çevirmek isteyen herkes için; bu sayfayı bitirdiğinizde bir çizgi ağının kapattığı
bütün gözleri tek seferde Araçlar panelinden, komut satırından ve betikten alana
çevirmeyi bileceksiniz.

## Ne yapar

`ALANÜRET`, kapsamdaki çizgilerin kapattığı **her gözü** ayrı bir **alan** olarak
çizer. Çizgiler olduğu gibi kalır; alanlar yeni nesnelerdir ve çıktı katmanına yazılır.

- Çizgiler birbirini kessin, T biçiminde birleşsin, üst üste binsin, köşeleri aşsın:
  ağ kesin aritmetikle düğümlenir, her göz bulunur. Köşeyi aşan uçlar bir şeyi bozmaz.
- **Bir gözün içindeki kapalı şekil o alanın deliği olur** ve kendisi de ayrı bir alan
  olarak çizilir: parselin içindeki havuz, parselde delik ve ayrıca kendi alanı olur.
  `ada=hayır` delikleri açmaz; göz dış sınırıyla dolu çizilir.
- **Hiçbir boşluk kendiliğinden kapanmaz.** Birbirine projenin düğüm toleransından
  (`core.topoloji.dugum_toleransi`, varsayılan 1 cm) yakın uçlar aynı nokta sayılır; daha
  uzak her uç açık uçtur. Açık uçların bir çizgiye yakın olanları sayılır, en dar
  boşluk söylenir ve uçlar tuvalde turuncu işaretlenir; kapanmayan göz alan olmaz.
  Boşlukları köprülemek isterseniz `bosluk=` ile açıkça söylersiniz; atılan her köprü
  sayılır. Bir ucun kendi çizgisi ya da az önce kestiği çizgi boşluk sayılmaz.
- **Yaylar yay kalır.** Yalnız düz kenarlı göz alan, yayla kapanan ve deliği olmayan göz
  **yaylı çoklu çizgi**, dairenin içi **daire** olur. Hem yay hem delik taşıyan göz alan
  olarak yazılır, yaylar kirişlerle, ve sapma söylenir. Elips ve spline çizildiği
  hâliyle izlenir.

Aynı çekirdeği [`SINIR`](boundary.md) kullanır: bir gözün içine tıklayıp SINIR ile
çıkarılan alan, ALANÜRET'in aynı göz için ürettiği alanla köşe köşe aynıdır.

Bu bir [işlem aracıdır](../islem/README.md): kapsam, asenkron çalışma, Durdur ve tek
geri alma adımı orada anlatılır.

## Adlar

| Ad | Tür |
|---|---|
| `ALANÜRET` | Türkçe, birincil |
| `ALANURET` | ASCII karşılık |
| `POLYGONIZE` | İngilizce karşılık |
| `ALÜ` | Kısaltma |
| `islem.alan_uret` | Komut kimliği |

## Sözdizimi

```text
ALANÜRET [nesneler=<k>] [kapsam=secili|gorunum|proje] [ada=evet|hayır]
         [bosluk=<metre>] [katman=<ad>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Ağı kuran nesnelerin kimlikleri; verilmezse `kapsam` |
| `kapsam` | `secili` (varsayılan), `gorunum`, `proje` |
| `pencere` | `gorunum` için görünümün iki köşesi |
| `katman` | Alanların yazılacağı katman; yoksa oluşturulur; boşsa etkin katman |
| `ada` | Bir gözün içindeki kapalı çizgiler o alanın deliği olsun (öntanımlı `evet`) |
| `bosluk` | Bu genişliğe kadar açık uçları köprüler, **metre**. Öntanımlı `0`: köprü yok |

## Örnekler

### Komut satırı

Yeni bir çizimde, köşeleri aşan beş çizgiden iki parsel; ikisi de alana:

<!-- örnek: yeni çizim -->
```
ÇİZGİ -1,0 21,0
ÇİZGİ -1,10 21,10
ÇİZGİ 0,-1 0,11
ÇİZGİ 10,-1 10,11
ÇİZGİ 20,-1 20,11
ALANÜRET kapsam=proje katman=PARSEL
```

```text
Çizgilerden alan üret: 5 nesneye uygulandı, 2 nesne üretildi (katman: PARSEL).
  not: 2 kapalı göz bulundu, toplam alan 200,00 m².
```

Yeni bir çizimde, köşesine 5 cm varmayan bir kenar. Göz kapanmaz, alan üretilmez ve
boşluk söylenir; sonra 10 cm'ye kadar köprülemek:

<!-- örnek: yeni çizim -->
```
ÇİZGİ 0,0 20,0
ÇİZGİ 20,0 20,10
ÇİZGİ 20,10 0,10
ÇİZGİ 0,10 0,0.05
ALANÜRET kapsam=proje katman=DENEME
ALANÜRET kapsam=proje bosluk=0.1 katman=PARSEL
```

```text
Çizgilerden alan üret: 4 nesneye uygulandı, 0 nesne üretildi (katman: DENEME).
  not: Çizgiler kapalı bir göz oluşturmuyor.
  not: 2 açık uç bir çizgiye yakın ama değmiyor; en dar boşluk 5 cm. Uçlar tuvalde işaretlendi; kapanmayan göz alan olmadı. Köprülemek için bosluk=<metre> verin.
Çizgilerden alan üret: 4 nesneye uygulandı, 1 nesne üretildi (katman: PARSEL).
  not: 1 kapalı göz bulundu, toplam alan 200,00 m².
  not: 1 boşluk köprülendi: 5 cm.
```

### Arayüz

Şeritte **Analiz ▸ İşlem araçları ▸ Alan Üret**'e (ya da **Kadastro ▸ Yazım ▸ Alan Üret**'e)
basın ya da sağ paneldeki **Araçlar** sekmesinde **Geometri ▸ Çizgilerden alan üret**'i
seçin. Kartta kapsamı (Seçili · Görünüm · Proje)
seçin, çıktı katmanını yazın ve **Çalıştır**'a basın. Kapsam seçiliyse ve seçim boşsa
araç tuvalden seçtirir: her tık ekler, **sağ tık** başlatır. Kapanmayan gözlerin açık
uçları iş bitince tuvalde turuncu halkalarla ve boşluk uzaklıklarıyla gösterilir.

### Betik

```json
{
  "ad": "Parselleri alana çevir",
  "komutlar": [
    { "cmd": "core.line", "args": { "noktalar": [[-1000, 0], [21000, 0]] } },
    { "cmd": "core.line", "args": { "noktalar": [[-1000, 10000], [21000, 10000]] } },
    { "cmd": "core.line", "args": { "noktalar": [[0, -1000], [0, 11000]] } },
    { "cmd": "core.line", "args": { "noktalar": [[10000, -1000], [10000, 11000]] } },
    { "cmd": "core.line", "args": { "noktalar": [[20000, -1000], [20000, 11000]] } },
    { "cmd": "islem.alan_uret",
      "args": { "nesneler": [1, 2, 3, 4, 5], "katman": "PARSEL" } }
  ]
}
```

Betikte koordinatlar milimetre, `bosluk` metredir.

## Geri alma

Tek adımdır: bir `ALANÜRET` kaç alan çizmiş olursa olsun tek [`GERİAL`](undo.md) hepsini
kaldırır; çizgiler zaten değişmemiştir.

## Betikten kullanım

Betikte `nesneler` ya da `kapsam` verilmelidir, çünkü betik çalışırken "etkin seçim"
olmayabilir. Komut günlüğüne uygulanan nesnelerin kimlikleri ve her parametre yazılır;
yeniden oynatılan satır aynı alanları çizer. Python'dan `cad.polygonize(objects=[1, 2],
islands=True)` olarak çağrılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Çizgiler kapalı bir göz oluşturmuyor.` (not) | Kapsamdaki çizgiler hiçbir yeri tam kapatmıyor | Açık uçları kapatın ya da `bosluk=` verin |
| `N açık uç bir çizgiye yakın ama değmiyor; …` (not) | Bazı uçlar düğüm toleransından uzak | Uçları yakalamayla birleştirin ya da `bosluk=` ile köprüleyin |
| `Köprülenecek boşluk eksi olamaz; …` | `bosluk` eksi verildi | 0 ya da daha büyük bir metre değeri verin |
| `Kapsamda bu araca uygun nesne yok (N nesne bakıldı). …` | Kapsamda yalnız noktalar ya da yazılar var | Çizgi, alan ya da eğri seçin |
| `Bu derleme CGAL olmadan yapıldı; …` | Program CGAL kütüphanesi olmadan derlenmiş | CGAL'ı kurup `KENTOS_WITH_CGAL=ON` ile derleyin |

## İlgili

- [`SINIR`](boundary.md) — tek bir gözün sınırını içine tıklayarak çıkarır
- [`ALANAÇEVİR`](to_area.md) — uç uca değen çizgileri, onları silerek tek alana çevirir
- [İşlem araçları](../islem/README.md) — kapsam, Durdur, çıktı katmanı
