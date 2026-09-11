# ALANDÜZENLE — Alanı İstenen Değere Getirme

Tapuda 1 250,00 m² yazan parselin çizimde 1 248,71 m² çıktığını gören, sınırı
bozmadan alanı tam değere oturtmak isteyen herkes için; bu sayfayı bitirdiğinizde
bir alanı kenarından kaydırarak, köşesinden çekerek ya da her yandan eşit
daraltıp genişleterek istenen alana getirmeyi bileceksiniz.

## Ne yapar

`ALANDÜZENLE`, kapalı bir alanın alanını **istenen değere** getirir. Nesne aynı nesne
kalır: kimliği, katmanı, öznitelikleri değişmez; yalnız köşeleri kayar. Üç yolu vardır:

| `mod=` | Ne olur |
|---|---|
| `hepsi` (varsayılan) | Bütün kenarlar aynı mesafede içe ya da dışa alınır; köşeler yerinde çözülür. Komut satırının ve toplu işin yolu |
| `kenar` | **Bir kenar** kendi doğrultusunda kaydırılır; iki komşu kenar uzar ya da kısalır, şekil bozulmaz |
| `kose` | **Bir köşe** çekilir; alanı istenen değere getiren yerler bir doğru oluşturur, köşe o doğruya oturur |

Arayüzde `kenar` ve `kose` **etkileşimlidir**: kenarı ya da köşeyi tıklarsınız, bir
**hayalet** fareyi izler, hedef alana yaklaşınca üzerine **oturur** (vurgu rengiyle
çizilir ve `✓ hedef` yazar); **Enter** el nerede olsa hedefe tam oturan yeri kabul eder,
sol tık elin durduğu yeri alır, Esc vazgeçer. Özgün sınır siz kabul edene kadar
ekranda ve çizimde olduğu gibi durur.

Bu bir [işlem aracıdır](../islem/README.md): kapsam, asenkron çalışma ve tek geri alma
adımı orada anlatılır. `hepsi` kipi kapsamdaki **her** alana uygulanır; `kenar` ve `kose`
tek bir alan ister.

## Adlar

| Ad | Tür |
|---|---|
| `ALANDÜZENLE` | Türkçe, birincil |
| `ALANDUZENLE` | ASCII karşılık |
| `ADJUSTAREA` | İngilizce karşılık |
| `ADZ` | Kısaltma |
| `islem.alan_duzenle` | Komut kimliği |

## Sözdizimi

```text
ALANDÜZENLE nesneler=<k> alan=<m²>
ALANDÜZENLE nesneler=<k> alan=<m²> mod=kenar kenar=<sıra>
ALANDÜZENLE nesneler=<k> alan=<m²> mod=kose kose=<sıra> nokta=<n>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Alanı değişecek nesnelerin kimlikleri; verilmezse `kapsam` |
| `kapsam` | `secili` (varsayılan), `gorunum`, `proje` |
| `pencere` | `gorunum` için görünümün iki köşesi |
| `alan` | **Hedef alan, metrekare** (zorunlu) |
| `mod` | `hepsi` (varsayılan), `kenar`, `kose` |
| `kenar` | Kaydırılacak kenar; 1. köşeden çıkan kenar 1'dir. Arayüzde tıklayarak seçilir |
| `kose` | Çekilecek köşe; ilk köşe 1'dir. Arayüzde tıklayarak seçilir |
| `nokta` | Kenarın ya da köşenin gideceği yer. Verilmezse komut satırı hedefe **tam** oturtur; arayüz sürükletir |

Kenar kaydırma ve köşe çekme köşeleri milimetreye yuvarlar; kalan fark bir kenarın
uzunluğu kadar milimetrekaredir ve iki ondalıklı metrekarede görünmez.

## Örnekler

### Komut satırı

Seçili parseli her yandan 1 500 m²'ye getir:

```text
SEÇ
ALANDÜZENLE alan=1500
```

Üçüncü kenarı kaydırarak:

```text
ALANDÜZENLE nesneler=1 alan=1500 mod=kenar kenar=3
```

Üçüncü köşeyi çekerek, elin bıraktığı yere:

```text
ALANDÜZENLE nesneler=1 alan=1300 mod=kose kose=3 nokta=485360,4310258
```

### Arayüz

Parseli seçin; **Araçlar ▸ Düzenleme ▸ Alanı düzenle**. Kartta parselin şimdiki alanı
yazar; `alan` kutusuna hedefi, `mod` listesinden `kenar` ya da `kose` seçin ve
**Çalıştır**'a basın. Komut "Çekilecek kenarı tıklayın" der; tıklayın, sürükleyin,
hayalet hedefe oturunca **Enter**. Durum çubuğu ne beklendiğini ve Enter'ın kabul
ettiğini yazar. `hepsi` kipi soru sormadan uygular.

### Betik

```json
{
  "ad": "Alan düzeltme",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.area",  "args": { "noktalar": [[485320000,4310220000],[485360000,4310220000],
                                                  [485360000,4310250000],[485320000,4310250000]] } },
    { "cmd": "islem.alan_duzenle",
      "args": { "nesneler": [1], "alan": 1250, "mod": "kenar", "kenar": 3 } }
  ]
}
```

## Geri alma

`ALANDÜZENLE` **tek bir geri alma adımıdır**: [`GERİAL`](undo.md) alanı eski köşelerine
döndürür.

## Betikten kullanım

Betikte `nesneler` ya da `kapsam` ve `alan` verilmelidir. `kenar`/`kose` kiplerinde
`nokta` verilmezse hedefe tam oturan yer hesaplanır; günlüğe `kenar`, `kose` ve
`nokta` yazıldığı için yeniden oynatma el gerektirmez.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Hedef alan sıfırdan büyük olmalı: alan=<m²>.` | `alan` yok ya da sıfır | Metrekare olarak hedefi yazın |
| `Kenardan ya da köşeden çekmek tek bir alan ister; kapsamda N nesne var.` | `mod=kenar/kose` ile birden çok nesne | Tek alan seçin ya da `mod=hepsi` |
| `Bu alanın N. kenarı yok; M köşesi var.` | `kenar`/`kose` sırası aralık dışı | 1 ile M arasında verin |
| `Kenar bu kadar kaydırılamaz: komşu kenarlar onunla kesişmiyor.` | Komşu kenarlar kaydırılan kenara paralel | Başka bir kenar ya da `mod=hepsi` |
| `Alan bu değere her taraftan daraltılarak getirilemiyor …` | Şekil hedefe varmadan bölünüyor ya da yok oluyor | Daha yakın bir hedef ya da kenar/köşe kipi |
| `Kapsamda bu araca uygun nesne yok …` | Kapsamda kapalı alan yok | Bir alan seçin |

## İlgili

- [İşlem araçları](../islem/README.md)
- [`ALANÖLÇ`](measure_area.md) — alanı okur
- [`KÖŞETAŞI`](vertex_move.md) — köşeyi serbestçe taşır
- [`OFSET`](offset.md) — paralel kopya çıkarır
