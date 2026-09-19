# Yapay Zeka Paneli

Yapmak istediği işi Türkçe anlatıp programın komutlarını önermesini isteyen harita
mühendisi ve şehir plancısı için; bu sayfayı bitirdiğinizde paneli açmayı, bir turun
nasıl yürüdüğünü, önerinin nereye düştüğünü, akıl yürütme metninin ne olduğunu ve
bağlam ölçerinin ne ölçtüğünü bileceksiniz.

> Modelin ürettiği her şey **öneridir**: panel hiçbir şeyi kendiliğinden uygulamaz.
> Uygulayan sizsiniz ve karar öneri kartındaki düğmeyle verilir
> ([Onay ve denetim](onay.md)).

## Paneli açmak

Dört yol aynı paneli açar; pencerenin sağında bir yuva olarak gelir:

| Yol | Nerede |
|---|---|
| **Pencere ▸ Yapay Zeka** | Menü çubuğu; yuvayı açıp kapatır |
| **Analiz ▸ Yapay Zeka** | Menü çubuğu |
| **Ctrl+Shift+A** | Klavye |
| **Yapay Zeka** simgesi | Üst araç çubuğu |

Panelin en üstünde **model seçici** vardır ve tanımlı sağlayıcı profillerini listeler;
listeyi `Seçenekler ▸ Yapay Zeka Modelleri` sayfası ya da
[`YAPAYZEKAMODELİ`](../komutlar/ai_provider.md) besler, orada bir şey değişince seçici
kendiliğinden tazelenir. Yanındaki **Yeni sohbet** düğmesi mesajları ve bağlam hesabını
birlikte siler.

Hiç profil tanımlı değilse panel bunu ilk denemede söyler:

```text
Tanımlı bir yapay zeka modeli yok. Seçenekler ▸ Yapay Zeka Modelleri sayfasından bir
sağlayıcı ekleyin ya da YAPAYZEKAMODELİ islem=ekle komutunu kullanın.
```

## Bir tur nasıl yürür

Yazma alanına işi **Türkçe** yazarsınız — **Enter** gönderir, **Shift+Enter** satır
atlar:

```text
1284 ada 7 parseli iki eşit parçaya böl.
```

Sonrası hep aynı sırayla olur ve hiçbir adımı atlanmaz:

1. Modele, sizin cümlenizin yanında **programın kullanım kılavuzu** ve o anda ekranda
   duran alan verilir. Kılavuz, [`llms.txt`](../llms.txt) dosyasının açılış
   paragraflarının ta kendisidir: birimler, eksen adları, tutamak kuralı, uygulama
   kuralı. Dışarıdan bağlanan bir ajanla panelin içindeki model **aynı cümlelerle**
   uyarılır; iki ayrı metin, öğrenilecek iki ayrı program demek olurdu.
2. Model çizime dair bilmesi gerekeni **okuma araçlarıyla** sorar. Hiçbir şeyi
   değiştirmeyen bir araç **hemen çalışır** ve sonucu — ürettiği **tutamaklarla**
   birlikte — modele geri gider. Çizim prompta dökülmez: beş milyon parseli bir metne
   yazmak hem işe yaramaz hem gereksiz bir veri açığıdır.
3. Model yeniden okumak isterse tur devam eder. **En çok sekiz tur**: model hâlâ
   okuyorsa panel durur ve bunu söyler.
4. Model bir **komut dizisi** yazar. Yazdığı şey komuttur, geometri değildir; dizi katı
   bir şemayla denetlenir ve tanınmayan bir araç, tanımlı olmayan bir parametre ya da
   bozuk bir argüman doğrudan reddedilir.
5. Bir turdaki **bütün yazma çağrıları tek bir öneri olur** ve o cevabın balonunun
   altında bir **öneri kartı** olarak görünür.
6. **Siz** uygularsınız ya da reddedersiniz. Karar denetim kaydına yazılır.

Panel model yanıtını beklerken program **çalışmaya devam eder**: çizebilir,
ölçebilir, kaydedebilirsiniz.

Sekiz tur dolduğunda:

```text
Model 8 turdur okumaya devam ediyor; durduruldu. Sorunuzu daha somut yazmayı deneyin.
```

## Dur

**Dur** düğmesi süren turu keser. Yarım kalan cevap konuşmaya hiç eklenmez — model
geçmişi turdan önceki hâliyle kalır — ve panel bunu söyler:

```text
İptal edildi. Çizimde hiçbir şey değişmedi.
```

