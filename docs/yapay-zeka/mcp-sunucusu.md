# MCP Sunucusu

Bir yapay zeka ajanını çalışan KentOSCad oturumuna bağlamak isteyen harita mühendisi
için; bu sayfayı bitirdiğinizde sunucuyu açmayı, istemciye hangi adresi vereceğinizi,
protokol sürümünün neden tek olduğunu, bir ajanın neyi doğrudan çalıştırıp neyi
önereceğini ve bağlanamayan bir istemcinin hatasını okumayı bileceksiniz.

> Sunucu, ayar sayfası, araç kataloğu, öneri defteri, **öneri kartı** ve denetim kaydı
> çalışır durumdadır: bir ajan bağlanır, çizime dair her şeyi okur, öneri açar ve
> önerisi bilgisayar başındaki mühendisin kartına düşer. Yalnız `mevzuat_ara`
> henüz yoktur ([Yapay zeka ve ajanlar](README.md)).

## MCP nedir

**MCP** (Model Context Protocol), bir yapay zeka modelinin ya da ajanının bir programın
yeteneklerine **araç** olarak erişmesi için kullanılan açık protokoldür. KentOSCad bu
protokolü konuşan bir sunucu **gömer**: program açıkken, aynı makinedeki bir ajan
çizime soru sorabilir ve çizimde yapılmasını istediği işi önerebilir.

Sunucunun sunduğu araçlar elle yazılmaz: **komut kataloğundan üretilir**. Bir komut
ajan arayüzüne yalnızca kaydında AI erişimi işaretlendiğinde girer, ve o işaret konduğu
anda araç listesinde, üretilmiş referansta ve `llms.txt` içinde birlikte görünür. İkinci
bir araç listesi yoktur — olsaydı, bir soruya iki farklı cevap veren iki liste olurdu.

Bir aracın adı da üretilir: komut kimliğindeki noktalar alt çizgiye çevrilir, yani
`core.line` aracın adıyla `core_line` olur. Tek istisna, adları elle konmuş beş okuma
aracıdır (`katmanlari_listele`, `oznitelik_semasi`, `sorgula`, `secimi_al`,
`gorunum_bilgisi`) ve protokol katmanının kendi `llms_txt` aracıdır.

## Sunucuyu açmak

Sunucu **kendiliğinden açılmaz**: dinleyen bir portu kullanıcı açar. Komut satırından:

```text
MCPSUNUCU islem=baslat
```

```text
MCP sunucusu açık: http://127.0.0.1:8765/mcp/<belirteç> (MCP 2026-07-28). Belirteç:
847126… (32 karakter) — tamamını Seçenekler ▸ MCP Sunucusu sayfasından kopyalayın.
```

Durumunu sormak, kapatmak ve belirteci yenilemek de aynı komuttadır
([`MCPSUNUCU`](../komutlar/mcp.md)). Aynı işleri **Analiz ▸ MCP Sunucusunu Başlat**,
**Analiz ▸ MCP Belirteci Üret** ve durum çubuğundaki **ajan hücresi** de yapar; hepsi
bu komutu çağırır.

`Seçenekler ▸ MCP Sunucusu` sayfası üç ayarı — port (`ajan_sunucu_portu`, öntanımlı
**8765**), belirtecin zorunluluğu ve açılışta kendiliğinden başlama
(`kendiliğinden_başlat`, öntanımlı kapalı) — ve bir **DİNLEYİCİ** bloğunu taşır:
durum satırı, adresi tutan salt okunur alan, `Başlat`/`Durdur`, `Yeni belirteç üret` ve
`Adresi kopyala`. **Belirtecin tamamı yalnız oradadır**, çünkü komut satırı yalnız ilk
altı hanesini yazar; adresi kopyaladığınızda program size onun bir parola olduğunu da
hatırlatır.

Projenin `core.ai.hassas` ayarı açıkken sunucu **hiç başlatılmaz** ve nedenini söyler:
verisi kurum dışına çıkamayan bir çizim, bulutta olabilecek bir istemciden bağlantı
kabul etmez.

