# YAKINLAŞ — Görünüm Ayarlama

Çiziminde gezinen herkes için; bu sayfayı bitirdiğinizde görünümü kapsama sığdırmayı,
oranla yakınlaştırmayı, önceki görünümlere geri dönmeyi ve bunu çalışan bir komutu
bozmadan yapmayı bileceksiniz.

## Ne yapar

Harita görünümünü değiştirir. Dokuz kipi vardır: çizimin tamamını pencereye sığdırmak, bir
çarpanla yakınlaştırıp uzaklaştırmak, başlangıç görünümüne dönmek, görünüm geçmişinde bir
adım geri ya da ileri gitmek, iki köşeli bir pencereye yakınlaşmak, bir noktayı ortaya
alıp istenen ölçeğe geçmek, seçili nesneleri ve bir katmanın bütün nesnelerini pencereye
sığdırmak.

`YAKINLAŞ` çizime dokunmaz. Görünüm ayarıdır, çizimin verisi değildir; bu yüzden geri
alma yığınına girmez ve `GERİAL` ile geri gelmez.

**Şeffaf komuttur:** başka bir komut çalışırken araya girebilir. Çizgi çizerken
yakınlaşıp kaldığınız yerden devam edebilirsiniz.

## Adlar

| Ad | Tür |
|---|---|
| `YAKINLAŞ` | Türkçe, birincil |
| `YAKINLAS` | Türkçe karaktersiz klavye için |
| `LİMİTBUL` | Türkçe eş ad: Netcad'deki adı (Limit Bul). Tek başına yazılınca, `YAKINLAŞ` gibi, çizimin kapsamına yakınlaşır |
| `LIMITBUL` | ASCII karşılık |
| `ZOOM` | İngilizce karşılık |
| `Z` | Kısaltma |
| `core.zoom` | Komut kimliği |

## Sözdizimi

```
YAKINLAŞ
YAKINLAŞ KAPSAM
YAKINLAŞ ÇARPAN carpan=<sayı>
YAKINLAŞ SIFIRLA
YAKINLAŞ ÖNCEKİ
YAKINLAŞ SONRAKİ
YAKINLAŞ PENCERE pencere=<köşe> <köşe>
YAKINLAŞ MERKEZ merkez=<nokta> [olcek=<N>]
YAKINLAŞ SEÇİM [nesneler=<kimlik>…]
YAKINLAŞ KATMAN katman=<ad>
```

Kip verilmezse `KAPSAM` varsayılır; `pencere=` verilmişse `PENCERE`, `merkez=` verilmişse
`MERKEZ`, `nesneler=` verilmişse `SEÇİM`, `katman=` verilmişse `KATMAN` anlaşılır, yani
`YAKINLAŞ pencere=… …` ya da `YAKINLAŞ katman=PARSEL` da olur.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `mod` | `KAPSAM`, `ÇARPAN`, `SIFIRLA`, `ÖNCEKİ` ya da `SONRAKİ`. İngilizce karşılıkları `EXTENTS`, `FACTOR`, `RESET`, `PREVIOUS`, `NEXT` de kabul edilir; Türkçe harfsiz yazım da geçer (`ONCEKI`, `SONRAKI`) |
| `carpan` | `ÇARPAN` kipinde ölçek katsayısı. Birden büyük yakınlaştırır, birden küçük uzaklaştırır |
| `pencere` | `PENCERE` kipinde pencerenin iki karşı köşesi, hangi sırayla olursa olsun. `pencere=` bir kez yazılıp iki köşe ardına verilebilir ya da iki kez yazılabilir |
| `merkez` | `MERKEZ` kipinde görünümün ortasına gelecek nokta |
| `nesneler` | `SEÇİM` kipinde çerçevelenecek nesnelerin kimlikleri. Verilmezse o anki seçim çerçevelenir. Betik ve yapay zekâ nesneleri bununla adlandırır, ekranda neyin seçili kaldığına güvenmez |
| `katman` | `KATMAN` kipinde bütün nesneleri çerçevelenecek katmanın adı. Türkçe harfsiz yazım da geçer |
| `olcek` | `MERKEZ` kipinde ölçek paydası: `500` yazılırsa görünüm 1:500 olur, durum çubuğu da `1 : 500` yazar. Verilmezse ölçek değişmez. 1 ile 100 000 000 arasında bir tam sayı |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

