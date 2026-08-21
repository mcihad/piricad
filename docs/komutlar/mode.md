# MOD — Oturum Modları

Ekranda çizim yapan herkes için; bu sayfayı bitirdiğinizde nesne yakalama, dik mod,
kutupsal izleme ve ızgaraya yakalama gibi **çizerken yardımcı olan** modları
listelemeyi, okumayı ve değiştirmeyi bileceksiniz.

Modlar **çalışır durumdadır**: yazdığınız değer imleci gerçekten yönlendirir.
Nesne yakalama, dik mod, kutupsal izleme ve ızgaraya yakalama; hepsi fareyle
çizerken de, komut satırına koordinat yazarken de, bir betik çizerken de aynı
biçimde uygulanır — çünkü hepsi noktanın üretildiği tek yolda çalışır.

## Ne yapar

`MOD`, **oturum kapsamındaki** ayarları yönetir: çizerken imlecin nereye oturacağını
belirleyen girdi yardımları. Nesne yakalama modları, dik mod, kutupsal izleme açısı ve
ızgaraya yakalama böyledir.

Bunlar çizim dosyasına **yazılmaz**, tercih dosyasına da **yazılmaz** ve programı
kapattığınızda kaybolur. Sebebi tek cümlede şudur: *bir girdi yardımı, çizimin verisi
değildir; çizerken tuttuğunuz cetveldir.* Cetvel masada kalır, çizim gider.

Üç kapsam ve üç komut vardır; hiçbiri diğerinin kapsamına giremez:

| Kapsam | Komut | Nerede yaşar |
|---|---|---|
| Proje | [`AYAR`](setting.md) | Çizim dosyasında; geri alınabilir, günlüğe girer |
| Uygulama | [`TERCİH`](preference.md) | Kullanıcı profilinde; makineye aittir |
| Oturum | `MOD` | Yalnızca bellekte; program kapanınca biter |

Üç kullanım biçimi vardır:

- **Argümansız** — bütün modları, değerleriyle ve varsayılan olup olmadıklarıyla listeler
- **Yalnızca ad** — o modun değerini, kimliğini, türünü, varsayılanını ve kaynağını yazar
- **Ad ve değer** — modu değiştirir ve önceki değerini söyler

## Adlar

| Ad | Tür |
|---|---|
| `MOD` | Türkçe, birincil |
| `MODE` | İngilizce karşılık |
| `MD` | Kısaltma |
| `core.mode` | Komut kimliği |

## Sözdizimi

```
MOD
MOD <ad>
MOD <ad> <deger>
MOD ad=<ad> deger=<deger>
MOD <ad> varsayilan
```

Mod adı yerine kimliği de yazılabilir: `MOD core.yakalama.dik_mod evet`.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Mod adı veya kimliği. Verilmezse bütün modlar listelenir |
| `deger` | Yeni değer. Verilmezse mod yalnızca okunur. `varsayilan` yazarsanız mod bildirilen varsayılanına döner |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

Bugün oturum kapsamında dört mod vardır:

| Mod | Tür | Varsayılan | Ne yapar |
|---|---|---|---|
| `yakalama_modları` | Bit maskesi | `7` | Etkin nesne yakalama modları |
| `dik_mod` | Evet/hayır | `hayır` | İmleci yatay ve düşey eksene kilitler |
| `kutupsal_açı` | µderece | `45000000` (45°) | Kutupsal izleme açı adımı |
| `ızgaraya_yakala` | Evet/hayır | `hayır` | Girilen noktayı en yakın ızgara kesişimine oturtur |

Açı değerleri **mikro derece** cinsindendir: 45° = `45000000`. Ondalık sayı hiçbir ayarda
kabul edilmez; bildirilen birim yeterince incedir.

### Yakalama modları bit maskesi

`yakalama_modları` bir bit maskesidir; istediğiniz modların değerlerini toplarsınız.

