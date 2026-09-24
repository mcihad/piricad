# BULDEĞİŞTİR — Yazılarda Bulma ve Toplu Değiştirme

Bir paftadaki bütün "ADA" yazılarını "Ada" yapmak, adı değişen bir mahalleyi plan
notlarının hepsinde düzeltmek ya da içinde "PARSEL" geçen her yazıyı seçip başka bir
katmana almak isteyen herkes için. Bu sayfayı bitirdiğinizde bir sözcüğü çizimin bütün
yazılarında bulmayı, değişikliği uygulamadan önce görmeyi ve hepsini tek adımda
değiştirmeyi bileceksiniz.

## Ne yapar

`BULDEĞİŞTİR`, `bul` ile verilen yazıyı çizimdeki yazıların hepsinde, soldan sağa ve
üst üste binmeden arar.

**`degistir` verilmezse bulur ve seçer.** Bulunan yazılar çizimde seçili olur ve
dökümde listelenir. Sıradaki komut — [YAZIDÜZENLE](edittext.md), [SİL](erase.md), bir
taşıma — tam da bu yazılara uygulanır. Bulmadan önceki seçim kaybolmaz:
`SEÇ mod=ÖNCEKİ` onu geri getirir.

**`degistir` verilirse önce gösterir, sonra değiştirir.** Değişecek her yazı, önceki
ve sonraki hâliyle listelenir; bu listeye **önizleme** denir. Önizleme bitene kadar
hiçbir yazı değişmez. Komut satırında program "Önizlemedeki değişiklikler uygulansın
mı?" diye sorar; betikte bu sorunun cevabı `uygula=evet` ile verilir. Değişiklik bütün
yazılara birden, tek geri alma adımıyla yazılır.

**Büyük/küçük harf Türkçe kurallarla ayrılmaz.** `ada`, "ADA 101"i de "Ada 102"yi de
bulur. Türk alfabesinde `i`'nin büyüğü `İ`, `ı`'nınki `I`'dır: `İSTANBUL` "istanbul"u
bulur, ama `irmak` "IRMAK"ı bulmaz — onun küçüğü "ırmak"tır. `buyuk_kucuk=evet` harfleri
yazıldığı gibi karşılaştırır.

**Tam kelime.** `tam_kelime=evet` ile yazı yalnız kendi başına bir kelime olarak
durduğu yerde bulunur: `ADA`, "ADA 101"de ve "«ADA»"da bulunur, "ADALET"te ve
"ADAÇAYI"nda bulunmaz. Her alfabenin harfleri ve rakamlar kelimeye dahildir; boşluk,
`/`, `-`, noktalama ve simgeler (`«»`, `…`, `°`, `²`, `⌀`) kelimeyi bitirir.

`bul` ve `degistir` içinde `\n` satır sonudur. Böylece iki satırlı bir notun satır sonu
da aranabilir ya da değiştirilebilir.

**Neye dokunmaz, ve bunu söyler:**

- **Ölçü yazısı.** Ölçünün rakamı ölçülen uzunluktur; önekini ve sonekini
  [ÖLÇÜDÜZENLE](dimension_edit.md) değiştirir.
- **Kalıptan doldurulan yazı.** Bir nesneye [bağlı](bagla.md) ve sözü `{ada}`,
  `{#alan} m²` gibi bir kalıptan gelen yazı, nesnesi bir sonraki değişiminde kalıbından
  yeniden yazılır. Böyle bir yazıya burada yazılan söz ilk fırsatta geri alınırdı. Bu
  yazılar atlanır ve sayısı söylenir; sözünü kalıbı değiştirir ([ETİKET](label.md) ya da
  [BAĞLA](bagla.md)).
- **Blok tanımının içindeki yazı.** O yazı her blok referansında görünür; bir yerde
  değiştirmek hepsini değiştirirdi.
- **Boş kalacak yazı.** Değiştirme sonunda hiç kelimesi kalmayacak ya da yalnız
  boşluktan oluşacak bir yazı yazılmaz: kendi sözünü korur ve atlandığı söylenir. Sözü
  olmayan bir yazı, paftada çıplak bir çizgiye dönüşürdü. Bir yazıyı çizimden SİL
  kaldırır.

Yazının taban çizgisi yeni söze göre uzar ya da kısalır, [YAZIDÜZENLE](edittext.md)'de
olduğu gibi. Başlangıç noktası, yönü, yüksekliği, hizalaması ve satır aralığı
değişmez.

## Adlar

| Ad | Tür |
|---|---|
| `BULDEĞİŞTİR` | Türkçe, birincil |
| `BULDEGISTIR` | ASCII karşılık |
| `FINDREPLACE` | İngilizce karşılık |
| `BUL` | Kısaltma |
| `core.find_replace` | Komut kimliği |

