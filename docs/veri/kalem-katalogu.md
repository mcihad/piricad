# Kalem kataloğu — sayısallaştırma sınıflarını yazma

Kurumunun "bina", "yol ekseni", "parsel" tanımlarını PiriCAD'e öğretmek isteyen harita
mühendisleri ve GIS sorumluları için. Bu sayfayı bitirdiğinizde bir sınıf paketi yazabilir,
programa gösterebilir ve yanlış yazdığınız bir tanımın neden ve nerede reddedildiğini
okuyabilirsiniz.

Bir **sınıf** (kalem), çizilen bir şeyin geometri türünü, katmanını, taşıdığı alanları ve
başlangıç değerlerini, gösterimini ve denetimlerini birlikte söyler. Tanımlar **veridir**: bir
dosya kopyalanır, program yeniden derlenmez ve bir yönetmelik değişikliği bir veri sürümüdür
(CLAUDE.md 5.13).

## Paket nasıl gösterilir

PiriCAD'in örnek paketi `data/catalogs/cad/kalem-katalogu.json` dosyasıdır. Kendi paketiniz için
bu dosyayı kopyalayın, düzenleyin ve **Tercihler ▸ Veri Kaynakları ▸ Kalem kataloğu** alanına
(`core.kalem.katalog` ayarı) yolunu yazın:

```text
AYAR ad=core.kalem.katalog deger=kurumum/kalem-katalogu.json
```

Ayar **uygulama ayarıdır** (makineye özgü bir dosya yoludur) ve en çok 48 karakterdir; uzun bir
yol için dosyayı kısa bir dizine koyun. Dosya değişince bir sonraki komut yeni sürümü okur;
programı yeniden başlatmak gerekmez.

## Paketin başlığı

| Alan | Ne |
|---|---|
| `schema_version` | Biçimin sürümü; şimdi `1` |
| `package_version` | Her içerik değişikliğinde artar (`0.1.0`). Katman hangi sürümle bağlandığını saklar |
| `id` | Paketin kimliği; `@` ve `/` içeremez. Bir katman `paket@sürüm/sınıf` biçiminde bağlanır |
| `source` | Tanımları kimin koyduğu: kurum, yönetmelik ve ek |
| `published` | Yayın tarihi (`2026-10-03`) |
| `licence` | Dağıtım koşulu |
| `siniflar` | Sınıflar, gösterildikleri sırayla |

## Bir sınıf

```json
{
  "id": "bina",
  "ad": "Bina",
  "adlar": ["BINA", "BUILDING", "BNA"],
  "ozet": "Bir yapının zemindeki oturum alanı; kapalı bir alan.",
  "geometri": "alan",
  "katman": { "ad": "BINA", "grup": "TOPOGRAFYA > YAPI", "renk": "#7A4A1C",
              "kalinlik_mm": 0.35, "dolgu": "#7A4A1C30", "aciklama": "Bina oturum alanları" },
  "alanlar": [
    { "kimlik": "sinif_kodu", "ad": "Sınıf kodu", "tur": "metin", "varsayilan": "BNA" },
    { "kimlik": "kat_sayisi", "ad": "Kat sayısı", "tur": "tam_sayi", "varsayilan": "1" },
    { "kimlik": "yapi_turu", "ad": "Yapı türü", "tur": "metin", "varsayilan": "Betonarme",
      "secenekler": ["Betonarme", "Yığma", "Çelik", "Ahşap", "Diğer"] }
  ],
  "dogrulama": { "en_az_alan_m2": 4.0 }
}
```

| Alan | Ne |
|---|---|
| `id` | Kararlı, küçük harf, paket içinde tek; **bir daha kullanılmaz** (veri paketi kuralı). `@`, `/` ve boşluk içeremez |
| `ad` | Türkçe birincil ad |
| `adlar` | Sınıfı bulduran başka sözcükler: ASCII karşılığı, İngilizce, kısaltma. Arama Türkçe büyük/küçük harf kurallarıyla yapılır |
| `ozet` | Bir cümle; kalem listesinin ipucunda çıkar |
| `kaynak` | Sınıf bir yönetmelikten ya da standarttan geliyorsa atıf: yönetmelik, ek, madde, Resmî Gazete tarihi. Boşsa sınıf bir kurum tanımıdır, mevzuat iddiası taşımaz |
| `geometri` | `nokta`, `cizgi` ya da `alan` |
| `katman.ad` | Katmanın adı. İki sınıf aynı katmanı kullanamaz |
| `katman.grup` | Katman ağacındaki yer, düzeyler `>` ile ayrılır |
| `katman.renk` | Çizgi rengi, `#RRGGBB` |
| `katman.kalinlik_mm` | Çizgi kalınlığı, kâğıt milimetresi (0–20) |
| `katman.dolgu` | Alan sınıfında iç rengi: `#RRGGBB` ya da saydamlıklı `#RRGGBBAA` |
| `katman.aciklama` | Katmanın açıklaması |
| `alanlar` | Sınıfın taşıdığı sütunlar |
| `dogrulama` | `en_az_alan_m2` ve `en_az_uzunluk_m`; [KALEMDENETİM](../komutlar/feature_class_check.md) bakar |

