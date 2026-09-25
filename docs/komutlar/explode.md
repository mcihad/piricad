# PATLAT — Parçalarına Ayır

Bir parselin bir köşesini oynatması gereken ama parselin bir *alan* olduğunu
gören, ya da bir çitin bir ayağını atması gereken ama çitin tek bir *çizgi*
olduğunu gören herkes için.

## Ne yapar

Bir nesneyi çizildiği parçalara ayırır ve kendisini siler:

| Nesne | Ne çıkar |
|---|---|
| Çok köşeli çizgi | Her kenar ayrı bir çizgi |
| Alan | Sınırı ayrı kenarlar hâlinde — **kapanış kenarı dâhil** |
| Yaylı çoklu çizgi | Düz kenarları çizgi, bükük kenarları **gerçek yay** (aynı merkez, aynı yarıçap) |
| Blok referansı | Bileşenleri **kendi türünde**, durdukları yere yerleştirilmiş; dizinin her kopyası için |

Parçalar nesnenin görünüşünü taşır: kırmızı bir çizginin kenarları da kırmızıdır.

### Blok referansı nasıl açılır

Tanımdaki her bileşen, referansın onu çizdiği yerleşimle — ölçek, açı, ayna ve dizi
adımı — yeniden yapılır. Bir daire daire, bir yay yay, bir yazı yazı, bir elips elips,
bir tarama tarama, bir ölçü ölçü olarak çıkar; hiçbiri çizilmiş dış çizgisine
çevrilmez. Yerleştirme referansın kendini çizdiği tam sayı aritmetiğiyle yapılır, bu
yüzden bir parçanın köşesi, referansın o köşeyi çizdiği milimetrededir: patlatmadan
önce yakaladığınız nokta, patlattıktan sonra aynı noktadır.

| Bileşen | Parçası |
|---|---|
| `0` katmanında | Referansın katmanına iner |
| Kendi katmanında | Kendi katmanında kalır |
| Rengi katmandan (ByLayer), `0` katmanında | Referansın görünüşünü alır — çizimde öyle görünüyordu |
| Rengi bloktan (ByBlock) | `0` katmanındaysa referansın görünüşünü, kendi katmanındaysa o katmanın görünüşünü alır |
| Kendi rengi olan | Rengini korur |
| Gizli | Gizli kalır |
| İç içe bir blok | **Bir kat** açılır: iç blok, bileşik yerleşimiyle yine bir blok referansı olur |
| Yazı | Harfleri referansın ölçeğiyle büyür; taban çizgisi referansın çizdiği yönde kalır |

Bir **alan** yazısı (`{no}`) referansın değeriyle yazılmış düz yazı olarak çıkar: parça
çizimde ne yazıyorsa onu yazar. Referansın hiçbir yazıda görünmeyen öznitelik değerleri
parçalara geçmez; kaç tane olduğu çıktıda söylenir. Bileşenlerin kendi öznitelikleri
parçalarıyla gider.

## Ne reddeder ve neden

Referans x ve y'de **farklı** ölçekle konmuşsa (`olcek=2 olcek_y=1`) iki tür
yerleştirilemez ve **adıyla reddedilir**:

- **Yaylı çoklu çizgi:** bu ölçek yaylarını eliptik yapar; yaylı çoklu çizgi yalnız
  dairesel yay taşır.
- **Dik açının katı olmayan bir açıyla döndürülmüş iç blok:** bu ölçekte eğilir ve blok
  referansı eğikliği taşıyamaz.

İkisinde de hiçbir şey değişmez — yarım patlatma yoktur. Referansı önce eşit ölçeğe
getirin ([ÖLÇEKLE](scale.md)). Böyle bir referanstaki daire ise **elips** olarak
çıkar, çünkü çizimde öyle görünüyordur.

Daire, yay, elips, nokta ve yazı zaten tek parçadır; doğrudan seçilirlerse PATLAT
bunu söyler.

[BLOKKIRP](block_clip.md) ile **kırpılmış** bir referans, kırpması yok sayılarak açılır —
AutoCAD'in EXPLODE'u gibi: parçalar tanımın bütün bileşenleridir ve çıktı bunu söyler
(`kırpma sınırı yok sayıldı, parçalar bütün çıktı`).

## Adlar

