# AYAR — Proje Ayarları

Bir çizimin koordinat sistemini, hassasiyetini, birimini ve çizim varsayılanlarını
yöneten herkes için; bu sayfayı bitirdiğinizde proje ayarlarını listelemeyi, tek bir
ayarın değerini ve **nereden geldiğini** öğrenmeyi ve güvenle değiştirmeyi bileceksiniz.

> **Faz 0 durumu.** Proje ayarları bugün oturum boyunca yaşar ve komut günlüğüne
> yazılır. Çizim dosyasına kaydedilmeleri, `GERİAL` ile geri alınabilmeleri ve çizimin
> içerik özetine girmeleri **Faz 1'de** gelecek; ayrıntısı `CLAUDE.md` Article 8 ve
> `.claude/model.md` R39'dadır. Bugün varsayılana dönmek için `AYAR <ad> varsayilan`
> kullanın.

## Ne yapar

`AYAR`, **proje kapsamındaki** ayarları yönetir. Proje kapsamı şu kurala göre
belirlenir: *dışa aktarılan bir belgenin baytını değiştirebilen her ayar proje
ayarıdır.* Koordinat sistemi, koordinat cetvelindeki ondalık hane sayısı, çizim birimi,
çizgi tipi ölçeği, varsayılan yazı yüksekliği ve veri paketi sürümü bu yüzden buradadır.

Arayüz teması, dil, otomatik kayıt gibi kullanıcıya ve makineye ait tercihler `AYAR`
ile **değiştirilemez**; onlar [`TERCİH`](preference.md) komutuna aittir. Bu sınır kasıtlıdır:
imzalanan bir belge, kullanıcının koyu temayı seçmesinden etkilenmemelidir.

Üç kullanım biçimi vardır:

- **Argümansız** — bütün proje ayarlarını, değerleriyle ve varsayılan olup olmadıklarıyla listeler
- **Yalnızca ad** — o ayarın değerini, kimliğini, türünü, varsayılanını, aralığını ve **kaynağını** yazar
- **Ad ve değer** — ayarı değiştirir ve önceki değerini söyler

Liste ve açıklamalar ayar bildiriminden üretilir; elle yazılmış bir ayar tablosu yoktur,
bu yüzden kılavuz ile program birbirinden ayrı düşemez.

## Adlar

| Ad | Tür |
|---|---|
| `AYAR` | Türkçe, birincil |
| `SETTING` | İngilizce karşılık |
| `AY` | Kısaltma |
| `core.setting` | Komut kimliği |

## Sözdizimi

```
AYAR
AYAR <ad>
AYAR <ad> <deger>
AYAR ad=<ad> deger=<deger>
AYAR <ad> varsayilan
```

Ayar adı yerine ayar kimliği de yazılabilir: `AYAR core.crs.hassasiyet 4`.

Adlar Türkçe katlanır: `çizgi_tipi_ölçeği`, `cizgi_tipi_olcegi` ve `ltscale` aynı ayarı
açar. Komut satırında Türkçe harf yazmak zorunda kalmamak için ASCII karşılıkları da
tanımlıdır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Ayar adı veya kimliği. Verilmezse bütün proje ayarları listelenir |
| `deger` | Yeni değer. Verilmezse ayar yalnızca okunur. `varsayilan` yazarsanız ayar bildirilen varsayılanına döner |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

Her ayarın türü bildirilmiştir ve değer o türe göre okunur:

| Tür | Kabul edilen yazım |
|---|---|
| Evet/hayır | `evet`, `hayır`, `açık`, `kapalı`, `yes`, `no`, `true`, `false`, `1`, `0` |
| Tam sayı | `4`, `-7`, `0x1F` (onaltılık) |
| Uzunluk | Zeminde tam sayı milimetre: `2500` |
| Metin | Boşluk içeriyorsa tırnak içinde: `"TUREF/TM33"` |
| Seçenek | Seçeneğin adı: `metre` |

**Hiçbir ayar ondalık sayı değildir.** Yarım değer isteyen bir ayar, bildirilmiş bir
birimde tam sayıdır: uzunluk milimetre, açı mikro derece, oran bindedir. Çizgi tipi
ölçeğinin 0,5 olması demek `500` yazmak demektir. Sebebi, ondalık sayıların iki
bilgisayarda aynı baytı vermemesi ve dışa aktarılan belgenin bit düzeyinde aynı olması
gerektiğidir.

