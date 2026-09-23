# YAPIŞTIR — Panodakini Koy

Panoya alınmış bir parseli, bir bloğu ya da bir detayı çizime koyacak herkes
için.

## Ne yapar

Panodaki nesneleri çizime koyar. Katmanları, stilleri ve öznitelikleri birlikte
getirir; çizimde aynı adda bir katman varsa **onun içine** koyar, yenisini
açmaz. Her nesne **bütün** taşınır: yaylı çoklu çizginin yay merkezleri, bloğun
kutusu ve taramanın desen başlangıcı köşeleriyle birlikte gider.

## İki yerleştirme

| Nasıl | Sonuç |
|---|---|
| `nokta=<nokta>` (varsayılan olarak sorulur) | Yükün **taban noktası** — kopyalarken verildiyse o, verilmediyse yükün sol alt köşesi — o noktaya taşınır; bir elin yapıştırmaktan anladığı şey |
| `yerinde=evet` | Her koordinat kopyalandığı gibi kalır — aynı koordinat sistemindeki iki çizim arasında kopyalamanın istediği şey |

## Adlar

| Ad | Tür |
|---|---|
| `YAPIŞTIR` | Türkçe, birincil |
| `YAPISTIR` | ASCII katlanmış Türkçe |
| `PASTE` | İngilizce karşılık |
| `YP` | Kısaltma |
| `core.paste` | Komut kimliği |

## Sözdizimi

```text
YAPIŞTIR <nokta>
YAPIŞTIR yerinde=evet
YAPIŞTIR nokta=<nokta> dosya=<yol>
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nokta` | nokta | 0..1 | Yapıştırılacak yerin sol alt köşesi; `yerinde=evet` ile gereksiz |
| `yerinde` | mantıksal | 0..1 | Kopyalandığı koordinatlara yapıştırır |
| `dosya` | metin | 0..1 | Okunacak pano dosyası; verilmezse ortak pano dosyası |

## Örnekler

### Komut satırı

```text
YAPIŞTIR 485320,4310220
```

```text
Yapıştırıldı: 2 nesne, 1 yeni katman.
```

Aynı koordinatlara:

```text
YAPIŞTIR yerinde=evet
```

Bir dosyadan:

```text
YAPIŞTIR nokta=0,0 dosya="/tmp/blok.pcad"
```

### Arayüz

**Düzen > Yapıştır** ya da **Ctrl+V**; komut yapıştırılacak yeri sorar. Aynı
koordinatlara koymak için komut satırına `YAPIŞTIR yerinde=evet` yazın.

### Betik

```json
{ "cmd": "core.paste", "args": { "nokta": [485320150, 4310220400] } }
```

## Geri alma

Tek işlem, tek **Ctrl+Z**: kaç nesne geldiyse birlikte gider. İki nesnenin iki
adım geri gitmesi, kullanıcının geri alamadığı bir yapıştırma olurdu (Article 1.5).

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Panoda bir şey yok. Önce PANOYAKOPYALA ya da KES ile bir şey alın (pano dosyası: …)` | Pano boş | Bir şey kopyalayın |
| `io.format_too_new: …` | Pano dosyası daha yeni bir sürümle yazılmış | Programı güncelleyin |
| `Katman kilitli: …` | Hedef katman kilitli | `KATMAN` ile kilidi kaldırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [PANOYAKOPYALA](copy_clip.md) — panoya alır
- [KES](cut.md) — panoya alır ve siler
- [İÇEAKTAR](import.md) — dış bir biçimden okur