Katmanın rengi, grubu, kalınlığı ve dolgusu katman **ilk yaratılırken** uygulanır. Katman zaten
varsa kullanıcının renklerine ve grubuna dokunulmaz.

## Bir alan

| Alan | Ne |
|---|---|
| `kimlik` | Sütun kimliği, küçük harf ASCII (`kat_sayisi`) |
| `ad` | Tabloda görünen ad |
| `ozet` | Bir cümle |
| `tur` | [SÜTUN](../komutlar/column.md)'un türleri: `tam_sayi`, `uzunluk`, `evet_hayir`, `metin`, `kod`, `ondalik`, `tarih` |
| `varsayilan` | Yeni nesne bu değerle başlar. Türün okuyabileceği metin olmalıdır; `uzunluk` türünde milimetre |
| `zorunlu` | Boşsa [KALEMDENETİM](../komutlar/feature_class_check.md) raporlar. Çizimi engellemez: parsel önce çizilir, numarası sonra gelir |
| `basamak` | `ondalik` türünde ondalık basamak sayısı |
| `secenekler` | Alanın alabileceği tek değerler. Varsayılan bunların içinde olmalıdır |

**Ortak alan.** Aynı kimlikli alanı paketin birden çok sınıfı taşıyorsa (`sinif_kodu`) sütun baştan
projenin sütunu olur; yalnız bir sınıfın taşıdığı alan o sınıfın katmanına özeldir. Aynı kimlik
**aynı türde** olmalıdır; başlangıç değeri her sınıfta ayrı olabilir.

## Yüklenirken ne reddedilir

Çalışmayacak bir tanım paket yüklenirken reddedilir ve mesaj sınıfı ve alanı adıyla söyler;
hiçbir şey yarım kurulmaz. Reddedilenler: bilinmeyen alan türü, türün okuyamadığı varsayılan,
kendi `secenekler` listesinin dışında bir varsayılan, geçersiz `geometri`, `katman.ad` olmayan
sınıf, `#RRGGBB` olmayan renk, 0–20 dışında kalınlık, iki kez tanımlanmış sınıf ya da alan
kimliği, iki sınıfın aynı katmanı kullanması, kimliği ya da sürümü olmayan paket.

## Mevzuat ve onay

Örnek paket **hiçbir yönetmelik atfı taşımaz**. BÖHHBÜY ya da MPYY kodlarına bağlı gerçek bir sınıf
paketi, `kaynak` alanında yönetmeliği, ekini, maddesini ve Resmî Gazete tarihini taşır ve harita
mühendisi / şehir plancısı onayıyla yayımlanır (CLAUDE.md 6.11); bir mevzuat değişikliği paketin
yeni bir sürümüdür.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `'<yol>' açılamadı; kalem kataloğu yok ya da okunamıyor. …` | Dosya yok | `core.kalem.katalog` yolunu düzeltin |
| `'<yol>' geçerli JSON değil: …` | Biçim hatası | JSON'u düzeltin |
| `'<yol>' paket kimliğini (id) ya da sürümünü (package_version) taşımıyor.` | Başlık eksik | `id` ve `package_version` yazın |
| `'<yol>' `siniflar` dizisi taşımıyor.` | Sınıf yok | En az bir sınıf yazın |
| `'<yol>' sınıf '<id>': `geometri` nokta, cizgi ya da alan olmalı.` | Geçersiz geometri | Üçünden birini yazın |
| `'<yol>' sınıf '<id>': `katman.ad` yok.` | Katman adı eksik | `katman.ad` yazın |
| `'<yol>' sınıf '<id>' alanı '<kimlik>': `tur` '<tür>' bilinmiyor.` | Alan türü yanlış | Yukarıdaki türlerden birini yazın |
| `'<yol>' sınıf '<id>' alanı '<kimlik>': varsayılan '<değer>' okunamıyor — …` | Varsayılan türüne uymuyor | Değeri türüne göre yazın |
| `… varsayılan '<değer>' kendi seçenekleri arasında değil.` | Varsayılan listede yok | Listeye ekleyin ya da varsayılanı değiştirin |
| `'<yol>' sınıf '<id>': '<katman>' katmanını başka bir sınıf da kullanıyor.` | İki sınıf bir katmanda | Katman adlarını ayırın |
| `… aynı kimlik iki kez tanımlı.` | Yinelenen sınıf ya da alan | Kimlikleri tek yapın |
| `… renk '<…>' #RRGGBB biçiminde olmalı.` / `… dolgu '<…>' #RRGGBB ya da #RRGGBBAA olmalı.` | Renk yazımı | `#RRGGBB` yazın |
| `… `kalinlik_mm` 0–20 arasında olmalı.` | Kalınlık aralık dışı | 0–20 arası bir sayı |

## İlgili

- [KALEM](../komutlar/feature_class.md), [KALEMBAĞLA](../komutlar/feature_class_bind.md),
  [KALEMDENETİM](../komutlar/feature_class_check.md)
- [Kalemle sayısallaştırma](../baslangic/kalemle-sayisallastirma.md)
- [Öznitelik sütunu: SÜTUN](../komutlar/column.md)
