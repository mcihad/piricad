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
SÜTUN kimlik=<kimlik> tur=<tur> ad=<ad> aciklama=<açıklama> zorunlu=<evet|hayır>
SÜTUN kimlik=<kimlik> basamak=<sayı>          (yalnız ondalık için)
SÜTUN kimlik=<kimlik> katalog=<katalog>       (yalnız kod için)
SÜTUN kimlik=<kimlik> sil=evet
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `kimlik` | Sütunun kimliği. Verilmezse tanımlı sütunlar listelenir |
| `tur` | Sütunun tuttuğu veri türü. Var olan bir sütunda **değiştirilemez** |
| `ad` | Panelde görünen Türkçe ad. Verilmezse kimliğin kendisi kullanılır |
| `aciklama` | Tek satırlık açıklama |
| `zorunlu` | Her satır bir değer taşımalı mı |
| `basamak` | Yalnız `ondalik` için: noktadan sonra kaç basamak. Varsayılan `2` |
| `katalog` | Yalnız `kod` için: kodların çekildiği katalog kimliği |
| `katman` | Sütunu **yalnız o katmana** tanımlar. Verilmezse **proje geneli** |
| `sil` | `evet` verilirse sütunu ve içindeki bütün değerleri siler |

Türler:

| Tür | Ne tutar | Örnek değer |
|---|---|---|
| `tam_sayi` | Tam sayı — ada no, parsel no, kat adedi | `1234` |
| `ondalik` | Kesirli sayı, **sabit noktalı** — oran, eğim | `0.40` |
| `uzunluk` | Zeminde **milimetre** tam sayı | `12345` (12,345 m) |
| `evet_hayir` | Doğru/yanlış | `evet` |
| `metin` | Serbest metin | `TOPLU KONUT ALANI` |
| `tarih` | Takvim günü, `YYYY-AA-GG` | `2026-09-08` |
| `kod` | Bir `/data` kataloğundan çekilen kod | `A-1` |

### İki tür sütun: projenin ve katmanın

`ada` ve `parsel` çizimdeki **her** parselin bilgisidir; `direk_yuksekligi` ise yalnız
`ENERJİ` katmanındaki nesnelerin. İkincisini proje geneline tanımlamak, çizimdeki her
yolun, her ağacın ve her parselin öznitelik panelinde asla doldurulamayacak boş bir
satır demektir.

| Yazılış | Kim taşır |
|---|---|
| `SÜTUN kimlik=ada tur=tam_sayi` | **Her** nesne |
| `SÜTUN kimlik=direk tur=uzunluk katman=ENERJİ` | Yalnız `ENERJİ` katmanındaki nesneler |

Arayüzde ikisinin iki ayrı yeri vardır:

| Sütun | Nereden tanımlanır |
|---|---|
| Proje sütunu | **Seçenekler ▸ Proje Öznitelikleri** |
| Katman sütunu | Katmana **sağ tık → Katman Özellikleri… → Öznitelikler** |

Katmanın sayfası proje sütunlarını da listeler — `proje sütunu` diye işaretli ve
düzenlenemez — çünkü o katmandaki bir nesnenin **taşıyacağı** alanların tamamı budur.

Katman adı **Türkçe katlanarak** karşılaştırılır: `Enerji` ile `ENERJİ` aynı katmandır.

Bir sütunun kapsamı sonradan da değiştirilebilir; yanlışlıkla proje geneline
tanımlanmış bir alan tek komutla yerine oturur:

```
SÜTUN kimlik=direk katman=ENERJİ
```

### Ondalık neden "float" değil

`ondalik` bir kayan noktalı sayı **değildir**: hücrede bir **tam sayı** durur ve
noktanın nereye geleceğini sütunun `basamak` bildirimi söyler. `basamak=2` ile `0,40`
diskte `40` olarak yazılır.

Sebebi şudur: `0.4` ikili kayan noktada tam olarak temsil edilemez. Böyle bir değer
yazılıp okunduğunda son bitte değişebilir — ve bu belgenin altın fikstürleri üç
işletim sisteminde **bayt bayt** karşılaştırılır. Sabit nokta ile aritmetik kesin,
sıralama tam sayıların kendi sıralaması, metin karşılığı ise tam gidip tam gelir.

