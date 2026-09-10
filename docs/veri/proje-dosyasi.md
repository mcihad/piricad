# KentOSCad Proje Dosyası (`.pcad`)

Çizimini kaydeden, arşivleyen ya da başka bir kuruma teslim eden kullanıcı için;
bu sayfayı bitirdiğinizde `.pcad` dosyasının içinde ne olduğunu, neyin
korunduğunu, neyin korunmadığını ve bir dosya bozulduğunda KentOSCad'in ne
söyleyeceğini bileceksiniz.

Kaydetme ve açma komutları için: [AÇ](../komutlar/open.md),
[KAYDET](../komutlar/save.md), [FARKLIKAYDET](../komutlar/saveas.md).

## Ne işe yarar

`.pcad`, KentOSCad'in kendi proje dosyasıdır. Çizimin tamamını **kayıpsız** taşır:
geometri, katmanlar, stiller, nesne kimlikleri ve proje ayarları. DXF ya da
GeoPackage'a dışa aktarmak her zaman bir şeyler kaybeder — proje dosyası
kaybetmez. Çalışmanızı `.pcad` olarak saklayın, dış biçimleri teslim için
kullanın.

## Dosyanın içinde ne var

| Taşınan | Neden |
|---|---|
| Koordinat sistemi ve katalog paket sürümü | Hangi mevzuata göre çizildiği bilinmeyen bir belge sessizce açılmamalıdır |
| Katmanlar: ad, açıklama, görünürlük, kilit, renk, ölçek sınırları, saydamlık | Katman kaydının saklanan her alanı |
| Stil tablosu | Nesne başına renk, kalınlık, tarama, dolgu ve çizim sırası |
| Nesne satırları: sınırlayıcı kutu, bayraklar, katman, stil, tür, geometri yuvası | Ekranın ilk karesi dosyadan gelir, yeniden hesaplanmaz |
| **Nesne ve katman anahtarları** | "Bu parsel hangisiydi?" hukuki bir sorudur; anahtar kalıcıdır ve asla yeniden kullanılmaz |
| Silinmiş nesnelerin satırları | Silinen bir nesnenin anahtarı boşta kalır; boşluk korunmazsa o anahtar başka bir parsele verilir |
| Halka geometrisi: tepe noktaları, halka rolleri, parça numaraları | Boşluklu ve çok parçalı parsel — yola terk ve irtifak bunu rutin olarak üretir |
| **Blok tanımları**: ad, açıklama, taban noktası, üye nesnelerin anahtarları | Bir kez çizilip çok kez yerleştirilen sembol (rögar kapağı, kuzey oku, antet). Tanımın nesneleri aynı nesne tablosunda "blok içinde" bayrağıyla durur; kendi başlarına çizilmez, seçilmez, düzenlenmez — yerleştiren blok referansı (Faz 2) çizer |
| **Yabancı veri**: başka bir programın nesneye bağladığı baytlar (DXF XDATA) | Bu program okuyamaz ama kaybedemez: dosya geldiği baytlarla geri gider. Öznitelik paneli yalnız sayısını gösterir ("ek_veri: 2 kayıt") |
| **Tür yükü**: bir nesne türünün halkalarının söyleyemediğini taşıyan baytlar | Yaylı çoklu çizginin yayları, spline'ın düğümleri, taramanın deseni, blok referansının dönüşümü, ölçünün sayıları buradadır; yükü olmayan bir çizim bu sütunları hiç yazmaz. Bu sürümün **tanımadığı bir tür** de bu yolla korunur: nesne görünür, halkaları ve yükü bayt bayt aynı kalır, düzenlenmeye kalkışılırsa "Bu yapının tanımadığı türdeki nesne düzenlenemez; olduğu gibi korunur." denir |
| **Proje kapsamlı ayarlar** | Dışa aktarılan belgenin baytını değiştirebilen her ayar |

Taşınmayan, kasten:

| Taşınmayan | Neden |
|---|---|
| Uygulama tercihleri (tema, yazı boyu, otomatik kaydetme aralığı) | Kullanıcıya ve makineye özeldir; başkasının bilgisayarında sizin temanız açılmamalı |
| Oturum modları (yakalama, dik mod, kutupsal izleme) | Geçicidir; program kapanınca biter |
| Seçim, görünüm, aktif katman, komut geçmişi | Belge durumu değildir |
| Geri alma yığını | Dosya açıldığında geri alınacak bir şey yoktur |

Bu ayrım bir alışkanlık değil, kuraldır: **dışa aktarılan bir belgenin baytını
değiştirebilen her ayar proje kapsamındadır**, gerisi değildir.

## Koordinatlar

Dosyadaki her koordinat **tam sayı milimetredir** (`int64`). Dosyada hiçbir yerde
ondalıklı sayı yoktur — ne koordinatta, ne çizgi kalınlığında, ne açıda, ne
ölçekte.

Sebebi tek cümlede: aynı çizim Linux, Windows ve macOS'ta **aynı baytları**
üretmeli. Ondalıklı sayı bunu vaat edemez; `485320.150` metre bir makinede
`485320.14999999998` olarak yuvarlanırsa alan hesabı da o kadar kayar ve alan
hesabı imzalanan çıktıdır.

Ayrıntı: [Koordinat sistemleri](koordinat-sistemleri.md).

## Sürümler ve uyumluluk

Dosyanın ilk 32 baytı üç şey söyler: bunun bir KentOSCad dosyası olduğu, hangi
sürümün yazdığı ve **okumak için en az hangi sürümün gerektiği**.

| Durum | KentOSCad ne yapar |
|---|---|
| Dosya bu sürümün yazdığından eski | Açar. Eski dosyalar açılmaya devam eder |
| Dosyada tanımadığı bir veri bloğu var | Açar, bloğu atlar ve size kaç blok atladığını söyler |
| Dosya daha yeni bir okuyucu istiyor | **Açmaz.** Gereken sürümü söyler ve yarım yüklemez |

Üçüncü satır önemlidir: yarım açılmış bir proje, açılmamış bir projeden çok daha
tehlikelidir. KentOSCad ya tamamını okur ya da hiçbirini.

Yeni bir özellik geldiğinde dosyaya yeni bir blok eklenir; eski KentOSCad o bloğu
atlayarak dosyayı açmaya devam eder. Gereken okuyucu sürümü ancak var olan bir
bloğun **anlamı** değişirse yükselir, ve bu bugüne kadar olmadı.

## Kaydetme kesintiye dayanıklıdır

`KAYDET` önce yanına geçici bir dosya yazar, ancak son bayt diske indikten sonra
onu yerine koyar. Kaydetme sırasında elektrik giderse, disk dolarsa ya da program
kapanırsa **bir önceki kaydınız yerinde durur**. Yarım yazılmış bir proje dosyası
bırakılmaz.

## Dosya bozulursa

KentOSCad dosyadaki hiçbir sayıya güvenmez: her uzunluk, her konum ve her sayaç
dosyanın gerçek boyutuna karşı denetlenir. Bozuk bir dosya **çökme değil, hata
mesajı** üretir, ve mesaj neyin bozuk olduğunu söyler.

