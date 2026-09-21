# KILAVUZ — Cetvel Kılavuzu

## Ne yapar

Sonsuz bir **yapı çizgisi** koyar: yatay ya da düşey, sabit bir koordinatta.
İmleç ona oturur, pafta onu **basmaz**.

Çizim tahtasındaki kurşun kalem işaretlerinin karşılığıdır. Bir binayı bir sınıra
belli bir uzaklıkta oturtmak, bir aksı üç parselde aynı yerden geçirmek, bir
kesiti hep aynı hatta almak için kullanılır.

### Kılavuz bir nesne değildir

Geometrisi, stili, katmanı ve özniteliği yoktur. **Seçime girmez**, alan
hesabında sayılmaz, dışa aktarılmaz. Belgenin **mobilyasıdır**: dosyayla gider,
dosyayla gelir ve çizilen hakkında akıl yürüten hiçbir şey onu görmez.

Bu bilerek böyledir — bir yapı çizgisinin tapuya karışması, varlık tablosuna
konsaydı kaçınılmaz olurdu.

## Adlar

| Türkçe | İngilizce | Kısaltma |
|---|---|---|
| `KILAVUZ` | `GUIDE` | `KLV` |

## Sözdizimi

```text
KILAVUZ                                     → kılavuzları listeler
KILAVUZ yon=yatay|düşey deger=<mm>          → cetvel kılavuzu ekler
KILAVUZ yon=<açı> nokta=<n>                 → açılı kılavuz ekler
KILAVUZ yon=<açı> nokta=<n> tur=isin        → ışın ekler (tek yöne)
KILAVUZ yon=yatay deger=<mm> sil=evet       → siler
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `yon` | `yatay`, `düşey` ya da **bir açı** (`45`, `45g`, `30d`, `0.7r`); yoksa kılavuzlar listelenir |
| `deger` | Koordinat, **milimetre** — yatayda yukarı, düşeyde sağa. Yalnız cetvel kılavuzunda |
| `nokta` | Açılı kılavuzun **geçtiği nokta**. Yalnız `yon` bir açıysa; verilmezse sorulur |
| `tur` | `doğru` (varsayılan, iki yöne sonsuz) ya da `ışın` (noktadan ileriye). Yalnız açılı kılavuzda |
| `sil` | Verilen yerdeki kılavuzu siler |

Silme **yerini söyleyerek** yapılır, sıra numarasıyla değil: bir kılavuzu silmek
sonrakilerin sırasını kaydırır, yani biraz önce saydığınız numara Enter'a
bastığınızda başka bir çizgiyi gösterir. Yarım metre yakınındaki kılavuz silinir.

### Açı hangi kurala göre okunur

`yon` bir açıysa, oturumun **açı kuralı ve birimi** geçerlidir: varsayılan
**semt + grad**, yani kuzeyden saat yönünde grad. `yon=50` varsayılanda 50 grad
demektir; `yon=50d` 50 derece, `yon=0.7r` 0,7 radyan. Sonek ayarı **geçersiz
kılar** ve betiklerin kendini anlatabilmesi için vardır.

`@mesafe<açı` ne anlıyorsa `yon` da onu anlar — bir yerde grad, öbüründe derece
okuyan program yoktur ([`MOD`](mode.md), [`AYAR`](setting.md)).

Günlüğe **birimiyle** yazılır (`yon: "50.000000g"`): `50` yazan bir satır bugün
bir yönü, `core.aci.birim` değiştikten sonra başkasını gösterirdi.

### Doğru ile ışın

`tur=doğru` (varsayılan) iki yöne sonsuz bir çizgidir. `tur=isin` noktadan
**ileriye** gider: gerisinde çizgi yoktur, yani imleç noktanın arkasındayken
yakalama **noktanın kendisine** oturur — bir köşeden tek yöne çekilmiş kılavuzun
elde verdiği şey budur.

## Örnekler

### Komut satırı

```text
KILAVUZ yon=yatay deger=4310220500
KILAVUZ yon=düşey deger=485320000
KILAVUZ
```

```text
2 kılavuz:
  yatay  4310220,500 m
  düşey  485320,000 m
