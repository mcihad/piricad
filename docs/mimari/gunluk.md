# Komut Günlüğü

Yaptığı işi geri izlemek, tekrarlamak veya belgelemek isteyen ileri kullanıcı için; bu
sayfayı bitirdiğinizde günlüğü okuyabilecek, betiğe çevirebilecek ve destek talebine
ekleyebileceksiniz.

## Nedir

Çalıştırdığınız her komut, argümanlarıyla birlikte bir günlük satırı olarak kaydedilir.
Günlük hem bellekte tutulur hem de diske yazılır.

Bu yalnızca bir kayıt değildir. Geri alma, makro kaydı ve oturum kurtarma aynı günlükten
beslenir; ayrı ayrı yazılmış üç mekanizma değildir, tek bir mekanizmanın üç kullanımıdır.

## Nerede durur

Günlük, işletim sisteminin uygulama verisi dizinindeki `oturum.jsonl` dosyasına yazılır.
Yolu programın açılışında transkriptte görürsünüz:

```text
Komut günlüğü: /home/kullanici/.local/share/KentOSCad/KentOSCad/oturum.jsonl
```

| Sistem | Tipik yol |
|---|---|
| Linux | `~/.local/share/KentOSCad/KentOSCad/oturum.jsonl` |
| Windows | `%LOCALAPPDATA%\KentOSCad\KentOSCad\oturum.jsonl` |
| macOS | `~/Library/Application Support/KentOSCad/KentOSCad/oturum.jsonl` |

Yazma işlemi ayrı bir iş parçacığında yapılır; disk yavaş olsa bile program beklemez.

## Panelden okumak

Pencerenin altındaki **Komut Günlüğü** sekmesi aynı kayıtları canlı gösterir. Her satır
bir komuttur:

```jsonl
{"seq":1,"cmd":"core.layer","args":{"ad":"PARSEL","renk":4281236786},"origin":"gui","crs":"TUREF/TM30","katman":"PARSEL"}
{"seq":2,"cmd":"core.line","args":{"noktalar":[[485300000,4310200000],[485360000,4310200000]]},"origin":"cli","crs":"TUREF/TM30","katman":"PARSEL"}
```

## Satır alanları

| Alan | İçerik |
|---|---|
| `seq` | Sıra numarası, birden başlar |
| `cmd` | Komut kimliği, örneğin `core.line` |
| `args` | Komutun **gerçekten** kullandığı argümanlar |
| `origin` | Komutun hangi istemciden geldiği |
| `crs` | Komut çalıştığı andaki koordinat sistemi |
| `katman` | Komut çalıştığı andaki aktif katman |
| `ts` | Zaman damgası, milisaniye. Yalnız dosyada bulunur |

### `origin` — komut nereden geldi

| Değer | Anlamı |
|---|---|
| `gui` | Araç kutusu düğmesi, menü veya harita alanına tıklama |
| `cli` | Komut satırına yazıldı |
| `script` | Bir betikten geldi |
| `ai` | AI önerisinden geldi (Faz 3) |
| `batch` | Toplu işlemden geldi |
| `test` | Otomatik testten geldi |

Bu alan "bu çizgiyi ben mi çizdim, betik mi çizdi?" sorusunun cevabıdır.

### `args` — gerçekten kullanılan değerler

`args` alanı komutun **çalışırken kullandığı** değerleri tutar, size sorduklarını değil.
Fareyle beş nokta tıklayarak çizdiğiniz bir çizgi, günlüğe tek bir satır olarak ve beş
noktanın tamamıyla yazılır:

```json
{"seq":3,"cmd":"core.line","args":{"noktalar":[[485320150,4310220400],[485370150,4310250400],[485440861,4310321111]]},"origin":"gui","crs":"TUREF/TM30","katman":"PARSEL"}
```

Bu yüzden günlük satırı, komutu **yeniden çalıştırmaya yeter**.

## Aynı iş, aynı satır

Bir komutu arayüzden, komut satırından ve betikten çalıştırırsanız üç günlük satırı da
`cmd` ve `args` bakımından **birbirinin aynısıdır**; yalnız `origin` alanı farklıdır.

Bu eşitlik her derlemede otomatik olarak sınanır. Pratik sonucu şudur: arayüzde
yaptığınız bir işin günlük satırını alıp betiğe koyduğunuzda aynı sonucu alırsınız.

## Günlüğü betiğe çevirmek

Bir günlük satırından `cmd` ve `args` alanlarını alın, bir diziye koyun:

```json
[
  { "cmd": "core.layer", "args": { "ad": "PARSEL", "renk": 4281236786 } },
  { "cmd": "core.line",  "args": { "noktalar": [[485300000,4310200000],
                                                [485360000,4310200000]] } }
]
```

Dosyayı kaydedip çalıştırın:

```
BETİK yeniden.json
```

Böylece arayüzde elle yaptığınız işi tekrarlanabilir bir betiğe dönüştürmüş olursunuz.
Ayrıntı: [Betik yazma](../betik/README.md).

Linux ve macOS'ta `jq` ile de yapabilirsiniz:

```bash
jq -s '[.[] | {cmd, args}]' ~/.local/share/KentOSCad/KentOSCad/oturum.jsonl > yeniden.json
```

## Destek talebine eklemek

Bir sorunu tarif etmek yerine `oturum.jsonl` dosyasını ekleyin. Dosya, sorunun ortaya
çıktığı ana kadar yapılan her şeyi sırasıyla içerir ve aynı adımlar başka bir makinede
tekrar oynatılabilir.

Günlükte çiziminizin koordinatları bulunur; hassas veriyle çalışıyorsanız dosyayı
paylaşmadan önce bunu göz önünde bulundurun.

## Geri alma ile ilişkisi

Geri alma yığını ile günlük iki ayrı şeydir ama aynı olayları anlatır:

- **Geri alma yığını** bellektedir ve program kapanınca kaybolur
- **Günlük** diskte kalıcıdır

`GERİAL` ve `YİNELE` günlüğe **yazılmaz**; çizimi kendileri değiştirmezler, daha önce
yazılmış bir işlemi geri sararlar. Aynı sebeple `YAKINLAŞ` ve `YARDIM` de günlüğe girmez.

Hiçbir şey çizmeden iptal edilen bir komut da günlüğe yazılmaz — hiç olmamış sayılır.

## Bu sürümde olmayanlar

| Özellik | Ne zaman |
|---|---|
| Çökme sonrası günlükten kendiliğinden kurtarma | Faz 1 |
| Uygulama içinden makro kaydet / oynat düğmesi | Faz 2 |
| Günlüğü doğrudan betiğe dönüştüren menü öğesi | Faz 2 |
| Günlük tabanlı çok kullanıcılı düzenleme | Faz 5 |

Bugün bunları elle yapabilirsiniz; altyapı hazır, arayüz kısayolları henüz yok.

## Sırada ne var

- [Betik yazma](../betik/README.md)
- [Komut sistemi](../komutlar/README.md)
