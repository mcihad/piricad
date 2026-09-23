# RENK — Nesne Rengi

Bir yolu kırmızıya, bir yeşil alanı yeşile boyamak ya da bir nesneyi katmanının
rengine geri döndürmek isteyen herkes için; bu sayfayı bitirdiğinizde nesnelerin
çizgi ve dolgu rengini araç kutusundaki renk kutularından, komut satırından ve
betikten değiştirmeyi bileceksiniz.

## Ne yapar

`RENK`, seçili nesnelerin **çizgi rengini**, **dolgu rengini** ya da ikisini birden
değiştirir. Yalnız söylediğiniz değişir: çizgi rengini değiştirdiğinizde kalınlık,
çizgi tipi ve dolgu olduğu gibi kalır.

Renk dört biçimde yazılır:

| Yazım | Anlamı |
|---|---|
| `#RRGGBB` | Onaltılık renk, örnek `#C0392B` |
| `#AARRGGBB` | Saydamlığıyla, örnek `#803366CC` (`80` yarı saydam) |
| renk adı | `siyah`, `kırmızı`, `sarı`, `yeşil`, `camgöbeği`, `mavi`, `macenta`, `gri`, `beyaz` |
| `katman` | Nesnenin katmanının rengine dön |

Dolgu için bunlara ek olarak `yok` yazılır: dolgu kaldırılır.

Renk adları AutoCAD'in 1–8 numaralı renkleridir (7 kâğıtta siyah, koyu ekranda
beyaz olduğu için iki adla yer alır); büyük harf ya da Türkçe karaktersiz yazım da
geçerlidir (`KIRMIZI`, `kirmizi`). Hangi yazımla verirseniz verin günlüğe
onaltılık yazılır.

**Başında `#` şarttır.** Komut satırı `112233` gibi bir yazımı **sayı** olarak okur;
`#` olmadan `11223A` bir renk, `112233` başka bir şey olurdu. Böyle bir değer
reddedilir ve mesaj sebebini söyler. `STİL renk=` komutunun kullandığı tamsayı
biçimi (`0xFFC0392B`) saydamlık baytı taşıdığı için kabul edilir.

### Dolgu bir dolgu katmanıyla boyanır

Bir alanın içi yalnız bir **dolgu katmanıyla** boyanır; düz bir çizginin dolgu
değeri çizilmez. Bu yüzden dolgusu olmayan bir alana `dolgu=` verdiğinizde komut
çizginin altına bir dolgu katmanı ekler — her çizim programındaki "dolgu ve kenar
çizgisi". `dolgu=yok` o katmanı geri kaldırır.

### Çok katmanlı semboller

MPYY gösterimleri gibi birden çok katmanlı bir sembolde çizgi rengi çizgi ve
işaret katmanlarına, dolgu rengi dolgu katmanlarına gider. **Rengi kilitli**
katmanlar kendi renklerini korur: yönetmeliğin siyah bastığı bir sınır, dolgu ne
olursa olsun siyah kalır. Hiç çizgisi olmayan bir sembole çizgi rengi verirseniz
üstüne bir çizgi eklenir.

### Katmana dönüş

`renk=katman` ve `dolgu=katman` rengi katmana geri verir. Nesnenin bütün
özellikleri yeniden katmanınkiyle aynı olduğunda nesne **yeniden katmanını izler**:
katmanın rengi sonradan değişirse nesne de değişir.

## Adlar

| Ad | Tür |
|---|---|
| `RENK` | Türkçe, birincil |
| `COLOR` | İngilizce karşılık |
| `COLOUR` | İngilizce karşılık (İngiliz yazımı) |
| `RNK` | Kısaltma |
| `core.colour` | Komut kimliği |

## Sözdizimi

