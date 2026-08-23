// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/core/json.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace piricad::core {
namespace {

const std::string& empty_string()
{
    static const std::string s;
    return s;
}

const JsonArray& empty_array()
{
    static const JsonArray a;
    return a;
}

const JsonObject& empty_object()
{
    static const JsonObject o;
    return o;
}

/// Our tree to nlohmann's, for dumping.
nlohmann::ordered_json to_nlohmann(const Json& v)
{
    switch (v.type()) {
    case Json::Type::Null: return nullptr;
    case Json::Type::Bool: return v.as_bool();
    case Json::Type::Int: return v.as_int();
    case Json::Type::Double: return v.as_double();
    case Json::Type::String: return v.as_string();
    case Json::Type::Array: {
        auto out = nlohmann::ordered_json::array();
        for (const Json& e : v.as_array())
            out.push_back(to_nlohmann(e));
        return out;
    }
    case Json::Type::Object: {
        auto out = nlohmann::ordered_json::object();
        for (const auto& [key, value] : v.as_object())
            out[key] = to_nlohmann(value);
        return out;
    }
    }
    return nullptr;
}

/// nlohmann's tree to ours, after parsing.
Json from_nlohmann(const nlohmann::ordered_json& j)
{
    if (j.is_null()) return Json::null();
    if (j.is_boolean()) return Json::boolean(j.get<bool>());

    // The integer/double distinction is kept, not collapsed. A coordinate is an
    // integer count of millimetres and writing it back as 485320150.0 would make
    // the journal disagree with itself across a round trip (model.md R21).
    if (j.is_number_integer() || j.is_number_unsigned())
        return Json::integer(j.get<std::int64_t>());
    if (j.is_number_float()) return Json::number(j.get<double>());
    if (j.is_string()) return Json::string(j.get<std::string>());

    if (j.is_array()) {
        JsonArray items;
        items.reserve(j.size());
        for (const auto& e : j)
            items.push_back(from_nlohmann(e));
        return Json::array(std::move(items));
    }

    JsonObject fields;
    fields.reserve(j.size());
    for (auto it = j.begin(); it != j.end(); ++it)
        fields.emplace_back(it.key(), from_nlohmann(it.value()));
    return Json::object(std::move(fields));
}

} // namespace

Json Json::null()
{
    return Json{};
}

Json Json::boolean(bool v)
{
    Json j;
    j.type_ = Type::Bool;
    j.b_    = v;
    return j;
}

Json Json::integer(std::int64_t v)
{
    Json j;
    j.type_ = Type::Int;
    j.i_    = v;
    return j;
}

Json Json::number(double v)
{
    Json j;
    j.type_ = Type::Double;
    j.d_    = v;
    return j;
}

Json Json::string(std::string v)
{
    Json j;
    j.type_ = Type::String;
    j.s_    = std::move(v);
    return j;
}

Json Json::array(JsonArray v)
{
    Json j;
    j.type_ = Type::Array;
    j.a_    = std::move(v);
    return j;
}

Json Json::object(JsonObject v)
{
    Json j;
    j.type_ = Type::Object;
    j.o_    = std::move(v);
    return j;
}

bool Json::as_bool(bool d) const
{
    return type_ == Type::Bool ? b_ : d;
}

std::int64_t Json::as_int(std::int64_t d) const
{
    if (type_ == Type::Int) return i_;
    if (type_ == Type::Double) return static_cast<std::int64_t>(d_ >= 0 ? d_ + 0.5 : d_ - 0.5);
    return d;
}

double Json::as_double(double d) const
{
    if (type_ == Type::Double) return d_;
    if (type_ == Type::Int) return static_cast<double>(i_);
    return d;
}

const std::string& Json::as_string() const
{
    return type_ == Type::String ? s_ : empty_string();
}

const JsonArray& Json::as_array() const
{
    return type_ == Type::Array ? a_ : empty_array();
}

const JsonObject& Json::as_object() const
{
    return type_ == Type::Object ? o_ : empty_object();
}

const Json* Json::find(std::string_view key) const
{
    if (type_ != Type::Object) return nullptr;
    for (const auto& [k, v] : o_)
        if (k == key) return &v;
    return nullptr;
}

void Json::set(std::string key, Json value)
{
    if (type_ != Type::Object) {
        type_ = Type::Object;
        o_.clear();
    }
    for (auto& [k, v] : o_) {
        if (k == key) {
            v = std::move(value);
            return;
        }
    }
    o_.emplace_back(std::move(key), std::move(value));
}

void Json::push(Json value)
{
    if (type_ != Type::Array) {
        type_ = Type::Array;
        a_.clear();
    }
    a_.push_back(std::move(value));
}

std::string Json::dump() const
{
    return to_nlohmann(*this).dump();
}

std::string Json::dump_pretty(int indent) const
{
    return to_nlohmann(*this).dump(indent);
}

Result<Json> Json::parse(std::string_view text)
{
    // nlohmann's parser, not ours. Parsing is the dangerous half of a JSON
    // facade: it is what a hostile .json script, a corrupt catalogue and a
    // truncated journal all reach first, and a hand-rolled scanner is exactly the
    // kind of code CLAUDE.md 5.16 says not to write when a hardened one exists.
    // The dumper follows it so that what we write is what it reads.
    //
    // ordered_json, NOT json: the default container sorts object keys, and the
    // journal is compared byte for byte across three clients (6.4). Verified
    // before the migration that the output is identical to the hand-rolled
    // dumper's, so no golden fixture moved.
    //
    // Exceptions are caught here and turned into a Result. Nothing above this
    // line throws, and core::Result is how core reports failure (core.md).
    try {
        const auto parsed = nlohmann::ordered_json::parse(text, nullptr, true, false);
        return from_nlohmann(parsed);
    } catch (const nlohmann::ordered_json::parse_error& e) {
        return err(ErrorCode::ParseError,
                   "JSON ayrıştırma hatası, " + std::to_string(e.byte) + ". bayt: " + e.what());
    } catch (const std::exception& e) {
        return err(ErrorCode::ParseError, std::string("JSON okunamadı: ") + e.what());
    }
}

} // namespace piricad::core
