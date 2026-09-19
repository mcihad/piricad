# SORGULA — Koşula Uyan Nesneleri Sayma

Bir katmanda kaç nesne olduğunu, bir özniteliğin belirli bir değerini taşıyan nesnelerin
hangileri olduğunu ve o nesnelerin çizimde nereye düştüğünü öğrenmek isteyen herkes için;
bu sayfayı bitirdiğinizde sorguyu komut satırından ve betikten çalıştırmayı, sınırın ne
anlama geldiğini ve neden bir ifade dili olmadığını bileceksiniz.

## Ne yapar

Verilen koşula uyan nesneleri **sayar**, en çok `sinir` kadarının **anahtarını** bildirir
ve bildirdiklerinin **kapsayan dikdörtgenini** verir.

Koşul en çok üç parçadan oluşur ve hepsi isteğe bağlıdır:

- **`katman`** — yalnız o katmana bakar. Verilmezse bütün çizime bakar.
- **`alan`** — yalnız o öznitelik sütununda **değeri olan** nesneleri alır.
- **`deger`** — `alan` ile birlikte verilir: sütunun değeri **tam olarak** bu olmalıdır.

Sayı ile bildirim ayrı iki şeydir ve bu kasıtlıdır: **`adet` her zaman doğrudur**,
`sinir` yalnız kaç anahtarın geri döndüğünü belirler. 5000 parseli olan bir katmanda
`SORGULA katman=PARSEL` size "5000" der ve ilk 200 anahtarı verir. Böylece "kaç tane
var" sorusunun cevabı hiçbir zaman kırpılmaz.

Değer karşılaştırması, **öznitelik tablosunda gördüğünüz metinle** yapılır: sayı olarak
saklanan bir ada numarası `128` yazılarak aranır. Tam eşleşme arar; "şununla başlayan",
"şundan büyük" ya da "ve/veya" yoktur.

### Neden bir ifade yazılmıyor

Çünkü bu programda **tek bir ayrıştırıcı** vardır (`CLAUDE.md` 5.11). Komut satırı ve
betik motoru aynı dilbilgisini kullanır; süzme için ikinci bir küçük dil eklemek, aynı
soruya iki farklı cevap veren iki ayrıştırıcı demek olurdu. Bu yüzden `SORGULA`'nın
koşulu **bildirilmiş parametrelerdir**: veri yolu onları komut gövdesi çalışmadan önce
doğrular, tipleri ve sınırları bellidir, bir ajan da onları uydurmadan yazabilir.

Daha zengin bir süzme gerekiyorsa yeri öznitelik tablosudur: onun süzme ifadesi ayrı bir
konudur ve [Öznitelik tablosu](../veri/oznitelik-tablosu.md) sayfasında anlatılır.

Çizimi değiştirmediği için bir ajan `SORGULA`'yı **onay beklemeden** çalıştırabilir, ve
dönen nesne kümesi ona bir **tutamak** olarak verilir: bir sonraki komutta o nesneleri
ancak bu tutamakla anabilir ([Onay ve denetim](../yapay-zeka/onay.md)).

## Adlar

| Ad | Tür |
|---|---|
| `SORGULA` | Türkçe, birincil |
| `QUERY` | İngilizce karşılık |
| `SRG` | Kısaltma |
| `core.query` | Komut kimliği |
| `sorgula` | Ajan arayüzündeki araç adı |

## Sözdizimi

```text
SORGULA
SORGULA katman=<ad>
SORGULA katman=<ad> sinir=<1..1000>
SORGULA alan=<sütun>
SORGULA katman=<ad> alan=<sütun> deger=<değer>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `katman` | Hangi katmanda aranacağı; verilmezse bütün çizim |
| `alan` | Öznitelik sütunu; verilirse o sütunu taşıyan nesneler |
| `deger` | Sütunun eşit olması istenen değer; yalnız `alan` ile birlikte |
| `sinir` | En çok kaç nesne bildirileceği: 1–1000, varsayılan 200 |

`deger` tek başına, `alan` olmadan verilirse **dikkate alınmaz** — süzme yapılmaz ve
komut yine de bütün nesneleri sayar. Bir değeri arıyorsanız sütununu da yazın.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Bütün çizimde kaç nesne var:

```
SORGULA
```

Boş bir çizimde:

```text
0 nesne eşleşti.
```

Bir katmandaki nesneleri saymak:

```
KATMAN ad=PARSEL
SORGULA katman=PARSEL
```

Olmayan bir katman adı hata değil, cevaptır:

```
SORGULA katman=YOKBÖYLE
```

```text
Katman yok: 'YOKBÖYLE'. KATMANLAR ile listeyi alın.
```

Bir öznitelik değeriyle aramak — önce sütunu tanımlayın, sonra sorun:

```
SÜTUN ada_no tam_sayi
SORGULA katman=PARSEL alan=ada_no deger=128
```

Yalnız ilk elliyi bildirmek, sayının tamamını yine görmek:

```
SORGULA katman=PARSEL sinir=50
```

Çok nesne eşleşirse satır ikisini birden söyler:

```text
5000 nesne eşleşti, ilk 50 bildirildi.
```

Kısaltmayla aynı iş:

```
SRG katman=PARSEL
```

### Arayüz

Sorgunun gündelik arayüz karşılığı **Öznitelikler** panelidir: tablo satırlarını süzer,
süzülen satırları seçime çevirir ve seçim sayısını durum çubuğunda gösterir
([Öznitelik tablosu](../veri/oznitelik-tablosu.md)).

Komutun kendisini arayüzden çalıştırmak için **Ctrl+K** ile komut aramayı açıp `SORGULA`
yazın: ad komut satırına yerleşir, parametreleri yazıp **Enter**'a basarsınız. Yanıt
komut satırının üstündeki döküm alanında görünür.

Eşleşen nesneleri ekranda **seçili** hâle getirmek `SORGULA`'nın işi değildir; onu
[`SEÇ`](select.md) yapar.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.query", "args": { "katman": "PARSEL", "alan": "ada_no", "deger": "128" } }
```

