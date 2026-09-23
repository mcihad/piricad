# İZ — Geçici İzleme

Bir noktayı **şu köşeyle aynı hizada ve bu köşeyle aynı doğrultuda** koymak
zorunda olan herkes için; bu sayfayı bitirdiğinizde iki köşeyi işaretleyip
aralarındaki boşlukta kalan noktayı arayüzden, komut satırından ve betikten
bulmayı bileceksiniz.

## Ne yapar

`İZ`, verdiğiniz noktayı **işaretler**. İşaretli her noktadan bir **yatay** ve bir
**düşey** iz geçer; imleç bu izlere yakalanır.

Bu, bir çizimin kendi başına cevaplayamadığı en yaygın aplikasyon sorusudur:
*şu köşeyle aynı hizada, bu köşeyle aynı doğrultuda bir nokta.* Cevabın yerinde
hiçbir geometri yoktur ve iki köşenin hiçbirinde oraya işaret eden bir şey yoktur;
nokta yalnızca o iki köşe var olduğu için vardır.

### İki işaret bir kesişim verir

İki nokta işaretlediğinizde iz çiftlerinin **kesişimi** alınır: birinin sağası ile
öbürünün yukarısı. İki eşleşme vardır ve hangisinin kastedildiğine **işaretleme
sırası değil, imleci nereye götürdüğünüz** karar verir.

Bir kesişim, tek bir izi **yener**. İki işaret koyan kullanıcı onların belirlediği
noktayı hedefliyordur; tek bir iz her zaman bir eksende daha yakındır, yani ikisi
yalnız mesafeye göre yarıştırılsa kesişim hiç yakalanamazdı.

### Tek işaret kendi iki izini verir

Tek işaret varsa (ya da kesişim erişim dışındaysa) o işaretin yatay ve düşey izi
çalışır: imleci bir köşeyle aynı hizada tutup mesafeyi yazmak, elin sıkça yaptığı
bir şeydir.

### En çok iki işaret

Üçüncü işaret **en eskisinin yerini alır**. Üç işaret üçüncü bir eksen değil, yeni
bir çifttir. Aynı noktayı iki kez işaretlemek bir kez işaretlemektir: bir köşeyi
alıp uzaklaşıp geri dönmek bir iz demektir, ve iki özdeş işaretin kesişimi
işaretin kendisi olurdu.

### İşaretler hizmet ettikleri komutla gider

Bir işaret, **o sırada çalışan komut için** konur: `ÇİZGİ`'nin ortasında konan
işaretler o çizgi bitince — tamamlandığında, reddedildiğinde ya da Esc ile
bırakıldığında — silinir. Hiçbir komut çalışmıyorken konan işaretler **sıradaki
komutu** bekler ve o komut bitince silinir. Yeni bir çizim ([`YENİ`](new.md))
eskisinin işaretlerini hiçbir zaman devralmaz.

`İZ` kendisi ve [`YAKINLAŞ`](zoom.md) gibi **şeffaf** komutlar çalışan komutun
yanında koşar; işaretlere dokunmazlar. Satırı daha çalışmadan reddedilen bir komut
(örneğin var olmayan bir nesne kimliği) bir şey yapmamıştır; işaretler yerinde
kalır.

Komut çalışmıyorken **Esc** işaretleri de siler; bunu `İZ sil=evet` ile yapar ve
kaç işaret silindiğini söyler.

### Yazılı karşılığı zaten vardı

Aynı noktayı tek satırda yazmak için nokta fonksiyonu `xy(P,Q)` kullanılır
([komut satırı](komut-satiri.md)). `İZ` onun **el hâlidir** ve ikisi aynı noktayı
tanım gereği verir: `xy` iki noktayı birlikte alıp kesişimi hesaplar, `İZ`
onları tek tek işaretler ve yakalama motoru aynı kesişimi hesaplar. Tek kural,
iki yol.

## Adlar

| Ad | Tür |
|---|---|
| `İZ` | Türkçe, birincil |
| `IZ` | ASCII katlanmış Türkçe |
| `TRACK` | İngilizce karşılık |
| `TRK` | Kısaltma |
| `core.tracking` | Komut kimliği |

## Sözdizimi

```text
İZ                → işaretleri listeler
İZ <nokta>        → işaretler
İZ sil=evet       → bütün işaretleri siler
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nokta` | İşaretlenecek nokta. Yoksa işaretler listelenir |
| `sil` | Bütün işaretleri siler |

## Şeffaf komut

`İZ` **şeffaftır**: çalışan bir komutu iptal etmez, onun **yanında** çalışır. Bütün
tasarımı budur — işaret başka bir komutun ortasında konur:

