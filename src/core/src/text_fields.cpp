// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/core/text_fields.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/dimension.hpp"

#include <algorithm>
#include <optional>

namespace kentos::core {
namespace {

/// The figure a `#` name measures, or nothing for a name that is not one.
std::optional<std::string> figure(const Document& doc, EntityId e, std::string_view name,
                                  const FieldFormat& how)
{
    if (name == "alan") return format_area(doc.entity_area(e), how.precision, how.separator);
    if (name == "cevre" || name == "uzunluk")
        return format_dimension_length(doc.entity_perimeter(e), how.unit, how.precision,
                                       how.separator);
    return std::nullopt;
}

} // namespace

Result<std::string> fill_fields(const Document& doc, EntityId e, std::string_view format,
                                const FieldFormat& how)
{
    std::string out;
    out.reserve(format.size());
    for (std::size_t i = 0; i < format.size();) {
        if (format[i] != '{') {
            out += format[i++];
            continue;
        }
        const std::size_t close = format.find('}', i + 1);
        if (close == std::string_view::npos) {
            out += format[i++];
            continue;
        }
        const std::string_view name  = format.substr(i + 1, close - i - 1);
        const std::string_view whole = format.substr(i, close - i + 1);
        i                            = close + 1;

        if (name.starts_with('#')) {
            if (const auto measured = figure(doc, e, name.substr(1), how); measured)
                out += *measured;
            else
                out += whole;
            continue;
        }
        const AttrId col = doc.attributes().find(name);
        if (col == kNoAttr) {
            out += whole;
            continue;
        }
        auto value = doc.attribute(col, e);
        if (!value) return value.error();
        out += attr_display(value.value(), DecimalMark::Comma);
    }
    return out;
}

std::vector<std::string> field_names(std::string_view format)
{
    std::vector<std::string> out;
    for (std::size_t open = format.find('{'); open != std::string_view::npos;
         open             = format.find('{', open + 1)) {
        const std::size_t close = format.find('}', open + 1);
        if (close == std::string_view::npos) break;
        const std::string_view name = format.substr(open + 1, close - open - 1);
        if (name.empty() || name.starts_with('#')) continue;
        if (std::find(out.begin(), out.end(), name) == out.end()) out.emplace_back(name);
    }
    return out;
}

bool has_fields(std::string_view format)
{
    const std::size_t open = format.find('{');
    return open != std::string_view::npos && format.find('}', open + 1) != std::string_view::npos;
}

} // namespace kentos::core
