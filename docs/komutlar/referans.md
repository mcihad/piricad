<!-- ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN. -->
<!-- Kaynak: piricad::command::Registry.  Yeniden üret: make reference -->
<!-- Bir komutun burada görünmesi için tek yapılması gereken onu kaydetmektir; -->
<!-- projede elle tutulan ikinci bir komut listesi yoktur (CLAUDE.md 5.10). -->

# Komut Referansı

Bu tablo komut kaydından üretilir. Her komutun ayrıntılı kullanım sayfası
`docs/komutlar/` altındadır ve tablodan bağlanır.

| Komut | Adlar | Kategori | Geri alma | Özellikler | Açıklama |
|---|---|---|---|---|---|
| [`core.line`](line.md) | `ÇİZGİ`, `CIZGI`, `LINE`, `Ç`, `L` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | İki veya daha fazla nokta arasında doğru parçaları çizer. |
| [`core.text`](text.md) | `METİN`, `METIN`, `YAZI`, `TEXT`, `MT` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Çizime metin yazar; yükseklik ve hizalama verilebilir. |
| [`core.exportstyle`](exportstyle.md) | `STİLAKTAR`, `STILAKTAR`, `EXPORTSTYLE`, `STAKTAR` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar. |
| [`core.area`](area.md) | `ALAN`, `AREA`, `POLİGON`, `POLIGON`, `AL` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Kapalı bir alan çizer; istenirse içine delik açar. |
| [`core.attribute`](attribute.md) | `ÖZNİTELİK`, `OZNITELIK`, `ATTRIBUTE`, `ÖZN`, `OZN` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar. |
| [`core.column`](column.md) | `SÜTUN`, `SUTUN`, `COLUMN`, `STN` | Düzenleme | geri alınmaz | betiklenebilir | Belgeye öznitelik sütunu tanımlar ve tanımlı sütunları listeler. |
| [`core.erase`](erase.md) | `SİL`, `SIL`, `ERASE`, `E` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Seçilen nesneleri siler. |
| [`core.select`](select.md) | `SEÇ`, `SEC`, `SELECT`, `S` | Düzenleme | geri alınmaz | betiklenebilir, salt okunur | Nesneleri seçer: tümü, kimlikle, pencere, kesen kutu veya tek nokta. |
| [`core.label`](label.md) | `ETİKET`, `ETIKET`, `LABEL`, `ETK` | Çizim | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Katmandaki nesneleri özniteliklerinden okuyarak etiketler. |
| [`core.layer`](layer.md) | `KATMAN`, `LAYER`, `KAT` | Katman | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Katman oluşturur, aktif yapar ve özelliklerini değiştirir. |
| [`core.style`](style.md) | `STİL`, `STIL`, `STYLE`, `ST` | Katman | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar. |
| [`core.symbol`](symbol.md) | `SEMBOL`, `SEMBOLLER`, `SYMBOL`, `SMB` | Katman | geri alınmaz | betiklenebilir, AI erişimli | Gösterim rafını yükler, ağacında gezer ve içinde arar. |
| [`core.zoom`](zoom.md) | `YAKINLAŞ`, `YAKINLAS`, `ZOOM`, `Z` | Görünüm | geri alınmaz | betiklenebilir, AI erişimli, şeffaf, salt okunur | Görünümü çizim kapsamına veya verilen çarpana ayarlar. |
| [`core.undo`](undo.md) | `GERİAL`, `GERIAL`, `UNDO`, `U` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Son işlemi geri alır. |
| [`core.redo`](redo.md) | `YİNELE`, `YINELE`, `REDO` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Geri alınan işlemi yineler. |
| [`core.open`](open.md) | `AÇ`, `AC`, `OPEN` | Dosya | geri alınmaz | etkileşimli, betiklenebilir | Bir PiriCAD proje dosyasını açar ve çizimin yerine koyar. |
| [`core.save`](save.md) | `KAYDET`, `SAVE`, `KYD` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Çizimi bağlı olduğu PiriCAD proje dosyasına kaydeder. |
| [`core.saveas`](saveas.md) | `FARKLIKAYDET`, `SAVEAS`, `FKAYDET` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Çizimi yeni bir PiriCAD proje dosyasına kaydeder ve ona bağlar. |
| [`core.import`](import.md) | `İÇEAKTAR`, `ICEAKTAR`, `IMPORT`, `IAKTAR` | Dosya | tek işlem | etkileşimli, betiklenebilir | Dış bir veri dosyasını çizime ekler. |
| [`core.export`](export.md) | `DIŞAAKTAR`, `DISAAKTAR`, `EXPORT`, `DAKTAR` | Dosya | geri alınmaz | etkileşimli, betiklenebilir, salt okunur | Çizimi dış bir veri biçimine yazar. |
| [`core.script`](script.md) | `BETİK`, `BETIK`, `SCRIPT` | Betik | komuta özel | etkileşimli, betiklenebilir, salt okunur | Bir betik dosyasını komut veri yolu üzerinden çalıştırır. |
| [`core.setting`](setting.md) | `AYAR`, `SETTING`, `AY` | Sistem | tek işlem | betiklenebilir | Proje ayarlarını listeler, okur ve değiştirir. |
| [`core.preference`](preference.md) | `TERCİH`, `TERCIH`, `PREFERENCE`, `PREF` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Uygulama tercihlerini listeler, okur ve değiştirir. |
| [`core.mode`](mode.md) | `MOD`, `MODE`, `MD` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur ve değiştirir. |
| [`core.help`](help.md) | `YARDIM`, `HELP`, `?` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Komut listesini veya tek bir komutun ayrıntısını gösterir. |

