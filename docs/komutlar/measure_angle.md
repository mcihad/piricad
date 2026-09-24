# AÇIÖLÇ — Açı Ölç

Bir köşedeki açıyı **öğrenmek** isteyen, çizmek istemeyen herkes için; bu sayfayı
bitirdiğinizde bir tepe ve iki kol vererek açıyı oturumunuzun birim ve kuralıyla
okumayı arayüzden, komut satırından ve betikten bileceksiniz.

## Ne yapar

`AÇIÖLÇ`, bir **tepe** ve iki **kol noktası** alır, iki kol arasındaki açıyı ölçer
ve yazar. Çizime hiçbir şey eklemez.

[`ÖLÇÜ tur=acisal`](dimension.md) ile karıştırılmamalıdır: o komut paftaya **açısal
ölçü nesnesi çizer** ve bir geri alma adımı yer. Bu komut ise elin şeritmetreyle
sorup unuttuğu soruyu sorar — *bu köşe kaç?* — ve cevabı komut günlüğüne yazar.

### İki açı, ikisi de yazılır

Bir köşe **iki açıdır**: gidiş ve bütünleyeni. `AÇIÖLÇ` birinci koldan ikinci kola
olan süpürmeyi verir, **tersini de yanında** yazar. Program hangisinin
kastedildiğine sessizce karar vermez, çünkü bu karar bir sınırın hangi tarafının
*iç* olduğuna karar vermek demektir.

Süpürme, oturumun **kuralının kendi yönündedir**:

| `core.aci.kural` | Sıfır nerede | Hangi yöne büyür |
|---|---|---|
| `semt` (varsayılan) | Kuzey | Saat yönünde — aletin okuduğu |
| `matematik` | Doğu | Saat yönünün tersine — trigonometrinin okuduğu |

Birim `core.aci.birim` ayarından gelir: **grad** (varsayılan), derece ya da radyan.
Yazılan açı ile `@mesafe<açı` yazarken okunan açı **tek ayar çiftidir** — bir yerde
grad, öbüründe derece yazan program yoktur.

## Adlar

| Ad | Tür |
|---|---|
| `AÇIÖLÇ` | Türkçe, birincil |
| `ACIOLC` | ASCII katlanmış Türkçe |
| `MEASUREANGLE` | İngilizce karşılık |
| `AÇÖ` | Kısaltma |
| `core.measure_angle` | Komut kimliği |

`AÖ` bu komutun kısaltması **değildir**: o [`ALANÖLÇ`](measure_area.md)'in
kısaltmasıdır.

## Sözdizimi

```text
AÇIÖLÇ
AÇIÖLÇ <tepe> <birinci> <ikinci>
AÇIÖLÇ tepe=<n> birinci=<n> ikinci=<n>
```

Nokta yazımı [`ÇİZGİ`](line.md) ile aynıdır: mutlak (`485320,4310220`), göreli
(`@50,30`), kutupsal (`@100<45`) ve nokta fonksiyonları (`orta(A,B)`, `n(1284)`).

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `tepe` | Açının tepe noktası — iki kolun birleştiği yer |
| `birinci` | Birinci kolun üzerinde herhangi bir nokta. Süpürme buradan başlar |
| `ikinci` | İkinci kolun üzerinde herhangi bir nokta. Süpürme burada biter |

Kol noktalarının tepeden **uzaklığı fark etmez**: yalnız doğrultuları okunur. Kol
uzunlukları yine de yazılır, çünkü yanlış noktayı tıkladığınızı en çabuk oradan
anlarsınız.

## Örnekler

### Komut satırı — dik açı, semt kuralında

```text
AÇIÖLÇ 0,0 0,10 10,0
```

Yazılan:

```text
Açı: 100,0000 grad   (ters: 300,0000 grad)   kenarlar: 10,000 m ve 10,000 m   (kuzeyden saat yönünde)
```

Birinci kol kuzeye, ikinci kol doğuya bakar. Semt kuralı saat yönünde büyüdüğü için
kuzeyden doğuya süpürme çeyrek turdur: **100 grad**.

### Kollar değişirse

```text
AÇIÖLÇ 0,0 10,0 0,10
```

```text
Açı: 300,0000 grad   (ters: 100,0000 grad)   …
```

İki okuma yer değiştirir. İkisinin birlikte yazılmasının sebebi tam olarak budur.

### Aynı köşe, matematik kuralında ve derecede

```text
AYAR açı_birimi derece
MOD kural matematik
AÇIÖLÇ 0,0 10,0 0,10
```

```text
Açı: 90,0000°   (ters: 270,0000°)   …   (doğudan saat yönünün tersine)
```

Aynı üç nokta, başka bir sözleşme. `core.aci.birim` bir **proje** ayarıdır ve
[`AYAR`](setting.md) ile değişir; `core.aci.kural` bir **oturum** ayarıdır ve
[`MOD`](mode.md) ile değişir.

