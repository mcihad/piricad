# YUVARLA — Köşe Yuvarlatma

Yol kurbu, kavşak dönüşü ya da yuvarlatılmış bir yapı köşesi çizen herkes için;
bu sayfayı bitirdiğinizde köşe yuvarlatmayı arayüzden, komut satırından ve
betikten yapmayı bileceksiniz.

## Ne yapar

`YUVARLA`, bir köşeyi verdiğiniz **yarıçapta bir yayla** yuvarlatır. Yay her iki
kenara da **teğettir**: kenarlar yaya kırılmadan bağlanır.

Köşe noktası, teğet noktalarıyla değiştirilir. Teğet noktalarının köşeye uzaklığı
`r / tan(θ/2)`'dir — dik bir köşede bu tam olarak `r` kadardır.

### Açık çizgide yay ayrı bir nesnedir

Açık bir çizgide yuvarlatma **üç nesne** bırakır: köşenin iki yanındaki iki çizgi
ve aralarında yeni bir [`YAY`](arc_draw.md).

Bu bir eksiklik değil, dürüstlüktür. Bu belge modelinde çoklu çizgi köşe
noktalarını tutar, "bulge" denen yay katsayılarını değil. Ayrı bir `YAY`
nesnesinin gerçek merkezi ve gerçek yarıçapı vardır, dolayısıyla uzunluğu ve
geometrisi kesindir.

Yayın süpürme yönü köşenin dönüş yönünden anlaşılır; ayrıca belirtmeniz gerekmez.

### Kapalı şekil yerinde yuvarlatılır

Bir **dikdörtgenin, alanın ya da kapalı çizginin** köşesi de yuvarlatılır ve şekil
**aynı nesne** olarak kalır: kimliği, katmanı, öznitelikleri (ada/parsel no) ve
ona bağlı yazılar korunur, alan olmaya devam eder — alanı ölçülür, ifraz edilir,
tampon alınır.

Kapalı bir şekli ikiye bölmek onu bir şeyi çevrelemez hâle getirirdi; bu yüzden yay
şeklin sınırına **köşe noktalarıyla** çizilir: iki teğet noktası ve aralarında,
[`YAY`](arc_draw.md)'ın kendisinin çizildiği noktalar (çeyrek daire için 16 kenar).
Komut, çizilen köşenin gerçek yaydan en çok ne kadar saptığını söyler:

```text
Köşe yuvarlatıldı (yarıçap 5,000 m). Kapalı şeklin sınırı köşe noktalarından oluştuğu için yay 16 kenarla çizildi; gerçek yaydan en çok 6 mm sapar.
```

Alan da yuvarlatılmış hâliyle hesaplanır: 40 × 30 m'lik bir alanın bir köşesi 5 m
yarıçapla yuvarlatılınca [`ALANÖLÇ`](measure_area.md) 1200,00 m² yerine 1194,60 m²
okur. Gerçek yayla alan `r² − πr²/4` kadar, yani 1194,64 m² olurdu; aradaki
0,04 m² yayın 16 kenarla çizilmesinden gelir.

Diğer kurallar [`PAH`](chamfer.md) ile aynıdır: açık çizginin uçları köşe
değildir ve teğet noktaları komşu kenarların dışına taşamaz.

**Hangi köşe?** Nesne verilmemişse ve tek bir nesne seçili değilse komut önce
köşeyi sorar: köşeye **bir kez tıklamak** hem nesneyi hem köşeyi seçer. Ardından
yarıçap sorulur; **yazabilir** ya da tuvalde **gösterebilirsiniz** — köşeden
imlece olan uzaklık yarıçaptır. İmleç hareket ettikçe yuvarlatılmış köşe tuvalde
vurgulu çizilir ve imlecin yanında `yarıçap 5 m` yazar.

## Adlar

| Ad | Tür |
|---|---|
| `YUVARLA` | Türkçe, birincil |
| `FILLET` | İngilizce karşılık |
| `YV` | Kısaltma |
| `core.fillet` | Komut kimliği |

## Sözdizimi

```text
YUVARLA nesne=<k> nokta=<n> yaricap=<metre>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Köşesi yuvarlatılacak nesnenin kimliği |
| `nokta` | İşlem yapılacak köşe. En yakın köşe seçilir |
| `yaricap` | Yuvarlatma yarıçapı, metre |

## Örnekler

### Komut satırı

Açık bir çizginin köşesi:

```text
YUVARLA nesne=1 nokta=485300,4310200 yaricap=8
```

```text
Köşe yuvarlatıldı.
```

Bir dikdörtgenin köşesi — şekil aynı nesne kalır:

```text
DİKDÖRTGEN 0,0 20,12
YUVARLA nesne=1 nokta=20,12 yaricap=5
```

### Arayüz

Sol araç sütununda **köşe ailesinin** düğmesini basılı tutun ya da sağ tıklayın ve
**Yuvarla**'yı seçin; aynı araç **Değiştir → Yuvarla** menüsündedir.

1. Yuvarlatılacak köşeye tıklayın. Nesne de bu tıklamayla seçilir.
2. İmleci köşeden uzaklaştırın: yay ve iki bacak tuvalde vurgulu çizilir.
3. İstediğiniz yerde tıklayın **ya da** yarıçapı komut satırına yazıp Enter'a
   basın (`8`).

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.polyline",
      "args": { "noktalar": [[485300000, 4310260000], [485300000, 4310200000],
                             [485360000, 4310200000]] } },
    { "cmd": "core.fillet",
      "args": { "nesne": [1], "nokta": [485300000, 4310200000], "yaricap": 8 } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`YUVARLA` tek bir geri alma adımıdır: [`GERİAL`](undo.md) açık çizgide hem yayı
kaldırır hem köşeyi geri getirir; kapalı şekilde köşeyi eski hâline döndürür.

## Betikten kullanım

Betikten çağrıldığında `nesne`, `nokta` ve `yaricap` verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `YUVARLA için nesne belirtilmedi. Örnek: YUVARLA nesne=1 nokta=10,10 yaricap=3` | Betik ne nesneyi ne köşeyi verdi | `nesne=` ve `nokta=` verin |
| `Orada köşesi kesilecek bir çizgi ya da alan yok. ...` | Tıklanan yerde nesne yok | Bir çizginin iki kenarının buluştuğu köşeye tıklayın |
| `Nesne N bir eğri, yazı ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda çalışır.` | Daire, yay, yazı ya da nokta verildi | Çizgi ya da alan seçin |
| `Burada iki kenarın buluştuğu bir köşe yok. ...` | Açık bir çizginin ucu gösterildi | İki kenarın buluştuğu bir köşe gösterin |
| `Bu köşede kenarlar aynı doğrultuda; kesilecek bir köşe yok.` | Kenarlar doğrusal | Gerçek bir köşe gösterin |
| `Yarıçap sıfırdan büyük olmalı.` | Sıfır ya da eksi yarıçap | Artı bir yarıçap verin |
| `Kesim komşu kenardan uzun: kenarlar 12,000 m ve 20,000 m, gereken 21,000 m. ...` | Yarıçap bu köşeye büyük | Daha küçük bir yarıçap verin ya da daha yakına tıklayın |
| `Bu köşe yuvarlatılamıyor: kenarlar üst üste geliyor.` | Kenarlar aynı doğrultuda geri dönüyor | Gerçek bir köşe gösterin |

## İlgili

- [`PAH`](chamfer.md) — köşeyi yay yerine düz kenarla keser
- [`YAY`](arc_draw.md) — üretilen yayın kendisi
