# MPYY gösterimlerinin vektör dili

Bu belge, Mekânsal Planlar Yapım Yönetmeliği'nin (MPYY) EK-1 gösterimlerinin resimden
**sayıya** nasıl yazıldığını anlatır: hangi katman tipi neyi çizer, hangi alan hangi
birimdedir, bir satır ne zaman SVG ister.

**İş bitmiştir.** 476 gösterimin 467'si yazıldı, 9'u gerekçesiyle atlandı. Bu belge
artık bir iş emri değil, paketin **sözlüğüdür**: yönetmelik yeni bir gösterim
yayımladığında ya da yazılmış bir satır düzeltileceğinde okunacak yer burasıdır.
Sayıların hangi ölçümden geldiği `docs/veri/mpyy-gosterimleri.md` sayfasındadır.

Yanındaki dosyalar:

| Dosya | Ne işe yarar |
|---|---|
| `TALIMAT.md` | Bu belge. Katman dilini tarif eder |
| `YAPILACAKLAR.md` | 476 satırın listesi ve hangisinin yazıldığı |
| `ATLANANLAR.md` | Çizilmeyen dokuz satır ve her birinin gerekçesi |
| `scripts/ci-gate-mpyy-vektor.py` | Paketi denetler ve listenin doğru söylediğini kanıtlar |

---

## 1. Bu paket neden var

`data/catalogs/mpyy/plan-gosterim.json` yönetmelik eklerinden çıkarılmış **476
gösterim** taşır. Her gösterimin karşılığı, ekten kesilmiş bir **JPEG ya da PNG**
resimdir: `data/catalogs/mpyy/semboller/` altında, 608 dosya.

Raster bir gösterim üç şey yapamaz ve üçü de bu işin sebebidir:

1. **Köşe dönemez.** Bir sınır çizgisi 90° döndüğünde raster desen kırılır; bir
   çizgi tipi dönmelidir.
2. **Yeniden renklendirilemez.** Plan türüne göre aynı gösterim başka renkte
   basılır; resimde renk piksele gömülüdür.
3. **Ölçekle büyümez.** 1/1000'de okunan bir tarama, 1/5000'de gri bir lekedir.

Vektör karşılık bunların üçünü de yapar. İş, her resmin **ne çizdiğini okuyup**
onu sayılarla yeniden söylemektir.

---

## 2. Değişmez kurallar

Bu beş kural tartışmaya açık değildir. Bir çıktı bunlardan birini çiğniyorsa
yanlıştır, "yaklaşık doğru" değildir.

### 2.1 Her gösterime tek tek bakılır

`YAPILACAKLAR.md` içindeki her satır, adı geçen görsel dosyayı **açmayı** ve ne
çizdiğine **bakmayı** gerektirir. Bir gösterimin adından ne olduğunu tahmin etmek
bu işin yapılmaması demektir: `ORMAN ALANI` ile `KENT ORMANI` farklı çizilir,
`AKARSU` ile `KURU DERE` farklı çizilir ve hangisinin hangisi olduğunu yalnız
resim söyler.

### 2.2 Ölçüler mikrometredir ve kâğıda aittir

Bütün uzunluklar **tam sayı mikrometre** (`µm`) yazılır: `1 mm = 1000`.

Birim `"kagit"` olur, çünkü yönetmelik bir gösterimin kalınlığını **paftada**
verir ve o kalınlık 1/1000'de de 1/5000'de de aynıdır.

Tek istisna, ölçüsü **zemine** ait olan dokulardır: bir orman deseninin sıklığı
araziye aittir, kâğıda değil. Bunlar için `"zemin"` yazılır ve satırın
`belirsiz_nedeni` alanına bunun bir okuma olduğu not edilir.

`"piksel"` bu pakette **kullanılmaz**.

### 2.3 Renk `#AARRGGBB`'dir

Sekiz haneli, alfa önde: `#FF000000` opak siyah, `#00000000` görünmez.
Yönetmelik bir renk vermiyorsa `#FF000000` yazılır — gösterimlerin çoğu siyah
basılır. Ek bir RGB kodu veriyorsa (EK-1ç ve EK-1d'de `ALAN RENK KODU (RGB)`
sütunu vardır) o kod alfa `FF` ile önüne eklenerek yazılır.

### 2.4 Her satır bir OKUMADIR, alıntı değil

Yönetmelik bir **resim** basar; milimetre vermez. Yazdığınız her sayı, o resme
bakıp verilmiş bir karardır. Bu yüzden her satır şu ikisini taşır:

```json
"belirsiz": true,
"belirsiz_nedeni": ["cizim-yorumu"]
```

Bu bir formalite değildir. Bu paket bir plana uygulanmadan önce harita mühendisi
ve şehir plancısı onayı bekler ve o onay, hangi sayıların ölçüldüğünü değil
**yorumlandığını** bilerek verilir. İşaret düşerse onay yanlış bir zemine oturur.

### 2.5 Raster bu pakete girmez

`data/catalogs/mpyy-vektor/semboller/` altına **yalnız `.svg`** konur. JPEG ya da
PNG kopyalamak, kurtulmak için var olan şeyi geri getirmektir.

---

## 3. Çıktı biçimi

Her gösterim, `data/catalogs/mpyy-vektor/plan-gosterim.json` içindeki `stiller`
dizisine bir nesne olarak eklenir.

### 3.1 Satırın iskeleti

```json
{
  "id": "ortak-ulke-siniri",
  "ad": "ÜLKE SINIRI",
  "ek": "EK-1a",
  "kaynak": "Mekânsal Planlar Yapım Yönetmeliği (MPYY), EK-1a (RG-22/1/2026-33145), SINIRLAR > İDARİ SINIRLAR",
  "bolum": ["SINIRLAR", "İDARİ SINIRLAR"],
  "katmanlar": [ ... ],
  "belirsiz": true,
  "belirsiz_nedeni": ["cizim-yorumu"]
}
```

`id`, `ad`, `ek`, `kaynak` ve `bolum` alanları **resmî katalogdan olduğu gibi
kopyalanır**. Uydurulmaz, düzeltilmez, Türkçeleştirilmez. Kimlik bir harf bile
tutmazsa satır hiçbir zaman eşleşmez.

### 3.2 İki yol: `katmanlar` ya da `gorsel`

**Tercih edilen yol `katmanlar`dır.** Sayılarla söylenmiş bir sembol katmanı
yığını köşe döner, yeniden renklendirilir, çizgi tipi olarak dışa aktarılır.

`gorsel` (SVG) yalnızca katman tiplerinin **ifade edemediği** şekiller içindir:
bir kuş silueti, bir mercan, bir cami. "Uğraşmamak için SVG" doğru cevap
değildir; bir daire, bir üçgen, bir çapraz tarama katmanlarla yazılır.

### 3.3 Katman tipleri

| `tip` | Ne çizer | Nerede kullanılır |
|---|---|---|
| `cizgi` | Düz kontur. `desen` ile kesikli olur | Her sınır, her yol ekseni |
| `isaretci-cizgi` | Çizgi boyunca tekrar eden **şekil** | Nokta-çizgi sınırlar, ok dizileri |
| `tarak-cizgi` | Çizgi boyunca tekrar eden **çizik** | Tırtıklı sınırlar, şev tarakları |
| `dolgu` | Düz alan dolgusu | Renkli fonksiyon alanları |
| `cizgi-desen-dolgu` | Paralel çizgilerden tarama | Eğik taramalar, ızgaralar |
| `nokta-desen-dolgu` | Izgaraya dizilmiş şekil | Nokta taramaları, ağaç dizileri |
| `merkez-isaretci` | Alanın ORTASINA tek şekil | Çember içi simgeler |
| `isaretci` | Nokta nesnesinin şekli | Nirengi, tesis simgeleri |
| `gorsel-dolgu` / `gorsel-isaretci` / `gorsel-cizgi` | SVG'yi döşer / basar / dizer | Şekli yazılamayan gösterimler |
| `yazi-isaretci` | Sabit bir yazı | Harf taşıyan gösterimler |

### 3.4 Şekil, yerleşim, birim

```
sekil     : daire · kare · ucgen · baklava · yildiz · arti · carpi ·
            ok · yarim-daire · besgen · altigen · cizik
yerlesim  : aralik · tepe · ilk · son · orta
birim     : kagit · zemin        (piksel bu pakette kullanılmaz)
```

### 3.5 Katmanın alanları

