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

Üstte **şerit** (sekmelere ve panellere ayrılmış bütün komutlar), sağda katman ve öznitelik
panelleri, altta komut satırı ve durum çubuğu vardır.

Aşağıdaki adımların bir kısmı komut satırını kullanır; her zaman açıktır, **Ctrl+9** onu
gizler ve geri getirir.

## 2. Fareyle çizgi çizin

Şeridin **Giriş** sekmesinde, **Çizim** panelinin ilk büyük düğmesi olan **Çizgi**'ye basın.
Durum çubuğu ve komut satırı
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
- `@100<45` — bir önceki noktadan 45 grad yönünde 100 m. Açı **semt açısıdır**:
  kuzeyden saat yönünde sayılır ve varsayılan birimi grad'dır (45 grad = 40,5°).
  Derece yazmak için sonek ekleyin: `@100<45d`

Komut adı yerine kısaltma da yazabilirsiniz: `Ç`, `L` ve `LINE` aynı komuttur.
Bütün biçimler için [Komut satırı](../komutlar/komut-satiri.md) sayfasına bakın.

Beşinci bir biçim daha var: koordinatı yazmak yerine **onu nasıl bulduğunuzu**
yazabilirsiniz. Arazide en çok kullanılanı dik ayak / dik boy'dur — bir cepheyi
taban alıp üzerinde kaç metre gidildiğini ve oradan kaç metre yana çıkıldığını
söylemek:

```
ÇİZGİ dik(0,0,100,0,30,5) dik(0,0,100,0,30,-5)
```

Taban `0,0` → `100,0` doğrusudur. İki nokta da taban üzerinde 30 metre ilerideki
noktadan 5 metre yana çıkar: ilki **sola**, ikincisi sağa. Kural budur —
**sol pozitif, sağ negatif**, tabana A'dan B'ye yürüyor gibi bakarak. `0,0`'dan
`100,0`'a, yani doğuya yürürken sol el kuzeyi gösterir, bu yüzden ilk nokta
`(30, 5)`, ikincisi `(30, −5)` olur.

`orta`, `kes`, `semt`, `ara` ve numaralı ölçü noktasını getiren `n(1284)` de aynı
şekilde yazılır: [Nokta fonksiyonları](../komutlar/komut-satiri.md#nokta-fonksiyonları).

## 4. Katman yaratın

```
KATMAN ad=PARSEL renk=4281236786
```

`PARSEL` katmanı yaratılır ve aktif katman olur. Transkriptte şunu görürsünüz:

```text
Aktif katman: PARSEL
```

Sağdaki **Katmanlar** sekmesinde yeni katman, renk kutucuğu ve nesne sayısıyla belirir.
Şeridin **Giriş ▸ Katmanlar** panelindeki katman listesi de artık `PARSEL` gösterir; oradan
başka bir katman seçmek aynı komutu gönderir. Bundan sonra çizdiğiniz her şey bu katmana
gider.

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

Komutları kategorilerine göre özetler ve **Komut Listesi sayfasını** açar: solda bütün
komutlar, sağda seçili komutun parametreleri. Tek bir komutun ayrıntısı için:

```
YARDIM komut=ÇİZGİ
```

Sayfa o komutun üzerinde açılır. Aynı sayfaya **KentOS CAD ▸ Komut Listesi** (`F1`) ve
`Ctrl+K` ile de ulaşılır; üçü de aynı komutu çalıştırır. Liste komut kaydından üretilir;
elle tutulan ikinci bir liste yoktur.

## Sırada ne var

- [Arayüz](arayuz.md) — pencerede ne nerede
- [Komut sistemi](../komutlar/README.md) — KentOSCad'in çalışma mantığı
- [Komut satırı](../komutlar/komut-satiri.md) — koordinat girişinin tamamı
- [Sorun giderme](../sorun-giderme.md) — bir şey ters giderse