### Kipler

| Kip | Ne yapar |
|---|---|
| `KAPSAM` | Görünür bütün nesneleri, kenarlarda pay bırakarak pencereye sığdırır. Çizim boşsa başlangıç görünümüne döner |
| `ÇARPAN` | Görünümün merkezini koruyarak `carpan` kadar ölçekler |
| `SIFIRLA` | Başlangıç görünümüne döner |
| `ÖNCEKİ` | Bir önceki görünüme döner. Netcad'in Önceki Pencere'si gibi otuz adım geri gider |
| `SONRAKİ` | `ÖNCEKİ` ile geri dönülen görünümden bir adım ileri gider |
| `PENCERE` | İki köşesi verilen pencereyi görünüme **paysız** sığdırır: pencerenin kenarı ekranın kenarına oturur. Netcad'in Pencere Büyüt'ü |
| `MERKEZ` | Noktayı görünümün ortasına alır; `olcek` verilmişse o ölçeğe geçer |
| `SEÇİM` | Seçili nesneleri — ya da `nesneler=` ile verilenleri — `KAPSAM` gibi kenarlarda pay bırakarak pencereye sığdırır |
| `KATMAN` | Katmanın **bütün** nesnelerini, gizli olanlar da dahil, `KAPSAM` gibi pay bırakarak pencereye sığdırır. Netcad'in katman menüsündeki Limit Bul'u |

### Görünüm geçmişi

Program her görünüm değişikliğini hatırlar: `YAKINLAŞ`'ın her kipi, `KAYDIR`, fare
tekerleği ve orta tuşla sürükleme. `ÖNCEKİ` bunlardan otuz adım geri, `SONRAKİ` geri
gidilen adımlar kadar ileri gider.

| Ne olur | Geçmişte |
|---|---|
| Fare tekerleğiyle art arda yakınlaşmak | **Tek adım**: birbirinden en fazla 0,7 saniye arayla dönen çentikler bir bakıştır, on çentiği geri almak için on kez `ÖNCEKİ` gerekmez |
| Orta tuşla bir sürükleme | Tek adım, ne kadar uzun sürerse sürsün |
| Görünümü değiştirmeyen bir hamle (görünüm zaten kapsamdayken `KAPSAM`) | Adım değildir |
| `ÖNCEKİ`'den sonra yeni bir hamle | İleri adımlar silinir; tarayıcıdaki gibi yeni bir dal başlar |
| `YENİ` ile yeni çizim | Geçmiş silinir: eski çizimin görünümüne geri gidilmez |
| Otuz birinci adım | En eskisi düşer |

Geri gidilecek görünüm yoksa `ÖNCEKİ` hata vermez, durumu söyler: `Geri dönülecek görünüm
yok: görünüm geçmişi boş.` Böylece bir betik duruncaya kadar geri gidebilir.

Geçmiş oturumun görünüm durumudur: çizim dosyasına yazılmaz, geri alma yığınına ve komut
günlüğüne girmez.

## Örnekler

### Komut satırı

Çizimin tamamını göster:

```
YAKINLAŞ KAPSAM
```

Kip yazmadan da olur, `KAPSAM` varsayılandır:

```
YAKINLAŞ
```

Dörtte bir yakınlaştır:

```
YAKINLAŞ ÇARPAN carpan=1.25
```

Uzaklaştır:

```
YAKINLAŞ ÇARPAN carpan=0.8
```

Başlangıç görünümüne dön:

```
YAKINLAŞ SIFIRLA
```

Bir köşeye yakınlaşıp koordinat okuduktan sonra paftaya geri dön:

```
YAKINLAŞ ÖNCEKİ
```

Geri dönülen görünümden yeniden ileri git:

```
YAKINLAŞ SONRAKİ
```

Bir parselin çevresine pencereyle yakınlaş — köşelerin sırası önemsizdir:

```
YAKINLAŞ PENCERE pencere=485300,4310200 485340,4310230
```

Bir röper noktasını ortaya al ve 1:500'e geç:

```
YAKINLAŞ MERKEZ merkez=485320,4310215 olcek=500
```

Bir parsel çizip seçin, sonra seçtiğinize yakınlaşın:

```
KATMAN ad=PARSEL
ALAN 485300,4310200 485340,4310200 485340,4310230 485300,4310230
SEÇ mod=TÜMÜ
YAKINLAŞ SEÇİM
```

Bir katmanın bütününü görün:

```
YAKINLAŞ KATMAN katman=PARSEL
```

Çizgi çizerken araya girmek — komut kaldığı yerden devam eder:

```
ÇİZGİ                    ← komut "İlk nokta" ister
485300,4310200           ← ilk nokta girildi
YAKINLAŞ KAPSAM          ← araya girer, görünüm değişir
@50,30                   ← çizgi kaldığı yerden devam eder
                         ← Esc
```

### Arayüz

| Yol | Sonuç |
|---|---|
| **Görünüm ▸ Gezinme ▸ Kapsama Yakınlaş** | `YAKINLAŞ KAPSAM` |
| **Görünüm ▸ Gezinme ▸ Yakınlaştır** | `YAKINLAŞ ÇARPAN carpan=1.25` |
| **Görünüm ▸ Gezinme ▸ Uzaklaştır** | `YAKINLAŞ ÇARPAN carpan=0.8` |
| **Görünüm ▸ Gezinme ▸ Önceki Görünüm** ya da **Alt+C** | `YAKINLAŞ ÖNCEKİ` |
| **Görünüm ▸ Gezinme ▸ Sonraki Görünüm** | `YAKINLAŞ SONRAKİ` |
| **Görünüm ▸ Gezinme ▸ Seçime Yakınlaş** | `YAKINLAŞ SEÇİM` |
| **Katmanlar** panelinde satıra sağ tık ▸ **Katmana yakınlaş** | `YAKINLAŞ KATMAN katman="<ad>"` |
| **Görünüm ▸ Gezinme ▸ Pencereyle Yakınlaş** ya da **Alt+Z** | Tuvalde pencerenin bir köşesinden karşı köşesine sürükleyin ya da iki köşesine birer kez tıklayın; bırakınca `YAKINLAŞ PENCERE pencere=<köşe> <köşe>` gider. **Esc** ya da sağ tık vazgeçer |
| **Ctrl+0** | `YAKINLAŞ KAPSAM` |
| **Ctrl++** / **Ctrl+-** | Yakınlaştır / uzaklaştır |

**Pencere, çalışan komutu bozmaz.** Bir çizgi çizerken Alt+Z'ye basıp pencere
çizebilirsiniz: görünüm pencereye gelir, çizgi aynı noktayı beklemeye devam eder. Pencere
kurulu iken tuvale yapılan ilk sol tık pencerenin köşesidir, çizginin noktası değil.

Fare tekerleği ve orta tuşla kaydırma her zaman çalışır ve komut göndermez; bunlar
doğrudan görünüm etkileşimleridir. Görünüm geçmişine yine de girerler: `ÖNCEKİ` bir
tekerlek dizisinden ya da bir sürüklemeden önceki görünüme döner.

Ölçek durum çubuğunda "1 px = 0.1418 m" biçiminde yazar.

### Betik

```json
{
  "ad": "Çiz ve göster",
  "komutlar": [
    { "cmd": "core.line", "args": { "noktalar": [[485300000,4310200000],[485360000,4310245000]] } },
    { "cmd": "core.zoom", "args": { "mod": "KAPSAM" } }
  ]
}
```

`--betik` seçeneğiyle açtığınız betiklerde bunu yazmanıza gerek yoktur; program betik
bittikten sonra kendiliğinden kapsama yakınlaşır.

Her çağrı ne olduğunu yapılandırılmış olarak da söyler:

```json
{ "mod": "ÖNCEKİ", "degisti": true, "geri": 4, "ileri": 1 }
```

