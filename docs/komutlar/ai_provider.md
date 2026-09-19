# YAPAYZEKAMODELİ — Model Sağlayıcıları

Hangi yapay zeka modelinin, hangi makinede çalışacağına karar veren harita mühendisi ve
kurum bilgi işlemcisi için; bu sayfayı bitirdiğinizde model profillerini listelemeyi,
eklemeyi, silmeyi, birini varsayılan yapmayı ve bir uç noktanın bağlantısını denemeyi
bileceksiniz — ve **API anahtarının neden bu komutun bir parametresi olmadığını**.

## Ne yapar

Bir **model profili** adlandırılmış bir uç noktadır: adres, uç nokta yolu, model
kimliği, uç noktanın konuştuğu **lehçe** ve anahtarı tutan **kaydın adı**. Kurum birkaç
tane tutar — "Kurum vLLM", "Ollama", "DeepSeek" — ve tam **biri varsayılandır**: sohbet
onu kullanır ([Yapay Zeka paneli](../yapay-zeka/sohbet.md)).

`YAPAYZEKAMODELİ` bu listeyi yönetir.

| İşlem | Ne yapar |
|---|---|
| `listele` | Profilleri yazar; varsayılan `*` ile işaretlidir |
| `ekle` | Profil ekler, ya da aynı addaki profili **değiştirir** |
| `sil` | Bir profili siler; **son profil silinemez** |
| `varsayilan` | Birini varsayılan yapar |
| `dene` | Uç noktaya tek sözcüklük bir istek gönderir ve ne döndüğünü söyler |

Profiller **kullanıcı profilinizde** bir JSON dosyasında tutulur
(`ai-modelleri.json`); çizim dosyasına yazılmaz ve [`GERİAL`](undo.md) ile geri
alınmaz — bir yazıcı tercihi gibi, size ve makinenize aittir. Değişiklik **komut
günlüğüne yazılır**: hangi kurumun hangi modele, hangi adrese yöneldiği ve bunu ne
zaman yaptığı, günlüğün tuttuğu türden bir bilgidir.

**Anahtarın kendisi bu komuta hiç girmez.** `anahtar_ref` yalnız anahtarı **tutan
kaydın adını** taşır: işletim sisteminin anahtar deposundaki kaydın adını, ya da bir
**ortam değişkeninin** adını. Anahtar bu yüzden ne komut argümanına, ne günlük
satırına, ne komut dökümüne, ne bir denetim kaydına, ne de bir hata mesajına
ulaşabilir. Anahtarı **girdiğiniz yer** `Seçenekler ▸ Yapay Zeka Modelleri`
sayfasındaki tek alandır; ayrıntısı [Model sağlayıcıları](../yapay-zeka/modeller.md)
sayfasında.

Yeni bir kurulumda on beş profil gelir ve varsayılan, kendi makinenizde çalışan
**Ollama**'dır.

## Adlar

| Ad | Tür |
|---|---|
| `YAPAYZEKAMODELİ` | Türkçe, birincil |
| `YAPAYZEKAMODELI` | ASCII karşılık |
| `AIMODEL` | İngilizce karşılık |
| `YZM` | Kısaltma |
| `core.ai_provider` | Komut kimliği |

Bu komut **yapay zekaya açılmamıştır**: hangi modelin çalıştığını değiştirebilen bir
model, kendisini sınırlayan çerçeveyi düzenliyor olurdu. Modelin ne yapabildiği
[Onay ve denetim](../yapay-zeka/onay.md) sayfasında.

## Sözdizimi

```text
YAPAYZEKAMODELİ islem=listele
YAPAYZEKAMODELİ islem=ekle ad=<ad> lehce=<lehçe> adres=<adres> model=<model>
YAPAYZEKAMODELİ islem=ekle ad=<ad> anahtar_ref=<kayıt adı> baglam=<jeton> azami=<jeton>
YAPAYZEKAMODELİ islem=varsayilan ad=<ad>
YAPAYZEKAMODELİ islem=dene ad=<ad>
YAPAYZEKAMODELİ islem=sil ad=<ad>
```

Argümansız çağırırsanız komut önce işlemi, sonra profil adını sorar.

## Parametreler

