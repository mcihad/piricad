# Öznitelik Tablosu

Bir katmanın satırlarını ve sütunlarını bir tabloda açar; süzer, sıralar, düzenler
ve seçili sütunun sayısal özetini çıkarır.

**F6** ile ya da **Analiz ▸ Öznitelik Tablosu** ile açılır. Araç çubuğundaki
*pencereler* grubunda da bir düğmesi vardır. Bu üç yol tabloyu **aktif katmanda**
açar.

Başka bir katmanın tablosunu açmak için katman panelinde o satıra **sağ tıklayıp
Öznitelik tablosu** deyin — katmanı aktif yapmanız gerekmez
([KATMAN](../komutlar/layer.md)).

**Tablo tek bir katmanın satırlarını gösterir**, çizimin tamamını değil: başlıkta
yazan katman, alttaki sayıda sayılan katman ve süzgecin üzerinde çalıştığı katman
aynı katmandır. Silinmiş ya da adı değişmiş bir katmanın tablosu açık kalırsa
tablo boşalır — orada olmayan bir katmanın satırları yoktur.

## Pencerede ne var

```text
┌ Öznitelik Tablosu — PARSEL   18 nesne ──────────────────── ✕ ┐
│ ✎ 💾 ↶ ↷ │ ＋ ✖ ⧉ │ ▣ ➤ │ ▽ ƒ Σ ▦ │ ⭳ 🖨      [Tablo|Form] │
├──────────────────────────────────────────────────────────────┤
│ ƒ  "alan_m2" > 3000 AND "plan_fonksiyon" = 'Konut'           │
│                              [Filtrele] [Kaydet ▾] [ara…]    │
├────────────────────────────────────────┬─────────────────────┤
│  fid  ada_no  parsel_no  alan_m2  …    │ Alan İstatistikleri │
│  1    1284    19         2 940.12      │ alan_m2 · gerçek    │
│  2    1284    20         3 105.80      │ ▁▃█▅▂▁              │
│  3    1284    21         3 482.64      │ Nesne sayısı     18 │
│                                        │ Ortalama  4 404.84  │
├────────────────────────────────────────┴─────────────────────┤
│ 1 – 18 / 18                        Σ alan_m2 = 79 287.21     │
└──────────────────────────────────────────────────────────────┘
```

## Sütunlar

İlk sütun **fid**'dir: nesnenin kalıcı kimliği. Bu bir öznitelik değildir,
**kimliktir** — düzenlenemez ve hiçbir zaman bir sütuna dönüşmez.

Ondan sonrası `SÜTUN` komutuyla tanımlanmış sütunlardır, tanımlandıkları sırayla.
Yeni bir sütun tanımlamak için bkz. [SÜTUN](../komutlar/column.md).

### Sayılar nasıl yazılır

| Değer | Tabloda |
|---|---|
| Ondalıklı sayı | Binlik ayracı **boşluk**, ondalık ayracı **nokta**: `18 904.36` |
| Tam sayı | Olduğu gibi, ayraçsız: `1284` |
| `uzunluk` sütunu | **Metre** olarak |
| `evet_hayir` | `evet` / `hayır` |
| Boş hücre | `—` |

Tam sayı gruplanmaz, ondalıklı sayı gruplanır ve bunun sebebi vardır: tam sayı bu
tabloda neredeyse her zaman bir **kimliktir** — ada, parsel, UAVT kodu — ve hiç
kimse ada numarasını `1 284` diye yazmaz. Ondalıklı bir sayı ise bir **ölçüdür** ve
okuyan kişi büyüklüğünü bir bakışta görmek ister.

Ondalık ayracı burada **nokta**dır, kâğıttaki gibi virgül değil. Sebebi:
süzme ifadesi, sıralama ve dışa aktarma bu değeri geri okur ve `2 940,12` bunların
hiçbiri için bir sayı değildir. Paftaya basılan yazı virgül kullanmaya devam eder;
ikisi aynı sayının iki yazımıdır ve tek bir yerden üretilirler.

## Süzme ifadesi

Üstteki `ƒ` çubuğuna bir koşul yazıp **Filtrele**'ye basın (ya da **Enter**).
Koşulu sağlamayan satırlar gizlenir; hiçbir nesne silinmez, hiçbir şey değişmez.

```
"alan_m2" > 2000
"plan_fonksiyon" = 'Konut'
"alan_m2" > 2000 AND "plan_fonksiyon" = 'Konut'
"nitelik" = 'Tarla' OR "nitelik" = 'Bahçe'
NOT "malik" = 'Belediye'
"beyan" IS NULL
("ada_no" = 1284 OR "ada_no" = 1285) AND "alan_m2" > 3000
```

### Yazım kuralları

