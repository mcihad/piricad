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
KILAVUZ                                  → kılavuzları listeler
KILAVUZ yon=yatay|düşey deger=<mm>       → ekler
KILAVUZ yon=yatay deger=<mm> sil=evet    → siler
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `yon` | `yatay` ya da `düşey`; yoksa kılavuzlar listelenir |
| `deger` | Koordinat, **milimetre** — yatayda yukarı, düşeyde sağa |
| `sil` | Verilen yerdeki kılavuzu siler |

Silme **yerini söyleyerek** yapılır, sıra numarasıyla değil: bir kılavuzu silmek
sonrakilerin sırasını kaydırır, yani biraz önce saydığınız numara Enter'a
bastığınızda başka bir çizgiyi gösterir. Yarım metre yakınındaki kılavuz silinir.

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

Silmek:

```text
KILAVUZ yon=yatay deger=4310220500 sil=evet
```

### Arayüz

**Cetvelden sürükleyin.** Üst cetvelden aşağı çekmek yatay, sol cetvelden sağa
çekmek düşey kılavuz bırakır. Bıraktığınız yerde çizilir.

Vazgeçmek için cetvele **geri bırakın** — kılavuz konmaz.

Kılavuza yakalanmak için `Shift+F3` listesinden **kılavuz** kipini açın. İki
kılavuz kesişiyorsa imleç **kesişime** oturur; tek kılavuza yakınsa üzerinde
kayar. Gerçek bir parsel köşesi her zaman kılavuzu yener — kılavuz kullanıcının
kendi çizdiği bir çizgidir, ölçülmüş bir nokta değildir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.guide", "args": { "yon": "yatay", "deger": 4310220500 } },
    { "cmd": "core.guide", "args": { "yon": "düşey", "deger": 485320000 } }
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

## İlgili

- [Arayüz — nesne yakalama](../baslangic/arayuz.md) — kılavuz kipini açma
- [MOD](mode.md) — yakalama maskesi