| Parametre | Ne yapar |
|---|---|
| `islem` | `listele`, `ekle`, `sil`, `varsayilan` ya da `dene`. Zorunlu |
| `ad` | Profilin adı; `ekle`, `sil`, `varsayilan` ve `dene` için zorunlu |
| `lehce` | `openai_chat`, `openai_responses`, `anthropic_messages` ya da `ollama_native`; varsayılan `openai_chat` |
| `adres` | Uç noktanın adresi; `http://` ya da `https://` ile başlar, satıcının ön eki dahil |
| `yol` | Adresin altındaki uç nokta; `/` ile başlar. Verilmezse lehçenin kendi yolu kullanılır |
| `model` | Model kimliği, uç noktanın yazdığı gibi |
| `anahtar_ref` | Anahtarı **tutan kaydın adı** — anahtarın kendisi değil |
| `baglam` | Bağlam penceresi, jeton (0–100 000 000); `0` bilinmiyor demektir |
| `azami` | Çıktı jeton sınırı (0–10 000 000); `0` demek "bu alanı hiç gönderme" |
| `sicaklik` | Örnekleme sıcaklığı, 0 ile 2 arasında; verilmezse **hiç gönderilmez** |
| `akis` | Cevap parça parça mı istensin; varsayılan `evet` |
| `dusunme` | Modelin düşünme metni gösterilsin mi |
| `araclar` | Uç noktaya araç kataloğu gönderilsin mi; varsayılan `evet` |

`ekle`, var olan bir ada **yazar**: aynı adla ikinci kez eklemek o profili
değiştirir ve verilmeyen her alan **yerleşik varsayılanına** döner — ofisin o anki
varsayılan profiline değil.

`yol` verilmediğinde lehçenin kendi uç noktası yazılır: `openai_chat` için
`/chat/completions`, `openai_responses` için `/responses`, `anthropic_messages` için
`/messages`, `ollama_native` için `/api/chat`. `anthropic_messages` seçildiğinde kimlik
başlığı da `x-api-key` olur ve önüne `Bearer` konmaz — bu protokolün kuralıdır, ayar
değil.

Sözcükler Türkçe katlamayla eşleşir: `EKLE` ile `ekle`, `OPENAI_CHAT` ile
`openai_chat` aynıdır. Tipleri ve adetleri için üretilmiş
[komut referansına](referans.md) bakın.

## Örnekler

### Komut satırı

Profilleri listelemek — varsayılan `*` ile işaretlidir:

```text
YAPAYZEKAMODELİ islem=listele
```

Kurumun kendi vLLM sunucusunu eklemek ve varsayılan yapmak. Uç nokta yolu lehçeden
gelir, model adı vLLM'in sunduğu addır:

```text
YAPAYZEKAMODELİ islem=ekle ad="Kurum vLLM" lehce=openai_chat adres=http://vllm.kurum.lan:8000/v1 model=Qwen2.5-7B-Instruct baglam=32768
YAPAYZEKAMODELİ islem=varsayilan ad="Kurum vLLM"
```

Bulut profili: anahtarın **adı** yazılır, anahtarın kendisi değil. Buradaki
`DEEPSEEK_API_KEY` bir ortam değişkeninin adıdır; anahtar deposundaki bir kaydın adı
da olabilir:

```text
YAPAYZEKAMODELİ islem=ekle ad=DeepSeek lehce=openai_chat adres=https://api.deepseek.com/v1 model=deepseek-chat anahtar_ref=DEEPSEEK_API_KEY azami=4096
```

Bağlantıyı denemek. Cevap **hemen gelmez**: komut isteği gönderir, sonucu geldiğinde
komut dökümüne yazar — bir modelden cevap beklerken program donmaz:

```text
YAPAYZEKAMODELİ islem=dene ad=DeepSeek
```

```text
Model denemesi başladı: DeepSeek — https://api.deepseek.com/v1/chat/completions. Sonuç
geldiğinde bu satırın altına yazılacak (en çok 20 saniye).
Model denemesi — DeepSeek (https://api.deepseek.com/v1/chat/completions, deepseek-chat):
bağlantı kuruldu. HTTP 200, 412 bayt, 638 ms.
```

Silmek. Anahtar deposundaki kayıt **olduğu gibi kalır**: aynı anahtarı başka bir
profil de kullanıyor olabilir:

```text
YAPAYZEKAMODELİ islem=sil ad=DeepSeek
```

### Arayüz

`Seçenekler ▸ Yapay Zeka Modelleri` sayfasının başında **Yapay Zeka Modelleri**
tablosu vardır: her satır bir uç nokta — ad, lehçe, model ve bağlam penceresi — ve `●`
olan varsayılandır. Bağlam penceresinin yanında **kimin söylediği** yazar
(`sunucu`, `kullanıcı`, `gömülü`), çünkü "sunucu bildirdi" ile "biri yazdı" aynı şey
değildir.

Bir satır seçince dört düğme açılır: **Düzenle**, **Varsayılan yap**, **Bağlantıyı
dene** (sonucu komut dökümüne yazar) ve **Sil** (her zaman onay ister). Tablonun
altındaki **Yeni profil…** düğmesi — ve bir satıra çift tıklamak — **model profili
penceresini** açar.

