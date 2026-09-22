# DAİRE — Daire Çizme

Kuyu ağzı, direk, ağaç, koruma alanı ya da bir röper noktasının etki yarıçapını
çizen herkes için; bu sayfayı bitirdiğinizde daireyi arayüzden, komut satırından ve
betikten çizmeyi bileceksiniz.

## Ne yapar

`DAİRE`, bir **merkez** ve **çember üzerinde bir nokta** alır ve daire çizer.
Yarıçapı iki nokta arasındaki uzaklık belirler.

> **Faz 0 durumu.** Daire, KentOSCad'in ilk **eğri** nesnesidir. Bugün çizilir,
> seçilir, kaydedilir ve geri alınır. Yakalama henüz dairenin **merkezine ve
> çemberine ayrıca oturmaz** — genel yakalama kuralları geçerlidir; merkez ve dörtte
> bir noktaları yakalaması Faz 1'de gelecek. Yay ve daire dilimi de o fazda bu
> nesnenin üstüne oturacak.

### Daire neden çokgen değil

KentOSCad daireyi **tanımıyla** saklar: merkez ve yarıçap. Ekranda çizilen 128 kenarlı
çokgen yalnız **resimdir**; belgede duran sayı değildir.

Bunun sebebi hukukidir. Daire 128 kenarlı bir çokgen olarak saklansaydı çevresi
2·π·r **olmazdı**, alanı π·r² **olmazdı**. 128 kenarlı bir çokgenin alanı gerçek
daireden binde 0,6 küçüktür; 300 metre yarıçaplı bir koruma alanında bu **170 m²**
eksik demektir. Tapuya giden sayı budur (`kentoscad.md` §12).

Dolayısıyla:

- **Alan** her zaman π·r²'dir, çizilen çokgenin alanı değil.
- **Yarıçap** tam sayıdır; karekök alınarak geri hesaplanmaz.
- Çizilen çokgenin kaç kenarlı olduğu yalnız görüntüyü etkiler.

Bu ayrım nesnenin **türünde** durur. Bir daire ile iki noktalı bir çizgi belgede
aynı iki tepe noktasını taşır; hangisinin ne olduğunu nesne türü söyler. Dosyaya da
tür yazılır, yoksa kaydedilen her daire açılışta doğuya bakan kısa bir çizgiye
dönerdi.

### Dairenin köşesi yoktur

`DAİRE` bir eğridir, köşeli bir şekil değildir. Bu yüzden
[`KÖŞETAŞI`](vertex_move.md), [`KÖŞEEKLE`](vertex_insert.md) ve
[`ALANAÇEVİR`](to_area.md) daireyi **reddeder** — daireyi köşeliymiş gibi düzenlemek,
söylemeden merkezini kaydırmak ya da yarıçapını değiştirmek olurdu.

Yarıçapı değiştirmek için bugün daireyi silip yeniden çizersiniz.

## Adlar

| Ad | Tür |
|---|---|
| `DAİRE` | Türkçe, birincil |
| `DAIRE` | ASCII karşılık |
| `CIRCLE` | İngilizce karşılık |
| `DR` | Kısaltma |
| `core.circle_draw` | Komut kimliği |

## Dört yöntem

Bir daire bir çizime dört şekilde gelir ve `yontem` hangisi olduğunu söyler:

| `yontem` | Ne ister | Nerede kullanılır |
|---|---|---|
| `merkez` (varsayılan) | Merkez + çember üzerinde bir nokta | Günlük olan; çekme mesafesi, röper dairesi |
| `2n` | Çapın iki ucu | Bir kapı boşluğu, bir boru, bir sondaj |
| `3n` | Çember üzerinde üç nokta | Ölçülmüş bir kavisin geri kurulması: bordür, ada dönüşü, duvar kavsi |
| `ttr` | İki doğru + yarıçap + gösterilen köşe | Yol kavşağının pahı |

`3n` üç noktası aynı doğru üzerindeyse **reddeder**: bir doğrunun üzerindeki üç
noktanın çevrel çemberi sonsuz yarıçaplıdır ve int64'e sığan en büyüğünü vermek
yanlış bir cevabı cevap kılığına sokmak olur.

`ttr`'nin **dört** çözümü vardır — iki doğrunun yaptığı her çeyrekte bir tane — ve
hangisinin istendiği sayıların içinde yoktur. Kullanıcı köşeyi gösterir ve dörtten
en yakını alınır; sessiz bir seçim pahı kavşağın yanlış köşesine koyardı.

`3n`'in hesabı `YAY yontem=3n` ile paylaşılır (`core::circumcircle`), yani bir
daire ile ondan kesilen yay birbirine uyar.

### Kılavuz: her yöntemde çemberin kendisi

Dört yöntemin de kılavuzu, tıklayınca oluşacak **çemberi** gösterir — çizgisini
değil. `2n` ve `3n` eskiden lastik bir çizgi gösteriyordu, `ttr` ise hiçbir şey
göstermiyordu.

| `yontem` | Kılavuz neyi gösterir |
|---|---|
| `merkez` | Merkezden imlece yarıçap, ve o yarıçaptaki çember |
| `2n` | Çapın ilk ucu sabit; çember imleçle büyür |
| `3n` | İlk iki nokta sabitlendikten sonra üçüncüsünün çevrel çemberi |
| `ttr` | İki teğet doğru ve imlecin bulunduğu çeyrekteki pah çemberi |

