# PiriCAD

**Türkiye odaklı CBS + CAD harita yazılımı.**
GPLv3 · C++20 · Qt 6 · komut merkezli mimari · BÖHHBÜY / MPYY / TUCBS / TKGM uyumu.

---

## Tek Kural

> **Uygulamanın durumunu değiştiren her şey bir komuttur.
> Arayüz, komut veri yolunun sadece bir istemcisidir.**

```
   GUI butonu ─┐
   Komut satırı ┤
   Betik ───────┼──►  KOMUT VERİ YOLU  ──►  Doğrulama ──►  İşlem ──►  Doküman
   AI ──────────┤        (Command Bus)          │
   Toplu iş ────┘                               └──►  Günlük (Journal)
```

Bu diyagramdaki hiçbir istemcinin ayrıcalığı yok. Bir araç çubuğu butonu, `ÇİZGİ`
komutunu bir betiğin çağırdığı şekilde çağırır — ve bu **test edilmiş bir
iddiadır**, niyet beyanı değil: `tests/unit/test_proof.cpp`.

## Hızlı Başlangıç

```bash
make doctor        # bu makine neyi derleyebiliyor?
make build         # her şeyi derle
make test          # testler + CI kapıları
make run           # PiriCAD'i başlat
make help          # bütün hedefler
```

Örnek bir çizimle açmak için:

```bash
make run-script SCRIPT=tests/journal/ornek-parsel.json
```

## Komut Satırı

```
ÇİZGİ 485320.150,4310220.400 @50,30 @100<45     mutlak · göreli · kutupsal
ÇİZGİ @(100*3),0                                satır içi ifade
KATMAN ad=PARSEL gorunur=evet renk=4290822336   anahtar=değer
YAKINLAŞ KAPSAM                                 şeffaf komut
YARDIM  ·  YARDIM komut=ÇİZGİ                   üretilmiş yardım
```

Komut adları çift dilli ve kısaltmalı: `ÇİZGİ` = `CIZGI` = `LINE` = `Ç` = `L`.
Büyük harf dönüşümü Türkçe kurallarına göre yapılır (`i` → `İ`, `ı` → `I`);
`std::toupper` bu projede yasaktır.

## Depo Yapısı

| Dizin | İçerik |
|---|---|
| `src/core` | Qt'siz çekirdek: sabit-nokta koordinat, SoA geometri, doküman |
| `src/command` | Komut veri yolu, kayıt, coroutine, işlem, günlük, **tek ayrıştırıcı** |
| `src/render` | Sahne kurulumu, görünüm dönüşümü, arka uç arayüzü |
| `src/script` | Betik motoru (Faz 0: JSON; Faz 2: Lua + Python) |
| `src/app` | Qt Widgets kabuğu |
| `src/io` `src/ai` `src/domain` `src/plugin-api` | Faz 1–3 |
| `data/` | Mevzuat katalogları, CRS gridleri, mevzuat korpusu — **veri, kod değil** |
| `tests/` | unit · golden · bench · fuzz · journal · ai-eval |
| `scripts/` | CI kapıları |

## Kurallar

- **[CLAUDE.md](CLAUDE.md)** — projenin anayasası. Önce bu okunur.
- **[.claude/](.claude/)** — her motorun kendi kesin kuralları ve kesin yasakları.
- **[piricad.md](piricad.md)** — teknik referans ve yol haritası (niyetin kaynağı).

Her kural bir CI kapısına, teste veya benchmark'a bağlıdır. Bağlanamayan kural
yanlış yazılmıştır.

## Faz 0 Sapmaları

Üç tanesi var, üçü de CLAUDE.md Article 8'de kaldırma koşuluyla birlikte yazılı:
canvas `QRhiWidget` yerine `QPainter` (`qsb` kurulu değil), Qt dışında bağımlılık
yok, betik motoru JSON komut dizisi. `make doctor` her birinin durumunu söyler.

## Lisans

GPLv3 veya sonrası. Sunucu/web bileşenleri AGPLv3. Ayrıntı: [LICENSE](LICENSE),
[NOTICE](NOTICE).
