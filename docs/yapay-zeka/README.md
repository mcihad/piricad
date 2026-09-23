# Yapay Zeka ve Ajanlar

KentOSCad'i bir yapay zeka modeline ya da bir ajana kullandırmak isteyen harita
mühendisi ve şehir plancısı için; bu bölümü bitirdiğinizde bir ajanın neyi kendi başına
yapabildiğini, neyi yapamadığını, kararın nerede verildiğini ve bunun neden böyle
olduğunu bileceksiniz.

> **Ne çalışıyor.** Bu bölümün anlattığı her şey çalışır durumdadır: sekiz komut, MCP
> sunucusu ve ayar sayfası, uygulama içi **Yapay Zeka paneli**, kararın verildiği
> **öneri kartı**, tutamak deposu, denetim kaydı ve sağlayıcı profilleri. Üç şey
> gelecek zamanla yazılmıştır ve henüz yoktur: **`mevzuat_ara`** aracı, bir önerinin
> **sonucunun** tuvalde hayalet önizlemesi ve kendi ölçtüğünüz koordinat listesini
> tutamağa çeviren yol (`.claude/ai.md`).

## Tek kural

**Yapay zeka geometri üretmez; komut üretir.**

KentOSCad'de programın durumunu değiştiren her şey bir komuttur ve arayüz, komut satırı,
betik ile yapay zeka aynı komut veri yolunun **eşit istemcileridir**. Yapay zekanın
ayrıcalıklı bir yolu, hızlı bir geçidi ya da kendine ait bir "çiz" işlevi yoktur. Bir
model bir çizgi çizmek istiyorsa, sizin de yazabileceğiniz `ÇİZGİ` komutunu üretir.

Bu, yapay zekayı kısıtlayan bir karar değil, onu **öğretilebilir** kılan karardır: bir
yeteneğe yalnızca fareyle erişilebiliyorsa, o yetenek bir modele hiç öğretilemez.

## Dört kural, sırayla

Bir ajanın ilk çağrısından önce bilmesi gereken dört şey vardır. Aynı dört şey bağlanan
her istemciye sunucunun kendi ağzından da söylenir.

**1. Konum uydurulamaz.** Nokta, nokta listesi ya da nesne seçimi isteyen bir
parametreye bir ajan yalnız **tutamak** yazabilir: bir okuma aracının döndürdüğü
`@0123456789abcdef` biçiminde bir dize. Oraya sayı yazmak reddedilir, ve ret denetim
kaydına geçer. Tutamak, alındığı çizim sürümüne bağlıdır: çizim değişirse eski tutamak
kabul edilmez.

**2. Yazan bir araç bir öneri açar.** Çizimi ya da diski değiştiren bir araç çağrısı bir
**öneri** açar ve uygulanacak komut satırlarını geri döndürür. Öneriyi uygulayan
**istemci değildir**: ya bilgisayar başındaki mühendis kartta uygular, ya da o
mühendisin **önceden kendisi için seçtiği onay politikası** — `otomatik` seçildiyse
öneri hemen uygulanır ve istemciye bu söylenir. Uygulanan bir öneri tek bir işlemdir ve
tek `Ctrl+Z` ile geri alınır. Hangi yoldan geçtiği denetim kaydına yazılır
([Onay ve denetim](onay.md)).

**3. Birim tam sayı milimetredir.** Bütün koordinatlar `int64` sabit noktalı milimetre;
ondalık yoktur. 485320,15 metre `485320150` demektir, alan milimetrekaredir. Eksen
adları `Sağa (Y)` (doğuya doğru) ve `Yukarı (X)` (kuzeye doğru) ve bir nokta **doğu
önce** yazılır.

**4. Adlar Türkçedir ve katlanır.** `GİZLE`, `gizle`, `GIZLE` ve `gızle` aynı
sözcüktür. Her komutun Türkçe birincil adı, ASCII karşılığı, İngilizce karşılığı ve
kısaltması vardır.

## Hiçbir şeyi değiştirmeyen beş araç

Bunlar **doğrudan çalışır**: onay beklemez, çizime dokunmaz ve sonucunu döndürür. Bir
ajanın da, sizin de bir işe başlamadan önce sorduğunuz sorular bunlardır.

