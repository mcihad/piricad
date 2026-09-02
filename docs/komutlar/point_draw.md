# NOKTA — Ölçülmüş Nokta

Nirengi, poligon noktası, röper ya da herhangi bir ölçülmüş tek noktayı çizime
işleyen herkes için; bu sayfayı bitirdiğinizde noktayı arayüzden, komut
satırından ve betikten yerleştirmeyi bileceksiniz.

## Ne yapar

`NOKTA`, çizime **ölçülmüş tek bir nokta** koyar: bir nirengi, bir poligon
noktası, bir röper.

Nokta bir yerdir — kendisiyle başka bir yer arasında hiçbir şey yoktur. Çizgi
değildir, alan değildir, yüzölçümü yoktur ve sınır kutusunun boyu sıfırdır.

Nokta nesnesi olmadan **DÜĞÜM yakalaması** da olamazdı: bir kadastro işinde her
sınır bir röperden ölçülür, dolayısıyla röper bir paftadaki en önemli yakalama
hedefidir. `NOKTA` ile birlikte DÜĞÜM modu açılabilir hale geldi
([`MOD`](mode.md)) ve **köşe yakalamasının önünde** sıralanır: bir röper ile bir
parsel köşesi bir milimetre arayla dururken ölçülmüş olan röperdir, köşe ondan
türetilmiştir.

Noktanın **neye benzediğine bu komut karar vermez**. Nesne tek bir tepe noktası
bildirir; artı mı, üçgen mi, numaralı röper işareti mi olacağını gösterim
kataloğu söyler.

## Adlar

| Ad | Tür |
|---|---|
| `NOKTA` | Türkçe, birincil |
| `POINT` | İngilizce karşılık |
| `NK` | Kısaltma |
| `core.point_draw` | Komut kimliği |

## Sözdizimi

```text
NOKTA
NOKTA <n1> <n2> <n3> …
NOKTA noktalar=<n1> noktalar=<n2> …
```

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `noktalar` | Yerleştirilecek noktalar. Her biri ayrı bir nesne olur |

## Örnekler

### Komut satırı

Bir istasyonun röperlerini arka arkaya yerleştirin:

```text
NOKTA 485300,4310200 485360,4310245 485410,4310190
```

```text
3 nokta yerleştirildi.
```

### Arayüz

Sol paletteki **nokta** aracına basın ya da `NOKTA` yazın, sonra sırayla
tıklayın — bir istasyonun yirmi röperi için palete yirmi kez uzanmanız gerekmez.
Bitirmek için **Esc**.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.point_draw",
      "args": { "noktalar": [[485300000, 4310200000],
                             [485360000, 4310245000]] } }
  ]
}
```

Betikte koordinatlar **milimetredir** — komut satırında metre yazılır, betikte ham
depolama birimi kullanılır (`485300000` = 485 300 m).

## Geri alma

`NOKTA` tek bir geri alma adımıdır: bir çalıştırmada kaç nokta koyduysanız tek
[`GERİAL`](undo.md) hepsini birden kaldırır.

## Betikten kullanım

Betikten çağrıldığında `noktalar` verilmelidir.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `'<katman>' katmanı kilitli.` | Etkin katman kilitli | [`KATMAN`](layer.md) ile kilidi açın |

## İlgili

- [`MOD`](mode.md) — DÜĞÜM yakalamasını açar
- [`ÇİZGİ`](line.md) · [`ALAN`](area.md)
- [`ÖZNİTELİK`](attribute.md) — noktaya numara ve kot yazar
