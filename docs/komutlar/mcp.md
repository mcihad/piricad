# MCPSUNUCU — Ajan Sunucusunu Yönetme

Bir yapay zeka ajanının programa bağlanmasını isteyen, sonra bağlantıyı kapatmak,
erişim belirtecini yenilemek ya da tek bir ajanı durdurmak isteyen harita mühendisi
için; bu sayfayı bitirdiğinizde sunucuyu başlatmayı, durdurmayı, durumunu okumayı, yeni
bir belirteç üretmeyi, bağlantıyı sınamayı, kimlerin konuştuğunu görmeyi ve birinin
yetkisini kaldırmayı bileceksiniz.

> Sunucusu olmayan bir ortamda — başsız çalıştırma, betik koşucusu — bu komut isteği
> reddeder ve nedenini söyler; tam metni [Hatalar](#hatalar) bölümünde.

## Ne yapar

**MCP sunucusunu** — yapay zeka ajanlarının programa bağlandığı yerel uç noktayı —
başlatır, durdurur, durumunu söyler, yeni bir **erişim belirteci** üretir, bağlantıyı
sınar, konuşmuş istemcileri listeler ve tek birinin yetkisini kaldırır.

| İşlem | Ne yapar |
|---|---|
| `baslat` | Dinleyiciyi açar ve bağlanılacak adresi söyler |
| `durdur` | Dinleyiciyi kapatır; yeni bağlantı kabul edilmez. **Bekleyen öneriler defterde kalır** ve kararları yine size aittir |
| `durum` | Sunucu açık mı, hangi portta, belirteç isteniyor mu |
| `belirtec` | **Yeni** bir belirteç üretir; eskisi o anda geçersizleşir. Konuşmuş istemcilerin listesi de sıfırlanır |
| `sina` | Bu adrese **gerçek bir istek** gönderir ve cevabı okur |
| `istemciler` | Bu sunucuya konuşmuş ajanları, çağrı sayılarını ve son hatalarını sayar |
| `iptal` | Tek bir istemcinin erişimini kaldırır. **Belirteç değişmez**: diğer ajanlar çalışmaya devam eder |
| `izin` | Kaldırılmış bir yetkiyi geri verir |

Sunucu yalnız **bu makineyi** dinler: `127.0.0.1` ve `::1`. Kurum ağından ya da
internetten erişilemez; bu bir ayar değil, sunucunun kendisidir.

Bağlanılacak adres iki biçimde yazılır ve ikisi de aynı kapıdır:

```text
http://127.0.0.1:<port>/mcp/<belirteç>
http://127.0.0.1:<port>/mcp        (belirteç Authorization: Bearer <belirteç> başlığında)
```

Başlık biçimi **tercih edilen** olandır. Yol biçimi, adres verilebilen ama başlık
gönderilemeyen istemciler için bilinçli bir yerel kolaylıktır; belirteç hiçbir günlüğe,
hiçbir denetim kaydına yazılmaz.

**Belirteç varsayılan olarak zorunludur.** Ayarı kasıtlı olarak kapatan biri belirteçsiz
bir uç nokta açabilir; sunucu bunu açılışta söyler ("KORUMASIZ: belirteç istenmiyor, bu
makinedeki her süreç bağlanabilir."), durum sorulduğunda belirteç yerine "YOK —
KORUMASIZ" yazar ve **durum çubuğu** o hâlde `MCP 8765 KORUMASIZ` der. Uyarının sebebi
açıktır: belirteçsiz uç nokta, bir kadastro belgesine soru soran ve ona öneri yazan bir
kapıdır.

Sunucunun konuştuğu protokol **MCP `2026-07-28`** sürümüdür ve **yalnız** o sürümdür.
Eski sürümler (`2025-03-26` … `2025-11-25`) reddedilir; ret cevabı hangi sürümün
konuşulduğunu adıyla söyler. Ayrıntısı [MCP sunucusu](../yapay-zeka/mcp-sunucusu.md)
sayfasındadır.

Bir ajan bağlandığında **okuma araçlarını doğrudan** çalıştırır, **yazan araçları
çalıştıramaz**: yazan bir araç çağrısı bir [öneri](suggestion.md) açar ve bilgisayar
başındaki mühendisi bekler.

## Adlar

| Ad | Tür |
|---|---|
| `MCPSUNUCU` | Türkçe, birincil |
| `MCPSERVER` | İngilizce karşılık |
| `MCP` | Kısaltma |
| `core.mcp` | Komut kimliği |

Bu komut ajan arayüzüne **açılmamıştır**: bağlanan bir istemci sunucuyu durduramaz,
kendi belirtecini yenileyemez ve portu değiştiremez.

## Sözdizimi

```text
MCPSUNUCU islem=durum
MCPSUNUCU islem=baslat
MCPSUNUCU islem=baslat port=<1024..65535>
MCPSUNUCU islem=durdur
MCPSUNUCU islem=belirtec
MCPSUNUCU islem=sina
MCPSUNUCU islem=istemciler
MCPSUNUCU islem=iptal ad=<istemci>
MCPSUNUCU islem=izin ad=<istemci>
```

Argümansız çağırırsanız komut önce işlemi sorar.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `islem` | Yapılacak iş. Zorunlu. `baslat`, `durdur`, `durum`, `belirtec`, `sina`, `istemciler`, `iptal` ya da `izin` |
| `port` | **Yalnız bu başlatma için** port: 1024–65535. Verilmezse ayardaki port (öntanımlı **8765**) kullanılır. Diğer işlemlerle anlamı yoktur |
| `ad` | Hangi istemci: `iptal` ve `izin` için zorunlu. Adları `islem=istemciler` ile görün |

`ad` bir **addır, parola değil**: istemcinin kendini tanıttığı ad ile belirtecin sekiz
haneli parmak izinden oluşur — denetim kaydına giren dizenin aynısı. Belirtecin kendisi
hiçbir komut argümanına, günlük satırına ya da denetim kaydına girmez.

Kalıcı ayarlar `Seçenekler ▸ MCP Sunucusu` sayfasındadır ve komut satırından
[`TERCİH`](preference.md) ile de verilir:

| Ayar | Adı | Öntanımlı | Ne yapar |
|---|---|---|---|
| `core.mcp.port` | `ajan_sunucu_portu` | `8765` | Sunucunun portu |
| `core.mcp.belirtec_zorunlu` | `belirteç_zorunlu` | açık | Bağlanmak için belirteç istenir. Kapatılırsa uç nokta bu makinedeki her sürece açılır ve durum **KORUMASIZ** olur |
| `core.mcp.otomatik` | `kendiliğinden_başlat` | kapalı | Sunucu program açılırken kendiliğinden başlar. Kapalı olması öntanımlıdır: dinleyen bir portu kullanıcı açar |

Örnek: `TERCİH ajan_sunucu_portu 9100`. Kimlikler de yazılabilir
(`TERCİH core.mcp.port 9100`) ve eski adlar (`mcp_port`, `mcp_otomatik`) çalışmaya
devam eder.

Bir de projeye ait bir ayar vardır: `core.ai.hassas`. Açıkken **sunucu hiç
başlatılmaz** ([Model sağlayıcıları](../yapay-zeka/modeller.md)).

Sözcükler Türkçe katlamayla eşleşir: `BAŞLAT` ve `baslat` aynı işlemdir.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Sunucunun durumunu sormak:

```text
MCPSUNUCU islem=durum
```

Kapalıyken cevabı açmanın yolunu da söyler:

```text
MCP sunucusu kapalı. Açmak için: MCPSUNUCU islem=baslat
```

Sunucuyu ayardaki portta açmak:

```text
MCPSUNUCU islem=baslat
```

```text
MCP sunucusu açık: http://127.0.0.1:8765/mcp/<belirteç> (MCP 2026-07-28). Belirteç:
847126… (32 karakter) — tamamını Seçenekler ▸ MCP Sunucusu sayfasından kopyalayın.
```

Satırda yazan belirtecin **ilk altı hanesi ve uzunluğudur**, kendisi değil: belirteç
hiçbir günlüğe girmez, dolayısıyla komut dökümüne de girmez. Tamamı — 32 onaltılık
hane, işletim sisteminin kendi rastgele sayı üreticisinden 128 bit — yalnız ayar
sayfasındaki kopyalama alanında durur.

Açıkken durumu sormak:

```text
MCP sunucusu açık: 127.0.0.1:8765, MCP 2026-07-28, 69 araç (64 tanesi onay ister),
belirteç 847126… (32 karakter)
```

Araç sayısı komut kataloğundan gelir ve komut eklendikçe artar; hangi araçların
sunulduğunu üretilmiş [komut referansının](referans.md) sonundaki katalog bölümü
gösterir.

Bir kereye mahsus başka bir portta açmak — ayar değişmez:

```text
MCPSUNUCU islem=baslat port=7345
```

Belirteci yenilemek. Eski belirteç o anda geçersizleşir, dolayısıyla açık sunucu
yeniden başlatılır ve bağlı istemcilerin yeni belirteci alması gerekir:

```text
MCPSUNUCU islem=belirtec
```

```text
Yeni belirteç üretildi (4c19f0… (32 karakter)). Tamamını Seçenekler ▸ MCP Sunucusu
sayfasından kopyalayın; eski belirteç artık geçersiz.
```

Bağlantıyı sınamak. Bu, motorun kendi kendini yoklaması değildir: gerçek bir soket
açılır, ayar sayfasındaki adrese gerçek bir `server/discover` isteği gider ve cevap
okunur — yani portun bağlı olduğu, belirtecin doğru olduğu ve araya başka bir programın
girmediği sınanmış olur:

```text
MCPSUNUCU islem=sina
```

```text
Bağlantı çalışıyor: 127.0.0.1:8765, MCP 2026-07-28, 69 araç. Ajana verilecek adres bu
sayfadaki adrestir.
```

Kimlerin konuştuğunu görmek. Bu protokol sürümünde **oturum yoktur** — `initialize`
el sıkışması, oturum kimliği ve açık kalan akış yok — dolayısıyla "bağlı istemci" diye
okunabilecek bir soket tablosu da yoktur. Dürüst cevap "kim konuştu ve ne zaman"dır:

```text
MCPSUNUCU islem=istemciler
```

```text
2 istemci (en son konuşan üstte):
  Claude Code #3f81ac20  41 çağrı, 6 öneri, son: tools/call
  yerel-betik #3f81ac20  3 çağrı, 1 ret, son: tools/list
      son hata: Böyle bir katman yok: 'YOKKATMAN'.
```

Tek bir ajanı durdurmak. Belirteci yenilemek **kör bir araçtır**: bütün ajanları, o
sırada birlikte çalıştığınızı da kapatır. Bu keskin olanıdır — yalnız adı verilen
istemci durur, belirteç değişmez:

```text
MCPSUNUCU islem=iptal ad="yerel-betik #3f81ac20"
```

```text
'yerel-betik #3f81ac20' artık bu sunucuya erişemiyor. Belirteç değişmedi: diğer ajanlar
çalışmaya devam eder. Geri vermek için: MCPSUNUCU islem=izin ad=yerel-betik #3f81ac20
```

Yetkisi kaldırılmış bir istemci `403` alır ve **sebebini okur**: belirteci hâlâ geçerli
olduğu için, yalnız "yasak" diyen bir cevap onu sonsuza kadar yeniden denemeye iterdi.

Kapatmak:

```text
MCPSUNUCU islem=durdur
```

```text
MCP sunucusu kapatıldı.
```

Kısaltmayla aynı iş:

```text
MCP islem=durum
```

### Arayüz

`Seçenekler ▸ MCP Sunucusu` sayfası **DİNLEYİCİ** bloğuyla açılır: durum satırı,
ajana vereceğiniz **adresi** taşıyan salt okunur bir alan ve dört düğme —
**Başlat** / **Durdur**, **Bağlantıyı sına**, **Yeni belirteç üret**, **Adresi
kopyala**. Belirtecin tamamı yalnız buradadır; komut satırı yalnız ilk altı hanesini
yazar. Sayfa, adresin bir parola olduğunu da söyler: belirteci taşır.

Altında **İSTEMCİLER** bloğu vardır: konuşmuş her ajan için bir satır — adı, çağrı ve
ret sayısı, açtığı öneri sayısı, son görülme zamanı ve son hatası. Bir satırı seçip
**Yetkisini kaldır** derseniz yalnız o ajan durur; **Yetkiyi geri ver** kararı geri
alır. Tablonun üstündeki satır her istemcinin **aynı kapsamda** çalıştığını yazar:
kaç araçtan kaçının çizimi değiştirdiğini, hepsinin önizlemeli bir öneriye
dönüştüğünü ve hiçbir ajanın bir başkasının tutamağını ya da önerisini
kullanamayacağını.

Belirteç zorunluluğu kapalıyken blok bunu sözle yazar ve geri açılacak ayarı adıyla
gösterir.

Aynı işleri menüden de yaparsınız: **Analiz ▸ MCP Sunucusunu Başlat** (sunucu açıkken
giriş **MCP Sunucusunu Durdur** olur) ve **Analiz ▸ MCP Belirteci Üret**.

Durum çubuğundaki **ajan hücresi** sunucunun hâlini sürekli gösterir:

| Görünüm | Anlamı |
|---|---|
| İçi boş halka · `MCP kapalı` | Sunucu kapalı |
| Dolu nokta · `MCP 8765` | Sunucu açık, belirteç zorunlu |
| Dolu üçgen · `MCP 8765 KORUMASIZ` | Sunucu açık, belirteç istenmiyor |
| İçi boş halka · `MCP yok` | Bu yapıda dinleyici yok; menü girişi de kapalıdır |

Hücreye tıklamak sunucuyu açar ya da kapatır. Bütün bu düğmeler ve girişler **bu
komutu** çağırır: arayüz komut veri yolunun bir istemcisidir ve kendine ait bir yolu
yoktur.

Komutu doğrudan yazmak için **Ctrl+K** ile komut aramayı açıp `MCPSUNUCU` yazın; ad
komut satırına yerleşir, işlemi yazıp **Enter**'a basarsınız. Argümansız bırakırsanız
komut işlemi sorar.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.mcp", "args": { "islem": "baslat", "port": 7345 } }
```

Durumu sormak:

```json
{ "cmd": "core.mcp", "args": { "islem": "durum" } }
```

Bir istemcinin yetkisini kaldırmak:

```json
{ "cmd": "core.mcp", "args": { "islem": "iptal", "ad": "yerel-betik #3f81ac20" } }
```

## Geri alma

`MCPSUNUCU` çizime dokunmaz: geri alınacak bir şey yoktur ve [`GERİAL`](undo.md)
listesine girmez. Sunucuyu açmak bir çizim işlemi değildir; `GERİAL` onu kapatmaz,
kapatmak için `islem=durdur` yazın.

`islem=belirtec` de geri alınamaz: üretilmiş bir belirteç geri getirilemez, yalnız
yenisi üretilir.

## Betikten kullanım

Komut betiklerde `core.mcp` kimliğiyle çağrılır. `port` betikte de **tam sayıdır**,
metin değil.

Komut **etkileşimlidir**: eksik bıraktığınız `islem` sorulur. Bir betikte soracak kimse
olmadığı için `islem`'i her zaman yazın.

Bir betik sunucuyu açabilir ama **açtığı sunucu adına karar veremez**: bir ajanın
önerisi yine öneri kalır ve yine kartta onaylanır. Sunucuyu bir betikten başlatmak, kapıyı
açmaktır; kapıdan geçenin imzasını atmak değildir.

`port` yalnız o başlatma için geçerlidir ve hiçbir ayarı değiştirmez: verilmezse
`core.mcp.port` ayarındaki port kullanılır. Kalıcı portu `Seçenekler ▸ MCP Sunucusu`
sayfasından ya da [`TERCİH`](preference.md) ile verirsiniz.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Aracı sunucusu bu yapıda bağlı değil; uygulama içinden çalıştırın.` | Komut, dinleyici bağlı olmayan bir ortamda çalıştı (başsız çalıştırma, betik koşucusu, test) | Uygulama içinden çalıştırın |
| `Bu yapıda MCP sunucusu yok (KENTOS_WITH_MCP kapalı).` | Yapı MCP dinleyicisi olmadan derlenmiş | Dinleyicisi olan bir yapı kullanın |
| `Bu proje hassas işaretli: MCP sunucusu başlatılmaz. Ayarı Seçenekler ▸ Yapay Zeka Modelleri sayfasından değiştirebilirsiniz.` | Projenin `core.ai.hassas` ayarı açık | Karar size ait: ya proje hassas kalır ve sunucu açılmaz, ya ayarı değiştirirsiniz |
| `MCP sunucusu <port> portunda açılamadı: <sebep>. Başka bir port deneyin: MCPSUNUCU islem=baslat port=<numara>` | Port başka bir program tarafından tutuluyor ya da işletim sistemi izin vermedi | Başka bir port verin |
| `MCP sunucusu porta bağlanamadı.` | Dinleyici kuruldu ama HTTP sunucusu porta bağlanamadı | Sunucuyu yeniden başlatın; sürerse portu değiştirin |
| `MCP sunucusu zaten açık: <port>` | Aynı portta ikinci `baslat` | Bir şey yapmanız gerekmez; başka bir port verirseniz sunucu o porta taşınır |
| `MCP sunucusu zaten kapalı.` | Kapalı sunucuya `durdur` | Bir şey yapmanız gerekmez |
| `Tanınmayan işlem: 'ac'. İşlemler: baslat / durdur / durum / belirtec / istemciler / iptal / izin / sina` | `islem` sekiz sözcükten biri değil (komut satırında sorulan işlem) | Sekiz sözcükten birini yazın |
| `'core.mcp': 'islem' için tanınmayan değer 'ac'. Kabul edilenler: baslat / durdur / durum / belirtec / istemciler / iptal / izin / sina` | Aynı hata, betikten geldiğinde: veri yolu gövde çalışmadan önce yakalar | Sekiz sözcükten birini yazın |
| `'iptal' için istemcinin adı gerekir: ad=<istemci>. Adları MCPSUNUCU islem=istemciler ile görün.` | `iptal` ya da `izin` adsız çağrıldı | Adı yazın; boşluk içerdiği için tırnak içinde |
| `Böyle bir istemci yok: '<ad>'.` | `izin` hiç konuşmamış ya da hiç yetkisi kaldırılmamış bir ada verildi | Adı `islem=istemciler` listesinden kopyalayın |
| `'<ad>' zaten yetkisiz.` / `'<ad>' zaten yetkili.` | Karar zaten alınmış | Bir şey yapmanız gerekmez |
| `Sunucu kapalı; sınanacak bir bağlantı yok. Açmak için: MCPSUNUCU islem=baslat` | `sina`, kapalı sunucuya verildi | Önce başlatın |
| `Sunucu 2 saniyede cevap vermedi (127.0.0.1:<port>). Sunucuyu durdurup yeniden başlatmayı deneyin.` | Dinleyici açık görünüyor ama cevap vermiyor | Durdurup yeniden başlatın |
| `Sunucu açık ama belirteci kabul etmedi (401). Ayarlar sayfasındaki adresi yeniden kopyalayın.` | Adresteki belirteç dinleyicinin istediği belirteç değil | Adresi sayfadan yeniden kopyalayın |
| `127.0.0.1:<port> cevap verdi ama bu bir KentOSCad MCP sunucusu değil. Portu başka bir program kullanıyor olabilir.` | O portta başka bir program var | Başka bir port verin |
| `'core.mcp': 'port' 1024 ile 65535 arasında olmalı, 80 geldi.` | Port aralığın dışında — bu ret **betikten** geldiğinde, komut gövdesi hiç çalışmadan verilir | 1024–65535 arasında bir port yazın |
| `MCP sunucusu 80 portunda açılamadı: The address is protected. Başka bir port deneyin: MCPSUNUCU islem=baslat port=<numara>` | Aynı port **komut satırından** verildiğinde işi işletim sistemi reddeder: 1024'ün altı ayrıcalıklıdır | 1024'ün üstünde bir port yazın |
| `'core.mcp': 'port' parametresi tam sayı bekliyor. Girilen: 'abc'` | Komut satırına sayı olmayan bir port yazıldı | Tam sayı yazın |
| `'core.mcp': 'port' parametresi tam sayı bekliyor, başka türde bir değer geldi.` | Aynı hata betikten geldiğinde: `port` alanına metin yazılmış | Betikte `"port": 7345` gibi tam sayı yazın, tırnaksız |
| `'core.mcp': bilinmeyen parametre 'adres'. Tanımlı parametreler: islem, port, ad` | Tanımlı olmayan bir parametre adı | `islem`, `port` ve `ad` dışında parametre yoktur; adres her zaman `127.0.0.1`'dir |

## İlgili

- [MCP sunucusu](../yapay-zeka/mcp-sunucusu.md) — protokol, belirteç, uç nokta, hangi araçlar
- [`ÖNERİ`](suggestion.md) — bir ajanın açtığı önerileri okuma
- [Onay ve denetim](../yapay-zeka/onay.md) — onay kuralı, tutamaklar, denetim kaydı
- [Model sağlayıcıları](../yapay-zeka/modeller.md) — hangi model, hangi adres, anahtar nerede
