# KALEMBAĞLA — Nesneleri Sınıfa Bağla

Önceden çizilmiş, katmanı ve öznitelikleri kendi başına kurulmuş bir paftayı bir sayısallaştırma
sınıfına geçirmek isteyenler için: üç yüz binayı tek tek `KATMANAT` ve `ÖZNİTELİK` ile düzeltmek
yerine seçip "bunlar **Bina**" demek. Bu sayfayı bitirdiğinizde önce önizleyip sonra tek adımda
bağlamayı, eski sütundaki değerleri sınıfın alanlarına taşımayı ve uymayan nesnenin neden atlandığını
bileceksiniz.

## Ne yapar

`KALEMBAĞLA`, verilen nesneleri bir sınıfa bağlar:

1. **Sınıfı kurar** ([KALEM](feature_class.md) ile aynı işi yapar): katman yoksa yaratılır,
   sınıfın alanları sütun olarak tanımlanır, katman sınıfı izlemeye başlar.
2. **Nesneleri sınıfın katmanına taşır.**
3. **Eski sütunları sınıfın alanlarına taşır**, `esle=` ile söylediğiniz kadarını (aşağıda).
4. **Boş kalan hücreleri sınıfın başlangıç değerleriyle doldurur** — çizilen bir nesneyle aynı yolu
   izler: sınıf katmanına gelen nesneye, nasıl geldiyse gelsin, aynı kural uygulanır.

**Hangi nesneler:** `nesneler=` verilmişse onlar; yoksa `katman=` ile söylenen katmanın bütün
nesneleri; o da yoksa çizimdeki **seçim**. Üçü de yoksa komut ne yapacağını bilemez ve reddeder.

**Uymayan nesne atlanır, hata sayılmaz.** Bir seçimde bina ile çit birlikte olabilir; sınıf
`kapalı alan` ister, açık çizgi bu sınıfa girmez. Atlananlar özette **geometri türüyle birlikte
sayılır ve ilk beşinin kimliği yazılır**: sessiz bir kayıp yoktur. Kilitli katmandaki nesne de
atlanır ve sayılır. Hiçbir nesne bağlanamıyorsa komut nedenini söyleyerek reddedilir.

**Eşleme (`esle=`).** Eski bir sütunun değerini sınıfın bir alanına taşır:
`esle=kat:kat_sayisi` "`kat` sütununu `kat_sayisi` alanına taşı" demektir. İki koşul vardır:
iki sütun **aynı türde** olmalıdır ya da hedef bir **metin** alanı olmalıdır (sayı, tablonun
gösterdiği yazıyla metne taşınır). Başka bir dönüşüm uydurulmaz: metni sayıya çevirmek bir
karardır, eşleme onu sessizce vermez. Kaynak hücre boşsa taşınacak bir şey yoktur ve hedef hücre
sınıfın başlangıç değerini alır. `esle=` birden çok kez verilebilir.

**Aynı adlı ortak sütunlar** (örneğin projede zaten var olan `ada`) eşleme istemeden korunur:
sınıf aynı kimlikli bir alan taşıyorsa nesnenin hücresi yerinde durur.

**Önizleme.** `onizle=evet` hiçbir şey yazmaz — katman yaratılmaz, sütun tanımlanmaz, nesne
taşınmaz — ve tam olarak ne olacağını söyler. Önizleme ve gerçek çağrı aynı kodu çalıştırır.

## Adlar

| Türkçe | Tür |
|---|---|
| `KALEMBAĞLA` | Türkçe, birincil |
| `KALEMBAGLA` | ASCII karşılık |
| `SINIFABAĞLA` | Türkçe eşanlamlı |
| `KLB` | Kısaltma |
| `BINDCLASS` | İngilizce karşılık |
| `core.feature_class_bind` | Komut kimliği |

## Sözdizimi

