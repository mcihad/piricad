# AYNALA — Nesne Aynalama

Simetrik bir yapı adasını yarısından üreten, bir çizimi eksende yansıtan herkes
için; bu sayfayı bitirdiğinizde seçili nesneleri arayüzden, komut satırından ve
betikten aynalamayı bileceksiniz.

## Ne yapar

`AYNALA`, seçili nesneleri **iki noktadan geçen eksende** yansıtır.

**Yatay ve düşey eksen tamdır.** Bu iki eksende yansıtma bir tam sayı işaret
değişimidir, dolayısıyla hiçbir koordinat yuvarlanmaz. Bir surveyorun seçtiği
eksen çoğu zaman bunlardan biridir; genel formülden geçirmek tam cevabı olan bir
koordinatı yuvarlamak olurdu. Aynı eksende iki kez aynalanan nesne bit bit
başladığı yere döner.

**Sarım yönü düzeltilir.** Yansıma düzlemi ters çevirir, yani saat yönünün tersine
dolanan bir halka saat yönüne döner. Program köşe sırasını ters çevirerek bunu
geri alır — çünkü bir halkanın alanının işareti sarımının işaretidir ve yanlış
dolanan bir dış sınır **eksi alan** hesaplar. Alan hesabı hukuki çıktıdır.

Yayın süpürme yönü de çevrilir: yansıtılan yay artık öteki yönde süpürür ve iki
ucu takas edilir.

## Adlar

| Ad | Tür |
|---|---|
| `AYNALA` | Türkçe, birincil |
| `MIRROR` | İngilizce karşılık |
| `AYN` | Kısaltma |
| `core.mirror` | Komut kimliği |

## Sözdizimi

```text
AYNALA
AYNALA nesneler=<k> baslangic=<n> bitis=<n>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Aynalanacak nesnelerin kimlikleri. Verilmezse etkin seçim |
| `baslangic` | Ayna ekseninin ilk noktası |
| `bitis` | Ayna ekseninin ikinci noktası |

## Örnekler

### Komut satırı

Düşey eksende yansıtın:

```text
SEÇ
AYNALA baslangic=485300,4310200 bitis=485300,4310250
```

Yatay eksende:

```text
AYNALA nesneler=1 baslangic=0,0 bitis=100,0
```

### Arayüz

Nesneleri seçin, `AYNALA` yazın, ayna ekseninin iki noktasını tıklayın.
İki tıklama arasında eksen kesikli çizgiyle gösterilir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.mirror",
      "args": { "nesneler": [1],
                "baslangic": [0, 0], "bitis": [0, 100000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`AYNALA` tek bir geri alma adımıdır. Aynı eksende ikinci kez uygulamak da
nesneyi geri getirir.

## Betikten kullanım

Betikten çağrıldığında `nesneler`, `baslangic` ve `bitis` verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne belirtilmedi ve seçim boş. Örnek: ...` | Ne kimlik verildi ne seçim var | Nesneleri seçin ya da kimliklerini yazın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Ayna ekseni tek noktadan geçemez; iki farklı nokta verin.` | İki nokta çakışık | Eksenin ikinci noktasını başka bir yere verin |

## İlgili

- [`TAŞI`](move.md) · [`KOPYALA`](copy.md) · [`DÖNDÜR`](rotate.md) · [`ÖLÇEKLE`](scale.md)
