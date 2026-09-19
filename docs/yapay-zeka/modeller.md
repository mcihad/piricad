# Model Sağlayıcıları

Hangi yapay zeka modelinin, hangi makinede çalışacağına karar verecek harita mühendisi
ve kurum bilgi işlemcisi için; bu sayfayı bitirdiğinizde bir sağlayıcı tanımlamayı, API
anahtarının nerede durduğunu, yerel modelin neden varsayılan olduğunu ve hassas bir
projede bulutun neden kapandığını bileceksiniz.

> **Faz notu.** Sağlayıcı profilleri, dört lehçe, hassasiyet denetimi, anahtar
> başvurusu, profilleri yöneten [`YAPAYZEKAMODELİ`](../komutlar/ai_provider.md) komutu,
> `Seçenekler ▸ Yapay Zeka Modelleri` **ayar sayfası** ve anahtarları işletim
> sisteminin anahtar deposuna yazan bağlantı bu sürümde çalışır. Bu sayfada gelecek
> zamanla yazılmış bir cümle kalmadıysa sebebi budur.

## Bir sağlayıcı nedir

**Sağlayıcı profili**, bir modele nasıl ulaşılacağını anlatan adlandırılmış bir kayıttır.
Bir programın koduna gömülmüş bir model listesi yoktur: yeni bir sağlayıcı eklemek
normalde bir **kayıt** eklemektir, bir sürüm çıkarmak değil.

Bir profil şunları taşır:

| Alan | Ne söyler |
|---|---|
| **Ad** | Profili sizin çağırdığınız ad; Türkçe katlamayla tekildir |
| **Lehçe** | Uç noktanın konuştuğu telli dil: aşağıdaki dört taneden biri |
| **Adres** | Şema, makine, port ve satıcının ön eki: `https://api.openai.com/v1`, `http://localhost:11434` |
| **Yol** | Adresin altındaki uç nokta: `/chat/completions`, `/messages`, `/api/chat` |
| **Model** | Model kimliği, uç noktanın yazdığı gibi |
| **Kimlik başlığı** | Anahtarı taşıyan başlık: çoğu yerde `Authorization`, Anthropic'te `x-api-key` |
| **Ek başlıklar** | Satıcının istediği başka başlıklar, örneğin `anthropic-version` |
| **Ek gövde** | İstek gövdesine eklenen, satıcıya özel ayarlar |
| **Akış** | Cevabın parça parça mı isteneceği |
| **Düşünme** | Akıl yürütme ayarı; aşağıda kendi başlığı var |
| **Çıktı sınırı** | En çok kaç jeton üretilsin; `0` demek "bu alanı hiç gönderme" |
| **Sıcaklık** | Verilmediğinde hiç gönderilmez — bazı modeller varsayılandan başka değer kabul etmez |
| **Bağlam penceresi** | Modelin ne kadar tutabildiği ve bu sayıyı kimin söylediği |
| **Araçlar** | Uç noktaya araç kataloğu gönderilsin mi |
| **Anahtar adı** | Anahtar zincirindeki **kaydın adı** — anahtarın kendisi değil |

Profillerden **biri varsayılandır**.

## Dört lehçe

On üçten fazla satıcı incelendiğinde ortaya dört gerçekten farklı telli dil çıktı;
gerisi aynı dili başka bir adreste konuşuyor. Bu yüzden lehçe **koddur**, sağlayıcı
**veridir**.

| Lehçe | Uç nokta | Kim konuşur |
|---|---|---|
| `openai_chat` | `{adres}/chat/completions` | OpenAI, DeepSeek, Qwen, Gemini'nin uyumlu ucu, Mistral, Groq, OpenRouter, xAI, vLLM, llama.cpp, LM Studio |
| `openai_responses` | `{adres}/responses` | OpenAI'ın Responses ucu |
| `anthropic_messages` | `{adres}/messages` | Anthropic |
| `ollama_native` | `{adres}/api/chat` | Ollama'nın kendi ucu |

Yeni bir satıcı bu dörtten birini konuşuyorsa, onu eklemek bir profil yazmaktır.

