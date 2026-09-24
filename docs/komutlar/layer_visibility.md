# KATMANGÖRÜNÜM — Katman Görünürlüğü

Kırk katmanlı bir imar planında çalışan herkes için; bu sayfayı bitirdiğinizde bir
katmanı gösterip gizlemeyi, yalnız birini bırakmayı, hepsini geri getirmeyi ve
gösterimi ters çevirmeyi bileceksiniz.

## Ne yapar

Katmanların **görünürlüğünü** değiştirir. Tek bir katmanı gösterir ya da gizler,
**yalnız** onu bırakıp diğerlerini gizler, **hepsini** gösterir veya gösterimi **ters
çevirir**.

İki şeyi yapmadığı için `KATMAN`'ın yanında ayrı bir komuttur:

- **Aktif katmanı değiştirmez.** `KATMAN ad=PARSEL gorunur=hayır` PARSEL'i aktif katman
  yapar, çünkü bir katmanı adıyla anmak onu seçmenin yoludur. Kırk katmanı gizlemek bir
  katman seçmek değildir ve kırkıncısı hiç değildir.
- **Katman yaratmaz.** Olmayan bir katman adı hatadır, sessizce yeni bir katman değil.

Katmanın kilidine, rengine ve grubuna dokunmaz; onlar `KATMAN`'ın işidir.

## Adlar

| Ad | Tür |
|---|---|
| `KATMANGÖRÜNÜM` | Türkçe, birincil |
| `KATMANGORUNUM` | Türkçe, ASCII katlanmış |
| `LAYERVIEW` | İngilizce karşılık |
| `KGÖ` / `KGO` | Kısaltma |
| `core.layer_visibility` | Komut kimliği |

## Sözdizimi

```
KATMANGÖRÜNÜM islem=<goster|gizle|yalniz> katman=<ad>
KATMANGÖRÜNÜM islem=tumu
KATMANGÖRÜNÜM islem=tersine [katman=<ad>]
```

Argümansız çağırırsanız komut önce işlemi, sonra gerekiyorsa katman adını sorar.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `islem` | Yapılacak iş. Zorunlu. Aşağıdaki beş sözcükten biri |
| `katman` | Katman adı. `goster`, `gizle` ve `yalniz` için zorunlu; `tersine` için isteğe bağlı; `tumu` ile verilemez |

| `islem` | Ne yapar |
|---|---|
| `goster` | Verilen katmanı gösterir |
| `gizle` | Verilen katmanı gizler |
| `yalniz` | Verilen katmanı gösterir, **diğer bütün katmanları gizler** |
| `tumu` | Bütün katmanları gösterir |
| `tersine` | `katman` verilirse o katmanın görünürlüğünü, verilmezse **bütün katmanların** görünürlüğünü ters çevirir |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

Sözcükler Türkçe katlamayla eşleşir: `yalniz` ve `yalnız` aynı işlemdir, büyük harfle de
yazabilirsiniz.

## Örnekler

### Komut satırı

```
KATMAN ad=PARSEL
KATMAN ad=YOL
KATMANGÖRÜNÜM islem=gizle katman=YOL
KATMANGÖRÜNÜM islem=goster katman=YOL
```

Yalnız bir katmanla çalışmak — ada/parsel kontrolü yaparken en çok kullanılan:

```
KATMANGÖRÜNÜM islem=yalniz katman=PARSEL
```

Sonra hepsini geri getirmek:

```
KATMANGÖRÜNÜM islem=tumu
```

Gösterimi ters çevirmek, yani görünenleri gizleyip gizlileri göstermek:

```
KATMANGÖRÜNÜM islem=tersine
```

Kısaltmayla aynı iş:

```
KGO islem=tersine katman=PARSEL
```

### Arayüz

Katmanlar panelinde bir satıra **sağ tıklayın** ve **Görünüm** alt menüsünü açın:

