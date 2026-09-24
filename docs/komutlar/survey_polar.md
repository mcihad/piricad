# ALIM — Açı ve Kenarla Nokta

Elinde bir istasyonun ölçü karnesi olan herkes için; bu sayfayı bitirdiğinizde
okunan açı ve kenarları arayüzden, komut satırından ve betikten koordinata
çevirmeyi bileceksiniz.

## Ne yapar

Bilinen bir noktada duran alet her detay için bir **açı** ve bir **kenar** okur.
`ALIM` bu iki sütunu koordinata çevirir: istasyon, isteğe bağlı bağlama noktası,
sonra sırayla açı–kenar çiftleri. Her çift bir nokta olur.

**`APLİKASYON`un tersidir.** Aplikasyon koordinattan açı ve kenar üretir; bu,
açı ve kenardan koordinat üretir. İkisi de `core/angle.hpp` üzerinden geçer, yani
aplike edilen bir nokta ile geri okunan aynı nokta milimetresine kadar aynı yere
düşer — iki ayrı yuvarlamaya değil.

### Bağlama noktası açının anlamını değiştirir

| Bağlama | Okunan açı ne demek |
|---|---|
| Verilmedi | Açı **semt açısıdır**: kuzeyden itibaren sayılır |
| Verildi | Açı **bağlamadan itibaren** sayılır; alet o doğrultuda sıfırlanmıştır |

İkincisi bir aletin gerçekten okuduğu şeydir. Hangisi olduğu her koordinatı
değiştirir, bu yüzden komut hangisini kullandığını yazar.

Birim ve kural oturumun ayarlarındandır: varsayılan **grad** ve **kuzeyden saat
yönünde** (`core.aci.birim`, `core.aci.kural`). Bir Türk ölçü karnesi böyle
yazılır ve `@mesafe<açı` de aynı kuralı okur.

## Adlar

| Ad | Tür |
|---|---|
| `ALIM` | Türkçe, birincil |
| `SURVEY` | İngilizce karşılık |
| `ALM` | Kısaltma |
| `core.survey_polar` | Komut kimliği |

`AL` değil `ALM`: `AL` `ALAN` komutunun kısaltmasıdır.

## Sözdizimi

```text
ALIM <istasyon> [baglama=<nokta>] aci=<açı> kenar=<m> [aci=… kenar=… …] [cizgi=evet]
```

`aci` ve `kenar` **sırayla eşleşir**. Kaç çift verirseniz o kadar nokta çıkar.

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `istasyon` | nokta | 1 | Aletin durduğu bilinen nokta |
| `baglama` | nokta | 0..1 | Verilirse açılar bu doğrultudan itibaren okunmuş sayılır |
| `aci` | sayı | 0..n | Okunan açı, oturumun açı biriminde |
| `kenar` | sayı | 0..n | Alete olan uzaklık, metre; eksi olamaz |
| `cizgi` | mantıksal | 0..1 | Noktaları okundukları sırayla çizgiyle birleştirir |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

`aci` ve `kenar` **sırayla eşleşir** ve **sayıca eşit olmalılar**; eşit değilse
komut hiçbir şey yapmaz ve iki sayıyı da söyler. Yarım bir karne, hiç
olmamasından kötüdür.

Okumaları **adıyla** verin (`aci=50 kenar=42.315`): çıplak sayılar üçüncü
parametreye — bağlama noktasına — bağlanmaya çalışır ve komut daha gövdesi
çalışmadan reddedilir.

## Örnekler

### Komut satırı

Varsayılanlarla (grad, kuzeyden saat yönünde) dört ana yön:

```text
ALIM 0,0 aci=0 kenar=100 aci=100 kenar=100 aci=200 kenar=100 aci=300 kenar=100
```

```text
4 nokta alımdan hesaplandı (semt açısı, kuzeyden saat yönünde).
```

Noktalar sırasıyla kuzeyde, doğuda, güneyde ve batıda 100 metrededir.

