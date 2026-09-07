# METİN — Çizime Yazı Yazma

Paftaya ada ve parsel numarası, plan notu, lejant açıklaması ya da herhangi bir
yazı koyan herkes için; bu sayfayı bitirdiğinizde metni konumlandırmayı,
yüksekliğini ve hizalamasını vermeyi bileceksiniz.

> **Faz 0 durumu.** Metin belgeye yazılıyor, ekranda çiziliyor, döndürülebiliyor ve
> hizalanabiliyor. **Faz 1'de** gelecekler: yazı tipi seçimi (bugün sistem yazı tipi
> kullanılır), çok satırlı metin, ve öznitelikten üretilen otomatik etiketler —
> "her parselin ada/parsel numarasını merkezine yaz" gibi.

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
```

İçinde boşluk olan yazı tırnak içine alınır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Taban çizgisinin başlangıç noktası |
| `yazi` | Yazılacak metin. Zorunlu |
| `yukseklik` | Zeminde milimetre. Verilmezse proje ayarı `metin_yüksekliği` kullanılır |
| `bitis` | Taban çizgisinin bitişi. Verilmezse yatay yazılır |
| `hizalama` | `sol`, `orta`, `sag` veya `merkez` |

Hizalama, taban çizgisinin harflerin neresinde durduğunu belirler:

| Değer | Taban çizgisi nerede |
|---|---|
| `sol` | Yazının solunda başlar — varsayılan |
| `orta` | Yazının yatay ortasında |
| `sag` | Yazının sağında biter |
| `merkez` | Hem yatay hem düşey ortada — bir parsel numarasının istediği |

`bitis` verildiğinde yazı o yöne döner. Bir yol adını yolun kendi doğrultusunda
yazmak, ya da bir cephe ölçüsünü cepheye paralel koymak böyle yapılır.

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

Yol doğrultusunda dönmüş bir yol adı — taban çizgisi iki nokta arasında:

```
METIN 485300,4310255 "ATATÜRK CADDESİ" 3000 bitis=485420,4310265
```

Göreli koordinatla, bir önceki noktadan:

```
METIN @0,-5 "Plan notu 3" 1500
```

### Arayüz

Sol paletteki **metin** aracına basın ya da komut satırına `METİN` yazın. Önce
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

Var olan bir yazının metnini değiştirmek Faz 1'de gelecek; bugün silip yeniden
yazmak gerekir.

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

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
