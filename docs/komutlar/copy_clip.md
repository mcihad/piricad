# PANOYAKOPYALA — Panoya Al

Bir parseli, bir bina bloğunu ya da bir detayı başka bir çizime taşıyacak herkes
için.

## Ne yapar

Seçili nesneleri **çizimin kendi biçiminde** panoya yazar. Çizimde hiçbir şeyi
değiştirmez ve geri alma yığınına girmez.

**Pano yükü ikinci bir biçim değildir.** Yük, `KAYDET`'in yazdığı yerli proje
biçiminin kendisidir: aynı yazıcı yazar, aynı okuyucu okur. Kendine ait bir JSON
taşıyan bir pano, bir belgenin ikinci tarifi olurdu ve bu programda her türü,
her stili, her katmanı ve her öznitelik sütununu gidiş-dönüş taşıyan bir tarif
zaten var.

Bu yüzden panoya **her şey** gider: katman adı, stil, çizgi tipi, öznitelik
sütunları ve değerleri, blok tanımları, ve çizimin koordinat sistemi.

## Taban noktası

Yapıştırılan nesneler öntanımlı olarak **sol alt köşelerinden** gösterilen yere
konur. Bir yapının köşesini komşu parselin köşesine oturtmak gibi belli bir nokta
gerekiyorsa kopyalarken **taban noktası** verin: `taban=<nokta>` ya da
`tabanli=evet` (nesneleri seçtikten sonra sorulur). Yapıştırırken o nokta tam
gösterdiğiniz yere gelir. Düzen menüsündeki **Taban Noktasıyla Kopyala** bunu yapar.

## Pano nerede

`dosya=` verilmezse kullanıcı başına ortak bir pano dosyası kullanılır
(`kentoscad-pano.pcad`, geçici dizinde). Bu, bu programın **iki penceresinin**
aynı panoyu paylaşması ve bir çökmenin yükü kaybetmek yerine yerinde bırakması
demektir.

`dosya=` verilirse yük o dosyaya yazılır — bir betiğin ve başsız bir çalıştırmanın
yolu budur ve **aynı** yoldur (Article 1.2).

## Adlar

| Ad | Tür |
|---|---|
| `PANOYAKOPYALA` | Türkçe, birincil |
| `PANOKOPYALA` | Kısa Türkçe |
| `COPYCLIP` | İngilizce karşılık |
| `PKP` | Kısaltma |
| `core.copy_clip` | Komut kimliği |

## Sözdizimi

```text
PANOYAKOPYALA [nesneler=<kimlik> …] [dosya=<yol>]
PANOYAKOPYALA [nesneler=<kimlik> …] taban=<nokta>
PANOYAKOPYALA tabanli=evet
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | seçim | 0..n | Panoya alınacak nesneler; verilmezse **seçim** kullanılır |
| `dosya` | metin | 0..1 | Panonun yazılacağı dosya; verilmezse ortak pano dosyası |
| `taban` | nokta | 0..1 | Yapıştırırken gösterilen yere gelecek **taban noktası**; verilmezse nesnelerin sol alt köşesi |
| `tabanli` | mantıksal | 0..1 | `evet`: taban noktası nesneler seçildikten sonra sorulur |

## Örnekler

### Komut satırı

```text
SEÇ mod=TÜMÜ
PANOYAKOPYALA
```

```text
Panoya alındı: 2 nesne (1184 bayt).
```

Bir dosyaya:

```text
PANOYAKOPYALA nesneler=1 nesneler=2 dosya="/tmp/blok.pcad"
```

Taban noktasıyla — yapıştırırken parselin kuzeydoğu köşesi gösterilen yere gelir:

```text
PANOYAKOPYALA nesneler=1 taban=40,30
```

### Arayüz

**Düzen > Panoya Kopyala** ya da **Ctrl+C**; araç çubuğunda da bir düğmesi var.
Önce nesneleri seçebilirsiniz; seçim boşsa komut hangi nesnelerin kopyalanacağını
sorar: tıklayın ya da kutu sürükleyin, sonra Enter.

### Betik

```json
{ "cmd": "core.copy_clip", "args": { "nesneler": [1, 2], "dosya": "/tmp/blok.pcad" } }
```

## Geri alma

Geri alınacak bir şey yoktur: bir kopya çizimde hiçbir şeyi değiştirmez. Bu
yüzden bir **dosya** komutudur, bir düzenleme komutu değil.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `İşlem yapılacak nesne yok: seçim boş ve 'nesneler' verilmedi.` | Betik ne `nesneler` verdi ne seçim vardı | `SEÇ` ile seçin ya da `nesneler=` yazın |
| `Seçilen nesneler bulunamadı; panoya bir şey yazılmadı.` | Verilen kimlikler silinmiş | Kimlikleri `SEÇİMBİLGİSİ` ile doğrulayın |
| `Dosya motoru bağlı değil; pano bu yapıda çalışmıyor.` | Başsız bir yapı | Uygulamayı kullanın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [KES](cut.md) — panoya alır **ve** siler
- [YAPIŞTIR](paste.md) — panodakini çizime koyar
- [KOPYALA](copy.md) — aynı çizim içinde çoğaltır, panoya dokunmaz
