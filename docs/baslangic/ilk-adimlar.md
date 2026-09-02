# İlk Adımlar

KentOSCad'i ilk kez açan kullanıcı için; bu sayfayı bitirdiğinizde çizgi çizmiş, katman
yaratmış, yaptığınızı geri almış ve bir betik çalıştırmış olacaksınız.

Programı henüz derlemediyseniz önce [Kurulum](kurulum.md) sayfasına bakın.

## 1. Programı açın

```bash
make run
```

Transkript panelinde açılış satırlarını görürsünüz:

```text
KentOSCad 0.1.0 — komut merkezli mimari, GPLv3.
Aynı komut arayüzden, komut satırından ve betikten tıpatıp aynı yolu izler.
Başlamak için: ÇİZGİ  ·  ÇİZGİ 485320,4310220 @50,30 @100<45  ·  YARDIM
```

Üstte beş araç çubuğu, solda araç kutusu, sağda katman ve öznitelik panelleri vardır.

Bu sürümde komut satırı varsayılan olarak gizlidir. Aşağıdaki adımların bir kısmı onu
kullanıyor; açmak için **Ctrl+9**'a basın (kapatmak için de aynı kısayol).

## 2. Fareyle çizgi çizin

Sol kenardaki araç kutusundan **Çizgi** düğmesine basın. Durum çubuğu ve komut satırı
`İlk nokta` ister. Harita alanına sol tıklayın; sonraki istek `Sonraki nokta` olur ve
imlecinizi takip eden kesikli bir kılavuz çizgi belirir. Birkaç nokta daha tıklayın,
sonra **Esc**'e basın veya sağ tıklayın.

Çizdiğiniz her segment ayrı bir nesnedir, ama hepsi **tek bir geri alma adımıdır**.

## 3. Klavyeyle çizgi çizin

**Ctrl+9** ile komut satırını açın ve yazın:

```
ÇİZGİ 485320.150,4310220.400 @50,30 @100<45
```

Enter'a basın. Üç köşeli bir çizgi belirir. Girdiğiniz üç koordinat üç ayrı biçimdedir:

- `485320.150,4310220.400` — mutlak koordinat, metre
- `@50,30` — bir önceki noktadan 50 m doğu, 30 m kuzey
- `@100<45` — bir önceki noktadan 45° yönünde 100 m

Komut adı yerine kısaltma da yazabilirsiniz: `Ç`, `L` ve `LINE` aynı komuttur.
Bütün biçimler için [Komut satırı](../komutlar/komut-satiri.md) sayfasına bakın.

## 4. Katman yaratın

```
KATMAN ad=PARSEL renk=4281236786
```

`PARSEL` katmanı yaratılır ve aktif katman olur. Transkriptte şunu görürsünüz:

```text
Aktif katman: PARSEL
```

Sağdaki **Katmanlar** sekmesinde yeni katman, renk kutucuğu ve nesne sayısıyla belirir.
Üstteki **Katman** araç çubuğundaki liste de artık `PARSEL` gösterir; oradan başka bir
katman seçmek aynı komutu gönderir. Durum çubuğunun sağında da aktif katman adı yazar. Bundan sonra çizdiğiniz her şey bu
katmana gider.

`renk` değeri `0xAARRGGBB` biçiminde bir tam sayıdır; `4281236786` yeşile karşılık
gelir. Ayrıntı: [Katman yönetimi](../komutlar/layer.md).

## 5. Geri alın ve yineleyin

```
GERİAL
```

veya **Ctrl+Z**. Transkript ne geri alındığını söyler:

```text
Geri alındı: İki veya daha fazla nokta arasında doğru parçaları çizer.
```

`YİNELE` (veya **Ctrl+Shift+Z**) geri aldığınızı iade eder. Bir komut = bir adım
kuralı gereği, dört noktalı bir çizgi tek `GERİAL` ile tamamen kalkar.

## 6. Görünümü ayarlayın

```
YAKINLAŞ KAPSAM
```

veya **Ctrl+0**. Çizimin tamamı pencereye sığar. Fare tekerleği yakınlaştırır, orta
fare tuşu basılı sürüklemek kaydırır. `YAKINLAŞ` başka bir komut çalışırken de
kullanılabilir; çalışan komutu bozmaz.

## 7. Bir betik çalıştırın

Kapatın ve örnek çizimle açın:

```bash
make run-script SCRIPT=tests/journal/ornek-parsel.json
```

Beş katman ve on dört nesne yüklenir; transkript her adımı yazar:

```text
Aktif katman: PARSEL
Aktif katman: YOL
Aktif katman: BINA
Aktif katman: SINIR
Betik tamamlandı: tests/journal/ornek-parsel.json
```

Şimdi **Ctrl+Z**'ye basın. Dokuz komutluk betiğin tamamı tek adımda kalkar: bir betik
bloğu tek bir geri alma adımıdır. Ctrl+Shift+Z ile geri getirin.

## 8. Ne yaptığınıza bakın

Alt taraftaki **Komut Günlüğü** sekmesine geçin. Yaptığınız her şey JSON satırları
hâlinde durur:

```json
{"seq":1,"cmd":"core.layer","args":{"ad":"PARSEL","renk":4281236786},"origin":"script","crs":"TUREF/TM30","katman":"PARSEL"}
```

`origin` alanı komutun nereden geldiğini söyler: `gui`, `cli`, `script`, `ai`, `batch`.
Bu günlük yalnızca bir kayıt değil; geri almanın, makro kaydının ve oturum kurtarmanın
ortak kaynağıdır. Ayrıntı: [Komut günlüğü](../mimari/gunluk.md).

## 9. Komutları keşfedin

```
YARDIM
```

Bütün komutları adları ve açıklamalarıyla listeler. Tek bir komutun ayrıntısı için:

```
YARDIM komut=ÇİZGİ
```

Aynı listeye **Yardım > Komut Listesi** menüsünden de ulaşılır. Her iki liste de komut
kaydından üretilir; elle tutulan ikinci bir liste yoktur.

## Sırada ne var

- [Arayüz](arayuz.md) — pencerede ne nerede
- [Komut sistemi](../komutlar/README.md) — KentOSCad'in çalışma mantığı
- [Komut satırı](../komutlar/komut-satiri.md) — koordinat girişinin tamamı
- [Sorun giderme](../sorun-giderme.md) — bir şey ters giderse
