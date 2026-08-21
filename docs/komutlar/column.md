# SÜTUN — Öznitelik Sütunu Tanımlama

Çizime yeni bir veri alanı ekleyen herkes için; bu sayfayı bitirdiğinizde bir
öznitelik sütununu tanımlamayı ve tanımlı sütunları listelemeyi bileceksiniz.

> **Faz 0 durumu.** Sütunlar elle tanımlanıyor. **Faz 1'de** BÖHHBÜY ve TUCBS
> şemaları veri paketinden yüklenecek ve bir kadastro çizimi açıldığında doğru
> sütunlarla açılacak; `SÜTUN` o zaman da kalacak, çünkü kataloğun bilmediği bir
> çalışma alanı — bir arazi notu, bir ölçü referansı — hâlâ gerekir.

## Ne yapar

`SÜTUN`, belgeye bir **öznitelik sütunu** tanımlar. Sütun bir kez tanımlanır ve
belgedeki bütün nesnelerin o sütunda bir hücresi olur; çoğu boş kalsa bile.

Bu, her nesnenin kendi içinde ad-değer çiftleri taşıdığı tasarımdan kasıtlı olarak
farklıdır. Beş milyon parselin her birinde üç etiketlik bir torba taşımak on beş milyon
ayrı bellek ayırma demektir; üç dizi ise üç dizidir. Ayrıca "bu belgede hangi alanlar
var" sorusunun tek ve kesin bir cevabı olur — nesneleri tek tek gezmek gerekmez.

Sütun **tanımlamak** ile hücreye **yazmak** ayrı komutlardır. Yazmak için
[`ÖZNİTELİK`](attribute.md) kullanın.

## Adlar

| Ad | Tür |
|---|---|
| `SÜTUN` | Türkçe, birincil |
| `SUTUN` | ASCII karşılık |
| `COLUMN` | İngilizce karşılık |
| `STN` | Kısaltma |
| `core.column` | Komut kimliği |

## Sözdizimi

```text
SÜTUN
SÜTUN <kimlik> <tur>
SÜTUN kimlik=<kimlik> tur=<tur>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `kimlik` | Sütunun kimliği. Verilmezse tanımlı sütunlar listelenir |
| `tur` | Sütunun tuttuğu veri türü |

Türler:

| Tür | Ne tutar | Örnek değer |
|---|---|---|
| `tam_sayi` | Tam sayı — ada no, parsel no, kat adedi | `1234` |
| `uzunluk` | Zeminde **milimetre** tam sayı | `12345` (12,345 m) |
| `evet_hayir` | Doğru/yanlış | `evet` |
| `metin` | Serbest metin | `TOPLU KONUT ALANI` |

**Hiçbir sütun ondalık sayı tutmaz.** Yarım değer isteyen bir alan, bildirilmiş bir
birimde tam sayıdır: uzunluk milimetre, açı mikro derecedir. Sebebi, ondalık sayıların
iki bilgisayarda aynı baytı vermemesi ve dışa aktarılan belgenin bit düzeyinde aynı
olması gerektiğidir.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Kadastro çizimi için üç sütun:

```
SÜTUN ada_no tam_sayi
SUTUN parsel_no tam_sayi
SUTUN malik metin
```

Bina için kat adedi ve yapı yüksekliği:

```
SUTUN kat_adedi tam_sayi
SUTUN yapi_yuksekligi uzunluk
```

Bir bayrak alanı:

```
SUTUN kamulastirildi evet_hayir
```

Tanımlı sütunları listeleyin:

```
SÜTUN
```

### Arayüz

Sağdaki **Öznitelikler** panelinin başlığındaki **+** düğmesi bu komutu gönderir.
Panelden tanımlanan bir sütun ile komut satırından tanımlanan bir sütun arasında hiçbir
fark yoktur; ikinci bir şema listesi yoktur.

### Betik

```json
{
  "ad": "Kadastro şeması",
  "komutlar": [
    { "cmd": "core.column", "args": { "kimlik": "ada_no",    "tur": "tam_sayi" } },
    { "cmd": "core.column", "args": { "kimlik": "parsel_no", "tur": "tam_sayi" } },
    { "cmd": "core.column", "args": { "kimlik": "malik",     "tur": "metin" } },
    { "cmd": "core.column", "args": { "kimlik": "yuzolcumu", "tur": "uzunluk" } }
  ]
}
```

## Geri alma

`SÜTUN` **geri alınamaz.** `GERİAL` bir sütun bildirimini kaldırmaz.

Sebebi şudur: satırlar sütunlara göre adreslenir, ve bir bildirimi geri almak komut
günlüğünün elinde tuttuğu bütün satır indislerini geçersiz kılardı — geri alma
yığınında duran eski bir öznitelik yazımı artık var olmayan bir sütuna işaret ederdi.
Aynı sebeple boşalmış bir katman da geri alınırken silinmez.

Bir sütun yanlış tanımlandıysa hücrelerini `ÖZNİTELİK <ad> <nesne> yok` ile boşaltın;
sütunun kendisi belgede kalır ama hiçbir şey söylemez.

## Betikten kullanım

`SÜTUN` betiklenebilir. Bir çizim şablonu betiğinin ilk satırları tipik olarak budur:
şema kurulur, sonra geometri gelir.

`SÜTUN` **AI erişimine kapalıdır**: bir belgenin hangi alanları taşıdığı, imzalayan
mühendisin kararıdır ve bir öneri motorunun sessizce genişleteceği bir şey değildir.

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Kullanım: SÜTUN <kimlik> <tur>. Türler: tam_sayi, uzunluk, evet_hayir, metin.` | Tür verilmemiş | Kimlikten sonra türü de yazın |
| `Bilinmeyen öznitelik türü: 'ondalik'. Beklenen: tam_sayi, uzunluk, evet_hayir, metin.` | Listede olmayan bir tür yazılmış | Ondalık sayı yoktur; `uzunluk` milimetre tam sayısıdır |
| `Aynı kimlikte bir öznitelik zaten var: 'ada_no'` | Sütun daha önce tanımlanmış | Argümansız `SÜTUN` ile listeyi görün |
| `Öznitelik kimliği boş olamaz.` | Kimlik boş dize verilmiş | Bir kimlik yazın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
