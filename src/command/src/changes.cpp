// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/command/changes.hpp"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace kentos::command {
namespace {

using core::Op;

// What an op did to the object it names, as one bit of `Touch::bits`.
constexpr std::uint8_t kShape = 1U << 0U; ///< its geometry or kind
constexpr std::uint8_t kWords = 1U << 1U; ///< its caption's words, height or anchor
constexpr std::uint8_t kValue = 1U << 2U; ///< an attribute cell or foreign data
constexpr std::uint8_t kLayer = 1U << 3U; ///< its layer
constexpr std::uint8_t kLook  = 1U << 4U; ///< its own style or hidden flag
constexpr std::uint8_t kTie   = 1U << 5U; ///< its tie to another object or its origin
constexpr std::uint8_t kAlive = 1U << 6U; ///< whether it is alive; `was_alive` is what it was

/// One op's mark on one object, in record order.
struct Touch
{
    core::EntityId entity{core::kNoEntity};
    std::uint32_t order{0}; ///< position in the record, so the FIRST alive op is found
    std::uint8_t bits{0};
    bool was_alive{false};
};

/// The bit an op sets, or zero for an op that names no object.
std::uint8_t bit_of(Op::Kind kind) noexcept
{
    switch (kind) {
    case Op::Kind::SetGeometry:
    case Op::Kind::SetKindGeometry: return kShape;
    case Op::Kind::SetText: return kWords;
    case Op::Kind::SetAttribute:
    case Op::Kind::AttachForeign:
    case Op::Kind::DetachForeign: return kValue;
    case Op::Kind::SetEntityLayer: return kLayer;
    case Op::Kind::SetEntityStyle:
    case Op::Kind::SetEntityHidden: return kLook;
    case Op::Kind::SetAttachment:
    case Op::Kind::SetDimensionLinks:
    case Op::Kind::SetHatchLinks:
    case Op::Kind::SetLineage: return kTie;
    case Op::Kind::SetEntityAlive: return kAlive;
    default: return 0;
    }
}

/// `count` followed by `what`, the way the sentence says it.
std::string counted(std::size_t count, const char* what)
{
    return std::to_string(count) + " " + what;
}

/// "a, b ve c".
std::string listed(const std::vector<std::string>& parts)
{
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i != 0) out += (i + 1 == parts.size()) ? " ve " : ", ";
        out += parts[i];
    }
    return out;
}

} // namespace

bool ChangeSummary::empty() const noexcept
{
    return created == 0 && erased == 0 && reshaped == 0 && reworded == 0 && revalued == 0 &&
           relayered == 0 && restyled == 0 && retied == 0 && layers == 0 && !blocks && !sheets &&
           !guides && !crs;
}