O pencerede bir profilin tamamı vardır, beş alanı değil:

| Bölüm | İçindekiler |
|---|---|
| **Sağlayıcı** | Şablon seçici (katalogdaki 41 uç nokta), profil adı, lehçe |
| **Uç nokta** | Adres, yol; sağlayıcının zorunlu ek başlıkları bir satırda yazılı |
| **Model** | Seçtiğiniz uç noktanın **kendi modelleri** — yanlarında bağlam penceresi ve düşünüp düşünmediği — ve **Modelleri getir** |
| **Ayarlar** | Bağlam penceresi, azami çıktı, sıcaklık, düşünme kipi, akış, araçlar |
| **Anahtar** | Anahtar adı ve API anahtarı alanı |

Bir şablon seçmek bütün alanları o sağlayıcının değerleriyle doldurur; hepsi
düzeltilebilir. Model kutusu **yazılabilir** de: kendi sunucunuzun sunduğu ad hiçbir
katalogda olmayabilir.

**Modelleri getir**, uç noktaya modellerini sorar ve gelen liste katalogdakinin
yerine geçer — bir anahtarın gerçekten erişebildiği modeller ancak uç noktadan
öğrenilir. Sağlayıcının model listeleme ucu yoksa düğme kapalıdır ve nedenini söyler.

**API anahtarı** sayfadaki tek istisnadır: değeri ekranda noktalarla görünür, hiçbir
yere yazılmaz ve **bir komuta dönüşmez** — doğrudan işletim sisteminin anahtar
deposuna gider, alan da kaydedildikten sonra temizlenir. Geri kalan her şey
**Kaydet**'e basıldığında tek bir `YAPAYZEKAMODELİ islem=ekle` satırı olarak veri
yolundan geçer, dolayısıyla pencerenin komut satırında olmayan bir yolu yoktur.

Tablo komut satırını izler: `YAPAYZEKAMODELİ` ile yaptığınız değişiklik pencere
açıkken de görünür, çünkü ikisi aynı listeye bakar.

### Betik

Betikte bir satır olarak — kurumun profili:

```json
{ "cmd": "core.ai_provider",
  "args": { "islem": "ekle", "ad": "Kurum vLLM", "lehce": "openai_chat",
            "adres": "http://vllm.kurum.lan:8000/v1", "model": "Qwen2.5-7B-Instruct",
            "baglam": 32768, "araclar": true } }
```

Ve onu varsayılan yapmak:

```json
{ "cmd": "core.ai_provider", "args": { "islem": "varsayilan", "ad": "Kurum vLLM" } }
```

Bir kurulum betiği profilleri böyle dağıtır ve **hiçbir anahtar taşımaz**: anahtar her
makinede ya anahtar deposuna girilir ya bir ortam değişkeninde durur.

## Geri alma

Profiller uygulama ayarıdır: [`GERİAL`](undo.md) onlara dokunmaz. Yanlış bir
değişikliği geri almanın yolu profili yeniden `ekle` ile yazmak ya da silmektir.
`dene` hiçbir şeyi değiştirmez, yalnız sorar.

## Betikten kullanım

Betikte `islem` her zaman, `ad` ise `listele` dışındaki her işlemde verilmelidir:
betikte soracak kimse yoktur. Sayılar betikte de **sayıdır** (`baglam`, `azami`),
`akis` / `dusunme` / `araclar` ise **doğru/yanlış** değerleridir.

`dene` bir betikte de isteği başlatır ama cevabı beklemez: betik biter, sonuç komut
dökümüne düşer. Bir bağlantıyı betikten sınamak istiyorsanız çıktıyı dökümden
okuyun.

Komut satırı, arayüz ve betik aynı satırı üretir ve aynı günlük kaydını bırakır;
bu eşitlik `tests/unit/test_ai_provider.cpp` içinde kanıtlanır.

## Hatalar

