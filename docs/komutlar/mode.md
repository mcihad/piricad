# MOD — Oturum Modları

Ekranda çizim yapan herkes için; bu sayfayı bitirdiğinizde nesne yakalama, dik mod,
kutupsal izleme ve ızgaraya yakalama gibi **çizerken yardımcı olan** modları
listelemeyi, okumayı ve değiştirmeyi bileceksiniz.

> **Faz 0 durumu.** Modların tamamı bildirilmiş, okunabilir ve yazılabilir durumda;
> `MOD` komutu, betik ve yapay zekâ dahil her istemciden aynı değeri görür. Modların
> imleci fiilen yönlendirmesi — yani yakalama motoru, dik kilit ve kutupsal izleme
> çizgileri — **Faz 1'de** gelecektir. Bugün mod yazmak değeri gerçekten değiştirir,
> ancak fare hareketi henüz o değere bakmaz. Izgara ise bugün çalışır: bkz.
> [`TERCİH`](preference.md) altındaki `ızgara` tercihleri.

## Ne yapar

`MOD`, **oturum kapsamındaki** ayarları yönetir: çizerken imlecin nereye oturacağını
belirleyen girdi yardımları. Nesne yakalama modları, dik mod, kutupsal izleme açısı ve
ızgaraya yakalama böyledir.

Bunlar çizim dosyasına **yazılmaz**, tercih dosyasına da **yazılmaz** ve programı
kapattığınızda kaybolur. Sebebi tek cümlede şudur: *bir girdi yardımı, çizimin verisi
değildir; çizerken tuttuğunuz cetveldir.* Cetvel masada kalır, çizim gider.

Üç kapsam ve üç komut vardır; hiçbiri diğerinin kapsamına giremez:

| Kapsam | Komut | Nerede yaşar |
|---|---|---|
| Proje | [`AYAR`](setting.md) | Çizim dosyasında; geri alınabilir, günlüğe girer |
| Uygulama | [`TERCİH`](preference.md) | Kullanıcı profilinde; makineye aittir |
| Oturum | `MOD` | Yalnızca bellekte; program kapanınca biter |

Üç kullanım biçimi vardır:

- **Argümansız** — bütün modları, değerleriyle ve varsayılan olup olmadıklarıyla listeler
- **Yalnızca ad** — o modun değerini, kimliğini, türünü, varsayılanını ve kaynağını yazar
- **Ad ve değer** — modu değiştirir ve önceki değerini söyler

## Adlar

| Ad | Tür |
|---|---|
| `MOD` | Türkçe, birincil |
| `MODE` | İngilizce karşılık |
| `MD` | Kısaltma |
| `core.mode` | Komut kimliği |

## Sözdizimi

```
MOD
MOD <ad>
MOD <ad> <deger>
MOD ad=<ad> deger=<deger>
MOD <ad> varsayilan
```

Mod adı yerine kimliği de yazılabilir: `MOD core.yakalama.dik_mod evet`.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `ad` | Mod adı veya kimliği. Verilmezse bütün modlar listelenir |
| `deger` | Yeni değer. Verilmezse mod yalnızca okunur. `varsayilan` yazarsanız mod bildirilen varsayılanına döner |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

Bugün oturum kapsamında dört mod vardır:

| Mod | Tür | Varsayılan | Ne yapar |
|---|---|---|---|
| `yakalama_modları` | Bit maskesi | `7` | Etkin nesne yakalama modları |
| `dik_mod` | Evet/hayır | `hayır` | İmleci yatay ve düşey eksene kilitler |
| `kutupsal_açı` | µderece | `45000000` (45°) | Kutupsal izleme açı adımı |
| `ızgaraya_yakala` | Evet/hayır | `hayır` | Girilen noktayı en yakın ızgara kesişimine oturtur |

Açı değerleri **mikro derece** cinsindendir: 45° = `45000000`. Ondalık sayı hiçbir ayarda
kabul edilmez; bildirilen birim yeterince incedir.

## Örnekler

### Komut satırı

Bütün modları görün:

```
MOD
```

Dik modu açın — yalnız yatay ve düşey çizgi çizilir:

```
MOD dik_mod evet
```

