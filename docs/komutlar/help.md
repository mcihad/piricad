# YARDIM — Yardım

Hangi komutun ne yaptığını hatırlamak isteyen herkes için; bu sayfayı bitirdiğinizde
program içinden komut listesine ve tek bir komutun parametrelerine ulaşabileceksiniz.

## Ne yapar

Parametresiz çağrıldığında bütün komutları adları ve açıklamalarıyla listeler. `komut`
parametresi verildiğinde tek bir komutun parametrelerini, tiplerini ve adetlerini yazar.

Listenin kaynağı komut kaydıdır. Yeni bir komut eklendiğinde `YARDIM` onu **kendiliğinden**
bilir; elle güncellenen ikinci bir liste yoktur.

## Adlar

| Ad | Tür |
|---|---|
| `YARDIM` | Türkçe, birincil |
| `HELP` | İngilizce karşılık |
| `?` | Kısaltma |
| `core.help` | Komut kimliği |

## Sözdizimi

```
YARDIM
YARDIM komut=<komut-adı>
```

## Parametreler

Tek parametresi vardır: **`komut`** — ayrıntısı istenen komutun adı. İsteğe bağlıdır.
Türkçe adı, İngilizce karşılığını, kısaltmasını veya komut kimliğini yazabilirsiniz.

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Bütün komutları listele:

```
YARDIM
```

Çıktı:

```text
Komutlar (8):
    ÇİZGİ, CIZGI, LINE, Ç, L  — İki veya daha fazla nokta arasında doğru parçaları çizer.
    SİL, SIL, ERASE, E  — Seçilen nesneleri siler.
    KATMAN, LAYER, KAT  — Katman oluşturur, aktif yapar ve özelliklerini değiştirir.
    YAKINLAŞ, YAKINLAS, ZOOM, Z  — Görünümü çizim kapsamına veya verilen çarpana ayarlar.
    GERİAL, GERIAL, UNDO, U  — Son işlemi geri alır.
    YİNELE, YINELE, REDO  — Geri alınan işlemi yineler.
    BETİK, BETIK, SCRIPT  — Bir betik dosyasını komut veri yolu üzerinden çalıştırır.
    YARDIM, HELP, ?  — Komut listesini veya tek bir komutun ayrıntısını gösterir.
```

Tek bir komutun ayrıntısı:

```
YARDIM komut=ÇİZGİ
```

Çıktı:

```text
core.line  (ÇİZGİ, CIZGI, LINE, Ç, L)  — İki veya daha fazla nokta arasında doğru parçaları çizer.
    noktalar : point_list [en az 2]  Ardışık doğru parçalarının köşe noktaları
```

Kısaltma da çalışır:

```
? komut=KAT
```

### Arayüz

**Yardım > Komut Listesi** menüsü aynı listeyi biçimlendirilmiş bir tablo hâlinde açar:
komut kimliği, adları, kategorisi, geri alma davranışı, özellikleri ve açıklaması.

**Yardım > Hakkında** sürüm, lisans, etkin çizim arka ucu ve komut sayısını gösterir.

### Betik

```json
{
  "ad": "Komutları listele",
  "komutlar": [
    { "cmd": "core.help", "args": {} }
  ]
}
```

Çıktı transkripte yazılır. Betikte nadiren gerekir; genellikle komut satırından
kullanılır.

## Geri alma

`YARDIM` çizime dokunmaz, geri alma yığınına girmez. Salt okunur bir komuttur.

## Betikten kullanım

`YARDIM` betiklenebilir ve salt okunur işaretlidir.

Komut kataloğunun tamamını makine biçiminde istiyorsanız `YARDIM` yerine üretilmiş
[komut referansına](referans.md) bakın; orada her komutun tam parametre tablosu ve AI
araç şemasının JSON hâli de vardır. Referansı yeniden üretmek için:

```bash
make reference
```

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bilinmeyen komut: 'XYZ'` | `komut=` ile verilen ad bulunamadı | Parametresiz `YARDIM` ile listeye bakın |
| `'core.help': bilinmeyen parametre 'ad'. Tanımlı parametreler: komut` | Parametre adı yanlış | `komut=` yazın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
