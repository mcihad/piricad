# Bağımlılıklar ve sonuçlar

Çizimde bir nesneyi başka bir nesneden yapan herkes için; bu sayfayı bitirdiğinizde hangi
nesnenin kaynağı değişince kendini yenilediğini, hangisinin yalnız "güncel değil" dediğini,
hangisinin bundan hiç etkilenmediğini ve bir kaynak silinince ne olduğunu bileceksiniz.

## Kim neye bağlı

Bir nesne başka bir nesneye dört biçimde bağlı olabilir. Hepsi kaynağı **kimliğiyle**
bilir, konumuyla değil; bu yüzden kaynak taşınınca, döndürülünce, kaydedilip açılınca
bağ kopmaz ([nesne kimliği](kimlik-ve-koken.md)).

| Bağımlı | Kaynağı | Kaynak değişince | Kaynak silinince |
|---|---|---|---|
| **Bağlı yazı** — [UZUNLUKYAZ](../komutlar/uzunluk_yaz.md), [KÖŞENUMARALA](../komutlar/kose_numarala.md), [ETİKET](../komutlar/label.md), [BAĞLA](../komutlar/bagla.md) | Bir kenar, köşe ya da nesnenin ortası | **İzler**: yeniden yerleşir, sayısı ya da kalıbı yeniden yazılır | Yazı da silinir (kilitliyse kalır, bağı kopuk olur) |
| **Bağlı ölçü** — [ÖLÇÜ](../komutlar/dimension.md) | Bir köşe, merkez, yay ucu | **İzler**: yeniden yerleşir ve yeniden ölçülür | Bağ kopar: ölçü yerinde durur, tuvalde **bağ koptu** |
| **Bağlı tarama** — [TARAMA](../komutlar/hatch.md) | Sınırını veren nesneler | **İzler**: sınırdan yeniden kurulur | Bağ kopar: tarama son hâlinde durur, tuvalde **sınır bağı koptu** |
| **Sonuç** — [TAMPON](../komutlar/tampon.md), [ALANÜRET](../komutlar/alan_uret.md), [SINIR](../komutlar/boundary.md), [EŞYÜKSELTİ](../komutlar/contour.md) | Hesaplandığı nesneler | **Güncel değil** olur ve bunu söyler; kendi kendine yeniden hesaplanmaz, istenince hesaplanır | **Kaynaksız** olur; son hâlinde kendi başına durur |

Bir de bağ olmayan ilişki vardır: **köken**. Bir kopya, bir budamanın parçası, bir
ifrazın parseli hangi nesneden yapıldığını bilir, ama o nesne hakkında bir şey söylemez;
kaynağı değişince ona hiçbir şey olmaz. Köken geçmiştir.

Bağlı yazının ayrıntısı [Bağlı nesneler](../islem/bagli-nesneler.md) sayfasındadır.

## Kilitli katmanda

Kilitli bir katmandaki bağlı yazı, ölçü ya da tarama **düzenlenemez**, bu yüzden kaynağı
değişince onu izleyemez: yerinde kalır ve bu söylenir (`Bağlı 2 yazı kilitli katmanda
olduğu için kaynağını izleyemedi; yerinde kaldı. Katmanın kilidi açılınca kaynağına
yetişir.`). Bundan sonra da **geride olduğunu söyler**: tuvalde uyarı renkli **kilitli:
kaynağının gerisinde** işareti, öznitelik panelinde **GÜNCEL DEĞİL** rozeti,
[BAĞIMLILIK](../komutlar/dependency.md)'te `güncel değil` satırı.

**Bağ yine de doğru köşeyi izler.** Kilitliyken parsele köşe eklenir ya da bir köşesi
silinirse bağın kendisi — hangi kenara, hangi köşeye bağlı olduğu — kaynağıyla birlikte
yeniden numaralanır; yazının ya da ölçünün yeri ve sözü değişmez.

**Kilit açıldığı anda yetişir.** Katmanın kilidini açan komut
(`KATMAN ad=OLCU kilitli=hayır`, katman panelindeki kilit düğmesi) geride kalanları aynı
geri alma adımında kaynağına yetiştirir: `Kilidi açılan 2 bağlı nesne kaynağına yetişti.`
Kilidi geri almak ikisini birlikte geri alır.

## Sonuç ne zaman güncel değildir

