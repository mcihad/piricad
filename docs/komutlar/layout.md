# ÇIKTIYERLEŞİMİ — Çıktı Yerleşimi

Çizimini kâğıda dökecek herkes için; bu sayfayı bitirdiğinizde bir çıktı yerleşimi
açmayı, kâğıdını ve yönünü seçmeyi, adını değiştirmeyi ve silmeyi bileceksiniz.

## Ne yapar

Bir **çıktı yerleşimi**, çiziminizin basılacağı sayfa düzenidir: kâğıt boyu, yönü,
kenar boşluğu ve üzerine yerleştirilmiş öğeler — harita çerçevesi, başlık, ölçek
çubuğu, kuzey oku, lejant. `ÇIKTIYERLEŞİMİ` bu sayfaların kendisini yönetir;
üzerindeki öğeleri [`ÇIKTIÖĞE`](layout_item.md) yönetir.

> Bu programda **pafta**, kadastronun böldüğü harita sayfasıdır — `29-30-K` gibi bir
> adı olan, paylaşılan bir paftalama sisteminin karesi. Bastığınız sayfaya ise
> **çıktı yerleşimi** denir. İkisi karışmasın diye ayrı adlandırılmıştır: bir çıktı
> yerleşimi bir paftayı gösterebilir, ama bir pafta değildir.

**Çıktı yerleşimi çizimle birlikte kaydedilir.** Yazdırma profillerinden farkı budur:
bir profil bu bilgisayarın ayarıdır, bir yerleşim ise teslim edilen işin parçasıdır.
Dosyayı bir meslektaşınıza gönderdiğinizde yerleşim de gider, çizimin parmak izine
(`content_hash`) girer ve her düzenlemesi **tek `Ctrl+Z` ile geri alınır**.

Yeni bir yerleşim boş bir sayfa değildir: içinde bir harita çerçevesi, bir başlık, bir
ölçek çubuğu ve bir kuzey oku ile gelir. Hepsi taşınabilir, değiştirilebilir ve
silinebilir; amaç ilk anda basılabilir bir şey görmenizdir.

## Adlar

| Ad | Açıklama |
|---|---|
| `ÇIKTIYERLEŞİMİ` | Türkçe birincil ad |
| `CIKTIYERLESIMI` | ASCII karşılığı |
| `LAYOUT` | İngilizce karşılığı |
| `ÇYR`, `CYR` | Kısaltma |

## Sözdizimi

```
ÇIKTIYERLEŞİMİ islem=listele
ÇIKTIYERLEŞİMİ islem=ekle ad=<ad> [kagit=A4] [yon=dikey] [kenar=10] [dpi=300]
ÇIKTIYERLEŞİMİ islem=ekle ad=<ad> kagit=ozel genislik=<mm> yukseklik=<mm>
ÇIKTIYERLEŞİMİ islem=sil ad=<ad>
ÇIKTIYERLEŞİMİ islem=ad ad=<ad> yeni_ad=<ad>
ÇIKTIYERLEŞİMİ islem=sayfa ad=<ad> [sayfa=<n>] kagit=A3 yon=yatay [kenar=<mm>]
ÇIKTIYERLEŞİMİ islem=sayfaekle ad=<ad> [kagit=A3] [yon=yatay] [sayfa=<n>]
ÇIKTIYERLEŞİMİ islem=sayfasil ad=<ad> [sayfa=<n>]
ÇIKTIYERLEŞİMİ islem=sayfacogalt ad=<ad> [sayfa=<n>]
ÇIKTIYERLEŞİMİ islem=sayfatasi ad=<ad> sayfa=<n> yeni_sira=<m>
ÇIKTIYERLEŞİMİ islem=denetle ad=<ad>
ÇIKTIYERLEŞİMİ islem=atlas ad=<ad> katman=<katman> [sirala=<sütun>] [kenar_payi=10] [tek_dosya=evet]
ÇIKTIYERLEŞİMİ islem=rapor ad=<ad> [grup=<sütun>]
```

## Parametreler

