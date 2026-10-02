# PRİZMA — Dik Ayak ve Dik Boy

Bir tabandan alım yapmış ya da aplikasyona hazırlanan herkes için; bu sayfayı
bitirdiğinizde çizimdeki noktaların iki noktalı bir tabana göre dik ayağını ve dik boyunu
okumayı bileceksiniz.

## Ne yapar

İki noktalı bir taban (A→B) ve bir ya da daha çok nokta alır; her nokta için tabana
indirilen dikmenin ayağını ve boyunu yazar:

| Değer | Anlamı |
|---|---|
| **Dik ayak** | Dikmenin tabanı kestiği yerin A'dan, B'ye doğru uzaklığı. A'nın gerisindeyse eksidir |
| **Dik boy** | Noktanın tabana dik uzaklığı. A'dan B'ye bakarken **sağda pozitif, solda negatif** |

Netcad'in Prizma aracının işidir. İşaret kuralı programın her yerinde aynıdır:
[`dik(A,B,ayak,boy)`](komut-satiri.md#dik-ayak-ve-dik-boy--işaret-kuralı) ve
[`DİKAYAK`](perp_offset.md) noktayı bu iki sayıdan **koyar**, `PRİZMA` aynı iki sayıyı
noktadan **okur**. `dik()` ile konmuş bir nokta `PRİZMA`'da kendi ayağını ve boyunu verir.

Nokta çizimdeki numaralı bir nokta ise (`NOKTALAR` ile okunmuş, `nokta_no` sütunu dolu),
satır noktayı numarasıyla adlandırır.

Hiçbir şey çizmez ve değiştirmez. Taban ve her dikme tuvalde, ayağı ve boyu üzerinde
yazılı bir ölçü işaretiyle kalır; işaretler çizim değişince ya da hiçbir komut
çalışmıyorken **Esc**'e basınca kalkar.

## Adlar

| Ad | Tür |
|---|---|
| `PRİZMA` | Türkçe, birincil — Netcad'deki adı |
| `PRIZMA` | ASCII karşılık |
| `STATIONOFFSET` | İngilizce karşılık |
| `PRZ` | Kısaltma |
| `core.station_offset` | Komut kimliği |

## Sözdizimi

```
PRİZMA <A> <B> <nokta> [<nokta> …]
PRİZMA baslangic=<A> bitis=<B> noktalar=<nokta> …
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `baslangic` | Tabanın başlangıcı, A |
| `bitis` | Tabanın sonu, B. A ile aynı nokta olamaz |
| `noktalar` | Ayağı ve boyu okunacak noktalar; en az bir tane |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Doğuya giden 100 metrelik bir taban ve iki nokta:

```
PRİZMA 0,0 100,0 30,5 70,-2.5
```

Çıktı:

```text
PRİZMA: taban 100,000 m; ayak A'dan B'ye, boy A'dan B'ye bakarken sağda pozitif, solda negatif.
  1. nokta: ayak 30,000 m · boy -5,000 m (solda)
  2. nokta: ayak 70,000 m · boy 2,500 m (sağda)
```

Doğuya giden bir tabanın kuzeyi solundadır; bu yüzden (30,5) noktasının boyu eksidir.

Numaralı bir noktayı numarasıyla sorun:

```
NOKTA 20,3
SÜTUN nokta_no metin "nokta no"
ÖZNİTELİK nokta_no 1 1284
PRİZMA 0,0 100,0 n(1284)
```

Çıktının son satırı:

```text
  1284 numaralı nokta: ayak 20,000 m · boy -3,000 m (solda)
```

### Arayüz

**Harita ▸ Ölçüm** panelindeki **Ölç** düğmesinin okundan **Prizma**'yı seçin. Önce tabanın iki ucunu (A ve B), sonra noktaları
gösterin; her tıklama bir satır ve tuvalde bir dikme bırakır. Taban nokta seçerken
ekranda kalır. **Enter** ya da sağ tık bitirir. Nesne yakalama açıkken tıklama köşeye ve
noktaya oturur; yazılan koordinat yazıldığı yere düşer.

### Betik

```json
{
  "ad": "Tabandan dik ayak ve boy",
  "komutlar": [
    { "cmd": "core.station_offset",
      "args": { "baslangic": [0, 0], "bitis": [100000, 0],
                "noktalar": [[30000, 5000], [70000, -2500]] } }
  ]
}
```

Betikte koordinatlar **milimetredir**. Python'da adı `station_offset`'tir.

### Üçü de aynı

Tuvalde tıklanan, komut satırına yazılan ve betikte verilen aynı noktalar aynı cevabı alır.

## Yapılandırılmış cevap

```json
{ "taban": { "baslangic": [0, 0], "bitis": [100000, 0], "uzunluk_mm": 100000 },
  "noktalar": [ { "nokta": [30000, 5000], "ayak_mm": 30000, "boy_mm": -5000 },
                { "nokta": [70000, -2500], "ayak_mm": 70000, "boy_mm": 2500 } ] }
```

Numaralı bir noktanın satırında `nokta_no` da vardır.

## Geri alma

Geri alınacak bir şey yoktur: `PRİZMA` çizimi değiştirmez, geri alma yığınına ve komut
günlüğüne girmez.

## Betikten kullanım

Betiklenebilir ve yapay zekâ erişimlidir; hiçbir şeyi değiştirmediği için bir yapay zekâ
istemcisi onu onay beklemeden çalıştırabilir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Tabanın iki ucu aynı nokta; bir doğrultu tanımlamıyor.` | A ile B aynı | B'yi A'dan farklı verin |
| `PRİZMA en az bir nokta ister: tabandan sonra ölçülecek noktaları verin.` | Tabandan sonra nokta verilmeden bitirildi | En az bir nokta gösterin ya da yazın |
| `1284 numaralı nokta yok. Nokta listesini NOKTALAR ile okuyun.` | `n(1284)` çizimde yok | Numarayı denetleyin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili sayfalar

- [`DİKAYAK`](perp_offset.md) — dik ayak ve dik boydan nokta koymak
- [Nokta fonksiyonları](komut-satiri.md#nokta-fonksiyonları) — `dik()` ve `n()`
- [`ÖLÇ`](measure.md) — iki nokta arası mesafe ve açı
