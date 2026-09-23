# HİZALA — Taşı, Döndür, Ölçekle

Yerel bir krokiyi ya da bir detay çizimini iki bilinen noktaya oturtacak herkes
için.

## Ne yapar

Bir, iki ya da üç **nokta çiftiyle** nesneleri taşır, döndürür, istenirse ölçekler
ve gerekirse ters çevirir:

| Verilen | Sonuç |
|---|---|
| Bir çift (`kaynak`, `hedef`) | Yalnız taşır |
| İki çift (+ `kaynak2`, `hedef2`) | Taşır ve döndürür |
| İki çift + `olcekle=evet` | Taşır, döndürür ve iki çiftin uzunluk oranıyla ölçekler |
| Üç çift (+ `kaynak3`, `hedef3`) | Üçüncü hedef, ilk iki hedefi birleştiren doğrunun **öbür yanındaysa** nesneleri ayrıca ters çevirir — üçüncü nokta kendi hedefinin yanına düşer |

**Her türü hizalar**: çizgi ve alanın yanında daire (ölçeklenince daire kalır),
yay, elips, spline, yazı (döner, ölçeklenince harfleri büyür), ölçü (yeniden
ölçülür), blok referansı ve tarama. Hizalama öbür dönüşüm komutlarıyla aynı
kuralla yapılır.

**`OTURT` ile karıştırmayın.** `OTURT` çok sayıda ortak nokta üzerinden en küçük
kareler Helmert dönüşümüdür ve jeodezik bir işlemdir — artık hataları dağıtır,
bir raporu vardır. Bu, çizim fiilidir: bir ya da iki çift, dengeleme yok.
Yanlışını seçmek bir çizimin *neyi iddia ettiğini* sessizce değiştirir, bu yüzden
her iki sayfa öbürünü adıyla anar.

## Adlar

| Ad | Tür |
|---|---|
| `HİZALA` | Türkçe, birincil |
| `HIZALA` | ASCII katlanmış Türkçe |
| `ALIGN` | İngilizce karşılık |
| `HZL` | Kısaltma |
| `core.align` | Komut kimliği |

## Sözdizimi

```text
HİZALA nesne=<kimlik> kaynak=<nokta> hedef=<nokta>
HİZALA nesne=<kimlik> kaynak=<n> hedef=<n> kaynak2=<n> hedef2=<n> [olcekle=evet]
HİZALA nesne=<kimlik> kaynak=<n> hedef=<n> kaynak2=<n> hedef2=<n> kaynak3=<n> hedef3=<n>
```

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `nesne` | seçim | 0..n | Hizalanacak nesneler |
| `kaynak` | nokta | 1 | Birinci kaynak nokta |
| `hedef` | nokta | 1 | Birinci kaynağın gideceği yer |
| `kaynak2` | nokta | 0..1 | İkinci kaynak; verilirse döndürme de yapılır |
| `hedef2` | nokta | 0..1 | İkinci kaynağın gideceği yer |
| `olcekle` | mantıksal | 0..1 | İki çiftin uzunluk oranıyla ölçekler de |
| `kaynak3` | nokta | 0..1 | Üçüncü kaynak; hedefi öbür yandaysa nesneler ters çevrilir |
| `hedef3` | nokta | 0..1 | Üçüncü kaynağın gideceği yan |

## Örnekler

### Komut satırı

Yalnız taşımak:

```text
HİZALA nesne=1 kaynak=0,0 hedef=485320,4310220
```

Doğuya bakan bir çizgiyi kuzeye döndürmek — uzunluğu korunur:

```text
HİZALA nesne=1 kaynak=0,0 hedef=0,0 kaynak2=10,0 hedef2=0,20
```

Aynı dönüş, ama ikinci hedefe **uzayarak**:

```text
HİZALA nesne=1 kaynak=0,0 hedef=0,0 kaynak2=10,0 hedef2=0,20 olcekle=evet
```

```text
1 nesne hizalandı (taşındı, döndürüldü ve ölçeklendi).
```

Yeni bir çizimde, bir üçgeni üçüncü noktası öbür yana düşecek biçimde, ters
çevirerek hizalamak:

```text
ALAN 0,0 10,0 0,5
HİZALA nesne=1 kaynak=0,0 hedef=100,0 kaynak2=10,0 hedef2=110,0 kaynak3=0,5 hedef3=100,-5
```

```text
1 nesne hizalandı (taşındı ve döndürüldü; üçüncü nokta öbür yana düştüğü için ters çevrildi).
```

### Arayüz

**Değiştir > Hizala**, ya da araç kolonundaki değiştirme ailesinde **Hizala**.

1. Nesneleri seçip Enter'a basın.
2. Birinci kaynağı tıklayın, sonra hedefini: nesneler **imleçle birlikte taşınır**.
3. **İkinci kaynağı** tıklayın — ya da yalnız taşımak için **Enter**'a basın. Birinci
   çift bu sırada tuvalde kalır.
4. İkinci kaynağın gideceği **doğrultuyu** gösterin: nesneler imleçle birlikte
   **döner**; tıklamak hizalamayı uygular.

Ölçekleyerek hizalamak için aynı ailedeki **Hizala — ölçekleyerek** aracını seçin
(`HİZALA olcekle=evet`): ikinci çiftin uzunluğu nesneleri de büyütür ya da küçültür,
önizlemede de öyle görünür. Bir hizalamada ikinci çift bir kez verilir, her nesne
için değil.

### Betik

```json
{ "cmd": "core.align", "args": {
    "nesne": [1],
    "kaynak": [0, 0], "hedef": [485320150, 4310220400],
    "kaynak2": [10000, 0], "hedef2": [485330150, 4310220400],
    "olcekle": true } }
```

## Geri alma

Tek işlem, tek **Ctrl+Z**.

## Betikten kullanım

Betiklenebilir ve yapay zekâya açıktır; her parametre için tip ve adet üretilmiş
[komut referansındadır](referans.md).

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `İki kaynak ya da iki hedef nokta aynı; doğrultu ve ölçek hesaplanamaz.` | İkinci çiftin iki noktası çakışık | Ayrı noktalar verin |
| `Üçüncü kaynağın gideceği yer verilmedi: hedef3=<nokta>.` | `kaynak3` var, `hedef3` yok | `hedef3` verin |
| `Üçüncü nokta ilk ikisiyle aynı doğru üzerinde; hangi yana düştüğü okunamıyor.` | Üçüncü nokta ya da hedefi ilk iki noktanın doğrusunda | Doğrunun bir yanında bir nokta verin |
| `Nesne N yaylı bir çoklu çizgi: …` gibi tür iletileri | Dönüşümü tutamayan bir tür | İletideki öneriyi izleyin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [OTURT](fit.md) — çok noktalı Helmert dönüşümü, artık hatalarıyla
- [TAŞI](move.md) · [DÖNDÜR](rotate.md) · [ÖLÇEKLE](scale.md) — tek tek
