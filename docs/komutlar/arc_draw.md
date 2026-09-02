# YAY — Yay Çizme

Yol kurbunu, kavşak dönüşünü, dere kıvrımını ya da bir kadastro sınırının yay
şeklindeki kenarını çizen herkes için; bu sayfayı bitirdiğinizde yayı arayüzden,
komut satırından ve betikten çizmeyi bileceksiniz.

## Ne yapar

`YAY`, bir **merkez**, bir **başlangıç noktası** ve bir **bitiş noktası** alır ve
yay çizer. Yarıçapı merkez ile başlangıç noktası arasındaki uzaklık belirler.

> **Faz 0 durumu.** Yay çizilir, seçilir, kaydedilir ve geri alınır. Yakalama
> henüz yayın merkezine, uçlarına ve orta noktasına **ayrıca oturmaz**; genel
> yakalama kuralları geçerlidir. Yayın uzunluğu ve kiriş hesabı Faz 1'de gelecek.

### Süpürme her zaman saat yönünün tersine

Yay, başlangıç noktasından bitiş noktasına **saat yönünün tersine** süpürür. Bu
tek kural, yön denetiminin tamamıdır:

- Küçük yayı istiyorsanız uçları o sırayla verin.
- **Büyük yayı** istiyorsanız aynı iki ucu **ters sırayla** verin.

"Büyük yay" diye bir seçenek yoktur ve olmamalıdır: olsaydı aynı resmi anlatan iki
farklı kayıt olurdu, ve iki kayıt aynı şeyi anlatınca hangisinin doğru olduğu
sorusu ortaya çıkar. Aynı iki uç, ters sırayla, zaten dairenin öteki yayıdır.

Bitiş noktasının yarıçap üzerinde olması gerekmez; yalnız **yönü** okunur. Yay her
zaman başlangıç noktasının belirlediği yarıçapta çizilir.

### Yay da tanımıyla saklanır

[`DAİRE`](circle_draw.md) gibi yay da **tanımıyla** durur: merkez, tam yarıçap ve
ölçülen iki uç. Ekranda görünen çok kenarlı çizgi yalnız resimdir.

Yay hiçbir şeyi çevrelemez, dolayısıyla **alanı sıfırdır** — açık bir çizginin
alanının sıfır olmasıyla aynı sebeple. Kapalı bir yüzey istiyorsanız
[`ALAN`](area.md) ya da [`DAİRE`](circle_draw.md) kullanın.

Yayın **köşesi yoktur**: [`KÖŞETAŞI`](vertex_move.md), [`KÖŞEEKLE`](vertex_insert.md)
ve [`ALANAÇEVİR`](to_area.md) yayı reddeder.

## Adlar

| Ad | Tür |
|---|---|
| `YAY` | Türkçe, birincil |
| `ARC` | İngilizce karşılık |
| `YY` | Kısaltma |
| `core.arc_draw` | Komut kimliği |

## Sözdizimi

```text
YAY
YAY <merkez> <baslangic> <bitis>
YAY merkez=<n> baslangic=<n> bitis=<n>
```

Nokta yazımı [`ÇİZGİ`](line.md) ile aynıdır: mutlak koordinat (`485320,4310220`),
göreli (`@50,30`) ve kutupsal (`@100<45`).

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `merkez` | Yayın merkezi |
| `baslangic` | Yayın başladığı nokta. Merkezle arasındaki uzaklık yarıçaptır |
| `bitis` | Yayın biteceği yön. Yalnız yönü okunur, uzaklığı değil |

## Örnekler

### Komut satırı

Doğudan kuzeye çeyrek yay — yarıçap 30 metre:

```text
YAY merkez=485300,4310200 baslangic=485330,4310200 bitis=485300,4310230
```

Aynı iki uç ters sırayla: dairenin öteki yayı, yani 270 derecelik olan:

```text
YAY merkez=485300,4310200 baslangic=485300,4310230 bitis=485330,4310200
```

Kutupsal koordinatla, yarıçapı tam 50 metre olan 90 derecelik kurp:

```text
YAY 485300,4310200 @50<0 @50<90
```

### Arayüz

Sol paletteki **yay** aracına basın ya da komut satırına `YAY` yazın. Üç tıklama:
merkez, başlangıç, bitiş.

İkinci tıklamaya kadar kılavuz **bütün çemberi** gösterir — çünkü o anda
seçtiğiniz şey yarıçaptır, ve bir yarıçap bir çemberdir. Başlangıç noktasını
tıkladıktan sonra kılavuz **yayın kendisine** döner ve imleciniz döndükçe süpürme
büyür: bırakacağınız yay tam olarak gördüğünüz yaydır.

Yakalama açıkken üç nokta da mevcut nesnelere oturur ([`MOD`](mode.md)).

Araç kalıcıdır: bir yayı bitirdiğinizde `YAY` yeniden kurulur. Aracı bırakmak için
**Esc**'e basın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.arc_draw",
      "args": { "merkez": [485300000, 4310200000],
                "baslangic": [485330000, 4310200000],
                "bitis": [485300000, 4310230000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`YAY` tek bir geri alma adımıdır. [`GERİAL`](undo.md) yayı kaldırır,
[`YİNELE`](redo.md) geri getirir.

## Betikten kullanım

Betikten çağrıldığında `merkez`, `baslangic` ve `bitis` üçü de verilmelidir; komut
hiçbir şey sormaz.

Komut günlüğüne **üç nokta** yazılır, yarıçap ve açı değil: yarıçap merkez ile
başlangıcın *anlamıdır*, kaydedilen ise çalıştırmanın kendisidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Başlangıç noktası merkezle aynı yerde; yarıçap sıfır olamaz.` | Merkez ile başlangıç çakışık | Başlangıcı merkezden uzağa verin |
| `Bitiş noktası merkezle aynı yerde; yayın nereye kadar gideceği belirsiz.` | Bitiş merkezle çakışık; yön okunamıyor | Bitişi merkezden uzağa verin |
| `Yay yarıçapı sıfırdan büyük olmalı: N` | Yarıçap sıfır ya da negatif geldi | Geçerli bir başlangıç noktası verin |
| `'<katman>' katmanı kilitli.` | Etkin katman kilitli | [`KATMAN`](layer.md) ile kilidi açın |
| `Nesne N bir eğri; eğrinin köşesi yoktur. ...` | Yaya [`KÖŞETAŞI`](vertex_move.md) uygulandı | Yayı silip yeniden çizin |
| `Nesne N bir eğri; eğri çizgi gibi birleştirilemez.` | Yaya [`ALANAÇEVİR`](to_area.md) uygulandı | Yayı seçimden çıkarın |

## İlgili

- [`DAİRE`](circle_draw.md) — tam daire çizer
- [`ÇİZGİ`](line.md) · [`ALAN`](area.md) · [`DİKDÖRTGEN`](rectangle.md)
- [`MOD`](mode.md) — yakalama modları
- [`SİL`](erase.md) · [`GERİAL`](undo.md)
