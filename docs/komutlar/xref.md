# DIŞREFERANS — Dış Referans

Başkasının tuttuğu bir altlığın — kadastro paftası, halihazır harita, komşu projenin
çizimi — üstünde çalışan herkes için. Bu sayfayı bitirdiğinizde bir dosyayı çizime
bağlayabilecek, kaynağı değişince yenileyebilecek ve proje klasörünüzü başka bir yere
ya da makineye taşıdığınızda neyin kendiliğinden bulunduğunu bileceksiniz.

## Ne yapar

Bir proje (`.pcad`), DXF ya da DWG dosyasını çizime **dış referans** olarak bağlar. Dosya
çizimde, **kendi koordinatlarında, yerinde** çizilir; köşelerine, merkezlerine ve
kenarlarına her zamanki gibi yakalanırsınız. Ama **düzenlenmez**: onu değiştirmenin yeri
kendi dosyasıdır. Kaynak değişince `DIŞREFERANS islem=yenile` değişikliği getirir; çizimi
bir sonraki açışınızda da değişmiş hâli gelir.

Dış referans bir **blok tanımıdır** ([Blok referansı](../nesneler/blokreferansi.md)):
dosyanın nesneleri tanımın üyeleridir, çizimdeki referans onları çizer. Referansı
taşıyabilir, döndürebilir, ikinci bir referansını koyabilirsiniz; tanımın içine
dokunulmaz.

### Proje dosyasına ne yazılır

Proje dosyası dış referansın **adını ve yolunu** tutar, **nesnelerini tutmaz**. Nesneler
her açılışta kaynaktan okunur; 200 MB'lık bir halihazır altlık bağlı diye projeniz
büyümez ve altlığı güncelleyen kişinin değişikliği sizin dosyanızda eski kalmaz.

Yol, proje dosyasının klasörüne **göre** yazılır (`altliklar/altlik.pcad` gibi). Proje
klasörünü bütün olarak başka bir yere ya da makineye taşırsanız referanslar yeni yerinde
bulunur. Dosya kayıtlı yerinde yoksa proje dosyasının yanında **adıyla** aranır — e-postayla
gelen bir proje ile altlığını aynı klasöre koymak yeter; program bunu açılışta söyler ve
bir sonraki kayıtta yeni yeri yazar.

Kaynak hiç bulunamazsa çizim **yine açılır**: referans yerinde, boş çizilir ve açılışta bir
uyarı hangi dosyanın eksik olduğunu söyler. `islem=yol` ile yeni yerini gösterirsiniz.

Dış referans taşıyan bir proje dosyası, bu sürümden eski KentOSCad'lerde açılmaz; eski
sürüm bunu "daha yeni bir okuyucu istiyor" diye söyler ([Proje dosyası](../veri/proje-dosyasi.md)).

### Katmanlar ve bloklar

Dosyanın katmanları çizime **`AD|KATMAN`** adıyla gelir (`altlik|YOL`) ve katman
panelinde dış referansın adıyla gruplanır; `0` katmanındaki nesneler referansın
katmanında çizilir. Bu katmanların rengini, görünürlüğünü ve kilidini değiştirebilirsiniz;
yenilemede ve yeniden açılışta **sizin ayarınız kalır**. Dosyanın içindeki bloklar da
`AD|BLOK` adıyla gelir (`altlik|KAPAK`).

### Koordinat sistemi

Dosya çizimle **aynı koordinat sisteminde** olmalıdır. Başka sistemdeki bir proje
dosyası bu sürümde dış referans olarak yüklenmez: kilometrelerce ötede, doğruymuş gibi
çizilirdi. Dönüşümlü dış referans C-14'ün sonraki bir aşamasında gelecek.

## Adlar

| Ad | Tür |
|---|---|
| `DIŞREFERANS` | Türkçe, birincil |
| `DISREFERANS` | Türkçe karaktersiz klavye için |
| `XREF` | İngilizce karşılık |
| `DRF` | Kısaltma |
| `core.xref` | Komut kimliği |

## Sözdizimi

```text
DIŞREFERANS dosya=<dosya> [ad=<ad>] [nokta=<nokta>] [olcek=<ölçek>] [aci=<derece>]
DIŞREFERANS islem=listele
DIŞREFERANS islem=yenile [ad=<ad>]
DIŞREFERANS islem=bosalt ad=<ad>
DIŞREFERANS islem=yukle ad=<ad>
DIŞREFERANS islem=yol ad=<ad> dosya=<dosya>
DIŞREFERANS islem=bagla ad=<ad>
DIŞREFERANS islem=kaldir ad=<ad>
```

## Parametreler