Bir projeyi açıp iki sorgu çalıştırmak:

```json
{
  "ad": "Parsel sayımı",
  "komutlar": [
    { "cmd": "core.open", "args": { "dosya": "veri/ada128.pcad" } },
    { "cmd": "core.query", "args": { "katman": "PARSEL" } },
    { "cmd": "core.query", "args": { "katman": "YOL", "sinir": 1000 } }
  ]
}
```

## Geri alma

`SORGULA` çizime dokunmaz: geri alınacak bir şey yoktur ve [`GERİAL`](undo.md) listesine
girmez. Seçimi de değiştirmez — sorgulamak seçmek değildir.

**Komut günlüğüne de girmez**: salt okunur bir komut günlüğe yazılmaz, çünkü günlük
çizimin tarihidir ve bir soru çizimin tarihinin parçası değildir.

## Betikten kullanım

Komut betiklerde `core.query` kimliğiyle çağrılır. `sinir` betikte de **tam sayıdır**,
metin değil.

Betiğe ve ajana dönen yapı şu alanları taşır:

| Alan | Ne taşır |
|---|---|
| `adet` | Koşula uyan nesne sayısı — kırpılmaz, her zaman doğrudur |
| `bildirilen` | Kaç anahtarın geri döndüğü; en çok `sinir` kadar |
| `sinir` | Uygulanan sınır |
| `nesneler` | Bildirilen nesnelerin **kalıcı anahtarları**, sıra numarası değil |
| `kutu_mm` | Bildirilen nesneleri kapsayan dikdörtgenin dört sayısı, tam sayı milimetre: en küçük `Sağa (Y)`, en küçük `Yukarı (X)`, en büyük `Sağa (Y)`, en büyük `Yukarı (X)`. Eşleşme yoksa alan hiç yazılmaz |

Koordinatlar **tam sayı milimetredir** ve doğuya doğru olan (`Sağa (Y)`) önce yazılır
([Koordinat sistemleri](../veri/koordinat-sistemleri.md)).

Bir ajan bu sorguyu [MCP sunucusu](../yapay-zeka/mcp-sunucusu.md) üzerinden `sorgula`
adıyla çağırır. Yanıtla birlikte iki tutamak alır: eşleşen **nesneler** için bir nesne
tutamağı, `kutu_mm` için bir pencere tutamağı. Sonraki komutta nesneleri yalnız bu
tutamakla gösterebilir; çizim bu arada değişirse tutamak eskir ve yeniden sorması
gerekir.

## Hatalar

`SORGULA` bulunamayan bir katmanı ya da sütunu **cevap olarak** bildirir: komut
başarısız olmaz, yalnız aşağıdaki satırı yazar ve hiçbir şey saymaz.

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Katman yok: 'YOKBÖYLE'. KATMANLAR ile listeyi alın.` | O adda katman yok | Adı denetleyin; listeyi [`KATMANLAR`](layers.md) verir |
| `Öznitelik sütunu yok: 'ada'. ÖZNİTELİKŞEMASI ile listeyi alın.` | O adda öznitelik sütunu yok | Listeyi [`ÖZNİTELİKŞEMASI`](attr_schema.md) verir; sütun tanımlamak [`SÜTUN`](column.md) işidir |
| `'core.query': 'sinir' 1 ile 1000 arasında olmalı, 5000 geldi.` | `sinir` aralığın dışında | 1–1000 arasında bir sayı yazın; `adet` zaten kırpılmaz |
| `'core.query': bilinmeyen parametre 'limit'. Tanımlı parametreler: katman, alan, deger, sinir` | Tanımlı olmayan bir parametre adı | Dört addan birini kullanın |
| `'core.query': 'sinir' parametresi tam sayı bekliyor. Girilen: 'elli'` | Komut satırına sayı olmayan bir sınır yazıldı | Tam sayı yazın |
| `'core.query': 'sinir' parametresi tam sayı bekliyor, başka türde bir değer geldi.` | Aynı hata betikten geldiğinde: `sinir` alanına metin yazılmış | Betikte `"sinir": 50` gibi tam sayı yazın, tırnaksız |

## İlgili

- [`KATMANLAR`](layers.md) — hangi katmanlar var, hangisinde kaç nesne
- [`ÖZNİTELİKŞEMASI`](attr_schema.md) — `alan` parametresine yazılacak adlar
- [`SEÇ`](select.md) — nesneleri ekranda seçili hâle getirme
- [`SEÇİMBİLGİSİ`](selection_info.md) — o anki seçimi okuma
- [Öznitelik tablosu](../veri/oznitelik-tablosu.md) — daha zengin süzme
- [Yapay zeka ve ajanlar](../yapay-zeka/README.md) — tutamak kuralı ve okuma sırası
