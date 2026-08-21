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
| [`core.erase`](erase.md) | `SİL`, `SIL`, `ERASE`, `E` | Düzenleme | tek işlem | betiklenebilir, AI erişimli | Seçilen nesneleri siler. |
| [`core.layer`](layer.md) | `KATMAN`, `LAYER`, `KAT` | Katman | tek işlem | etkileşimli, betiklenebilir, AI erişimli | Katman oluşturur, aktif yapar ve özelliklerini değiştirir. |
| [`core.zoom`](zoom.md) | `YAKINLAŞ`, `YAKINLAS`, `ZOOM`, `Z` | Görünüm | geri alınmaz | betiklenebilir, AI erişimli, şeffaf, salt okunur | Görünümü çizim kapsamına veya verilen çarpana ayarlar. |
| [`core.undo`](undo.md) | `GERİAL`, `GERIAL`, `UNDO`, `U` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Son işlemi geri alır. |
| [`core.redo`](redo.md) | `YİNELE`, `YINELE`, `REDO` | Sistem | geri alınmaz | betiklenebilir, salt okunur | Geri alınan işlemi yineler. |
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

### `core.erase` — SİL

Seçilen nesneleri siler.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | selection | en az 1 | Silinecek nesnelerin kimlikleri |

Ayrıntılı kullanım: [SİL](erase.md)

### `core.layer` — KATMAN

Katman oluşturur, aktif yapar ve özelliklerini değiştirir.

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `ad` | text | 1 | Katman adı; yoksa oluşturulur ve aktif yapılır |
| `gorunur` | bool | isteğe bağlı | Katmanın görünürlüğü |
| `kilitli` | bool | isteğe bağlı | Katmanın kilit durumu |
| `renk` | integer | isteğe bağlı | Çizim rengi, 0xAARRGGBB |

Ayrıntılı kullanım: [KATMAN](layer.md)

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
          "min": 1,
          "max": -1,
          "required": true,
          "help": "Silinecek nesnelerin kimlikleri"
        }
      ],
      "flags": [
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
