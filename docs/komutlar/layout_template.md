# ÇIKTIŞABLON — Çıktı Yerleşimi Şablonları

Aynı sayfayı her işte yeniden kuran herkes için; bu sayfayı bitirdiğinizde kurumun
standart çıktı yerleşimini saklamayı, başka bir çizimde kullanmayı ve meslektaşınıza
göndermeyi bileceksiniz.

## Ne yapar

Bir **çıktı yerleşimi** çizimin içindedir ve o çizime aittir. Bir **şablon** ise çizimin
dışındadır: kurumun standart sayfası — antedi, lejant kutusu, ızgara ayarı — her işte
kullanılır, dolayısıyla tek bir işin dosyasında duramaz.

Şablonlar **kullanıcı profilinizde bir klasörde** (`…/yerleşimler`), her biri kendi
JSON dosyasında durur. Tek bir büyük dosya değil, çünkü bir şablon kendi başına bir
belgedir: postalanır, sürüm denetimine konur, antet değişince yamalanır. "Bana ada
yerleşimini gönder" demek, "bütün kitaplığını gönder" demek olmamalıdır.

**Şablon düzeni taşır, zemini taşımaz.** Bir şablon sayfanın nasıl *düzenlendiğini*
söyler, nereye *baktığını* değil: Trabzon'daki bir çizimin koordinatlarını Ankara'daki
bir sayfaya taşımak, şablonun yerleşimi yanlış yere hedeflemesidir. Uyguladığınız
yerleşim harita çerçevesi **hedefsiz** gelir; tuvalden alan seçerek ya da
[`ÇIKTIÖĞE`](layout_item.md) ile hedeflersiniz.

## Adlar

| Ad | Açıklama |
|---|---|
| `ÇIKTIŞABLON` | Türkçe birincil ad |
| `CIKTISABLON` | ASCII karşılığı |
| `LAYOUTTEMPLATE` | İngilizce karşılığı |
| `ÇŞB` / `CSB` | Kısaltma |

## Sözdizimi

```
ÇIKTIŞABLON islem=listele
ÇIKTIŞABLON islem=kaydet ad=<şablon adı> [yerlesim=<yerleşim>]
ÇIKTIŞABLON islem=uygula ad=<şablon adı> [yerlesim=<yeni yerleşim adı>]
ÇIKTIŞABLON islem=sil ad=<şablon adı>
```


### Eski ad: `pafta=`

Bu parametrenin adı önceden `pafta` idi. Program **eski adı okur, yeni adı yazar**:
eskiden yazılmış bir betik ya da komut günlüğü aynı işi yapmaya devam eder, ama
programın ürettiği her satır `yerlesim=` der. İkisini bir arada vermek hatadır —
tek argümanın iki yazımı, hangisinin kastedildiğini bilmeyen bir çağrıdır.

## Parametreler

| Parametre | Zorunlu | Anlamı |
|---|---|---|
| `islem` | evet | `listele`, `kaydet`, `uygula`, `sil` |
| `ad` | `listele` dışında | **Şablonun** adı |
| `yerlesim` | hayır | `kaydet`: hangi yerleşim saklanacak — çizimde tek yerleşim varsa gerekmez. `uygula`: kurulacak yerleşimin adı — verilmezse şablonun adı kullanılır |

## Örnekler

### Komut satırı

Kurumun A3 sayfasını saklamak:

```
ÇIKTIŞABLON islem=kaydet ad="Kurum A3" yerlesim="Kurum Sayfası"
```

```
Çıktı yerleşimi şablonu kaydedildi: Kurum A3 — 'Kurum Sayfası' yerleşiminden, …/yerleşimler/Kurum A3.yerleşim.json
```

Başka bir çizimde kullanmak:

```
ÇIKTIŞABLON islem=uygula ad="Kurum A3" yerlesim="Ada 900"
```

```
Şablondan çıktı yerleşimi kuruldu: Ada 900 — A3 420×297 mm, yatay, 5 öğe
```

Kitaplığı görmek ve bir şablonu atmak:

```
ÇIKTIŞABLON islem=listele
ÇIKTIŞABLON islem=sil ad="Kurum A3"
```

