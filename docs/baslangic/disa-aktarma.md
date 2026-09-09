# Dışa Aktar penceresi

Çizimini, bir nesnenin köşelerini ya da bir katmanın stilini başka bir programa
götürmek isteyen kullanıcı için; bu sayfayı bitirdiğinizde programdan çıkan her
dosyanın aynı pencereden nasıl yazıldığını ve o pencerenin altında görünen komut
satırının ne işe yaradığını bileceksiniz.

## Tek pencere, üç konu

Programdan dışarı ne çıkıyorsa **Dışa Aktar** penceresinden çıkar. Pencere üç
soruyu sorar — *ne yazıyorum, hangi biçimde, nereye* — ve konuya göre yalnız biçim
listesi ile seçenekler değişir:

| Konu | Nereden açılır | Çalıştırdığı komut |
|---|---|---|
| **Çizim** | **Dosya ▸ Dışa Aktar…**, araç çubuğundaki **Dışa Aktar**, öznitelik tablosundaki **Dışa aktar** işareti | [`DIŞAAKTAR`](../komutlar/export.md) |
| **Bir nesnenin köşeleri** | Öznitelikler panelinde nesneye **sağ tık ▸ Koordinatları dışa aktar…** | [`NOKTALAR … yon=yaz nesneler=…`](../komutlar/points.md) |
| **Bir katmanın stili** | Katman Özellikleri ▸ **Stil ▸ QGIS stiline aktar…** | [`STİLAKTAR`](../komutlar/exportstyle.md) |

## Pencerede ne nerede

```text
┌ ⤓ Dışa Aktar — nesne 4128 köşeleri ───────────────────────────── ? ✕ ┐
│ BİÇİM ─────────────────────    HEDEF ──────────────────────────────  │
│ (•) Nokta listesi (.txt)       Dosya *                                │
│     nokta no; Y; X — noktalı   [ /teslim/koseler.txt      ] [Gözat…] │
│     virgülle ayrılmış                                                 │
│                                SEÇENEKLER ─────────────────────────  │
│                                Sütun sırası                           │
│                                [ Y X | X Y ]                          │
│ KOMUT ── aynı satır komut satırından ve betikten de yazılır ───────  │
│ NOKTALAR dosya="/teslim/koseler.txt" yon=yaz nesneler=4128            │
├───────────────────────────────────────────────────────────────────────┤
│ ? Yardım                                     [ İptal ] [ ⤓ Dışa aktar ] │
└───────────────────────────────────────────────────────────────────────┘
```

- **BİÇİM** — konunun izin verdiği biçimler. Çizim için liste, programın okuyup
  yazabildiği dış biçimlerdir; her biçimin altında bir satır not vardır
  (GeoPackage öznitelikleri de taşır, DXF katmanları ve çizgi tiplerini korur).
- **HEDEF** — yazılacak dosya. **Gözat…** sistemin dosya penceresini seçili biçime
  süzülmüş açar; uzantı yazmazsanız biçimin uzantısı eklenir.
- **SEÇENEKLER** — konuya göre: köşe listesinde sütun sırası (`Y X` Türkiye'de
  olağan sıra, `X Y` matematik sırası isteyen bir program için), stil için katman adı.
- **KOMUT** — pencerenin **Dışa aktar** dediğinizde çalıştıracağı satır, olduğu gibi.
  Dosya adını girdiğiniz anda oluşur. Bu satırı komut satırına yazsanız ya da bir
  betiğe koysanız aynı dosya çıkar; pencere dosyayı kendi yazmaz.

Komut reddederse — sürücü bu yapıda yok, dizin yok, katman adı değişmiş — pencere
kapanmaz; sebep formun üstünde kırmızı şeritte yazar ve düzeltip yeniden
deneyebilirsiniz.

## Klavye

| Tuş | Ne yapar |
|---|---|
| **Tab / Shift+Tab** | biçimler, dosya kutusu, Gözat, seçenekler ve düğmeler arasında gezer |
| **↑ ↓** | biçim seçeneklerinde gezer |
| **Space** | biçimi seçer; segmentte seçeneği değiştirir |
| **Enter** | **Dışa aktar** (varsayılan düğme) |
| **Esc** | İptal |

Öznitelikler panelindeki menü klavyeden **Menü** tuşu ya da **Shift+F10** ile açılır.

## İlgili

- [Dış biçime yazma](../komutlar/export.md) — `DIŞAAKTAR`
- [Ölçü nokta listesi](../komutlar/points.md) — `NOKTALAR`
- [Stili QGIS'e aktarma](../komutlar/exportstyle.md) — `STİLAKTAR`
- [Bileşenler](bilesenler.md) — penceredeki denetimlerin dili
