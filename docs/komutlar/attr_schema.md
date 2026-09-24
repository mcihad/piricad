# ÖZNİTELİKŞEMASI — Öznitelik Sütunlarının Dökümü

Bir çizimde hangi öznitelik sütunlarının tanımlı olduğunu, hangi tipte olduklarını ve
hangisinin zorunlu olduğunu öğrenmek isteyen herkes için; bu sayfayı bitirdiğinizde
şemayı komut satırından okumayı, betikten çağırmayı ve bir sorgu yazmadan önce neden
buraya bakmak gerektiğini bileceksiniz.

## Ne yapar

Çizimde tanımlı **öznitelik sütunlarını** sırayla listeler. Her sütun için kimliğini,
sakladığı **tipi**, varsa Türkçe etiketini, açıklamasını ve bağlı olduğu **katalog**
kimliğini, bir de **zorunlu** olup olmadığını bildirir. Sonunda öznitelik tablosunun
**satır sayısını** söyler.

Sütun tanımlamaz ve değer yazmaz: onlar [`SÜTUN`](column.md) ve
[`ÖZNİTELİK`](attribute.md) işidir. Bu komut yalnız "elimde ne var" sorusunu yanıtlar.

Bir [`SORGULA`](query.md) yazmadan önce bakılacak yer burasıdır: `alan=` parametresine
yazılacak ad, bu dökümdeki `ad` alanıdır. Uydurulan bir sütun adı sorguyu boş döndürmez,
açıkça reddeder.

Komut **iki kere yanıt verir**: klavyedeki insana bir satır Türkçe, betiğe ve ajana aynı
bilgi veri olarak. Alan adları [Betikten kullanım](#betikten-kullanım) bölümünde.

Çizimi değiştirmediği için bir ajan onu **onay beklemeden** çalıştırabilir
([Onay ve denetim](../yapay-zeka/onay.md)).

## Adlar

| Ad | Tür |
|---|---|
| `ÖZNİTELİKŞEMASI` | Türkçe, birincil |
| `OZNITELIKSEMASI` | Türkçe, ASCII katlanmış |
| `ATTRSCHEMA` | İngilizce karşılık |
| `ÖŞ` | Kısaltma |
| `core.attr_schema` | Komut kimliği |
| `oznitelik_semasi` | Ajan arayüzündeki araç adı |

## Sözdizimi

```text
ÖZNİTELİKŞEMASI
```

## Parametreler

Parametre almaz. Argümansız çalışır ve hiçbir şey sormaz.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Sütun tanımlanmamış bir çizimde:

```
ÖZNİTELİKŞEMASI
```

```text
Çizimde tanımlı öznitelik sütunu yok.
```

Kadastro çizimi için üç sütun tanımlayıp yeniden sorduğunuzda:

```
SÜTUN ada_no tam_sayi
SÜTUN parsel_no tam_sayi
SÜTUN malik metin
ÖZNİTELİKŞEMASI
```

```text
3 sütun: ada_no, parsel_no, malik
```

Kısaltmayla aynı iş:

```
ÖŞ
```

### Arayüz

**Öznitelikler** panelinin **Şema** sayfası aynı sütunları bir tabloda gösterir:
`Kimlik`, `Ad`, `Tür`, `Ayrıntı`, `Zorunlu` ve `Açıklama`. Panel kapalıysa **Görünüm ▸
Pencereler ▸ Öznitelikler** ile açılır; yeni sütun eklemek de o sayfadadır ve her
düzenlemesini [`SÜTUN`](column.md) komutuyla yapar.

Komutun kendisini çalıştırmak için şeritte **Harita** sekmesinin sonundaki **Diğer**
listesinden **Öznitelik Şeması**'nı seçin ya da **Ctrl+K** ile komut aramayı açıp
`ÖZNİTELİKŞEMASI` yazın; ad komut satırına yerleşir, **Enter** çalıştırır.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.attr_schema", "args": {} }
```

Bir projeyi açıp şemasını okumak:

```json
{
  "ad": "Şema dökümü",
  "komutlar": [
    { "cmd": "core.open", "args": { "dosya": "veri/ada128.pcad" } },
    { "cmd": "core.attr_schema", "args": {} }
  ]
}
```

## Geri alma

`ÖZNİTELİKŞEMASI` çizime dokunmaz: geri alınacak bir şey yoktur ve
[`GERİAL`](undo.md) listesine girmez.

**Komut günlüğüne de girmez**: salt okunur bir komut günlüğe yazılmaz, çünkü günlük
çizimin tarihidir ve bir soru çizimin tarihinin parçası değildir.

## Betikten kullanım

Komut betiklerde `core.attr_schema` kimliğiyle çağrılır ve argüman almaz.

Betiğe ve ajana dönen yapı şu alanları taşır:

| Alan | Ne taşır |
|---|---|
| `sutunlar` | Sütun kayıtları dizisi, tanımlandıkları sırayla |
| `sutunlar[].ad` | Sütunun kimliği; `SORGULA alan=` buna yazılır |
| `sutunlar[].tur` | Saklanan tip: `Int64`, `Ondalik`, `Uzunluk`, `Bool`, `Text`, `CodeRef`, `Tarih` |
| `sutunlar[].etiket` | Türkçe etiket; tanımlı değilse alan hiç yazılmaz |
| `sutunlar[].aciklama` | Sütunun açıklaması; yoksa yazılmaz |
| `sutunlar[].katalog` | Bağlı olduğu katalog kimliği; yoksa yazılmaz |
| `sutunlar[].zorunlu` | Değeri zorunlu mu |
| `satir_sayisi` | Öznitelik tablosundaki satır sayısı |

`tur` alanı **saklanan tipin adıdır**, `SÜTUN` komutuna yazdığınız sözcük değil: bir
sütunu `tam_sayi` diye tanımlarsınız, döküm `Int64` der. İkisi aynı tiptir; biri
insanın, biri dosyanın sözcüğüdür.

Bir ajan bu dökümü [MCP sunucusu](../yapay-zeka/mcp-sunucusu.md) üzerinden
`oznitelik_semasi` adıyla çağırır. Sonuç bir tutamak üretmez.

## Hatalar

Komutun kendi reddettiği bir durum yoktur: sütun yoksa bunu bir hata değil, bir cevap
sayar. Yanlış yazılan bir argüman veri yolunun doğrulamasına takılır:

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `'core.attr_schema': bilinmeyen parametre 'katman'. Tanımlı parametreler: (yok)` | Komuta argüman verildi | Argümanı silin; komut tek başına çalışır |
| `Bilinmeyen komut: 'OZNITELIK'. YARDIM yazarak komut listesini görün.` | Ad yanlış yazıldı; `ÖZNİTELİK` başka bir komuttur | Şema için `ÖZNİTELİKŞEMASI`, değer yazmak için [`ÖZNİTELİK`](attribute.md) |

## İlgili

- [`SÜTUN`](column.md) — öznitelik sütunu tanımlama ve silme
- [`ÖZNİTELİK`](attribute.md) — bir nesnenin öznitelik değerlerini yazma ve okuma
- [`SORGULA`](query.md) — sütun ve değere göre nesne sayma
- [Öznitelik tablosu](../veri/oznitelik-tablosu.md) — satırları süzme ve düzenleme
- [Yapay zeka ve ajanlar](../yapay-zeka/README.md) — okuma araçlarının kullanım sırası