## Sözdizimi

```text
BULDEĞİŞTİR bul=<yazı>
BULDEĞİŞTİR bul=<yazı> degistir=<yazı>
BULDEĞİŞTİR bul=<yazı> degistir=<yazı> uygula=evet
BULDEĞİŞTİR bul=<yazı> degistir=<yazı> katman=<katman> buyuk_kucuk=evet tam_kelime=evet uygula=evet
BULDEĞİŞTİR bul=<yazı> nesneler=<kimlik> <kimlik> …
```

Kısa yazım `BUL <yazı>`: tek başına verilen kelime aranacak yazıdır. İçinde boşluk olan
yazı tırnak içine alınır.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `bul` | Aranacak yazı. `\n` satır sonudur. Boş olamaz |
| `degistir` | Yerine yazılacak. Verilmezse bulunanlar seçilir; boş (`""`) verilirse bulunan silinir |
| `katman` | Yalnız bu katmandaki yazılarda arar. Verilmezse bütün çizimde |
| `nesneler` | Yalnız bu kimliklerdeki yazılarda arar |
| `buyuk_kucuk` | `evet`: büyük ve küçük harf ayrılır. Varsayılan `hayır`; harfler Türkçe kurallarla eşlenir |
| `tam_kelime` | `evet`: yalnız kendi başına bir kelime olarak duran eşleşmeler |
| `uygula` | `evet`: önizlenen değişiklik yazılır; `hayır`: yalnız önizleme. Verilmezse komut satırında sorulur, betikte hiçbir şey değişmez |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

İçinde "ada" geçen bütün yazıları bulup seçin:

```
BUL ada
```

İki yazı çizip "ADA"yı "Ada" yapın. Komut önce önizlemeyi yazar, sonra sorar:

```
METİN noktalar=0,0 yazi="ADA 101"
METİN noktalar=0,6 yazi="ADA 102 / ADA 103"
BULDEĞİŞTİR bul=ADA degistir=Ada
```

Dökümde şu görünür; soruya `evet` yazıp Enter'a basın:

```text
2 yazıda 3 eşleşme:
  1: «ADA 101» → «Ada 101»
  2: «ADA 102 / ADA 103» → «Ada 102 / Ada 103»
Önizlemedeki değişiklikler uygulansın mı?
```

Soru sorulmadan, doğrudan uygulayarak:

```
BULDEĞİŞTİR bul=ADA degistir=Ada uygula=evet
```

Yalnız büyük harfle yazılmış ve kendi başına duran "Ada"yı değiştirin:

```
BULDEĞİŞTİR bul=Ada degistir=ADA buyuk_kucuk=evet tam_kelime=evet uygula=evet
```

Bir sözcüğü bütün yazılardan silin (baştaki boşlukla birlikte):

```
BULDEĞİŞTİR bul=" (eski)" degistir="" uygula=evet
```

Yalnız bir katmanın yazılarında. Önce katmanı yaratıp üzerine bir not yazın; öbür
katmanlardaki yazılar bu değiştirmeye girmez:

```
KATMAN ad=PLAN_NOTLARI
METİN noktalar=0,12 yazi="Çamlık Mah. 12. Sokak"
BULDEĞİŞTİR bul="Çamlık Mah." degistir="Çamlıbel Mah." katman=PLAN_NOTLARI uygula=evet
```

### Arayüz

Şeritte **Giriş ▸ Açıklama ▸ Bul ve Değiştir…**'e (aynı düğme **Açıklama ▸ Yazı**'da ve bir
yazı seçiliyken beliren **Yazı** sekmesinde de vardır) ya da **Ctrl+H**'ye basın (macOS'ta
**Cmd+Option+F**; orada Cmd+H programı gizler). Pencere açık kalır ve arkasındaki çizim
kullanılabilir durumda kalır.

1. **Bul** alanına aranacak yazıyı, **Değiştir** alanına yerine yazılacağı yazın.
   Değiştir alanı boş kalırsa bulunan sözcük silinir.
2. İsterseniz **Katman** listesinden bir katman seçin. **Büyük/küçük harf ayrılsın** ve
   **Yalnız tam kelime** kutuları komutun `buyuk_kucuk` ve `tam_kelime` parametreleridir.
3. **Tümünü Bul** bulunan yazıları çizimde seçer ve tabloda listeler.
4. **Önizle** (ya da Enter) değişecek her yazıyı tabloya **Şimdi** ve **Olacak**
   sütunlarıyla yazar. Hiçbir yazı değişmez. Kalıptan doldurulan ya da boş kalacağı için
   atlanan yazıların sayısı tablonun üstünde yazar.
