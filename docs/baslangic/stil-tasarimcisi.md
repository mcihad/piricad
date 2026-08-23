# Stil Tasarımcısı

Bir katmanın nasıl çizileceğini fare ile tasarlamak için. Yazdığı şey `STİL`
komutlarıdır: tasarımcının belgeye giden özel bir yolu yoktur, dolayısıyla burada
kurduğunuz her sembolü bir betik de yazabilir.

## Nasıl açılır

Katmanlar panelinde katmana **sağ tıklayın → Stili düzenle…**

Pencere seçili katmanın şu an ne çizdiğiyle açılır: nesneleri bir gösterim
taşıyorsa o, taşımıyorsa katmanın kendi görünümü.

## Pencerede ne nerede

Düzen QGIS'in sembol seçicisinin aynısıdır ve bu bilerek böyledir — QGIS
kullanmış bir plancı pencereyi tanır.

| Yer | Ne |
|---|---|
| Üst | Bütün sembolün önizlemesi |
| Sol | **Sembol katmanları** yığını, üstteki en son çizilen |
| Sağ | Seçili katmanın özellikleri |
| Alt | Uygula, kütüphaneye kaydet, vazgeç |

Yığın listesi **üstten alta** okunur: listedeki ilk satır en son çizilen, yani
ekranda en üstte görünen katmandır. `▲` ve `▼` düğmeleri satırı gördüğünüz yöne
taşır.

Önizlemeler tuvalin **kendi arka ucundan** geçer. Yani soldaki küçük resim,
çizimde göreceğiniz şeyin aynısıdır — ayrı bir önizleme çizicisi olsaydı ikisi
er geç ayrışırdı.

## Katman özellikleri

Her alan `STİL` komutunun bir parametresidir; hangisi olduğu
[STİL sayfasında](../komutlar/style.md) tablo hâlinde yazılı.

| Alan | `STİL` parametresi |
|---|---|
| Tip | `tip` |
| Çizgi rengi | `renk` |
| Çizgi kalınlığı | `kalinlik` |
| Dolgu rengi | `dolgu` |
| Ölçü birimi | `birim` |
| Boyut | `boyut` |
| Aralık | `aralik` |
| İkinci eksen | `aralik_y` |
| Açı | `aci` |
| Şekil | `sekil` |
| Yerleşim | `yerlesim` |
| Saydamlık | `saydamlik` |

Tip kutusunda parantez içinde yazan (`gorsel-dolgu` gibi) makine adıdır ve
komut satırına yazacağınız şeydir.

## Uygula

**OK** yığındaki her sembol katmanı için bir `STİL` satırı gönderir: ilki sembolü
kurar, kalanlar `ekle=evet` ile üstüne biner. Komut günlüğünde satırların
kendisini görürsünüz.

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

Alınabilecek olan alındı: **düzenin kendisi**.
