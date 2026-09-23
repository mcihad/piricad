# ESNET — Esnetme

Bir yol genişlediğinde kenarındaki parsellerin bir tarafını çekip öbür tarafını
tapudaki yerinde bırakmak zorunda olan herkes için; bu sayfayı bitirdiğinizde bir
pencerenin içindeki köşeleri taşımayı, dışındakileri yerinde bırakmayı arayüzden,
komut satırından ve betikten bileceksiniz.

## Ne yapar

`ESNET`, bir **pencere** ve bir **öteleme** alır. Pencerenin içinde kalan her köşe
ötelenir, dışında kalan her köşe yerinde durur. Aradaki kenarlar da onlarla birlikte
uzar, kısalır ya da eğrilir.

Diğer iki taşıma fiilinin arasındaki boşluk buydu:

| Komut | Neyi taşır |
|---|---|
| [`TAŞI`](move.md) | Nesnenin **hepsini** |
| `ESNET` | Nesnenin **pencereye giren kısmını** |
| [`KÖŞETAŞI`](vertex_move.md) | **Adı verilen tek** köşeyi |

### Pencere bir seçim değildir, bir süzgeçtir

Bu komut planda ertelenmişti ve sebebi şuydu: bir **köşe** seçmek, bu programda
olmayan bir altyapı gerektiriyor — seçim nesne düzeyinde çalışır, köşe düzeyinde
değil. Ama pencere zaten süzgecin kendisidir: içindeki köşe gider, dışındaki kalır,
hiçbir köşesi içinde olmayan nesneye hiç dokunulmaz. Bunun için hiçbir şeyin
seçilebilir olması gerekmiyor.

Pencere **kesen** penceredir: uzak sınırı yerinde kalacak bir parsel de adaydır.
Bütünüyle içeride olmasını isteyen bir pencere, geride bırakacak hiçbir köşe
bulamazdı.

### Eğriler ve tanım noktaları

Bir çokluçizginin köşeleri onun **şeklidir**; bir dairenin, yayın, elipsin, ölçünün
ve blok referansının sakladığı noktalar ise bir **tanımdır**. İkisi ayrı yollardan
gider:

- **Çokluçizgi** (ve `ALAN`): pencereye giren bütün köşeler **tek yazımda** taşınır.
  Yarısı taşınmış bir halka hiçbir zaman denetleyiciye sunulmaz.
- **Diğer her tür**: her tutamak, o türün tutamağının ne anlama geldiğini bilen tek
  yerden geçer ([`KÖŞETAŞI`](vertex_move.md) ile aynı tablo).

**Her hedef, hiçbir şey kıpırdamadan önce hesaplanır.** Bir dairenin tutamakları
merkezi ve bir yarıçap kolundan ibarettir; merkezi taşımak kolu da beraberinde
taşır. Hedefler canlı geometriye göre hesaplansaydı kol iki kez taşınır ve daire
öteleme kadar büyürdü. Anlık görüntüye göre hesaplandığında ikinci taşıma kolu
zaten bulunduğu yere koyar ve hiçbir şey değişmez — yani penceresine tamamen giren
bir daire **ötelenir**, büyümez. Yalnız yarıçap kolu pencereye girerse daire
**yeniden boyutlanır**; bir tanımın esnetilmesi tam olarak budur.

## Adlar

| Ad | Tür |
|---|---|
| `ESNET` | Türkçe, birincil |
| `STRETCH` | İngilizce karşılık |
| `ES` | Kısaltma |
| `core.stretch` | Komut kimliği |

## Sözdizimi

