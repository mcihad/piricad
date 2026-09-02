// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/render/text_atlas.hpp"

#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_OUTLINE_H
#include FT_TRUETYPE_TABLES_H

#include <hb-ft.h>
#include <hb.h>

#include <msdfgen.h>

#define STB_RECT_PACK_IMPLEMENTATION
#include <stb_rect_pack.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <unordered_map>

namespace kentos::render {
namespace {

using core::ErrorCode;

/// How many atlas pixels one EM occupies.
///
/// 48, and the number is a trade rather than a preference. An MSDF's sharpness
/// comes from the DISTANCE FIELD, not the resolution, so this only has to be big
/// enough that a glyph's own corners survive being sampled — below about 32 the
/// dot of an `i` and the cedilla of a `ş` start to merge with the stem. 48 fits
/// the five faces of `design.md` §3 into one 2048 texture with room left.
constexpr int kGlyphPx = 48;

/// The distance range, in atlas pixels.
///
/// This is what the shader divides by, and it is the width of the band in which
/// the field carries usable information. Too small and a glyph scaled up shows
/// the field's own quantisation as a ripple along the stem; too large and the
/// corners of `Ç` round off because neighbouring edges start to influence each
/// other. Four is msdfgen's own recommendation and holds from 8 px to 200.
constexpr double kPxRange = 4.0;

/// The atlas starts here and doubles. 1024 holds the Latin-1 and Turkish letters
/// of all five faces; a drawing full of unusual captions grows it once.
constexpr int kInitialSide = 1024;
constexpr int kMaxSide     = 4096;

/// The five faces, in `Face` order. `data/fonts` is a PERMIT-recorded directory
/// (`data.md`), and the file names are the ones that ship.
constexpr std::array<const char*, 5> kFaceFiles{
    "IBMPlexSans-Regular.ttf", "IBMPlexSans-Medium.ttf", "IBMPlexSans-SemiBold.ttf",
    "IBMPlexMono-Regular.ttf", "IBMPlexMono-Medium.ttf",
};

/// FreeType hands an outline over as a sequence of move/line/conic/cubic calls.
/// This turns those into an `msdfgen::Shape` in EM units.
///
/// A context struct rather than a lambda capture, because FreeType's decomposer
/// takes C function pointers.
struct OutlineSink
{
    msdfgen::Shape* shape{nullptr};
    msdfgen::Contour* contour{nullptr};
    msdfgen::Point2 pen{};
    double scale{1.0}; ///< 1 / units_per_EM

    msdfgen::Point2 at(const FT_Vector* v) const
    {
        return msdfgen::Point2(static_cast<double>(v->x) * scale,
                               static_cast<double>(v->y) * scale);
    }
};

int move_to(const FT_Vector* to, void* user)
{
    auto* s = static_cast<OutlineSink*>(user);
    // An EMPTY contour is left behind by a `moveTo` that no segment follows, and
    // msdfgen's colouring divides by its edge count. Fonts do produce them.
    if (s->contour != nullptr && s->contour->edges.empty()) s->shape->contours.pop_back();
    s->contour = &s->shape->addContour();
    s->pen     = s->at(to);
    return 0;
}

int line_to(const FT_Vector* to, void* user)
{
    auto* s                     = static_cast<OutlineSink*>(user);
    const msdfgen::Point2 point = s->at(to);
    if (s->contour != nullptr && point != s->pen) {
        s->contour->addEdge(msdfgen::EdgeHolder(s->pen, point));
        s->pen = point;
    }
    return 0;
}

int conic_to(const FT_Vector* control, const FT_Vector* to, void* user)
{
    auto* s                     = static_cast<OutlineSink*>(user);
    const msdfgen::Point2 point = s->at(to);
    if (s->contour != nullptr && point != s->pen) {
        s->contour->addEdge(msdfgen::EdgeHolder(s->pen, s->at(control), point));
        s->pen = point;
    }
    return 0;
}

int cubic_to(const FT_Vector* c1, const FT_Vector* c2, const FT_Vector* to, void* user)
{
    auto* s                     = static_cast<OutlineSink*>(user);
    const msdfgen::Point2 point = s->at(to);
    if (s->contour != nullptr && point != s->pen) {
        s->contour->addEdge(msdfgen::EdgeHolder(s->pen, s->at(c1), s->at(c2), point));
        s->pen = point;
    }
    return 0;
}

} // namespace

// -----------------------------------------------------------------------------

struct TextAtlas::Impl
{
    struct FaceSlot
    {
        FT_Face ft{nullptr};
        hb_font_t* hb{nullptr};
        double upem{1000.0};
        float ascent{0.0f};
        float descent{0.0f};
        float cap{0.0f};
    };

