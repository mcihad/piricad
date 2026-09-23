# Onay ve Denetim

Bir yapay zeka önerisinin çizime tam olarak nasıl girdiğini — ve nasıl giremediğini —
bilmek isteyen harita mühendisi, kontrol mühendisi ve kurum denetçisi için; bu sayfayı
bitirdiğinizde öneri boru hattını, tutamak kuralını, tek işlem–tek geri alma
garantisini ve denetim kaydının ne taşıdığını bileceksiniz.

> **Ne çalışıyor.** Bu sayfadaki her şey çalışır durumdadır: öneri defteri, öneri kartı,
> onay kapısı, tutamak deposu ve denetim kaydı. **Kendi ölçtüğünüz koordinat listesini
> tutamağa çeviren yol** henüz yoktur ve gelecek zamanla yazılmıştır (`.claude/ai.md`).

## Boru hattı

Her yapay zeka turu aynı sırayı izler. Hiçbir adım atlanmaz, hiçbiri yer değiştirmez:

1. **Bağlam toplama** — okuma araçlarıyla. Çizim prompta dökülmez.
2. **Model** — komut dizisini yazar.
3. **Doğrulama** — katı şema: tanınmayan araç, tanımlı olmayan parametre, bozuk argüman
   doğrudan reddedilir.
