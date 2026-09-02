# EŞYÜKSELTİ — Eş Yükselti Eğrileri

## Ne yapar

Kotlu noktalardan **eş yükselti eğrileri** çizer. Bir ekip araziyi nivelman
yapıp birkaç yüz numaralı nokta getirir; paftaya giren şey o noktalar değil,
tasarımcının araziyi okuduğu bu eğrilerdir.

Eğriler kendi katmanına (`ESYUKSELTI`) düşer, çünkü bir pafta onları **küme
olarak** biçimlendirir ve kapatır. Her eğri kendi **kot** özniteliğini taşır —
kotu olmayan bir eğri kimsenin etiketleyemeyeceği bir çizgidir.

## Kotlar `kot` sütunundan gelir

`NOKTALAR` bir saha listesi okurken bu sütunu doldurur. **Kotu olmayan nokta
kullanılmaz**, sıfır sayılmaz: bir yamacın ortasındaki deniz seviyesi noktası
etrafındaki bütün eğrileri aşağı çekerdi.

## Üçgenleme saklanmaz

Ara üründür. Saklamak, programın tamamına — kırpma, seçme, alan, dışa aktarma —
kimsenin çizmediği bir şey için yeni bir nesne türü öğretmek olurdu. Noktalar
değişince yeniden kurulur, ki eğrilerin noktalara sadık kalmasının tek yolu da
budur.

Üçgenlemeyi **CDT** yapar (MPL 2.0). Delaunay'ın bilinen dejenerelikleri vardır —
eş çemberli noktalar, doğrusal diziler, çakışıklar — ve bir nivelman ızgarası
**her yerde** eş çemberlidir: elle yazılmış bir üçgenlemenin çapraz üçgen
ürettiği yer tam orasıdır.

## Kotlar tam katlara düşer

1 m aralık 845, 846, 847 verir — "en alçak nokta artı bir metre" değil. Paftanın
bastığı sayılar bunlardır.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `EŞYÜKSELTİ` | `ESYUKSELTI` | `CONTOUR` | `EŞY` |

## Sözdizimi

```text
EŞYÜKSELTİ [aralik=<mm>] [katman=<ad>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `aralik` | Eş yükselti aralığı, **milimetre**; varsayılan `1000` (1 m) |
| `katman` | Eğrilerin çizileceği katman; varsayılan `ESYUKSELTI` |

Seçim varsa yalnız seçili noktalar, yoksa çizimdeki bütün kotlu noktalar
kullanılır.

## Örnekler

### Komut satırı

```text
KATMAN ad=NIRENGI
NOKTALAR dosya="nivelman.txt"
EŞYÜKSELTİ aralik=500
```

```text
37 eş yükselti eğrisi çizildi (0,500 m aralıkla, 169 kotlu noktadan), 'ESYUKSELTI' katmanına.
```

### Arayüz

Kotlu bir nokta listesi okuyun, sonra `EŞYÜKSELTİ aralik=500` yazın. Eğriler
kendi katmanına düşer; **Katmanlar** panelinden biçimlendirebilir ya da
kapatabilirsiniz.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.layer",   "args": { "ad": "NIRENGI" } },
    { "cmd": "core.points",  "args": { "dosya": "nivelman.txt" } },
    { "cmd": "core.contour", "args": { "aralik": 500 } }
  ]
}
```

## Geri alma

Tek adımdır: bir `EŞYÜKSELTİ` kaç eğri çizmiş olursa olsun tek `GERİAL` hepsini
kaldırır.

## Betikten kullanım

Aralık argüman olduğu için bir betik aynı araziden birkaç aralık üretebilir —
1/1000 için 0,5 m, 1/5000 için 1 m gibi — her birini kendi katmanına.

## Hatalar

> `Çizimde 'kot' sütunu yok. Kotlu bir nokta listesini NOKTALAR ile okuyun.`

Hiç kot bilgisi yok.

> `Kotlu nokta sayısı yetersiz: N. Yüzey en az üç kotlu nokta ister.`

Üçten az noktanın kotu var.

> `Yüzey en az üç FARKLI kotlu nokta ister…`

Noktalar var ama çoğu aynı yerde.

> `Bu noktalardan yüzey kurulamadı: hepsi aynı doğru üzerinde olabilir.`

Noktalar doğrusal; üçgenlenecek bir alan yok.

> `Bu aralıkta eş yükselti eğrisi yok: arazinin kot farkı aralıktan küçük.`

Arazi bu aralığa göre düz.

> `Bu aralık çok fazla eğri veriyor…`

Aralık çok küçük.

> `Üçgenleme bu yapıda yok…`

Program CDT'siz derlenmiş.

## İlgili

- [NOKTALAR](points.md) — kotlu nokta listesi okuma
- [NOKTA](point_draw.md) — tek nokta çizme
