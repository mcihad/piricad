# KATMAN — Katman Yönetimi

Çizimini katmanlara ayıran herkes için; bu sayfayı bitirdiğinizde katman yaratmayı, aktif
katmanı değiştirmeyi, görünürlük, kilit ve renk ayarlamayı bileceksiniz.

## Ne yapar

Adı verilen katmanı **yoksa yaratır** ve her hâlükârda **aktif katman yapar**. Bundan
sonra çizdiğiniz her şey bu katmana gider.

İsteğe bağlı parametrelerle katmanın görünürlüğünü, kilidini ve rengini de aynı komutta
değiştirebilirsiniz.

Her çizim `0` adlı katmanla açılır.

## Adlar

| Ad | Tür |
|---|---|
| `KATMAN` | Türkçe, birincil |
| `LAYER` | İngilizce karşılık |
| `KAT` | Kısaltma |
| `core.layer` | Komut kimliği |

## Sözdizimi

```
KATMAN
KATMAN <ad>
KATMAN ad=<ad> [gorunur=<evet|hayır>] [kilitli=<evet|hayır>] [renk=<tamsayı>]
```

Argümansız çağırırsanız komut katman adını sorar.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Katman adı. Zorunlu. Yoksa katman yaratılır, her hâlükârda aktif olur |
| `gorunur` | Katmanın görünürlüğü. `evet` / `hayır` |
| `kilitli` | Katman kilidi. Kilitli katmana çizilemez |
| `renk` | Çizim rengi, `0xAARRGGBB` biçiminde tam sayı |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

Evet/hayır değerleri için `evet`, `hayır`, `yes`, `no`, `true`, `false`, `1`, `0` kabul
edilir.

### Renk değerleri

`renk` bir tam sayıdır ve `0xAARRGGBB` düzenindedir: alfa, kırmızı, yeşil, mavi.

| Renk | Onaltılık | Ondalık |
|---|---|---|
| Yeşil | `0xFF2E7D32` | `4281236786` |
| Gri | `0xFF5D6470` | `4284310640` |
| Turuncu | `0xFFE06C00` | `4292897792` |
| İndigo | `0xFF3949AB` | `4281944491` |

Komut satırından ondalık değeri yazın:

```
KATMAN ad=PARSEL renk=4281236786
```

Katmanın o anki rengini `#AARRGGBB` biçiminde **Öznitelikler** panelinden okuyabilirsiniz.

## Örnekler

### Komut satırı

Katman yarat ve aktif yap:

```
KATMAN ad=PARSEL
```

Yaratırken rengini de ver:

```
KATMAN ad=PARSEL renk=4281236786
```

Var olan bir katmanı gizle:

```
KATMAN ad=YOL gorunur=hayır
```

Kilitle, böylece yanlışlıkla üzerine çizilmesin — ve işiniz bitince aç:

```
KATMAN ad=SINIR kilitli=evet
KATMAN ad=SINIR kilitli=hayır
```

Kilitli bir katmana yeni nesne çizilemez. Kilidi açmayı unutursanız çizim komutu
sizi reddeder ve sebebini söyler.

Adında boşluk olan katman:

```
KATMAN ad="YOL KENARI"
```

Aktif katmanı geri değiştir:

```
KATMAN ad=0
```

### Katman ağacı

Katmanlar bir ağaçta gruplanabilir ve grup çizimin parçasıdır: dosyaya yazılır,
başka makinede geri gelir, paftayı beş yıl sonra açan kişinin ilk okuduğu şeydir.

```
KATMAN ad=PARSEL grup=KADASTRO
KATMAN ad=BINA grup=KADASTRO
KATMAN ad=YOL grup="ULASIM > KARAYOLU"
KATMAN ad=DEMIRYOLU grup="ULASIM > RAYLI"
```

Katmanlar paneli bu ağacı gösterir. Ayraç `>`, sembol rafındakiyle aynı sebeple:
MPYY kendi bölüm yollarını böyle yazıyor ve pakette hiçbir ad bu karakteri
içermiyor.

Bir katmanı kökten çıkarmak için grubu boş verin:

```
KATMAN ad=NOT grup=""
```

### Arayüz

**Katman** araç çubuğundaki **Katman** düğmesi veya **Çizim > Katman** menüsü komutu
başlatır ve katman adını sorar.

Aynı araç çubuğundaki **aktif katman listesi** doğrudan çalışır: renk kutucuklarıyla
katmanları gösterir, seçtiğinizde `KATMAN ad="..."` komutunu gönderir.

Sağdaki **Katmanlar** paneli her katmanı tek bir satırda gösterir: solda **göz**,
yanında renk kutucuğu, katmanın adı, sağda nesne sayısı ve **kilit**.

