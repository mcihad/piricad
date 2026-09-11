# UZUNLUKYAZ — Kenar Uzunluklarını Yazma

Paftadaki her parsel kenarına, her yol çizgisine uzunluğunu kenara paralel yazmak
isteyen herkes için; bu sayfayı bitirdiğinizde uzunlukları Araçlar panelinden, komut
satırından ve betikten, istediğiniz birim ve biçimde yazdırmayı bileceksiniz.

## Ne yapar

`UZUNLUKYAZ`, kapsamındaki her **çizginin** her parçasına ve her **alanın** her kenarına
uzunluğunu yazar. Yazı kenara **paraleldir**, soldan sağa okunur, kenarın ortasına
oturur ve kenarın seçtiğiniz yanına yüksekliğinin yarısı kadar açıkta durur. Ürettiği
her yazı sıradan bir [`METİN`](text.md) nesnesidir: taşınır, düzenlenir, silinir.

Bu bir [işlem aracıdır](../islem/README.md): kapsamı seçim, görünüm ya da bütün
projedir; iş ayrı iş parçacığında koşar ve **Durdur** ile kesilir; sonuç tek geri alma
adımıdır ve istenirse ayrı bir katmana yazılır.

## Adlar

| Ad | Tür |
|---|---|
| `UZUNLUKYAZ` | Türkçe, birincil |
| `LABELLENGTH` | İngilizce karşılık |
| `UZY` | Kısaltma |
| `islem.uzunluk_yaz` | Komut kimliği |

## Sözdizimi

```text
UZUNLUKYAZ
UZUNLUKYAZ nesneler=<k1> nesneler=<k2>
UZUNLUKYAZ kapsam=proje birim=metre ondalik=2 katman=UZUNLUK
UZUNLUKYAZ kapsam=gorunum pencere=485300,4310200 pencere=485400,4310300
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Uygulanacak nesnelerin kimlikleri; verilirse `kapsam` okunmaz |
| `kapsam` | `secili` (varsayılan), `gorunum` ya da `proje` |
| `pencere` | `gorunum` için görünümün iki köşesi; iki `pencere=` değeri |
| `katman` | Yazıların gideceği katman; yoksa oluşturulur; boşsa etkin katman |
| `birim` | `metre` (varsayılan), `santimetre`, `milimetre`, `kilometre` |
| `ondalik` | Virgülden sonraki basamak, 0–6; varsayılan 2 |
| `bicim` | Yazının kalıbı; `{}` sayının yeridir. Varsayılan `{} m` (birime göre `{} cm` vb.) |
| `ayrac` | Ondalık ayracı: `virgul` (varsayılan) ya da `nokta` |
| `taraf` | Yazının yanı: `otomatik` (varsayılan: alanda dışı, çizgide okuma yönünün solu, yani yatay çizgide üstü), `sol`, `sag`, `dis` (alanın dışı), `ic` |
| `yukseklik` | Yazı yüksekliği, zemin milimetresi; `0` = plan ölçeğinde 2,5 mm |
| `bosluk` | Kenar ile yazı arası, milimetre; `0` = yüksekliğin yarısı |
| `enaz` | Bundan kısa kenarlara yazılmaz, milimetre; varsayılan 0 |

## Örnekler

### Komut satırı

Seçili parsellerin bütün kenarları, iki ondalık, metre:

```text
SEÇ
UZUNLUKYAZ
```

Projedeki her şey, santimetre, ondalıksız, `L=` önekiyle, kendi katmanına:

```text
UZUNLUKYAZ kapsam=proje birim=santimetre ondalik=0 bicim="L={}" katman=UZUNLUK
```

Parsellerde yazı dışa, 35 metreden kısa kenarlar boş:

```text
UZUNLUKYAZ nesneler=1 taraf=dis enaz=35000
```

### Arayüz

Sağ panelde **Araçlar ▸ Etiketleme ▸ Kenar uzunluklarını yaz**. Kartta kapsamı seçin,
birimi ve biçimi ayarlayın, gerekirse bir katman adı yazın ve **Çalıştır**'a basın; ya
da **Analiz ▸ İşlem Araçları ▸ Kenar uzunluklarını yaz**. Seçim boşsa araç
"nesneleri seçin" der: tuvalden tıklayın, **sağ tık** başlatır. Durum çubuğu yüzdeyi
gösterir; **Durdur** keser.

### Betik

```json
{
  "ad": "Kenar uzunlukları",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.area",  "args": { "noktalar": [[485320000,4310220000],[485360000,4310220000],
                                                  [485360000,4310250000],[485320000,4310250000]] } },
    { "cmd": "islem.uzunluk_yaz",
      "args": { "nesneler": [1], "birim": "metre", "ondalik": 2, "taraf": "dis",
                "katman": "UZUNLUK" } }
  ]
}
```

## Geri alma

`UZUNLUKYAZ` **tek bir geri alma adımıdır**: [`GERİAL`](undo.md) ürettiği bütün yazıları
birlikte kaldırır; oluşturduğu katman kalır.

## Betikten kullanım

Betikte `nesneler` ya da `kapsam` verilmelidir. Günlüğe uygulanan nesnelerin kimlikleri
ve kullanılan her parametre (varsayılanlar dâhil) yazılır; yeniden oynatma aynı yazıları
üretir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Kapsamda bu araca uygun nesne yok …` | Kapsamda çizgi ya da alan yok | Çizgi ya da alan seçin |
| `'birim' için tanınmayan değer …` | Birim sözcüğü listede yok | `metre`, `santimetre`, `milimetre`, `kilometre` |
| `'ondalik' 0 ile 6 arasında olmalı …` | Basamak sayısı aralık dışı | 0–6 verin |
| `gorunum kapsamı görünümün iki köşesini ister …` | `pencere` eksik | İki köşe verin ya da paneli kullanın |
| `İşlem durduruldu; çizim değişmedi.` | Durdur'a basıldı | Yeniden çalıştırın |

## İlgili

- [İşlem araçları](../islem/README.md)
- [`KÖŞENUMARALA`](kose_numarala.md) — köşeleri numaralar
- [`METİN`](text.md) · [`ÖLÇÜ`](dimension.md)
