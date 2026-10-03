# İfade dili — süzme, hesap ve etiket için tek dil

Bir tabloda "alanı 2000'den büyük olanlar" demek, bir sütuna "alanın yüzde kırkını yaz"
demek ve iki sütundan bir kod kurmak aynı konuşmanın üç hâlidir. PiriCAD'de bunların
**hepsi tek bir dille** yazılır; bu sayfa o dilin tamamıdır. (Etiketlerin de bu dili
konuşması planlıdır; bugün yalnız süzme çubuğu ve alan hesaplayıcı konuşur.) Sayfayı bitirdiğinizde
bir ifadeyi okuyabilecek, yazabilecek ve boş bir hücrenin neden sıfır olmadığını
bileceksiniz.

İfadeyi iki yerde yazarsınız:

- [Öznitelik tablosunun](oznitelik-tablosu.md) süzme çubuğunda — sonuç **evet/hayır**'dır,
  satır ya kalır ya gizlenir.
- [ÖZNİTELİKHESAPLA](../komutlar/attribute_calc.md) komutunda ve tablonun **Alan
  hesaplayıcı** penceresinde — sonuç bir **değer**dir, sütuna yazılır.

## Üç örnek

```text
"alan_m2" > 2000 AND "plan_fonksiyon" = 'Konut'              bir süzgeç
round("alan_m2" * 0.4, 2)                                       bir sayı
"ada" || '/' || lpad("parsel", 3, '0')                          bir metin
CASE WHEN "alan_m2" > 3000 THEN 'Büyük' ELSE 'Küçük' END       bir seçim
```

## Yazım kuralları

| Yazım | Anlamı |
|---|---|
| `"sütun_adı"` | **Çift tırnak** bir sütundur. Tırnağın içi sütunun **kimliğidir** (`alan_m2`), tabloda görünen başlık değil |
| `'metin'` | **Tek tırnak** bir metindir. İçinde tek tırnak gerekiyorsa ikiletin: `'Ali''nin'` |
| `2000`, `3.14` | Sayı. Ondalık ayracı **her zaman nokta**dır; yerel ayarınız ne olursa olsun |
| `$alan`, `$x` … | Satırın kendi değerleri; aşağıda |
| `+` `-` `*` `/` `%` | Aritmetik; `*` `/` `%` önce işler. `/` ve `%` sıfıra bölünmez |
| `\|\|` | İki metni art arda yazar |
| `=` `!=` `<>` `<` `<=` `>` `>=` | Karşılaştırma |
| `AND` `OR` `NOT` | Mantık; `AND`, `OR`'dan sıkı bağlar |
| `IS NULL` / `IS NOT NULL` | Hücre dolu mu |
| `CASE WHEN … THEN … ELSE … END` | Koşula göre değer seçer |
| `( … )` | Öncelik |

Büyük/küçük harf `AND`, `CASE` ve işlev adlarında önemsizdir. **Metin karşılaştırması
harfi harfine yapılır**: `'İSTANBUL'` ile `'ISTANBUL'` ve `'Konut'` ile `'konut'` ayrı
metinlerdir. Büyük/küçük harfe bakmadan karşılaştırmak için iki yanı da `lower(…)` ya da
`upper(…)` içine alın; ikisi de Türkçe kurallarla çalışır.

## Boş hücre sıfır değildir

Doldurulmamış bir hücre **NULL**'dır (boş, bilinmiyor). `0` ve `''` ise birer
değerdir. Bu ayrım iki yerde sonuç değiştirir:

- **Aritmetik ve `||` boşla boştur.** `"cephe" * 2`, cephe boşsa boştur; sıfır değil.
  Sonuç boşsa hesap o hücreyi **boşaltır** ve özette "boşaltıldı" diye sayılır.