| Bit | Değer | Mod | Neye oturur |
|---|---|---|---|
| 0 | `1` | Uç nokta | Bir halkanın köşesine |
| 1 | `2` | Orta nokta | Bir kenarın ortasına |
| 2 | `4` | Merkez | Kapalı bir halkanın ağırlık merkezine |
| 3 | `8` | Kesişim | İki kenarın gerçekten kesiştiği noktaya |
| 4 | `16` | Dik ayak | Önceki noktadan bir kenara indirilen dikin ayağına |
| 5 | `32` | En yakın | Bir kenarın imlece en yakın noktasına |
| 6 | `64` | Izgara | En yakın ızgara kesişimine — `ızgaraya_yakala` da bu biti açar |
| 7 | `128` | Kutupsal | Önceki noktadan çıkan en yakın kutupsal ışına |

Varsayılan `7` = uç nokta + orta nokta + merkez. Onaltılık de yazabilirsiniz:
`MOD yakalama_modları 0x2F`.

`0` bütün nesne yakalamayı kapatır. Kısayolu **F3**'tür.

### Hangi yardım önce uygulanır

Sıra sabittir ve bilerek böyledir:

1. **Nesne yakalama** — gerçek bir nesnenin gerçek bir noktası her şeyi yener
2. **Dik mod / kutupsal izleme** — önceki noktadan gelen yön kilidi
3. **Izgara** — geriye kalan hâlde en yakın kafes kesişimi

Bir parselin köşesine oturmuş noktayı ızgaraya çekmek, ikisinden de olmayan bir yer
üretirdi; bu yüzden birinci adım tuttuğunda diğerleri çalışmaz.

Dik mod ve kutupsal izleme yalnızca **önceki bir nokta varken** iş görür: ilk nokta
kilitlenecek bir yöne sahip değildir.

### Tolerans ve ekran

Yakalama arama yarıçapı `yakalama_toleransı`, seçme kutusu `seçim_toleransı`
tercihidir ve ikisi de **ekran pikselidir** (bkz. [`TERCİH`](preference.md)). Nişan
alan göz ekrana bakar; tolerans yakınlaştırmayla birlikte değişmelidir.

Bunun bir sonucu vardır: **ekranı olmayan bir istemcide nesne yakalama etkisizdir.**
Başsız çalışan bir betik, bir toplu iş ve bir günlük tekrar oynatması yazdıkları
koordinatı aynen çizerler. Bu bir ayrıcalık değil, aynı kuralın (yarıçap = piksel ×
ölçek) ekransız bağlamdaki sonucudur — ve günlüğü dürüst tutan şeydir: kaydedilmiş
bir nokta, o sırada var olmayan bir komşuya sonradan yapışamaz.

Uygulama açıkken çalışan bir betiğin ekranı vardır ve elle çizim ile aynı yakalamayı
alır.

## Örnekler

### Komut satırı

Bütün modları görün:

```
MOD
```

Dik modu açın — yalnız yatay ve düşey çizgi çizilir:

```
MOD dik_mod evet
```

Kutupsal izlemeyi 30 dereceye çekin:

```
MOD kutupsal_açı 30000000
```

Izgaraya yakalamayı açın:

```
MOD ızgaraya_yakala evet
```

Bir modun ne olduğunu sorun:

```
MOD yakalama_modları
```

Bir modu varsayılanına döndürün:

```
MOD dik_mod varsayilan
```

Denemeyi bitirince açtığınız yardımları kapatın; oturum modları siz kapatana kadar
açık kalır:

```
MOD ızgaraya_yakala varsayilan
MOD kutupsal_açı varsayilan
MOD yakalama_modları varsayilan
```

### Arayüz

Komutu pencerenin altındaki **komut satırına** yazın; sonuç **Transkript** panelinde
görünür. Arayüzün ayrıcalığı yoktur: menüden yapılan da, komut satırından yazılan da aynı
komuttur.

**Görünüm** menüsündeki üç kalem ve kısayolları bu komutu çalıştırır; ikinci bir mod
listesi yoktur:

| Kalem | Kısayol | Gönderdiği komut |
|---|---|---|
| Nesne Yakalama | **F3** | `MOD yakalama_modları <maske>` |
| Dik Mod | **F8** | `MOD dik_mod evet` / `hayır` |
| Izgaraya Yakala | **F9** | `MOD ızgaraya_yakala evet` / `hayır` |

Menü kalemlerinin işareti değerin kendisinden okunur: komut satırına
`MOD dik_mod evet` yazdığınızda **F8**'e basmışsınız gibi işaret gelir.

F3 yakalamayı kapatırken maskeyi hatırlar; yeniden açtığınızda seçtiğiniz modlar geri
gelir, varsayılana dönmez.

