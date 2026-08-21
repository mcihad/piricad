# ÇİZGİ — Çizgi Çizme

Çizim yapan herkes için; bu sayfayı bitirdiğinizde fareyle, klavyeyle ve betikle çizgi
çizebilecek ve dördü de aynı sonucu verdiğini bileceksiniz.

## Ne yapar

Verdiğiniz noktaları sırayla birleştirerek doğru parçaları çizer. Üç nokta verirseniz
iki segment, dört nokta verirseniz üç segment oluşur.

Her segment ayrı bir nesnedir — tek tek silinebilir, tek tek seçilebilir. Ama komutun
tamamı **tek bir geri alma adımıdır**.

Çizgiler aktif katmana çizilir. Aktif katmanı durum çubuğunun sağında görür, `KATMAN`
komutuyla değiştirirsiniz.

## Adlar

| Ad | Tür |
|---|---|
| `ÇİZGİ` | Türkçe, birincil |
| `CIZGI` | Türkçe karaktersiz klavye için |
| `LINE` | İngilizce karşılık |
| `Ç` | Kısaltma |
| `L` | Kısaltma |
| `core.line` | Komut kimliği — betikler ve otomasyon için, hiç değişmez |

## Sözdizimi

```
ÇİZGİ
ÇİZGİ <nokta> <nokta> [<nokta> ...]
ÇİZGİ noktalar=<nokta listesi>
```

Argümansız çağırırsanız komut noktaları tek tek sorar. Noktaları satırda verirseniz
komut hemen çizer ve biter.

## Parametreler

Tek parametresi vardır: **`noktalar`** — en az iki nokta. Ardışık doğru parçalarının
köşe noktalarıdır; birinci ile ikinci arasında bir segment, ikinci ile üçüncü arasında
bir segment, diye devam eder.

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

Noktalar dört biçimde girilebilir — mutlak, göreli, kutupsal ve satır içi ifade.
Hepsinin ayrıntısı: [Komut satırı](komut-satiri.md).

## Örnekler

### Komut satırı

Üç köşeli bir çizgi, üç farklı koordinat biçimiyle:

```
ÇİZGİ 485320.150,4310220.400 @50,30 @100<45
```

Bir parsel sınırı — başladığı noktaya dönen kapalı bir çokgen:

```
ÇİZGİ 485300,4310200 485360,4310200 485360,4310245 485300,4310245 485300,4310200
```

Satır içi hesapla, tam üç yüz metrelik yatay bir çizgi:

```
ÇİZGİ 485300,4310200 @(100*3),0
```

Adım adım, noktaları teker teker vererek:

```
ÇİZGİ                       ← Enter; komut "İlk nokta" ister
485320.150,4310220.400      ← Enter; komut "Sonraki nokta" ister
@50,30                      ← Enter
@100<45                     ← Enter
                            ← Esc; komut biter
```

### Arayüz

Sol kenardaki araç kutusundan **Çizgi** düğmesine basın veya **Çizim > Çizgi**
menüsünü kullanın. Durum çubuğu `İlk nokta` ister; harita alanına sol tıklayın.

Sonraki istek `Sonraki nokta` olur ve son noktadan imlecinize kesikli bir kılavuz çizgi
uzanır. İstediğiniz kadar nokta tıklayın.

Bitirmek için **Esc**'e basın veya sağ tıklayın.

Fareyle tıklamak yerine, komut çalışırken komut satırına koordinat da yazabilirsiniz.
Komutun bakış açısından ikisi arasında hiçbir fark yoktur.

### Betik

```json
{
  "ad": "Üç köşeli çizgi",
  "komutlar": [
    { "cmd": "core.line",
      "args": { "noktalar": [[485320150, 4310220400],
                             [485370150, 4310250400],
                             [485440861, 4310321111]] } }
  ]
}
```

Betikte koordinatlar **milimetre tam sayıdır**, metre değil. `485320150` milimetre =
`485320.150` metre. Ayrıntı: [Betik yazma](../betik/README.md).

Çalıştırmak için:

```bash
make run-script SCRIPT=cizgi.json
```

### Üçü de aynı sonucu verir

Yukarıdaki üç yol — fare tıklamaları, komut satırı ve JSON betiği — tıpatıp aynı çizimi
ve tıpatıp aynı günlük satırını üretir. Bu, her derlemede otomatik olarak sınanır.

## Geri alma

Komutun tamamı tek bir geri alma adımıdır. Dört noktalı bir çizgi üç segment yaratır ama
tek `GERİAL` ile üçü birden kalkar.

```
GERİAL
```

Transkript ne geri alındığını söyler:

```
Geri alındı: İki veya daha fazla nokta arasında doğru parçaları çizer.
```

Bkz. [Geri alma](undo.md) ve [Yineleme](redo.md).

## Betikten kullanım

`ÇİZGİ` betiklenebilir ve AI erişimlidir. Betikte `noktalar` argümanına bütün noktaları
tek seferde verirsiniz:

```json
{ "cmd": "core.line", "args": { "noktalar": [[0,0],[10000,0],[10000,10000]] } }
```

Aynı betikte birden fazla `core.line` satırı olabilir; hepsi tek bir geri alma adımı
oluşturur.

Nokta listesi tek tek okunur: komut, bir kullanıcının fareyle tıklaması gibi, listeden
sırayla nokta alır ve liste bitince tıpkı **Esc**'e basılmış gibi durur. Bu yüzden
komutun kodu arayüzden mi betikten mi çağrıldığını bilmez.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `'core.line': 'noktalar' parametresi en az 2 değer istiyor, 1 değer geldi.` | Tek nokta verilmiş | İkinci noktayı ekleyin |
| `'core.line': zorunlu 'noktalar' parametresi eksik. Beklenen: nokta listesi` | Hiç nokta verilmemiş | En az iki nokta girin |
| `'core.line': 'noktalar' parametresi nokta listesi bekliyor. Girilen: 'abc'` | Koordinat yerine metin girilmiş | Koordinat biçimlerinden birini kullanın |
| `'PARSEL' katmanı kilitli.` | Aktif katman kilitli | `KATMAN ad=PARSEL kilitli=hayır` ile kilidi açın veya başka katmana geçin |
| `Bir çoklu çizgi en az 2 tepe noktası ister, verilen: 1` | Betikten tek noktalı liste gelmiş | Listeye ikinci noktayı ekleyin |
| `Beklenen: '@dx,dy' veya '@mesafe<açı'. Girilen: '@50'` | Göreli koordinat eksik yazılmış | `@50,0` veya `@50<0` yazın |

İlk noktayı vermeden **Esc**'e basarsanız hata olmaz; komut hiç çalışmamış sayılır,
geri alma adımı bırakmaz ve günlüğe yazılmaz. Transkriptte `İptal edildi` görünür.

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
