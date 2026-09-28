# TAMPON — Tampon Bölge Çizme

Bir derenin 10 m koruma bandını, bir boru hattının 5 m kamulaştırma şeridini ya da bir
kuyunun 50 m koruma alanını çıkaran herkes için; bu sayfayı bitirdiğinizde tampon
bölgeyi Araçlar panelinden, komut satırından ve betikten çizmeyi bileceksiniz.

## Ne yapar

`TAMPON`, kapsamdaki nesnelerin verilen **mesafe içindeki bütün zeminini** yeni bir
**alan** olarak çizer:

| Kaynak | Tamponu |
|---|---|
| Çizgi, çoklu çizgi | Çizginin **iki yanını** saran alan; uçlar yuvarlak, düz ya da kare |
| Nokta | Noktanın çevresinde **yuvarlak bir alan**, sınırı gerçek yay |
| Alan | Dışa doğru büyümüş alan; **delikleri korunur** (delik de o kadar daralır) |
| Daire, elips, kapalı spline | Çizildiği şeklin kendisi ve çevresi — göletin 2 m çevresine gölet de dahildir |
| Yay, açık spline | Çizildiği eğrinin iki yanı |

**Üst üste binen tamponlar tek alan olur** (`birlestir=evet`, öntanımlı): bir koruma
bandı kuralın geçerli olduğu yerdir ve kural iki kez geçerli olmaz. Her nesnenin
tamponunu ayrı istiyorsanız — "her kuyunun 50 m'si" gibi nesne başına bir döküm için —
`birlestir=hayir` verin.

**Eksi mesafe** yalnız alanlarda anlam taşır: alanı içeri doğru **aşındırır**. Alan
mesafenin iki katından darsa geriye bir şey kalmaz; araç bunu söyler ve bir şey çizmez.

**[`OFSET`](offset.md) ile farkı.** OFSET bir **paraleldir**: açık bir çizginin paraleli
tek yanda bir çizgidir, bir alanın paraleli yine bir alandır. TAMPON bir **zemin**
sorusunun cevabıdır ve her zaman alan üretir. İkisi ayrı araçlardır, çünkü ayrı
sorulardır.

Bu bir [işlem aracıdır](../islem/README.md): kapsam, asenkron çalışma, Durdur ve tek
geri alma adımı orada anlatılır. Kaynak nesnelere dokunmaz; tamponlar yeni nesnelerdir.

### Yuvarlak köşe ve uç gerçek yaydır

Öntanımlı `kose=yuvarlak` ve `uc=yuvarlak` ile tamponun her yuvarlak kenarı **gerçek bir
yaydır** ve tampon **yaylı kenarlı alan** olarak çizilir: noktanın çevresi bir daire
alan, çizginin bandının iki ucu yarım daire, alanın büyüyen köşeleri çeyrek daire.
20 m'lik bir çizginin 5 m tamponu 278,539816 m² tutar — 20 × 10 m'lik şerit ve iki
yarım daire, π · 5², milimetre karesine kadar.

Hesap bu durumda [geometri çekirdeğiyle](../veri/geometri-cekirdegi.md) (OpenCASCADE)
yapılır: çizginin her parçasının bandı ayrı alınır ve hepsi birleştirilir; bir çizginin
kendine dönmesi, iki bandın buluşması ve kolları yakın bir U'nun ortasında kalan avlu
doğru çıkar. Üç durum hızlı yoldan (**Clipper2**) çizilir, yuvarlak kenarlar kısa
kenarlarla:

- `kose=koseli`, `kose=pah`, `uc=duz` ya da `uc=kare` verildiğinde;
- kaynak elips ya da spline olduğunda (çizildiği hâliyle alınır);
- kapsamda **256 köşeden uzun** bir çizgi olduğunda — köşe köşe sayısallaştırılmış bir dere
  gibi. Çekirdek her parça için birkaç milisaniye harcar; araç bu durumda hızlı yoldan
  gittiğini söyler.

