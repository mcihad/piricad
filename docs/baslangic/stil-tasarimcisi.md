# Stil Tasarımcısı

Bir katmanın nasıl çizileceğini fare ile tasarlamak için. Yazdığı şey `STİL`
komutlarıdır: tasarımcının belgeye giden özel bir yolu yoktur, dolayısıyla burada
kurduğunuz her sembolü bir betik de yazabilir.

## Nasıl açılır

- Katmanlar panelinde katmana **sağ tıklayın → Katman Özellikleri…**
- Araç çubuğunda *pencereler* grubundaki **palet** düğmesi
- **Katman ▸ Stil Tasarımcısı**

Pencere seçili katmanın şu an ne çizdiğiyle açılır: nesneleri bir gösterim
taşıyorsa o, taşımıyorsa katmanın kendi görünümü.

## Pencerede ne nerede

```text
┌ 🎨 Katman Özellikleri — Kadastro Parselleri ─────────────── ? ✕ ┐
│ Bilgi        │ SİMGELEYİCİ                    SEMBOL BOYUT BİRİMİ│
│ Kaynak       │ [Tek Sembol]           [Milimetre|Harita|Piksel] │
│ ▸Simgeleyici │──────────────────────────┬───────────────────────│
│ Etiketler    │ [Alan] [Çizgi] [Nokta]   │ ┌──────┐ SEMBOL KATMANLARI│
│ 3B Görünüm   │ ┌ hazır gösterimler ───┐ │ │önizl.│ ▾ Sembol       │
│ Şeffaflık    │ │ arama                │ │ │      │   ✓ Nokta deseni│
│ Ölçek        │ │ ▸ EK-1a  ▸ EK-1b     │ │ └──────┘   ✓ Dolgu      │
│ Öznitelik F. │ │ ┌──┐ ┌──┐ ┌──┐ ┌──┐  │ │  +  ⧉  −         ▲  ▼ │
│ Geçerlilik   │ │ └──┘ └──┘ └──┘ └──┘  │ │ KATMAN                 │
│ Eylemler     │ │                      │ │ Katman tipi  [Dolgu   ]│
│ Bağlantılar  │ │                      │ │ DOLGU                  │
│ Sürüm        │ │ 335 gösterim         │ │ Dolgu rengi  [#228B22 ]│
│              │ │ [ Seçileni kullan ]  │ │ KENAR                  │
│              │ └──────────────────────┘ │ Çizgi rengi  [#000000 ]│
│              │                          │ Kalınlık     [0 µm    ]│
├──────────────┴──────────────────────────┴───────────────────────┤
│ Stil ▾                              Yardım    İptal Uygula [Tamam]│
└───────────────────────────────────────────────────────────────────┘
```

Soldaki liste on iki bölüm taşır. Bugün **Bilgi** ve **Simgeleyici** doludur;
kalan onu hangi fazda geleceğini kendi sayfasında yazar. Gizlenmiş bir bölüm,
kullanıcının varlığından haberdar olamayacağı bir yetenektir; adı yazılmış bir
bölüm ise tarihi belli bir sözdür.

### Simgeleyici satırı

| Alan | Ne yapar |
|---|---|
| **SİMGELEYİCİ** | Katmanın nasıl çizileceği. Bugün **Tek Sembol**; kategorize ve aralıklı Faz 2 |
| **DEĞER** | Kategorize simgeleyicinin hangi sütuna bakacağı — Faz 2'de etkinleşir |
| **SEMBOL BOYUT BİRİMİ** | Sembolün ölçülerinin birimi: **Milimetre** · **Harita birimi** · **Piksel** |

### Sembol boyut birimi — en çok kullanacağınız denetim

Üç düğme, üç farklı davranış:

- **Milimetre** — pafta ölçüsü. MPYY bir sınırın kalınlığını paftada milimetre
  verir ve o kalınlık 1/1000'de de 1/5000'de de aynıdır. Ekranda
  **yakınlaştırdığınızda sembol büyümez.**
- **Harita birimi** — zemin ölçüsü. Orman deseninin sıklığı alana aittir; ölçekle
  küçülmesine izin vermek okunur bir dokuyu gri bir lekeye çevirir. **Çizimle
  birlikte büyür.**
- **Piksel** — ham ekran pikseli. Ne paftaya ne zemine bağlıdır; ekran
  yardımcıları dışında ender kullanılır.

Seçim sembolün **bütün** katmanlarını birden değiştirir. Katmanlar farklı
birimler kullanıyorsa hiçbiri işaretli görünmez ve alttaki not bunu söyler.

### Geometri sekmeleri

