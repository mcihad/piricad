# YUVARLA — Köşe Yuvarlatma

Yol kurbu, kavşak dönüşü ya da yuvarlatılmış bir yapı köşesi çizen herkes için;
bu sayfayı bitirdiğinizde köşe yuvarlatmayı arayüzden, komut satırından ve
betikten yapmayı bileceksiniz.

## Ne yapar

`YUVARLA`, bir köşeyi verdiğiniz **yarıçapta bir yayla** yuvarlatır. Yay her iki
kenara da **teğettir**: kenarlar yaya kırılmadan bağlanır.

Köşe noktası, teğet noktalarıyla değiştirilir. Teğet noktalarının köşeye uzaklığı
`r / tan(θ/2)`'dir — dik bir köşede bu tam olarak `r` kadardır.

### Yay ayrı bir nesnedir

Yuvarlatma **iki nesne** bırakır: kırılan çizgi ve yeni bir [`YAY`](arc_draw.md).

Bu bir eksiklik değil, dürüstlüktür. Bu belge modelinde çoklu çizgi köşe
noktalarını tutar, "bulge" denen yay katsayılarını değil. Yayı çizginin içine
gömmek onu kendi yaklaşık hâliyle saklamak olurdu; ayrı bir `YAY` nesnesi olarak
ise gerçek merkezi ve gerçek yarıçapı vardır, dolayısıyla uzunluğu ve geometrisi
kesindir.

Yayın süpürme yönü köşenin dönüş yönünden anlaşılır; ayrıca belirtmeniz gerekmez.

Diğer kurallar [`PAH`](chamfer.md) ile aynıdır: kapalı alanın her köşesi köşedir,
açık çizginin uçları değildir, ve yarıçap komşu kenarlardan kısa olmalıdır.

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

```text
YUVARLA nesne=1 nokta=485300,4310200 yaricap=8
```

```text
Köşe yuvarlatıldı.
```

### Arayüz

`YUVARLA` yazın, nesneyi verin, köşeyi tıklayın, yarıçapı yazın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.fillet",
      "args": { "nesne": [1], "nokta": [485300000, 4310200000], "yaricap": 8 } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`YUVARLA` tek bir geri alma adımıdır: [`GERİAL`](undo.md) hem yayı kaldırır hem
köşeyi geri getirir.

## Betikten kullanım

Betikten çağrıldığında `nesne`, `nokta` ve `yaricap` verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Nesne N bir eğri ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda çalışır.` | Daire, yay ya da nokta verildi | Çizgi ya da alan seçin |
| `Burada iki kenarın buluştuğu bir köşe yok. ...` | Açık bir çizginin ucu gösterildi | İki kenarın buluştuğu bir köşe gösterin |
| `Bu köşede kenarlar aynı doğrultuda; kesilecek bir köşe yok.` | Kenarlar doğrusal | Gerçek bir köşe gösterin |
| `Kesim komşu kenardan uzun: kenarlar A m ve B m, gereken C m.` | Değer kenarlardan büyük | Daha küçük bir değer verin |
| `Bu köşe yuvarlatılamıyor: kenarlar üst üste geliyor.` | Kenarlar aynı doğrultuda geri dönüyor | Gerçek bir köşe gösterin |

## İlgili

- [`PAH`](chamfer.md) — köşeyi yay yerine düz kenarla keser
- [`YAY`](arc_draw.md) — üretilen yayın kendisi
