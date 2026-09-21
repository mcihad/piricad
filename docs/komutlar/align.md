# HİZALA — Taşı, Döndür, Ölçekle

Yerel bir krokiyi ya da bir detay çizimini iki bilinen noktaya oturtacak herkes
için.

## Ne yapar

Bir ya da iki **nokta çiftiyle** nesneleri taşır, döndürür ve istenirse
ölçekler:

| Verilen | Sonuç |
|---|---|
| Bir çift (`kaynak`, `hedef`) | Yalnız taşır |
| İki çift (+ `kaynak2`, `hedef2`) | Taşır ve döndürür |
| İki çift + `olcekle=evet` | Taşır, döndürür ve iki çiftin uzunluk oranıyla ölçekler |

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

### Arayüz

**Değiştir > Hizala**. Nesneleri seçip Enter'a basın, sonra birinci kaynağı ve
hedefini tıklayın. İkinci çift `kaynak2=`/`hedef2=` ile verilir — bir hizalamada
ikinci çift bir kez verilir, her nesne için değil.

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
| `Nesne N bir daire; HİZALA bu sürümde çizgileri ve alanları hizalar.` | Eğri ya da yazı seçildi | `TAŞI` + `DÖNDÜR` kullanın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [OTURT](fit.md) — çok noktalı Helmert dönüşümü, artık hatalarıyla
- [TAŞI](move.md) · [DÖNDÜR](rotate.md) · [ÖLÇEKLE](scale.md) — tek tek
