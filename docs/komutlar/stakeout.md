# APLİKASYON — Aplikasyon Listesi

## Ne yapar

Bir istasyondan her noktaya **mesafe** ve **açı** listesi çıkarır. Tasarımı sahaya
geri götüren belge budur: alet bilinen bir noktaya kurulur, bilinen bir yöne
bağlanır ve operatör aplike edeceği her nokta için dönmesi gereken açıyı ve
ölçmesi gereken mesafeyi okur.

Çizimi değiştirmez.

## Bağlama verirseniz açı ondan ölçülür

Alet **bağlamaya** (arka görüşe) sıfırlanır, yani operatörün çevirdiği açı o
yönden itibarendir — kuzeyden değil.

- `baglama` **verilmezse**: açılar kuzeyden saat yönünde, yani **azimut**.
- `baglama` **verilirse**: açılar bağlama yönünden, yani **semt açısı**.

Rapor hangisi olduğunu her satırın üstünde yazar. Alet göreli açı okurken azimut
vermek, operatörü tripodun başında, güneşin altında kafadan çevirmeye
zorlardı — bir hatanın sınıra dönüştüğü yer tam orasıdır.

## Açı birimi

`açı_birimi` tercihine uyar, varsayılanı **grad**: Türkiye'de nirengi, poligon ve
aplikasyon hesapları gradla yürür ve tam daire 400'dür. `Seçenekler ▸ Genel`den
dereceye ya da radyana çevirebilirsiniz.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `APLİKASYON` | `APLIKASYON` | `STAKEOUT` | `APL` |

## Sözdizimi

```text
APLİKASYON istasyon=<Y>,<X> [baglama=<Y>,<X>] [nesneler=<kimlikler>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `istasyon` | Aletin durduğu nokta |
| `baglama` | Arka görüş noktası; verilirse açılar ondan ölçülür |
| `nesneler` | Aplike edilecek noktalar; yoksa seçim, o da boşsa çizimdeki bütün noktalar |

Seçim boşken **bütün noktalar** listelenir: aplikasyon isteyip hiçbir şey
seçmemek "hepsi" demektir.

Bir alan ya da çizgi seçiliyse **ilk köşesi** listeye girer — bir parseli aplike
etmek köşelerini aplike etmektir.

## Örnekler

### Komut satırı

```text
KATMAN ad=NIRENGI
NOKTALAR dosya="olcu.txt"
APLİKASYON istasyon=485320,4310220 baglama=485320,4310320
```

```text
Aplikasyon — istasyon 485320,000 / 4310220,000
  açılar bağlama yönünden (semt açısı)
  nokta        mesafe (m)        açı
  1                    40,000   100,0000 grad
  2                    56,569   150,0000 grad
  2 nokta.
```

### Arayüz

`APLİKASYON` yazın ve istasyonu tıklayın; bağlama isterseniz onu da. Liste sağ
paneldeki **Geçmiş** sekmesinde durur, son satırı durum çubuğunda.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer",    "args": { "ad": "NIRENGI" } },
    { "cmd": "core.points",   "args": { "dosya": "olcu.txt" } },
    { "cmd": "core.stakeout", "args": { "istasyon": [485320000, 4310220000] } }
  ]
}
```

## Geri alma

Geri alınacak bir şey yoktur: `APLİKASYON` çizimi değiştirmez ve geri alma
yığınına adım eklemez.

## Betikten kullanım

Salt okunur olduğu için bir betiğin herhangi bir yerinde çağrılabilir. Bir ölçü
klasörünü okuyup her istasyon için liste üreten bir betik yazılabilir.

## Hatalar

> `Aplike edilecek nokta yok. NOKTALAR ile bir liste okuyun, NOKTA ile çizin ya da nesne seçin.`

Çizimde nokta yok ve seçim boş.

> `Nesne bulunamadı veya silinmiş: <kimlik>`

`nesneler` içinde artık var olmayan bir kimlik var.

İstasyon tıklanmadan `Esc`'e basarsanız komut sessizce biter; bu bir hata değil,
vazgeçmedir.

## İlgili

- [NOKTALAR](points.md) — ölçü nokta listesi
- [ÖLÇ](measure.md) — iki nokta arası mesafe ve açı
- [KOORDİNAT](coordinate.md) — bir noktanın değeri
