# Python Betikleri

Döngü, koşul ve hesap gerektiren işleri otomatikleştirmek isteyen kullanıcı için; bu
sayfayı bitirdiğinizde bir Python betiği yazabilecek, çizimden veri okuyabilecek ve kum
havuzunun neye izin verip neye vermediğini bileceksiniz.

[JSON betiği](README.md) düz bir komut dizisidir: değişken yok, döngü yok, koşul yok.
Beş yerine beş yüz çizgi çizmek gerektiğinde beş yüz satır yazmak zorunda kalırsınız.
Python bunun için vardır.

## Bu sürümde durumu

Python motoru **seçimlik** derlenir ve varsayılan yapıda **kapalıdır**. Açmak için:

```bash
cmake --preset dev -DKENTOS_WITH_PYTHON=ON
cmake --build --preset dev
```

Makinenizde CPython 3.14 ve geliştirme başlıkları kurulu olmalıdır (`brew install
python@3.14`, `apt install python3.14-dev`). Yapılandırma bulamazsa hangi paketin
gerektiğini yazarak durur — sessizce kapanmaz. Ayrıntı:
[Kurulum](../baslangic/kurulum.md).

Kapalı yapıda `.py` dosyası çalıştırmayı denerseniz `BETİK` komutu JSON beklediğini
söyleyerek hata döndürür; çizim değişmez.

## İlk betik

```python
cad.run("KATMAN PARSEL")
cad.run("ÇİZGİ 485320.150,4310220.400 485370.150,4310250.400")
```

`cad` modülü betiğin programa açılan tek kapısıdır. İki işi vardır: **komut çalıştırmak**
ve **çizimden okumak**.

Adı yazmadan da kullanılır — `cad` her betikte hazır bekler. Nereden geldiğini yazmak
isterseniz içe aktarma da çalışır:

```python
import kentos.cad          # kentos.cad.run(...)
from kentos import cad     # cad.run(...)
```

`cad` iki şey taşır: **her komut için bir fonksiyon** (üretilmiş) ve `cad.doc` altında
çizimden **okuma** çağrıları.

### İsimler neden İngilizce

Program Türkçedir, komutlar Türkçedir, bu el kitabı Türkçedir. Python API'si değildir:
`cad.run`, `cad.layers`, `points=` — hepsi İngilizce. Bir Python modülü, her Python
kütüphanesinin yazıldığı dilde okunur ve yazılır; yarısı Türkçe bir API
(`cad.line(noktalar=...)`) ikisinin de en kötüsü olurdu. Komutun kendi adı
(`"ÇİZGİ"`) Türkçe kalır, çünkü onu yazan yine sizsiniz.

## `cad.run` — tek yazma yolu

```python
cad.run("ÇİZGİ 485320.150,4310220.400 485370.150,4310250.400")
cad.run("ÇİZGİ", "485320.150,4310220.400", "485370.150,4310250.400")   # aynısı
```

Birden çok argüman verirseniz araya boşluk konarak birleştirilir. İkisi de aynı satırı
üretir; hangisinin okunaklı olduğuna siz karar verin.

**Yazdığınız metin, komut satırına yazdığınızın aynısıdır.** Aynı ayrıştırıcı, aynı
doğrulama, aynı geri alma yığını, aynı günlük. Bu yüzden:

- Koordinatlar **metredir**, JSON betiğindeki gibi milimetre değil. Komut satırına ne
  yazıyorsanız onu yazın.
- Türkçe komut adları, kısaltmalar ve İngilizce adlar aynı biçimde çalışır:
  `ÇİZGİ`, `CIZGI`, `LINE`, `Ç` hepsi aynı komuttur.

`cad.run` çalıştırdığı komutun kaç ilkel düzenleme yaptığını döndürür:

```python
n = cad.run("ÇİZGİ 0,0 10,10")
```

Bir komut başarısız olursa bir istisna yükselir. Yakalamazsanız betik orada durur ve o
ana kadar yapılan her şey geri alınır. Yakalarsanız — aşağıya bakın.

### Çizimi değiştirmenin başka yolu yoktur

`cad.run` ve aşağıdaki üretilmiş çağrılar dışında çizime dokunan hiçbir bağlantı yoktur —
bilerek. İkisi de aynı komut veri yoluna gider. Çizimi değiştiren her şey komut olmak
zorundadır, çünkü doğrulama, geri alma ve günlük oraya bağlıdır. Bir betiğin çizime
"kısa yoldan" ulaşabilmesi, o üçünü atlayabilmesi demek olurdu.

