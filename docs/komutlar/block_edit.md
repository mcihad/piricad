# BLOKDÜZENLE — Bloğu Düzenleme

Bir sembolü — rögar kapağını, direği, kuzey okunu — çizimin her yerinde birden
değiştirmek isteyen herkes için.

## Ne yapar

Bir blok **tanımını** çizimin üstünde düzenlemenizi sağlar; kaydettiğinizde o bloğun
**bütün referansları** yeni biçimi çizer. Üç adımdır ve her adım ayrı bir komut, ayrı bir
geri alma adımıdır:

| Adım | Ne olur |
|---|---|
| `islem=ac` | Tanımın üyeleri, sıradan nesneler olarak — kendi türleri, katmanları, renkleri ve yazılarıyla, tanımda nasılsalar öyle — referansın ekleme noktasına çıkar. Referans, düzenleme bitene dek gizlenir. |
| `islem=kaydet` | Tanım, verdiğiniz nesnelerden yeniden kurulur. Değişmeden dönen nesne, karşılığı olan üyeyi **olduğu gibi** bırakır; değişen ya da yeni çizilen nesne tanıma yazılır; karşılığı dönmeyen üye tanımdan çıkar. Bloğu çizen her referansın kutusu yenilenir, gizli referans geri gelir. |
| `islem=vazgec` | Açılan nesneler kaldırılır, referans geri gelir; tanım değişmez. |

### Nerede açılır