## Sağlayıcı kataloğu: liste programda değil, veride

Programın konuşmayı bildiği uç noktalar **`data/catalogs/ai/saglayicilar.json`**
dosyasındadır: 41 sağlayıcı ve 124 model kimliği, her biri adresi, kimlik başlığı,
lehçesi ve bağlam penceresiyle. Kaynak, her kaydın `belge` alanındaki resmî API
başvurusudur.

Bu bir tercih değil, bir zorunluluktur: **bir model kimliği haftalar içinde eskir.**
Bir sağlayıcı modelini yeniden adlandırdığında ya da adresine bir sürüm öneki
eklediğinde, bu dosyanın bir satırı düzeltilir — programın yeniden derlenmesi
gerekmez. Dosya bir **veri paketidir** ve şeması `data/catalogs/schema/` altındadır.

Katalog üç şeyi birden besler ve bu yüzden tek liste vardır:

1. **Yeni kurulumun başlangıç profilleri** — `baslangic` işaretli kayıtlar.
2. **Profil penceresindeki şablon listesi** — kataloğun tamamı.
3. **Model açılır listesi** — seçtiğiniz uç noktanın kendi modelleri.

Ve katalog **son söz değildir**: `Modelleri getir` düğmesi uç noktaya modellerini
sorar ve gelen cevap listenin yerine geçer. Bir anahtarın gerçekten erişebildiği
modeller ancak uç noktanın kendisinden öğrenilir; katalog, kimse sormadan önce
bilinendir.

## Yeni kurulumda gelen profiller

Katalogdaki 41 sağlayıcıdan **8 tanesi** yeni bir kurulumda profil olarak gelir.
Gerisi bir tık uzaktadır: **Yeni profil…** penceresindeki şablon listesinde
hepsi vardır. Kırk profille açılan bir seçici, kimsenin kullanamayacağı bir
seçicidir.

Hiçbiri anahtar taşımaz; bulut profilleri yalnız **anahtar adını** taşır ve o ad
karşılığı boş olduğu sürece profil kullanılamaz.

| Profil | Lehçe | Adres | Model |
|---|---|---|---|
| **Ollama** (varsayılan) | `ollama_native` | `http://localhost:11434` | `llama3.1` |
| **OpenAI (Responses)** | `openai_responses` | `https://api.openai.com/v1` | `gpt-5` |
| **Anthropic** | `anthropic_messages` | `https://api.anthropic.com/v1` | `claude-sonnet-4-5` |
| **Google Gemini (OpenAI uyumlu)** | `openai_chat` | `https://generativelanguage.googleapis.com/v1beta/openai` | `gemini-2.5-pro` |
| **DeepSeek** | `openai_chat` | `https://api.deepseek.com` | `deepseek-chat` |
| **Mistral AI** | `openai_chat` | `https://api.mistral.ai/v1` | `mistral-large-latest` |
| **Groq** | `openai_chat` | `https://api.groq.com/openai/v1` | `llama-3.3-70b-versatile` |
| **OpenRouter** | `openai_chat` | `https://openrouter.ai/api/v1` | `openai/gpt-5` |

Şablon listesindeki diğerleri: Azure OpenAI, Z.ai (GLM), MiniMax, Moonshot (Kimi),
Volcengine Ark (Doubao), StepFun, Cohere, AI21, Cerebras, SambaNova, Together AI,
Fireworks, DeepInfra, Nebius, Novita, Hyperbolic, Perplexity, NVIDIA NIM,
GitHub Models, Cloudflare Workers AI, Hugging Face Inference, SGLang, TGI,
LocalAI, Jan ve LiteLLM ağ geçidi.

**vLLM'in model adı yer tutucudur**: vLLM sunulan adı harfi harfine arar ve başkasını
kabul etmez, dolayısıyla sunucunun `--model` ile açıldığı adı yazmanız gerekir.

## Varsayılan yereldir, ve sebebi ofisin kendisidir

Yeni bir kurulumun varsayılan sağlayıcısı **Ollama**'dır: kendi makinenizde çalışan
yerel bir model. Bu bir tercih değil, bu programın kuralıdır — yerel model desteği
isteğe bağlı değildir.

