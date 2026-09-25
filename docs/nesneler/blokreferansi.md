# Blok referansı

## Nedir

Bir **blok tanımını** çizime yerleştiren nesne: rögar kapağı sembolü, kuzey oku,
antet. Tanım bir kez çizilir; referans onu bir noktaya, ölçekle, açıyla, aynalı ve
dizi hâlinde koyar. DXF `INSERT` budur.

## Nasıl saklanır

Halka tek tepe noktasıdır: **ekleme noktası**. Yükte hangi blok, x ve y ölçeği
(kesin oran; eksi ölçek aynalar), dönme açısı (mikroderece), dizi sütun/satır sayısı
ve aralıkları, ve çizilen biçimin kutusu durur. Tanımın nesneleri aynı nesne
tablosunda "blok içinde" bayrağıyla durur ([Nesne türleri](README.md#bloklar));
referans onları **kopyalamaz**, çizerken yerleştirir.

## Nasıl çizilir

Her üyenin çizilen biçimi alınır, taban noktasına göre ölçeklenir, döndürülür ve
ekleme noktasına konur — hepsi tam sayı aritmetiğiyle: ölçek `çarp-böl-yuvarla`,
dönme çeyrek turlarda tam. Üye kendi katmanını, rengini ve yazısını korur; `0`
katmanındaki üye referansın katmanına, rengi **bloktan** (ByBlock) olan üye referansın
rengine uyar. İç içe bloklar 32 kata kadar izlenir.

Bir üye yazısının harfleri, taban çizgisi ne kadar uzadıysa o kadar büyür: 2 kat
ölçekli bir referansta 0,5 m'lik yazı 1 m çizilir. x ve y ölçeği farklıysa harfler
taban çizgisinin uzadığı kadar büyür; harfin eni ayrıca esnetilmez.

## Öznitelikler

Tanımdaki `{no}` gibi süslü ayraçlı bir yazı bloğun **alanıdır**; referans onu kendi `no`
hücresinin değeriyle çizer (DXF'in `ATTDEF` ve `ATTRIB` ikilisi). Değerler referansın
sıradan öznitelik hücreleridir: nitelik panelinde görünür, `ÖZNİTELİK` ile değişir,
dosyaya yazılır. Değeri olmayan referansın alan yazısı boş çizilir. İç içe bir bloğun
alanları en dıştaki referansın değerleriyle çizilir.

## Yakalama noktaları

Yakalama, referansın **çizdiği** şeyi tutar; her nokta üyeyi çizen yerleşimden geçer, bu
yüzden döndürülmüş, aynalanmış ve iç içe bir blokta da ekranda gördüğünüz köşeye
oturur — ve referans [patlatıldığında](../komutlar/explode.md) aynı nokta parçada durur.

| Mod | Nereye |
|---|---|
| Ekleme noktası | Referansın yerleştirildiği noktaya; iç içe bloğun ekleme noktasına da |
| Uç nokta | Üye çizgilerin köşelerine, yayların ve elipslerin uçlarına |
| Orta nokta | Üye çizgilerin kenar ortalarına, yayların ortasına |
| Merkez | Üye dairelerin, yayların ve elipslerin merkezine |
| Çeyrek | Üye dairelerin çizimdeki kuzey, doğu, güney, batı noktalarına (x ve y ölçeği eşitse) |
| Ağırlık merkezi | Üye alanların ağırlık merkezine |
| Düğüm | Üye noktalara |
| En yakın, dik, kesişim | Üyelerin çizilen kenarlarına |

Dizili bir referansta her kopyanın noktaları ayrı ayrı sunulur.

## Dış referans

Tanımın üyeleri **başka bir dosyadan** da gelebilir: [DIŞREFERANS](../komutlar/xref.md) bir
proje, DXF ya da DWG dosyasını böyle bir tanım olarak bağlar. Referans her blok referansı
gibi çizilir, seçilir ve yakalanır; tanımın içi ise dosyanındır — düzenlenmez,
patlatılmaz, proje dosyasına yazılmaz, her açılışta ve her yenilemede dosyasından okunur.
Dosyanın katmanları ve blokları `AD|KATMAN`, `AD|BLOK` adıyla gelir.

## Ölçüler

Referansın kendi alanı ve çevresi sıfırdır; ölçmek istediğiniz üyeyi tanımda
ölçün.

## Dosya ve dış biçimler

Proje dosyasında tür sütunu `9` (`core.block_reference`); blok tanımları kendi
tablosunda ([proje dosyası](../veri/proje-dosyasi.md)). DXF `BLOCK` tanımları ve
`INSERT` referansları yapısıyla gelir ve gider: dosyadaki blok yapısı korunur, açılmaz.
GeoPackage'a üyelerin çizilen biçimleri yazılır.

## Komutlar

```
KATMAN ad=SEMBOL
DAİRE merkez=0,0 cevre=1,0
SEÇ KATMAN katman=SEMBOL
BLOK ad=BACA taban=0,0
BLOKEKLE ad=BACA nokta=20,0 olcek=2 aci=90
```

[BLOK](../komutlar/block.md), [BLOKEKLE](../komutlar/insert.md) (bir kitaplık dosyasından da:
`BLOKEKLE dosya=`), [BLOKDÜZENLE](../komutlar/block_edit.md),
[DIŞREFERANS](../komutlar/xref.md).

## Sınırlar

Tanımın üyeleri doğrudan düzenlenemez; tanımı [BLOKDÜZENLE](../komutlar/block_edit.md)
ile açıp düzenlersiniz ve kaydettiğinizde bütün referanslar yeni biçimi çizer. Tek bir
referansı tanımdan koparmak için onu [PATLAT](../komutlar/explode.md) ile açın: her üye
kendi türünde, referansın çizdiği yerde çıkar. Blok tanımı silinemez (ekle-yalnız
tablo); kullanılmayan tanım dosyada kalır. Aynalanmış bir referanstaki
üye yazısı, taban çizgisi ters döndüğü için baş aşağı okunur; ekran harfi aynalamaz.
