# Lua Betikleri

Döngü, koşul ve hesap gerektiren işleri otomatikleştirmek isteyen kullanıcı için; bu
sayfayı bitirdiğinizde bir Lua betiği yazabilecek, çizimden veri okuyabilecek ve kum
havuzunun neye izin verip neye vermediğini bileceksiniz.

[JSON betiği](README.md) düz bir komut dizisidir: değişken yok, döngü yok, koşul yok.
Beş yerine beş yüz çizgi çizmek gerektiğinde beş yüz satır yazmak zorunda kalırsınız.
Lua bunun için vardır.

## Bu sürümde durumu

Lua motoru **seçimlik** derlenir ve varsayılan yapıda **kapalıdır**. Açmak için:

```bash
cmake --preset dev -DKENTOS_WITH_LUA=ON
cmake --build --preset dev
```

Makinenizde Lua kurulu olması gerekmez; kaynak sabitlenmiş bir sürümden indirilip
programla birlikte derlenir. Ayrıntı: [Kurulum](../baslangic/kurulum.md).

Kapalı yapıda `.lua` dosyası çalıştırmayı denerseniz `BETİK` komutu JSON beklediğini
söyleyerek hata döndürür; çizim değişmez.

## İlk betik

```lua
h.komut("KATMAN PARSEL")
h.komut("ÇİZGİ 485320.150,4310220.400 485370.150,4310250.400")
```

`h` tablosu betiğin programa açılan tek kapısıdır. İki işi vardır: **komut çalıştırmak**
ve **çizimden okumak**.

## `h.komut` — tek yazma yolu

```lua
h.komut("ÇİZGİ 485320.150,4310220.400 485370.150,4310250.400")
h.komut("ÇİZGİ", "485320.150,4310220.400", "485370.150,4310250.400")   -- aynısı
```

Birden çok argüman verirseniz araya boşluk konarak birleştirilir. İkisi de aynı satırı
üretir; hangisinin okunaklı olduğuna siz karar verin.

**Yazdığınız metin, komut satırına yazdığınızın aynısıdır.** Aynı ayrıştırıcı, aynı
doğrulama, aynı geri alma yığını, aynı günlük. Bu yüzden:

- Koordinatlar **metredir**, JSON betiğindeki gibi milimetre değil. Komut satırına ne
  yazıyorsanız onu yazın.
- Türkçe komut adları, kısaltmalar ve İngilizce adlar aynı biçimde çalışır:
  `ÇİZGİ`, `CIZGI`, `LINE`, `Ç` hepsi aynı komuttur.

`h.komut` çalıştırdığı komutun kaç ilkel düzenleme yaptığını döndürür:

```lua
local n = h.komut("ÇİZGİ 0,0 10,10")
```

Bir komut başarısız olursa betik **orada durur** ve o ana kadar yapılan her şey geri
alınır. Aşağıya bakın.

### Çizimi değiştirmenin başka yolu yoktur

`h.komut` dışında çizime dokunan hiçbir bağlantı yoktur — bilerek. Çizimi değiştiren her
şey komut olmak zorundadır, çünkü doğrulama, geri alma ve günlük oraya bağlıdır. Bir
betiğin çizime "kısa yoldan" ulaşabilmesi, o üçünü atlayabilmesi demek olurdu.

## Okuma

Hepsi **değer** döndürür: sayı, metin, tablo. Hiçbiri çizimin içine tutamak vermez.

| Çağrı | Döndürdüğü |
|---|---|
| `h.katmanlar()` | Katman adlarının tablosu, çizimdeki sırayla |
| `h.katman_sayisi()` | Katman sayısı |
| `h.aktif_katman()` | Etkin katmanın adı |
| `h.nesne_sayisi()` | Çizimdeki canlı nesne sayısı |
| `h.secim_sayisi()` | Seçili nesne sayısı |
| `h.krs()` | Koordinat sisteminin kimliği, örneğin `TUREF/TM30` |
| `h.ayar(kimlik)` | Bir ayarın değeri — mantıksal, sayı ya da metin olarak |
| `h.kum_havuzu()` | Bu çalıştırmanın kum havuzu seviyesi |

```lua
for _, ad in ipairs(h.katmanlar()) do
    print(ad, h.nesne_sayisi())
end

if h.ayar("ızgara.acik") then
    h.komut("TERCİH ızgara.acik hayır")
end
```

## Bir betik = bir geri alma adımı

JSON betiğinde olduğu gibi:

- Kaç komut çalıştırırsa çalıştırsın **tek `GERİAL`** ile geri alınır
- **Tek doğrulama geçişinden** geçer
- Bir komut başarısız olursa **tamamı geri alınır**

```lua
-- Beş çizgi, tek Ctrl+Z.
for i = 0, 4 do
    local y = 4310220.400 + i
    h.komut(string.format("ÇİZGİ 485320.150,%.3f 485370.150,%.3f", y, y))
end
```