## Parametreler

### `core.line` — ÇİZGİ

İki veya daha fazla nokta arasında doğru parçaları çizer.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 2 | Ardışık doğru parçalarının köşe noktaları |

Ayrıntılı kullanım: [ÇİZGİ](line.md)

### `core.text` — METİN

Çizime metin yazar; yükseklik ve hizalama verilebilir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point | 1 | Yazının başlangıç noktası |
| `yazi` | text | 1 | Yazılacak metin |
| `yukseklik` | integer | isteğe bağlı | Yazı yüksekliği, zeminde milimetre; yoksa proje ayarı |
| `bitis` | point_list | isteğe bağlı | Taban çizgisinin bitişi; yoksa yatay |
| `hizalama` | text | isteğe bağlı | sol, orta, sag veya merkez |

Ayrıntılı kullanım: [METİN](text.md)

### `core.exportstyle` — STİLAKTAR

Bir katmanın sembolojisini QGIS QML stil dosyası olarak yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `katman` | text | 1 | Stili aktarılacak katmanın adı |
| `dosya` | text | 1 | Yazılacak .qml dosyasının yolu |

Ayrıntılı kullanım: [STİLAKTAR](exportstyle.md)

### `core.area` — ALAN

Kapalı bir alan çizer; istenirse içine delik açar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `noktalar` | point_list | en az 3 | Alanın köşe noktaları; kapanış noktası tekrarlanmaz |
| `bolum` | integer | en az 0 | Halka uzunlukları: ilki dış sınır, sonrakiler delik |

Ayrıntılı kullanım: [ALAN](area.md)

### `core.attribute` — ÖZNİTELİK

Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Öznitelik kimliği; yoksa tanımlı sütunlar listelenir |
| `nesne` | integer | isteğe bağlı | Nesnenin kalıcı kimliği |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur. 'yok' hücreyi boşaltır |

Ayrıntılı kullanım: [ÖZNİTELİK](attribute.md)

### `core.column` — SÜTUN