## Adres ve belirteç

Sunucu yalnız **bu makineyi** dinler: `127.0.0.1` ve `::1`. Kurum ağından ya da
internetten erişilemez.

İstemciye vereceğiniz adres iki biçimde yazılabilir; ikisi de aynı kapıdır ve ikisi de
`POST` ister:

```text
http://127.0.0.1:<port>/mcp/<belirteç>
http://127.0.0.1:<port>/mcp        (Authorization: Bearer <belirteç>)
```

**Başlık biçimi tercih edilendir.** Belirtecin adres içinde taşınması, protokolün kendi
önerisine aykırıdır — bir adres vekil sunucu günlüklerine, tarayıcı geçmişine ve
`Referer` başlığına düşer. KentOSCad yine de kabul eder, çünkü uç nokta yerel
döngüdedir ve birçok MCP istemcisine bir adres verilebilirken bir başlık
öğretilemez. Belirteç, hangi biçimde gelirse gelsin, **hiçbir günlüğe ve hiçbir denetim
kaydına yazılmaz**; kayda yalnız sekiz haneli bir parmak izi girer, iki istemciyi
ayırt etmeye yeter, bir şeyi geri kurmaya yetmez.

Belirteçler **sabit zamanda** karşılaştırılır: yanlış bir belirtecin nerede yanlış
olduğu, cevabın süresinden anlaşılmaz.

**Belirteç varsayılan olarak zorunludur** (`core.mcp.belirtec_zorunlu`). Ayarı kasıtlı
olarak kapatan biri belirteçsiz bir uç nokta açabilir; sunucu bunu açılışta söyler —
"KORUMASIZ: belirteç istenmiyor, bu makinedeki her süreç bağlanabilir." — durumu
sorduğunuzda belirteç yerine "YOK — KORUMASIZ" yazar, ayar sayfası bunu sözle uyarır ve
**durum çubuğundaki ajan hücresi** `MCP 8765 KORUMASIZ` der.

### Tarayıcıdan gelen istek

Sunucu, gelen isteğin `Origin` başlığını **her şeyden önce** denetler ve tanımadığı bir
kökene `403` döner. Sebebi somut bir saldırıdır: DNS yeniden bağlama. Yerel döngüde
dinleyen bir sunucuya internetten erişilemez — ama kullanıcının kendi tarayıcısındaki
bir sayfa, kendi alan adını `127.0.0.1`'e çözdürüp bu uç noktaya kullanıcının
kimliğiyle istek atabilir. `Origin` başlığı, o sayfayı yerel bir istemciden ayıran tek
şeydir.

Hiç `Origin` göndermeyen bir istek kabul edilir: tarayıcılar kökenler arası istekte bu
başlığı her zaman gönderir, yerel bir istemci ise hiç göndermez. `Origin: null` —
korumalı bir çerçeve ya da `file://` bir belge — reddedilir, çünkü hiçbir kökeni
adlandırmaz.

## Protokol sürümü: yalnız 2026-07-28

Sunucu **MCP `2026-07-28`** sürümünü konuşur ve **başka hiçbir sürümü** konuşmaz. Eski
sürümler (`2025-03-26`, `2025-06-18`, `2025-11-25`) uygulanmadı ve hiçbir kod yolu
onlara düşmez.

Eski bir istemci bağlanmaya çalıştığında aldığı cevap şunu söyler:

```text
Desteklenmeyen MCP sürümü: '2025-06-18'. Bu sunucu yalnız '2026-07-28' sürümünü
konuşur; eski (legacy) akış hiç uygulanmadı.
```

Cevabın veri kısmında hatanın adı (`UnsupportedProtocolVersionError`) ve konuşulan
sürümlerin listesi de bulunur. **İstemciniz bağlanamıyorsa ilk bakacağınız yer
budur:** çözümü, istemciyi bu sürümü konuşan bir yapıya yükseltmektir.

