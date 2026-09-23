# ÇİZGİDÜZENLE — Çizgiyi Düzenle

Bir çizgiyi kapatması, açması, yönünü çevirmesi ya da elle sayısallaştırılmış
gereksiz köşelerinden kurtarması gereken herkes için.

## Ne yapar

Dört küçük işlem, ve başka hiçbir komutun yapmadığı şeyler:

| `islem` | Ne yapar | Ne zaman |
|---|---|---|
| `kapat` | Açık çizgiyi kapalı alana çevirir | Sayısallaştırılmış bir sınır alan olmalı |
| `ac` | Kapalı alanı açık çizgiye çevirir | `UÇUCA` ya da `KIR` için |
| `ters` | Köşe sırasını çevirir | Ofsetin öbür tarafa gitmesi, kilometrajın öbür uçtan sayılması |
| `sadelestir` | Şekle bilgi katmayan köşeleri atar | Elle sayısallaştırılmış bir dere, taranmış bir pafta |

`sadelestir`, bir köşenin komşuları arasındaki doğruya olan **dik uzaklığını**
ölçer; `tolerans=`'tan yakın olan atılır. **Uçlar hiçbir zaman atılmaz** — onlar
çizginin neye değdiği yerdir.

### Yaylı çoklu çizgi

Kenarları yay olan bir çoklu çizgide (DXF'ten gelen bir bordür, [`UÇUCA`](join.md)
ile yaya eklenmiş bir çizgi) `kapat`, `ac` ve `ters` **yayları koruyarak**
çalışır: `kapat` son köşeden ilk köşeye düz bir kapanış kenarı ekler, `ac` kapanış
kenarını — düz ya da yay — kaldırır, `ters` her yayı öbür yönde yürür; hiçbir yay
kirişine indirgenmez. `sadelestir` yaylı çoklu çizgide çalışmaz, çünkü köşe atmak
yayların uçlarını atmak olurdu.

## Adlar

| Ad | Tür |
|---|---|
| `ÇİZGİDÜZENLE` | Türkçe, birincil |
| `CIZGIDUZENLE` | ASCII katlanmış Türkçe |
| `PEDIT` | İngilizce karşılık |
| `ÇZD` / `CZD` | Kısaltma |
| `core.pedit` | Komut kimliği |

## Sözdizimi

```text
ÇİZGİDÜZENLE nesne=<kimlik> islem=kapat|ac|ters
ÇİZGİDÜZENLE nesne=<kimlik> islem=sadelestir tolerans=<m>
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | seçim | 0..n | Düzenlenecek çizgiler |
| `islem` | sözcük | 0..1 | `kapat`, `ac`, `ters`, `sadelestir` |
| `tolerans` | sayı | 0..1 | `sadelestir`: bu uzaklıktan yakın köşeler atılır (m) |

## Örnekler

### Komut satırı

```text
ÇİZGİDÜZENLE nesne=1 islem=kapat
ÇİZGİDÜZENLE nesne=1 islem=ters
ÇİZGİDÜZENLE nesne=1 islem=sadelestir tolerans=0.05
```

```text
1 çizgi düzenlendi (sadelestir), 12 köşe atıldı.
```

### Arayüz

**Değiştir > Çizgi Düzenle** ya da sol araç sütunundaki **köşe ailesi**. Çizgileri
seçip Enter'a basın, sonra işlemi yazın (`kapat`, `ac`, `ters`, `sadelestir`);
`sadelestir` toleransı da ister ve odak kendiliğinden komut satırına geçer.
Tanınmayan bir işlem yazılırsa komut hiçbir çizgiye dokunmadan söyler.

### Betik

```json
{ "cmd": "core.pedit", "args": {
    "nesne": [1, 2], "islem": "sadelestir", "tolerans": 0.05 } }
```

## Geri alma

Tek işlem, tek **Ctrl+Z**: kaç çizgi düzenlediyse birlikte geri döner.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır; her parametre için tip ve adet üretilmiş
[komut referansındadır](referans.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Sadeleştirme toleransı sıfırdan büyük olmalı.` | Tolerans verilmedi ya da sıfır | Artı bir değer verin |
| `Nesne N bir daire; ÇİZGİDÜZENLE çizgilerle, yaylı çoklu çizgilerle ve alanlarla çalışır.` | Daire, yay, elips ya da spline seçildi | Bir eğrinin köşesi yoktur |
| `Nesne N yaylı bir çoklu çizgi; sadeleştirmek yaylarının uçlarını atardı. Yayları korumak için sadeleştirmeyin; gerekirse önce PATLAT ile ayırın.` | Yaylı çoklu çizgiye `sadelestir` | Sadeleştirmeyin ya da önce `PATLAT` ile ayırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [UÇUCA](join.md) — uçları değen çizgileri tek çizgi yapar
- [ALANAÇEVİR](to_area.md) — kapalı çizgiyi alan NESNESİNE çevirir (bu, halka rolünü değiştirir)
- [BÖLÜMLE](divide.md) — `ters` sonrası kilometraj öbür uçtan sayılır
