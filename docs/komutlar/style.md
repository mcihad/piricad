# STİL — Nesne Stili ve Gösterim Kataloğu

Çizimini renklendiren, paftaya hazırlayan ve plan gösterimlerini uygulayan herkes için;
bu sayfayı bitirdiğinizde bir katmandaki nesnelere doğrudan stil vermeyi, stil kataloğu
paketinden gösterim uygulamayı ve stili geri almayı bileceksiniz.

## Ne yapar

Bir katmandaki **canlı nesnelerin her birine bir stil yazar**.

PiriCAD'de görünüm çizim anında hesaplanmaz. `STİL` çalıştığında görünüm bir kez çözülür,
çizimin stil tablosuna tek satır olarak yazılır ve her nesne o satırın numarasını taşır.
Ekran çizerken kural işletmez, öznitelik okumaz, ifade değerlendirmez; tek bir sayı okur.
Beş milyon parselli bir katmanda kaydırmanın akıcı kalmasının sebebi budur.

Stil üç kaynaktan gelebilir; sıralama şudur:

1. **Katalog satırı** — `paket=` ile bir stil kataloğu verirseniz, satır ya `kod=` ile
   doğrudan seçilir ya da katalogdaki eşleme kuralları nesneye bakarak seçer.
2. **Komut satırında verilen değerler** — `renk`, `kalinlik`, `dolgu`, `sira`. Bunlar
   katalog satırının üzerine yazar.
3. **Katmanın kendi görünümü** — yukarıdaki ikisinin dokunmadığı her özellik katmandan
   gelmeye devam eder. Katman rengini değiştirdiğinizde bu özellikler de değişir.

`sifirla=evet` yazarsanız stil silinir ve nesneler tamamen katman görünümüne döner.

Aynı görünüm iki kez istendiğinde stil tablosunda **tek satır** açılır: on bin parsele aynı
gösterimi vermek tabloya bir satır ekler, on bin satır değil.

### Gösterim katalogları hakkında

Plan gösterimleri koda gömülmez; `data/catalogs/` altında veri olarak durur. Bir yönetmelik
değişikliği veri paketinin güncellenmesidir, programın yeniden derlenmesi değildir.

Mekânsal Planlar Yapım Yönetmeliği'nin (MPYY) gösterim ekleri EK-1a (ortak gösterimler),
EK-1b (mekânsal strateji planı), EK-1c (çevre düzeni planı), EK-1ç (nazım imar planı),
EK-1d (uygulama imar planı) ve EK-1e (detay kataloğu) olarak paketlenir. PiriCAD'in bu
paketi `data/catalogs/mpyy/plan-gosterim.json` dosyasındadır ve **476 gösterim satırı**
içerir. Paketin hangi Resmî Gazete sürümüne dayandığı, ne çıkarıldığı ve neyin eksik
kaldığı [MPYY plan gösterimleri](../veri/mpyy-gosterimleri.md) sayfasında yazılıdır.

**Paketin eşleme kuralları BOŞTUR ve uzman onayı BEKLİYOR.** Satırların kendisi resmî ek
metninden çıkarılmıştır, ama hangi nesnenin hangi satırı alacağını söyleyen `kurallar`
dizisi boştur: bu, ek metninden okunabilecek bir şey değil, plan türü ve öznitelik
şemasıyla birlikte verilen bir uzman kararıdır. Bu yüzden bugün satırı `kod=` ile adıyla
seçersiniz; `paket=` tek başına verildiğinde komut `Stil kataloğunda bu nesneye uyan kural
yok` hatasını verir.

Paket harita mühendisi / şehir plancısı onayından geçmeden resmî bir plan paftasında
kullanılmaz. Kurallar ve onay **Faz 3'te** gelecektir. Dilediğiniz zaman `paket=` ile kendi
stil kataloğunuzu verebilirsiniz; biçim
`data/catalogs/schema/plan-gosterim.schema.json` dosyasında tanımlıdır.

## Adlar

| Ad | Tür |
|---|---|
| `STİL` | Türkçe, birincil |
| `STIL` | Türkçe, ASCII karşılık |
| `STYLE` | İngilizce karşılık |
| `ST` | Kısaltma |
| `core.style` | Komut kimliği |

## Sözdizimi

```text
STİL
STİL <katman adı>
STİL katman=<ad> [renk=<tamsayı>] [kalinlik=<µm>] [dolgu=<tamsayı>] [sira=<tamsayı>]
STİL katman=<ad> paket=<katalog yolu> [kod=<satır kimliği>] [olcek=<payda>]
STİL katman=<ad> sifirla=evet
```

