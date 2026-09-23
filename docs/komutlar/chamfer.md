# PAH — Köşe Pahı Kırma

Kavşakta köşesi kesilmiş bir yapı adası, bir bordür dönüşü ya da kırılmış bir
parsel köşesi çizen herkes için; bu sayfayı bitirdiğinizde pah kırmayı arayüzden,
komut satırından ve betikten yapmayı bileceksiniz.

## Ne yapar

`PAH`, bir köşeyi **düz bir kenarla** keser. Köşe noktası, her iki kenar boyunca
verdiğiniz **mesafe** kadar içeride duran iki noktayla değiştirilir. Üç biçimde
çalışır:

| Biçim | Ne kesilir |
|---|---|
| **Bir köşe** | Bir çizginin ya da alanın kendi köşesi |
| **İki çizgi arasında** | Ayrı iki düz çizginin buluştuğu ya da buluşacağı köşe, iki kenarda iki ayrı mesafeyle |
| **Bütün köşeler** (`hepsi=evet`) | Bir ya da birçok çizginin ve alanın bütün köşeleri, aynı mesafeyle |

Nesne aynı nesne olarak kalır; kimliği, katmanı, stili ve öznitelikleri değişmez.
Yeni nesne oluşmaz — kesilen köşenin yerine bir kenar gelir, o kadar.

**Kapalı bir alanın her köşesi köşedir**, kapanış kenarının vardığı köşe dahil.
Açık bir çizginin **ilk ve son noktası ise köşe değildir**: orada tek kenar
buluşur ve karşıdan kesilecek bir şey yoktur. Komut bunu söyler.

Kesim mesafesi komşu kenarlardan **kısa olmalıdır**; uzun olsaydı kenar ortadan
kalkardı ve komut bunu yapmak yerine söyler.

**Hangi köşe?** Nesne verilmemişse ve tek bir nesne seçili değilse komut önce
köşeyi sorar: köşeye **bir kez tıklamak** hem nesneyi hem köşeyi seçer. Köşe
olmayan bir yere tıklarsanız o çizgi **iki çizginin birincisidir** ve komut ikinciyi
ister. Ardından mesafe sorulur; mesafeyi **yazabilir** ya da tuvalde
**gösterebilirsiniz** — köşeden imlece olan uzaklık mesafedir. İmleç hareket ettikçe
kesilmiş köşe tuvalde vurgulu çizilir; gördüğünüz, tıkladığınızda olacak olandır.

### İki çizgi arasında

Ayrı iki düz çizginin köşesi kesilir. Çizgileri **kalacak parçalarından**
tıklarsınız: iki çizgi kesiştikleri yerden, tıkladığınız parçalara doğru
`mesafe` ve `ikinci_mesafe` kadar geri kesilir ve aralarına düz bir kenar konur.
Birbirine yetişmeyen iki çizgi önce kesişecekleri yere uzatılmış sayılır.

- `ikinci_mesafe` verilmezse iki kenarda da aynı `mesafe` kesilir.
- Kısaltmadan yalnız pah kenarını koymak için `budama=hayir` verin.
- Pah yalnız **düz** kenarlar arasında kırılır; bir yay ya da daire ile köşe için
  [`YUVARLA`](fillet.md) kullanın. Paralel iki çizgi arasında köşe yoktur.
- Mesafe tıkladığınız parçanın tamamını götürüyorsa komut bunu söyleyerek reddeder.
- Pah kenarı birinci çizginin katmanında ve stilindedir.

### Bütün köşeler

`hepsi=evet` bir çizginin ya da alanın bütün köşelerini aynı mesafeyle keser;
köşeler birbiri ardınca işlenir. Mesafenin sığmadığı köşe **atlanır ve sayılır**,
işin geri kalanı reddedilmez. Alan alan olarak kalır: parsel kimliği ve
öznitelikleri korunur.

Birden çok nesne verilirse — ya da önceden seçilmişse — hepsinin bütün köşeleri
**tek adımda** kesilir ve tek bir geri alma adımıdır. Köşeli bir çizgi ya da alan
olmayan nesne (daire, yay, yazı) atlanır ve sayılır.

Hesap `atan2` ya da başka bir trigonometri çağrısı kullanmaz — yalnız birim
vektörler ve `sqrt`. Sebebi taşınabilirliktir: libm'in trigonometri işlevleri
platformlar arası bit bit aynı sonucu vermez, KentOSCad ise verir (§7.3).

