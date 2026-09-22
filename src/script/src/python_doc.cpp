// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/script/python_doc.hpp"

#include "kentos_cad/command/registry.hpp"

#include <string>
#include <vector>

namespace kentos::script {
namespace {

using namespace kentos::command;

} // namespace

/// What a parameter looks like from Python.
///
/// THE UNITS ARE THE POINT OF THIS TABLE. A `Point` is two `Mm` integers and a
/// `Number` is metres, and a reader who guesses wrong puts the parcel a thousand
/// times too far away. `Mm` is what the document stores (Article 2.4), so it is
/// what a generated call takes — the metre spelling belongs to `cad.run`, which
/// is the command line and reads the command line's grammar.
std::string python_type_name(const Param& p)
{
    // ONE POINT OR SEVERAL IS THE ARITY'S QUESTION, not the kind's. A `PointList`
    // declared `Arity::optional()` takes ONE point — `core.circle_draw`'s centre
    // is declared that way — and calling it `list[list[int]]` told every reader to
    // write `center=[[y, x]]`, which is not what the bus accepts and not what the
    // manual shows.
    const bool many = p.arity.max > 1;
    switch (p.kind) {
    // `Coord` and `Coords` rather than the shapes spelled out: a coordinate is
    // `cad.Point(east, north)` OR the two-integer list that has always worked,
    // and writing the union at every parameter would make a nine-parameter
    // signature unreadable for a fact that is true of all of them. The stub
    // declares the aliases; the reference says so in one sentence.
    case ParamKind::Point: return many ? "Coords" : "Coord";
    case ParamKind::PointList: return many ? "Coords" : "Coord";
    case ParamKind::Number: return many ? "list[float]" : "float";
    case ParamKind::Integer: return many ? "list[int]" : "int";
    case ParamKind::Text: return many ? "list[str]" : "str";
    case ParamKind::Bool: return "bool";
    case ParamKind::Selection: return "list[int]";
    }
    return "object";
}

namespace {

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
    out += "- `Coord` bir koordinattır: `cad.Point(east, north)` ya da iki elemanlı\n";
    out += "  tam sayı listesi. `Coords` bunlardan bir liste.\n";
    out += "- Koordinatlar **milimetre tam sayıdır** ve `[Sağa, Yukarı]` sırasındadır.\n";
    out += "  Metre yazmak isterseniz `cad.run(\"ÇİZGİ 0,0 10,10\")` komut satırının\n";
    out += "  dilbilgisini kullanır.\n";
    out += "- Dönen değer, komutun kaç ilkel düzenleme yaptığıdır.\n";
    out += "- Bir hata istisna yükseltir; yakalamazsanız betiğin tamamı geri alınır.\n\n";

    out += "## Değer tipleri\n\n";
    out += "Bunlar programın kendi tipleridir, Python tarafında yeniden tanımlanmış\n";
    out += "kopyaları değil: `cad.Point`, `core::Point2`'nin ta kendisidir.\n\n";
    out += "**Eksen adları harf DEĞİL.** `Point2.x` doğuya gider ve bir paftada **Y**\n";
    out += "yazar; `.y` kuzeye gider ve paftada **X** yazar. Harflerden hangisini\n";
    out += "seçersek okuyucuların yarısı tersini anlar, bu yüzden API `east` ve `north`\n";
    out += "der. Sıralama komut satırı ve günlükle aynıdır: önce doğu.\n\n";
    out += "| Çağrı | Döndürdüğü |\n|---|---|\n";
    out += "| `cad.Point(east, north)` | Bir koordinat, milimetre tam sayı |\n";
    out += "| `p.east` · `p.north` | `int` |\n";
    out += "| `p[0]` · `p[1]` · `list(p)` | doğu, kuzey sırasıyla |\n";
    out += "| `p.distance_to(q)` | `float` — **metre** |\n";
    out += "| `cad.Box(min_east, min_north, max_east, max_north)` | Bir dikdörtgen |\n";
    out += "| `b.min_east` · `b.min_north` · `b.max_east` · `b.max_north` | `int` |\n";
    out += "| `b.width` · `b.height` | `int` — milimetre, boş kutuda 0 |\n";
    out += "| `b.center` | `cad.Point` |\n";
    out += "| `b.corners` | dört `cad.Point`, sol alttan saat yönünün tersine |\n";
    out += "| `b.contains(point)` | `bool` — kenarlar dahil |\n";
    out += "| `b.is_empty()` | `bool` — boş kutu gerçek bir durumdur, hata değil |\n\n";
    out += "Bir `Point`, iki sayılık liste kabul eden her yere doğrudan verilebilir:\n";
    out += "`cad.line(points=[a, b])`.\n\n";

