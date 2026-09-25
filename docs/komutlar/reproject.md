# DÖNÜŞTÜR — Koordinat Sistemi Dönüşümü

## Ne yapar

Çizimin **tamamını** bir koordinat sisteminden diğerine taşır ve belgenin
koordinat sistemi etiketini de yeni sisteme çevirir.

Türkiye'de günlük bir iştir: kadastro arşivi ED50 paftalarıyla dolu, her yeni
pafta TUREF; bir belediyenin katmanları yan paftadan farklı bir üç derecelik
dilimde olabilir.

Dönüşümü **PROJ** yapar. Bir datum kaymasını yeniden yazmak, bir sınırın kimse
fark etmeden yarım metre kaymasının yoludur.

## Projeksiyonlu sistemler arasında

Çizim geometrisi **tam sayı milimetredir**. Derece bu birime sığmaz:
`29,830716°` en yakın "milimetreye" yuvarlandığında nokta yaklaşık **yüz metre**
kayar. Bu yüzden bir ucu coğrafi olan dönüşüm reddedilir:

> `Bu dönüşümün bir ucu coğrafi (derece)… Projeksiyonlu bir hedef seçin.`

WGS84 okuması ya da dışa aktarma için coğrafi sistem gerekiyorsa dereceleri
belgeden uzak tutun.

## Her nesne kendi biçimiyle taşınır

| Nesne | Nasıl taşınır |
|---|---|
| Çizgi, çoklu çizgi, alan (parsel), nokta, yaylı çoklu çizgi, spline | **Her köşesi** PROJ ile, tek tek; parselin yasal geometrisinde hiçbir şey yaklaşık değildir |
| Daire, yay, elips, tarama, ölçü, lider, blok referansı | Çapası (merkezi, ekleme noktası, ilk noktası) PROJ ile **tam**; biçimi o noktadaki **yerel dönme ve ölçekle**: daire daire, yay yay kalır; yazı ve blok, iki dilimin grid kuzeyleri arasındaki açı kadar döner |
| Blok tanımının içi | **Dokunulmaz**: tanım kendi koordinatındadır, referansı taşınınca bütün kopyalar taşınır |

[Dış referanslar](xref.md) bu tabloda yoktur: tanımları kendi dosyalarının koordinatındadır.
Dönüşüm bitince, aynı işlemin içinde, dosyalarından **yeni sisteme dönüştürülerek
yeniden okunurlar**.

Yerel ölçek gerçektir: bir dilimin orta meridyeninden uzakta harita metresi yer
metresinden kısadır — örneğin TUREF TM36'dan TM30'a geçen, orta meridyenden 5,8°
uzaktaki bir noktada binde üç kadar. 5 m yarıçaplı bir daire yeni haritada 5,016 m
çizilir; bu yuvarlama değil, izdüşümün kendisidir.

## Etiket koordinatları izler

Sayıları taşınmış ama koordinat sistemi hâlâ eski sistemi söyleyen bir çizim,
hiç dönüştürülmemiş olandan **daha kötüdür**: aşağıdaki her okuyucu etikete
güvenir. Bu yüzden dönüşüm belgenin CRS'ini de yazar.

## Adlar

| Türkçe | ASCII | İngilizce | Kısaltma |
|---|---|---|---|
| `DÖNÜŞTÜR` | `DONUSTUR` | `REPROJECT` | `DNS` |

## Sözdizimi

```text
DÖNÜŞTÜR hedef=<sistem> [kaynak=<sistem>]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `hedef` | Hedef sistem: `EPSG:5256`, `TUREF/TM36`, bir PROJ dizesi ya da WKT |
| `kaynak` | Kaynak sistem; verilmezse çizimin kendi koordinat sistemi |

`kaynak` yalnız etiketi yanlış olan ve doğrusunu bildiğiniz bir çizim için
gerekir.

## Türkiye'de sık kullanılan kodlar

| EPSG | Sistem |
|---|---|
| `5253` | TUREF / TM30 |
| `5254` | TUREF / TM33 |
| `5255` | TUREF / TM39 (bkz. not) |
| `5256` | TUREF / TM36 |
| `5262`–`5269` | TUREF / 3 derecelik dilimler |
| `23035`–`23038` | ED50 / UTM 35–38 |

Doğru kodu `Seçenekler ▸ Koordinat Sistemleri` sayfasından da seçebilirsiniz.

## Örnekler

### Komut satırı

```text
AYAR ad=koordinat_sistemi deger=EPSG:5256
KATMAN ad=PARSEL
ALAN noktalar=485300,4310200 485360,4310200 485360,4310245 485300,4310245
DÖNÜŞTÜR hedef=EPSG:5254
```

```text
1 nesne dönüştürüldü: EPSG:5256 -> EPSG:5254   (PROJ 9.8.1)
```

### Arayüz

Şeritte **Harita ▸ Jeodezi ▸ Dönüştür** ya da komut satırından `DÖNÜŞTÜR hedef=EPSG:5254`.
Şeritten başlatıldığında komut hedef sistemi sorar ve veri paketindeki TM 3° dilimlerini
(`TUREF/TM27` … `TUREF/TM45`) önerir; listede olmayan bir EPSG kodu ya da PROJ
tanımı da yazılabilir. Durum çubuğundaki koordinat sistemi okuması dönüşümden sonra
yeni sistemi gösterir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.setting",   "args": { "ad": "koordinat_sistemi", "deger": "EPSG:5256" } },
    { "cmd": "core.layer",     "args": { "ad": "PARSEL" } },
    { "cmd": "core.area",      "args": { "noktalar": [[485300000,4310200000],[485360000,4310200000],[485360000,4310245000],[485300000,4310245000]] } },
    { "cmd": "core.reproject", "args": { "hedef": "EPSG:5254" } }
  ]
}
```

## Geri alma

**Tek adımdır ve bu bir gerekliliktir**: çizimin her köşesi ya taşınır ya hiçbiri
taşınmaz. Yarı dönüştürülmüş bir kadastro paftası Anayasa 1.6'nın adını koyduğu
hatadır ve burada iki yarısı da makul görünür.

## Betikten kullanım

Bir arşivi toplu dönüştüren betik yazılabilir. Herhangi bir nokta dönüşemezse
**hiçbiri** dönüşmez — bir dilimin dışına düşen nokta işlemi bütünüyle geri alır.

## Hatalar

> `PROJ bu yapıda yok; koordinat dönüşümü yapılamaz.`

Program PROJ'suz derlenmiş. Kimlik dönüşümü döndürüp datum kayması yapmış gibi
davranmak yanlış bir hukuki belge üretirdi, bu yüzden sessizce geçilmez.

> `Çizimin koordinat sistemi tanımsız.`

Belgenin CRS'i yok ve `kaynak` da verilmedi.

> `Kaynak ve hedef aynı sistem: <ad>. Yapılacak bir şey yok.`

Dönüştürecek bir şey yok.

> `Bu dönüşümün bir ucu coğrafi (derece)…`

Hedef ya da kaynak derece cinsinden. Projeksiyonlu bir sistem seçin.

## İlgili

- [OTURT](fit.md) — yerel çizimi kontrol noktalarıyla haritaya oturtma
- [AYAR](setting.md) — `koordinat_sistemi`
