# ALAN — Kapalı Alan Çizme

Parsel, ada, yapı adası, plan lekesi ya da herhangi bir kapalı yüzey çizen herkes
için; bu sayfayı bitirdiğinizde kapalı bir alanı, içine delik açarak da,
arayüzden, komut satırından ve betikten çizmeyi bileceksiniz.

> **Faz 0 durumu.** `ALAN` kapalı yüzeyi belgeye yazar ve delikli alanı taşır.
> Yüzeyin **içi henüz boyanmaz**: tuval bugün yalnız sınırı çizer, çünkü dolgu ve
> tarama sembol yığını ile birlikte **Faz 1'de** gelecek. Alan hesabı, ifraz ve
> tevhit de o fazda bu nesne üzerine oturacak.

## Ne yapar

`ALAN`, verdiğiniz köşe noktalarından **kapalı bir yüzey** oluşturur. Çizgiden farkı
şudur: bir çizgi iki nokta arasındadır ve bir şeyi çevrelemez; bir alan çevreler, ve
çevrelediği şeyin bir yüzölçümü vardır. Bir parselin yaptığı tek şey budur.

Kapanış noktasını **siz tekrarlamazsınız**. Dört köşe verirseniz dört köşeli bir alan
olur; son köşeden ilkine dönen kenarı program ekler. Bu kasıtlıdır: tekrarlanan
kapanış noktası dosyada iki kez saklanır, iki kez sayılır ve çevre hesabını bozar.

Bir alanın içine **delik** açabilirsiniz — bir avlu, bir ada içi park, bir istimlak
boşluğu. Delik ayrı bir nesne değildir: dış sınırla birlikte **tek nesnedir**, birlikte
seçilir, birlikte taşınır, birlikte silinir.

## Adlar

| Ad | Tür |
|---|---|
| `ALAN` | Türkçe, birincil |
| `POLİGON` | Türkçe eşanlamlı |
| `POLIGON` | ASCII karşılık |
| `AREA` | İngilizce karşılık |
| `AL` | Kısaltma |
| `core.area` | Komut kimliği |

## Sözdizimi

```text
ALAN
ALAN <n1> <n2> <n3> …
ALAN noktalar=<n1> noktalar=<n2> …
ALAN <n1> <n2> … bolum=<uzunluk> bolum=<uzunluk>
```

Nokta yazımı [`ÇİZGİ`](line.md) ile aynıdır: mutlak koordinat (`485320,4310220`),
göreli (`@50,30`) ve kutupsal (`@100<45`).

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Alanın köşeleri. En az üç nokta. Kapanış noktası tekrarlanmaz |
| `bolum` | Halka uzunlukları. Verilmezse bütün noktalar tek bir dış sınırdır |

`bolum` verildiğinde nokta listesi sırayla bölünür: **ilk halka dış sınır**, sonraki her
halka onun içinde bir **deliktir**. Uzunlukların toplamı nokta sayısına eşit olmalıdır ve
hiçbir halka üçten kısa olamaz.

Örnek: sekiz nokta ve `bolum=4 bolum=4` demek, ilk dört noktanın dış sınır, son dört
noktanın onun içindeki delik olması demektir.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Dört köşeli bir parsel — kapanış noktası yazılmaz:

```
ALAN 485300,4310200 485360,4310200 485360,4310245 485300,4310245
```

Aynı parsel, göreli koordinatlarla:

```
ALAN 485300,4310200 @60,0 @0,45 @-60,0
```

Üçgen bir artık parça:

```
ALAN 485300,4310200 @40,0 @0,30
```

Avlulu bir yapı adası — dış sınır dört köşe, avlu dört köşe:

```
ALAN 485370,4310200 485430,4310200 485430,4310245 485370,4310245 485385,4310212 485415,4310212 485415,4310232 485385,4310232 bolum=4 bolum=4
```

### Arayüz

Sol paletteki **alan** aracına basın ya da komut satırına `ALAN` yazın; ikisi aynı
komutu gönderir. Köşeleri sırayla tıklayın ve **Esc** ile alanı kapatın. Yakalama
açıkken köşeler mevcut nesnelere oturur — komşu parselin köşesine tam oturmak için
[`MOD`](mode.md) ile uç nokta yakalamasını açık tutun.