```

**Açılı kılavuz** — bir köşeden 50 grad (yani kuzeydoğuya 45°):

```text
KILAVUZ yon=50g nokta=485320,4310220
KILAVUZ yon=30d nokta=0,0 tur=isin
KILAVUZ
```

```text
3 kılavuz:
  yatay  4310220,500 m
  açılı  50,0000 grad  (485320,000 m, 4310220,000 m)
  açılı  33,3333 grad  (0,000 m, 0,000 m)  ışın
```

Listede açı **oturumun biriminde** yazılır: `yon=30d` ile girilen 30 derece,
varsayılan grad ayarında 33,3333 grad olarak okunur — aynı yön, başka birim.

Silmek:

```text
KILAVUZ yon=yatay deger=4310220500 sil=evet
```

### Arayüz

**Cetvelden sürükleyin.** Üst cetvelden aşağı çekmek yatay, sol cetvelden sağa
çekmek düşey kılavuz bırakır. Bıraktığınız yerde çizilir.

Vazgeçmek için cetvele **geri bırakın** — kılavuz konmaz.

**Açılı kılavuz için** komutu açıyla çalıştırın: `KILAVUZ yon=45g` yazın, program
kılavuzun geçtiği noktayı sorar, tuvalde tıklarsınız. Yakalama açıktır, yani
kılavuzu mevcut bir parsel köşesinden geçirebilirsiniz. Cetvelden sürüklemek
yalnız yatay ve düşey verir; açılı olan bir açı gerektirir ve açı bir sayıdır.

Kılavuza yakalanmak için `Shift+F3` listesinden **kılavuz** kipini açın. İki
kılavuz kesişiyorsa imleç **kesişime** oturur; tek kılavuza yakınsa üzerinde
kayar. Gerçek bir parsel köşesi her zaman kılavuzu yener — kılavuz kullanıcının
kendi çizdiği bir çizgidir, ölçülmüş bir nokta değildir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.guide", "args": { "yon": "yatay", "deger": 4310220500 } },
    { "cmd": "core.guide", "args": { "yon": "düşey", "deger": 485320000 } },
    { "cmd": "core.guide", "args": { "yon": "50g", "nokta": [485320000, 4310220000] } },
    { "cmd": "core.guide", "args": { "yon": "30d", "nokta": [0, 0], "tur": "isin" } }
  ]
}
```

## Geri alma

Tek adımdır. `GERİAL` eklediğiniz kılavuzu kaldırır, sildiğinizi geri getirir.

## Betikten kullanım

Kılavuzlar belgeye aittir, uygulamaya değil: bir betiğin koyduğu kılavuz dosyayla
gider ve dosyayı açan herkeste aynı yerdedir. Aplikasyon listesi hazırlayan bir
betik, aksları kılavuz olarak bırakabilir.

## Hatalar

> `Beklenen yön: yatay | düşey. Girilen: '<yön>'`

`yon` tanınmadı.

> `Kılavuzun koordinatı eksik. Örnek: KILAVUZ yon=yatay deger=4310220.5`

`yon` verildi ama `deger` verilmedi.

> `Orada <yön> kılavuz yok: <koordinat>`

`sil=evet` verildi ama o koordinatın yarım metre yakınında o yönde kılavuz yok.

## Dosya biçimi ve eski sürümler

Açılı kılavuz dosyaya **dört ek sütunla** yazılır (açı, geçtiği noktanın iki
koordinatı, ışın olup olmadığı) ve bu sütunlar **yalnız çizimde açılı kılavuz
varsa** yazılır. Yani yalnız cetvel kılavuzu taşıyan bir çizim, bu sürümden önceki
bir yapının yazdığı baytların aynısını yazar.

Açılı kılavuz taşıyan bir dosya **okuyucu sürümünü yükseltir**: eski bir yapı onu
"daha yeni bir sürüm gerekiyor" diye açıkça reddeder. Reddetmesinin sebebi şudur:
eksen sütunu eski bir sütundur ve `2` değeri yenidir, yani eski yapı o sütunu
bozuk sayar — doğru bir ret, yanıltıcı bir gerekçeyle. Sürümü yükseltmek, reddin
ne olduğunu söylemesini sağlar.

## İlgili

- [Arayüz — nesne yakalama](../baslangic/arayuz.md) — kılavuz kipini açma
- [MOD](mode.md) — yakalama maskesi