Argümansız çağırırsanız komut yalnız katman adını sorar; geri kalan her şey argümanla
verilir.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `katman` | Stilin yazılacağı katmanın adı. Zorunlu. Katman var olmalıdır |
| `paket` | Stil kataloğu paketinin dosya yolu. Göreli yol çalışma dizinine göre çözülür |
| `kod` | Katalogdaki satırın kimliği. Verilmezse katalogdaki eşleme kuralları çalışır |
| `sinifla` | Sınıflandırmada kullanılacak öznitelik. Her nesne KENDİ değerine göre stillenir |
| `olcek_min` | Bu ölçek paydasından daha yakında çizilmez (1:N'deki N) |
| `olcek_max` | Bu ölçek paydasından daha uzakta çizilmez |
| `olcek` | Ölçek paydası (1:N). Ölçeğe bağlı satır ve kuralların hangisinin geçerli olduğunu belirler. `0` = ölçekten bağımsız |
| `renk` | Çizgi rengi, `0xAARRGGBB` düzeninde tam sayı |
| `kalinlik` | Çizgi kalınlığı, **kâğıt mikrometresi**. `1000` = paftada 1 mm |
| `dolgu` | Dolgu rengi, `0xAARRGGBB`. `0` = dolgusuz |
| `sira` | Çizim sırası. Büyük olan üste gelir |
| `sifirla` | `evet` yazılırsa stili siler; nesneler katman görünümüne döner |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

`renk` ve `dolgu` değerleri `KATMAN` komutundakiyle aynı düzendedir; hazır değerler için
[Katman yönetimi](layer.md) sayfasındaki renk tablosuna bakın.

### Kalınlık neden mikrometre

Mevzuat çizgi kalınlığını **paftada milimetre** olarak verir. Ekrandaki piksel kalınlığı
ölçekle ve ekran çözünürlüğüyle her karede değişir; kâğıt kalınlığı değişmez. Bu yüzden
kalınlık kâğıt mikrometresinde saklanır ve piksel karşılığı her karede yeniden hesaplanır:

| Paftada | `kalinlik` |
|---|---|
| 0,13 mm | `130` |
| 0,25 mm | `250` |
| 0,35 mm | `350` |
| 0,50 mm | `500` |
| 1,00 mm | `1000` |

### Kategorize çizici

Bir katmandaki nesneleri, hepsine aynı stili yazmak yerine **her birinin kendi
özniteliğine göre** stillemek `sinifla` ile yapılır:

```
KATMAN ad=PLAN
SÜTUN gosterim metin
STİL katman=PLAN paket=data/catalogs/mpyy/plan-gosterim.json sinifla=gosterim
```

Bu tek komut, `PLAN` katmanındaki her nesnenin `gosterim` özniteliğini okur, o değeri
katalogda kimlik ya da ad olarak arar ve bulduğu satırın rengini, kalınlığını ve
dolgusunu o nesneye yazar. Aynı katmandaki iki parsel farklı gösterim taşıyorsa farklı
görünür: **katman üyeliği görünümü belirlemez, nesnenin kendi verisi belirler.**

Değer önce kimlik olarak aranır, bulunamazsa ad olarak. İkisi de kabul edilir çünkü
çizimi etiketleyen bir insandır: `nip-toplu-konut-alani-siniri` paketin satıra verdiği
addır, `TOPLU KONUT ALANI` ise bir plancının öznitelik hücresine yazdığıdır.

Özniteliği olmayan bir nesne **katman varsayılanında kalır** ve sayılır. Gösterimini
bildirmeyen bir parsele gösterim uydurmak, hukuki bir çizimin taşımaması gereken tam
olarak o icattır; komut kaç nesnenin eşleştiğini ve kaçının öznitelik taşımadığını
söyler.

Bağlamanın komutta durması kasıtlıdır: yönetmelik bir gösterimin **neye benzediğini**
söyler, sizin öznitelik sütununuzun **adını** asla söylemez. O yüzden "hangi sütun
gösterim tutuyor" bilgisi katalog paketine değil, çağrı yerine aittir.

Sayısal bir sütunla aralık kuralları da aynı yoldan çalışır — nüfus yoğunluğuna göre
beş kademeli konut lekesi, beş aralık penceresi demektir.

### Ölçeğe bağlı görünürlük

Bir stil yalnız belirli ölçek aralığında çizilebilir:

```
KATMAN ad=CDP_LEKE
STİL katman=CDP_LEKE renk=0xFF6A1B9A dolgu=0xFFD7B8E8 olcek_min=3000
```

Bu leke yalnız 1/3000'den **uzakta** görünür; yakınlaştıkça kaybolur ve altındaki
uygulama imar planı parselleri okunur hâle gelir. Tersi de olur:

```
KATMAN ad=UIP_PARSEL
STİL katman=UIP_PARSEL renk=0xFF2E7D32 dolgu=0xFFE1DFB3 olcek_max=3000
```

Değerler 1:N gösteriminin **N**'idir; büyüyen N uzaklaşmak demektir. Biri
verilmezse o yönde sınır yoktur.

Bu planlama işinde süs değildir: 1/100000 ölçekli bir çevre düzeni planı bir leke
gösterir, 1/1000 ölçekli uygulama imar planı o lekenin parsellerini gösterir, ve
ikisini aynı anda çizmek kimsenin okuyamayacağı bir pafta üretir.

Pencere sembolün üstünde saklanır, yani kare yolunda bir dizi araması ve bir
karşılaştırmadır — hiçbir kural değerlendirilmez.

## Örnekler

### Komut satırı

Bir katman kurun, üzerine çizin ve stilini verin:

```
KATMAN ad=IMAR
ÇİZGİ 485300,4310200 485360,4310200
STİL katman=IMAR renk=4281236786 kalinlik=350
```

Dolgulu bir alan görünümü, üstte çizilsin diye sırası büyük:

```
STİL katman=IMAR renk=4281236786 kalinlik=350 dolgu=4294703769 sira=20
```

Yalnız kalınlığı değiştirin; renk katmandan gelmeye devam etsin diye `renk` vermeyin:

```
STİL katman=IMAR kalinlik=500
```

Stili silin, nesneler katman görünümüne dönsün:

```
STİL katman=IMAR sifirla=evet
```

Adında boşluk olan katman:

```
KATMAN ad="YOL KENARI"
ÇİZGİ 485300,4310260 485360,4310260
STİL katman="YOL KENARI" renk=4284310640
```

MPYY paketinden bir gösterim satırını adıyla uygulamak şöyle görünür:

```text
STİL katman=IMAR paket=<depo kökü>/data/catalogs/mpyy/plan-gosterim.json kod=uip-ticaret-alani
1 nesneye stil yazıldı: 'IMAR', katalog satırı 'TİCARET ALANI', stil kimliği 4.
```

Bu blok bir iskelettir, olduğu gibi çalıştırılamaz: `paket=` yolu çalışma dizinine göre
çözülür, bu yüzden `<depo kökü>` yerine kendi yolunuzu yazmanız gerekir. Satır kimlikleri
`<ek kısaltması>-<ad>` biçimindedir: `ortak-` (EK-1a), `msp-` (EK-1b), `cdp-` (EK-1c),
`nip-` (EK-1ç), `uip-` (EK-1d).

`kod=` vermeden yalnız `paket=` verirseniz komut `Stil kataloğunda bu nesneye uyan kural
yok` hatasını verir: pakette eşleme kuralı yoktur. **Faz 3'te** kurallar eklendiğinde
katmanı tek tek kodlamak gerekmeyecektir.

### Arayüz

Komutu pencerenin altındaki **komut satırına** yazın; sonuç **Transkript** panelinde
görünür. Arayüzün hiçbir ayrıcalığı yoktur: menüden yapılan da, komut satırından yazılan
da aynı komuttur, aynı doğrulamadan geçer ve aynı günlüğe yazılır.

Komut satırına `STİL` yazıp **Enter**'a basarsanız katman adı sorulur; adı yazıp yeniden
**Enter**'a basmak yeter. **Esc** komutu iptal eder ve çizimde hiçbir iz bırakmaz.

**Katmanlar** panelinde bir katmana sağ tıklayarak açılan **Stil** iletişim kutusu ve
gösterim kataloğu seçici **Faz 1'de** gelecek; ikisi de bu komutu gönderecek, ikinci bir
stil listesi olmayacak.

### Betik

```json
{
  "ad": "Katman stilleri",
  "komutlar": [
    { "cmd": "core.layer", "args": { "ad": "IMAR" } },
    { "cmd": "core.line",  "args": { "noktalar": [[485300000,4310200000],[485360000,4310200000]] } },
    { "cmd": "core.style", "args": { "katman": "IMAR", "renk": 4281236786, "kalinlik": 350, "sira": 20 } }
  ]
}
```

## Geri alma

Stil değişikliği geri alınabilir ve bir komut bir adımdır:

```
GERİAL
```

`GERİAL` stil sütununu eski hâline döndürür. Stil tablosuna eklenen satır tabloda kalır:
tablo yalnız büyür, verilen bir stil numarası çizimin ömrü boyunca geçerli kalır. Bu
bilinçlidir — bir satırı geri almak, ona işaret eden başka nesnelerin ve günlükteki eski
değerlerin numarasını kaydırırdı. Kullanılmayan satır zararsızdır; hiçbir nesne onu
göstermez.

Komut hata verirse çizime **hiç dokunulmaz**. Katalog okunamazsa, satır bulunamazsa veya
hiçbir kural uymazsa karar aşamasında durulur; yarısı stillenmiş bir katman oluşmaz.

## Betikten kullanım

`STİL` betiklenebilir ve AI erişimlidir. Tipik kullanım, bir betiğin sonunda bütün
katmanların görünümünü tek seferde kurmaktır. Betiğin tamamı tek geri alma adımıdır:

```json
[
  { "cmd": "core.layer", "args": { "ad": "PARSEL" } },
  { "cmd": "core.line",  "args": { "noktalar": [[485300000,4310200000],[485360000,4310200000]] } },
  { "cmd": "core.layer", "args": { "ad": "YOL" } },
  { "cmd": "core.line",  "args": { "noktalar": [[485300000,4310260000],[485360000,4310260000]] } },
  { "cmd": "core.style", "args": { "katman": "PARSEL", "renk": 4281236786, "kalinlik": 350, "sira": 20 } },
  { "cmd": "core.style", "args": { "katman": "YOL", "renk": 4284310640, "kalinlik": 500, "sira": 10 } }
]
```

Katalog kullanan bir betikte `paket` alanına dosya yolunu yazarsınız; yol göreliyse betiği
çalıştıran işlemin çalışma dizinine göre çözülür, bu yüzden paylaşılan betiklerde tam yol
tercih edilir.

Ayrıntı: [Betik yazma](../betik/README.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Katman bulunamadı: 'IMAR'. Önce KATMAN komutuyla oluşturun.` | Verilen adda katman yok | Adı denetleyin veya `KATMAN ad=IMAR` ile oluşturun |
| `'core.style': zorunlu 'katman' parametresi eksik. Beklenen: metin` | Katman adı verilmemiş | `katman=` ile ad verin |
| `'kod' verildi ama 'paket' verilmedi: hangi katalogdan okunacağı belirsiz. 'paket=' ile katalog dosyasını verin.` | Satır kimliği var, katalog yok | `paket=` ile katalog dosyasını da verin |
| `'sifirla' ile 'paket' aynı komutta kullanılamaz: biri stili siler, diğeri yazar.` | İki zıt istek aynı komutta | İki ayrı komut çalıştırın |
| `Stil kataloğu okunamadı: '...'. Dosya yolunu denetleyin; göreli yol çalışma dizinine göre çözülür.` | Dosya yok veya okunamıyor | Yolu denetleyin, tam yol yazın |
| `Stil kataloğu geçerli JSON değil: '...'` | Paket bozuk | Dosyayı bir JSON doğrulayıcıdan geçirin |
| `Stil kataloğu: zorunlu 'published' alanı eksik veya boş. Beklenen: metin.` | Paket künyesi eksik | `schema_version`, `package_version`, `id`, `source`, `published`, `licence` alanlarının hepsini yazın |
| `Stil kataloğunda 'K' kimlikli satır yok. Katalog: mpyy-plan-gosterimleri 0.2.0.` | `kod` katalogda yok | Katalogdaki satır kimliklerini denetleyin |
| `Stil kataloğunda bu nesneye uyan kural yok.` | Hiçbir eşleme kuralı nesneye uymadı | Katalogda koşulsuz bir "kalan hepsi" kuralı tanımlayın veya `kod=` ile satırı doğrudan seçin |
| `Stil kataloğu: 'k1' kuralı 'yok-boyle' satırını gösteriyor, ama katalogda böyle bir satır yok.` | Katalogda kural ile satır kimliği tutmuyor | Kuraldaki `stil` alanını düzeltin |
| `Renk '#RRGGBB' veya '#AARRGGBB' biçiminde olmalı. Girilen: 'kirmizi'` | Katalogdaki renk metni bozuk | Rengi onaltılık yazın |
| `'core.style': bilinmeyen parametre 'renkler'. Tanımlı parametreler: katman, paket, kod, olcek, renk, kalinlik, dolgu, sira, sifirla` | Parametre adı yanlış yazılmış | Doğru adı kullanın |

Katmanda hiç nesne yoksa hata olmaz; transkriptte `'IMAR' katmanında nesne yok; stil
yazılmadı.` yazar.

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
