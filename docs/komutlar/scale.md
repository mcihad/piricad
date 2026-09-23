# ÖLÇEKLE — Nesne Ölçekleme

Yanlış ölçekte gelmiş bir çizimi düzelten, bir eskizi gerçek ölçüsüne getiren
herkes için; bu sayfayı bitirdiğinizde seçili nesneleri arayüzden, komut
satırından ve betikten ölçeklemeyi bileceksiniz.

## Ne yapar

`ÖLÇEKLE`, seçili nesneleri bir **merkeze** göre **çarpan** kadar büyütür ya da
küçültür. Merkez noktası yerinde kalır.

Uzunluklar çarpanla, **alanlar çarpanın karesiyle** değişir: 2 çarpanı çevreyi iki
katına, alanı dört katına çıkarır. Dairenin ve yayın yarıçapı da ölçeklenir.

Çarpan **sıfırdan büyük olmalıdır**. Negatif çarpan reddedilir; sessizce yarım tur
dönmeye çevrilmez. "Eksi birle ölçekle" ile "aynala" farklı niyetlerdir ve yanlış
işaret yazan kullanıcıya itaat etmek yerine söylemek gerekir — aynalamak için
[`AYNALA`](mirror.md) vardır.

**Yazılar da büyür**: bir yazının harf yüksekliği çarpanla ölçeklenir, ölçünün
yazısı yeni uzunluğu söyler ve harfleri, okları büyür. Bloklar ölçeklerini
çarpanla taşır.

### Referansla

`yontem=referans` çizimdeki bir uzunluğu — iki noktayla gösterilen ya da
`referans=` ile yazılan — yeni bir uzunluğa getirir; çarpan ikisinin oranıdır. Bir
kenarı sahada ölçülmüş taranmış bir krokiyi gerçek ölçüsüne getirmenin yolu budur:
kenarın iki ucunu gösterin, sonra yeni uzunluğu yazın ya da merkezden o uzaklıkta
bir yeri gösterin. Günlüğe bulunan çarpan yazılır.

### İki çarpanla

`carpan_y=` verilirse `carpan` yalnız **sağa**, `carpan_y` **yukarı** yöndeki
çarpandır: eşit olmayan ölçek.

| Nesne | Ne olur |
|---|---|
| Çizgi, alan, spline, tarama sınırı | Köşeleri iki çarpanla taşınır |
| Daire | **Aynı kimlikle elips olur**; eksenleri yeniden dik, uzunu birinci eksen |
| Yay | Aynı kimlikle **eliptik yay** olur |
| Elips | Elips kalır; eksenleri yeniden dik bulunur |
| Blok referansı | Dik duruyorsa (0°, 90°, 180°, 270°) ölçekleri çarpanlarla değişir; başka bir açıyla dönükse eğileceği için **reddedilir** |
| Yaylı çoklu çizgi | Yayları eliptik olacağı için **reddedilir**; önce [`PATLAT`](explode.md) ile ayırın |
| Yazı | Yeri ve doğrultusu değişir, **harf yüksekliği korunur** |
| Ölçü | Yeniden ölçülür; yazı boyu ve ok boyu korunur |

### Kopyalayarak

`kopya=evet` nesnelerin kendisini değil kopyasını ölçekler; özgün yerinde kalır.

## Adlar

| Ad | Tür |
|---|---|
| `ÖLÇEKLE` | Türkçe, birincil |
| `OLCEKLE` | ASCII karşılık |
| `SCALE` | İngilizce karşılık |
| `core.scale` | Komut kimliği |

## Sözdizimi

