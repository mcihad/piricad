# Komut Satırı

Klavyeden hızlı çalışmak isteyen kullanıcı için; bu sayfayı bitirdiğinizde koordinatı
dört ayrı biçimde girebilecek, satır içi hesap yapabilecek ve hata mesajlarını
çözebileceksiniz.

Komut satırı harita alanının hemen altındadır. **Bu sürümde varsayılan olarak
gizlidir**; **Ctrl+9** ile veya **Görünüm > Paneller > Komut Satırı** ile açılır, aynı
kısayolla kapanır. Açtığınızda odak doğrudan oraya gelir.

## Komut çağırmak

Komut adını yazıp **Enter**'a basın:

```
YARDIM
```

Adı yazarken satır içi tamamlama önerir. Türkçe adı, İngilizce karşılığını, kısaltmasını
veya komut kimliğini yazabilirsiniz — hepsi aynı komuta gider:

```text
ÇİZGİ        CIZGI        LINE        Ç        L        core.line
```

Büyük/küçük harf farkı yoktur; dönüşüm Türkçe kurallarına göre yapılır, yani `çizgi`
doğru şekilde `ÇİZGİ` olur.

## Koordinat girmek

Dört biçim vardır. Hepsi hem komut argümanı olarak hem de bir komut nokta beklerken
kullanılabilir.

### Mutlak koordinat

```
485320.150,4310220.400
```

Metre cinsinden, dokümanın koordinat sisteminde. Ondalık ayırıcı **noktadır**, virgül X
ile Y'yi ayırır. Boşluk kullanmayın.

### Göreli koordinat

```
@50,30
```

Bir önceki noktadan 50 metre doğu, 30 metre kuzey. Negatif değer ters yöne gider:
`@-25,0` yirmi beş metre batı.

### Kutupsal koordinat

```
@100<45
```

Bir önceki noktadan **45 grad** yönünde 100 metre. Açı, arazide aletin okuduğu gibi
**semt açısıdır**: kuzeyden başlar, saat yönünde artar ve varsayılan birimi **grad**'dır
(tam daire 400): `0` kuzey, `100` doğu, `200` güney, `300` batı. `@100<45` bu yüzden
kuzeydoğuya, kuzeye daha yakın bir doğrultuya (40,5°) gider.

Birimi tek bir koordinat için açıya **sonek** ekleyerek söylersiniz. Sonek büyük ya da
küçük harf olabilir ve parantezli bir ifadeden sonra da yazılır (`@100<(40+5)g`):

| Sonek | Birim | Örnek | Anlamı |
|---|---|---|---|
| `g` | grad | `@100<45g` | 45 grad |
| `d` | derece | `@100<45d` | 45 derece |
| `r` | radyan | `@100<0.7r` | 0,7 radyan |

Soneksiz bir açı `açı_birimi` proje ayarıyla okunur (`AYAR açı_birimi derece`). Açının
nereden başlayıp hangi yöne arttığını ise `açı_kuralı` oturum modu söyler; kısa adı
`kural`. **Matematik kuralına** — açı doğudan başlar, saat yönünün tersine artar — şöyle
geçilir:

```
MOD kural matematik
```

Bu kuralda `@100<0` doğuya, `@100<100` (grad) kuzeye gider. Önceki sürümlerin anlamı —
derece, doğudan saat yönünün tersine — şu iki satırla geri gelir:

```
MOD kural matematik
AYAR açı_birimi derece
```

Semt kuralına ve grada dönmek için modu ve ayarı varsayılanına alın:

```
MOD kural varsayilan
AYAR açı_birimi varsayilan
```

Kural ve birim yalnız **yazdığınız metni** etkiler — komut satırını, betik dizesini ve
yapay zekâ önerisini. Komut günlüğü çözülmüş koordinatı (milimetre) tutar; bu yüzden eski
bir günlük ya da betik hangi ayarla oynatılırsa oynatılsın aynı çizimi verir. `ÖLÇ`,
`APLİKASYON` ve sürüklerken kılavuz üzerinde okunan açı da aynı iki ayarla yazılır.
Ayrıntı: [Oturum modları](mode.md).

### Satır içi ifade

Koordinatın herhangi bir bileşeni parantezli bir hesap olabilir:

```
@(100*3),0
@(45.5+12.25),(80/2)
```

Böylece hesap makinesi açmadan koordinat üretirsiniz.

### Hepsi bir arada

```
ÇİZGİ 485320.150,4310220.400 @50,30 @100<45 @(100*3),0 @80<90d
```

## İfade değerlendirici

Parantez içinde ve sayı beklenen her yerde çalışır.

| İşleç | Anlamı | Örnek |
|---|---|---|
| `+` `-` | Toplama, çıkarma | `(100+50)` |
| `*` `/` | Çarpma, bölme | `(100*3)` |
| `%` | Kalan | `(100%30)` |
| `^` | Üs | `(2^10)` |
| `( )` | Gruplama | `((2+3)*4)` |
| `-` (önek) | Negatif | `(-5+2)` |

Öncelik matematikteki gibidir: `(2+3*4)` sonucu `14`, `((2+3)*4)` sonucu `20`. Üs
sağdan birleşir: `(2^3^2)` sonucu `512`.

Ondalık ayırıcı her zaman noktadır ve makinenin bölge ayarından etkilenmez.

## Anahtar=değer argümanları

Parametreleri adıyla verebilirsiniz; sıra önemli değildir:

```
KATMAN ad=PARSEL gorunur=evet kilitli=hayır renk=4281236786
YAKINLAŞ mod=ÇARPAN carpan=1.5
```

Evet/hayır değerleri için `evet`, `hayır`, `yes`, `no`, `true`, `false`, `1`, `0`
kabul edilir.

İçinde boşluk olan metin tırnak içine alınır:

```
KATMAN ad="YOL KENARI"
```

Hangi komutun hangi parametreleri aldığını [komut referansından](referans.md) veya
program içinden görebilirsiniz:

```
YARDIM komut=KATMAN
```

## Çalışan komuta değer vermek

Bir komut `İlk nokta` gibi bir istek gösterirken haritaya tıklayabilir **veya** komut
satırına koordinat yazabilirsiniz. İkisi de aynı kapıya çıkar:

```
ÇİZGİ                          ← Enter, komut nokta istemeye başlar
485320.150,4310220.400         ← Enter
@50,30                         ← Enter
@100<45                        ← Enter
                               ← Esc, komut biter
```

Komut çalışırken şeffaf bir komut yazarsanız (`YAKINLAŞ` gibi) araya girer, görünümü
değiştirir ve çalışan komut kaldığı yerden devam eder.

## Geçmiş

| Tuş | İşlev |
|---|---|
| **Yukarı ok** | Bir önceki komut |
| **Aşağı ok** | Bir sonraki komut |
| **Esc** | Satırı temizler; satır zaten boşsa çalışan komutu iptal eder |

Aynı komutu iki kez yazarsanız geçmişte bir kez durur.

Komut satırına yazdığınız her şey Transkript panelinde `> ` önekiyle görünür, böylece
oturum boyunca ne yaptığınız yukarı doğru okunabilir kalır.

## Neden tek harfli kısayol yok

`Ç` ve `L` gibi kısaltmalar komut satırına **yazılır**, kısayol tuşu değildir. Tek harfli
genel kısayol olsaydı komut satırına `ÇİZGİ` yazarken ilk harf komuta kaçardı. Kısayollar
bu yüzden `Ctrl` ile birleşiktir.

## Hata mesajları

Hata mesajları ne beklendiğini ve ne geldiğini birlikte söyler.

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Bilinmeyen komut: 'XYZ'. YARDIM yazarak komut listesini görün.` | Komut adı yanlış | `YARDIM` ile listeye bakın |
| `'core.line': 'noktalar' parametresi en az 2 değer istiyor, 1 değer geldi.` | Çizgi için tek nokta verilmiş | İkinci noktayı ekleyin |
| `'core.layer': bilinmeyen parametre 'yokboyle'. Tanımlı parametreler: ad, gorunur, kilitli, renk` | Parametre adı yanlış yazılmış | Doğru adı listeden alın |
| `'core.layer': zorunlu 'ad' parametresi eksik. Beklenen: metin` | Zorunlu parametre verilmemiş | Parametreyi ekleyin |
| `'core.line': 'noktalar' parametresi nokta listesi bekliyor. Girilen: 'abc'` | Koordinat yerine metin yazılmış | Koordinat biçimlerinden birini kullanın |
| `Beklenen: '@dx,dy' veya '@mesafe<açı'. Girilen: '@50'` | `@` sonrası eksik | `@50,0` veya `@50<0` yazın |
| `Kutupsal açı: beklenmeyen 'x' karakteri (konum 2)` | Açı sonekinde `g`, `d`, `r` dışında bir harf, ya da iki harf | Soneki düzeltin ya da kaldırın: `@100<45g` |
| `Beklenen: koordinat (x,y \| @dx,dy \| @mesafe<açı). Girilen: 'abc'` | Nokta beklenen yere koordinat olmayan bir şey girilmiş | Koordinat girin |
| `'(1+2' ifadesi: kapanmamış parantez` | Parantez kapatılmamış | Parantezi kapatın |
| `'1/0' ifadesi: sıfıra bölme` | Sıfıra bölme | İfadeyi düzeltin |
| `'abc' ifadesi: sayı bekleniyordu (konum 0)` | İfadede sayı olmayan bir şey var | İfadeyi düzeltin |
| `Komut satırında kapanmamış tırnak var.` | Tırnak açılmış kapatılmamış | Tırnağı kapatın |
| `'core.line' daha fazla argüman almıyor. Fazlalık: ...` | Komuta kapasitesinden fazla argüman verilmiş | Fazlalığı çıkarın |

Bütün mesajlar ve çözümleri: [Sorun giderme](../sorun-giderme.md).

## Bu sürümde henüz olmayanlar

Aşağıdakiler tasarımın parçasıdır ama bugün çalışmaz:

| Özellik | Ne zaman |
|---|---|
| **Ctrl+R** ile komut geçmişinde arama | Faz 1 |
| `alias.json` ile kullanıcı tanımlı kısaltmalar | Faz 1 |
| Yazarken açılan parametre ipucu balonu | Faz 1 |
| Komut ortasında nesne yakalama geçersiz kılma (`ORTA` gibi) | Faz 2 |
| Ayrılabilir transkript penceresi | Faz 2 |

## Sırada ne var

- [Komut referansı](referans.md) — bütün komutlar ve parametreleri
- [Çizgi çizme](line.md) — koordinat girişinin en çok kullanıldığı komut
- [Betik yazma](../betik/README.md) — komut satırında yaptığınızı otomatikleştirmek
