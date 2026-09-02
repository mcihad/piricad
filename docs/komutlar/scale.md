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
ÖLÇEKLE nesneler=<k> merkez=<n> carpan=<sayı>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Ölçeklenecek nesnelerin kimlikleri. Verilmezse etkin seçim |
| `merkez` | Ölçekleme merkezi; bu nokta yerinde kalır |
| `carpan` | Ölçek çarpanı; sıfırdan büyük olmalı |

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

### Arayüz

Nesneleri seçin, `ÖLÇEKLE` yazın, merkezi tıklayın, çarpanı yazın.

### Betik

```json
{
  "komutlar": [
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

Betikten çağrıldığında `nesneler`, `merkez` ve `carpan` verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne belirtilmedi ve seçim boş. Örnek: ...` | Ne kimlik verildi ne seçim var | Nesneleri seçin ya da kimliklerini yazın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Ölçek çarpanı sıfırdan büyük olmalı; aynalamak için AYNALA kullanın.` | Sıfır ya da negatif çarpan | Artı bir çarpan verin; aynalamak için [`AYNALA`](mirror.md) |
| `Ölçekleme daireyi sıfır yarıçapa indiriyor.` | Çarpan daireyi yok ediyor | Daha büyük bir çarpan verin |

## İlgili

- [`TAŞI`](move.md) · [`KOPYALA`](copy.md) · [`DÖNDÜR`](rotate.md) · [`AYNALA`](mirror.md)
