# DİKAYAK — Dik Ayak / Dik Boy

Elinde şerit metre ve iki bilinen nokta olan herkes için; bu sayfayı
bitirdiğinizde bir taban çizgisine göre ölçülmüş detayları arayüzden, komut
satırından ve betikten çizime işlemeyi bileceksiniz.

## Ne yapar

`DİKAYAK`, iki bilinen noktadan geçen bir **taban çizgisi** kurar ve o çizgiye
göre okunan her detayı bir noktaya çevirir. Her detay iki ölçüdür:

| Ölçü | Anlamı |
|---|---|
| **ayak** | Taban çizgisi üzerinde, birinci noktadan itibaren kaç metre ilerlediğiniz |
| **boy** | O ayaktan tabana dik olarak kaç metre çıktığınız |

Bir Türk ölçü karnesinde bir duvar, bir bordür, bir direk ya da bir bina köşesi
tam bu iki sayıyla yazılır. Alet yalnız şerit metre ve prizma çubuğu olduğunda
detay çizime böyle girer.

**Boy'un işareti: A→B yönünde SOL pozitiftir.** Netcad'in işaretiyle aynıdır.
`boy=5` çizginin solunda, `boy=-5` sağında bir nokta koyar. Hangi taraf olduğu
taban çizgisini hangi sırayla verdiğinize bağlıdır: `0,0 100,0` ile `100,0 0,0`
aynı çizgi ama ters yöndür, dolayısıyla aynı `boy` karşı tarafa düşer.

