# ÖNERİ — Bekleyen Yapay Zeka Önerileri

Bir ajanın ya da sohbetin önüne koyduğu önerileri görmek, durumlarını okumak ve
kararın nerede verildiğini bilmek isteyen harita mühendisi için; bu sayfayı
bitirdiğinizde bekleyen önerileri listelemeyi, bir önerinin durumunu sormayı ve
uygulama kararının neden yalnız öneri kartında verildiğini bileceksiniz.

> Öneri defteri, öneri kartı, onay kapısı ve denetim kaydı çalışır durumdadır.
> Defteri bağlamayan bir ortamda — başsız çalıştırma, betik koşucusu — komut isteği
> reddeder ve nedenini söyler; metni [Hatalar](#hatalar) bölümünde.

## Ne yapar

**Öneri** (İngilizcesi *suggestion*), bir yapay zeka istemcisinin çizimde yapılmasını
istediği işin **kaydıdır**: bir kimlik, bir durum ve uygulanacak **komut satırları**.
Öneri açıldığı anda hiçbir şey uygulanmaz.

`ÖNERİ` bu defteri okur ve kararın sonucunu taşır:

| İşlem | Ne yapar |
|---|---|
| `listele` | Bekleyen bütün önerileri, adım sayılarını, isteyen istemciyi ve komut satırlarını yazar |
| `durum` | Bir önerinin durumunu ve adım sayısını söyler |
| `uygula` | Kartta verilmiş bir "uygula" kararını taşır |
| `reddet` | Kartta verilmiş bir "reddet" kararını taşır |

**Kararın kendisi bu komut değildir.** Bir öneri ancak öneri kartındaki düğmeyle
uygulanır ya da reddedilir; komut satırı kararı veremez. Sebebi şudur: kadastro ve imar
çıktısı hukuki bir belgedir, altına imza atan kişi lisanslı mühendistir ve yapay zeka
imza atamaz. Bir komutun, bir belirtecin, bir başlığın ya da bir ayarın o tıklamanın
yerine geçmesi programın hiçbir yerinde mümkün değildir; bu, programın
değiştirilemeyen kuralıdır (`CLAUDE.md` 5.7, `.claude/ai.md` R3).

Uygulanan bir öneri **tek bir işlemdir**: adımların tamamı tek bir toplu iş içinde
çalışır, tek bir `Ctrl+Z` hepsini geri alır, aradaki bir ret hepsini geri sarar ve
çizim bit düzeyinde eski hâlinde kalır. Karar — uygula da olsa reddet de olsa —
**denetim kaydına** yazılır ([Onay ve denetim](../yapay-zeka/onay.md)).

### Bir önerinin durumları

| Durum | Anlamı |
|---|---|
| `beklemede` | Karar verilmedi; çizim değişmedi |
| `uygulaniyor` | Kart onayladı, adımlar **şu an** çalışıyor; henüz bitmedi |
| `uygulandi` | Kart onayladı, adımlar tek işlem olarak uygulandı |
| `reddedildi` | Kart reddetti; çizim değişmedi |
| `geri_cekildi` | İsteyen istemci bağlantıyı kapattı ya da defter doldu |
| `basarisiz` | Kart onayladı, program uygulayamadı; toplu iş geri sarıldı |

`uygulaniyor` bir **karar değildir**: kararla sonuç arasındaki süredir. Dört yüz parsellik
bir atlas dakikalar sürer, ve o sırada durumu soran bir istemciye `beklemede` demek
yanlıştır — "hâlâ bir insanı bekliyor" demek olur ve ajanı kullanıcıya "neden hâlâ
tıklamadınız" diye sormaya gönderir. Bu durumda cevap **biten adım** ve **toplam adım**
sayılarını da taşır.

**Yarım bir çıktı bitmiş sayılmaz.** Yazılan dosyalar, yeni sürüm ve uyarılar ancak durum
`uygulandi` olduğunda tamamdır; `uygulaniyor` hâlindeki cevap bunları hiç taşımaz.

Bir önerinin kararı **bir kere** verilir. Verilmiş bir karar değiştirilemez; gerekiyorsa
istemci yeni bir öneri açar.

Defterde en çok **32 bekleyen öneri** durur. Otuz üçüncü açıldığında **en eski
bekleyen** öneri geri çekilir — kararı verilmiş olanlara dokunulmaz, çünkü onlar birinin
hâlâ okuyabileceği bir kayıttır.

## Adlar

| Ad | Tür |
|---|---|
| `ÖNERİ` | Türkçe, birincil |
| `ONERI` | Türkçe, ASCII katlanmış |
| `SUGGESTION` | İngilizce karşılık |
| `ÖN` | Kısaltma |
| `core.suggestion` | Komut kimliği |

Bu komut ajan arayüzüne **açılmamıştır**: bir istemci kendi önerisini kendisi
uygulayamaz.

## Sözdizimi

```text
ÖNERİ islem=listele
ÖNERİ islem=durum oneri=<kimlik>
ÖNERİ islem=uygula oneri=<kimlik>
ÖNERİ islem=reddet oneri=<kimlik>
```

Argümansız çağırırsanız komut önce işlemi, sonra gerekiyorsa öneri kimliğini sorar.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `islem` | Yapılacak iş. Zorunlu. `uygula`, `reddet`, `durum` ya da `listele` |
| `oneri` | Öneri kimliği: `p` ve on altı onaltılık hane. `uygula`, `reddet` ve `durum` için gerekir; `listele` ile verilmez |

Sözcükler Türkçe katlamayla eşleşir: `LİSTELE` ve `listele` aynı işlemdir.

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Bekleyenleri görmek:

```text
ÖNERİ islem=listele
```

```text
1 bekleyen öneri:
  p0f3a1c7b9e4d2856 — 2 adım, isteyen: MCP istemcisi #7ab31c04
      KATMAN ad="YOL KENARI"
      KATMANGÖRÜNÜM islem=gizle katman=ALTLIK
```

Defter boşsa:

```text
Bekleyen öneri yok.
```

Bir önerinin durumunu sormak:

```text
ÖNERİ islem=durum oneri=p0f3a1c7b9e4d2856
```

```text
Öneri p0f3a1c7b9e4d2856: beklemede, 2 adım.
```

Uygulanırken sorarsanız nerede olduğunu da söyler:

```text
Öneri p0f3a1c7b9e4d2856: uygulaniyor, 9 adım. 4 adım bitti; henüz tamamlanmadı.
```

Bittiğinde yazdığı dosyaları sayar:

```text
Öneri p0f3a1c7b9e4d2856: uygulandi, 9 adım. Yazılan dosyalar:
    /Users/ali/isler/ada1284-atlas.pdf
```

Komut satırından uygulamayı denemek — komut kararı vermez ve nereye bakmanız
gerektiğini söyler:

```text
ÖNERİ islem=uygula oneri=p0f3a1c7b9e4d2856
```

```text
Bir öneri ancak öneri kartındaki düğmeyle uygulanır ya da reddedilir; komut satırı
kararı veremez. Kartı görmek için Pencere ▸ Yapay Zeka'yı açın.
```

### Arayüz

Bir öneri açıldığı anda komut dökümüne düşer ve damgasını da yanında taşır:

```text
Yapay zeka önerisi p0f3a1c7b9e4d2856 (öneri — uygulanmadı): 2 adım
    KATMAN ad="YOL KENARI"
    KATMANGÖRÜNÜM islem=gizle katman=ALTLIK
```

Satır dökümde **kalır**: öneri karara bağlandıktan sonra da orada durur, çünkü siz
çizime bakarken gelen bir öneri sessiz bir sürpriz olmamalıdır.

**Öneri kartı** — kararın verildiği yer — [Yapay Zeka panelindedir](../yapay-zeka/sohbet.md)
ve öneriyi açan cevabın altında görünür. Panel **Pencere ▸ Yapay Zeka**, **Analiz ▸
Yapay Zeka**, **Ctrl+Shift+A** ya da araç çubuğundaki simgeyle açılır.

Kartta uygulanacak komut satırları olduğu gibi yazılıdır; yanında koordinatların hangi
**tutamaklardan** geldiği, isteyenin adı ve varsa modelin kimliği durur. Çizim öneriden
sonra değiştiyse bir uyarı şeridi iki sürüm numarasını da yazar. İki düğme vardır:
ikincil **Reddet**, birincil **Uygula**.

Önerinin **sonucunun** tuvalde hayalet bir önizleme olarak gösterilmesi bu sürümde
yoktur ve sonraki bir fazın işidir (`.claude/ai.md` R3, `.claude/render.md`): kartta
gördüğünüz şey uygulanacak satırlardır.

Kart, programda bir insan kararının girebildiği **tek yerdir**. Kararı veren kişinin
adı — `core.ai.sorumlu` ayarından, boşsa işletim sisteminin kullanıcı adı olarak —
verdiği karar ve zamanı denetim kaydına yazılır; kayıt reddedilen öneriler için de
yazılır, çünkü aylar sonra sorulan soru genellikle mühendisin neyi **reddettiğidir**.

### Betik

Betikte bir satır olarak:

```json
{ "cmd": "core.suggestion", "args": { "islem": "listele" } }
```

Bir önerinin durumunu betikten sormak:

```json
{ "cmd": "core.suggestion", "args": { "islem": "durum", "oneri": "p0f3a1c7b9e4d2856" } }
```

`uygula` ve `reddet` betikten de karar veremez: aynı reddi alır. Bir betik bir insan
değildir, ve bu ayrımın kaybolmaması feature değil, kuraldır.

## Geri alma

`ÖNERİ islem=listele` ve `ÖNERİ islem=durum` çizime dokunmaz: geri alınacak bir şey
yoktur.

Uygulanan bir öneri **tek bir geri alma adımıdır**. On bir komutluk bir öneri de tek
`Ctrl+Z` ile geri döner, çünkü geri alma adımı önerinin adımlarına değil, hepsini
saran **toplu işe** aittir. Bu yüzden komutun kendi geri alma politikası "komuta
özel"dir: işlem sınırını komut değil, uygulanan öneri çizer.

Geri almak **kararı silmez**: denetim kaydı ve önerinin durumu olduğu gibi kalır.
"Uygulandı, sonra geri alındı" ile "hiç uygulanmadı" aynı şey değildir ve kayıt ikisini
ayırt eder.

## Betikten kullanım

Komut betiklerde `core.suggestion` kimliğiyle çağrılır. `islem` ve `oneri` betikte de
**metindir**.

Öneri kimliği `p` harfi ve on altı onaltılık haneden oluşur; **belirlenimlidir**, yani
aynı sırayla açılan öneriler aynı kimlikleri alır. Rastgele değildir, çünkü bir testin
iki çalıştırması ve bir günlüğün iki oynatması aynı kimliği üretmek zorundadır.

Komut **etkileşimlidir**: eksik bıraktığınız `islem` ve `oneri` sorulur. Bir betikte
soracak kimse olmadığı için ikisini de yazın.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bir öneri ancak öneri kartındaki düğmeyle uygulanır ya da reddedilir; komut satırı kararı veremez. Kartı görmek için Pencere ▸ Yapay Zeka'yı açın.` | `uygula` ya da `reddet` komut satırından veya betikten çağrıldı | Kararı kartta verin; komut yalnız kararın sonucunu taşır |
| `Tanınmayan işlem: 'parlat'. İşlemler: uygula / reddet / durum / listele` | `islem` dört sözcükten biri değil (komut satırında sorulan işlem) | Dört sözcükten birini yazın |
| `'core.suggestion': 'islem' için tanınmayan değer 'parlat'. Kabul edilenler: uygula / reddet / durum / listele` | Aynı hata, betikten geldiğinde: veri yolu gövde çalışmadan önce yakalar | Dört sözcükten birini yazın |
| `'uygula' için öneri kimliği gerekir: oneri=<kimlik>. Bekleyenleri ÖNERİ islem=listele ile görün.` | `uygula`, `reddet` ya da `durum` kimliksiz çağrıldı | `oneri=<kimlik>` ekleyin |
| `Öneri defteri bu yapıda bağlı değil; uygulama içinden çalıştırın.` | Komut, defterin bağlı olmadığı bir ortamda çalıştı: başsız çalıştırma, betik koşucusu, test | Komutu uygulama içinden çalıştırın |
| `Böyle bir öneri yok: 'p0000000000000000'.` | O kimlikte bir öneri yok | Kimliği `ÖNERİ islem=listele` ile alın |
| `Öneri 'p0f3a1c7b9e4d2856' zaten uygulandi.` | Kararı verilmiş bir öneriye ikinci karar veriliyor | Yeni bir öneri açtırın; verilmiş karar değiştirilemez |
| `Öneri 'p0f3a1c7b9e4d2856' zaten uygulandi; kararı değiştirilemez.` | Aynı durum, defterin kendi reddi | Yeni bir öneri açtırın |
| `Öneri 'p0f3a1c7b9e4d2856' artık beklemiyor (uygulandi); yeni bir öneri açın.` | İstemci, kararı verilmiş bir öneriye adım eklemeye çalıştı | İstemci yeni bir öneri açar |
| `Öneri boş; uygulanacak adım yok.` | Adımsız bir öneri uygulanmak istendi | İstemci adımları öneriye eklemeden onay istememelidir |
| `Boş öneri kaydedilmez; en az bir adım gerekir.` | Adımsız bir öneri açılmak istendi | Aynı sebep; istemci tarafındaki hatadır |
| `Çok fazla bekleyen öneri: en eskisi geri çekildi.` | Defterdeki bekleyen öneri sayısı 32'ye ulaştı | Bekleyenleri karara bağlayın; geri çekilen öneri yeniden istenebilir |
| `İstemci bağlantıyı kapattı.` | Öneriyi açan istemci gitti; öneri geri çekildi | Karar gerekmez; istemci yeniden bağlanıp yeniden önerir |
| `Öneri uygulayıcı bağlı değil; bu yapıda uygulanamaz.` | Onay kapısına adımları çalıştıracak taraf bağlanmamış | Uygulama içinden çalıştırın |
| `Denetim kaydı yazılamıyor: <yol>. Yapay zeka önerileri uygulanabilir ama kaydı tutulamaz.` | Denetim kaydı dizinine yazılamıyor | Dizin izinlerini düzeltin; kayıtsız onay hukuki olarak savunulamaz |

## İlgili

- [`MCPSUNUCU`](mcp.md) — ajanların bağlandığı sunucu
- [Onay ve denetim](../yapay-zeka/onay.md) — öneri boru hattı, tutamaklar, denetim kaydı
- [Yapay Zeka paneli](../yapay-zeka/sohbet.md) — sohbet ve öneri kartı
- [`GERİAL`](undo.md) — geri alma
- [Komut günlüğü](../mimari/gunluk.md) — yapılan işin kaydı
