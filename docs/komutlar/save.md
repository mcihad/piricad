# KAYDET — Çizimi Kaydetme

Çalışmasını kaybetmek istemeyen kullanıcı için; bu sayfayı bitirdiğinizde çizimi
bağlı olduğu proje dosyasına kaydedebilecek ve kaydetmenin neden kesintiye
dayanıklı olduğunu bileceksiniz.

## Ne yapar

Çizimi, bağlı olduğu KentOSCad proje dosyasına (`.pcad`) yazar.

Çizim henüz bir dosyaya bağlı değilse `KAYDET` **hata verir** ve
[FARKLIKAYDET](saveas.md) kullanmanızı ister. Sessizce bir ad uydurmaz.

Kaydetme kesintiye dayanıklıdır: KentOSCad önce yanına geçici bir dosya yazar,
ancak son bayt diske indikten sonra onu yerine koyar. Kaydetme sırasında
elektrik giderse ya da disk dolarsa **bir önceki kaydınız yerinde durur**.

Dosyanın neyi taşıdığı: [KentOSCad proje dosyası](../veri/proje-dosyasi.md).

## Adlar

| Ad | Tür |
|---|---|
| `KAYDET` | Türkçe, birincil |
| `SAVE` | İngilizce karşılık |
| `KYD` | Kısaltma |
| `core.save` | Komut kimliği |

## Sözdizimi

```text
KAYDET
KAYDET <dosya-yolu>
KAYDET dosya=<dosya-yolu>
```

Yol verilmezse çizimin bağlı olduğu dosyaya yazılır. Yol verilirse oraya yazılır
ve çizim oraya bağlanır — yani `KAYDET <yol>` ile `FARKLIKAYDET <yol>` aynı işi
yapar, ikisi de vardır çünkü menüde iki ayrı eylemdir.

## Parametreler

Tek parametresi vardır: **`dosya`** — isteğe bağlı hedef yol. Verilmezse çizimin
bağlı olduğu dosya kullanılır.

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Argümansız `KAYDET`, çizimi bağlı olduğu dosyaya yazar:

```text
KAYDET
```

Transkript şunu yazar:

```text
Kaydedildi: ada12-parselasyon.pcad  (14 nesne, 1688 bayt)
```

Başka bir yola kaydeder ve oraya bağlanır:

```
KAYDET <yedek/ada12-2024-05>.pcad
```

### Arayüz

**Dosya > Kaydet** menüsü, **Dosya** araç çubuğundaki **Kaydet** düğmesi veya
**Ctrl+S**. Çizim henüz bir dosyaya bağlı değilse arayüz **Farklı Kaydet**
penceresini açar; komut ise hatayı söyler. İkisi de aynı komuta gider.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.save", "args": {} }
```

Hedefi açıkça vererek:

```json
{ "cmd": "core.save", "args": { "dosya": "veri/ada12.pcad" } }
```

## Geri alma

`KAYDET` çizimi değiştirmez, dolayısıyla geri alınacak bir şey yoktur ve geri
alma yığınına girmez. `GERİAL` kaydetmeden önceki adıma değil, kaydetmeden
önceki **çizim** adımına döner.

Kaydedilmiş bir dosyayı "geri almanın" yolu yoktur; yedek almak sizin işinizdir.

## Betikten kullanım

`KAYDET` betiklenebilirdir ve salt okunur işaretlidir — çizimi değiştirmez.
Günlüğe de yazılmaz: bir oturumu yeniden oynatmak, o oturumda basılmış her
kaydetmeyi yeniden yapmamalıdır.

Toplu işlemde her komuttan sonra değil, betiğin sonunda bir kez çağırın.

`KAYDET` yapay zekâya kapalıdır: bir öneri, kullanıcının diskindeki dosyanın
üzerine yazmamalıdır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bu çizim henüz bir dosyaya bağlı değil. FARKLIKAYDET ile bir ad verin.` | Hiç kaydedilmemiş çizim | [FARKLIKAYDET](saveas.md) kullanın |
| `'...' dizini yok. Önce dizini oluşturun ya da başka bir yol seçin.` | Hedef klasör yok | Klasörü oluşturun |
| `'...' yazmak için açılamadı. Dizin izinlerini ve boş alanı denetleyin.` | İzin yok | Yazma iznini denetleyin |
| `'...' yazılırken hata oluştu; disk dolu olabilir. Önceki dosya değiştirilmedi.` | Disk doldu | Yer açın; önceki dosyanız duruyor |
| `'...' yerine konamadı: ... Önceki dosya değiştirilmedi.` | Hedef dosya kilitli ya da başka bir programda açık | Dosyayı kapatın ve yeniden deneyin |
| `Kaydedilecek dosya adı boş. FARKLIKAYDET ile bir ad verin.` | Boş yol verildi | Bir ad yazın |
| `Dosya motoru bağlı değil; bu ortamda dosya açılıp kaydedilemez.` | Dosya motoru olmayan bir ortam | Uygulama içinden çalıştırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