    FT_Library library{nullptr};
    std::array<FaceSlot, 5> faces{};

    std::vector<GlyphBox> boxes;

    /// (face, glyph id) -> index into `boxes`. Keyed on a packed 64-bit word
    /// rather than a pair, so the lookup is one hash of one integer — this runs
    /// once per character of every caption on the sheet.
    std::unordered_map<std::uint64_t, std::uint32_t> seen;

    std::vector<std::uint8_t> pixels;
    int side{kInitialSide};
    std::uint64_t revision{1};

    std::vector<stbrp_node> nodes;
    stbrp_context packer{};

    /// Scratch for shaping, kept between calls so the draw loop allocates nothing.
    hb_buffer_t* buffer{nullptr};
    hb_language_t turkish{nullptr};

    ~Impl()
    {
        if (buffer != nullptr) hb_buffer_destroy(buffer);
        for (FaceSlot& slot : faces) {
            if (slot.hb != nullptr) hb_font_destroy(slot.hb);
            if (slot.ft != nullptr) FT_Done_Face(slot.ft);
        }
        if (library != nullptr) FT_Done_FreeType(library);
    }

    void reset_packer()
    {
        nodes.assign(static_cast<std::size_t>(side), stbrp_node{});
        stbrp_init_target(&packer, side, side, nodes.data(), side);
        pixels.assign(static_cast<std::size_t>(side) * static_cast<std::size_t>(side) * 4u, 0);
    }

    /// Rasterises one glyph into the atlas and returns its table index.
    std::uint32_t intern(std::uint8_t face_index, std::uint32_t gid);

