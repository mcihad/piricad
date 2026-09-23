# NESNEBİLGİ — Nesne Bilgisi

Çizimdeki bir nesnenin **ne olduğunu** tek soruda öğrenmek isteyen herkes için; bu
sayfayı bitirdiğinizde bir parselin türünü, katmanını, köşe sayısını, çevresini,
alanını ve özniteliklerini arayüzden, komut satırından ve betikten okumayı
bileceksiniz.

## Ne yapar

`NESNEBİLGİ`, seçtiğiniz nesneler için şu soruya cevap verir: **bu nedir?**

Bu bilgilerin hepsi programda zaten vardı ama hiçbiri **tek soruda** alınamıyordu:
alanı [`ALANÖLÇ`](measure_area.md), katmanı katman panelinden, ada numarasını
öznitelik tablosundan okumak gerekiyordu — bir bakış için üç yol. Bu komut hepsini
bir satırda söyler ve aynı cevabı **yapılandırılmış** olarak da döndürür, böylece
betik ve yapay zeka istemcisi Türkçe cümle ayrıştırmak zorunda kalmaz.

Her nesne için yazılanlar:

| Ne | Nereden gelir |
|---|---|
| Tür | Nesne türleri tablosundaki Türkçe adı (`ÇİZGİ`, `DAİRE`, `ALAN` …) |
| Katman | Nesnenin bulunduğu katmanın adı |
| Halka sayısı | Dış sınır ve delikler; delikli bir alanda birden çoktur |
| Köşe sayısı | Bütün halkaların tepe sayısı toplamı |
| Çevre | Metre, üç ondalık. Dairede çevre gerçek çevredir, çap değildir |
| Alan | Metrekare, iki ondalık — tapunun taşıdığı hassasiyet |
| Kapsam | Nesnenin sınır dikdörtgeni, milimetre olarak `[solY, altX, sağY, üstX]` |
| Öznitelikler | Dolu olan her hücre; boş hücre hiç yazılmaz |

Tür adı bu komutun içine **yazılmamıştır**: nesne türleri tablosundan okunur. Bir
eklenti yeni bir tür tanımladığında adı kendiliğinden buradan da çıkar.

Komut hiçbir şeyi değiştirmez: **geri alma adımı yemez**, günlüğe bir belge
değişikliği olarak düşmez. Bir soru, bir düzenleme değildir.

## Adlar

| Ad | Tür |
|---|---|
| `NESNEBİLGİ` | Türkçe, birincil |
| `NESNEBILGI` | ASCII katlanmış Türkçe |
| `OBJINFO` | İngilizce karşılık |
| `NB` | Kısaltma |
| `core.entity_info` | Komut kimliği |

## Sözdizimi

```text
NESNEBİLGİ
NESNEBİLGİ nesneler=<kimlik>
NESNEBİLGİ nesneler=<kimlik> nesneler=<kimlik> …
```

Argümansız çağırırsanız komut nesneleri **sorar**: seçim varsa onu kullanır, yoksa
tuvalde seçmenizi bekler ve `Enter` ile bitirirsiniz.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Bilgisi istenen nesnelerin kalıcı kimlikleri. Verilmezse seçim kullanılır, seçim de yoksa sorulur |

## Örnekler

### Komut satırı

```text
KATMAN ad=PARSEL
ALAN 0,0 20,0 20,10 0,10
NESNEBİLGİ nesneler=1
```

Yazılan:

```text
Nesne 1 — ÇOKLUÇİZGİ, katman PARSEL, 4 köşe, çevre 60,000 m, alan 200,00 m².
```

Birden çok nesne verirseniz her biri için bir satır çıkar; bulunamayan ya da silinmiş
bir kimlik kendi satırında söylenir ve komut kalanlarla devam eder.

### Arayüz

**Harita → Nesne Bilgisi**, ya da araç kolonundaki ölçüm ailesinde **Nesne Bilgisi**
(ölçüm düğmesini basılı tutunca açılan listede). Araç kollanır, tuvalde nesneleri
seçer, `Enter`'a basarsınız. Cevap sözle geldiği için **komut günlüğü paneli
kendiliğinden açılır** — cevabın kapalı bir çekmeceye yazılmaması için.

### Betik

Betik önce yan yana üç parsel çizer, sonra üçünün bilgisini ister:

```json
{
  "ad": "Parsel bilgisi",
  "komutlar": [
    { "cmd": "core.area", "args": { "noktalar": [[0,0],[20000,0],[20000,15000],[0,15000]] } },
    { "cmd": "core.area", "args": { "noktalar": [[20000,0],[40000,0],[40000,15000],[20000,15000]] } },
    { "cmd": "core.area", "args": { "noktalar": [[40000,0],[60000,0],[60000,15000],[40000,15000]] } },
    { "cmd": "core.entity_info", "args": { "nesneler": [1, 2, 3] } }
  ]
}
```

### Üçü de aynı

Aynı nesne için arayüzden, komut satırından ve betikten sorulan bilgi **bayt bayt
aynı** yapılandırılmış cevabı verir ve üçü de belgeyi ve geri alma yığınını
bulduğu gibi bırakır. Bu, `tests/unit/test_proof.cpp` içindeki
`PROOF: NESNEBİLGİ gui, komut satırı ve betikten aynı cevabı verir` vakasıyla
kanıtlanır.

## Yapılandırılmış cevap

Betik ve yapay zeka istemcisi cümleyi değil bu tabloyu okur:

```json
{
  "adet": 1,
  "nesneler": [
    {
      "nesne": 1, "tur": "ÇOKLUÇİZGİ", "tur_no": 1, "katman": "PARSEL",
      "halka": 1, "kose": 4, "cevre_mm": 60000, "alan_mm2": 200000000,
      "kapsam": [0, 0, 20000, 10000],
      "oznitelik": { "ada": "1453" }
    }
  ],
  "surum": 2
}
```

Uzunluklar **milimetre**, alanlar **milimetrekare** tam sayı olarak verilir: metreye
çevirmek okuyanın işidir, çünkü belgenin sakladığı değer budur ve yuvarlama
kararını program sizin yerinize vermez.

## Geri alma

Geri alınacak bir şey yoktur. `NESNEBİLGİ` hiçbir şeyi değiştirmediği için
`Ctrl+Z` bu komuttan önceki son düzenlemeyi geri alır.

## Betikten kullanım

`core.entity_info` betikten çağrılabilir ve **yapay zeka okuma aracıdır**
(`Flags::AiAccessible`). Bir ajan neyin ne olduğunu sorabildiği için, çizim
hakkında kendisine anlatılmadan konuşabilir. Ajan yolunda nesneler daima
**argümanla** verilir; ortam seçimine güvenilmez.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Geçersiz nesne kimliği: 0. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik verildi | Kalıcı kimlikleri `SEÇİMBİLGİSİ` ile okuyun |
| `Nesne bulunamadı veya silinmiş: 7` | Kimlik hiç var olmadı ya da nesne silindi | `GERİAL` ile geri getirin ya da kimliği doğrulayın |
| `Bilgisi verilecek nesne bulunamadı.` | Verilen kimliklerin hiçbiri yaşamıyor | Seçimi yenileyin |

## İlgili sayfalar

- [`ALANÖLÇ`](measure_area.md) — yalnız alan ve çevre, toplamıyla
- [`SORGULA`](query.md) — koşula uyan nesneleri sayar ve seçer
- [`ÖZNİTELİK`](attribute.md) — hücreyi okur ve yazar
- [`AÇIÖLÇ`](measure_angle.md) — bir köşedeki açı
