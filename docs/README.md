# PiriCAD Kullanıcı Kılavuzu

Türkiye odaklı CBS + CAD harita yazılımının kullanıcı belgeleri. Harita mühendisi,
şehir plancısı ve kadastro teknisyeni için yazıldı.

## Nereden başlamalı

Daha önce PiriCAD kullanmadıysanız sırayla okuyun:

1. [Kurulum ve derleme](baslangic/kurulum.md) — programı çalışır hâle getirin
2. [İlk adımlar](baslangic/ilk-adimlar.md) — on dakikada ilk çiziminiz
3. [Arayüz](baslangic/arayuz.md) — pencerede ne nerede
4. [Komut sistemi](komutlar/README.md) — PiriCAD'in çalışma mantığı

## Komutlar

| Sayfa | İçerik |
|---|---|
| [Komut sistemi](komutlar/README.md) | Komut nedir, istemciler neden eşittir, günlük ne işe yarar |
| [Komut satırı](komutlar/komut-satiri.md) | Koordinat girişi, ifadeler, geçmiş, kısaltmalar |
| [Komut referansı](komutlar/referans.md) | Bütün komutların üretilmiş tablosu |

Tek tek komutlar:

| Komut | Sayfa |
|---|---|
| `ÇİZGİ` | [Çizgi çizme](komutlar/line.md) |
| `SİL` | [Nesne silme](komutlar/erase.md) |
| `KATMAN` | [Katman yönetimi](komutlar/layer.md) |
| `YAKINLAŞ` | [Görünüm ayarlama](komutlar/zoom.md) |
| `GERİAL` | [Geri alma](komutlar/undo.md) |
| `YİNELE` | [Yineleme](komutlar/redo.md) |
| `BETİK` | [Betik çalıştırma](komutlar/script.md) |
| `YARDIM` | [Yardım](komutlar/help.md) |

## İleri konular

| Sayfa | İçerik |
|---|---|
| [Betik yazma](betik/README.md) | JSON betik biçimi, toplu işlem, kum havuzu |
| [Komut günlüğü](mimari/gunluk.md) | Yaptığınız işi geri izleme, makro, oturum kaydı |
| [Koordinat sistemleri](veri/koordinat-sistemleri.md) | TUREF/TM30, TM 3° dilimleri, milimetre depolama |
| [Sürüm ve uyumluluk politikası](api-stability.md) | Neyin sabit kaldığı, neyin değişebileceği |

## Başvuru

| Sayfa | İçerik |
|---|---|
| [Sözlük](sozluk.md) | Haritacılık, imar ve PiriCAD terimleri |
| [Sorun giderme](sorun-giderme.md) | Hata mesajları, sebepleri ve çözümleri |

## Bu kılavuz hakkında

Kullanıcı belgelerinin tamamı `/docs` altında, Markdown biçiminde ve Türkçedir. Bu
bir tercih değil, projenin anayasasında yazılı kati bir kuraldır: belgelenmemiş
özellik yayımlanmamış sayılır (`CLAUDE.md` Article 11). Bir komut kaydedilip
sayfası yazılmazsa derleme kırılır.

[Komut referansı](komutlar/referans.md) elle yazılmaz; komut kaydından üretilir
(`make reference`). Elle düzenlenirse CI kapısı fark eder.

Katkı kuralları ve mimari gerekçeler burada değil, depo kökündeki `CLAUDE.md` ile
`.claude/` altındadır; onlar İngilizcedir ve geliştiriciye hitap eder.