```text
ÇİZGİ
  Çizginin ilk noktası:  0,0
  Sonraki nokta:  İZ 12,8          ← işaret konur, ÇİZGİ beklemeye devam eder
  Sonraki nokta:  İZ 26,18         ← ikinci işaret
  Sonraki nokta:  (kesişime tıklayın)
```

İşaretler `Bus` üzerinde durur, oturumda: bir işaret onu koyan **komuttan uzun
yaşar** ve bu, şeffaf `İZ`'in bütün anlamıdır.

## Örnekler

### Komut satırı

```text
ALAN 0,0 12,0 12,8 0,8
ALAN 26,18 38,18 38,26 26,26
İZ 12,8
İZ 26,18
İZ
```

```text
İşaretlendi: 12,000, 8,000.  1 işaret; yatay ve düşey izi açık.
İşaretlendi: 26,000, 18,000.  2 işaret; kesişim 12,000, 18,000 ya da 26,000, 8,000.
2 işaret:
  12,000, 8,000
  26,000, 18,000
Kesişimler:  12,000, 18,000   ve   26,000, 8,000
```

### Arayüz

Nokta isteminde **Shift + sağ tık**, imlecin altındaki noktayı işaretler.
İşaretlenen nokta **yakalanmış** noktadır, piksel değil: köşeyi tam almak jestin
tamamıdır, yarım milimetre kayan bir işaret bütün izleri yarım milimetre kaydırır.

İşaretli her noktada küçük bir kare ve ondan geçen yatay/düşey kesikli izler
çizilir. İki iz her çiftte kesiştiği için, kesişim tek başına hangi noktaların
işaretlendiğini söylemez — kare onu söyler.

Jest komuttan geçer, doğrudan motora değil: fareyle konan bir işaret ile
komut satırında yazılan bir işaret **tek şey** olmalıdır.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.tracking", "args": { "nokta": [12000, 8000] } },
    { "cmd": "core.tracking", "args": { "nokta": [26000, 18000] } }
  ]
}
```

## Yakalama kipi

İzleme bir yakalama kipidir (`izleme`) ve **varsayılan olarak açıktır**. Kimseden
bir şey almaz: işaret yoksa kip hiçbir şey yapmaz, ve bir iz **her zaman** gerçek
bir köşenin altında sıralanır — kullanıcının kurduğu iskele, ölçülmüş bir noktayı
elinden almaz.

Kapatmak için [`MOD`](mode.md) kullanın:

```text
MOD yakalama_modları 66047     (izleme biti kapalı)
```

F3 listesinde **izleme** olarak görünür.

## Geri alma

Geri alınacak bir şey yoktur. Bir işaret **oturum durumudur**: belgeye girmez,
günlüğe düşmez, geri alma adımı yemez. Bir işaret iskeledir.

`İZ sil=evet` işaretleri temizler; hizmet ettikleri komut bitince de kendiliğinden
silinirler (yukarıda).

## Betikten kullanım

`core.tracking` betikten çağrılabilir ve `AiAccessible`'dır. Ajan yolunda nokta
yalnız araç sonucu tutamağıyla gelir (CLAUDE.md 5.8).

## Hatalar

Bu komutun kendi reddi yoktur: bir nokta işaretlenebilir ya da zaten
işaretlidir, ve ikisi de bir hata değildir.

| Yazılan | Sebep | Ne yapmalı |
|---|---|---|
| `İşaretli nokta yok. Bir köşeyi işaretlemek için İZ <nokta> yazın…` | `İZ` argümansız çağrıldı ve hiç işaret yok | Bir nokta verin ya da nokta isteminde Shift + sağ tık yapın |
| `İşaretli nokta yoktu.` | `İZ sil=evet` çağrıldı, silinecek işaret yoktu | Bir şey gerekmiyor |

İmleç kesişime **yakalanmıyorsa** sebep hemen hemen her zaman ikisinden biridir:
kip kapalıdır (`MOD` ile açın) ya da imleç **erişim dışındadır** — yakalama
açıklığı bir piksel sayısıdır ve o mesafe yakınlaştırmayla değişir. Yarım metre
uzaktan yakalamak, açıklık o kadar genişse olur.

## İlgili sayfalar

- [`MOD`](mode.md) — yakalama kipleri, dik mod, kutupsal izleme
- [`KILAVUZ`](guide.md) — kalıcı kılavuz çizgisi; iz geçicidir, kılavuz dosyayla gider
- [Komut satırı](komut-satiri.md) — `xy(P,Q)` ve öbür nokta fonksiyonları
- [`APLİKASYON`](stakeout.md) — istasyondan hedefe semt açısı ve kenar