Bir komut nokta beklerken imlecin altında **yakalama işareti** belirir: her modun
kendi sembolü ve adı vardır — uç nokta kare, orta nokta üçgen, merkez daire, kesişim
çarpı, dik ayak dik açı işareti, en yakın kum saati, ızgara kafes, kutupsal ve dik mod
baklava. Kesikli kılavuz çizgi de yakalanan noktaya uzanır, çünkü çizgi oraya
düşecektir.

### Betik

```json
{
  "ad": "Dik çizim ortamı",
  "komutlar": [
    { "cmd": "core.mode", "args": { "ad": "core.yakalama.dik_mod",     "deger": "evet" } },
    { "cmd": "core.mode", "args": { "ad": "core.yakalama.izgara",      "deger": "evet" } },
    { "cmd": "core.mode", "args": { "ad": "core.yakalama.kutupsal_aci","deger": "30000000" } }
  ]
}
```

## Geri alma

`MOD` **geri alınamaz ve geri alınmamalıdır.** `GERİAL` çizimin verisini geri alır; bir
girdi yardımı çizimin verisi değildir. Aynı sebeple mod değişiklikleri komut günlüğüne de
yazılmaz: günlük, belgeye ne olduğunun kaydıdır.

Geri dönmek için modu varsayılanına döndürün ya da eski değerini yeniden yazın:

```
MOD dik_mod varsayilan
```

Komut, değiştirdiği her modun önceki değerini de yazar; not almak için oradan
okuyabilirsiniz.

## Betikten kullanım

`MOD` betiklenebilir. Bir kurulum betiğinin başında çizim ortamını hazırlamak için
kullanılır:

```json
[
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.dik_mod", "deger": "evet" } },
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.izgara",  "deger": "evet" } }
]
```

Modlar oturumla birlikte biter, bu yüzden bir betiğin açtığı dik mod bir sonraki
oturuma taşınmaz. Kalıcı olmasını istediğiniz şey bir mod değil, bir tercihtir; onlar
[`TERCİH`](preference.md) komutuna aittir.

`MOD` AI erişimine kapalıdır: bir öneri motoru kendi işini kolaylaştırmak için
kullanıcının çizim yardımlarını değiştiremez.

Bir betiğin çizdiği noktaların **hiç** yönlendirilmemesini istiyorsanız, betiğin
başında yakalamayı kapatın:

```json
[
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.modlar",  "deger": "0" } },
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.dik_mod", "deger": "hayır" } },
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.izgara",  "deger": "hayır" } },
  { "cmd": "core.line", "args": { "noktalar": [[485320150,4310220400],[485370150,4310250400]] } }
]
```

Kadastro koordinatını milimetresi milimetresine çizen bir betik için doğru alışkanlık
budur.

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bilinmeyen ayar: 'dikmod'. Beklenen: tanımlı bir ayar kimliği veya adı (23 tanımlı ayar).` | Mod adı yanlış yazılmış | Mesajın devamındaki `Bunu mu demek istediniz:` önerisine bakın veya `MOD` yazıp listeyi görün |
| `'core.crs.id' ayarı proje kapsamındadır; bu komut oturum ayarlarını yönetir.` | Proje ayarı `MOD` ile değiştirilmeye çalışılmış | [`AYAR`](setting.md) komutunu kullanın |
| `'core.arayuz.tema' ayarı uygulama kapsamındadır; bu komut oturum ayarlarını yönetir.` | Tercih `MOD` ile değiştirilmeye çalışılmış | [`TERCİH`](preference.md) komutunu kullanın |
| `'core.yakalama.dik_mod' ayarı evet/hayır bekliyor. Girilen: 'açık'` | Evet/hayır isteyen bir moda başka bir şey verilmiş | `evet`, `hayır`, `1` veya `0` yazın |
| `'core.yakalama.kutupsal_aci' için 400000000 değeri [1000, 360000000] aralığının dışında; 360000000 değerine kırpıldı.` | Değer bildirilen aralığın dışında | Hata değildir: değer aralığa kırpılır ve size söylenir |
| `'core.mode' daha fazla argüman almıyor. Fazlalık: 'evet'` | İkiden fazla argüman verilmiş | Boşluk içeren değerleri tırnak içine alın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