`degisti` görünümün yerinden oynayıp oynamadığını, `geri` ve `ileri` görünüm geçmişinde
kaç adım kaldığını söyler. Bir şeyi çerçeveleyen kiplerde (`PENCERE`, `SEÇİM`, `KATMAN`)
cevapta çerçevelenen kutu da vardır, milimetre olarak `[sağa_en_az, yukarı_en_az,
sağa_en_çok, yukarı_en_çok]`:

```json
{ "mod": "KATMAN", "degisti": true, "geri": 3, "ileri": 0,
  "kutu": [485300000, 4310200000, 485340000, 4310230000] }
```

Yani bir katmana yakınlaşan betik o katmanın kapsamını da aynı cevaptan öğrenir.

## Geri alma

`YAKINLAŞ` geri alınmaz. Görünüm ayarı çizimin verisi değildir, bu yüzden geri alma
yığınına hiç girmez. `GERİAL` bir önceki **çizim** işlemine gider, bir önceki görünüme
değil.

Bir önceki görünüme dönmek için `YAKINLAŞ ÖNCEKİ` (**Alt+C**) kullanın.

## Betikten kullanım

`YAKINLAŞ` betiklenebilir ve AI erişimlidir.

Görünüm istemcisi bağlı olmadan çalıştırılırsa — örneğin başsız bir toplu işlemde —
komut hata vermez, transkripte açıklama yazar:

```text
Görünüm istemcisi bağlı değil (başsız çalışma).
```

Bu sayede aynı betik hem arayüzde hem başsız çalışabilir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Beklenen mod: KAPSAM \| ÇARPAN \| SIFIRLA \| ÖNCEKİ \| SONRAKİ \| PENCERE \| MERKEZ \| SEÇİM \| KATMAN. Girilen: 'OLMAYAN'` | Geçersiz kip adı | Dokuz kipten birini yazın |
| `PENCERE iki köşe ister: YAKINLAŞ PENCERE pencere=<köşe> <köşe>. Verilen: 1 köşe.` | `PENCERE` kipinde köşe eksik. Komut köşeleri sormaz, çünkü şeffaf bir komut soru sorsaydı araya girdiği komutun sorusunu alırdı | İki köşeyi yazın ya da Alt+Z ile tuvalde gösterin |
| `Pencerenin iki köşesi aynı nokta; bir pencere tanımlamıyor.` | İki köşe aynı | Karşı köşeyi verin |
| `MERKEZ bir nokta ister: YAKINLAŞ MERKEZ merkez=<nokta> [olcek=<1:N>].` | `MERKEZ` kipinde nokta yok | `merkez=` ile noktayı verin |
| `SEÇİM için seçili nesne yok: önce nesneleri seçin ya da nesneler= ile verin.` | Seçim boş ve `nesneler=` verilmedi | Nesneleri seçin ya da kimliklerini verin |
| `KATMAN bir katman adı ister: YAKINLAŞ KATMAN katman=<ad>.` | `KATMAN` kipinde ad yok | `katman=` ile adı verin |
| `'PARSEL' adlı katman yok.` | Çizimde bu adda katman yok | `KATMANLAR` ile adları görün |
| `'PARSEL' katmanında nesne yok; gösterilecek bir şey yok.` | Katman boş | Görünüm değişmez; başka bir katman seçin |
| `Geri dönülecek görünüm yok: görünüm geçmişi boş.` | `ÖNCEKİ` ile gidilecek daha eski bir görünüm kalmadı | Hata değildir; görünüm yerinde kalır |
| `İleri gidilecek görünüm yok: ÖNCEKİ ile geri gidilmedi ya da o zamandan beri görünüm değişti.` | `SONRAKİ` için geri gidilmiş bir adım yok | Hata değildir; önce `ÖNCEKİ` ile geri gidin |
| `Görünüm istemcisi bağlı değil (başsız çalışma).` | Arayüz olmadan çalışılıyor | Hata değildir; beklenen davranıştır |
| `'core.zoom': bilinmeyen parametre 'oran'. Tanımlı parametreler: mod, carpan, pencere, merkez, olcek, nesneler, katman` | Parametre adı yanlış | `carpan` yazın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
