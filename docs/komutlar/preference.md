# TERCİH — Uygulama Tercihleri

PiriCAD'i kendi çalışma alışkanlığına göre ayarlayan herkes için; bu sayfayı
bitirdiğinizde tema, dil, otomatik kayıt ve tuval rengi gibi **kullanıcıya ait**
tercihleri listelemeyi, okumayı ve değiştirmeyi bileceksiniz.

> **Faz 0 durumu.** Tercihler artık kullanıcı profiline yazılıyor: `TERCİH` ile
> değiştirdiğiniz her tercih, programı kapatırken kaydediliyor ve yeniden açtığınızda
> geri geliyor. Kaydedilen dosya, işletim sisteminizin standart uygulama ayarları
> dosyasıdır ve her tercih kendi kimliğiyle (`core.arayuz.tema` gibi) yazılır.
>
> Tema, `TERCİH tema koyu` yazdığınızda da, **Görünüm > Koyu Tema** menüsünü
> işaretlediğinizde de aynı yoldan geçer: menü de komut yolunun bir istemcisidir, ikinci
> bir tercih listesi yoktur. Arayüz dilinin değişmesi için programın yeniden başlatılması
> **Faz 1'de** kalkacaktır; ayrıntısı `.claude/model.md` R38 ve R39'dadır.

## Ne yapar

`TERCİH`, **uygulama kapsamındaki** ayarları yönetir: kullanıcıya ve makineye ait olan,
çizimin verisiyle hiçbir ilgisi bulunmayan tercihler. Tema, arayüz dili, otomatik kayıt
aralığı, son dosya listesinin uzunluğu ve tuval arka plan rengi böyledir.

Bunlar çizim dosyasına **yazılmaz**, komut günlüğüne **girmez** ve `GERİAL` ile geri
alınmaz. Sebebi tek cümlede şudur: *bir tercihin, imzaladığınız belgenin tek bir baytını
bile değiştirmemesi gerekir.* Belgeyi değiştirebilen her ayar proje ayarıdır ve
[`AYAR`](setting.md) komutuna aittir.

Üç kullanım biçimi vardır:

- **Argümansız** — bütün tercihleri, değerleriyle ve varsayılan olup olmadıklarıyla listeler
- **Yalnızca ad** — o tercihin değerini, kimliğini, türünü, varsayılanını ve kaynağını yazar
- **Ad ve değer** — tercihi değiştirir ve önceki değerini söyler

## Adlar

| Ad | Tür |
|---|---|
| `TERCİH` | Türkçe, birincil |
| `TERCIH` | ASCII karşılık |
| `PREFERENCE` | İngilizce karşılık |
| `PREF` | Kısaltma |
| `core.preference` | Komut kimliği |

## Sözdizimi

```
TERCİH
TERCİH <ad>
TERCİH <ad> <deger>
TERCİH ad=<ad> deger=<deger>
TERCİH <ad> varsayilan
```

Tercih adı yerine kimliği de yazılabilir: `TERCIH core.arayuz.tema koyu`.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Tercih adı veya kimliği. Verilmezse bütün tercihler listelenir |
| `deger` | Yeni değer. Verilmezse tercih yalnızca okunur. `varsayilan` yazarsanız tercih bildirilen varsayılanına döner |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

Değer yazımı [`AYAR`](setting.md) ile aynıdır: evet/hayır, tam sayı, onaltılık (`0x…`),
metin ve seçenek adı. Ondalık sayı hiçbir ayarda kabul edilmez.

## Örnekler

### Komut satırı

Bütün tercihleri görün:

```
TERCİH
```

Temayı koyu yapın:

```
TERCIH tema koyu
```

Arayüz dilini sorun:

```
TERCİH dil
```

Otomatik kaydı on dakikaya çekin — değer saniyedir:

```
TERCIH otomatik_kayit 600
```

Otomatik kaydı kapatın:

```
TERCIH otomatik_kayit 0
```

Son dosya listesini kısaltın:

```
TERCIH son_dosya_sayisi 5
```

Tuval arka planını onaltılık `0xAARRGGBB` ile verin:

```
TERCIH arkaplan 0xFF101418
```

Kılavuz ızgarayı kapatın:

```
TERCIH ızgara hayır
```

Izgarayı sabit 10 metrelik bir ağa oturtun:

```
TERCIH ızgara_modu sabit
TERCIH ızgara_adımı 10000
```

