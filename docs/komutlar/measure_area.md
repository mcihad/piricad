# ALANÖLÇ — Alan ve Çevre Ölçme

Bir parselin yüzölçümünü ya da çevresini — ya da çizimde olmayan bir alanın,
köşelerini göstererek — okumak isteyen herkes için; bu sayfayı bitirdiğinizde alan
ölçümünü arayüzden, komut satırından ve betikten yapmayı bileceksiniz.

## Ne yapar

`ALANÖLÇ` iki yolla ölçer:

- **Nesneden** (`yontem=nesne`, öntanımlı): seçili nesnelerin **alanını** ve
  **çevresini** yazar. Birden çok nesne verildiğinde her birini ayrı ayrı yazar ve
  sonunda **toplam alanı** bildirir.
- **Köşelerden** (`yontem=nokta`): çizimde **olmayan** bir alanı ölçer — iki bina
  arasındaki avlu, yolun parselden alacağı kısım, adımlanmış bir tarla. Köşeleri
  sırayla gösterirsiniz; alan, köşeler konuldukça imleçle birlikte yazılır, Enter
  sonucu verir. En az üç köşe gerekir.

**Sonuç tuvalde kalır**: ölçülen alan vurgulu ve hafif dolgulu çizilir, alanı ve
çevresi ortasında yazar. Çizim değiştiğinde ya da hiçbir komut çalışmıyorken Esc'e
bastığınızda silinir; çizimin bir parçası değildir.

Alan hesabı **nesnenin türüne sorulur**. Bu, dairede fark eder: daire gerçek
alanını, yani **π·r²**'yi bildirir — ekranda çizildiği 128 kenarlı çokgenin alanını
değil. Yay ve açık çizgi hiçbir şey çevrelemediği için alanları yoktur: komut
bunu söyler ve yerine **uzunluğu** yazar ("kapalı değil, alanı yok; uzunluk: …").

Alan **halkalar üzerinden, role göre işaretli** hesaplanır: dış sınır artı,
delikler eksi. Delikli bir parselin alanı deliksiz gösterilmez.

`ALANÖLÇ` çizimi **değiştirmez** ve geri alma adımı üretmez.

## Adlar

| Ad | Tür |
|---|---|
| `ALANÖLÇ` | Türkçe, birincil |
| `ALANOLC` | ASCII karşılık |
| `ALANSOR` | Türkçe eş ad: Netcad'deki adı (Alan Sor) |
| `AREAOF` | İngilizce karşılık |
| `AÖ` | Kısaltma |
| `core.measure_area` | Komut kimliği |

## Sözdizimi

```text
ALANÖLÇ
ALANÖLÇ nesneler=<k1> nesneler=<k2> …
ALANÖLÇ yontem=nokta
ALANÖLÇ noktalar=<n1> <n2> <n3> …
ALANÖLÇ yontem=ic [nokta=<n>] [ada=evet|hayır] [bosluk=<mm>]
ALANÖLÇ nokta=<n>
```

Kimlik verilmezse **etkin seçim** ölçülür; o da boşsa arayüz nesneleri sorar.
`noktalar` verilirse yöntem kendiliğinden `nokta` olur; anahtardan sonra gelen
bütün koordinatlar köşedir. `nokta` verilirse yöntem kendiliğinden `ic` olur.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Ölçülecek nesnelerin kimlikleri. Verilmezse etkin seçim; o da boşsa tuvalden seçtirir |
| `yontem` | `nesne` (öntanımlı), `nokta` — köşeleri gösterilen alan — ya da `ic` — içine tıklanan bölge |
| `noktalar` | `yontem=nokta` için köşeler; verilirse yöntem kendiliğinden `nokta` olur |
| `nokta` | `yontem=ic` için bölgenin içindeki nokta; verilmezse tuvalde sorulur |
| `ada` | `yontem=ic`: bölgenin içindeki kapalı çizgiler ada olarak düşülür (öntanımlı `evet`) |
| `bosluk` | `yontem=ic`: bu kadar milimetreye kadar açık uçlar köprülenir; öntanımlı 0, hiç köprülenmez |

### İçine tıklayarak: `yontem=ic`

Netcad'in Alan Seçim Aracı gibi: çizimde alan nesnesi olmayan bir bölgenin — gevşek
çizgilerin, yayların, çoklu çizgilerin kapattığı zeminin — içine tıklarsınız, onu çevreleyen
çizgiler bulunur ve alanı ölçülür. Hesap [`SINIR`](boundary.md)'ınkidir: düğüm toleransı
içindeki uçlar birleşir, içerideki kapalı çizgiler ada olarak düşülür, yaylar yay kalır.
Kapanmayan bir bölge `SINIR` ile **aynı sözle** reddedilir ve açık uçlar tuvalde işaretlenir.

Tıklama **yakalanmaz**: nesne yakalama, ızgara ya da dik mod açık olsa bile tıkladığınız
nokta olduğu gibi alınır. Yakalansaydı bir parselin kenarına yakın bir tıklama o kenarın
üstüne oturur ve "çizginin üstünde" diye reddedilirdi.

Ölçüm hiçbir şey çizmez; sonuçta **Sınır olarak çiz** düğmesi aynı noktayla `SINIR`
satırını çalıştırır ve bölgeyi bir alan olarak yazar.

## Örnekler

### Komut satırı

```text
SEÇ
ALANÖLÇ
```

```text
Nesne 1 — alan: 2700,00 m²   çevre: 210,000 m
```

Birden çok parselin toplamı:

```text
ALANÖLÇ nesneler=1 nesneler=2 nesneler=3
```

Çizimde olmayan bir alan, köşelerinden:

```text
ALANÖLÇ noktalar=0,0 20,0 20,10 0,10
```

