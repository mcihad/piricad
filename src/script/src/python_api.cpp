// SPDX-License-Identifier: GPL-3.0-or-later
#include "python_impl.hpp"

#include "kentos_cad/command/registry.hpp"
#include "kentos_cad/core/json.hpp"

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