```text
RENK [nesneler=<k>] [renk=#RRGGBB|<ad>|katman] [dolgu=#RRGGBB|<ad>|yok|katman]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Rengi değişecek nesneler; verilmezse etkin seçim, o da boşsa sorulur |
| `renk` | Çizgi rengi |
| `dolgu` | Dolgu rengi |

İkisi de verilmezse çizgi rengi sorulur; komut satırının yanında renk adları
renk örnekleriyle listelenir, birine tıklamak cevaptır.

## Örnekler

### Komut satırı

Bir çizgiyi kırmızıya boyamak:

```text
ÇİZGİ 0,0 20,0
RENK nesneler=1 renk=kırmızı
```

```text
1 nesnenin rengi değişti (çizgi kırmızı (#FF0000)).
```

Bir parseli koyu yeşil kenarlı ve yarı saydam yeşil dolgulu yapmak, sonra dolguyu
kaldırmak:

```text
ALAN 0,0 30,0 30,20 0,20
RENK nesneler=1 renk=#1E6B3A dolgu=#8000FF00
RENK nesneler=1 dolgu=yok
```

Rengi katmana geri vermek:

```text
RENK nesneler=1 renk=katman dolgu=katman
```

### Arayüz

Sol araç kutusunun en altındaki iki **renk kutusu** o anda elinizde olanı gösterir:
seçim varsa seçili ilk nesnenin çizgi ve dolgu rengini, yoksa etkin katmanınkini —
yeni çizilecek nesnenin rengini. Dolgusuz olan kutu boş ve çaprazlıdır.

1. Boyamak istediğiniz nesneleri seçin (seçmeden de başlayabilirsiniz).
2. Üstteki kutuya (çizgi) ya da alttakine (dolgu) tıklayın. Kutunun yanında renk
   menüsü açılır: dokuz renk örneği, **Başka bir renk…** (renk seçici),
   **Katmanın rengi** ve dolgu için **Dolgu yok**.
3. Bir renge tıklayın. Seçim varsa hemen boyanır; yoksa komut boyanacak nesneleri
   sorar: tıklayın ya da kutu sürükleyin, sonra Enter.

Aynı komut **Değiştir → Renk** menüsünde ve araç kutusunda **Stil Kopyala**
ailesindedir; oradan başlatınca renk komut satırında sorulur.

Klavyeyle: Tab renk kutularına gelir, yukarı/aşağı ok iki kutu arasında gezer,
Enter ya da Boşluk menüyü açar; menüdeki renk sırasında sol/sağ ok gezer, Enter
seçer.

### Betik

```json
{
  "ad": "Yolu kırmızıya boya",
  "komutlar": [
    { "cmd": "core.polyline", "args": { "noktalar": [[0,0],[20000,0]] } },
    { "cmd": "core.colour", "args": { "nesneler": [1], "renk": "#FF0000" } }
  ]
}
```

## Geri alma

Tek adımdır: bir `RENK` kaç nesneyi boyamış olursa olsun tek [`GERİAL`](undo.md)
hepsini eski rengine döndürür.

## Betikten kullanım

Betikte `nesneler` ve en az biri `renk` ya da `dolgu` verilmelidir; betik soruya
cevap veremez. Günlüğe nesnelerin kimlikleri ve renklerin onaltılık yazımı
yazılır, yeniden oynatılan satır aynı rengi verir. Python'dan
`cad.colour(objects=[1], color="#FF0000")` olarak çağrılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Tanınmayan renk: '…'. #RRGGBB (örnek #C0392B), bir renk adı ya da katman yazın. Renk adları: …` | Renk yazımı tanınmadı | Mesajdaki biçimlerden birini yazın |
| `Tanınmayan renk: '112233'. Sayı olarak okundu; rengi # ile yazın. …` | Onaltılık renk `#` olmadan yazıldı | `#112233` yazın |
| `Tanınmayan dolgu: '…'. …` | Dolgu yazımı tanınmadı | `#RRGGBB`, bir renk adı, `yok` ya da `katman` yazın |
| `Hangi renk: renk=#RRGGBB ya da bir renk adı, …` | Betik ne `renk` ne `dolgu` verdi | Birini verin |
| `İşlem yapılacak nesne yok: seçim boş ve 'nesneler' verilmedi.` | Betikte seçim yok ve `nesneler` verilmedi | `nesneler=` verin |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `'…' katmanı kilitli; üzerindeki nesne düzenlenemez. …` | Nesne kilitli bir katmanda; hiçbir nesnenin rengi değişmez | Kilidi `KATMAN ad=<katman> kilitli=hayır` ile açın ([`KATMAN`](layer.md)) |

Bir nesnenin rengi zaten istenen renkse ya da renkleri kilitliyse komut
reddetmez; kaç nesnenin değişmediğini söyler.

## İlgili

- [`STİLKOPYALA`](match_style.md) — bir nesnenin bütün stilini başkalarına uygular
- [`STİL`](style.md) — katmanın rengini ve sembolünü değiştirir
- [`KATMANAT`](set_layer.md) — nesneyi başka bir katmana taşır
