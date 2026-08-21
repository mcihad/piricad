// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — core: minimal deterministic JSON.
//
// Phase 0 has no external dependency beyond Qt (canon). glaze / simdjson replace
// this behind PIRICAD_WITH_JSON when the dependency set lands (piricad.md §9.1).
// Requirements it must keep: object key order is preserved (journal diffs must be
// stable) and number formatting is locale-independent.
#pragma once

#include "piricad/core/result.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace piricad::core {

class Json;
using JsonArray  = std::vector<Json>;
using JsonObject = std::vector<std::pair<std::string, Json>>; // insertion-ordered

class Json
{
public:
    enum class Type : std::uint8_t { Null, Bool, Int, Double, String, Array, Object };

    Json() = default;
    static Json null();
    static Json boolean(bool v);
    static Json integer(std::int64_t v);
    static Json number(double v);
    static Json string(std::string v);
    static Json array(JsonArray v);
    static Json object(JsonObject v);

    Type type() const noexcept { return type_; }

    bool is_null() const noexcept { return type_ == Type::Null; }

    bool is_bool() const noexcept { return type_ == Type::Bool; }

    bool is_int() const noexcept { return type_ == Type::Int; }

    bool is_number() const noexcept { return type_ == Type::Int || type_ == Type::Double; }

    bool is_string() const noexcept { return type_ == Type::String; }

    bool is_array() const noexcept { return type_ == Type::Array; }

    bool is_object() const noexcept { return type_ == Type::Object; }

    bool as_bool(bool d = false) const;
    std::int64_t as_int(std::int64_t d = 0) const;
    double as_double(double d = 0.0) const;
    const std::string& as_string() const;
    const JsonArray& as_array() const;
    const JsonObject& as_object() const;

    /// Object member lookup; returns nullptr when absent or not an object.
    const Json* find(std::string_view key) const;

    void set(std::string key, Json value);
    void push(Json value);

    /// Compact, deterministic, locale-independent serialisation.
    std::string dump() const;
    /// Indented form, for human-facing files only.
    std::string dump_pretty(int indent = 2) const;

    static Result<Json> parse(std::string_view text);

private:
    Type type_{Type::Null};
    bool b_{false};
    std::int64_t i_{0};
    double d_{0.0};
    std::string s_{};
    JsonArray a_{};
    JsonObject o_{};

    void dump_to(std::string& out, int indent, int depth) const;
};

} // namespace piricad::core
