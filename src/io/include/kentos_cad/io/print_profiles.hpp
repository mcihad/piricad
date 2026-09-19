// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — io: print profiles, the sheets a drawing is put on.
//
// A PROFILE is a named sheet: paper (or a custom size), orientation, resolution
// and margin. The office keeps a handful — "A3 yatay 300 dpi" for the pafta,
// "A4 dikey" for the krokis — and exactly one is the default the toolbar's plain
// YAZDIR uses. They are APPLICATION state (model.md R39): kept in the user's
// configuration directory as JSON, never in the document, never in its hash.
//
// Qt-free on purpose. The application's print service owns the file's location
// and the Qt printing; this module owns the shape, the paper table and the
// JSON, so a test can prove the store without a window (io.md R3).
#pragma once

#include "kentos_cad/command/bus.hpp"
#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::io {

/// One named sheet. Dimensions are the PORTRAIT ones; `landscape` turns them
/// when the sheet is used.
struct PrintProfile
{
    std::string name;            ///< what the user calls it; unique under Turkish folding
    std::string paper{"A4"};     ///< `A5`…`A0`, or `ozel`
    std::int64_t width_mm{210};  ///< the paper's portrait width
    std::int64_t height_mm{297}; ///< the paper's portrait height
    bool landscape{false};       ///< yatay
    std::int64_t dpi{300};       ///< output resolution
    std::int64_t margin_mm{10};  ///< the same on all four sides

    /// The sheet as it is used: width and height after the turn.
    std::int64_t sheet_width_mm() const noexcept { return landscape ? height_mm : width_mm; }

    std::int64_t sheet_height_mm() const noexcept { return landscape ? width_mm : height_mm; }

    /// The printable area: the sheet minus the margin on every side.
    std::int64_t printable_width_mm() const noexcept { return sheet_width_mm() - 2 * margin_mm; }

    std::int64_t printable_height_mm() const noexcept { return sheet_height_mm() - 2 * margin_mm; }

    friend bool operator==(const PrintProfile&, const PrintProfile&) = default;
};

/// One ISO 216 paper size, portrait, in millimetres. The table this program
/// knows: `A5`…`A0`. Nothing for an unknown name or for `ozel`, whose size the
/// user gives.
std::optional<std::pair<std::int64_t, std::int64_t>> paper_size_mm(std::string_view paper);

/// The paper names, in the order a list shows them, `ozel` last.
std::span<const char* const> paper_names();

/// One line describing a profile: `A3 Yatay — A3 420×297 mm, yatay, 300 dpi, kenar 10 mm`.
std::string describe_print_profile(const PrintProfile& p);

/// The store: every profile and which one is the default.
class PrintProfiles
{
public:
    /// The profiles a fresh installation has: A4 dikey (the default), A4 yatay,
    /// A3 yatay, A2 yatay, A1 yatay, A0 yatay, all at 300 dpi with a 10 mm margin.
    static PrintProfiles builtin();

    /// Reads the store from a JSON file. A missing file is the builtin store;
    /// a file that does not parse is an error naming the reason.
    static core::Result<PrintProfiles> load(const std::string& path);

    /// Writes the store as pretty JSON; creates the file, never a directory.
    core::Status save(const std::string& path) const;

    /// Parses the JSON text the file holds; the inverse of `to_json`.
    static core::Result<PrintProfiles> from_json(std::string_view text);
    std::string to_json() const;

    std::span<const PrintProfile> all() const noexcept { return profiles_; }

    bool empty() const noexcept { return profiles_.empty(); }

    /// The profile of that name, Turkish-folded, or null.
    const PrintProfile* find(std::string_view name) const;

    /// The default profile, or null when the store is empty.
    const PrintProfile* fallback() const;

    const std::string& default_name() const noexcept { return default_; }

    /// Adds `p`, or replaces the profile of the same name in place. Refuses an
    /// empty name, an unknown paper, a custom paper without a size, a
    /// resolution outside 72–4800 or a margin that leaves no printable area.
    core::Status upsert(PrintProfile p);

    /// Removes the profile of that name. Refuses the last profile — a program
    /// with no sheet to print on is a program whose YAZDIR does nothing — and
    /// moves the default to the first remaining one when the default went.
    core::Status remove(std::string_view name);

    /// Makes the profile of that name the default. Refuses an unknown name.
    core::Status set_default(std::string_view name);

    /// A profile made from a request: the named profile (or the default) with
    /// every field the request gives laid over it. Refuses an unknown profile
    /// name, and an unknown paper.
    core::Result<PrintProfile> resolve(const command::PrintRequest& request) const;

    /// The Turkish listing `YAZDIRMAPROFİLİ listele` prints, one profile per
    /// line, the default marked.
    std::string listing() const;

private:
    std::vector<PrintProfile> profiles_;
    std::string default_;
};

} // namespace kentos::io
