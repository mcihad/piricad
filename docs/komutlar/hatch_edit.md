# TARAMADÜZENLE — Çizilmiş Taramayı Düzenleme

Bir imar lekesinin desenini değiştirmek, bir malzeme taramasının açısını ya da
sıklığını paftaya uydurmak, çapraz taramaya çevirmek ya da bir parselin içindeki
havuzun taranıp taranmayacağına karar vermek isteyen herkes için; bu sayfayı
bitirdiğinizde bunları arayüzden, komut satırından ve betikten yapmayı bileceksiniz.

## Ne yapar

`TARAMADÜZENLE`, çizimde **duran** taramaların desenini, açısını, ölçeğini, kendi
aralığını, çapraz çizimini, desen başlangıcını ve **ada kuralını** değiştirir. Sınırı
değişmez; [bağlı](hatch.md#bağlı-tarama) bir tarama bağlı kalır ve sınırını izlemeyi
sürdürür.

**Verilmeyen hiçbir şey değişmez.** Yalnız `aci` verirseniz desen, ölçek ve adalar
olduğu gibi kalır.

**Taramanın kendisini ya da sınırını gösterebilirsiniz.** Tarama doldurduğu parselin
kenarları üzerinde durur; parseli gösterdiğinizde (`nesneler=` ile ya da tıklayarak)
ona bağlı taramalar düzenlenir. Arayüzde bu araç yalnız tarama sorduğu için, parselin
kenarına tıklamak "hangisi?" diye sormadan taramayı seçer.

**Ada kuralı** (`stil`), iç içe sınırların nasıl taranacağıdır:

| `stil` | Ne olur |
|---|---|
| `normal` | İç içe sınırlar sırayla delik ve dolu: parsel dolu, içindeki havuz boş, havuzdaki ada dolu |
| `dis` | Yalnız en dıştaki sınırlar ve onların ilk delikleri |
| `yoksay` | Adalar yok sayılır: her şey dolu |

Bağlı bir taramada adalar sınır nesnelerinden yeniden bulunur, yani `yoksay`dan
`normal`e dönünce havuz yine delik olur. Bağsız bir taramada kural yalnız elindeki
halkalara uygulanır: kaybettiği bir adayı yeniden bulamaz.

## Adlar

| Ad | Tür |
|---|---|
| `TARAMADÜZENLE` | Türkçe, birincil |
| `TARAMADUZENLE` | ASCII karşılık |
| `HATCHEDIT` | İngilizce karşılık |
| `TDZ` | Kısaltma |
| `core.hatch_edit` | Komut kimliği |

## Sözdizimi

```text
TARAMADÜZENLE [nesneler=<kimlik> …] [desen=<ad>|aralik=<metre>] [aci=<derece>] [olcek=<çarpan>]
              [cift=evet|hayır] [baslangic=<nokta>] [stil=normal|dis|yoksay]
```

`nesneler` verilmezse **seçim** kullanılır; o da boşsa taramayı göstermeniz istenir.
Hiçbir şey verilmezse desen sorulur ve taramanın bugünkü deseni önerilir.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `nesneler` | Düzenlenecek taramalar ya da onların sınır nesneleri. Ne tarama ne de bir taramanın sınırı olan nesneler atlanır ve sayılır |
| `desen` | Katalogdaki desen adı |
| `aralik` | Kendi desen çizgilerinizin aralığı, metre; `desen=` yerine |
| `aci` | Desenin dönme açısı, derece |
| `olcek` | Desen ölçeği |
| `cift` | Desen bir de dik açıyla çizilsin mi (çapraz tarama) |
| `baslangic` | Desenin geçtiği nokta |
| `stil` | Ada kuralı: `normal`, `dis`, `yoksay` |
| `katalog` | Desen kataloğu dosyası; varsayılan `TERCİH desen_kataloğu` |

Tipleri ve adetleri için üretilmiş [komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Havuzlu bir parselin taramasını `ANSI37` yapmak, döndürmek, sonra havuzu da taramak:

<!-- örnek: yeni çizim -->
```
ALAN 0,0 30,0 30,30 0,30
DAİRE merkez=15,15 cevre=20,15
TARAMA nesneler=1 nesneler=2 desen=ANSI31
TARAMADÜZENLE nesneler=3 desen=ANSI37 aci=15
TARAMADÜZENLE nesneler=3 stil=yoksay
```

```text
Tarama düzenlendi: 1 tarama; 'ANSI37' deseni, açı 15,00°.
Tarama düzenlendi: 1 tarama; 'ANSI37' deseni, açı 15,00°.
```

### Arayüz

**Çizim ▸ Tarama ▸ Taramayı Düzenle**, ya da **Giriş ▸ Çizim** panelindeki **Tarama**
düğmesinin okundan **Taramayı Düzenle**: taramayı tıklayın, Enter'a basın, deseni yazın.
Komut çalışmıyorken taramaya **çift tıklamak** da aynısıdır: tarama tek başına seçilir ve
komut desenini sorar.

Açıyı, ölçeği, çapraz çizimi ve ada kuralını tek tek değiştirmek için taramayı seçin:
beliren **Tarama** sekmesinde desen galerisi, **Açı** ve **Ölçek** kutuları, **Çapraz** ve
üç ada kuralı vardır; her biri seçili taramalarda bu komutun ilgili satırını çalıştırır.
Aynı değerler nitelik panelinin **TARAMA** grubunda da birer hücredir.

### Betik

```json
{
  "komutlar": [
    { "cmd": "core.area",
      "args": { "noktalar": [[0, 0], [20000, 0], [20000, 10000], [0, 10000]] } },
    { "cmd": "core.hatch", "args": { "nesneler": [1], "desen": "ANSI31" } },
    { "cmd": "core.hatch_edit", "args": { "nesneler": [2], "aralik": 1.5, "cift": true } }
  ]
}
```

## Geri alma

Tek adımdır: [`GERİAL`](undo.md) bütün taramaları düzenlemeden önceki hâline döndürür.

## Betikten kullanım

Betikten çağrıldığında komut hiçbir şey sormaz; hiçbir şey verilmemişse ne
verileceğini söyleyerek reddeder. Günlüğe `nesneler` ve verilen her parametre yazılır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `Seçimde tarama yok; TARAMADÜZENLE yalnız taramaları düzenler.` | Seçilenlerin hiçbiri tarama değil | Taramayı seçin ya da `nesneler=` verin |
| `aralik kendi çizdiğiniz desenin aralığıdır; katalogdaki bir desenle birlikte verilmez: …` | `desen=` ve `aralik=` birlikte verildi | Birini verin |
| `Tarama aralığı sıfırdan büyük olmalı.` | `aralik=0` ya da eksi | Pozitif bir aralık verin |
| `Tanınmayan tarama deseni: '…'. Katalogdaki desenler: …` | Desen katalogda yok | Listelenen desenlerden birini yazın |
| `Değiştirilecek bir şey verilmedi: …` | Betikten hiçbir parametre verilmedi | Değiştireceğiniz alanı verin |

## İlgili

- [`TARAMA`](hatch.md) — tarama çizmek; bağlı tarama ve delikler
- [Tarama nesnesi](../nesneler/tarama.md)
