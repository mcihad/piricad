# BETİK — Betik Çalıştırma

Tekrarlayan işi otomatikleştirmek isteyen kullanıcı için; bu sayfayı bitirdiğinizde bir
JSON betiğini çalıştırabilecek ve tamamının tek adımda geri alınabildiğini bileceksiniz.

## Ne yapar

Bir betik dosyasındaki komutları sırayla komut veri yolundan çalıştırır.

Betiğin **tamamı tek bir geri alma adımıdır** ve **tek bir doğrulama geçişinden** geçer.
Bir satır başarısız olursa betiğin o ana kadar yaptığı **her şey geri alınır** — yarım
uygulanmış bir betik bırakılmaz.

Betik yazmanın tamamı: [Betik yazma](../betik/README.md).

## Adlar

| Ad | Tür |
|---|---|
| `BETİK` | Türkçe, birincil |
| `BETIK` | Türkçe karaktersiz klavye için |
| `SCRIPT` | İngilizce karşılık |
| `core.script` | Komut kimliği |

## Sözdizimi

```
BETİK
BETİK <dosya-yolu>
BETİK dosya=<dosya-yolu>
```

Dosya yolu verilmezse komut yolu sorar. İçinde boşluk olan yol tırnak içine alınır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `dosya` | Çalıştırılacak betik dosyasının yolu. Göreli yol programın çalışma dizinine göre çözülür |
| `onizle` | `evet` ise betik **çalıştırılmaz**: ne değiştireceği söylenir, çizime dokunulmaz ([ÖNİZLE](preview.md)). Yalnız JSON betikleri önizlenir. Varsayılan `hayır` |

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

```
BETİK tests/journal/ornek-parsel.json
```

Yolunda boşluk varsa:

```
BETİK "Belgelerim/parsel çizimi.json"
```

Başarılı çalışmada transkript betiğin adını ve tek adımının ne yaptığını yazar:

```text
Betik tamamlandı: tests/journal/ornek-parsel.json
Örnek parsel çizimi: 9 komut, tek geri alma adımı — 14 nesne eklendi; 4 katmanın ayarları değişti.
```