- **Karşılaştırma boşla yanlıştır** — `!=` dahil. Ölçülmemiş bir cephe "5'ten farklı"
  değildir; bilinmiyordur. Boşları aramak için `IS NULL` yazın, boşa bir varsayılan
  vermek için `coalesce("cephe", 0)`.

## Satırın kendi değerleri: `$` sözcükleri

Bu değerler bir sütunda durmaz; satırın nesnesinden hesaplanır.

| Sözcük | Değer |
|---|---|
| `$fid` | Nesnenin kalıcı kimliği; silmeyle değişmez |
| `$katman` | Katmanın adı |
| `$alan` | Kapalı bir nesnenin alanı, **metrekare** |
| `$uzunluk` | Çevre ya da uzunluk, **metre** |
| `$x` | İlk köşenin X'i — Türkiye düzeninde **yukarı** (kuzey), metre |
| `$y` | İlk köşenin Y'si — Türkiye düzeninde **sağa** (doğu), metre |

`$x` ile `$y` matematik kitabının tersidir; [nokta listeleri](../komutlar/points.md) ve
koordinat okuması ile aynı düzendir.

## İşlevler

Yazım `ad(değer, …)` biçimindedir. Aşağıdaki tablo, hesap penceresinin **İşlevler**
listesiyle aynı kaynaktan gelir.

| İşlev | Ne yapar |
|---|---|
| `coalesce(a, b, …)` | İlk boş olmayan değer; hiçbiri yoksa boş |
| `if(koşul, evetse, hayırsa)` | Koşula göre iki değerden biri |
| `nullif(a, b)` | `a`, `b`'ye eşitse boş; değilse `a` |
| `round(x, basamak)` | Sayıyı verilen ondalık basamağa yuvarlar (0–12) |
| `abs(x)` | Mutlak değer |
| `floor(x)` | Aşağıya yuvarlanmış tam sayı |
| `ceil(x)` | Yukarıya yuvarlanmış tam sayı |
| `sqrt(x)` | Karekök; negatif sayıda hata |
| `pow(x, üs)` | `x`'in üssü |
| `min(a, b, …)` | En küçük değer; boşlar atlanır |
| `max(a, b, …)` | En büyük değer; boşlar atlanır |
| `upper(metin)` | Türkçe kurallarıyla büyük harf (`i` → `İ`) |
| `lower(metin)` | Türkçe kurallarıyla küçük harf (`I` → `ı`) |
| `trim(metin)` | Baştaki ve sondaki boşlukları atar |
| `length(metin)` | Karakter sayısı |
| `left(metin, n)` | Metnin ilk `n` karakteri |
| `right(metin, n)` | Metnin son `n` karakteri |
| `substr(metin, başlangıç, uzunluk)` | Metnin bir parçası; başlangıç 1'den sayılır |
| `replace(metin, ara, yerine)` | Bulunan her parçayı değiştirir |
| `lpad(metin, uzunluk, dolgu)` | Solu dolgu karakteriyle doldurur |
| `rpad(metin, uzunluk, dolgu)` | Sağı dolgu karakteriyle doldurur |
| `to_text(x)` | Değeri metne çevirir |
| `to_number(metin)` | Metni sayıya çevirir; olmazsa hata |
| `contains(metin, parça)` | Metin parçayı içeriyor mu |
| `starts_with(metin, önek)` | Metin bu önekle başlıyor mu |
| `ends_with(metin, sonek)` | Metin bu sonekle bitiyor mu |

`upper` ve `lower` işletim sisteminin dil ayarına bakmaz: `lower('IRMAK')` her bilgisayarda
`ırmak` verir.

## Sayı ve metin: hücrenin yazdığı korunur

Bir hücre `0012` yazıyorsa ve ifade onu `||` ile bir metne katıyorsa sonuç `0012` olur,
`12` değil. Ada ve parsel numaralarının başındaki sıfırlar böyle kaybolmaz. Aritmetik ise
hücreyi sayı olarak okur: `"parsel" + 1`, `0012` için `13` verir.

