# BAĞLA — Yazıyı Nesneye Bağlama

Elle yazdığı ya da dışarıdan aldığı bir yazının bir çizgiyi ya da parseli izlemesini
isteyen herkes için; bu sayfayı bitirdiğinizde bir yazıyı en yakın kenara, köşeye ya da
nesnenin ortasına bağlamayı, yazının kenar uzunluğunu, parselin alanını ya da bir
kalıpla sütun değerlerini söylemesini sağlamayı ve kaynak nesneyi sahneden seçmeyi
bileceksiniz.

## Ne yapar

`BAĞLA`, kapsamındaki **yazıları** seçtiğiniz bir nesneye — çizgiye ya da alana —
**bağlar**. Bağlı bir yazı [bağlı nesnedir](../islem/bagli-nesneler.md): nesne
taşındığında, döndürüldüğünde ya da bir köşesi çekildiğinde yazı onunla birlikte gider.
Her yazı, kaynağın **en yakın kenarına** (`bag=kenar`, varsayılan), **en yakın
köşesine** (`bag=kose`) ya da **ortasına** (`bag=merkez`, dış halkasının kutusunun
ortası — bir parsel numarasının durduğu yer) bağlanır.

Bağlamak yazıyı **yerinden oynatmaz**: yazının o anki yeri ile kuralın yeri arasındaki
fark "el payı" olarak saklanır ve izleme oradan başlar. Yazının sözü `tur` ile seçilir:

| `tur` | Yazı ne söyler |
|---|---|
| `sabit` (varsayılan) | Kendi sözünü korur |
| `uzunluk` | Kenarın uzunluğunu; `bag=merkez` ile nesnenin bütün uzunluğunu |
| `alan` | Nesnenin alanını, metrekare: `200,00 m²` |
| `bicim` | `bicim` kalıbını, nesneden doldurulmuş olarak |

Kalıpta `{}` sayının yerini tutar; `{#alan}` alan, `{#cevre}` çevre, `{#uzunluk}` uzunluk
olarak ölçülür, `{sutun}` o sütunun değeridir: `bicim="Ada {ada}: {#alan} m²"`. Uzunluk,
alan ve kalıplı yazı **nesne değiştikçe** — köşesi çekilince, ölçeklenince ya da bir
sütunu değişince — o işlemin içinde yeniden yazılır; bağlandığı anda da hemen doldurulur.
Sütunu boşalan bir yazı silinmez, boş kalır ve sütun dolunca geri gelir.

[`UZUNLUKYAZ`](uzunluk_yaz.md) ve [`KÖŞENUMARALA`](kose_numarala.md) yazdıkları yazıyı
zaten bağlar; `BAĞLA`, [`METİN`](text.md) ile yazılmış ya da DXF'ten gelmiş serbest
yazılar içindir. Tersi [`BAĞÇÖZ`](bag_coz.md)'dür.

Bu bir [işlem aracıdır](../islem/README.md): kapsam, tek geri alma adımı ve Durdur orada
anlatılır. Yerinde değiştiren bir araçtır; çıktı katmanı kullanmaz.

## Adlar

| Ad | Tür |
|---|---|
| `BAĞLA` | Türkçe, birincil |
| `BAGLA` | ASCII karşılık |
| `ATTACH` | İngilizce karşılık |
| `BĞ`, `BG` | Kısaltma |
| `islem.bagla` | Komut kimliği |

## Sözdizimi

