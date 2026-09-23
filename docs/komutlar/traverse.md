# POLİGON — Poligon Hesabı

Bir poligon güzergâhını ölçü karnesinden koordinata çevirecek herkes için; bu
sayfayı bitirdiğinizde kapanma hatalarını hesaplamayı, dağıtmayı ve mevzuat
toleransına karşı denetlemeyi bileceksiniz.

## Ne yapar

Poligon, her ölçünün üzerine oturduğu iskelettir. Ekip bilinen bir noktada
bilinen bir bağlamayla başlar, her istasyonda bir **kırılma açısı** ve bir
**kenar** okuyarak güzergâhı yürür, ve bilinen bir bitiş noktasına bilinen bir
bağlamayla varır. `POLİGON` bunu üç ayrı soruya böler:

1. **Açı kapanması.** Okunan kırılma açılarının toplamı, başlangıç semtini
   bitiş semtine taşımak zorundadır. Kaçırdığı miktar `f_β`'dır ve istasyonlara
   **eşit** dağıtılır — kötü okunmuş bir açı bir istasyonda kötü okunmuştur ve
   sayıların içinde hangisi olduğunu söyleyen bir şey yoktur, dolayısıyla dürüst
   düzeltme her istasyonda aynıdır.
2. **Koordinatlar**, düzeltilmiş semtlerle hesaplanır.
3. **Kenar kapanması.** Son hesaplanan istasyon bilinen bitiş noktasına oturmak
   zorundadır. Kaçırdığı miktar `f_s`'dir ve `dagitim=` ile ya eşit ya **kenar
   orantılı** dağıtılır (Bowditch kuralı: uzun kenar hatanın daha büyük payını
   alır, çünkü uzun kenar ölçünün daha büyük payını taşır).

### Kapanmanın kabul edilebilirliği mevzuat kararıdır

Toleranslar `/data/catalogs/geodesy/poligon-toleranslari.json`'dan okunur;
programa gömülü değildir (CLAUDE.md 5.13). Toleransı aşan bir güzergâh
**reddedilir** ve ret cümlesi yönetmeliği adıyla söyler.

> **Bu tolerans paketi harita mühendisi onayını BEKLİYOR.** Değerler BÖHHBÜY'ün
> poligon bölümünden bir harita mühendisi tarafından teyit edilmemiştir
> (CLAUDE.md 6.11). Komut her retinde bunu yazar. Bir üretim işinin kabulü için
> kullanılmamalıdır; paketin `kapsam.onay` alanı `ONAYLI` olana kadar bu uyarı
> durur.

| Sınıf | Açı toleransı | Kenar toleransı |
|---|---|---|
| `ana` | f_β ≤ 100·√n cc | f_s/[S] ≤ 1/15000 |
| `ara` | f_β ≤ 150·√n cc | f_s/[S] ≤ 1/10000 |
| `tamamlayici` | f_β ≤ 200·√n cc | f_s/[S] ≤ 1/5000 |

## Adlar

| Ad | Tür |
|---|---|
| `POLİGON` | Türkçe, birincil |
| `POLIGON` | ASCII katlanmış Türkçe |
| `TRAVERSE` | İngilizce karşılık |
| `PLG` | Kısaltma |
| `geodesy.traverse` | Komut kimliği |

`POLİGON` eskiden `ALAN` komutunun eşadıydı. Bu bir yanlış çeviriydi: Türkçe
haritacılıkta *poligon* bir güzergâhtır, `ALAN`'ın çizdiği şekil ise *çokgen*'dir.
`ALAN`, `AREA` ve `AL` adları değişmedi.

## Sözdizimi

```text
POLİGON baslangic=<nokta> baglama=<nokta> aci=<açı> kenar=<m> [aci=… kenar=… …]
        [bitis=<nokta>] [bitis_baglama=<nokta>]
        [sinif=ana|ara|tamamlayici] [dagitim=esit|kenar] [cizgi=evet|hayır]
```

`aci` ve `kenar` **sırayla eşleşir** ve sayıları eşit olmak zorundadır: her
istasyonda bir açı ve ondan sonraki bir kenar okunur.