## Öneri kartı

Kart, kararın verildiği **tek yerdir**. Kesik çizgili bir kenarı ve **ÖNERİ** rozeti
vardır; içinde uygulanacak **komut satırları** olduğu gibi yazılıdır — sizin de
yazabileceğiniz satırlar. Okuyamadığınız bir öneriden sorumlu olamazsınız, bu yüzden
kart bir özet değil satırların kendisini gösterir.

Kartta ayrıca koordinatların hangi **tutamaklardan** geldiği, isteyenin adı ve varsa
modelin kimliği yazılıdır. Kart açıldıktan sonra çizimi değiştirdiyseniz bir uyarı
şeridi çıkar ve iki sürüm numarasını yazar: adımlar hazırlandıkları çizimden başka bir
çizime uygulanacaktır, önce gözden geçirin.

İki düğme vardır: ikincil **Reddet** ve birincil **Uygula**. Altında kartın kendi
hatırlatması durur:

```text
Adımlar tek işlem olarak uygulanır; tek Ctrl+Z ile geri alınır.
```

Ayrıntısı [Onay ve denetim](onay.md) ve [`ÖNERİ`](../komutlar/suggestion.md)
sayfalarındadır.

### Uyguladıktan sonra iş devam eder

**Uygula** dediğinizde konuşma kaldığı yerden sürer. Modele ne olduğu anlatılır —
önerinin durumu, çizimin yeni sürümü, yazılan dosyalar ve varsa uyarılar — ve
değişikliği **doğrulaması**, iş bittiyse bittiğini söylemesi istenir.

Sebebi şu: bir iş çoğu zaman tek adım değildir. "Şu adanın paftasını çıkar" katman
kurmayı, nesneleri yerleştirmeyi, yerleşimi kurmayı ve PDF'i basmayı gerektirir.
Eskiden konuşma **ilk kartta bitiyordu**: siz Uygula'ya basıyordunuz ve hiçbir şey
olmuyordu; kalan her adımı yeniden istemek zorundaydınız.

**Bu otomatik uygulama değildir.** Burada hiçbir şey uygulanmaz — kararı siz zaten
verdiniz — devam eden **konuşmadır**. Modelin bundan sonra isteyeceği her çizim adımı
yine bir öneri ve yine bir kart olur.

**Reddet** dediğinizde tur harcanmaz. Ret konuşmaya yazılır — model bir sonraki
mesajınızda görür — ama model o anda söz almaz: az önce hayır demiş birine cevap
vermek, kararla tartışmaktır.

Tur sınırı bir onayla sıfırlanmaz: turunu tüketmiş bir iş orada durur ve durduğunu
söyler.

### Tur sınırı

Bir soru için modelin en çok kaç tur okuma aracı çalıştırabileceğini
`Seçenekler ▸ Yapay Zeka Modelleri` sayfasındaki **tur sınırı** belirler (1–50,
öntanımlı 8). Sınıra gelince panel durur ve durduğunu **söyler**; yapılmış işler
kaybolmaz.

Bu bir **yetki** ayarıdır: modelin kendisi değiştiremez. Sınıra çarpıp onu yükselten
bir model, reddedildiği turları kendine vermiş olurdu ([Onay ve denetim](onay.md)).

### Durdurunca

**Dur**, o anda uçan turu iptal eder. İptal edilen tur çizimde hiçbir şey değiştirmez —
ama **bu konuşmada daha önce uyguladığınız öneriler çizimde durur**, ve mesaj bunu adıyla
söyler:

```text
İptal edildi. Bu turda hiçbir şey uygulanmadı — ama bu konuşmada daha önce
uyguladığınız öneriler çizimde duruyor: p0f3a1c7b9e4d2856. Geri almak için Ctrl+Z.
```

Eskiden yalnız "çizimde hiçbir şey değişmedi" yazıyordu; bir öneriyi uyguladıktan hemen
sonra Dur'a basmak en olası andır ve o cümle orada **yanlıştı**.

## Bir mesaj neyden oluşur

Sohbetteki her mesaj **parçalardan** oluşur, düz bir metinden değil:

| Parça | Ne taşır |
|---|---|
| Metin | Okuduğunuz cevap |
| Akıl yürütme | Modelin düşünme metni ya da onun özeti |
| Araç çağrısı | Modelin istediği araç ve yazdığı argümanlar, **olduğu gibi** |
| Araç sonucu | Çağrının ne döndürdüğü, modele geri giderken |

