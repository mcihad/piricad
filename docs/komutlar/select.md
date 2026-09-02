# SEÇ — Nesne Seçme

Çizimde nesne seçmek isteyen kullanıcı için; bu sayfayı bitirdiğinizde fareyle,
komut satırından ve betikten seçim yapmayı, seçime ekleyip çıkarmayı ve seçimi
silmeyle birlikte kullanmayı bileceksiniz.

## Ne yapar

`SEÇ`, üzerinde işlem yapılacak nesneleri belirler. Seçim yapmanın altı yolu vardır:
bütün nesneler, kimlik listesi, pencere kutusu, kesen kutu, sürükleme yönüne bakan
kutu ve tek nokta.

Seçim **çizimin verisi değildir.** Bir çizimi farklı seçimle açmak onu değiştirmez:
seçim dosyaya yazılmaz, içerik özetine girmez, günlüğe komut olarak düşmez ve
`GERİAL` ile geri alınmaz. `GERİAL` çizdiğinizi geri alır, vurguladığınızı değil.

Nesneler **kalıcı kimlikleriyle** tutulur. Kimlik `1`'den başlar, hiç yeniden
kullanılmaz ve nesne silinse bile başka bir nesneye verilmez. Bu yüzden bir seçim
kaydetmeyi, yeniden sıralamayı ve yeniden yüklemeyi atlatır — ve `SEÇ`'in yazdığı
kimlikler doğrudan [`SİL`](erase.md) komutuna verilebilir.

### Pencere ve kesen kutu

CAD dünyasının kırk yıllık ayrımı, KentOSCad'de de aynıdır:

| Kutu | Ne alır | Ekranda |
|---|---|---|
| **Pencere** (soldan sağa) | Yalnızca **tamamen içeride** kalan nesneleri | Düz çerçeve |
| **Kesen** (sağdan sola) | Kutuya **değen** her nesneyi | Kesik çerçeve |

`KUTU` modu bu kararı iki köşenin sırasından okur: ikinci nokta birinciden sağdaysa
pencere, solundaysa kesen. Fareyle sürüklediğinizde gönderilen mod budur.

## Adlar

| Ad | Tür |
|---|---|
| `SEÇ` | Türkçe, birincil |
| `SEC` | Türkçe karaktersiz klavye için |
| `SELECT` | İngilizce karşılık |
| `S` | Kısaltma |
| `core.select` | Komut kimliği |

## Sözdizimi

```text
SEÇ
SEÇ TÜMÜ
SEÇ TEMİZLE
SEÇ NESNE nesneler=<kimlik> nesneler=<kimlik> ...
SEÇ PENCERE <köşe> <köşe>
SEÇ KESEN <köşe> <köşe>
SEÇ KUTU <köşe> <köşe>
SEÇ NOKTA <nokta> [tolerans=<metre>]
SEÇ <mod> ... islem=EKLE | ÇIKAR | TERSİNE
```

Argümansız çağrı hiçbir şeyi değiştirmez; yalnızca seçimde ne olduğunu yazar.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `mod` | `TÜMÜ`, `TEMİZLE`, `NESNE`, `PENCERE`, `KESEN`, `KUTU` veya `NOKTA`. Verilmezse seçim yalnızca raporlanır |
| `noktalar` | Kutu köşeleri (iki nokta) veya `NOKTA` modunda tek tıklama noktası |
| `nesneler` | `NESNE` modunda nesne kimlikleri. Birden fazla `nesneler=` yazılabilir |
| `islem` | `DEĞİŞTİR` (varsayılan), `EKLE`, `ÇIKAR` veya `TERSİNE` |
| `tolerans` | `NOKTA` modunda arama yarıçapı, **metre**. Verilmezse `seçim_toleransı` tercihi (ekran pikseli) kullanılır |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

Her modun İngilizce ve karaktersiz karşılıkları da kabul edilir: `TUMU`/`ALL`,
`TEMIZLE`/`CLEAR`, `WINDOW`, `CROSSING`, `BOX`, `POINT`, `ADD`, `REMOVE`, `TOGGLE`.

### Tolerans neden pikseldir

`seçim_toleransı` ve `yakalama_toleransı` ekran pikselidir, zemin metresi değil.
Sebebi tek cümlede şudur: *nişan alan göz ekrana bakar.* 1:5000 ölçekte altı piksel
metrelerce yer, 1:50 ölçekte santimetrelerce yer demektir; tolerans yakınlaştırmayla
birlikte değişmezse yakın planda hiçbir şeyi tutturamazsınız.

Bunun bir sonucu vardır ve bilerek böyledir: **ekranı olmayan bir istemcinin
piksel toleransı yoktur.** Başsız çalışan bir betik `SEÇ NOKTA` yazdığında yalnızca
noktanın tam üstündeki nesneyi bulur. Aralık isterse `tolerans=` ile metre cinsinden
söyler — böylece betik bir şey uydurmak yerine ne istediğini yazmış olur.

## Örnekler

### Komut satırı

Önce birkaç nesne çizin:

```
KATMAN ad=SINIR
ÇİZGİ 0,0 20,0 20,20 0,20
```

Bütün görünür nesneleri seçin:

```
SEÇ TÜMÜ
```

Seçimi boşaltın:

```
SEÇ TEMİZLE
```

Kimlikle seçin — birden fazla `nesneler=` yazabilirsiniz:

```
SEÇ NESNE nesneler=1 nesneler=2
```

Bir pencere kutusu içinde **tamamen** kalanları seçin:

```
SEÇ PENCERE -1,-1 21,21
```

Kutuya değen her şeyi seçin:

```
SEÇ KESEN 5,-1 25,1
```

