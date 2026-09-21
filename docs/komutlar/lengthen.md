# UZUNLUK — Uzunluğu Değiştir

"Bordürü 2 metre daha uzat", "kenarı 48 metreye getir" gibi bir talimatı
uygulayacak herkes için.

## Ne yapar

Çizginin bir ucunu **kendi doğrultusunda** hareket ettirerek uzunluğunu
değiştirir. `UZAT`tan farkı: `UZAT` bir sınıra kadar uzatır ve uzatılacak bir
şey ister; bu hiçbir şey istemez, çizginin kendisi yeter.

Çok köşeli bir çizgide yalnız **son parça** değişir: ölçülmüş köşeler yerinde
kalır, ki "bunu 60 metre yap" bir poligon kenarında bunu demektir.

## Üç yol, tam olarak biri

| Parametre | Anlamı |
|---|---|
| `delta=<m>` | Eklenecek uzunluk; eksi kısaltır |
| `yuzde=<n>` | İstenen uzunluk, şimdikinin yüzdesi |
| `toplam=<m>` | İstenen toplam uzunluk |

İkisini birden vermek, hangisini kastettiğini bilmeyen bir çağrıdır ve
**reddedilir** — ret cümlesi şimdiki uzunluğu söyler.

`uc=bas` başlangıç ucunu hareket ettirir; varsayılan `son`dur, çünkü bir çizgi
gittiği yöne doğru çizilir.

## Adlar

| Ad | Tür |
|---|---|
| `UZUNLUK` | Türkçe, birincil |
| `LENGTHEN` | İngilizce karşılık |
| `UZN` | Kısaltma |
| `core.lengthen` | Komut kimliği |

## Sözdizimi

```text
UZUNLUK nesne=<kimlik> delta=<m> [uc=son|bas]
UZUNLUK nesne=<kimlik> yuzde=<n> [uc=son|bas]
UZUNLUK nesne=<kimlik> toplam=<m> [uc=son|bas]
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | seçim | 1 | Uzunluğu değişecek çizgi |
| `delta` | sayı | 0..1 | Eklenecek uzunluk (m); eksi kısaltır |
| `yuzde` | sayı | 0..1 | İstenen uzunluk, şimdikinin yüzdesi |
| `toplam` | sayı | 0..1 | İstenen toplam uzunluk (m) |
| `uc` | sözcük | 0..1 | `son` (varsayılan) ya da `bas` |

## Örnekler

### Komut satırı

```text
UZUNLUK nesne=1 delta=20
UZUNLUK nesne=1 yuzde=150
UZUNLUK nesne=1 toplam=60
UZUNLUK nesne=1 toplam=120 uc=bas
```

```text
Çizgi uzunluğu 100.000000 m -> 120.000000 m yapıldı.
```

### Arayüz

**Değiştir > Uzunluk**. Çizgiyi seçip Enter'a basın; komut **eklenecek uzunluğu
sorar** (eksi değer kısaltır) ve şimdiki uzunluğu istemde yazar.

`yuzde=` ve `toplam=` **yazılan yollardır**: bir sayı hangisi olduğunu söyleyemez,
bu yüzden fareyle sorulan `delta`'dır — elin sorduğu soru odur ("bu ucu şu kadar
uzat"). Baştan biri verilmişse hiç sorulmaz.

### Betik

```json
{ "cmd": "core.lengthen", "args": { "nesne": [1], "toplam": 48.5 } }
```

## Geri alma

Tek işlem, tek **Ctrl+Z**.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır; her parametre için tip ve adet üretilmiş
[komut referansındadır](referans.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Tam olarak birini verin: delta=, yuzde= ya da toplam=` | Hiçbiri ya da birden çoğu verildi | Birini verin |
| `İstenen uzunluk sıfır ya da eksi olamaz.` | `delta` çizgiyi tüketiyor | Daha küçük bir kısaltma verin |
| `İstenen uzunluk, hareket etmeyen köşelerin toplamından küçük` | Çok köşeli çizgide son parça eksi kalıyor | Daha büyük bir toplam verin ya da `KIR` kullanın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [UZAT](extend.md) — bir sınıra kadar uzatır
- [BUDA](trim.md) — bir sınıra kadar kısaltır
- [KIR](break.md) — parça çıkarır
