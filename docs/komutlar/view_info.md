# GÖRÜNÜMBİLGİSİ — Ekranda Görünen Alan

Ekranda tam olarak hangi alanı, hangi ölçekte ve hangi koordinat sisteminde
gördüğünüzü öğrenmek isteyen herkes için; bu sayfayı bitirdiğinizde görünüm bilgisini
komut satırından ve betikten okumayı, başsız çalıştırmada neden "pencere yok" cevabını
aldığınızı bileceksiniz.

## Ne yapar

Ekranda görünen dikdörtgenin **iki köşesini**, **merkezini**, **ölçek paydasını**, bir
pikselin kaç milimetreye denk geldiğini, **tuvalin piksel boyutunu** ve çizimin
**koordinat sistemini** bildirir.

Görünümü değiştirmez: yakınlaştırmaz, kaydırmaz. Onlar [`YAKINLAŞ`](zoom.md) ve
[`KAYDIR`](pan.md) işidir.

Söylediği ölçek, **durum çubuğunda yazan ölçeğin kendisidir**: aynı kaynaktan okunur, o
ekranın kendi DPI'ında hesaplanır. Yani pencereyi büyütmek ya da ekranı değiştirmek bu
sayıyı değiştirir, çünkü 1/1000 kâğıt üzerinde bir orandır ve ekran da bir yüzeydir.

**Görüntü penceresi yoksa bunu açıkça söyler.** Başsız bir çalıştırmada, bir günlük
yeniden oynatmasında ve bir testte gerçekten pencere yoktur; böyle bir durumda bir
dikdörtgen uydurmak, bir ajanın sonraki çizimini kimsenin bakmadığı bir yere koymak
olurdu.

Çizimi değiştirmediği için bir ajan onu **onay beklemeden** çalıştırabilir, ve görünen
dikdörtgeni bir **pencere tutamağı** olarak alır. Bu, sayıları okunabilen tek tutamak
türüdür ve sebebi açıktır: bunlar ekranın kendi köşeleridir, çizimin içeriği değil
([Onay ve denetim](../yapay-zeka/onay.md)).

Ekranın **ortası** da ayrıca bir **nokta tutamağı** olarak gelir ("görünümün ortası").
Kullanıcı bir yer söylemediğinde bir ajanın yeni çizimi oraya yapması bu yüzdendir:
"bir altıgen çiz" isteği, merkezi bu tutamak olan bir [`ÇOKGEN`](polygon_regular.md)
önerisine dönüşür.

## Adlar

| Ad | Tür |
|---|---|
| `GÖRÜNÜMBİLGİSİ` | Türkçe, birincil |
| `GORUNUMBILGISI` | Türkçe, ASCII katlanmış |
| `VIEWINFO` | İngilizce karşılık |
| `GRB` | Kısaltma |
| `core.view_info` | Komut kimliği |
| `gorunum_bilgisi` | Ajan arayüzündeki araç adı |

## Sözdizimi

```text
GÖRÜNÜMBİLGİSİ
```

## Parametreler

Parametre almaz. Argümansız çalışır ve hiçbir şey sormaz.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```
GÖRÜNÜMBİLGİSİ
```

Uygulamanın içinde, harita ekranda dururken:

```text
Görünüm: Sağa 485320,000 – 485420,000 m, Yukarı 4310220,000 – 4310300,000 m; ölçek 1:1000
```

Sayılar **metredir** ve üç ondalıkla yazılır — çünkü çizim milimetre saklar ve üçüncü
ondalık tam olarak o milimetredir. Ondalık ayırıcı virgüldür.

Pencere olmadan çalıştırıldığında (başsız çalıştırma, günlük oynatma, test):

```text
Bu çalıştırmada görüntü penceresi yok (başsız çalışıyor).
```

Kısaltmayla aynı iş:

```
GRB
```

### Arayüz

Aynı iki bilgi **durum çubuğunda** sürekli yazılıdır: ölçek `1:N` olarak ve koordinat
sistemi `EPSG:…` olarak. İmlecin o andaki konumu da oradadır.