Bir sonuç hesaplandığı anda her kaynağının **içeriğini** kaydeder: şeklini (köşeleri,
eğrinin tanımı), yazısını ve değerlerini. Sonradan bu içerikten biri değişirse sonuç
**güncel değildir**:

| Kaynakta değişen | Sonuç |
|---|---|
| Köşe, yer, dönüklük, ölçek — [TAŞI](../komutlar/move.md), [KÖŞETAŞI](../komutlar/vertex_move.md), tutamak | Güncel değil |
| Bir değeri — [ÖZNİTELİK](../komutlar/attribute.md), tablodan yazmak; örneğin bir noktanın `kot`'u | Güncel değil |
| Yazısı | Güncel değil |
| Katmanı, rengi, stili, görünürlüğü | Güncel kalır: bir nesneyi başka türlü çizmek hiçbir tamponu değiştirmez |
| Çizime yeni bir sütun eklenmesi | Güncel kalır |

**Durum saklanmaz, her seferinde çizimden okunur.** Bu yüzden:

- Kaynağı değiştiren işi **geri aldığınızda** sonuç yeniden güncel olur.
- Kaynağı başka bir komutla **eski hâline** getirdiğinizde de güncel olur.
- Kaydedip açtığınızda sonuç aynı durumda açılır; kaynağın o günkü içeriği dosyadadır.

Güncel olmayan sonuç üç yerde görünür: kaynağı değiştiren komut bittiği anda komut
satırında (`Kaynağı değiştiği için 1 sonuç artık güncel değil (TAMPON).`), tuvalde
sonucun üstündeki uyarı renkli **güncel değil** işaretinde ve öznitelik panelinde `koken`
satırının **GÜNCEL DEĞİL** rozetinde. Çizimin bütününe [BAĞIMLILIK](../komutlar/dependency.md)
bakar; hangi sonucun hangi kaynağı değiştiği için güncel olmadığını yazar.

**Yalnız etkilenen sonuçlar sorulur.** Bir komut bir kuyuyu taşıdığında yalnız o kuyudan
yapılan sonuçlar kaynaklarıyla karşılaştırılır; çizimdeki öbür sonuçlar için hiçbir şey
hesaplanmaz.

Bir sonucun durumu dörtten biridir: **güncel**, **güncel değil**, **kaynaksız**
(kaynağı silindi, başka bir şey değişmedi) ya da **geçmiş** (sonuç değil ya da
kaynağından çözülmüş).

## Sonucun kendisi değişince

| Ne yaptınız | Sonuç |
|---|---|
| Sonucu **kaynaklarıyla birlikte** taşıdınız, döndürdünüz (kuyuyu ve koruma alanını aynı seçimde) | Güncel kalır; kaynakların yeni hâli kaydedilir |
| Sonucun **kendisini** kaynağından ayrı değiştirdiniz (koruma alanının bir köşesini çektiniz) | Kaynağından çözülür: artık hesaplanan sonuç değil sizin nesnenizdir. Kökeni geçmiş olarak kalır |

Bu, bağlı ölçünün ve taramanın davranışıyla aynıdır: tanım noktasını elle çektiğiniz ölçü
ve sınırından ayrı taşıdığınız tarama da bağından çözülür. Çözülme komut satırında
söylenir ve komutla birlikte geri alınır.

## Güncel olmayanla ne yapılır

[BAĞIMLILIK](../komutlar/dependency.md) üç karar verir:

- `islem=yenile` — geride kalan bağlı yazı, ölçü ya da tarama kaynağına yetişir; güncel
  olmayan sonuç **yeniden hesaplanır**.
- `islem=kabul` — sonuç olduğu gibi doğrudur; kaynaklarının şimdiki hâli kaydedilir.
- `islem=coz` — bağ kalkar: bağlı nesne yerinde durup kaynağını artık izlemez, sonuç
  kendi başına bir nesne olur; kökeni kalır, güncel olup olmadığı bir daha sorulmaz.

**Yeniden hesaplamak** sonucu üreten komutu, ilk çalıştırıldığı değerlerle (tamponun
mesafesi, eş yükseltinin aralığı, sınırın tıklanan noktası) ve kaynaklarının şimdiki
hâliyle yeniden çalıştırır; sonuç her zaman nasıl hesaplandığını kökeninde taşır.

