# DÖNDÜR — Nesne Döndürme

Bir pafta parçasını grid kuzeyine oturtan, yanlış açıyla ölçülmüş bir çizimi
düzelten herkes için; bu sayfayı bitirdiğinizde seçili nesneleri arayüzden, komut
satırından ve betikten döndürmeyi bileceksiniz.

## Ne yapar

`DÖNDÜR`, seçili nesneleri bir **merkez** etrafında verilen **açı** kadar
döndürür. Artı açı **saat yönünün tersinedir**, matematikteki gibi.

Dönme nesnenin alanını ve ölçülerini değiştirmez; yalnız yönünü değiştirir.

**Dik açılar tam çıkar.** 90, 180 ve 270 derece döndürmede köşeler tam yerlerine
oturur, yaklaşık değil: açı tam sayı mikro-derece olarak indirgenir ve sinüs ile
kosinüs eksen açılarında tam 0 ve tam 1 verir. Dört kez 90 derece döndürülen bir
parsel bit bit başladığı yere döner — bu programda test edilen bir davranıştır,
çünkü neredeyse dik olan bir açı bir dikdörtgeni bir milimetre eğik paralelkenara
çevirir.

## Adlar

| Ad | Tür |
|---|---|
| `DÖNDÜR` | Türkçe, birincil |
| `DONDUR` | ASCII karşılık |
| `ROTATE` | İngilizce karşılık |
| `DÖN` | Kısaltma |
| `core.rotate` | Komut kimliği |

## Sözdizimi

```text
DÖNDÜR
DÖNDÜR nesneler=<k> merkez=<n> aci=<derece>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Döndürülecek nesnelerin kimlikleri. Verilmezse etkin seçim |
| `merkez` | Döndürme merkezi; bu nokta yerinde kalır |
| `aci` | Dönme açısı, derece. Artı yön saat yönünün tersi |

## Örnekler

### Komut satırı

Çeyrek tur:

```text
SEÇ
DÖNDÜR merkez=485300,4310200 aci=90
```

Saat yönünde 30 derece — negatif açı:

```text
DÖNDÜR nesneler=1 merkez=0,0 aci=-30
```

### Arayüz

Nesneleri seçin, `DÖNDÜR` yazın, merkezi tıklayın, sonra açıyı yazın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.rotate",
      "args": { "nesneler": [1], "merkez": [0, 0], "aci": 90 } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`DÖNDÜR` tek bir geri alma adımıdır.

## Betikten kullanım

Betikten çağrıldığında `nesneler`, `merkez` ve `aci` verilmelidir. Açı derecedir,
radyan değil.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne belirtilmedi ve seçim boş. Örnek: ...` | Ne kimlik verildi ne seçim var | Nesneleri seçin ya da kimliklerini yazın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |

## İlgili

- [`TAŞI`](move.md) · [`KOPYALA`](copy.md) · [`ÖLÇEKLE`](scale.md) · [`AYNALA`](mirror.md)
- [`SEÇ`](select.md) · [`GERİAL`](undo.md)