Komutun kendisini arayüzden çalıştırmak için şeritte **Harita** sekmesinin sonundaki
**Diğer** listesinden **Görünüm Bilgisi**'ni seçin ya da **Ctrl+K** ile komut aramayı açıp
`GÖRÜNÜMBİLGİSİ` yazın; ad komut satırına yerleşir, **Enter** çalıştırır. Köşe
koordinatlarını okumak istediğinizde asıl sebebi budur: durum çubuğu ölçeği söyler,
görünen dikdörtgenin köşelerini söylemez.

Görünümü değiştirmek için şeritteki **Görünüm ▸ Gezinme** düğmeleri, tekerlek ve
[`YAKINLAŞ`](zoom.md) vardır.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.view_info", "args": {} }
```

Bir projeyi açıp görünümü bildirmek:

```json
{
  "ad": "Görünüm bilgisi",
  "komutlar": [
    { "cmd": "core.open", "args": { "dosya": "veri/ada128.pcad" } },
    { "cmd": "core.view_info", "args": {} }
  ]
}
```

## Geri alma

`GÖRÜNÜMBİLGİSİ` çizime dokunmaz: geri alınacak bir şey yoktur ve [`GERİAL`](undo.md)
listesine girmez. Görünüm zaten çizimin verisi değildir; kaydedilmez ve geri alınmaz.

**Komut günlüğüne de girmez**: salt okunur bir komut günlüğe yazılmaz, çünkü günlük
çizimin tarihidir ve bir soru çizimin tarihinin parçası değildir.

## Betikten kullanım

Komut betiklerde `core.view_info` kimliğiyle çağrılır ve argüman almaz.

Betiğe ve ajana dönen yapı şu alanları taşır:

| Alan | Ne taşır |
|---|---|
| `kutu_mm` | Görünen dikdörtgenin dört sayısı, tam sayı milimetre: en küçük `Sağa (Y)`, en küçük `Yukarı (X)`, en büyük `Sağa (Y)`, en büyük `Yukarı (X)` |
| `merkez_mm` | Görünümün merkezi: `Sağa (Y)`, `Yukarı (X)`, tam sayı milimetre |
| `olcek` | Ölçek paydası; 1000 demek 1/1000 demektir |
| `mm_piksel` | Bir ekran pikselinin kaç milimetreye denk geldiği |
| `ekran_px` | Tuvalin genişliği ve yüksekliği, piksel |
| `crs` | Koordinat sistemi kimliği |

Pencere yoksa **yapı hiç dönmez**: komut yalnız yukarıdaki Türkçe satırı yazar ve
başarıyla sonlanır. Bir istemci bunu bir hata değil, "bu çalıştırmada görünüm diye bir
şey yok" cevabı olarak okumalıdır.

Bir ajan bu bilgiyi [MCP sunucusu](../yapay-zeka/mcp-sunucusu.md) üzerinden
`gorunum_bilgisi` adıyla çağırır ve genellikle ilk çağrısı budur: nerede olduğunu
bilmeden hiçbir şey soramaz.

## Hatalar

Komutun kendi reddettiği bir durum yoktur; pencerenin olmaması bir hata değil,
bildirilen bir olgudur. Yanlış yazılan bir argüman veri yolunun doğrulamasına takılır:

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `'core.view_info': bilinmeyen parametre 'olcek'. Tanımlı parametreler: (yok)` | Komuta argüman verildi | Argümanı silin; komut tek başına çalışır |
| `Bilinmeyen komut: 'GORUNUM'. YARDIM yazarak komut listesini görün.` | Ad yanlış yazıldı | `GÖRÜNÜMBİLGİSİ`, `GORUNUMBILGISI`, `VIEWINFO` ya da `GRB` yazın |

## İlgili

- [`YAKINLAŞ`](zoom.md) — görünümü ayarlama
- [`KAYDIR`](pan.md) — görünümü kaydırma
- [`KOORDİNAT`](coordinate.md) — tek bir noktanın koordinatını okuma
- [Koordinat sistemleri](../veri/koordinat-sistemleri.md) — TUREF/TM30, eksen adları, milimetre
- [Yapay zeka ve ajanlar](../yapay-zeka/README.md) — tutamak kuralı ve okuma sırası