## Her komut bir fonksiyon

`cad.run("ÇİZGİ 0,0 10,10")` komut satırına yazdığınızın aynısıdır. Bunun yanında her
komutun **kendi Python fonksiyonu** vardır:

```python
cad.line(points=[[485320150, 4310220400], [485370150, 4310250400]])
cad.circle_draw(center=[485400000, 4310230000], rim=[485410000, 4310230000])
cad.polygon_regular(center=[485450000, 4310230000], sides=6, method="ic", radius=8.0)
```

Bu fonksiyonlar **elle yazılmadı; komut kaydından üretiliyor.** Programa bugün eklenen bir
komut bugün bir Python fonksiyonudur; güncellenmesi gereken ikinci bir liste yoktur.

| | `cad.run(...)` | `cad.<komut>(...)` |
|---|---|---|
| Ad | Türkçe komut adı | İngilizce fonksiyon adı |
| Parametre | Komut satırı dilbilgisi | İngilizce anahtar kelime |
| Koordinat | **metre** | **milimetre tam sayı** |
| Hata | çalışma anında | bilinmeyen anahtar hemen reddedilir |

Fonksiyon adı komut kimliğinden gelir: `core.line` → `cad.line`, `core.circle_draw` →
`cad.circle_draw`. `core` dışındaki kimlikler İngilizce adlarını kendileri bildirir —
`islem.uzunluk_yaz` → `cad.label_length`, `geodesy.traverse` → `cad.traverse`.

Anahtar kelimeler **yalnız İngilizcedir** ve konumsal argüman yoktur:

```python
cad.line(noktalar=[[0, 0], [1, 1]])   # HATA: "points" bekleniyor
cad.line([[0, 0], [1, 1]])            # HATA: yalnız anahtar kelime
```

Bir komutun neyi kabul ettiğini her zaman Python'un kendisine sorabilirsiniz:

```python
help(cad.line)
print(cad.__all__)          # bütün komut fonksiyonlarının adları
```

Tam liste: [Python API referansı](../python/referans.md). O sayfa da bu fonksiyonlar da
komut kaydından üretilir; yanında `docs/python/kentos_cad.pyi` tip taslağı vardır ve
programın dışında betik yazan bir düzenleyici onu okuyup tamamlama yapabilir.

## Okuma

Hepsi **değer** döndürür: sayı, metin, liste. Hiçbiri çizimin içine tutamak vermez.

Çizime dair okumalar `cad.doc` altındadır. Bunun sebebi teknik ve önemlidir: `cad`'in üst
düzeyi **üretilen komutlara** aittir ve o küme kendiliğinden büyür. Oraya elle bir ad
koymak, aynı adı taşıyan bir komut eklendiği gün sessizce ezilmek demektir — nitekim
`KATMANLAR` ve `AYAR` gerçek komutlardır.

| Çağrı | Döndürdüğü |
|---|---|
| `cad.doc.layers()` | Katman adlarının listesi, çizimdeki sırayla |
| `cad.doc.layer_count()` | Katman sayısı |
| `cad.doc.active_layer()` | Etkin katmanın adı |
| `cad.doc.entity_count()` | Çizimdeki canlı nesne sayısı |
| `cad.doc.selection_count()` | Seçili nesne sayısı |
| `cad.doc.crs()` | Koordinat sisteminin kimliği, örneğin `TUREF/TM30` |
| `cad.doc.setting(kimlik)` | Bir ayarın değeri — `bool`, `int` ya da `str` olarak |
| `cad.sandbox()` | Bu çalıştırmanın kum havuzu seviyesi |

```python
for ad in cad.doc.layers():
    print(ad, cad.doc.entity_count())

if cad.doc.setting("core.arayuz.dinamik_girdi"):
    cad.run("TERCİH core.arayuz.dinamik_girdi hayır")
```

`cad.setting` değeri **olduğu tipte** döndürür: mantıksal bir ayar `bool` olarak gelir,
metin `str`, sayı `int`. Hepsini metin olarak döndürmek `if cad.doc.setting(...)` yazan
herkesi yanıltırdı.

## `print` nereye yazar

Doğrudan komut satırına:

```python
print("Toplam nesne:", cad.doc.entity_count())
```

Pencereli bir uygulamanın terminali yoktur; ilerlemesini `print` ile bildiren bir betik
boşluğa bildirmiş olurdu. `sys.stdout` ve `sys.stderr` betik boyunca programın kendi
çıktısına bağlanır, betik bitince eski hâline döner.

