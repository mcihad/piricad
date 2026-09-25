# SINIR — Kapalı Bölgenin Sınırını Çıkarma

Parselleri, yapı adalarını ya da yeşil alanları ayrı ayrı çizgiler hâlinde alan —
bir DXF'ten, total station'dan ya da eski bir paftadan — ve onları gerçek alanlar
olarak kullanmak isteyen herkes için; bu sayfayı bitirdiğinizde kapalı bir bölgenin
içine tıklayarak sınırını arayüzden, komut satırından ve betikten çıkarmayı
bileceksiniz.

## Ne yapar

`SINIR`, verilen noktayı çevreleyen **en küçük kapalı bölgeyi** bulur ve sınırını
**yeni bir nesne** olarak çizer. Bölgeyi kuran çizgiler olduğu gibi kalır.

- Bölge, çizimde görünen bütün çizgilerden bulunur: çizgi, çoklu çizgi, alan, yay,
  daire, yaylı çoklu çizgi; birbirini kesen, T biçiminde birleşen, üst üste binen
  çizgiler dahil. Kesişimler kesin aritmetikle bulunur.
- **İçerideki adalar delik olur.** Bölgenin içinde kalan kapalı bir şekil — bir havuz,
  bir bina — alanın deliği olarak yazılır ve alandan düşülür. `ada=hayır` adaları yok
  sayar. Bir adanın içine tıklarsanız adanın kendisi çıkar.
- **Hiçbir boşluk kendiliğinden kapanmaz.** Birbirine projenin düğüm toleransından
  (`core.topoloji.dugum_toleransi`, varsayılan 1 cm) yakın iki uç aynı nokta sayılır;
  bir kenara bu kadar yakın bir uç o kenarda buluşur. Daha uzak her uç **açık uçtur**:
  bölge kapanmıyorsa komut hiçbir şey çizmez, açık uçları tuvalde işaretler ve en yakın
  çizgiye uzaklıklarını yazar. Boşluğu köprülemek isterseniz `bosluk=` ile açıkça
  söylersiniz; atılan her köprü sonuçta sayılır.
- **Yaylar yay kalır.** Yalnız düz kenarlı bölge **alan** olur; yayla sınırlanan ve
  deliği olmayan bölge **yaylı çoklu çizgi**, yaylar çizimdeki merkez ve yarıçaplarıyla;
  tam bir dairenin içi **daire** olur. Hem yay hem delik taşıyan bölge — tek bir nesne
  türünün taşıyamadığı biçim — alan olarak yazılır, yaylar kirişlerle, ve sapma
  söylenir.
- Elips ve spline çizildiği hâliyle izlenir; sonuçta bu ve sapması söylenir.
- Gizli katmanlardaki çizgiler, yazılar, ölçüler, taramalar ve blok içleri sınır
  sayılmaz.

Yeni nesne etkin katmana çizilir.

## Adlar

| Ad | Tür |
|---|---|
| `SINIR` | Türkçe, birincil |
| `BOUNDARY` | İngilizce karşılık |
| `SNR` | Kısaltma |
| `core.boundary` | Komut kimliği |

## Sözdizimi

```text
SINIR [nokta=<n>] [ada=evet|hayır] [bosluk=<mm>] [nesneler=<kimlik> …]
```

