# Komut Satırı

Klavyeden hızlı çalışmak isteyen kullanıcı için; bu sayfayı bitirdiğinizde koordinatı
dört ayrı biçimde girebilecek, satır içi hesap yapabilecek ve hata mesajlarını
çözebileceksiniz.

Komut satırı harita alanının hemen altındadır. **Bu sürümde varsayılan olarak
gizlidir**; **Ctrl+9** ile veya **Görünüm > Paneller > Komut Satırı** ile açılır, aynı
kısayolla kapanır. Açtığınızda odak doğrudan oraya gelir.

## Komut çağırmak

Komut adını yazıp **Enter**'a basın:

```
YARDIM
```

Adı yazarken satır içi tamamlama önerir. Türkçe adı, İngilizce karşılığını, kısaltmasını
veya komut kimliğini yazabilirsiniz — hepsi aynı komuta gider:

```text
ÇİZGİ        CIZGI        LINE        Ç        L        core.line
```

Büyük/küçük harf farkı yoktur; dönüşüm Türkçe kurallarına göre yapılır, yani `çizgi`
doğru şekilde `ÇİZGİ` olur.

### Bir komut sözcük sorduğunda

Bir komut bir **sözcük** sorduğunda — bir katman adı, bir renk, bir işlem — soru
satırda yazılı durur ve komutun bildiği cevaplar satırın üstünde bir **liste** olarak
açılır. Birine **tıklamak** cevaptır; yazmaya başlamak listeyi daraltır, ok tuşları
listede gezer ve Enter seçer. Renk adlarının yanında renk örneği vardır.

Böyle bir soruda tuvale tıklamak bir cevap değildir: komut soruyu açık tutar ve
cevabın yazılması ya da listeden seçilmesi gerektiğini söyler.

## Koordinat girmek

Beş biçim vardır. Hepsi hem komut argümanı olarak hem de bir komut nokta beklerken
kullanılabilir.

### Mutlak koordinat

```
485320.150,4310220.400
```

Metre cinsinden, dokümanın koordinat sisteminde. Ondalık ayırıcı **noktadır**, virgül X
ile Y'yi ayırır. Boşluk kullanmayın.

### Göreli koordinat

```
@50,30
```

Bir önceki noktadan 50 metre doğu, 30 metre kuzey. Negatif değer ters yöne gider:
`@-25,0` yirmi beş metre batı.

### Kutupsal koordinat

```
@100<45
```

Bir önceki noktadan **45 grad** yönünde 100 metre. Açı, arazide aletin okuduğu gibi
**semt açısıdır**: kuzeyden başlar, saat yönünde artar ve varsayılan birimi **grad**'dır
(tam daire 400): `0` kuzey, `100` doğu, `200` güney, `300` batı. `@100<45` bu yüzden
kuzeydoğuya, kuzeye daha yakın bir doğrultuya (40,5°) gider.

Birimi tek bir koordinat için açıya **sonek** ekleyerek söylersiniz. Sonek büyük ya da
küçük harf olabilir ve parantezli bir ifadeden sonra da yazılır (`@100<(40+5)g`):

| Sonek | Birim | Örnek | Anlamı |
|---|---|---|---|
| `g` | grad | `@100<45g` | 45 grad |
| `d` | derece | `@100<45d` | 45 derece |
| `r` | radyan | `@100<0.7r` | 0,7 radyan |

Soneksiz bir açı `açı_birimi` proje ayarıyla okunur (`AYAR açı_birimi derece`). Açının
nereden başlayıp hangi yöne arttığını ise `açı_kuralı` oturum modu söyler; kısa adı
`kural`. **Matematik kuralına** — açı doğudan başlar, saat yönünün tersine artar — şöyle
geçilir:

```
MOD kural matematik
```

Bu kuralda `@100<0` doğuya, `@100<100` (grad) kuzeye gider. Önceki sürümlerin anlamı —
derece, doğudan saat yönünün tersine — şu iki satırla geri gelir:

```
MOD kural matematik
AYAR açı_birimi derece
```

Semt kuralına ve grada dönmek için modu ve ayarı varsayılanına alın:

```
MOD kural varsayilan
AYAR açı_birimi varsayilan
```

Kural ve birim yalnız **yazdığınız metni** etkiler — komut satırını, betik dizesini ve
yapay zekâ önerisini. Komut günlüğü çözülmüş koordinatı (milimetre) tutar; bu yüzden eski
bir günlük ya da betik hangi ayarla oynatılırsa oynatılsın aynı çizimi verir. `ÖLÇ`,
`APLİKASYON` ve sürüklerken kılavuz üzerinde okunan açı da aynı iki ayarla yazılır.
Ayrıntı: [Oturum modları](mode.md).