| Parametre | Zorunlu | Anlamı |
|---|---|---|
| `islem` | evet | `listele`, `ekle`, `sil`, `ad`, `sayfa`, `sayfaekle`, `sayfasil`, `sayfacogalt`, `sayfatasi`, `denetle`, `atlas` |
| `ad` | `listele` dışında | Yerleşimin adı. Türkçe katlamayla tekildir: `Ada 1284` ile `ada 1284` aynı yerleşimdir |
| `yeni_ad` | `islem=ad` için | Yerleşimin yeni adı |
| `kagit` | hayır | `A5`, `A4`, `A3`, `A2`, `A1`, `A0` ya da `ozel` (varsayılan `A4`) |
| `genislik`, `yukseklik` | `ozel` için | Sayfa boyu, milimetre |
| `yon` | hayır | `dikey` ya da `yatay` (varsayılan `dikey`) |
| `kenar` | hayır | Kenar boşluğu, milimetre (varsayılan 10) |
| `dpi` | hayır | Çıktı çözünürlüğü (varsayılan 300) |
| `sayfa` | sayfa işlemlerinde | Hangi sayfa; **1'den başlar**. `islem=sayfa`'da verilmezse bütün sayfalar değişir |
| `yeni_sira` | `sayfatasi` için | Sayfanın gideceği sıra |
| `katman` | `atlas` için | Hangi katmanın nesneleri için sayfa basılacak; `yok` atlası kapatır |
| `sirala` | hayır | Sayfaların sıralanacağı ve adlandırılacağı öznitelik sütunu; verilmezse nesne anahtarı |
| `kenar_payi` | hayır | Nesnenin çevresinde bırakılacak pay, yüzde (varsayılan 10) |
| `tek_dosya` | hayır | Tek çok sayfalı belge mi, nesne başına bir dosya mı (varsayılan evet) |

`islem=ekle` var olan bir adı **değiştirir**, yenisini eklemez — `YAZDIRMAPROFİLİ`
ve `YAPAYZEKAMODELİ` ile aynı davranış.

## Örnekler

### Komut satırı

Bir ada için A3 yatay yerleşim:

```
ÇIKTIYERLEŞİMİ islem=ekle ad="Ada 1284" kagit=A3 yon=yatay
```

Program şunu yazar:

```
Çıktı yerleşimi: Ada 1284 — A3 420×297 mm, yatay, 4 öğe
```

Çizimdeki yerleşimleri listelemek:

```
ÇIKTIYERLEŞİMİ islem=listele
```

Kâğıdı büyütmek — öğeler **yerinde kalır**, yeniden ölçeklenmez; üstten 20 mm'de
duran bir başlık A3'te de üstten 20 mm'dedir:

```
ÇIKTIYERLEŞİMİ islem=sayfa ad="Ada 1284" kagit=A2 yon=yatay kenar=15
```

### Çok sayfalı yerleşim

Bir yerleşimin sayfaları **aynı boyda olmak zorunda değildir**. A4 dikey bir
kapak ve A3 yatay bir harita sayfası aynı belgede durur ve PDF'e doğru ölçülerle
çıkar:

```
ÇIKTIYERLEŞİMİ islem=ekle ad=Karma kagit=A4 yon=dikey
ÇIKTIYERLEŞİMİ islem=sayfaekle ad=Karma kagit=A3 yon=yatay
YAZDIR yerlesim=Karma dosya=karma.pdf
```

Bir sayfayı **öğeleriyle birlikte** çoğaltmak, sırasını değiştirmek ve silmek:

```
ÇIKTIYERLEŞİMİ islem=sayfacogalt ad=Karma sayfa=1
ÇIKTIYERLEŞİMİ islem=sayfatasi ad=Karma sayfa=3 yeni_sira=1
ÇIKTIYERLEŞİMİ islem=sayfasil ad=Karma sayfa=2
```

**Silinen sayfa öğelerini de götürür** ve kaç öğe gittiğini söyler: geride kalan
kutular var olmayan bir sayfayı gösterirdi, ve onları taşıyacak dürüst bir sayfa
yok — kullanıcı bu sayfanın var olmamasını istedi. **Son sayfa silinemez.**

Tek bir sayfanın kâğıdını ayrı değiştirmek:

```
ÇIKTIYERLEŞİMİ islem=sayfa ad=Karma sayfa=2 kagit=A4
```

