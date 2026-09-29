# KAPSAMDENETİM — Kapsam Denetimi

Kapsama yakınlaştığında çizimi bir nokta kadar küçük gören herkes için; bu sayfayı
bitirdiğinizde çizimin geri kalanından kopuk nesneleri bulmayı ve onlar hakkında ne
yapacağınıza kendiniz karar vermeyi bileceksiniz.

## Ne yapar

`YAKINLAŞ KAPSAM` bütün Türkiye'yi gösteriyorsa, çizimde neredeyse her zaman birkaç
nesne yanlış yerdedir: koordinatını yitirip 0,0'a düşmüş bir nokta, başka bir TM
diliminde gelmiş bir blok (aynı plan TM36 ile TM39 arasında 250 km kayar), sağa ve
yukarı değerleri yer değiştirmiş bir çizgi. `KAPSAMDENETİM` bu nesneleri **bulur,
tuvalde işaretler ve adlarıyla bildirir; hiçbirini taşımaz.**

Netcad bunları Shift+Limit Bul ile kendiliğinden HATALI tabakasına taşır. Burada karar
sizindir: komut, sonuç satırlarının altına işi yapacak satırı yazar — seçmek için bir
`SEÇ`, ayrı bir katmana almak için bir `KATMANAT` — ve çalıştırıp çalıştırmamak size
kalır. Kadastro çizimi resmî bir belgedir; hangi nesnenin yanlış olduğunu mühendis söyler.

### Kural

Bir nesne, onu çizimin çoğunluğundan ayıran **boş bir bant** varsa kopuktur:

1. Her nesnenin kapsamının ortası alınır. Çoğunluğun merkezi bu ortaların **ortancasıdır**
   (sağa ve yukarı değer ayrı ayrı); birkaç uzak nesne onu kendine çekemez.
2. Nesneler bu merkeze uzaklıklarına göre dizilir. **Yakın yarısı** her zaman
   çoğunluktur.
3. Dışarı doğru yürünür: sıradaki nesneye kadarki boşluk, çoğunluğun o ana kadarki
   yarıçapının **kopukluk çarpanı** katından (varsayılan 8) genişse o boşluğun ötesindeki
   her şey kopuktur. Değilse nesne çoğunluğa katılır ve yürüyüş sürer. Yarıçap 100
   metreden küçük sayılmaz; bir parselin yanındaki röper noktası kopuk görünmesin.

Bu yüzden yavaş yavaş seyrekleşen bir çizimde — yoğun bir merkez, dağınık bir çevre —
kopuk nesne **çıkmaz**: çevredeki hiçbir boşluk içindekinin sekiz katı değildir. Yalnız
gerçekten bir boşlukla ayrılmış olanlar bildirilir.

Üçten az nesnede çoğunluk yoktur; komut bunu söyler ve bir şey bildirmez.

### Kopukluk çarpanı

Eşik bir **proje ayarıdır**: `kopukluk_çarpanı` (`core.denetim.kopukluk_carpani`),
2 ile 1000 arasında bir tam sayı. Büyüdükçe denetim yalnız çok uzaktakileri bildirir.

```
AYAR kopukluk_çarpanı 20
```

Çarpan çok büyük seçilirse en yakın kopuk nesne bandın içinde kalıp çoğunluğa katılır ve
ondan sonrakiler de onun yanında sayılır; 2 km'lik bir kasabada 250 km uzaktaki bir
nesne çarpan 100'de kopuktur, 200'de değildir.

## Adlar

| Ad | Tür |
|---|---|
| `KAPSAMDENETİM` | Türkçe, birincil |
| `KAPSAMDENETIM` | ASCII karşılık |
| `EXTENTCHECK` | İngilizce karşılık |
| `KPD` | Kısaltma |
| `core.extent_check` | Komut kimliği |

## Sözdizimi

```
KAPSAMDENETİM
```

## Parametreler

