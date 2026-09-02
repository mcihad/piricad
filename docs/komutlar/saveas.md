# FARKLIKAYDET — Yeni Ada Kaydetme

Çizimini ilk kez kaydeden ya da bir kopyasını ayıran kullanıcı için; bu sayfayı
bitirdiğinizde çizimi yeni bir proje dosyasına yazabilecek ve çizimin artık hangi
dosyaya bağlı olduğunu bileceksiniz.

## Ne yapar

Çizimi verdiğiniz yola bir KentOSCad proje dosyası (`.pcad`) olarak yazar ve
**çizimi o dosyaya bağlar**. Bundan sonra [KAYDET](save.md) oraya yazar.

Hedefte aynı adlı bir dosya varsa üzerine yazılır. KentOSCad önce yanına geçici bir
dosya yazıp ancak tamamlandığında yerine koyduğu için, yarıda kesilen bir
`FARKLIKAYDET` **eski dosyayı bozmaz**.

Dosyanın neyi taşıdığı: [KentOSCad proje dosyası](../veri/proje-dosyasi.md).

## Adlar

| Ad | Tür |
|---|---|
| `FARKLIKAYDET` | Türkçe, birincil |
| `SAVEAS` | İngilizce karşılık |
| `FKAYDET` | Kısaltma |
| `core.saveas` | Komut kimliği |

## Sözdizimi

```text
FARKLIKAYDET
FARKLIKAYDET <dosya-yolu>
FARKLIKAYDET dosya=<dosya-yolu>
```

Dosya yolu verilmezse komut yolu sorar. İçinde boşluk olan yol tırnak içine
alınır. Uzantıyı `.pcad` yazmanız beklenir; KentOSCad uzantı eklemez, çünkü adı
siz koyarsınız.

## Parametreler

Tek parametresi vardır: **`dosya`** — yeni proje dosyasının yolu. Göreli yol
programın çalışma dizinine göre çözülür.

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```
FARKLIKAYDET <ada12-parselasyon>.pcad
```

Yolunda boşluk varsa:

```
FARKLIKAYDET "<Belgelerim/Ada 12 parselasyon>.pcad"
```

Transkript şunu yazar:

```text
Farklı kaydedildi: ada12-parselasyon.pcad  (14 nesne, 1688 bayt)
```

### Arayüz

**Dosya > Farklı Kaydet…** menüsü, **Dosya** araç çubuğundaki **Farklı Kaydet**
düğmesi veya **Ctrl+Shift+S** bir dosya adı penceresi açar. Pencere yalnızca
argümanı toplar; iptal ederseniz hiçbir şey olmaz ve hata da verilmez.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.saveas", "args": { "dosya": "veri/ada12-parselasyon.pcad" } }
```

Aynı çizimi üç farklı istemciden — arayüzden, komut satırından ve betikten —
kaydettiğinizde **bayt bayt aynı dosya** üretilir. Bu, sürüm denetimine giren bir
proje dosyasının kimin kaydettiğine göre değişmemesi demektir ve sınanmaktadır.

## Geri alma

`FARKLIKAYDET` çizimi değiştirmez, dolayısıyla geri alınacak bir şey yoktur ve
geri alma yığınına girmez.

Geri alınamayan tek etkisi, çizimin artık **yeni dosyaya bağlı** olmasıdır:
bundan sonraki `KAYDET` eski dosyaya değil yenisine yazar. Eskiye dönmek için
`FARKLIKAYDET` ile eski adı yeniden verin.

## Betikten kullanım

`FARKLIKAYDET` betiklenebilirdir ve salt okunur işaretlidir. Günlüğe yazılmaz:
bir oturumu yeniden oynatmak, o oturumdaki her kaydetmeyi yeniden yapmamalıdır.

Toplu üretimde tipik kullanım, betiğin sonunda tek çağrıdır:

```json
{
  "ad": "Ada 12 parselasyon çıktısı",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.line",  "args": { "noktalar": [[485300000,4310200000],
                                                  [485360000,4310200000],
                                                  [485360000,4310245000],
                                                  [485300000,4310245000],
                                                  [485300000,4310200000]] } }
  ]
}
```

Yukarıdaki betiğin sonuna `core.saveas` satırı eklendiğinde çıktı diske yazılır.

`FARKLIKAYDET` yapay zekâya kapalıdır: bir öneri, kullanıcının diskinde dosya
oluşturmamalıdır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Kaydedilecek dosya adı boş. FARKLIKAYDET ile bir ad verin.` | Boş yol verildi | Bir ad yazın |
| `'...' dizini yok. Önce dizini oluşturun ya da başka bir yol seçin.` | Hedef klasör yok | Klasörü oluşturun |
| `'...' yazmak için açılamadı. Dizin izinlerini ve boş alanı denetleyin.` | İzin yok | Yazma iznini denetleyin |
| `'...' yazılırken hata oluştu; disk dolu olabilir. Önceki dosya değiştirilmedi.` | Disk doldu | Yer açın |
| `'...' yerine konamadı: ... Önceki dosya değiştirilmedi.` | Hedef dosya kilitli | Dosyayı kapatın ve yeniden deneyin |
| `Belge dosya biçiminin üst sınırından büyük` | Çizim biçim sınırını aştı | Belgeyi bölerek kaydedin |
| `Dosya motoru bağlı değil; bu ortamda dosya açılıp kaydedilemez.` | Dosya motoru olmayan bir ortam | Uygulama içinden çalıştırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