### Arayüz

**`Dosya ▸ Çıktı Yerleşimleri ▸ Şablonlar`** kayıtlı şablonları listeler. Birine
tıklamak yerleşimin adını sorar, kurar ve **tasarımcıyı açar** — çünkü şablondan gelen
bir yerleşimin haritası hâlâ hedeflenmeyi bekler.

Aynı alt menüdeki **Yerleşimi Şablon Olarak Kaydet…** hangi yerleşimin saklanacağını
(çizimde birden çoksa) ve şablonun adını sorar.

### Betik

```json
[
  { "cmd": "core.layout_template", "args": { "islem": "uygula", "ad": "Kurum A3",
                                             "yerlesim": "Ada 900" } },
  { "cmd": "core.layout_item", "args": { "islem": "ayarla", "yerlesim": "Ada 900",
                                         "ad": "harita", "olcek": 1000 } }
]
```

## Geri alma

**`uygula` geri alınabilir**: şablondan kurulan yerleşim sıradan bir yerleşim
düzenlemesidir, tek `Ctrl+Z` ile kalkar. `kaydet` ve `sil` ise **çizimi değiştirmez** —
kullanıcı profilindeki bir dosyaya dokunurlar, tıpkı yazdırma profilleri gibi —
dolayısıyla geri alma yığınına girmezler. Silinen bir şablon dosya sisteminden gider.

## Betikten kullanım

Bir çizimi açıp kurumun sayfasını uygulayan ve PDF üreten bir betik:

```json
[
  { "cmd": "core.open", "args": { "yol": "ada900.pcad" } },
  { "cmd": "core.layout_template", "args": { "islem": "uygula", "ad": "Kurum A3",
                                             "yerlesim": "Ada 900" } },
  { "cmd": "core.layout_item", "args": { "islem": "ayarla", "yerlesim": "Ada 900",
                                         "ad": "harita", "olcek": 1000,
                                         "pencere": [[485200, 4310100], [485420, 4310200]] } },
  { "cmd": "core.print", "args": { "yerlesim": "Ada 900", "dosya": "ada900.pdf" } }
]
```

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bu yapıda çıktı yerleşimi şablonu deposu yok.` | Uygulama katmanı bağlı değil (başsız betik koşucusu) | Şablonlar uygulamadan kullanılır |
| `Şablon adı gerekir: ad=<ad>` | `listele` dışında bir işlem adsız çağrıldı | `ad=` ekleyin |
| `Çıktı yerleşimi şablonu yok: 'X'. Olanlar: …` | O adda şablon bulunamadı | `islem=listele` ile adları görün |
| `Çizimde N çıktı yerleşimi var; hangisi olduğunu yazın: yerlesim=<ad>` | `kaydet` birden çok yerleşim varken çağrıldı | `yerlesim=` ekleyin |
| `Şablon adı dosya adı olarak kullanılamıyor: 'X'.` | Ad yalnız ayraç ve noktadan oluşuyor | Harf içeren bir ad verin |
| `Çıktı şablonu bu sürümden yeni (dosya N, bu sürüm M).` | Şablon ileri bir sürümle yazılmış | Programı güncelleyin |
| `'X' öğesinin türü bu sürümde yok: 'Y'.` | Şablon tanınmayan bir öğe türü taşıyor | Şablonu yazan sürümü kullanın |
| `'X' ve 'Y' aynı parametrenin iki adı; ikisi birden verilmez. Yeni adı 'Z'.` | Bir parametrenin eski ve yeni adı birlikte verildi | Yalnız yeni adı bırakın |

Şablon adındaki `/`, `:`, `*` gibi dosya adı olamayacak karakterler **`_` ile
değiştirilir**, reddedilmez: kurum sayfasına `18. madde / askı` demek isteyen kimse
engellenmez, ama yazılan dosya klasörün dışına çıkamaz.

## İlgili

- [`ÇIKTIYERLEŞİMİ`](layout.md) — çizimin kendi yerleşimleri
- [`ÇIKTIÖĞE`](layout_item.md) — yerleşimin üzerindeki öğeler
