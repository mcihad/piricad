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

## Yakalama noktaları

| Mod | Nereye |
|---|---|
| Ekleme noktası | Referansın yerleştirildiği noktaya |
| Uç nokta, orta, en yakın, kesişim | Üyelerin çizilen kenarlarına |

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

[BLOK](../komutlar/block.md), [BLOKEKLE](../komutlar/insert.md).

## Sınırlar

Tanımın üyeleri yerinde düzenlenemez; `BLOKDÜZENLE` Faz 2'nin sonraki işidir. Ölçekli
bir referansta üye yazısının yüksekliği ölçeklenmez. Blok tanımı silinemez (ekle-yalnız
tablo); kullanılmayan tanım dosyada kalır.
