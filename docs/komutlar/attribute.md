# ÖZNİTELİK — Nesnelerin Verisi

Bir parselin ada ve parsel numarasını, bir plan lekesinin gösterim türünü, bir
binanın kat adedini yazan herkes için; bu sayfayı bitirdiğinizde nesnelere veri
yazmayı, okumayı ve silmeyi bileceksiniz.

> **Faz 0 durumu.** Öznitelikler belgeye yazılıyor, `.pcad` dosyasıyla gidiyor ve
> belgenin parmak izinin parçası. Yazdığınız değere göre **otomatik sembol atama**
> — yani "gösterim = TOPLU KONUT ALANI olan her parsel MPYY rengini alsın" —
> **Faz 1'de** gelecek; kural motoru hazır, kural tablosu henüz boş.

## Ne yapar

`ÖZNİTELİK`, bir nesnenin **ne olduğunu** yazar. Geometri nerede olduğunu söyler;
öznitelik ne olduğunu söyler. Bir parselin dört köşesi onun konumudur; ada numarası,
parsel numarası ve niteliği onun kimliğidir.

Şema **koleksiyona** aittir, nesneye değil. Yani "ada_no" bir kez tanımlanır ve
belgedeki bütün nesnelerin bir ada_no hücresi olur — çoğu boş olsa bile. Bu, her
nesnenin kendi torbasında etiket taşıdığı bir tasarımdan hem çok daha hızlıdır hem de
"bu belgede hangi alanlar var" sorusunun bir cevabı olmasını sağlar.

Sütun tanımlamak ayrı bir komuttur: [`SÜTUN`](column.md).

Üç kullanım biçimi vardır:

- **Argümansız** — tanımlı sütunları, türleriyle listeler
- **Ad ve nesne** — o nesnenin o özniteliğini okur
- **Ad, nesne ve değer** — özniteliği yazar ve önceki değerini söyler

## Adlar

| Ad | Tür |
|---|---|
| `ÖZNİTELİK` | Türkçe, birincil |
| `OZNITELIK` | ASCII karşılık |
| `ATTRIBUTE` | İngilizce karşılık |
| `ÖZN`, `OZN` | Kısaltma |
| `core.attribute` | Komut kimliği |

## Sözdizimi