Yazarken **hem virgül hem nokta** kabul edilir: `0,40` ile `0.40` aynı sayıdır.
Bildirilen basamaktan **fazlası hata verir**, yuvarlanmaz — `basamak=2` bir sütuna
`0.405` yazmak reddedilir, çünkü sessizce `0,40`'a çevirmek belgeye kullanıcının
söylemediği bir şeyi yazdırmak olurdu.

### Tarih neden yalnız gün

`tarih` bir **gün** tutar, bir an değil: 1970-01-01'den bu yana geçen gün sayısı.
Bir plan bir **tarihte** onaylanır, bir saatte değil; saat tutmak, saat dilimi
taşımak zorunda olmayan bir belgeye saat dilimi sokardı.

Biçim `YYYY-AA-GG`'dir ve yalnız odur. `12/03/2026`'nın mart mı aralık mı olduğu
ülkeye göre değişir; ISO 8601 tam da bu belirsizliği bitirmek için vardır.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Sütunu düzenlemek

Var olan bir kimlikle çağrılan `SÜTUN` yeni bir sütun tanımlamaz, **var olanı
düzenler**:

```
SÜTUN kimlik=oran ad="Ölçülen Oran" aciklama="Arazide ölçülen oran" zorunlu=evet
```

Değiştirilebilenler: `ad`, `aciklama`, `zorunlu`, `katalog`, `basamak` ve `katman`.

**Kimlik ve tür değiştirilemez.** Kimlik, sütuna atıfta bulunan her sembolün, her
kuralın ve her günlük satırının adlandırdığı şeydir; tür ise saklanan sayıların
**anlamıdır**. Başka bir tür istiyorsanız sütunu silip yeniden tanımlayın.

`basamak` **artırılabilir, azaltılamaz.** İki basamaktan üçe çıkmak `0,40`'ı `0,400`
yapar — aynı sayıdır, hücreler ölçeklenir ve kimsenin girdiği bir değer değişmez.
Azaltmak ise birinin bilerek girdiği bir basamağı atmak olurdu.

## Sütunu silmek

```
SÜTUN kimlik=oran sil=evet
```

**Geri alınamaz.** Şema değişiklikleri [`GERİAL`](undo.md) ile geri gelmez (satırların
adreslendiği şey şemanın kendisidir), ve sütunla birlikte içindeki bütün değerler
gider. Arayüzde bu işlem bir onay sorusu sorar.

## Arayüz

Katmanlar panelinde bir katmana **sağ tıklayın → Katman Özellikleri… → Öznitelikler**.
Sayfa tanımlı bütün sütunları tablo hâlinde gösterir ve altında üç düğme taşır:
**Ekle…**, **Düzenle…**, **Sil**.

Açılan form türe göre değişir: `ondalik` seçildiğinde **Basamak** satırı, `kod`
seçildiğinde **Katalog** satırı görünür; diğer türlerde görünmezler, çünkü değeri
kullanılmayacak bir kutu formun söylediğine güveni azaltır.

Düzenlemede **Kimlik** ve **Tür** kutuları kapalıdır — komut da onları reddeder ve
bir formun reddedilecek bir şeyi yazdırması yanıltıcı olurdu.

Bu sayfadan tanımlanan sütun **yalnız o katmana** aittir. Çizimin tamamına ait bir
alan için **Seçenekler ▸ Proje Öznitelikleri** sayfasını kullanın; sayfanın üstündeki
not hangisinde olduğunuzu yazar.

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

Ondalık ve tarih sütunları:

```
SÜTUN kimlik=oran tur=ondalik basamak=2 ad="Oran"
SÜTUN kimlik=onay_tarihi tur=tarih ad="Onay Tarihi"
SÜTUN kimlik=tescilli tur=evet_hayir ad="Tescilli" zorunlu=hayır
```

Tanımlı sütunları listeleyin:

```
SÜTUN
```

### Arayüz

Katmanlar panelinde katmana **sağ tık → Katman Özellikleri… → Öznitelikler**; sayfanın
altındaki **Ekle…**, **Düzenle…** ve **Sil** düğmeleri bu komutu gönderir. Sayfadan
tanımlanan bir sütun ile komut satırından tanımlanan bir sütun arasında hiçbir fark
yoktur; ikinci bir şema listesi yoktur.

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
