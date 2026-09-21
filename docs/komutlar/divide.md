# BÖLÜMLE — Nesne Boyunca İşaret

Bir yol ekseni üzerine istasyon kazığı ya da bir kenara eşit bölme işareti
koyacak herkes için.

## Ne yapar

Var olan bir nesne **boyunca** nokta yerleştirir:

| Parametre | Anlamı |
|---|---|
| `sayi=<k>` | Nesneyi k eşit parçaya bölen **k−1** nokta |
| `aralik=<m>` | Başlangıçtan itibaren sabit aralıkla yürür — bir kilometraj listesi |

Tam olarak **birini** verin; ikisi birden hangisini kastettiğini bilmeyen bir
çağrıdır ve ret cümlesi nesnenin uzunluğunu söyler.

**`ARANOKTA` ile farkı:** `ARANOKTA` iki nokta arasındaki bir doğruyu böler; bu
bir **nesneyi** böler — ölçülmüş bir bordürü, bir yol eksenini — ve bir kazık
listesi gerçekte böyle istenir. Aralık bir köşeyi geçebilir: 60 metrelik istasyon
bir L'nin ikinci ayağına düşerse, onu bulmak tek bir kenar boyunca değil **run
boyunca** okumak demektir ve komut bunu yapar.

## Adlar

| Ad | Tür |
|---|---|
| `BÖLÜMLE` | Türkçe, birincil |
| `BOLUMLE` | ASCII katlanmış Türkçe |
| `DIVIDE` | İngilizce karşılık |
| `BLM` | Kısaltma |
| `core.divide` | Komut kimliği |

## Sözdizimi

```text
BÖLÜMLE nesne=<kimlik> sayi=<k>
BÖLÜMLE nesne=<kimlik> aralik=<m>
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | seçim | 1 | Bölünecek nesne |
| `sayi` | tamsayı | 0..1 | 2–10000: kaç eşit parçaya bölünecek |
| `aralik` | sayı | 0..1 | Sabit aralık (m) |
| `blok` | Nokta yerine **bu bloğu** koyar; blok önceden `BLOK` ile tanımlı olmalı |
| `hizala` | Bloğu üzerinde durduğu **kenarın doğrultusuna** çevirir; varsayılan hayır |

## Örnekler

### Komut satırı

100 metrelik bir çizgiyi dörde bölen üç kazık:

```text
BÖLÜMLE nesne=1 sayi=4
```

```text
3 işaret yerleştirildi (uzunluk 100.000000 m).
```

L biçimli bir eksende 20 metrelik kilometraj:

```text
BÖLÜMLE nesne=1 aralik=20
```

### Arayüz

**Değiştir > Bölümle**. Nesneyi seçip Enter'a basın, sonra `sayi=` ya da
`aralik=` yazın.

### Betik

```json
{ "cmd": "core.divide", "args": { "nesne": [1], "aralik": 25 } }
```

## Nokta yerine blok

Bir güzergâh boyunca **direk, rögar, ağaç ya da bordür işareti** dizmek — bu
parametrenin bütün varlık sebebi budur. Her istasyona tek tek `BLOKEKLE` yazmak
aynı işi elle yapmaktır.

```text
BLOK ad=DİREK nesneler=1 taban=0,0
BÖLÜMLE nesne=2 aralik=25 blok=DİREK
```

```text
3 blok yerleştirildi (uzunluk 100.000000 m).
```

Blok **önceden tanımlı** olmalıdır: burada yeni bir tanım üretmek
[`BLOK`](block.md)'un işini ikinci bir yerde yapmak olurdu. Tanımsız bir ad
reddedilir ve ret ne yapılacağını söyler.

`hizala=evet` bloğu, üzerinde durduğu **kenarın doğrultusuna** çevirir — bir okun
ya da bordür işaretinin istediği şey budur. Bir rögar kapağı için gereksizdir, bu
yüzden varsayılan kapalıdır: dik çizilmiş bir blok, istenmedikçe dik kalır.

Doğrultu `atan2_udeg`'den gelir, libm'den değil: bir blok referansının dönüşü
mikro-derece olarak saklanır ve o fonksiyon tam olarak onu verir (§7.3).

## Geri alma

Tek işlem, tek **Ctrl+Z**: kaç işaret koyduysa birlikte gider.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır; her parametre için tip ve adet üretilmiş
[komut referansındadır](referans.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Tam olarak birini verin: sayi= ya da aralik=` | Hiçbiri ya da ikisi verildi | Birini verin |
| `Aralık sıfır ya da eksi olamaz.` | `aralik` geçersiz | Artı bir değer verin |
| `Nesne bir daire; BÖLÜMLE bu sürümde çizgileri ve alan sınırlarını böler.` | Eğri seçildi | Eğriler Faz 2'de |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [ARANOKTA](point_along.md) — iki nokta arasındaki doğruyu böler
- [ÇİZGİDÜZENLE](pedit.md) — `islem=ters` ile listeyi öbür uçtan saydırmak
