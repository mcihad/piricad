# YAZDIR — Yazdırma ve PDF

Çizimin bir bölümünü kâğıda ya da PDF'e, istediği ölçekte çıkarmak isteyen herkes
için; bu sayfayı bitirdiğinizde yazdırma alanını seçmeyi, ölçeği ve merkezi
vermeyi, çıktıyı yazıcıya ya da şifreli bir PDF'e göndermeyi bileceksiniz.

## Ne yapar

`YAZDIR`, çizimin bir **penceresini** bir **yazdırma profilinin** kâğıdına yerleştirir
ve bunu ya bir **PDF dosyasına** yazar ya da bir **yazıcıya** gönderir. Kâğıdı, yönü,
çözünürlüğü ve kenar boşluğunu profil söyler
([`YAZDIRMAPROFİLİ`](print_profile.md)); bu komut nereyi, hangi ölçekte ve nereye
sorularını yanıtlar.

Yazdırılacak alan iki biçimde verilir:

- **Merkez ve ölçek** — `merkez=<nokta> olcek=<N>`: kâğıt o noktada ortalanır ve
  1/N ölçeğinde çıkar. Paftanın dili budur; `olcek` verilmezse projenin plan
  ölçeği ([`AYAR plan_ölçeği`](setting.md)) kullanılır.
- **İki köşe** — `pencere=<x1,y1> pencere=<x2,y2>`: verilen dikdörtgen kâğıda
  sığdırılır. Arayüzdeki çerçeve bunu üretir. İkisi birlikte verilirse **merkez**
  geçerlidir.

Verilen alan kâğıdın en–boy oranına göre **büyütülerek** oturtulur: çizim asla
gerilmez, gerekiyorsa kâğıda biraz daha yer girer.

Çizim, ekrandaki ile aynı boru hattından geçer: aynı semboloji, aynı çizgi
kalınlıkları, profilin çözünürlüğünde. Ekranda gördüğünüz kâğıt çıkan kâğıttır.

> **PDF şifreleme** `qpdf` ile yapılır ve `KENTOS_WITH_QPDF` kapalı derlenmiş bir
> yapıda yoktur; o zaman `sifre`, `sahip_sifresi` ve `yazar` verilirse komut
> nedenini söyleyerek durur.

## Adlar

| Ad | Tür |
|---|---|
| `YAZDIR` | Türkçe, birincil |
| `PRINT` | İngilizce karşılık |
| `PLOT` | İngilizce karşılık (CAD alışkanlığı) |
| `YZDR` | Kısaltma |
| `core.print` | Komut kimliği |

## Sözdizimi