Sebebi şu: kadastro ve imar verisi çoğu zaman kurumdan **çıkamaz**. Bir belediyenin
parsel sınırlarını, malik adlarını ya da bir imar dosyasının eklerini başka birinin
bilgisayarına göndermek, çoğu kurumda bir teknik ayar değil, bir izin sorunudur. Kredi
kartı isteyen bir varsayılan, "yerel destek" değildir.

## Anahtar nerede durur

Profil **anahtarı taşımaz**. Taşıdığı şey, işletim sisteminin anahtar deposundaki
**kaydın adıdır**; anahtarın kendisi orada durur ve ayar dosyasına hiç yazılmaz.

Aynı kural kayıtların her biri için geçerlidir: bir API anahtarı ne ayar dosyasına, ne
komut günlüğüne, ne denetim kaydına, ne de bir hata satırına yazılır. Bu yüzden
anahtar, profilleri yöneten [`YAPAYZEKAMODELİ`](../komutlar/ai_provider.md) komutunun
**bir parametresi değildir**: bir komutun argümanları günlüğe yazılır, dolayısıyla
anahtarı alabilen bir parametre anahtarı günlüğe yazan bir parametre olurdu.

**Anahtarı girdiğiniz tek yer** `Seçenekler ▸ Yapay Zeka Modelleri` sayfasının
altındaki **API anahtarı** alanıdır. Ekranda noktalarla görünür, yazıldığı anda
alandan silinir ve doğrudan işletim sisteminin deposuna gider; alanın yanındaki not
hangi kayda yazıldığını ve bu yapıda hangi deponun kullanıldığını söyler.

### İki yol, ikisi de işletim sisteminin

| Yol | Nerede durur | Ne zaman |
|---|---|---|
| **Sistem anahtar deposu** | macOS Anahtar Zinciri, Windows kimlik deposu, Linux'ta Secret Service (libsecret) | Ayar sayfasından girdiğiniz anahtarlar |
| **Ortam değişkeni** | Kabuğunuzda, kurumun hizmet tanımında ya da kapsayıcının ortamında | Anahtar adı bir değişkenin adı olduğunda |

Anahtar aranırken önce **anahtar deposu**, sonra anahtar adının kendisiyle **aynı
addaki ortam değişkeni**, sonra `<ANAHTAR_ADI>_API_KEY` biçimindeki alışılmış
değişken okunur. Yani yeni kurulumda gelen `deepseek` adlı kayıt,
`DEEPSEEK_API_KEY` değişkenini de bulur — satıcının kendi belgesinin söylediği
değişkeni.

Ortam değişkeni yolu **her yapıda** açıktır ve bu, anahtar deposu olmayan bir makinenin
(başsız sunucu, kapsayıcı, Secret Service çalışmayan bir Linux) doğru cevabıdır:
değişken **ilk kullanımda** okunur ve program onu hiçbir yere yazmaz. Aynı biçimi
PostGIS yolu `~/.pgpass` ile zaten kullanır. Böyle bir yapıda anahtar **kaydedilemez**
ve program bunu söyler:

```text
Bu yapıda sistem anahtar deposu yok (KENTOS_WITH_KEYCHAIN kapalı), bu yüzden anahtar
kaydedilemez. Anahtarı bir ortam değişkeninde tutun: deepseek ya da DEEPSEEK_API_KEY
değişkenini ayarlayıp programı yeniden başlatın.
```

Bir profili sildiğinizde anahtar deposundaki kayıt **silinmez**: aynı anahtarı başka
bir profil de kullanıyor olabilir, ve kimsenin istemediği bir silme, silmenin en kötü
türüdür.

Bir anahtarı yanlışlıkla anahtar **adı** kutusuna yapıştırırsanız profil reddedilir:

```text
'OpenAI (Sohbet)' için anahtar adı bir API anahtarına benziyor. Buraya anahtarın
kendisi değil, anahtar zincirindeki kaydın adı yazılır.
```

Yerel bir sağlayıcının anahtarı yoktur ve anahtar adı boş kalır; bu doğru ve
yaygın durumdur.

