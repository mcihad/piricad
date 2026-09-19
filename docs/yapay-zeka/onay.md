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
5. **Açık onay** — insan uygular ya da reddeder.
6. **Tek işlem** — onaylanan dizi tek bir toplu işte uygulanır.
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
kullanıcının seçtiğini, `sorgula` eşleşenleri, `gorunum_bilgisi` pencerenin köşelerini
verir. Sayılar çizimin kendi sayılarıdır.

### Üç tür tutamak

| Tür | Ne taşır | Kim üretir |
|---|---|---|
| nokta | Bir ya da daha çok koordinat | Okuma araçları |
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

Bir öneri açıldığında kart, onu açan cevabın altında görünür
([Yapay Zeka paneli](sohbet.md)). Kesik çizgili bir kenarı ve **ÖNERİ** rozeti vardır;
şunları gösterir:

| Kartta ne var | Ne söyler |
|---|---|
| Başlık | `Öneri p0f3a1c7b9e4d2856 · 2 adım` ve önerinin durumu |
| Komut satırları | Uygulanacak satırların **tamamı**, sizin de yazabileceğiniz hâlleriyle |
| `Koordinat kaynağı` | Adımlardaki konumların hangi tutamaklardan geldiği |
| `İsteyen` | İstemcinin adı, varsa modelin kimliği |
| Uyarı şeridi | Çizim öneriden sonra değiştiyse: `Öneri 12 numaralı sürüme göre hazırlandı, çizim şimdi 14.` |
| **Reddet** / **Uygula** | Kararın kendisi. `Uygula` karttaki tek birincil düğmedir |

Uyarı şeridi göründüğünde adımlar, hazırlandıkları çizimden **başka** bir çizime
uygulanacak demektir: içlerindeki nesne anahtarları hâlâ geçerli olabilir ama artık
başka bir şeyi gösteriyor olabilir. Uygulamadan önce satırları gözden geçirin.

Kart, programda bir insan kararının girebildiği **tek yerdir**: onay nesnesini
üretebilen tek çağıran odur ve bunu bir CI kapısı denetler. Bir öneri artık defterde
yoksa kart bunu söyler ve düğmeleri kapatır.

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
| `karar` | `uygula`, `reddet`, `geri_cek` ya da `koordinat_reddi` |
| `kullanici` | Bilgisayar başındaki kişi: kararı veren. `core.ai.sorumlu` ayarından gelir |
| `isteyen` | İsteyen istemcinin adı ve belirtecinin **parmak izi** |
| `model` | Modelin kimliği ve sürümü; düz bir MCP istemcisinde boş kalır |
| `uc_nokta` | Sağlayıcı ve uç nokta, ya da istemcinin bildirdiği ad |
| `istem` | Ne istendiği, istemcinin kendi ifadesiyle |
| `komutlar` | Komut satırları, çalışacakları hâliyle |
| `sonuc` | Uygulandığında ne olduğu ya da neden uygulanmadığı |
| `olusan_nesneler` | Onaylanan önerinin oluşturduğu **kalıcı nesne anahtarları** |

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
durumuna geçer; kendi denetim satırını yazması **Faz 3'ün kalan işlerindendir**
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

Onayın program içinde tek bir kapısı vardır ve o kapıdan geçen şey **bir insanın
tıklamasıdır**. Onay nesnesi bir argümanla, bir başlıkla, bir belirteçle ya da bir
ayarla üretilemez: yapıcısı özeldir, tek bir üretici işlevi vardır ve o işlevin tek bir
çağıranı olduğu **CI kapısıyla denetlenir**. İkinci bir çağıran belirirse derleme kırılır.

"Güven kipi", "hep onayla" ya da "bir daha sorma" diye bir ayar yoktur ve eklenemez
(`CLAUDE.md` 5.7).

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
betik, sohbetteki model — değiştiremez; denemesi adıyla reddedilir ve ret denetim kaydına
girer. Aynı kural `MCPSUNUCU` ve `YAPAYZEKAMODELİ` komutlarının **tamamı** için geçerlidir:
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