| Parametre | Ne işe yarar |
|---|---|
| `islem` | `ekle` (varsayılan), `yenile`, `bosalt`, `yukle`, `yol`, `bagla`, `kaldir` ya da `listele` |
| `dosya` | `ekle` ve `yol` için dosya: proje, DXF ya da DWG. Göreli yol proje dosyasının klasörüne göre okunur; hiç kaydedilmemiş bir çizimde çalışma klasörüne göre |
| `ad` | Dış referansın adı. `ekle`'de verilmezse dosyanın adı olur; içinde `|` olamaz |
| `nokta` | `ekle` için referansın konduğu nokta. Verilmezse başlangıç noktası (0,0): dosya kendi koordinatlarında, yerinde çizilir |
| `olcek` | `ekle` için ölçek; varsayılan 1 |
| `aci` | `ekle` için dönme açısı, derece; varsayılan 0 |

| İşlem | Ne olur |
|---|---|
| `ekle` | Dosyayı okur, dış referansı tanımlar ve bir referans koyar. Aynı ad aynı dosyaya zaten bağlıysa yalnız bir referans daha konur |
| `yenile` | Dosyayı yeniden okur; `ad=` verilmezse yüklü bütün dış referansları |
| `bosalt` | Nesneleri çizimden çıkarır, referans yerinde boş kalır. Boşaltılan dış referans açılışta da okunmaz |
| `yukle` | Boşaltılanı geri getirir |
| `yol` | Dış referansı başka bir dosyaya bağlar ve okur |
| `bagla` | Dış referansı çizime katar: sıradan bir blok olur, nesneleri bu çizimle birlikte kaydedilir, dosyası değişse de değişmez. Bundan sonra [BLOKDÜZENLE](block_edit.md) ile düzenlenebilir |
| `kaldir` | Çizimdeki referanslarını siler ve dış referansı çizimden kaldırır; dosyasına dokunulmaz. Aynı ad sonra yeniden bağlanabilir |
| `listele` | Dış referansları durumlarıyla (yüklü, boşaltıldı, bulunamadı) sayar |

Tipi ve adedi için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Proje dosyasının yanında duran bir altlığı bağlamak:

```text
DIŞREFERANS dosya=altlik.pcad
```

```text
'altlik' dış referansı bağlandı: 4 nesne, 1 yeni katman. Dosyanın kendi koordinatlarında, yerinde çizildi.
```

Bağlı olanları görmek ve altlık güncellenince yenilemek:

```text
DIŞREFERANS islem=listele
DIŞREFERANS islem=yenile ad=altlik
```

Bir süre görmek istemediğinizde boşaltıp sonra geri getirmek:

```text
DIŞREFERANS islem=bosalt ad=altlik
DIŞREFERANS islem=yukle ad=altlik
```

Altlığın yeri değiştiyse yenisini göstermek:

```text
DIŞREFERANS islem=yol ad=altlik dosya=arsiv/altlik.pcad
```

Komşu parselin DXF çizimini kendi adıyla, 250 m doğuya kaydırarak bağlamak ve işi
bitince kaldırmak:

```text
DIŞREFERANS dosya=komsu.dxf ad=KOMSU nokta=250,0
DIŞREFERANS islem=kaldir ad=KOMSU
```

Teslimde altlığın o günkü hâlini çizimin parçası yapmak:

```text
DIŞREFERANS islem=bagla ad=altlik
```

### Arayüz

**Harita ▸ Veri ▸ Dış Referans** (ya da **Çizim ▸ Blok ▸ Dış Referans**) bir dosya
penceresi açar; seçtiğiniz dosya kendi koordinatlarında bağlanır. **Harita ▸ Veri ▸ Dış
Referansları Yenile** — bir blok seçiliyken **Blok Araçları** sekmesinde de — yüklü bütün
dış referansları dosyalarından yeniden okur. Dış referansın içindeki bir nesneye tıklamak
referansı seçer; taşıyabilir, silebilirsiniz. Çift tıklamak onu düzenlemeye açmaz,
neden açmadığını söyler.