```text
YAZDIR
YAZDIR merkez=<nokta> olcek=<N> dosya=<yol>
YAZDIR pencere=<x1,y1> pencere=<x2,y2> yazici=<ad>
YAZDIR merkez=<nokta> profil=<ad> dosya=<yol> sifre=<şifre> kopyalanabilir=hayır
YAZDIR yerlesim=<ad> dosya=<yol>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `yerlesim` | Basılacak [çıktı yerleşiminin](layout.md) adı. Kâğıt, kenar ve harita penceresi yerleşimden gelir; `pencere`, `merkez`, `olcek` ve `profil` ile birlikte verilmez |
| `merkez` | Kâğıdın ortalanacağı nokta; `pencere` yerine kullanılır |
| `olcek` | Ölçek paydası (1000 = 1/1000); `merkez` ile; verilmezse projenin plan ölçeği |
| `pencere` | Yazdırılacak alanın iki köşesi; `merkez` de yoksa arayüz tıklatır |
| `dosya` | PDF yazılacak dosyanın yolu; `yazici` ile birlikte verilmez |
| `yazici` | Yazıcının adı; `""` sistem varsayılanı. `dosya` ile birlikte verilmez |
| `profil` | Yazdırma profili; verilmezse varsayılan profil |
| `kagit` | Profili geçersiz kılar: `A5`, `A4`, `A3`, `A2`, `A1`, `A0` ya da `ozel` |
| `genislik` | `ozel` kâğıdın eni, milimetre (dikey duruşta) |
| `yukseklik` | `ozel` kâğıdın boyu, milimetre (dikey duruşta) |
| `yon` | `dikey` ya da `yatay`; profili geçersiz kılar |
| `dpi` | Çözünürlük, 72–4800; profili geçersiz kılar |
| `kenar` | Dört yandaki kenar boşluğu, milimetre; profili geçersiz kılar |
| `baslik` | PDF belge başlığı |
| `yazar` | PDF yazar alanı |
| `sifre` | PDF'i açmak için istenen şifre (kullanıcı şifresi); günlüğe yazılmaz |
| `sahip_sifresi` | İzinleri değiştirmek için istenen şifre; boşsa açma şifresiyle aynı |
| `yazdirilabilir` | Şifreli PDF: şifresiz okuyucu yazdırabilir mi; varsayılan `evet` |
| `kopyalanabilir` | Şifreli PDF: metin ve grafik kopyalanabilir mi; varsayılan `evet` |
| `degistirilebilir` | Şifreli PDF: belge değiştirilebilir mi; varsayılan `evet` |

### Eski ad: `pafta=`

`yerlesim=` parametresinin adı önceden `pafta` idi. Eski ad **okunur**, yeni ad
**yazılır**; ikisi bir arada verilmez.

## Örnekler

### Komut satırı

Bir parselin çevresini 1/500 ölçeğinde PDF'e:

```text
YAZDIR merkez=485340,4310235 olcek=500 dosya=pafta.pdf baslik="Ada 128 Parsel 7"
```

Aynı alanı A3 yatay bir profille ve sistem varsayılanı yazıcıya:

```text
YAZDIR merkez=485340,4310235 olcek=500 profil="A3 Yatay" yazici=""
```

Antetli, lejantlı bir çıktı yerleşimini basmak — kâğıt, kenar ve harita penceresi
yerleşimden geldiği için başka hiçbir şey söylenmez:

```text
YAZDIR yerlesim="Ada 1284" dosya=ada1284.pdf
```

İki köşe ile, şifreli ve sadece yazdırmaya izinli bir PDF:

```text
YAZDIR pencere=485300,4310200 pencere=485400,4310280 dosya=gizli.pdf sifre=2026 kopyalanabilir=hayır degistirilebilir=hayır
```

### Arayüz

Araç çubuğunda **Yazdır** simgesine (Ctrl+P) basın: tuvalin ortasında, varsayılan
profilin kâğıt oranında bir **yazdırma çerçevesi** açılır, dışı grileşir. Haritayı
sürükleyip tekerlekle yaklaşarak çerçeveye ne gireceğini seçin; çerçevenin
ortasındaki **+** işareti kâğıdın merkezini, yanındaki sayılar o noktanın `Sağa (Y)`
ve `Yukarı (X)` değerlerini gösterir. Çerçeve ekranda hep aynı boydadır: yaklaşınca
içine daha az yer girer.

**Yazdır**'a ikinci kez basmak (ya da **Enter**) o görüntüyü yakalar ve **önizleme
penceresini** açar: solda kâğıdın kendisi, sağda profil, ölçek, merkez ve çıktı.
**Kâğıda giden alan çerçevenin alanıdır** — pencere ölçeği kendi başına
yuvarlamaz; satırı da çerçevenin iki köşesiyle gönderir
(`pencere=… pencere=…`). Ölçeği ve merkezi elle yazabilirsiniz ya da ölçeğin
yanındaki **Yuvarla** düğmesiyle bir pafta ölçeğine (1/184 → 1/200) çıkarabilirsiniz;
o anda satır `merkez=… olcek=…` olur ve alanı artık bu ikisi belirler. Önizleme her
iki durumda da gidecek olanı gösterir. **Esc** ya da **sağ tık** çerçeveyi kapatır.

Simgenin yanındaki küçük ok profilleri listeler: birine basmak o profille çerçeve
açar, **Profilleri Yönet…** `Seçenekler ▸ Plot ve Çıktı`'yı açar.

### Betik

```json
{
  "ad": "Pafta çıktısı",
  "komutlar": [
    { "cmd": "core.print",
      "args": { "merkez": [485340000, 4310235000], "olcek": 500,
                "profil": "A3 Yatay", "dosya": "pafta.pdf",
                "baslik": "Ada 128 Parsel 7", "yazar": "Harita Mühendisi" } }
  ]
}
```

## Geri alma

`YAZDIR` çizime dokunmaz: geri alınacak bir şey yoktur ve
[`GERİAL`](undo.md) listesine girmez. Yazdığı PDF dosyası diskte kalır; istemezseniz
dosyayı silin.

## Betikten kullanım

Betikte `merkez` (ve istenirse `olcek`) ya da `pencere` verilmelidir; arayüzdeki
çerçeve betikte yoktur. `merkez` ve `pencere` betikte **milimetre tam sayısıdır**.

Yazdırma komutu **komut günlüğüne yazılmaz.** Sebebi tek cümlede şudur: günlük
çökme kurtarmada yeniden oynatılır ve yeniden oynatılan bir yazdırma, kimsenin
istemediği bir anda birinin PDF'ini yeniden yazardı. Aynı karar [`AÇ`](open.md) ve
[`KAYDET`](save.md) için de geçerlidir. Bunun bir yan sonucu şudur: **şifreler
günlüğe hiç ulaşmaz.**

Şifreyi komut satırına yazarsanız o satır oturumun komut geçmişinde kalır (Yukarı
okla geri gelir). Şifre vermenin temiz yolu önizleme penceresidir: kutudaki
karakterler nokta olarak görünür ve satır ekranda yazılıdır ama geçmişe girmez.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Çıktının yeri verilmedi: PDF için dosya=<yol>, yazıcı için yazici=<ad> yazın …` | Ne `dosya` ne `yazici` verildi | Birini verin |
| `yerlesim ile 'X' birlikte verilmez: çıktı yerleşimi kendi kâğıdını ve kendi harita penceresini taşır.` | `yerlesim` ile `pencere`, `merkez`, `olcek` ya da `profil` birlikte verildi | Yalnız `yerlesim=` bırakın |
| `Çıktı yerleşimi yok: 'X'. ÇIKTIYERLEŞİMİ islem=listele ile adları görün.` | O adda yerleşim yok | Adı listeden alın |
| `'X' ve 'Y' aynı parametrenin iki adı; ikisi birden verilmez. Yeni adı 'Z'.` | Bir parametrenin eski ve yeni adı birlikte verildi | Yalnız yeni adı bırakın |
| `dosya ve yazici birlikte verilemez; çıktı ya PDF dosyasına ya yazıcıya gider.` | İkisi birlikte verildi | Birini silin |
| `Yazdırma penceresinin iki köşesi bir dikdörtgen çizmeli; iki köşe aynı doğru üzerinde.` | `pencere` bir çizgi verdi | İki farklı köşe verin |
| `Ölçek bilinmiyor: olcek=<N> verin (1:N) ya da projenin plan ölçeğini ayarlayın.` | `merkez` verildi, ölçek hiçbir yerden okunamadı | `olcek=` verin |
| `Böyle bir yazdırma profili yok: 'X'. Profiller: …` | `profil` adı yanlış | Listeden bir ad yazın |
| `Tanınmayan kâğıt: 'X'. Kâğıtlar: …` | `kagit` tablonun dışında | A5–A0 ya da `ozel` |
| `Kenar boşluğu kâğıtta yazdırılacak yer bırakmıyor: …` | `kenar` kâğıdı tüketiyor | Küçültün |
| `Bu ölçekte kâğıda sığacak bir alan çıkmıyor: olcek=…` | Ölçek ya da kâğıt tutarsız | Ölçeği büyütün |
| `Yazıcı bulunamadı: 'X'. Yazıcılar: …` | Böyle bir yazıcı yok | Listeden seçin ya da `yazici=""` |
| `Sistemde varsayılan yazıcı yok. Yazıcılar: …` | `yazici=""` verildi, varsayılan yok | Yazıcıyı adıyla verin |
| `PDF yazılamadı: dizin yok — …` | Hedef dizin yok | Dizini oluşturun |
| `Bu yapı PDF şifreleme ve yazar alanını içermiyor (KENTOS_WITH_QPDF). …` | qpdf'siz derlenmiş yapı | Şifresiz yazın ya da qpdf ile derleyin |
| `Yazdırma motoru bağlı değil; bu ortamda yazdırılamaz ve PDF alınamaz. …` | Arayüz olmadan çalıştırıldı | Uygulama içinden çalıştırın |

## İlgili

- [Yazdırma ve PDF](../baslangic/yazdirma.md) — çerçeve, önizleme ve profiller
- [`ÇIKTIYERLEŞİMİ`](layout.md) — antetli, lejantlı, ızgaralı bir sayfa kurmak
- [`YAZDIRMAPROFİLİ`](print_profile.md) — kâğıt, yön, çözünürlük, kenar boşluğu
- [`DIŞAAKTAR`](export.md) — çizimi başka bir veri biçimine yazma
- [`AYAR`](setting.md) — projenin plan ölçeği