## Aynı sonuç her yerde

İfadenin değeri saate, bilgisayara ve yerel ayara bağlı değildir. Aynı ifade aynı
satırlar üzerinde her işletim sisteminde aynı değeri verir; bu yüzden bir hesap günlükten
yeniden oynatıldığında aynı belgeyi üretir.

İfade çok iç içe olamaz (yaklaşık 120 düzey); bu sınır bir yazım hatasının programı
çökertmesini önler.

## Hatalar

İfade okunamazsa hiçbir satır hesaplanmaz ve hiçbir hücre yazılmaz. Mesaj ifadeyi ve
yeri söyler.

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `bilinmeyen işlev '<ad>'` | Dilde böyle bir işlev yok | Yukarıdaki listeden seçin |
| `bilinmeyen sözcük '<ad>': metin tek tırnak içinde ('Konut'), sütun adı çift tırnak içinde ("alan_m2") yazılır` | Tırnaksız bir sözcük | Metne tek, sütuna çift tırnak koyun |
| `'<ad>' en az N değer ister, M verildi` (ya da `N–M`, ya da yalnız `N`) | İşlevin değer sayısı yanlış | İşlev listesindeki yazıma bakın |
| `Kapanmayan parantez` | `(` kapatılmamış | Parantezi kapatın |
| `'<ad>' işlevinin parantezi kapanmadı` | İşlev çağrısında `)` yok | `)` ekleyin |
| `İfade beklenmedik biçimde bitti` | `"ada" +` gibi yarım ifade | İfadeyi tamamlayın |
| `beklenmeyen '<işaret>'; bir işleç ya da bitiş bekleniyordu` | İki değer arasında işleç yok | Aradaki işleci yazın |
| `beklenmeyen '<işaret>'; bir değer bekleniyordu` | Değer yerine işaret | İşaretin yerine bir değer yazın |
| `'IS' sonrası beklenen: NULL veya NOT NULL` | `"beyan" IS bos` | `IS NULL` ya da `IS NOT NULL` yazın |
| `CASE sonrası WHEN bekleniyordu` | `CASE` bir koşulla başlamadı | `CASE WHEN … THEN …` yazın |
| `WHEN koşulundan sonra THEN bekleniyordu` | `THEN` yok | `THEN` ekleyin |
| `CASE ifadesi END ile bitmeli` | `END` yok | `END` ekleyin |
| `'$' ardından bir ad gelmeli ($alan, $uzunluk, $x …)` | Yalnız `$` | Bir `$` sözcüğü yazın |
| `'<yazı>' geçerli bir sayı değil` | `1.2.3` gibi | Sayıyı düzeltin |
| `sayının hemen ardından harf gelemez` | `12abc` | Sayıyla harf arasına işleç koyun |
| `ifade çok iç içe` | 120 düzeyden derin | İfadeyi bölün |
| `sıfıra bölme` / `sıfıra göre mod` | Bölen sıfır | Bölenin sıfır olmadığını `if` ile sınayın |
| `sonuç tanımsız ya da sonsuz` | Taşan ya da tanımsız işlem | Girdileri kontrol edin |
| `round: basamak sayısı 0–12 arasında bir tam sayı olmalı` | `round(x, 20)` | 0–12 arası bir sayı verin |
| `sqrt: negatif sayının karekökü alınamaz` | `sqrt` negatif sayı aldı | `abs` ya da `if` ile sınayın |
| `pow: sonuç tanımsız` | Tanımsız üs | Girdileri kontrol edin |

## İlgili

- [ÖZNİTELİKHESAPLA](../komutlar/attribute_calc.md) — ifadeyi bir sütuna yazar
- [Öznitelik tablosu](oznitelik-tablosu.md) — süzme çubuğu ve Alan hesaplayıcı penceresi
- [SÜTUN](../komutlar/column.md) — hesabın yazacağı sütunu tanımlar
