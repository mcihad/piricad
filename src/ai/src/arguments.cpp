// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/ai/arguments.hpp"

#include <locale>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

namespace kentos::ai {
namespace {

using core::Json;

/// Locale-independent formatting of a real number.
///
/// The idiom is `parser.cpp`'s, and for its reason: the default stream locale
/// would write `3,5` on a Turkish system, and a command line a person can retype
/// must not depend on where they live.
std::string format_number(double v)
{
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << v;
    return out.str();
}

/// A coordinate as the command line writes it: easting first, integer
/// millimetres, no decimal point (model.md R37a, and `Mm` is an int64).
std::string format_point(core::Point2 p)
{
    return std::to_string(p.x) + "," + std::to_string(p.y);
}

/// Whether a text value has to be quoted to survive the command line's tokeniser.
bool needs_quotes(const std::string& text)
{
    if (text.empty()) return true;
    for (const char c : text)
        if (c == ' ' || c == '\t' || c == '=' || c == '"') return true;
    return false;
}

/// Why a value is not a position this road accepts, and where one comes from.
std::string position_help(const std::string& name, const Json& given, bool selection)
{
    return "`" + name + "` bir " + (selection ? std::string("TUTAMAK") : std::string("KONUM")) +
           " bekler; koordinat ya da anahtar yazılamaz. Gelen: " + given.dump() + ". " +
           (selection
                ? std::string("Nesneler için `secimi_al` ya da `sorgula` çağırın ve dönen "
                              "`@0123456789abcdef` tutamağını yazın.")
                : std::string("Konum bir okuma aracının tutamağıdır — ekranın ortası için "
                              "`gorunum_bilgisi`, bir nesnenin merkezi, köşeleri ya da uçları "
                              "için `nesne_noktalari` — ya da ondan bir ÖLÇÜYLE uzaklaşan bir "
                              "nokta: {\"taban\": \"@….0\", \"dogu\": 10000, \"kuzey\": 0} "
                              "(milimetre)."));
}

/// One position: a handle naming exactly one point, or such a handle moved by an
/// offset. Fills `point`, records what it used, or answers the refusal.
std::optional<std::string> one_point(const std::string& name, const Json& given,
                                     const HandleStore& handles, std::uint64_t revision,
                                     CompiledArguments& out, core::Point2& point, bool& literal)
{
    literal               = false;
    const Json* base_text = &given;
    std::int64_t east     = 0;
    std::int64_t north    = 0;
    bool relative         = false;

    if (given.is_object()) {
        // A RELATIVE POINT: a base the drawing supplied and a dimension from it.
        const Json* base = given.find("taban");
        if (base == nullptr)
            return "`" + name +
                   "` göreli bir nokta ise `taban` gerekir: bir nokta tutamağı. "
                   "Gelen: " +
                   given.dump() + ".";
        for (const auto& [key, value] : given.as_object()) {
            if (key == "taban") continue;
            if (key != "dogu" && key != "kuzey")
                return "`" + name +
                       "` göreli noktasında yalnız `taban`, `dogu` ve `kuzey` "
                       "olur; `" +
                       key + "` tanınmıyor.";
            if (!value.is_int())
                return "`" + name + "." + key +
                       "` tam sayı milimetre bekler; gelen: " + value.dump() + ".";
            const std::int64_t mm = value.as_int();
            if (mm > kMaxRelativeOffset || mm < -kMaxRelativeOffset)
                return "`" + name + "." + key + "` çok büyük: " + std::to_string(mm) +
                       " mm. Göreli bir nokta tabanından en çok 1000 km uzaklaşır.";
            (key == "dogu" ? east : north) = mm;
        }
        base_text = base;
        relative  = true;
    }

    const std::optional<HandleRef> ref =
        base_text->is_string() ? HandleRef::parse(base_text->as_string()) : std::nullopt;
    if (!ref) {
        literal = true;
        return position_help(name, given, false);
    }
    core::Result<HandleValue> resolved = handles.resolve(*ref, revision);
    if (!resolved) return resolved.error().message;

    const HandleValue& value = resolved.value();
    if (value.kind != HandleKind::Points)
        return "`" + name + "` nokta tutamağı bekler; '" + value.id + "' nokta taşımıyor. " +
               (value.kind == HandleKind::Window
                    ? std::string("Pencere tutamağı bir dikdörtgendir; ortası için "
                                  "`gorunum_bilgisi`nin nokta tutamağını kullanın.")
                    : std::string("Bir nesnenin noktaları için `nesne_noktalari` çağırın."));
    if (value.points.size() != 1)
        return "`" + name + "` tek nokta bekler; '" + value.id + "' " +
               std::to_string(value.points.size()) + " nokta taşıyor. Birini seçmek için `" +
               value.id + ".0` biçiminde yazın.";

    point = value.points.front();
    point.x += east;
    point.y += north;
    out.handles.push_back(value.id);
    if (relative)
        out.constructions.push_back(base_text->as_string() + " + doğu " + std::to_string(east) +
                                    ", kuzey " + std::to_string(north) + " mm");
    return std::nullopt;
}

} // namespace

/// The command line a person reads on the suggestion card — and could type.
///
/// THE SAME WORDS THEY WOULD TYPE, because the GUI, the command line, a script
/// and the agent roads are equal clients (Article 1.2) and a suggestion the engineer
/// cannot read is a suggestion they cannot be responsible for. Points go out as
/// bare `Y,X` tokens and everything else as `name=value`, which is exactly what
/// `command/parser.hpp` accepts; a value holding a space or an `=` is quoted, the
/// form `ad="YOL KENARI"` the tokeniser already understands.
std::string render_line(const command::CommandSpec& spec, const command::Args& args)
{
    std::string out = spec.names.empty() ? spec.id : spec.names.front();

    for (const auto& [name, value] : args.items()) {
        switch (value.kind()) {
        case command::Value::Kind::Empty: break;

        case command::Value::Kind::Point:
            out += ' ';
            out += format_point(value.as_point());
            break;

        case command::Value::Kind::PointList:
            for (const core::Point2& p : value.as_points()) {
                out += ' ';
                out += format_point(p);
            }
            break;

        case command::Value::Kind::IdList:
            for (const std::int64_t id : value.as_ids()) {
                out += ' ' + name + '=' + std::to_string(id);
            }
            break;

        case command::Value::Kind::Bool:
            out += ' ' + name + '=' + (value.as_bool() ? "evet" : "hayır");
            break;

        case command::Value::Kind::Int:
            out += ' ' + name + '=' + std::to_string(value.as_int());
            break;

        case command::Value::Kind::Number:
            out += ' ' + name + '=' + format_number(value.as_number());
            break;

        case command::Value::Kind::Text: {
            const std::string& text = value.as_text();
            out += ' ' + name + '=';
            out += needs_quotes(text) ? '"' + text + '"' : text;
            break;
        }

        case command::Value::Kind::TextList:
            // THE KEY WRITTEN ONCE PER WORD, which is how the tokeniser reads a
            // list — the same shape `pencere=` and `nesneler=` already use.
            for (const std::string& word : value.as_texts()) {
                out += ' ' + name + '=';
                out += needs_quotes(word) ? '"' + word + '"' : word;
            }
            break;

        case command::Value::Kind::NumberList:
            // Once per reading, the same shape: `ayak=10 ayak=30 ayak=60`. The
            // tokeniser reads repeated keys as a run and `bind_tokens`
            // accumulates them, so what is rendered here parses back to what was
            // held (Article 1.4).
            for (const double one : value.as_numbers()) {
                out += ' ' + name + '=';
                out += format_number(one);
            }
            break;
        }
    }
    return out;
}

CompiledArguments compile_arguments(const command::CommandSpec& spec, const Json& arguments,
                                    const HandleStore& handles, std::uint64_t revision)
{
    CompiledArguments out;
    if (!arguments.is_object()) {
        out.protocol_fault = true;
        out.refusal = "Araç argümanları bir nesne olmalı; gelen: " + arguments.dump() + ".";
        return out;
    }

    for (const command::Param& param : spec.params) {
        const Json* given = arguments.find(param.name);
        if (given == nullptr || given->is_null()) {
            if (param.arity.min > 0) {
                out.protocol_fault = true;
                out.refusal = "`" + param.name + "` parametresi zorunlu ve verilmedi. Beklenen: " +
                              command::param_kind_label(param.kind) + ".";
                return out;
            }
            continue;
        }

        switch (param.kind) {
        case command::ParamKind::Point: {
            // THE REFUSAL THAT HAPPENS BEFORE EVERYTHING ELSE (CLAUDE.md 5.8, ai.md
            // R9/R10): a number is not a position. It is answered while the
            // arguments are still JSON, before an `Args` exists.
            core::Point2 at{};
            bool literal = false;
            if (auto why = one_point(param.name, *given, handles, revision, out, at, literal)) {
                out.coordinate_literal = literal;
                out.refusal            = std::move(*why);
                return out;
            }
            out.args.set(param.name, command::Value::point(at));
            break;
        }

        case command::ParamKind::PointList: {
            std::vector<core::Point2> points;
            if (given->is_string()) {
                // ONE HANDLE FOR THE WHOLE LIST: the corners a read tool found.
                const std::optional<HandleRef> ref = HandleRef::parse(given->as_string());
                if (!ref) {
                    out.coordinate_literal = true;
                    out.refusal            = position_help(param.name, *given, false);
                    return out;
                }
                core::Result<HandleValue> resolved = handles.resolve(*ref, revision);
                if (!resolved) {
                    out.refusal = resolved.error().message;
                    return out;
                }
                if (resolved.value().kind != HandleKind::Points) {
                    out.refusal = "`" + param.name + "` nokta tutamağı bekler; '" +
                                  resolved.value().id + "' nokta taşımıyor.";
                    return out;
                }
                points = resolved.value().points;
                out.handles.push_back(resolved.value().id);
            } else if (given->is_array()) {
                // A LIST OF POSITIONS, each a handle or a relative point: how a
                // polygon's corners are said from one centre or one corner.
                for (const Json& element : given->as_array()) {
                    core::Point2 at{};
                    bool literal = false;
                    if (auto why =
                            one_point(param.name, element, handles, revision, out, at, literal)) {
                        out.coordinate_literal = literal;
                        out.refusal            = std::move(*why);
                        return out;
                    }
                    points.push_back(at);
                }
            } else {
                out.coordinate_literal = true;
                out.refusal            = position_help(param.name, *given, false);
                return out;
            }
            out.args.set(param.name, command::Value::points(std::move(points)));
            break;
        }

        case command::ParamKind::Selection: {
            const std::optional<HandleRef> ref =
                given->is_string() ? HandleRef::parse(given->as_string()) : std::nullopt;
            if (!ref) {
                out.coordinate_literal = true;
                out.refusal            = position_help(param.name, *given, true);
                return out;
            }
            core::Result<HandleValue> resolved = handles.resolve(*ref, revision);
            if (!resolved) {
                // A DOMAIN REFUSAL, not a protocol one: the reference was
                // well-formed and the drawing moved on under it.
                out.refusal = resolved.error().message;
                return out;
            }
            if (resolved.value().kind != HandleKind::Entities) {
                out.refusal = "`" + param.name + "` nesne tutamağı bekler; '" +
                              resolved.value().id +
                              "' nesne taşımıyor. `secimi_al` ya da `sorgula` çağırın.";
                return out;
            }
            out.handles.push_back(resolved.value().id);
            out.args.set(param.name, command::Value::ids(resolved.value().entities));
            break;
        }

        case command::ParamKind::Number:
            if (!given->is_number()) {
                out.protocol_fault = true;
                out.refusal = "`" + param.name + "` sayı bekler; gelen: " + given->dump() + ".";
                return out;
            }
            out.args.set(param.name, command::Value::number(given->as_double()));
            break;

        case command::ParamKind::Integer:
            // AN INTEGER PARAMETER WHOSE ARITY ALLOWS SEVERAL IS A LIST, and the
            // published schema says `array` for exactly those (catalog.cpp). The
            // two have to agree or a model writes what the bus then refuses.
            if (param.arity.max > 1) {
                if (!given->is_array()) {
                    out.protocol_fault = true;
                    out.refusal        = "`" + param.name +
                                  "` tam sayı dizisi bekler; gelen: " + given->dump() + ".";
                    return out;
                }
                command::Value::Ints ids;
                for (const Json& element : given->as_array()) {
                    if (!element.is_int()) {
                        out.protocol_fault = true;
                        out.refusal        = "`" + param.name +
                                      "` yalnız tam sayı taşır; gelen: " + element.dump() + ".";
                        return out;
                    }
                    ids.push_back(element.as_int());
                }
                out.args.set(param.name, command::Value::ids(std::move(ids)));
            } else {
                if (!given->is_int()) {
                    out.protocol_fault = true;
                    out.refusal =
                        "`" + param.name + "` tam sayı bekler; gelen: " + given->dump() + ".";
                    return out;
                }
                out.args.set(param.name, command::Value::integer(given->as_int()));
            }
            break;

        case command::ParamKind::Text:
            if (!given->is_string()) {
                out.protocol_fault = true;
                out.refusal = "`" + param.name + "` metin bekler; gelen: " + given->dump() + ".";
                return out;
            }
            out.args.set(param.name, command::Value::text(given->as_string()));
            break;

        case command::ParamKind::Bool:
            if (!given->is_bool()) {
                out.protocol_fault = true;
                out.refusal =
                    "`" + param.name + "` evet/hayır bekler; gelen: " + given->dump() + ".";
                return out;
            }
            out.args.set(param.name, command::Value::boolean(given->as_bool()));
            break;
        }
    }

    // AN UNDECLARED ARGUMENT IS REFUSED HERE, with its name. The bus refuses one
    // too (command.md P15) and the published schema closes the object
    // (`additionalProperties: false`), but a refusal from three layers down does
    // not say which field was wrong, and a client cannot fix what it cannot see.
    for (const auto& [key, value] : arguments.as_object()) {
        (void)value;
        if (key == "_meta") continue;
        bool declared = false;
        for (const command::Param& param : spec.params)
            if (param.name == key) declared = true;
        if (!declared) {
            out.protocol_fault = true;
            out.refusal        = "Bilinmeyen parametre: `" + key +
                          "`. Bu araç yalnız şemasında yazan parametreleri kabul eder.";
            return out;
        }
    }
    return out;
}

} // namespace kentos::ai