Araç **kalıcıdır**: bir alanı bitirdiğinizde `ALAN` yeniden kurulur ve bir sonrakini
çizmeye devam edebilirsiniz — her parsel için palete uzanmanız gerekmez. Aracı
bırakmak için bir kez daha **Esc**'e basın. Yani bir pafta çizerken parmağınız
Esc'te kalır: bir Esc alanı kapatır, iki Esc aracı bırakır.

İkinci köşeden itibaren, o ana kadar verdiğiniz **bütün köşeler** kesikli bir kılavuzla
birbirine bağlanır; kılavuz son köşeden imlecinize, imleçten de ilk köşeye döner. Yani
tıklamayı bitirmeden önce kapanacak alanı bütün olarak görürsünüz. Kılavuz bir ön
izlemedir: alan belgeye ancak **Enter** ile yazılır, çünkü iki köşeli bir halka alan
değildir.

Arayüzün ayrıcalığı yoktur: fareyle çizdiğiniz alan ile komut satırına yazdığınız alan
aynı komuttur ve komut günlüğüne aynı satır olarak düşer.

### Betik

```json
{
  "ad": "İki parsel ve bir avlu",
  "crs": "TUREF/TM30",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
    { "cmd": "core.area",  "args": { "noktalar": [
        [485300000, 4310200000], [485360000, 4310200000],
        [485360000, 4310245000], [485300000, 4310245000] ] } },

    { "cmd": "core.layer", "args": { "ad": "AVLULU" } },
    { "cmd": "core.area",  "args": {
        "noktalar": [
          [485370000, 4310200000], [485430000, 4310200000],
          [485430000, 4310245000], [485370000, 4310245000],
          [485385000, 4310212000], [485415000, 4310212000],
          [485415000, 4310232000], [485385000, 4310232000] ],
        "bolum": [4, 4] } }
  ]
}
```

Betikte koordinatlar **milimetre tam sayısıdır**; komut satırındaki `485300,4310200`
metredir. İkisi aynı noktayı gösterir, sadece birim bildirimi farklıdır.

## Geri alma

`ALAN` **tek bir geri alma adımıdır**. Delikli bir alan çizdiyseniz `GERİAL` dış sınırı
ve deliği birlikte kaldırır; delik ayrı bir nesne olmadığı için ayrı geri alınamaz.

```
GERİAL
```

Çizim sırasında **Esc**'e basarsanız hiçbir şey oluşmaz ve geri alma yığınına da hiçbir
şey girmez — yarım kalmış bir alan diye bir şey yoktur.

## Betikten kullanım

`ALAN` betiklenebilir ve AI erişimine açıktır. Bir toplu içe aktarma betiğinde parselleri
oluşturmak için kullanılır; her `core.area` çağrısı kendi geri alma adımıdır, dolayısıyla
yüz parsel yükleyen bir betik yüz adım bırakır. Tamamını tek adım yapmak istiyorsanız
betiği [`BETİK`](script.md) ile çalıştırın.

AI bu komutu kullanabilir ama **koordinatları kendi üretemez**: her `Mm` değeri kayıtlı
bir araç çağrısı sonucuna kadar izlenebilir olmak zorundadır ve öneri onaylanmadan
uygulanmaz (`CLAUDE.md` 5.7, 5.8).

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bir alan en az üç köşe ister; 2 nokta verildi.` | İki nokta verilmiş | Üçüncü köşeyi ekleyin; iki nokta bir alan değil bir çizgidir, [`ÇİZGİ`](line.md) kullanın |
| `'bolum' değerleri nokta listesiyle uyuşmuyor: 8 nokta verildi, halka uzunlukları toplamı bunu aşıyor ya da üçten kısa bir halka var.` | `bolum` toplamı nokta sayısını aşıyor ya da bir halka üçten kısa | Uzunlukları sayın: toplamları nokta sayısına eşit, her biri en az 3 olmalı |
| `'bolum' değerleri nokta listesini tam kapatmıyor: 8 noktanın 4 tanesi halkalara dağıtıldı.` | `bolum` toplamı nokta sayısından az | Artan noktalar için bir halka uzunluğu daha ekleyin |
| `'core.area' parametresi 'noktalar' en az 3 değer bekliyor.` | Argümansız ya da eksik çağrı | En az üç köşe verin |
| `'bolum' parametresi tam sayı bekliyor, başka türde bir değer geldi.` | `bolum` metin ya da ondalık verilmiş | Tam sayı yazın: `bolum=4` |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