| Ad | Tür |
|---|---|
| `PATLAT` | Türkçe, birincil |
| `EXPLODE` | İngilizce karşılık |
| `PTL` | Kısaltma |
| `core.explode` | Komut kimliği |

## Sözdizimi

```text
PATLAT nesne=<kimlik> [nesne=<kimlik> …]
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | seçim | 0..n | Patlatılacak nesneler |

## Örnekler

### Komut satırı

Üç kenarlı bir çizgi üç çizgi olur:

```text
PATLAT nesne=1
```

```text
1 nesne patlatıldı, 3 parça çıktı.
```

Dört köşeli bir alan **dört** kenar verir — kapanış kenarı da bir kenardır:

```text
PATLAT nesne=2
```

```text
1 nesne patlatıldı, 4 parça çıktı.
```

### Blok referansı

Bir daire ve bir yazıdan oluşan `KAPAK` bloğu 2 kat büyük, 30° dönük konmuş olsun:

```text
PATLAT nesne=6
```

```text
Nesne 6 (blok referansı 'KAPAK') → 1 daire, 1 çizgi.
1 nesne patlatıldı, 2 parça çıktı.
```

Daire 2 kat yarıçaplı bir daire, yazı 2 kat yüksek bir yazı olarak çıkar. Aynı blok
`sutun=2 satir=2` dizisiyle konmuşsa dört kopyanın her biri açılır ve çıktı `4 kopya`
der; `0` katmanındaki bileşenler için `… parça 0 katmanından referansın katmanına`
satırı eklenir.

### Yapılandırılmış rapor

Betik ve yapay zekâ istemcisi raporu okur: her nesne için `tur`, blok için `blok`,
`kopya`, `katman_devri`, `gorunus_devri`, `gizli`, `yazilan_deger` (değeri yazıya işlenen
alan yazıları), `birakilan_oznitelik`, `kirpma_yok_sayildi` (referans kırpılmışsa `true`),
türlere göre parça sayısı (`turler`) ve parçaların kimlikleri (`parcalar`); en üstte
`patlatilan` ve `parca` toplamları.

### Arayüz

**Değiştir ▸ Birleştir ▸ Patlat** (aynı düğme **Giriş ▸ Değiştir**'de, **Çizim ▸ Blok**'ta ve
bir blok seçiliyken beliren **Blok** sekmesinde de vardır). Nesneleri seçip Enter'a basın.

### Betik

```json
{ "cmd": "core.explode", "args": { "nesne": [1, 2] } }
```

## Geri alma

Tek işlem, tek **Ctrl+Z**: geri alma nesneyi bütün hâline döndürür, parçalarından
birini değil.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır; her parametre için tip ve adet üretilmiş
[komut referansındadır](referans.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Blok '…' içindeki bir yaylı çizgi yerine konamadı: Blokta yaylı bir çoklu çizgi var ve referans eşit olmayan bir ölçekle konmuş…` | x ve y ölçeği farklı referansta yaylı çoklu çizgi | Referansı `ÖLÇEKLE` ile eşit ölçeğe getirip yeniden patlatın |
| `Blok '…' içindeki bir blok referansı yerine konamadı: Blokta döndürülmüş bir iç blok var…` | x ve y ölçeği farklı referansta, dik açının katı olmayan açıyla dönük iç blok | Referansı eşit ölçeğe getirin |
| `Nesne N bir daire; PATLAT çizgileri, alanları, yaylı çoklu çizgileri ve blok referanslarını patlatır.` | Doğrudan tek parçalık bir nesne seçildi | Bir daire, yay ya da yazı zaten parçalarına ayrılmış değildir |
| `Blok tanımı bulunamadı.` | Referansın adlandırdığı tanım dosyada yok | Dosyayı onarın ya da referansı silin |
| `'…' katmanı kilitli.` | Bir parçanın ineceği katman kilitli | `KATMAN` ile kilidi kaldırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [BLOK](block.md) — bileşenlerden blok tanımlar
- [BLOKEKLE](insert.md) — tanımlı bloğu yerleştirir
- [BLOKDÜZENLE](block_edit.md) — tanımı düzenler; bütün referanslar birden değişir
- [Blok referansı](../nesneler/blokreferansi.md) — referansın nasıl çizildiği ve yakalandığı
- [ALANAÇEVİR](to_area.md) — kapalı çizgiyi alana çevirir
