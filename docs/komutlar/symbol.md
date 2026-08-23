# SEMBOL — Gösterim Rafı

`SEMBOL`, mevzuatın yayımladığı gösterim setini **gezilebilir** hale getirir:
paketi rafa yükler, ağacında dolaşır ve içinde arar.

## Ne yapar

MPYY EK-1, 476 gösterimi kendi ağacında yayımlar — önce ek (EK-1a ortak, EK-1b
MSP, EK-1c ÇDP, EK-1ç NİP, EK-1d UİP), sonra bölüm yolu (`SINIRLAR > İDARİ
SINIRLAR` gibi). `SEMBOL` bu ağacı olduğu gibi gösterir; kendi sınıflandırmasını
uydurmaz. Ağaç veriden gelir (bkz. [MPYY gösterimleri](../veri/mpyy-gosterimleri.md)).

Uygulama açılırken `sembol_kütüphanesi` ayarındaki paketi otomatik yükler, yani
raf hazır gelir. Ayar boşsa raf boş başlar ve çizim yine açılır: bir çizim
kullandığı sembolleri **kendi içinde** taşır, raf kurulu olmayan bir makinede de
aynı açılır.

`SEMBOL` **belgeyi değiştirmez**. Rafta bulduğunuz gösterimi çizime uygulamak
[`STİL`](style.md) komutunun işidir — stil sütununa giden tek yol odur.

## Adlar

`SEMBOL` · `SEMBOLLER` · `SYMBOL` · `SMB`

## Sözdizimi

```
SEMBOL
SEMBOL paket=<dosya yolu>
SEMBOL grup=<grup yolu>
SEMBOL ara=<metin>
SEMBOL kod=<gösterim kimliği>
```

Parametresiz çağrıldığında ağacın en üst düzeyini listeler.

### Grup yolu nasıl yazılır

Düzeyler `>` ile ayrılır:

```
SEMBOL grup="EK-1a — ORTAK GÖSTERİMLER > SINIRLAR > İDARİ SINIRLAR"
```

`>` bu programın seçtiği bir işaret değil: MPYY kendi kaynak künyelerinde tam
olarak bunu yazıyor (`SINIRLAR > İDARİ SINIRLAR`). Pakette hiçbir ek adı, bölüm
adı veya gösterim etiketi bu karakteri içermiyor, dolayısıyla hiçbir ad yanlışlıkla
bölünemez. `/` bu yüzden reddedildi — `HAVAALANI/HAVALİMANI KORUMA KUŞAĞI` tek bir
addır.

Yolun etrafındaki boşluklar kırpılır; basılı ekteki gibi boşluklu yazabilirsiniz.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `paket` | Yüklenecek gösterim paketinin dosya yolu. Göreli yol çalışma dizinine göre çözülür |
| `grup` | Gezilecek grup yolu, düzeyler `>` ile ayrılır. Verilmezse en üst düzey |
| `ara` | Etikette, kimlikte, etiketlerde ve grup yolunda arar. Türkçe büyük/küçük harf kuralına göre katlar: `orman` yazınca `ORMAN ALANI` bulunur |
| `kod` | Tek bir gösterimin ayrıntısı: etiketi, ağaçtaki yeri, mevzuat künyesi, sembol katmanı sayısı |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Rafın en üst düzeyi — hangi ekler yüklü:

```
SEMBOL
```

Bir ekin bölümleri:

```
SEMBOL grup="EK-1d — UYGULAMA İMAR PLANI GÖSTERİMLERİ"
```

İki düzey aşağısı, gösterimlerin kendisi:

```
SEMBOL grup="EK-1a — ORTAK GÖSTERİMLER > SINIRLAR > İDARİ SINIRLAR"
```

Arama — hangi ekte olduğunu bilmeden:

```
SEMBOL ara=orman
```

Tek satırın künyesi:

```
SEMBOL kod=ortak-orman-alani
```

Başka bir paketi rafa eklemek:

```
SEMBOL paket=data/catalogs/mpyy/plan-gosterim.json
```

### Arayüz

Uygulama açılırken `sembol_kütüphanesi` ayarındaki paketi kendiliğinden yükler ve
transkriptte kaç gösterimin rafa girdiğini yazar. Ağaçta gezinmek için komut
satırına yukarıdaki satırların aynısı yazılır — arayüzün ayrı bir yolu yoktur.

### Betik

```json
[
  {"cmd": "core.symbol", "args": {"paket": "data/catalogs/mpyy/plan-gosterim.json"}},
  {"cmd": "core.symbol", "args": {"ara": "orman"}}
]
```

## Geri alma

`SEMBOL` belgeyi değiştirmez, bu yüzden geri alınacak bir şey üretmez ve geri
alma yığınına satır yazmaz. Raf oturuma aittir: uygulama kapandığında gider,
proje dosyasına yazılmaz.

## Betikten kullanım

Komut kimliği `core.symbol`. Betikten ve yapay zekâdan erişilebilir; belgeye
dokunmadığı için kum havuzunun her düzeyinde çalışır.

Çıktısı transkripte yazılır. Bir betiğin gösterim listesini işlemesi gerekiyorsa
paketi doğrudan okumak daha uygundur — dosya biçimi
[MPYY gösterimleri](../veri/mpyy-gosterimleri.md) sayfasında anlatılıyor.

## Hatalar

| İleti | Sebep | Çözüm |
|---|---|---|
| `Sembol paketi açılamadı: '<yol>'` | Dosya yok ya da okunamıyor | Yolu kontrol edin; göreli yol çalışma dizinine göre çözülür |
| `Sembol paketi okunamadı: '<yol>': ...` | Dosya JSON olarak ayrıştırılamadı | Paket bozuk. Sürümünü ve `sha256` künyesini doğrulayın |
| `Sembol rafı boş. 'SEMBOL paket=<yol>' ile bir gösterim paketi yükleyin.` | Raf henüz yüklenmedi | `sembol_kütüphanesi` ayarını kontrol edin ya da paketi elle yükleyin |
| `Rafta böyle bir gösterim yok: '<kimlik>'` | `kod` ile verilen kimlik yüklü paketlerde yok | `SEMBOL ara=` ile arayın; kimlikler kalıcıdır ve yeniden kullanılmaz |
| `(boş — böyle bir grup yok)` | `grup` yolu hiçbir satırla eşleşmiyor | Bir üst düzeyi listeleyip adı olduğu gibi kopyalayın |