```text
Alan: 200,00 m²   çevre: 60,000 m   (4 köşe)
```

Dört gevşek çizginin kapattığı 40 × 30 metrelik bir avlu, içine tıklayarak:

```
ÇİZGİ 0,0 40,0
ÇİZGİ 40,0 40,30
ÇİZGİ 40,30 0,30
ÇİZGİ 0,30 0,0
ALANÖLÇ yontem=ic nokta=20,15
```

```text
Alan: 1200,00 m²   çevre: 140,000 m   — içine tıklanan bölge, 4 nesnenin çizgisinden
Sınır olarak çizmek için: SINIR nokta=20.000,15.000
```

### Arayüz

**Nesneden.** Şeritte **Harita ▸ Ölçüm ▸ Alan Ölç**'e (ya da **Kadastro ▸ Denetim ▸ Alan
Ölç**'e) basın; kapalı bir alan seçiliyken beliren **Alan** sekmesinde de vardır. Seçili nesne
varsa hemen ölçülür; yoksa komut "Ölçülecek nesneleri seçin" der, tuvalden
tıkladığınız her nesne seçime eklenir ve **sağ tık** (ya da Enter) ölçtürür. Sonuç
transkripte ve tuvale düşer, araç elinizde kalır: seçim temizlenir, sıradaki parsel
için yeniden sorar. Bırakmak için Esc.

**İçine tıklayarak.** Aynı okun altındaki **Alan Ölç — içine tıklayarak**'ı seçin ve
bölgenin içine tıklayın; imleç gezdikçe altındaki bölge çizilir.

**Köşelerden.** **Harita ▸ Ölçüm ▸ Alan Ölç** düğmesinin okundan **Alan Ölç —
köşelerden**'i seçin. Köşelere sırayla
tıklayın: ikinci köşeden sonra alan imleçle birlikte dolgulu çizilir, imlecin yanında
alanı ve çevresi yazar. **Enter** ya da **sağ tık** bitirir.

**Alan olarak çizmek.** Köşelerden ölçüm bitince tuvalin üstünde **Alan olarak çiz**
düğmesi belirir (Netcad'in Alan Sor'u ölçtüğü alanı nesne olarak da üretir). Düğme aynı
köşelerle `ALAN` satırını çalıştırır; transkript de aynı satırı yazar, komut satırında
çalışan el onu kopyalayabilir:

```text
Alan olarak çizmek için: ALAN 485300.000,4310200.000 485340.000,4310200.000 485340.000,4310230.000 485300.000,4310230.000
```

`ALANÖLÇ` kendisi yine hiçbir şey çizmez; alan `ALAN`'ın işidir, tek geri alma adımıdır ve
günlüğe `core.area` satırı olarak girer. Nesneden ölçümde düğme çıkmaz: nesne zaten
çizimdedir.

### Betik

Betik önce iki alan çizer, sonra ikisini birlikte ölçer:

```json
{
  "komutlar": [
    { "cmd": "core.area", "args": { "noktalar": [[0,0],[20000,0],[20000,10000],[0,10000]] } },
    { "cmd": "core.area", "args": { "noktalar": [[30000,0],[40000,0],[40000,10000],[30000,10000]] } },
    { "cmd": "core.measure_area", "args": { "nesneler": [1, 2] } }
  ]
}
```

## Geri alma

`ALANÖLÇ` geri alınmaz, çünkü hiçbir şeyi değiştirmez.

## Betikten kullanım

Betikten çağrıldığında `nesneler` ya da `noktalar` verilmelidir; betik çalışırken
"etkin seçim" diye bir şey olmayabilir. Köşelerden ölçümün yapılandırılmış sonucu
(`alan_mm2`, `cevre_mm`, `kose`) bir ajana ve Python'a da döner.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `İşlem yapılacak nesne yok: seçim boş ve 'nesneler' verilmedi.` | Ne kimlik verildi ne seçim var, tuvalde de seçilmedi | Nesneleri seçin ya da kimliklerini yazın; örnek satır mesajın altındadır |
| `Geçersiz nesne kimliği: N. Kimlikler 1'den başlar.` | Sıfır ya da negatif kimlik | Kimlikler 1'den başlar |
| `Nesne bulunamadı veya silinmiş: N` | Kimlik yok ya da nesne silinmiş | [`SEÇ`](select.md) ile doğru kimliği bulun |
| `Alan ölçmek için en az üç köşe gerekir; N köşe verildi.` | Köşelerden ölçümde iki ya da daha az köşe | En az üç köşe gösterin |
| `Tanınmayan yöntem: '…'. Yöntemler: nesne / nokta / ic` | `yontem` yanlış yazıldı | `nesne`, `nokta` ya da `ic` yazın |
| `Bu bölge kapanmıyor: N açık uç var; …` | `yontem=ic`: bölgeyi çevreleyen çizgilerde düğüm toleransından geniş boşluk var | Boşluğu yakalamayla kapatın ya da `bosluk=<mm>` verin |
| `Bu noktayı çevreleyen kapalı bir çizgi yok. …` | `yontem=ic`: nokta hiçbir kapalı bölgenin içinde değil | Bölgenin içine tıklayın; gizli katmanlardaki çizgiler sınır sayılmaz |
| `Nokta bir çizginin üstünde; bölgenin İÇİNE, çizgiden uzağa tıklayın.` | `yontem=ic`: nokta tam bir çizginin üzerinde | Bölgenin içine tıklayın |

## İlgili

- [`ÖLÇ`](measure.md) — noktadan noktaya mesafe ve toplam uzunluk
- [`ÖZNİTELİK`](attribute.md) — alanı özniteliğe yazmak için
- [`SEÇ`](select.md)