## Adlar

| Ad | Tür |
|---|---|
| `PAH` | Türkçe, birincil |
| `CHAMFER` | İngilizce karşılık |
| `PH` | Kısaltma |
| `core.chamfer` | Komut kimliği |

## Sözdizimi

```text
PAH nesne=<k> nokta=<n> mesafe=<metre>
PAH nesne=<k> <k> nokta=<n> ikinci_nokta=<n> mesafe=<metre> [ikinci_mesafe=<metre>] [budama=hayir]
PAH nesne=<k> [<k> …] hepsi=evet mesafe=<metre>
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesne` | Köşesi kesilecek nesne; iki çizgi verilirse aralarındaki köşe; `hepsi=evet` ile bir ya da daha çok nesne |
| `nokta` | Tek nesnede işlem yapılacak köşe (en yakın köşe seçilir); iki çizgide birincinin kalacak parçası |
| `ikinci_nokta` | İki çizgide ikincinin kalacak parçası |
| `mesafe` | Köşeden kenar boyunca kesilecek mesafe, metre; iki çizgide birincideki |
| `ikinci_mesafe` | İki çizgide ikincideki mesafe; verilmezse `mesafe` |
| `budama` | İki çizgide çizgiler pah uçlarına kadar kısaltılıp uzatılsın mı; varsayılan `evet` |
| `hepsi` | `evet`: verilen nesnelerin bütün köşeleri; sığmayan köşe ve köşesi olmayan nesne atlanır |

## Örnekler

### Komut satırı

```text
PAH nesne=1 nokta=485300,4310200 mesafe=5
```

```text
Köşeye pah kırıldı.
```

Yeni bir çizimde, L biçiminde buluşan iki çizgi arasında 2 m ve 3 m'lik pah:

```text
ÇİZGİ 0,0 10,0
ÇİZGİ 10,0 10,10
PAH nesne=1 2 nokta=5,0 ikinci_nokta=10,5 mesafe=2 ikinci_mesafe=3
```

```text
İki çizgi arasında pah kırıldı.
```

Yeni bir çizimde, bir parselin dört köşesine birden 1 m'lik pah:

```text
ALAN 0,0 10,0 10,10 0,10
PAH nesne=1 hepsi=evet mesafe=1
```

```text
4 köşeye pah kırıldı.
```

Yeni bir çizimde, iki parselin bütün köşelerine birden:

```text
ALAN 0,0 10,0 10,10 0,10
ALAN 20,0 30,0 30,10 20,10
PAH nesne=1 2 hepsi=evet mesafe=1
```

```text
8 köşeye pah kırıldı (2 nesnede).
```

### Arayüz

Sol araç sütununda **köşe ailesinin** düğmesine basın (Pah, Yuvarla, Köşe Taşı,
Köşe Ekle ve Çizgi Düzenle aynı düğmededir; basılı tutunca ya da sağ tıklayınca
kart açılır). Aynı araç **Değiştir → Pah** menüsündedir.

1. Kesilecek köşeye tıklayın. Nesne de bu tıklamayla seçilir.
2. İmleci köşeden uzaklaştırın: kesilmiş köşe tuvalde vurgulu çizilir, yanında
   uzaklık yazar.
3. İstediğiniz yerde tıklayın **ya da** mesafeyi komut satırına yazıp Enter'a
   basın (`5`).

Araç açık kalır; bir sonraki köşeye tıklayarak devam edebilirsiniz, Esc bırakır.

İki çizgi arasında: birinci çizgiye **kalacak parçasından** tıklayın, sonra
ikinciye. İmleci iki çizginin buluştuğu yerden uzaklaştırdıkça pah ve kısaltılmış
çizgiler tuvalde çizilir ve imlecin yanında `mesafe X m` yazar. Tıklayın ya da
mesafeyi yazın.

