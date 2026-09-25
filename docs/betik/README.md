# Betik Yazma

Tekrarlayan çizim işini otomatikleştirmek isteyen kullanıcı için; bu sayfayı
bitirdiğinizde JSON betiği yazabilecek, koordinat birimini doğru verecek ve hata
durumunda ne olduğunu bileceksiniz.

## Neden betik

Arayüzde yaptığınız her şey bir komuttur ve her komut betikten de çağrılabilir. Bu
demektir ki elle bir kez yaptığınız işi ikinci kez elle yapmak zorunda değilsiniz.

Betikten çalışan bir komut, arayüzden çalışan komuttan **hiçbir biçimde ayırt edilmez**:
aynı doğrulamadan geçer, aynı geri alma yığınına girer, aynı günlüğe yazılır.

## Dosya biçimi

Betik bir JSON dosyasıdır. İki biçim kabul edilir.

### Nesne biçimi

```json
{
  "ad": "Parsel çizimi",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.line",  "args": { "noktalar": [[485300000,4310200000],
                                                  [485360000,4310200000]] } }
  ]
}
```

`ad` alanı geri alma adımının adı olur; **Geri Al**'dan (**Ctrl+Z**) sonra durum çubuğunda
`Geri alındı: <ad>` yazar ve ardından geri almanın ne yaptığı gelir.
Alan isteğe bağlıdır, verilmezse `Betik` kullanılır.

`komutlar` yerine `commands` de yazılabilir.

### Dizi biçimi

Ad gerekmiyorsa doğrudan dizi yazın:

```json
[
  { "cmd": "core.line", "args": { "noktalar": [[0,0],[10000,0]] } },
  { "cmd": "core.line", "args": { "noktalar": [[0,0],[0,10000]] } }
]
```

### Komut satırı

Her satır bir nesnedir:

| Alan | Zorunlu | İçerik |
|---|---|---|
| `cmd` | evet | Komut kimliği veya adı. `komut` da yazılabilir |
| `args` | hayır | Parametre nesnesi. Parametresiz komutlarda `{}` |

`cmd` alanında komut kimliğini (`core.line`) kullanın. Kimlik hiç değişmez; Türkçe ad da
çalışır ama kimlik daha güvenlidir.

## Koordinatlar milimetredir

**Betikte koordinatlar metre değil, milimetre tam sayıdır.** En sık yapılan hata budur.

| Metre | Betikte |
|---|---|
| `485320.150` | `485320150` |
| `4310220.400` | `4310220400` |
| `100.000` | `100000` |
| `0.001` | `1` |

Komut satırında metre yazarsınız, betikte milimetre. Sebebi, betiğin doğrudan çizimin iç
biçimini kullanmasıdır — böylece hiçbir yuvarlama olmaz ve betik her makinede aynı sonucu
verir. Ayrıntı: [Koordinat sistemleri](../veri/koordinat-sistemleri.md).

### Metin olarak koordinat

Bir nokta parametresine **komut satırında yazdığınız biçimde** bir metin de verebilirsiniz:
mutlak (`"485320.150,4310220.400"`), göreli (`"@50,30"`), kutupsal (`"@100<45g"`) ve
[nokta fonksiyonu](../komutlar/komut-satiri.md#nokta-fonksiyonları) (`"dik(0,0,100,0,30,-5)"`,
`"orta(n(1284),n(1285))"`). Bu metinler komut satırının tek gramerinden geçer, dolayısıyla
**metre** cinsindendir; kutupsal açı `açı_kuralı` ve `açı_birimi` ayarlarıyla okunur — komut
satırıyla birebir aynı (bkz. [Komut satırı](../komutlar/komut-satiri.md)). Göreli biçim
listede kendinden önceki noktaya göredir; `son` da odur. Günlüğe yine çözülmüş milimetre
yazılır, fonksiyonun kendisi değil — bu yüzden bir günlük, gramerin ne dediğinden bağımsız
olarak aynı çizimi verir.

```json
{
  "komutlar": [
    { "cmd": "core.line",
      "args": { "noktalar": ["485320.150,4310220.400", "@50,30", "@100<45g"] } },
    { "cmd": "core.line",
      "args": { "noktalar": ["dik(0,0,100,0,30,-5)", "orta(0,0,100,0)"] } }
  ]
}
```

## Argüman değerleri

| Parametre tipi | JSON karşılığı | Örnek |
|---|---|---|
| nokta | İki elemanlı sayı dizisi (milimetre), ya da komut satırı yazımında metin (metre) | `[485320150, 4310220400]` · `"485320.150,4310220.400"` · `"@100<45g"` · `"kes(n(1),n(2),n(3),n(4))"` |
| nokta listesi | Nokta dizisi, ya da metin dizisi | `[[0,0],[10000,0],[10000,10000]]` · `["0,0", "@10,0", "@10<100", "orta(son,@20,0)"]` |
| sayı | JSON sayısı | `1.25` |
| tam sayı | JSON tam sayısı | `4281236786` |
| metin | JSON metni | `"PARSEL"` |
| evet/hayır | JSON boolean | `true` |
| nesne seçimi | Tam sayı dizisi | `[0, 1, 2]` |