`sayfa=` verilmezse bütün sayfalar değişir — "kâğıdı değiştir" burada her zaman
bunu demiştir.

### Atlas — her parsel için bir sayfa

Bir kadastro bürosunun gerçekten istediği şey: yüz parsel, yüz sayfa, her biri
kendi parseline hedefli ve onun adıyla.

```
ÇIKTIYERLEŞİMİ islem=atlas ad=Askı katman=PARSEL sirala=parsel_no
YAZDIR yerlesim=Askı dosya=aski.pdf
```

Harita çerçevesi her nesne için yeniden hedeflenir; **çizim değişmez** — belgeyi
düzenleyen bir baskı, geri alma gerektiren bir baskı olurdu.

- **Sıra belirlidir.** Aynı çizim iki kez aynı sırayı verir; 47. sayfanın tekrar
  basımı, 47. sayfanın parseli olmak zorundadır.
- **Adlar benzersizdir.** `sirala=` verilmezse nesne **anahtarı** kullanılır
  (yuva numarası değil). Aynı ad iki kez çıkarsa ikincisine `-2` eklenir: farklı
  adalarda 21 numaralı iki parsel bu ülkede olağandır, ve birini öbürünün üstüne
  yazan bir koşum onu sessizce kaybeder.
- **Bildirilen ölçek korunur.** 1:1000 bir atlas, 1:1000 yüz sayfadır.
- **Hiçbir nesne bulunmazsa baskı olmaz**, hata verilir — sıfır dosya yazıp
  başarı bildirmek yerine.
- Boş geometrili nesneler atlanır: hedeflenemeyen bir nesne için boş bir sayfa
  basmak, boş bir sayfayı sonuç sanmaktır.

### Rapor: ada başına bölüm, parsel başına sayfa

Bir **atlas** düz bir döngüdür: aynı sayfa, her nesne için bir kez. Bir **rapor**
hiyerarşidir: ada 1284 kendi başlığını ve kendi toplamlarını alır, sonra her parseli
için bir sayfa gelir, sonra ada 1285 başlar. Döngü bunu **anlatamaz**, çünkü bölüm diye
bir kavramı yoktur.

`islem=rapor` bölümlemeyi **okur ve bildirir**; hiçbir şey basmaz ve hiçbir şeyi
değiştirmez. Yüz sayfa yazılmadan önce bölümlemenin doğru olup olmadığı görülsün diye:

```
ÇIKTIYERLEŞİMİ islem=atlas ad=Rapor katman=PARSEL
ÇIKTIYERLEŞİMİ islem=rapor ad=Rapor grup=ada_no
```

```text
Rapor: 2 bölüm, 3 sayfa ('ada_no' ile bölümlendi):
  1284 — 2 nesne
  1285 — 1 nesne
```

Yapılandırılmış sonuçta her bölüm `deger`, `adet` ve `kutu_alani_mm2` taşır.

> **`kutu_alani_mm2` ölçülen alan DEĞİLDİR.** Nesnelerin sınır kutularının toplamıdır ve
> adı bunu söyler. Ölçülen alan `ÖLÇÜM_ALAN`'ın cevabıdır; bir raporun üstündeki toplamın
> hukuki alan sanılması, imzalanan bir belgede yapılabilecek en pahalı karışıklıktır.

`grup=` verilmezse **tek bölüm** olur — ki bu tam olarak bir atlastır, ve cevap vermek
reddetmekten daha yararlıdır.

Grup değeri **olmayan** bir nesne kendi bölümünü oluşturur, atılmaz: ada numarası henüz
girilmemiş bir parsel kurulmakta olan bir çizimde olağandır, ve onu sessizce atlayan bir
rapor, eksik veriyle imzalanan bir rapordur.

### Basmadan önce denetlemek

```
ÇIKTIYERLEŞİMİ islem=denetle ad="Ada 1284"
```

**Bunların hiçbiri basmayı engellemez** — dosya çıkar ve bitmiş görünür. Tam da
bu yüzden söylenmeleri gerekiyor:

- Hedeflenmemiş harita çerçevesi (boş kutu basar)
- Sayfanın dışına taşan öğe (kesik basar)
- Kopmuş harita bağı (ölçek çubuğu hiçbir şey söyleyemez)
- Katmanı verilmemiş tablo, dosyası verilmemiş resim
- Eni ya da boyu sıfır olan kutu
- Hiç harita çerçevesi olmayan yerleşim
- Tamamen başka bir öğenin altında kalan ve **hiç görünmeyecek** olan öğe

#### Ne neyin üstünde

Denetim, sorunların ardından **üst üste binen öğeleri** de sayar — ama bunları
sorun ilan etmez:

```
Üst üste binen öğeler (sorun olmayabilir):
  · 'baslik' 'harita' üzerinde, sayfa 1, %4 (altındakini gizliyor)
  · 'lejant' 'harita' üzerinde, sayfa 1, %9 (saydam)
```

Çünkü çoğu çakışma **tasarımın kendisidir**: başlık, ölçek çubuğu ve kuzey oku
harita çerçevesinin üstünde durur. Bunları kusur saymak, doğru kurulmuş her
sayfada boşuna alarm vermek olurdu.

Ama "lejant haritanın üstüne binmiş" insanın söylediği bir cümledir, ve buna
cevap verebilmek için programın **neyin neyi kapattığını** söyleyebilmesi gerekir.
Bu yüzden ikisi ayrı: sorun listesi yalnız hiç istenmeyen hâli — bir öğenin
tamamen görünmez kalmasını — taşır, geri kalanı sorulduğunda cevaplanır.

Yapılandırılmış sonuçta `sorunlar` ve `ust_uste_binen` alanları bulunur;
ikincisinin her satırı `ustte`, `altta`, `sayfa`, `kapanan_yuzde` ve `gizliyor`
taşır. Liste **en çok kapanan** üstte sıralıdır.

`YAZDIR yerlesim=` aynı denetimi kendiliğinden yapar ve bulduklarını sonucunun
**uyarıları** olarak döndürür; baskıyı durdurmaz.

Kuruma özel bir kâğıt:

```
ÇIKTIYERLEŞİMİ islem=ekle ad="Askı Sayfası" kagit=ozel genislik=700 yukseklik=500
```

### Arayüz

#### Menüden

**`Dosya ▸ Çıktı Yerleşimleri`** yerleşimlerin ana kapısıdır:

| Giriş | Ne yapar |
|---|---|
| **Yeni Çıktı Yerleşimi…** | Ad sorar, A3 yatay bir yerleşim kurar ve tasarımcıyı açar |
| **Çıktı Yerleşimi Yöneticisi…** (`Ctrl+Shift+P`) | Çizimdeki yerleşimleri listeler: aç, yeniden adlandır, **çoğalt**, sil |
| *yerleşim adı* ▸ **Tasarımcıyı Aç** | Sayfayı düzenlemeye açar |
| *yerleşim adı* ▸ **Tuvalden Alan Seç…** | Haritanın bakacağı alanı tuvalden çerçeveletir, sonra tasarımcıyı açar |
| *yerleşim adı* ▸ **PDF'e Aktar…** | `YAZDIR yerlesim=` çalıştırır |

Menü her açılışta çizimden yeniden kurulur: komut satırından eklediğiniz bir yerleşim
orada olur.

**Çıktı Yerleşimi Yöneticisi**'ndeki **Çoğalt**, yerleşimi bütün öğeleriyle kopyalar.
Bunu tek bir "kopyala" fiiliyle değil, bir elin yazacağı satırlarla yapar — bir
`ÇIKTIYERLEŞİMİ islem=ekle` ve her öğe için bir `ÇIKTIÖĞE` — hepsi tek toplu iş, yani
tek `Ctrl+Z`. Günlükte gerçekte ne kurulduğu görünür, fiilin arkasına saklanmaz.

#### Araç çubuğundan

Araç çubuğundaki **yazdırma düğmesinin yanındaki ok** hem yazdırma profillerini hem
çizimdeki **çıktı yerleşimlerini** listeler. Listeden bir yerleşim seçtiğinizde:

1. Tuval, o yerleşimin **harita çerçevesinin en-boy oranında** bir seçme çerçevesi açar
   — çerçevelediğiniz alan haritanın göstereceği alandır, kâğıdın tamamı değil.
