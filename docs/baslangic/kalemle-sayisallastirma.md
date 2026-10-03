# Kalemle Sayısallaştırma

Bir paftayı bina, yol, dere, direk ve ağaç olarak sayısallaştıran, çizdiği şeyin **hem çizim
hem GIS kaydı** olmasını isteyen herkes için. Bu sayfayı bitirdiğinizde sınıf seçerek çizmeyi,
eski çizimi sınıfa geçirmeyi ve teslimden önce paftayı denetlemeyi, kurulum dışında başka bir
yardım almadan yapabileceksiniz.

## Neden kalem

Sayısallaştırırken çizilen şeyin üç ayrı yanı vardır ve her biri ayrı ayrı unutulur: **şekli**
(bina kapalı bir alandır, dere açık bir çizgi), **katmanı** (hangisine gideceği) ve **kaydı**
(bina numarası, kat sayısı, yapı türü). Bir **kalem**, bu üçünü tek bir sözcüğe bağlar: "Bina".
Kalemi seçince katman ve sütunlar kurulur, çizdiğiniz nesne sınıfın başlangıç değerleriyle
başlar ve sınıfa uymayan bir şekil çizime hiç girmez.

Kalemlerin tanımı bir veri paketindedir; PiriCAD yedi örnek sınıfla gelir (bina, yol ekseni,
parsel, dere, direk, ağaç, çit). Bunlar **kurum tanımı ya da yönetmelik değildir**: kodları ve
alan adları örnektir. Kurumunuzun kendi sınıfları için bkz. [Kalem kataloğu](../veri/kalem-katalogu.md).

## 1. Kalemi seçin ve çizin

**Harita** sekmesindeki **Kalem** listesini açın ve **Bina**'yı seçin ya da komut satırına yazın:

```
KALEM bina
```

Etkin katman `BINA` olur. Şimdi bir bina çizin:

```
ALAN noktalar=0,0 12,0 12,9 0,9
```

Çizdiğiniz nesne `sinif_kodu=BNA`, `kat_sayisi=1` ve `yapi_turu=Betonarme` ile başlamıştır.
Kat sayısı farklıysa **Öznitelikler** panelinden ya da komutla değiştirin:

```
ÖZNİTELİK kat_sayisi 1 4
```

![Harita sekmesinde Kalem paneli: Bina seçili; çizimde binalar kahverengi dolgulu, parseller kırmızı, yol ve dere çizgi, direkler mor, ağaçlar yeşil noktalar](../komutlar/kalem-serit.png)

## 2. Sınıfa uymayanı çizemezsiniz

Bina katmanına açık bir çizgi çizmeyi deneyin:

```text
[hata] 'Bina' sınıfı kapalı alan ister (katman 'BINA'); çizilen nesne açık çizgi. Şekli sınıfa uydurun ya da başka bir sınıfı seçin (KALEM).
```

Komut bütünüyle geri sarılır; çizimde yarım nesne kalmaz. Doğru sınıfı seçip çizin:

```
KALEM yol_ekseni
ÇİZGİ 0,20 40,20
KALEM dere
ÇİZGİ 0,35 15,30 30,34 45,28
KALEM direk
NOKTA 10,15
KALEM agac
NOKTA 8,25
```

Aynı kural bütün istemciler için geçerlidir: fareyle, komut satırında, betikte ya da yapay zekâ
önerisi olarak çizdiğiniz aynı sonucu verir.

## 3. Eski çizimi sınıfa geçirin

Elinizde sınıfsız, eski katmanlarda çizilmiş nesneler varsa onları bir sınıfa **bağlayın**: nesneleri
seçin, kalem listesinden sınıfı seçin ve **Seçimi bağla**'ya basın. Komut satırından önizleme ve
eski sütundan alan taşıma da yapılır:

```
KATMAN ad=ESKI
ALAN noktalar=60,0 72,0 72,9 60,9
SÜTUN kimlik=kat tur=tam_sayi
ÖZNİTELİKHESAPLA ad=kat ifade=3 katman=ESKI
KALEMBAĞLA ad=bina katman=ESKI esle=kat:kat_sayisi onizle=evet
KALEMBAĞLA ad=bina katman=ESKI esle=kat:kat_sayisi
```

Önizleme hiçbir şey yazmaz; ne bağlanacağını, neyin atlanacağını (sınıfa uymayan nesneler tür
adıyla sayılır) ve hangi değerlerin taşınacağını söyler. Ayrıntı: [KALEMBAĞLA](../komutlar/feature_class_bind.md).

## 4. Teslimden önce denetleyin

**Sınıfı denetle** düğmesi (ya da `KALEMDENETİM`), sınıf izleyen her katmandaki her nesneye
sınıfın kurallarını sorar: geometri, en küçük alan ve uzunluk, zorunlu alanlar, izinli değerler:

```
KALEMDENETİM
```

Bulguları seçmek için `KALEMDENETİM sec=evet` yazın; sorunlu nesneler seçilir ve bir sonraki komut
tam onlara uygulanır. Ayrıntı: [KALEMDENETİM](../komutlar/feature_class_check.md).

## Katman panelinde

Bir sınıf katmanını seçince özellik paneli, `grup` satırının altında `sinif` satırıyla katmanın
hangi sınıfı izlediğini yazar. Katmanın hangi paketin hangi sürümüyle bağlandığı dosyada saklanır;
paket sonradan değişirse [KALEMDENETİM](../komutlar/feature_class_check.md) bunu söyler.

![Bina katmanı seçili: özellik panelinde sinif satırı ve katman ağacındaki grup](../komutlar/kalem-katman-paneli.png)

## Sırada ne var

Bugün olmayanlar:

- **Etiket kuralı** — sınıfın nesnelerine otomatik etiket; sınıf tanımı henüz bir etiket şablonu
  taşımıyor.
- **Topoloji kuralları** (komşu parsel sınırı, çakışma, boşluk) — sınıf bazında topoloji
  denetimi ayrı bir iştir ve henüz yoktur; bu komutlar yalnız nesnenin kendisine bakar.
- **Kurum onaylı kod listeleri** (BÖHHBÜY, MPYY detay kodları) — gerçek bir sınıf paketi olarak,
  harita mühendisi / şehir plancısı onayıyla gelir; örnek paket böyle bir iddia taşımaz.

## İlgili

- [KALEM](../komutlar/feature_class.md), [KALEMBAĞLA](../komutlar/feature_class_bind.md),
  [KALEMDENETİM](../komutlar/feature_class_check.md)
- [Kalem kataloğu](../veri/kalem-katalogu.md) — kendi sınıflarınızı yazın
- [Öznitelik tablosu](../veri/oznitelik-tablosu.md) — sınıfın alanlarını tabloda görün ve toplu hesaplayın
