# BAĞIMLILIK — Bağlar ve Sonuçlar Güncel mi

Bir kuyunun koruma alanını çizip sonra kuyunun yerini düzelttiğinizde, uzunluk yazılarının
katmanını kilitleyip parselin köşesini oynattığınızda ya da kotlu noktalardan eş yükselti
eğrilerini çizip bir kotu yeniden okuduğunuzda. Bu sayfayı bitirdiğinizde hangi nesnenin
kaynağıyla artık uyuşmadığını görecek, bağlı olanı kaynağına yetiştirecek, bir sonucu
şimdiki hâliyle kabul edecek ya da bağından çözebileceksiniz.

## Ne yapar

Çizimde iki tür bağımlı nesne vardır:

- **Bağlı nesneler** kaynaklarını izler: [UZUNLUKYAZ](uzunluk_yaz.md) ve
  [KÖŞENUMARALA](kose_numarala.md)'nın yazıları kenarlarını ve köşelerini, bağlı bir
  [ÖLÇÜ](dimension.md) ölçtüğü köşeyi, bağlı bir [TARAMA](hatch.md) sınırını. Kaynak
  değişince kendileri de değişir — katmanları **kilitli** değilse.
- **Sonuçlar** kaynakları hakkında bir cümledir: [TAMPON](tampon.md)'un koruma alanı,
  [ALANÜRET](alan_uret.md)'in alanları, [SINIR](boundary.md)'ın alanı,
  [EŞYÜKSELTİ](contour.md)'nin eğrileri. Hesaplandıkları anda kaynaklarının ne olduğunu
  kaydederler; kaynak değişince kendiliğinden yeniden hesaplanmazlar ama **güncel
  olmadıklarını söylerler**.

