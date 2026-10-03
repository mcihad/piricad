# KOORDİNAT — Nokta Koordinatı Okuma

## Ne yapar

Tıkladığınız noktanın **sağa** ve **yukarı** değerini, çizimin kendi koordinat
sisteminde yazar. Bir arazi çalışmasında ilk sorulan soru budur: burası neresi.

Çizimi değiştirmez. Bir okuma, bir düzenleme değildir — `GERİAL` listesine
girmez, günlüğe bir değişiklik olarak yazılmaz.

Okunan değer **belgenin** koordinat sistemindedir, ekranın değil. Görünümü ne
kadar yakınlaştırdığınızın okunan sayıya etkisi yoktur; yazdığınız defterdeki
değerle paftadaki değer aynıdır.

Değer projenin **koordinat hassasiyetiyle** yazılır (`AYAR koordinat_hassasiyeti`,
varsayılan 3 ondalık = milimetre). İki ondalıkta `485320,155` okuması `485320,16` olarak
yazılır: yuvarlama tam sayılarla, yarımdan uzağa yapılır. Ayrıntı:
[Koordinat hassasiyeti](../veri/koordinat-sistemleri.md#koordinat-hassasiyeti).

## Adlar

| Ad | Tür |
|---|---|
| `KOORDİNAT` | Türkçe, birincil |
| `KOORDINAT` | ASCII karşılık |
| `XYZSOR` | Türkçe eş ad: Netcad'deki adı (XYZ Sor) |
| `COORDINATE` | İngilizce karşılık |
| `KRD` | Kısaltma |
| `core.coordinate` | Komut kimliği |

## Sözdizimi

```text
KOORDİNAT [nokta=<sağa>,<yukarı>] [sistem=<sistem>] [kaba=evet]
```

Nokta verilmezse komut sizden bir nokta tıklamanızı ister.

## Aynı noktayı başka bir sistemde okumak

`sistem=` noktayı ayrıca **başka bir koordinat sisteminde** de yazar: bir telefon haritası için
WGS 84 (`EPSG:4326`), yan dilimin sayıları, bir yabancı kurumun ayak sayan sistemi. Dönüşümü
**PROJ** yapar — birim ve datum kayması dahil — ve kullandığı işlemi ve doğruluğunu yazar:

```text
Sağa: 485320,000 m   Yukarı: 4310220,000 m   (EPSG:5254)
Boylam: 29,830714669°   Enlem: 38,925256696°   (EPSG:4326)
PROJ işlemi: Inverse of TUREF to WGS 84 (1) + 3-degree Gauss-Kruger CM 30E. Doğruluk: yaklaşık 1 m.
```

Bu bir **okumadır, dönüşüm değildir**: çizim kendi sisteminde kalır, hiçbir şey değişmez ve geri alınacak
bir şey olmaz. Çizimin tamamını başka bir sisteme taşımak [`DÖNÜŞTÜR`](reproject.md) işidir. Ayak sayan
bir sistemin birimi (`US survey foot` gibi) PROJ'un kendi adıyla yazılır. PROJ iki sistemi yalnız kaba
(*ballpark*) bir kaydırmayla bağlayabiliyorsa okuma reddedilir ve `kaba=evet` ile açıkça izin vermeniz
istenir.

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

Şeritte **Harita ▸ Sorgu ▸ Koordinat Oku**'ya basın (kapalı bir alan seçiliyken beliren
**Alan** sekmesinde de vardır), sonra noktayı tıklayın. Sonuç durum çubuğunda yazar; tam metni sağ paneldeki **Geçmiş** sekmesinde
bulursunuz. **Okuma tuvalde de kalır**: noktada küçük bir işaret ve yanında `Y … X …`
yazar. Birkaç noktayı arka arkaya okuyabilirsiniz; işaretler çizim değişince ya da
hiçbir komut çalışmıyorken Esc'e basınca silinir.

Yakalama açıkken tıklamanız en yakın köşeye oturur, yani bir parsel köşesinin
gerçek koordinatını okursunuz — göz kararı bir noktanınkini değil.

Başka bir sistemde okumak için (nokta önce çizime konur, ardından okunur):

```
KATMAN ad=PARSEL
KOORDİNAT nokta=485320,4310220 sistem=EPSG:5253
```

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

| Mesaj | Neden | Çözüm |
|---|---|---|
| `Koordinat sistemi tanınmıyor: 'X'. …` | `sistem=` PROJ'un tanımadığı bir ad | EPSG kodu (`EPSG:4326`), katalogdaki bir ad ya da PROJ/WKT tanımı yazın |
| `… dönüşümü kurulamadı: PROJ bu ikili için doğruluğu bilinen … işlem bulamadı …` | İki sistem yalnız kaba bir kaydırmayla ya da bu makinede olmayan bir grid ile bağlanıyor | Gridi PROJ veri dizinine koyun ya da sonucun metrelerce kayabileceğini bilerek `kaba=evet` yazın |
| `'YEREL' yerel bir sistem; haritadaki yeri bilinmediği için …` | Çizim `YEREL` ve `sistem=` istendi | Önce [`OTURT`](fit.md) ile ortak noktalardan haritaya bağlayın; `OTURT sistem=` gerçek sistemi de söyler |
| `Nokta … sistemlerinin geçerli alanının dışında; o sistemde okunamaz.` | Nokta iki sistemden birinin alanı dışında | Sistemleri denetleyin (yanlış dilim olağan nedendir) |

Koordinat sistemi tanımsız bir belgede parantez içindeki sistem adı yazılmaz;
sayılar yine de okunur. Çizimi bir sisteme oturtmak için `AYAR
koordinat_sistemi=` kullanın.

## İlgili

- [ÖLÇ](measure.md) — iki nokta arası mesafe, koordinat farkı ve açı
- [ALANÖLÇ](measure_area.md) — seçili nesnelerin alanı ve çevresi
