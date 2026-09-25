# GERİAL — Geri Alma

Yanlış bir işlem yapan herkes için; bu sayfayı bitirdiğinizde neyin bir adım sayıldığını
ve neyin geri alınamayacağını bileceksiniz.

## Ne yapar

Son çizim işlemini geri alır ve onu yineleme yığınına koyar.

Geri alma **işlem** düzeyinde çalışır, tek tek nesne düzeyinde değil. Dört noktalı bir
`ÇİZGİ` üç segment yaratır ama tek `GERİAL` üçünü birden kaldırır.

## Adlar

| Ad | Tür |
|---|---|
| `GERİAL` | Türkçe, birincil |
| `GERIAL` | Türkçe karaktersiz klavye için |
| `UNDO` | İngilizce karşılık |
| `U` | Kısaltma |
| `core.undo` | Komut kimliği |

## Sözdizimi

```
GERİAL
```

## Parametreler

`GERİAL` parametre almaz. Üretilmiş [komut referansına](referans.md) bakabilirsiniz.

## Örnekler

### Komut satırı

Bir çizgi çizin, sonra geri alın:

```
ÇİZGİ 0,0 100,0
GERİAL
```

Transkript ne geri alındığını ve geri almanın ne yaptığını söyler:

```text
Geri alındı: İki veya daha fazla nokta arasında doğru parçaları çizer.
Geri almayla 1 nesne silindi.
```

İkinci satır adımın **tamamını** sayar: bin satırlık bir betiği geri almak, hangi
nesnelerin yerine döndüğünü ve hangi yazıların eski metnine kavuştuğunu tek cümlede söyler
([Betik ne değiştirdiğini söyler](../betik/README.md#betik-ne-değiştirdiğini-söyler)).

Arka arkaya birkaç kez yazarak birkaç işlem geri gidebilirsiniz.

### Arayüz

**Ctrl+Z** ya da sekme satırının sağındaki hızlı erişim düğmelerinden **Geri Al**.

**Çizerken** — [`ÇİZGİ`](line.md), [`ÇOKLUÇİZGİ`](polyline.md), [`ALAN`](area.md) ya da
[`SPLINE`](spline.md) nokta beklerken — Ctrl+Z ve **Geri Al** komutu değil yalnız **son
noktayı** geri alır; çizimin geri kalanı yerinde kalır. Aynı iş komut satırında `G`, `GERİ`
ya da `U` yazılarak, tuvalde **⌫** ile yapılır. Komut bittikten sonra Ctrl+Z çizimin
tamamını kaldırır.

Geri alınacak bir şey kalmadığında düğme ve menü öğesi pasifleşir; çizerken geri
alınacak bir nokta varsa etkin kalır.

### Betik

Betiğin tamamı **tek** bir geri alma adımıdır: betik bittiğinde tek `GERİAL`, kaç komut
çalıştırdıysa hepsini birlikte kaldırır. Bu betik iki çizgi çizer; ardından yazılan bir
`GERİAL` ikisini de götürür:

```json
{
  "ad": "Tek adımda iki çizgi",
  "komutlar": [
    { "cmd": "core.line", "args": { "noktalar": [[0,0],[10000,0]] } },
    { "cmd": "core.line", "args": { "noktalar": [[0,5000],[10000,5000]] } }
  ]
}
```

Betik adımı olarak `GERİAL` şöyle yazılır:

```json
{ "cmd": "core.undo", "args": {} }
```

Bu adım betiğin **kendi** çizdiklerini geri alamaz: betik bitmeden onun geri alma
adımı yoktur. Betik bir şey çizdikten sonra çağrılırsa hata döner ve betik orada durur,
çünkü uzanabileceği tek adım betikten **önce** yaptığınız iştir — ve betik bitince o iş
geri getirilemez biçimde kaybolurdu. Henüz hiçbir şey çizmemiş bir betikte, ya da Python
konsolunda tek başına yazılan `cad.undo()`, komut satırındaki `GERİAL` ile aynıdır.

## Geri alma

`GERİAL` kendisi geri alınmaz — salt okunur bir komuttur ve geri alma yığınına girmez.
Geri aldığınızı iade etmek için [`YİNELE`](redo.md) kullanın.

## Betikten kullanım

`GERİAL` betiklenebilirdir ve salt okunur işaretlidir.

### Neyin bir adım sayıldığı

| İşlem | Kaç adım |
|---|---|
| Bir `ÇİZGİ` komutu, kaç segment yaratırsa yaratsın | 1 |
| Bir `SİL` komutu, kaç nesne silerse silsin | 1 |
| Bir `KATMAN` komutunun görünürlük/kilit/renk değişikliği | 1 |
| Bir betiğin tamamı, kaç komut içerirse içersin | 1 |
| Boş katman yaratmak | 0 — geri alınmaz |
| Aktif katmanı değiştirmek | 0 — görünüm durumu |
| `YAKINLAŞ` | 0 — görünüm durumu |
| Hiçbir şey çizmeden iptal edilen komut | 0 — hiç olmamış sayılır |

### Yineleme dalı

Geri aldıktan sonra **yeni bir düzenleme yaparsanız** yineleme dalı silinir; geri
aldığınıza artık dönemezsiniz. Bu bütün CAD ve çizim programlarında böyledir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Geri alınacak işlem yok` | Geri alma yığını boş | Normaldir; geri alınacak bir şey yapılmamış |
| `Düzenleme yapmış bir toplu işin içinde geri alınamaz: …` | Bir betik bir şey çizdikten sonra `core.undo` çağırdı | Adımı betikten çıkarın; betik bittikten sonra tek `GERİAL` betiğin tamamını geri alır |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
