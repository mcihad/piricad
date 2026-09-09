# STİLAKTAR — Stili QGIS'e Aktarma

Bir katmanın görünümünü QGIS'e taşıyan herkes için; bu sayfayı bitirdiğinizde
KentOSCad'de kurduğunuz sembolojiyi QML stil dosyası olarak yazmayı bileceksiniz.

> **Faz 0 durumu.** Tek sembollü dışa aktarım çalışıyor: renk, kontur, kalınlık,
> dolgu ve ölçek penceresi QGIS'e taşınır. **Faz 1'de** gelecekler: kategorize
> dışa aktarım (hangi öznitelikle sınıflandığı belgede saklanmadığı için), tarama
> desenleri ve simgeler, ve SLD ile **içe** aktarım.

## Ne yapar

`STİLAKTAR`, bir katmanın sembolojisini **QGIS QML stil dosyası** olarak yazar.
Çizimi değil, çizimin nasıl göründüğünü aktarır — geometri için
[`DIŞAAKTAR`](export.md) kullanın.

İkisi ayrı komuttur ve bu kasıtlıdır: biri geometriyi başka bir biçimde yazar,
öbürü onun nasıl göründüğünü. Tek komuta katlamak, çizimdeki her nesneyi sessizce
yok sayan bir `bicim=QML` demek olurdu.

**Neden QML elle yazılıyor?** Çünkü yazan bir kitaplık yok: QML QGIS'in kendi
biçimidir ve tek yazıcısı QGIS'tir. QGIS'i bağlamak lisans yüzünden değil
(QGIS **GPL-2+**, yani GPLv3 ile uyumlu), mimari yüzünden reddedildi —
`QgsGeometry` ondalık saklar, KentOSCad `int64` milimetre; QGIS sembolojisi
`QPainter`'a çizer, KentOSCad GPU'ya gidiyor; `QgsExpression` ikinci bir
ayrıştırıcıdır ve yasaktır. Kırk satır XML üretmek QGIS'i yeniden yazmak değil,
onunla konuşmaktır.

## Adlar

| Ad | Tür |
|---|---|
| `STİLAKTAR` | Türkçe, birincil |
| `STILAKTAR` | ASCII karşılık |
| `EXPORTSTYLE` | İngilizce karşılık |
| `STAKTAR` | Kısaltma |
| `core.exportstyle` | Komut kimliği |

## Sözdizimi

```text
STİLAKTAR
STİLAKTAR <katman> <dosya>
STİLAKTAR katman=<ad> dosya=<yol>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `katman` | Stili aktarılacak katmanın adı. Katman var olmalı |
| `dosya` | Yazılacak `.qml` dosyasının yolu |

QGIS'te bu dosya **Katman Özellikleri > Stil > Stil Yükle** ile açılır, ya da
katman dosyasının yanına aynı adla konursa kendiliğinden yüklenir.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Ne taşınır, ne taşınmaz

| Taşınır | Nasıl |
|---|---|
| Çizgi rengi | `line_color` / `outline_color`, `r,g,b,a` |
| Çizgi kalınlığı | `line_width` / `outline_width`, milimetre |
| Dolgu rengi | `color`; dolgusuz katman `style=no` olarak gider |
| Kesikli çizgi | `line_style=dash` |
| Sembol yığını | Her katman ayrı bir QGIS sembol katmanı olur, aynı sırayla |
| Ölçek penceresi | `minScale` / `maxScale`, aynı paydalar |

| Taşınmaz | Neden |
|---|---|
| Kategorize sınıflandırma | Hangi öznitelikle sınıflandığı belgede saklanmıyor; Faz 1 |
| Tarama deseni | MPYY tarama atlası bir `/data` varlığı; Faz 1 |
| Simge | Aynı sebep |

Bir katmandaki nesneler **tek bir stil taşımıyorsa** komut bunu söyler ve ilk
nesnenin stilini yazar. Sessizce katman varsayılanını yazmak, ekranda görülenle
eşleşmeyen bir stil dosyası üretirdi.

## Örnekler

### Komut satırı

```
KATMAN ad=KONUT renk=0xFF8C541A
STİL katman=KONUT renk=0xFF5D3A12 kalinlik=700 dolgu=0xFF8C541A
STİLAKTAR KONUT konut.qml
```

Ölçek penceresiyle birlikte — QGIS'te de aynı aralıkta görünür:

```
KATMAN ad=CDP_LEKE
STİL katman=CDP_LEKE renk=0xFF6A1B9A dolgu=0xFFD7B8E8 olcek_min=3000
STİLAKTAR CDP_LEKE cdp.qml
```

### Arayüz

Katmanın **Katman Özellikleri** penceresinde (katmana sağ tıklayın → **Katman
Özellikleri…**) alt banttaki **Stil** menüsünden **QGIS stiline aktar…** deyin.
[Dışa Aktar](../baslangic/disa-aktarma.md) penceresi katman adını doldurur, dosyayı
sorar ve çalıştıracağı `STİLAKTAR` satırını gösterir. Menüden yapılan da komut
satırından yazılan da aynı komuttur.

### Betik

```json
{
  "ad": "Katman stillerini QGIS'e ver",
  "komutlar": [
    { "cmd": "core.exportstyle", "args": { "katman": "PARSEL", "dosya": "parsel.qml" } },
    { "cmd": "core.exportstyle", "args": { "katman": "YOL",    "dosya": "yol.qml" } }
  ]
}
```

## Geri alma

`STİLAKTAR` **geri alınamaz ve geri alınmamalıdır.** Çizime hiçbir şey yapmaz; bir
dosya yazar. `GERİAL` çizimin verisini geri alır, diskteki dosyayı değil.

Yanlış bir stil yazdıysanız stili düzeltip komutu yeniden çalıştırın; dosyanın
üstüne yazılır.

## Betikten kullanım

`STİLAKTAR` betiklenebilir. Bir teslim betiğinin sonunda bütün katmanların
stillerini yanına yazmak tipik kullanımıdır.

**AI erişimine kapalıdır**: bir öneri motorunun kullanıcının diskine dosya
yazması, onay akışının dışına çıkmak olurdu.

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Katman bulunamadı: 'PARSELL'. Önce KATMAN komutuyla oluşturun.` | Katman adı yanlış yazılmış | Adı denetleyin; katman listesi sağdaki panelde |
| `'/yok/olan/dizin/a.qml' yazılamadı. Dizin izinlerini ve boş alanı denetleyin.` | Dizin yok ya da yazma izni yok | Yolu ve izinleri denetleyin |
| `'core.exportstyle' parametresi 'dosya' 1 değer bekliyor.` | Dosya yolu verilmemiş | Katman adından sonra yolu da yazın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