Doğuya bakan bir bağlamayla — alet doğuda sıfırlanmış:

```text
ALIM 0,0 baglama=50,0 aci=0 kenar=100 aci=100 kenar=100
```

```text
2 nokta alımdan hesaplandı (bağlamaya göre açı, kuzeyden saat yönünde).
```

Birinci nokta **doğuda** (okunan 0, bağlamanın kendi doğrultusu), ikincisi
**güneyde** (okunan 100 grad, yani semt 200 grad).

Bir sınırı tek istasyondan dolaşıp birleştirmek:

```text
ALIM 0,0 aci=0 kenar=50 aci=100 kenar=50 aci=200 kenar=50 aci=300 kenar=50 cizgi=evet
```

### Arayüz

Şeritte **Çizim ▸ Nokta ve Alım ▸ Alım**'a basın ya da **Giriş ▸ Çizim** panelindeki
**Nokta** düğmesinin okundan **Alım**'ı seçin.

**İstasyon ekranda kalır**, ve `baglama` verdiyseniz bağlama doğrultusu da:
karne yazılırken aletin durduğu yer işaretli durur. İkisi de çizimin nesnesi
değil, komutun hatırladığı noktalardır; eskiden istasyon verildiği anda ekrandan
kayboluyor ve her açı görünmeyen bir şeye göre okunuyordu.

1. İstasyona tıklayın (nokta yakalama açıkken mevcut nirengiye yakalar).
2. Komut satırı **açı** ister ve odak kendiliğinden oraya geçer; okumayı yazın.
3. Ardından **kenar** ister; onu da yazın. Nokta hemen çizime düşer.
4. 2. ve 3. adım tekrarlanır. Bitirmek için **sağ tık** ya da **Esc**.

Bağlama noktası arayüzde ayrı bir istem değildir; gerekiyorsa komut satırına
`baglama=` ile yazılır — bir istasyonun bağlaması bir kez verilir, her nokta için
değil.

### Betik

```json
{
  "ad": "1. istasyon",
  "komutlar": [
    { "cmd": "core.survey_polar", "args": {
        "istasyon": [485320150, 4310220400],
        "baglama": [485370150, 4310220400],
        "aci":   [0, 100, 200],
        "kenar": [42.315, 56.720, 38.005],
        "cizgi": true } }
  ]
}
```

Betikte koordinatlar **milimetredir**, `kenar` metre, `aci` oturumun açı
birimindedir.

### Üçü de aynı

Aynı istasyon ve aynı iki okuma, üç istemciden aynı belgeyi ve bayt bayt aynı
günlüğü bırakır; `tests/unit/test_proof.cpp` bunu sınar.

## Geri alma

Bir çağrı tek bir işlemdir: hesaplanan noktalar ve — `cizgi=evet` verildiyse —
onları birleştiren çizgi tek bir **Ctrl+Z** ile birlikte gider.

## Betikten kullanım

`ALIM` betiklenebilir ve yapay zekâya açıktır. `aci` ve `kenar` birer **dizi**
olarak verilir; eşleşme sıraya göredir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Kenar eksi olamaz; bir uzaklığın işareti yoktur.` | `kenar`a eksi değer verildi | Uzaklığı artı yazın; yön açıdan gelir |
| `'core.survey_polar': zorunlu 'istasyon' parametresi eksik.` | İstasyon verilmedi | `ALIM <istasyon> …` yazın |
| `Katman kilitli: …` | Aktif katman kilitli | `KATMAN` ile kilidi kaldırın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [APLİKASYON](stakeout.md) — aynı işin tersi: koordinattan açı ve kenar
- [DİKAYAK](perp_offset.md) — şerit metreyle taban çizgisine göre alım
- [NOKTALAR](points.md) — nokta no, Y, X listesinden okuma
- [Komut satırı](komut-satiri.md) — `semt(S,açı,kenar)` nokta fonksiyonu, tek noktanın yazılı hâli
