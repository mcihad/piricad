// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/llmstxt.hpp"

#include "kentos_cad/ai/catalog.hpp"

#include <string>

namespace kentos::ai {
namespace {

/// The part that is prose rather than data: the rules of the house.
///
/// TURKISH, because every command, parameter and value in this program is
/// Turkish and a model that reads the rules in English still has to write the
/// Turkish verb. The audience is a machine, but the vocabulary is the product's
/// (CLAUDE.md 2.6).
std::string preamble()
{
    return R"(# KentOSCad

> Türkiye odaklı CBS + CAD masaüstü programı: jeodezi, kadastro, imar ve yüzey işleri.
> Programın durumunu değiştiren her şey bir KOMUTtur; arayüz, komut satırı, betik ve
> yapay zeka aynı komut veri yolunun eşit istemcileridir. Bu dosya bir ajanın programı
> kullanmadan önce bilmesi gerekenleri anlatır; araçların tam şeması `tools/list` ile
> alınır ve tek kaynağı komut kataloğudur.

## Önce bunlar

- **Birim milimetredir ve tam sayıdır.** Bütün koordinatlar `int64` sabit noktalı
  milimetre (`Mm`). Ondalık yoktur; 485320.15 metre `485320150` demektir. Alan
  milimetrekaredir.
- **Eksen adları:** `Sağa (Y)` doğuya doğru (easting), `Yukarı (X)` kuzeye doğru
  (northing). Bir nokta **doğu önce** yazılır: `[485320150, 4310220000]`.
- **Türkçe adlar katlanır.** `GİZLE`, `gizle`, `GIZLE` ve `gızle` aynı sözcüktür; i/ı
  ve büyük/küçük ayrımı ad çözümlemede yok sayılır. Her komutun Türkçe birincil adı,
  ASCII karşılığı, İngilizce karşılığı ve kısaltması vardır.
- **Koordinat uyduramazsınız, ama çizebilirsiniz.** Nokta ya da nokta listesi isteyen
  bir parametreye sayı yazmak reddedilir; konum her zaman bir araç sonucundan gelir.
  Bir KONUM iki biçimde yazılır:
  - bir **tutamak**: bir okuma aracının döndürdüğü `@0123456789abcdef` ya da listenin bir
    elemanı için `@0123456789abcdef.3`;
  - bir tutamaktan **ölçüyle** uzaklaşan **göreli nokta**:
    `{"taban": "@0123456789abcdef.0", "dogu": 10000, "kuzey": -5000}` — tabanın 10 m
    doğusu, 5 m güneyi. `dogu` ve `kuzey` her zaman tam sayı milimetredir; batı ve güney
    eksidir.
  Nokta listesi (bir çokgenin köşeleri) ya tek bir tutamaktır ya da her elemanı bir
  tutamak veya göreli nokta olan bir dizidir. Uzunluk, yarıçap, mesafe gibi ÖLÇÜLER
  düz sayıdır ve **birimi parametrenin açıklamasında yazar**: `(m)` metre demektir
  (`yaricap`, `kenar_uzunlugu` metredir), `milimetre` milimetre (OFSET'in `mesafe`si).
  Birimi okumadan sayı yazmayın; 10 metre yarıçap `"yaricap": 10`dur.
- **Konum nereden gelir.** `gorunum_bilgisi` ekranın ortasını ("görünümün ortası") bir
  nokta tutamağı olarak verir; kullanıcı bir yer söylemediyse yeni çizim oraya yapılır.
  `nesne_noktalari` bir nesnenin merkezini, köşelerini, uçlarını, kutusunu ya da kenar
  ortalarını verir; nesnenin kendisi `sorgula` ya da `secimi_al` tutamağıyla seçilir.
  Tutamağın koordinatlarını görmezsiniz; `ad` ve `noktalar` alanları hangisinin ne
  olduğunu söyler. Çizim her değiştiğinde tutamaklar eskir; okuma aracını yeniden
  çağırın.
- **Örnek: ekranın ortasına 20 m kenarlı kare.** `gorunum_bilgisi` → dönen nokta
  tutamağı `@m` ise `core_area` şu argümanla:
  `{"noktalar": [{"taban": "@m", "dogu": -10000, "kuzey": -10000},
  {"taban": "@m", "dogu": 10000, "kuzey": -10000}, {"taban": "@m", "dogu": 10000,
  "kuzey": 10000}, {"taban": "@m", "dogu": -10000, "kuzey": 10000}]}`.
- **Örnek: 10 m yarıçaplı altıgen.** `core_polygon_regular` şu argümanla:
  `{"merkez": {"taban": "@m"}, "kenar_sayisi": 6, "yaricap": 10}`. 3 m yarıçaplı daire:
  `core_circle_draw` `{"merkez": "@m", "cevre": {"taban": "@m", "dogu": 3000}}`. Bir nesneyi 5 m
  doğuya taşımak: `core_move` `{"nesneler": "@n", "baslangic": {"taban": "@m"},
  "bitis": {"taban": "@m", "dogu": 5000}}`.
- **Yazan araçlar çağrıldığında uygulamaz.** Belgeyi ya da diski değiştiren bir araç
  çağrısı bir ÖNERİ kaydı açar, uygulanacak komut satırlarını döndürür ve bilgisayar
  başındaki harita mühendisi uygulayana kadar bekler. Onaylanan bir öneri tek bir
  işlemdir ve tek `Ctrl+Z` ile geri alınır. Kadastro ve imar çıktısı hukuki belgedir;
  imzayı yapay zeka atamaz.
- **Hiçbir şeyi değiştirmeyen araçlar** (`no_effect`) doğrudan çalışır ve sonucunu
  döndürür: katman listesi, öznitelik şeması, sorgu, seçim, görünüm bilgisi.

## Sıra

1. `gorunum_bilgisi` — ekranda hangi alanı görüyorsunuz, hangi ölçekte, hangi CRS'te;
   ekranın ortası bir nokta tutamağı olarak gelir.
2. `katmanlari_listele` — katmanlar, geometri türleri, nesne sayıları.
3. `oznitelik_semasi` — bir katmanın öznitelik alanları ve tipleri.
4. `sorgula` — koşula uyan nesneler; sonuç sayısı sınırlıdır, nesne tutamağı döndürür.
5. `secimi_al` — kullanıcının o anki seçimi, nesne tutamağı olarak.
6. `nesne_noktalari` — nesnelerin merkez, köşe, uç, kutu ya da kenar ortası noktaları,
   nokta tutamağı olarak.
7. Bir yazma aracı — tutamaklar ve göreli noktalarla; dönen öneriyi kullanıcı uygular.

)";
}

} // namespace

std::string agent_preamble()
{
    return preamble();
}

std::string llms_txt(const command::Registry& registry)
{
    const Catalog catalog = build_catalog(registry);

    std::string out = kGeneratedNotice;
    out += "\n\n";
    out += preamble();

    out += "## Araç yüzeyi\n\n";
    out += "- Araç sayısı: " + std::to_string(catalog.tools.size()) + " (";
    out += std::to_string(catalog.mutating_count()) + " tanesi onay ister)\n";
    out += "- Katalog parmak izi: `" + std::to_string(catalog.fingerprint) + "`\n";
    out += "- MCP sürümü: `" + std::string(Catalog::kProtocolVersion) + "`\n";
    out += "- Tam şema: `tools/list`; bu dosyanın uzun hâli: `llms-full.txt`\n\n";

    out += "## Kılavuz\n\n";
    out += "- [Komut referansı](komutlar/referans.md): her komut, adları, parametreleri\n";
    out += "- [Nesne türleri](nesneler/referans.md): çizimdeki nesne türleri\n";
    out += "- [Sözlük](sozluk.md): ifraz, tevhit, irtifak ve diğer alan terimleri\n";
    out += "- [Arayüz](baslangic/arayuz.md): pencerede ne nerede\n";
    out += "- [Koordinat sistemleri](veri/koordinat-sistemleri.md): TUREF/TM3, dönüşümler\n";
    return out;
}

std::string llms_full_txt(const command::Registry& registry)
{
    const Catalog catalog = build_catalog(registry);

    std::string out = llms_txt(registry);
    out += "\n## Araçlar\n\n";

    for (const ToolDef& tool : catalog.tools) {
        out += "### `" + tool.name + "` — " + tool.title + "\n\n";
        out += tool.description + "\n\n";
        out += tool.mutates ? "Onay ister.\n\n" : "Doğrudan çalışır.\n\n";

        const core::Json* properties = tool.input_schema.find("properties");
        if (properties == nullptr || !properties->is_object()) {
            out += "Parametresi yok.\n\n";
            continue;
        }
        out += "| Parametre | Tür | Zorunlu | Açıklama |\n|---|---|---|---|\n";
        for (const auto& [name, schema] : properties->as_object()) {
            const core::Json* type = schema.find("type");
            const core::Json* help = schema.find("description");
            // A POSITION IS ONE OF TWO SHAPES (`anyOf`), which has no single
            // `type`; the table names what it is instead of printing `?`.
            std::string kind = type != nullptr && type->is_string() ? type->as_string() : "?";
            if (type == nullptr && schema.find("anyOf") != nullptr)
                kind = name == "noktalar" || (help != nullptr && help->is_string() &&
                                              help->as_string().starts_with("nokta listesi"))
                           ? "nokta listesi (tutamak ya da konum dizisi)"
                           : "konum (tutamak ya da göreli nokta)";
            bool required = false;
            if (const core::Json* list = tool.input_schema.find("required");
                list != nullptr && list->is_array()) {
                for (const core::Json& entry : list->as_array())
                    if (entry.is_string() && entry.as_string() == name) required = true;
            }
            out += "| `" + name + "` | " + kind + " | " + (required ? "evet" : "hayır") + " | " +
                   (help != nullptr && help->is_string() ? help->as_string() : "") + " |\n";
        }
        out += "\n";
    }
    return out;
}

} // namespace kentos::ai
