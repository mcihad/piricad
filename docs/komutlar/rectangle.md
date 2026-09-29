# DİKDÖRTGEN — İki Köşeden, Derinlikle ya da Ölçüyle Dörtgen Çizme

Yapı adası, bina oturumu, yapı yaklaşma sınırı, plan paftası çerçevesi — kısacası
bir planın çoğunu oluşturan dik köşeli yüzeyleri çizen herkes için; bu sayfayı
bitirdiğinizde dikdörtgeni ve karesini iki köşeden, bir kenar ve derinlikten ya da
en, boy ve kâğıt boyundan arayüzden, komut satırından ve betikten çizmeyi bileceksiniz.

> **Faz 0 durumu.** `DİKDÖRTGEN` kapalı yüzeyi belgeye yazar. Yüzeyin **içi henüz
> boyanmaz**: tuval bugün yalnız sınırı çizer, dolgu ve tarama **Faz 1'de**
> sembol yığınıyla birlikte gelecek. Ürettiği nesne [`ALAN`](area.md) ile aynı
> türdendir; alan hesabı, ifraz ve tevhit o fazda bu nesnenin üzerine oturur.

## Ne yapar

`DİKDÖRTGEN`, **karşılıklı iki köşeden** dört köşeli kapalı bir yüzey üretir.
Kalan iki köşeyi program hesaplar. Aynı yüzeyi bir kenar ve **derinlikten** ya da bir
köşe ve **ölçüden** — en, boy ya da bir kâğıt boyu — de üretir; bkz. [Dört
yöntem](#dört-yöntem).

Aynı şeyi [`ALAN`](area.md) ile dört köşe tıklayarak da çizebilirsiniz — ama
çizemezsiniz: elle tıklanan dört köşe **neredeyse** dik olur, ve imzalanan bir
paftada "neredeyse dik" bir kusurdur. İki köşe şeklin tamamını belirlediğinde
köşelerin dikliği hesabın sonucudur, dikkatin değil.

**Karşı köşe**, sıradaki köşe değildir: çapraz köşedir. Her çizim programının
dikdörtgen sürüklemesi böyledir, ve köşegen kilidinin kare üretmesinin sebebi de
budur.

## Kare çizmek

Çizerken **Ctrl** basılı tutun: ikinci köşe, ilkinden 45°'nin katlarına kilitlenir
ve dikdörtgen **tam kare** çıkar. Bıraktığınızda kilit kalkar.

Ctrl bir fare hüneri değildir, bir **modu** basılı tutar. Aynı kilit
[`MOD`](mode.md) ile de açılır ve betikten de açılabilir:

```text
MOD köşegen=evet
DİKDÖRTGEN 485320,4310220 485360,4310180
MOD köşegen=hayır
```

Kilit açıkken kenar uzunluğu, sürüklediğiniz uzaklığın 45°'ye izdüşümüdür: köşe
imlecin yönüne değil, ona en yakın köşegene oturur. `MOD kutupsal_açı` ile
kurduğunuz açı adımını, kilit açık olduğu sürece geçersiz kılar.

Kilit yalnız dikdörtgene özel değildir; ikinci ve sonraki noktasını öncekine göre
alan her komut — [`ÇİZGİ`](line.md), [`ALAN`](area.md) — aynı kilide uyar.

## Adlar

| Ad | Tür |
|---|---|
| `DİKDÖRTGEN` | Türkçe, birincil |
| `DIKDORTGEN` | ASCII karşılık |
| `RECTANGLE` | İngilizce karşılık |
| `DKD` | Kısaltma |
| `REC` | Kısaltma |
| `core.rectangle` | Komut kimliği |
| `KUTU` | **Netcad'deki adı, komut adı değil.** Komut Ara (`Ctrl+K`) ve `YARDIM komut=KUTU` bulur; komut satırında bu komutu başlatmaz. Bir komut soru sorarken yazılan `KUTU` o sorunun yanıtıdır (bir yazı, bir katman adı), `SEÇ mod=KUTU` da bir seçim kipidir |

## Dört yöntem

| `yontem` | Ne ister | Sonuç |
|---|---|---|
| `2n` (varsayılan) | Karşılıklı iki köşe | Eksenlere paralel dikdörtgen |
| `3n` | Bir kenarın iki köşesi + karşı kenarın geçtiği nokta | **Döndürülmüş** dikdörtgen |
| `derinlik` | Bir kenarın iki köşesi + `derinlik` (m) | **Döndürülmüş** dikdörtgen; derinlik **sağ pozitif, sol negatif** |
| `olcu` | Bir köşe + `en` ve `boy` (m) ya da `kagit` | Eksenlere paralel ya da `aci` kadar döndürülmüş **kutu** |

`3n`, ızgaraya paralel olmayan her yapı için: yolu doğu–batı gitmeyen bir ada,
eğik bir sınırın üzerindeki bir duvar. Üçüncü nokta bir köşe **değildir**, yalnız
**yüksekliği** verir: kenarın kendi normaline izdüşürülür, yani eli birkaç
milimetre kayan bir kullanıcı paralelkenar değil dikdörtgen alır. Üçüncü nokta
kenarın üzerindeyse yükseklik sıfırdır ve komut reddeder.

**Kılavuz `2n` ve `3n`'de şeklin kendisidir.** `2n`'de ilk köşeden itibaren
dikdörtgen, `3n`'de kenar sabitlendikten sonra **döndürülmüş dikdörtgenin dört
köşesi** fareyi izler — köşeleri, tıklayınca onları üretecek olan fonksiyonun
kendisinden gelir (`core::edge_rectangle_corners`), yani gördüğünüz şekil
oluşacak şekildir.

### Derinlikle: iki köşe ve kaç metre

`derinlik`, bir ölçü krokisinin bina satırıdır (Netcad'in Bina Oluştur'u): cephe
kenarının iki ucu ölçülmüştür, bina o kenardan şu kadar derindir. Üçüncü bir nokta
tıklamak yerine derinliği **yazarsınız**.

**İşaret programın tek kuralıdır: sağ pozitif, sol negatif.** İlk köşeden ikinciye
bakarken sağ elinizin tarafı artıdır — [`dik(A,B,ayak,boy)`](komut-satiri.md#dik-ayak-ve-dik-boy--işaret-kuralı)
ve [`DİKAYAK`](perp_offset.md)'ın `boy`'uyla aynı kural. Doğuya giden bir kenarın sağı
güneydir: `derinlik=6` binayı kenarın güneyine, `derinlik=-6` kuzeyine kurar. Kenarı
ters yönde verirseniz sağ da yön değiştirir; işaret zeminde değil, **yürüyüş yönünde**
okunur.

Karşı kenar, kenarın kendisi kadardır milimetresine dek: iki uç aynı vektörle taşınır,
ayrı ayrı yuvarlanmaz. Derinlik sıfırsa (ya da bir milimetreden küçükse) komut reddeder.

Arayüzde iki köşe tıklanınca kenar ekranda kalır ve derinlik komut satırına yazılır;
derinlik bir sayıdır, tıklamayla cevaplanmaz.

### Ölçüyle: bir köşe, en ve boy — ya da bir kâğıt

`olcu`, ölçüsü belli bir kutuyu tek köşesinden koyar (Netcad'in Kutu'su). Kutu önce
eksenlere paralel kurulur: **`en` doğu–batı, `boy` kuzey–güney** boyutudur ve verilen
köşe kutunun **ilk köşesidir** — `aci` yokken sol alt köşe. `aci` verilirse kutu o köşe
etrafında döner.

**`aci` bir dönüştür, bir yön değil.** Oturumun açı birimi ve kuralıyla okunur
(varsayılan grad, kuzeyden saat yönünde) ve açıların arttığı yönde döndürür: `aci=0`
kutuyu eksenlere paralel bırakır — hiç yazmamakla aynıdır —, `aci=100` çeyrek tur
saat yönünde çevirir; semt kuralında boy kenarının semti `aci`'nin kendisidir.
`MOD kural matematik` ile açılar saat yönünün tersine artar ve dönüş de öyle olur.

**Kâğıt boyu.** `en` ve `boy` yerine `kagit` verilebilir: **A5, A4, A3, A2, A1, A0**.
Zeminde kapladığı yer kâğıdın ölçüsü çarpı ölçek paydasıdır — 1:1000'de A3, 420 m ×
297 m'dir; bu, yerel bir paftanın çerçevesidir. Ölçek `olcek=` ile verilir, verilmezse
çizimin kendi plan ölçeği ([`AYAR plan_ölçeği`](setting.md)) kullanılır. Kâğıt varsayılan
olarak **yataydır** (uzun kenar doğu–batı); `yon=dikey` ayağa kaldırır. Ölçüler ISO 216'dır
(A4 210 × 297 mm) ve [yazdırma profilleri](print_profile.md) ile [çıktı yerleşimi](layout.md)
aynı tabloyu okur; ikinci bir liste yoktur.

Kutu, kullanılan ölçekle **günlüğe yazılır**: plan ölçeği sonradan değişse de günlük
yeniden oynatılınca aynı kutu çıkar.

`en`/`boy` ya da `kagit` verilmemişse arayüzde önce eni, sonra boyu sorar, en son köşeyi
tıklatır. Bir kâğıtta yalnız köşeyi sorar.

### Hangi parametre hangi yöntemin

Bir yöntemin okumadığı parametre **sessizce yok sayılmaz**, reddedilir: `derinlik`
yalnız `yontem=derinlik` ile, `en`, `boy`, `kagit`, `yon`, `olcek` ve `aci` yalnız
`yontem=olcu` ile verilir. `aci=30` yazıp bir döndürme beklerken `3n` çizmesi, bir
kâğıdın ölçeğini yazıp karşılığını görmemek gibi yanlışları bu kural önler.

## Sözdizimi

```text
DİKDÖRTGEN <köşe> <karşı köşe>
DİKDÖRTGEN yontem=3n noktalar=<n> noktalar=<n> noktalar=<n>
DİKDÖRTGEN yontem=derinlik noktalar=<ilk köşe> <ikinci köşe> derinlik=<m>
DİKDÖRTGEN yontem=olcu noktalar=<köşe> en=<m> boy=<m> [aci=<açı>]
DİKDÖRTGEN yontem=olcu noktalar=<köşe> kagit=<A5…A0> [olcek=<N>] [yon=yatay|dikey] [aci=<açı>]
```

Nokta yazımı [`ÇİZGİ`](line.md) ile aynıdır: mutlak koordinat
(`485320,4310220`), göreli (`@50,30`) ve kutupsal (`@100<45`).

## Parametreler

| Parametre | Zorunlu | Tür | Anlamı |
|---|---|---|---|
| `noktalar` | evet | nokta ×1–3 | `2n`: karşılıklı iki köşe. `3n`: kenarın iki köşesi ve karşı kenarın geçtiği nokta. `derinlik`: kenarın iki köşesi. `olcu`: kutunun ilk köşesi. Kalan köşeler bunlardan türetilir |
| `yontem` | hayır | `2n`, `3n`, `derinlik`, `olcu` | Varsayılan `2n` |
| `derinlik` | `derinlik` yönteminde | sayı, metre | Kenardan karşı kenara uzaklık; ilk köşeden ikinciye bakarken **sağ pozitif, sol negatif** |
| `en` | `olcu`'da `kagit` yoksa | sayı, metre | Kutunun doğu–batı boyutu (`aci` yokken) |
| `boy` | `olcu`'da `kagit` yoksa | sayı, metre | Kutunun kuzey–güney boyutu (`aci` yokken) |
| `kagit` | hayır | `A5`, `A4`, `A3`, `A2`, `A1`, `A0` | `olcu`'da en ve boy yerine kâğıt boyu: kâğıdın ölçüsü çarpı ölçek paydası |
| `yon` | hayır | `yatay`, `dikey` | `kagit` ile: `yatay` (varsayılan) kâğıdın uzun kenarı doğu–batı |
| `olcek` | hayır | tam sayı, 1–1000000 | `kagit` ile ölçek paydası (1:N); verilmezse çizimin plan ölçeği |
| `aci` | hayır | sayı | `olcu`'da kutunun dönüşü, oturumun açı biriminde (varsayılan grad), açıların arttığı yönde; varsayılan 0 |

Köşe sayısı sabittir; delik açmak, üçten çok köşe vermek ya da bir dörtgeni köşe köşe
çizmek [`ALAN`](area.md) işidir. Üç köşeden dördüncüyü hesaplamak için
[`DÖRDÜNCÜKÖŞE`](fourth_corner.md) vardır.

## Örnekler

**Arayüzden.** Şeritte **Giriş ▸ Çizim ▸ Dikdörtgen**'e basın, bir köşeye tıklayın,
karşı köşeye tıklayın. Kare için ikinci tıklamada **Ctrl** basılı tutun.

**Döndürülmüş dikdörtgen arayüzden.** **Dikdörtgen** düğmesinin okundan **Dikdörtgen —
döndürülmüş**'ü seçin. Bir kenarın iki köşesini tıklayın; üçüncü tıklamaya kadar
dikdörtgenin tamamı fareyi izler ve yüksekliği gösterir.

**Komut satırından.** 40 m × 20 m bir yapı adası:

```text
DİKDÖRTGEN 485320,4310220 485360,4310200
```

Göreli koordinatla, aynı dikdörtgen:

```text
DİKDÖRTGEN 485320,4310220 @40,-20
```

**Kare.** Kilidi açıp 25 m'lik bir bina oturumu:

```text
MOD köşegen=evet
DİKDÖRTGEN 485320,4310220 @25,-25
MOD köşegen=hayır
```

**Derinlik ve ölçü arayüzden.** Komut satırına `DİKDÖRTGEN yontem=derinlik` ya da
`DİKDÖRTGEN yontem=olcu` yazıp **Enter**'a basın (ya da **Ctrl+K** ile bulun). `derinlik`
iki köşeyi tıklatır, kenarı ekranda tutar ve derinliği sorar; `olcu` önce eni ve boyu
sorar, sonra köşeyi tıklatır. `yontem=olcu kagit=A3` yalnız köşeyi sorar. **Esc** her
adımda hiçbir şey çizmeden çıkar.

**Derinlikle.** Doğuya giden 18 m'lik bir cephe kenarı ve 11,5 m derinliğinde bir bina.
Derinlik sağ pozitiftir, yani bina kenarın güneyine kurulur:

```
DİKDÖRTGEN yontem=derinlik noktalar=485320,4310220 485338,4310220 derinlik=11.5
```

```text
Dikdörtgen çizildi: 18,000 m × 11,500 m (derinlik sağda).
```

Aynı bina kenarın öbür yanında — negatif derinlik sol demektir, kuzeydir:

```
DİKDÖRTGEN yontem=derinlik noktalar=485320,4310220 485338,4310220 derinlik=-11.5
```

```text
Dikdörtgen çizildi: 18,000 m × 11,500 m (derinlik solda).
```

**Ölçüyle.** İlk köşesi `485320,4310220` olan 40 m × 20 m bir kutu:

```
DİKDÖRTGEN yontem=olcu noktalar=485320,4310220 en=40 boy=20
```

```text
Kutu çizildi: 40,000 m × 20,000 m.
```

Aynısı 50 grad döndürülmüş — semt kuralında saat yönünde, yani boy kenarı kuzeydoğuya
bakar:

```
DİKDÖRTGEN yontem=olcu noktalar=485320,4310220 en=40 boy=20 aci=50
```

```text
Kutu çizildi: 40,000 m × 20,000 m; 50,0000 grad döndürüldü.
```

**Kâğıt boyuyla.** Plan ölçeği 1:1000 iken — `AYAR plan_ölçeği`'nin varsayılanı — bir A3
paftanın zemindeki çerçevesi:

```
DİKDÖRTGEN yontem=olcu noktalar=485000,4310000 kagit=A3
```

```text
Kutu çizildi: 420,000 m × 297,000 m (A3 yatay, 1:1000).
```

Başka bir ölçek ve dikey kâğıt:

```
DİKDÖRTGEN yontem=olcu noktalar=485000,4310000 kagit=A4 olcek=500 yon=dikey
```

```text
Kutu çizildi: 105,000 m × 148,500 m (A4 dikey, 1:500).
```

## Geri alma

Bir `DİKDÖRTGEN` çağrısı **tek** geri alma adımıdır: [`GERİAL`](undo.md) dörtgenin
tamamını kaldırır, köşe köşe değil. Yarım kalmış bir dörtgen belgeye hiç
yazılmaz — ESC ile vazgeçerseniz hiçbir iz kalmaz.

## Betikten kullanım

JSON betiğinde iki köşe, tek listede:

```json
[
  { "cmd": "DİKDÖRTGEN", "args": { "noktalar": [[485320000, 4310220000],
                                                [485360000, 4310200000]] } }
]
```

Betikteki koordinatlar **milimetredir** (tam sayı), komut satırındaki metredir.
Kare için betiğin başına `{"cmd": "MOD", "args": {"ad": "köşegen", "deger": "evet"}}`
koyun.

Günlüğe **iki köşe** yazılır, türetilen dördü değil: günlük yeniden oynatıldığında
aynı dörtgen aynı hesapla üretilir, ve dört köşe yazılsaydı sonradan biri
oynatıldığında artık dikdörtgen olmayan bir "dikdörtgen" kalırdı.

Yeni yöntemlerde de günlük **sorulanı** tutar: `derinlik` için iki köşe ve derinlik,
`olcu` için köşe, `en` ile `boy` (ya da `kagit`, `yon` ve **o gün geçerli olan `olcek`**)
ve verildiyse `aci`. Sayılar hangi istemciden gelirse gelsin aynı biçimde yazılır.

Derinlikle dikdörtgen, bir kenar ve derinlik verilen betikte:

```json
{
  "ad": "Bina, derinlikle",
  "komutlar": [
    { "cmd": "core.rectangle",
      "args": { "yontem": "derinlik",
                "noktalar": [[485320000, 4310220000], [485338000, 4310220000]],
                "derinlik": 11.5 } }
  ]
}
```

Ölçülü kutu — bir A3 paftanın çerçevesi ve 50 grad dönük bir bina:

```json
{
  "ad": "Kutular",
  "komutlar": [
    { "cmd": "core.rectangle",
      "args": { "yontem": "olcu", "noktalar": [[485000000, 4310000000]], "kagit": "A3" } },
    { "cmd": "core.rectangle",
      "args": { "yontem": "olcu", "noktalar": [[485320000, 4310220000]],
                "en": 40, "boy": 20, "aci": 50 } }
  ]
}
```

`derinlik`, `en`, `boy` metredir; koordinatlar milimetredir. Python'da parametreler
`depth`, `width`, `length`, `paper`, `orientation`, `scale` ve `angle`'dır.

Betikte değer **sorulamaz**: yöntemin istediği köşelerden biri, `derinlik`, `en`/`boy` ya
da `kagit` verilmemişse komut cevap beklemeden iptal olur ve hiçbir şey çizmez — arayüzde
bunlar sorulur. Bu yüzden betikte yönteminizin istediğini eksiksiz yazın: `2n` ve `3n` için
ikişer ve üçer köşe, `derinlik` için iki köşe ve derinlik, `olcu` için köşe ile `en` ve
`boy` ya da `kagit`.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bu iki köşe bir alan kapatmaz: karşı köşenin hem doğusu hem kuzeyi ilkinden farklı olmalı.` | İki köşe aynı x ya da aynı y üzerinde; aralarında yüzey yok | Karşı köşeyi her iki eksende de kaydırın |
| `Bir alanın sınırı kendini kesemez.` | Geometri katmanı bozuk halkayı reddetti | Köşelerin sırasını denetleyin |
| `'PARSEL' katmanı kilitli.` | Etkin katman kilitliyse çizim yazılmaz | [`KATMAN`](layer.md) ile kilidi açın |
| `Kenarın iki köşesi aynı nokta; bir kenar tanımlamıyor.` | `derinlik`: iki köşe aynı yerde | İkinci köşeyi kaydırın |
| `Derinlik sıfır olamaz (en az 1 mm): sağa artı, sola eksi verin.` | `derinlik=0` ya da bir milimetreden küçük | Sıfırdan farklı bir derinlik verin; sağ artı, sol eksi |
| `yontem=derinlik iki köşe ister: kenarın iki ucu; 3 nokta verildi.` | `derinlik` yöntemine ikiden çok nokta yazıldı | İki köşe verin; üçüncü nokta `3n`'in işidir |
| `yontem=olcu tek nokta ister: kutunun ilk köşesi; 2 nokta verildi.` | `olcu` yöntemine birden çok nokta yazıldı | Yalnız kutunun ilk köşesini verin |
| `` `en` yalnız yontem=olcu ile verilir; yontem=derinlik onu kullanmaz. `` | Bir yöntemin okumadığı parametre yazıldı (`derinlik`, `en`, `boy`, `kagit`, `yon`, `olcek`, `aci` için aynı biçim) | Parametreyi kaldırın ya da doğru `yontem=`i yazın |
| `` `kagit` verilince en ve boy kâğıttan gelir; `kagit` ile `en` ya da `boy` birlikte verilmez. `` | Kâğıt boyuyla birlikte `en` ya da `boy` verildi | Ya kâğıdı ya da eni ve boyu verin |
| `` `yon` ve `olcek` yalnız `kagit` ile verilir: kâğıdın yönü ve ölçeği kâğıt boyunu zemine indirir. `` | `en`/`boy` ile birlikte `yon` ya da `olcek` verildi | Bunları yalnız `kagit=` ile kullanın |
| `Kutunun eni ve boyu sıfırdan büyük olmalı; en 0,000 m, boy 5,000 m geldi.` | Eksi ya da sıfır en veya boy | Pozitif ölçü verin |
| `Ölçek bilinmiyor: olcek=<N> verin (1:N) ya da projenin plan ölçeğini ayarlayın.` | Verilen ya da projedeki ölçek payda olamaz | `olcek=` verin ya da `AYAR plan_ölçeği` ile kurun |
| `'core.rectangle': 'kagit' için tanınmayan değer 'A9'. Kabul edilenler: A5 / A4 / A3 / A2 / A1 / A0` | Tabloda olmayan kâğıt (`ozel` de dahil: özel ölçü `en` ve `boy` ile verilir) | Listedeki kâğıtlardan birini yazın |
| `'core.rectangle': 'yon' için tanınmayan değer 'capraz'. Kabul edilenler: yatay / dikey` | `yon` için yatay ya da dikey dışında bir sözcük | `yon=yatay` ya da `yon=dikey` yazın |
| `'core.rectangle': 'olcek' 1 ile 1000000 arasında olmalı, 0 geldi.` | Ölçek paydası sıfır ya da milyonu aşıyor | 1 ile 1000000 arasında bir payda verin |
| `'core.rectangle': 'yontem' için tanınmayan değer 'xyz'. Kabul edilenler: 2n / 3n / derinlik / olcu` | Bilinmeyen yöntem | Dört yöntemden birini yazın |
| `Beklenen yön: yatay \| dikey. Girilen: 'capraz'` | Arayüzde başlatılan satırda `yon` için başka sözcük | `yatay` ya da `dikey` yazın |
| `Tanınmayan kâğıt: 'A9'. Kâğıtlar: A5, A4, A3, A2, A1, A0; başka bir ölçü için en= ve boy= verin.` | Arayüzde başlatılan satırda tabloda olmayan kâğıt | Listedeki kâğıtlardan birini yazın ya da `en` ve `boy` verin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili sayfalar

- [`DÖRDÜNCÜKÖŞE`](fourth_corner.md) — üç köşeden dördüncü köşe ve dik açı sapması
- [`ALAN`](area.md) — çok köşeli ve delikli yüzeyler
- [`ÇİZGİ`](line.md) — açık doğru parçaları, nokta yazımı
- [`DİKAYAK`](perp_offset.md) — dik ayak ve dik boy; derinlikle aynı işaret kuralı
- [`MOD`](mode.md) — köşegen kilidi, dik mod, kutupsal açı ve yakalama modları
- [`YAZDIR`](print.md) ve [`ÇIKTIYERLEŞİMİ`](layout.md) — kâğıt boyu tablosunu paylaşan komutlar
- [`AYAR`](setting.md) — `plan_ölçeği`