| Mesaj | Sebebi | Çözümü |
|---|---|---|
| `'core.ai_provider': 'islem' için tanınmayan değer 'X'. Kabul edilenler: listele / ekle / sil / varsayilan / dene` | `islem` beş sözcükten biri değil | Beşinden birini yazın |
| `Tanınmayan işlem: 'X'. İşlemler: listele / ekle / sil / varsayilan / dene` | Aynı hata, komut satırında sorulan işleme verildiğinde | Beşinden birini yazın |
| `'sil' için profil adı gerekir: ad=<ad>. Tanımlı profilleri YAPAYZEKAMODELİ islem=listele ile görün.` | `ad` verilmedi | Profil adını verin |
| `'core.ai_provider': 'lehce' için tanınmayan değer 'X'. Kabul edilenler: openai_chat / openai_responses / anthropic_messages / ollama_native` | Lehçe dördünden biri değil | Dördünden birini yazın |
| `'anahtar_ref' bir API anahtarına benziyor. Buraya anahtarın kendisi değil, anahtar zincirindeki kaydın (ya da ortam değişkeninin) adı yazılır; anahtarı Seçenekler ▸ Yapay Zeka Modelleri sayfasından girersiniz.` | Anahtarın kendisi anahtar **adı** kutusuna yazıldı | Kayıt adını yazın; anahtarı ayar sayfasından girin |
| `'X' için adres http:// ya da https:// ile başlamalı; verilen 'Y'.` | Adres şemasız | Şemayı yazın |
| `'X' için uç nokta yolu '/' ile başlamalı; verilen 'Y'.` | `yol` eğik çizgisiz | `/` ile başlayan bir yol yazın |
| `'X' için model adı boş olamaz.` | `model=""` verildi | Model kimliğini yazın |
| `'X' için sıcaklık 0 ile 2 arasında olmalı.` | Sıcaklık aralık dışı | 0–2 arasında bir değer verin |
| `'core.ai_provider': 'baglam' 0 ile 100000000 arasında olmalı, N geldi.` | Bağlam penceresi aralık dışı | Aralıkta bir sayı verin |
| `Sağlayıcı adı boş olamaz.` | `ad=""` verildi | Bir ad yazın |
| `Böyle bir sağlayıcı yok: 'X'.` | `sil` / `varsayilan` bilinmeyen ad | `listele` ile bakın |
| `Böyle bir model sağlayıcısı yok: 'X'. Tanımlı olanları YAPAYZEKAMODELİ islem=listele ile görün.` | `dene` bilinmeyen ad | `listele` ile bakın |
| `Son sağlayıcı silinemez; önce başka bir sağlayıcı tanımlayın.` | Tek profil kalmış | Önce yeni profil ekleyin |
| `Proje gizli olarak işaretli: yalnızca yerel ya da kurum ağındaki bir model kullanılabilir. 'X' profili … adresine, yani kurum dışına çıkıyor.` | Proje hassas işaretli, profil ise internete çıkıyor | Yerel bir profil seçin ya da projenin hassasiyet ayarını değiştirin |
| `'X' profilinin anahtarı bulunamadı: 'Y' adıyla ne anahtar deposunda bir kayıt ne de böyle bir ortam değişkeni var. …` | `dene`, anahtar isteyen bir uç noktada anahtarı bulamadı | Anahtarı ayar sayfasından girin ya da `Y` ortam değişkenini ayarlayın |
| `Bu ortamda ağ taşıyıcısı bağlı değil, bu yüzden bağlantı denenemedi; komutu uygulama içinden çalıştırın. Profil olduğu gibi duruyor.` | `dene` ağ taşıyıcısı olmayan bir ortamda çalıştı | Uygulama içinden çalıştırın |
| `Model sağlayıcı deposu bu ortamda bağlı değil; komutu uygulama içinden çalıştırın.` | Komut, profil deposu bağlı olmayan bir ortamda çalıştı (başsız çalıştırma, betik koşucusu, test) | Uygulama içinden çalıştırın |
| `Model sağlayıcı dosyası yazılamadı: <yol>` | Ayar dizini yazılamıyor | Dizinin yazma iznini denetleyin |
| `Bu yapıda sistem anahtar deposu yok (KENTOS_WITH_KEYCHAIN kapalı), bu yüzden anahtar kaydedilemez. Anahtarı bir ortam değişkeninde tutun: … değişkenini ayarlayıp programı yeniden başlatın.` | Anahtar deposu olmayan bir yapıda anahtar kaydedilmek istendi | Ortam değişkeni yolunu kullanın |
| `Sağlayıcı dosyası bu sürümden yeni (dosya N, bu sürüm M). Programı güncelleyin; dosya olduğu gibi bırakıldı.` | `ai-modelleri.json` ileri bir sürümle yazılmış | Programı güncelleyin |

## İlgili

- [Model sağlayıcıları](../yapay-zeka/modeller.md) — lehçeler, anahtarın nerede durduğu, hassas proje
- [Yapay Zeka paneli](../yapay-zeka/sohbet.md) — varsayılan profilin kullanıldığı yer
- [`MCPSUNUCU`](mcp.md) — dışarıdan bağlanan ajan sunucusu
- [`TERCİH`](preference.md) — uygulama tercihleri