| Yazım | Anlamı |
|---|---|
| `"sütun_adı"` | **Çift tırnak** bir sütun adıdır |
| `'metin'` | **Tek tırnak** bir metin değeridir |
| `2000`, `3.14`, `(1000*2)` | Sayı; aritmetik de yazılabilir |
| `=` `!=` `<>` `<` `<=` `>` `>=` | Karşılaştırma |
| `AND` `OR` `NOT` | Mantık; `AND` `OR`'dan sıkı bağlar |
| `IS NULL` / `IS NOT NULL` | Hücre dolu mu |
| `( … )` | Öncelik |

Çift tırnak sütun, tek tırnak metin — bu SQL'in yazımıdır ve CBS kullanan herkesin
elinde zaten vardır.

### Boş hücre bilinmeyendir

Doldurulmamış bir hücreye yapılan **her** karşılaştırma yanlıştır — `!=` dahil.
Ölçülmemiş bir cephe "5'ten farklı" değildir; **bilinmiyordur**. Aksi olsaydı
ölçüsü girilmemiş her parsel, "şundan farklı olanları göster" diyen her süzgece
düşerdi.

Boş hücreleri aramak için `IS NULL` yazın.

### Aynı dilbilgisi, her yerde

Bu ifadeyi okuyan çözümleyici, komut satırındaki `@(100*3),0` ifadesini okuyanla
**aynıdır**. Projede tam olarak bir dilbilgisi vardır
(`kentos_cad/command/parser.hpp`, CLAUDE.md 5.11) ve öznitelik süzgeci onun bir
istisnası değildir. Bunun pratik sonucu şudur: burada işe yarayan bir ifade
betikte de, yapay zekâya verilen bir görevde de aynı anlama gelir.

Hatalı bir ifade tabloyu boşaltmaz — hiçbir şey süzülmez ve çubuğun ipucunda hata
nedeni yazar.

## Düzenleme

Bir hücreye çift tıklayın, yeni değeri yazın, **Enter**.

Bu bir `ÖZNİTELİK` komutu gönderir. Yani:

- **Geri alınabilir** — **Ctrl+Z** hücreyi eski değerine döndürür.
- **Günlüğe yazılır** — komut günlüğünde, komut satırından yazılmış hâliyle
  ayırt edilemez bir satır olarak durur.
- **Betikten de yapılabilir** — aynı satırı bir betiğe koyarsanız aynı şey olur.

Boş bırakılan bir hücre `yok` değerini alır, yani gerçekten boşalır — sıfır olmaz.

## Seçim haritayla ortaktır

Tabloda bir satır seçmek çizimde o nesneyi seçer; ikisi aynı nesnenin iki
görünümüdür. Seçim `SEÇ` komutuyla gider, tıpkı tuvalde seçmek gibi.

## Alan istatistikleri

Sağdaki panel, **imlecin bulunduğu sütunun** özetini çıkarır. Sütunu değiştirmek
için o sütundaki bir hücreye tıklamanız yeterlidir.

| Satır | Ne demek |
|---|---|
| **Nesne sayısı** | Süzgeçten geçen satır sayısı |
| **Geçerli değer** | Bunların kaçında sayı var |
| **NULL** | Kaçı boş |
| **Minimum / Maksimum** | En küçük ve en büyük değer |
| **Ortalama** | Aritmetik ortalama |
| **Ortanca** | Sıralandığında ortadaki değer |
| **Std. sapma** | Standart sapma (popülasyon) |
| **Toplam** | Değerlerin toplamı |

Üstteki çubuk grafik, değerlerin yirmi kovaya dağılımıdır: bir sütunun tek bir
uç değeri olup olmadığını sayıya bakmadan gösterir.

Sayı taşımayan bir sütun seçildiğinde panel bunu yazar ve özet çıkarmaz.

## Sırada ne var

Bugün olmayanlar ve hangi fazda gelecekleri:

- **Satır ekleme ve silme** tablodan — Faz 2. Bugün nesneler çizimden eklenir.
- **Form görünümü** (bir kayıt, tek sayfa) — Faz 2.
- **Kayıtlı süzgeçler** — Faz 2.
- **Dışa aktarma ve yazdırma** tablodan — Faz 2; bugün `DIŞAAKTAR` komutu vardır.
- **Sayfalama**, 1 482 satırdan büyük tablolar için — Faz 1.

## Hatalar

| İleti | Sebep | Çözüm |
|---|---|---|
| `Kapanmayan sütun adı tırnağı` | `"alan_m2` gibi, çift tırnak kapanmamış | Tırnağı kapatın |
| `Kapanmayan metin tırnağı` | `'Konut` gibi | Tırnağı kapatın |
| `Beklenen bir karşılaştırma: = != < <= > >= veya IS NULL` | `"alan_m2" 2000` — işleç yok | Aradaki işleci yazın |
| `'IS' sonrası beklenen: NULL veya NOT NULL` | `"beyan" IS bos` | `IS NULL` ya da `IS NOT NULL` yazın |
| `Kapanmayan parantez` | `("ada_no" = 1284` | Parantezi kapatın |