5. **Tümünü Değiştir** yalnız bir önizlemeden sonra açılır. Önizlemeden sonra bir alanı,
   katmanı ya da kutuyu değiştirirseniz yeniden kapanır: tablo artık yazılacak olanı
   göstermez, bu yüzden yeniden **Önizle**'ye basmanız gerekir. Değiştirdikten sonra
   sütunlar **Önce** ve **Sonra** olur ve **Ctrl+Z** hepsini birlikte geri alır.

Tablodaki bir satıra tıklamak o yazıyı çizimde seçer.

Pencere belgeye kendisi dokunmaz. Düğmeler yukarıdaki komut satırlarını kurar ve veri
yoluna verir; önizlemede kullanılan satır `uygula=hayır`, Tümünü Değiştir'de
kullanılan `uygula=evet` taşır. Pencereden yapılan değiştirmeyle komut satırından
yapılan aynı işlemdir, aynı günlüğe düşer.

### Betik

```json
{
  "ad": "Ada yazılarını düzelt",
  "komutlar": [
    { "cmd": "core.find_replace",
      "args": { "bul": "ADA", "degistir": "Ada", "tam_kelime": true, "uygula": true } }
  ]
}
```

## Geri alma

Tek adım. Bir çağrıda kaç yazı değiştiyse **hepsi birlikte** geri alınır: söz ve taban
çizgisi eski hâline döner. Bir yazı yazılamazsa **hiçbiri** yazılmaz; işlem bütün
olarak geri sarılır.

Önizleme (`uygula=hayır` ya da soruya `hayır`) ve bulmak geri alma adımı bırakmaz.
Bulmak belgeyi değil seçimi değiştirir; bir önceki seçim `SEÇ mod=ÖNCEKİ` ile geri
gelir.

## Betikten kullanım

Komut kimliği `core.find_replace`; parametreler yukarıdaki tabloyla aynıdır. Python'dan
`cad.find_replace(find="ADA", replace="Ada", whole_word=True, apply=True)` olarak
çağrılır; öbür adlar `layer`, `objects`, `match_case`.

Betik `uygula` vermezse sorunun cevabı olmaz: önizleme yazılır, hiçbir şey değişmez
ve döküm bunu söyler. Önizleme betiğe **yapılandırılmış** olarak da döner:

| Alan | Ne |
|---|---|
| `yazi` | Değişecek (ya da bulunan) yazı sayısı |
| `eslesme` | Eşleşme sayısı; bir yazıda birden çok olabilir |
| `atlanan` | Kalıptan doldurulduğu için atlanan yazı sayısı |
| `bos_kalacak` | Boş kalacağı için atlanan yazı sayısı |
| `satirlar` | Her yazı için `nesne` (kimlik), `once`, `sonra` ve `adet` |

Bu, pencerenin tablosunu dolduran yanıtın aynısıdır.

Yapay zekâ da bu komutu çağırabilir, ama her öneri gibi **önizlenir ve onaylanır**;
onaysız hiçbir yazı değişmez.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Aranacak yazı boş olamaz.` | `bul=""` verildi | Aranacak bir yazı yazın |
| `Katman bulunamadı: '<ad>'.` | `katman=` çizimde olmayan bir ad | Katman adını Katmanlar panelinden doğrulayın |
| `'<yazı>' hiçbir yazıda bulunmadı.` | Eşleşme yok; bir hata değil, bilgi | Büyük/küçük harf ve tam kelime seçeneklerini kapatıp yeniden arayın |
| `Değiştirilmedi; uygulamak için uygula=evet verin.` | Betik `uygula` vermedi | `uygula=evet` ekleyin |
| `…; kalıptan doldurulan N yazı atlandı (kalıbı BAĞLA ya da ETİKET ile değişir)` | Bu yazıların sözü bağlı oldukları nesnenin kalıbından geliyor | Kalıbı [ETİKET](label.md) ya da [BAĞLA](bagla.md) ile değiştirin |
| `…; boş kalacak N yazı atlandı (bir yazıyı SİL kaldırır)` | Değiştirme bu yazılarda hiç söz bırakmayacaktı | Yazıyı kaldırmak istiyorsanız [SİL](erase.md) kullanın |

## İlgili

- [YAZIDÜZENLE](edittext.md) — tek bir yazının sözünü, yüksekliğini, hizalamasını değiştirir
- [METİN](text.md) — yeni yazı çizer
- [ETİKET](label.md) — yazıyı öznitelikten ve ölçülen alandan yazar
- [SEÇ](select.md) — `ÖNCEKİ` ile bulmadan önceki seçime döner