| Nerede | Ne yapar |
|---|---|
| **Göz** simgesine tek tık | Görünürlüğü ters çevirir |
| **Kilit** simgesine tek tık | Kilidi ters çevirir |
| Satıra çift tık | O katmanı **aktif** yapar |
| Satıra sağ tık | Katman menüsü: **Tümünü seç**, aktif yap, stili düzenle, gruba taşı… |

**Tümünü seç**, o katmandaki bütün nesneleri seçer — çalıştırdığı satır
[`SEÇ mod=KATMAN katman="..."`](select.md) satırıdır.

Çizimde bir nesne seçtiğinizde **panel o nesnenin katmanını kendiliğinden
işaretler**, böylece kırk katmanlı bir listede aramak zorunda kalmazsınız.
Seçimde birden çok katmandan nesne varsa tek doğru cevap olmadığı için işaret
yerinde bırakılır. Bu işaretleme yalnızca listedeki vurgudur: katmanı **aktif
yapmaz**, çünkü aktif katmanı değiştirmek kimsenin istemediği bir düzenlemedir.

Panelden yapılan her değişiklik arka planda `KATMAN` komutunu gönderir. Yani
buradan yaptığınız değişiklik de günlüğe yazılır ve **Ctrl+Z** ile geri alınır.

Aktif katman panelde **kalın** yazılır ve durum çubuğunun **Katman:** bölmesinde görünür.

### Betik

```json
{
  "ad": "Katman kurulumu",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL", "renk": 4281236786 } },
    { "cmd": "core.layer", "args": { "ad": "YOL",    "renk": 4284310640 } },
    { "cmd": "core.layer", "args": { "ad": "BINA",   "renk": 4292897792 } },
    { "cmd": "core.layer", "args": { "ad": "SINIR",  "renk": 4281944491, "kilitli": true } },
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } }
  ]
}
```

Son satır aktif katmanı `PARSEL`'e geri alır, çünkü betikte son çalışan `KATMAN` komutu
aktif katmanı belirler.

## Geri alma

Görünürlük, kilit ve renk değişiklikleri geri alınabilir. `GERİAL` **son** komutu
geri alır, o yüzden örnek neyi geri aldığını kendisi yazar — ve daha önce hangi
durumda olduğunuza bağlı kalmasın diye kendi katmanını kullanır:

```
KATMAN ad=ÖLÇÜ kilitli=evet
GERİAL
KATMAN ad=0
```

`GERİAL`'den sonra `ÖLÇÜ` yeniden düzenlenebilir. Son satır aktif katmanı geri
alır; **aktif katman değişikliği geri alınmaz**, çünkü aktif katman görünüm
durumudur, çizimin verisi değildir.

**Boş bir katman yaratmak geri alınmaz.** Bunun sebebi şudur: boş katman hiçbir şeyi
etkilemez, ama katmanı silmek ona bağlı nesne kimliklerini geçersiz kılardı. Yarattığınız
ama istemediğiniz bir katman çiziminizde zararsız biçimde durur.

Aktif katman değişikliği de geri alınmaz; aktif katman görünüm durumudur, çizimin verisi
değildir.

## Betikten kullanım

`KATMAN` betiklenebilir ve AI erişimlidir. Tipik kullanım, bir betiğin başında bütün
katmanları renkleriyle kurmak, sonra her çizim grubundan önce aktif katmanı değiştirmek:

```json
[
  { "cmd": "core.layer", "args": { "ad": "PARSEL", "renk": 4281236786 } },
  { "cmd": "core.line",  "args": { "noktalar": [[485300000,4310200000],[485360000,4310200000]] } },
  { "cmd": "core.layer", "args": { "ad": "BINA", "renk": 4292897792 } },
  { "cmd": "core.line",  "args": { "noktalar": [[485315000,4310212000],[485345000,4310212000]] } }
]
```

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `'core.layer': zorunlu 'ad' parametresi eksik. Beklenen: metin` | Katman adı verilmemiş | `ad=` ile ad verin |
| `'core.layer': bilinmeyen parametre 'renkler'. Tanımlı parametreler: ad, gorunur, kilitli, renk` | Parametre adı yanlış yazılmış | Doğru adı kullanın |
| `'core.layer': 'gorunur' parametresi evet/hayır bekliyor. Girilen: 'belki'` | Geçersiz evet/hayır değeri | `evet` veya `hayır` yazın |
| `Bilinmeyen katman kimliği: 7` | Var olmayan katmana işlem yapılmaya çalışılmış | Katman adını denetleyin |

Ad vermeden **Esc**'e basarsanız hata olmaz; komut hiç çalışmamış sayılır ve transkriptte
`İptal edildi` yazar.

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