| Sonuç | Yeniden hesaplanınca |
|---|---|
| Tek nesne: bir koruma alanı, bir sınır alanı, bir üretilen alan | **Yerinde** yenilenir: kimliği, değerleri, stili ve ona bağlı yazılar kalır; yalnız biçimi değişir. Ona bağlı yazı yeni biçimin en yakın kenarına yetişir |
| Birden çok nesne: bir çalışmanın eş yükselti eğrileri | Eskiler silinir — onlara bağlı yazılar da —, yenileri aynı katmana çizilir |

Yeniden hesaplamanın bütün işleri tek geri alma adımıdır. Üreten komut yeni kaynaklarla
çalışamazsa (çizgiler artık bir alan kapatmıyorsa) hiçbir şey değişmez ve sebebi söylenir.

## Zincir ve döngü

**Bir sonuçtan yapılan sonuç yalnız kendi kaynağına bakar.** Bir koruma alanının
tamponunu çizdiyseniz ve kuyu taşınırsa yalnız ilk koruma alanı güncel değil olur: ikinci
tampon ilk koruma alanından yapılmıştır ve o değişmemiştir. İlk koruma alanını yeniden
hesaplayıp değiştirdiğinizde ikincisi de güncel değil olur.

**Döngü kurulamaz.** Her bağ türü için:

| Bağ | Kural |
|---|---|
| Bağlı yazı | Bir yazıyı izleyen yazı olabilir (zincir), ama bir yazı kendisine ya da onu izleyenlerden birine bağlanamaz: [BAĞLA](../komutlar/bagla.md) `Bağ döngüsü` diye reddeder |
| Bağlı ölçü | Yalnız çizgiyi, çoklu çizgiyi, noktayı, yayı ve daireyi ölçer; başka bir ölçüye, taramaya ya da yazıya bağlanmaz, bu yüzden döngü kurulamaz |
| Bağlı tarama | Bir tarama kendi sınırı olamaz; sınırı, dolaylı olarak kendisinden doldurulan bir tarama da olamaz (`Tarama bağ döngüsü`) |
| Sonuç | Bir sonuç yalnız kendinden önce var olan nesnelerden yapılır; bir nesne kendi kökeni olamaz |

## Dosyada

Sonuçların kaynakları ve kaynakların içeriği [proje dosyasının](proje-dosyasi.md) kendi
bloklarında saklanır; bir çalışmanın bütün çıktıları — örneğin bir EŞYÜKSELTİ'nin bütün
eğrileri — kaynaklarını bir kez yazar. Sonucu olmayan bir çizim bu blokları hiç yazmaz.
Bu blokları tanımayan eski bir sürüm dosyayı açar; yalnız sonuçların güncel olup
olmadığını bilemez.

## Hata mesajları

| Mesaj | Neden | Çözüm |
|---|---|---|
| `Bir sonucun her kaynağının bir sürümü olmalı: …` | Bir sonucun kökeni, kaynak sayısıyla kaynak içeriği sayısı uyuşmadan yazılmak istendi; programın bir iç hatasıdır | Hatayı, onu doğuran komutla birlikte bildirin; çizim değişmeden kalır |
| `Bağ döngüsü: …` | Bir yazı, kendisini dolaylı olarak izleyen bir yazıya bağlanmak istendi | Zinciri öbür yönde kurun ya da bağlardan birini çözün |
| `Tarama bağ döngüsü: …` | Bir taramaya, dolaylı olarak kendisinden doldurulan bir tarama sınır verilmek istendi | Sınır için taramanın kendisini değil parseli seçin |

## Henüz gelmemiş olanlar

| Yetenek | Ne zaman |
|---|---|
| EŞYÜKSELTİ'den sonra çizime eklenen yeni bir kotlu noktanın eğrileri güncel değil yapması (bugün eğriler yalnız hesaplandıkları noktaları bilir) | Faz 1, kalıcı arazi yüzeyiyle |
| Pafta tablosunun, grafiğinin ve lejantının kopan bağlarının (silinen ya da adı değişen katman) çizimde görünmesi | Faz 1 |

## İlgili

- [BAĞIMLILIK](../komutlar/dependency.md)
- [Nesne kimliği ve kökeni](kimlik-ve-koken.md)
- [Bağlı nesneler](../islem/bagli-nesneler.md)
