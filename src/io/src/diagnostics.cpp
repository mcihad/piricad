// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/io/diagnostics.hpp"

#include <algorithm>
#include <utility>

namespace kentos::io {

const char* severity_prefix(Severity level) noexcept
{
    switch (level) {
    case Severity::Info: return "not";
    case Severity::Warning: return "uyarı";
    case Severity::Degraded: return "düşürme";
    case Severity::Skipped: return "atlandı";
    case Severity::Error: return "hata";
    }
    return "not";
}

void ImportDiagnostics::note(Severity level, std::string text)
{
    if (notes.size() >= kMaxNotes) {
        ++dropped_notes;
        return;
    }
    notes.push_back(Diagnostic{level, std::move(text)});
}

void ImportDiagnostics::tally(std::string_view type, std::uint64_t read_count,
                              std::uint64_t skipped_count, std::uint64_t degraded_count)
{
    // Kept sorted by name: the census is printed, and two imports of the same
    // file must print the same sentence (core.md P11 — no hash-order iteration).
    auto at = std::lower_bound(
        types.begin(), types.end(), type,
        [](const TypeTally& row, std::string_view name) { return row.type < name; });
    if (at == types.end() || at->type != type) {
        TypeTally fresh;
        fresh.type = std::string(type);
        at         = types.insert(at, std::move(fresh));
    }
    at->read += read_count;
    at->skipped += skipped_count;
    at->degraded += degraded_count;
}

void ImportDiagnostics::merge(const ImportDiagnostics& other)
{
    for (const TypeTally& row : other.types)
        tally(row.type, row.read, row.skipped, row.degraded);
    if (unit_source == UnitSource::Unknown) {
        unit          = other.unit;
        unit_source   = other.unit_source;
        declared_unit = other.declared_unit;
    }
    paper_space_skipped += other.paper_space_skipped;
    skipped += other.skipped;
    if (skipped_reason.empty()) skipped_reason = other.skipped_reason;
    for (const Diagnostic& d : other.notes)
        note(d.level, d.text);
    dropped_notes += other.dropped_notes;
}

bool ImportDiagnostics::empty() const noexcept
{
    // The unit line is silent only in the one case that needs no telling: the
    // header agrees with the setting and both say metres.
    const bool unit_quiet =
        unit_source != UnitSource::Setting ||
        (declared_unit.has_value() && *declared_unit == unit && unit == core::DrawingUnit::Metre);
    return types.empty() && notes.empty() && paper_space_skipped == 0 && skipped == 0 &&
           dropped_notes == 0 && unit_quiet;
}

std::string ImportDiagnostics::type_summary() const
{
    // Three groups, each the eight biggest by count and a tail count for the
    // rest. Sorting is by count, then by name, so equal counts print stably.
    const auto group = [this](const char* heading, auto pick) {
        std::vector<const TypeTally*> rows;
        for (const TypeTally& row : types)
            if (pick(row) > 0) rows.push_back(&row);
        if (rows.empty()) return std::string();

        std::sort(rows.begin(), rows.end(), [&pick](const TypeTally* a, const TypeTally* b) {
            if (pick(*a) != pick(*b)) return pick(*a) > pick(*b);
            return a->type < b->type;
        });

        std::string out         = heading;
        const std::size_t shown = std::min<std::size_t>(rows.size(), 8);
        for (std::size_t i = 0; i < shown; ++i) {
            if (i != 0) out += ", ";
            out += rows[i]->type + " " + std::to_string(pick(*rows[i]));
        }
        if (rows.size() > shown) out += " (+" + std::to_string(rows.size() - shown) + " tür)";
        return out;
    };

    std::string out;
    const auto append = [&out](std::string part) {
        if (part.empty()) return;
        if (!out.empty()) out += "; ";
        out += std::move(part);
    };
    append(group("Okunan türler: ", [](const TypeTally& r) { return r.read; }));
    append(group("parçalanan: ", [](const TypeTally& r) { return r.degraded; }));
    append(group("atlanan: ", [](const TypeTally& r) { return r.skipped; }));
    return out;
}

std::vector<Diagnostic> ImportDiagnostics::lines() const
{
    std::vector<Diagnostic> out = notes;

    // THE FIELDS, rendered after the notes so the cap cannot hide them.
    //
    // THE UNIT. The project setting decided; what the file's header said stands
    // beside it. A disagreement is the one case that is a WARNING, and it names
    // the exact command that reads the file the other way — the setting's three
    // choices are millimetre, centimetre and metre, so a header naming anything
    // else is told the file itself has to change.
    if (unit_source == UnitSource::Setting) {
        const std::string used = core::drawing_unit_name(unit);
        if (declared_unit.has_value() && *declared_unit != unit) {
            const std::string said = core::drawing_unit_name(*declared_unit);
            const bool settable    = static_cast<std::uint8_t>(*declared_unit) <= 2;
            out.push_back(Diagnostic{
                Severity::Warning,
                "Çizim " + used + " olarak okundu (AYAR çizim_birimi); dosya başlığı " + said +
                    " diyor. Sayılar gerçekten " + said +
                    (settable ? " ise GERİAL ile geri alın, AYAR çizim_birimi " + said +
                                    " deyin ve yeniden İÇEAKTAR."
                              : " ise bu sürüm o birimi çizim birimi olarak sunmuyor; dosyayı "
                                "kaynağında metreye çevirip yeniden aktarın.")});
        } else if (!declared_unit.has_value()) {
            out.push_back(Diagnostic{Severity::Info,
                                     "Dosya birim bildirmiyor; çizim " + used +
                                         " olarak okundu (AYAR çizim_birimi). Yanlışsa GERİAL "
                                         "ile geri alın, AYAR çizim_birimi ile doğrusunu kurun "
                                         "ve yeniden İÇEAKTAR."});
        } else if (unit != core::DrawingUnit::Metre) {
            out.push_back(Diagnostic{Severity::Info,
                                     "Çizim " + used +
                                         " olarak okundu (AYAR çizim_birimi); dosya başlığı da "
                                         "öyle diyor. Koordinatlar milimetreye ölçeklendi."});
        }
    }

    if (paper_space_skipped != 0)
        out.push_back(Diagnostic{Severity::Skipped,
                                 std::to_string(paper_space_skipped) +
                                     " öğe kâğıt alanında (layout) olduğu için atlandı; antet "
                                     "ve pafta çerçevesi çizim değildir."});

    if (skipped != 0)
        out.push_back(Diagnostic{
            Severity::Skipped,
            std::to_string(skipped) +
                " öğe geometrisi kullanılamadığı için atlandı. İlki: " + skipped_reason});

    if (const std::string census = type_summary(); !census.empty())
        out.push_back(Diagnostic{Severity::Info, census});

    return out;
}

std::vector<Diagnostic> ImportDiagnostics::ordered() const
{
    std::vector<Diagnostic> out = lines();
    // Most serious first; `stable_sort` keeps emission order inside a level.
    const auto rank = [](Severity s) {
        switch (s) {
        case Severity::Error: return 0;
        case Severity::Warning: return 1;
        case Severity::Degraded: return 2;
        case Severity::Skipped: return 3;
        case Severity::Info: return 4;
        }
        return 4;
    };
    std::stable_sort(out.begin(), out.end(), [&rank](const Diagnostic& a, const Diagnostic& b) {
        return rank(a.level) < rank(b.level);
    });
    return out;
}

std::string ImportDiagnostics::transcript() const
{
    std::string out;
    for (const Diagnostic& d : lines()) {
        out += "\n  ";
        out += severity_prefix(d.level);
        out += ": ";
        out += d.text;
    }
    if (dropped_notes != 0) out += "\n  … ve " + std::to_string(dropped_notes) + " not daha.";
    return out;
}

} // namespace kentos::io