Yaylı kenarlı bir tampon **içinde boşluk** kalırsa (kolları yakın bir U'nun bandı gibi)
kısa kenarlarla yazılır ve araç bunu söyler: yaylı kenarlı bir nesne tek halka taşır,
boşluğu tutamaz.

## Adlar

| Ad | Tür |
|---|---|
| `TAMPON` | Türkçe, birincil |
| `BUFFER` | İngilizce karşılık |
| `TMP` | Kısaltma |
| `islem.tampon` | Komut kimliği |

## Sözdizimi

```text
TAMPON [nesneler=<k>] [kapsam=secili|gorunum|proje] mesafe=<metre>
       [birlestir=evet|hayir] [kose=yuvarlak|koseli|pah] [uc=yuvarlak|duz|kare]
       [katman=<ad>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Tamponu alınacak nesnelerin kimlikleri; verilmezse `kapsam` |
| `kapsam` | `secili` (varsayılan), `gorunum`, `proje` |
| `pencere` | `gorunum` için görünümün iki köşesi |
| `katman` | Tamponların yazılacağı katman; yoksa oluşturulur; boşsa etkin katman |
| `mesafe` | **Tampon mesafesi, metre** (zorunlu); eksi değer yalnız alanları aşındırır |
| `birlestir` | Üst üste binen tamponları tek alanda birleştir (öntanımlı `evet`) |
| `kose` | Dış köşelerin biçimi: `yuvarlak` (öntanımlı), `koseli`, `pah` |
| `uc` | Çizgi uçlarının biçimi: `yuvarlak` (öntanımlı), `duz`, `kare` |

## Örnekler

### Komut satırı

Bir dere ekseninin iki yanında 10 m koruma bandı, `KORUMA` katmanına:

```text
ÇOKLUÇİZGİ 0,0 40,5 80,0
TAMPON nesneler=1 mesafe=10 katman=KORUMA
```

İki kuyunun 30 m koruma alanları, her biri ayrı:

```text
NOKTA 0,0
NOKTA 40,0
TAMPON nesneler=2 nesneler=3 mesafe=30 birlestir=hayir katman=KUYU
```

Bir parselin 3 m içerisi (aşındırma):

```text
ALAN 0,0 30,0 30,20 0,20
TAMPON nesneler=4 mesafe=-3 kose=koseli
```

### Arayüz

Şeritte **Analiz ▸ İşlem araçları ▸ Tampon**'a (ya da **Kadastro ▸ Denetim ▸ Tampon**'a)
basın ya da sağ paneldeki **Araçlar** sekmesinde **Analiz ▸ Tampon bölge**'yi seçin. Kartta kapsamı (Seçili · Görünüm · Proje), mesafeyi
ve öteki seçenekleri doldurun, çıktı katmanını yazın ve **Çalıştır**'a basın. Kapsam
seçiliyse ve seçim boşsa araç tuvalden seçtirir: her tık ekler, **sağ tık** başlatır.

### Betik

```json
{
  "ad": "Dere koruma bandı",
  "komutlar": [
    { "cmd": "core.polyline", "args": { "noktalar": [[0,0],[40000,5000],[80000,0]] } },
    { "cmd": "islem.tampon",
      "args": { "nesneler": [1], "mesafe": 10, "katman": "KORUMA" } }
  ]
}
```

## Geri alma

Tek adımdır: bir `TAMPON` kaç alan çizmiş olursa olsun tek [`GERİAL`](undo.md) hepsini
kaldırır.

## Betikten kullanım

Betikte `nesneler` ya da `kapsam` verilmelidir, çünkü betik çalışırken "etkin seçim"
olmayabilir. Komut günlüğüne uygulanan nesnelerin kimlikleri ve her parametre yazılır;
yeniden oynatılan satır aynı tamponu çizer. Python'dan `cad.buffer(objects=[1],
distance=10)` olarak çağrılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Tampon mesafesi sıfır olamaz. Kaç metre istediğinizi yazın.` | `mesafe=0` | Artı bir mesafe verin |
| `Bu mesafede tampon kalmıyor: …` | Eksi mesafe alanların genişliğini aştı | Daha küçük bir aşındırma verin |
| `Kapsamda bu araca uygun nesne yok (N nesne bakıldı). …` | Kapsamda yalnız yazılar ya da desteklenmeyen türler var | Nokta, çizgi, alan ya da eğri seçin |
| `'kose' için tanınmayan değer: '…'. Seçenekler: …` | Seçenekli bir parametreye listede olmayan sözcük verildi | Mesajdaki seçeneklerden birini yazın |

## İlgili

- [`OFSET`](offset.md) — tek yanda paralel çizgi, alana paralel alan
- [İşlem araçları](../islem/README.md) — kapsam, Durdur, çıktı katmanı
- [`ALANÖLÇ`](measure_area.md) — tamponun alanını okumak için
