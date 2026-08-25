# Stil Tasarımcısı

Bir katmanın nasıl çizileceğini fare ile tasarlamak için. Yazdığı şey `STİL`
komutlarıdır: tasarımcının belgeye giden özel bir yolu yoktur, dolayısıyla burada
kurduğunuz her sembolü bir betik de yazabilir.

## Nasıl açılır

- Katmanlar panelinde katmana **sağ tıklayın → Stili düzenle…**
- Araç çubuğunda *pencereler* grubundaki **palet** düğmesi
- **Katman ▸ Stil Tasarımcısı**

Pencere seçili katmanın şu an ne çizdiğiyle açılır: nesneleri bir gösterim
taşıyorsa o, taşımıyorsa katmanın kendi görünümü.

## Pencerede ne nerede

```text
┌ 🎨 Katman Özellikleri — Kadastro Parselleri ────────── ? ✕ ┐
│ Bilgi        │ SİMGELEYİCİ  DEĞER        SEMBOL BOYUT BİRİMİ│
│ Kaynak       │ [Tek Sembol] [—]      [Milimetre|Harita|Piksel]│
│ ▸Simgeleyici │─────────────────────────────────────────────│
│ Etiketler    │ [Alan] [Çizgi] [Nokta]                      │
│ 3B Görünüm   │ ┌ önizleme ─────────────────────────────┐   │
│ Şeffaflık    │ └───────────────────────────────────────┘   │
│ Ölçek        │ ┌ hazır gösterimler ┐ ┌ sembol katmanları ┐ │
│ Öznitelik F. │ │  raf / arama      │ │  ✓ Basit dolgu    │ │
│ Geçerlilik   │ │                   │ │  ✓ Çizgi dolgu    │ │
│ Eylemler     │ │                   │ ├───────────────────┤ │
│ Bağlantılar  │ │                   │ │ katman özellikleri│ │
│ Sürüm        │ └───────────────────┘ └───────────────────┘ │
├────────────────────────────────────────────────────────────┤
│ Stil ▾  Sembolü kütüphaneye kaydet   İptal  Uygula  [Tamam] │
└────────────────────────────────────────────────────────────┘
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
(düz çizgi bir desenin köşede ne yaptığını gizler), nokta için tek nokta.

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

Liste **üstten alta** okunur: ilk satır en son çizilen, yani ekranda en üstte
görünen katmandır. `▲` ve `▼` satırı gördüğünüz yöne taşır.

Her satırın başındaki kutu o katmanı **kapatır**. Kapalı katman silinmez —
sembolde durur, dosyaya yazılır, parmak izine girer — sadece çizilmez. Bir
katmanın ne kattığını görmek için kapatıp açmak en hızlı yoldur.

`⧉` seçili katmanı kopyalar; iki farklı kalınlıkta aynı çizgi (yol kaplaması)
böyle kurulur.

### Katman özellikleri

Sağ alt yalnız **seçili tipin okuduğu** alanları gösterir. Bir `dolgu` katmanının
işaretçi yerleşimi yoktur, o yüzden o satır orada değildir — soluk değil, yok.
Görmediğiniz bir alan, çizicinin yok sayacağı bir alan değildir.

Her ölçünün **kendi birim kutusu** vardır: boyut kâğıtta, aralık zeminde
olabilir. İkisi aynı sembolde farklı birimlerde durabilir ve bu normaldir.

Önizlemeler tuvalin **kendi arka ucundan** geçer. Yani gördüğünüz küçük resim,
çizimde göreceğiniz şeyin aynısıdır — ayrı bir önizleme çizicisi olsaydı ikisi
er geç ayrışırdı.

## Katman özellikleri

Her alan `STİL` komutunun bir parametresidir; hangisi olduğu
[STİL sayfasında](../komutlar/style.md) tablo hâlinde yazılı.

| Alan | `STİL` parametresi |
|---|---|
| Katman tipi | `tip` |
| Çizgi rengi | `renk` |
| Çizgi kalınlığı | `kalinlik` |
| Dolgu rengi | `dolgu` |
| Boyut + birim | `boyut`, `birim` |
| Aralık | `aralik` |
| İkinci eksen | `aralik_y` |
| Kaydırma | `kaydirma` |
| Açı | `aci` |
| Şekil | `sekil` |
| Yerleşim | `yerlesim` |
| Saydamlık | `saydamlik` |

Tip kutusunda parantez içinde yazan (`gorsel-dolgu` gibi) makine adıdır ve
komut satırına yazacağınız şeydir.

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
| Linux | `~/.config/PiriCAD/stiller/` |
| Windows | `%APPDATA%\PiriCAD\stiller\` |
| macOS | `~/Library/Application Support/PiriCAD/stiller/` |

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
  `QgsRasterFillSymbolLayer` bir **dosya yolu** tutar, PiriCAD'in görsel dolgusu
  ise baytları ve künyesini belgenin içinde taşır — çizim e-postayla gittiğinde
  ayakta kalmasını sağlayan şey bu.

Alınabilecek olan alındı: **düzenin kendisi** — ve sadeleştirilerek. QGIS bunu iki
pencereye bölüyor (sembol seçici ve Stil Yöneticisi) ve seçili tipin okumadığı
alanları da soluk hâlde gösteriyor. Burada tek pencere var ve görünmeyen alan yok
sayılan alan değildir.