```text
ÖZNİTELİK
ÖZNİTELİK <ad> <nesne>
ÖZNİTELİK <ad> <nesne> <deger>
ÖZNİTELİK ad=<ad> nesne=<kimlik> deger=<deger>
ÖZNİTELİK <ad> <nesne> yok
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Öznitelik kimliği. Verilmezse tanımlı sütunlar listelenir |
| `nesne` | Nesnenin **kalıcı kimliği**. `SEÇ` ile öğrenilir |
| `deger` | Yeni değer. Verilmezse yalnızca okur. `yok` hücreyi boşaltır |

`nesne` kalıcı kimliktir, ekrandaki sıra numarası değil. Bu ayrım kritiktir: sıra
numarası bir silme işleminden sonra başka bir nesneye geçer, kalıcı kimlik geçmez.
Günlüğe yazılan bir öznitelik satırı yeniden oynatıldığında **aynı** parsele düşmelidir.

Boş hücre ile sıfır **aynı şey değildir**. "Ada numarası kaydedilmemiş" ile "ada
numarası 0" bir parsel hakkında iki farklı gerçektir, ve belgenin parmak izinde de
farklı görünürler.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Tanımlı sütunları görün:

```
ÖZNİTELİK
```

1 numaralı nesneye ada numarası yazın:

```
ÖZNİTELİK ada_no 1 1234
```

Gösterim türü yazın — boşluk içerdiği için tırnak içinde:

```
ÖZNİTELİK gosterim 1 "TOPLU KONUT ALANI"
```

Bir özniteliği okuyun:

```
ÖZNİTELİK gosterim 1
```

Bir hücreyi boşaltın:

```
ÖZNİTELİK ada_no 1 yok
```

### Arayüz

Sağdaki **Öznitelikler** panelinde seçili nesnenin bütün sütunları görünür ve
düzenlenebilir. Panelden yapılan her değişiklik bu komutu gönderir — panel ikinci bir
yazma yolu değildir, komutun bir istemcisidir.

### Betik

```json
{
  "ad": "İki parsele öznitelik yaz",
  "komutlar": [
    { "cmd": "core.column",    "args": { "kimlik": "ada_no",    "tur": "tam_sayi" } },
    { "cmd": "core.column",    "args": { "kimlik": "parsel_no", "tur": "tam_sayi" } },
    { "cmd": "core.column",    "args": { "kimlik": "gosterim",  "tur": "metin" } },

    { "cmd": "core.attribute", "args": { "ad": "ada_no",    "nesne": 1, "deger": "1234" } },
    { "cmd": "core.attribute", "args": { "ad": "parsel_no", "nesne": 1, "deger": "7" } },
    { "cmd": "core.attribute", "args": { "ad": "gosterim",  "nesne": 1,
                                         "deger": "TOPLU KONUT ALANI" } }
  ]
}
```

## Geri alma

Bir öznitelik yazımı **tek bir geri alma adımıdır** ve `GERİAL` önceki değeri geri
getirir — hücre boştu ise yine boşalır.

```
GERİAL
```

**Sütun bildirimi geri alınmaz.** [`SÜTUN`](column.md) ile tanımlanan bir sütun,
`GERİAL` sonrasında da durur. Sebebi şudur: satırlar sütunlara göre adreslenir, ve bir
bildirimi geri almak günlüğün elinde tuttuğu bütün satır indislerini geçersiz kılardı.
Aynı sebeple boşalmış bir katman da geri alınırken silinmez.

## Betikten kullanım

`ÖZNİTELİK` betiklenebilir ve AI erişimine açıktır. Toplu öznitelik yazımı — bir
TKGM dökümünden ada/parsel numaralarını işlemek gibi — betiğin doğal işidir.

Her çağrı kendi geri alma adımıdır. Beş yüz parsele numara yazan bir betik beş yüz adım
bırakır; tamamını tek adım yapmak için betiği [`BETİK`](script.md) ile çalıştırın.

AI bu komutu kullanabilir; ama yazdığı her değer önizlenir ve onaylanmadan uygulanmaz
(`CLAUDE.md` 5.7).

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bu belgede tanımlı öznitelik yok. Şema veri paketinden yüklenir; elle sütun eklemek için ÖZNİTELİK TANIMLA kullanın.` | Hiç sütun tanımlanmamış | [`SÜTUN`](column.md) ile bir sütun tanımlayın |
| `Bilinmeyen öznitelik: 'gosterm'. Tanımlı olanları görmek için argümansız ÖZNİTELİK yazın.` | Sütun adı yanlış yazılmış | Argümansız `ÖZNİTELİK` ile listeyi görün |
| `Hangi nesne? Kullanım: ÖZNİTELİK <ad> <nesne-kimliği> [deger]` | Nesne kimliği verilmemiş | Kimliği ekleyin; `SEÇ` ile öğrenebilirsiniz |
| `Bilinmeyen nesne: 99. Nesne kimliklerini SEÇ ile görebilirsiniz.` | O kimlikte nesne yok ya da silinmiş | `SEÇ TÜMÜ` ile mevcut kimlikleri listeleyin |
| `'ada_no' özniteliği tam sayı bekliyor. Girilen: 'bin iki yüz'` | Sayı isteyen bir sütuna metin verilmiş | Rakamla yazın; eski değer yerinde kalır |
| `'ada_no' özniteliği evet/hayır bekliyor. Girilen: 'belki'` | Evet/hayır sütununa başka bir şey verilmiş | `evet`, `hayır`, `1` veya `0` yazın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
