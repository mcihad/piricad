// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/script/python_doc.hpp"

#include "kentos_cad/command/registry.hpp"

#include <string>
#include <vector>

namespace kentos::script {
namespace {

using namespace kentos::command;

/// What a parameter looks like from Python.
///
/// THE UNITS ARE THE POINT OF THIS TABLE. A `Point` is two `Mm` integers and a
/// `Number` is metres, and a reader who guesses wrong puts the parcel a thousand
/// times too far away. `Mm` is what the document stores (Article 2.4), so it is
/// what a generated call takes — the metre spelling belongs to `cad.run`, which
/// is the command line and reads the command line's grammar.
std::string type_of(const Param& p)
{
    const bool many = p.arity.max > 1;
    switch (p.kind) {
    case ParamKind::Point: return many ? "list[list[int]]" : "list[int]";
    case ParamKind::PointList: return "list[list[int]]";
    case ParamKind::Number: return many ? "list[float]" : "float";
    case ParamKind::Integer: return many ? "list[int]" : "int";
    case ParamKind::Text: return many ? "list[str]" : "str";
    case ParamKind::Bool: return "bool";
    case ParamKind::Selection: return "list[int]";
    }
    return "object";
}

/// The Turkish note that says what the numbers are measured in.
std::string unit_note(const Param& p)
{
    if (!p.unit.empty()) return " [" + p.unit + "]";
    switch (p.kind) {
    case ParamKind::Point:
    case ParamKind::PointList: return " [mm, Sağa (Y) önce]";
    case ParamKind::Selection: return " [kalıcı nesne anahtarı]";
    default: return {};
    }
}

bool exported(const CommandSpec& spec)
{
    return spec.run != nullptr && has_flag(spec.flags, Flags::Scriptable);
}

/// The commands, in the registry's own order.
std::vector<const CommandSpec*> surface(const Registry& reg)
{
    std::vector<const CommandSpec*> out;
    for (const CommandSpec& spec : reg.all())
        if (exported(spec)) out.push_back(&spec);
    return out;
}

const char* kDoNotEdit =
    "<!-- ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN. -->\n"
    "<!-- Kaynak: kentos::command::Registry.  Yeniden üret: make reference -->\n"
    "<!-- Python yüzeyi de aynı kayıttan üretilir; elle yazılan ikinci bir -->\n"
    "<!-- bağlama listesi yoktur (CLAUDE.md 5.10, 5.20). -->\n";

} // namespace

std::string python_reference(const Registry& reg)
{
    const std::vector<const CommandSpec*> cmds = surface(reg);

    std::string out = kDoNotEdit;
    out += "\n# Python API Referansı\n\n";
    out += "Bu sayfa komut kaydından üretilir. Programa bir komut eklendiğinde Python\n";
    out += "fonksiyonu ve bu satır kendiliğinden gelir.\n\n";
    out += "Nasıl kullanıldığı: [Python betikleri](../betik/python.md).\n\n";

    out += "## Kurallar\n\n";
    out += "- Her çağrı **yalnız anahtar kelime** alır; konumsal argüman yoktur.\n";
    out += "- Anahtar kelimeler **İngilizcedir**. Komutun kendi adı Türkçe kalır.\n";
    out += "- Koordinatlar **milimetre tam sayıdır** ve `[Sağa, Yukarı]` sırasındadır.\n";
    out += "  Metre yazmak isterseniz `cad.run(\"ÇİZGİ 0,0 10,10\")` komut satırının\n";
    out += "  dilbilgisini kullanır.\n";
    out += "- Dönen değer, komutun kaç ilkel düzenleme yaptığıdır.\n";
    out += "- Bir hata istisna yükseltir; yakalamazsanız betiğin tamamı geri alınır.\n\n";

    out += "## Çizimden okuma\n\n";
    out += "Bunlar komut değildir, çizimi değiştirmezler ve `cad.doc` altındadır.\n\n";
    out += "| Çağrı | Döndürdüğü |\n|---|---|\n";
    out += "| `cad.doc.layers()` | `list[str]` — katman adları |\n";
    out += "| `cad.doc.layer_count()` | `int` |\n";
    out += "| `cad.doc.active_layer()` | `str` |\n";
    out += "| `cad.doc.entity_count()` | `int` |\n";
    out += "| `cad.doc.selection_count()` | `int` |\n";
    out += "| `cad.doc.crs()` | `str` — örneğin `TUREF/TM30` |\n";
    out += "| `cad.doc.setting(id)` | `bool` \\| `int` \\| `str` |\n";
    out += "| `cad.run(satır)` | `int` — komut satırını çalıştırır |\n";
    out += "| `cad.sandbox()` | `str` — `güvenli`, `proje` ya da `tam` |\n";
    out += "| `cad.read_file(path)` | `str` — kum havuzu izin verirse |\n";
    out += "| `cad.write_file(path, text)` | — kum havuzu izin verirse |\n\n";

    out += "## Komutlar\n\n";
    out += "| Fonksiyon | Komut | Adı | Ne yapar |\n|---|---|---|---|\n";
    for (const CommandSpec* spec : cmds) {
        out += "| [`cad." + python_callable_name(*spec) + "`](#cad" + python_callable_name(*spec) +
               ") | `" + spec->id + "` | `" +
               (spec->names.empty() ? std::string("—") : spec->names.front()) + "` | " +
               spec->summary + " |\n";
    }

    out += "\n---\n";
    for (const CommandSpec* spec : cmds) {
        const std::string fn = python_callable_name(*spec);

        out += "\n### `cad." + fn + "`\n\n";
        out += spec->summary + "\n\n";
        out += "Komut: `" + spec->id + "`";
        if (!spec->names.empty()) out += " — `" + spec->names.front() + "`";
        out += "\n\n```python\ncad." + fn + "(";
        for (std::size_t i = 0; i < spec->params.size(); ++i) {
            out += "\n    " + spec->params[i].english + ": " + type_of(spec->params[i]);
            out += (i + 1 == spec->params.size()) ? "," : ",";
        }
        if (!spec->params.empty()) out += "\n";
        out += ") -> int\n```\n";

        if (!spec->params.empty()) {
            out += "\n| Anahtar | Tür | Türkçe adı | Açıklama |\n|---|---|---|---|\n";
            for (const Param& p : spec->params) {
                out += "| `" + p.english + "` | `" + type_of(p) + "` | `" + p.name + "` | " +
                       p.help + unit_note(p) + " |\n";
            }
        }
        out +=
            "\n[Komut sayfası](../komutlar/" + spec->id.substr(spec->id.rfind('.') + 1) + ".md)\n";
    }
    return out;
}

std::string python_stub(const Registry& reg)
{
    std::string out = "# SPDX-License-Identifier: GPL-3.0-or-later\n";
    out += "# ÜRETİLMİŞ DOSYA — ELLE DÜZENLEMEYİN.  Yeniden üret: make reference\n";
    out += "#\n";
    out += "# `kentos.cad`'in tip taslağı. Program bu dosyayı ÇALIŞTIRMAZ: gerçek yüzey\n";
    out += "# çalışma anında komut kaydından kurulur. Bu taslak, betiği programın\n";
    out += "# dışında yazan bir düzenleyicinin tamamlama ve tip denetimi yapabilmesi\n";
    out += "# içindir.\n";
    out += "#\n";
    out += "# Koordinatlar milimetre tam sayıdır ve [Sağa, Yukarı] sırasındadır.\n\n";
    out += "from typing import Any\n\n";

    out += "class _Document:\n";
    out += "    \"\"\"Çizimden okuma. Hiçbiri çizimi değiştirmez.\"\"\"\n\n";
    out += "    def layers(self) -> list[str]: ...\n";
    out += "    def layer_count(self) -> int: ...\n";
    out += "    def active_layer(self) -> str: ...\n";
    out += "    def entity_count(self) -> int: ...\n";
    out += "    def selection_count(self) -> int: ...\n";
    out += "    def crs(self) -> str: ...\n";
    out += "    def setting(self, id: str) -> Any: ...\n\n";

    out += "doc: _Document\n\n";
    out += "def run(*parts: str) -> int:\n";
    out += "    \"\"\"Bir komut satırını veri yoluna gönderir; metre cinsinden.\"\"\"\n\n";
    out += "def sandbox() -> str: ...\n";
    out += "def read_file(path: str) -> str: ...\n";
    out += "def write_file(path: str, text: str) -> None: ...\n\n";

    for (const CommandSpec* spec : surface(reg)) {
        out += "def " + python_callable_name(*spec) + "(\n";
        out += "    *,\n";
        for (const Param& p : spec->params)
            out += "    " + p.english + ": " + type_of(p) + " = ...,\n";
        out += ") -> int:\n";
        out += "    \"\"\"" + spec->summary + "\n\n";
        out += "    Komut: " + spec->id;
        if (!spec->names.empty()) out += " (" + spec->names.front() + ")";
        out += "\n";
        for (const Param& p : spec->params)
            out += "        " + p.english + " — " + p.help + unit_note(p) + "\n";
        out += "    \"\"\"\n\n";
    }
    return out;
}

} // namespace kentos::script