Paftalar da sayılır: bir tablonun, grafiğin, haritanın ya da atlasın adıyla okuduğu
katman ya da sütun çizimde yoksa o **pafta bağı kopuktur** ([Pafta](../veri/bagimliliklar.md#pafta)).

Kaynağıyla uyuşmayan bir nesne üç yerde görünür: kaynağı değiştiren komut bittiği anda
komut satırında, tuvalde uyarı renkli işaretle (**güncel değil**, kilitli bir bağlı nesne
için **kilitli: kaynağının gerisinde**) ve öznitelik panelinde **GÜNCEL DEĞİL** rozetiyle.
BAĞIMLILIK çizimin bütününü ya da verdiğiniz nesneleri sayar ve hangisinin hangi kaynağı
yüzünden güncel olmadığını yazar.

Bir bağımlı nesnenin durumu şunlardan biridir:

| Durum | Anlamı |
|---|---|
| Güncel | Kaynaklarının söylediğini söylüyor |
| Güncel değil | Bir kaynağı değişti ve o izleyemedi (kilitliydi) ya da bir sonuç kaynağından eski |
| Bağı kopuk | Bağlı bir ölçünün ya da taramanın kaynağı silindi; son hâlinde duruyor, bir şey izlemiyor |
| Kaynaksız | Bir sonucun kaynağı silindi, başka bir şey değişmedi; kendi başına duruyor |

Güncel olmayan için verilecek kararlar:

- **Yenile** (`islem=yenile`): bağlı nesne kaynağına yetişir — yazı kenarına yerleşir ve
  sayısını yeniden yazar, ölçü köşesine oturup yeniden ölçer, tarama sınırından yeniden
  kurulur. Kilitli katmandaki nesneye dokunulmaz; kilit açıldığı anda o zaten
  kendiliğinden yetişir. Güncel olmayan bir **sonuç** ise **yeniden hesaplanır**: onu
  üreten komut, ilk çalıştırıldığı değerlerle, kaynaklarının şimdiki hâli üzerinde yeniden
  çalışır. Tek nesnelik sonuç (bir koruma alanı, bir sınır alanı) **yerinde** yenilenir:
  kimliği, değerleri, stili ve ona bağlı yazılar kalır, yalnız biçimi değişir. Birden çok
  nesnelik sonucun (bir çalışmanın eş yükselti eğrileri) eskileri silinir, yenileri aynı
  katmana çizilir.
- **Kabul** (`islem=kabul`): sonuç olduğu gibi doğrudur; kaynaklarının şimdiki hâli
  kaydedilir ve sonuç yeniden güncel sayılır.
- **Çöz** (`islem=coz`): bağ kalkar. Bağlı nesne yerinde kalır ve kaynağını artık izlemez;
  sonucun kökeni geçmiş olarak kalır, güncel olup olmadığı bir daha sorulmaz.

**Durum saklanmaz, çizimden okunur.** Kaynağı değiştiren işi geri aldığınızda bağımlı
nesne yeniden güncel olur; kaynağı ayrı bir komutla eski hâline getirdiğinizde de. Ayrıntı:
[Bağımlılıklar ve sonuçlar](../veri/bagimliliklar.md).

## Adlar

| Ad | Tür |
|---|---|
| `BAĞIMLILIK` | Türkçe ana ad |
| `BAGIMLILIK` | ASCII |
| `DEPENDENCY` | İngilizce |
| `BĞM` | Kısaltma |

## Sözdizimi

```text
BAĞIMLILIK [islem=durum|yenile|kabul|coz] [nesneler=<kimlik…>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `islem` | `durum` (varsayılan): bağlı nesneleri ve sonuçları sayar, güncel olmayanları yazar. `yenile`: geride kalan bağlı nesneleri kaynağına yetiştirir, güncel olmayan sonuçları yeniden hesaplar. `kabul`: sonucun kaynaklarını şimdiki hâliyle kaydeder. `coz`: bağlı nesnenin bağını kaldırır, sonucu kaynağından çözer |
| `nesneler` | Sorulacak nesneler. Verilmezse `durum` çizimdeki bütün bağlı nesnelere ve sonuçlara, `yenile`, `kabul` ve `coz` güncel olmayanlara bakar |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Bir kuyu ve onun 5 m koruma alanı; sonra kuyunun yeri 2 m düzeltiliyor:

```
NOKTA 485320,4310220
TAMPON nesneler=1 mesafe=5 katman=KORUMA
TAŞI nesneler=1 baslangic=485320,4310220 bitis=485322,4310220
```

TAŞI bittiğinde komut satırı şunu yazar:

```text
Kaynağı değiştiği için 1 sonuç artık güncel değil (TAMPON). Hangileri olduğunu görmek için: BAĞIMLILIK
```

Hangi sonuç, neden:

```
BAĞIMLILIK
```

```text
1 sonuç: 0 güncel, 1 güncel değil, 0 kaynaksız.
  güncel değil: nesne 2 (TAMPON) — değişen kaynak: nesne 1
Sonuçları yeniden hesaplamak için: BAĞIMLILIK islem=yenile — şimdiki hâliyle kabul etmek için: BAĞIMLILIK islem=kabul — kaynağından çözmek için: BAĞIMLILIK islem=coz
```

Koruma alanını kuyunun yeni yerine göre yeniden hesaplatın — aynı nesne, aynı değerlerle,
yeni biçimiyle:

```
BAĞIMLILIK islem=yenile nesneler=2
```

```text
1 sonuç kaynaklarının şimdiki hâlinden yeniden hesaplandı (TAMPON).
```

Koruma alanı olduğu gibi doğruysa yeniden hesaplamak yerine kabul edebilir, artık kuyuya
bağlı saymak istemiyorsanız çözebilirsiniz:

```
BAĞIMLILIK islem=kabul nesneler=2
BAĞIMLILIK islem=coz nesneler=2
```

Kilitli katmandaki uzunluk yazıları — yeni bir çizimde, parselin kenar uzunlukları
yazdırılıp katmanları kilitleniyor ve bir köşe oynatılıyor:

<!-- örnek: yeni çizim -->

```
KATMAN ad=PARSEL
ALAN 485300,4310200 485340,4310200 485340,4310230 485300,4310230
UZUNLUKYAZ nesneler=1 katman=OLCU
KATMAN ad=OLCU kilitli=evet
KÖŞETAŞI nesne=1 kose=3 nokta=485348,4310230
BAĞIMLILIK
```

```text
4 bağlı nesne: 2 güncel, 2 güncel değil, 0 bağı kopuk.
  güncel değil: nesne 3 (bağlı yazı) — kaynağı: nesne 1
  güncel değil: nesne 4 (bağlı yazı) — kaynağı: nesne 1
Bağlı nesneleri kaynağına yetiştirmek için: BAĞIMLILIK islem=yenile
```

Katmanın kilidini açmak yazıları kaynağına yetiştirir:

```
KATMAN ad=OLCU kilitli=hayır
```

```text
Kilidi açılan 2 bağlı nesne kaynağına yetişti.
```

### Arayüz

Şeritte **Analiz ▸ Denetim ▸ Bağımlılıklar**'a (ya da **Kadastro ▸ Denetim ▸
Bağımlılıklar**'a) basın: komut satırı çizimdeki bağlı nesneleri ve sonuçları sayar ve
güncel olmayanları yazar. Yanındaki **Güncelle** (`BAĞIMLILIK islem=yenile`) geride
kalanları yetiştirir ve güncel olmayan sonuçları yeniden hesaplar. Güncel olmayan nesne tuvalde uyarı renginde işaretlidir;
seçtiğinizde öznitelik panelindeki `koken` (sonuç), `bag` (bağlı yazı), `baglar` (ölçü)
ya da `sinir` (tarama) satırı **GÜNCEL DEĞİL** rozetini taşır.

### Betik

Bir kuyunun koruma alanını çizmek, kuyuyu taşımak ve koruma alanını kabul etmek:

```json
{"ad": "koruma alanını kabul et", "komutlar": [
  {"cmd": "core.point_draw", "args": {"noktalar": [[485320000, 4310220000]]}},
  {"cmd": "islem.tampon", "args": {"nesneler": [1], "mesafe": 5, "katman": "KORUMA"}},
  {"cmd": "core.move", "args": {"nesneler": [1],
                                "baslangic": [485320000, 4310220000],
                                "bitis": [485322000, 4310220000]}},
  {"cmd": "core.dependency", "args": {"islem": "kabul"}}
]}
```

Python'dan: `cad.dependency()` durumu, `cad.dependency(action="yenile")` yetiştirmeyi ve
yeniden hesaplamayı, `cad.dependency(action="kabul")` kabulü yapar.

### Üçü de aynı

Arayüz, komut satırı ve betik aynı nesneleri yetiştirir, kabul eder ya da çözer ve aynı
belgeyi bırakır. Günlüğe işlenen nesnelerin kimlikleri yazılır; günlüğü oynatmak
aynılarını işler.

## Geri alma

`durum` çizimi değiştirmez ve geri alma adımı bırakmaz. `yenile`, `kabul` ve `coz` tek
adımda geri alınır (Ctrl+Z ya da `GERİAL`): yenileme geri alınınca bağlı nesne yine geride,
yeniden hesaplanan sonuç eski biçimine döner; kabul geri alınınca sonuç yine güncel değil,
çözme geri alınınca bağ yeniden yerindedir. Yapılacak bir şey yoksa geri alma adımı da
bırakılmaz.

Yeniden hesaplamayı yapan komut (TAMPON, EŞYÜKSELTİ …) günlüğe ayrıca yazılmaz: günlükte
yalnız `BAĞIMLILIK islem=yenile nesneler=…` satırı durur; günlüğü oynatmak aynı
yeniden hesaplamayı yapar.

Katmanın kilidini açan komut, geride kalanları aynı adımda yetiştirir; kilidi geri almak
ikisini birlikte geri alır.

## Betikten kullanım

`durum`'un yapılandırılmış cevabı üç liste taşır. `sonuclar`: her sonuç için `nesne`
(kimliği), `islem` (onu yapan işin kimliği, örneğin `islem.tampon`), `ad` (`TAMPON`),
`durum` (`guncel`, `guncel_degil`, `kaynaksiz`), `degisen` ve `silinen` (kaynakların
kimlikleri); yanında üç sayı: `guncel`, `guncel_degil`, `kaynaksiz`. `baglilar`: her bağlı
nesne için `nesne`, `tur` (`yazi`, `olcu`, `tarama`), `durum` (`guncel`, `guncel_degil`,
`kopuk`), `kaynaklar`, `degisen` ve `silinen`. `paftalar`: bir çıktı yerleşiminin
çizimden adıyla okuduğu her katman ve sütun için `yerlesim`, `oge` (öğenin adı; atlas için
boş), `tur` (`katman`, `sutun`), `ad` ve `durum` (`guncel`, `kopuk`). `yenile`, `kabul` ve
`coz`'un cevabı `islem` ve `nesneler` (işlenen nesnelerin kimlikleri) taşır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | Kimliği [NESNEBİLGİ](entity_info.md) ya da seçimle doğrulayın |
| `Seçilen nesnelerin hiçbiri kaynağına bağlı değil.` | Verilen nesneler ne bağlı nesne ne sonuç: çizilmiş, kopyalanmış ya da çözülmüş | Hata değildir |
| `Çizimde kaynağına bağlı bir nesne yok.` | Çizimde hiç bağlı nesne ya da sonuç yok | Hata değildir |
| `Güncel olmayan bir sonuç yok; değişen bir şey olmadı.` | `kabul` ya da `coz` için güncel olmayan yok | Hata değildir |
| `Kaynağının gerisinde kalmış bir bağlı nesne ya da sonuç yok.` | `yenile` için geride kalan yok | Hata değildir |
| `N bağlı nesne kilitli katmanda; katmanın kilidi açılınca kaynağına yetişir.` | `yenile` kilitli katmandaki nesneye dokunmaz | Katmanın kilidini açın ([KATMAN](layer.md)); açıldığı anda yetişir |
| `N sonucun kaynağı silinmiş; yeniden hesaplanmaz. Kabul etmek için islem=kabul, kaynağından çözmek için islem=coz.` | Kaynaksız bir sonuç yeniden hesaplanmaz | Kabul edin ya da çözün |
| `… sonucu (nesne N) yeniden hesaplanamaz: nasıl hesaplandığı kayıtlı değil. …` | Sonuç, nasıl hesaplandığını kaydetmeyen bir sürümde üretildi | Kabul edin, çözün ya da silip üreten komutu yeniden çalıştırın |
| `… sonucunun (nesne N) hiçbir kaynağı kalmadı; yeniden hesaplanamaz.` | Sonucun bütün kaynakları silinmiş | Kabul edin ya da çözün |
| `… sonucu (nesne N) yeniden hesaplanamadı: …` | Üreten komut yeni kaynaklarla çalışamadı (örneğin çizgiler artık bir alan kapatmıyor); sebebi mesajın sonundadır. Hiçbir şey değişmez | Kaynakları düzeltin ya da o sonucu çözün (`islem=coz nesneler=N`) |
| `N bağlı nesne kabul edilmez: kaynağına yetiştirmek için islem=yenile, bağından çözmek için islem=coz.` | `kabul` yalnız sonuçlar içindir | `yenile` ya da `coz` kullanın |
| `N nesne kaynağına bağlı değil; atlandı.` | Verilenlerin bir kısmı bağımlı nesne değil | Hata değildir; yalnız bağımlı olanlar işlendi |
| `Yazı N kaynağına yerleştirilemedi: bağlı olduğu kenar ya da köşe artık yok. …` | Yazının izlediği kenar ya da köşe kaynaktan kalkmış | `BAĞIMLILIK islem=coz` ile bağından çözün ya da [BAĞLA](bagla.md) ile yeniden bağlayın |
| `Ölçü N ölçtüğü noktalara yeniden kurulamadı.` | Yeni noktalarla ölçü kurulamıyor (iki nokta çakıştı) | Ölçüyü silip yeniden çizin ya da bağından çözün |
| `Tarama N sınırının bir parçasını kaybetmiş; kalanlardan kurulmaz. …` | Taramanın sınır nesnelerinden biri silinmiş ya da artık kapanmıyor | [TARAMADÜZENLE](hatch_edit.md) ile yeni sınır verin ya da bağından çözün |

## İlgili

- [Bağımlılıklar ve sonuçlar](../veri/bagimliliklar.md) — hangi nesne neyi izler, neyi bilir
- [Bağlı nesneler](../islem/bagli-nesneler.md) — kaynağını izleyen yazılar
- [Nesne kimliği ve kökeni](../veri/kimlik-ve-koken.md)
- [NESNEBİLGİ](entity_info.md)