| Araç adı | Komut | Ne sorar |
|---|---|---|
| `gorunum_bilgisi` | [`GÖRÜNÜMBİLGİSİ`](../komutlar/view_info.md) | Ekranda hangi alanı görüyorum, hangi ölçekte, hangi CRS'te |
| `katmanlari_listele` | [`KATMANLAR`](../komutlar/layers.md) | Hangi katmanlar var, her birinde kaç nesne |
| `oznitelik_semasi` | [`ÖZNİTELİKŞEMASI`](../komutlar/attr_schema.md) | Hangi öznitelik sütunları tanımlı, hangi tipte |
| `sorgula` | [`SORGULA`](../komutlar/query.md) | Koşula uyan kaç nesne var, hangileri, nerede |
| `secimi_al` | [`SEÇİMBİLGİSİ`](../komutlar/selection_info.md) | Kullanıcı şu anda neyi seçmiş |

Sıralama tesadüf değil, önerilen sıradır: nerede olduğunu bilmeden ne soracağını
bilemezsiniz.

**Altıncı araç `mevzuat_ara` henüz yok.** Mevzuat araması, arkasındaki **mevzuat
derlemi** dolmadan gelemez: madde numarası ve yayım tarihi taşımayan bir mevzuat cevabı
bu programda gösterilmez, bastırılır. Derlem hazır olduğunda araç da gelecek
(`.claude/ai.md` R11, R15).

## Bu bölümdeki sayfalar

| Sayfa | İçerik |
|---|---|
| [MCP sunucusu](mcp-sunucusu.md) | Bir ajanı bağlama: adres, belirteç, protokol sürümü, hangi araçlar, hangi hatalar |
| [Onay ve denetim](onay.md) | Öneri boru hattı, tutamaklar, tek işlem–tek Ctrl+Z, denetim kaydı |
| [Yapay Zeka paneli](sohbet.md) | Uygulama içi sohbet: mesajlar, akıl yürütme metni, araç döngüsü, bağlam penceresi |
| [Model sağlayıcıları](modeller.md) | Hangi model, hangi adres, hangi lehçe; anahtarın nerede durduğu; yerel modelin neden varsayılan olduğu |
| [Lisans ve ağ yükümlülüğü](lisans.md) | Sunucu bileşeninin AGPLv3 olması ne demek, kimi bağlar |

Komut tarafı için: [`ÖNERİ`](../komutlar/suggestion.md) bekleyen önerileri okur,
[`MCPSUNUCU`](../komutlar/mcp.md) sunucuyu yönetir.

## Ajanlara verilen iki kılavuz

Bağlanan bir istemci, programın kullanım kılavuzunu **programın kendisinden** okur:

| Dosya | İçerik |
|---|---|
| [`llms.txt`](../llms.txt) | Birimler, eksen adları, tutamak kuralı, uygulama kuralı, araçların kullanım sırası |
| [`llms-full.txt`](../llms-full.txt) | Aynı özet, ardından her aracın adı, açıklaması ve parametre tablosu |

İkisi de **üretilmiş dosyalardır**: kaynakları komut kataloğudur ve `make reference` ile
yeniden üretilirler. Elle düzenlenmezler; düzenlenirse CI kapısı fark eder. Bir ajan
aynı metni sunucudan `kentoscad://llms.txt` kaynağı ya da `llms_txt` aracı olarak da
alabilir.

Neden ayrı bir kılavuz: bir JSON şeması "bu bir tam sayıdır" der ama "bu tam sayı
milimetredir, doğuya doğru olan önce yazılır ve bu değeri uyduramazsın" demez. O
cümleler bu iki dosyadadır.

## Sorumluluk

Yapay zekanın ürettiği her şey **öneridir** ve arayüzün her yerinde öyle adlandırılır.
Program hiçbir yerde bir yapay zeka çıktısını "onaylandı", "kontrol edildi" ya da
"mevzuata uygundur" diye göstermez; bu sözcükler kod düzeyinde yasaklıdır ve bir CI
kapısı onları arar.

Kadastro ve imar çıktısı hukuki bir belgedir. Altına imza atan kişi lisanslı harita
mühendisidir; bir modelin, bir belirtecin ya da bir ayarın o imzanın yerine geçmesi
programın hiçbir yerinde mümkün değildir (`CLAUDE.md` 5.7).