```text
BAĞLA kaynak=<kimlik>
BAĞLA nesneler=<yazı> kaynak=<kimlik> bag=kose
BAĞLA nesneler=<yazı> kaynak=<kimlik> tur=uzunluk birim=metre ondalik=2
BAĞLA nesneler=<yazı> kaynak=<kimlik> bag=merkez tur=alan
BAĞLA nesneler=<yazı> kaynak=<kimlik> bag=merkez tur=bicim bicim="Ada {ada}: {#alan} m²"
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Bağlanacak yazıların kimlikleri; verilirse `kapsam` okunmaz |
| `kapsam` | `secili` (varsayılan), `gorunum` ya da `proje` |
| `pencere` | `gorunum` için görünümün iki köşesi |
| `katman` | Bu araçta kullanılmaz; yazı kendi katmanında kalır |
| `kaynak` | Yazıların bağlanacağı nesne (çizgi ya da alan), **zorunlu**; kartta sahneden seçilir |
| `bag` | Neye bağlanacağı: `kenar` (varsayılan), `kose` ya da `merkez` |
| `tur` | Yazının sözü: `sabit` (varsayılan), `uzunluk`, `alan` ya da `bicim` (yukarıdaki tablo) |
| `birim` | `tur=uzunluk` için birim: `metre` (varsayılan), `santimetre`, `milimetre`, `kilometre` |
| `ondalik` | Ölçülen sayının virgülden sonraki basamak sayısı, 0–6; varsayılan 2 |
| `bicim` | Kalıp; `{}` sayının yerini tutar, `{#alan}`, `{#cevre}`, `{#uzunluk}` ölçülür, `{sutun}` sütunun değeridir. `tur=bicim` için zorunlu |
| `ayrac` | Ondalık ayracı: `virgul` (varsayılan) ya da `nokta` |

## Örnekler

### Komut satırı

Bir çizgi, yanına elle yazılmış bir not; not çizgiye bağlanır ve çizgiyle taşınır:

```
KATMAN ad=YOL
ÇİZGİ 0,0 10,0
METİN 5,2 "yol kenarı"
BAĞLA nesneler=2 kaynak=1
TAŞI nesneler=1 baslangic=0,0 bitis=20,0
```

Aynı not kenarın uzunluğunu söylesin; çizgi uzayınca sayı değişir:

```
BAĞLA nesneler=2 kaynak=1 tur=uzunluk bicim="L={}"
ÖLÇEKLE nesneler=1 merkez=20,0 carpan=2
```

Bir parselin ortasındaki yazı parselin alanını söylesin; köşe çekilince alan yeniden
yazılır:

<!-- örnek: yeni çizim -->
```
ALAN 0,0 20,0 20,10 0,10
METİN noktalar=10,5 yazi=? hizalama=merkez
BAĞLA nesneler=2 kaynak=1 bag=merkez tur=alan
KÖŞETAŞI nesne=1 kose=3 nokta=20,20
```

### Arayüz

Sağ panelde **Araçlar ▸ Etiketleme ▸ Yazıyı nesneye bağla**. Bağlanacak yazıları seçin
ya da kapsamı **Seçili** bırakıp aracın seçtirmesini bekleyin. **kaynak** alanının
yanındaki nişan düğmesine basın: işaretçi seçim işaretçisine döner, durum satırı
"Sahneden bir nesne tıklayın" der; çizgiye tıklayın, kimliği alana yazılır. (Klavyeyle:
alana Tab ile gidin, **F4** ya da **Alt+↓** nişanı basar; kimliği doğrudan da
yazabilirsiniz.) **Çalıştır**'a basın. Kartın altındaki satır komut satırına
yazılacak olanın kendisidir.

### Betik

```json
{
  "ad": "Notu yola bağla",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "YOL" } },
    { "cmd": "core.line",  "args": { "noktalar": [[0, 0], [10000, 0]] } },
    { "cmd": "core.text",  "args": { "noktalar": [5000, 2000], "yazi": "yol kenarı" } },
    { "cmd": "islem.bagla", "args": { "nesneler": [2], "kaynak": [1], "bag": "kenar" } }
  ]
}
```

## Geri alma

`BAĞLA` **tek bir geri alma adımıdır**: [`GERİAL`](undo.md) bağı kaldırır ve
`tur=uzunluk` ile değişmiş yazıyı eski sözüne döndürür.

## Betikten kullanım

Betikte `nesneler` ya da `kapsam` ve `kaynak` verilmelidir; `kaynak` bir kimlik listesidir
(`[1]`). Günlüğe bağlanan yazılar ve kullanılan her parametre yazılır; yeniden oynatma
aynı bağı kurar.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bağlanılacak nesne verilmedi: kaynak=<kimlik> yazın ya da karttaki Kaynak alanından sahneden seçin.` | `kaynak` yok | Kaynağı verin |
| `'kaynak' nesnesi bulunamadı veya silinmiş: …` | Kimlik yanlış ya da nesne silinmiş | Doğru kimliği verin |
| `Kapsamda bu araca uygun nesne yok …` | Kapsamda yazı yok | Yazıları seçin |
| `Bir nesne kendisine bağlanamaz.` | Yazı kendi kimliğine bağlanmak istendi | Başka bir kaynak verin |
| `Bağ döngüsü: …` | Kaynak zaten bu yazıyı dolaylı olarak izliyor | Zinciri kırın: önce `BAĞÇÖZ` |
| `'tur' için tanınmayan değer …` | `tur` sözcüğü `sabit`, `uzunluk`, `alan` ya da `bicim` değil | Birini yazın |
| `tur=bicim bir kalıp ister: bicim="Ada {ada} · {#alan} m²" gibi.` | `tur=bicim` verildi ama `bicim` yok | Kalıbı verin |
| `İşlem durduruldu; çizim değişmedi.` | Durdur'a basıldı | Yeniden çalıştırın |

## İlgili

- [Bağlı nesneler](../islem/bagli-nesneler.md)
- [`BAĞÇÖZ`](bag_coz.md) · [`UZUNLUKYAZ`](uzunluk_yaz.md) · [`KÖŞENUMARALA`](kose_numarala.md)
- [`METİN`](text.md) · [`TAŞI`](move.md)
