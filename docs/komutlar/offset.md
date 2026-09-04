# OFSET — Paralel Çizme

## Ne yapar

Seçili nesnelerin verilen mesafede **paralelini** çizer. Yol şeridini eksenden,
çekme mesafesini parsel sınırından, dere koruma bandını dere ekseninden bu komutla
çıkarırsınız.

**Aslını yerinde bırakır.** Paralel yeni bir nesnedir; sınır ölçülmüş olandır ve
oynatılmaz.

Eksi mesafe içeri, artı mesafe dışarı gider. Açık bir çizginin ofseti, çizginin
iki yanını saran **kapalı bir bant**tır.

### Kapanan şekiller

Bir parseli kendi genişliğinin yarısından fazla içeri ofsetlerseniz geriye bir şey
kalmaz. Komut bunu söyler ve hiçbir şey çizmez:

> `Bu mesafede paralel kalmıyor: şekil kendi içinde kapanıyor.`

Bel veren bir şekli içeri ofsetlemek onu **ikiye bölebilir**; o zaman iki paralel
birden çizilir. İkisi de doğru cevaptır ve elle yazılmış bir ofsetin yanlış
yaptığı yer tam burasıdır.

### Neden Clipper2

Ofset çözülmüş bir problemdir ve CLAUDE.md 5.16 çözülmüş bir problemi yeniden
yazmayı yasaklar. "Her kenarı yana kaydır ve yeniden kesiştir" yaklaşımı ters
köşelerde yanlış sonuç verir, içbükey girdide kendini kesen halkalar üretir ve
keskin bir köşede sınırsız sivrilir. Clipper2 üçünü de yirmi yıldır doğru yapıyor
ve BSL-1.0 lisansı GPLv3 ile uyumlu.

## Adlar

| Türkçe | İngilizce | Kısaltma |
|---|---|---|
| `OFSET` | `OFFSET` | `OF` |

## Sözdizimi

```text
OFSET [nesneler=<kimlikler>] [mesafe=<mm>] [kose=KÖŞE|YUVARLAK|PAH]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Ofseti alınacak nesneler; verilmezse etkin seçim |
| `mesafe` | Ofset mesafesi, **milimetre**; eksi değer içeri |
| `kose` | Dış köşenin biçimi: `KÖŞE` (varsayılan), `YUVARLAK`, `PAH` |

Mesafe verilmezse komut sizden **metre** cinsinden ister — komut satırında metre
konuşulur, argümanda milimetre yazılır.

## Örnekler

### Komut satırı

```text
KATMAN ad=PARSEL
ALAN noktalar=0,0 10,0 10,10 0,10
SEÇ nesneler=1
OFSET mesafe=1000
```

```text
1 paralel çizildi (1 m).
```

5 metre içeri çekme mesafesi:

```text
OFSET nesneler=1 mesafe=-5000
```

### Arayüz

Nesneleri seçin, sol araç kutusundaki **Ofset** düğmesine basın, mesafeyi metre
olarak yazın ve Enter'a basın.

Seçim boşken de çalışır: düğmeye basın, komut satırı hangi nesneleri istediğini
yazar, tuvalden tıklayarak seçin ve **Enter**'a basın. Vazgeçmek için Esc.
Nesneleri önceden seçtiyseniz sorulmaz.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer",  "args": { "ad": "PARSEL" } },
    { "cmd": "core.area",   "args": { "noktalar": [[0,0],[10000,0],[10000,10000],[0,10000]] } },
    { "cmd": "core.offset", "args": { "nesneler": [1], "mesafe": 1000 } }
  ]
}
```

## Geri alma

Tek adımdır: bir `OFSET` kaç paralel çizmiş olursa olsun tek `GERİAL` hepsini
kaldırır.

## Betikten kullanım

`nesneler` verilirse seçime dokunmaz, yani bir betik seçim kurmadan çalışabilir.
Herhangi bir nesne bulunamazsa **hiçbiri** çizilmez — işlem bütünüyle geri alınır.

## Hatalar

> `Ofseti alınacak nesne yok. Önce seçin, ya da OFSET nesneler=1 yazın.`

Seçim boş ve `nesneler` verilmedi.

> `Ofset mesafesi sıfır olamaz. Kaç metre paralel istediğinizi yazın.`

Sıfır ofset, girdinin kopyasıdır; değeri okumayı unutmuş olma ihtimali yüksek
olduğu için reddedilir.

> `Bu mesafede paralel kalmıyor: şekil kendi içinde kapanıyor.`

İçeri ofset şeklin genişliğini aştı. Daha küçük bir mesafe deneyin.

> `Nesne bulunamadı veya silinmiş: <kimlik>`

`nesneler` içinde artık var olmayan bir kimlik var.

## İlgili

- [ALAN](area.md) — kapalı alan çizme
- [BUDA](trim.md) — fazlalığı kesme
