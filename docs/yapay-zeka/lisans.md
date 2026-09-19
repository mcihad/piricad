# Lisans ve Ağ Yükümlülüğü

KentOSCad'i değiştirip dağıtan, kurum içinde derleyen ya da MCP sunucusunu başkalarının
erişebileceği bir makinede çalıştıran herkes için; bu sayfayı bitirdiğinizde programın
iki lisansının hangi dosyaları kapsadığını, ağ yükümlülüğünün ne zaman doğduğunu ve
bunun sizi bağlayıp bağlamadığını bileceksiniz.

Bu sayfa bir hukuk görüşü değil, deponun kendi durumunun tarifidir. Bağlayıcı metinler
`LICENSE`, `LICENSES/AGPL-3.0-or-later.txt` ve `NOTICE` dosyalarıdır.

## İki lisans, bilinçli bir ayrım

KentOSCad'in tamamı **GPL-3.0-or-later** ile lisanslıdır — bir istisna dışında.

Program, yapay zeka ajanlarının çalışan bir oturuma bağlanabilmesi için bir **MCP
sunucusu gömer**. Bir sunucu bileşeni, projenin anayasasında **AGPL-3.0-or-later** ile
lisanslanır (`CLAUDE.md` Article 2.1), ve o sunucu **olan** dosyalar da öyle
lisanslanmıştır:

```text
src/ai/include/kentos_cad/ai/jsonrpc.hpp    src/ai/src/jsonrpc.cpp
src/ai/include/kentos_cad/ai/endpoint.hpp   src/ai/src/endpoint.cpp
src/ai/include/kentos_cad/ai/mcp.hpp        src/ai/src/mcp.cpp
src/app/include/kentos_cad/app/mcp_service.hpp
src/app/src/mcp_service.cpp
tests/unit/test_ai_mcp.cpp
```

Listenin güncel hâli ve her dosyanın SPDX satırı `NOTICE` dosyasındadır; bir CI kapısı
listeyle dosyaların başındaki satırların **birbirinden ayrılmamasını** denetler.

Deponun kalan her şeyi GPL-3.0-or-later'dır. İki lisans kendi 13. maddeleri
uyarınca **iki yönde de uyumludur**, dolayısıyla birleşik program dağıtılabilir.

## AGPL yarısı ne getirir

Pratik sonuç, AGPLv3'ün **ağ maddesidir**: programın değiştirilmiş bir sürümünü bir ağ
üzerinden kullanıcıların erişimine açıyorsanız, o kullanıcılara **kaynak kodunu sunmak**
zorundasınız.

Bu, bir sunucu bileşeninin taşıması amaçlanan yükümlülüğün ta kendisidir; kaza değil,
karardır.

## Sizi bağlar mı

| Durum | Ağ maddesi |
|---|---|
| Programı kendi makinenizde kullanıyorsunuz | Hayır. Kullanmak dağıtmak değildir |
| MCP sunucusunu açıyorsunuz, aynı makinedeki bir ajan bağlanıyor | Hayır. Sunucu yalnız yerel döngüyü dinler; başka kullanıcıya erişim açmış olmazsınız |
| Programı **değiştirmediniz**, olduğu gibi dağıtıyorsunuz | Kaynağı sunma yükümlülüğü zaten GPL/AGPL'in olağan dağıtım koşullarıdır; ağ maddesi için değiştirmiş olmanız gerekir |
| Programın **değiştirilmiş** bir sürümünü bir ağ üzerinden başkalarının kullanımına açtınız | **Evet.** O kullanıcılara kaynak kodunu sunmanız gerekir |

Sunucunun yalnız `127.0.0.1` ve `::1` dinlediğini burada tekrar etmek anlamlıdır: uç
noktayı kurum ağına açmak bir ayar değildir, sunucunun yapmadığı bir şeydir
([MCP sunucusu](mcp-sunucusu.md)). Böyle bir erişimi bir vekil sunucuyla kendiniz
kurarsanız, kurduğunuz şeyin lisans sonuçlarını da siz taşırsınız.

## Üçüncü taraf bileşenler

Programın bağladığı her kütüphane, tam sürümüyle ve okunmuş lisans metniyle `NOTICE`
dosyasında listelenir; aynı değişiklikte CycloneDX biçiminde bir **yazılım malzeme
listesi** (SBOM) da yeniden üretilir. Bu, GPL uyumluluğunun bir gereğidir.

Yapay zeka katmanının kendisi ek bir çalışma zamanı bağımlılığı getirmez: protokol
motoru, araç kataloğu ve sağlayıcı lehçeleri Qt'siz, ağsız ve dosyasız yazılmıştır;
soketi ve pencereleri uygulama tarafı sağlar.

## Model sağlayıcılarının lisansı ayrı bir konudur

Bir model sağlayıcısına bağlanmak, o sağlayıcının **kendi koşullarına** tabidir ve
KentOSCad'in lisansı onları değiştirmez. Kurum verisinin bir bulut sağlayıcısına
gitmesi çoğu zaman bir lisans sorusu değil, bir **veri koruma** sorusudur; programın
buna verdiği cevap yereldir ve hassasiyet işareti bunu zorlar
([Model sağlayıcıları](modeller.md)).

## İlgili

- [`NOTICE`](../../NOTICE) — iki lisansın dosya listesi ve bütün bağımlılıklar
- [`LICENSE`](../../LICENSE) — GPL-3.0-or-later metni
- [`LICENSES/AGPL-3.0-or-later.txt`](../../LICENSES/AGPL-3.0-or-later.txt) — AGPLv3 metni
- [MCP sunucusu](mcp-sunucusu.md) — sunucunun ne dinlediği, ne dinlemediği
- [Sürüm ve uyumluluk politikası](../api-stability.md) — neyin sabit kaldığı
