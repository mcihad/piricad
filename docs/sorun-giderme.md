# Sorun Giderme

Bir hata mesajıyla karşılaşan herkes için; bu sayfa KentOSCad'in verebileceği mesajları
sebebi ve çözümüyle birlikte listeler.

Mesajlar burada göründüğü gibi yazılır; değişken kısımlar örnek değerlerle gösterilmiştir.

## Komut çözümleme

### `Bilinmeyen komut: 'XYZ'. YARDIM yazarak komut listesini görün.`

**Sebep.** Yazdığınız ad hiçbir komuta karşılık gelmiyor.

**Çözüm.** `YARDIM` yazarak listeye bakın. Türkçe ad, İngilizce karşılık, kısaltma veya
komut kimliğinin herhangi biri çalışır. Türkçe karakter yazamıyorsanız `CIZGI`, `SIL`,
`YAKINLAS`, `GERIAL`, `YINELE`, `BETIK` karşılıklarını kullanın.

### `Bilinmeyen komut: 'XYZ'`

**Sebep.** `YARDIM komut=XYZ` ile sorduğunuz komut yok.

**Çözüm.** Parametresiz `YARDIM` ile listeye bakın.

## Parametre doğrulama

### `'core.layer': zorunlu 'ad' parametresi eksik. Beklenen: metin`

**Sebep.** Komutun zorunlu bir parametresi verilmemiş.

**Çözüm.** Mesajda adı ve beklenen tipi yazıyor. `YARDIM komut=KATMAN` bütün
parametreleri listeler.

### `'core.layer': bilinmeyen parametre 'renkler'. Tanımlı parametreler: ad, gorunur, kilitli, renk`

**Sebep.** Parametre adı yanlış yazılmış. KentOSCad yazım hatasını sessizce yutmaz.

**Çözüm.** Mesajın sonundaki tanımlı parametrelerden doğru olanı seçin.

### `'core.line': 'noktalar' parametresi en az 2 değer istiyor, 1 değer geldi.`

**Sebep.** Parametreye gereken sayıda değer verilmemiş.

**Çözüm.** Eksik değeri ekleyin. Çizgi en az iki nokta ister.

### `'core.zoom': 'carpan' parametresi en fazla 1 değer alır, 2 değer geldi.`

**Sebep.** Parametreye kapasitesinden fazla değer verilmiş.

**Çözüm.** Fazlalığı çıkarın.

### `'core.line': 'noktalar' parametresi nokta listesi bekliyor. Girilen: 'abc'`

**Sebep.** Parametre yanlış türde bir değer almış.

**Çözüm.** Mesaj beklenen tipi söylüyor: nokta, nokta listesi, sayı, tam sayı, metin,
evet/hayır veya nesne seçimi.

### `'core.erase': 'nesneler' parametresi nesne seçimi bekliyor. Girilen: point(1.000000,2.000000). Birden çok değer için anahtarı yineleyin: nesneler=1 nesneler=2`

**Sebep.** Birden çok kimlik virgülle yazılmış. `1,2` bu programın tek dilbilgisinde
bir koordinattır — iki kimlik değil — ve üç kimlik (`1,2,3`) koordinat olarak da
okunamaz.

**Çözüm.** Anahtarı yineleyin: `SİL nesneler=1 nesneler=2 nesneler=3`. Aynı kural
çok değerli her parametre için geçerlidir. Betikte liste dizidir: `"nesneler": [1, 2, 3]`.

### `'core.line' daha fazla argüman almıyor. Fazlalık: 'xyz'`

**Sebep.** Komutun bütün parametreleri dolmuş, ama satırda argüman kalmış.

**Çözüm.** Fazlalığı çıkarın; muhtemelen bir yerde boşluk fazladır.

### `'ad=' anahtarına değer verilmemiş.`

**Sebep.** Eşittir işaretinden sonra bir şey yazılmamış.

**Çözüm.** Değeri ekleyin: `ad=PARSEL`.

## Koordinat ve ifade

### `Beklenen: koordinat (x,y | @dx,dy | @mesafe<açı | nokta fonksiyonu: orta, dik, semt, kes, ara, uzanti, xy, n, son). Girilen: 'abc'`

**Sebep.** Nokta beklenen yere koordinat olmayan bir şey girilmiş.

**Çözüm.** Beş koordinat biçiminden birini kullanın. Bkz.
[Komut satırı](komutlar/komut-satiri.md).

## Nokta fonksiyonları

### `kes(): argümanlar hiçbir biçime uymuyor. Biçimler: kes(A,açı1,B,açı2) · kes(A,r1,B,r2,sol|sağ|yon=<nokta>) · kes(A,B,C,D). Girilen: 'kes(0,0,50)'`

