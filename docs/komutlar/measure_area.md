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
| `AREAOF` | İngilizce karşılık |
| `AÖ` | Kısaltma |
| `core.measure_area` | Komut kimliği |

## Sözdizimi

```text
ALANÖLÇ
ALANÖLÇ nesneler=<k1> nesneler=<k2> …
ALANÖLÇ yontem=nokta
ALANÖLÇ noktalar=<n1> <n2> <n3> …
```

Kimlik verilmezse **etkin seçim** ölçülür; o da boşsa arayüz nesneleri sorar.
`noktalar` verilirse yöntem kendiliğinden `nokta` olur; anahtardan sonra gelen
bütün koordinatlar köşedir.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Ölçülecek nesnelerin kimlikleri. Verilmezse etkin seçim; o da boşsa tuvalden seçtirir |
| `yontem` | `nesne` (öntanımlı) ya da `nokta` — köşeleri gösterilen alan |
| `noktalar` | `yontem=nokta` için köşeler; verilirse yöntem kendiliğinden `nokta` olur |

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

### Arayüz

**Nesneden.** Araç kutusunda **Ölç** ailesinden **Alan Ölç**'e basın. Seçili nesne
varsa hemen ölçülür; yoksa komut "Ölçülecek nesneleri seçin" der, tuvalden
tıkladığınız her nesne seçime eklenir ve **sağ tık** (ya da Enter) ölçtürür. Sonuç
transkripte ve tuvale düşer, araç elinizde kalır: seçim temizlenir, sıradaki parsel
için yeniden sorar. Bırakmak için Esc.

**Köşelerden.** Aynı ailede **Alan Ölç — köşelerden**'i seçin. Köşelere sırayla
tıklayın: ikinci köşeden sonra alan imleçle birlikte dolgulu çizilir, imlecin yanında
alanı ve çevresi yazar. **Enter** ya da **sağ tık** bitirir.

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
| `Tanınmayan yöntem: '…'. Yöntemler: nesne / nokta` | `yontem` yanlış yazıldı | `nesne` ya da `nokta` yazın |

## İlgili

- [`ÖLÇ`](measure.md) — noktadan noktaya mesafe ve toplam uzunluk
- [`ÖZNİTELİK`](attribute.md) — alanı özniteliğe yazmak için
- [`SEÇ`](select.md)
