# METİN — Çizime Yazı Yazma

Paftaya ada ve parsel numarası, plan notu, lejant açıklaması ya da herhangi bir
yazı koyan herkes için; bu sayfayı bitirdiğinizde metni konumlandırmayı,
yüksekliğini, dokuz hizalamadan birini, çok satırlı bir notun satır aralığını ve
satırların hangi genişlikte kırılacağını vermeyi bileceksiniz.

Yazılar programla gelen **IBM Plex Sans** yazı tipiyle çizilir; ekranda, PDF'te ve
yazıcıda aynı yazı tipi ve aynı satır düzeniyle. Yazı tipi seçimi ileride gelecek
(TODOS C-12'nin sonraki aşamaları); öznitelikten otomatik etiket bugün
[`ETİKET`](label.md) ile yazılır.

**Yazı tipinde olmayan bir harf boş bir kutu olarak görünür** — ekranda da, PDF'te ve
yazıcıda da aynı kutu. Harf, bilgisayarda bulunan başka bir yazı tipinden **ödünç
alınmaz**: öyle olsaydı pafta ekrandakinden ve başka bir bilgisayarda basılan aynı
paftadan farklı çıkardı. Türkçe harflerin hepsi (ç, ğ, ı, İ, ö, ş, ü) ve Ø, °, ±
yazı tipindedir; kutu ancak Çince bir karakter, bir matematik simgesi (`⌀`) gibi
harflerde çıkar. Tuval böyle bir yazının yanında hangi harfin eksik olduğunu uyarı
renginde söyler — `yazı tipinde yok: 漢 (U+6F22)`; bu not yalnız ekrandadır, paftaya
çıkmaz. [`NESNEBİLGİ`](entity_info.md) de aynı harfleri adıyla ve koduyla verir.

## Ne yapar

`METİN`, çizime bir **yazı nesnesi** koyar. Bu bir etiket değil, çizilmiş bir
nesnedir: kendi konumu, kendi yüksekliği, kendi dönüklüğü vardır, seçilir, silinir,
geri alınır ve dosyayla birlikte gider.

Yazının **geometrisi taban çizgisidir** — iki nokta: başlangıç ve satır sonu. Bu
bir tasarım kararıdır ve iki şey kazandırır. Birincisi, yazı sıradan bir açık halka
taşıdığı için kırpma, yakalama ve seçme kodu onu özel bir durum olarak bilmez.
İkincisi, **dönüklük saklanan bir açı değil, tam sayı bir doğru parçasının yönüdür**;
yani hiçbir trigonometri saklanmaz ve iki bilgisayar dönüklük konusunda anlaşmazlığa
düşemez. DXF'in TEXT nesnesi de aynı sebeple bir yerleştirme ve bir hizalama noktası
tutar.

**Yükseklik zemin milimetresidir**, kâğıt değil. 2500 yazarsanız yazı zeminde 2,5 m
yüksekliğindedir; 1/1000 ölçekli bir paftada bu kâğıtta 2,5 mm eder. Ölçek
değiştiğinde yazının paftadaki boyu da değişir — MPYY'nin öngördüğü budur ve
çizimin söylediği şeyin bir parçasıdır.

Ölçülen yükseklik **büyük harfin boyudur**, yazı tipinin em boyu değil: `2500`
dediğinizde `A` harfi zeminde tam 2,5 m olur. CAD'in her yerinde anlam budur —
DXF'in 40 grup kodu da, bir paftadaki yazı yüksekliği de aynı şeyi söyler — ve
içe aktarılan bir yazı bu yüzden dosyadaki boyuyla çizilir.

### Çok satırlı yazı

Yazının içindeki `\n` bir satır sonudur: `yazi="İMAR NOTU\nYapı yaklaşma 5 m"` iki
satırdır. Boş bir satır (`\n\n`) kalır; iki paragrafı birbirinden böyle
ayırırsınız.

- Satırlar arası, **yüksekliğin 5/3'ü** kadardır — DXF MTEXT'in "3'e 5" aralığı, yani
  AutoCAD'in aynı yazıya verdiği aralık. `satir_araligi=1.5` bunu bir buçuk katına
  çıkarır (0,25–4).
- `genislik=12` verilirse uzun satırlar **kelime sınırından** alt satıra geçer ve hiçbir
  satır 12 m'yi aşmaz; tek başına 12 m'den uzun bir kelime kendi satırında kalır.
  Genişlik verilmezse satır yalnız `\n` olan yerde kırılır.
- Ekran ve PDF satırları aynı kuralla ve aynı ölçüyle kırar, dizer ve hizalar; DXF'e
  çok satırlı yazı MTEXT olarak, satırları, aralığı, genişliği ve hizasıyla gider
  (ayrıntı: [yazı nesnesi](../nesneler/yazi.md)).

### Hizalama

Nokta yazının **neresinde** durur: sütun satırın solu, ortası ya da sağı; sıra ilk
satırın büyük harflerinin üstü, yazının ortası ya da son satırın tabanı. Her satır
kendi başına hizalanır — ortalanmış bir notun her satırı ortalıdır.

| | sol | orta | sağ |
|---|---|---|---|
| **İlk satırın üstü** | `ust_sol` | `ust_orta` | `ust_sag` |
| **Ortası** | `orta_sol` | `merkez` | `orta_sag` |
| **Son satırın tabanı** | `sol` — varsayılan | `orta` | `sag` |

Tek satırlı bir yazıda üç sıra DXF TEXT'in üst, orta ve taban hizalarıdır. Bir parsel
numarası `merkez` ister; bir başlığın altına sarkan not `ust_sol`.

## Adlar

| Ad | Tür |
|---|---|
| `METİN` | Türkçe, birincil |
| `METIN` | ASCII karşılık |
| `YAZI` | Türkçe eşanlamlı |
| `TEXT` | İngilizce karşılık |
| `MT` | Kısaltma |
| `core.text` | Komut kimliği |

## Sözdizimi

```text
METİN
METİN <nokta> <yazı>
METİN <nokta> <yazı> <yukseklik>
METİN noktalar=<nokta> yazi=<yazı> yukseklik=<mm> hizalama=<hiza> bitis=<nokta>
      satir_araligi=<kat> genislik=<metre>
```

İçinde boşluk olan yazı tırnak içine alınır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Taban çizgisinin başlangıç noktası |
| `yazi` | Yazılacak metin. Zorunlu |
| `yukseklik` | Zeminde milimetre. Verilmezse proje ayarı `metin_yüksekliği` kullanılır |
| `bitis` | Yazının döneceği yön için bir nokta. Verilmezse yatay yazılır |
| `hizalama` | Noktanın yazının neresinde durduğu: yukarıdaki dokuz sözcükten biri; verilmezse `sol` |
| `satir_araligi` | Satırlar arası, tek aralığın katı: 0,25–4. Tek aralık yüksekliğin 5/3'üdür |
| `genislik` | Satırların kırılacağı genişlik, metre. Verilmezse satır yalnız `\n`'de kırılır |

`bitis` verildiğinde yazı o yöne döner. Bir yol adını yolun kendi doğrultusunda
yazmak, ya da bir cephe ölçüsünü cepheye paralel koymak böyle yapılır. Taban
çizgisinin uzunluğu `bitis`'e kadar değil, **yazının kendi genişliği** kadardır —
yazının kutusu harflerin kapladığı yerdir, uzağa verilmiş bir nokta kutuyu
uzatmaz ([Yazı ▸ Genişlik](../nesneler/yazi.md#genişlik)). `genislik` ile birlikte
verilirse yön `bitis`'ten, uzunluk genişlikten gelir.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Basit bir yazı, proje ayarındaki yükseklikle:

```
METİN 485320,4310220 "Ada: 1234"
```

Yüksekliği verilmiş bir parsel numarası — zeminde 2 m:

```
METIN 485330,4310225 "7" 2000
```

Parselin merkezine hizalanmış numara:

```
METIN 485330,4310225 "1234/7" 2000 hizalama=merkez
```

Yol doğrultusunda dönmüş bir yol adı — yazı ikinci noktaya doğru döner:

```
METIN 485300,4310255 "ATATÜRK CADDESİ" 3000 bitis=485420,4310265
```

Göreli koordinatla, bir önceki noktadan:

```
METIN @0,-5 "Plan notu 3" 1500
```

Bir başlığın altına sarkan, bir buçuk aralıklı, 30 m'de kırılan plan notu:

```
METİN noktalar=485300,4310300 yazi="PLAN NOTLARI\nYapı yaklaşma mesafesi ön bahçede 5 m, yan bahçelerde 3 m'dir." yukseklik=2000 hizalama=ust_sol satir_araligi=1.5 genislik=30
```

İki satırlı, merkezine hizalı bir yapılaşma koşulu — TAKS üstte, KAKS altta:

```
METİN noktalar=485330,4310225 yazi="TAKS 0,30\nKAKS 1,20" yukseklik=1500 hizalama=merkez
```

### Arayüz

Şeritte **Giriş ▸ Açıklama ▸ Metin**'e (ya da **Açıklama ▸ Yazı ▸ Metin**'e) basın ya da
komut satırına `METİN` yazın. Önce
başlangıç noktasını tıklayın, sonra yazıyı girin. Yakalama açıkken başlangıç noktası
mevcut nesnelere oturur — bir parsel köşesinden tam ölçülü bir yere yazı koymak için
[`MOD`](mode.md) ile uç nokta yakalamasını açık tutun.

Yazı ekranda okunamayacak kadar küçüldüğünde (3 pikselin altı) çizilmez. Bu bir
eksiklik değil: okunmayan bir yazıyı boyamak paftayı gri bir lekeye çevirir.

### Betik

```json
{
  "ad": "Parsel numaraları",
  "crs": "TUREF/TM30",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "NUMARA" } },
    { "cmd": "core.text",  "args": { "noktalar": [[485330000, 4310222000]],
                                     "yazi": "1234/7", "yukseklik": 2000,
                                     "hizalama": "merkez" } },
    { "cmd": "core.text",  "args": { "noktalar": [[485400000, 4310222000]],
                                     "yazi": "1234/8", "yukseklik": 2000,
                                     "hizalama": "merkez" } }
  ]
}
```

Betikte koordinatlar milimetre tam sayısıdır; komut satırındaki `485330,4310222`
metredir.

## Geri alma

`METİN` **tek bir geri alma adımıdır**. `GERİAL` hem yazıyı hem taban çizgisini
birlikte kaldırır — ikisi tek nesnedir.

```
GERİAL
```

Yazı yazılırken **Esc**'e basarsanız hiçbir şey oluşmaz; yarım kalmış bir metin
nesnesi diye bir şey yoktur.

Var olan bir yazının metnini, yüksekliğini, hizasını, satır aralığını ya da
genişliğini [`YAZIDÜZENLE`](edittext.md) değiştirir; o da tek geri alma adımıdır.

## Betikten kullanım

`METİN` betiklenebilir ve AI erişimine açıktır. Bir paftanın bütün parsel
numaralarını yazan betik bu komutun doğal işidir.

Her çağrı kendi geri alma adımıdır. Beş yüz numara yazan bir betik beş yüz adım
bırakır; tamamını tek adım yapmak için betiği [`BETİK`](script.md) ile çalıştırın.

AI bu komutu kullanabilir ama **koordinatları kendi üretemez**: her nokta kayıtlı
bir araç çağrısı sonucuna kadar izlenebilir olmalıdır ve öneri onaylanmadan
uygulanmaz (`CLAUDE.md` 5.7, 5.8).

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `'core.text' parametresi 'yazi' 1 değer bekliyor.` | Yazı verilmemiş | Noktadan sonra yazıyı ekleyin; boşluk içeriyorsa tırnak içine alın |
| `Yazı yüksekliği 0 mm geçersiz; sıfırdan büyük olmalı ve zemin sınırını aşmamalı.` | Yükseklik sıfır ya da negatif verilmiş | Milimetre cinsinden pozitif bir değer yazın, örnek `2000` |
| `Bir geometri en az bir halka ister, verilen: 0.` | Başlangıç noktası çözülememiş | Koordinatı denetleyin |
| `'core.text' daha fazla argüman almıyor. Fazlalık: '...'` | Fazladan argüman verilmiş | Boşluk içeren yazıyı tırnak içine alın |
| `'core.text': 'hizalama' için tanınmayan değer 'yukari'. Kabul edilenler: sol / orta / …` | Dokuz sözcükten biri değil | Tablodaki sözcüklerden birini yazın |
| `Satır aralığı 0,25 ile 4 arasında olmalı; 1 tek aralıktır.` | `satir_araligi` aralık dışında | 0,25 ile 4 arasında bir kat verin |
| `Genişlik eksi olamaz; satırları kırmamak için genislik=0 verin.` | `genislik` eksi | Pozitif bir genişlik verin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