    /// Doubles the atlas and re-rasterises everything that was in it.
    bool grow();
};

// -----------------------------------------------------------------------------

std::uint32_t TextAtlas::Impl::intern(std::uint8_t face_index, std::uint32_t gid)
{
    const std::uint64_t key = (static_cast<std::uint64_t>(face_index) << 32) | gid;
    if (auto it = seen.find(key); it != seen.end()) return it->second;

    FaceSlot& slot = faces[face_index];

    GlyphBox out;
    out.blank = true;

    // NO_SCALE: font units, which is what the outline is stored in and what makes
    // the result independent of any pixel size (`render.md` R8, "scale
    // independent"). Hinting would be a per-size decision baked into a texture
    // that serves every size.
    if (FT_Load_Glyph(slot.ft, gid, FT_LOAD_NO_SCALE | FT_LOAD_NO_HINTING) == 0 &&
        slot.ft->glyph->format == FT_GLYPH_FORMAT_OUTLINE &&
        slot.ft->glyph->outline.n_contours > 0) {

        msdfgen::Shape shape;
        OutlineSink sink;
        sink.shape = &shape;
        sink.scale = 1.0 / slot.upem;

        FT_Outline_Funcs funcs{};
        funcs.move_to  = move_to;
        funcs.line_to  = line_to;
        funcs.conic_to = conic_to;
        funcs.cubic_to = cubic_to;

        if (FT_Outline_Decompose(&slot.ft->glyph->outline, &funcs, &sink) == 0) {
            if (!shape.contours.empty() && shape.contours.back().edges.empty())
                shape.contours.pop_back();

            if (!shape.contours.empty()) {
                shape.normalize();
                shape.inverseYAxis = false;

                // Edge colouring is what makes this MULTI-channel: each edge gets
                // one of three colours, and the shader takes the median of the
                // three distances. That median is what preserves a sharp corner —
                // a single-channel field rounds every corner it has.
                msdfgen::edgeColoringSimple(shape, 3.0);

                const msdfgen::Shape::Bounds bounds = shape.getBounds();
                const double range                  = kPxRange / kGlyphPx;

                const double x0 = bounds.l - range;
                const double y0 = bounds.b - range;
                const double x1 = bounds.r + range;
                const double y1 = bounds.t + range;

                const int w = static_cast<int>(std::ceil((x1 - x0) * kGlyphPx));
                const int h = static_cast<int>(std::ceil((y1 - y0) * kGlyphPx));

                if (w > 0 && h > 0 && w < side && h < side) {
                    // One pixel of padding, so two neighbours in the atlas cannot
                    // bleed into each other when the sampler filters.
                    stbrp_rect rect{};
                    rect.w = static_cast<stbrp_coord>(w + 2);
                    rect.h = static_cast<stbrp_coord>(h + 2);

                    if (stbrp_pack_rects(&packer, &rect, 1) == 0) {
                        if (!grow()) return 0;
                        return intern(face_index, gid);
                    }

                    msdfgen::Bitmap<float, 3> bitmap(w, h);
                    const msdfgen::SDFTransformation transform(
                        msdfgen::Projection(msdfgen::Vector2(kGlyphPx, kGlyphPx),
                                            msdfgen::Vector2(-x0, -y0)),
                        msdfgen::DistanceMapping(msdfgen::Range(range)));
                    msdfgen::generateMSDF(bitmap, shape, transform);

                    const int px = rect.x + 1;
                    const int py = rect.y + 1;
                    for (int y = 0; y < h; ++y) {
                        for (int x = 0; x < w; ++x) {
                            const float* src = bitmap(x, y);
                            // The atlas is top-down and the field is bottom-up.
                            const std::size_t at = (static_cast<std::size_t>(py + (h - 1 - y)) *
                                                        static_cast<std::size_t>(side) +
                                                    static_cast<std::size_t>(px + x)) *
                                                   4u;
                            for (int c = 0; c < 3; ++c) {
                                const float v = std::clamp(src[c] * 255.0f + 0.5f, 0.0f, 255.0f);
                                pixels[at + static_cast<std::size_t>(c)] =
                                    static_cast<std::uint8_t>(v);
                            }
                            pixels[at + 3] = 255;
                        }
                    }

                    const float inv = 1.0f / static_cast<float>(side);
                    out.u0          = static_cast<float>(px) * inv;
                    out.v0          = static_cast<float>(py) * inv;
                    out.u1          = static_cast<float>(px + w) * inv;
                    out.v1          = static_cast<float>(py + h) * inv;
                    out.left        = static_cast<float>(x0);
                    out.bottom      = static_cast<float>(y0);
                    out.right       = static_cast<float>(x1);
                    out.top         = static_cast<float>(y1);
                    out.blank       = false;

                    ++revision;
                }
            }
        }
    }

    const auto index = static_cast<std::uint32_t>(boxes.size());
    boxes.push_back(out);
    seen.emplace(key, index);
    return index;
}

bool TextAtlas::Impl::grow()
{
    if (side >= kMaxSide) return false;

    side *= 2;
    reset_packer();

    // Everything that was interned is rasterised again into the bigger sheet. The
    // TABLE INDICES must not move: a `PlacedGlyph` from an earlier frame names a
    // box by index, so the boxes are rewritten in place and nothing above this
    // notices except through `revision`.
    const std::unordered_map<std::uint64_t, std::uint32_t> again = seen;
    seen.clear();

    for (const auto& [key, index] : again) {
        const auto face_index = static_cast<std::uint8_t>(key >> 32);
        const auto gid        = static_cast<std::uint32_t>(key & 0xFFFFFFFFu);

        const std::uint32_t fresh = intern(face_index, gid);
        if (fresh != index) {
            boxes[index] = boxes[fresh];
            seen[key]    = index;
        }
    }

    ++revision;
    return true;
}

// -----------------------------------------------------------------------------

TextAtlas::TextAtlas() : impl_(std::make_unique<Impl>()) {}

TextAtlas::~TextAtlas() = default;

core::Result<std::unique_ptr<TextAtlas>> TextAtlas::open(const std::string& font_dir)
{
    std::unique_ptr<TextAtlas> atlas(new TextAtlas());
    Impl& impl = *atlas->impl_;

    if (FT_Init_FreeType(&impl.library) != 0)
        return core::err(ErrorCode::Internal, "FreeType başlatılamadı.");

    for (std::size_t i = 0; i < kFaceFiles.size(); ++i) {
        const std::string path = font_dir + "/" + kFaceFiles[i];

        Impl::FaceSlot& slot = impl.faces[i];
        if (FT_New_Face(impl.library, path.c_str(), 0, &slot.ft) != 0)
            return core::err(ErrorCode::IoFailure,
                             "Yazı tipi açılamadı: '" + path +
                                 "'. Beş IBM Plex yüzü programla birlikte gelir; "
                                 "eksikse paketleme kusurudur.");

        slot.upem = slot.ft->units_per_EM > 0 ? static_cast<double>(slot.ft->units_per_EM) : 1000.0;
        slot.ascent  = static_cast<float>(static_cast<double>(slot.ft->ascender) / slot.upem);
        slot.descent = static_cast<float>(static_cast<double>(slot.ft->descender) / slot.upem);

        // The OS/2 table's own cap height when the font declares one, and 0.7 EM
        // when it does not. IBM Plex declares it; the fallback is what the metric
        // is worth on a face that does not, and it is better than centring a
        // parcel number on its ascender.
        const auto* os2 = static_cast<TT_OS2*>(FT_Get_Sfnt_Table(slot.ft, FT_SFNT_OS2));
        slot.cap        = (os2 != nullptr && os2->version >= 2 && os2->sCapHeight > 0)
                              ? static_cast<float>(static_cast<double>(os2->sCapHeight) / slot.upem)
                              : 0.7f;

        slot.hb = hb_ft_font_create_referenced(slot.ft);
        if (slot.hb == nullptr)
            return core::err(ErrorCode::Internal, "HarfBuzz yüzü oluşturulamadı: '" + path + "'");

        // Advances come back in FONT UNITS, which is what the boxes are in. The
        // alternative is 26.6 fixed point at some pixel size, and then every
        // measurement in this file would depend on a size the atlas does not have.
        const auto upem = static_cast<int>(slot.upem);
        hb_font_set_scale(slot.hb, upem, upem);
    }

    impl.buffer  = hb_buffer_create();
    impl.turkish = hb_language_from_string("tr", -1);
    impl.reset_packer();

    return atlas;
}

RunMetrics TextAtlas::shape(Face face, std::string_view utf8, std::vector<PlacedGlyph>& out)
{
    Impl& impl                 = *impl_;
    const auto face_index      = static_cast<std::uint8_t>(face);
    const Impl::FaceSlot& slot = impl.faces[face_index];

    RunMetrics metrics;
    metrics.ascent  = slot.ascent;
    metrics.descent = slot.descent;
    metrics.cap     = slot.cap;
    if (utf8.empty() || slot.hb == nullptr) return metrics;

    hb_buffer_clear_contents(impl.buffer);
    hb_buffer_add_utf8(impl.buffer, utf8.data(), static_cast<int>(utf8.size()), 0,
                       static_cast<int>(utf8.size()));
    hb_buffer_set_direction(impl.buffer, HB_DIRECTION_LTR);
    hb_buffer_set_script(impl.buffer, HB_SCRIPT_LATIN);

    // TURKISH, declared rather than guessed. The language selects the font's own
    // locale-specific features, and the one this alphabet needs is the dotted and
    // dotless i: a font that ships `locl` rules for `tr` maps them here and
    // nowhere else (CLAUDE.md 5.6 is the same rule one layer down).
    hb_buffer_set_language(impl.buffer, impl.turkish);

    hb_shape(slot.hb, impl.buffer, nullptr, 0);

    unsigned int count             = 0;
    const hb_glyph_info_t* infos   = hb_buffer_get_glyph_infos(impl.buffer, &count);
    const hb_glyph_position_t* pos = hb_buffer_get_glyph_positions(impl.buffer, &count);

    const auto to_em = static_cast<float>(1.0 / slot.upem);

    float pen_x = 0.0f;
    float pen_y = 0.0f;
    for (unsigned int i = 0; i < count; ++i) {
        const std::uint32_t index = impl.intern(face_index, infos[i].codepoint);

        if (!impl.boxes[index].blank) {
            PlacedGlyph glyph;
            glyph.box = index;
            glyph.x   = pen_x + static_cast<float>(pos[i].x_offset) * to_em;
            glyph.y   = pen_y + static_cast<float>(pos[i].y_offset) * to_em;
            out.push_back(glyph);
        }

        pen_x += static_cast<float>(pos[i].x_advance) * to_em;
        pen_y += static_cast<float>(pos[i].y_advance) * to_em;
    }

    metrics.advance = pen_x;
    return metrics;
}

const GlyphBox& TextAtlas::box(std::uint32_t index) const
{
    static const GlyphBox kNone{};
    return index < impl_->boxes.size() ? impl_->boxes[index] : kNone;
}

const std::vector<std::uint8_t>& TextAtlas::pixels() const noexcept
{
    return impl_->pixels;
}

int TextAtlas::width() const noexcept
{
    return impl_->side;
}

int TextAtlas::height() const noexcept
{
    return impl_->side;
}

float TextAtlas::px_range() const noexcept
{
    return static_cast<float>(kPxRange);
}

std::uint64_t TextAtlas::revision() const noexcept
{
    return impl_->revision;
}

} // namespace kentos::render