Kutupsal izlemeyi 30 dereceye çekin:

```
MOD kutupsal_açı 30000000
```

Izgaraya yakalamayı açın:

```
MOD ızgaraya_yakala evet
```

Bir modun ne olduğunu sorun:

```
MOD yakalama_modları
```

Bir modu varsayılanına döndürün:

```
MOD dik_mod varsayilan
```

### Arayüz

Komutu pencerenin altındaki **komut satırına** yazın; sonuç **Transkript** panelinde
görünür. Arayüzün ayrıcalığı yoktur: menüden yapılan da, komut satırından yazılan da aynı
komuttur.

Durum çubuğundaki mod düğmeleri ve `F8` / `F9` kısayolları **Faz 1'de** gelecek; her
düğme bu komutu çalıştıracak, ikinci bir mod listesi olmayacak.

### Betik

```json
{
  "ad": "Dik çizim ortamı",
  "komutlar": [
    { "cmd": "core.mode", "args": { "ad": "core.yakalama.dik_mod",     "deger": "evet" } },
    { "cmd": "core.mode", "args": { "ad": "core.yakalama.izgara",      "deger": "evet" } },
    { "cmd": "core.mode", "args": { "ad": "core.yakalama.kutupsal_aci","deger": "30000000" } }
  ]
}
```

## Geri alma

`MOD` **geri alınamaz ve geri alınmamalıdır.** `GERİAL` çizimin verisini geri alır; bir
girdi yardımı çizimin verisi değildir. Aynı sebeple mod değişiklikleri komut günlüğüne de
yazılmaz: günlük, belgeye ne olduğunun kaydıdır.

Geri dönmek için modu varsayılanına döndürün ya da eski değerini yeniden yazın:

```
MOD dik_mod varsayilan
```

Komut, değiştirdiği her modun önceki değerini de yazar; not almak için oradan
okuyabilirsiniz.

## Betikten kullanım

`MOD` betiklenebilir. Bir kurulum betiğinin başında çizim ortamını hazırlamak için
kullanılır:

```json
[
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.dik_mod", "deger": "evet" } },
  { "cmd": "core.mode", "args": { "ad": "core.yakalama.izgara",  "deger": "evet" } }
]
```

Modlar oturumla birlikte biter, bu yüzden bir betiğin açtığı dik mod bir sonraki
oturuma taşınmaz. Kalıcı olmasını istediğiniz şey bir mod değil, bir tercihtir; onlar
[`TERCİH`](preference.md) komutuna aittir.

`MOD` AI erişimine kapalıdır: bir öneri motoru kendi işini kolaylaştırmak için
kullanıcının çizim yardımlarını değiştiremez.

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bilinmeyen ayar: 'dikmod'. Beklenen: tanımlı bir ayar kimliği veya adı (23 tanımlı ayar).` | Mod adı yanlış yazılmış | Mesajın devamındaki `Bunu mu demek istediniz:` önerisine bakın veya `MOD` yazıp listeyi görün |
| `'core.crs.id' ayarı proje kapsamındadır; bu komut oturum ayarlarını yönetir.` | Proje ayarı `MOD` ile değiştirilmeye çalışılmış | [`AYAR`](setting.md) komutunu kullanın |
| `'core.arayuz.tema' ayarı uygulama kapsamındadır; bu komut oturum ayarlarını yönetir.` | Tercih `MOD` ile değiştirilmeye çalışılmış | [`TERCİH`](preference.md) komutunu kullanın |
| `'core.yakalama.dik_mod' ayarı evet/hayır bekliyor. Girilen: 'açık'` | Evet/hayır isteyen bir moda başka bir şey verilmiş | `evet`, `hayır`, `1` veya `0` yazın |
| `'core.yakalama.kutupsal_aci' için 400000000 değeri [1000, 360000000] aralığının dışında; 360000000 değerine kırpıldı.` | Değer bildirilen aralığın dışında | Hata değildir: değer aralığa kırpılır ve size söylenir |
| `'core.mode' daha fazla argüman almıyor. Fazlalık: 'evet'` | İkiden fazla argüman verilmiş | Boşluk içeren değerleri tırnak içine alın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