`ttr`'de bu en çok gerekli olan yerdir: dört çözümden hangisini aldığınızı
tıklamadan **önce** görürsünüz. Kılavuz, çemberi kuracak olan fonksiyonun
kendisinden gelir (`core::circle_from_guide`), yani gördüğünüz çember oluşacak
çemberdir.

## Sözdizimi

```text
DAİRE <merkez> <cevre>
DAİRE yontem=2n birinci=<nokta> ikinci=<nokta>
DAİRE yontem=3n birinci=<nokta> ikinci=<nokta> ucuncu=<nokta>
DAİRE yontem=ttr birinci=<n> ikinci=<n> ucuncu=<n> dorduncu=<n> yaricap=<m> yon=<nokta>
```

Nokta yazımı [`ÇİZGİ`](line.md) ile aynıdır: mutlak koordinat (`485320,4310220`),
göreli (`@50,30`) ve kutupsal (`@100<45`).

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `merkez` | Dairenin merkezi |
| `cevre` | Çember üzerinde bir nokta. Merkezle arasındaki uzaklık yarıçaptır |

Yarıçap doğrudan yazılmaz, **çember noktasıyla** verilir. Bunun sebebi pratiktir:
fareyle daire böyle çizilir, ve bütün yakalama kuralları bu noktaya da uygulanır —
çemberi bir parsel köşesine yakalarsanız daire tam o köşeden geçer.

Sabit bir yarıçap istiyorsanız göreli koordinat kullanın: `cevre=@25,0` merkezden
25 metre doğuya gider, yani yarıçap tam 25 metredir.

## Örnekler

### Komut satırı

Merkezi ve çember noktasını yazarak:

```text
DAİRE merkez=485300,4310200 cevre=485325,4310200
```

Yarıçapı tam 25 metre olan daire, göreli koordinatla:

```text
DAİRE merkez=485300,4310200 cevre=@25,0
```

Bir röper noktasının 50 metrelik etki alanı:

```text
DAİRE 485300,4310200 @50,0
```

### Arayüz

Sol paletteki **daire** aracına basın ya da komut satırına `DAİRE` yazın; ikisi aynı
komutu gönderir. Önce merkeze, sonra çember üzerinde bir yere tıklayın. İki tıklama
arasında merkezden imlecinize kesikli bir kılavuz uzanır: göreceğiniz uzunluk
yarıçaptır.

Öteki üç yöntem aynı düğmenin **kartında**: düğmeyi basılı tutun ya da köşesindeki
işarete tıklayın. Kartta **Daire — çapın iki ucu**, **Daire — üç nokta** ve
**Daire — iki doğruya teğet** vardır; aynı karttaki **Elips** ve **Halka** ayrı
şekillerdir ve artık kendi ikonlarını taşırlar.

Yakalama açıkken hem merkez hem çember noktası mevcut nesnelere oturur —
[`MOD`](mode.md) ile hangi yakalamaların açık olduğunu ayarlayabilirsiniz.

Araç kalıcıdır: bir daireyi bitirdiğinizde `DAİRE` yeniden kurulur ve bir sonrakini
çizebilirsiniz. Aracı bırakmak için **Esc**'e basın.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.circle_draw",
      "args": { "merkez": [485300000, 4310200000], "cevre": [485325000, 4310200000] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte
ham depolama birimi kullanılır (`485300000` = 485 300 m). Ayrıntısı
[Betik yazma](../betik/README.md) sayfasındadır.

## Geri alma

`DAİRE` tek bir geri alma adımıdır. [`GERİAL`](undo.md) daireyi kaldırır,
[`YİNELE`](redo.md) geri getirir.

## Betikten kullanım

Betikten çağrıldığında `merkez` ve `cevre` verilmelidir; komut hiçbir şey sormaz.

Komut günlüğüne **iki nokta** yazılır, yarıçap değil. Bu kasıtlıdır: yarıçap iki
noktanın *anlamıdır*, kaydedilen ise çalıştırmanın kendisidir — türetilmiş bir sayı
kaydedilseydi günlüğü tekrar çalıştırmak özgün çizimden farklı bir daire üretebilirdi.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Çember noktası merkezle aynı yerde; yarıçap sıfır olamaz.` | İki nokta çakışık | Çember noktasını merkezden uzağa verin |
| `Daire yarıçapı sıfırdan büyük olmalı: N` | Yarıçap sıfır ya da negatif geldi | Geçerli bir çember noktası verin |
| `'<katman>' katmanı kilitli.` | Etkin katman kilitli | [`KATMAN`](layer.md) ile kilidi açın |
| `Nesne N bir daire; dairenin köşesi yoktur. ...` | Daireye [`KÖŞETAŞI`](vertex_move.md) uygulandı | Daireyi silip yeniden çizin |
| `Nesne N bir daire; daire zaten kapalı bir şekildir ve çizgi gibi birleştirilemez.` | Daireye [`ALANAÇEVİR`](to_area.md) uygulandı | Daireyi seçimden çıkarın |

## İlgili

- [`ÇİZGİ`](line.md) · [`ALAN`](area.md) · [`DİKDÖRTGEN`](rectangle.md) — diğer çizim komutları
- [`MOD`](mode.md) — yakalama modları
- [`SİL`](erase.md) · [`GERİAL`](undo.md)
