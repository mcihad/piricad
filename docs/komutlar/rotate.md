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

**Yazılar ve ölçüler okunur kalır.** Bir yazı nesneyle birlikte döner; bir ölçünün
yazısı ise yarım tur dönse de baş aşağı durmaz, soldan sağa okunacak biçimde
yeniden yerleşir ve sayısı değişmez.

### Referansla

`yontem=referans` çizimdeki bir doğrultuyu — iki noktayla gösterilen ya da
`referans=` ile yazılan açı — yeni bir doğrultuya döndürür; dönme açısı ikisinin
farkıdır. Bir krokideki bina duvarını ölçülen doğrultuya getirmek için açıyı
hesaplamanız gerekmez: duvarın iki ucunu, sonra yeni doğrultuyu gösterin. Günlüğe
bulunan dönme açısı yazılır.

### Kopyalayarak

`kopya=evet` nesnelerin kendisini değil kopyasını döndürür; özgün yerinde kalır.

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
DÖNDÜR nesneler=<k> merkez=<n> aci=<derece> [kopya=evet]
DÖNDÜR nesneler=<k> merkez=<n>              # açıyı fareyle gösterirsiniz
DÖNDÜR nesneler=<k> merkez=<n> referans=<derece> aci=<yeni derece>
DÖNDÜR nesneler=<k> merkez=<n> yontem=referans  # referans iki noktayla gösterilir
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Döndürülecek nesnelerin kimlikleri. Verilmezse etkin seçim |
| `merkez` | Döndürme merkezi; bu nokta yerinde kalır |
| `aci` | Dönme açısı, derece. Artı yön saat yönünün tersi. Verilmezse sorulur |
| `aci_nokta` | Açının gösterildiği nokta; `aci` verilmişse sorulmaz |
| `yontem` | `referans`: bir doğrultu yenisine döndürülür |
| `referans` | Referans doğrultunun açısı, derece; `aci` onun yeni açısıdır |
| `referans_nokta` | Referans doğrultuyu gösteren iki nokta |
| `kopya` | `evet`: kopya döndürülür, özgün yerinde kalır |

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

Yeni bir çizimde, 30° doğrultusundaki bir duvarı kuzeye (90°) çevirin — 60° döner:

```text
ÇİZGİ 10,0 20,0
DÖNDÜR nesneler=1 merkez=0,0 referans=30 aci=90
```

### Arayüz

Nesneleri seçin, araç kutusundaki **Taşı** düğmesini basılı tutup karttan
**Döndür**'ü seçin (ya da `DÖNDÜR` yazın), merkezi tıklayın — ve sonra **açıyı
fareyle gösterin**.

**Nesneler imlecin altında döner.** Hayalet, komutun uygulayacağı dönüşümün
kendisiyle çizilir (`core::transformed`), yani gördüğünüz duruş tıklayınca
oluşacak duruştur. İmlecin merkeze göre doğrultusu açıdır: doğuya doğru 0
derece, kuzeye doğru 90. Okumayı imlecin yanındaki yazı söyler.

Açıyı yazmak isterseniz `aci=` verin; o zaman komut hiçbir şey sormaz.

**Döndür — referansla** aynı karttadır: merkezi, sonra referans doğrultunun iki
noktasını gösterin; nesneler imleçle döner, referans doğrultu imlecin doğrultusuna
gelecek biçimde. Tıklayın ya da yeni açıyı yazın.

### Betik

Betik önce 10 m bir çizgi çizer, sonra onu başlangıç noktası çevresinde 90 derece
döndürür:

```json
{
  "komutlar": [
    { "cmd": "core.line", "args": { "noktalar": [[0,0],[10000,0]] } },
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

Betikten çağrıldığında `nesneler`, `merkez` ve `aci` verilmelidir; referansla
döndürmede `referans` da. Açı derecedir, radyan değil. Günlüğe her zaman bulunan dönme
açısı yazılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne belirtilmedi ve seçim boş. Örnek: ...` | Ne kimlik verildi ne seçim var | Nesneleri seçin ya da kimliklerini yazın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |

## İlgili

- [`TAŞI`](move.md) · [`KOPYALA`](copy.md) · [`ÖLÇEKLE`](scale.md) · [`AYNALA`](mirror.md)
- [`SEÇ`](select.md) · [`GERİAL`](undo.md)
