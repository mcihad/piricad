# DİKDÖRTGEN — İki Köşeden Dörtgen Çizme

Yapı adası, bina oturumu, yapı yaklaşma sınırı, plan paftası çerçevesi — kısacası
bir planın çoğunu oluşturan dik köşeli yüzeyleri çizen herkes için; bu sayfayı
bitirdiğinizde dikdörtgeni ve karesini arayüzden, komut satırından ve betikten
çizmeyi bileceksiniz.

> **Faz 0 durumu.** `DİKDÖRTGEN` kapalı yüzeyi belgeye yazar. Yüzeyin **içi henüz
> boyanmaz**: tuval bugün yalnız sınırı çizer, dolgu ve tarama **Faz 1'de**
> sembol yığınıyla birlikte gelecek. Ürettiği nesne [`ALAN`](area.md) ile aynı
> türdendir; alan hesabı, ifraz ve tevhit o fazda bu nesnenin üzerine oturur.

## Ne yapar

`DİKDÖRTGEN`, **karşılıklı iki köşeden** dört köşeli kapalı bir yüzey üretir.
Kalan iki köşeyi program hesaplar.

Aynı şeyi [`ALAN`](area.md) ile dört köşe tıklayarak da çizebilirsiniz — ama
çizemezsiniz: elle tıklanan dört köşe **neredeyse** dik olur, ve imzalanan bir
paftada "neredeyse dik" bir kusurdur. İki köşe şeklin tamamını belirlediğinde
köşelerin dikliği hesabın sonucudur, dikkatin değil.

**Karşı köşe**, sıradaki köşe değildir: çapraz köşedir. Her çizim programının
dikdörtgen sürüklemesi böyledir, ve köşegen kilidinin kare üretmesinin sebebi de
budur.

## Kare çizmek

Çizerken **Ctrl** basılı tutun: ikinci köşe, ilkinden 45°'nin katlarına kilitlenir
ve dikdörtgen **tam kare** çıkar. Bıraktığınızda kilit kalkar.

Ctrl bir fare hüneri değildir, bir **modu** basılı tutar. Aynı kilit
[`MOD`](mode.md) ile de açılır ve betikten de açılabilir:

```text
MOD köşegen=evet
DİKDÖRTGEN 485320,4310220 485360,4310180
MOD köşegen=hayır
```

Kilit açıkken kenar uzunluğu, sürüklediğiniz uzaklığın 45°'ye izdüşümüdür: köşe
imlecin yönüne değil, ona en yakın köşegene oturur. `MOD kutupsal_açı` ile
kurduğunuz açı adımını, kilit açık olduğu sürece geçersiz kılar.

Kilit yalnız dikdörtgene özel değildir; ikinci ve sonraki noktasını öncekine göre
alan her komut — [`ÇİZGİ`](line.md), [`ALAN`](area.md) — aynı kilide uyar.

## Adlar

| Ad | Tür |
|---|---|
| `DİKDÖRTGEN` | Türkçe, birincil |
| `DIKDORTGEN` | ASCII karşılık |
| `RECTANGLE` | İngilizce karşılık |
| `DKD` | Kısaltma |
| `REC` | Kısaltma |
| `core.rectangle` | Komut kimliği |

## Sözdizimi

```text
DİKDÖRTGEN
DİKDÖRTGEN <köşe> <karşı köşe>
DİKDÖRTGEN noktalar=<köşe> noktalar=<karşı köşe>
```

Nokta yazımı [`ÇİZGİ`](line.md) ile aynıdır: mutlak koordinat
(`485320,4310220`), göreli (`@50,30`) ve kutupsal (`@100<45`).

## Parametreler

| Parametre | Zorunlu | Tür | Anlamı |
|---|---|---|---|
| `noktalar` | evet | nokta ×2 | Karşılıklı iki köşe. Kalan ikisi bunlardan türetilir |

Başka parametresi yoktur. Köşe sayısı sabittir; delik açmak, üçten çok köşe vermek
ya da açılı bir dörtgen çizmek [`ALAN`](area.md) işidir.

## Örnekler

**Arayüzden.** Araç kutusundan dikdörtgen aracını seçin, bir köşeye tıklayın,
karşı köşeye tıklayın. Kare için ikinci tıklamada **Ctrl** basılı tutun.

**Komut satırından.** 40 m × 20 m bir yapı adası:

```text
DİKDÖRTGEN 485320,4310220 485360,4310200
```

Göreli koordinatla, aynı dikdörtgen:

```text
DİKDÖRTGEN 485320,4310220 @40,-20
```

**Kare.** Kilidi açıp 25 m'lik bir bina oturumu:

```text
MOD köşegen=evet
DİKDÖRTGEN 485320,4310220 @25,-25
MOD köşegen=hayır
```

## Geri alma

Bir `DİKDÖRTGEN` çağrısı **tek** geri alma adımıdır: [`GERİAL`](undo.md) dörtgenin
tamamını kaldırır, köşe köşe değil. Yarım kalmış bir dörtgen belgeye hiç
yazılmaz — ESC ile vazgeçerseniz hiçbir iz kalmaz.

## Betikten kullanım

JSON betiğinde iki köşe, tek listede:

```json
[
  { "cmd": "DİKDÖRTGEN", "args": { "noktalar": [[485320000, 4310220000],
                                                [485360000, 4310200000]] } }
]
```

Betikteki koordinatlar **milimetredir** (tam sayı), komut satırındaki metredir.
Kare için betiğin başına `{"cmd": "MOD", "args": {"ad": "köşegen", "deger": "evet"}}`
koyun.

Günlüğe **iki köşe** yazılır, türetilen dördü değil: günlük yeniden oynatıldığında
aynı dörtgen aynı hesapla üretilir, ve dört köşe yazılsaydı sonradan biri
oynatıldığında artık dikdörtgen olmayan bir "dikdörtgen" kalırdı.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Bu iki köşe bir alan kapatmaz: karşı köşenin hem doğusu hem kuzeyi ilkinden farklı olmalı.` | İki köşe aynı x ya da aynı y üzerinde; aralarında yüzey yok | Karşı köşeyi her iki eksende de kaydırın |
| `Bir alanın sınırı kendini kesemez.` | Geometri katmanı bozuk halkayı reddetti | Köşelerin sırasını denetleyin |
| `Etkin katman kilitli.` | Katman kilitliyse çizim yazılmaz | [`KATMAN`](layer.md) ile kilidi açın |

## İlgili sayfalar

- [`ALAN`](area.md) — çok köşeli ve delikli yüzeyler
- [`ÇİZGİ`](line.md) — açık doğru parçaları, nokta yazımı
- [`MOD`](mode.md) — köşegen kilidi, dik mod, kutupsal açı ve yakalama modları
