// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — render: the SDF glyph atlas (`.claude/render.md` R8, kentoscad.md §9.4).
//
// ONE TEXTURE, SCALE INDEPENDENT. A cadastral sheet's captions are drawn at every
// zoom between 1:200 and 1:25 000, and a bitmap atlas is right at exactly one of
// them. A multi-channel signed distance field is a picture of the OUTLINE rather
// than of the pixels, so one 32 px cell draws a sharp glyph at 8 px and at 200,
// and every caption on the sheet comes from one texture and one draw call.
//
// WHY THREE LIBRARIES AND NOT ONE. They do three different jobs and none of them
// does another's (§9.4):
//
//   FreeType   reads the font and hands over the glyph OUTLINE in font units.
//   HarfBuzz   turns a Turkish string into positioned glyph ids — which is not
//              the same as mapping characters to glyphs. `ğ`, `ş`, `ı` and the
//              dotted capital `İ` are ordinary letters in this alphabet, and the
//              kerning that makes `TAKS` readable lives in the font's tables.
//   msdfgen    turns one outline into the distance field.
//
// QT-FREE, like everything else under /src/render (CLAUDE.md 3.4). This class
// produces PIXELS and NUMBERS; uploading them to a texture is the backend's job,
// which is what keeps the atlas testable in a suite that links no Qt.
//
// Compiled only when `KENTOS_WITH_TEXT=ON`.
#pragma once

#include "kentos_cad/core/result.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kentos::render {

/// The faces the shell draws with, from `data/fonts`.
///
/// An enum and not a font name, because a backend must not be able to ask for a
/// face that is not there: `design.md` §3 fixes five faces and every one of them
/// ships with the program, so a missing font is a packaging failure to report at
/// start-up rather than a silent substitution at draw time.
enum class Face : std::uint8_t {
    Sans,         ///< body text
    SansMedium,   ///< a panel heading
    SansSemiBold, ///< a dialog title
    Mono,         ///< every NUMBER — a coordinate, a ruler division, a measurement
    MonoMedium,   ///< a number that carries emphasis
};

/// Where one glyph sits in the atlas, and where its quad sits around the pen.
///
/// EM UNITS for the quad, texture coordinates for the atlas. The caller scales by
/// the pixel height it wants; that is what makes one cell serve every zoom.
struct GlyphBox
{
    float u0{0.0f}, v0{0.0f}; ///< top-left in the atlas, 0..1
    float u1{0.0f}, v1{0.0f}; ///< bottom-right in the atlas, 0..1

    /// The quad around the pen, in EM, y UP — `left` is usually slightly negative
    /// because the distance field is wider than the outline by its own range.
    float left{0.0f}, bottom{0.0f}, right{0.0f}, top{0.0f};

    /// Whitespace: advance the pen and draw nothing. A space has no outline, and a
    /// zero-area quad through the pipeline is a draw call that renders nothing.
    bool blank{true};
};

/// One shaped glyph, positioned along the run.
struct PlacedGlyph
{
    std::uint32_t box{0};   ///< index into the atlas' glyph table
    float x{0.0f}, y{0.0f}; ///< pen position in EM from the run's origin
};

/// The measurements of one shaped run, in EM.
struct RunMetrics
{
    float advance{0.0f}; ///< total pen movement; the run's width at 1 EM
    float ascent{0.0f};  ///< the face's ascender, for vertical centring
    float descent{0.0f}; ///< the face's descender, negative

    /// The height of a capital letter. What a label is centred on, because a
    /// caption of digits has no descenders and centring it on the ascender puts
    /// it visibly above the point it labels.
    float cap{0.0f};
};

class TextAtlas
{
public:
    /// Opens the five bundled faces from `font_dir` (`data/fonts`).
    ///
    /// Fails rather than substituting. A drawing whose captions came out in a
    /// different face than the one `design.md` names is a drawing that does not
    /// match the sheet beside it, and silently picking another font is how that
    /// happens without anyone noticing.
    static core::Result<std::unique_ptr<TextAtlas>> open(const std::string& font_dir);

    ~TextAtlas();

    TextAtlas(const TextAtlas&)            = delete;
    TextAtlas& operator=(const TextAtlas&) = delete;

    /// Shapes `utf8` and APPENDS its glyphs to `out`, rasterising any it has not
    /// seen. Returns the run's metrics in EM.
    ///
    /// Appends rather than returns, so a frame's worth of captions goes into one
    /// buffer the draw loop already owns — the draw loop must not allocate
    /// (`render.md` R20).
    RunMetrics shape(Face face, std::string_view utf8, std::vector<PlacedGlyph>& out);

    /// The box for a `PlacedGlyph::box`. Never out of range: indices come from
    /// `shape()` and the table only grows.
    const GlyphBox& box(std::uint32_t index) const;

    /// The atlas image: `width * height * 4` bytes, RGBA, three MSDF channels and
    /// an unused alpha. RGBA rather than RGB because every GPU API wants four-byte
    /// rows and packing three is a per-row copy on upload.
    const std::vector<std::uint8_t>& pixels() const noexcept;

    int width() const noexcept;
    int height() const noexcept;

    /// The distance range the field was generated with, in ATLAS PIXELS. The
    /// fragment shader needs it to turn a distance into a coverage, and getting it
    /// from anywhere but here is how a text pass comes out blurry at one zoom and
    /// jagged at another.
    float px_range() const noexcept;

    /// Bumped whenever `pixels()` changed. The backend re-uploads when this moves,
    /// and only then — the atlas fills up over the first few frames and is then
    /// static for the rest of the session.
    std::uint64_t revision() const noexcept;

private:
    TextAtlas();

    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace kentos::render