## Bir betik = bir geri alma adımı

JSON betiğinde olduğu gibi:

- Kaç komut çalıştırırsa çalıştırsın **tek `GERİAL`** ile geri alınır
- **Tek doğrulama geçişinden** geçer
- Yakalanmamış bir hata olursa **tamamı geri alınır**

```python
# Beş çizgi, tek Ctrl+Z.
for i in range(5):
    y = 4310220.400 + i
    cad.run(f"ÇİZGİ 485320.150,{y:.3f} 485370.150,{y:.3f}")
```

Son madde önemlidir. Yarım uygulanmış bir ifraz veya tevhit kabul edilemez: betik ya
tümüyle uygulanır ya hiç uygulanmaz. Python'un kendi hatası da (yazım hatası, `None`
üzerinde işlem, `raise`) aynı sonucu verir.

### `try` / `except` bu kuralı değiştirir

```python
cad.run("ÇİZGİ 0,0 10,10")
try:
    cad.run("BÖYLEBİRKOMUTYOK 1 2 3")
except Exception as hata:
    print("atlandı:", hata)
cad.run("ÇİZGİ 10,10 20,20")
```

Burada betik **başarılı** sayılır ve iki çizgi kalır: hatayı siz yakaladınız, yani
kalanının uygulanmasına siz karar verdiniz. Başarısız komut hiçbir şey bırakmaz —
geri alınan onun yaptıklarıdır, sizinkiler değil.

Bunu bilerek yapın. Yarım bir parselasyon istemiyorsanız hatayı yakalamayın, ya da
yakaladığınız yerde `raise` ile yeniden yükseltin.

## Kum havuzu

Betiğin dosya sistemine ve ağa erişimi üç seviyeyle sınırlanır ve seviye her çalıştırmada
**açıkça** verilir.

| Seviye | İzin |
|---|---|
| `güvenli` | Bağlamalarda dosya sistemi yok. Varsayılan |
| `proje` | Yalnız proje dizini ve altı |
| `tam` | Sınırsız — kullanıcının o betik için açık onayı gerekir |

### Bu seviyeler neyi kapsar, neyi kapsamaz

Dürüst olmak gerekir: **CPython kafese konamaz.** `os`, `socket`, `subprocess`,
`ctypes` ve `__import__` tek bir yorumlayıcının içindedir; Python'un kendi belgeleri de
bir Python kum havuzunun başarılabilir olmadığını söyler. `güvenli` seviyede `import os`
çalışır.

Seviyenin yönettiği şey şudur:

- **Bağlamaların kapsamı** — `cad.read_file` ve `cad.write_file` seviyeye uyar
- **Yorumlayıcının yalıtımı** — `PYTHONPATH`, `PYTHONHOME` ve `PYTHONSTARTUP` okunmaz,
  betiğin kendi dizini `sys.path`'in başına eklenmez, kullanıcı paket dizini kapalıdır
- **`tam` için istenen onay** — aşağıya bakın
- **Günlüğe düşen kayıt** — hangi seviyede çalıştığı her çalıştırmada yazılır

Yani: güvenmediğiniz bir Python betiğini çalıştırmayın. Seviye, betiğin **niyetini**
beyan etmesini ve programın bunu kayda geçirmesini sağlar; kötü niyetli koda karşı bir
duvar değildir. Bunu söylemeyen bir kural, çalışma zamanının veremeyeceği bir söz
vermiş olurdu.

### `proje` seviyesinde dosya

İki çağrı vardır ve ikisi de proje dizininin dışına çıkamaz:

```python
metin = cad.read_file("olcum.txt")
cad.write_file("rapor.txt", f"Toplam: {cad.doc.entity_count()}")
```

Yol **çözülerek** denetlenir. `../../etc/passwd` yazarak dışarı çıkamazsınız: metin
olarak proje dizininin içinde görünse de gerçekte dışındadır ve reddedilir.

`güvenli` seviyede aynı çağrılar şu yanıtı verir:

```text
Dosya okuma 'güvenli' kum havuzunda kapalıdır. Gerekli seviye: 'proje' veya 'tam'.
```

### `tam` seviyesi ve onay

`tam`, betiğin kendisine verilen bir onaydır. Onaysız çalıştırılırsa **hiçbir komutu
çalışmaz**:

```text
'tam' kum havuzu bu betik için onaylanmamış. Onay betiğin kendisine verilir;
dosya değiştiyse yeniden sorulur.
```