Bu sürüm **oturumsuzdur**: `initialize` el sıkışması yoktur, protokol düzeyinde oturum
yoktur, `GET` ile açılan kalıcı akış yoktur. Her istek kendi sürümünü hem
`MCP-Protocol-Version` başlığında hem gövdesinin `_meta` alanında söyler ve **ikisi
aynı olmak zorundadır**: farklıysa istek reddedilir, çünkü arada duran iki vekil sunucu
onu iki farklı çağrı sayabilirdi. `Mcp-Method` başlığı da gövdedeki `method` ile aynı
olmalıdır.

## Sunucunun karşıladığı yöntemler

| Yöntem | Ne yapar |
|---|---|
| `server/discover` | Sunucunun kim olduğunu, sürümünü, yeteneklerini ve **Türkçe kullanım yönergesini** döndürür |
| `tools/list` | Bütün araçları şemalarıyla listeler |
| `tools/call` | Bir aracı çağırır: okuyan araç çalışır, yazan araç öneri açar |
| `resources/list` | İki kaynağı listeler: `kentoscad://llms.txt` ve `kentoscad://llms-full.txt` |
| `resources/read` | O iki kaynağın metnini döndürür |
| `subscriptions/listen` | Araç yüzeyi değişirse haber veren akışı açık tutar |

Bunların dışındaki her yöntem — `initialize`, `ping`, `logging/setLevel`,
`sampling/createMessage`, `roots/list`, `resources/subscribe` — bilinçli olarak
karşılanmaz ve adları sayılarak reddedilir. `POST` dışındaki her HTTP yöntemi `405`
alır.

## Okuyan araç çalışır, yazan araç önerir

Bu, sunucunun en önemli davranışıdır ve her araç çağrısında geçerlidir.

**Hiçbir şeyi değiştirmeyen bir araç** — beş okuma aracı ve `llms_txt` — çağrıldığı
anda çalışır ve sonucunu döndürür. Sonuçta hem insanın okuyacağı Türkçe metin hem
istemcinin okuyacağı yapılandırılmış veri bulunur, ve varsa yeni **tutamaklar** adlarıyla
sayılır.

**Çizimi ya da diski değiştiren bir araç çağrıldığında hiçbir şey yapılmaz.** Çağrı bir
öneri kaydı açar ve istemciye hemen cevap verilir: önerinin kimliği, durumu ve
uygulanacak **komut satırlarının tamamı**. Cevap bunu Türkçe olarak da söyler:

```text
Öneri kaydı açıldı: p0f3a1c7b9e4d2856 (durum: beklemede).
Uygulanacak komut satırları:
  KATMAN ad="YOL KENARI"
Çizim değişmedi. Bu satırları bilgisayar başındaki harita mühendisi uygulayana kadar
hiçbir şey uygulanmaz; uygulanırsa tamamı tek bir işlem ve tek `Ctrl+Z` olur.
```

Akışı dinleyen bir istemci, cevaptan önce bir bildirim de alır:

```text
Öneri p06f1f2d4aaee5e47 açıldı ve uygulanmadı; karar bilgisayar başındaki mühendise
ait.
```

Bunun her seferinde söylenmesinin bir sebebi var: yazdığının bir insanı beklediğini
bilmeyen bir ajan, boş sonucu başarısızlık sayıp yeniden dener — ve birinin ekranında
on bir tane aynı öneri birikir.

Geri dönen komut satırları **sizin de yazabileceğiniz satırlardır**. Bu tesadüf değil:
okuyamadığınız bir öneriden sorumlu olamazsınız.

### Bir diziyi tek onayda toplamak

Bir istemci birkaç adımı **tek bir öneride** toplayabilir: ikinci ve sonraki çağrılarda
`_meta` içindeki `plan` alanına bekleyen önerinin kimliğini yazar. Adımlar o öneriye
eklenir ve hepsi tek onayla, tek işlem olarak uygulanır. `_meta` anahtarının iki yazımı
da okunur: kısa `plan` ve protokolün istediği ön ekli biçim `cad.kentos/plan`.

