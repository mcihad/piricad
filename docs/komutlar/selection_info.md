# SEÇİMBİLGİSİ — O Anki Seçimi Okuma

Ekranda kaç nesnenin seçili olduğunu ve hangilerinin seçili olduğunu tam olarak
öğrenmek isteyen herkes için; bu sayfayı bitirdiğinizde seçimi komut satırından ve
betikten okumayı, bir ajanın seçimi neden değiştiremeyeceğini bileceksiniz.

## Ne yapar

Kullanıcının **o anki seçimini** bildirir: kaç nesne seçili ve hangi **kalıcı
anahtarlar** seçili. Yanıtta çizimin o andaki **sürüm numarası** da yer alır.

Seçimi değiştirmez, büyütmez, temizlemez. Seçmek [`SEÇ`](select.md) işidir; bu komut
yalnız "şu anda ne seçili" sorusunu yanıtlar.

Seçim **çizimin verisi değildir**: dosyaya yazılmaz, geri alınmaz, içerik özetine
girmez. Dolayısıyla bu komutun yanıtı çizimin değil, **oturumun** durumudur.

Çizimi değiştirmediği için bir ajan onu **onay beklemeden** çalıştırabilir, ve dönen
nesne kümesini bir **tutamak** olarak alır. Ajanın seçime dokunamaması kasıtlıdır:
[`SEÇ`](select.md) ajan arayüzüne hiç açılmamıştır, çünkü vurguyu değiştirmek bir
sonraki [`SİL`](erase.md)'in neyi sileceğini hiçbir `SİL` yazılmadan değiştirirdi. Bir
ajan nesnelerini argümanda adlandırır; ortamdaki seçime yaslanmaz
([Onay ve denetim](../yapay-zeka/onay.md)).

## Adlar

| Ad | Tür |
|---|---|
| `SEÇİMBİLGİSİ` | Türkçe, birincil |
| `SECIMBILGISI` | Türkçe, ASCII katlanmış |
| `SELECTIONINFO` | İngilizce karşılık |
| `SÇB` | Kısaltma |
| `core.selection_info` | Komut kimliği |
| `secimi_al` | Ajan arayüzündeki araç adı |

## Sözdizimi

```text
SEÇİMBİLGİSİ
```

## Parametreler

Parametre almaz. Argümansız çalışır ve hiçbir şey sormaz.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Hiçbir şey seçili değilken:

```
SEÇİMBİLGİSİ
```

```text
Seçim boş.
```

Seçtikten sonra:

```text
3 nesne seçili.
```

Kısaltmayla aynı iş:

```
SÇB
```

### Arayüz

Seçimi yapmanın yolu faredir: bir nesneye tıklamak, soldan sağa sürükleyerek **pencere
seçim**, sağdan sola sürükleyerek **kesen seçim**. Seçili nesneler tuvalde vurgulanır.

Komutun kendisini arayüzden çalıştırmak için **Ctrl+K** ile komut aramayı açıp
`SEÇİMBİLGİSİ` yazın; ad komut satırına yerleşir, **Enter** çalıştırır. Yanıt komut
satırının üstündeki döküm alanında görünür — anahtarları görmek istediğinizde asıl
sebebi budur, çünkü tuvaldeki vurgu size kaç tane olduğunu söyler, hangileri olduğunu
söylemez.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.selection_info", "args": {} }
```

Bir projeyi açıp seçimi okumak — açılışta seçim boştur, dolayısıyla bu satır bir
betikte ancak kendi seçtiklerinizden sonra anlam taşır:

```json
{
  "ad": "Seçimi bildir",
  "komutlar": [
    { "cmd": "core.open", "args": { "dosya": "veri/ada128.pcad" } },
    { "cmd": "core.selection_info", "args": {} }
  ]
}
```

## Geri alma

`SEÇİMBİLGİSİ` çizime dokunmaz: geri alınacak bir şey yoktur ve [`GERİAL`](undo.md)
listesine girmez. Seçimin kendisi de geri alınmaz — `GERİAL` çizimi geri alır, vurguyu
değil.

**Komut günlüğüne de girmez**: salt okunur bir komut günlüğe yazılmaz, çünkü günlük
çizimin tarihidir ve bir soru çizimin tarihinin parçası değildir.

## Betikten kullanım

Komut betiklerde `core.selection_info` kimliğiyle çağrılır ve argüman almaz.

Betiğe ve ajana dönen yapı şu alanları taşır:

| Alan | Ne taşır |
|---|---|
| `adet` | Seçili nesne sayısı |
| `nesneler` | Seçili nesnelerin **kalıcı anahtarları**, sıra numarası değil |
| `surum` | Çizimin o andaki sürüm numarası |

`surum` alanı bir ajan için önemlidir: aldığı tutamak bu sürüme bağlıdır. Çizim
değişince sürüm ilerler, eski tutamak reddedilir ve ajanın yeniden okuması gerekir. Bu
sessiz bir kayıp değil, açık bir rettir — arada birinin taşıdığı bir parselin eski
koordinatına çizgi çizmek, reddedilmekten çok daha pahalıdır.

Bir ajan seçimi [MCP sunucusu](../yapay-zeka/mcp-sunucusu.md) üzerinden `secimi_al`
adıyla okur.

## Hatalar

Komutun kendi reddettiği bir durum yoktur: boş seçim bir hata değil, bir cevaptır.
Yanlış yazılan bir argüman veri yolunun doğrulamasına takılır:

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `'core.selection_info': bilinmeyen parametre 'nesneler'. Tanımlı parametreler: (yok)` | Komuta argüman verildi | Argümanı silin; komut tek başına çalışır |
| `Bilinmeyen komut: 'SECIM'. YARDIM yazarak komut listesini görün.` | Ad yanlış yazıldı | `SEÇİMBİLGİSİ`, `SECIMBILGISI`, `SELECTIONINFO` ya da `SÇB` yazın |

## İlgili

- [`SEÇ`](select.md) — nesne seçme, pencere ve kesen seçim
- [`SORGULA`](query.md) — koşula uyan nesneleri sayma ve anahtarlarını alma
- [`SİL`](erase.md) — seçili nesneleri silme
- [Yapay zeka ve ajanlar](../yapay-zeka/README.md) — tutamak kuralı ve okuma sırası