Onay, betiğin **metnine** verilir. Dosyayı bir harf değiştirirseniz onay düşer ve
yeniden sorulur — çünkü onayladığınız betik artık o betik değildir.

Bu seviyeyi açan hiçbir ayar, ortam değişkeni, komut satırı bayrağı veya betik başlığı
yoktur. Yalnızca kullanıcının sorulan soruya verdiği yanıt açar.

## Günlüğe ne yazılır

Her çalıştırma, ilk komuttan **önce**, günlüğe kendi kaydını yazar:

```json
{"kind":"meta","ne":"betik","konak":"python","ad":"parsel.py","kum_havuzu":"proje","kimlik":"a3f1c0d2e4b58971"}
```

`kimlik`, betik metninin özetidir; `tam` seviyede bu satıra `"onay":true` de eklenir.
Bu kayıt **çalıştırılmaz**: günlük tekrar oynatıldığında atlanır, çünkü neyin
yapıldığını değil neye izin verildiğini anlatır.

Ayrıntı: [Komut günlüğü](../mimari/gunluk.md).

## Uzun betiği durdurmak

Motor iptal edilebilir. Bir durdurma isteği geldiğinde yorumlayıcıya kesme gönderilir ve
betik en geç birkaç milisaniye içinde durur — sonsuz döngü de dahil:

```text
Betik iptal edildi.
```

Durdurulan betik yarım bırakmaz — o ana kadar yapılan her şey geri alınır. Betik kesmeyi
`except BaseException` ile yutsa bile çalıştırma iptal sayılır ve geri alınır; yutmak
çalışmayı sürdürmenin yolu değildir.

Tek istisna, kilidi bırakmadan çalışan bir C uzantısıdır: o çağrı dönene kadar kesme
beklemede kalır. `time.sleep` böyle değildir, anında durur.

Bu sürümde betik hâlâ arayüz iş parçacığında çalışır, yani uzun bir betik pencereyi
bekletir ve iptali tetikleyecek bir düğme yoktur. Betiği kendi iş parçacığına taşıyan
ve **İptal** düğmesini ekleyen çalışma Faz 2'dedir; motor tarafı hazırdır.

## Çalıştırma

| Yol | Nasıl |
|---|---|
| Komut satırından | `BETİK olcum.py` |
| Menüden | **Dosya > Betik Çalıştır…** veya **Ctrl+R** |
| Açılışta | `kentos_cad --betik olcum.py` |
| Make ile | `make run-script SCRIPT=olcum.py` |

Uzantı hangi motorun çalışacağını belirler: `.py` Python motoruna, geri kalanı JSON
çalıştırıcısına gider.

## Örnek: bir ada boyunca parsel cepheleri

```python
# Ada sınırındaki her parsele 20 m cephe, 30 m derinlik.
x0, y0 = 485300.000, 4310200.000
cephe, derinlik, adet = 20.0, 30.0, 6

cad.run("KATMAN PARSEL")

for i in range(adet):
    sol = x0 + i * cephe
    sag = sol + cephe
    alt, ust = y0, y0 + derinlik

    cad.run(f"ÇİZGİ {sol:.3f},{alt:.3f} {sag:.3f},{alt:.3f} "
            f"{sag:.3f},{ust:.3f} {sol:.3f},{ust:.3f} {sol:.3f},{alt:.3f}")

cad.run("YAKINLAŞ KAPSAM")
```

Yirmi dört köşe, altı parsel, tek **Ctrl+Z**.

## Nesne başına çalışan ifadeler Python'a yazılmaz

Bir etiket ifadesi, bir stil kuralı ya da bir alan hesabı **nesne başına** çalışır ve bir
kadastro paftasında bu milyonlarca kez demektir. Oraya Python çağrısı koymak yanlıştır —
bütçe ne olursa olsun yanlıştır.

Onların yeri komut satırının kendi ifade motorudur: derlenmiş, deterministik ve zaten
programın tek dilbilgisi. Bkz. [Komut satırı](../komutlar/komut-satiri.md).

Python'un yeri şudur: eklentiler, toplu işleme, veri boru hatları ve bilimsel analiz.

## Sırada ne var

- [Betik yazma](README.md) — JSON biçimi, argümanlar, örnekler
- [BETİK komutu](../komutlar/script.md) — komutun kendisi
- [Komut satırı](../komutlar/komut-satiri.md) — `cad.run`'a yazdığınız metnin dilbilgisi
- [Komut günlüğü](../mimari/gunluk.md) — yaptığınız işi betiğe çevirmek
