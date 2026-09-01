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

`ad` alanı geri alma adımının adı olur; **Düzen > Geri Al** menüsünde bunu görürsünüz.
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

## Argüman değerleri

| Parametre tipi | JSON karşılığı | Örnek |
|---|---|---|
| nokta | İki elemanlı sayı dizisi | `[485320150, 4310220400]` |
| nokta listesi | Nokta dizisi | `[[0,0],[10000,0],[10000,10000]]` |
| sayı | JSON sayısı | `1.25` |
| tam sayı | JSON tam sayısı | `4281236786` |
| metin | JSON metni | `"PARSEL"` |
| evet/hayır | JSON boolean | `true` |
| nesne seçimi | Tam sayı dizisi | `[0, 1, 2]` |

## Bir betik = bir geri alma adımı

Betiğin tamamı tek bir işlemdir:

- Kaç komut içerirse içersin **tek `GERİAL`** ile geri alınır
- **Tek doğrulama geçişinden** geçer, bu yüzden büyük betikler hızlı çalışır
- Bir satır başarısız olursa **tamamı geri alınır** — yarım uygulanmış betik bırakılmaz

Son madde önemlidir. Yarım uygulanmış bir ifraz veya tevhit kabul edilemez; betik ya
tümüyle uygulanır ya hiç uygulanmaz.

## Çalıştırma

| Yol | Nasıl |
|---|---|
| Komut satırından | `BETİK tests/journal/ornek-parsel.json` |
| Menüden | **Dosya > Betik Çalıştır…** veya **Ctrl+R** |
| Açılışta | `piricad --betik <dosya>` |
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

## Örnek: bir katmanı temizlemek

```json
{
  "ad": "Yardımcı çizgileri sil",
  "komutlar": [
    { "cmd": "core.erase", "args": { "nesneler": [12, 13, 14] } }
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
| `proje` | Yalnız proje dizini | PiriCAD uygulamasının kullandığı seviye |
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

Bunun için gömülü **Lua** motoru vardır — `PIRICAD_WITH_LUA=ON` ile derlenir ve
varsayılan yapıda kapalıdır:

```lua
for i = 0, 4 do
    h.komut(string.format("ÇİZGİ 485320.150,%.3f 485370.150,%.3f", 4310220.400 + i,
                          4310220.400 + i))
end
```

Aynı komut veri yolunu kullanır: bu sayfadaki her kural Lua betiği için de geçerlidir.
Ayrıntı: [Lua betikleri](lua.md).

Üçüncü bir katman, **Python** (isteğe bağlı modül), Faz 2'de gelecek: eklentiler, toplu
işleme, veri boru hatları ve bilimsel analiz için. Nesne başına çalışan bir ifade orada
değil Lua'da yazılır. Ayrıntı: `CLAUDE.md` Article 8.3.

## Sırada ne var

- [BETİK komutu](../komutlar/script.md) — komutun kendisi
- [Komut günlüğü](../mimari/gunluk.md) — yaptığınız işi betiğe çevirmek
- [Sorun giderme](../sorun-giderme.md) — betik hata verirse