Araç çağrısının argümanları modelin yazdığı hâliyle saklanır — reddedilmiş, bozuk bir
istek de dahil — çünkü denetim kaydının göstermesi gereken şey **ne istendiğidir**.

**Akıl yürütme metni yalnız `core.ai.dusunme_goster` ayarı açıkken gösterilir**, o zaman
da katlanabilir bir blokta. Ayar kapalıyken model düşünürken üç nokta ve geçen saniye
sayısı görünür. Akıl yürütme **cevap değildir**: dört lehçenin üçü onu geri kabul
etmez, dolayısıyla bir sonraki tura yalnız cevabın kendisi gider.

## Dosya ekleme

**Dosya ekle** düğmesi bir mesaja metin, görüntü ya da PDF ekler
(`.txt .csv .json .md .png .jpg .jpeg .pdf`). Eklenen dosyalar yazma alanının üstünde
sıralanır.

En büyük dosya **8 MB**'dır; daha büyüğü reddedilir ve panel sayıyı söyler:

```text
Dosya çok büyük: 12.4 MB. En çok 8.0 MB eklenebilir.
```

Eklenen dosya **çizime girmez**. Bir model, eklediğiniz bir dosyadaki koordinatı çizime
yazdıramaz: konum kuralı burada da geçerlidir — nokta isteyen bir parametre yalnız bir
okuma aracının ürettiği tutamağı kabul eder ([Onay ve denetim](onay.md)).

## Bağlam ölçeri

Ölçer, turun ne kadar yer kapladığını gösterir ve iki kaynaktan beslenir:

- Akış sürerken programın **kendi tahmini**: dört bayta bir jeton. Türkçede `ğ`, `ı`,
  `ş` ve büyük harfleri UTF-8'de iki bayt tuttuğu için tahmin **yüksekten** sayar —
  düşük sayan bir ölçer, turun sağlayıcıda patlamasına izin veren ölçerdir.
- Tur bittiğinde sağlayıcının **bildirdiği** sayı. O geldiğinde tahmin atılır, ikisinin
  ortalaması alınmaz: ölçülmüş bir sayı ile tahmin edilmiş bir sayı aynı toplamda
  durmaz.

Yüzde olarak doluluk, ancak seçili profilin **bağlam penceresi biliniyorsa** gösterilir.
Bulut uçlarının çoğunda bilinmez ve panel bunu gizlemez
([Model sağlayıcıları](modeller.md)).

## Öneri, onay değildir

Panelin ürettiği her şey **öneridir** ve her durumda öyle etiketlenir: yüklenirken,
hata verirken, yarım kalırken.

Program bir yapay zeka çıktısını hiçbir yerde "onaylandı", "kontrol edildi", "uygundur",
"mevzuata uygundur" ya da "imzalandı" diye göstermez. Bu sözcükler kod düzeyinde
yasaklıdır ve bir CI kapısı panelde onları arar. Bir öneri metni imza yerine geçmez,
denetim yerine geçmez ve mevzuat uygunluğu beyanı değildir.

Bir de panelin **yapmadığı** bir şey vardır: bir önerinin **sonucunu** tuvalde hayalet
bir önizleme olarak göstermek. O, sonraki bir fazın işidir (`.claude/ai.md` R3,
`.claude/render.md`); bugün kartta gördüğünüz şey uygulanacak satırlardır.

## Aynı iş, komut satırından

Panel komut veri yolunun bir istemcisidir; ayrıcalığı yoktur. Modelin ilk soracağı üç
soruyu kendiniz de sorabilirsiniz:

```
KATMANLAR
SORGULA katman=PARSEL
GÖRÜNÜMBİLGİSİ
```

Bekleyen önerileri [`ÖNERİ islem=listele`](../komutlar/suggestion.md) yazar, dış bir
ajanı [MCP sunucusu](mcp-sunucusu.md) bağlar.

## İlgili

- [Onay ve denetim](onay.md) — öneri boru hattı, tutamaklar, denetim kaydı
- [Model sağlayıcıları](modeller.md) — hangi model, hangi adres, anahtar nerede
- [MCP sunucusu](mcp-sunucusu.md) — dış bir ajanı bağlama
- [`ÖNERİ`](../komutlar/suggestion.md) — bekleyen önerileri komut satırından okuma
- [Bileşenler](../baslangic/bilesenler.md) — sohbet bileşenlerinin durumları ve klavyesi
- [Yapay zeka ve ajanlar](README.md) — dört kural ve okuma araçları
