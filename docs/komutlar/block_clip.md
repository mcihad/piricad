# BLOKKIRP — Blok Kırp

Bir imar planını, halihazır altlığı ya da komşu paftayı bütün olarak bağlayıp yalnız
çalıştığı bölgeyi görmek isteyen herkes için. Bu sayfayı bitirdiğinizde bir blok
referansını ya da [dış referansı](xref.md) bir dikdörtgenle, bir çokgenle ya da çizimdeki
kapalı bir nesneyle kırpabilecek, sınırı görebilecek ve kırpmayı kaldırabileceksiniz.

## Ne yapar

Bir [blok referansını](../nesneler/blokreferansi.md) ya da dış referansı bir **sınırla**
kırpar: sınırın içi çizilir, dışı **çizilmez, yakalanmaz ve seçilmez**. Referansın kutusu
da görünen kısmın kutusu olur; KAPSAM ve pencereyle seçim ona göre çalışır.

Tanıma dokunulmaz. Sınır **referansın üzerinde** durur ve tanımın kendi koordinatlarında
tutulur; bu yüzden referansı taşıdığınızda, döndürdüğünüzde ya da ölçeklediğinizde sınır da
onunla gider. Aynı bloğun başka bir referansı bütün çizilmeye devam eder. Kırpmayı
kaldırdığınızda her şey yeniden görünür.

Sınırın kestiği şekiller şöyle çizilir:

| Şekil | Kırpılınca |
|---|---|
| Açık çizgi | Sınırın içindeki parçaları |
| Kapalı çizgi, alan, daire | Kenarlarının içerideki parçaları; **dolgusu** sınırın içinde dolu kalır ve kesik boyunca **çizgi çizilmez** |
| Tarama | Deseni ve dolgusu sınırın içinde |
| Yazı | Bütün hâlde ya da hiç: başladığı nokta içerideyse görünür |
| İç içe blok | Onun da içindekileri, dış referansın sınırıyla |

Yakalama yalnız görünen noktalara oturur: sınırın dışında kalan bir köşe, orta nokta ya da
merkez sunulmaz; görünen parçanın üzerinde YAKIN, DİK ve KESİŞİM çalışır. Sınırın kestiği
yer yeni bir **köşe değildir** — orada UÇ noktası yoktur.

Kırpılmış bir referansı seçtiğinizde sınırı **kesikli çizgiyle** görünür (yalnız ekranda;
yazdırılmaz ve dışa aktarılmaz). Sınırı kalıcı bir çizgi olarak istiyorsanız
`islem=sinir` onu etkin katmana kapalı çizgi olarak çizer.

## Adlar