Nesne yakalama nişan alanını genişletin — değer **ekran pikselidir**, zemin metresi
değil, çünkü el 1/100 ölçekte daha sabit olmaz:

```
TERCIH yakalama_toleransı 16
```

Seçme kutusunu daraltın:

```
TERCIH seçim_toleransı 4
```

Bir tercihi varsayılanına döndürün:

```
TERCIH tema varsayilan
```

### Arayüz

Komutu pencerenin altındaki **komut satırına** yazın; sonuç **Transkript** panelinde
görünür. Arayüzün ayrıcalığı yoktur: menüden yapılan da, komut satırından yazılan da aynı
komuttur.

Katalogdan üretilen **Tercihler** iletişim kutusu **Faz 1'de** gelecek; her tercihin
alanı, aralığı ve açıklaması bildiriminden üretilecek, elle yazılmayacak.

### Betik

```json
{
  "ad": "Çalışma ortamı",
  "komutlar": [
    { "cmd": "core.preference", "args": { "ad": "core.arayuz.tema",           "deger": "koyu" } },
    { "cmd": "core.preference", "args": { "ad": "core.arayuz.dil",            "deger": "tr" } },
    { "cmd": "core.preference", "args": { "ad": "core.dosya.otomatik_kayit",  "deger": "600" } },
    { "cmd": "core.preference", "args": { "ad": "core.dosya.son_dosya_sayisi","deger": "8" } }
  ]
}
```

## Geri alma

`TERCİH` **geri alınamaz ve geri alınmamalıdır.** `GERİAL` çizimin verisini geri alır;
bir uygulama tercihi çizimin verisi değildir. Aynı sebeple tercih değişiklikleri komut
günlüğüne de yazılmaz: günlük, belgeye ne olduğunun kaydıdır.

Geri dönmek için tercihi varsayılanına döndürün ya da eski değerini yeniden yazın:

```
TERCIH tema varsayilan
```

Komut, değiştirdiği her tercihin önceki değerini de yazar; not almak için oradan
okuyabilirsiniz.

## Betikten kullanım

`TERCİH` betiklenebilir. Bir kurulum betiğinde çalışma ortamını hazırlamak için
kullanılır:

```json
[
  { "cmd": "core.preference", "args": { "ad": "core.arayuz.tema",          "deger": "koyu" } },
  { "cmd": "core.preference", "args": { "ad": "core.tuval.arkaplan",       "deger": "0xFF101418" } },
  { "cmd": "core.preference", "args": { "ad": "core.dosya.otomatik_kayit", "deger": "300" } }
]
```

Aynı betikte proje ayarları da varsa onlar [`AYAR`](setting.md) ile yazılır; iki komut
birbirinin kapsamına giremez ve bu kasıtlıdır.

`TERCİH` AI erişimine kapalıdır: kullanıcının çalışma ortamı, bir öneri motorunun işi
değildir.

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bilinmeyen ayar: 'temaa'. Beklenen: tanımlı bir ayar kimliği veya adı (23 tanımlı ayar).` | Tercih adı yanlış yazılmış | Mesajın devamındaki `Bunu mu demek istediniz:` önerisine bakın veya `TERCİH` yazıp listeyi görün |
| `'core.crs.hassasiyet' ayarı proje kapsamındadır; bu komut uygulama ayarlarını yönetir.` | Proje ayarı `TERCİH` ile değiştirilmeye çalışılmış | [`AYAR`](setting.md) komutunu kullanın |
| `'core.arayuz.tema' ayarı şu seçeneklerden birini bekliyor: sistem, acik, koyu. Girilen: 'karanlik'` | Listede olmayan bir seçenek yazılmış | Mesajın saydığı seçeneklerden birini yazın |
| `'core.dosya.otomatik_kayit' ayarı tam sayı bekliyor. Girilen: 'on dakika'` | Sayı isteyen bir tercihe metin verilmiş | Saniye cinsinden tam sayı yazın |
| `'core.dosya.son_dosya_sayisi' için 99 değeri [0, 50] aralığının dışında; 50 değerine kırpıldı.` | Değer bildirilen aralığın dışında | Hata değildir: değer aralığa kırpılır ve size söylenir |
| `'core.preference' daha fazla argüman almıyor. Fazlalık: 'koyu'` | İkiden fazla argüman verilmiş | Boşluk içeren değerleri tırnak içine alın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