## Örnekler

### Komut satırı

Bütün proje ayarlarını görün:

```
AYAR
```

Tek bir ayarın değerini ve nereden geldiğini sorun:

```
AYAR koordinat_hassasiyeti
```

Çıktı, "neden böyle?" sorusunun cevabını verir:

```text
koordinat_hassasiyeti = 3 hane
    kaynak      : varsayılan
    kimlik      : core.crs.hassasiyet
    kapsam      : proje
    tür         : tam sayı
    varsayılan  : 3 hane
    aralık      : 0 .. 6
```

Projenin koordinat sistemini değiştirin:

```
AYAR koordinat_sistemi "TUREF/TM33"
```

Koordinat cetvelindeki ondalık hane sayısını dörde çıkarın:

```
AYAR koordinat_hassasiyeti 4
```

Çizim birimini seçin:

```
AYAR cizim_birimi metre
```

Çizgi tipi ölçeğini yarıya indirin — binde cinsinden, yani `500`:

```
AYAR ltscale 500
```

Varsayılan yazı yüksekliğini 3 metre yapın (zeminde milimetre):

```
AYAR metin_yuksekligi 3000
```

Bir ayarı bildirilen varsayılanına döndürün:

```
AYAR koordinat_hassasiyeti varsayilan
```

Kimlikle de çalışır, betiklerde tercih edilen yazım budur:

```
AYAR core.crs.hassasiyet 3
```

### Arayüz

Komutu pencerenin altındaki **komut satırına** yazın; arayüzün hiçbir ayrıcalığı yoktur,
aynı komut aynı yolu izler. Sonuç **Transkript** panelinde görünür.

Koordinat sistemi durum çubuğunun sağ ucunda yazar; `AYAR koordinat_sistemi` ile
değiştirdiğinizde oradan doğrular.

**Ayarlar** penceresi (menüde `Düzen > Ayarlar…`, kısayolu **Ctrl+,**) bildirilen her
ayarı gösterir. Pencerenin tamamı ayar kataloğundan **üretilir**: satırın adı ayarın
kendi birincil adı, alanı bildirilen tipinden, sınırları bildirilen aralığından,
üzerine gelince çıkan açıklaması bildirilen özetinden gelir. Kataloğa eklenen bir ayar
bu pencereye kendiliğinden düşer.

Üç sekme, üç kapsam: **Proje** çizimle birlikte giden ayarlar, **Uygulama** bu
bilgisayardaki tercihleriniz, **Oturum** yalnız bu açık pencere için geçerli olanlar.
Her satırın sağında değerin sizin mi yoksa programın mı olduğu (`ayarlanmış` /
`varsayılan`) ve varsayılana döndüren bir düğme vardır. Üstteki arama kutusu ad,
kimlik ve açıklama üzerinde birden arar.

Penceredeki her değişiklik komut yolundan geçer: kapsamına göre `AYAR`, `TERCİH` ya da
`MOD` komutu kurulup çalıştırılır. Yani transkriptte, günlükte ve yeniden oynatmada
pencereden yapılanla komut satırına yazılan arasında hiçbir fark yoktur.

### Betik

```json
{
  "ad": "Proje kurulumu",
  "komutlar": [
    { "cmd": "core.setting", "args": { "ad": "core.crs.id",           "deger": "TUREF/TM33" } },
    { "cmd": "core.setting", "args": { "ad": "core.crs.hassasiyet",   "deger": "4" } },
    { "cmd": "core.setting", "args": { "ad": "core.cizim.birim",      "deger": "metre" } },
    { "cmd": "core.setting", "args": { "ad": "core.katalog.paket_surumu", "deger": "0.1.0" } }
  ]
}
```

Bir betiğin tamamı tek geri alma adımıdır; ayarlar da bu adımın içindedir.

## Geri alma