| Ad | Tür |
|---|---|
| `BLOKKIRP` | Türkçe, birincil |
| `XCLIP` | İngilizce karşılık (AutoCAD'deki adı) |
| `BKR` | Kısaltma |
| `core.block_clip` | Komut kimliği |

## Sözdizimi

```text
BLOKKIRP nesne=<kimlik> noktalar=<köşe> <köşe>
BLOKKIRP nesne=<kimlik> noktalar=<köşe> <köşe> <köşe> [<köşe> …]
BLOKKIRP nesne=<kimlik> cizgi=<kimlik>
BLOKKIRP nesne=<kimlik> tur=<dikdortgen|cokgen|cizgi>
BLOKKIRP islem=sinir nesne=<kimlik>
BLOKKIRP islem=kaldir nesne=<kimlik>
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `islem` | sözcük | 0..1 | `yeni` (varsayılan): sınırı koyar, varsa eskisinin yerine; `kaldir`: sınırı kaldırır; `sinir`: sınırı etkin katmana kapalı çizgi olarak çizer |
| `nesne` | seçim | 0..1 | Kırpılacak blok referansı ya da dış referans; verilmezse etkin seçim, o da yoksa sorulur |
| `tur` | sözcük | 0..1 | `noktalar` verilmediğinde sınırın nasıl gösterileceği: `dikdortgen` (varsayılan, iki köşe), `cokgen` (köşe köşe), `cizgi` (çizimdeki kapalı bir nesne) |
| `noktalar` | nokta listesi | 0..n | Sınırın köşeleri, çizimde: **iki** köşe bir dikdörtgen, **üç ya da daha çok** köşe bir çokgen |
| `cizgi` | seçim | 0..1 | Sınır olacak kapalı nesne: kapalı çizgi, alan, daire ya da elips — çizildiği biçimiyle |

Dikdörtgen **ekranda gördüğünüz** eksenlere göredir: döndürülmüş bir referansta tanımın
içinde döndürülmüş bir dikdörtgen olarak saklanır. Tipi ve adedi için üretilmiş
[komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

100 metrelik bir yol ile 80. metresinde bir ağaçtan oluşan `YOL` bloğunu tanımlayıp
yerinde bırakın:

```
ÇİZGİ 0,0 100,0
DAİRE merkez=80,0 cevre=82,0
BLOK ad=YOL taban=0,0 nesneler=1 nesneler=2
```

Yalnız ilk 50 metresini göstermek için iki köşeli bir dikdörtgen:

```
BLOKKIRP nesne=5 noktalar=-5,-10 50,10
```

```text
Nesne 5 ('YOL') 4 köşeli sınırla kırpıldı; dışında kalan çizilmiyor, yakalanmıyor. Kaldırmak için: BLOKKIRP islem=kaldir nesne=5
```

Ağaç artık çizilmiyor ve merkezine yakalanılmıyor; yolun 100. metredeki ucu da. Daha çok
köşe bir çokgen verir ve öncekinin yerine geçer:

```
BLOKKIRP nesne=5 noktalar=-5,-10 60,-10 60,10 30,20 -5,10
```

```text
Nesne 5 ('YOL') 5 köşeli sınırla kırpıldı (önceki sınırın yerine); dışında kalan çizilmiyor, yakalanmıyor. Kaldırmak için: BLOKKIRP islem=kaldir nesne=5
```

Sınır referansla birlikte gider — referansı 50 m kuzeye taşımak görünen parçayı da taşır:

```
TAŞI nesneler=5 baslangic=0,0 bitis=0,50
```

Sınırı kalıcı bir çizgi olarak çizmek, sonra kırpmayı kaldırmak:

```
BLOKKIRP islem=sinir nesne=5
BLOKKIRP islem=kaldir nesne=5
```

```text
Nesne 5 ('YOL') kırpma sınırı 5 köşeli kapalı çizgi olarak etkin katmana çizildi.
Nesne 5 ('YOL') yeniden bütün çiziliyor; kırpma sınırı kaldırıldı.
```

Çizimdeki bir daireyle kırpmak — sınır dairenin çizildiği biçimidir:

```
DAİRE merkez=80,50 cevre=95,50
BLOKKIRP nesne=5 cizgi=7
```

```text
Nesne 5 ('YOL') 128 köşeli sınırla kırpıldı; dışında kalan çizilmiyor, yakalanmıyor. Kaldırmak için: BLOKKIRP islem=kaldir nesne=5
```

### Arayüz

- **Çizim ▸ Blok ▸ Kırp** — bölünmüş düğme: yüzü en son kullanılan yolu çalıştırır, oku
  **Kırp** (dikdörtgen), **Çokgenle Kırp** ve **Nesneyle Kırp**'ı listeler.
- **Harita ▸ Veri ▸ Kırp** — dış referansların yanında.
- Bir blok ya da dış referans seçiliyken beliren **Blok** sekmesinde **Kırpma** paneli:
  **Kırp**, **Çokgenle Kırp**, **Nesneyle Kırp**, **Kırpma Sınırını Çiz**, **Kırpmayı
  Kaldır**.

Referans seçili değilse önce onu göstermeniz istenir. **Kırp**'ta dikdörtgenin iki
köşesini tıklarsınız; dikdörtgen imleçle birlikte çizilir. **Çokgenle Kırp**'ta köşeleri
tıklarsınız, çizilen çokgen kapanmış hâliyle görünür; **Enter** ya da sağ tık sınırı
kapatır, **⌫** son köşeyi geri alır. **Nesneyle Kırp** çizimdeki kapalı bir çizgiyi, alanı,
daireyi ya da elipsi sorar. **Esc** her adımda vazgeçer ve çizimde bir şey değişmez.

Klavyeyle: komut satırına `BKR` yazıp Enter'a basın; köşeleri koordinatla yazabilirsiniz
(`-5,-10` Enter, `50,10` Enter).

### Betik

```json
{
  "ad": "Blok kırpma",
  "komutlar": [
    { "cmd": "core.line", "args": { "noktalar": [[0, 0], [100000, 0]] } },
    { "cmd": "core.circle_draw", "args": { "merkez": [80000, 0], "cevre": [82000, 0] } },
    { "cmd": "core.block", "args": { "ad": "YOL", "taban": [0, 0], "nesneler": [1, 2] } },
    { "cmd": "core.block_clip",
      "args": { "nesne": [5], "noktalar": [[-5000, -10000], [50000, 10000]] } }
  ]
}
```

### Üçü de aynı

Arayüz, komut satırı ve betik aynı komutu çalıştırır; aynı belgeyi ve aynı günlüğü
bırakırlar. Fareyle tıklanan iki köşe, yazılan iki köşe ve betiğin iki köşesi günlüğe aynı
`noktalar` olarak yazılır; oynatılan günlük aynı sınırı koyar.

## Geri alma

Her işlem tek **Ctrl+Z**'dir: kırpmayı geri almak referansı bütün hâline, kaldırmayı geri
almak sınırı geri getirir; sınır çizmeyi geri almak çizilen çizgiyi siler. Yarıda **Esc**
geri alma adımı bırakmaz.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır. Her işlem yapılandırılmış bir rapor verir:
`islem`, `nesne`, `blok` (tanımın adı) ve `kose` (sınırın köşe sayısı; kaldırmada 0).
Günlüğe `nesne` ve `noktalar` yazılır — dikdörtgende iki köşe, çokgende köşeler; `cizgi` ile
verilen bir nesnenin **köşeleri** de yazılır, böylece oynatma o nesnenin hâlâ çizimde
olmasına bağlı kalmaz. `islem` ve `tur` verildiyse onlar da yazılır.

Python'dan `cad.block_clip(reference=[5], points=[[-5000, -10000], [50000, 10000]])` olarak
çağrılır; öbür adlar `action`, `shape`, `boundary` ([Python API referansı](../python/referans.md)).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Nesne N bir çizgi; BLOKKIRP blok referanslarını ve dış referansları kırpar.` | Blok referansı olmayan bir nesne verildi | Bir blok referansı ya da dış referans seçin |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik çizimde yok | Kimliği denetleyin |
| `Kırpma sınırı verilmedi: noktalar= ile köşelerini ya da cizgi= ile kapalı bir nesneyi verin.` | Sınır verilmedi | `noktalar=` ya da `cizgi=` verin |
| `Kırpma sınırı bir alan çevirmiyor (N ayrı köşe). İki köşe bir dikdörtgen, üç ya da daha çok köşe bir çokgen verir; köşeler bir doğru üzerinde olmamalı.` | Tek köşe, aynı noktalar ya da bir doğru üzerindeki köşeler | Alan çeviren köşeler verin |
| `Kırpma sınırı, nesne N ('AD') referansının çizdiği hiçbir şeyi içine almıyor; referans görünmez olurdu. Sınırı referansın üzerine çizin.` | Sınır referansın çizdiği her şeyin dışında | Sınırı referansın üzerine çizin |
| `Kırpma sınırı referansın ölçeğinde bir milimetreden küçük kalıyor; daha büyük bir sınır çizin.` | Çok küçük ölçekli bir referansta çok küçük bir sınır | Daha büyük bir sınır çizin |
| `Kırpma sınırı en çok 65535 köşe alır; N verildi.` | Sınırın köşesi çok fazla | Sınırı sadeleştirin ([ÇİZGİDÜZENLE](pedit.md) `islem=sadelestir`) |
| `Nesne N bir çizgi ve bir alanı çevirmiyor; sınır kapalı bir çizgi, alan, daire ya da elips olmalı.` | `cizgi=` açık bir nesne gösteriyor | Kapalı bir nesne verin ya da çizgiyi [ÇİZGİDÜZENLE](pedit.md) `islem=kapat` ile kapatın |
| `Sınır olacak nesne bulunamadı veya silinmiş: N` | `cizgi=` çizimde olmayan bir kimlik | Kimliği denetleyin |
| `Nesne N ('AD') kırpılmamış; kaldırılacak sınır yok.` | Kırpılmamış bir referansın kırpması kaldırılmak istendi | — |
| `Nesne N ('AD') kırpılmamış; çizilecek sınır yok.` | Kırpılmamış bir referansın sınırı çizilmek istendi | Önce kırpın |
| `Nesne N bilinmeyen bir blok tanımını çiziyor.` | Referansın adlandırdığı tanım dosyada yok | Dosyayı onarın ya da referansı silin |
| `İşlem yapılacak nesne yok: seçim boş ve 'nesne' verilmedi.` | Referans verilmedi ve seçili değil | `nesne=` verin ya da referansı seçin |
| `'nesne' en fazla 1 nesne alır; N verildi.` | Birden çok referans verildi | Referansları tek tek kırpın |
| `'…' katmanı kilitli; üzerindeki nesne düzenlenemez. …` | Referansın katmanı kilitli | `KATMAN` ile kilidi kaldırın |
| `Blok tanımındaki nesne doğrudan düzenlenemez; tanımı BLOKDÜZENLE ile açıp düzenleyin.` | Bir bloğun içindeki referans verildi | Dıştaki referansı kırpın ya da tanımı [BLOKDÜZENLE](block_edit.md) ile açın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## Sınırlar

- [PATLAT](explode.md) kırpılmış bir referansı **kırpmayı yok sayarak** açar: parçalar
  tanımın bütün bileşenleridir ve çıktı bunu söyler (`kırpma sınırı yok sayıldı, parçalar
  bütün çıktı`) — AutoCAD'in EXPLODE'u da böyle yapar.
- DXF'e dışa aktarırken kırpma **taşınmaz**: referans DXF'te bütün görünür ve dışa aktarma
  bunu söyler (`… blok referansının BLOKKIRP sınırı DXF'e taşınmadı`). AutoCAD kırpmayı
  bir `SPATIAL_FILTER` nesnesinde tutar ve libdxfrw onu yazamaz; içe aktarılan bir DXF'teki
  AutoCAD kırpması da okunmaz. İkisi DXF uyumluluk işinde (I-03) gelecek.
- Sınırın içini gizleyip dışını göstermek (AutoCAD'in ters kırpması) yoktur.
- Bir diziyle konmuş referansta (`sutun`, `satir`) sınır, üzerine çizildiği **ilk
  kopyanın** yerinden tanıma taşınır ve her kopyayı aynı yerinden kırpar.
- Kırpılmış referans taşıyan bir proje dosyası bu sürümden eski KentOSCad'lerde açılmaz;
  eski sürüm bunu "daha yeni bir okuyucu istiyor" diye söyler
  ([Proje dosyası](../veri/proje-dosyasi.md)).

## İlgili

- [DIŞREFERANS](xref.md) — bir dosyayı dış referans olarak bağlamak
- [BLOKEKLE](insert.md) — tanımlı bir bloğu yerleştirmek
- [BLOKDÜZENLE](block_edit.md) — tanımın kendisini değiştirmek; bütün referanslar değişir
- [Blok referansı türü](../nesneler/blokreferansi.md)
