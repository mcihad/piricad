# TEMİZLE — Yinelenen ve Boş Nesneleri Temizleme

İki kez içe aktarılmış bir DXF'i, kendi üstüne sayısallaştırılmış bir parseli, çift
tıklamanın bıraktığı tek noktalık çizgiyi ya da iki kez kaydedilmiş bir köşeyi
ayıklamak isteyen herkes için; bu sayfayı bitirdiğinizde bunları arayüzden, komut
satırından ve betikten bulmayı, seçmeyi ve tek adımda onarmayı bileceksiniz.

## Ne yapar

`TEMİZLE`, kapsamındaki nesnelerde üç şeyi arar:

| Bulgu | Ne demek | Onarım |
|---|---|---|
| **Yinelenen** | Aynı tür, aynı katman, aynı köşelerle ikinci kez çizilmiş nesne. Ters yönde ya da başka köşeden başlanarak çizilmiş aynı çizgi ve aynı alan da yinelenendir | Silinir; en eski kopya kalır |
| **Boş** | Bütün köşeleri düğüm toleransı içinde tek noktada duran çizgi, ya da alanı sıfır olan alan: hiçbir şey çizmez | Silinir |
| **Tekrarlanan köşe** | Bir öncekiyle düğüm toleransı içinde aynı yerde duran köşe | Çıkarılır; nesne aynı nesne olarak kalır |

Birbirine projenin düğüm toleransından (`core.topoloji.dugum_toleransi`, varsayılan
1 cm) yakın iki köşe aynı köşe sayılır — [`SINIR`](boundary.md), [`ALANAÇEVİR`](to_area.md)
ve [`İFRAZ`](split_parcel.md)'ın okuduğu aynı tolerans.

**Önce bulur, istenirse onarır.** `islem=bul` (öntanımlı) hiçbir şeyi değiştirmez:
bulduklarını seçer, tuvalde işaretler ve tek tek yazar. Hemen ardından verilen
`islem=onar` bu seçime bakar; bir kopyanın aslı seçimde olmasa da çizimin her yerinde
aranır, kopya yine kopya sayılır. `islem=onar` hepsini **tek geri
alma adımında** onarır ve nesne nesne ne yaptığını söyler; köşesi çıkarılan her alanın
alanını **önce ve sonra** yazar — bir sınırı milimetre oynatan onarımı mühendisin
görebilmesi gerekir.

**Veri taşıyan silinmez.** İkizinde olmayan bir öznitelik taşıyan kopya, ya da ona
bağlı bir yazı olan nesne silinmez; adıyla söylenir, kararı siz verirsiniz.

Aynı bulucuyu [`TOPOLOJİ`](topology.md) kullanır: denetimin "yinelenen" dediği nesneyi
TEMİZLE de yinelenen görür.

## Adlar

| Ad | Tür |
|---|---|
| `TEMİZLE` | Türkçe, birincil |
| `TEMIZLE` | ASCII karşılık |
| `OVERKILL` | İngilizce karşılık |
| `TMZ` | Kısaltma |
| `core.cleanup` | Komut kimliği |

## Sözdizimi

```text
TEMİZLE [nesneler=<kimlik> …] [islem=bul|onar]
```

Nesne verilmezse etkin seçim, o da boşsa **bütün çizim** taranır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Bakılacak nesneler; yoksa seçim, o da boşsa bütün çizim |
| `islem` | `bul` (öntanımlı): bulur, seçer, işaretler, hiçbir şeyi değiştirmez · `onar`: yinelenenleri ve boş nesneleri siler, tekrarlanan köşeleri çıkarır |

## Örnekler

### Komut satırı

Yeni bir çizimde, iki kez çizilmiş bir parsel, köşelerinden biri 9 mm'lik bir kayıt
tekrarı olan bir parsel ve 3 mm'lik bir çizgi; önce bulmak, sonra onarmak:

<!-- örnek: yeni çizim -->
```
ALAN 0,0 20,0 20,10 0,10
ALAN 0,0 20,0 20,10 0,10
ALAN 30,0 40,0 40.009,0.009 40,10 30,10
ÇİZGİ 50,0 50,0.003
TEMİZLE
TEMİZLE islem=onar
```

```text
Temizlik (bütün çizim): 1 yinelenen, 1 boş nesne, 1 nesnede 1 tekrarlanan köşe. Seçildi ve işaretlendi.
  Nesne 2, nesne 1'in aynısı (aynı tür, aynı katman, aynı köşeler).
  Nesne 3: 1 köşe bir öncekiyle aynı yerde.
  Nesne 4 hiçbir şey çizmiyor (uzunluğu ya da alanı yok).
  Onarmak için: TEMİZLE islem=onar
Temizlik onarıldı (3 nesne): 2 nesne silindi, 1 nesneden 1 köşe çıkarıldı; değişen alanlar önce 100,05 m², sonra 100,00 m².
  Nesne 2 silindi (aynısı nesne 1 duruyor, alanı 200,00 m²).
  Nesne 3: 1 köşe çıkarıldı; alan önce 100,05 m², sonra 100,00 m².
  Nesne 4 silindi (hiçbir şey çizmiyordu).
```

### Arayüz

**Değiştir** menüsünde **Temizle — bul** ve **Temizle — onar**. Önce "bul" deyin:
bulunan nesneler seçilir ve her biri tuvalde "yinelenen", "boş" ya da "tekrarlanan
köşe" diye işaretlenir. Onaylıyorsanız "onar"; beğenmezseniz tek [`GERİAL`](undo.md).
Belli nesnelere bakmak için önce onları seçin.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.area",
      "args": { "noktalar": [[0, 0], [20000, 0], [20000, 10000], [0, 10000]] } },
    { "cmd": "core.area",
      "args": { "noktalar": [[0, 0], [20000, 0], [20000, 10000], [0, 10000]] } },
    { "cmd": "core.cleanup", "args": { "islem": "onar" } }
  ]
}
```

## Geri alma

`islem=onar` tek bir geri alma adımıdır: [`GERİAL`](undo.md) silinenlerin hepsini ve
köşesi çıkarılanların hepsini geri getirir. `islem=bul` çizimi değiştirmez; geri
alınacak bir şeyi yoktur.

## Betikten kullanım

Betikten çağrıldığında komut hiçbir şey sormaz. Günlüğe verilen `nesneler` ve çözülmüş
`islem` yazılır. Yapay zekâ ve betik istemcilerine `islem=onar` şu raporu döndürür:
`silinen` (kimlikler), `degisen` (her biri için `nesne`, `cikan_kose`,
`onceki_alan_mm2`, `sonraki_alan_mm2`), `korunan` (veri taşıdığı için silinmeyenlerin
sayısı), `onceki_alan_mm2`, `sonraki_alan_mm2`.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Nesne bulunamadı veya silinmiş: N` | `nesneler` içinde olmayan bir kimlik var | Kimlikleri denetleyin |
| `'core.cleanup': 'islem' için tanınmayan değer '…'. Kabul edilenler: bul / onar` | `islem` `bul` ya da `onar` değil | İki sözcükten birini yazın |
| `Nesne N silinmedi: öznitelik taşıyor ya da ona bağlı bir yazı var; karar sizin.` (not) | Silinecek kopya veri taşıyor | Özniteliği ikizine aktarıp kopyayı [`SİL`](erase.md) ile silin |

## İlgili

- [`TOPOLOJİ`](topology.md) — aynı bulucuyla denetler, hiçbir şeyi değiştirmez
- [`SİL`](erase.md) — seçili nesneleri siler
- [`GERİAL`](undo.md)