Son madde önemlidir. Yarım uygulanmış bir ifraz veya tevhit kabul edilemez: betik ya
tümüyle uygulanır ya hiç uygulanmaz. Lua'nın kendi hatası da (yazım hatası, `nil`
üzerinde işlem, `error()` çağrısı) aynı sonucu verir.

## Kum havuzu

Betiğin dosya sistemine ve ağa erişimi üç seviyeyle sınırlanır ve seviye her çalıştırmada
**açıkça** verilir.

| Seviye | İzin |
|---|---|
| `güvenli` | Dosya sistemi yok, ağ yok. Varsayılan |
| `proje` | Yalnız proje dizini ve altı |
| `tam` | Sınırsız — kullanıcının o betik için açık onayı gerekir |

### `güvenli` seviyede ne yoktur

Lua'nın `io`, `os`, `package` ve `debug` kütüphaneleri **hiç açılmaz**; `require`,
`dofile`, `loadfile` ve `load` de yoktur. Bunlar sonradan kapatılmış değildir — hiç
verilmemiştir, ki bir yolu unutulup açık kalmasın.

Kalan her şey durur: `string`, `table`, `math`, `utf8`, `coroutine` ve `h`.

### `proje` seviyesinde dosya

İki çağrı vardır ve ikisi de proje dizininin dışına çıkamaz:

```lua
local metin = h.dosya_oku("olcum.txt")
h.dosya_yaz("rapor.txt", "Toplam: " .. h.nesne_sayisi())
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
{"kind":"meta","ne":"betik","konak":"lua","ad":"parsel.lua","kum_havuzu":"proje","kimlik":"a3f1c0d2e4b58971"}
```

`kimlik`, betik metninin özetidir; `tam` seviyede bu satıra `"onay":true` de eklenir.
Bu kayıt **çalıştırılmaz**: günlük tekrar oynatıldığında atlanır, çünkü neyin
yapıldığını değil neye izin verildiğini anlatır.

Ayrıntı: [Komut günlüğü](../mimari/gunluk.md).

## Uzun betiği durdurmak

Motor iptal edilebilir: bir durdurma isteği geldiğinde yorumlayıcı en geç birkaç
mikrosaniye içinde durur, sonsuz döngü de dahil.

```text
Betik iptal edildi.
```

Durdurulan betik yarım bırakmaz — o ana kadar yapılan her şey geri alınır.

Bu sürümde betik hâlâ arayüz iş parçacığında çalışır, yani uzun bir betik pencereyi
bekletir ve iptali tetikleyecek bir düğme yoktur. Betiği kendi iş parçacığına taşıyan
ve **İptal** düğmesini ekleyen çalışma Faz 2'dedir; motor tarafı hazırdır.

## Çalıştırma

| Yol | Nasıl |
|---|---|
| Komut satırından | `BETİK olcum.lua` |
| Menüden | **Dosya > Betik Çalıştır…** veya **Ctrl+R** |
| Açılışta | `kentos_cad --betik olcum.lua` |
| Make ile | `make run-script SCRIPT=olcum.lua` |

Uzantı hangi motorun çalışacağını belirler: `.json` JSON çalıştırıcısına, `.lua` Lua
motoruna gider.

## Örnek: bir ada boyunca parsel cepheleri

```lua
-- Ada sınırındaki her parsele 20 m cephe, 30 m derinlik.
local x0, y0 = 485300.000, 4310200.000
local cephe, derinlik, adet = 20.0, 30.0, 6

h.komut("KATMAN PARSEL")

for i = 0, adet - 1 do
    local sol  = x0 + i * cephe
    local sag  = sol + cephe
    local alt  = y0
    local ust  = y0 + derinlik

    h.komut(string.format(
        "ÇİZGİ %.3f,%.3f %.3f,%.3f %.3f,%.3f %.3f,%.3f %.3f,%.3f",
        sol, alt, sag, alt, sag, ust, sol, ust, sol, alt))
end

h.komut("YAKINLAŞ KAPSAM")
```

Yirmi dört köşe, altı parsel, tek **Ctrl+Z**.

## Ne için Lua, ne için Python

İkisinin işi farklıdır ve karıştırılmamalıdır:

| Katman | Dil | Nerede |
|---|---|---|
| Hızlı yol | **Lua** | Etiket ifadeleri, stil kuralları, alan hesapları, hafif makrolar |
| Ekosistem | **Python** (Faz 2, seçimlik modül) | Eklentiler, toplu işleme, veri boru hatları, bilimsel analiz |

Nesne başına çalışan bir ifade — bir kadastro paftasında milyonlarca kez — Lua'dır.
Python o yerde bir kusurdur.

## Sırada ne var

- [Betik yazma](README.md) — JSON biçimi, argümanlar, örnekler
- [BETİK komutu](../komutlar/script.md) — komutun kendisi
- [Komut satırı](../komutlar/komut-satiri.md) — `h.komut`'a yazdığınız metnin dilbilgisi
- [Komut günlüğü](../mimari/gunluk.md) — yaptığınız işi betiğe çevirmek
