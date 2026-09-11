# İşlem Araçları

Bir paftadaki yüzlerce nesneye aynı işi tek seferde uygulamak isteyen herkes için:
her kenara uzunluğunu yazmak, her parselin köşelerini numaralamak, ileride tampon
çizmek, sadeleştirmek, birleştirmek. Bu sayfayı bitirdiğinizde bir işlem aracını
**Araçlar** panelinden, komut satırından ve betikten çalıştırmayı, kapsamını seçmeyi,
sonucunu ayrı bir katmana yazmayı ve süren bir işlemi durdurmayı bileceksiniz.

## İşlem aracı nedir

Bir **işlem aracı**, kapsamındaki bütün nesnelere aynı işi uygulayan komuttur. QGIS'in
"Processing" araçlarının KentOSCad'deki karşılığıdır, ama bu programın kuralı geçerlidir:
**her araç bir komuttur.** Parametreleri bir kere bildirilir ve komut sisteminin
doğrulamasından geçer; panelden basmak, komut satırına yazmak, betikten çağırmak ve
yapay zekânın önermesi aynı yoldan gider ve aynı komut günlüğü satırını üretir.

Bir aracı sıradan bir komuttan ayıran dört şey vardır:

| Özellik | Ne demek |
|---|---|
| **Uygulandığı türler** | Her araç hangi geometri sınıflarına uygulandığını söyler: nokta, çizgi, alan, eğri, yazı. Kapsamdaki uymayan nesneler **atlanır ve sayılır**; sessizce kaybolmaz |
| **Kapsam** | Nesneler seçimden, görünümden ya da bütün projeden alınır |
| **Asenkron** | İş, arayüz donmasın diye ayrı iş parçacığında koşar; durum çubuğunda adı, yüzdesi ve **Durdur** vardır |
| **Çıktı katmanı** | Sonuç yeni nesneler olarak istediğiniz katmana yazılır; katman yoksa oluşturulur |

## Kapsam

| `kapsam=` | Nesneler nereden gelir |
|---|---|
| `secili` (varsayılan) | Etkin seçim. Seçim boşsa araç tuvalden seçtirir: her tık ekler, **sağ tık** başlatır |
| `gorunum` | Görünümün içine giren ya da ona değen nesneler. Görünümün iki köşesi `pencere=<x1,y1> pencere=<x2,y2>` ile verilir; panel bunu kendisi doldurur |
| `proje` | Çizimdeki bütün nesneler |

`nesneler=<kimlik>` verilirse kapsam okunmaz; araç yalnız o nesnelere uygulanır. Komut
günlüğüne her zaman **uygulanan nesnelerin kimlikleri** yazılır, kapsam sözcüğü değil:
günlük yeniden oynatıldığında o günkü seçim ya da görünüm değil, aynı nesneler işlenir.

## Asenkron çalışma ve Durdur

Bir araç üç adımda çalışır. Önce kapsamdaki nesnelerin **kopyası** alınır; sonra iş
bu kopya üzerinde ayrı iş parçacığında koşar ve durum çubuğu `Kenar uzunluklarını yaz ·
%42` gibi ilerler; en sonda sonuç, komutun **tek işlemi** içinde çizime yazılır. İş
sürerken çizime dokunulmaz. **Durdur** çipi ya da **Esc** işi keser: araç "İşlem
durduruldu; çizim değişmedi" der ve hiçbir şey yazılmaz. Bitmiş bir araç tek geri alma
adımıdır; [`GERİAL`](../komutlar/undo.md) ürettiği her şeyi birlikte kaldırır.

Komut satırından ve betikten çağrıldığında iş yerinde koşar; belge ve günlük her iki
yolda da birebir aynıdır.

## Araçlar paneli

Sağ paneldeki **Araçlar** sekmesi (Öznitelikler ve Geçmiş'in yanında; **Analiz ▸ İşlem
Araçları ▸ Araçlar Paneli** de açar) araçları gruplar hâlinde bir ağaçta gösterir. Üstteki
kutu ada, açıklamaya ya da komut adına göre süzer. Bir satırı seçince altında aracın
**kartı** açılır:

- adı ve bir cümlelik açıklaması; uygulandığı türler birer çip olarak;
- **Kapsam**: Seçili · Görünüm · Proje;
- **Parametreler**: aracın her parametresi için bir alan; seçenekli olanlar açılır liste;
- **Çıktı**: katman adı — boş bırakılırsa etkin katman;
- gönderilecek **komut satırı**, olduğu gibi. Kartta ne görüyorsanız komut satırına
  yazılacak olan odur; bu satırı kopyalayıp bir betiğe koyabilirsiniz;
- **Çalıştır**. Ağaçta bir satıra çift tıklamak ya da Enter da çalıştırır.

Kartın **kendi penceresinde** açılmasını isterseniz `Ayarlar ▸ Uygulama ▸ Görünüm ve Tema`
altındaki **Araç penceresi** tercihini (`TERCİH araç_penceresi evet`) açın: ağaçta bir
araca tıklamak kartı bir pencerede açar, **Çalıştır** çalıştırıp pencereyi kapatır.
Kapalıyken kart ağacın altında açılır. İki yol da aynı komut satırını gönderir.

Kart, o aracın bu çizimde **en son çalıştığı değerlerle** açılır: değerler komut
günlüğünden okunur, ayrı bir yerde tutulmaz. İstemiyorsanız `AYAR son_değerler hayır`
(`core.islem.hatirla`, proje kapsamı).

Bir araç, işi başlamadan **soru sorabilir**: `ALANDÜZENLE` kenarı ya da köşeyi
tıklatır ve sürükletir. Sorduğu her şey günlüğe yazılır; yeniden oynatma el gerektirmez.

## Araçlar

| Grup | Araç | Komut | Sayfa |
|---|---|---|---|
| Etiketleme | Kenar uzunluklarını yaz | `UZUNLUKYAZ` | [Kenar uzunluklarını yazma](../komutlar/uzunluk_yaz.md) |
| Etiketleme | Köşeleri numarala | `KÖŞENUMARALA` | [Köşe numaralama](../komutlar/kose_numarala.md) |
| Düzenleme | Alanı düzenle | `ALANDÜZENLE` | [Alanı istenen değere getirme](../komutlar/alan_duzenle.md) |

Yeni bir araç eklendiğinde ağaçta, **Analiz** menüsünde ve
[komut referansında](../komutlar/referans.md) kendiliğinden görünür; üçü de aynı
kayıttan üretilir.

## Betikten kullanım

Bir araç betikte komut kimliğiyle çağrılır; `nesneler` ya da `kapsam` verilmelidir,
çünkü betik çalışırken "etkin seçim" olmayabilir:

```json
{
  "ad": "Kenar uzunlukları",
  "komutlar": [
    { "cmd": "islem.uzunluk_yaz",
      "args": { "kapsam": "proje", "birim": "metre", "ondalik": 2, "katman": "UZUNLUK" } }
  ]
}
```

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Kapsamda bu araca uygun nesne yok (N nesne bakıldı). Araç şunlara uygulanır: …` | Kapsamdaki hiçbir nesne aracın aldığı türlerden değil | Uygun nesneleri seçin ya da kapsamı genişletin |
| `gorunum kapsamı görünümün iki köşesini ister: pencere=…` | Komut satırından `kapsam=gorunum` verildi ama `pencere` yok | İki köşeyi verin ya da paneli kullanın |
| `Tanınmayan kapsam: 'X'. Kapsamlar: secili, gorunum, proje.` | `kapsam` sözcüğü yanlış | Üç sözcükten birini yazın |
| `'birim' için tanınmayan değer: 'X'. Seçenekler: …` | Seçenekli bir parametreye listede olmayan sözcük verildi | Mesajdaki seçeneklerden birini yazın |
| `İşlem durduruldu; çizim değişmedi.` | Durdur ya da Esc'e basıldı | Bir hata değil; yeniden çalıştırın |

## İlgili

- [Arayüz](../baslangic/arayuz.md) — sağ panel ve durum çubuğu
- [Komut sistemi](../komutlar/README.md) — neden her araç bir komuttur
- [Betik yazma](../betik/README.md)
