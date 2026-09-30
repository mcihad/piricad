// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — core: minimal deterministic JSON.
//
// Phase 0 has no external dependency beyond Qt (canon). glaze / simdjson replace
// this behind KENTOS_WITH_JSON when the dependency set lands (kentoscad.md §9.1).
// Requirements it must keep: object key order is preserved (journal diffs must be
// stable) and number formatting is locale-independent.
#pragma once

#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::core {

/// Declared before the aliases below, which are written in terms of it.
class Json;

/// The elements of a JSON array, in order.
using JsonArray = std::vector<Json>;

/// The members of a JSON object, in INSERTION order.
///
/// A vector of pairs rather than a map, and that is the whole point: object key
/// order is preserved because the journal is compared byte for byte across three
/// clients (CLAUDE.md 6.4) and golden fixtures record the exact bytes. A map would
/// sort by name and make the file's key order an accident of the alphabet.
using JsonObject = std::vector<std::pair<std::string, Json>>;

/// One JSON value.
///
/// This is a FACADE. Parsing and writing are nlohmann/json's since the library
/// migration; what stays here is the value type the rest of the product holds, so
/// that swapping the backend again touches one .cpp file and no call site
/// (Article 9).
class Json
{
public:
    /// What this value holds. `Int` and `Double` are separate on purpose — see
    /// `is_number()` below.
    enum class Type : std::uint8_t { Null, Bool, Int, Double, String, Array, Object };

    /// A null value, which is what a default-constructed Json is.
    Json() = default;

    /// Constructors, one per type. Free functions rather than overloads so the
    /// TYPE is written at every call site: an integer coordinate and a double of
    /// the same magnitude serialise differently and mean different things.
    static Json null();
    static Json boolean(bool v);
    static Json integer(std::int64_t v);
    static Json number(double v);
    static Json string(std::string v);
    static Json array(JsonArray v);
    static Json object(JsonObject v);

    /// What this value holds, for a caller that must branch on it.
    Type type() const noexcept { return type_; }

    /// Type tests. Each answers one question and none of them converts.
    bool is_null() const noexcept { return type_ == Type::Null; }

    bool is_bool() const noexcept { return type_ == Type::Bool; }

    /// Strictly an integer. A caller storing an `Mm` asks THIS and not
    /// `is_number()`, because 485320150.0 is not an integer count of millimetres
    /// even though it reads like one (model.md R21).
    bool is_int() const noexcept { return type_ == Type::Int; }

    /// Either numeric form, for a caller that genuinely accepts both — a scale
    /// factor, a ratio.
    bool is_number() const noexcept { return type_ == Type::Int || type_ == Type::Double; }

    bool is_string() const noexcept { return type_ == Type::String; }

    bool is_array() const noexcept { return type_ == Type::Array; }

    bool is_object() const noexcept { return type_ == Type::Object; }

    /// Readers, each returning `d` — or an empty container — when this value is of
    /// another type. They do not convert and they do not throw: a file written by
    /// another version may hold anything, and an exception at that depth would
    /// take the editor down instead of producing a message naming the field.
    bool as_bool(bool d = false) const;
    std::int64_t as_int(std::int64_t d = 0) const;
    double as_double(double d = 0.0) const;
    const std::string& as_string() const;
    const JsonArray& as_array() const;
    const JsonObject& as_object() const;

    /// Object member lookup; returns nullptr when absent or not an object.
    const Json* find(std::string_view key) const;

    /// Adds or replaces an object member, turning this value into an object if it
    /// was not one. A replaced member keeps its original position, so rewriting a
    /// field cannot reorder the file.
    void set(std::string key, Json value);

    /// Appends to an array, turning this value into an array if it was not one.
    void push(Json value);

    /// Compact, deterministic, locale-independent serialisation.
    std::string dump() const;
    /// Indented form, for human-facing files only.
    std::string dump_pretty(int indent = 2) const;

    /// Parses text into a value, or says why it could not.
    ///
    /// Never throws: parsing is what a hostile script, a corrupt catalogue and a
    /// truncated journal all reach first, and core reports failure with `Result`
    /// (core.md). The backing library's exceptions are caught at the boundary.
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

} // namespace kentos::core