### Anahtar ne zaman okunur

Anahtar deposuna sorulan her soru, işletim sisteminin **izin penceresini** açabilir —
macOS Anahtar Zinciri'nde en görünür hâliyle. Bu yüzden soru, **sizin başlattığınız bir
anda** sorulur: sohbet panelinin üstündeki **model seçicisinden** bir profil
seçtiğinizde ya da [`YAPAYZEKAMODELİ islem=dene`](../komutlar/ai_provider.md) ile
bağlantıyı denediğinizde. Cevap **o oturum boyunca bellekte** tutulur, dolayısıyla aynı
profille yazdığınız ikinci ileti anahtar deposuna hiç uğramaz ve izin penceresi bir daha
çıkmaz.

Ayar sayfasından bir anahtar kaydettiğinizde hiç sorulmaz: yazdığınız değer zaten
bilinmektedir, programın onu işletim sisteminden geri istemesi gereksizdir.

Bellekte tutulan bu kopya **hiçbir yere yazılmaz** — ne ayar dosyasına, ne günlüğe, ne
denetim kaydına — ve program kapanınca gider. Bunun tek görünür sonucu şudur: bir
anahtarı program çalışırken **kabuktan** değiştirirseniz (ortam değişkeni yolu), yeni
değer o oturumda değil, programı yeniden başlattığınızda geçerli olur. Ayar
sayfasından kaydedilen anahtar ise **hemen** geçerlidir.

Anahtar hiçbir zaman arayüzün beklediği bir işlem değildir: soru sorulurken pencere
çalışmaya devam eder ve bekleyen bir sohbet turu her an **durdurulabilir**.

## Sayfanın üç ayarı

Profillerin tablosunun yanında `Seçenekler ▸ Yapay Zeka Modelleri` sayfası üç ayar
taşır. Hepsi komut satırından da verilir: proje kapsamındaki [`AYAR`](../komutlar/setting.md)
ile, uygulama kapsamındakiler [`TERCİH`](../komutlar/preference.md) ile.

| Ayar | Adı | Kapsam | Ne yapar |
|---|---|---|---|
| `core.ai.hassas` | `hassas_proje` | Proje | Bu çizimin verisi kurum dışına çıkamaz: yalnız yerel ya da kurum içi sağlayıcılar kullanılabilir ve MCP sunucusu başlatılmaz. Verinin kendi niteliği olduğu için dosyayla birlikte taşınır |
| `core.ai.dusunme_goster` | `düşünmeyi_göster` | Uygulama | Modelin düşünme metni sohbette katlanabilir bir blokta gösterilir. Kapalıyken yalnız düşündüğünü söyleyen bir gösterge çıkar |
| `core.ai.sorumlu` | `sorumlu` | Uygulama | **Öneriyi onaylayan kişinin adı.** Her onay ve her ret denetim kaydına bu adla yazılır; boş bırakılırsa işletim sisteminin kullanıcı adı kullanılır ve kayıt bunu böyle işaretler |

`core.ai.sorumlu` uygulama kapsamındadır ve bu kasıtlıdır: bir pafta başka bir ofise
gittiğinde onu orada kim onaylarsa, o makinenin kaydına o ad girer. Kadastro ve imar
çıktısından sorumlu mühendisin adını buraya yazın — boş bırakılan bir ad dürüsttür ama
imza değildir ([Onay ve denetim](onay.md)).

## Hassas proje: bulut kapanır

Bir proje **hassas** olarak işaretlendiğinde, istek yalnız **yerel döngüye** ya da
**kurumun kendi ağına** gidebilir. Bu bir onay kutusu denetimi değil: izin, programın
tip sisteminde verilir, dolayısıyla denetimi atlayan bir kod yolu derlenmez.

Nereye gittiği, profilin **adresindeki makineden** okunur — profilin kendisi hakkındaki
iddiasından değil. Yani projeyi hassas işaretleyip "Yerel" adlı bir profili
`api.openai.com` adresine yönlendirmek **reddedilir**; bir arayüz denetimi bunu geçirirdi.

