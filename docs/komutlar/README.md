# Komut Sistemi

KentOSCad'i günlük işinde kullanan herkes için; bu sayfayı bitirdiğinizde arayüzde
yaptığınız her şeyi neden betikle tekrarlayabildiğinizi ve komut günlüğünün size ne
kazandırdığını bileceksiniz.

## Tek kural

> Uygulamanın durumunu değiştiren her şey bir komuttur.
> Arayüz, komut veri yolunun sadece bir istemcisidir.

Çizim yapmak, katman açmak, nesne silmek, görünümü değiştirmek — hepsi komuttur. Araç
çubuğundaki düğme de bir komut gönderir, komut satırına yazdığınız satır da, betiğinizin
her satırı da.

```
   Arayüz düğmesi ─┐
   Komut satırı ───┤
   Betik ──────────┼──►  KOMUT VERİ YOLU  ──►  Doğrulama ──►  İşlem ──►  Çizim
   AI ─────────────┤                                │
   Toplu iş ───────┘                                └──►  Komut Günlüğü
```

## İstemciler eşittir

Bu diyagramdaki hiçbir istemcinin ayrıcalığı yoktur. Arayüz düğmesi `ÇİZGİ` komutunu,
bir betiğin çağırdığı biçimde çağırır.

Bu bir tasarım iddiası değil, sınanan bir gerçektir: aynı `ÇİZGİ` komutu arayüzden fare
tıklamalarıyla, komut satırından yazılarak ve JSON betiğinden çalıştırıldığında **tıpatıp
aynı çizimi ve tıpatıp aynı günlüğü** üretir. Bu eşitlik her derlemede otomatik olarak
sınanır.

Size ne kazandırır:

- **Elle yaptığınız işi otomatikleştirebilirsiniz.** Arayüzde bir kez yaptığınızı
  günlükten alıp betiğe çevirirsiniz; ikinci kez elle yapmazsınız.
- **Yaptığınızı geri izleyebilirsiniz.** "Bu çizgi neden burada?" sorusunun cevabı
  günlükte durur.
- **Destek talebine oturumunuzu ekleyebilirsiniz.** Sorunu tarif etmek yerine kaydını
  gönderirsiniz.

## Komutun yolculuğu

Bir komut gönderdiğinizde sırayla şunlar olur:

1. **Çözümleme.** Yazdığınız ad karşılığı olan komuta çevrilir. `ÇİZGİ`, `CIZGI`,
   `LINE`, `Ç`, `L` ve `core.line` aynı komuttur.
2. **Doğrulama.** Argümanlar komutun bildirdiği parametrelere göre denetlenir: eksik
   zorunlu parametre, bilinmeyen parametre, yanlış tip, yetersiz sayıda değer burada
   yakalanır. Doğrulama **veri yolunda** çalışır; hiçbir istemci onu atlayamaz.
3. **İşlem.** Komutun yaptığı bütün değişiklikler tek bir işlem içinde toplanır.
4. **Günlük.** Komut, argümanlarıyla birlikte günlüğe yazılır.

Doğrulama başarısız olursa **hiçbir şey uygulanmaz.** Yarım uygulanmış bir ifraz ya da
tevhit kabul edilemez; ya hepsi olur ya hiçbiri.

## Bir komut, bir geri alma adımı

Dört noktalı bir `ÇİZGİ` üç ayrı segment yaratır ama tek bir `GERİAL` ile tamamen kalkar.

Bir **betik bloğunun tamamı da tek bir adımdır.** Dokuz komutluk bir betiği
çalıştırdıktan sonra bir kez **Ctrl+Z** yapmak, betiğin tamamını geri alır.

İstisnalar açıkça bildirilir: `YAKINLAŞ`, `GERİAL`, `YİNELE` ve `YARDIM` çizimi
değiştirmedikleri için geri alma yığınına girmez.

## Komut adları

Her komutun Türkçe birincil adı, İngilizce karşılığı ve kısaltmaları vardır.

```text
ÇİZGİ = CIZGI = LINE = Ç = L
KATMAN = LAYER = KAT
SEÇ  = SEC = SELECT = S
SİL  = SIL = ERASE = E
```

Büyük/küçük harf farkı yoktur ve dönüşüm Türkçe kurallarına göre yapılır: `çizgi` yazmak
`ÇİZGİ` ile aynıdır. Türkçe karakter yazamadığınız bir klavyede `CIZGI` ve `SIL`
karşılıkları da çalışır.

Komut kimliği (`core.line`) de doğrudan yazılabilir; betikler ve otomasyon bunu kullanır
çünkü kimlik hiçbir zaman değişmez.

## Şeffaf komutlar

Bazı komutlar başka bir komut çalışırken araya girebilir. `YAKINLAŞ` böyledir: çizgi
çizerken görünümü değiştirip kaldığınız yerden devam edebilirsiniz.

## Salt okunur komutlar

Çizimi değiştirmeyen komutlar salt okunur işaretlidir: `YAKINLAŞ`, `GERİAL`, `YİNELE`,
`YARDIM`, `SEÇ`, `MOD` ve `TERCİH`. Geri alma yığınına girmezler ve komut günlüğüne
belge değişikliği olarak yazılmazlar.

Seçim, görünüm, oturum modları ve uygulama tercihleri çizimin verisi değildir; bu
yüzden dosyaya yazılmaz, içerik özetine girmez ve `GERİAL` ile geri alınmaz.

## Toplu iş

Bir betik yüz bin nesne yaratırken her komut için ayrı doğrulama ve ayrı geri alma kaydı
tutulmaz. Betiğin tamamı tek bir toplu iş olarak yürür: tek doğrulama geçişi, tek geri
alma adımı. Bu yüzden büyük betikler hızlı çalışır ve tek hamlede geri alınır.

## Komutu iptal etmek

Girdi bekleyen bir komut **Esc**, sağ tık veya araç kutusundaki **Seç** düğmesiyle
iptal edilir. Hiçbir komut çalışmıyorken **Esc** seçimi temizler. Hiçbir şey çizilmeden iptal edilen komut sanki hiç çalışmamış gibidir:
geri alma adımı bırakmaz, günlüğe de yazılmaz. Transkriptte görürsünüz:

```text
İptal edildi
```

Bir şey çizdikten sonra **Esc**'e basarsanız çizdiğiniz kalır ve normal biçimde tek bir
geri alma adımı olur.

## Nereye bakmalı

| İhtiyaç | Sayfa |
|---|---|
| Koordinat girişi, ifadeler, geçmiş | [Komut satırı](komut-satiri.md) |
| Bütün komutların listesi ve parametreleri | [Komut referansı](referans.md) |
| Betik yazmak | [Betik yazma](../betik/README.md) |
| Günlüğü okumak ve kullanmak | [Komut günlüğü](../mimari/gunluk.md) |
| Bir hata mesajının anlamı | [Sorun giderme](../sorun-giderme.md) |

Program içinden de bakabilirsiniz: `YARDIM` bütün komutları, `YARDIM komut=ÇİZGİ` tek bir
komutun parametrelerini yazar. Bu listeler komut kaydından üretilir; elle tutulan ikinci
bir liste yoktur, dolayısıyla hiçbir zaman güncelliğini yitirmez.