Belgeye öznitelik sütunu tanımlar ve tanımlı sütunları listeler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `kimlik` | text | isteğe bağlı | Sütun kimliği; yoksa tanımlı sütunlar listelenir |
| `tur` | text | isteğe bağlı | tam_sayi, uzunluk, evet_hayir veya metin |

Ayrıntılı kullanım: [SÜTUN](column.md)

### `core.erase` — SİL

Seçilen nesneleri siler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 0 | Silinecek nesnelerin kimlikleri; yoksa etkin seçim |

Ayrıntılı kullanım: [SİL](erase.md)

### `core.select` — SEÇ

Nesneleri seçer: tümü, kimlikle, pencere, kesen kutu veya tek nokta.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `mod` | text | isteğe bağlı | TÜMÜ | TEMİZLE | NESNE | PENCERE | KESEN | KUTU | NOKTA |
| `noktalar` | point_list | 0–2 | Kutu köşeleri (iki nokta) veya tek tıklama noktası |
| `nesneler` | selection | en az 0 | NESNE modunda nesne kimlikleri |
| `islem` | text | isteğe bağlı | DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE |
| `tolerans` | number | isteğe bağlı | NOKTA modunda arama yarıçapı, metre; yoksa seçim toleransı |

Ayrıntılı kullanım: [SEÇ](select.md)

### `core.label` — ETİKET

Katmandaki nesneleri özniteliklerinden okuyarak etiketler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `katman` | text | 1 | Etiketlenecek katmanın adı |
| `bicim` | text | 1 | Etiket biçimi; {sutun} o sütunun değeriyle değişir, \n satır kırar |
| `hedef` | text | isteğe bağlı | Etiketlerin yazılacağı katman; yoksa '<katman> ETİKET' |
| `yukseklik` | integer | isteğe bağlı | Yazı yüksekliği, zemin milimetresi |

Ayrıntılı kullanım: [ETİKET](label.md)

### `core.layer` — KATMAN

Katman oluşturur, aktif yapar ve özelliklerini değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | 1 | Katman adı; yoksa oluşturulur ve aktif yapılır |
| `grup` | text | isteğe bağlı | Katman ağacındaki yer, düzeyler '>' ile ayrılır; boş = kök |
| `gorunur` | bool | isteğe bağlı | Katmanın görünürlüğü |
| `kilitli` | bool | isteğe bağlı | Katmanın kilit durumu |
| `renk` | integer | isteğe bağlı | Çizim rengi, 0xAARRGGBB |

Ayrıntılı kullanım: [KATMAN](layer.md)