Seçime ekleyin ve seçimden çıkarın:

```
SEÇ NESNE nesneler=3 islem=EKLE
SEÇ NESNE nesneler=1 islem=ÇIKAR
```

Bir noktanın yarım metre yakınındaki nesneyi seçin:

```
SEÇ NOKTA 10,0 tolerans=0.5
```

Seçimde ne olduğunu sorun:

```
SEÇ
```

Transkript şuna benzer bir satır yazar:

```text
2 nesne seçili: 2, 3
```

### Arayüz

Harita alanında hiçbir komut çalışmıyorken sol fare tuşu seçim yapar:

| Hareket | Sonuç |
|---|---|
| Tek tık | İmlecin `seçim_toleransı` kadar yakınındaki nesneyi seçer |
| Soldan sağa sürükle | Pencere kutusu — tamamen içeride kalanları seçer |
| Sağdan sola sürükle | Kesen kutu — kutuya değen her şeyi seçer |
| **Shift** + tık/sürükle | Seçime **ekler** |
| **Ctrl** + tık/sürükle | Seçimden **çıkarır** |
| **Esc** | Seçimi temizler |

Menüden: **Düzen > Tümünü Seç** (**Ctrl+A**) ve **Düzen > Seçimi Temizle**
(**Ctrl+Shift+A**).

Seçili nesneler tuvalde kalın ve renkli çizilir. Sürükleme sırasında kutunun kendisi
de görünür: pencere kutusu düz çerçeveli, kesen kutu kesik çerçevelidir.

Arayüzün ayrıcalığı yoktur: fareyle çizdiğiniz kutu, komut satırına
`SEÇ KUTU ...` yazmakla **aynı komuttur** ve aynı yoldan geçer.

### Betik

```json
{
  "ad": "Sınır katmanını seç ve temizle",
  "komutlar": [
    { "cmd": "core.line",   "args": { "noktalar": [[0,0],[20000,0]] } },
    { "cmd": "core.line",   "args": { "noktalar": [[0,5000],[20000,5000]] } },
    { "cmd": "core.select", "args": { "mod": "KESEN", "noktalar": [[-1000,-1000],[21000,6000]] } },
    { "cmd": "core.erase",  "args": {} }
  ]
}
```

Betikte `nesneler` bir kimlik dizisidir; `noktalar` milimetre çiftlerinden oluşan bir
dizidir. Argümansız `core.erase` etkin seçimi siler.

## Geri alma

`SEÇ` **geri alınamaz ve geri alınmamalıdır.** Seçim çizimin verisi değildir, bu yüzden
geri alma yığınına girmez ve komut günlüğüne belge değişikliği olarak yazılmaz.

Önceki seçime dönmek için seçimi yeniden yapın:

```
SEÇ TEMİZLE
SEÇ NESNE nesneler=1
```

Seçili nesneleri sildikten sonra `GERİAL` nesneleri geri getirir, seçimi geri
getirmez: silinen bir nesnenin kimliği emekliye ayrılır ve seçimde kalması yanlış
olurdu.

## Betikten kullanım

`SEÇ` betiklenebilir. Tipik kullanım, seçip silmektir:

```json
[
  { "cmd": "core.line",   "args": { "noktalar": [[0,0],[10000,0]] } },
  { "cmd": "core.select", "args": { "mod": "TÜMÜ" } },
  { "cmd": "core.erase",  "args": {} }
]
```

Bir betik bloğu tek geri alma adımıdır; seçim o adıma dahil değildir, çünkü seçim
belge durumu değildir.

`SEÇ` **AI erişimine kapalıdır.** Sebebi şudur: bir öneri motoru seçimi
değiştirebilseydi, kendisi hiç `SİL` göndermeden bir sonraki `SİL`'in neyi sileceğini
değiştirebilirdi. Yapay zekânın çizime dokunan her adımı önizleme ve açık onaydan
geçer (`CLAUDE.md` Article 2.8).

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Beklenen mod: TÜMÜ \| TEMİZLE \| NESNE \| PENCERE \| KESEN \| KUTU \| NOKTA. Girilen: 'OLMAYAN'` | Tanınmayan mod adı | Tablodaki adlardan birini yazın |
| `Beklenen işlem: DEĞİŞTİR \| EKLE \| ÇIKAR \| TERSİNE. Girilen: 'BİLİNMEYEN'` | Tanınmayan `islem` değeri | `EKLE`, `ÇIKAR` veya `TERSİNE` yazın |
| `'PENCERE' 2 nokta bekliyor. Girilen: 1 nokta.` | Kutu için tek köşe verilmiş | İki köşe verin |
| `'NESNE' en az bir nesne kimliği bekliyor. Örnek: SEÇ NESNE nesneler=1` | `NESNE` modunda kimlik verilmemiş | `nesneler=` ile kimlik verin |
| `Geçersiz nesne kimliği: 0. Kimlikler 1'den başlar.` | Sıfır veya negatif kimlik | Kimlikler `1`'den başlar; `SEÇ` ile listeleyin |
| `Nesne bulunamadı veya silinmiş: 99` | Kimlik yok ya da nesne silinmiş | Kimliği denetleyin; seçim değişmedi |
| `Bu aramada nesne bulunamadı. Seçimde 0 nesne var.` | Kutu ya da nokta boş yere düşmüş | Hata değildir; kutuyu büyütün veya `tolerans=` verin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## Sırada ne var

- [Nesne silme](erase.md) — seçtiğinizi silmek
- [Oturum modları](mode.md) — nesne yakalama, dik mod, kutupsal izleme
- [Arayüz](../baslangic/arayuz.md) — harita alanında fare ve klavye