İlk karar bu: sembol hangi geometri için. Sekme iki şeyi birden belirler —
önizlemenin hangi şekil üzerinde çizileceğini ve soldaki rafın hangi çekmecesinin
açık olduğunu. `Çizgi` sekmesindeyken raf size alan gösterimi vermez.

Önizleme şekli de bilerek seçilmiştir: alan için dikdörtgen, çizgi için **zikzak**
(düz çizgi bir desenin köşede ne yaptığını gizler), nokta için tek nokta. Kare
önizlemenin altındaki tek kelime — *kapalı alan*, *kırıklı çizgi*, *tek nokta* —
resmin hangi geometri üzerinde çizildiğini söyler; sekmeyi değiştirince o da değişir.

### Hazır gösterimler

Soldaki raf, mevzuatın yayımladığı gösterim setidir ve ağacı da mevzuatın
kendisinindir: EK-1a ortak, EK-1b MSP, EK-1c ÇDP, EK-1ç NİP, EK-1d UİP, her ekin
altında kendi bölüm yolu.

- Ağaçtan bir bölüm seçin, ya da
- Arama kutusuna yazın — arama **ağacı dinlemez**, bir kelimeyi nerede olursa
  bulur.

Küçük resimler taramanın, çizgi tipinin ve simgenin **gerçek görselleridir**;
renk değil. `Seçileni al` ya da çift tıklama, o gösterimi yığına **koyar** —
üstüne eklemez, çünkü yayımlanmış bir gösterimi seçmek "bu böyle görünmeli"
demektir.

Raf çok kalabalıksa altındaki not kaç tanesinin gösterildiğini yazar. Sessizce
kesilmez.

### Sembol katmanları

Önizlemenin hemen yanındaki **SEMBOL KATMANLARI** listesi. **Üstten alta** okunur:
ilk satır en son çizilen, yani ekranda en üstte görünen katmandır. `▲` ve `▼`
satırı gördüğünüz yöne taşır. Listenin en üstündeki **Sembol** satırı bir katman
değil, sembolün kendisidir: onu seçince bütün katmanlara birden uygulanan
özellikler (birim, renk, kalınlık, saydamlık) gelir.

Her satırın başındaki kutu o katmanı **kapatır**. Kapalı katman silinmez —
sembolde durur, dosyaya yazılır, parmak izine girer — sadece çizilmez. Bir
katmanın ne kattığını görmek için kapatıp açmak en hızlı yoldur.

`⧉` seçili katmanı kopyalar; iki farklı kalınlıkta aynı çizgi (yol kaplaması)
böyle kurulur.

### Katman özellikleri

Listenin altında, seçili katmanın özellikleri **dört başlık** altında sıralanır;
her satırda etiket solda, değer sağdadır:

| Başlık | İçinde ne var |
|---|---|
| **KATMAN** | Katman tipi; tipe göre yazı, şekil, yerleşim |
| **DOLGU** | Dolgu rengi |
| **KENAR** | Çizgi rengi, kalınlık, uç biçimi, birleşim |
| **GEOMETRİ** | Boyut, aralık, ikinci eksen, kaydırma, açı, faz |
| **GÖRÜNÜRLÜK** | Saydamlık, renk kilidi |

Yalnız **seçili tipin okuduğu** satırlar gösterilir, ve içinde satırı olmayan bir
başlık da gösterilmez. Bir `dolgu` katmanının işaretçi yerleşimi yoktur, o yüzden
o satır orada değildir — soluk değil, yok. Görmediğiniz bir alan, çizicinin yok
sayacağı bir alan değildir.

Birim, etikette değil değerin yanındadır: kalınlık `µm`, açı `°` sonekiyle yazılır.
Her ölçünün **kendi birim kutusu** vardır: boyut kâğıtta, aralık zeminde
olabilir. İkisi aynı sembolde farklı birimlerde durabilir ve bu normaldir.

Önizlemeler tuvalin **kendi arka ucundan** geçer. Yani gördüğünüz küçük resim,
çizimde göreceğiniz şeyin aynısıdır — ayrı bir önizleme çizicisi olsaydı ikisi
er geç ayrışırdı.

## Katman özellikleri

Her alan `STİL` komutunun bir parametresidir; hangisi olduğu
[STİL sayfasında](../komutlar/style.md) tablo hâlinde yazılı.

| Başlık · Alan | `STİL` parametresi |
|---|---|
| KATMAN · Katman tipi | `tip` |
| KATMAN · Şekil | `sekil` |
| KATMAN · Yerleşim | `yerlesim` |
| DOLGU · Dolgu rengi | `dolgu` |
| KENAR · Çizgi rengi | `renk` |
| KENAR · Kalınlık | `kalinlik` |
| GEOMETRİ · Boyut + birim | `boyut`, `birim` |
| GEOMETRİ · Aralık | `aralik` |
| GEOMETRİ · İkinci eksen | `aralik_y` |
| GEOMETRİ · Kaydırma | `kaydirma` |
| GEOMETRİ · Açı | `aci` |
| GÖRÜNÜRLÜK · Saydamlık | `saydamlik` |

