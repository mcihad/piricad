# KOORDİNAT — Nokta Koordinatı Okuma

## Ne yapar

Tıkladığınız noktanın **sağa** ve **yukarı** değerini, çizimin kendi koordinat
sisteminde yazar. Bir arazi çalışmasında ilk sorulan soru budur: burası neresi.

Çizimi değiştirmez. Bir okuma, bir düzenleme değildir — `GERİAL` listesine
girmez, günlüğe bir değişiklik olarak yazılmaz.

Okunan değer **belgenin** koordinat sistemindedir, ekranın değil. Görünümü ne
kadar yakınlaştırdığınızın okunan sayıya etkisi yoktur; yazdığınız defterdeki
değerle paftadaki değer aynıdır.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `KOORDİNAT` | `KOORDINAT` | `COORDINATE` | `KRD` |

## Sözdizimi

```text
KOORDİNAT [nokta=<sağa>,<yukarı>]
```

Nokta verilmezse komut sizden bir nokta tıklamanızı ister.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nokta` | Okunacak nokta; verilmezse tıklamanız istenir |

## Örnekler

### Komut satırı

```text
KOORDİNAT nokta=485320.5,4310220.25
```

```text
Sağa: 485320,500 m   Yukarı: 4310220,250 m   (TUREF/TM30)
```

### Arayüz

Sol araç kutusundaki **Koordinat Oku** düğmesine basın, sonra noktayı tıklayın.
Sonuç durum çubuğunda yazar; tam metni sağ paneldeki **Geçmiş** sekmesinde
bulursunuz.

Yakalama açıkken tıklamanız en yakın köşeye oturur, yani bir parsel köşesinin
gerçek koordinatını okursunuz — göz kararı bir noktanınkini değil.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.coordinate",
      "args": { "nokta": [485320500, 4310220250] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485320500` = 485 320,5 m).

## Geri alma

Geri alınacak bir şey yoktur: `KOORDİNAT` hiçbir şeyi değiştirmez. `GERİAL`
bundan bir önceki değişikliğe gider.

## Betikten kullanım

Salt okunur olduğu için bir betiğin herhangi bir yerinde, herhangi bir sayıda
çağrılabilir; işlem açmaz, geri alma yığınına dokunmaz. Çıktısı komut
günlüğündeki `echo` satırıdır.

## Hatalar

Nokta tıklanmadan `Esc`'e basarsanız komut sessizce biter — bu bir hata değildir,
vazgeçmedir.

Koordinat sistemi tanımsız bir belgede parantez içindeki sistem adı yazılmaz;
sayılar yine de okunur. Çizimi bir sisteme oturtmak için `AYAR
koordinat_sistemi=` kullanın.

## İlgili

- [ÖLÇ](measure.md) — iki nokta arası mesafe, koordinat farkı ve açı
- [ALANÖLÇ](measure_area.md) — seçili nesnelerin alanı ve çevresi