ChangeSummary summarize_changes(const core::Document& doc, std::span<const core::Op> step)
{
    ChangeSummary out;
    std::vector<Touch> touches;
    std::vector<core::LayerId> layers;
    touches.reserve(step.size());

    for (std::uint32_t i = 0; i < step.size(); ++i) {
        const Op& op = step[i];
        switch (op.kind) {
        case Op::Kind::SetLayerVisible:
        case Op::Kind::SetLayerLocked:
        case Op::Kind::SetLayerAppearance:
        case Op::Kind::SetLayerStyle:
        case Op::Kind::SetLayerGroup: layers.push_back(op.layer); continue;
        case Op::Kind::SetCrs: out.crs = true; continue;
        case Op::Kind::SetGuides: out.guides = true; continue;
        case Op::Kind::SetLayouts: out.sheets = true; continue;
        case Op::Kind::SetBlockBase:
        case Op::Kind::SetBlockExternal: out.blocks = true; continue;
        default: break;
        }
        const std::uint8_t bit = bit_of(op.kind);
        if (bit == 0 || op.entity == core::kNoEntity) continue;
        // The alive op's argument is the state it PUTS BACK — what the object
        // was before this edit (`Document::set_entity_alive`).
        touches.push_back(Touch{
            .entity    = op.entity,
            .order     = i,
            .bits      = bit,
            .was_alive = op.kind == Op::Kind::SetEntityAlive && op.bool_arg,
        });
    }

    // BY OBJECT, IN RECORD ORDER: the first alive op of each object says what it
    // was before the step began.
    std::ranges::sort(touches, [](const Touch& a, const Touch& b) {
        return a.entity != b.entity ? a.entity < b.entity : a.order < b.order;
    });

    const core::EntityTable& entities = doc.entities();
    for (std::size_t i = 0; i < touches.size();) {
        const core::EntityId e = touches[i].entity;
        std::uint8_t bits      = 0;
        bool before_known      = false;
        bool before            = false;
        for (; i < touches.size() && touches[i].entity == e; ++i) {
            bits |= touches[i].bits;
            if ((touches[i].bits & kAlive) != 0 && !before_known) {
                before_known = true;
                before       = touches[i].was_alive;
            }
        }
        const bool now = e < entities.size() && entities.alive(e);
        if (!before_known) before = now;

        // A MEMBER OF A BLOCK DEFINITION is the definition's, not an object of
        // the drawing: making a block of two lines adds one reference, and the
        // two members it now holds are "the block changed".
        if (e < entities.size() && (entities.flags[e] & core::FlagInBlock) != 0) {
            out.blocks = true;
            continue;
        }

        if (!before && now) {
            ++out.created;
            continue;
        }
        if (before && !now) {
            ++out.erased;
            continue;
        }
        if (!now) continue; // born and gone inside the step: neither

        if ((bits & kShape) != 0) ++out.reshaped;
        if ((bits & kWords) != 0) ++out.reworded;
        if ((bits & kValue) != 0) ++out.revalued;
        if ((bits & kLayer) != 0) ++out.relayered;
        if ((bits & kLook) != 0) ++out.restyled;
        if ((bits & kTie) != 0) ++out.retied;
    }

    std::ranges::sort(layers);
    out.layers = static_cast<std::size_t>(
        std::ranges::distance(layers.begin(), std::ranges::unique(layers).begin()));
    return out;
}

std::string describe_changes(const ChangeSummary& s)
{
    std::vector<std::string> came;
    if (s.created != 0) came.push_back(counted(s.created, "nesne eklendi"));
    if (s.erased != 0) came.push_back(counted(s.erased, "nesne silindi"));

    std::vector<std::string> changed;
    if (s.reshaped != 0) changed.push_back(counted(s.reshaped, "nesnenin yeri ya da biçimi"));
    if (s.reworded != 0) changed.push_back(counted(s.reworded, "yazının metni"));
    if (s.revalued != 0) changed.push_back(counted(s.revalued, "nesnenin öznitelik değeri"));
    if (s.relayered != 0) changed.push_back(counted(s.relayered, "nesnenin katmanı"));
    if (s.restyled != 0) changed.push_back(counted(s.restyled, "nesnenin görünüşü"));
    if (s.retied != 0) changed.push_back(counted(s.retied, "nesnenin bağı"));
    if (s.layers != 0) changed.push_back(counted(s.layers, "katmanın ayarları"));
    if (s.blocks) changed.emplace_back("blok tanımları");
    if (s.sheets) changed.emplace_back("çıktı yerleşimleri");
    if (s.guides) changed.emplace_back("kılavuz çizgiler");
    if (s.crs) changed.emplace_back("koordinat sistemi");

    std::string out;
    for (std::size_t i = 0; i < came.size(); ++i)
        out += (i == 0 ? "" : "; ") + came[i];
    if (!changed.empty()) out += (out.empty() ? "" : "; ") + listed(changed) + " değişti";
    return out;
}

core::Json changes_json(const ChangeSummary& s)
{
    const auto n = [](std::size_t v) { return core::Json::integer(static_cast<std::int64_t>(v)); };
    core::Json out;
    out.set("eklenen", n(s.created));
    out.set("silinen", n(s.erased));
    out.set("yeri_bicimi_degisen", n(s.reshaped));
    out.set("metni_degisen", n(s.reworded));
    out.set("degeri_degisen", n(s.revalued));
    out.set("katmani_degisen", n(s.relayered));
    out.set("gorunusu_degisen", n(s.restyled));
    out.set("bagi_degisen", n(s.retied));
    out.set("ayari_degisen_katman", n(s.layers));
    out.set("bloklar", core::Json::boolean(s.blocks));
    out.set("yerlesimler", core::Json::boolean(s.sheets));
    out.set("kilavuzlar", core::Json::boolean(s.guides));
    out.set("koordinat_sistemi", core::Json::boolean(s.crs));
    return out;
}

} // namespace kentos::command