| Alan | Tür | Anlamı |
|---|---|---|
| `tip` | metin | Yukarıdaki tiplerden biri. **Zorunlu** |
| `sekil` | metin | İşaretçi ve tarak tiplerinde **zorunlu** |
| `yerlesim` | metin | Şeklin çizgi üzerinde nereye düşeceği |
| `birim` | metin | `kagit` ya da `zemin` |
| `boyut` | tam sayı | Şeklin ölçüsü, µm |
| `aralik` | tam sayı | İki tekrar arası, µm |
| `aralik_y` | tam sayı | Nokta deseninde dikey aralık, µm |
| `kaydirma` | tam sayı | Çizgiden dik kaydırma, µm |
| `faz` | tam sayı | İlk tekrardan önceki mesafe, µm |
| `aci` | tam sayı | Mikroderece. `45°` = `45000000` |
| `renk` | `#AARRGGBB` | Kontur rengi |
| `dolgu_renk` | `#AARRGGBB` | Şeklin içi. `#00000000` = içi boş |
| `kalinlik` | tam sayı | Kontur kalınlığı, µm |
| `desen` | sayı dizisi | Kesik deseni, **kontur kalınlığının katları** |
| `yazi` | metin | Yalnız `yazi-isaretci` için |
| `renk_kilidi` | doğru/yanlış | Katman rengi sembolün rengiyle değişmesin |

**`desen` bir istisnadır ve dikkat ister:** birimi mikrometre değildir. Değerler
kontur kalınlığının **katıdır**, çift sayıda yazılır — çiz, boşluk, çiz, boşluk.
`[7.5, 6.25]` = kalınlığın 7,5 katı çiz, 6,25 katı boşluk bırak.

### 3.6 Tam bir örnek — ÜLKE SINIRI

Ekteki resim şudur: kalın kesikli bir çizgi, kesiklerin arasında kısa dikey
çizikler, aralarda küçük içi boş çemberler.

```json
{
  "id": "ortak-ulke-siniri",
  "ad": "ÜLKE SINIRI",
  "ek": "EK-1a",
  "kaynak": "Mekânsal Planlar Yapım Yönetmeliği (MPYY), EK-1a (RG-22/1/2026-33145), SINIRLAR > İDARİ SINIRLAR",
  "bolum": ["SINIRLAR"],
  "katmanlar": [
    { "tip": "cizgi", "renk": "#FF000000", "kalinlik": 1600, "desen": [7.5, 6.25] },
    { "tip": "tarak-cizgi", "sekil": "cizik", "yerlesim": "aralik",
      "birim": "kagit", "boyut": 4600, "aralik": 22000, "faz": 0,
      "renk": "#FF000000", "kalinlik": 500 },
    { "tip": "tarak-cizgi", "sekil": "cizik", "yerlesim": "aralik",
      "birim": "kagit", "boyut": 4600, "aralik": 22000, "faz": 12000,
      "renk": "#FF000000", "kalinlik": 500 },
    { "tip": "isaretci-cizgi", "sekil": "daire", "yerlesim": "aralik",
      "birim": "kagit", "boyut": 3400, "aralik": 22000, "faz": 17000,
      "renk": "#FF000000", "kalinlik": 300, "dolgu_renk": "#00000000" }
  ],
  "belirsiz": true,
  "belirsiz_nedeni": ["cizim-yorumu"]
}
```

Dikkat edilecek üç şey:

- Yığın **alttan üste** çizilir. Önce ana çizgi, sonra üzerine binenler.
- İki `tarak-cizgi` aynı aralıkta ama farklı `faz` ile: bir tekrarda iki çizik.
- `dolgu_renk: "#00000000"` çemberin içini boş bırakır.

### 3.7 SVG yazılacaksa

Dosya `semboller/` altına, kimliğini anlatan bir adla konur:
`kent-ormani.svg`, `mercan.svg`.

```xml
<?xml version="1.0" encoding="UTF-8"?>
<!-- MPYY EK-1ç · TARIM ALANI. Ek bir ızgaraya dizilmiş içi boş çember basıyor;
     bu, o çemberin basıldığı orandaki hâli. -->
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16" width="16" height="16">
  <circle cx="8" cy="8" r="6" fill="none" stroke="#000000" stroke-width="1"/>
</svg>
```

Kurallar:

- `viewBox` **kare** ya da resmin kendi oranında olur, `width`/`height` ile aynı.
- Renk `#000000`'dır. Yeniden renklendirme çizim tarafında yapılır.
- Metin (`<text>`) **kullanılmaz** — yazı tipi olmayan makinede kaybolur.
  Harf gerekiyorsa yolu (`<path>`) çizilir ya da `yazi-isaretci` kullanılır.
- Başına bir yorum satırı konur: hangi ek, hangi gösterim, ve şeklin ne olduğu.

Sonra `gorseller` dizisine kaydı eklenir:

```json
{ "id": "gorsel-<sha256'nın ilk 16 hanesi>",
  "dosya": "semboller/tarim-alani.svg",
  "sha256": "<tam sha256>",
  "bayt": <dosya boyutu>,
  "tur": "svg" }
```