Parametre almaz. Eşik proje ayarından okunur (bkz. [Kopukluk çarpanı](#kopukluk-çarpanı)).

## Örnekler

### Komut satırı

Yüz parselli bir ada ve koordinatını yitirmiş bir nokta:

```
KATMAN ad=PARSEL
ALAN 485300,4310200 485320,4310200 485320,4310220 485300,4310220
DİZİ nesneler=1 satir=10 sutun=10 satir_aralik=20 sutun_aralik=20
NOKTA 10,10
KAPSAMDENETİM
```

Çıktı:

```text
KAPSAMDENETİM: 101 nesne denetlendi; 1 nesne çizimin çoğunluğundan kopuk (çoğunluğun yarıçapı 141,4 m, kopukluk çarpanı 8):
  nesne 101 · PARSEL · çoğunluğun merkezinden 4337,5 km · sıfıra yakın: koordinatını yitirmiş olabilir
Seçmek için: SEÇ nesneler=101
Ayrı bir katmana almak için: KATMANAT nesneler=101 katman=HATALI
```

**Sıfıra yakın** notu yalnız çoğunluk 0,0'dan 100 km'den uzaksa düşer: TUREF'te 0,0'da
parsel olmaz, ama yerel bir çizimde istasyon oradadır.

Yazdığı satırla nesneyi ayrı bir katmana alın — isterseniz:

```
KATMANAT nesneler=101 katman=HATALI
```

### Arayüz

**Analiz ▸ Denetim ▸ Kapsam Denetimi** aynı komutu çalıştırır. Sonuç Transkript'e yazılır,
tuvalin üstünde kopuk nesneleri seçen **Seç** düğmesi belirir (transkriptteki `SEÇ` satırını
çalıştırır) ve her kopuk nesne tuvalde `kopuk · 4337,5 km` yazılı bir işaretle gösterilir; işaretler
çizim değişince ya da hiçbir komut çalışmıyorken **Esc**'e basınca kalkar. Uzak nesneyi
görmek için `YAKINLAŞ KAPSAM`, bulduktan sonra ona gitmek için
[`YAKINLAŞ SEÇİM`](zoom.md).

### Betik

```json
{
  "ad": "Kopuk nesneleri bul",
  "komutlar": [ { "cmd": "core.extent_check", "args": {} } ]
}
```

### Üçü de aynı

Arayüzdeki düğme, komut satırı ve betik aynı komutu çalıştırır ve aynı cevabı alır.

## Yapılandırılmış cevap

```json
{ "denetlenen": 101, "carpan": 8, "merkez": [485390000, 4310290000], "yaricap_mm": 141421,
  "kopuk": [ { "nesne": 101, "katman": "PARSEL", "merkez": [10000, 10000],
               "uzaklik_mm": 4337523190, "sifira_yakin": true } ] }
```

| Alan | Anlamı |
|---|---|
| `denetlenen` | Kapsamı olan, kurala giren nesne sayısı |
| `carpan` | Kullanılan kopukluk çarpanı |
| `merkez` | Çoğunluğun merkezi, milimetre |
| `yaricap_mm` | Çoğunluğun, bandın başladığı yerdeki yarıçapı |
| `kopuk` | Kopuk nesneler, yakından uzağa: kimliği, katmanı, kapsamının ortası, merkeze uzaklığı ve sıfıra yakın olup olmadığı |

Transkript en çok yirmi nesnenin kimliğini sonraki adım satırlarına yazar; `kopuk`
listesinde hepsi vardır.

## Geri alma

Geri alınacak bir şey yoktur: `KAPSAMDENETİM` çizimi değiştirmez, geri alma yığınına ve
komut günlüğüne girmez. Tuvaldeki işaretler görünüm durumudur.

## Betikten kullanım

Betiklenebilir ve yapay zekâ erişimlidir. Hiçbir şeyi değiştirmediği için bir yapay zekâ
istemcisi onu onay beklemeden çalıştırabilir; bulduklarıyla ne yapılacağı — `KATMANAT`
gibi çizimi değiştiren bir adım — yine öneri ve onay yolundan geçer.

## Hatalar

| Mesaj | Sebep | Çözüm |
|---|---|---|
| `KAPSAMDENETİM: kopukluk için en az üç nesne gerekir; çizimde 2 nesne var.` | Çoğunluğu olmayan bir çizim | Hata değildir; bildirilecek bir şey yoktur |
| `KAPSAMDENETİM: 101 nesne denetlendi; hiçbiri çizimin çoğunluğundan kopuk değil.` | Kopuk nesne yok | Hata değildir |
| `'core.extent_check' daha fazla argüman almıyor. Fazlalık: …` | Komut parametre almaz | Yalnız `KAPSAMDENETİM` yazın; eşiği `AYAR kopukluk_çarpanı` ile değiştirin |

Bütün hata mesajları: [Sorun giderme](../sorun-giderme.md).

## İlgili sayfalar

- [`YAKINLAŞ`](zoom.md) — kapsama, seçime ve katmana yakınlaşmak
- [`TOPOLOJİ`](topology.md) — paftanın geometri denetimi
- [`KATMANAT`](set_layer.md) — nesneleri başka bir katmana almak
- [`AYAR`](setting.md) — proje ayarları
