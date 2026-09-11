# KÖŞENUMARALA — Köşe Numaralama

Parsel köşelerini paftada ve koordinat çizelgesinde aynı sırayla numaralamak isteyen
herkes için; bu sayfayı bitirdiğinizde numaralamayı hangi köşeden başlatacağınızı, hangi
yöne saydıracağınızı ve numaranın biçimini vermeyi bileceksiniz.

## Ne yapar

`KÖŞENUMARALA`, kapsamındaki her **alanın** köşelerini seçtiğiniz köşeden başlayarak
sırayla numaralar ve her numarayı köşenin **dışına**, köşenin açıortayı boyunca yazar;
yazı yataydır. Açık **çizgiler** de numaralanır: sayım başlangıç noktasına yakın uçtan
başlar. Ürettiği her numara sıradan bir [`METİN`](text.md) nesnesidir.

Numaranın biçimi sizindir: önek, en az basamak sayısı ve dolgu, ilk numara, sonek. `A`
öneki, 5 basamak ve `0` dolgusu `A00001, A00002, …` verir.

Bu bir [işlem aracıdır](../islem/README.md): kapsam, asenkron çalışma, Durdur, tek geri
alma adımı ve çıktı katmanı orada anlatılır.

## Adlar

| Ad | Tür |
|---|---|
| `KÖŞENUMARALA` | Türkçe, birincil |
| `KOSENUMARALA` | ASCII karşılık |
| `NUMBERVERTICES` | İngilizce karşılık |
| `KNM` | Kısaltma |
| `islem.kose_numarala` | Komut kimliği |

## Sözdizimi

```text
KÖŞENUMARALA
KÖŞENUMARALA nesneler=<k> baslangic=<nokta> onek=A basamak=5
KÖŞENUMARALA kapsam=proje yon=saat ilk=1 katman=NUMARA
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Uygulanacak nesnelerin kimlikleri; verilirse `kapsam` okunmaz |
| `kapsam` | `secili` (varsayılan), `gorunum` ya da `proje` |
| `pencere` | `gorunum` için görünümün iki köşesi |
| `katman` | Numaraların gideceği katman; yoksa oluşturulur |
| `baslangic` | Sayımın başlayacağı köşeye en yakın nokta; verilmezse ilk köşe |
| `yon` | `ters` (saat yönünün tersi, varsayılan) ya da `saat` |
| `onek` | Numaranın önü: `A`, `K-` |
| `basamak` | En az basamak sayısı, 0–12; eksikler `dolgu` ile tamamlanır |
| `dolgu` | Dolgu karakteri; varsayılan `0` |
| `ilk` | İlk köşenin numarası; varsayılan 1 |
| `sonek` | Numaranın arkası: `"."` (nokta tırnak içinde yazılır; tırnaksız nokta sayı okunur) |
| `yukseklik` | Yazı yüksekliği, zemin milimetresi; `0` = plan ölçeğinde 2,5 mm |
| `bosluk` | Köşe ile yazı arası, milimetre; `0` = yüksekliğin yarısı |

## Örnekler

### Komut satırı

Seçili parsel, kuzeydoğu köşesinden başlayarak, `A00001` biçiminde:

```text
SEÇ
KÖŞENUMARALA baslangic=485360,4310250 onek=A basamak=5
```

Saat yönünde, 7'den başlayarak, noktayla biten numaralar, ayrı katmana:

```text
KÖŞENUMARALA nesneler=1 yon=saat ilk=7 sonek="." katman=NUMARA
```

### Arayüz

Sağ panelde **Araçlar ▸ Etiketleme ▸ Köşeleri numarala**. Başlangıç noktasını `x,y`
olarak yazın ya da boş bırakın, öneki ve basamak sayısını verin, **Çalıştır**'a basın.
Kartın altındaki satır komut satırına yazılacak olanın kendisidir.

### Betik

```json
{
  "ad": "Köşe numaraları",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.area",  "args": { "noktalar": [[485320000,4310220000],[485360000,4310220000],
                                                  [485360000,4310250000],[485320000,4310250000]] } },
    { "cmd": "islem.kose_numarala",
      "args": { "nesneler": [1], "baslangic": [485360000, 4310250000],
                "onek": "A", "basamak": 5, "katman": "NUMARA" } }
  ]
}
```

## Geri alma

`KÖŞENUMARALA` **tek bir geri alma adımıdır**: [`GERİAL`](undo.md) bütün numaraları
birlikte kaldırır.

## Betikten kullanım

Betikte `nesneler` ya da `kapsam` verilmelidir. `baslangic` betikte milimetre tam
sayısıdır. Günlüğe uygulanan nesneler ve kullanılan her parametre yazılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Kapsamda bu araca uygun nesne yok …` | Kapsamda alan ya da çizgi yok | Alan seçin |
| `'yon' için tanınmayan değer …` | Yön sözcüğü `ters` ya da `saat` değil | Birini yazın |
| `'basamak' 0 ile 12 arasında olmalı …` | Basamak sayısı aralık dışı | 0–12 verin |
| `İşlem durduruldu; çizim değişmedi.` | Durdur'a basıldı | Yeniden çalıştırın |

## İlgili

- [İşlem araçları](../islem/README.md)
- [`UZUNLUKYAZ`](uzunluk_yaz.md) — kenar uzunluklarını yazar
- [`METİN`](text.md) · [`KÖŞETAŞI`](vertex_move.md)