**Sebep.** Argüman listesi fonksiyonun hiçbir biçimine oturmuyor. En sık sebebi,
mutlak yazılan bir noktanın **iki** argüman yeri harcadığının atlanmasıdır:
`kes(0,0,50)` iki nokta değil, bir nokta ve bir sayıdır.

**Çözüm.** Mesajdaki biçimlerden birini yazın. Bkz.
[Nokta fonksiyonları](komutlar/komut-satiri.md#nokta-fonksiyonları).

### `orta(): 1. argüman nokta olmalı (x,y | @dx,dy | @mesafe<açı | son | fonksiyon). Girilen: 'abc'`

**Sebep.** Tek biçimi olan bir fonksiyonun bir argümanı okunamadı; mesaj kaçıncı
argüman olduğunu ve ne geldiğini söyler.

**Çözüm.** O argümanı koordinat, `son` ya da başka bir nokta fonksiyonu olarak yazın.

### `orta(): fazla argüman. Beklenen: orta(A,B)`

**Sebep.** Fonksiyona biçiminin aldığından çok argüman verilmiş.

**Çözüm.** Fazlalığı çıkarın. `orta(0,0,10,10)` iki noktadır; `orta(0,0,10,10,20,20)`
üç nokta olurdu ve `orta` iki alır.

### `1284 numaralı nokta yok. Nokta listesini NOKTALAR ile okuyun.`

**Sebep.** `n(1284)` çizimde bulunamadı. Numaralı noktalar `nokta_no` özniteliğiyle
gelir; onu `NOKTALAR` doldurur.

**Çözüm.** Ölçü listesini `NOKTALAR dosya="olcu.txt"` ile okuyun ya da numarayı
düzeltin. Numara metin olarak birebir karşılaştırılır: `1284` `1284/A`'yı bulmaz.

### `n(): bu bağlamda çizim yok, numaralı nokta aranamaz.`

**Sebep.** `n()` çizimin olmadığı bir bağlamda çağrılmış — örneğin yalnız bir ifadenin
denendiği yerde.

**Çözüm.** Numaralı noktayı bir çizim açıkken kullanın.

### `kes(): iki doğrultu paralel, kesişmiyorlar. Açılar: 100,0000 grad ve 300,0000 grad`

**Sebep.** İki doğrultu aynı ya da tam ters; kesişimleri tek bir nokta değil. Aynı
mesaj iki doğru için `kes(): iki doğru paralel, kesişmiyorlar.` biçiminde gelir.

**Çözüm.** Açılardan ya da noktalardan birini düzeltin.

### `kes(): çemberler birbirine ulaşmıyor. Yarıçaplar 10,000 m ve 20,000 m, merkezler arası 100,000 m.`

**Sebep.** İki mesafe ölçüsü kesişmiyor. Mesaj iki yarıçapı ve merkezler arası mesafeyi
birlikte yazar, çünkü hangisinin yanlış olduğu ancak üçü bir arada görülünce anlaşılır.
`kes(): bir çember ötekinin tamamen içinde.` aynı ölçünün öteki türlü tutmamasıdır.

**Çözüm.** Mesafeleri ve merkez noktalarını karşılaştırın; genellikle bir ölçü yanlış
noktaya yazılmıştır.

### `kes(): yön noktası iki çözüme eşit uzaklıkta, hangisi olduğu belli değil. 'yon=sol' ya da 'yon=sağ' yazın.`

**Sebep.** İki mesafe kesişiminin iki çözümü vardır ve verilen yakın nokta tam
ortalarında kaldı.

**Çözüm.** `yon=sol` ya da `yon=sağ` yazın.

### `Nokta fonksiyonları en fazla 16 kat iç içe yazılır.`

**Sebep.** İç içe yazılmış fonksiyonlar sınırı aştı.

**Çözüm.** İnşayı birkaç komuta bölün: ara noktayı `NOKTA` ile çizip `n(...)` ile
kullanmak da bir yoldur.

### `Beklenen: '@dx,dy' veya '@mesafe<açı'. Girilen: '@50'`

**Sebep.** `@` ile başlayan koordinat eksik yazılmış.

**Çözüm.** Göreli için `@50,0`, kutupsal için `@50<0` yazın.

### `Kutupsal açı: beklenmeyen 'x' karakteri (konum 2)`

**Sebep.** Açının sonunda `g` (grad), `d` (derece) ve `r` (radyan) dışında bir harf var.
Birim soneki yalnız bu üçünden biridir ve tek harftir; `@100<45gg` de aynı sebeple
reddedilir.

**Çözüm.** Soneki düzeltin ya da kaldırın: `@100<45g`. Sonek yoksa açı `açı_birimi`
ayarıyla okunur. Bkz. [Komut satırı](komutlar/komut-satiri.md).

### `X koordinatı: sayı bekleniyordu (konum 0)`

**Sebep.** Koordinatın X bileşeni sayıya çözülemedi. `Y koordinatı:`, `Göreli dx:`,
`Göreli dy:`, `Kutupsal mesafe:` ve `Kutupsal açı:` önekleri de aynı anlama gelir.

**Çözüm.** Ondalık ayırıcının nokta olduğundan ve koordinatta boşluk bulunmadığından emin
olun: `485320.150,4310220.400`.

### `'(1+2' ifadesi: kapanmamış parantez`

**Sebep.** Satır içi ifadede parantez kapatılmamış.

**Çözüm.** Parantezi kapatın: `@(1+2),0`.

### `'1/0' ifadesi: sıfıra bölme`

**Sebep.** İfadede sıfıra bölme var. `sıfıra göre mod` mesajı da aynı sebeptendir.

**Çözüm.** İfadeyi düzeltin.

### `'2+' ifadesi: ifade beklenmedik yerde bitti`

**Sebep.** İfade yarım kalmış.

**Çözüm.** Eksik terimi tamamlayın.

### `'2 3' ifadesi: beklenmeyen '3' karakteri (konum 2)`

**Sebep.** İfadede beklenmeyen bir karakter var.

**Çözüm.** İşleç eksik olabilir: `2*3`.

### `Komut satırında kapanmamış tırnak var.`

**Sebep.** Tırnak açılmış ama kapatılmamış.

**Çözüm.** Tırnağı kapatın: `KATMAN ad="YOL KENARI"`.

### `Boş komut satırı.`

**Sebep.** Boş bir satır gönderilmiş.

**Çözüm.** Bir komut yazın.

## Çizim ve katman

### `'PARSEL' katmanı kilitli.`

**Sebep.** Aktif katman kilitli; kilitli katmana çizilemez.

**Çözüm.** `KATMAN ad=PARSEL kilitli=hayır` ile kilidi açın, veya **Katmanlar**
panelinde **Kilit** sütununa çift tıklayın, veya başka bir katmana geçin.

### `Bir çoklu çizgi en az 2 tepe noktası ister, verilen: 1`

**Sebep.** Betikten tek noktalı bir liste gelmiş.

**Çözüm.** Listeye ikinci noktayı ekleyin.

### `Bilinmeyen katman kimliği: 7`

**Sebep.** Var olmayan bir katmana işlem yapılmaya çalışılmış.

**Çözüm.** Katman adını denetleyin; **Katmanlar** paneli mevcut katmanları listeler.

### `Nesne bulunamadı veya zaten silinmiş: 99`

**Sebep.** `SİL` komutuna var olmayan ya da zaten silinmiş bir kimlik verilmiş.

**Çözüm.** Kimliği **Komut Günlüğü** panelinden denetleyin. Bu durumda **hiçbir nesne
silinmez**; komut çizime dokunmadan durur.

### `Geçersiz nesne kimliği: 0. Kimlikler 1'den başlar.`

**Sebep.** Sıfır ya da negatif kimlik verilmiş.

**Çözüm.** Nesne kimlikleri `1`'den başlar. Kimlikleri [`SEÇ`](komutlar/select.md) ile
ya da **Komut Günlüğü** panelinden okuyun.

### `İşlem yapılacak nesne yok: seçim boş ve 'nesneler' verilmedi.`

**Sebep.** Nesnelerle çalışan bir komut (`SİL`, `KES`, `TAŞI`, `TEVHİT` …) bir
betikten ya da tek satırdan kimliksiz çağrılmış ve etkin seçim de boş. Arayüzde bu
komutlar sormaz, nesneleri ister; bu cümleyi soruyu cevaplayamayan bir betik görür
— ya da istenen nesneleri seçmeden Enter'a basan kullanıcı.

**Çözüm.** Önce [`SEÇ`](komutlar/select.md) ile nesne seçin ya da `nesneler=` ile en az
bir kimlik verin; mesajın altındaki örnek satır komutun kendi yazımıdır.

### `Bir seferde en fazla 1 nesne; 2 nesne seçili.`

**Sebep.** Tek nesneyle çalışan bir araca (`KIR`, `UZUNLUK`, `BÖLÜMLE`, `İFRAZ` …)
birden çok nesne seçiliyken bir betikten ulaşılmış. Arayüzde araç reddetmez;
"2 nesne seçili; bu araç bir seferde 1 nesneyle çalışır" diyerek istediğini sorar.

**Çözüm.** İstenen nesneye tıklayıp Enter'a basın, betikte ise `nesne=` ile tek bir
kimlik verin.

### `Kırpma sınırı, nesne N ('AD') referansının çizdiği hiçbir şeyi içine almıyor; referans görünmez olurdu.`

**Sebep.** [BLOKKIRP](komutlar/block_clip.md) sınırı, referansın çizdiği her şeyin
dışında çizildi. Böyle bir kırpma referansı görünmez, seçilmez ve bulunmaz yapardı; bu
yüzden reddedilir ve hiçbir şey değişmez.

**Çözüm.** Sınırı referansın üzerine çizin. Referansın nerede olduğunu KAPSAM ile görün;
kırpılmış bir referansın eski sınırı, referans seçiliyken kesikli çizilir.

### `Kırpma sınırı verilmedi: noktalar= ile köşelerini ya da cizgi= ile kapalı bir nesneyi verin.`

**Sebep.** BLOKKIRP bir betikten ya da tek satırdan sınırsız çağrıldı; arayüzde sınırın
köşelerini sorar.

**Çözüm.** `noktalar=` ile iki köşe (dikdörtgen) ya da üç ve daha çok köşe (çokgen), ya da
`cizgi=` ile çizimdeki kapalı bir nesneyi verin.

## Seçim

### `Beklenen mod: TÜMÜ | TEMİZLE | NESNE | PENCERE | KESEN | KUTU | NOKTA. Girilen: 'OLMAYAN'`

**Sebep.** `SEÇ` komutuna tanınmayan bir mod adı verilmiş.

**Çözüm.** Mesajdaki listeden birini yazın. İngilizce ve karaktersiz karşılıkları da
kabul edilir: `ALL`, `CLEAR`, `WINDOW`, `CROSSING`, `BOX`, `POINT`.

### `Beklenen işlem: DEĞİŞTİR | EKLE | ÇIKAR | TERSİNE. Girilen: 'BİLİNMEYEN'`

**Sebep.** `islem=` parametresine tanınmayan bir değer verilmiş.

**Çözüm.** `EKLE`, `ÇIKAR` veya `TERSİNE` yazın; hiç yazmazsanız seçim değiştirilir.

### `'PENCERE' 2 nokta bekliyor. Girilen: 1 nokta.`

**Sebep.** Kutu seçimine tek köşe verilmiş.

**Çözüm.** İki köşe verin: `SEÇ PENCERE 0,0 100,100`.

### `Bu aramada nesne bulunamadı. Seçimde 0 nesne var.`

**Sebep.** Kutu ya da nokta boş yere düşmüş.

**Çözüm.** Hata değildir. Kutuyu büyütün, ya da `SEÇ NOKTA` kullanıyorsanız `tolerans=`
ile metre cinsinden bir yarıçap verin. Ekranı olmayan bir betikte piksel toleransı
yoktur; `tolerans=` bunun içindir.

## Geri alma

### `Geri alınacak işlem yok`

**Sebep.** Geri alma yığını boş.

**Çözüm.** Kaygılanacak bir şey yoktur: geri alınacak bir düzenleme yapılmamış demektir.
`YAKINLAŞ`, `YARDIM` ve boş katman yaratmak geri alma adımı bırakmaz. Bir betik bu
cevabı hata olarak alır ve orada durur, çünkü istediği geri alma gerçekleşmemiştir.

### `Düzenleme yapmış bir toplu işin içinde geri alınamaz: …`

Aynısı `yinelenemez` ile de gelir.

**Sebep.** Bir betik ya da toplu iş bir şey çizdikten sonra `core.undo` veya `core.redo`
çağırdı. Betik bitmeden onun geri alma adımı yoktur; geri alma, betikten **önce**
yaptığınız işe uzanır ve betik bitince o iş geri getirilemez biçimde kaybolurdu.

**Çözüm.** Adımı betikten çıkarın. Betik bittikten sonra tek `GERİAL` betiğin tamamını
geri alır. Bkz. [Geri alma](komutlar/undo.md).

### `Yinelenecek işlem yok`

**Sebep.** Yineleme yığını boş.

**Çözüm.** Ya hiç geri alma yapılmamış, ya da geri aldıktan sonra yeni bir düzenleme
yapılmış — o durumda yineleme dalı silinir. Bkz. [Yineleme](komutlar/redo.md).

## Görünüm

### `Beklenen mod: KAPSAM | ÇARPAN | SIFIRLA. Girilen: 'OLMAYAN'`

**Sebep.** `YAKINLAŞ` komutuna geçersiz bir kip verilmiş.

**Çözüm.** Üç kipten birini yazın. İngilizce karşılıkları `EXTENTS`, `FACTOR`, `RESET` de
kabul edilir.

### `Görünüm istemcisi bağlı değil (başsız çalışma).`

**Sebep.** `YAKINLAŞ` arayüz olmadan çalıştırılmış.

**Çözüm.** Hata değildir; başsız çalışmada beklenen davranıştır. Aynı betik arayüzde
çalıştırıldığında görünümü değiştirir.

### Boş çizimde `YAKINLAŞ KAPSAM` bir şey değiştirmiyor

**Sebep.** Sığdırılacak nesne yok.

**Çözüm.** Beklenen davranıştır; görünüm başlangıç konumuna döner. **Öznitelikler**
panelindeki **Kapsam** grubu `boş çizim` yazar.

## Betik

### `Betik dosyası açılamadı: yol/dosya.json`

**Sebep.** Dosya yok veya okunamıyor.

**Çözüm.** Yolu denetleyin. Göreli yol programın çalışma dizinine göre çözülür.

### `Betik dosya erişimi 'güvenli' kum havuzunda kapalıdır. Gerekli seviye: 'proje' veya 'tam'.`

**Sebep.** Kum havuzu seviyesi dosya okumaya izin vermiyor.

**Çözüm.** Betiği KentOSCad uygulaması içinden çalıştırın; uygulama `proje` seviyesini
kullanır.

### `'X' sütunu bir betiğin ya da toplu işin içinde silinmez: …`

**Sebep.** Betik (ya da uygulanan bir yapay zekâ önerisi) var olan bir sütunu siliyor ya
da tanımını değiştiriyor. İkisi de kayıt bırakmaz; betik sonradan yarıda kalsaydı silinen
değerler geri getirilemezdi.

**Çözüm.** O `SÜTUN` satırını betikten çıkarıp betikten önce tek başına çalıştırın.

### `Betik ya bir komut dizisi ya da "komutlar" alanı olan bir nesne olmalı`

**Sebep.** Dosyanın kök yapısı beklenen iki biçimden hiçbirine uymuyor.

**Çözüm.** Ya doğrudan bir dizi yazın, ya da `komutlar` alanı olan bir nesne. Bkz.
[Betik yazma](betik/README.md).

### `Betik satırı 2 bir nesne olmalı: 42. Betik çalıştırılmadı.`

**Sebep.** Komut dizisinin ikinci öğesi nesne değil. Bütün satırlar çalıştırılmadan önce
okunur; bozuk bir satır varsa betik **hiç çalışmaz**, öncesindeki satırlar da.

**Çözüm.** Her satırı `{ "cmd": ..., "args": ... }` biçiminde yazın.

### `Betik satırı 2: "cmd" alanı yok: {"args":{}}. Betik çalıştırılmadı.`

**Sebep.** İkinci satırda komut adı belirtilmemiş; betik hiç çalışmadı.

**Çözüm.** `"cmd"` (veya `"komut"`) alanını ekleyin.

### `Betik satırı 3 (core.line): 'core.line': 'noktalar' parametresi en az 2 değer istiyor, 1 değer geldi. Betik bütünüyle geri alındı; çizim betikten önceki hâlinde.`

**Sebep.** Betiğin üçüncü komutu doğrulamayı geçemedi.

**Çözüm.** Mesaj satır numarasını ve komutu verir. Çizim betikten önceki hâlindedir;
**betiğin tamamı geri alınmıştır** — `YİNELE` onu geri getirmez, komut günlüğünde de
çalışan satırlarından iz kalmaz.

### `Komut argümanları bir JSON nesnesi olmalı.`

**Sebep.** `args` alanı nesne değil.

**Çözüm.** `"args": { ... }` yazın.

### `Nokta [x_mm, y_mm] biçiminde olmalı. Girilen: [1,2,3]`

**Sebep.** Nokta iki bileşenli değil.

**Çözüm.** Her noktayı `[x, y]` olarak yazın. Değerler **milimetre tam sayıdır**.

### `Komut argümanı nesne olamaz: {"x":1}`

**Sebep.** Argüman değeri olarak JSON nesnesi verilmiş.

**Çözüm.** Sayı, metin, boolean veya dizi kullanın.

### `Kimlik listesinde sayı bekleniyordu, gelen: "a"`

**Sebep.** Nesne kimliği listesinde sayı olmayan bir öğe var.

**Çözüm.** Kimlikleri sayı olarak yazın: `[0, 1, 2]`.

### `Betik motoru bağlı değil.`

**Sebep.** Betik motoru olmayan bir ortamda `BETİK` çağrılmış.

**Çözüm.** Betiği KentOSCad uygulaması içinden çalıştırın.

### Betik çalıştı ama çizim görünmüyor

**Sebep.** Görünüm betiğin çizdiği yerde değil. Betikteki koordinatlar milimetre
olduğundan, metre yazdıysanız çizim bin kat küçük bir bölgeye düşmüştür.

**Çözüm.** Önce `YAKINLAŞ KAPSAM` deneyin. Hâlâ tuhafsa koordinat birimini denetleyin:
`485320.150` metre, betikte `485320150` yazılmalıdır.

## Günlük

### `Günlük dosyası açılamadı: yol`

**Sebep.** Uygulama veri dizinine yazılamıyor.

**Çözüm.** Dizin izinlerini denetleyin. Program çalışmaya devam eder; yalnız günlük diske
yazılmaz, panel yine dolar.

### `Günlük satırı 5: Günlük satırında metin türünde "cmd" alanı yok.`

**Sebep.** Okunmak istenen günlük dosyası bozuk.

**Çözüm.** Söylenen satırı düzeltin veya çıkarın.

## Kurulum ve derleme

### `KENTOS_WITH_RHI=ON but qsb was not found.`

**Sebep.** GPU çizim arka ucu istenmiş ama gölgelendirici derleyicisi kurulu değil.

**Çözüm.** `qt6-shadertools` paketini kurun veya `-DKENTOS_WITH_RHI=OFF` ile
yapılandırın. Bu sürümde GPU arka ucu zaten kapalıdır.

### `KENTOS_WITH_PYTHON=ON but CPython 3.14 ... was not found`

**Sebep.** Python betik motoru açılmış ama makinede CPython 3.14 ve geliştirme
başlıkları yok.

**Çözüm.** Yapılandırmanın yazdığı paketi kurun (`brew install python@3.14`,
`apt install python3.14-dev`, `winget install Python.Python.3.14`) ya da
`-DKENTOS_WITH_PYTHON=OFF` ile yapılandırın.

### Qt bulunamıyor

**Sebep.** Qt 6 kurulu değil ya da CMake onu göremiyor.

**Çözüm.** `make doctor` çalıştırın; Qt satırı `MISSING` diyorsa Qt 6 geliştirme
paketlerini kurun. Ubuntu/Debian'da `sudo apt install qt6-base-dev`.

### `make test` kırmızı

**Sebep.** Bir test veya bir CI kapısı başarısız.

**Çözüm.** `make gates` yalnız kapıları çalıştırır ve hangisinin ne sebeple durduğunu
dosya ve satır numarasıyla yazar.

## Dosya açma ve kaydetme

### `io.not_a_project: '...' bir KentOSCad proje dosyası değil.`

**Sebep.** `AÇ` yalnızca KentOSCad proje dosyalarını (`.pcad`) açar; verdiğiniz dosya
başka bir biçim.

**Çözüm.** DXF, GeoPackage gibi dış biçimler için `İÇEAKTAR` kullanın.
Bkz. [Dış veri alma](komutlar/import.md).

### `io.format_too_new: '...' en az N. sürüm biçim okuyucusu istiyor`

**Sebep.** Dosyayı, bu yapının okuyamayacağı daha yeni bir KentOSCad yazmış.

**Çözüm.** Mesajda adı geçen sürüme yükseltin. KentOSCad dosyayı yarım açmaz; yarım
açılmış bir proje, açılmamış bir projeden tehlikelidir.

### `io.truncated: '...' N bayt olduğunu bildiriyor, ama M bayt.`

**Sebep.** Dosya yarım kopyalanmış, aktarım kesilmiş ya da disk hatası olmuş.

**Çözüm.** Yedeğinizden geri alın ve kopyalamayı yeniden yapın. KentOSCad bozuk bir
dosyayı kendiliğinden onarmaz; sessizce "düzeltilmiş" bir kadastro dosyası, bozuk
olduğu bilinen bir dosyadan kötüdür.

### `io.bad_block: ...` · `io.inconsistent: ...` · `io.key_mismatch: ...`

**Sebep.** Dosyanın iç yerleşimi bozulmuş: bir bloğun yeri, uzunluğu ya da nesne
anahtarlarının sırası tutmuyor.

**Çözüm.** Yedeğinizden geri alın. Bu üç mesajdan biri görünüyorsa dosya güvenilir
değildir. Bkz. [KentOSCad proje dosyası](veri/proje-dosyasi.md).

### `io.unknown_kind: ... 65535 numaralı türde; bu değer 'tür yok' anlamına ayrılmıştır`

**Sebep.** Dosyanın tür sütununa ayrılmış değer yazılmış; hiçbir KentOSCad sürümü bunu
yazmaz. Bu sürümün tanımadığı gerçek bir tür bu hatayı **vermez**: nesne görünür ve
korunur, yalnız düzenlenemez ("Bu yapının tanımadığı türdeki nesne düzenlenemez;
olduğu gibi korunur.").

**Çözüm.** Yedeğinizden geri alın. Düzenlenemeyen bir nesneyle karşılaşıyorsanız
dosyayı yazan KentOSCad sürümüne yükseltin; nesne o sürümde tam anlamıyla açılır.

### `Bu çizim henüz bir dosyaya bağlı değil. FARKLIKAYDET ile bir ad verin.`

**Sebep.** `KAYDET` hiç kaydedilmemiş bir çizimde çalıştırıldı.

**Çözüm.** `FARKLIKAYDET` ile bir ad verin. KentOSCad ad uydurmaz.

### `'...' yazılırken hata oluştu; disk dolu olabilir. Önceki dosya değiştirilmedi.`

**Sebep.** Kaydetme sırasında disk doldu ya da yazma kesildi.

**Çözüm.** Yer açıp yeniden kaydedin. Son cümle önemlidir: KentOSCad önce yanına
geçici bir dosya yazıp ancak tamamlandığında yerine koyduğu için **bir önceki
kaydınız yerinde durur**.

### `Dosya motoru bağlı değil; bu ortamda dosya açılıp kaydedilemez.`

**Sebep.** Dosya motoru kurulmamış bir ortamda (örneğin başsız bir sınama) çalışılıyor.

**Çözüm.** Komutu uygulama içinden çalıştırın.

### `uyarı: 'X' dış referansı yüklenemedi: … Çizim açıldı; referans boş çizilir.`

**Sebep.** Çizime bağlı bir [dış referansın](komutlar/xref.md) dosyası kayıtlı yerinde de,
proje dosyasının klasöründe de bulunamadı ya da okunamadı. Çizim yine açılır; referans
yerinde, boş durur.

**Çözüm.** Dosyanın yeni yerini gösterin: `DIŞREFERANS islem=yol ad=X dosya=<yol>`. Proje
başka bir makineye taşındıysa altlık dosyasını proje dosyasının yanına koymanız da yeter;
bir sonraki açılışta adıyla bulunur.

### `io.format_too_new: … en az 4. sürüm biçim okuyucusu istiyor`

**Sebep.** Dosya bir [dış referans](komutlar/xref.md) taşıyor; dış referansın nesneleri
dosyada değil kaynağındadır ve bunu ancak 4. sürüm biçimi okuyan bir KentOSCad bilir.

**Çözüm.** KentOSCad'i güncelleyin. Dosyayı eski sürümde açmak gerekiyorsa, yeni sürümde
`DIŞREFERANS islem=bagla` ile dış referansı çizime katıp kaydedin.

### `io.format_too_new: … en az 5. sürüm biçim okuyucusu istiyor`

**Sebep.** Dosyada [BLOKKIRP](komutlar/block_clip.md) ile kırpılmış bir blok referansı ya
da dış referans var; kırpma sınırını ancak 5. sürüm biçimi okuyan bir KentOSCad bilir.

**Çözüm.** KentOSCad'i güncelleyin. Dosyayı eski sürümde açmak gerekiyorsa, yeni sürümde
kırpmaları `BLOKKIRP islem=kaldir` ile kaldırıp kaydedin.

## Dış veri biçimleri

### `io.no_driver: Dış biçim desteği KAPALI.`

**Sebep.** Bu yapı `KENTOS_WITH_GDAL=OFF` ile derlenmiş.

**Çözüm.** Mesaj kurulum komutunu içerir: Debian/Ubuntu'da
`sudo apt install libgdal-dev`, sonra `-DKENTOS_WITH_GDAL=ON` ile yapılandırın.
`make doctor` durumu özetler.

### `io.no_driver: '...' için sürücü bulunamadı.`

**Sebep.** Dosyanın uzantısı izin listesinde değil. KentOSCad, altındaki kütüphanenin
tanıdığı yüzden fazla biçimin yalnızca açıkça izin verilenlerini açar.

**Çözüm.** `bicim` parametresiyle sürücüyü söyleyin ya da dosyayı desteklenen bir
biçime çevirin. Bkz. [Dış veri biçimleri](veri/dis-formatlar.md).

### `'...' katmanı hiçbir koordinat sistemi bildirmiyor.`

**Sebep.** İçe aktarılan veri kümesi koordinat sistemini bildirmiyor. DXF biçiminin
koordinat sistemi için yeri yoktur.

**Çözüm.** Dosyanın yanına aynı adlı bir `.prj` dosyası koyun. KentOSCad "herhâlde
TUREF/TM30'dur" varsayımı yapmaz: TM30 ile TM33 karışması sessizdir ve ancak tapuya
gittiğinde ortaya çıkar.

### `Çizimin koordinat sistemi '...' dışa aktarım için çözülemedi.`

**Sebep.** Projenin koordinat sistemi ayarı, dışa aktarımın çözebileceği bir kod değil.

**Çözüm.** `AYAR koordinat_sistemi EPSG:5254` gibi bir EPSG kodu verin.

### `'...' sanal dosya sistemi yolu.`

**Sebep.** Yol `/vsicurl/`, `/vsis3/` ya da `/vsizip/` ile başlıyor.

**Çözüm.** Dosyayı diske indirip yerel yolunu verin. Bir veri dosyasının adı ağ
isteğine dönüşemez — komut satırından, betikten ya da yapay zekâ önerisinden gelmiş
olması fark etmez.

### `'...' okunabilir çizgi ya da alan içermiyor`

**Sebep.** Dosyada desteklenen geometri yok.

**Çözüm.** Bu sürüm çizgi ve alan okur; nokta, çoklu nokta ve eğriler okunmaz.
Atlanan öğe sayısı transkriptte söylenir.

### `N öğe geometrisi kullanılamadığı için atlandı`

**Sebep.** Dosya, geometrisi kullanılamayan öğeler taşıyor — bütün noktaları aynı
yerde olan bir çokgen, sıfır uzunlukta bir çizgi gibi. Çizim programlarının
bıraktığı artıklardır.

**Çözüm.** Genelde bir şey yapmanız gerekmez: geri kalan her şey okunmuştur ve
not, kaç öğenin neden atlandığını söyler. Atlanan sayı beklediğinizden çoksa
dosyayı üreten programda bir temizleme (`PURGE`, `OVERKILL`) çalıştırıp yeniden
aktarın.

### `N blok referansının BLOKKIRP sınırı DXF'e taşınmadı; bu referanslar DXF'te kırpılmadan, bütün görünür.`

**Sebep.** DXF'e dışa aktarılan çizimde [kırpılmış](komutlar/block_clip.md) blok
referansları var. AutoCAD kırpmayı bir `SPATIAL_FILTER` nesnesinde tutar ve DXF yazıcısı
onu yazamaz; referanslar DXF'te bütün görünür.

**Çözüm.** DXF'te yalnız görünen kısım isteniyorsa referansı dışa aktarmadan önce
[PATLAT](komutlar/explode.md) ile açıp sınırın dışında kalanı [BUDA](komutlar/trim.md) ile
kesin; ya da AutoCAD'de XCLIP ile yeniden kırpın.

### `Dışa aktarma durduruldu; dosya yazılmadı, '...' olduğu gibi.`

**Sebep.** Dışa aktarım yazılırken durduruldu. (Arayüzden durdurma Faz 1'de, uzun
işlerin Durdur düğmesiyle gelecek.)

**Çözüm.** Bir şey kaybolmadı: dosya hedefin yanında hazırlanıyordu ve yerine konmadı,
eski dosya bayt bayt yerinde. Hazır olduğunuzda komutu yeniden çalıştırın. Bkz.
[Yazma yarıda kalırsa](veri/dis-formatlar.md#yazma-yarıda-kalırsa).

### `'...' yerine tam konamadı. Yerine konan: … Konamayan: …`

**Sebep.** Dosya takımı (DXF ve `.prj`'si, bir görüntü ve world file'ı, bir yerleşimin
sayfaları) hazırlandı, ama içinden biri yerine konamadı: başka bir programda açık
(Windows'ta) ya da yerinde aynı adlı bir klasör var. Mesaj hangi dosyaların yenilendiğini
ve hangilerinin yenilenmediğini tek tek sayar; takım şu an eski ve yeni dosyaların
karışımı olabilir.

**Çözüm.** Dosyaları kullanan programı kapatın (ya da yoldaki klasörü kaldırın) ve
komutu yineleyin; bütün takım yeniden, birlikte yazılır.

## Program çöktüğünde

Program bir çökme anında **yığın izini** standart hata akışına yazar:

```text
[kentos] ÇÖKME. Aşağıdaki yığın izini hata bildirimine ekleyin.
/.../kentos_cad(+0x103373) [0x5d75ec5c2373]
...
```

Bu izi ve o ana kadarki [komut günlüğünüzü](mimari/gunluk.md) birlikte gönderin:
ikisi bir arada, çökmenin nerede olduğunu ve oraya nasıl gelindiğini söyler.
Programı bir uçbirimden başlattıysanız iz doğrudan uçbirime düşer; kısayoldan
başlattıysanız sistem günlüğüne yazılır (Linux'ta `journalctl --user -b`).

## Yardım alamadığınızda

Sorununuzu tarif etmek yerine oturum günlüğünüzü paylaşın: dosya, sorunun ortaya çıktığı
ana kadar yaptığınız her şeyi sırasıyla içerir ve başka bir makinede tekrar oynatılabilir.
Yolu programın açılışında transkriptte yazar. Bkz. [Komut günlüğü](mimari/gunluk.md).