```text
ESNET
ESNET pencere=<n1> pencere=<n2> baslangic=<n> bitis=<n>
ESNET nesneler=<kimlik> pencere=<n1> pencere=<n2> baslangic=<n> bitis=<n>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `pencere` | Pencerenin iki köşesi. İçinde kalan köşeler taşınır. Alanı olmalıdır |
| `baslangic` | Ötelemenin başlangıç noktası |
| `bitis` | Ötelemenin bitiş noktası. Öteleme, bu ikisinin farkıdır |
| `nesneler` | Yalnız bu nesneler esnetilir. Verilmezse pencerenin dokunduğu her nesne |

## Örnekler

### Komut satırı — bir parselin sağ kenarını 5 m çekmek

```text
ALAN 0,0 20,0 20,10 0,10
ESNET pencere=15,-5 pencere=25,15 baslangic=0,0 bitis=5,0
```

Yazılan:

```text
2 köşe esnetildi (1 nesne).
```

Parsel artık `0,0 25,0 25,10 0,10`: sağdaki iki köşe 5 m doğuya gitti, soldaki ikisi
yerinde kaldı.

### Arayüz

**Değiştir → Esnet**, ya da araç kolonundaki değiştirme ailesinde **Esnet**.
Pencerenin bir köşesine, sonra karşı köşesine tıklarsınız (arada lastik dikdörtgen
görünür); sonra ötelemenin başlangıç ve bitiş noktasına. Yakalama açıktır: öteleme
noktalarını mevcut köşelere `UÇ` ile yakalayabilirsiniz.

Başlangıç noktası sorulurken **pencere tuvalde kalır**; hangi köşelerin gideceğini
o belirler. Bitiş noktası sorulurken pencerenin yakaladığı her nesne **esnetilmiş
hâliyle** imleci izler: pencere içindeki köşeler imleçle gider, dışındakiler yerinde
kalır. Gördüğünüz, tıklamanın yapacağının kendisidir — aynı hesap.

Dik mod açıkken pencerenin ikinci köşesi ve ötelemenin başlangıç noktası eksene
kilitlenmez (kilitli bir dikdörtgen köşesi eni ya da boyu olmayan bir pencere
olurdu); bitiş noktası başlangıca göre kilitlenir.

### Betik

```json
{
  "ad": "Sağ uçları 5 m doğuya çek",
  "komutlar": [
    { "cmd": "core.area",
      "args": { "noktalar": [[0,0],[20000,0],[20000,10000],[0,10000]] } },
    { "cmd": "core.line", "args": { "noktalar": [[0,-3000],[20000,-3000]] } },
    { "cmd": "core.line", "args": { "noktalar": [[0,13000],[20000,13000]] } },
    { "cmd": "core.stretch", "args": {
        "nesneler": [1, 2, 3],
        "pencere": [[15000, -5000], [25000, 15000]],
        "baslangic": [0, 0], "bitis": [5000, 0] } }
  ]
}
```

### Üçü de aynı

Aynı iş arayüzden, komut satırından ve betikten **özdeş belge** ve **bayt-özdeş
günlük** bırakır; `tests/unit/test_proof.cpp` içindeki
`PROOF: ESNET gui, komut satırı ve betikten aynı belgeyi ve aynı günlüğü bırakır`
vakası bunu kanıtlar.

Günlüğe **çözülmüş nesne kimlikleri** yazılır, pencerenin şansı değil: bir replay,
bu çalıştırmanın esnettiği nesneleri esnetmelidir — oynatıldığı belgede aynı
pencerenin altında başka şeyler durabilir.

## Geri alma

Tek komut, tek geri alma adımı. `Ctrl+Z` bütün köşeleri birlikte geri getirir;
yarısı taşınmış bir parsel diye bir ara durum yoktur (CLAUDE.md 1.6).

Pencereyi verdikten sonra `Esc`'e basarsanız hiçbir şey olmaz: geri alma yığınına
boş bir adım bile bırakmaz.

## Betikten kullanım

`core.stretch` betikten çağrılabilir ve `AiAccessible`'dır. Ajan yolunda noktalar
yalnız araç sonucu tutamağıyla gelir (CLAUDE.md 5.8).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Esnetme penceresi bir çizgi: iki köşe aynı sırada ya da aynı kolonda.` | Pencerenin alanı yok | Alanı olan bir pencere verin |
| `Pencerede esnetilecek köşe yok. Pencere, taşınacak köşelerin üzerinden geçmelidir.` | Pencere hiçbir köşenin üstünden geçmiyor | Pencereyi taşınacak köşeleri kapsayacak şekilde verin |
| `'TAPU' katmanı kilitli; üzerindeki nesne düzenlenemez.` | Nesne kilitli katmanda | `KATMAN ad=TAPU kilitli=hayır` |
| `Pencerede esnetilecek köşe yok, N nesne kilitli katmanda atlandı.` | Penceredeki bütün köşeler kilitli katmandaki nesnelerin | Katmanın kilidini açın ya da pencereyi değiştirin |
| `…, N nesne bu sürümün tanımadığı türde olduğu için olduğu gibi kaldı` | Başka bir programdan gelen, bu sürümün okumadığı türde nesne | Nesne korunur ve çizilir, düzenlenmez; kendi programında düzenleyin |
| Halkanın kendisiyle kesişmesi hakkında bir ileti | Esnetme parseli kendi üzerine katlıyor | Pencereyi ya da ötelemeyi değiştirin; hiçbir köşe taşınmaz |

Kilitli katmandaki nesneler ve bu sürümün tanımadığı türdeki nesneler **atlanır ve
sebebiyle sayılır**: bir sayfa üzerine atılan pencere kaba bir jesttir ve altındaki
tek kilitli parsel bütün esnetmeyi iptal etmek için sebep değildir. Kaç nesnenin
hangi sebeple atlandığı yazılır, çünkü sessiz bir atlama çalışmış gibi görünen bir
esnetmedir.

Bir blok referansı ekleme noktası pencerede ise taşınır; döndürme tutamağı pencereye
girse de blok **dönmez** — esnetme yerleri taşır, açıları değil.

## İlgili sayfalar

- [`TAŞI`](move.md) — nesnenin hepsini taşır
- [`KÖŞETAŞI`](vertex_move.md) — adı verilen tek köşeyi taşır
- [`UZUNLUK`](lengthen.md) — bir ucu boyunca uzatır ya da kısaltır
- [`HİZALA`](align.md) — taşır, döndürür, isterse ölçekler
