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

```
GERİAL
```

Transkript ne geri alındığını söyler:

```text
Geri alındı: İki veya daha fazla nokta arasında doğru parçaları çizer.
```

Arka arkaya birkaç kez yazarak birkaç işlem geri gidebilirsiniz.

### Arayüz

**Ctrl+Z**, araç kutusundaki **Geri Al** düğmesi veya **Düzen > Geri Al** menüsü.

Geri alınacak bir şey kalmadığında düğme ve menü öğesi pasifleşir.

### Betik

```json
{
  "ad": "Çiz ve geri al",
  "komutlar": [
    { "cmd": "core.line", "args": { "noktalar": [[0,0],[10000,0]] } },
    { "cmd": "core.undo", "args": {} }
  ]
}
```

Bir betiğin içinde `core.undo` kullanmak nadiren gerekir, çünkü betiğin tamamı zaten tek
bir geri alma adımıdır.

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

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).
