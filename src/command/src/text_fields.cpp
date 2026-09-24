// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/text_fields.hpp"

#include "kentos_cad/core/attribute.hpp"
#include "kentos_cad/core/dimension.hpp"

namespace kentos::command {
namespace {

/// The figure a `#` name measures, or nothing for a name that is not one.
std::optional<std::string> figure(const core::Document& doc, core::EntityId e,
                                  std::string_view name, const FieldFormat& how)
{
    if (name == "alan") return core::format_area(doc.entity_area(e), how.precision, how.separator);
    if (name == "cevre" || name == "uzunluk")
        return core::format_dimension_length(doc.entity_perimeter(e), how.unit, how.precision,
                                             how.separator);
    return std::nullopt;
}

} // namespace

core::Result<std::string> fill_fields(const core::Document& doc, core::EntityId e,
                                      std::string_view format, const FieldFormat& how)
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
        const core::AttrId col = doc.attributes().find(name);
        if (col == core::kNoAttr) {
            out += whole;
            continue;
        }
        auto value = doc.attribute(col, e);
        if (!value) return value.error();
        out += core::attr_display(value.value(), core::DecimalMark::Comma);
    }
    return out;
}

bool has_fields(std::string_view format)
{
    const std::size_t open = format.find('{');
    return open != std::string_view::npos && format.find('}', open + 1) != std::string_view::npos;
}

} // namespace kentos::command