Tip kutusunda parantez içinde yazan (`gorsel-dolgu` gibi) makine adıdır ve
komut satırına yazacağınız şeydir.

### Sembol parametreleri

Bir sembol katmanı, çizeceği değeri nesnenin **öznitelik sütunundan** alabilir:
dairenin içine `taks` sütununu yazdırmak, çizgi kalınlığını `kat` sütununa
sürmek gibi. Bunu bugün [`STİL alan=`](../komutlar/style.md) ile yazarsınız.

Bu pencerede **henüz bir satırı yoktur**. Bir süre vardı ve çalışmıyordu: girilen
parametre önizlemeye yansıyor, **Uygula** ise onu belgeye hiç göndermiyordu. Bunun
yerine sütun · özellik · tür üçlüsünü seçtiren gerçek bir tablo tasarlanacak ve
buraya, katmanın öznitelik şemasının yanına gelecek. Tarihi **Faz 2**.

Sembolünde parametre olan bir katmanı bu pencerede açıp **Uygula** demek
parametreleri **silmez**; pencere kendisinde satırı olmayan bir şeye dokunmaz.

## Stil ▾ menüsü

Alt çubuğun solundaki **Stil ▾** düğmesi üç işi taşır:

| Kalem | Ne yapar |
|---|---|
| **Katmanın çizdiğine dön** | Bu penceredeki değişiklikleri atar; katmana dokunmaz |
| **Stili temizle** | `STİL katman=… sifirla=evet` gönderir: katmanın stili silinir, nesneler katman görünümüne döner |
| **Sembolü kütüphaneye kaydet…** | Sembolü kendi gösterim paketiniz olarak diske yazar |

**Stili temizle** eskiden katmanın sağ tık menüsündeydi. Bir stili silmek, bir stili
kurmakla aynı pencerede durur; katmanın menüsü ise katmanın kendisiyle ilgili
kalemlere ayrıldı.

## Uygula

**Uygula** yığındaki her **açık** sembol katmanı için bir `STİL` satırı gönderir:
ilki sembolü kurar, kalanlar `ekle=evet` ile üstüne biner. Kapalı katmanlar
gönderilmez. Komut günlüğünde satırların kendisini görürsünüz.

Tek bir geri alma adımıdır: `GERİAL` tasarımı bütünüyle geri alır.

## Kütüphaneye kaydet

**Kütüphaneye kaydet…** sembolü uygulamanın kendi ayar dizinine bir **gösterim
paketi** olarak yazar:

| Sistem | Yer |
|---|---|
| Linux | `~/.config/KentOSCad/stiller/` |
| Windows | `%APPDATA%\KentOSCad\stiller\` |
| macOS | `~/Library/Application Support/KentOSCad/stiller/` |

Proje dizinine değil: tasarladığınız sembol size aittir, çizimden çizime sizinle
gelir ve birinin paftasının yanında takip edilmeyen bir dosya olarak durmamalıdır.

Kaydedilen dosya normal bir gösterim paketidir, yani rafa geri alınabilir:

```
SEMBOL paket="<ayar dizini>/stiller/benim-stilim.json"
```

Kullanıcı stili ile yayımlanmış gösterim, bundan sonrası için aynı türden şeydir.

## Neden QGIS'in penceresi doğrudan kullanılmıyor

Lisans engel değil — QGIS GPL-2.0-or-later ve uyumlu. Engeller ölçülebilir:

- `libqgis_gui` **254 paylaşımlı kütüphane** ve 75 MB getiriyor.
- `QgsApplication::initQgis()` sağlayıcı kaydını ve SRS veritabanını yüklüyor;
  bu programın soğuk açılış bütçesi **2 saniye**.
- Gidiş-dönüş tam da bizim bilerek ayrıldığımız yerde kayıplı:
  `QgsRasterFillSymbolLayer` bir **dosya yolu** tutar, KentOSCad'in görsel dolgusu
  ise baytları ve künyesini belgenin içinde taşır — çizim e-postayla gittiğinde
  ayakta kalmasını sağlayan şey bu.

Alınabilecek olan alındı: **düzenin kendisi** — ve sadeleştirilerek. QGIS bunu iki
pencereye bölüyor (sembol seçici ve Stil Yöneticisi) ve seçili tipin okumadığı
alanları da soluk hâlde gösteriyor. Burada tek pencere var ve görünmeyen alan yok
sayılan alan değildir.