### Satır içi ifade

Koordinatın herhangi bir bileşeni parantezli bir hesap olabilir:

```
@(100*3),0
@(45.5+12.25),(80/2)
```

Böylece hesap makinesi açmadan koordinat üretirsiniz.

### Nokta fonksiyonu

```
orta(485320,4310220,485370,4310250)
```

Koordinatı **hesaplatmak** yerine **tarif etmek**: iki noktanın ortası, bir doğruya
indirilen dik, iki doğrultunun kesişimi. Kâğıt üzerinde yaptığınız inşa, koordinatın
yazıldığı her yere yazılır. Ayrıntı ve bütün liste: [Nokta fonksiyonları](#nokta-fonksiyonları).

### Hepsi bir arada

```
ÇİZGİ 485320.150,4310220.400 @50,30 @100<45 @(100*3),0 @80<90d orta(son,@50,0)
```

## Nokta fonksiyonları

Bir nokta yerine **onu nasıl bulduğunuzu** yazarsınız. Fonksiyon, komut çalışmadan
önce tek bir koordinata çözülür: komut çözülmüş noktayı görür, komut günlüğü de onu
tutar. Bu yüzden nokta fonksiyonu komut satırında, betikte ve çalışan bir komutun
istemine yazdığınız yanıtta aynı şeydir — üçü de aynı gramerden geçer.

```
ÇİZGİ orta(0,0,100,0) dik(0,0,100,0,30,-5)
```

### Bütün fonksiyonlar

| Yazım | Ne verir |
|---|---|
| `son` | Bir önceki nokta. Yalnız argüman içinde; tek başına `son()` ya da `@0,0` yazın |
| `n(1284)` | Çizimdeki 1284 numaralı ölçü noktası |
| `orta(A,B)` | A ile B'nin tam ortası |
| `ile(P,@dx,dy)` · `ile(P,@d<a)` | P'den ölçülen göreli nokta |
| `dik(A,B,ayak,boy)` | AB doğrultusunda A'dan `ayak` metre, oradan `boy` metre dik |
| `semt(S,açı,kenar)` | S istasyonundan `açı` semtinde `kenar` metre |
| `kes(A,açı1,B,açı2)` | A'dan ve B'den çıkan iki **doğrultunun** kesişimi |
| `kes(A,r1,B,r2,yön)` | A'ya `r1`, B'ye `r2` metre olan nokta — iki çözüm, `yön` seçer |
| `kes(A,B,C,D)` | AB **doğrusu** ile CD doğrusunun kesişimi |
| `ara(A,B,oran)` · `ara(A,B,mesafe m)` | AB üzerinde oranla ya da metreyle |
| `uzanti(A,B,mesafe)` | AB doğrultusunda B'den `mesafe` metre öte |
| `xy(P,Q)` | P'nin sağa değeri, Q'nun yukarı değeri |

Adlar büyük/küçük harf ve noktalı/noktasız i farkı gözetmez: `ORTA`, `orta`,
`uzantı` ve `uzanti` aynı fonksiyondur.

### Argüman yazmanın kuralı

Argümanlar virgülle ayrılır — ve **koordinat da virgülle yazılır**. Bu yüzden bir
nokta argümanı mutlak yazıldığında **iki** argüman yeri harcar:

```
orta(0,0,100,0)           iki nokta: (0,0) ve (100,0)
dik(0,0,100,0,30,-5)      iki nokta, sonra iki sayı: ayak 30, boy −5
```

Bir nokta argümanı tek yer harcayan biçimlerde de yazılabilir — kutupsal, `son`, ya da
başka bir fonksiyon:

```
orta(son,@100<50)
dik(n(1284),n(1285),12.5,3)
orta(orta(0,0,100,0),orta(0,100,100,100))
```

Bir argüman **sayı** ise satır içi ifade olabilir (`(40+5)`), **açı** ise `g`, `d`, `r`
sonekini alır (`semt(0,0,45g,100)`) ve soneksizse `açı_birimi` ile `açı_kuralı`
ayarlarından okunur — kutupsal koordinatla tıpatıp aynı kural.

`ile`'nin ikinci argümanı `@` ile başlamak zorundadır: ölçüm P'den yapılır ve mutlak
bir çift verilseydi P sessizce boşa giderdi.

### Dik ayak ve dik boy — işaret kuralı

`dik(A,B,ayak,boy)`, A'dan B'ye **yürürken** düşünülür: `ayak` bu yönde kaç metre
gidildiği, `boy` oradan kaç metre yana çıkıldığıdır. **Sol pozitif, sağ negatiftir**
(Netcad'deki kuralın aynısı).

```
ÇİZGİ dik(0,0,100,0,30,5) dik(0,0,100,0,30,-5)
```

Taban `0,0` → `100,0`, yani doğu. Doğuya yürürken sol el kuzeyi gösterir, bu yüzden
ilk nokta `(30, 5)`, ikincisi `(30, −5)` olur.

Bir ölçü krokisindeki cephe alımı tek satırdır — taban bir kez yazılır, cepheye ait
ayak/boy çiftleri sırayla gelir:

```
ÇOKLUÇİZGİ dik(0,0,40,0,0,0) dik(0,0,40,0,12.4,3.1) dik(0,0,40,0,27.8,3.1) dik(0,0,40,0,40,0)
```

Gerçek bir krokide taban iki ölçü noktasıdır; `0,0` ve `40,0` yerine `n(1)` ve `n(2)`
yazarsınız. Aynı işin fareyle yapılan hâli P1b'de `DİKAYAK` komutu olarak gelecek.

### İki mesafe kesişimi — iki çözüm vardır

İki bilinen noktadan şeritle ölçülmüş iki mesafe **iki** noktada kesişir: doğrunun
solundaki ve sağındaki. Program kendiliğinden birini seçmez; hangisi olduğunu
söylersiniz:

```
NOKTA kes(0,0,60,100,0,80,sol) kes(0,0,60,100,0,80,sağ) kes(0,0,60,100,0,80,yon=40,40)
```

Sırasıyla `(36, 48)`, `(36, −48)` ve yine `(36, 48)` — sonuncusunda çözümü yönle değil,
aradığınıza yakın bir noktayla seçtiniz.

`sol` ve `sağ`, A'dan B'ye bakarken hangi el tarafı olduğunu söyler. Yakın bir nokta
verecekseniz **`yon=` ile yazmak zorundasınız**: çıplak bir koordinat orada
`kes(A,B,C,D)` okumasından ayırt edilemez ve program iki okumadan birini sessizce
seçmez. Çemberler birbirine ulaşmıyorsa hata iki yarıçapı ve merkezler arası mesafeyi
birlikte yazar, böylece hangi ölçünün yanlış olduğunu görürsünüz.

### Çizim örnekleri

Ölçü listesini okuduktan sonra noktalarınıza numarasıyla ulaşırsınız:

```
NOKTALAR dosya="olcu.txt"
```

```
ÇİZGİ n(1284) n(1285)                        ← iki ölçü noktası arasına
ÇİZGİ orta(n(1),n(2)) @0,25                  ← kenar ortasından kuzeye 25 m
ÇİZGİ kes(n(1),n(2),n(3),n(4)) @10<0         ← iki cephe hattının köşesinden
NOKTA semt(n(10),128.4560,62.317)            ← istasyondan semt ve kenar
NOKTA kes(n(1),34.28,n(2),51.06,sol)         ← iki şerit ölçüsünden
NOKTA ara(n(1),n(2),0.5) ara(n(1),n(2),12 m) ← kenar üzerinde oran ve metre
```

Aynı inşalar numarasız da yazılır; `n(...)` yerine koordinatı, `son`'u ya da başka bir
fonksiyonu koyabilirsiniz:

```
ÇİZGİ 10,20 orta(son,@40,0) kes(0,0,100,100,0,100,100,0) uzanti(0,0,30,40,25)
```

## İfade değerlendirici

Parantez içinde ve sayı beklenen her yerde çalışır.

| İşleç | Anlamı | Örnek |
|---|---|---|
| `+` `-` | Toplama, çıkarma | `(100+50)` |
| `*` `/` | Çarpma, bölme | `(100*3)` |
| `%` | Kalan | `(100%30)` |
| `^` | Üs | `(2^10)` |
| `( )` | Gruplama | `((2+3)*4)` |
| `-` (önek) | Negatif | `(-5+2)` |

Öncelik matematikteki gibidir: `(2+3*4)` sonucu `14`, `((2+3)*4)` sonucu `20`. Üs
sağdan birleşir: `(2^3^2)` sonucu `512`.

Ondalık ayırıcı her zaman noktadır ve makinenin bölge ayarından etkilenmez.

## Anahtar=değer argümanları

Parametreleri adıyla verebilirsiniz; sıra önemli değildir:

```
KATMAN ad=PARSEL gorunur=evet kilitli=hayır renk=4281236786
YAKINLAŞ mod=ÇARPAN carpan=1.5
```

Evet/hayır değerleri için `evet`, `hayır`, `yes`, `no`, `true`, `false`, `1`, `0`
kabul edilir.

**Bir nokta listesinin anahtarı, ardından gelen koordinatları da toplar.** Birden çok
nokta alan bir parametrenin adını bir kez yazmanız yeter; arkasından boşlukla
yazdığınız her koordinat o listeye eklenir:

```text
ALANÖLÇ noktalar=0,0 20,0 20,10 0,10
KOPYALA baslangic=0,0 bitis=20,0 40,0 60,0
```

Liste, koordinat olmayan ilk değerde ya da başka bir anahtarda biter. Nesne kimliği
gibi başka listelerde anahtarı yinelersiniz: `SİL nesneler=1 nesneler=2`.

İçinde boşluk olan metin tırnak içine alınır:

```
KATMAN ad="YOL KENARI"
```

Hangi komutun hangi parametreleri aldığını [komut referansından](referans.md) veya
program içinden görebilirsiniz:

```
YARDIM komut=KATMAN
```

## Çalışan komuta değer vermek

Bir komut `İlk nokta` gibi bir istek gösterirken haritaya tıklayabilir **veya** komut
satırına koordinat yazabilirsiniz. İkisi de aynı kapıya çıkar:

```
ÇİZGİ                          ← Enter, komut nokta istemeye başlar
485320.150,4310220.400         ← Enter
@50,30                         ← Enter
@100<45                        ← Enter
                               ← Esc, komut biter
```

Komut çalışırken şeffaf bir komut yazarsanız (`YAKINLAŞ` gibi) araya girer, görünümü
değiştirir ve çalışan komut kaldığı yerden devam eder.

**Son noktayı geri almak.** `ÇİZGİ`, `ÇOKLUÇİZGİ`, `ALAN` ya da `SPLINE` bir sonraki noktayı
beklerken `G` yazıp Enter'a basarsanız yalnız son nokta geri alınır ve komut onu yeniden
ister; `GERİ`, `U` ve `GERİAL` de bu istemde aynı şeyi yapar. Boş satırda **⌫** da
aynıdır:

```
ÇİZGİ                          ← Enter
0,0                            ← Enter
10,0                           ← Enter
99,99                          ← Enter — yanlış nokta
G                              ← Enter — 99,99 geri alındı, çizgi 10,0'dan devam eder
10,10                          ← Enter
                               ← Enter, iki çizgi yazılır
```

Komutla birlikte yazdığınız noktalar (`ÇİZGİ 0,0 10,0 10,10`) yakalama yardımcılarından
geçer ama çalışmanın kendi noktalarına çekilmez; onlar çalışma başlamadan yazılmış kesin
değerlerdir.

## Geçmiş

| Tuş | İşlev |
|---|---|
| **Yukarı ok** | Bir önceki komut |
| **Aşağı ok** | Bir sonraki komut |
| **Esc** | Satırı temizler; satır zaten boşsa çalışan komutu iptal eder |

Aynı komutu iki kez yazarsanız geçmişte bir kez durur.

Komut satırına yazdığınız her şey Transkript panelinde `> ` önekiyle görünür, böylece
oturum boyunca ne yaptığınız yukarı doğru okunabilir kalır.

## Neden tek harfli kısayol yok

`Ç` ve `L` gibi kısaltmalar komut satırına **yazılır**, kısayol tuşu değildir. Tek harfli
genel kısayol olsaydı komut satırına `ÇİZGİ` yazarken ilk harf komuta kaçardı. Kısayollar
bu yüzden `Ctrl` ile birleşiktir.

## Hata mesajları

Hata mesajları ne beklendiğini ve ne geldiğini birlikte söyler.

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bilinmeyen komut: 'XYZ'. YARDIM yazarak komut listesini görün.` | Komut adı yanlış | `YARDIM` ile listeye bakın |
| `'core.line': 'noktalar' parametresi en az 2 değer istiyor, 1 değer geldi.` | Çizgi için tek nokta verilmiş | İkinci noktayı ekleyin |
| `'core.layer': bilinmeyen parametre 'yokboyle'. Tanımlı parametreler: ad, gorunur, kilitli, renk` | Parametre adı yanlış yazılmış | Doğru adı listeden alın |
| `'core.layer': zorunlu 'ad' parametresi eksik. Beklenen: metin` | Zorunlu parametre verilmemiş | Parametreyi ekleyin |
| `'core.line': 'noktalar' parametresi nokta listesi bekliyor. Girilen: 'abc'` | Koordinat yerine metin yazılmış | Koordinat biçimlerinden birini kullanın |
| `Beklenen: '@dx,dy' veya '@mesafe<açı'. Girilen: '@50'` | `@` sonrası eksik | `@50,0` veya `@50<0` yazın |
| `Kutupsal açı: beklenmeyen 'x' karakteri (konum 2)` | Açı sonekinde `g`, `d`, `r` dışında bir harf, ya da iki harf | Soneki düzeltin ya da kaldırın: `@100<45g` |
| `Beklenen: koordinat (x,y \| @dx,dy \| @mesafe<açı \| nokta fonksiyonu: …). Girilen: 'abc'` | Nokta beklenen yere koordinat olmayan bir şey girilmiş | Koordinat ya da nokta fonksiyonu girin |
| `kes(): argümanlar hiçbir biçime uymuyor. Biçimler: …` | `kes` üç biçimden hiçbirine uymayan argüman almış | Mesajdaki üç biçimden birini yazın |
| `orta(): 1. argüman nokta olmalı (…). Girilen: 'abc'` | Nokta beklenen argümana koordinat olmayan bir şey girilmiş | Koordinat, `son` ya da başka bir fonksiyon yazın |
| `orta(): fazla argüman. Beklenen: orta(A,B)` | Fonksiyona biçiminden çok argüman verilmiş | Fazlalığı çıkarın; mutlak bir noktanın iki argüman yeri harcadığını unutmayın |
| `1284 numaralı nokta yok. Nokta listesini NOKTALAR ile okuyun.` | `n(1284)` çizimde bulunamadı | Listeyi `NOKTALAR` ile okuyun ya da numarayı düzeltin |
| `n(): bu bağlamda çizim yok, numaralı nokta aranamaz.` | `n()` çizimi olmayan bir yerde çağrılmış | Numaralı noktayı çizim açıkken kullanın |
| `kes(): iki doğrultu paralel, kesişmiyorlar. Açılar: …` | İki doğrultu aynı ya da tam ters | Açılardan birini düzeltin |
| `kes(): çemberler birbirine ulaşmıyor. Yarıçaplar … merkezler arası …` | İki mesafe ölçüsü kesişmiyor | Mesafeleri ve merkez noktalarını karşılaştırın |
| `kes(): yön noktası iki çözüme eşit uzaklıkta…` | Verilen yakın nokta iki çözümün tam ortasında | `yon=sol` ya da `yon=sağ` yazın |
| `Nokta fonksiyonları en fazla 16 kat iç içe yazılır.` | İç içe fonksiyon çok derin | İnşayı birkaç komuta bölün |
| `'(1+2' ifadesi: kapanmamış parantez` | Parantez kapatılmamış | Parantezi kapatın |
| `'1/0' ifadesi: sıfıra bölme` | Sıfıra bölme | İfadeyi düzeltin |
| `'abc' ifadesi: sayı bekleniyordu (konum 0)` | İfadede sayı olmayan bir şey var | İfadeyi düzeltin |
| `Komut satırında kapanmamış tırnak var.` | Tırnak açılmış kapatılmamış | Tırnağı kapatın |
| `'core.line' daha fazla argüman almıyor. Fazlalık: ...` | Komuta kapasitesinden fazla argüman verilmiş | Fazlalığı çıkarın |

Bütün mesajlar ve çözümleri: [Sorun giderme](../sorun-giderme.md).

## Bu sürümde henüz olmayanlar

Aşağıdakiler tasarımın parçasıdır ama bugün çalışmaz:

| Özellik | Ne zaman |
|---|---|
| **Ctrl+R** ile komut geçmişinde arama | Faz 1 |
| `alias.json` ile kullanıcı tanımlı kısaltmalar | Faz 1 |
| Yazarken açılan parametre ipucu balonu | Faz 1 |
| Komut ortasında nesne yakalama geçersiz kılma (`ORTA` gibi) | Faz 2 |
| Ayrılabilir transkript penceresi | Faz 2 |

## Sırada ne var

- [Komut referansı](referans.md) — bütün komutlar ve parametreleri
- [Çizgi çizme](line.md) — koordinat girişinin en çok kullanıldığı komut
- [Betik yazma](../betik/README.md) — komut satırında yaptığınızı otomatikleştirmek
