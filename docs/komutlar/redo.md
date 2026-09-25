# YİNELE — Yineleme

Geri aldığını geri getirmek isteyen herkes için; bu sayfayı bitirdiğinizde yinelemenin
ne zaman çalıştığını ve ne zaman çalışmadığını bileceksiniz.

## Ne yapar

En son geri alınan işlemi tekrar uygular ve onu geri alma yığınına koyar.

[`GERİAL`](undo.md) ile tam simetriktir: geri aldığınız her şeyi, geri aldığınız
sıranın tersinden geri getirir.

## Adlar

| Ad | Tür |
|---|---|
| `YİNELE` | Türkçe, birincil |
| `YINELE` | Türkçe karaktersiz klavye için |
| `REDO` | İngilizce karşılık |
| `core.redo` | Komut kimliği |

## Sözdizimi

```
YİNELE
```

## Parametreler

`YİNELE` parametre almaz. Üretilmiş [komut referansına](referans.md) bakabilirsiniz.

## Örnekler

### Komut satırı

Bir çizgi çizin, geri alın, sonra yineleyin:

```
ÇİZGİ 0,0 100,0
GERİAL
YİNELE
```

Transkript ne yinelendiğini ve yinelemenin ne yaptığını söyler:

```text
Yinelendi: İki veya daha fazla nokta arasında doğru parçaları çizer.
Yinelemeyle 1 nesne eklendi.
```

Yineleme adımı **yapıldığı sırayla** yeniden kurar: aynı parseli iki kez taşıyan ya da
taşıyıp değerini değiştiren bir betik, yinelenince betiğin bıraktığı hâle döner.

### Arayüz

**Ctrl+Shift+Z** ya da sekme satırının sağındaki hızlı erişim düğmelerinden **Yinele**.
Klavye düzeninize göre **Ctrl+Y** de çalışabilir.

Yinelenecek bir şey olmadığında düğme ve menü öğesi pasifleşir.

### Betik

Betik adımı olarak `YİNELE` şöyle yazılır:

```json
{ "cmd": "core.redo", "args": {} }
```

[`GERİAL`](undo.md) için geçen kural bunun için de geçerlidir: betik bir şey çizdikten
sonra çağrılırsa hata döner, çünkü betik bitmeden onun geri alma adımı yoktur. Henüz
hiçbir şey çizmemiş bir betikte, ya da Python konsolunda tek başına yazılan
`cad.redo()`, komut satırındaki `YİNELE` ile aynıdır.

## Geri alma

`YİNELE` kendisi geri alınmaz — salt okunur bir komuttur. Yinelediğinizi tekrar kaldırmak
için `GERİAL` kullanın; ikisi arasında istediğiniz kadar gidip gelebilirsiniz.

## Betikten kullanım

`YİNELE` betiklenebilirdir ve salt okunur işaretlidir.

### Yineleme dalı ne zaman kaybolur

Geri aldıktan sonra **yeni bir düzenleme yaparsanız** yineleme yığını temizlenir.
Örnek:

```
ÇİZGİ 0,0 10,0        ← A işlemi
GERİAL                ← A geri alındı, yinelenebilir
ÇİZGİ 0,0 0,10        ← B işlemi; A'nın yineleme dalı silindi
YİNELE                ← "Yinelenecek işlem yok"
```

Bu, bütün çizim programlarında geçerli olan standart davranıştır.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Yinelenecek işlem yok` | Yineleme yığını boş | Ya hiç geri alma yapılmamış ya da araya yeni bir düzenleme girmiş |
| `Düzenleme yapmış bir toplu işin içinde yinelenemez: …` | Bir betik bir şey çizdikten sonra `core.redo` çağırdı | Adımı betikten çıkarın; yinelemeyi betik bittikten sonra yapın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
