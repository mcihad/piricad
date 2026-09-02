# ALANAÇEVİR — Çizgileri Kapalı Alana Çevirme

Sınırı ayrı ayrı çizgiler halinde gelmiş bir parseli, adayı ya da yapı adasını
kapalı alana çeviren herkes için; bu sayfayı bitirdiğinizde uç uca değen çizgileri
arayüzden, komut satırından ve betikten tek bir alana çevirmeyi bileceksiniz.

## Ne yapar

`ALANAÇEVİR`, **uç uca değen çizgileri** birleştirip **tek bir kapalı alan** üretir
ve kaynak çizgileri siler.

Bir parsel sınırı çoğu zaman böyle gelir: DXF'ten, totalstation çıktısından ya da
başka birinin elinden, ekranda parsel **gibi görünen** ama aslında uçları birbirine
değen **ayrı çizgiler** olarak. Bu haliyle çizim hakkında hiçbir şey söylenemez —
açık bir çizgi hiçbir şeyi çevrelemez, dolayısıyla yüzölçümü yoktur, çevresi
kapanmaz ve üzerine ifraz ya da tevhit oturtulamaz.

Komut çizgileri **sırasına ve yönüne bakmadan** zincirler: hangi çizginin önce
çizildiği ya da hangi uçtan hangi uca gittiği önemli değildir, uçların buluşması
yeterlidir. Zincir başladığı yere dönüyorsa kapalı alan oluşur.

Kaynak çizgiler **silinir**. Bu kasıtlıdır: aynı sınırın hem alan hem de altındaki
çizgiler olarak iki kez var olması, bir sonraki topoloji denetiminin ya da dışa
aktarmanın hesap soracağı bir çift kayıttır. Silme aynı işlemin parçasıdır, yani
tek bir [`GERİAL`](undo.md) hem alanı kaldırır hem çizgileri geri getirir.

Kapanış noktasını program ekler; zincirin son köşesi ilk köşeyle çakışıyorsa tekrar
saklanmaz.

### Uçlar ne kadar yakınsa "değiyor" sayılır

Düğüm toleransı **projenin** ayarıdır: `core.topoloji.dugum_toleransi`, öntanımlı
**10 mm**. İki uç bu mesafe içindeyse aynı köşe sayılır.

Bu sayının proje kapsamında olması bir tercih değil zorunluluktur: iki ucun tek
köşe sayılıp sayılmaması ifrazın ürettiği koordinatı değiştirir, yani tapunun
baytını değiştirir. Makineye ait bir tercih olsaydı aynı proje iki bilgisayarda iki
farklı sonuç verirdi.

Toleransı [`AYAR`](setting.md) ile değiştirebilirsiniz:

```text
AYAR düğüm_toleransı 50mm
```

## Adlar

| Ad | Tür |
|---|---|
| `ALANAÇEVİR` | Türkçe, birincil |
| `ALANACEVIR` | ASCII karşılık |
| `TOAREA` | İngilizce karşılık |
| `ALÇ` | Kısaltma |
| `core.to_area` | Komut kimliği |

## Sözdizimi

```text
ALANAÇEVİR
ALANAÇEVİR nesneler=<k1> nesneler=<k2> …
```

Kimlik verilmezse **etkin seçim** kullanılır — seç, sonra çevir.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Birleştirilecek çizgilerin kimlikleri. Verilmezse etkin seçim |

## Örnekler

### Komut satırı

Dört kenarı ayrı ayrı çizin, sonra alana çevirin:

```text
ÇİZGİ 485300,4310200 485360,4310200
ÇİZGİ 485360,4310200 485360,4310245
ÇİZGİ 485360,4310245 485300,4310245
ÇİZGİ 485300,4310245 485300,4310200
ALANAÇEVİR nesneler=1 nesneler=2 nesneler=3 nesneler=4
```

```text
4 çizgi tek bir alana çevrildi; 4 köşe.
```

Seçimi kullanarak:

```text
SEÇ
ALANAÇEVİR
```

### Arayüz

Çevrilecek çizgileri seçin — kutu sürükleyerek hepsini birden alabilirsiniz — sonra
komut satırına `ALANAÇEVİR` yazın. Kimlik yazmanız gerekmez; komut seçimi kullanır.

Arayüzün ayrıcalığı yoktur: seçip çevirdiğiniz alan ile komut satırına yazdığınız
alan aynı komuttur ve komut günlüğüne aynı satır olarak düşer.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.to_area", "args": { "nesneler": [1, 2, 3, 4] } }
  ]
}
```

## Geri alma

`ALANAÇEVİR` tek bir geri alma adımıdır: [`GERİAL`](undo.md) alanı kaldırır **ve**
kaynak çizgileri aynı anda geri getirir. Kaç çizgi birleştirdiğinizden bağımsız
olarak tek `GERİAL` yeter.

[`YİNELE`](redo.md) çevrimi geri getirir.

## Betikten kullanım

Betikten çağrıldığında `nesneler` verilmelidir; betik çalışırken "etkin seçim" diye
bir şey olmayabilir ve bir betiğin ekranda ne seçili olduğuna bağlı olması, aynı
betiğin iki çalıştırmada iki farklı sonuç vermesi demektir.

Çevrim başarısız olursa **hiçbir şey değişmez**: ne alan oluşur ne çizgi silinir.
Yarım uygulanmış bir çevrim yoktur.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Çevrilecek çizgi belirtilmedi ve seçim boş. Örnek: ALANAÇEVİR nesneler=1 nesneler=2` | Ne kimlik verildi ne seçim var | Çizgileri seçin ya da kimliklerini yazın |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Nesne N tek parçalı bir çizgi değil; ALANAÇEVİR yalnız açık çizgileri birleştirir.` | Çok parçalı ya da delikli bir nesne verildi | Yalnız açık çizgileri seçin |
| `Nesne N zaten kapalı bir alan. Kapalı bir alan yeniden çevrilmez.` | Seçime bir alan karışmış | Alanı seçimden çıkarın |
| `Çizgiler tek bir zincir oluşturmuyor: N / M çizgi birleşti, kalanların ucu zincire değmiyor.` | Bir çizginin ucu diğerlerine değmiyor | Uçları yakalama açıkken yeniden çizin ([`MOD`](mode.md)) ya da düğüm toleransını büyütün |
| `Zincir kapanmıyor: ilk köşe ile son köşe birbirine değmiyor.` | Sınırın bir kenarı eksik | Eksik kenarı çizin, sonra tekrar deneyin |
| `Bir alan en az üç köşe ister; ...` | Zincir üç köşeden az bıraktı | Daha fazla kenar verin |

Zincir kendi üzerinden geçiyorsa geometri katmanı alanı reddeder ve sebebini yazar;
bu durumda da hiçbir şey değişmez.

## İlgili

- [`ÇİZGİ`](line.md) — çizgi çizer
- [`ALAN`](area.md) — köşeleri tıklayarak doğrudan kapalı alan çizer
- [`KÖŞETAŞI`](vertex_move.md) · [`KÖŞEEKLE`](vertex_insert.md) — çevrilen alanın köşelerini düzeltir
- [`AYAR`](setting.md) — düğüm toleransını değiştirir
- [`SEÇ`](select.md) · [`GERİAL`](undo.md)
