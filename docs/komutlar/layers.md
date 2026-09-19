# KATMANLAR — Katman Dökümü

Çizimde hangi katmanların olduğunu, her birinde kaç nesne bulunduğunu ve hangisinin
gizli, kilitli ya da baskıya kapalı olduğunu tek satırda öğrenmek isteyen herkes için;
bu sayfayı bitirdiğinizde dökümü komut satırından almayı, betikten çağırmayı ve bir
ajanın aynı dökümü neden ilk iş olarak istediğini bileceksiniz.

## Ne yapar

Çizimin **katman dökümünü** verir. Her katman için adını, kalıcı anahtarını, üzerindeki
**canlı nesne sayısını**, görünürlüğünü, kilidini, baskıya girip girmediğini ve varsa
grup yolunu bildirir. Sonunda **aktif katmanı** ve çizimin **koordinat sistemini** söyler.

Hiçbir şeyi değiştirmez: katman yaratmaz, aktif katmanı oynatmaz, görünürlüğe dokunmaz.
Onlar [`KATMAN`](layer.md) ve [`KATMANGÖRÜNÜM`](layer_visibility.md) işidir.

Komut **iki kere yanıt verir**, çünkü iki ayrı okuyucusu vardır:

- Klavyedeki insana **bir satır Türkçe** yazar: kaç katman var ve her birinde kaç nesne.
- İnsan olmayan istemciye — betiğe, yapay zeka ajanına — aynı bilgiyi **veri** olarak
  döndürür. O yapının alan adları bu sayfanın [Betikten kullanım](#betikten-kullanım)
  bölümünde yazılıdır.

Nesneler **canlı** olanlardır: silinmiş bir nesne hiçbir katmanın sayısına girmez.

`KATMANLAR` çizimi değiştirmediği için bir ajan onu **onay beklemeden** çalıştırabilir
([Onay ve denetim](../yapay-zeka/onay.md)). Bir ajanın çizime dair ilk sorusu genellikle
budur; sıralaması [llms.txt](../yapay-zeka/README.md) içinde yazılıdır.

## Adlar

| Ad | Tür |
|---|---|
| `KATMANLAR` | Türkçe, birincil |
| `LAYERS` | İngilizce karşılık |
| `KTL` | Kısaltma |
| `core.layers` | Komut kimliği |
| `katmanlari_listele` | Ajan arayüzündeki araç adı |

## Sözdizimi

```text
KATMANLAR
```

## Parametreler

Parametre almaz. Argümansız çalışır ve hiçbir şey sormaz.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```
KATMANLAR
```

Boş bir çizimde tek bir katman vardır — her CAD geleneğinin açılışta getirdiği `0`
katmanı:

```text
1 katman: 0 (0)
```

Katman açıp nesne çizdikten sonra aynı komut:

```
KATMAN ad=PARSEL
KATMAN ad=YOL
KATMANLAR
```

```text
3 katman: 0 (0), PARSEL (0), YOL (0)
```

Kısaltmayla aynı iş:

```
KTL
```

### Arayüz

**Katmanlar** paneli aynı listeyi sürekli gösterir: her satırda katmanın adı, göz
simgesiyle görünürlüğü, kilidi ve grubu vardır. Panel kapalıysa **Görünüm ▸ Paneller ▸
Katmanlar** ile açılır.

Komutun kendisini arayüzden çalıştırmak için **Ctrl+K** ile komut aramayı açıp
`KATMANLAR` yazın: seçtiğiniz ad komut satırına yerleşir, **Enter** çalıştırır. Yanıt
komut satırının üstündeki döküm alanında görünür.

Aktif katman **durum çubuğunda** yazılıdır; koordinat sistemi de öyle.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.layers", "args": {} }
```

Bir projeyi açıp dökümünü almak:

```json
{
  "ad": "Katman dökümü",
  "komutlar": [
    { "cmd": "core.open", "args": { "dosya": "veri/ada128.pcad" } },
    { "cmd": "core.layers", "args": {} }
  ]
}
```

## Geri alma

`KATMANLAR` çizime dokunmaz: geri alınacak bir şey yoktur ve
[`GERİAL`](undo.md) listesine girmez.

**Komut günlüğüne de girmez.** Salt okunur komutlar günlüğe yazılmaz: günlük çizimin
tarihidir ve bir soru çizimin tarihinin parçası değildir. Aynı karar
[`YAZDIR`](print.md), [`AÇ`](open.md) ve [`KAYDET`](save.md) için de geçerlidir.

## Betikten kullanım

Komut betiklerde `core.layers` kimliğiyle çağrılır ve argüman almaz.

Betiğe ve ajana dönen yapı şu alanları taşır:

| Alan | Ne taşır |
|---|---|
| `katmanlar` | Katman kayıtları dizisi, çizimdeki sırayla |
| `katmanlar[].ad` | Katmanın adı |
| `katmanlar[].anahtar` | Katmanın kalıcı anahtarı; ad değişse de değişmez |
| `katmanlar[].nesne_sayisi` | O katmandaki canlı nesne sayısı |
| `katmanlar[].gorunur` | Görünür mü |
| `katmanlar[].kilitli` | Kilitli mi |
| `katmanlar[].basilir` | Baskıya giriyor mu |
| `katmanlar[].grup` | Grup yolu; grubu yoksa alan hiç yazılmaz |
| `aktif` | Aktif katmanın adı |
| `crs` | Çizimin koordinat sistemi kimliği |

Bir ajan bu dökümü [MCP sunucusu](../yapay-zeka/mcp-sunucusu.md) üzerinden
`katmanlari_listele` adıyla çağırır. Sonuç bir **tutamak** üretmez: katman adı bir
koordinat değildir, dolayısıyla bir sonraki komuta olduğu gibi yazılabilir. Tutamak
kuralı yalnız nokta, nokta listesi ve nesne seçimi için geçerlidir
([Onay ve denetim](../yapay-zeka/onay.md)).

## Hatalar

Komutun kendi reddettiği bir durum yoktur: parametresi yoktur, bulunamayacak bir şey
istemez ve boş çizimde de bir katman bulur. Yanlış yazılan bir argüman veri yolunun
doğrulamasına takılır:

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `'core.layers': bilinmeyen parametre 'katman'. Tanımlı parametreler: (yok)` | Komuta argüman verildi | Argümanı silin; `KATMANLAR` tek başına çalışır |
| `Bilinmeyen komut: 'KATMANLR'. YARDIM yazarak komut listesini görün.` | Ad yanlış yazıldı | `KATMANLAR`, `LAYERS` ya da `KTL` yazın |

## İlgili

- [`KATMAN`](layer.md) — katman yaratma, silme, kilitleme, renk ve grup
- [`KATMANGÖRÜNÜM`](layer_visibility.md) — göster, gizle, yalnız bunu göster
- [`SORGULA`](query.md) — bir katmandaki nesneleri sayma ve anahtarlarını alma
- [`ÖZNİTELİKŞEMASI`](attr_schema.md) — hangi öznitelik sütunları tanımlı
- [Yapay zeka ve ajanlar](../yapay-zeka/README.md) — okuma araçlarının kullanım sırası
