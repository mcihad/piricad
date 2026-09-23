# KES — Panoya Al ve Sil

Bir şeyi bir çizimden alıp başka bir yere **taşıyacak** herkes için.

## Ne yapar

Seçili nesneleri panoya alır ve çizimden siler. Kopyalama ve silme **tek bir
işlemdir**, dolayısıyla tek bir **Ctrl+Z** ikisini birden geri alır — kopyası
kendi geri almasından sağ çıkan bir kes, kullanıcının geri alamadığı bir kes
olurdu.

Geri alma panoyu **boşaltmaz**: kesilen şey panoda durmaya devam eder, ki bir
kesmenin bütün amacı budur.

Pano yükü ve nerede durduğu için bkz. [PANOYAKOPYALA](copy_clip.md).

## Adlar

| Ad | Tür |
|---|---|
| `KES` | Türkçe, birincil |
| `CUT` | İngilizce karşılık |
| `KS` | Kısaltma |
| `core.cut` | Komut kimliği |

## Sözdizimi

```text
KES [nesneler=<kimlik> …] [dosya=<yol>]
KES [nesneler=<kimlik> …] taban=<nokta>
KES tabanli=evet
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesneler` | seçim | 0..n | Kesilecek nesneler; verilmezse **seçim** kullanılır |
| `dosya` | metin | 0..1 | Panonun yazılacağı dosya; verilmezse ortak pano dosyası |
| `taban` | nokta | 0..1 | Yapıştırırken gösterilen yere gelecek **taban noktası**; verilmezse nesnelerin sol alt köşesi |
| `tabanli` | mantıksal | 0..1 | `evet`: taban noktası nesneler seçildikten sonra sorulur |

## Örnekler

### Komut satırı

```text
SEÇ mod=KUTU noktalar=-1,-1 41,31
KES
```

```text
Panoya alındı: 1 nesne (912 bayt). 1 nesne çizimden silindi.
```

### Arayüz

**Düzen > Kes** ya da **Ctrl+X**. Önce nesneleri seçebilirsiniz; seçim boşsa
komut hangi nesnelerin kesileceğini sorar: tıklayın ya da kutu sürükleyin, sonra
Enter.

### Betik

```json
{ "cmd": "core.cut", "args": { "nesneler": [1] } }
```

## Geri alma

Tek işlem, tek **Ctrl+Z**: kopyalama ve silme birlikte geri döner ve pano dolu
kalır.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `İşlem yapılacak nesne yok: seçim boş ve 'nesneler' verilmedi.` | Betik ne `nesneler` verdi ne seçim vardı | `SEÇ` ile seçin ya da `nesneler=` yazın |
| `Katman kilitli: …` | Nesnenin katmanı kilitli | `KATMAN` ile kilidi kaldırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [PANOYAKOPYALA](copy_clip.md) — silmeden panoya alır
- [YAPIŞTIR](paste.md) — panodakini çizime koyar
- [SİL](erase.md) — panoya almadan siler
