# PiriCAD Kullanıcı Kılavuzu

Türkiye odaklı CBS + CAD harita yazılımının kullanıcı belgeleri. Harita mühendisi,
şehir plancısı ve kadastro teknisyeni için yazıldı.

## Nereden başlamalı

Daha önce PiriCAD kullanmadıysanız sırayla okuyun:

1. [Kurulum ve derleme](baslangic/kurulum.md) — programı çalışır hâle getirin
2. [İlk adımlar](baslangic/ilk-adimlar.md) — on dakikada ilk çiziminiz
3. [Arayüz](baslangic/arayuz.md) — pencerede ne nerede
4. [Stil tasarımcısı](baslangic/stil-tasarimcisi.md) — bir katmanın nasıl çizileceğini tasarlayın
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
| `AÇ` | [Proje dosyası açma](komutlar/open.md) |
| `KAYDET` | [Çizimi kaydetme](komutlar/save.md) |
| `FARKLIKAYDET` | [Yeni ada kaydetme](komutlar/saveas.md) |
| `İÇEAKTAR` | [Dış veri alma](komutlar/import.md) |
| `DIŞAAKTAR` | [Dış biçime yazma](komutlar/export.md) |
| `VERİTABANI` | [PostGIS ile çalışma](komutlar/database.md) |
| `ÇİZGİ` | [Çizgi çizme](komutlar/line.md) |
| `ALAN` | [Kapalı alan çizme](komutlar/area.md) |
| `DİKDÖRTGEN` | [İki köşeden dörtgen ve kare çizme](komutlar/rectangle.md) |
| `METİN` | [Çizime yazı yazma](komutlar/text.md) |
| `STİLAKTAR` | [Stili QGIS'e aktarma](komutlar/exportstyle.md) |
| `ÖZNİTELİK` | [Nesnelerin verisi](komutlar/attribute.md) |
| `SÜTUN` | [Öznitelik sütunu tanımlama](komutlar/column.md) |
| `SEÇ` | [Nesne seçme](komutlar/select.md) |
| `SİL` | [Nesne silme](komutlar/erase.md) |
| `KATMAN` | [Katman yönetimi](komutlar/layer.md) |
| `STİL` | [Nesne stili ve gösterim kataloğu](komutlar/style.md) |
| `SEMBOL` | [Gösterim rafı](komutlar/symbol.md) |
| `ETİKET` | [Özniteliklerden yazı](komutlar/label.md) |
| `YAKINLAŞ` | [Görünüm ayarlama](komutlar/zoom.md) |
| `GERİAL` | [Geri alma](komutlar/undo.md) |
| `YİNELE` | [Yineleme](komutlar/redo.md) |
| `BETİK` | [Betik çalıştırma](komutlar/script.md) |
| `AYAR` | [Proje ayarları](komutlar/setting.md) |
| `TERCİH` | [Uygulama tercihleri](komutlar/preference.md) |
| `MOD` | [Oturum modları](komutlar/mode.md) |
| `YARDIM` | [Yardım](komutlar/help.md) |

## İleri konular

| Sayfa | İçerik |
|---|---|
| [Betik yazma](betik/README.md) | JSON betik biçimi, toplu işlem, kum havuzu |
| [Lua betikleri](betik/lua.md) | Döngü, koşul ve hesapla betik yazma; `h` API'si |
| [Komut günlüğü](mimari/gunluk.md) | Yaptığınız işi geri izleme, makro, oturum kaydı |
| [Koordinat sistemleri](veri/koordinat-sistemleri.md) | TUREF/TM30, TM 3° dilimleri, milimetre depolama |
| [PiriCAD proje dosyası](veri/proje-dosyasi.md) | `.pcad` ne taşır, sürüm politikası, bozuk dosya |
| [Dış veri biçimleri](veri/dis-formatlar.md) | DXF ve GeoPackage, koordinat sistemi, `.prj` dosyası |
| [Öznitelik tablosu](veri/oznitelik-tablosu.md) | Satırları süzme, düzenleme, alan istatistikleri; süzme ifadesinin dilbilgisi |
| [MPYY plan gösterimleri](veri/mpyy-gosterimleri.md) | Gösterim veri paketi: hangi RG sürümü, ne çıkarıldı, ne eksik, nasıl yeniden üretilir |
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