`sha256` ve `bayt` gerçek dosyanın değerleridir:

```bash
sha256sum data/catalogs/mpyy-vektor/semboller/tarim-alani.svg
stat -c %s data/catalogs/mpyy-vektor/semboller/tarim-alani.svg
```

---

## 4. Yeni bir gösterim nasıl yazılır

Her satır için, sırayla:

1. **Resmî katalogdaki satırı bul.** `data/catalogs/mpyy/plan-gosterim.json`
   satırın kimliğini, adını, ekini ve gömülü görsellerini taşır.
2. **Adı geçen görsel dosyayı aç ve bak.** Ne çizildiğini yaz: kaç çizgi, hangi
   şekil, hangi aralık, dolu mu boş mu.
3. **Şekli katmanlara ayır.** "Kesikli bir çizgi ve üzerinde üçgenler" iki
   katmandır. Karar veremiyorsan resme geri dön; tahmin etme.
4. **Ölçüleri oku.** Resimdeki oranları paftadaki milimetreye çevir. Bir sınır
   çizgisi tipik olarak 0,3–2 mm (`300`–`2000` µm) kalınlıktadır; bir tarama
   aralığı 2–8 mm; bir işaretçi 2–5 mm. Bunlar başlangıç noktasıdır, kural
   değil — resim ne diyorsa o yazılır.
5. **Satırı `plan-gosterim.json` içindeki `stiller` dizisine ekle.** Kimliği ve
   kaynağı resmî katalogdan kopyala.
6. **`scripts/ci-gate-mpyy-vektor.py`'yi çalıştır.** Kusur varsa düzelt; kusur bitmeden ilerleme.
7. **`YAPILACAKLAR.md`'de o satırın kutusunu işaretle** (`- [x]`).

**Bir gösterim çizilemiyorsa** kutusu boş bırakılır ve `ATLANANLAR.md` dosyasına
şu biçimde bir satır yazılır:

```
- `gosterim-kimligi` — en az yirmi karakterlik gerçek bir gerekçe
```

"Zor", "belirsiz", "sonra" gerekçe değildir. Gerekçe, bakan birinin aynı sonuca
varmasını sağlayacak kadar somut olmalıdır: _"EK-1d'deki resim 40×40 piksel ve
sıkıştırma bozuk; şeklin üçgen mi ok mu olduğu ayırt edilemiyor"_.

---

## 5. Doğrulama

Depo kökünden:

```bash
python3 scripts/ci-gate-mpyy-vektor.py
```

Üç şey söyler:

| Çıkış | Anlamı |
|---|---|
| `0` | Paket tutarlı **ve** iş bitmiş |
| `2` | Paket tutarlı ama gösterim kalmış — kaç tane olduğunu yazar |
| `1` | Kusur var. Her kusur kendi kimliğiyle listelenir |

Denetlediği şeyler:

- Her katmanın `tip`, `sekil`, `yerlesim`, `birim` değeri **tanınan** bir
  değerdir. Tanınmayan bir alan adı da kusurdur: çizici onu okumaz, gösterim
  eksik çizilir ve hiçbir yerde hata çıkmaz.
- Renkler `#AARRGGBB`, ölçüler tam sayı, `desen` çift sayıda pozitif sayı.
- Her satır `belirsiz: true` ve `cizim-yorumu` taşır.
- Her `gorsel` gerçekten var olan bir **SVG** dosyasını gösterir.
- **İş listesi doğru söyler.** İşaretli olup pakette karşılığı olmayan her satır
  kimliğiyle raporlanır.

Son madde, bu işin en önemli denetimidir. `YAPILACAKLAR.md`'de bir kutuyu
işaretlemek bir **iddiadır**: "bu gösterime baktım ve karşılığını yazdım".
`scripts/ci-gate-mpyy-vektor.py` her iddiayı pakete karşı sınar. Bu yüzden kutu, satır yazıldıktan
**sonra** işaretlenir — önce değil.

---

## 6. Paket ne zaman tutarlıdır

Kapı **çıkış kodu 0** verdiğinde paket tutarlıdır. Bunun anlamı:

- 476 satırın her biri ya vektörleşmiştir ya `ATLANANLAR.md`'de gerekçelenmiştir,
- `YAPILACAKLAR.md`'deki her işaret pakette karşılığını bulur,
- `semboller/` altında raster dosya yoktur.

Toplu işaretleme yoktur: bir kutu, satırı yazdıktan **sonra** işaretlenir. Kapı
`2` dönerken paket eksiktir ve kaç gösterimin kaldığını kendisi yazar.
