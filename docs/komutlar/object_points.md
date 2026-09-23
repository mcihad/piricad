# NESNENOKTALARI — Bir Nesnenin Noktaları

Bir yapay zeka ajanına "bu parselin ortasına", "bu çizginin ucundan" dedirtmek isteyen
herkes için; bu sayfayı bitirdiğinizde bir nesnenin merkezinin, köşelerinin ve uçlarının
bir ajana nasıl **konum** olarak verildiğini ve bir ajanın bu konumlardan nasıl
**ölçüyle** yeni noktalar kurduğunu bileceksiniz.

## Ne yapar

Verilen nesnelerin istenen noktalarını bulur ve adlandırır:

| `tur` | Noktalar |
|---|---|
| `merkez` | Alanın ağırlık merkezi (delikler düşülerek), çizginin **uzunluk** ortası, dairenin, yayın ve elipsin merkezi, noktanın kendisi |
| `koseler` | Çizginin ve alanın köşeleri; bir eğride tutamaklar (dairenin merkezi ve dört çeyreği gibi) |
| `uclar` | Açık bir çizginin ve yayın başlangıcı ve bitişi |
| `kutu` | Nesneyi saran dikdörtgenin dört köşesi: güneybatı, güneydoğu, kuzeydoğu, kuzeybatı |
| `orta_noktalar` | Her kenarın ortası; kapalı bir alanda kapanış kenarı dahil |

Çizime dokunmaz. Asıl kullanıcısı bir **ajandır**: bir ajan koordinat yazamaz, konumu
her zaman bir okuma aracından alır ([Onay ve denetim](../yapay-zeka/onay.md)). Bu komut
bulduğu noktaları ajana bir **nokta tutamağı** olarak verir; ajan sayıları görmez, her
noktanın adını görür ("nesne 3: köşe 2") ve tutamağın `.N` elemanıyla ona başvurur.
Oradan bir ölçüyle uzaklaşmak da bir konumdur: `{"taban": "@….0", "dogu": 5000}`
tabandan 5 m doğudur.

Koordinatları kendiniz görmek istiyorsanız [`KOORDİNAT`](coordinate.md) komutunu ya da
yakalamayı kullanın; bu komut noktaları adlandırır.

## Adlar

| Ad | Tür |
|---|---|
| `NESNENOKTALARI` | Türkçe, birincil |
| `OBJECTPOINTS` | İngilizce karşılık |
| `NNK` | Kısaltma |
| `core.object_points` | Komut kimliği |
| `nesne_noktalari` | Ajan arayüzündeki araç adı |

## Sözdizimi

```text
NESNENOKTALARI nesneler=<kimlik> [tur=merkez|koseler|uclar|kutu|orta_noktalar]
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Noktaları istenen nesneler. Zorunlu |
| `tur` | Hangi noktalar; yukarıdaki tablo. Verilmezse `merkez` |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Bir parsel çizin ve merkezini isteyin:

```
ALAN 0,0 20,0 20,10 0,10
NESNENOKTALARI nesneler=1
```

```text
1 nokta: nesne 1: merkez.
```

Köşeleri ve kenar ortaları:

```
NESNENOKTALARI nesneler=1 tur=koseler
NESNENOKTALARI nesneler=1 tur=orta_noktalar
```

```text
4 nokta: nesne 1: köşe 1 … nesne 1: köşe 4.
4 nokta: nesne 1: kenar 1 ortası … nesne 1: kenar 4 ortası.
```

### Arayüz

Komut bir ajan içindir; arayüzde karşılığı nesneye tıklarken çalışan **yakalamadır**:
merkez, uç, orta nokta ve çeyrek yakalamaları aynı noktaları verir. Komutun kendisini
arayüzden çalıştırmak için **Ctrl+K** ile komut aramayı açıp `NESNENOKTALARI` yazın.

### Betik

```json
{
  "ad": "Parselin köşeleri",
  "komutlar": [
    { "cmd": "core.area", "args": { "noktalar": [[0,0],[20000,0],[20000,10000],[0,10000]] } },
    { "cmd": "core.object_points", "args": { "nesneler": [1], "tur": "koseler" } }
  ]
}
```

### Bir ajan bununla nasıl çizer

Sohbette "bu parselin ortasına 3 m yarıçaplı bir daire çiz" denince ajan önce parseli
`sorgula` ya da `secimi_al` ile nesne tutamağı olarak alır, sonra `nesne_noktalari` ile
merkezini nokta tutamağı olarak ister ve daireyi o tutamakla önerir:

```text
core_circle_draw  {"merkez": "@….0", "yaricap": 3000}
```

Öneri kartında koordinatlar ve konumun kaynağı görünür; uygulanması sizin onayınızla
olur.

## Geri alma

`NESNENOKTALARI` çizime dokunmaz: geri alınacak bir şey yoktur ve günlüğe girmez.

## Betikten kullanım

Betiklerde `core.object_points` kimliğiyle çağrılır. Betiğe ve ajana dönen yapı:

| Alan | Ne taşır |
|---|---|
| `etiketler` | Her noktanın adı, sırasıyla: `nesne 1: köşe 2` |
| `adet` | Kaç nokta bulunduğu |
| `tutamaklar` | Yalnız ajana: noktaların nokta tutamağı, `ad` ve `noktalar` alanlarıyla |

Koordinatların kendisi ajana gitmez; tutamağın arkasında durur.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Nesne bulunamadı veya silinmiş: 7` | O kimlikte bir nesne yok | Kimliği `SEÇ` ya da `sorgula` ile öğrenin |
| `nesne 1 kapalı bir şekil; ucu yoktur. Köşeleri için tur=koseler.` | Kapalı bir alanın ucu istendi | `tur=koseler` ya da `tur=merkez` verin |
| `'core.object_points': 'tur' için tanınmayan değer 'orta'. Kabul edilenler: merkez / koseler / uclar / kutu / orta_noktalar` | Tanınmayan nokta türü | Listedeki sözcüklerden birini yazın |

## İlgili

- [`GÖRÜNÜMBİLGİSİ`](view_info.md) — ekranın ortası da bir nokta tutamağı olarak gelir
- [`SORGULA`](query.md), [`SEÇİMBİLGİSİ`](selection_info.md) — nesne tutamağı
- [`KOORDİNAT`](coordinate.md) — bir noktanın koordinatını okuma
- [Onay ve denetim](../yapay-zeka/onay.md) — tutamak kuralı ve göreli noktalar