    out += "## Görünüm\n\n";
    out += "`cad.viewport`, pencerenin o an baktığı yeri **değer olarak** verir.\n";
    out += "`GÖRÜNÜMBİLGİSİ` komutu aynı kaynağı okur ama transkripte bir cümle yazar:\n";
    out += "biri insanın, öbürü betiğin okuduğu biçimdir.\n\n";
    out += "| Çağrı | Döndürdüğü |\n|---|---|\n";
    out +=
        "| `cad.viewport.exists()` | `bool` — başsız çalıştırmada, oynatmada ve testte `False` |\n";
    out += "| `cad.viewport.bbox()` | `cad.Box` — görünen dikdörtgen |\n";
    out += "| `cad.viewport.center()` | `cad.Point` |\n";
    out += "| `cad.viewport.scale()` | `int` — 1:N'deki N, bilinmiyorsa 0 |\n";
    out += "| `cad.viewport.mm_per_pixel()` | `float` |\n";
    out += "| `cad.viewport.size_px()` | `(genişlik, yükseklik)` piksel |\n";
    out += "| `cad.viewport.crs()` | `str` |\n\n";
    out += "Pencere yoksa `exists()` dışındaki çağrılar hata verir — uydurulmuş bir\n";
    out += "dikdörtgen, sonraki çizimi kimsenin bakmadığı bir yere koyardı.\n\n";

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
            out += "\n    " + spec->params[i].english + ": " + python_type_name(spec->params[i]);
            out += (i + 1 == spec->params.size()) ? "," : ",";
        }
        if (!spec->params.empty()) out += "\n";
        out += ") -> int\n```\n";

        if (!spec->params.empty()) {
            out += "\n| Anahtar | Tür | Türkçe adı | Açıklama |\n|---|---|---|---|\n";
            for (const Param& p : spec->params) {
                out += "| `" + p.english + "` | `" + python_type_name(p) + "` | `" + p.name +
                       "` | " + p.help + unit_note(p) + " |\n";
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

    out += "# Bir koordinat iki biçimde yazılabilir ve ikisi de her yerde geçerlidir.\n";
    out += "Coord = 'Point | list[int]'\n";
    out += "Coords = 'list[Coord]'\n\n";

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

    out += "class Point:\n";
    out += "    \"\"\"Bir koordinat, milimetre tam sayı.\n\n";
    out += "    east  — sağa değer (paftada Y).   north — yukarı değer (paftada X).\n";
    out += "    Harfler bilerek sunulmuyor: kodda ve paftada ters şeyler demek.\n";
    out += "    \"\"\"\n\n";
    out += "    def __init__(self, east: int, north: int) -> None: ...\n";
    out += "    @property\n    def east(self) -> int: ...\n";
    out += "    @property\n    def north(self) -> int: ...\n";
    out += "    def distance_to(self, other: 'Point') -> float: ...\n";
    out += "    def __len__(self) -> int: ...\n";
    out += "    def __getitem__(self, i: int) -> int: ...\n";
    out += "    def __iter__(self) -> Any: ...\n\n";

    out += "class Box:\n";
    out += "    \"\"\"Eksenlere paralel bir dikdörtgen, milimetre tam sayı.\"\"\"\n\n";
    out += "    def __init__(self, min_east: int, min_north: int,\n";
    out += "                 max_east: int, max_north: int) -> None: ...\n";
    out += "    @property\n    def min_east(self) -> int: ...\n";
    out += "    @property\n    def min_north(self) -> int: ...\n";
    out += "    @property\n    def max_east(self) -> int: ...\n";
    out += "    @property\n    def max_north(self) -> int: ...\n";
    out += "    @property\n    def width(self) -> int: ...\n";
    out += "    @property\n    def height(self) -> int: ...\n";
    out += "    @property\n    def center(self) -> Point: ...\n";
    out += "    @property\n    def corners(self) -> tuple[Point, Point, Point, Point]: ...\n";
    out += "    def contains(self, point: Point) -> bool: ...\n";
    out += "    def is_empty(self) -> bool: ...\n";
    out += "    def __iter__(self) -> Any: ...\n\n";

    out += "class _Viewport:\n";
    out += "    \"\"\"Pencerenin o an baktığı yer. Pencere yoksa exists() False döner.\"\"\"\n\n";
    out += "    def exists(self) -> bool: ...\n";
    out += "    def bbox(self) -> Box: ...\n";
    out += "    def center(self) -> Point: ...\n";
    out += "    def scale(self) -> int: ...\n";
    out += "    def mm_per_pixel(self) -> float: ...\n";
    out += "    def size_px(self) -> tuple[int, int]: ...\n";
    out += "    def crs(self) -> str: ...\n\n";
    out += "viewport: _Viewport\n\n";
    out += "def run(*parts: str) -> int:\n";
    out += "    \"\"\"Bir komut satırını veri yoluna gönderir; metre cinsinden.\"\"\"\n\n";
    out += "def sandbox() -> str: ...\n";
    out += "def read_file(path: str) -> str: ...\n";
    out += "def write_file(path: str, text: str) -> None: ...\n\n";

    for (const CommandSpec* spec : surface(reg)) {
        out += "def " + python_callable_name(*spec) + "(\n";
        out += "    *,\n";
        for (const Param& p : spec->params)
            out += "    " + p.english + ": " + python_type_name(p) + " = ...,\n";
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
