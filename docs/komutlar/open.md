# AÇ — Proje Dosyası Açma

Kaydedilmiş bir çizime dönmek isteyen kullanıcı için; bu sayfayı bitirdiğinizde
bir KentOSCad proje dosyasını açabilecek ve açmanın geri alınamayan bir işlem
olduğunu bileceksiniz.

## Ne yapar

Bir `.pcad` proje dosyasını okur ve **ekrandaki çizimin yerine koyar**.

Dosya tamamen okunana kadar açık olan çizime dokunulmaz. Dosya bozuksa, kesikse
ya da bu sürümün okuyamayacağı kadar yeniyse `AÇ` hata verir ve **çiziminiz
olduğu gibi kalır**. Yarım yüklenmiş bir proje bırakılmaz.

Açma başarılı olduğunda:

- Geri alma yığını **temizlenir**. Açmadan önceki çizime `GERİAL` ile dönülemez.
- Aktif katman `0` olur.
- Çizim, açılan dosyaya bağlanır; artık `KAYDET` oraya yazar.

Kaydedilmemiş çalışmanız varsa **önce onu kaydedin**. `AÇ` sormaz.

Dosyanın içinde ne olduğu: [KentOSCad proje dosyası](../veri/proje-dosyasi.md).

## Adlar

| Ad | Tür |
|---|---|
| `AÇ` | Türkçe, birincil |
| `AC` | Türkçe karaktersiz klavye için |
| `OPEN` | İngilizce karşılık |
| `core.open` | Komut kimliği |

## Sözdizimi

```text
AÇ
AÇ <dosya-yolu>
AÇ dosya=<dosya-yolu>
```

Dosya yolu verilmezse komut yolu sorar. İçinde boşluk olan yol tırnak içine
alınır.

## Parametreler

Tek parametresi vardır: **`dosya`** — açılacak KentOSCad proje dosyasının yolu.
Göreli yol programın çalışma dizinine göre çözülür.

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```
AÇ <ada12-parselasyon>.pcad
```

Yolunda boşluk varsa:

```
AÇ "<Belgelerim/Ada 12 parselasyon>.pcad"
```

Başarılı açmada transkript şunu yazar:

```text
Açıldı: ada12-parselasyon.pcad  (14 nesne, 5 katman, 63 nokta, biçim 1)
```

Dosyada bu sürümün tanımadığı veri varsa uyarı da yazılır:

```text
Açıldı: yeni-surumden.pcad  (14 nesne, 5 katman, 63 nokta, biçim 1)
  uyarı: Dosyada bu sürümün tanımadığı 2 veri bloğu var; içerikleri korunmadı.
         Dosyayı yazan KentOSCad sürümüyle açarsanız tamamını görürsünüz.
```

### Arayüz

**Dosya > Aç…** menüsü, **Dosya** araç çubuğundaki **Aç** düğmesi veya **Ctrl+O**
bir dosya seçme penceresi açar. Pencere yalnızca argümanı toplar; komutun
kendisi aynı komuttur ve klavyeden de, betikten de çalışır.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.open", "args": { "dosya": "veri/ada12-parselasyon.pcad" } }
```

Program açılırken bir betikle proje açmak da mümkündür:

```bash
./build/dev/bin/kentos_cad --betik acilis.json
```

## Geri alma

**`AÇ` geri alınamaz.** Açmak, bir çizimi değiştirmez; onun yerine bir başkasını
koyar. Geri alma yığını açılan belgeyle birlikte sıfırlanır, çünkü yığındaki
adımlar artık var olmayan bir belgenin nesnelerini gösteriyordu.

Bu, kullanıcıların geldiği her CAD ve CBS programının yaptığı şeydir. Yanlış
dosyayı açtıysanız doğru dosyayı açın; kaydetmediğiniz iş gitmiştir.

## Betikten kullanım

`AÇ` betiklenebilirdir. Bir betiğin ortasında proje açmak, o betiğin o ana kadar
yaptığı işi ekrandan kaldırır — betiğin ilk satırı olarak kullanın.

`AÇ` yapay zekâya **kapalıdır**. Belgeyi kaydedilmemiş çalışmayla birlikte
değiştiren, geri alınamayan bir işlem bir öneriyle tetiklenmemelidir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `'...' açılamadı: No such file or directory. Yolu ve okuma iznini denetleyin.` | Dosya yok ya da okunamıyor | Yolu ve izinleri denetleyin |
| `'...' boş; KentOSCad proje dosyası değil.` | Dosya sıfır bayt | Yedeğinden geri alın |
| `io.not_a_project: '...' bir KentOSCad proje dosyası değil.` | Başka bir biçim | Dış biçimler için [İÇEAKTAR](import.md) kullanın |
| `io.format_too_new: '...' en az N. sürüm biçim okuyucusu istiyor` | Dosyayı daha yeni bir KentOSCad yazmış | Mesajdaki sürüme yükseltin |
| `io.truncated: '...' N bayt olduğunu bildiriyor, ama M bayt.` | Dosya kesilmiş | Yedeğinden geri alın |
| `io.bad_block: ...` | Dosyanın iç yerleşimi bozuk | Yedeğinden geri alın |
| `io.inconsistent: ...` | Dosyanın iki yeri birbirini tutmuyor | Yedeğinden geri alın |
| `io.key_mismatch: ...` | Nesne kimlik düzeni bozulmuş | Yedeğinden geri alın; bu dosya güvenilir değil |
| `io.unknown_kind: ... 65535 numaralı türde` | Tür sütununa ayrılmış değer yazılmış (bozuk dosya) | Yedeğinden geri alın; tanınmayan bir tür hata değildir, korunarak açılır |
| `Dosya motoru bağlı değil; bu ortamda dosya açılıp kaydedilemez.` | Dosya motoru olmayan bir ortamda çalışılıyor | Uygulama içinden çalıştırın |
| `Proje dosyası yalnız boş bir belgeye okunabilir.` | İç hata; bu mesajı görürseniz bildirin | — |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