| Giriş | Ne yapar |
|---|---|
| **Göster** | Seçili katmanları gösterir. Tek satırda, katmanın o anki durumu işaretle görünür |
| **Gizle** | Seçili katmanları gizler |
| **Yalnız bunu göster** | Seçili katmanları bırakır, diğerlerini gizler. Birden fazla satır seçiliyse başlık **Yalnız seçili 3 katmanı göster** olur |
| **Tümünü göster** | Gizli bütün katmanları geri getirir |
| **Gösterimi ters çevir** | Görünenleri gizler, gizlileri gösterir |

Şeritte **Görünüm ▸ Katmanlar ▸ Tümünü Göster** ve **Gösterimi Ters Çevir** aynı iki işi
yapar (**Tümünü Göster** **Giriş ▸ Katmanlar** panelinde de vardır). Onlar şeritte, çünkü
bir satıra bağlı değildirler — bütün katmanları gizlemiş
biri sağ tıklayacak satır bulamaz.

**Birden fazla katman seçebilirsiniz**: Ctrl ile tek tek, Shift ile aralık. Menü
seçimin tamamına uygulanır — seçili olmayan bir satıra sağ tıklarsanız yalnız o satıra.
Kaç katman seçili olduğu menü başlıklarında yazılıdır.

Seçili satırların tamamı **tek bir Ctrl+Z** ile geri gelir: panel her katman için bir
komut gönderir ama hepsini tek bir küme hâlinde gönderir.

Bir satırın solundaki **göz** simgesi de aynı komutu kullanır ve yalnız o satırı
değiştirir.

Katman adlarını **vurgulamak** aktif katmanı değiştirmez; bir satırı aktif yapmak için
menüdeki **Aktif katman yap** girişi vardır.

### Betik

```json
{
  "ad": "Yalnız parselleri göster",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.layer", "args": { "ad": "YOL" } },
    { "cmd": "core.layer_visibility", "args": { "islem": "yalniz", "katman": "PARSEL" } }
  ]
}
```

## Geri alma

Her çağrı **tek bir geri alma adımıdır**: `islem=tumu` kırk katmanı gösterse de tek bir
Ctrl+Z hepsini eski hâline döndürür.

Arayüzde birden fazla katman seçip sağ tık menüsünden bir giriş çalıştırdığınızda da öyledir: panel
katman başına bir komut gönderir, hepsi tek bir kümede birleşir ve tek adımda geri döner.

Görünürlük çizimin kendisine yazılır ve **proje dosyasıyla birlikte kaydedilir**; seçim
ve vurgulama kaydedilmez.

## Betikten kullanım

Komut betiklerde `core.layer_visibility` kimliğiyle çağrılır. Bir katmanın adı betikte de
adıdır; katman kimliği ya da sıra numarası kullanılmaz.

Birden fazla katmanı tek çağrıda saymak mümkün değildir: her katman için bir satır yazın.
Bir betiğin tamamı zaten tek bir küme olarak çalışır, dolayısıyla art arda yazılan
satırlar tek bir geri alma adımı olur.

```json
{
  "ad": "Altlıkları gizle",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "ALTLIK" } },
    { "cmd": "core.layer", "args": { "ad": "ORTOFOTO" } },
    { "cmd": "core.layer_visibility", "args": { "islem": "gizle", "katman": "ALTLIK" } },
    { "cmd": "core.layer_visibility", "args": { "islem": "gizle", "katman": "ORTOFOTO" } }
  ]
}
```

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Tanınmayan işlem: 'parlat'. İşlemler: goster / gizle / yalniz / tumu / tersine` | `islem` beş sözcükten biri değil | Sözcüklerden birini yazın |
| `Katman yok: 'YOKBÖYLE'.` | O adda katman yok | Adı denetleyin; katman yaratmak `KATMAN ad=<ad>` işidir |
| `'tumu' bütün katmanlara bakar; katman= almaz. Bir katmanı göstermek için islem=goster katman=<ad> yazın.` | `tumu` ile katman adı verilmiş | Adı kaldırın ya da `islem=goster` kullanın |
| `'yalniz' için katman adı gerekir: katman=<ad>` | `goster`, `gizle` ya da `yalniz` katmansız çağrılmış | `katman=<ad>` ekleyin |