**Pah — bütün köşeler** köşe ailesinin kartında ve **Değiştir** menüsündedir:
nesneye tıklayın ya da önce birden çok nesne seçip düğmeye basın; mesafeyi yazın ya
da gösterin. Bütün nesnelerin bütün köşeleri birlikte önizlenir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.polyline",
      "args": { "noktalar": [[485300000, 4310260000], [485300000, 4310200000],
                             [485360000, 4310200000]] } },
    { "cmd": "core.chamfer",
      "args": { "nesne": [1], "nokta": [485300000, 4310200000], "mesafe": 5 } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`PAH` tek bir geri alma adımıdır; [`GERİAL`](undo.md) köşeyi geri getirir.

## Betikten kullanım

Betikten çağrıldığında `nesne`, `nokta` ve `mesafe` verilmelidir; iki çizgide
`ikinci_nokta` da. Yapılandırılmış cevap, kısaltılan çizgilerin kimliklerini
(`duzenlenen`) ve eklenen pah kenarının kimliğini (`eklenen`) söyler.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `PAH için nesne belirtilmedi. Örnek: PAH nesne=1 nokta=10,10 mesafe=3` | Betik ne nesneyi ne köşeyi verdi | `nesne=` ve `nokta=` verin |
| `Orada köşesi kesilecek bir çizgi ya da alan yok. ...` | Tıklanan yerde nesne yok | Bir çizginin iki kenarının buluştuğu köşeye tıklayın |
| `Nesne N bir eğri, yazı ya da nokta; köşe işlemleri yalnız çizgi ve alanlarda çalışır.` | Daire, yay, yazı ya da nokta verildi | Çizgi ya da alan seçin |
| `Burada iki kenarın buluştuğu bir köşe yok. ...` | Açık bir çizginin ucu gösterildi | İki kenarın buluştuğu bir köşe gösterin |
| `Bu köşede kenarlar aynı doğrultuda; kesilecek bir köşe yok.` | Kenarlar doğrusal | Gerçek bir köşe gösterin |
| `Mesafe sıfırdan büyük olmalı.` | Sıfır ya da eksi mesafe | Artı bir mesafe verin |
| `Kesim komşu kenardan uzun: kenarlar 12,000 m ve 20,000 m, gereken 21,000 m. ...` | Değer kenarlardan büyük | Daha küçük bir değer verin ya da daha yakına tıklayın |
| `Pah iki düz kenar arasında kırılır; yay ile köşe için YUVARLA kullanın.` | İki çizgiden biri yay ya da daire | [`YUVARLA`](fillet.md) kullanın |
| `İki çizgi paralel; aralarında köşe yok.` | İki çizgi aynı doğrultuda | Kesişen iki çizgi seçin |
| `Pah mesafesi sıfırdan büyük olmalı.` | İki çizgide sıfır ya da eksi mesafe | Artı bir mesafe verin |
| `Köşe sığmıyor: seçtiğiniz parçanın tamamını götürüyor. Daha küçük bir değer verin.` | Mesafe tıklanan parçadan uzun | Daha küçük bir mesafe verin |
| `Köşe, çoklu çizginin ortadaki bir kenarının uzantısına düşüyor; ...` | Ortadaki bir kenar uzatılmak isteniyor | Çizginin ucundaki kenarı seçin |
| `Nesneleri köşenin kendisinden değil, kalacak parçalarından seçin.` | Tıklama tam iki çizginin kesiştiği yerde | Çizgiye köşeden biraz uzakta tıklayın |
| `İkinci tıklamanın altında bir nesne yok.` | İkinci tıklama boşluğa | İkinci çizginin üstüne tıklayın |
| `İki tıklama aynı nesnenin bitişik olmayan yerlerinde; köşesini işlemek için köşeye tıklayın.` | Aynı çizginin bitişik olmayan kenarları | Köşeye tıklayın |
| `Bu değer hiçbir köşeye sığmıyor; daha küçük bir değer verin.` | `hepsi=evet` ile hiçbir köşe mesafeyi almıyor | Daha küçük bir mesafe verin |
| `Bu çizginin köşesi yok.` | `hepsi=evet` iki köşeli bir çizgiye | Köşesi olan bir çizgi seçin |
| `Seçilen nesnelerin hiçbirinde işlenecek köşe yok; köşeli bir çizgi ya da alan seçin.` | `hepsi=evet` ile verilen nesnelerin hiçbiri köşeli çizgi ya da alan değil | Köşeli bir çizgi ya da alan seçin |
| `Köşe iki nesne arasında kurulur; N nesnenin bütün köşeleri için hepsi=evet verin.` | `hepsi` olmadan ikiden çok nesne | İki nesne verin ya da `hepsi=evet` ekleyin |

## İlgili

- [`YUVARLA`](fillet.md) — köşeyi düz kenar yerine yayla keser
- [`KÖŞETAŞI`](vertex_move.md) · [`KÖŞEEKLE`](vertex_insert.md)