`AYAR` bir proje ayarını değiştirir ve bu değişiklik komut günlüğüne yazılır: ne
yaptığınız, hangi ayarı hangi değere çektiğiniz kayıtlıdır. Günlüğü
[Komut günlüğü](../mimari/gunluk.md) sayfasından okuyabilirsiniz.

**Faz 0'da `GERİAL` bir ayar değişikliğini geri almaz.** Sebebi, proje ayarlarının henüz
çizim belgesinin içinde tutulmamasıdır; belgeye taşındıklarında bir ayar değişikliği tam
olarak bir geri alma adımı olacaktır (`.claude/model.md` R39, `CLAUDE.md` Article 8).

Bugün geri dönmenin yolu, ayarı varsayılanına döndürmek ya da eski değerini yeniden
yazmaktır:

```
AYAR koordinat_hassasiyeti varsayilan
```

Değiştirmeden önce eski değeri `AYAR <ad>` ile okuyup not almak iyi bir alışkanlıktır;
komut zaten değişiklikten sonra önceki değeri de yazar.

## Betikten kullanım

`AYAR` betiklenebilir. Tipik kullanım, bir işin başında projenin koordinat sistemini,
hassasiyetini ve birimini kurmak, sonra çizime geçmektir:

```json
[
  { "cmd": "core.setting", "args": { "ad": "core.crs.id",         "deger": "TUREF/TM30" } },
  { "cmd": "core.setting", "args": { "ad": "core.crs.hassasiyet", "deger": "3" } },
  { "cmd": "core.layer",   "args": { "ad": "PARSEL" } },
  { "cmd": "core.line",    "args": { "noktalar": [[485300000,4310200000],[485360000,4310200000]] } }
]
```

`AYAR` **AI erişimine kapalıdır.** Koordinat sistemini değiştirmek çizimdeki bütün
koordinatları yeniden yorumlamak demektir; bunu yalnızca yetkili bir mühendis yapar
(`kentoscad.md` §5.1).

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bilinmeyen ayar: 'hasasiyet'. Beklenen: tanımlı bir ayar kimliği veya adı (23 tanımlı ayar).` | Ayar adı yanlış yazılmış | Mesajın devamındaki `Bunu mu demek istediniz:` önerisine bakın veya `AYAR` yazıp listeyi görün |
| `'core.arayuz.tema' ayarı uygulama kapsamındadır; bu komut proje ayarlarını yönetir.` | Uygulama tercihi `AYAR` ile değiştirilmeye çalışılmış | [`TERCİH`](preference.md) komutunu kullanın |
| `'core.yakalama.dik_mod' ayarı oturum kapsamındadır; bu komut proje ayarlarını yönetir.` | Oturum ayarı `AYAR` ile değiştirilmeye çalışılmış | Dik mod ve yakalama çizimin verisi değildir, kaydedilmezler; oturum ayarlarının kendi komutu **Faz 1'de** gelecek |
| `'core.crs.hassasiyet' ayarı tam sayı bekliyor. Girilen: '0.500000'` | Tam sayı isteyen bir ayara ondalık verilmiş | Bildirilen birimde tam sayı yazın; oran isteyen ayarlarda binde kullanın |
| `'core.cizim.birim' ayarı şu seçeneklerden birini bekliyor: milimetre, santimetre, metre. Girilen: 'fersah'` | Listede olmayan bir seçenek yazılmış | Mesajın saydığı seçeneklerden birini yazın |
| `'core.crs.hassasiyet' için 9 değeri [0, 6] aralığının dışında; 6 değerine kırpıldı.` | Değer bildirilen aralığın dışında | Hata değildir: değer aralığa kırpılır ve size söylenir. Başka bir sürümde yazılmış dosya bu yüzden açılmaz olmaz |
| `Metin ayarı en çok 47 bayt alır. Girilen: 60 bayt.` | Metin ayarı kapasiteyi aşmış | Kısaltın. Metin kırpılmaz, reddedilir: yarısı kesilmiş bir koordinat sistemi kimliği yanlış bir koordinat sistemidir |
| `'core.setting' daha fazla argüman almıyor. Fazlalık: 'metre'` | İkiden fazla argüman verilmiş | Boşluk içeren değerleri tırnak içine alın: `AYAR koordinat_sistemi "TUREF/TM33"` |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
