# ELİPS — Elips Çizme

## Ne yapar

Merkez ve iki eksenden elips çizer. İkinci eksen **her zaman birincisine diktir**.

Üç nokta ister: merkez, birinci eksenin ucu, ve ikinci eksenin uzaklığı. Üçüncü
nokta serbest tıklanır ama yalnız **birinci eksene dik bileşeni** sayılır — eksen
boyunca ne kadar uzağa tıkladığınızın önemi yoktur.

Bu kısıt bilerektir: ikinci eksen serbest bırakılsaydı kullanıcı elips olmayan,
kaydırılmış bir şekil çizebilirdi ve kayıtta hiçbir elipsin sahip olmadığı iki
eksen dururdu — aşağıdaki hiçbir şey onu çizemezdi.

### Tanımıyla saklanır

`DAİRE` ve `YAY` gibi elips de **tanımıyla** saklanır: merkez ve iki eksen ucu,
beş sayı. Çizilen çok kenarlı hat yalnız görüntüdür.

Eksen uçları **nokta** olarak saklanır, uzunluk ve açı olarak değil: iki eksen
**vektörü** dönüklüğü zaten taşır, yani hiçbir yerde açı saklanmaz ve okunmaz.

Çizilen hat, `DAİRE`'nin kullandığı **aynı** birim çember tablosundan gelir — dört
tam eksen noktasından tekrarlı bölmeyle kurulmuş tablo — iki eksen boyunca
ölçeklenerek. Çarpma ve toplamadan başka işlem yoktur, yani üç platformda aynı
köşeler çıkar (§7.3).

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `ELİPS` | `ELIPS` | `ELLIPSE` | `EL` |

## Sözdizimi

```text
ELİPS merkez=<sağa>,<yukarı> birinci=<sağa>,<yukarı> ikinci=<sağa>,<yukarı>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `merkez` | Elipsin merkezi |
| `birinci` | Birinci eksenin ucu |
| `ikinci` | İkinci eksenin uzaklığı; **birinci eksene dik** ölçülür |

## Örnekler

### Komut satırı

```text
KATMAN ad=CIZIM
ELİPS merkez=0,0 birinci=10,0 ikinci=0,5
```

10 m ve 5 m yarı eksenli, doğu-batı yönünde bir elips. Alanı π·10·5 = 157,08 m².

Döndürülmüş:

```text
ELİPS merkez=0,0 birinci=10,10 ikinci=-1,1
```

### Arayüz

**Çizim ▸ Elips**. Merkezi tıklayın, birinci eksenin ucunu, sonra ikinci eksenin
uzaklığını.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "CIZIM" } },
    { "cmd": "core.ellipse_draw",
      "args": { "merkez": [0,0], "birinci": [10000,0], "ikinci": [0,5000] } }
  ]
}
```

## Geri alma

Tek adımdır: `GERİAL` elipsi kaldırır.

## Betikten kullanım

Üç noktası da argüman olarak verilebilir. Günlüğe **verdiğiniz** üç nokta yazılır,
komutun onlardan türettiği dik eksen değil: bir tekrar aynı aritmetiği yürütür,
bu çalıştırmanın yuvarladığı bir sayıya güvenmez.

## Hatalar

> `Birinci eksenin ucu merkezle aynı yerde; elipsin ekseni sıfır olamaz.`

`birinci` ile `merkez` çakışıyor.

> `İkinci eksen sıfır: üçüncü nokta birinci eksenin üzerinde. Eksene dik bir yer seçin.`

Üçüncü nokta birinci eksenin doğrusu üzerinde; dik bileşeni sıfır.

## İlgili

- [DAİRE](circle_draw.md) — elipsin eksenleri eşit hâli
- [YAY](arc_draw.md) — kapatmayan eğri
- [HALKA](annulus.md) — delikli halka