```text
ÖLÇEKLE
ÖLÇEKLE nesneler=<k> merkez=<n> carpan=<sayı> [kopya=evet]
ÖLÇEKLE nesneler=<k> merkez=<n>               # çarpanı fareyle gösterirsiniz
ÖLÇEKLE nesneler=<k> merkez=<n> carpan=<sağa> carpan_y=<yukarı>
ÖLÇEKLE nesneler=<k> merkez=<n> referans=<m> yeni=<m>
ÖLÇEKLE nesneler=<k> merkez=<n> yontem=referans   # referans iki noktayla gösterilir
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Ölçeklenecek nesnelerin kimlikleri. Verilmezse etkin seçim |
| `merkez` | Ölçekleme merkezi; bu nokta yerinde kalır |
| `carpan` | Ölçek çarpanı; sıfırdan büyük olmalı. Verilmezse sorulur |
| `carpan_nokta` | Çarpanın gösterildiği nokta; `carpan` verilmişse sorulmaz |
| `carpan_y` | Yukarı yöndeki çarpan; verilirse `carpan` yalnız sağa yöndekidir |
| `yontem` | `referans`: bir uzunluk yenisine ölçeklenir |
| `referans` | Referans uzunluk, metre |
| `yeni` | Referans uzunluğun yeni değeri, metre |
| `referans_nokta` | Referans uzunluğu gösteren iki nokta |
| `kopya` | `evet`: kopya ölçeklenir, özgün yerinde kalır |

## Örnekler

### Komut satırı

İki katına büyütün:

```text
SEÇ
ÖLÇEKLE merkez=0,0 carpan=2
```

1/1000 ölçekli gelmiş bir çizimi gerçek boyuta getirin:

```text
ÖLÇEKLE nesneler=1 merkez=0,0 carpan=1000
```

Yeni bir çizimde, 5 m çizilmiş bir kenarı sahada ölçülen 12 m'ye getirin:

```text
ÇİZGİ 0,0 5,0
ÖLÇEKLE nesneler=1 merkez=0,0 referans=5 yeni=12
```

Yeni bir çizimde, bir daireyi yukarı yönde iki katına çekip elips yapın:

```text
DAİRE merkez=0,0 cevre=10,0
ÖLÇEKLE nesneler=1 merkez=0,0 carpan=1 carpan_y=2
```

### Arayüz

Nesneleri seçin, araç kutusundaki **Taşı** düğmesini basılı tutup karttan
**Ölçekle**'yi seçin (ya da `ÖLÇEKLE` yazın), merkezi tıklayın — ve sonra
**çarpanı fareyle gösterin**.

**Nesneler imlecin altında büyür.** Hayalet, komutun uygulayacağı dönüşümün
kendisiyle çizilir, yani gördüğünüz boy tıklayınca oluşacak boydur. Çarpan,
imlecin merkeze **metre** cinsinden uzaklığıdır: iki metre dışarısı iki kat,
yarım metre dışarısı yarısı. Her CAD sürüklenen ölçeği böyle okur.

Çarpanı yazmak isterseniz `carpan=` verin; o zaman komut hiçbir şey sormaz.

**Ölçekle — referansla** aynı karttadır: merkezi, sonra referans uzunluğun iki
ucunu gösterin; nesneler imleçle, imlecin merkeze uzaklığı yeni uzunluk olacak
biçimde büyür. Tıklayın ya da yeni uzunluğu yazın.

### Betik

Betik önce 10 m bir çizgi çizer, sonra onu başlangıç noktasına göre iki katına
büyütür:

```json
{
  "komutlar": [
    { "cmd": "core.line", "args": { "noktalar": [[0,0],[10000,0]] } },
    { "cmd": "core.scale",
      "args": { "nesneler": [1], "merkez": [0, 0], "carpan": 2 } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`ÖLÇEKLE` tek bir geri alma adımıdır.

## Betikten kullanım

Betikten çağrıldığında `nesneler`, `merkez` ve `carpan` (ya da `referans` ile `yeni`)
verilmelidir. Günlüğe her zaman bulunan `carpan` yazılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne belirtilmedi ve seçim boş. Örnek: ...` | Ne kimlik verildi ne seçim var | Nesneleri seçin ya da kimliklerini yazın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Ölçek çarpanı sıfırdan büyük olmalı; aynalamak için AYNALA kullanın.` | Sıfır ya da negatif çarpan | Artı bir çarpan verin; aynalamak için [`AYNALA`](mirror.md) |
| `Ölçekleme daireyi sıfır yarıçapa indiriyor.` | Çarpan daireyi yok ediyor | Daha büyük bir çarpan verin |
| `Referans uzunluk sıfırdan büyük olmalı; …` | Referansın iki noktası aynı | İki farklı nokta verin |
| `Nesne N yaylı bir çoklu çizgi: eşit olmayan ölçek …` | Yaylı çoklu çizgiye iki çarpan | Eşit ölçek kullanın ya da önce [`PATLAT`](explode.md) |
| `Nesne N döndürülmüş bir blok: eşit olmayan ölçek onu eğer …` | Açılı bir bloğa iki çarpan | Eşit ölçek kullanın ya da önce [`PATLAT`](explode.md) |

## İlgili

- [`TAŞI`](move.md) · [`KOPYALA`](copy.md) · [`DÖNDÜR`](rotate.md) · [`AYNALA`](mirror.md)
