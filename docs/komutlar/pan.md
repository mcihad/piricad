# KAYDIR — Görünümü Kaydırma

## Ne yapar

Çizimi tutup başka bir yere taşır. Ölçek değişmez: yakınlaştırma neyse o kalır,
yalnız pencerenin baktığı yer kayar.

İki nokta ister. Birincisi **tuttuğunuz** noktadır, ikincisi o noktanın
**gideceği** yer. Kâğıdı masada kaydırmak gibidir.

Çizimi değiştirmez — bir görünüm komutudur. `GERİAL` listesine girmez.

**Saydam** bir komuttur: çalışan bir komutu bölebilir. `ÇİZGİ` çizerken görünümü
kaydırıp kaldığınız yerden devam edebilirsiniz.

## Adlar

| Türkçe | İngilizce | Kısaltma |
|---|---|---|
| `KAYDIR` | `PAN` | `KY` |

## Sözdizimi

```text
KAYDIR [baslangic=<sağa>,<yukarı>] [bitis=<sağa>,<yukarı>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `baslangic` | Tutulacak nokta |
| `bitis` | O noktanın taşınacağı yer |

## Örnekler

### Komut satırı

```text
KAYDIR baslangic=485300,4310200 bitis=485400,4310200
```

Çizim 100 m sola kayar — tuttuğunuz nokta 100 m sağa gittiği için.

### Arayüz

Sol araç kutusundaki **Kaydır** düğmesine basın, sonra iki nokta tıklayın.
Aradaki kılavuz ne kadar kaydıracağınızı gösterir.

Fare orta tuşuyla sürüklemek de aynı işi yapar ve komut istemez; orta tuşu olmayan
bir işaret aygıtı için düğme ve komut vardır.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.pan",
      "args": { "baslangic": [485300000, 4310200000],
                "bitis": [485400000, 4310200000] } }
  ]
}
```

Betikte koordinatlar **milimetredir**.

## Geri alma

Geri alınacak bir şey yoktur: `KAYDIR` çizimi değiştirmez. Önceki görünüme dönmek
için `YAKINLAŞ SIFIRLA` ya da `YAKINLAŞ KAPSAM` kullanın.

## Betikten kullanım

Salt okunur olduğu için işlem açmaz. Başsız çalışmada (görünüm istemcisi yokken)
"Görünüm istemcisi bağlı değil" yazar ve hiçbir şey yapmaz — bir betiğin başsız
tekrarı bu yüzden kırılmaz.

## Hatalar

İlk noktadan önce `Esc`'e basarsanız komut sessizce biter; bu bir hata değildir.

> `Görünüm istemcisi bağlı değil (başsız çalışma).`

Ekransız çalıştırıldınız. Komutun kaydıracağı bir görünüm yok.

## İlgili

- [YAKINLAŞ](zoom.md) — ölçeği değiştirir
- [KOORDİNAT](coordinate.md) — bir noktanın değerini okur
