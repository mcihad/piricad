# ÇİFTÇİZGİ — Eksenin İki Yanında Paralel

Bir yolu, kanalı ya da demiryolunu ekseninden anlatan herkes için; bu sayfayı
bitirdiğinizde eksenin soluna ve sağına, her birini ayrı genişlikte, eksen çizilirken
arayüzden, komut satırından ve betikten çizmeyi bileceksiniz.

## Ne yapar

Bir **eksen** çizer — köşelerini [`ÇOKLUÇİZGİ`](polyline.md) gibi tek tek verirsiniz — ve
eksenin **solunda** ile **sağında**, her biri kendi genişliğinde, birer paralel çizgi
çizer. Netcad'in Paralel Çizgi aracının işidir.

| Ne | Nasıl |
|---|---|
| **Sol ve sağ** | Eksenin **çizildiği yöne bakarak**. Yönü çevirirseniz sol ile sağ da yer değiştirir |
| **Genişlik** | Eksenden paralele uzaklık, **metre**. `sol` ve `sag` ayrı verilir; **`0` o yanı çizmez** |
| **Köşeler** | Eksenin kırıklarında paralellerin köşeleri kendiliğinden çözülür: içte kesişimde birleşir, dışta `kose` söylediği gibi olur |
| **Eksen** | Öntanımlı çizilir; `eksen=cizme` yalnız paralelleri çizer |
| **Uçlar** | Öntanımlı açıktır; `uclar=kapali` iki ucu birer çizgiyle kapatır |
| **Katman** | `katman_sol` ve `katman_sag` her yanı kendi katmanına koyar |

Genişlikler negatif verilmez: yön işaretten değil, **sol** ile **sağ** adından gelir. Bu,
programın her yerindeki kuralla uyumludur (sağ, çizim yönüne bakarken sağdır;
bkz. [`DİKAYAK`](perp_offset.md)).

### Genişlikler önce sorulur

Elle çizerken komut önce iki genişliği, sonra eksenin noktalarını sorar. Genişlikler
bilindiği için eksenin her noktasında **çift çizgi imlecin altında görünür** ve görünen,
tıklayınca çizilenin kendisidir: kılavuz, sonucu hesaplayan fonksiyonla çizilir
(`core::double_line`), ayrı bir tahminle değil. Yuvarlak bir köşe kılavuzda da gerçek
bir yaydır.

Genişlikleri komutla birlikte yazarsanız (`sol=` ve `sag=`) komut yalnız noktaları sorar.

### Köşeler

`kose` yalnız eksenin **dış** köşesini ayarlar; iç köşe her zaman iki paralelin kesişiminde
birleşir. Bir kırığın hangi yanı dış, hangisi iç: eksen sola dönüyorsa sol iç, sağ dıştır;
sağa dönüyorsa tersi.

| `kose` | Dış köşe |
|---|---|
| `keskin` (öntanımlı) | İki paralel kesişene kadar uzar |
| `yuvarlak` | **Gerçek bir yay**: merkezi eksenin köşesi, yarıçapı o yanın genişliği. Yay taşıyan yan bu yüzden bir **yaylı çoklu çizgi** olur |
| `pah` | Düz kesilir: birinci paralelin ucundan ikincinin başına kiriş |

Çok sivri bir kırıkta keskin köşe sonsuza uzamaz: köşe noktası eksenin köşesinden
genişliğin **iki katından** uzağa düşecekse köşe düz kesilir ([`OFSET`](offset.md)'te de
böyledir).

### Uçlar ve uç çizgileri

`uclar=kapali` eksenin iki ucuna birer **uç çizgisi** çizer. İki yan da çiziliyorsa çizgi
sol yanın ucundan sağ yanın ucuna, eksenin ucundan geçerek gider; yalnız bir yan
çiziliyorsa eksenin ucundan o yanın ucuna. Uç çizgileri etkin katmana çizilir.

Bir kırığın içi genişlikten dar kalırsa o yan **tek parça** çıkmayabilir, hiç de
kalmayabilir; komut bunu söyler (`(2 parça)`, `(bu genişlikte bir şey kalmadı)`). Böyle bir
yanın ucu belli olmadığından `uclar=kapali` reddedilir ve hiçbir şey çizilmez.