```text
KALEMBAĞLA ad=<sınıf> [nesneler=<k1> nesneler=<k2> …] [katman=<ad>] [esle=<eski>:<alan> …] [onizle=evet]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Bağlanacağı sınıfın kimliği ya da adı |
| `nesneler` | Bağlanacak nesnelerin kalıcı kimlikleri; verilirse `katman` ve seçim yok sayılır |
| `katman` | Bu katmanın bütün nesneleri bağlanır |
| `esle` | `eski_sutun:sinif_alani`. Eski bir sütunun değerini sınıfın alanına taşır; aynı türde ya da metin alanına. Birden çok kez verilebilir |
| `onizle` | `evet` ise hiçbir şey yazılmaz. Varsayılan `hayır` |

## Örnekler

### Komut satırı

Eski bir katmanda iki alan ve bir çizgi var; ayrıca eski bir `kat` sütunu:

```
KATMAN ad=ESKI_CIZIM
ALAN noktalar=0,0 10,0 10,10 0,10
ALAN noktalar=20,0 30,0 30,10 20,10
ÇİZGİ 0,20 10,20
SÜTUN kimlik=kat tur=tam_sayi
ÖZNİTELİK kat 1 3
```

Önce önizleyin:

```
KALEMBAĞLA ad=bina nesneler=1 nesneler=2 nesneler=3 esle=kat:kat_sayisi onizle=evet
```

```text
Önizleme (hiçbir şey yazılmadı): 2 nesne 'Bina' sınıfına bağlanacak (katman BINA, yeni)
  kapsam: verilen 3 nesne
  Uymayan, atlandı: 1 açık çizgi — sınıf kapalı alan ister. İlk nesneler: 3 (açık çizgi)
  Alan eşlemesi: kat → kat_sayisi
  Boş kalan alanlar şunlarla başlar: sinif_kodu=BNA · kat_sayisi=1 · yapi_turu=Betonarme
```

Beğendiyseniz aynı satırı `onizle` olmadan gönderin:

```
KALEMBAĞLA ad=bina nesneler=1 nesneler=2 nesneler=3 esle=kat:kat_sayisi
```

```text
Bağlandı: 2 nesne 'Bina' sınıfına bağlandı (katman BINA, yeni)
  kapsam: verilen 3 nesne
  Uymayan, atlandı: 1 açık çizgi — sınıf kapalı alan ister. İlk nesneler: 3 (açık çizgi)
  Alan eşlemesi: kat → kat_sayisi (1 hücre taşındı)
  Boş kalan alanlar şunlarla başlar: sinif_kodu=BNA · kat_sayisi=1 · yapi_turu=Betonarme