Dış referansları durumlarıyla gösteren ve yeniden bağlamayı, boşaltmayı pencereden
yaptıran referans yöneticisi paneli, kaynak değişince uyaran bildirimle birlikte
C-14'ün 2. aşamasında gelecek; o gelene kadar bu işler bu sayfadaki komutlarladır.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.xref", "args": { "dosya": "altlik.pcad" } },
    { "cmd": "core.xref", "args": { "islem": "yenile", "ad": "altlik" } }
  ]
}
```

### Üçü de aynı

Arayüz, komut satırı ve betik aynı komutu çalıştırır; aynı belgeyi ve aynı günlüğü
bırakırlar. Günlük dosyanın yolunu yazar: tekrar oynatılan bir günlük aynı dosyayı yeniden
okur.

## Geri alma

Her işlem tek **Ctrl+Z**'dir. Bağlamayı geri almak referansı ve tanımın içini kaldırır;
yenilemeyi geri almak bir önceki okumanın nesnelerini geri getirir; boşaltmayı geri almak
nesneleri geri koyar. Açılışta yapılan okuma geri alınmaz — açmak geri alınan bir iş
değildir.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır. `listele` yapılandırılmış rapor da verir:
`dis_referanslar` altında her biri için `ad`, `dosya`, `durum` (`yüklü`, `boş`,
`boşaltıldı`, `bulunamadı`), `nesne` ve `referans` sayısı. Günlüğe `islem`, `ad`,
`dosya` ve `ekle` için `nokta` (varsa `olcek`, `aci`) yazılır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bağlanacak dosyayı verin: DIŞREFERANS dosya=<…>.` | `ekle` dosyasız çağrıldı | `dosya=` verin |
| `Dış referans dosyası bulunamadı: …` | Yol yanlış ya da dosya yok | Yolu denetleyin; göreli yol proje dosyasının klasörüne göredir |
| `Dış referans adında '|' olamaz …` | `ad=` içinde `|` var | `|` dış referansın içindeki blokları ayırır; başka bir ad verin |
| `'X' adında bir blok zaten var; dış referansa ad= ile başka bir ad verin.` | Çizimde aynı adlı bir blok var | `ad=` ile başka bir ad verin |
| `Çizim kendi dosyasına dış referans olamaz: …` | Açık çizimin kendi dosyası bağlanmak istendi | Başka bir dosya seçin |
| `'…' … koordinat sisteminde, çizim … sisteminde. …` | Dosya başka bir koordinat sisteminde | Dosyayı çizimin sistemine dönüştürün |
| `'X' adında bir dış referans yok. …` | `ad=` bilinmeyen bir ad | Bağlı dış referansların adı mesajda yazılıdır |
| `Hangi dış referans …: ad=<ad>.` | İşlem bir ad istiyor | `ad=` verin |
| `'X' boşaltılmış; yeniden görmek için DIŞREFERANS islem=yukle ad=X.` | Boşaltılmış bir dış referans yenilenmek istendi | Önce `islem=yukle` |
| `'X' zaten yüklü; …` / `'X' zaten boşaltılmış.` | İşlem zaten yapılmış | — |
| `'X' boşaltılmış; bağlanacak bir şey yok. …` | Boşaltılmış dış referans çizime katılmak istendi | Önce `islem=yukle` |
| `'X' 'Y' bloğunun içinde kullanılıyor; …` | Kaldırılacak dış referans başka bir bloğun içinde | O bloktan [BLOKDÜZENLE](block_edit.md) ile çıkarın |
| `'X' bir dış referansın parçası; tanımı kendi dosyasında düzenlenir. …` | [BLOKDÜZENLE](block_edit.md) bir dış referansa uygulandı | Dosyasını düzenleyip `islem=yenile`; burada düzenlemek için önce `islem=bagla` |
| `'X' bir dış referans; patlatılamaz. …` | [PATLAT](explode.md) bir dış referansa uygulandı | Önce `islem=bagla` |
| `uyarı: 'X' dış referansı yüklenemedi: … Çizim açıldı; referans boş çizilir. …` | Açılışta kaynak okunamadı | `islem=yol` ile yeni yerini gösterin |
| `uyarı: 'X' dış referansı kayıtlı yerinde yoktu, proje klasöründe bulundu: …` | Kaynak proje dosyasının yanında adıyla bulundu | Bir şey gerekmez; kaydettiğinizde yeni yer yazılır |
| `Dosya motoru bağlı değil; dış referans bu yapıda okunamıyor.` | Dosya motoru olmayan bir istemci | Uygulamayı ya da dosya motoru bağlı bir istemciyi kullanın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## Sınırlar

- Bağlanan bir proje dosyasının **kendi** dış referansları bu sürümde okunmaz; o
  referanslar boş gelir.
- DWG ve DXF dosyalarının içindeki dış referanslar (XREF) okunmaz ve yazılmaz; DXF'e dışa
  aktarılan bir çizimde dış referans o anki içeriğiyle sıradan bir blok olarak yazılır ve
  adlardaki `|`, AutoCAD'in bağlanmış dış referansları adlandırdığı gibi `$0$` olur
  (`altlik$0$YOL`). Bu uyumluluk, kırpma (dış referansın yalnız bir bölgesini göstermek)
  ve dönüşümlü referans C-14'ün sonraki aşamalarında gelecek.

## İlgili

- [BLOKEKLE](insert.md) — kitaplık dosyasından bir bloğu **kopyalayarak** getirmek:
  getirilen blok çizimin olur, dosyası değişse de değişmez
- [BLOKDÜZENLE](block_edit.md) — çizime katılmış (bağlanmış) bir bloğu düzenlemek
- [İÇEAKTAR](import.md) — bir dosyanın nesnelerini çizimin **kendi** nesneleri yapmak
- [Blok referansı türü](../nesneler/blokreferansi.md)
- [Proje dosyası](../veri/proje-dosyasi.md)
