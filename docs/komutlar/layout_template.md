# PAFTAŞABLON — Pafta Şablonları

Aynı paftayı her işte yeniden kuran herkes için; bu sayfayı bitirdiğinizde kurumun
standart paftasını saklamayı, başka bir çizimde kullanmayı ve meslektaşınıza
göndermeyi bileceksiniz.

## Ne yapar

Bir **pafta** çizimin içindedir ve o çizime aittir. Bir **şablon** ise çizimin
dışındadır: kurumun standart sayfası — antedi, lejant kutusu, ızgara ayarı — her işte
kullanılır, dolayısıyla tek bir işin dosyasında duramaz.

Şablonlar **kullanıcı profilinizde bir klasörde**, her biri kendi JSON dosyasında
durur. Tek bir büyük dosya değil, çünkü bir şablon kendi başına bir belgedir:
postalanır, sürüm denetimine konur, antet değişince yamalanır. "Bana ada paftanı
gönder" demek, "bütün kitaplığını gönder" demek olmamalıdır.

**Şablon düzeni taşır, zemini taşımaz.** Bir şablon sayfanın nasıl *düzenlendiğini*
söyler, nereye *baktığını* değil: Trabzon'daki bir çizimin koordinatlarını Ankara'daki
bir paftaya taşımak, şablonun paftayı yanlış ile hedeflemesidir. Uyguladığınız pafta
harita çerçevesi **hedefsiz** gelir; tuvalden alan seçerek ya da
[`PAFTAÖĞE`](layout_item.md) ile hedeflersiniz.

## Adlar

| Ad | Açıklama |
|---|---|
| `PAFTAŞABLON` | Türkçe birincil ad |
| `PAFTASABLON` | ASCII karşılığı |
| `LAYOUTTEMPLATE` | İngilizce karşılığı |
| `PŞB` / `PSB` | Kısaltma |

## Sözdizimi

```
PAFTAŞABLON islem=listele
PAFTAŞABLON islem=kaydet ad=<şablon adı> [pafta=<pafta>]
PAFTAŞABLON islem=uygula ad=<şablon adı> [pafta=<yeni pafta adı>]
PAFTAŞABLON islem=sil ad=<şablon adı>
```

## Parametreler

| Parametre | Zorunlu | Anlamı |
|---|---|---|
| `islem` | evet | `listele`, `kaydet`, `uygula`, `sil` |
| `ad` | `listele` dışında | **Şablonun** adı |
| `pafta` | hayır | `kaydet`: hangi pafta saklanacak — çizimde tek pafta varsa gerekmez. `uygula`: kurulacak paftanın adı — verilmezse şablonun adı kullanılır |

## Örnekler

### Komut satırı

Kurumun A3 paftasını saklamak:

```
PAFTAŞABLON islem=kaydet ad="Kurum A3" pafta="Kurum Paftası"
```

```
Pafta şablonu kaydedildi: Kurum A3 — 'Kurum Paftası' paftasından, …/paftalar/Kurum A3.pafta.json
```

Başka bir çizimde kullanmak:

```
PAFTAŞABLON islem=uygula ad="Kurum A3" pafta="Ada 900"
```

```
Şablondan pafta kuruldu: Ada 900 — A3 420×297 mm, yatay, 5 öğe
```

Kitaplığı görmek ve bir şablonu atmak:

```
PAFTAŞABLON islem=listele
PAFTAŞABLON islem=sil ad="Kurum A3"
```

### Arayüz

**`Dosya ▸ Paftalar ▸ Şablonlar`** kayıtlı şablonları listeler. Birine tıklamak
paftanın adını sorar, kurar ve **tasarımcıyı açar** — çünkü şablondan gelen bir
paftanın haritası hâlâ hedeflenmeyi bekler.

Aynı alt menüdeki **Paftayı Şablon Olarak Kaydet…** hangi paftanın saklanacağını
(çizimde birden çoksa) ve şablonun adını sorar.

### Betik

```json
[
  { "cmd": "core.layout_template", "args": { "islem": "uygula", "ad": "Kurum A3",
                                             "pafta": "Ada 900" } },
  { "cmd": "core.layout_item", "args": { "islem": "ayarla", "pafta": "Ada 900",
                                         "ad": "harita", "olcek": 1000 } }
]
```

## Geri alma

**`uygula` geri alınabilir**: şablondan kurulan pafta sıradan bir pafta düzenlemesidir,
tek `Ctrl+Z` ile kalkar. `kaydet` ve `sil` ise **çizimi değiştirmez** — kullanıcı
profilindeki bir dosyaya dokunurlar, tıpkı yazdırma profilleri gibi — dolayısıyla geri
alma yığınına girmezler. Silinen bir şablon dosya sisteminden gider.

## Betikten kullanım

Bir çizimi açıp kurumun paftasını uygulayan ve PDF üreten bir betik:

```json
[
  { "cmd": "core.open", "args": { "yol": "ada900.pcad" } },
  { "cmd": "core.layout_template", "args": { "islem": "uygula", "ad": "Kurum A3",
                                             "pafta": "Ada 900" } },
  { "cmd": "core.layout_item", "args": { "islem": "ayarla", "pafta": "Ada 900",
                                         "ad": "harita", "olcek": 1000,
                                         "pencere": [[485200, 4310100], [485420, 4310200]] } },
  { "cmd": "core.print", "args": { "pafta": "Ada 900", "dosya": "ada900.pdf" } }
]
```

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bu yapıda pafta şablonu deposu yok.` | Uygulama katmanı bağlı değil (başsız betik koşucusu) | Şablonlar uygulamadan kullanılır |
| `Şablon adı gerekir: ad=<ad>` | `listele` dışında bir işlem adsız çağrıldı | `ad=` ekleyin |
| `Pafta şablonu yok: 'X'. Olanlar: …` | O adda şablon bulunamadı | `islem=listele` ile adları görün |
| `Çizimde N pafta var; hangisi olduğunu yazın: pafta=<ad>` | `kaydet` birden çok pafta varken çağrıldı | `pafta=` ekleyin |
| `Şablon adı dosya adı olarak kullanılamıyor: 'X'.` | Ad yalnız ayraç ve noktadan oluşuyor | Harf içeren bir ad verin |
| `Pafta şablonu bu sürümden yeni (dosya N, bu sürüm M).` | Şablon ileri bir sürümle yazılmış | Programı güncelleyin |
| `'X' öğesinin türü bu sürümde yok: 'Y'.` | Şablon tanınmayan bir öğe türü taşıyor | Şablonu yazan sürümü kullanın |

Şablon adındaki `/`, `:`, `*` gibi dosya adı olamayacak karakterler **`_` ile
değiştirilir**, reddedilmez: kurum paftasına `18. madde / askı` demek isteyen kimse
engellenmez, ama yazılan dosya klasörün dışına çıkamaz.

## İlgili

- [`PAFTA`](layout.md) — çizimin kendi paftaları
- [`PAFTAÖĞE`](layout_item.md) — paftanın üzerindeki öğeler