`nokta` verilmezse arayüz sorar.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nokta` | Sınırı çıkarılacak bölgenin içindeki nokta |
| `ada` | İçerideki kapalı çizgiler delik olsun mu. Varsayılan `evet` |
| `bosluk` | Bu genişliğe kadar açık uçları köprüler, **milimetre**. Varsayılan `0`: köprü yok |
| `nesneler` | Sınır sayılacak nesneler. Verilmezse görünen her çizgi sayılır |

## Örnekler

### Komut satırı

Yeni bir çizimde, dört ayrı çizgiden oluşan 20 × 10 m'lik bir parsel ve içinde 2 m
yarıçaplı bir havuz; parselin içine tıklamak:

<!-- örnek: yeni çizim -->
```
ÇİZGİ 0,0 20,0
ÇİZGİ 20,0 20,10
ÇİZGİ 20,10 0,10
ÇİZGİ 0,10 0,0
DAİRE 10,5 12,5
SINIR nokta=3,3
```

```text
Sınır çıkarıldı: 187,43 m² delikli alan (dış sınır 200,00 m², 1 ada 12,57 m²); 5 nesnenin çizgisinden. Delikli bir alan yay taşıyamadığı için yaylar kirişlerle yazıldı (sapma ≤ 1 cm).
```

Aynı noktada havuzu yok saymak:

```
SINIR nokta=3,3 ada=hayır
```

Yeni bir çizimde, köşesine 1,5 m varmayan bir kenar. Bölge kapanmaz; komut hiçbir
şey çizmez ve nedenini söyler:

```text
ÇİZGİ 0,0 20,0
ÇİZGİ 20,0 20,10
ÇİZGİ 20,10 0,10
ÇİZGİ 0,10 0,1.5
SINIR nokta=5,5
```

```text
Hata: Bu bölge kapanmıyor: 2 açık uç var; tıkladığınız yere en yakını bir çizgiye 1,50 m uzakta. Uçlar tuvalde işaretlendi. Boşluğu yakalamayla kapatın ya da köprülemek için bosluk=<mm> verin.
```

Aynı çizimde boşluğun 2 m'ye kadar köprülenmesini istemek:

<!-- örnek: yeni çizim -->
```
ÇİZGİ 0,0 20,0
ÇİZGİ 20,0 20,10
ÇİZGİ 20,10 0,10
ÇİZGİ 0,10 0,1.5
SINIR nokta=5,5 bosluk=2000
```

```text
Sınır çıkarıldı: 200,00 m² alan; 4 nesnenin çizgisinden. 1 boşluk köprülendi: 1,50 m.
```

Yazılan bir nokta yazıldığı yere düşer; yakalama yalnız fareyle nişan alınan noktaya
uygulanır. Küçük bir boşluğu fareyle çizerek denemek istiyorsanız önce yakınlaşın:
yakınlaşma uzaksa, bir köşeye birkaç santimetre kala tıklanan uç o köşeye oturur.

### Arayüz

Şeritte **Çizim ▸ Tarama ▸ Sınır Bul**'a basın ya da **Giriş ▸ Çizim** panelindeki
**Tarama** düğmesinin okundan **Sınır Bul**'u seçin (bir tarama seçiliyken beliren
**Tarama** sekmesinde de vardır).

1. İmleci bir bölgenin içine götürün. İmlecin bulunduğu bölge vurgulanır, adaları
   delik olarak boş bırakılır ve alanı imlecin yanında yazar — tıklamanın çizeceği
   şey budur.
2. Bölge kapanmıyorsa vurgu yerine açık uçlar turuncu halkalarla, en yakın çizgiye
   giden kesik çizgiler ve boşluk uzaklıklarıyla gösterilir.
3. Tıklayın. Araç açık kalır: sıradaki bölgeye tıklayarak devam edebilirsiniz, Esc
   bırakır.

Önizleme ekranda görünen çizgilerle çalışır; ekrandan büyük bir bölgeyi komut yine
bulur, önizleme onu tahmin etmez.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.line", "args": { "noktalar": [[0, 0], [20000, 0]] } },
    { "cmd": "core.line", "args": { "noktalar": [[20000, 0], [20000, 10000]] } },
    { "cmd": "core.line", "args": { "noktalar": [[20000, 10000], [0, 10000]] } },
    { "cmd": "core.line", "args": { "noktalar": [[0, 10000], [0, 0]] } },
    { "cmd": "core.circle_draw",
      "args": { "merkez": [10000, 5000], "cevre": [12000, 5000] } },
    { "cmd": "core.boundary", "args": { "nokta": [3000, 3000] } }
  ]
}
```

Betikte koordinatlar ve `bosluk` **milimetredir** (`3000` = 3 m).

## Geri alma

`SINIR` tek bir geri alma adımıdır. [`GERİAL`](undo.md) çıkarılan nesneyi kaldırır;
çizgiler zaten değişmemiştir.

## Betikten kullanım

Betikten çağrıldığında komut hiçbir şey sormaz: `nokta` verilmelidir. Günlüğe
`nokta` ve verilen seçenekler yazılır; oynatıldığında aynı çizimden aynı sınır
çıkar, çünkü bölge görünümden değil çizimden bulunur. Komut, yapay zekâ ve betik
istemcilerine şu raporu da döndürür: `nesne` (yeni nesnenin kimliği), `alan_mm2`,
`dis_alan_mm2`, `ada`, `kaynaklar` (sınırı çizen nesnelerin kimlikleri), `kopru`,
`birlesen`.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bu bölge kapanmıyor: N açık uç var; …` | Bölgeyi çevreleyen çizgilerde düğüm toleransından geniş boşluk var | Boşluğu yakalamayla kapatın ya da `bosluk=<mm>` verin |
| `Bu noktayı çevreleyen kapalı bir çizgi yok. …` | Nokta hiçbir kapalı bölgenin içinde değil | Bölgenin içine tıklayın; gizli katmanları açın |
| `Nokta bir çizginin üstünde; …` | Tıklama tam bir çizginin üzerine düştü | Bölgenin içine, çizgiden uzağa tıklayın |
| `Köprülenecek boşluk eksi olamaz; …` | `bosluk` eksi verildi | 0 ya da daha büyük bir milimetre değeri verin |
| `Nesne bulunamadı veya silinmiş: N` | `nesneler` içinde olmayan bir kimlik var | Kimlikleri denetleyin |
| `Bu derleme CGAL olmadan yapıldı; …` | Program CGAL kütüphanesi olmadan derlenmiş | CGAL'ı kurup `KENTOS_WITH_CGAL=ON` ile derleyin |

## İlgili

- [`ALANAÇEVİR`](to_area.md) — uç uca değen çizgileri, onları silerek tek alana çevirir
- [`ALAN`](area.md) — köşeleri vererek alan çizer
- [`TARAMA`](hatch.md) — kapalı nesnenin içini tarar
- [`TOPOLOJİ`](topology.md) — parsel kusurlarını denetler
- [`GERİAL`](undo.md)