`bitis` verilmezse güzergâh **açık** sayılır: koordinatlar hesaplanır, kapanma
hesaplanmaz. `bitis_baglama` da verilirse açı kapanması da hesaplanır.

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `baslangic` | nokta | 1 | Başlangıç istasyonu (bilinen) |
| `baglama` | nokta | 1 | Başlangıçtaki bağlama noktası (bilinen) |
| `aci` | sayı | 0..n | Her istasyondaki kırılma açısı, oturumun açı biriminde |
| `kenar` | sayı | 0..n | Her istasyondan sonraki kenar (m) |
| `bitis` | nokta | 0..1 | Bitiş istasyonu; verilirse kenar kapanması hesaplanır |
| `bitis_baglama` | nokta | 0..1 | Bitişteki bağlama; açı kapanması için gerekir |
| `sinif` | sözcük | 0..1 | `ana` (varsayılan), `ara`, `tamamlayici` |
| `dagitim` | sözcük | 0..1 | `esit` (varsayılan) ya da `kenar` |
| `ilk_no` | İlk istasyonun **nokta numarası**; verilmezse çizimdeki en büyük numaranın bir fazlası |
| `cizgi` | mantıksal | 0..1 | Güzergâhı çizgiyle bağlar; varsayılan **evet** |

## Örnekler

### Komut satırı

Ders kitabı poligonu: (0,0)'dan kuzeye bakan bağlamayla başlayıp dört 100
metrelik kenar ve her istasyonda 300 grad kırılma açısı — tam bir kare:

```text
POLİGON baslangic=0,0 baglama=0,100 aci=300 kenar=100 aci=300 kenar=100 aci=300 kenar=100 aci=300 kenar=100
```

```text
4 poligon noktası hesaplandı (Ana poligon, [S] = 400,000 m). UYARI: tolerans paketi harita mühendisi onayı bekliyor.
```

İstasyonlar (100,0), (100,−100), (0,−100) ve (0,0)'dır.

Aynı kare, ama bitiş noktası 20 milimetre kaçırıyor — kapanma kenar orantılı
dağıtılır ve son istasyon yayımlanmış noktaya **oturur**:

```text
POLİGON baslangic=0,0 baglama=0,100 aci=300 kenar=100 aci=300 kenar=100 aci=300 kenar=100 aci=300 kenar=100 bitis=0.02,0 dagitim=kenar
```

```text
4 poligon noktası hesaplandı (Ana poligon, [S] = 400,000 m, açı kapanma 0 cc, kenar kapanma 0,020 m, kenar orantılı dağıtıldı). …
```

Düzeltme pay pay büyür: 5, 10, 15 ve 20 milimetre.

Toleransı aşan bir kapanma **reddedilir**:

```text
POLİGON baslangic=0,0 baglama=0,100 aci=300 kenar=100 aci=300 kenar=100 aci=300 kenar=100 aci=300 kenar=100 bitis=5,0
```

```text
Kenar kapanma hatası toleransı aşıyor: 5,000 m ölçüldü, en çok 0,026 m olabilir
(Ana poligon, f_s/[S] ≤ 1/15000; [S] = 400,000 m). Kaynak: Büyük Ölçekli Harita ve
Harita Bilgileri Üretim Yönetmeliği (BÖHHBÜY), Üçüncü Kısım — Poligon Ölçüleri
(RG-15/7/2005-25876). UYARI: bu tolerans paketi harita mühendisi onayı BEKLİYOR
(CLAUDE.md 6.11); bir üretim işinin kabulü için kullanılmaz.
```

Ret hâlinde **hiçbir şey çizilmez**: işlemin tamamı geri sarılır (Article 1.6).

### Arayüz

**Çizim > Poligon Hesabı**, **Harita > Poligon Hesabı** ya da sol araç sütununda
**nokta ailesi**. Komut başlangıç istasyonunu ve bağlamayı sorar — bağlama
gösterilirken istasyondan imlece kılavuz çizgi durur — sonra **her istasyon için
sırayla kırılma açısını ve ondan sonraki kenarı sorar**; bu sırada istasyon ve
bağlaması tuvalde çizili kalır:

```text
İstasyon 1: kırılma açısı (Enter ya da sağ tık bitirir)
İstasyon 1: ondan sonraki kenar (m)
İstasyon 2: kırılma açısı (Enter ya da sağ tık bitirir)
…
```

Karneyi bitirdiğinizde **sağ tık** ya da `Esc` okumayı kapatır ve hesap çalışır.

`aci=` ve `kenar=` dizilerini baştan verirseniz hiç sorulmaz: bir betik hiçbir şey
sorulmadan çalışmalıdır ve kararı veren şey **argümanın boş olup olmadığıdır**,
istemcinin kim olduğu değil (CLAUDE.md 1.2).

> Bu komut bir süre yalnız argüman okuyordu — gerekçe "bir poligon ölçü
> karnesinden aktarılır, tıklanmaz" idi. Bu, sayıların NEREDEN geldiği için doğru,
> nasıl İÇERİ GİRDİĞİ için yanlıştı: araç kolonundan `POLİGON`'a basan kullanıcı
> iki bilinen noktayı veriyor ve sonra "aci= ve kenar= gerekir" cevabını alıyordu.
> Fareyle ulaşılabilen ama fareyle bitirilemeyen bir komut, CLAUDE.md 5.15'in
> yasakladığı şeydir.

