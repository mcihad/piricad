# KÖŞESİL — Köşe Silme

Fazladan alınmış bir kırık noktayı, sayısallaştırmada iki kez tıklanmış bir köşeyi
ya da artık gerekmeyen bir sınır dönüşünü kaldırmak isteyen herkes için; bu sayfayı
bitirdiğinizde bir nesnenin köşesini arayüzden, komut satırından ve betikten silmeyi
bileceksiniz.

## Ne yapar

`KÖŞESİL`, var olan bir nesnenin **bir köşesini** kaldırır. O köşede buluşan iki
kenar **tek kenar** olur: bir önceki köşeden bir sonrakine düz giden kenar. Nesne aynı
nesne olarak kalır: kimliği, katmanı, stili, öznitelikleri ve ona bağlı yazılar
değişmez.

| Nesne | Ne olur |
|---|---|
| Çizgi, alan | Köşe halkasından çıkar; açık çizginin ucu silinirse o uçtaki kenar da gider |
| Yaylı çoklu çizgi | İki kenar tek kenar olur: ikisi de **aynı çemberin aynı yöne dönen yaylarıysa** tek yay, değilse düz kenar. Yay kalmazsa nesne düz çoklu çizgi olur, kimliği değişmez |
| Spline | Kontrol noktası çıkar; kalan noktalar dereceyi taşımıyorsa derece düşer ve bu söylenir |
| Daire, yay, elips, ölçü, blok | Silinmez: tanımları köşelerinden oluşmuyor. Tutamaklarını [`KÖŞETAŞI`](vertex_move.md) ile taşıyın |

Bir çizgi **en az iki**, kapalı bir şekil **en az üç** köşeyle kalır; son köşeler
silinmez ve komut bunu söyler.

### Ortak köşe

Yan yana iki parselin **ortak köşesi** ikisinden birden silinir: birden çok nesne
verilirse — ya da birden çok nesne seçiliyken bir köşeye tıklanırsa — hepsinin o
noktadaki köşesi aynı adımda gider. Parsellerden biri o köşeyi kaybedip öteki
tutarsa aralarındaki sınır iki ayrı çizgi olurdu; birlikte silmek sınırı ortak tutar.
Kimliğiyle verilen bir nesnenin o noktada köşesi yoksa adıyla söylenir; seçimden
gelenlerden köşesi olmayanlar işin dışında kalır, kilitli katmandakiler atlanıp
sayılır.

## Adlar

| Ad | Tür |
|---|---|
| `KÖŞESİL` | Türkçe, birincil |
| `KOSESIL` | ASCII karşılık |
| `DELVERTEX` | İngilizce karşılık |
| `KSL` | Kısaltma |
| `core.vertex_delete` | Komut kimliği |

## Sözdizimi

```text
KÖŞESİL nesne=<kimlik> kose=<sıra>
KÖŞESİL yer=<n>
KÖŞESİL nesne=<kimlik> <kimlik> … kaynak=<n>
```

`kose` yerine `yer` verilebilir: o noktaya en yakın köşe silinir; `nesne` de
verilmemişse o noktanın altındaki nesnenin.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Köşesi silinecek nesnenin kimliği. Birden çok kimlik ortak köşeyi siler |
| `kose` | Silinecek köşenin sırası. İlk köşe `1`'dir |
| `yer` | Köşeyi gösteren nokta: `kose` verilmezse en yakın köşe, `nesne` de verilmezse altındaki nesne. Günlüğe `yer` değil, bulunan `kose` yazılır |
| `kaynak` | Ortak köşenin yeri: verilen nesnelerin o noktadaki köşesi birlikte silinir |

## Örnekler

### Komut satırı

Yeni bir çizimde, bir çizginin ortadaki köşesini silmek:

```text
ÇOKLUÇİZGİ 0,0 10,0 10,10 20,10
KÖŞESİL nesne=1 kose=2
```

```text
Köşe silindi.
```

Yeni bir çizimde, yan yana iki parselin ortak kırık noktasını ikisinden birden
silmek:

```text
ALAN 0,0 10,0 10,5 10,10 0,10
ALAN 10,0 20,0 20,10 10,10 10,5
KÖŞESİL nesne=1 2 kaynak=10,5
```

```text
2 nesnenin ortak köşesi silindi.
```

### Arayüz

Sol araç sütununda **köşe ailesinin** düğmesini basılı tutun ya da sağ tıklayın ve
**Köşe Sil**'i seçin; aynı araç **Değiştir → Köşe Sil** menüsündedir.

1. Silinecek köşeye tıklayın. Nesne de bu tıklamayla seçilir; köşe hemen silinir.
2. Araç açık kalır: sıradaki köşeye tıklayarak devam edebilirsiniz, Esc bırakır.

Ortak köşe için önce iki parseli seçin, sonra aracı açıp ortak köşeye tıklayın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.polyline",
      "args": { "noktalar": [[0, 0], [10000, 0], [10000, 10000], [20000, 10000]] } },
    { "cmd": "core.vertex_delete", "args": { "nesne": [1], "kose": 2 } }
  ]
}
```

Betikte koordinatlar **milimetredir** (`10000` = 10 m).

## Geri alma

`KÖŞESİL` tek bir geri alma adımıdır; ortak köşede de. [`GERİAL`](undo.md) köşeyi —
yaylı çoklu çizgide yayıyla, splinede derecesiyle — tam olarak geri getirir.

## Betikten kullanım

Betikten çağrıldığında komut hiçbir şey sormaz: `nesne` ile `kose` (ya da `yer`),
ortak köşe için birden çok `nesne` ile `kaynak` verilmelidir. Günlük ortak köşede her
zaman `kaynak`'ı yazar; oynatıldığında her nesne kendi köşesini o yerde bulur.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bir çizgi en az iki köşeyle kalır; bu köşe silinemez.` | İki köşeli çizgide köşe silinmek istendi | Çizgiyi [`SİL`](erase.md) ile silin |
| `Kapalı bir şekil en az üç köşeyle kalır; bu köşe silinemez.` | Üçgenin köşesi silinmek istendi | Şekli silin ya da köşeyi taşıyın |
| `Nesne N'in köşesi silinemez: tanımı köşelerinden oluşmuyor …` | Daire, yay, elips, ölçü, blok ya da yazı | Tutamaklarını [`KÖŞETAŞI`](vertex_move.md) ile taşıyın |
| `Bu tutamak bir köşe değil, bir yayın ortası; …` | Yaylı çoklu çizgide yay ortası gösterildi | Yayın ucundaki köşeyi gösterin |
| `Bir spline en az iki kontrol noktasıyla kalır; bu nokta silinemez.` | İki noktalı spline | Spline'ı silin |
| `Bu nesnenin N. köşesi yok; M köşesi var.` | Olmayan köşe numarası | 1 ile M arasında bir numara verin |
| `Nesne N'in bu noktada köşesi yok.` | Ortak köşede kimliğiyle verilen nesnenin o noktada köşesi yok | O nesneyi çıkarın ya da doğru `kaynak` verin |
| `Orada seçili nesnelerin bir köşesi yok. Bir köşeye tıklayın.` | Seçim birden çok nesneyken tıklama bir köşeye değmedi | Seçili nesnelerden birinin köşesine tıklayın |
| `Bu noktada seçili nesnelerin köşesi yok.` | `kaynak` hiçbir seçili nesnenin köşesine denk gelmiyor | Ortak köşenin tam yerini verin |
| `'TAPU' katmanı kilitli; üzerindeki nesne düzenlenemez. …` | Nesne kilitli katmanda | `KATMAN ad=TAPU kilitli=hayır` |

## İlgili

- [`KÖŞEEKLE`](vertex_insert.md) — kenarın ortasına köşe ekler
- [`KÖŞETAŞI`](vertex_move.md) — köşeyi ya da tutamağı taşır
- [`KENARTÜRÜ`](edge_kind.md) — kenarı yaya ya da düze çevirir
- [`GERİAL`](undo.md)