### Katmanlar ve köken

`katman_sol` ve `katman_sag` her paraleli adı verilen katmana koyar; katman yoksa
oluşturulur. Verilmezse paralel etkin katmandadır. Eksen ve uç çizgileri her zaman etkin
katmandadır.

Eksen çiziliyorsa paraleller ve uç çizgileri **eksenden türetilmiş** olarak kaydedilir;
[`NESNEBİLGİ`](entity_info.md) bunu söyler ([nesne kimliği ve
kökeni](../veri/kimlik-ve-koken.md)). `eksen=cizme` ile kaynak yoktur, köken de yoktur.

### Aynı hesap, ikinci bir hesap değil

Her yan, aynı eksene [`OFSET`](offset.md) ile çizilen paralelin **aynısıdır** — köşe
köşe, yay yay. `ÇİFTÇİZGİ` ayrı bir paralel hesabı taşımaz: düz ve pahlı köşeleri
Clipper2, yuvarlak köşeyi geometri çekirdeği OpenCASCADE hesaplar
([Geometri çekirdeği](../veri/geometri-cekirdegi.md)); sonuç milimetreye bir kez
yuvarlanır ve üç platformda aynı milimetreyi verir.

### Sınır

Eksen **açık** bir çizgidir. İlk ve son noktası aynı olsa bile iki ucu birleştirilmez.
Kapalı bir hattın paraleli için önce [`ALAN`](area.md) ile alanı çizin, sonra
[`OFSET`](offset.md) ile `taraf=dis` ya da `taraf=ic` verin.

## Adlar

| Ad | Tür |
|---|---|
| `ÇİFTÇİZGİ` | Türkçe, birincil — Netcad'deki adı Paralel Çizgi |
| `CIFTCIZGI` | ASCII karşılık |
| `DOUBLELINE` | İngilizce karşılık |
| `ÇFÇ`, `CFC` | Kısaltma |
| `core.double_line` | Komut kimliği |

## Sözdizimi