### İstemci giderse

Bu protokol sürümünde **iptal, akışı kapatmaktır**. Bir istemci öneri taşıyan akışını
kapattığında öneri **geri çekilir**: kimsenin okumayacağı bir karar için birinin
ekranında beklemez. Denetim kaydı bunu "reddedildi" değil "geri çekildi" olarak yazar.

## İki ayrı hata türü

Sunucunun cevapları iki gruba ayrılır ve ayrım bir istemci için önemlidir:

| Tür | Nasıl görünür | Ne yapmalı |
|---|---|---|
| **Protokol hatası** | JSON-RPC hata cevabı; HTTP `400`, `404` ya da `500` | İstek yanlış kurulmuş. Aynı isteği yeniden denemeyin |
| **Alan hatası** | Başarılı cevap, içinde `isError` ve Türkçe bir metin | İstek doğru kurulmuş, cevap olumsuz: olmayan katman, boş seçim, eski tutamak. Başka argümanlarla yeniden deneyin |

Örnek olarak, olmayan bir katman adı bir protokol hatası değildir: `SORGULA` size
"Katman yok: 'YOKBÖYLE'. KATMANLAR ile listeyi alın." der ve istemci listeyi alıp
yeniden sorabilir.

## Koordinat reddi

Bir ajan, nokta ya da nesne isteyen bir parametreye sayı yazarsa çağrı **şema
düzeyinde** reddedilir — henüz bir argüman nesnesi oluşmadan. Ret, hangi okuma
aracının tutamak ürettiğini de söyler:

```text
`noktalar` bir TUTAMAK bekler; koordinat ya da anahtar yazılamaz. Gelen:
[[0,0],[1000,0]]. Konum her zaman bir okuma aracının sonucundan gelir: noktalar için
`sorgula`, `secimi_al` ya da `gorunum_bilgisi` çağırın ve dönen `@0123456789abcdef`
biçimindeki tutamağı buraya yazın.
```

Bu ret **denetim kaydına da yazılır**. Ayrıntısı [Onay ve denetim](onay.md)
sayfasındadır.

## Bağlanamayan istemci: hangi cevap ne demek

| Cevap | Sebebi | Çözümü |
|---|---|---|
| `405` | `GET`, `DELETE` ya da başka bir yöntem kullanıldı | Yalnız `POST`; adres tek bir uç noktadır |
| `403` | `Origin` başlığı tanınmadı (ya da `null` geldi) | Tarayıcı içinden bağlanmayın; yerel bir istemci kullanın |
| `404` | Adres bu sunucunun uç noktası değil | Yolu denetleyin: `/mcp` ya da `/mcp/<belirteç>`; yolda fazladan bölüm olmamalı |
| `401` | Belirteç yok ya da yanlış | Doğru belirteci verin; yenisini `MCPSUNUCU islem=belirtec` üretir |
| `400` + `Desteklenmeyen MCP sürümü` | İstemci başka bir protokol sürümü konuşuyor | `2026-07-28` konuşan bir istemci kullanın |
| `400` + `başlığı yok` / `aynı değil` | Başlıklar ile gövde uyuşmuyor | İstemci `MCP-Protocol-Version`, `Mcp-Method` ve gereken yerde `Mcp-Name` başlıklarını gövdeyle aynı göndermelidir |
| `404` + `Bilinmeyen yöntem` | Sunucunun karşılamadığı bir yöntem çağrıldı | Yukarıdaki altı yöntemden birini kullanın |

## İlgili

- [`MCPSUNUCU`](../komutlar/mcp.md) — sunucuyu yönetme komutu
- [`ÖNERİ`](../komutlar/suggestion.md) — bekleyen önerileri okuma
- [Onay ve denetim](onay.md) — öneri boru hattı, tutamaklar, denetim kaydı
- [Yapay zeka ve ajanlar](README.md) — dört kural ve okuma araçları
- [Lisans ve ağ yükümlülüğü](lisans.md) — sunucu bileşeni neden AGPLv3