## Bir betik = bir geri alma adımı

Betiğin tamamı tek bir işlemdir:

- Kaç komut içerirse içersin **tek `GERİAL`** ile geri alınır; `YİNELE` onu yine tek
  adımda, bıraktığı hâliyle geri getirir
- **Tek doğrulama geçişinden** geçer, bu yüzden büyük betikler hızlı çalışır
- Bir satır başarısız olursa **tamamı geri alınır** — yarım uygulanmış betik bırakılmaz.
  Geri alınan kısım `YİNELE` ile geri getirilemez ve [komut günlüğünde](../mimari/gunluk.md)
  de iz bırakmaz: günlük yeniden oynatıldığında yarım betik geri gelmez. Betiğin açtığı
  katmanlar, tanımladığı sütunlar, blokları ve verdiği renkler de kaldırılır; etkin katman
  ve proje ayarları betikten önceki hâline döner. Kaydedilen dosya, betikten önceki
  kayıtla **bayt bayt aynıdır**
- Var olan bir sütunu silmek ya da tanımını değiştirmek betiğin içinde reddedilir: iş
  yarıda kalırsa geri getirilemezdi ([SÜTUN](../komutlar/column.md#sütunu-silmek))
- Satırlardan biri **bozuksa** — nesne değilse, `cmd` alanı yoksa, `args` okunamıyorsa —
  betik **hiç çalıştırılmaz**; hata satırın numarasını söyler

Son iki madde önemlidir. Yarım uygulanmış bir ifraz veya tevhit kabul edilemez; betik ya
tümüyle uygulanır ya hiç uygulanmaz.

### Betik ne değiştirdiğini söyler

Tamamlanan bir betik, tek adımının çizimde ne yaptığını tek cümlede söyler:

```text
Betik tamamlandı: tests/journal/ornek-parsel.json
Örnek parsel çizimi: 9 komut, tek geri alma adımı — 14 nesne eklendi; 4 katmanın ayarları değişti.
```

Cümle önce eklenen ve silinen nesneleri, sonra değişenleri sayar: yeri ya da biçimi değişen
nesneler, metni değişen yazılar, öznitelik değeri değişen nesneler, katmanı ya da görünüşü
değişen nesneler, ayarı değişen katmanlar, çıktı yerleşimleri, kılavuzlar, blok tanımları
ve koordinat sistemi. **Bağlı nesneler de sayılır:** 500 parseli taşıyıp değerlerini
değiştiren bir betik, parsellerin etiketleri de izlediği için şunu söyler:

```text
Kaydırma: 1000 komut, tek geri alma adımı — 1000 nesnenin yeri ya da biçimi, 500 yazının metni ve 500 nesnenin öznitelik değeri değişti.
```

Aynı betiğin içinde çizilip silinen bir nesne hiçbir yerde sayılmaz. `GERİAL` ve `YİNELE`
de aynı biçimde, **kendilerinin** ne yaptığını ikinci satırda söyler — bir çizgiyi geri
almak bir nesneyi siler:

```text
Geri alındı: İki veya daha fazla nokta arasında doğru parçaları çizer.
Geri almayla 1 nesne silindi.
```

## Çalıştırma

| Yol | Nasıl |
|---|---|
| Komut satırından | `BETİK tests/journal/ornek-parsel.json` |
| Arayüzden | **KentOS CAD ▸ Betik Çalıştır…** veya **Ctrl+R** |
| Açılışta | `kentos_cad --betik <dosya>` |
| Make ile | `make run-script SCRIPT=<dosya>` |

Ayrıntı: [BETİK komutu](../komutlar/script.md).

## Örnek: iki parsel ve bir yol

Bu dosya olduğu gibi çalışır.

```json
{
  "ad": "İki parsel ve yol",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL", "renk": 4281236786 } },
    { "cmd": "core.line",  "args": { "noktalar": [[485300000, 4310200000],
                                                  [485360000, 4310200000],
                                                  [485360000, 4310245000],
                                                  [485300000, 4310245000],
                                                  [485300000, 4310200000]] } },
    { "cmd": "core.line",  "args": { "noktalar": [[485370000, 4310200000],
                                                  [485410000, 4310200000],
                                                  [485410000, 4310245000],
                                                  [485370000, 4310245000],
                                                  [485370000, 4310200000]] } },

    { "cmd": "core.layer", "args": { "ad": "YOL", "renk": 4284310640 } },
    { "cmd": "core.line",  "args": { "noktalar": [[485280000, 4310255000],
                                                  [485420000, 4310255000]] } },
    { "cmd": "core.line",  "args": { "noktalar": [[485280000, 4310265000],
                                                  [485420000, 4310265000]] } },

    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.zoom",  "args": { "mod": "KAPSAM" } }
  ]
}
```

Sekiz komut, on iki nesne, iki katman — ve tek bir **Ctrl+Z**.

Depoda çalışan bir örnek daha var: [`tests/journal/ornek-parsel.json`](../../tests/journal/ornek-parsel.json).

## Örnek: yardımcı çizgileri silmek

Betik bir parsel ve iki yardımcı çizgi çizer, sonra yalnız yardımcıları kimlikleriyle
siler; parsel kalır:

```json
{
  "ad": "Yardımcı çizgileri sil",
  "komutlar": [
    { "cmd": "core.area", "args": { "noktalar": [[0,0],[20000,0],[20000,15000],[0,15000]] } },
    { "cmd": "core.line", "args": { "noktalar": [[0,7500],[20000,7500]] } },
    { "cmd": "core.line", "args": { "noktalar": [[10000,0],[10000,15000]] } },
    { "cmd": "core.erase", "args": { "nesneler": [2, 3] } }
  ]
}
```

Nesne kimliklerini **Komut Günlüğü** panelinden ya da [`SEÇ`](../komutlar/select.md)
komutundan okursunuz; kimlikler **1'den** başlar, yaratılış sırasına göre artar ve
hiçbir zaman yeniden kullanılmaz.

Kimlik saymaktan daha sağlamı seçmektir — argümansız `core.erase` etkin seçimi siler:

```json
{
  "ad": "Kutuya değen her şeyi sil",
  "komutlar": [
    { "cmd": "core.line",   "args": { "noktalar": [[0,0],[10000,0]] } },
    { "cmd": "core.select", "args": { "mod": "KESEN", "noktalar": [[-1000,-1000],[11000,1000]] } },
    { "cmd": "core.erase",  "args": {} }
  ]
}
```

## Kum havuzu

Betiklerin dosya sistemine ve ağa erişimi üç seviyeyle sınırlanır:

| Seviye | İzin | Ne zaman |
|---|---|---|
| `güvenli` | Dosya sistemi yok, ağ yok | Varsayılan; güvenilmeyen betik |
| `proje` | Yalnız proje dizini | KentOSCad uygulamasının kullandığı seviye |
| `tam` | Sınırsız | Kullanıcının açık onayı gerekir |

`güvenli` seviyede bir dosya açmaya çalışan betik şu yanıtı alır:

```text
Betik dosya erişimi 'güvenli' kum havuzunda kapalıdır. Gerekli seviye: 'proje' veya 'tam'.
```

## Günlüğü betiğe çevirmek

**Komut Günlüğü** panelindeki satırlar zaten betik satırlarına çok yakındır:

```json
{"seq":1,"cmd":"core.layer","args":{"ad":"PARSEL","renk":4281236786},"origin":"gui","crs":"TUREF/TM30","katman":"PARSEL"}
```

`cmd` ve `args` alanlarını alıp bir diziye koyarsanız çalışan bir betiğiniz olur:

```json
[
  { "cmd": "core.layer", "args": { "ad": "PARSEL", "renk": 4281236786 } }
]
```

Böylece arayüzde elle yaptığınız bir işi betiğe dönüştürebilirsiniz. Ayrıntı:
[Komut günlüğü](../mimari/gunluk.md).

## Değişken, döngü, koşul gerekiyorsa

Bu sayfanın anlattığı JSON biçimi düz bir komut dizisidir: değişken, döngü, koşul ve
fonksiyon yoktur. Beş yerine beş yüz çizgi çizmek gerektiğinde beş yüz satır yazmanız
gerekir.

Bunun için gömülü **Python** motoru vardır — `KENTOS_WITH_PYTHON=ON` ile derlenir ve
varsayılan yapıda kapalıdır:

```python
for i in range(5):
    y = 4310220.400 + i
    cad.run(f"ÇİZGİ 485320.150,{y:.3f} 485370.150,{y:.3f}")
```

Aynı komut veri yolunu kullanır: bu sayfadaki her kural Python betiği için de geçerlidir.
Ayrıntı: [Python betikleri](python.md).

Nesne başına çalışan bir ifade — bir etiket, bir stil kuralı, bir alan hesabı — oraya
yazılmaz; onun yeri komut satırının kendi ifade motorudur.

## Sırada ne var

- [BETİK komutu](../komutlar/script.md) — komutun kendisi
- [Komut günlüğü](../mimari/gunluk.md) — yaptığınız işi betiğe çevirmek
- [Sorun giderme](../sorun-giderme.md) — betik hata verirse