### `core.style` — STİL

Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `katman` | text | 1 | Stilin yazılacağı katmanın adı; katman var olmalı |
| `paket` | text | isteğe bağlı | Stil kataloğu paketinin dosya yolu |
| `olcek_min` | integer | isteğe bağlı | Bu ölçek paydasından daha yakında çizilmez (1:N'deki N) |
| `olcek_max` | integer | isteğe bağlı | Bu ölçek paydasından daha uzakta çizilmez |
| `sinifla` | text | isteğe bağlı | Sınıflandırmada kullanılacak öznitelik; her nesne kendi değerine göre stillenir |
| `kod` | text | isteğe bağlı | Katalogdaki satırın kimliği; verilmezse katalog kuralları eşleşir |
| `olcek` | integer | isteğe bağlı | Ölçek paydası (1:N); 0 = ölçekten bağımsız |
| `renk` | integer | isteğe bağlı | Çizgi rengi, 0xAARRGGBB |
| `kalinlik` | integer | isteğe bağlı | Çizgi kalınlığı, kâğıt mikrometresi (1000 = 1 mm) |
| `dolgu` | integer | isteğe bağlı | Dolgu rengi, 0xAARRGGBB; 0 = dolgusuz |
| `sira` | integer | isteğe bağlı | Çizim sırası; büyük olan üste gelir |
| `sifirla` | bool | isteğe bağlı | Stili siler; nesneler katman varsayılanına döner |
| `tip` | text | isteğe bağlı | Sembol katmanı tipi: cizgi, isaretci-cizgi, tarak-cizgi, dolgu, cizgi-desen-dolgu, nokta-desen-dolgu, merkez-isaretci, isaretci |
| `ekle` | bool | isteğe bağlı | Katmanı mevcut sembolün üstüne ekler; yoksa sembolü değiştirir |
| `sekil` | text | isteğe bağlı | İşaretçi şekli: daire, kare, ucgen, baklava, yildiz, arti, carpi, ok, yarim-daire, besgen, altigen, cizik |
| `yerlesim` | text | isteğe bağlı | İşaretçinin çizgi üzerindeki yeri: aralik, tepe, ilk, son, orta |
| `birim` | text | isteğe bağlı | Ölçülerin birimi: kagit (µm), zemin (mm), piksel |
| `boyut` | integer | isteğe bağlı | İşaretçi çapı ya da tarak dişinin boyu, `birim` cinsinden |
| `aralik` | integer | isteğe bağlı | Çizgi boyunca ya da desende birinci eksende aralık |
| `aralik_y` | integer | isteğe bağlı | Nokta deseninde ikinci eksen; verilmezse kare desen |
| `aci` | integer | isteğe bağlı | Desen açısı ya da işaretçi dönüklüğü, mikro derece |
| `kaydirma` | integer | isteğe bağlı | Geometriden dik kaydırma, `birim` cinsinden |
| `saydamlik` | integer | isteğe bağlı | Katman saydamlığı 0-255; 255 tam opak |
| `desen` | text | isteğe bağlı | Çizgi deseni tablosundaki satır |
| `yazi` | text | isteğe bağlı | yazi-isaretci katmanının yazdığı sabit metin |

Ayrıntılı kullanım: [STİL](style.md)

### `core.symbol` — SEMBOL

Gösterim rafını yükler, ağacında gezer ve içinde arar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `paket` | text | isteğe bağlı | Yüklenecek gösterim paketinin dosya yolu |
| `grup` | text | isteğe bağlı | Gezilecek grup yolu, düzeyler '>' ile ayrılır |
| `ara` | text | isteğe bağlı | Etikette, kimlikte ve grup yolunda arar |
| `kod` | text | isteğe bağlı | Tek bir gösterimin ayrıntısı |

Ayrıntılı kullanım: [SEMBOL](symbol.md)

### `core.zoom` — YAKINLAŞ

Görünümü çizim kapsamına veya verilen çarpana ayarlar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `mod` | text | isteğe bağlı | KAPSAM | ÇARPAN | SIFIRLA |
| `carpan` | number | isteğe bağlı | ÇARPAN modunda ölçek katsayısı |

Ayrıntılı kullanım: [YAKINLAŞ](zoom.md)

### `core.undo` — GERİAL

Son işlemi geri alır.

Parametre almaz.

Ayrıntılı kullanım: [GERİAL](undo.md)

### `core.redo` — YİNELE

Geri alınan işlemi yineler.

Parametre almaz.

Ayrıntılı kullanım: [YİNELE](redo.md)

### `core.open` — AÇ

Bir PiriCAD proje dosyasını açar ve çizimin yerine koyar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Açılacak PiriCAD proje dosyasının yolu (.pcad) |

Ayrıntılı kullanım: [AÇ](open.md)

### `core.save` — KAYDET

Çizimi bağlı olduğu PiriCAD proje dosyasına kaydeder.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | isteğe bağlı | Hedef yol; verilmezse çizimin bağlı olduğu dosyaya yazılır |

Ayrıntılı kullanım: [KAYDET](save.md)

### `core.saveas` — FARKLIKAYDET

Çizimi yeni bir PiriCAD proje dosyasına kaydeder ve ona bağlar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Yeni proje dosyasının yolu (.pcad) |

Ayrıntılı kullanım: [FARKLIKAYDET](saveas.md)

### `core.import` — İÇEAKTAR

Dış bir veri dosyasını çizime ekler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | İçe aktarılacak dosyanın yolu |
| `bicim` | text | isteğe bağlı | Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur |

Ayrıntılı kullanım: [İÇEAKTAR](import.md)

### `core.export` — DIŞAAKTAR

Çizimi dış bir veri biçimine yazar.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Yazılacak dosyanın yolu |
| `bicim` | text | isteğe bağlı | Sürücü adı (DXF, GPKG); verilmezse uzantıdan bulunur |

Ayrıntılı kullanım: [DIŞAAKTAR](export.md)

### `core.script` — BETİK

Bir betik dosyasını komut veri yolu üzerinden çalıştırır.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `dosya` | text | 1 | Çalıştırılacak betik dosyasının yolu |

Ayrıntılı kullanım: [BETİK](script.md)

### `core.setting` — AYAR

Proje ayarlarını listeler, okur ve değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Ayar adı veya kimliği; yoksa liste |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur |

Ayrıntılı kullanım: [AYAR](setting.md)

### `core.preference` — TERCİH

Uygulama tercihlerini listeler, okur ve değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Tercih adı veya kimliği; yoksa liste |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur |

Ayrıntılı kullanım: [TERCİH](preference.md)

### `core.mode` — MOD

Oturum modlarını (yakalama, dik mod, kutupsal izleme) listeler, okur ve değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | isteğe bağlı | Mod adı veya kimliği; yoksa liste |
| `deger` | text | isteğe bağlı | Yeni değer; yoksa yalnızca okur |

Ayrıntılı kullanım: [MOD](mode.md)

### `core.help` — YARDIM

Komut listesini veya tek bir komutun ayrıntısını gösterir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `komut` | text | isteğe bağlı | Ayrıntısı istenen komut adı |

Ayrıntılı kullanım: [YARDIM](help.md)

## AI araç kataloğu

AI'ın görebildiği komutlar `Flags::AiAccessible` bayrağından üretilir.
Elle tutulan ikinci bir araç şeması yoktur (piricad.md §2.3, §5.1).

```json
{
  "version": 1,
  "generated_from": "piricad::command::Registry",
  "tools": [
    {
      "id": "core.line",
      "names": [
        "ÇİZGİ",
        "CIZGI",
        "LINE",
        "Ç",
        "L"
      ],
      "category": "Çizim",
      "summary": "İki veya daha fazla nokta arasında doğru parçaları çizer.",
      "params": [
        {
          "name": "noktalar",
          "type": "point_list",
          "min": 2,
          "max": -1,
          "required": true,
          "help": "Ardışık doğru parçalarının köşe noktaları"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.text",
      "names": [
        "METİN",
        "METIN",
        "YAZI",
        "TEXT",
        "MT"
      ],
      "category": "Çizim",
      "summary": "Çizime metin yazar; yükseklik ve hizalama verilebilir.",
      "params": [
        {
          "name": "noktalar",
          "type": "point",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Yazının başlangıç noktası"
        },
        {
          "name": "yazi",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Yazılacak metin"
        },
        {
          "name": "yukseklik",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yazı yüksekliği, zeminde milimetre; yoksa proje ayarı"
        },
        {
          "name": "bitis",
          "type": "point_list",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Taban çizgisinin bitişi; yoksa yatay"
        },
        {
          "name": "hizalama",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "sol, orta, sag veya merkez"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.area",
      "names": [
        "ALAN",
        "AREA",
        "POLİGON",
        "POLIGON",
        "AL"
      ],
      "category": "Çizim",
      "summary": "Kapalı bir alan çizer; istenirse içine delik açar.",
      "params": [
        {
          "name": "noktalar",
          "type": "point_list",
          "min": 3,
          "max": -1,
          "required": true,
          "help": "Alanın köşe noktaları; kapanış noktası tekrarlanmaz"
        },
        {
          "name": "bolum",
          "type": "integer",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Halka uzunlukları: ilki dış sınır, sonrakiler delik"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.attribute",
      "names": [
        "ÖZNİTELİK",
        "OZNITELIK",
        "ATTRIBUTE",
        "ÖZN",
        "OZN"
      ],
      "category": "Düzenleme",
      "summary": "Nesnelerin özniteliklerini listeler, okur ve yazar; yeni sütun tanımlar.",
      "params": [
        {
          "name": "ad",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Öznitelik kimliği; yoksa tanımlı sütunlar listelenir"
        },
        {
          "name": "nesne",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Nesnenin kalıcı kimliği"
        },
        {
          "name": "deger",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yeni değer; yoksa yalnızca okur. 'yok' hücreyi boşaltır"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.erase",
      "names": [
        "SİL",
        "SIL",
        "ERASE",
        "E"
      ],
      "category": "Düzenleme",
      "summary": "Seçilen nesneleri siler.",
      "params": [
        {
          "name": "nesneler",
          "type": "selection",
          "min": 0,
          "max": -1,
          "required": false,
          "help": "Silinecek nesnelerin kimlikleri; yoksa etkin seçim"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.label",
      "names": [
        "ETİKET",
        "ETIKET",
        "LABEL",
        "ETK"
      ],
      "category": "Çizim",
      "summary": "Katmandaki nesneleri özniteliklerinden okuyarak etiketler.",
      "params": [
        {
          "name": "katman",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Etiketlenecek katmanın adı"
        },
        {
          "name": "bicim",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Etiket biçimi; {sutun} o sütunun değeriyle değişir, \\n satır kırar"
        },
        {
          "name": "hedef",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Etiketlerin yazılacağı katman; yoksa '<katman> ETİKET'"
        },
        {
          "name": "yukseklik",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yazı yüksekliği, zemin milimetresi"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.layer",
      "names": [
        "KATMAN",
        "LAYER",
        "KAT"
      ],
      "category": "Katman",
      "summary": "Katman oluşturur, aktif yapar ve özelliklerini değiştirir.",
      "params": [
        {
          "name": "ad",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Katman adı; yoksa oluşturulur ve aktif yapılır"
        },
        {
          "name": "grup",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katman ağacındaki yer, düzeyler '>' ile ayrılır; boş = kök"
        },
        {
          "name": "gorunur",
          "type": "bool",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katmanın görünürlüğü"
        },
        {
          "name": "kilitli",
          "type": "bool",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katmanın kilit durumu"
        },
        {
          "name": "renk",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizim rengi, 0xAARRGGBB"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.style",
      "names": [
        "STİL",
        "STIL",
        "STYLE",
        "ST"
      ],
      "category": "Katman",
      "summary": "Bir katmandaki nesnelerin stilini katalogdan veya doğrudan verilen değerlerden yazar.",
      "params": [
        {
          "name": "katman",
          "type": "text",
          "min": 1,
          "max": 1,
          "required": true,
          "help": "Stilin yazılacağı katmanın adı; katman var olmalı"
        },
        {
          "name": "paket",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Stil kataloğu paketinin dosya yolu"
        },
        {
          "name": "olcek_min",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Bu ölçek paydasından daha yakında çizilmez (1:N'deki N)"
        },
        {
          "name": "olcek_max",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Bu ölçek paydasından daha uzakta çizilmez"
        },
        {
          "name": "sinifla",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Sınıflandırmada kullanılacak öznitelik; her nesne kendi değerine göre stillenir"
        },
        {
          "name": "kod",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katalogdaki satırın kimliği; verilmezse katalog kuralları eşleşir"
        },
        {
          "name": "olcek",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Ölçek paydası (1:N); 0 = ölçekten bağımsız"
        },
        {
          "name": "renk",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizgi rengi, 0xAARRGGBB"
        },
        {
          "name": "kalinlik",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizgi kalınlığı, kâğıt mikrometresi (1000 = 1 mm)"
        },
        {
          "name": "dolgu",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Dolgu rengi, 0xAARRGGBB; 0 = dolgusuz"
        },
        {
          "name": "sira",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizim sırası; büyük olan üste gelir"
        },
        {
          "name": "sifirla",
          "type": "bool",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Stili siler; nesneler katman varsayılanına döner"
        },
        {
          "name": "tip",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Sembol katmanı tipi: cizgi, isaretci-cizgi, tarak-cizgi, dolgu, cizgi-desen-dolgu, nokta-desen-dolgu, merkez-isaretci, isaretci"
        },
        {
          "name": "ekle",
          "type": "bool",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katmanı mevcut sembolün üstüne ekler; yoksa sembolü değiştirir"
        },
        {
          "name": "sekil",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "İşaretçi şekli: daire, kare, ucgen, baklava, yildiz, arti, carpi, ok, yarim-daire, besgen, altigen, cizik"
        },
        {
          "name": "yerlesim",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "İşaretçinin çizgi üzerindeki yeri: aralik, tepe, ilk, son, orta"
        },
        {
          "name": "birim",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Ölçülerin birimi: kagit (µm), zemin (mm), piksel"
        },
        {
          "name": "boyut",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "İşaretçi çapı ya da tarak dişinin boyu, `birim` cinsinden"
        },
        {
          "name": "aralik",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizgi boyunca ya da desende birinci eksende aralık"
        },
        {
          "name": "aralik_y",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Nokta deseninde ikinci eksen; verilmezse kare desen"
        },
        {
          "name": "aci",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Desen açısı ya da işaretçi dönüklüğü, mikro derece"
        },
        {
          "name": "kaydirma",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Geometriden dik kaydırma, `birim` cinsinden"
        },
        {
          "name": "saydamlik",
          "type": "integer",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Katman saydamlığı 0-255; 255 tam opak"
        },
        {
          "name": "desen",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Çizgi deseni tablosundaki satır"
        },
        {
          "name": "yazi",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "yazi-isaretci katmanının yazdığı sabit metin"
        }
      ],
      "flags": [
        "interactive",
        "scriptable",
        "ai_accessible"
      ],
      "undo": "single_transaction"
    },
    {
      "id": "core.symbol",
      "names": [
        "SEMBOL",
        "SEMBOLLER",
        "SYMBOL",
        "SMB"
      ],
      "category": "Katman",
      "summary": "Gösterim rafını yükler, ağacında gezer ve içinde arar.",
      "params": [
        {
          "name": "paket",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Yüklenecek gösterim paketinin dosya yolu"
        },
        {
          "name": "grup",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Gezilecek grup yolu, düzeyler '>' ile ayrılır"
        },
        {
          "name": "ara",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Etikette, kimlikte ve grup yolunda arar"
        },
        {
          "name": "kod",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "Tek bir gösterimin ayrıntısı"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible"
      ],
      "undo": "none"
    },
    {
      "id": "core.zoom",
      "names": [
        "YAKINLAŞ",
        "YAKINLAS",
        "ZOOM",
        "Z"
      ],
      "category": "Görünüm",
      "summary": "Görünümü çizim kapsamına veya verilen çarpana ayarlar.",
      "params": [
        {
          "name": "mod",
          "type": "text",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "KAPSAM | ÇARPAN | SIFIRLA"
        },
        {
          "name": "carpan",
          "type": "number",
          "min": 0,
          "max": 1,
          "required": false,
          "help": "ÇARPAN modunda ölçek katsayısı"
        }
      ],
      "flags": [
        "scriptable",
        "ai_accessible",
        "transparent",
        "read_only"
      ],
      "undo": "none"
    }
  ]
}
```