### Betik

```json
{ "cmd": "geodesy.traverse", "args": {
    "baslangic": [0, 0], "baglama": [0, 100000],
    "aci":   [312.4567, 287.1234],
    "kenar": [42.315, 56.720],
    "sinif": "ana", "dagitim": "kenar" } }
```

Kesirli kenarlar günlükte **kayıpsız** durur ve replay aynı içerik hash'ini
verir; `tests/unit/test_geodesy.cpp` bunu sınar.

### Rapor

`Context::report` yapılandırılmış bir poligon cetveli döndürür: sınıf, kaynak,
onay durumu, istasyon sayısı, `[S]`, dağıtım, kapanma hataları ve her istasyon
için sıra no, semt, kenar, sağa ve yukarı. Bir ajan ya da betik bu cetveli
doğrudan okur.

## İstasyonlar numaralanır

Hesaplanan her istasyon bir **nokta numarası** taşır (`nokta_no` özniteliği) ve
bu numaranın bütün anlamı şudur: bir poligon istasyonu, ondan sonraki her
detayın ölçüldüğü **yer**dir.

```text
POLİGON baslangic=0,0 baglama=0,100 aci=300 kenar=100 aci=300 kenar=100
ÇİZGİ n(1) n(2)          → istasyonları adıyla kullanın
APLİKASYON istasyon=n(2) …
```

Numaralı olmayan bir istasyon, bir mühendisin **atıfta bulunamadığı** bir
noktadır. Sütun `nokta_no`'dur — [`NOKTALAR`](points.md)'ın okuyup yazdığı ve
`n(…)` nokta fonksiyonunun çözdüğü sütunun aynısı; aynı şeyi anlatan ikinci bir
sütun açılmaz.

**Varsayılan, çizimdeki en büyük numaranın bir fazlasıdır.** Bir poligon bir
işin tek ayağıdır ve önceki ayaklar numara kullanmış olur: 1'den yeniden başlayan
ikinci bir güzergâh iki istasyona tek ad verir ve `n(2)` o zaman aramanın önce
ulaştığını gösterir. `ilk_no=` ekibin kendi numaralamasını dayatır.

`R12` ya da `NIR-3` gibi **ad** taşıyan noktalar sayılmaz: en büyük **sayaç**
aranır, bir ad değil.

Çözülmüş `ilk_no` **günlüğe yazılır**, ve varsayılanı güvenli kılan şey budur:
başka numaralı noktalar taşıyan bir belgeye oynatılan bir günlük, bu istasyonları
aynı şekilde numaralamak zorundadır.

## Geri alma

Bir çağrı tek bir işlemdir: hesaplanan bütün noktalar ve güzergâh çizgisi tek
bir **Ctrl+Z** ile birlikte gider.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır. `aci` ve `kenar` birer **dizi**;
`sinif` ve `dagitim` sözcük listeleridir ve bus onları gövde çalışmadan önce
doğrular.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Poligon için kırılma açıları (aci=) ve kenarlar (kenar=) gerekir` | Ne dizi verildi ne de okuma sorulabildi (betik ya da tek okuma bile girilmeden bitirildi) | Ölçü karnesindeki sırayla verin |
| `Kırılma açısı ve kenar sayısı eşit olmalı: …` | Bir açı ya da kenar eksik | Karneyi sayın: her istasyonda bir açı, bir kenar |
| `Açı kapanma hatası toleransı aşıyor: … cc ölçüldü, en çok … cc olabilir` | `f_β > c·√n` | Açıları kontrol edin ya da sınıfı gözden geçirin |
| `Kenar kapanma hatası toleransı aşıyor: … m ölçüldü, en çok … m olabilir` | `f_s/[S] > 1/o` | Kenarları ve bitiş noktasını kontrol edin |
| `Poligon sınıfı bulunamadı: '…'. Katalogdaki sınıflar: …` | `sinif=` katalogda yok | Listelenen sınıflardan birini verin |
| `Poligon tolerans kataloğu okunamadı: …` | `/data` kurulu değil | `KENTOS_DATA` ile veri dizinini gösterin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [ALIM](survey_polar.md) — bir istasyonun detay alımı
- [APLİKASYON](stakeout.md) — koordinattan açı ve kenar
- [DİKAYAK](perp_offset.md) — taban çizgisine göre alım
- [OTURT](fit.md) — yerel ölçüyü bilinen noktalara oturtmak
