# DİZİ — Nesne Çoğaltma Dizisi

Aynı yapıdan bir blok, bir otopark dolusu araç yeri ya da bir bacayı çevreleyen
radyal elemanlar üreten herkes için; bu sayfayı bitirdiğinizde diziyi arayüzden,
komut satırından ve betikten kullanmayı bileceksiniz.

## Ne yapar

`DİZİ`, seçili nesneleri düzenli olarak çoğaltır. İki kipi vardır.

**Satır/sütun dizisi** (öntanımlı): nesneleri bir ızgara halinde çoğaltır. Aynı
tipteki yapı adaları, otopark cepleri ya da bir ölçü ağının kazıkları böyle
üretilir.

**Kutupsal dizi** (`mod=KUTUPSAL`): nesneleri bir merkez etrafında döndürerek
çoğaltır. Bir rögar halkası, bir kavşağın radyal bordürleri ya da dairesel bir
yapının kolonları böyle üretilir.

Özgün nesne **yerinde kalır** ve sayıya dahildir: `sayi=4` toplam dört nesne
demektir, özgün artı üç kopya.

Kutupsal dizide **tam tur sayıya, kısmi tur boşluklara bölünür**. Tam turda on iki
kolon 30 derece aralıklıdır; çeyrek turda on iki kolon 90/11 derece aralıklıdır,
çünkü hem ilk hem son uçta birer nesne durur.

Kopyalarla birlikte **stil, yazı ve öznitelikler** de gider — [`KOPYALA`](copy.md)
ile aynı kurallar geçerlidir.

## Adlar

| Ad | Tür |
|---|---|
| `DİZİ` | Türkçe, birincil |
| `DIZI` | ASCII karşılık |
| `ARRAY` | İngilizce karşılık |
| `DZ` | Kısaltma |
| `core.array` | Komut kimliği |

## Sözdizimi

```text
DİZİ nesneler=<k> satir=<n> sutun=<m> satir_aralik=<d> sutun_aralik=<d>
DİZİ nesneler=<k> mod=KUTUPSAL merkez=<n> sayi=<n>
DİZİ nesneler=<k> mod=KUTUPSAL merkez=<n> sayi=<n> aci=<derece>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Dizilecek nesnelerin kimlikleri. Verilmezse etkin seçim |
| `mod` | `KUTUPSAL` ise kutupsal dizi; verilmezse satır/sütun dizisi |
| `satir` | Satır sayısı (satır/sütun dizisi) |
| `sutun` | Sütun sayısı (satır/sütun dizisi) |
| `satir_aralik` | Satır aralığı, metre. Artı yön kuzey |
| `sutun_aralik` | Sütun aralığı, metre. Artı yön doğu |
| `merkez` | Dizinin merkezi (kutupsal dizi) |
| `sayi` | Toplam nesne sayısı, özgün dahil (kutupsal dizi) |
| `aci` | Süpürülecek toplam açı, derece. Verilmezse tam tur |

## Örnekler

### Komut satırı

Üç satır, dört sütun; 20 m ve 15 m aralıklı:

```text
SEÇ
DİZİ satir=3 sutun=4 satir_aralik=20 sutun_aralik=15
```

Bir merkez etrafında sekiz kolon:

```text
DİZİ nesneler=1 mod=KUTUPSAL merkez=485300,4310200 sayi=8
```

Çeyrek tur boyunca üç eleman:

```text
DİZİ nesneler=1 mod=KUTUPSAL merkez=0,0 sayi=3 aci=90
```

### Arayüz

Nesneleri seçin, `DİZİ` yazın, sorulan değerleri girin.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.array",
      "args": { "nesneler": [1], "satir": 3, "sutun": 4,
                "satir_aralik": 20, "sutun_aralik": 15 } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`DİZİ` tek bir geri alma adımıdır: kaç kopya ürettiyse [`GERİAL`](undo.md) hepsini
birden kaldırır.

## Betikten kullanım

Betikten çağrıldığında `nesneler` ve kipin gerektirdiği parametreler verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne belirtilmedi ve seçim boş. ...` | Ne kimlik verildi ne seçim var | Nesneleri seçin |
| `Tek satır ve tek sütun bir dizi değildir; kopya üretilmedi.` | `satir=1 sutun=1` | En az birini artırın |
| `Satır ve sütun sayısı en az bir olmalı.` | Sıfır ya da negatif sayı | Artı sayı verin |
| `Kutupsal dizi en az iki nesne ister; N istendi.` | `sayi` birden küçük | En az 2 verin |

## İlgili

- [`KOPYALA`](copy.md) — tek kopya
- [`TAŞI`](move.md) · [`DÖNDÜR`](rotate.md)