| Erişim | Neresi |
|---|---|
| Yerel döngü | `localhost`, `127.0.0.0/8`, `::1` — kullanıcının önündeki makine |
| Kurum ağı | RFC 1918 / RFC 4193 adresleri, `.local`, çıplak makine adı |
| İnternet | Geri kalan her şey: başkasının bilgisayarı |

Çözülemeyen bir adres **internet** sayılır: "bilemiyorum" sorusunun güvenli cevabı
"dışarı çıkıyor"dur.

Hassasiyet işaretliyken **MCP dinleyicisi de açılmaz** ve nedenini söyler.

## Düşünme (akıl yürütme) ayarı

Dört model ailesi akıl yürütmeyi aynı biçimde istemez; hatta aynı şeyi istemez. Bu
yüzden ayar bir açma–kapama düğmesi değildir:

| Kip | Ne gönderilir | Kim ister |
|---|---|---|
| yok | Hiçbir şey; model kendi karar verir | Akıl yürütme ayarı olmayan uçlar |
| çaba | `minimal` / `low` / `medium` / `high` bir sözcük | OpenAI sohbet ucu, Groq, Gemini |
| özet | Çaba **ve** özetin ayrıntı düzeyi | OpenAI Responses ucu |
| bütçe | Düşünmeye ayrılan jeton bütçesi | Anthropic |
| Qwen bütçesi | Bir anahtar ve bir bütçe | Qwen |

Akıl yürütme metninin **size gösterilip gösterilmeyeceği** ayrı bir sorudur ve profilde
ayrı bir alandır: gösterilmese de denetim kaydına yazılabilir.

## Bağlam penceresi kimin sözü

Bir modelin ne kadar tutabildiği programa gömülemez: OpenAI'ın model listesi bir
kimlik, bir sahip ve bir tarih döndürür, **bağlam uzunluğu döndürmez**. Ollama'nın
kendi ucu döndürür. Bu yüzden sayı, **kimin söylediğini** belirten bir işaretle
birlikte durur:

| Kaynak | Anlamı |
|---|---|
| bilinmiyor | Kimse söylemedi; yalnız yerel tahmin var |
| yerleşik | Programın bilinen bir model için getirdiği başlangıç değeri |
| kullanıcı | Profile elle yazıldı |
| bildirilen | Uç noktanın kendi API'si söyledi |

Panel bunu gizlemez, çünkü "sunucu söyledi" ile "birisi yazdı" aynı şey değildir.

## Profillerin kuralları

Bir profil kaydedilirken şunlar denetlenir ve tutmayan profil **kaydedilmez**:

- Ad boş olamaz.
- Adres `http://` ya da `https://` ile başlamalı.
- Uç nokta yolu `/` ile başlamalı.
- Model adı boş olamaz.
- Çıktı sınırı, düşünme bütçesi ve bağlam penceresi negatif olamaz.
- Sıcaklık 0 ile 2 arasında olmalı.
- Anahtar adı bir API anahtarına benzememeli.

**Son profil silinemez**: sağlayıcısı olmayan bir sohbet, kendi içinden yeniden
yapılandırılamaz. Varsayılan silindiğinde varsayılanlık başka bir profile geçer.

Profiller **kullanıcı profilinizde** bir JSON dosyasında tutulur, sürüm alanıyla
birlikte; çizimle gitmezler ve çizime yazılmazlar. Dosyayı ileri bir sürüm yazmışsa
program onu kısmen okumaya çalışmaz, hangi sürümün gerektiğini söyler.

## İlgili

- [`YAPAYZEKAMODELİ`](../komutlar/ai_provider.md) — profilleri komut satırından ve betikten yönetme
- [Yapay Zeka paneli](sohbet.md) — sohbetin kendisi, bağlam ölçeri ve araç döngüsü
- [MCP sunucusu](mcp-sunucusu.md) — bir dış ajanı bağlama
- [Onay ve denetim](onay.md) — modelin çıktısının çizime nasıl (ve nasıl değil) girdiği
- [Yapay zeka ve ajanlar](README.md) — dört kural