Tanım, referansın ekleme noktasında **kendi yönünde ve 1:1 ölçekte** açılır. Ölçeği 1,
açısı 0 olan bir referans (`BLOK`'un bıraktığı gibi) böylece **tam yerinde** düzenlenir.
Dönük ya da ölçekli bir referanstan açılan tanım o referansın üstüne oturmaz, dik ve gerçek
boyutunda açılır — çünkü düzenlediğiniz şey tanımın kendisidir, o referansın ona verdiği
görünüş değil. Bunun iyi bir sonucu var: açmak ve kaydetmek yalnız bir ötelemedir, tam sayı
toplamasıdır; bir bloğu açıp dokunmadan kaydetmek tanımı milimetresi milimetresine aynı
bırakır.

`ad=` ile açılan tanım, üyelerin tanımda durduğu yerde açılır.

### Hangi nesneler bloğa aittir

Arayüzde düzenleme açıkken çizdiğiniz her şey bloğa aittir; **Bloğu Kaydet** açılan
nesneleri ve sonradan çizdiklerinizi birlikte verir. Komut satırında ve betikte
nesneleri `nesneler=` ile kendiniz verirsiniz — komut hiçbir şeyi aklında tutmaz, her
adım günlüğe kendi başına oynatılabilir biçimde yazılır.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `BLOKDÜZENLE` | `BLOKDUZENLE` | `BEDIT` | `BDZ` |

## Sözdizimi

```text
BLOKDÜZENLE [nesne=<referans>]
BLOKDÜZENLE ad=<blok>
BLOKDÜZENLE islem=kaydet nesne=<referans> nesneler=<kimlik> [nesneler=<kimlik> …]
BLOKDÜZENLE islem=kaydet ad=<blok> nesneler=<kimlik> [nesneler=<kimlik> …]
BLOKDÜZENLE islem=vazgec nesne=<referans> nesneler=<kimlik> [nesneler=<kimlik> …]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `islem` | `ac` (varsayılan), `kaydet` ya da `vazgec` |
| `nesne` | Düzenlenen blok referansı. Açarken verilmezse etkin seçim, o da yoksa sorulur; kaydederken ve vazgeçerken açtığınız referansın kimliğini verin (gizli olduğu için tıklanamaz) |
| `ad` | Referans yerine bloğun adı: tanım kendi yerinde açılır |
| `nesneler` | `kaydet` ve `vazgec` için bloğun nesneleri: açılanlar ve sonradan çizilenler. Birden çok nesne için anahtarı yineleyin |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Bir daire ve bir çizgiden `KAPAK` bloğu yapılır; `BLOK` (0,0)'a bir referans bırakır
(kimliği 5):

```text
DAİRE merkez=0,0 cevre=0.6,0
ÇİZGİ -0.6,0 0.6,0
BLOK ad=KAPAK taban=0,0 nesneler=1 nesneler=2
BLOKDÜZENLE nesne=5
```

```text
'KAPAK' bloğu düzenlemeye açıldı: 2 nesne referansın yerinde; referans düzenleme bitene dek gizli. Bitirince BLOKDÜZENLE islem=kaydet (ya da vazgec).
```

Açılan daire 6, çizgi 7'dir. Çizginin ucunu taşıyıp bir nokta ekleyin (8), sonra
kaydedin:

```text
KÖŞETAŞI nesne=7 kose=2 nokta=1,0
NOKTA 0,0.8
BLOKDÜZENLE islem=kaydet nesne=5 nesneler=6 nesneler=7 nesneler=8
```

```text
'KAPAK' bloğu kaydedildi: 1 üye olduğu gibi kaldı, 2 üye yazıldı, 1 üye çıkarıldı. Bloğun 1 referansı yeni biçimi çiziyor.
```

Daire değişmediği için tanımda aynı üye olarak kalır; değişen çizgi ve yeni nokta
yazılır, çizginin eski hâli çıkar. Çizimde bu bloğun on iki referansı olsaydı on ikisi
de yeni çizgiyi ve noktayı çizerdi.

Vazgeçmek için, açtıktan sonra:

```text
BLOKDÜZENLE islem=vazgec nesne=5 nesneler=6 nesneler=7
```

### Arayüz

Bir bloğa **çift tıklayın** — ya da bloğu seçip **Blok ▸ Bloğu Düzenle**. Şeritte
**Blok: KAPAK** sekmesi açılır. Nesneleri her zamanki araçlarla düzenleyin, yeni
nesneler çizin; bitince **Bloğu Kaydet** ya da **Vazgeç**. Düzenleme açıkken projeyi
kaydetmek, yeni bir çizim açmak ya da pencereyi kapatmak istediğinizde program önce
bloğu kaydetmek mi, düzenlemeden vazgeçmek mi istediğinizi sorar.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.circle_draw", "args": { "merkez": [0, 0], "cevre": [600, 0] } },
    { "cmd": "core.line", "args": { "noktalar": [[-600, 0], [600, 0]] } },
    { "cmd": "core.block", "args": { "ad": "KAPAK", "taban": [0, 0], "nesneler": [1, 2] } },
    { "cmd": "core.block_edit", "args": { "nesne": [5] } },
    { "cmd": "core.vertex_move", "args": { "nesne": [7], "kose": 2, "nokta": [1000, 0] } },
    { "cmd": "core.block_edit",
      "args": { "islem": "kaydet", "nesne": [5], "nesneler": [6, 7] } }
  ]
}
```

Açma adımının yapılandırılmış raporu açılan nesnelerin kimliklerini `parcalar`
altında verir; betik ve yapay zekâ istemcisi kaydederken onları okur.

### Üçü de aynı

Arayüz, komut satırı ve betik aynı komutları çalıştırır; aynı belgeyi ve aynı günlüğü
bırakırlar.

## Geri alma

Her adım tek **Ctrl+Z**'dir. Kaydetmeyi geri almak eski tanımı bütün referanslarına geri
getirir ve açılan nesneleri yeniden sayfaya koyar; açmayı geri almak açılan nesneleri
kaldırır ve referansı gösterir.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır. Açmanın raporu: `islem`, `blok`, `referans`,
`oteleme` (milimetre, sağa ve yukarı) ve `parcalar`. Kaydetmenin raporu: `korunan`,
`yazilan`, `cikarilan`, `referans_sayisi`, `kutusu_yenilenen`. Günlüğe `islem`, `nesne`
ya da `ad` ve `nesneler` yazılır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Referans N zaten düzenlemeye açık (gizli).` | Aynı referans ikinci kez açılıyor | Önce `islem=kaydet` ya da `islem=vazgec` |
| `Referans N düzenlemeye açık değil; önce BLOKDÜZENLE nesne=N ile açın.` | Açılmamış bir referans için kaydetme ya da vazgeçme | Önce açın |
| `Kaydedilecek nesne yok: bir blok boş kalamaz.` | `kaydet` nesnesiz çağrıldı | Nesneleri `nesneler=` ile verin; açılanı geri almak için `islem=vazgec` |
| `Blok 'X' kaydedilemedi: …` | Tanıma yazılamayan bir nesne — örneğin bloğun kendisine bir referans (blok kendini içeremez) | O nesneyi bloğun nesnelerinden çıkarın |
| `Hangi bloğun düzenlendiğini söyleyin: nesne=<açılan referans> ya da ad=<blok>.` | Kaydederken referans ya da ad verilmedi | `nesne=` ya da `ad=` ekleyin |
| `'X' adında bir blok yok.` | `ad=` bilinmeyen bir blok | Tanımlı blokların adı mesajda yazılıdır |
| `'…' katmanı kilitli.` | Açılacak üyenin katmanı kilitli | `KATMAN` ile kilidi kaldırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [BLOK](block.md) — nesnelerden blok tanımlamak
- [BLOKEKLE](insert.md) — tanımı yerleştirmek
- [PATLAT](explode.md) — tek bir referansı parçalarına ayırmak; tanım değişmez
- [Blok referansı türü](../nesneler/blokreferansi.md)