Hesap, komut satırındaki [`dik(A,B,ayak,boy)`](komut-satiri.md#nokta-fonksiyonları)
nokta fonksiyonuyla **aynı** hesaptır — ikisi de `core::perpendicular_offset`
çağırır — yani fareyle koyduğunuz nokta yazarak koyduğunuzla milimetresine kadar
aynıdır.

## Adlar

| Ad | Tür |
|---|---|
| `DİKAYAK` | Türkçe, birincil |
| `DIKAYAK` | ASCII katlanmış Türkçe |
| `PERPOFFSET` | İngilizce karşılık |
| `DA` | Kısaltma |
| `core.perp_offset` | Komut kimliği |

## Sözdizimi

```text
DİKAYAK <A> <B>
DİKAYAK <A> <B> ayak=<m> boy=<m> [ayak=<m> boy=<m> …] [cizgi=evet]
```

`ayak` ve `boy` **sırayla eşleşir**: birinci `ayak` birinci `boy` ile, ikinci
ikinciyle. Kaç çift verirseniz o kadar nokta çıkar.

**Sayıca eşit olmalılar** ve eşit değilse komut **hiçbir şey yapmaz**: üç `ayak`
ile iki `boy`, birinin yanlış aktarıldığı bir ölçü karnesidir, ve tam olan iki
çifti koyup üçüncüyü sessizce düşürmek bir detayın ölçüden kaybolması demektir.
Ret, iki sayıyı da söyler.

**Okumaları adıyla verin.** `DİKAYAK 0,0 100,0 30 -5` gibi çıplak sayılar
çalışmaz: iki dizi arka arkaya bildirildiği için çıplak sayıların hepsi
**birincisine** bağlanır ve ikincisi boş kalır. Hangi sayının ayak hangisinin boy
olduğuna program karar veremez — ve bir detayın taban çizgisinin hangi tarafına
düştüğünü tahmin etmek, bir yapının sınırın yanlış tarafına oturması demektir.
Bu yüzden ret, doğru yazımı gösterir.

## Parametreler

| Parametre | Tip | Adet | Açıklama |
|---|---|---|---|
| `baslangic` | nokta | 1 | Taban çizgisinin ilk noktası (A) |
| `bitis` | nokta | 1 | Taban çizgisinin ikinci noktası (B) |
| `ayak` | sayı | 0..n | A'dan taban boyunca uzaklık (m) |
| `boy` | sayı | 0..n | Tabana dik uzaklık (m); **sol pozitif** |
| `cizgi` | mantıksal | 0..1 | Noktaları verildikleri sırayla çizgiyle birleştirir |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Doğu yönünde 100 metrelik bir taban çizgisi ve üzerindeki üç detay:

```text
DİKAYAK 0,0 100,0 ayak=10 boy=5 ayak=30 boy=-5 ayak=60 boy=5
```

```text
3 nokta dik ayak/dik boy ile yerleştirildi.
```

Noktalar sırasıyla (10, +5), (30, −5) ve (60, +5) metrededir — ikincisi çizginin
öbür tarafındadır.

Bir bina cephesini çizgiyle birleştirerek:

```text
DİKAYAK 0,0 100,0 ayak=12 boy=4 ayak=12 boy=9 ayak=28 boy=9 ayak=28 boy=4 cizgi=evet
```

```text
4 nokta dik ayak/dik boy ile yerleştirildi ve çizgiyle birleştirildi.
```

### Arayüz

**Çizim > Dik Ayak** menüsünden ya da araç kutusundaki **Nokta** düğmesini
basılı tutup açılan karttan **Dik Ayak**'ı seçin.

1. Taban çizgisinin ilk noktasına tıklayın.
2. İkinci noktasına tıklayın — aradaki kılavuz fareyi izler.

**Taban çizgisi ekranda kalır.** Okumaları yazarken taban görünür durur, ve
`ayak`'ı yazdığınız anda dik inilecek **ayak noktası** da işaretlenir — `boy` tam
olarak oradan ölçülür. Taban çizgisi çizimin nesnesi değil, komutun hatırladığı
iki noktadır; eskiden ikinci nokta verildiği anda ekrandan kayboluyor ve okumalar
görünmeyen bir tabana göre yazılıyordu.
3. Komut satırı **ayak** ister ve odak kendiliğinden oraya geçer; sayıyı yazıp
   Enter'a basın.
4. Ardından **boy** ister; onu da yazın. Nokta hemen çizime düşer.
5. 3. ve 4. adım tekrarlanır. Bitirmek için **sağ tık** ya da **Esc**.

Nokta yakalama açıkken taban çizgisinin uçlarını mevcut nirengilere
yakalayabilirsiniz.

### Betik

```json
{
  "ad": "Bordür alımı",
  "komutlar": [
    { "cmd": "core.perp_offset", "args": {
        "baslangic": [0, 0],
        "bitis": [100000, 0],
        "ayak": [10, 30, 60],
        "boy": [5, -5, 5],
        "cizgi": true } }
  ]
}
```

Betikte koordinatlar **milimetredir** (`[0, 0]` ve `[100000, 0]` yani 0 m ve
100 m), `ayak` ile `boy` ise metredir — komut satırındaki gibi.

### Üçü de aynı

Aynı taban ve aynı iki çift, üç istemciden aynı belgeyi ve bayt bayt aynı
günlüğü bırakır; `tests/unit/test_proof.cpp` bunu sınar ve günlüğü yeniden
oynatınca aynı içerik hash'ini verir.

## Geri alma

Bir çağrı tek bir işlemdir: noktalar ve — `cizgi=evet` verildiyse — onları
birleştiren çizgi tek bir **Ctrl+Z** ile birlikte gider. Yarısı kalmaz.

## Betikten kullanım

`DİKAYAK` betiklenebilir ve yapay zekâya açıktır. `ayak` ve `boy` birer **dizi**
olarak verilir; eşleşme sıraya göredir.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `Taban çizgisinin iki noktası aynı; dik indirilecek bir doğrultu yok.` | A ile B aynı nokta | İki ayrı nokta verin |
| `'core.perp_offset': zorunlu 'baslangic' parametresi eksik.` | Taban çizgisi verilmedi | `DİKAYAK <A> <B> …` yazın |
| `'core.perp_offset': 'ayak' parametresi sayı bekliyor, başka türde bir değer geldi.` | `ayak`a koordinat ya da metin verildi | Metre cinsinden bir sayı verin |
| `Katman kilitli: …` | Aktif katman kilitli | `KATMAN` ile kilidi kaldırın ya da başka katmanı aktif yapın |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili

- [Komut satırı](komut-satiri.md) — `dik(A,B,ayak,boy)` nokta fonksiyonu, aynı hesabın yazılı hâli
- [NOKTA](point_draw.md) — tıklanan tek nokta
- [NOKTALAR](points.md) — nokta no, Y, X listesinden okuma
- [ÇOKLUÇİZGİ](polyline.md) — noktaları elle birleştirmek