4. **Önizleme** — dizi bir öneri olur, komut satırlarıyla birlikte görünür.
5. **Karar** — bilgisayar başındaki kişi kartta uygular ya da reddeder; ya da o kişinin
   **önceden seçtiği onay politikası** izin veriyorsa öneri hemen uygulanır
   ([aşağıda](#onay-politikası-ayarı-ve-otomatik)).
6. **Tek işlem** — uygulanan dizi tek bir toplu işte uygulanır.
7. **Denetim kaydı** — karar, hangisi olursa, dosyaya yazılır.

Bir dış ajan için de aynı sıra geçerlidir; tek fark, ilk üç adımın ajanın kendi
tarafında olmasıdır.

## Konum uydurulamaz: tutamaklar

Bu, kuralların en somut olanıdır ve bir denetim değil, bir **tip** olarak kurulmuştur.

Bir ajana sunulan şemada nokta, nokta listesi ve nesne seçimi **dize** olarak
tanımlanır ve dizenin biçimi bellidir: `@` ve on altı onaltılık hane, istenirse listenin
bir elemanı için `.N`.

```text
@0f3a1c7b9e4d2856
@0f3a1c7b9e4d2856.3
```

Dolayısıyla bir model oraya sayı **yazamaz**: ret, argümanlar hâlâ JSON iken gerçekleşir
— henüz bir argüman nesnesi oluşmadan. "Doğrulamadan önce reddedilir" cümlesi böylece
bir kod sıralaması dikkati değil, harfi harfine doğru bir ifade olur.

**Tutamağı yalnız okuma araçları üretir** ve okuma araçları çizimi okur: `secimi_al`
kullanıcının seçtiğini, `sorgula` eşleşenleri **nesne** olarak verir; `gorunum_bilgisi`
pencerenin köşelerini ve ortasını, [`nesne_noktalari`](../komutlar/object_points.md) bir
nesnenin merkezini, köşelerini, uçlarını, kutusunu ya da kenar ortalarını **nokta**
olarak verir. Sayılar çizimin ve ekranın kendi sayılarıdır.

### Göreli nokta: bir tutamaktan ölçüyle

Bir ajan yeni bir şey çizerken köşeleri çizimde henüz yoktur. Onları bir tutamaktan
**ölçüyle** söyler — bir CAD kullanıcısının `@10,0` yazması gibi:

```text
{"taban": "@0f3a1c7b9e4d2856.0", "dogu": 10000, "kuzey": -5000}
```

Bu, tabanın 10 m doğusu ve 5 m güneyidir. Ölçüler tam sayı milimetredir; batı ve güney
eksidir. Bir çokgenin köşeleri böyle bir dizidir: "ekranın ortasına 20 m'lik kare"
isteği, `gorunum_bilgisi`nin "görünümün ortası" tutamağından ±10 m'lik dört köşedir.
Uzunluk, yarıçap ve mesafe gibi ölçüler düz sayıdır; bir konum ise hiçbir zaman düz
sayı değildir: tabanı her zaman bir tutamaktır.

Öneri kartında her konumun nereden geldiği görünür ve **denetim kaydına** yazılır:
hangi tutamak, ve göreli bir noktaysa hangi ölçüyle
(`@0f3a1c7b9e4d2856.0 + doğu 10000, kuzey -5000 mm`).

### Üç tür tutamak

| Tür | Ne taşır | Kim üretir |
|---|---|---|
| nokta | Bir ya da daha çok koordinat | `gorunum_bilgisi` (görünümün ortası), `nesne_noktalari` |
| nesne | Kalıcı nesne anahtarları | `sorgula`, `secimi_al` |
| pencere | Bir dikdörtgen | `gorunum_bilgisi`, `sorgula`'nın kapsayan kutusu |

### İstemciye ne söylenir, ne söylenmez

Bir istemci tutamak hakkında şunları öğrenir: kimliği, türü, kaç değer taşıdığı, hangi
araçtan geldiği, hangi çizim sürümünde alındığı ve kaynağı. **Koordinat listesinin
kendisini öğrenmez.** Sebebi basit: sayıları okuyabilen bir model, onları bir sonraki
çağrıda sayı olarak geri yazmaya bir adım uzaktadır — ki tutamak tam olarak bunu
engellemek için vardır.

Tek istisna **pencere tutamağıdır**: onun dört köşesi okunabilir. Onlar ekranın kendi
köşeleridir, çizimin içeriği değil; okunamayan bir pencere, üzerine akıl
yürütülemeyen bir penceredir.

### Kaynak: sayı nereden geldi

| Kaynak | Anlamı |
|---|---|
| `cizim` | Açık çizimden bir okuma aracıyla okundu |
| `hesap` | Çizim geometrisinden hesaplandı |
| `kullanici_onayli_olcum` | Bir insanın gözden geçirip kabul ettiği ölçüm listesi |

### Kendi ölçtüğünüz koordinatlar

Elinizde aletten gelmiş bir koordinat listesi varsa, onun tutamağa dönüşme yolu **henüz
yok**; **Faz 3'ün kalan işlerindendir** ve şöyle olacak: liste **teslim edilir**, size **kaç
nokta taşıdığı ve bir sağlama değeriyle** gösterilir, ve ancak **siz kabul
ettiğinizde** tutamak olur. Kaynağı da `kullanici_onayli_olcum` olarak işaretlenir.
Kaynak adı bugün de kayıt biçiminde vardır, ama onu üreten bir yol yoktur.

Buradaki ayrım kuralın tamamıdır: veriye kefil olan bir insandır, uyduran bir model
değildir.

### Tutamak eskirse

Her tutamak, alındığı **çizim sürümünü** taşır. Okuma ile yazma arasında biri bir şeyi
taşıdıysa, sildiyse ya da değiştirdiyse tutamak **reddedilir**:

```text
Tutamak '@0f3a1c7b9e4d2856' çizimin eski bir hâlinden: o zamandan beri çizim değişti.
Okuma aracını yeniden çağırın.
```

Reddetmek tek dürüst cevaptır. İstemci yeniden okur, bu ucuzdur; yanlış bir parsel
sınırı değildir.

Bir oturumun tutamak deposunda en çok **256** tutamak durur; dolduğunda en eskisi
düşer. İki istemcinin deposu ayrıdır: birinin tutamağı ötekinde hiçbir şey ifade etmez.

### Reddedilen koordinat kayda geçer

Bir ajan nokta isteyen bir parametreye sayı yazarsa iki şey olur: istemciye hangi okuma
aracının tutamak ürettiğini söyleyen bir ret döner, ve **denetim kaydına** bir satır
yazılır. Programın yaptığı en ilginç ret, iz bırakmadan geçmez.

## Öneri kartı: kararın verildiği yer

Sohbetteki modelin açtığı bir önerinin kartı, onu açan cevabın altında görünür
([Yapay Zeka paneli](sohbet.md)). Bir **MCP istemcisinin** açtığı öneri de aynı panelde,
istemcinin adını söyleyen bir bildirimle ve aynı kartla görünür; panel kapalıysa açılır
ve kartın düğmelerine kaydırılır. Kartın kesik çizgili bir kenarı ve **ÖNERİ** rozeti
vardır; şunları gösterir:

| Kartta ne var | Ne söyler |
|---|---|
| Başlık | `Öneri p0f3a1c7b9e4d2856 · 2 adım` ve önerinin durumu |
| Komut satırları | Uygulanacak satırların **tamamı**, sizin de yazabileceğiniz hâlleriyle |
| `Koordinat kaynağı` | Adımlardaki konumların hangi tutamaklardan geldiği; göreli noktalarda tutamak ve ölçü (kayıtta `konum_kaynagi`) |
| `Varsayımlar` | İstemcinin öneriyi hazırlarken yaptığını bildirdiği varsayımlar, kendi sözleriyle (kayıtta `varsayimlar`) |
| `Onay bekliyor` | Onay politikanızın bu öneriyi neden size bıraktığı, ör. `Her değişiklikte onay isteniyor.` |
| `İsteyen` | İstemcinin adı, varsa modelin kimliği |
| Uyarı şeridi | Çizim öneriden sonra değiştiyse: `Öneri 12 numaralı sürüme göre hazırlandı, çizim şimdi 14.` |
| **Reddet** / **Uygula** | Kararın kendisi. `Uygula` karttaki tek birincil düğmedir |

Uyarı şeridi göründüğünde adımlar, hazırlandıkları çizimden **başka** bir çizime
uygulanacak demektir: içlerindeki nesne anahtarları hâlâ geçerli olabilir ama artık
başka bir şeyi gösteriyor olabilir. Uygulamadan önce satırları gözden geçirin.

Onay nesnesini üretebilen **iki** çağıran vardır ve üçüncüsü bir CI kapısıyla engellenir:
kart — bir kişinin kararı — ve onay politikası yolu — aynı kişinin önceden, kendisi için
verdiği karar. Onay politikanız bir öneriyi uyguladıysa kart bunu söyler:
`Öneri onay politikanızla uygulandı — tek Ctrl+Z ile geri alınır.` Bir öneri artık
defterde yoksa kart bunu söyler ve düğmeleri kapatır.

## Kararı kim verdi: `core.ai.sorumlu`

Denetim kaydına yazılan ad, `Seçenekler ▸ Yapay Zeka Modelleri` sayfasındaki
**`core.ai.sorumlu`** ayarından gelir: öneriyi onaylayan kişinin adı. Uygulama
kapsamındadır, yani bu makinede çalışan kişiyi adlandırır ve çizimle taşınmaz — bir
pafta başka bir ofise gittiğinde onu orada kim onaylarsa o makinenin kaydına o ad
girer.

Ayar boşsa kayıt **işletim sisteminin kullanıcı adını** alır ve onu böyle işaretler:

```text
ayse (işletim sistemi kullanıcısı)
```

İşletim sistemi de bir ad vermiyorsa `(adsız kullanıcı)` yazılır. İkisi de dürüsttür
ama **imza değildir**: kadastro ve imar çıktısından sorumlu mühendisin adını bu ayara
yazın.

## Tek işlem, tek Ctrl+Z

Onaylanan bir öneri **tek bir toplu iş** olarak çalışır:

- On bir komutluk bir öneri **tek** `Ctrl+Z` ile geri alınır.
- Adımlardan biri reddedilirse **hepsi** geri sarılır ve çizim, denemeden önceki hâline
  **bit düzeyinde** eşit kalır. Yarım uygulanmış bir ifraz ya da tevhit, bu programda
  bir hata değil, derlemeyi kıran bir kusurdur.
- Reddedilen ya da iptal edilen bir öneri çizime hiç dokunmaz.

Adımlar, kartta okuduğunuz satırlar **yeniden ayrıştırılarak** çalıştırılmaz: satır
insanın okuduğu şeydir, argümanlar öneri derlenirken doğrulanmış olanlardır. İkisinin
ayrışması için ikinci bir şans verilmez.

Onay, önerinin **kimliğine değil, kartta okuduğunuz satırlara** bağlıdır. Kart
çizildikten sonra öneriyi açan istemci ona bir adım daha ekleyebilir — bir diziyi tek
geri alma adımında toplamanın yolu budur — ve o zaman iki satır gösteren bir kart üç
satır uygulardı. Uygula, kartın çizdiği hâlden farklı bir öneriyi **reddeder**:

```text
Öneri p0f3a1c7b9e4d2856 karttaki hâlinden farklı: onaydan sonra adım eklenmiş.
Uygulanmadı; kartı kapatıp yeniden açın.
```

Kırpılmaz, reddedilir: dürüst cevap önerinin şu anki hâlini gösteren yeni bir karttır.
Bu bir karar değildir, dolayısıyla öneri beklemeye devam eder.

Bir önerinin kararı **bir kere** verilir; verilmiş karar değiştirilemez.

Kart onayladığı hâlde program uygulayamazsa öneri `basarisiz` olur — `reddedildi`
değil. İnsan "evet" dedi, program "olmaz" dedi; bunlar aynı cevap değildir ve kayıt
ikisini ayırır.

## Denetim kaydı

Her karar bir **denetim kaydı** satırı yazar. Kayıt, aylar sonra sorulacak soruya cevap
verir: **bu sınırı buraya kim koydu, neye dayanarak?**

Kayıt, kullanıcı yapılandırma dizininizde, **aya bir dosya** hâlinde tutulur:

```text
<yapılandırma dizini>/denetim/2026-09.jsonl
```

Her satır bir JSON nesnesidir ve her karar alındığı anda **diske yazılıp boşaltılır**:
çökme sırasında kaybolan bir karar, kimsenin hesabını veremeyeceği bir karardır.

| Alan | Ne taşır |
|---|---|
| `sürüm` | Kayıt biçiminin sürümü |
| `kayit` | Kaydın kimliği: `d` ve on altı onaltılık hane |
| `oneri` | Kararın verildiği önerinin kimliği |
| `zaman_utc_ms` | Kararın zamanı, UTC |
| `karar` | `uygula`, `reddet` ya da `koordinat_reddi` |
| `kullanici` | Bilgisayar başındaki kişi: kararı veren. `core.ai.sorumlu` ayarından gelir |
| `karar_veren` | Kararı **ne** verdi: kartta tıklayan kişi için `insan`, onay politikası için `politika:otomatik` ya da `politika:riskli_islemlerde` |
| `onay_politikasi` | Karar anında yürürlükte olan onay politikası |
| `isteyen` | İsteyen istemcinin adı ve belirtecinin **parmak izi** |
| `model` | Modelin kimliği ve sürümü; düz bir MCP istemcisinde boş kalır |
| `uc_nokta` | Sağlayıcı ve uç nokta, ya da istemcinin bildirdiği ad |
| `istem` | Ne istendiği, istemcinin kendi ifadesiyle |
| `komutlar` | Komut satırları, çalışacakları hâliyle |
| `varsayimlar` | İstemcinin bildirdiği varsayımlar; yoksa alan yazılmaz |
| `sonuc` | Uygulandığında ne olduğu ya da neden uygulanmadığı |
| `olusan_nesneler` | Onaylanan önerinin oluşturduğu **kalıcı nesne anahtarları** |

`karar_veren` kararı kimin verdiğini **her satırda açıkça** yazar: kartta tıklayan bir
kişi `insan`, onay politikanız `politika:<değer>`. Otomatik bir uygulama hiçbir zaman bir
insan tıklaması gibi yazılmaz; ikisi aylar sonra da birbirinden ayırt edilir.

`onay_politikasi` kararın **hangi kural altında** verildiğini söyler. Bir karar ancak
verildiği kurala karşı açıklanabilir, ve o ayar kayıt okunana kadar iki kez değişmiş
olabilir.

`olusan_nesneler` alanı kayıt biçiminde vardır ama henüz dolmuyor; **Faz 3'ün kalan
işlerinden**
(`.claude/ai.md` R7): bir nesneden onu yetkilendiren kayda giden bağ o zaman kurulur ve
kalıcı anahtar kaydetmeden, yeniden açmaktan ve sıralamadan etkilenmediği için
**dosya kapanıp açıldıktan sonra da** durur. Bugün aynı soru `komutlar` alanı ve kararın
zamanı üzerinden yanıtlanır.

`model` alanının bir MCP istemcisinde boş kalması bir eksiklik değil: bir MCP istemcisi
kendisini hangi modelin sürdüğünü söylemez, ve hukuki bir kayda yazılan bir tahmin
yanlış bir olgudur.

**Reddedilen öneriler de yazılır.** Aylar sonra sorulan soru genellikle mühendisin neyi
**reddettiğidir**. Koordinat reddi de yazılır ve `karar` alanı `koordinat_reddi` olur.

Geri çekilen bir öneri — istemci bağlantıyı kapattığında — defterde `geri_cekildi`
durumuna geçer; kendi denetim satırını (`karar: geri_cek`) yazması **Faz 3'ün kalan
işlerindendir**
(`.claude/ai.md` R27). Geri
çekilmiş bir öneri hiç uygulanmadığı için çizimde de izi yoktur.

### Kimlik bilgisi asla kayda girmez

Ne bir API anahtarı, ne bir erişim belirteci, ne bir şifre denetim kaydına yazılır.
İstemcinin belirteci yerine sekiz haneli bir **parmak izi** yazılır: iki istemciyi
ayırt etmeye yeter, bir şeyi geri kurmaya yetmez. Kayıt yıllarca saklanır; içine
yazılan bir sır, o kayıt durduğu sürece sızmış bir sırdır.

### Komut günlüğüyle ilişkisi

Denetim kaydı, [komut günlüğünün](../mimari/gunluk.md) yerine geçmez; onun yanında
durur ve başka bir soruya cevap verir. Günlük **çizime ne olduğunu** yazar, denetim kaydı
**kimin neye dayanarak karar verdiğini**.

Uygulanan bir önerinin komutları günlüğe **olağan komutlar olarak** girer ve her
satırın kaynağı `ai` yazar; böylece günlüğü okuyan biri hangi satırların bir öneriden
geldiğini görür. Günlükten ilgili denetim kaydına giden doğrudan bağ **Faz 3'ün kalan
işlerindendir** (`.claude/ai.md` R27); bugün ikisi önerinin kimliği ve zamanı üzerinden
eşleştirilir.

Reddedilen bir öneri günlüğe hiç girmez — hiçbir komut çalışmadı — ama denetim kaydına
girer. İkisinin ayrı dosyalar olmasının bir sebebi de budur.

Geri almak **kararı silmez**: kayıt ve önerinin durumu olduğu gibi kalır. "Uygulandı,
sonra geri alındı" ile "hiç uygulanmadı" aynı şey değildir.

### Kayıt yazılamıyorsa

Denetim dizinine yazılamadığında program bunu **söyler**:

```text
Denetim kaydı yazılamıyor: <yol>. Yapay zeka önerileri uygulanabilir ama kaydı
tutulamaz.
```

Bu satırı gördüyseniz dizinin izinlerini düzeltmeden onay vermeyin: kaydı olmayan bir
onay, hukuki olarak savunulamayan bir onaydır.

## Onay zorlanamaz

Onayın program içinde tek bir kapısı vardır ve oradan yalnız iki şey geçer: **bir kişinin
kartta tıklaması**, ya da **o kişinin Ayarlar penceresinde kendisi için seçtiği onay
politikası**. Onay nesnesi bir argümanla, bir başlıkla, bir belirteçle ya da bir
istemcinin istediği bir ayarla üretilemez: yapıcısı özeldir, tek bir üretici işlevi
vardır ve o işlevin iki çağıranı olduğu **CI kapısıyla denetlenir**. Üçüncü bir çağıran
belirirse derleme kırılır.

Politika bir "güven kipi" değildir: onu yalnız bilgisayar başındaki kişi seçer, hiçbir
istemci değiştiremez ([aşağıda](#bir-istemci-kendi-iznini-genişletemez)), ve politikanın
verdiği her karar denetim kaydına politikanın adıyla yazılır (`CLAUDE.md` 5.7).

## Onay politikası ayarı ve `otomatik`

`Seçenekler ▸ Çalışma Davranışı` sayfasında **Yapay Zeka** başlığı altında üç ayar
vardır. Pencerede okunur adlarıyla görünürler; `TERCİH` komutu ve günlük değer adını
yazar.

**Onay politikası** (`core.ai.onay_politikasi`):

| Pencerede | Değer | Ne olur |
|---|---|---|
| Her değişiklikte onay iste | `her_degisiklikte` | Her öneri kartta sizi bekler. **Öntanımlı budur ve yükseltmede değişmez.** |
| Yalnız riskli işlemlerde onay iste | `riskli_islemlerde` | Geri alınabilir çizim değişiklikleri hemen uygulanır; var olan bir dosyanın üstüne yazmak ve bu makinenin dışına yazmak (yazdırmak) onay bekler |
| Otomatik uygula (yetki kapsamı içinde) | `otomatik` | Yetki kapsamı içindeki ve girdileri tam olan öneri hemen uygulanır |

Politika bir öneriyi uyguladığında istemciye bu **söylenir**: sohbetteki model
`UYGULANDI` yanıtını alır ve onay beklemeden işin sonraki adımına geçer; bir MCP
istemcisinin yanıtında `durum: uygulandi` ve `_meta` içinde
`cad.kentos/approval: policy-applied` yazar; döküm satırı
`(onay politikasıyla uygulandı — politika:otomatik)` der. Bekleyen bir önerinin yanıtı
ise bekleme sebebini söyler. Böylece `otomatik` seçen biri için birkaç adımlık bir iş,
arada kart tıklanmadan tek istekte biter; her öneri yine tek Ctrl+Z ile geri alınır.

**Hiçbir modda olmayan şeyler** — ve bunlar modun seçimiyle değişmez:

- **Kapsam dışındaki iş hiçbir modda yürümez ve kart açmaz.** Bir MCP istemcisi bu
  makinenin dışına yazan bir iş (bir yazdırma) isterse öneri açılmaz, istemciye sebebiyle
  reddedilir; kapsam dışı olmak onayla açılabilecek bir şey değildir. Programın kendi
  sohbeti sizin adınıza çalıştığı için yazdırma **önerebilir**; o öneri politikanızın
  istediği onayı bekler (`otomatik` dışında).
- **Girdisi eksik bir çağrı öneri olmaz**; zorunlu parametresi eksik bir çağrı daha
  öneri açılmadan, eksik parametrenin adıyla reddedilir.
- **Her karar denetim kaydına yazılır**, hangi politikanın verdiğiyle birlikte
  (`karar_veren`). Otomatik bir işlem, insan tıklaması gibi kaydedilmez.
- **Onay, kartta okunan adımlara bağlıdır.** Kart çizildikten sonra eklenen bir adım
  uygulanmaz.
- **Ayarı yalnız siz değiştirebilirsiniz.** Bir ajan kendi politikasını genişletemez.

Okuma ve görünüm her üç modda da doğrudan çalışır — onlar zaten hiçbir şeyi değiştirmez.

**Soru politikası** (`core.ai.soru_politikasi`):

| Pencerede | Değer | Model ne yapar |
|---|---|---|
| Sonucu değiştiren belirsizlikte sor | `etkili_belirsizlikte_sor` | Hangi nesne, hangi katman, hangi ölçü gibi sonucu değiştirecek bir belirsizlikte işe başlamadan tek bir soru sorar |
| Yalnız zorunlu bilgi eksikse sor | `yalniz_zorunlu` | Yalnız sonucu belirleyen bir bilgi hiçbir yerden çıkarılamıyorsa sorar; geri kalanında makul varsayımla ilerler. **Öntanımlı** |
| Sormadan varsayımla ilerle | `varsayimla_ilerle` | Soru sormadan ilerler |

Soru sormak modelin işidir; programın işi, seçtiğiniz kuralı sohbetteki modele ve MCP
istemcilerine **aynı sözlerle** söylemektir (sohbette sistem metninde, MCP'de
`server/discover` yanıtının `instructions` alanında ve `_meta` içindeki
`cad.kentos/policy`de). Model varsayım yaptığında onu yazan çağrının `varsayimlar`
alanına yazar; varsayımlar kartta, yanıtta, döküm satırında ve denetim kaydında görünür.
Hiçbir modda koordinat, koordinat sistemi ya da hedef nesne gibi sonucu belirleyen bir
bilgi uydurulmaz — model onu bulamıyorsa durur ve eksik olanı söyler.

**Üzerine yazma** (`core.ai.uzerine_yazma`) — bir önerinin yazacağı dosya zaten varsa:

| Pencerede | Değer | Ne olur |
|---|---|---|
| Sor | `sor` | Öneri onayınızı bekler, onay politikası `otomatik` olsa bile |
| Yeni bir ad üret | `yeni_ad_uret` | Dosyanın üstüne yazılmaz; öneri ` (2)`, ` (3)` … ekli boş bir ada çevrilir ve yanıtta söylenir. **Öntanımlı** |
| Üzerine yaz | `izin_ver` | Üstüne yazılabilir; onay politikası geçerlidir |

Bugün bir ajanın ulaşabildiği dosya yazan işler **yerleşim şablonu kaydı**
(`İŞŞABLONU islem=kaydet`) ve **PDF çıktısıdır** (`YAZDIR dosya=`); `KAYDET`,
`FARKLIKAYDET` ve `DIŞAAKTAR` ajana kapalıdır.

## Bir istemci kendi iznini genişletemez

Programın ayarlarının çoğu **tercihtir**: ızgara aralığı, ondalık ayracı, tema. Birkaçı
ise **yetkidir** — kimin neyi yapabileceğini belirler:

| Ayar | Ne belirler |
|---|---|
| `core.ai.onay_politikasi` | Ne sıklıkta onay sorulduğu |
| `core.ai.soru_politikasi` | Ne sıklıkta soru sorulduğu |
| `core.ai.uzerine_yazma` | Var olan bir dosyaya ne yapılacağı |
| `core.ai.hassas` | Bu projenin verisinin kurum dışına çıkıp çıkamayacağı |
| `core.mcp.port` | Dinleyicinin hangi kapıda olduğu |
| `core.mcp.belirtec_zorunlu` | Bağlanmak için belirteç istenip istenmediği |
| `core.mcp.otomatik` | Dinleyicinin kendiliğinden açılıp açılmadığı |

Bunları **yalnız bilgisayar başındaki kullanıcı** değiştirir. Bir istemci — bir ajan, bir
betik, sohbetteki model — değiştiremez; denemesi adıyla reddedilir. Reddin denetim kaydına
da yazılması **Faz 3'ün kalan işlerindendir**; bugün ret istemciye ve günlüğe yazılır. Aynı kural `MCPSUNUCU` ve `YAPAYZEKAMODELİ` komutlarının **tamamı** için geçerlidir:
bir ajanın kendi kapısını açması ya da kendi anahtar referansını yönetmesi, hangi argümanla
olursa olsun bir yetki genişletmesidir.

Sebebi kötü niyet beklemek değil. Şu davranış son derece olağandır: model bir ret alır,
yardımcı olmaya çalışır ve reti ortadan kaldıracak ayarı değiştirir. Bu bir engeli kaldıran
bir ajandır, saldırgan değil — ve cevabı, engelin ajanın durduğu yerden **erişilebilir
olmamasıdır**.

Okumak genişletme değildir: bir ajan kendi politikasını okuyabilir. Okuyamasaydı kendi
davranışını açıklayamazdı, ki bu denetim kaydının amacının tam tersidir.

## İlgili

- [`ÖNERİ`](../komutlar/suggestion.md) — önerileri listeleme ve durumlarını okuma
- [MCP sunucusu](mcp-sunucusu.md) — bir dış ajanın önerileri nasıl açtığı
- [Yapay Zeka paneli](sohbet.md) — uygulama içi sohbet ve öneri kartı
- [Komut günlüğü](../mimari/gunluk.md) — yapılan işin kaydı
- [Yapay zeka ve ajanlar](README.md) — dört kural
