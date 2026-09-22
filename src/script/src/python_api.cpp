// SPDX-License-Identifier: GPL-3.0-or-later
#include "python_impl.hpp"

#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/json.hpp"

#include <pybind11/operators.h>
#include <pybind11/stl.h>

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

namespace py = pybind11;

namespace kentos::script::detail {
namespace {

using core::ErrorCode;

/// A Python value as the JSON the bus already knows how to read.
///
/// THE CONVERSION IS NOT WRITTEN TWICE. `Args::from_json` is what the JSON script
/// host feeds, so a coordinate pair, a selection list and a bounded integer all
/// arrive at the bus by the one road that is already tested and already
/// journalled. A second converter would be a second set of edge cases — and the
/// edge cases here are `Mm` integers, which is precisely where a second answer
/// costs a parcel (CLAUDE.md 5.16).
core::Json to_json(const Host& host, const std::string& key, const py::handle& value)
{
    // BOOL BEFORE INT, and the order is load-bearing: in Python `True` IS an
    // integer, `isinstance(True, int)` is true, and a check that asked about
    // integers first would turn every yes/no argument into 1.
    if (py::isinstance<py::bool_>(value)) return core::Json::boolean(py::cast<bool>(value));
    if (py::isinstance<py::int_>(value)) return core::Json::integer(py::cast<std::int64_t>(value));
    if (py::isinstance<py::float_>(value)) return core::Json::number(py::cast<double>(value));
    if (py::isinstance<py::str>(value)) return core::Json::string(py::cast<std::string>(value));

    // THE PROGRAM'S OWN TYPES, so `cad.line(points=[p, q])` works with the Points a
    // read handed back rather than only with the lists a user spelled out. A
    // round trip through `list(p)` would work too and would be one more thing to
    // remember for no reason.
    if (py::isinstance<core::Point2>(value)) {
        const auto p = py::cast<core::Point2>(value);
        return core::Json::array({core::Json::integer(p.x), core::Json::integer(p.y)});
    }
    if (py::isinstance<core::Box2>(value)) {
        const auto b = py::cast<core::Box2>(value);
        return core::Json::array({core::Json::integer(b.min_x), core::Json::integer(b.min_y),
                                  core::Json::integer(b.max_x), core::Json::integer(b.max_y)});
    }

    if (py::isinstance<py::list>(value) || py::isinstance<py::tuple>(value)) {
        core::JsonArray items;
        for (const py::handle item : value)
            items.push_back(to_json(host, key, item));
        return core::Json::array(std::move(items));
    }

    if (value.is_none())
        host.fail(ErrorCode::InvalidArgument,
                  "'" + key +
                      "' için None verildi. Bir parametreyi atlamak için onu hiç yazmayın.");

    host.fail(ErrorCode::InvalidArgument,
              "'" + key + "' için verilen değer çevrilemiyor: " +
                  py::cast<std::string>(py::str(py::type::of(value).attr("__name__"))) +
                  ". Beklenen: int, float, str, bool ya da bunların listesi.");
    return core::Json::null(); // unreachable; `fail` throws
}

/// The parameter a Python keyword names, or null.
const command::Param* param_for(const command::CommandSpec& spec, const std::string& keyword)
{
    for (const command::Param& p : spec.params)
        if (p.english == keyword) return &p;
    return nullptr;
}

/// The keywords a command accepts, for the message a typo gets.
///
/// SPELLED OUT RATHER THAN "bilinmeyen parametre", because the reader is a
/// programmer at a prompt with the answer one line away and no reason to go
/// looking for it (`.claude/script.md` R21: expected versus received).
std::string keyword_list(const command::CommandSpec& spec)
{
    std::string out;
    for (const command::Param& p : spec.params) {
        if (!out.empty()) out += ", ";
        out += p.english;
    }
    return out.empty() ? std::string("(hiç)") : out;
}

/// `help(cad.line)`.
///
/// The Turkish summary and the Turkish parameter help, under an English
/// signature — which is exactly what this API is: an English way in to a Turkish
/// program. A docstring that translated the help would be a second description of
/// a command, and there is one description (CLAUDE.md 5.20).
std::string doc_for(const command::CommandSpec& spec, const std::string& fn)
{
    std::string doc = fn + "(";
    for (std::size_t i = 0; i < spec.params.size(); ++i) {
        if (i != 0) doc += ", ";
        doc += spec.params[i].english + "=...";
    }
    doc += ")\n\n";
    doc += spec.summary.empty() ? spec.id : spec.summary;
    doc += "\n\nKomut: " + spec.id;
    if (!spec.names.empty()) doc += " (" + spec.names.front() + ")";
    doc += "\n";

    if (!spec.params.empty()) {
        doc += "\nParametreler:\n";
        for (const command::Param& p : spec.params) {
            doc += "    " + p.english + " — " + p.help;
            if (!p.unit.empty()) doc += " [" + p.unit + "]";
            doc += "  (" + p.name + ")\n";
        }
    }
    return doc;
}

} // namespace

void bind_types(py::object& cad)
{
    // REGISTERED ONCE PER INTERPRETER, SHARED BY EVERY RUN.
    //
    // `py::class_` binds a C++ type into the interpreter's own type registry, and
    // that registry is process-wide: a second `py::class_<core::Point2>` raises
    // "type Point is already registered". The `cad` module, by contrast, is
    // rebuilt for every run on purpose — each run's callables capture that run's
    // host, and two runners sharing one interpreter must not share one write
    // path.
    //
    // So the classes live in a module of their own that is created once, and each
    // run's `cad` borrows them. The types are values with no host in them, so
    // sharing them is safe in a way sharing `cad.line` would not be.
    py::dict modules = py::module_::import("sys").attr("modules");

    const char* home_name = "kentos._types";
    if (modules.contains(home_name)) {
        const py::object home = modules[home_name];
        cad.attr("Point")     = home.attr("Point");
        cad.attr("Box")       = home.attr("Box");
        return;
    }

    py::object home    = py::module_::import("types").attr("ModuleType")(home_name);
    modules[home_name] = home;

    // THE ONE NAMING DECISION IN THIS FILE, and it is worth the paragraph.
    //
    // `core::Point2` calls its members `x` and `y`, and `x` is the EASTING — the
    // `sağa değer`, which a Turkish pafta labels **Y**. So the C++ `x` is the
    // paper `Y` and the C++ `y` is the paper `X`. That is not a mistake in either
    // place: `x` is the first Cartesian axis in code and `Y` is what EPSG:5254
    // declares for the axis order (model.md R37a).
    //
    // It is, however, a trap with a parcel at the bottom of it. So this API
    // exposes NEITHER letter: `east` and `north` mean one thing each, in every
    // language, to every reader, and a script that says `east` cannot be read the
    // other way round. Indexing and iteration keep the order the command line and
    // the journal use — east first — so `list(p)` and `[485320150, 4310220400]`
    // are the same two numbers in the same places.
    py::class_<core::Point2>(home, "Point", R"(A coordinate, in fixed-point millimetres.

    Point(east, north) — `east` is the sağa değer (labelled Y on a pafta) and
    `north` is the yukarı değer (labelled X). The letters are deliberately not
    offered: they mean opposite things in the code and on the sheet.

    Iterable and indexable as (east, north), so it may be passed anywhere a
    two-number list is accepted.
    )")
        .def(py::init([](core::Mm east, core::Mm north) { return core::Point2{east, north}; }),
             py::arg("east"), py::arg("north"))
        .def_property_readonly(
            "east", [](const core::Point2& p) { return p.x; },
            "Easting in millimetres — the sağa değer, labelled Y on a pafta.")
        .def_property_readonly(
            "north", [](const core::Point2& p) { return p.y; },
            "Northing in millimetres — the yukarı değer, labelled X on a pafta.")
        .def("__len__", [](const core::Point2&) { return 2; })
        .def("__getitem__",
             [](const core::Point2& p, std::size_t i) -> core::Mm {
                 if (i == 0) return p.x;
                 if (i == 1) return p.y;
                 throw py::index_error("Point has two components: east, north");
             })
        .def("__iter__", [](const core::Point2& p) { return py::iter(py::make_tuple(p.x, p.y)); })
        .def("distance_to", &core::distance_metres, py::arg("other"),
             "Straight-line distance to another point, in METRES.")
        .def(py::self == py::self)
        .def(py::self != py::self)
        .def("__hash__", [](const core::Point2& p) { return py::hash(py::make_tuple(p.x, p.y)); })
        .def("__repr__", [](const core::Point2& p) {
            return "cad.Point(east=" + std::to_string(p.x) + ", north=" + std::to_string(p.y) + ")";
        });

    py::class_<core::Box2>(home, "Box", R"(An axis-aligned rectangle, in fixed-point millimetres.

    Box(min_east, min_north, max_east, max_north). An EMPTY box is a real state
    and not an error: it is what a drawing with nothing in it has, and it is
    encoded as min > max rather than as four zeros — in TUREF the origin is a
    thousand kilometres from any Turkish parcel, so a zero box would span the
    country.
    )")
        .def(py::init(
                 [](core::Mm min_east, core::Mm min_north, core::Mm max_east, core::Mm max_north) {
                     return core::Box2{min_east, min_north, max_east, max_north};
                 }),
             py::arg("min_east"), py::arg("min_north"), py::arg("max_east"), py::arg("max_north"))
        .def_property_readonly(
            "min_east", [](const core::Box2& b) { return b.min_x; }, "Left edge, millimetres.")
        .def_property_readonly(
            "min_north", [](const core::Box2& b) { return b.min_y; }, "Bottom edge, millimetres.")
        .def_property_readonly(
            "max_east", [](const core::Box2& b) { return b.max_x; }, "Right edge, millimetres.")
        .def_property_readonly(
            "max_north", [](const core::Box2& b) { return b.max_y; }, "Top edge, millimetres.")
        .def_property_readonly(
            "width", [](const core::Box2& b) { return b.width(); },
            "Extent east to west, millimetres; 0 when empty.")
        .def_property_readonly(
            "height", [](const core::Box2& b) { return b.height(); },
            "Extent south to north, millimetres; 0 when empty.")
        .def_property_readonly(
            "center", [](const core::Box2& b) { return b.centre(); },
            "The middle, as a Point. Rounded toward the lower corner.")
        .def_property_readonly(
            "corners",
            [](const core::Box2& b) {
                return py::make_tuple(
                    core::Point2{b.min_x, b.min_y}, core::Point2{b.max_x, b.min_y},
                    core::Point2{b.max_x, b.max_y}, core::Point2{b.min_x, b.max_y});
            },
            "The four corners counter-clockwise from the lower left, as Points.")
        .def("is_empty", &core::Box2::empty, "Whether the box contains nothing at all.")
        .def(
            "contains",
            [](const core::Box2& b, const core::Point2& p) {
                return !b.empty() && p.x >= b.min_x && p.x <= b.max_x && p.y >= b.min_y &&
                       p.y <= b.max_y;
            },
            py::arg("point"), "Whether `point` is inside, edges included.")
        .def(
            "__iter__",
            [](const core::Box2& b) {
                return py::iter(py::make_tuple(b.min_x, b.min_y, b.max_x, b.max_y));
            },
            "Iterates as (min_east, min_north, max_east, max_north).")
        .def(py::self == py::self)
        .def("__repr__", [](const core::Box2& b) {
            if (b.empty()) return std::string("cad.Box(empty)");
            return "cad.Box(min_east=" + std::to_string(b.min_x) +
                   ", min_north=" + std::to_string(b.min_y) +
                   ", max_east=" + std::to_string(b.max_x) +
                   ", max_north=" + std::to_string(b.max_y) + ")";
        });

    cad.attr("Point") = home.attr("Point");
    cad.attr("Box")   = home.attr("Box");
}

/// `cad.viewport` — what the window is currently looking at.
///
/// VALUES, WHERE THE COMMAND GIVES A REPORT. `GÖRÜNÜMBİLGİSİ` (`core.view_info`)
/// already answers this question and writes it to the transcript, which is the
/// right shape for a person and the wrong one for a script: `bbox = cad.run(...)`
/// returns an operation count. This is the same `Bus::on_view_query` behind a
/// surface a script can compute with — one source, two presentations, which is
/// not the second list 5.10 forbids.
///
/// NO VIEWPORT IS AN HONEST ANSWER. A headless run, a journal replay and a test
/// genuinely have no window, and inventing a rectangle for them would put the
/// next drawing somewhere nobody is looking. `exists()` says so and every other
/// call raises rather than returning a made-up number.
void bind_viewport(Host& host, py::object& cad)
{
    py::object module_type = py::module_::import("types").attr("ModuleType");
    py::object viewport    = module_type("kentos.cad.viewport");

    const auto view = [&host]() -> command::ViewInfo {
        if (!host.bus.on_view_query)
            host.fail(ErrorCode::Unsupported,
                      "Bu çalıştırmada görüntü penceresi yok (başsız çalışıyor). "
                      "cad.viewport.exists() ile önce sorun.");
        return host.bus.on_view_query();
    };

    viewport.attr("exists") = py::cpp_function(
        [&host] { return static_cast<bool>(host.bus.on_view_query); }, py::name("exists"),
        py::scope(viewport),
        py::doc(
            "Whether this run has a window at all. False headless, in a replay and in a test."));

    viewport.attr("bbox") =
        py::cpp_function([view] { return view().window; }, py::name("bbox"), py::scope(viewport),
                         py::doc("The visible rectangle, as a cad.Box."));

    viewport.attr("center") =
        py::cpp_function([view] { return view().centre; }, py::name("center"), py::scope(viewport),
                         py::doc("The middle of the view, as a cad.Point."));

    viewport.attr("scale") =
        py::cpp_function([view] { return view().scale; }, py::name("scale"), py::scope(viewport),
                         py::doc("The denominator of the drawing scale, 1:N. 0 when unknown."));

    viewport.attr("mm_per_pixel") = py::cpp_function(
        [view] { return view().mm_per_pixel; }, py::name("mm_per_pixel"), py::scope(viewport),
        py::doc("Document millimetres per screen pixel — how much ground one pixel covers."));

    viewport.attr("size_px") = py::cpp_function(
        [view] {
            const command::ViewInfo v = view();
            return py::make_tuple(v.width_px, v.height_px);
        },
        py::name("size_px"), py::scope(viewport),
        py::doc("The viewport's own size in pixels, as (width, height)."));

    viewport.attr("crs") = py::cpp_function(
        [view, &host] {
            const command::ViewInfo v = view();
            return v.crs.empty() ? host.bus.document().crs().id() : v.crs;
        },
        py::name("crs"), py::scope(viewport),
        py::doc("The coordinate reference system the view's numbers are in."));

    cad.attr("viewport")                                              = viewport;
    py::module_::import("sys").attr("modules")["kentos.cad.viewport"] = viewport;
}

void bind_commands(Host& host, py::object& cad)
{
    py::list exported;

    for (const command::CommandSpec& entry : host.bus.registry().all()) {
        const command::CommandSpec* spec = &entry;
        if (spec->run == nullptr) continue;

        // SCRIPTABLE ONLY, and that is the same gate the other clients pass. A
        // command that is not reachable from a script is not reachable from
        // Python either; Article 1.2 makes the clients equal, not unlimited.
        if (!has_flag(spec->flags, command::Flags::Scriptable)) continue;

        const std::string fn = command::python_callable_name(*spec);

        // NEVER SHADOW WHAT IS ALREADY THERE. `cad.run`, `cad.doc` and the two
        // file helpers are written by hand and the projection grows on its own,
        // so the day a command is registered under one of those names the two
        // would collide — and the way that showed up the first time was a script
        // asking the document for its layer names and getting an operation count,
        // because `core.layers` had quietly replaced `cad.layers`.
        //
        // Loud rather than silent, and at IMPORT rather than at the call: the
        // person who added the command finds out from the first script that runs,
        // with both names in the message.
        if (py::hasattr(cad, fn.c_str()))
            host.fail(ErrorCode::Unsupported,
                      "'" + spec->id + "' komutunun Python adı '" + fn +
                          "', ve o ad zaten kentos.cad üzerinde var. Komuta CommandSpec::python "
                          "ile başka bir ad verin.");

        cad.attr(fn.c_str()) = py::cpp_function(
            [&host, spec](const py::args& positional, const py::kwargs& keywords) {
                // KEYWORDS ONLY. A command's parameters are a SET and the command
                // line has never had a positional order for them; inventing one
                // here would be a second grammar, and it would silently reorder
                // the day a parameter is added (CLAUDE.md 5.11).
                if (!positional.empty())
                    host.fail(ErrorCode::InvalidArgument,
                              command::python_callable_name(*spec) +
                                  "() yalnız anahtar kelime argümanı alır. Kabul edilenler: " +
                                  keyword_list(*spec));

                core::JsonObject obj;
                for (const auto item : keywords) {
                    const std::string key = py::cast<std::string>(py::str(item.first));

                    const command::Param* p = param_for(*spec, key);
                    if (p == nullptr)
                        host.fail(ErrorCode::InvalidArgument,
                                  command::python_callable_name(*spec) + "() '" + key +
                                      "' diye bir parametre almıyor. Kabul edilenler: " +
                                      keyword_list(*spec));

                    obj.emplace_back(p->name, to_json(host, key, item.second));
                }

                auto args = command::Args::from_json(core::Json::object(std::move(obj)));
                if (!args) host.fail(args.error().code, args.error().message);

                command::Invocation inv;
                inv.name   = spec->id;
                inv.args   = std::move(args.value());
                inv.origin = command::Origin::Script;

                auto result = host.bus.dispatch(inv);
                if (!result)
                    host.fail(result.error().code, command::python_callable_name(*spec) +
                                                       "(): " + result.error().message);

                ++host.commands;
                return result.value().ops;
            },
            py::name(fn.c_str()), py::scope(cad), py::doc(doc_for(*spec, fn).c_str()));

        exported.append(py::str(fn));
    }

    exported.attr("sort")();
    cad.attr("__all__") = py::tuple(exported);
}

} // namespace kentos::script::detail