2. Alanı sürükleyip bıraktığınızda **tasarımcı açılır** ve harita çerçevesi o alana
   bakıyor olur.

Aynı listenin altında da **Yeni çıktı yerleşimi…** vardır. Her adım `ÇIKTIYERLEŞİMİ` ve
`ÇIKTIÖĞE` satırları olarak geçer: komut günlüğünde görünür, tek `Ctrl+Z` ile geri
alınır.

### Betik

```json
{ "cmd": "core.layout",
  "args": { "islem": "ekle", "ad": "Ada 1284", "kagit": "A3", "yon": "yatay" } }
```

## Geri alma

Her `ÇIKTIYERLEŞİMİ` çağrısı tek bir işlemdir ve tek `Ctrl+Z` ile geri alınır: silinen
yerleşim bütün öğeleriyle geri gelir, değiştirilen kâğıt eski boyuna döner. Yerleşim
listesi bütün hâlinde geri yüklenir, tek tek öğe olarak değil — bir yerleşimin adı
değiştiğinde ya da bir öğe silindiğinde dizinlerin kayması bunu zorunlu kılar.

## Betikten kullanım

Yerleşimler çizimin içinde durduğu için bir betik önce çizimi açar, sonra yerleşimi
kurar:

```json
[
  { "cmd": "core.open", "args": { "yol": "ada1284.pcad" } },
  { "cmd": "core.layout", "args": { "islem": "ekle", "ad": "Ada 1284",
                                    "kagit": "A3", "yon": "yatay" } },
  { "cmd": "core.layout_item", "args": { "islem": "ayarla", "ad": "harita",
                                         "olcek": 1000, "izgara": "cizgi" } },
  { "cmd": "core.save", "args": {} }
]
```

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Yerleşim adı gerekir: ad=<ad>` | `listele` dışında bir işlem adsız çağrıldı | `ad=` ekleyin |
| `Çıktı yerleşimi yok: 'X'.` | O adda bir yerleşim bulunamadı | `ÇIKTIYERLEŞİMİ islem=listele` ile adları görün |
| `Tanınmayan kâğıt: 'X'. Kâğıtlar: A5, A4, A3, A2, A1, A0, ozel.` | Kâğıt adı tabloda yok | Listedeki adlardan birini yazın ya da `ozel` kullanın |
| `ozel kâğıt için genislik ve yukseklik milimetre olarak verilmeli (sıfırdan büyük).` | `kagit=ozel` verildi ama boy verilmedi | `genislik=` ve `yukseklik=` ekleyin |
| `Kenar boşluğu sayfanın içinde kalmalı: 0 ile N mm arası.` | Kenar boşluğu sayfayı yutuyor | Daha küçük bir `kenar=` verin |
| `Yeni ad gerekir: yeni_ad=<ad>` | `islem=ad` çağrıldı ama yeni ad yok | `yeni_ad=` ekleyin |
| `'X' yerleşiminde iki öğe aynı adı taşıyor: 'Y'.` | Öğe adları tekil olmalı | Öğelerden birini yeniden adlandırın |
| `'X' yerleşiminde N sayfa var; M. sayfa yok.` | `sayfa=` aralık dışında | `islem=listele` ile sayfa sayısını görün |
| `'X' atlası 'Y' katmanında basılacak nesne bulamadı.` | Kapsama katmanı boş ya da nesneleri geometrisiz | Katmanı denetleyin |
| `Son sayfa silinemez; bir yerleşimin en az bir sayfası olur.` | Tek kalan sayfa silinmek istendi | Yerleşimin kendisini silin |
| `Sayfa taşımak için sayfa=<n> ve yeni_sira=<m> gerekir.` | `sayfatasi` eksik çağrıldı | İkisini de verin |

## İlgili

- [`ÇIKTIÖĞE`](layout_item.md) — yerleşimin üzerindeki öğeler
- [`ÇIKTIŞABLON`](layout_template.md) — kurumun standart yerleşimleri
- [`YAZDIR`](print.md) — çizimi doğrudan kâğıda dökmek
- [`YAZDIRMAPROFİLİ`](print_profile.md) — kâğıt profilleri