Başarısız bir betik **komutun başarısızlığıdır**: `BETİK` hata döner, çizim betikten
önceki hâlindedir ve yarım kalan kısım `YİNELE` ile geri getirilemez. Ayrıntı:
[Bir betik = bir geri alma adımı](../betik/README.md#bir-betik--bir-geri-alma-adımı).

Çalıştırmadan önce ne yapacağını görmek için:

```text
BETİK tests/journal/ornek-parsel.json onizle=evet
```

```text
Önizleme: 9 adım — uygulanırsa 14 nesne eklenecek; 4 katmanın ayarları değişecek. Çizim değişmedi.
```

### Arayüz

**KentOS CAD ▸ Betik Çalıştır…** ya da **Ctrl+R** bir dosya seçme penceresi açar. Seçtiğiniz betik çalışır ve sonuç
kendiliğinden görünüme sığdırılır. **KentOS CAD ▸ Betiği Önizle…** aynı pencereyi açar ama
betiği çalıştırmaz; ne değiştireceğini komut satırına yazar.

### Betik

Program açılırken betik çalıştırmak için komut satırı seçeneğini kullanın:

```bash
./build/dev/bin/kentos_cad --betik tests/journal/ornek-parsel.json
```

veya

```bash
make run-script SCRIPT=tests/journal/ornek-parsel.json
```

Çalıştırdığınız betiğin kendisi de `core.script` çağırabilir, ama iç içe betik yerine
komutları tek dosyada toplamak daha okunaklıdır.

Örnek bir betik dosyası — olduğu gibi çalışır:

```json
{
  "ad": "İki parsel",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL", "renk": 4281236786 } },
    { "cmd": "core.line",  "args": { "noktalar": [[485300000,4310200000],
                                                  [485360000,4310200000],
                                                  [485360000,4310245000],
                                                  [485300000,4310245000],
                                                  [485300000,4310200000]] } },
    { "cmd": "core.line",  "args": { "noktalar": [[485370000,4310200000],
                                                  [485410000,4310200000],
                                                  [485410000,4310245000],
                                                  [485370000,4310245000],
                                                  [485370000,4310200000]] } },
    { "cmd": "core.zoom",  "args": { "mod": "KAPSAM" } }
  ]
}
```

Koordinatların **milimetre tam sayı** olduğuna dikkat edin: `485300000` mm =
`485300.000` m.

## Geri alma

Betiğin tamamı tek bir geri alma adımıdır. Dokuz komutluk bir betiği çalıştırdıktan sonra
tek `GERİAL` hepsini kaldırır:

```
BETİK tests/journal/ornek-parsel.json     ← 9 komut, 14 nesne, 5 katman
GERİAL                                    ← hepsi kalkar
YİNELE                                    ← hepsi geri gelir
```

`BETİK` komutunun kendisi geri alma yığınına ayrıca girmez; yığına giren, betiğin
yaptığı işin tamamıdır.

## Betikten kullanım

`BETİK` betiklenebilirdir ve salt okunur işaretlidir — çizimi kendisi değiştirmez,
içerdiği komutlar değiştirir.

### Kum havuzu

Betiklerin dosya sistemine erişimi üç seviyeyle sınırlanır:

| Seviye | İzin |
|---|---|
| `güvenli` | Dosya sistemi ve ağ erişimi yok. Varsayılan |
| `proje` | Yalnız proje dizini |
| `tam` | Kullanıcının açık onayı gerekir |

KentOSCad uygulaması betikleri **`proje`** seviyesinde çalıştırır. `güvenli` seviyede bir
betik dosyası açmaya çalışırsanız reddedilir.

### Bu sürümdeki betik dili

Varsayılan yapıda betik motoru yalnız JSON komut dizisi anlar. İfade, döngü ve koşul
yoktur.

`KENTOS_WITH_PYTHON=ON` ile derlenen yapıda `.py` uzantılı bir dosya gömülü **Python**
motoruna gider: değişken, döngü, koşul ve fonksiyon. Aynı komut veri yolunu kullanır,
dolayısıyla bu sayfadaki her kural orada da geçerlidir. Ayrıntı:
[Python betikleri](../betik/python.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Betik dosyası açılamadı: yol` | Dosya yok veya okunamıyor | Yolu ve izinleri denetleyin |
| `Betik dosya erişimi 'güvenli' kum havuzunda kapalıdır. Gerekli seviye: 'proje' veya 'tam'.` | Kum havuzu seviyesi yetersiz | Uygulama içinden çalıştırın |
| `Betik ya bir komut dizisi ya da "komutlar" alanı olan bir nesne olmalı` | Kök yapı yanlış | Dosyayı dizi ya da `komutlar` alanlı nesne yapın |
| `Betik satırı N bir nesne olmalı: 42. Betik çalıştırılmadı.` | N. öğe nesne değil | Her satırı `{ "cmd": ... }` nesnesi yapın; betik hiç çalışmadı, çizim değişmedi |
| `Betik satırı N: "cmd" alanı yok: {...}. Betik çalıştırılmadı.` | N. satırda komut adı yok | `"cmd"` veya `"komut"` alanı ekleyin; betik hiç çalışmadı |
| `Betik satırı N (komut): … Betik çalıştırılmadı.` | N. satırın `args` alanı okunamadı | Mesajdaki alanı düzeltin; betik hiç çalışmadı |
| `Betik satırı 3 (core.line): 'core.line': 'noktalar' parametresi en az 2 değer istiyor, 1 değer geldi. Betik bütünüyle geri alındı; çizim betikten önceki hâlinde.` | Üçüncü satırdaki komut doğrulamayı geçemedi | Satır numarasına gidip düzeltin; önceki satırların transkriptteki yanıtları da geri alındı |
| `Komut argümanları bir JSON nesnesi olmalı.` | `args` nesne değil | `args` alanını `{ }` yapın |
| `Nokta [x_mm, y_mm] biçiminde olmalı. Girilen: [1,2,3]` | Nokta iki bileşenli değil | Noktayı `[x, y]` yapın |
| `Betik motoru bağlı değil.` | Betik motoru olmayan bir ortamda çalışılıyor | Uygulama içinden çalıştırın |
| `Betik hatası: ...` | Betik çalışırken bir hata oluştu; `BETİK` başarısız olur | Mesajın devamı satır numarasını verir; betiğin tamamı geri alındı |
| `Betik önizlenemedi: Python betiği önizlenmez: ne yapacağı ancak çalışınca bellidir. …` | `onizle=evet` bir `.py` betiğine verildi | JSON betiğini ya da komut satırlarını önizleyin ([ÖNİZLE](preview.md)) |
| `Betik önizlenemedi: …` | Betik okunamadı ya da bir satırı bozuk | Mesajın devamı satır numarasını verir |
| `Bu yapıda betik önizlenmez.` | Betik motoru bağlı olmayan bir ortam | Uygulama içinden çalıştırın |

Hata mesajı her zaman **kaçıncı satırda** ve **hangi komutta** olduğunu söyler. Hata
durumunda çiziminiz betikten önceki hâlindedir.

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
