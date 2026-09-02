# HALKA — Delikli Halka Çizme

## Ne yapar

Merkez, iç ve dış yarıçaptan **delikli bir halka** çizer: bir kuyunun koruma
bandı, bir nirengi noktasının etki yarıçapı, sabit genişlikte bir tampon.

Deliği gerçek bir deliktir — alan hesabında sayılmaz. 10 m dış, 5 m iç yarıçaplı
bir halkanın alanı 235,6 m²'dir, 314 m² değil.

İç ve dış noktayı **hangi sırayla** verdiğinizin önemi yoktur: küçük olan delik,
büyük olan dış sınırdır.

## Adlar

| Türkçe | İngilizce | Kısaltma |
|---|---|---|
| `HALKA` | `ANNULUS` | `HLK` |

## Sözdizimi

```text
HALKA merkez=<sağa>,<yukarı> ic=<sağa>,<yukarı> dis=<sağa>,<yukarı>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `merkez` | Halkanın merkezi |
| `ic` | İç çember üzerinde bir nokta |
| `dis` | Dış çember üzerinde bir nokta |

## Örnekler

### Komut satırı

```text
KATMAN ad=KORUMA
HALKA merkez=0,0 ic=5,0 dis=10,0
```

### Arayüz

**Çizim ▸ Halka**. Merkezi tıklayın, sonra iki çember noktasını. Her tıklamada
kılavuz o yarıçapın çemberini gösterir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer",   "args": { "ad": "KORUMA" } },
    { "cmd": "core.annulus",
      "args": { "merkez": [0,0], "ic": [5000,0], "dis": [10000,0] } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` halkayı deliğiyle birlikte kaldırır.

## Betikten kullanım

Üç noktası da argüman olarak verilebilir.

## Hatalar

> `İç yarıçap sıfır: bu bir halka değil, daire. DAİRE komutunu kullanın.`

`ic` ile `merkez` çakışıyor.

> `İki çember aynı: halkanın genişliği sıfır olamaz.`

`ic` ve `dis` aynı yarıçapta.

## İlgili

- [DAİRE](circle_draw.md) — deliksiz daire
- [DİLİM](sector.md) — daire dilimi
- [OFSET](offset.md) — herhangi bir şeklin paraleli
