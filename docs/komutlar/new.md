# YENİ — Boş Çizim Başlatma

Elindeki işi bitirip yenisine başlayan kullanıcı için; bu sayfayı bitirdiğinizde
boş bir çizime geçebilecek, geçerken neyin sıfırlandığını ve neyin yerinde
kaldığını bileceksiniz.

## Ne yapar

Boş bir çizim açar ve **ekrandaki çizimin yerine koyar**.

`AÇ` ile aynı yer değiştirmedir; tek farkı okunacak bir dosyanın olmamasıdır.
Yeni çizim, programı yeni açmışsınız gibidir: içinde nesne yok, yalnız `0`
katmanı var, koordinat sistemi programın varsayılanıdır
([koordinat sistemleri](../veri/koordinat-sistemleri.md)).

Yer değiştirmeyle birlikte **sıfırlananlar**:

- **Çizim.** Nesneler, katmanlar, öznitelik sütunları, bloklar — hepsi gider.
- **Geri alma yığını.** Yeni çizimden `GERİAL` ile eskisine dönülemez.
- **Bağlı olunan dosya.** Yeni çizim adsızdır; `KAYDET` bir ad soracaktır.
- **Seçim.** Seçili nesne kalmaz.
- **Görünüm.** Tuval, yeni bir çizimin başladığı yere döner.
- **Proje ayarları.** Plan ölçeği, çizim birimi, koordinat sistemi gibi
  *çizime ait* ayarlar kendi varsayılanlarına döner — bunlar dosyayla birlikte
  giden, o çizime ait değerlerdir.

**Sıfırlanmayanlar** — bunlar çizime değil, size ve bu makineye aittir:

- **Uygulama tercihleri** (`TERCİH`): tema, kataloğa giden yollar, arayüz
  ayarları.
- **Sembol kitaplığı.** Rafta duran semboller neyi yüklediğinizle ilgilidir,
  hangi çizimin açık olduğuyla değil.
- **Yazdırma profilleri.** Kâğıt, yön, çözünürlük ve kenar boşlukları olduğu
  gibi kalır.
- **Oturum modları** (`MOD`): yakalama, dik mod, kutupsal izleme.

**`YENİ` komutun kendisi sormaz.** Kaydedilmemiş çalışmanız varsa soruyu
*pencere* sorar: `Dosya ▸ Yeni`, **Ctrl+N** ya da sekme şeridindeki **+**
düğmesi, komutu göndermeden önce **Kaydet / Atla / Vazgeç** seçeneklerini
gösterir. Komut satırına doğrudan `YENİ` yazarsanız ya da bir betikten
çağırırsanız soru sorulmaz — `AÇ` da aynı şekilde davranır. Sebebi mimaridir:
komut gövdesi fare, klavye, betik ve yapay zekâ için aynı çalışır ve toplu bir
çalıştırmada "kaydedilsin mi?" sorusunu yanıtlayacak kimse yoktur.

## Adlar

| Ad | Tür |
|---|---|
| `YENİ` | Türkçe, birincil |
| `YENI` | Türkçe karaktersiz klavye için |
| `NEW` | İngilizce karşılık |
| `core.new` | Komut kimliği |

## Sözdizimi

```text
YENİ
```

Parametresi yoktur; bir şey sormaz ve hemen çalışır.

## Parametreler

**Yoktur.** Komut hiçbir argüman almaz; verirseniz reddeder (bkz. *Hatalar*).

Üretilmiş tablo için [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```
YENİ
```

Transkript şunu yazar:

```text
Yeni çizim açıldı. Adsız — KAYDET bir dosya adı soracak.
```

Bitmiş bir işi kaydedip yenisine geçmenin tam hâli:

```
KAYDET
YENİ
KATMAN ad=PARSEL
```

### Arayüz

Üç yol vardır ve üçü de aynı komutu gönderir:

| Yol | Nerede |
|---|---|
| **Dosya ▸ Yeni** | Menü çubuğu |
| **Ctrl+N** | Klavye; fare gerekmez |
| **+** | Tuvalin üstündeki doküman sekmesi şeridinin sonunda |

Çizimde kaydedilmemiş değişiklik varsa önce şu soru gelir:

```text
<çizim-adı> üzerinde kaydedilmemiş değişiklikler var.

Yeni çizime geçmeden önce kaydedilsin mi?
```

- **Kaydet** — çizimi yazar, sonra yeni çizime geçer. Çizim henüz adsızsa
  *Farklı Kaydet* penceresi açılır; o pencereyi iptal ederseniz geçiş de iptal
  olur, işiniz durur.
- **Atla** — kaydetmeden geçer.
- **Vazgeç** — hiçbir şey olmaz.

Değişiklik yoksa soru sorulmaz.

### Betik

Bir betiğin ilk satırı olarak, temiz bir sayfadan başlamak için:

```json
{
  "ad": "Ada 12 parselasyonu",
  "komutlar": [
    { "cmd": "core.new", "args": {} },
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } }
  ]
}
```

Bir betiğin **ortasında** çalıştırmak da geçerlidir: o ana kadar çizilen her şey
ekrandan kalkar, betiğin geri kalanı boş çizim üzerinde çalışır ve tek bir geri
alma adımı olarak birleşir.

## Geri alma

**`YENİ` geri alınamaz.** Bir çizimi değiştirmez; onun yerine bir başkasını
koyar, dolayısıyla tersine çevrilecek bir düzenleme yoktur.

Geri alma yığını da temizlenir. Yığındaki adımlar artık var olmayan bir belgenin
nesnelerini gösteriyordu; bırakılsalardı bir `GERİAL`, eski çizimin
düzenlemelerini yenisinin üzerine uygulardı — kadastro çiziminde bu, komşu
parselin üzerine yazmak demektir.

Bu, `AÇ`'ın ve kullanıcıların geldiği her CAD ve CBS programının yaptığı şeydir.
Yanlışlıkla yeni çizime geçtiyseniz kaydettiğiniz dosyayı `AÇ` ile geri açın;
kaydetmediğiniz iş gitmiştir.

## Betikten kullanım

`YENİ` betiklenebilirdir ve betikte **soru sormaz**: kaydedilmemiş çalışma varsa
uyarısız gider. Betiğin başında kullanın, ya da bir satır önce `KAYDET` yazın.

`YENİ` yapay zekâya **kapalıdır**. Belgeyi kaydedilmemiş çalışmayla birlikte
değiştiren, geri alınamayan bir işlem bir öneriyle tetiklenmemelidir; `AÇ` da
aynı sebeple kapalıdır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `'core.new': bilinmeyen parametre 'dosya'. Tanımlı parametreler: (yok)` | Komuta ad=değer verildi | `YENİ` argüman almaz; yalnız adını yazın |
| `'core.new' daha fazla argüman almıyor. Fazlalık: ...` | Komutun ardına bir değer yazıldı | `YENİ` argüman almaz; yalnız adını yazın |
| `Dosya motoru bağlı değil; bu ortamda dosya açılıp kaydedilemez.` | Dosya motoru olmayan bir ortamda çalışılıyor | Uygulama içinden çalıştırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
