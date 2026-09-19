# BAĞÇÖZ — Yazının Bağını Çözme

Bir yazının artık bağlı olduğu nesneyi izlemesini istemeyen herkes için; bu sayfayı
bitirdiğinizde bir yazıyı serbest bırakmayı ve bunun ne anlama geldiğini bileceksiniz.

## Ne yapar

`BAĞÇÖZ`, kapsamındaki **yazıların** bağını çözer: yazı yerinde ve sözünde kalır, ama
bağlı olduğu nesne bundan sonra **tek başına** taşınır, döndürülür ya da silinir.
[`UZUNLUKYAZ`](uzunluk_yaz.md), [`KÖŞENUMARALA`](kose_numarala.md) ve
[`BAĞLA`](bagla.md) ile kurulan bağ bu komutla kalkar.

Hiçbir nesneye bağlı olmayan bir yazı **atlanır ve sayılır**; hata değildir.

Bu bir [işlem aracıdır](../islem/README.md): kapsam ve tek geri alma adımı orada
anlatılır. Yerinde değiştiren bir araçtır; çıktı katmanı kullanmaz.

## Adlar

| Ad | Tür |
|---|---|
| `BAĞÇÖZ` | Türkçe, birincil |
| `BAGCOZ` | ASCII karşılık |
| `DETACH` | İngilizce karşılık |
| `BÇ`, `BC` | Kısaltma |
| `islem.bag_coz` | Komut kimliği |

## Sözdizimi

```text
BAĞÇÖZ
BAĞÇÖZ nesneler=<yazı>
BAĞÇÖZ kapsam=proje
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Bağı çözülecek yazıların kimlikleri; verilirse `kapsam` okunmaz |
| `kapsam` | `secili` (varsayılan), `gorunum` ya da `proje` |
| `pencere` | `gorunum` için görünümün iki köşesi |
| `katman` | Bu araçta kullanılmaz; yazı kendi katmanında kalır |

## Örnekler

### Komut satırı

Bir çizginin uzunluk yazısı serbest bırakılır; çizgi taşınır, yazı yerinde kalır:

```
KATMAN ad=YOL
ÇİZGİ 0,0 10,0
UZUNLUKYAZ nesneler=1
BAĞÇÖZ nesneler=2
TAŞI nesneler=1 baslangic=0,0 bitis=0,5
```

Çizimdeki bütün yazıların bağı çözülür:

```
BAĞÇÖZ kapsam=proje
```

### Arayüz

Sağ panelde **Araçlar ▸ Etiketleme ▸ Yazının bağını çöz**. Yazıları seçin ya da kapsamı
**Proje** yapın, **Çalıştır**'a basın. Kartın altındaki satır komut satırına yazılacak
olanın kendisidir.

### Betik

```json
{
  "ad": "Bağları çöz",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "YOL" } },
    { "cmd": "core.line",  "args": { "noktalar": [[0, 0], [10000, 0]] } },
    { "cmd": "islem.uzunluk_yaz", "args": { "nesneler": [1] } },
    { "cmd": "islem.bag_coz", "args": { "kapsam": "proje" } }
  ]
}
```

## Geri alma

`BAĞÇÖZ` **tek bir geri alma adımıdır**: [`GERİAL`](undo.md) bağları geri kurar.

## Betikten kullanım

Betikte `nesneler` ya da `kapsam` verilmelidir. Günlüğe bağı çözülen yazıların kimlikleri
yazılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Kapsamda bu araca uygun nesne yok …` | Kapsamda yazı yok | Yazıları seçin ya da kapsamı genişletin |
| `N yazı zaten hiçbir nesneye bağlı değildi.` | Bir not, hata değil: kapsamdaki serbest yazılar | Bir şey yapmanız gerekmez |
| `İşlem durduruldu; çizim değişmedi.` | Durdur'a basıldı | Yeniden çalıştırın |

## İlgili

- [Bağlı nesneler](../islem/bagli-nesneler.md)
- [`BAĞLA`](bagla.md) · [`UZUNLUKYAZ`](uzunluk_yaz.md) · [`KÖŞENUMARALA`](kose_numarala.md)