### Arayüz

Şeritte **Harita ▸ Ölçüm ▸ Açı Ölç** (kendi simgesi vardır; eskiden `ÖLÇ` ile aynı
cetveli kullanıyordu). Araç kollanır; tepeye, sonra birinci kola, sonra ikinci kola tıklarsınız.

**Üçüncü tıklamaya kadar ölçtüğünüz açı ekranda durur.** Birinci kol yerinde
kalır, imlece ikinci kol uzanır, ve tepede ikisi arasındaki **süpürme bir yay
olarak** çizilir; yayın yanında okuma yazar. Eskiden her iki kol da tepeden
imlece giden düz bir çizgi olarak gösteriliyordu, yani ikinciyi nişanlarken
birincisi ekrandan kayboluyor ve komutun tek ölçtüğü şey — aradaki açı — cevap
döküme düşene kadar hiçbir yerde görünmüyordu.

**Yay, komutun bildirdiği süpürmedir.** Kısa olanı çizip uzun olanı yazmak gibi
bir şey yapılmaz: süpürme **birinci koldan ikinciye**, oturumun kuralının kendi
yönünde okunur (varsayılan *semt*'te saat yönünde). Yani kolları hangi sırayla
tıkladığınız sizin seçiminizdir; ters çevirince tümler açıyı alırsınız. Komut
ikisini de yazar.

Yakalama açıktır: köşeye `UÇ`, kenar ortasına `ORTA` yakalar, böylece ölçtüğünüz
açı gerçekten çizimdeki köşedir, tıklamanızın piksel hassasiyeti değildir.

**Okuma tuvalde kalır.** Üçüncü tıklamadan sonra iki kol, süpürme yayı ve açının
değeri vurgulu çizili kalır; çizim değişince ya da hiçbir komut çalışmıyorken Esc'e
basınca silinir.

### Betik

```json
{
  "ad": "Köşe açısı",
  "komutlar": [
    { "cmd": "core.measure_angle",
      "args": { "tepe": [0, 0], "birinci": [0, 10000], "ikinci": [10000, 0] } }
  ]
}
```

### Üçü de aynı

Aynı köşe arayüzden, komut satırından ve betikten sorulduğunda **bayt bayt aynı**
yapılandırılmış cevabı verir; hiçbiri belgeye dokunmaz.
`tests/unit/test_proof.cpp` içindeki `PROOF: AÇIÖLÇ gui, komut satırı ve betikten
aynı açıyı okur` vakası bunu kanıtlar.

## Yapılandırılmış cevap

```json
{
  "aci_udeg": 90000000, "ters_udeg": 270000000,
  "aci_metin": "100,0000 grad", "ters_metin": "300,0000 grad",
  "birinci_kenar_mm": 10000, "ikinci_kenar_mm": 10000,
  "birim": "g", "kural": "kuzeyden saat yönünde"
}
```

`aci_udeg` **mikro-derecedir** — tam dairede 360 000 000 — ve birimden bağımsızdır:
sayı olarak okunacak değer budur, çünkü tam sayıdır ve iki makinede aynı biti verir.
`aci_metin` ise oturumun birimiyle yazılmış, ekranda görünen metnin aynısıdır.
`birim` alanı `g`, `d` ya da `r` harfidir — `@100<45g` yazarken kullanılan sonekin
aynısı.

## Geri alma

Geri alınacak bir şey yoktur. `AÇIÖLÇ` hiçbir şeyi değiştirmediği için `Ctrl+Z`
bu komuttan önceki son düzenlemeyi geri alır. Paftada kalıcı bir açı ölçüsü
istiyorsanız [`ÖLÇÜ tur=acisal`](dimension.md) kullanın.

## Betikten kullanım

`core.measure_angle` betikten çağrılabilir ve **yapay zeka okuma aracıdır**.
Ajan yolunda noktalar yalnız **araç sonucu tutamağıyla** verilir: bir model
koordinat uyduramaz (CLAUDE.md 5.8).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bir kolun noktası tepeyle aynı: o kolun doğrultusu yok.` | Kol noktası tepenin üstüne düştü | Kolun üzerinde tepeden farklı bir nokta seçin |

## İlgili sayfalar

- [`ÖLÇ`](measure.md) — iki nokta arası mesafe, koordinat farkı ve açı
- [`ÖLÇÜ`](dimension.md) — paftaya çizilen ölçü nesnesi, açısal olanı dâhil
- [`APLİKASYON`](stakeout.md) — istasyondan hedefe semt açısı ve kenar
- [`MOD`](mode.md) — açı kuralı ve yakalama ayarları
- [`NESNEBİLGİ`](entity_info.md) — bu nesne nedir
