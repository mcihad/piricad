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
- **Koordinat uyduramazsınız.** Nokta, nokta listesi ya da nesne seçimi isteyen bir
  parametre yalnız **tutamak** kabul eder: bir okuma aracının döndürdüğü
  `@0123456789abcdef.3` biçiminde bir dize. Sayı yazmak reddedilir — konum her zaman
  bir araç sonucundan gelir, modelin metninden değil.
- **Yazan araçlar çağrıldığında uygulamaz.** Belgeyi ya da diski değiştiren bir araç
  çağrısı bir ÖNERİ kaydı açar, uygulanacak komut satırlarını döndürür ve bilgisayar
  başındaki harita mühendisi uygulayana kadar bekler. Onaylanan bir öneri tek bir
  işlemdir ve tek `Ctrl+Z` ile geri alınır. Kadastro ve imar çıktısı hukuki belgedir;
  imzayı yapay zeka atamaz.
- **Hiçbir şeyi değiştirmeyen araçlar** (`no_effect`) doğrudan çalışır ve sonucunu
  döndürür: katman listesi, öznitelik şeması, sorgu, seçim, görünüm bilgisi.

## Sıra

1. `gorunum_bilgisi` — ekranda hangi alanı görüyorsunuz, hangi ölçekte, hangi CRS'te.
2. `katmanlari_listele` — katmanlar, geometri türleri, nesne sayıları.
3. `oznitelik_semasi` — bir katmanın öznitelik alanları ve tipleri.
4. `sorgula` — koşula uyan nesneler; sonuç sayısı sınırlıdır, tutamak döndürür.
5. `secimi_al` — kullanıcının o anki seçimi, tutamak olarak.
6. Bir yazma aracı — tutamaklarla; dönen öneriyi kullanıcı uygular.

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
            bool required          = false;
            if (const core::Json* list = tool.input_schema.find("required");
                list != nullptr && list->is_array()) {
                for (const core::Json& entry : list->as_array())
                    if (entry.is_string() && entry.as_string() == name) required = true;
            }
            out += "| `" + name + "` | " +
                   (type != nullptr && type->is_string() ? type->as_string() : "?") + " | " +
                   (required ? "evet" : "hayır") + " | " +
                   (help != nullptr && help->is_string() ? help->as_string() : "") + " |\n";
        }
        out += "\n";
    }
    return out;
}

} // namespace kentos::ai