```

1 numaralı binanın `kat_sayisi` değeri `3` (eski sütundan), 2 numaranınki `1` (taşınacak değer yoktu,
sınıfın başlangıç değeri). Çizgi eski katmanında kaldı.

Bir katmanın hepsini ya da seçimi de bağlayabilirsiniz; seçimi `SEÇ` ile yapın:

```
KATMAN ad=ESKI_YOLLAR
ÇİZGİ 0,40 50,40
ÇİZGİ 0,45 50,45
SEÇ NESNE nesneler=4 nesneler=5
KALEMBAĞLA ad=yol_ekseni
```

### Arayüz

**Harita ▸ Kalem** panelinde önce listeden sınıfı seçin, sonra nesneleri seçin ve **Seçimi bağla**'ya
basın: `KALEMBAĞLA ad=<sınıf>` komutu seçimle çalışır. Kalem seçilmemişse transkript "Önce bir kalem
seçin" der ve hiçbir şey olmaz. Önizleme ve alan eşlemesi komut satırından yapılır (panel iki
seçeneği taşımaz).

### Betik

```json
{
  "ad": "Eski çizgiyi yol eksenine bağla",
  "komutlar": [
    { "cmd": "core.line", "args": { "noktalar": [[0,0],[50000,0]] } },
    { "cmd": "core.feature_class_bind", "args": { "ad": "yol_ekseni", "nesneler": [1] } }
  ]
}
```

## Geri alma

Tek adım: nesneler eski katmanına döner, taşınan ve doldurulan hücreler gider, katmanın sınıfı
izlemesi kalkar. Eski sütunun kendisine hiç dokunulmadığı için olduğu gibi durur. Tanımlanan
sütunlar ve yaratılan katman, [KALEM](feature_class.md) sayfasında anlatıldığı gibi yerinde kalır.
Önizleme geri alma adımı bırakmaz.

## Betikten kullanım

Komut kimliği `core.feature_class_bind`; Python'dan
`cad.feature_class_bind(name="bina", layer="ESKI_CIZIM", map=["kat:kat_sayisi"], preview=True)`.
Öbür adlar `objects`. Yapılandırılmış sonuç [KALEM](feature_class.md) sonucunun alanlarına ek
olarak şunları taşır:

| Alan | Ne |
|---|---|
| `onizleme` | `true`: hiçbir şey yazılmadı |
| `kapsam` | Hangi nesneler, bir cümleyle |
| `baglanan` | Bağlanan (ya da bağlanacak) nesne sayısı |
| `zaten` | Zaten sınıfın katmanında olan |
| `uymayan` | Geometrisi sınıfa uymadığı için atlanan |
| `kilitli` | Kilitli ya da salt görüntü katmanda olduğu için atlanan |
| `tasinan_hucre` | `esle=` ile taşınan hücre sayısı |

Yapay zekâ bu komutu çağırabilir; her öneri gibi önizlenir ve onaylanır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bağlanacak nesne yok. nesneler=, katman= ya da bir seçimle söyleyin.` | Nesne, katman ve seçim boş | Birini verin |
| `Bilinmeyen sınıf: '<ad>'. Tanımlı sınıflar: …` | Sınıf paketde yok | [KALEM](feature_class.md) ile listeyi görün |
| `Bilinmeyen nesne: N. Nesne kimliklerini SEÇ ile görebilirsiniz.` | `nesneler` çizimde olmayan bir kimlik | Kimliği [SEÇ](select.md) ile öğrenin |
| `Bilinmeyen katman: '<ad>'.` | `katman` çizimde yok | Katman adını doğrulayın |
| `esle 'eski_sutun:sinif_alani' biçiminde yazılır; verilen: '<…>'.` | İki nokta üst üste yok | `esle=kat:kat_sayisi` |
| `esle: belgede '<ad>' sütunu yok.` | Kaynak sütun tanımlı değil | Sütun kimliğini [SÜTUN](column.md) listesinden doğrulayın |
| `esle: '<sınıf>' sınıfının '<alan>' alanı yok. Alanları: …` | Hedef alan sınıfta yok | Sınıfın alanlarından birini yazın |
| `esle: '<a>' (<tür>) '<b>' (<tür>) alanına eşlenemez; aynı türde ya da metin alanına eşleyin.` | Tür dönüşümü gerekiyor | Aynı türde bir alan seçin ya da metin alanına eşleyin |
| `Sınıfa bağlanabilecek nesne yok: N nesne '<sınıf>' sınıfının <şekil> şekline uymuyor. Önizlemek için onizle=evet.` / `… hepsi zaten katmanda ya da kilitli …` | Hiçbir nesne bağlanamıyor | Doğru sınıfı seçin ya da kilidi açın |
| `'<katman>' katmanı zaten '<sınıf>' sınıfını izliyor. …` ve [KALEM](feature_class.md) sayfasındaki öbür sınıf kurma hataları | Sınıf kurulamıyor | Orada anlatılan çözümü uygulayın |

## İlgili

- [KALEM](feature_class.md) — sınıfı seçer ve çizim için kurar
- [KALEMDENETİM](feature_class_check.md) — bağlanan nesnelerin hâlâ sınıfa uyup uymadığına bakar
- [Kalemle sayısallaştırma](../baslangic/kalemle-sayisallastirma.md)
- [KATMANAT](set_layer.md), [ÖZNİTELİK](attribute.md)