KentOSCad bozuk bir dosyayı **kendiliğinden onarmaz**. Onarım ayrı ve açıkça
istenen bir iştir; sessizce "düzeltilmiş" bir kadastro dosyası, bozuk olduğu
bilinen bir dosyadan kötüdür.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `io.not_a_project: '...' bir KentOSCad proje dosyası değil.` | Dosya `.pcad` değil ya da başka bir program yazmış | Dış biçimler için [İÇEAKTAR](../komutlar/import.md) kullanın |
| `io.format_too_new: '...' en az N. sürüm biçim okuyucusu istiyor` | Dosyayı daha yeni bir KentOSCad yazmış | Mesajdaki sürüme yükseltin |
| `io.truncated: '...' N bayt olduğunu bildiriyor, ama M bayt.` | Dosya yarım kopyalanmış ya da kesilmiş | Yedeğinden geri alın; kopyalamayı yeniden yapın |
| `io.bad_block: ... 8 baytlık hizaya oturmuyor. Dosya bozuk.` | Dosyanın iç yerleşimi bozulmuş | Yedeğinden geri alın |
| `io.inconsistent: ... sütunu N öğe taşıyor, belge kaydı M bildiriyor.` | Dosyanın iki yeri birbirini tutmuyor | Yedeğinden geri alın |
| `io.key_mismatch: nesne anahtarları artan sırada değil` | Nesne kimlik düzeni bozulmuş | Yedeğinden geri alın; bu dosya güvenilir değil |
| `io.unknown_kind: ... 65535 numaralı türde; bu değer 'tür yok' anlamına ayrılmıştır` | Tür sütununa ayrılmış değer yazılmış | Yedeğinden geri alın; tanınmayan bir tür bu hatayı vermez, korunarak açılır |
| `Proje dosyası yalnız boş bir belgeye okunabilir.` | Var olan bir çizimin üzerine proje okunmaya çalışıldı | Açmak için `AÇ`, eklemek için `İÇEAKTAR` |
| `'...' dizini yok. Önce dizini oluşturun ya da başka bir yol seçin.` | Kaydedilecek klasör yok | Klasörü oluşturun |
| `'...' yazılırken hata oluştu; disk dolu olabilir.` | Disk doldu ya da izin yok | Yer açın; önceki dosyanız değişmedi |

Uyarılar hata değildir ve dosya yine açılır; transkriptte `uyarı:` ile başlarlar.
En sık görüleni, dosyanın bu sürümün tanımadığı bir blok taşımasıdır.

## Bu sürümde henüz olmayanlar

Aşağıdakiler dosya biçiminde **yer ayrılmış** ama Faz 1'de doldurulacak:

- **Önceden hesaplanmış genelleştirme (LOD) kademeleri.** Bugün ekran her karede
  tam geometriyi çizer. Faz 1'de dosya, dörtlü ağaç karolarına yazılmış 4–5
  kademe taşıyacak ve uzaklaşmış görünüm bunları okuyacak.
- **Önceden kurulmuş alansal dizin (R-ağacı).** Bugün dizin belge açılırken
  bellekte kurulur. Faz 1'de dosyadan okunacak.
- **Katman açıklaması, çizdirilebilirlik, ölçek sınırları, saydamlık ve katalog
  künyesi.** Dosya bunları yazar ve okur; bu sürümde bu alanları değiştirebilen
  bir komut yok, dolayısıyla varsayılan dışında bir değer taşıyan bir dosya
  açılırken uyarı verir.

Bunların gerekçesi ve takibi `CLAUDE.md` Article 8 ile `.claude/io.md` R6'dadır.

## Teknik yerleşim

Bu bölüm dosyayı kendi araçlarıyla okumak isteyenler içindir; günlük kullanım
için gerekmez.

Dosya küçük-endian, 8 bayt hizalı ve üç parçadan oluşur:

```text
[ 0                ) başlık, 64 bayt
[ 64               ) veri blokları, her biri 8 bayt hizalı
[ dizin_konumu     ) blok dizini, blok başına 32 bayt
```

Başlık sırayla: 8 baytlık `PIRICAD\x1A` imzası (ürünün adı değişse de imza değişmez: değişseydi o ana kadar yazılmış her dosya okunamaz olurdu), yazan sürüm, gereken en düşük
okuyucu sürümü, başlık uzunluğu, blok sayısı, dizin konumu, toplam dosya boyu,
içerik parmak izi ve proje ayarları parmak izi.

Her dizin girdisi bir bloğun kimliğini, öğe boyunu, konumunu, uzunluğunu ve öğe
sayısını söyler. Bloklar **sütun** hâlindedir: X'ler ayrı, Y'ler ayrı, halka
başlangıçları ayrı. Bu sayede dosya belleğe eşlenip geometri hiç ayrıştırılmadan
doğrudan kullanılabilir.

Tanımadığınız bir blok kimliğini uzunluğuna bakarak atlayın; biçim bunu
kasten böyle tasarlandı.

Blok kimliklerinin tam listesi ve kayıt yerleşimleri
`src/io/include/kentos_cad/io/format.hpp` dosyasındadır ve bu sayfayla aynı anda
güncellenir.