```text
ÇİFTÇİZGİ noktalar=<n1> <n2> <n3> … sol=<m> sag=<m>
          [kose=keskin|yuvarlak|pah] [eksen=ciz|cizme] [uclar=acik|kapali]
          [katman_sol=<ad>] [katman_sag=<ad>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Eksenin köşe noktaları; en az iki nokta. Verilmezse noktalar tek tek sorulur |
| `sol` | Sol paralelin eksene uzaklığı, **metre**. `0` sol yanı çizmez. Verilmezse sorulur |
| `sag` | Sağ paralelin eksene uzaklığı, **metre**. `0` sağ yanı çizmez. Verilmezse sorulur |
| `kose` | Dış köşenin biçimi: `keskin` (öntanımlı), `yuvarlak`, `pah` |
| `eksen` | `ciz` (öntanımlı): eksen de çizilir · `cizme`: yalnız paraleller |
| `uclar` | `acik` (öntanımlı) · `kapali`: iki uç birer çizgiyle kapatılır |
| `katman_sol` | Sol paralelin katmanı; yoksa oluşturulur. Verilmezse etkin katman |
| `katman_sag` | Sağ paralelin katmanı; yoksa oluşturulur. Verilmezse etkin katman |

**En az bir yan çizilmelidir**: `sol` ve `sag` birlikte `0` olamaz. Bir betikte ikisi de
yazılır; yazılmayan genişlik eksik sayılır, sıfır sayılmaz.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Doğuya sonra kuzeye giden bir eksenin 2 m solu ve 3 m sağı:

```
ÇİFTÇİZGİ noktalar=0,0 10,0 10,10 sol=2 sag=3
```

```text
Çift çizgi çizildi: eksen, sol paralel 2,000 m, sağ paralel 3,000 m.
```

Eksen sola döndüğü için solu **iç**, sağı **dış** yandır. Sol paralel `y = 2` ile `x = 8`
çizgilerinin kesiştiği `(8, 2)` köşesinden döner: `(0, 2)`, `(8, 2)`, `(8, 10)`. Sağ
paralel `y = -3` ile `x = 13` çizgilerinin `(13, -3)` köşesinden döner: `(0, -3)`,
`(13, -3)`, `(13, 10)`.

Yalnız sağ yan — solun genişliği `0`:

```
ÇİFTÇİZGİ noktalar=0,20 10,20 10,30 sol=0 sag=3
```

```text
Çift çizgi çizildi: eksen, sağ paralel 3,000 m.
```

Dış köşe gerçek bir yay olsun; sağ paralel `(10, 40)` merkezli, 3 m yarıçaplı bir yay
taşır:

```
ÇİFTÇİZGİ noktalar=0,40 10,40 10,50 sol=2 sag=3 kose=yuvarlak
```

```text
Çift çizgi çizildi: eksen, sol paralel 2,000 m, sağ paralel 3,000 m.
```

Eksen çizilmesin, iki uç kapatılsın, iki yan da `KENAR` katmanına gitsin:

```
ÇİFTÇİZGİ noktalar=0,60 40,60 40,85 sol=3.5 sag=3.5 eksen=cizme uclar=kapali katman_sol=KENAR katman_sag=KENAR
```

```text
Çift çizgi çizildi: sol paralel 3,500 m, sağ paralel 3,500 m, iki uç çizgisi.
```

Bir kırığın içi genişlikten dar kalınca:

```
ÇİFTÇİZGİ noktalar=0,100 10,100 10,101 0,101 sol=2 sag=2
```

```text
Çift çizgi çizildi: eksen, sol paralel 2,000 m (bu genişlikte bir şey kalmadı), sağ paralel 2,000 m.
```

### Arayüz

Komut satırına `ÇİFTÇİZGİ` (kısaca `ÇFÇ`) yazıp **Enter**'a basın ya da **Ctrl+K** ile
açılan komut listesinde `ÇİFTÇİZGİ` yazarak bulun. Komut sırayla şunları sorar:

```text
Sol genişlik (m) — 0: sol yan çizilmez
Sağ genişlik (m) — 0: sağ yan çizilmez
Eksenin ilk noktası
Eksenin sonraki noktası — ⌫: son noktayı geri al
```

1. **Sol genişliği** metre olarak yazıp **Enter**'a basın (`3`). `0` sol yanı çizmez.
2. **Sağ genişliği** aynı biçimde yazın.
3. Eksenin **ilk noktasını** tıklayın. Bundan sonra her noktada çift çizgi imlecin altında
   görünür: eksen, paraleller ve — `uclar=kapali` ise — uç çizgileri. Eksenin
   köşelerini sırayla tıklayın.
4. Bitirmek için **Enter** ya da sağ tık. **Esc** de ekseni o ana kadar verilen
   noktalarla bitirir; tek nokta verilmişse ya da genişlik sorulurken basılırsa hiçbir
   şey çizmeden vazgeçer.

**Yanlış bir köşeyi geri almak.** **⌫** (Backspace) ya da **Ctrl+Z**'ye basın, veya
komut satırına `G` yazıp Enter'a basın: yalnız son nokta geri alınır, kılavuz bir
önceki noktadan yeniden uzanır.

Nesne yakalama açıkken tıklama köşeye ve noktaya oturur; yazılan koordinat yazıldığı
yere düşer. Seçenekleri (`kose`, `eksen`, `uclar`, `katman_sol`, `katman_sag`) ve
genişlikleri komutu başlatan satıra yazabilirsiniz: yazılanlar sorulmaz.

### Betik

```json
{
  "ad": "Yol ekseni ve iki kenarı",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "YOL" } },
    { "cmd": "core.double_line",
      "args": { "noktalar": [[0, 0], [10000, 0], [10000, 10000]],
                "sol": 2, "sag": 3, "kose": "yuvarlak",
                "katman_sol": "YOL-SOL", "katman_sag": "YOL-SAG" } }
  ]
}
```

Betikte noktalar **milimetredir**, genişlikler ise **metredir**: `[[0, 0], [10000, 0]]`
on metrelik bir eksendir, `"sol": 2` iki metrelik bir paraleldir.

## Yapılandırılmış cevap

Komut, çizdiği nesnelerin anahtarlarını söyler; bir betik ya da bir yapay zekâ
istemcisi cümleyi ayrıştırmak zorunda kalmaz. Yalnız çizilenler yazılır:

```json
{ "eksen": 1, "sol": [2], "sag": [3], "uclar": [4, 5] }
```

Bir yan birden çok parçaya bölündüyse o yanın listesinde birden çok anahtar olur.

## Geri alma

Tek adımdır: eksen, paraleller ve uç çizgileri birlikte tek [`GERİAL`](undo.md) ile
kalkar. **Çizerken** Ctrl+Z komutu değil yalnız son noktayı geri alır (bkz. **Arayüz**).

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır. Betikte `noktalar`, `sol` ve `sag` verilmelidir;
öbür parametreler isteğe bağlıdır. Genişlikler günlüğe **metre** olarak yazılır ve yeniden
oynatılan satır aynı çift çizgiyi çizer.

Python'da:

```python
cad.double_line(points=[[0, 0], [10000, 0], [10000, 10000]], left=2, right=3)
```

Adları ve tipleri: [Python başvurusu](../python/referans.md#caddouble_line).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `'core.double_line': zorunlu 'sag' parametresi eksik. Beklenen: sayı` | Genişliklerden biri verilmedi (`sol` için de aynısı söylenir) | İkisini de verin; çizilmeyecek yana `0` yazın |
| `Yan genişliği eksi olamaz. Bir yanı çizmemek için o yanın genişliğine 0 yazın; yön sol ve sağ diye ayrılır, eksi işaretle değil.` | Bir genişlik eksi | Artı bir değer yazın; yönü `sol` ile `sag` adı belirler |
| `Çift çizgi için en az bir yanın genişliği sıfırdan büyük olmalı: sol ya da sağ.` | İki genişlik de `0` | En az birine artı bir değer verin |
| `Yan genişliği çizimin koordinat sınırını aşıyor; metre olarak yazdığınızdan emin olun.` | Genişlik akla gelmeyecek kadar büyük — çoğunlukla milimetreyle yazılmış | Metre yazın: 3,5 m için `3.5` |
| `'core.double_line': 'noktalar' parametresi en az 2 değer istiyor, 1 değer geldi.` | Eksen tek noktalı | En az iki nokta verin |
| `Bir eksen en az iki nokta ister; 1 nokta verildi.` | Arayüzde tek nokta verilip Enter'a basıldı | İkinci bir nokta tıklayın |
| `Paralel için çizginin en az iki ayrı noktası olmalı. Verilen: 1` | Eksenin bütün noktaları aynı yerde | Ayrı noktalar verin |
| `'core.double_line': 'kose' için tanınmayan değer 'yuvar'. Kabul edilenler: keskin / yuvarlak / pah` | `kose` yanlış yazıldı. `eksen` için `ciz / cizme`, `uclar` için `acik / kapali` aynı biçimde söylenir | Söylenen sözcüklerden birini yazın |
| `Eksenin sol yanındaki paralel tek parça çıkmadı; uçları kapatılacak bir uç yok. Daha dar bir genişlik verin ya da uclar=acik kullanın.` | Bir kırığın içi genişlikten dar; o yan parçalandı ya da kalmadı. Sağ yan için "sağ yanındaki" söylenir | Daha dar bir genişlik verin ya da `uclar=acik` kullanın |
| `'YOL' katmanı kilitli.` | Etkin katman ya da paralelin gideceği katman kilitli | [`KATMAN`](layer.md) ile kilidi açın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [`OFSET`](offset.md) — çizilmiş bir nesnenin paraleli; aynı hesap, aynı köşeler
- [`ÇOKLUÇİZGİ`](polyline.md) — yalnız eksen: tek nesne olarak çoklu çizgi
- [`ALAN`](area.md) — kapalı alan; paraleli için `OFSET` `taraf=dis` ve `taraf=ic`
- [`KATMAN`](layer.md) — yan katmanlarını hazırlamak ve kilitlemek
