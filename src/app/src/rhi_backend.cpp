// SPDX-License-Identifier: GPL-3.0-or-later
// KentOSCad — app: the QRhi canvas backend (kentoscad.md §6.3, CLAUDE.md Article 8.1).
//
// WHAT THIS IS. The GPU implementation of `render::Backend`. It consumes exactly
// the same `DrawList` and `Overlay` the QPainter backend consumes, so nothing
// above `render::Backend` was edited to make it exist — which is the claim
// Article 8.1 makes and the one render.md R1 exists to keep true.
//
// WHAT THIS DOES NOT DRAW YET. Published raster symbols, pattern fills and marker
// lines. Those are `handles()`'s job to refuse rather than claim: the QGIS backend
// once decided what it could draw by exclusion and silently dropped three raster
// types that way (`scripts/ci-gate-backends.sh` exists because of it), so this one
// lists what it CAN draw and refuses everything else.
//
// TEXT IS HERE when `KENTOS_WITH_TEXT=ON`, through the SDF atlas of render.md R8
// — msdfgen fields over FreeType outlines, shaped with HarfBuzz. Not QPainter:
// render.md P5 forbids Qt painting inside the QRhi path, and a second text
// renderer would disagree with the first about where a caption sits.
//
// COORDINATE SPACES, which are two and must not be confused (drawlist.hpp):
//   * document batches are CENTRE-RELATIVE with y UP — what the origin offset of
//     render.md R2 produces, and what a vertex buffer wants;
//   * overlay batches are WIDGET PIXELS with y DOWN — what a cursor position and
//     a viewport already are.
// Both become widget pixels here, once, on the way into the vertex buffer.
#include "kentos_cad/app/backend_factory.hpp"

#include "kentos_cad/render/drawlist.hpp"
#include "kentos_cad/render/symbology.hpp"

#include "kentos_cad/app/symbol_image.hpp"

#if KENTOS_HAVE_TEXT
#include "kentos_cad/app/data_root.hpp"
#include "kentos_cad/render/text_atlas.hpp"
#endif

#include <rhi/qrhi.h>

#include <QByteArray>
#include <QColor>
#include <QDebug>
#include <QFile>
#include <QImage>
#include <QMatrix4x4>
#include <QSize>
#include <QString>
#include <QtGlobal>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace kentos::app {
namespace {

/// The uniform block both pipelines declare, in std140 order.
///
/// One block per DRAW, addressed by a dynamic offset, because colour and width
/// change per batch and a buffer per batch would be an allocation per frame
/// (render.md R20).
struct Uniforms
{
    float mvp[16]{};   ///< pixels -> clip, clip-space correction folded in
    float colour[4]{}; ///< straight RGBA, 0..1

    /// x: half line width · y: the SDF distance range · z: the dash period in
    /// pixels · w: how many of `dash` are meaningful.
    float params[4]{};

    /// Mark and space lengths in pixels, alternating, mark first. Eight is what
    /// `PassStyle::dash_lengths` carries, which is enough for a dash-dot-dot.
    float dash[8]{};
};

static_assert(sizeof(Uniforms) == 128, "the shaders declare mat4 + vec4 + vec4 + vec4[2]");

/// Unit quad for the line pipeline: x runs along the segment, y picks the side.
/// Four vertices as a triangle strip, the cheapest quad there is.
constexpr float kLineCorners[8] = {
    0.0f, -1.0f, //
    0.0f, 1.0f,  //
    1.0f, -1.0f, //
    1.0f, 1.0f,  //
};

/// The em size an overlay label gets when it does not ask for one.
///
/// 12 px, which is `design.md` §3's body size. A label with `px == 0` means "the
/// backend's default UI font", and a backend that guessed differently from the
/// QPainter one would move the status readouts when the engine changed.
constexpr float kDefaultUiPx = 12.0f;

/// The unit quad for the text pipeline: 0..1 in both axes, as a triangle strip.
/// Distinct from `kLineCorners`, whose y runs -1..1 because a segment is widened
/// about its own centre while a glyph is placed from its corner.
constexpr float kQuadCorners[8] = {
    0.0f, 0.0f, //
    0.0f, 1.0f, //
    1.0f, 0.0f, //
    1.0f, 1.0f, //
};

constexpr double kPi = 3.14159265358979323846;

/// A colour with the symbol layer's opacity multiplied into its alpha.
///
/// A layer's opacity is not a second colour; it scales the one it has, and a
/// backend that ignored it would draw MPYY's translucent lekesi as an opaque
/// block over whatever it is meant to sit on.
std::uint32_t faded(std::uint32_t rgba, std::uint8_t opacity) noexcept
{
    if (rgba == 0 || opacity == 255) return rgba;
    const std::uint32_t a = ((rgba >> 24) & 0xFFu) * opacity / 255u;
    return (a << 24) | (rgba & 0x00FFFFFFu);
}

/// Straight RGBA (0xAARRGGBB) repacked so the BYTES read R, G, B, A in memory.
///
/// `UNormByte4` hands the shader the bytes in address order, and on a
/// little-endian machine a 0xAARRGGBB word is stored B, G, R, A — so a red label
/// arrives blue. Packing here rather than swizzling in the shader keeps the
/// fragment stage identical to every other pipeline's.
std::uint32_t rgba_bytes(std::uint32_t rgba) noexcept
{
    const std::uint32_t a = (rgba >> 24) & 0xFFu;
    const std::uint32_t r = (rgba >> 16) & 0xFFu;
    const std::uint32_t g = (rgba >> 8) & 0xFFu;
    const std::uint32_t b = rgba & 0xFFu;
    return (a << 24) | (b << 16) | (g << 8) | r;
}

/// Straight RGBA byte order matching `std::uint32_t` in the draw list: 0xAARRGGBB.
void unpack(std::uint32_t rgba, float out[4]) noexcept
{
    out[0] = static_cast<float>((rgba >> 16) & 0xFFu) / 255.0f;
    out[1] = static_cast<float>((rgba >> 8) & 0xFFu) / 255.0f;
    out[2] = static_cast<float>(rgba & 0xFFu) / 255.0f;
    out[3] = static_cast<float>((rgba >> 24) & 0xFFu) / 255.0f;
}

QShader load_shader(const char* path)
{
    QFile file(QString::fromLatin1(path));
    if (!file.open(QIODevice::ReadOnly)) return {};
    return QShader::fromSerialized(file.readAll());
}

/// One recorded draw. Built on the CPU in draw order, replayed inside one pass.
///
/// A flat POD in a vector that keeps its capacity between frames: the draw loop
/// must not allocate (render.md R20, P6), and after the first busy frame this one
/// does not.
struct Cmd
{
    enum class Kind : std::uint8_t {
        Fill,   ///< stencil the rings, then cover the bounding box with colour
        Mask,   ///< stencil the rings and stop — a pattern draws inside them next
        Unmask, ///< clear the mask with the cover quad, writing no colour
        Line,   ///< instanced segment quads
        Tri,    ///< a triangle list: marker interiors
        Marker, ///< ONE glyph, drawn once per stamp — see `marker.vert`
        Text,   ///< instanced glyph quads against the SDF atlas
        Image,  ///< instanced textured quads: a published picture
    };

    Kind kind{Kind::Line};

    /// Line and Tri: draw only where the mask is set.
    ///
    /// This is how a pattern fill is CLIPPED to its face, and the mask is the
    /// same even-odd stencil a solid fill uses. A hatch that was not clipped
    /// would run across the whole bounding box — over the neighbouring parcel
    /// and out into the sea.
    bool clipped{false};
    std::uint32_t uniform{0}; ///< slot in the uniform buffer

    /// Fill: the first VERTEX. Line and Text: the BYTE OFFSET of the first
    /// instance in its buffer.
    ///
    /// A byte offset and not a `firstInstance`, and the difference is not a style
    /// choice. `firstInstance` needs `QRhi::BaseInstance`, which OpenGL ES and
    /// plain GL without ARB_base_instance do not have — and where it is missing
    /// the draw is silently wrong rather than refused. That is exactly what it
    /// looked like: the first stroke batch of the frame drew and every one after
    /// it vanished, so the grid came out half there and the drawing not at all.
    /// Binding the vertex buffer at an offset is the portable way to say the same
    /// thing and needs no feature at all.
    std::uint32_t first{0};
    std::uint32_t count{0}; ///< vertices (Fill) or instances (Line, Text, Image)
    std::uint32_t cover{0}; ///< Fill only: first vertex of its 6-vertex cover quad

    /// Image only: which picture, as an index into this frame's texture list plus
    /// one. Zero means "no picture", which is every other kind.
    std::uint32_t image{0};

    /// Marker only: the glyph's own geometry, uploaded once for the whole pass.
    /// `first` and `count` carry the INSTANCES, as they do for Line and Text.
    std::uint32_t glyph_first{0}; ///< first vertex in the marker vertex buffer
    std::uint32_t glyph_count{0}; ///< how many vertices that glyph has
};

class RhiBackend final : public render::Backend
{
public:
    /// What the status strip says this canvas is — and, when it matters, what it
    /// is NOT.
    ///
    /// THE TEXT ATLAS IS OPTIONAL AND ITS ABSENCE IS INVISIBLE. Every caption in
    /// this file is drawn behind `KENTOS_HAVE_TEXT`, so a build without it opens
    /// a plan sheet correctly, at full speed, and silently without the 13 112 ada
    /// and parsel numbers on it. That is the worst shape a missing option can
    /// take: nothing is broken, nothing is slow, and what the surveyor came for is
    /// simply not there. The strip they are already looking at says so.
    std::string name() const override
    {
#if KENTOS_HAVE_TEXT
        return "QRhi (GPU · geometri · yazı)";
#else
        return "QRhi (GPU · geometri · YAZISIZ)";
#endif
    }

private:
    /// The frame's visible rectangle in logical pixels, plus a margin.
    ///
    /// Held rather than threaded through every emitter because the pattern
    /// generators are the only things that need it and they sit five calls deep.
    /// Set once at the top of `render()`; an empty box means "not known", and the
    /// generators then cover the whole face as they always did.
    render::PixelBox visible_{};

    /// `box` grown by `reach` on every side.
    ///
    /// A glyph is placed by its CENTRE, so a mark whose centre sits just outside
    /// the screen can still have half of itself inside it. Growing by the glyph's
    /// own size is what keeps the edge of the view from eating them.
    static render::PixelBox grown(const render::PixelBox& box, double reach)
    {
        if (box.empty()) return box;
        const auto r = static_cast<float>(std::max(0.0, reach));
        return render::PixelBox{box.min_x - r, box.min_y - r, box.max_x + r, box.max_y + r};
    }

public:
    bool gpu() const override { return true; }

    render::FrameStats stats() const override { return stats_; }

    void render(const render::DrawList& list, const render::Overlay& overlay,
                const render::FrameContext& ctx) override;

    /// Which symbol layer types this slice draws. A WHITELIST, and the `default`
    /// refuses — see the file header and `scripts/ci-gate-backends.sh`.
    static bool handles(core::SymbolLayerType type);

private:
    // ---- frame building ----------------------------------------------------

    /// The overlay's GROUND: the grid, which is the paper the drawing sits on and
    /// goes down before any pass (`Overlay::beneath`).
    void paint_ground(const render::Overlay& overlay);

    /// Everything over the document: selection, rubber band, snap glyph,
    /// crosshair, ruler lines. Captions are text and wait on render.md R8.
    void paint_aids(const render::DrawList& list, const render::Overlay& overlay);

    void emit_document(const render::DrawList& list, double cx, double cy);
    void emit_overlay(const render::Overlay& overlay, std::size_t from, std::size_t to);

    // ---- published symbology (`symbology.hpp`) ------------------------------

    /// A solid face, or one ring set stencilled as a MASK for a pattern to draw
    /// inside. Returns false when the batch had nothing to stencil.
    bool emit_face(const render::PolygonBatch& batch, double cx, double cy, std::uint32_t rgba,
                   bool as_mask, float box[4]);

    /// Clears a mask left by `emit_face(..., as_mask=true)`.
    void emit_unmask(const float box[4]);

    /// One glyph at every stamp: interior triangles and outline segments.
    ///
    /// `extra_cos`/`extra_sin` is the pass's own rotation, composed with each
    /// stamp's direction — a `çizik` asked to lie ACROSS a circle carries its
    /// angle here while the stamp carries the line's.
    void emit_stamps(const render::MarkerOutline& glyph, const render::PassStyle& ps,
                     float extra_cos, float extra_sin, bool clipped);

    /// `isaretci-cizgi` and `tarak-cizgi`: glyphs placed along the geometry.
    void emit_marker_line(const render::PolylineBatch& batch, const render::PassStyle& ps,
                          double cx, double cy, bool hash);

    /// The default disc on every LONE VERTEX in the batch.
    ///
    /// A point outlines to one vertex and a stroke through one vertex draws
    /// nothing, so without this a placed NOKTA is invisible on the GPU exactly as
    /// it was on the other two backends. Same disc, same size constant, because a
    /// point must not change shape when the engine changes.
    void emit_point_dots(const render::PolylineBatch& batch, const render::PassStyle& ps, double cx,
                         double cy);

    /// `cizgi-desen-dolgu`: parallel lines clipped to the face.
    void emit_line_pattern(const render::PolygonBatch& batch, const render::PassStyle& ps,
                           double cx, double cy);

    /// `nokta-desen-dolgu`: a grid of glyphs clipped to the face.
    void emit_point_pattern(const render::PolygonBatch& batch, const render::PassStyle& ps,
                            double cx, double cy);

    /// One glyph at the centre of each face.
    void emit_centroid(const render::PolygonBatch& batch, const render::PassStyle& ps, double cx,
                       double cy);

    // ---- published pictures (`symbol_image.hpp`) ----------------------------

    /// Decodes `ps`'s picture at `wanted_px` tall and returns its slot plus one,
    /// or zero when there is nothing readable. Touches no GPU object: the texture
    /// is created and uploaded later, inside `ensure_capacity`.
    std::uint32_t picture_slot(const render::PassStyle& ps, int wanted_px);

    /// Appends one textured quad per stamp, and the command that draws them.
    void emit_picture_stamps(std::uint32_t slot, float width, float height,
                             const render::PassStyle& ps, bool clipped);

    /// `gorsel-isaretci` and `gorsel-cizgi`: the picture placed along the geometry.
    void emit_picture_along(const render::PolylineBatch& batch, const render::PassStyle& ps,
                            double cx, double cy);

    /// A `gorsel-isaretci` inside the face it labels — one stamp per ring.
    void emit_picture_centres(const render::PolygonBatch& batch, const render::PassStyle& ps,
                              double cx, double cy);

    /// `gorsel-dolgu`: the picture tiled into the face, clipped to it.
    void emit_picture_fill(const render::PolygonBatch& batch, const render::PassStyle& ps,
                           double cx, double cy);

#if KENTOS_HAVE_TEXT
    /// The captions in the document and the labels in the overlay, as glyph quads.
    void emit_texts(const render::DrawList& list, double cx, double cy);
    void emit_labels(const render::Overlay& overlay);

    /// Appends one line of text and returns how many glyph instances it added.
    ///
    /// `origin` is where the BASELINE starts in widget pixels, `px` the em size,
    /// and `cos_a`/`sin_a` the baseline direction — a rotation the document never
    /// stores as an angle, because the two ends of the baseline already say it.
    std::uint32_t emit_line(render::Face face, std::string_view text, float origin_x,
                            float origin_y, float px, float cos_a, float sin_a, std::uint32_t rgba,
                            std::uint8_t anchor);
#endif

    /// Appends one ring as a triangle fan and grows `box`. Returns the vertex count.
    std::uint32_t emit_fan(const float* xs, const float* ys, std::uint32_t count, double cx,
                           double cy, bool flip_y, float box[4]);

    /// Appends the six vertices of an axis-aligned cover quad.
    std::uint32_t emit_cover(const float box[4]);

    /// Appends one segment instance. `along0` is the distance from the start of
    /// the RUN to `x0,y0` — what the dash pattern is measured against.
    void emit_segment(float x0, float y0, float x1, float y1, float along0 = 0.0f);

    /// Reserves a uniform slot and fills it.
    ///
    /// `dash` is the mark/space ladder in PIXELS, mark first; null means solid.
    std::uint32_t push_uniform(std::uint32_t rgba, float half_width, const float* dash = nullptr,
                               int dash_count = 0);

    // ---- GPU resources -----------------------------------------------------

    bool ensure_resources(QRhi* rhi, QRhiRenderPassDescriptor* rp, int sample_count);
    bool ensure_capacity(QRhi* rhi, QRhiResourceUpdateBatch* rub);

    /// Re-points every set of bindings at the current `uniforms_`.
    ///
    /// Called when the uniform buffer is replaced. Three sets name that buffer BY
    /// ADDRESS — `srb_`, `srb_text_` and one per cached picture — and a set left
    /// naming the old one is a read of freed memory inside the driver.
    bool rebind_uniforms();

    void release();

#if KENTOS_HAVE_TEXT
    /// Opens the atlas once, and keeps the failure so it is not retried per frame.
    void ensure_atlas();

    /// Creates or re-creates the atlas texture and re-uploads when it changed.
    bool ensure_atlas_texture(QRhi* rhi, QRhiResourceUpdateBatch* rub);
#endif

    QRhi* rhi_{nullptr};
    QRhiRenderPassDescriptor* rp_{nullptr};
    int samples_{1};

    std::unique_ptr<QRhiBuffer> corners_;  ///< the four static line-quad corners
    std::unique_ptr<QRhiBuffer> segments_; ///< per-instance segment endpoints
    std::unique_ptr<QRhiBuffer> vertices_; ///< fan and cover triangles
    std::unique_ptr<QRhiBuffer> uniforms_;
    std::unique_ptr<QRhiShaderResourceBindings> srb_;
    std::unique_ptr<QRhiGraphicsPipeline> line_;
    std::unique_ptr<QRhiGraphicsPipeline> line_clip_;
    std::unique_ptr<QRhiGraphicsPipeline> tri_;
    std::unique_ptr<QRhiGraphicsPipeline> tri_clip_;
    std::unique_ptr<QRhiGraphicsPipeline> fill_stencil_;
    std::unique_ptr<QRhiGraphicsPipeline> fill_cover_;
    std::unique_ptr<QRhiGraphicsPipeline> mask_clear_;

    /// The instanced marker pipeline and its two buffers.
    ///
    /// `marker_verts_` holds each pass's glyph in GLYPH-LOCAL pixels — a few
    /// dozen vertices — and `marker_instances_` holds sixteen bytes per stamp.
    /// The clipped variant draws only inside the stencil, which is how a pattern
    /// stays inside its parcel.
    std::unique_ptr<QRhiBuffer> marker_verts_;
    std::unique_ptr<QRhiBuffer> marker_instances_;
    std::unique_ptr<QRhiGraphicsPipeline> marker_;
    std::unique_ptr<QRhiGraphicsPipeline> marker_clip_;
    quint32 marker_vertex_capacity_{0};
    quint32 marker_instance_capacity_{0};
    std::vector<float> marker_vertex_data_;   ///< glyph-local xy pairs
    std::vector<float> marker_instance_data_; ///< x, y, cos, sin per stamp

#if KENTOS_HAVE_TEXT
    std::unique_ptr<QRhiBuffer> quad_;   ///< the four static glyph-quad corners
    std::unique_ptr<QRhiBuffer> glyphs_; ///< per-instance glyph rectangles
    std::unique_ptr<QRhiTexture> atlas_texture_;
    std::unique_ptr<QRhiSampler> sampler_;
    std::unique_ptr<QRhiShaderResourceBindings> srb_text_;
    std::unique_ptr<QRhiGraphicsPipeline> text_;
#endif

    std::unique_ptr<QRhiSampler> picture_sampler_;
    QShader picture_vs_;
    QShader picture_fs_;
    QRhiVertexInputLayout picture_input_;

    std::unique_ptr<QRhiBuffer> picture_quad_;
    std::unique_ptr<QRhiBuffer> picture_instances_;
    quint32 picture_capacity_{0};
    bool picture_quad_uploaded_{false};
    std::vector<float> picture_data_; ///< 13 floats per instance, like the text one

#if KENTOS_HAVE_TEXT

    std::unique_ptr<render::TextAtlas> atlas_;
    bool atlas_tried_{false};
    int atlas_side_{0}; ///< the side the texture was created at
    std::uint64_t atlas_uploaded_{0};
    bool quad_uploaded_{false};
    quint32 glyph_capacity_{0}; ///< bytes

    std::vector<float> glyph_data_;           ///< 13 floats per instance
    std::vector<render::PlacedGlyph> shaped_; ///< shaping scratch

    /// The lines of one caption. A member and not a local, because the draw loop
    /// must not allocate (render.md R20, P6) and a two-line TAKS/KAKS label on
    /// every parcel of a sheet would be one vector construction per parcel.
    std::vector<std::string_view> lines_;
#endif

    quint32 segment_capacity_{0}; ///< bytes
    quint32 vertex_capacity_{0};  ///< bytes
    quint32 uniform_capacity_{0}; ///< bytes
    quint32 uniform_stride_{256};
    bool corners_uploaded_{false};

    // ---- per-frame CPU buffers, capacity kept between frames ----------------

    std::vector<float> segment_data_; ///< x0,y0,x1,y1,along0 per instance
    std::vector<float> vertex_data_;  ///< x,y per vertex
    std::vector<char> uniform_data_;  ///< `uniform_stride_` bytes per slot
    std::vector<Cmd> cmds_;

    /// What the last frame cost; answered by `stats()` (render.md R7).
    render::FrameStats stats_{};

    // Symbology scratch. Members and not locals, because the draw loop must not
    // allocate (render.md R20, P6) and a sheet asks for these once per pass.
    /// A decoded picture on its way to the GPU, and once there.
    ///
    /// Keyed by the image store's CONTENT key combined with the size it was
    /// rasterised at — a vector source is rendered at the size it will be drawn,
    /// so the same picture at two zooms is two entries and not one resampling.
    struct Picture
    {
        QImage cpu; ///< kept until the upload lands
        std::unique_ptr<QRhiTexture> texture;
        std::unique_ptr<QRhiShaderResourceBindings> srb;

        /// ITS OWN PIPELINES, and that is the point.
        ///
        /// The first attempt built two pipelines against a layout-only bindings
        /// set and swapped a per-picture set in at draw time. QRhi allows a
        /// layout-compatible swap, but what actually happened was that no picture
        /// drew at all and the draws that followed came out as a fan of stretched
        /// quads from the canvas corner. A pipeline per picture removes the
        /// question: a sheet draws a handful of pictures, and a pipeline that owns
        /// its own bindings cannot be handed the wrong ones.
        std::unique_ptr<QRhiGraphicsPipeline> pipe;
        std::unique_ptr<QRhiGraphicsPipeline> pipe_clip;
        int width{0};
        int height{0};
        bool pending{true}; ///< still to be created/uploaded
    };

    std::unordered_map<std::uint64_t, Picture> pictures_;

    /// This frame's pictures in draw order; `Cmd::image` indexes it plus one.
    std::vector<std::uint64_t> picture_keys_;

    render::MarkerOutline glyph_;
    std::vector<render::Stamp> stamps_;
    std::vector<float> scratch_;
    std::vector<float> run_x_;
    std::vector<float> run_y_;

    /// The frame's atlas distance range, copied into every uniform slot beside the
    /// matrix. Zero when this build has no text.
    float px_range_{0.0f};

    /// This frame's widget centre. The captions are emitted from `paint_aids`,
    /// which runs after the passes and does not carry it — see `render()`.
    double last_cx_{0.0};
    double last_cy_{0.0};

    Uniforms proto_{}; ///< this frame's mvp, copied into every slot
};

// -----------------------------------------------------------------------------
// what it draws
// -----------------------------------------------------------------------------

bool RhiBackend::handles(core::SymbolLayerType type)
{
    switch (type) {
    // Geometry, and the vector symbology of `/data/catalogs/mpyy-vektor` — 816 of
    // the annex's 1 574 symbol layers. Markers along a line, glyph grids and
    // hatches clipped to a face, one glyph at a centroid: all of it is triangles
    // and segments through the pipelines above, clipped by the same even-odd
    // stencil a solid fill uses (`symbology.hpp`).
    case core::SymbolLayerType::SimpleFill:
    case core::SymbolLayerType::SimpleLine:
    case core::SymbolLayerType::SimpleMarker:
    case core::SymbolLayerType::MarkerLine:
    case core::SymbolLayerType::HashLine:
    case core::SymbolLayerType::CentroidFill:
    case core::SymbolLayerType::LinePatternFill:
    case core::SymbolLayerType::PointPatternFill:

    // The three published PICTURE types. The bytes travel inside the document and
    // are decoded by `symbol_image.hpp` — the same decoder the QPainter backend
    // uses, because the alpha keying is a decision about what counts as paper and
    // a second copy of it is a symbol that looks different per engine.
    case core::SymbolLayerType::RasterFill:
    case core::SymbolLayerType::RasterMarker:
    case core::SymbolLayerType::RasterLine: return true;

    // TextMarker is drawn from the caption list rather than from a pass, so
    // claiming it here would draw it twice. Nothing else is left, and the default
    // still REFUSES: the QGIS backend decided what it could draw by exclusion
    // once, and three raster types went out the door silently.
    case core::SymbolLayerType::TextMarker:
    default: return false;
    }
}

// -----------------------------------------------------------------------------
// frame building
// -----------------------------------------------------------------------------

std::uint32_t RhiBackend::push_uniform(std::uint32_t rgba, float half_width, const float* dash,
                                       int dash_count)
{
    const std::size_t slot = uniform_data_.size() / uniform_stride_;

    Uniforms u = proto_;
    unpack(rgba, u.colour);
    u.params[0] = half_width;
    u.params[1] = px_range_;

    if (dash != nullptr && dash_count > 0) {
        const int count = std::min(dash_count, 8);
        float period    = 0.0f;
        for (int i = 0; i < count; ++i) {
            u.dash[i] = dash[i];
            period += dash[i];
        }
        // An ODD ladder repeats inverted — mark, space, mark / space, mark, space.
        // Doubling the period is what makes the second repeat land on the space,
        // which is what a three-entry dash-dot pattern means on paper.
        u.params[2] = (count % 2 == 0) ? period : period * 2.0f;
        u.params[3] = static_cast<float>(count);
    }

    uniform_data_.resize(uniform_data_.size() + uniform_stride_, 0);
    std::memcpy(uniform_data_.data() + slot * uniform_stride_, &u, sizeof(u));
    return static_cast<std::uint32_t>(slot);
}

void RhiBackend::emit_segment(float x0, float y0, float x1, float y1, float along0)
{
    segment_data_.push_back(x0);
    segment_data_.push_back(y0);
    segment_data_.push_back(x1);
    segment_data_.push_back(y1);
    segment_data_.push_back(along0);
}

std::uint32_t RhiBackend::emit_fan(const float* xs, const float* ys, std::uint32_t count, double cx,
                                   double cy, bool flip_y, float box[4])
{
    if (count < 3) return 0;

    const auto px = [&](std::uint32_t i) {
        return static_cast<float>(cx + static_cast<double>(xs[i]));
    };
    const auto py = [&](std::uint32_t i) {
        const double y = static_cast<double>(ys[i]);
        return static_cast<float>(flip_y ? cy - y : cy + y);
    };

    for (std::uint32_t i = 0; i < count; ++i) {
        box[0] = std::min(box[0], px(i));
        box[1] = std::min(box[1], py(i));
        box[2] = std::max(box[2], px(i));
        box[3] = std::max(box[3], py(i));
    }

    const float ax = px(0);
    const float ay = py(0);
    for (std::uint32_t i = 1; i + 1 < count; ++i) {
        vertex_data_.push_back(ax);
        vertex_data_.push_back(ay);
        vertex_data_.push_back(px(i));
        vertex_data_.push_back(py(i));
        vertex_data_.push_back(px(i + 1));
        vertex_data_.push_back(py(i + 1));
    }
    return (count - 2) * 3;
}

std::uint32_t RhiBackend::emit_cover(const float box[4])
{
    const std::uint32_t first = static_cast<std::uint32_t>(vertex_data_.size() / 2);
    const float quad[12]      = {
        box[0], box[1], box[2], box[1], box[2], box[3],
        box[0], box[1], box[2], box[3], box[0], box[3],
    };
    vertex_data_.insert(vertex_data_.end(), std::begin(quad), std::end(quad));
    return first;
}

bool RhiBackend::emit_face(const render::PolygonBatch& batch, double cx, double cy,
                           std::uint32_t rgba, bool as_mask, float box[4])
{
    box[0] = 1e30f;
    box[1] = 1e30f;
    box[2] = -1e30f;
    box[3] = -1e30f;

    if (batch.runs.empty()) return false;

    const std::uint32_t first = static_cast<std::uint32_t>(vertex_data_.size() / 2);

    std::size_t offset  = 0;
    std::uint32_t total = 0;
    for (std::uint32_t run : batch.runs) {
        total += emit_fan(batch.xs.data() + offset, batch.ys.data() + offset, run, cx, cy,
                          /*flip_y=*/true, box);
        offset += run;
    }
    if (total == 0) return false;

    Cmd cmd;
    cmd.kind    = as_mask ? Cmd::Kind::Mask : Cmd::Kind::Fill;
    cmd.uniform = push_uniform(rgba, 0.0f);
    cmd.first   = first;
    cmd.count   = total;
    cmd.cover   = emit_cover(box);
    cmds_.push_back(cmd);
    return true;
}

void RhiBackend::emit_unmask(const float box[4])
{
    Cmd cmd;
    cmd.kind    = Cmd::Kind::Unmask;
    cmd.uniform = push_uniform(0u, 0.0f);
    cmd.cover   = emit_cover(box);
    cmds_.push_back(cmd);
}

void RhiBackend::emit_stamps(const render::MarkerOutline& glyph, const render::PassStyle& ps,
                             float extra_cos, float extra_sin, bool clipped)
{
    if (stamps_.empty() || glyph.runs.empty()) return;

    const std::uint32_t fill_rgba = faded(ps.fill_rgba, ps.opacity);
    const std::uint32_t line_rgba = faded(ps.line_rgba, ps.opacity);
    const bool stroked            = line_rgba != 0 && ps.line_width_px > 0.0f;

    // THE GLYPH ONCE, THE STAMPS MANY TIMES.
    //
    // This used to expand the glyph's own geometry — a fan of triangles and a
    // ring of segments — once per stamp. A mosque parcel's dot pattern at the
    // drawing's full extent is nineteen hundred stamps, so three parcels rebuilt
    // and re-uploaded 134 784 triangle vertices and 44 895 segment records EVERY
    // FRAME, to draw two-pixel dots. That is the whole of the answer to "why does
    // zooming get slow when I style a layer", and it is why QGIS — which caches a
    // symbol and stamps the cache — does not have it.
    //
    // The glyph now goes into `marker_verts_` in its own local pixels and each
    // stamp becomes sixteen bytes in `marker_instances_`. Same shape the text
    // pipeline has had from the beginning: one instance per glyph, not one buffer
    // per string.
    const auto push_local = [this](float x, float y) {
        marker_vertex_data_.push_back(x);
        marker_vertex_data_.push_back(y);
    };

    const std::uint32_t fill_first = static_cast<std::uint32_t>(marker_vertex_data_.size() / 2);
    std::uint32_t fill_count       = 0;

    if (fill_rgba != 0) {
        std::size_t offset = 0;
        for (std::size_t r = 0; r < glyph.runs.size(); ++r) {
            const std::uint32_t run = glyph.runs[r];
            const bool shut         = r < glyph.closed.size() && glyph.closed[r] != 0;

            // Fanned from the glyph's own ORIGIN rather than from a vertex. Every
            // shape in `symbology.hpp` is star-shaped about the centre — that is
            // what "a marker centred on its point" means — so the fan is a correct
            // triangulation of the star's notches and the arrow's tail without a
            // triangulator.
            if (shut && run >= 3) {
                for (std::uint32_t v = 0; v < run; ++v) {
                    const std::uint32_t next = (v + 1) % run;
                    push_local(0.0f, 0.0f);
                    push_local(glyph.xs[offset + v], glyph.ys[offset + v]);
                    push_local(glyph.xs[offset + next], glyph.ys[offset + next]);
                }
                fill_count += run * 3;
            }
            offset += run;
        }
    }

    // THE STROKE, AS LOCAL TRIANGLES, and this is not an approximation of what the
    // line pipeline draws — it is the same quad. `line.vert` widens a segment to
    // `half_width` about its own centre and extends it by `half_width` at each
    // end, and `line.frag` writes a flat colour with no antialiasing, so the two
    // paths rasterise identically. Building it here means a stroked glyph is
    // instanced like a filled one instead of costing one segment record per edge
    // per stamp.
    const std::uint32_t stroke_first = static_cast<std::uint32_t>(marker_vertex_data_.size() / 2);
    std::uint32_t stroke_count       = 0;

    if (stroked) {
        const float half = std::max(0.5f, ps.line_width_px * 0.5f);

        const auto quad = [&](float x0, float y0, float x1, float y1) {
            const float dx  = x1 - x0;
            const float dy  = y1 - y0;
            const float len = std::hypot(dx, dy);
            const float tx  = len > 0.0f ? dx / len : 1.0f;
            const float ty  = len > 0.0f ? dy / len : 0.0f;
            const float nx  = -ty;
            const float ny  = tx;

            // The segment extended by a half width at each end, exactly as the
            // `corner.x * 2 - 1` term in `line.vert` does.
            const float ax = x0 - tx * half;
            const float ay = y0 - ty * half;
            const float bx = x1 + tx * half;
            const float by = y1 + ty * half;

            push_local(ax - nx * half, ay - ny * half);
            push_local(ax + nx * half, ay + ny * half);
            push_local(bx - nx * half, by - ny * half);

            push_local(bx - nx * half, by - ny * half);
            push_local(ax + nx * half, ay + ny * half);
            push_local(bx + nx * half, by + ny * half);

            stroke_count += 6;
        };

        std::size_t offset = 0;
        for (std::size_t r = 0; r < glyph.runs.size(); ++r) {
            const std::uint32_t run = glyph.runs[r];
            const bool shut         = r < glyph.closed.size() && glyph.closed[r] != 0;

            for (std::uint32_t v = 0; v + 1 < run; ++v)
                quad(glyph.xs[offset + v], glyph.ys[offset + v], glyph.xs[offset + v + 1],
                     glyph.ys[offset + v + 1]);
            if (shut && run >= 3)
                quad(glyph.xs[offset + run - 1], glyph.ys[offset + run - 1], glyph.xs[offset],
                     glyph.ys[offset]);

            offset += run;
        }
    }

    if (fill_count == 0 && stroke_count == 0) return;

    // ONE INSTANCE BLOCK FOR BOTH DRAWS. The fill and the stroke sit at the same
    // places and face the same way; only the colour differs, and that is a
    // uniform.
    const std::uint32_t instance_first =
        static_cast<std::uint32_t>(marker_instance_data_.size() * sizeof(float));

    for (const render::Stamp& stamp : stamps_) {
        // The stamp's direction composed with the pass's own angle. Two rotations
        // and not one: the stamp carries where the LINE points, the pass carries
        // how the GLYPH is turned on it.
        marker_instance_data_.push_back(stamp.x);
        marker_instance_data_.push_back(stamp.y);
        marker_instance_data_.push_back(stamp.cos_a * extra_cos - stamp.sin_a * extra_sin);
        marker_instance_data_.push_back(stamp.cos_a * extra_sin + stamp.sin_a * extra_cos);
    }

    const auto submit = [&](std::uint32_t first, std::uint32_t count, std::uint32_t rgba) {
        if (count == 0) return;
        Cmd cmd;
        cmd.kind        = Cmd::Kind::Marker;
        cmd.clipped     = clipped;
        cmd.uniform     = push_uniform(rgba, 0.0f);
        cmd.first       = instance_first;
        cmd.count       = static_cast<std::uint32_t>(stamps_.size());
        cmd.glyph_first = first;
        cmd.glyph_count = count;
        cmds_.push_back(cmd);
    };

    submit(fill_first, fill_count, fill_rgba);
    submit(stroke_first, stroke_count, line_rgba);
}

void RhiBackend::emit_marker_line(const render::PolylineBatch& batch, const render::PassStyle& ps,
                                  double cx, double cy, bool hash)
{
    if (batch.runs.empty()) return;

    const double size = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 5.0;
    const double interval =
        ps.interval_px > 0.5f ? static_cast<double>(ps.interval_px) : size * 3.0;

    // A hash tick is a STROKE and has no interior to fill, which is why it is
    // asked for by shape rather than by flag.
    render::marker_outline(hash ? core::MarkerShape::Tick : ps.shape, size, glyph_);

    stamps_.clear();
    scratch_.clear();

    std::size_t offset = 0;
    for (std::uint32_t run : batch.runs) {
        // The placement walks SCREEN pixels, so the offset and the y flip happen
        // once, here, rather than inside the shared arithmetic. Two plain arrays
        // rather than one interleaved buffer, and they are MEMBERS: the draw loop
        // must not allocate (render.md R20) and a boundary with a marker line on
        // it has one run per parcel.
        run_x_.clear();
        run_y_.clear();
        for (std::uint32_t v = 0; v < run; ++v) {
            run_x_.push_back(static_cast<float>(cx + static_cast<double>(batch.xs[offset + v])));
            run_y_.push_back(static_cast<float>(cy - static_cast<double>(batch.ys[offset + v])));
        }
        render::place_along_run(run_x_.data(), run_y_.data(), run, ps.placement, interval,
                                static_cast<double>(ps.phase_px), grown(visible_, size), stamps_);
        offset += run;
    }

    const double radians = static_cast<double>(ps.angle_udeg) / 1'000'000.0 * kPi / 180.0;
    emit_stamps(glyph_, ps, static_cast<float>(std::cos(radians)),
                static_cast<float>(std::sin(radians)), /*clipped=*/false);
}

void RhiBackend::emit_point_dots(const render::PolylineBatch& batch, const render::PassStyle& ps,
                                 double cx, double cy)
{
    bool any = false;
    for (const std::uint32_t run : batch.runs)
        if (run == 1) {
            any = true;
            break;
        }
    if (!any) return;

    const double size = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 5.0;
    render::marker_outline(core::MarkerShape::Circle, size, glyph_);

    stamps_.clear();
    scratch_.clear();

    std::size_t offset = 0;
    for (const std::uint32_t run : batch.runs) {
        if (run == 1)
            stamps_.push_back(render::Stamp{
                static_cast<float>(cx + static_cast<double>(batch.xs[offset])),
                static_cast<float>(cy - static_cast<double>(batch.ys[offset])), 1.0f, 0.0f});
        offset += run;
    }

    // FILLED WITH THE STROKE COLOUR. A plain pass carries no fill — a stroke has
    // no interior — but the disc that stands in for an unstyled point is solid,
    // and it is solid in the STROKE's colour on the other two backends. A copy of
    // the pass rather than a second `emit_stamps`: the difference is one field.
    render::PassStyle dot = ps;
    dot.fill_rgba         = batch.rgba;
    dot.line_rgba         = batch.rgba;
    emit_stamps(glyph_, dot, 1.0f, 0.0f, /*clipped=*/false);
}

void RhiBackend::emit_line_pattern(const render::PolygonBatch& batch, const render::PassStyle& ps,
                                   double cx, double cy)
{
    float box[4] = {};
    // NO GROUND WASH. `dolgu_renk` is the GLYPH's colour — the black of a forest
    // triangle — and washing the face with it paints the whole parcel that colour
    // with the pattern invisible inside. Washing is a `dolgu` layer's job and the
    // MPYY package puts one underneath, which is what draws the green under the
    // trees.
    if (!emit_face(batch, cx, cy, 0u, /*as_mask=*/true, box)) return;

    const double spacing = ps.interval_px > 0.5f ? static_cast<double>(ps.interval_px) : 6.0;

    scratch_.clear();
    render::hatch_lines(render::PixelBox{box[0], box[1], box[2], box[3]}, visible_, spacing,
                        static_cast<double>(ps.angle_udeg) / 1'000'000.0, scratch_);

    const std::uint32_t first = static_cast<std::uint32_t>(segment_data_.size() * sizeof(float));
    for (std::size_t i = 0; i + 3 < scratch_.size(); i += 4)
        emit_segment(scratch_[i], scratch_[i + 1], scratch_[i + 2], scratch_[i + 3]);

    const std::uint32_t count =
        static_cast<std::uint32_t>(segment_data_.size() / 5 - first / (5 * sizeof(float)));
    if (count > 0) {
        Cmd cmd;
        cmd.kind    = Cmd::Kind::Line;
        cmd.clipped = true;
        cmd.uniform =
            push_uniform(faded(ps.line_rgba, ps.opacity), std::max(0.5f, ps.line_width_px * 0.5f));
        cmd.first = first;
        cmd.count = count;
        cmds_.push_back(cmd);
    }

    emit_unmask(box);
}

void RhiBackend::emit_point_pattern(const render::PolygonBatch& batch, const render::PassStyle& ps,
                                    double cx, double cy)
{
    float box[4] = {};
    if (!emit_face(batch, cx, cy, 0u, /*as_mask=*/true, box)) return;

    const double step_x = ps.interval_px > 0.5f ? static_cast<double>(ps.interval_px) : 12.0;
    // Zero means SQUARE, not zero: a glyph grid with no second spacing is a
    // regular grid, which is what a forest symbol wants.
    const double step_y = ps.spacing_y_px > 0.5f ? static_cast<double>(ps.spacing_y_px) : step_x;
    const double size   = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 4.0;

    scratch_.clear();
    render::pattern_points(render::PixelBox{box[0], box[1], box[2], box[3]}, visible_, step_x,
                           step_y, scratch_);

    stamps_.clear();
    stamps_.reserve(scratch_.size() / 2);
    for (std::size_t i = 0; i + 1 < scratch_.size(); i += 2)
        stamps_.push_back(render::Stamp{scratch_[i], scratch_[i + 1], 1.0f, 0.0f});

    render::marker_outline(ps.shape, size, glyph_);

    const double radians = static_cast<double>(ps.angle_udeg) / 1'000'000.0 * kPi / 180.0;
    emit_stamps(glyph_, ps, static_cast<float>(std::cos(radians)),
                static_cast<float>(std::sin(radians)), /*clipped=*/true);

    emit_unmask(box);
}

void RhiBackend::emit_centroid(const render::PolygonBatch& batch, const render::PassStyle& ps,
                               double cx, double cy)
{
    if (batch.runs.empty()) return;

    const double size = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 6.0;
    render::marker_outline(ps.shape, size, glyph_);

    stamps_.clear();

    std::size_t offset = 0;
    for (std::uint32_t run : batch.runs) {
        // The bounding-box centre, not the area centroid. For a plan symbol placed
        // inside a lekesi the difference is invisible, and the area centroid of a
        // ring with holes is a different computation that belongs in core.
        float min_x = 1e30f, min_y = 1e30f, max_x = -1e30f, max_y = -1e30f;
        for (std::uint32_t v = 0; v < run; ++v) {
            const auto x = static_cast<float>(cx + static_cast<double>(batch.xs[offset + v]));
            const auto y = static_cast<float>(cy - static_cast<double>(batch.ys[offset + v]));
            min_x        = std::min(min_x, x);
            min_y        = std::min(min_y, y);
            max_x        = std::max(max_x, x);
            max_y        = std::max(max_y, y);
        }
        if (run > 0)
            stamps_.push_back(
                render::Stamp{(min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f, 1.0f, 0.0f});
        offset += run;
    }

    const double radians = static_cast<double>(ps.angle_udeg) / 1'000'000.0 * kPi / 180.0;
    emit_stamps(glyph_, ps, static_cast<float>(std::cos(radians)),
                static_cast<float>(std::sin(radians)), /*clipped=*/false);
}

std::uint32_t RhiBackend::picture_slot(const render::PassStyle& ps, int wanted_px)
{
    if (ps.image.empty() || ps.image_key == 0) return 0;

    const int bucket        = std::clamp(wanted_px, 8, 512);
    const std::uint64_t key = ps.image_key ^ (static_cast<std::uint64_t>(bucket) << 48);

    auto it = pictures_.find(key);
    if (it == pictures_.end()) {
        // Decoded ONCE and cached even when it fails, so a picture this build
        // cannot read costs one attempt rather than one attempt per frame.
        Picture entry;
        entry.cpu = decode_symbol_image(ps.image, bucket);
        if (!entry.cpu.isNull()) {
            entry.cpu    = entry.cpu.convertToFormat(QImage::Format_RGBA8888);
            entry.width  = entry.cpu.width();
            entry.height = entry.cpu.height();
        }
        it = pictures_.emplace(key, std::move(entry)).first;
    }
    if (it->second.width <= 0 || it->second.height <= 0) return 0;

    // The FRAME's own list, so a draw names a small index rather than a hash and
    // the upload pass has exactly the pictures this frame asked for.
    for (std::size_t i = 0; i < picture_keys_.size(); ++i)
        if (picture_keys_[i] == key) return static_cast<std::uint32_t>(i + 1);

    picture_keys_.push_back(key);
    return static_cast<std::uint32_t>(picture_keys_.size());
}

void RhiBackend::emit_picture_stamps(std::uint32_t slot, float width, float height,
                                     const render::PassStyle& ps, bool clipped)
{
    if (slot == 0 || stamps_.empty()) return;

    const auto first = static_cast<std::uint32_t>(picture_data_.size() * sizeof(float));

    // Only the OPACITY tints a published picture. It is the regulation's own
    // drawing, and recolouring it would be answering a question the annex has
    // already answered.
    const std::uint32_t ink =
        rgba_bytes(0x00FFFFFFu | (static_cast<std::uint32_t>(ps.opacity) << 24));
    const float ink_bits = std::bit_cast<float>(ink);

    const double radians = static_cast<double>(ps.angle_udeg) / 1'000'000.0 * kPi / 180.0;
    const auto extra_cos = static_cast<float>(std::cos(radians));
    const auto extra_sin = static_cast<float>(std::sin(radians));

    const float hw = width * 0.5f;
    const float hh = height * 0.5f;

    for (const render::Stamp& stamp : stamps_) {
        const float ca = stamp.cos_a * extra_cos - stamp.sin_a * extra_sin;
        const float sa = stamp.cos_a * extra_sin + stamp.sin_a * extra_cos;

        const float instance[13] = {
            -hw,      -hh,     hw,   hh,   // local rect, centred on the stamp
            stamp.x,  stamp.y, ca,   sa,   // where, and which way it faces
            0.0f,     0.0f,    1.0f, 1.0f, // the whole picture
            ink_bits,
        };
        picture_data_.insert(picture_data_.end(), std::begin(instance), std::end(instance));
    }

    Cmd cmd;
    cmd.kind    = Cmd::Kind::Image;
    cmd.clipped = clipped;
    cmd.image   = slot;
    cmd.uniform = push_uniform(0xFFFFFFFFu, 0.0f);
    cmd.first   = first;
    cmd.count   = static_cast<std::uint32_t>(stamps_.size());
    cmds_.push_back(cmd);
}

void RhiBackend::emit_picture_along(const render::PolylineBatch& batch, const render::PassStyle& ps,
                                    double cx, double cy)
{
    if (batch.runs.empty()) return;

    const double height      = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 16.0;
    const std::uint32_t slot = picture_slot(ps, static_cast<int>(std::lround(height)));
    if (slot == 0) return;

    const Picture& entry = pictures_.at(picture_keys_[slot - 1]);
    const double width   = entry.height > 0 ? height * entry.width / entry.height : height;

    // A line type tiles edge to edge unless a spacing was asked for; a symbol is
    // placed once and does not tile.
    const double interval =
        ps.interval_px > 0.5f
            ? static_cast<double>(ps.interval_px)
            : (ps.type == core::SymbolLayerType::RasterLine ? width : width * 2.0);

    stamps_.clear();

    std::size_t offset = 0;
    for (std::uint32_t run : batch.runs) {
        run_x_.clear();
        run_y_.clear();
        for (std::uint32_t v = 0; v < run; ++v) {
            run_x_.push_back(static_cast<float>(cx + static_cast<double>(batch.xs[offset + v])));
            run_y_.push_back(static_cast<float>(cy - static_cast<double>(batch.ys[offset + v])));
        }
        render::place_along_run(run_x_.data(), run_y_.data(), run, ps.placement, interval,
                                static_cast<double>(ps.phase_px),
                                grown(visible_, std::max(width, height)), stamps_);
        offset += run;
    }

    emit_picture_stamps(slot, static_cast<float>(width), static_cast<float>(height), ps,
                        /*clipped=*/false);
}

void RhiBackend::emit_picture_centres(const render::PolygonBatch& batch,
                                      const render::PassStyle& ps, double cx, double cy)
{
    if (batch.runs.empty()) return;

    const double height      = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 16.0;
    const std::uint32_t slot = picture_slot(ps, static_cast<int>(std::lround(height)));
    if (slot == 0) return;

    const Picture& entry = pictures_.at(picture_keys_[slot - 1]);
    const double width   = entry.height > 0 ? height * entry.width / entry.height : height;

    stamps_.clear();

    std::size_t offset = 0;
    for (std::uint32_t run : batch.runs) {
        float min_x = 1e30f, min_y = 1e30f, max_x = -1e30f, max_y = -1e30f;
        for (std::uint32_t v = 0; v < run; ++v) {
            const auto x = static_cast<float>(cx + static_cast<double>(batch.xs[offset + v]));
            const auto y = static_cast<float>(cy - static_cast<double>(batch.ys[offset + v]));
            min_x        = std::min(min_x, x);
            min_y        = std::min(min_y, y);
            max_x        = std::max(max_x, x);
            max_y        = std::max(max_y, y);
        }
        if (run > 0)
            stamps_.push_back(
                render::Stamp{(min_x + max_x) * 0.5f, (min_y + max_y) * 0.5f, 1.0f, 0.0f});
        offset += run;
    }

    emit_picture_stamps(slot, static_cast<float>(width), static_cast<float>(height), ps,
                        /*clipped=*/false);
}

void RhiBackend::emit_picture_fill(const render::PolygonBatch& batch, const render::PassStyle& ps,
                                   double cx, double cy)
{
    // Tiled at a DECLARED size rather than at its pixel size, because a hatch
    // extracted from a Word annex has whatever resolution the annex had, and a
    // plan whose hatch spacing follows the scan resolution says the wrong thing.
    const double tile        = ps.size_px > 0.5f ? static_cast<double>(ps.size_px) : 24.0;
    const std::uint32_t slot = picture_slot(ps, static_cast<int>(std::lround(tile)));
    if (slot == 0) return;

    const Picture& entry = pictures_.at(picture_keys_[slot - 1]);
    const double ratio   = entry.width > 0 ? double(entry.height) / entry.width : 1.0;
    const double cell_w  = tile;
    const double cell_h  = tile * ratio;

    // SPACED, when the pass asks for it. A scanned hatch already carries whatever
    // spacing the annex printed and tiles edge to edge; a glyph somebody DREW
    // fills its own box, so tiling that edge to edge reads as a solid mat.
    const double gap    = static_cast<double>(ps.interval_px);
    const double step_x = std::max(cell_w, gap);
    const double step_y = std::max(cell_h, gap);

    float box[4] = {};
    if (!emit_face(batch, cx, cy, 0u, /*as_mask=*/true, box)) return;

    scratch_.clear();
    render::pattern_points(render::PixelBox{box[0], box[1], box[2], box[3]}, visible_, step_x,
                           step_y, scratch_);

    stamps_.clear();
    stamps_.reserve(scratch_.size() / 2);
    for (std::size_t i = 0; i + 1 < scratch_.size(); i += 2)
        stamps_.push_back(render::Stamp{scratch_[i], scratch_[i + 1], 1.0f, 0.0f});

    emit_picture_stamps(slot, static_cast<float>(cell_w), static_cast<float>(cell_h), ps,
                        /*clipped=*/true);

    emit_unmask(box);
}

void RhiBackend::emit_document(const render::DrawList& list, double cx, double cy)
{
    using core::SymbolLayerType;

    // IN DRAW ORDER, which is not index order: MPYY prescribes a draw order for
    // plan sheets and a symbol's stack runs bottom layer first. The scene builder
    // sorted it; this loop obeys it, exactly as the QPainter backend does.
    for (std::uint32_t index : list.order) {
        if (index >= list.passes.size()) continue;

        const render::PassStyle& ps       = list.passes[index];
        const render::PolylineBatch& line = list.polylines[index];
        const render::PolygonBatch& face  = list.polygons[index];

        switch (ps.type) {
        case SymbolLayerType::SimpleFill: {
            if (face.rgba != 0) {
                float box[4] = {};
                (void)emit_face(face, cx, cy, face.rgba, /*as_mask=*/false, box);
            }
            break;
        }

        case SymbolLayerType::SimpleLine: {
            if (line.runs.empty() || line.rgba == 0) break;

            // A lone vertex is a point, and the segment walk below skips it.
            emit_point_dots(line, ps, cx, cy);

            const std::uint32_t first =
                static_cast<std::uint32_t>(segment_data_.size() * sizeof(float));

            std::size_t offset = 0;
            for (std::uint32_t run : line.runs) {
                // The dash is measured along the WHOLE run, so the distance is
                // carried across the vertices rather than restarted at each one.
                // Restarting it at every corner is what turns a published kesik
                // çizgi into a row of unequal stubs, one per vertex.
                float along = 0.0f;
                for (std::uint32_t v = 0; v + 1 < run; ++v) {
                    const std::size_t a = offset + v;
                    const auto x0       = static_cast<float>(cx + static_cast<double>(line.xs[a]));
                    const auto y0       = static_cast<float>(cy - static_cast<double>(line.ys[a]));
                    const auto x1 = static_cast<float>(cx + static_cast<double>(line.xs[a + 1]));
                    const auto y1 = static_cast<float>(cy - static_cast<double>(line.ys[a + 1]));
                    emit_segment(x0, y0, x1, y1, along);
                    along += static_cast<float>(std::hypot(x1 - x0, y1 - y0));
                }
                offset += run;
            }

            const std::uint32_t count =
                static_cast<std::uint32_t>(segment_data_.size() / 5 - first / (5 * sizeof(float)));
            if (count > 0) {
                // The line type's own ladder, resolved out of the document's dash
                // store by the scene builder and carried in HUNDREDTHS of the
                // stroke width — a published kesik çizgi scales with its own
                // weight, so a heavier boundary gets a proportionally longer mark.
                float dash[8]{};
                const int dash_count = std::min<int>(ps.dash_count, 8);
                for (int i = 0; i < dash_count; ++i)
                    dash[i] = static_cast<float>(ps.dash_lengths[i]) * 0.01f *
                              std::max(1.0f, line.width_px);

                Cmd cmd;
                cmd.kind = Cmd::Kind::Line;
                cmd.uniform =
                    push_uniform(line.rgba, std::max(0.5f, line.width_px * 0.5f), dash, dash_count);
                cmd.first = first;
                cmd.count = count;
                cmds_.push_back(cmd);
            }
            break;
        }

        case SymbolLayerType::MarkerLine: emit_marker_line(line, ps, cx, cy, /*hash=*/false); break;

        case SymbolLayerType::HashLine: emit_marker_line(line, ps, cx, cy, /*hash=*/true); break;

        case SymbolLayerType::SimpleMarker:
            // A marker is a marker line whose placement says where. The scene
            // builder puts point geometry in the stroke batch, so this is the
            // same walk with a different default spacing.
            emit_marker_line(line, ps, cx, cy, /*hash=*/false);
            break;

        case SymbolLayerType::LinePatternFill: emit_line_pattern(face, ps, cx, cy); break;

        case SymbolLayerType::PointPatternFill: emit_point_pattern(face, ps, cx, cy); break;

        case SymbolLayerType::CentroidFill: emit_centroid(face, ps, cx, cy); break;

        case SymbolLayerType::RasterFill: emit_picture_fill(face, ps, cx, cy); break;

        case SymbolLayerType::RasterMarker:
            // A published sembol sits INSIDE the lekesi it labels, which is what
            // MPYY prints. Only a run that is not a face — an open line — puts it
            // on the geometry itself.
            if (!face.runs.empty())
                emit_picture_centres(face, ps, cx, cy);
            else
                emit_picture_along(line, ps, cx, cy);
            break;

        case SymbolLayerType::RasterLine: emit_picture_along(line, ps, cx, cy); break;

        case SymbolLayerType::TextMarker:
            // Nothing here: the scene builder turned it into a `TextItem`, so it
            // is drawn with the captions, over every fill and stroke. A word
            // inside a gösterim that a later pass could paint over is a word
            // nobody reads.
            break;
        }
    }
}

void RhiBackend::emit_overlay(const render::Overlay& overlay, std::size_t from, std::size_t to)
{
    for (std::size_t i = from; i < to && i < overlay.batches.size(); ++i) {
        const render::OverlayBatch& batch = overlay.batches[i];
        if (batch.runs.empty()) continue;

        // The filled half first — a selection box is a wash under its own outline.
        if (batch.fill_rgba != 0) {
            float box[4]              = {1e30f, 1e30f, -1e30f, -1e30f};
            const std::uint32_t first = static_cast<std::uint32_t>(vertex_data_.size() / 2);

            std::size_t offset  = 0;
            std::uint32_t total = 0;
            for (std::uint32_t run : batch.runs) {
                total += emit_fan(batch.xs.data() + offset, batch.ys.data() + offset, run, 0.0, 0.0,
                                  /*flip_y=*/false, box);
                offset += run;
            }

            if (total > 0) {
                Cmd cmd;
                cmd.kind    = Cmd::Kind::Fill;
                cmd.uniform = push_uniform(batch.fill_rgba, 0.0f);
                cmd.first   = first;
                cmd.count   = total;
                cmd.cover   = emit_cover(box);
                cmds_.push_back(cmd);
            }
        }

        const std::uint32_t first =
            static_cast<std::uint32_t>(segment_data_.size() * sizeof(float));

        std::size_t offset = 0;
        for (std::size_t r = 0; r < batch.runs.size(); ++r) {
            const std::uint32_t run = batch.runs[r];

            // The dash is measured along the WHOLE run, so the distance carries
            // across the vertices rather than restarting at each one.
            float along     = 0.0f;
            const auto step = [&](std::size_t a, std::size_t b) {
                emit_segment(batch.xs[a], batch.ys[a], batch.xs[b], batch.ys[b], along);
                along += std::hypot(batch.xs[b] - batch.xs[a], batch.ys[b] - batch.ys[a]);
            };

            for (std::uint32_t v = 0; v + 1 < run; ++v)
                step(offset + v, offset + v + 1);

            // A closed run's seam is a segment like any other; leaving it out is
            // what draws a selection rectangle with one side missing.
            if (run >= 3 && r < batch.closed.size() && batch.closed[r] != 0)
                step(offset + run - 1, offset);
            offset += run;
        }

        const std::uint32_t count =
            static_cast<std::uint32_t>(segment_data_.size() / 5 - first / (5 * sizeof(float)));
        if (count > 0) {
            // The overlay says only WHETHER it is dashed, not with what ladder: a
            // KESEN selection box reads as dashed before any label does, and four
            // on two is what Qt's own dash line draws — so the two backends agree
            // without the widget having to describe a pattern.
            const float w       = std::max(1.0f, batch.width_px);
            const float dash[2] = {4.0f * w, 2.0f * w};

            Cmd cmd;
            cmd.kind    = Cmd::Kind::Line;
            cmd.uniform = push_uniform(batch.rgba, std::max(0.5f, batch.width_px * 0.5f),
                                       batch.dashed ? dash : nullptr, batch.dashed ? 2 : 0);
            cmd.first   = first;
            cmd.count   = count;
            cmds_.push_back(cmd);
        }
    }
}

#if KENTOS_HAVE_TEXT

std::uint32_t RhiBackend::emit_line(render::Face face, std::string_view text, float origin_x,
                                    float origin_y, float px, float cos_a, float sin_a,
                                    std::uint32_t rgba, std::uint8_t anchor)
{
    shaped_.clear();
    const render::RunMetrics metrics = atlas_->shape(face, text, shaped_);
    if (shaped_.empty()) return 0;

    // The anchor decides where the baseline sits under the glyphs, and it is
    // measured from the SHAPED run rather than from the advance the command
    // guessed for its bounding box — the same rule the QPainter backend follows,
    // so a caption does not move when the engine changes.
    float shift_u = 0.0f;
    float shift_v = 0.0f;
    switch (anchor) {
    case 1: shift_u = -metrics.advance * px * 0.5f; break; // baseline centre
    case 2: shift_u = -metrics.advance * px; break;        // baseline right
    case 3:                                                // middle centre
        shift_u = -metrics.advance * px * 0.5f;
        shift_v = metrics.cap * px * 0.5f;
        break;
    default: break; // baseline left
    }

    const std::uint32_t ink = rgba_bytes(rgba);
    const float ink_bits    = std::bit_cast<float>(ink);

    for (const render::PlacedGlyph& glyph : shaped_) {
        const render::GlyphBox& box = atlas_->box(glyph.box);

        // EM, y up -> pixels along the baseline with v growing DOWN.
        const float u0 = (glyph.x + box.left) * px + shift_u;
        const float u1 = (glyph.x + box.right) * px + shift_u;
        const float v0 = -(glyph.y + box.top) * px + shift_v;
        const float v1 = -(glyph.y + box.bottom) * px + shift_v;

        const float instance[13] = {
            u0,       v0,       u1,     v1,     // local
            origin_x, origin_y, cos_a,  sin_a,  // place
            box.u0,   box.v0,   box.u1, box.v1, // uv
            ink_bits,
        };
        glyph_data_.insert(glyph_data_.end(), std::begin(instance), std::end(instance));
    }

    return static_cast<std::uint32_t>(shaped_.size());
}

void RhiBackend::emit_labels(const render::Overlay& overlay)
{
    if (!atlas_ || overlay.labels.empty()) return;

    const std::uint32_t first = static_cast<std::uint32_t>(glyph_data_.size() * sizeof(float));
    std::uint32_t count       = 0;

    for (const render::OverlayLabel& label : overlay.labels) {
        if (label.text.empty()) continue;

        // A ruler division, a coordinate and a measurement are all NUMBERS, and
        // design.md §3 puts every number in the monospaced face — digits that line
        // up column-wise are what makes a coordinate readable at a glance. The
        // label says which face it wants; the backend obeys.
        const render::Face face = label.mono ? render::Face::Mono : render::Face::Sans;
        const float px          = label.px > 0.0f ? label.px : kDefaultUiPx;

        count += emit_line(face, label.text, label.x, label.y, px, 1.0f, 0.0f, label.rgba,
                           /*anchor=*/0);
    }

    if (count > 0) {
        Cmd cmd;
        cmd.kind    = Cmd::Kind::Text;
        cmd.uniform = push_uniform(0xFFFFFFFFu, 0.0f);
        cmd.first   = first;
        cmd.count   = count;
        cmds_.push_back(cmd);
    }
}

void RhiBackend::emit_texts(const render::DrawList& list, double cx, double cy)
{
    if (!atlas_ || list.texts.empty()) return;

    const std::uint32_t first = static_cast<std::uint32_t>(glyph_data_.size() * sizeof(float));
    std::uint32_t count       = 0;

    for (const render::TextItem& item : list.texts) {
        // Under three pixels a caption is a smudge rather than a word, and drawing
        // it costs a glyph quad per character for something nobody can read. The
        // QPainter backend draws the same line here.
        if (item.text.empty() || item.height_px < 3.0f) continue;

        // CAP HEIGHT IN, EM SIZE OUT. `height_px` is the height of a CAPITAL
        // LETTER — that is what a DXF's group code 40 means, what the METIN
        // command promises, and what a lettering height is on a plotted sheet.
        // Everything below scales EM units, and the two differ by about a third:
        // fed straight in, an imported 3 m caption drew 2.1 m tall and a third
        // too narrow, which is how a TAKS over KAKS label stopped sitting
        // centred in its own circle.
        const float cap = atlas_->cap_height(render::Face::Sans);
        const float em  = cap > 0.0f ? item.height_px / cap : item.height_px;

        const double sx = cx + static_cast<double>(item.x0);
        const double sy = cy - static_cast<double>(item.y0);
        const double ex = cx + static_cast<double>(item.x1);
        const double ey = cy - static_cast<double>(item.y1);

        const double dx  = ex - sx;
        const double dy  = ey - sy;
        const double len = std::sqrt(dx * dx + dy * dy);

        // A caption whose baseline has no length has no direction either; it is
        // written along the page.
        const float cos_a = len > 0.0 ? static_cast<float>(dx / len) : 1.0f;
        const float sin_a = len > 0.0 ? static_cast<float>(dy / len) : 0.0f;

        // Stacked around the anchor, so a two-line label sits centred on the point
        // rather than hanging below it — a TAKS over a KAKS is one fraction.
        lines_.clear();
        std::size_t at = 0;
        while (at <= item.text.size()) {
            const std::size_t nl = item.text.find('\n', at);
            const std::size_t to = nl == std::string::npos ? item.text.size() : nl;
            if (to > at) lines_.emplace_back(item.text.data() + at, to - at);
            if (nl == std::string::npos) break;
            at = nl + 1;
        }
        if (lines_.empty()) continue;

        const float step = em * 1.25f;

        for (std::size_t line = 0; line < lines_.size(); ++line) {
            const float offset =
                (static_cast<float>(line) - static_cast<float>(lines_.size() - 1) * 0.5f) * step;

            // The stack runs perpendicular to the baseline, which is what keeps a
            // rotated two-line caption stacked across its own direction.
            const float ox = static_cast<float>(sx) - sin_a * offset;
            const float oy = static_cast<float>(sy) + cos_a * offset;

            count += emit_line(render::Face::Sans, lines_[line], ox, oy, em, cos_a, sin_a,
                               item.rgba, item.anchor);
        }
    }

    if (count > 0) {
        Cmd cmd;
        cmd.kind    = Cmd::Kind::Text;
        cmd.uniform = push_uniform(0xFFFFFFFFu, 0.0f);
        cmd.first   = first;
        cmd.count   = count;
        cmds_.push_back(cmd);
    }
}

#endif // KENTOS_HAVE_TEXT

void RhiBackend::paint_ground(const render::Overlay& overlay)
{
    emit_overlay(overlay, 0, overlay.beneath);
}

void RhiBackend::paint_aids(const render::DrawList& list, const render::Overlay& overlay)
{
    // The document's own captions go UNDER the aids and over the passes, which is
    // where the QPainter backend puts them: a parcel number belongs to the drawing,
    // and a snap marker belongs over whatever it is pointing at.
#if KENTOS_HAVE_TEXT
    emit_texts(list, last_cx_, last_cy_);
#else
    (void)list;
#endif

    emit_overlay(overlay, overlay.beneath, overlay.batches.size());

#if KENTOS_HAVE_TEXT
    emit_labels(overlay);
#endif
}

// -----------------------------------------------------------------------------
// GPU resources
// -----------------------------------------------------------------------------

void RhiBackend::release()
{
#if KENTOS_HAVE_TEXT
    text_.reset();
    srb_text_.reset();
    sampler_.reset();
    atlas_texture_.reset();
    glyphs_.reset();
    quad_.reset();
    glyph_capacity_ = 0;
    atlas_side_     = 0;
    atlas_uploaded_ = 0;
    quad_uploaded_  = false;
#endif

    picture_instances_.reset();
    picture_quad_.reset();
    picture_sampler_.reset();
    pictures_.clear();
    picture_capacity_      = 0;
    picture_quad_uploaded_ = false;

    line_.reset();
    line_clip_.reset();
    tri_.reset();
    tri_clip_.reset();
    fill_stencil_.reset();
    fill_cover_.reset();
    mask_clear_.reset();
    srb_.reset();
    uniforms_.reset();
    vertices_.reset();
    segments_.reset();
    corners_.reset();
    segment_capacity_ = 0;
    vertex_capacity_  = 0;
    uniform_capacity_ = 0;
    corners_uploaded_ = false;
}

bool RhiBackend::ensure_resources(QRhi* rhi, QRhiRenderPassDescriptor* rp, int sample_count)
{
    if (rhi_ == rhi && rp_ == rp && samples_ == sample_count && line_) return true;

    release();
    rhi_     = rhi;
    rp_      = rp;
    samples_ = sample_count;

    uniform_stride_ = static_cast<quint32>(
        std::max<int>(rhi->ubufAlignment(), static_cast<int>(sizeof(Uniforms))));

    const QShader line_vs = load_shader(":/kentos_cad/shaders/line.vert.qsb");
    const QShader line_fs = load_shader(":/kentos_cad/shaders/line.frag.qsb");
    const QShader fill_vs = load_shader(":/kentos_cad/shaders/fill.vert.qsb");
    const QShader fill_fs = load_shader(":/kentos_cad/shaders/fill.frag.qsb");
    if (!line_vs.isValid() || !line_fs.isValid() || !fill_vs.isValid() || !fill_fs.isValid())
        return false;

    corners_.reset(
        rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kLineCorners)));
    if (!corners_->create()) return false;

    uniforms_.reset(
        rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, uniform_stride_ * 64));
    if (!uniforms_->create()) return false;
    uniform_capacity_ = uniform_stride_ * 64;

    srb_.reset(rhi->newShaderResourceBindings());
    srb_->setBindings({QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
        0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
        uniforms_.get(), sizeof(Uniforms))});
    if (!srb_->create()) return false;

    QRhiGraphicsPipeline::TargetBlend blend;
    blend.enable   = true;
    blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
    blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
    blend.srcAlpha = QRhiGraphicsPipeline::One;
    blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

    // ---- lines: one instance per segment, widened in the vertex shader (R5) --
    {
        QRhiVertexInputLayout layout;
        layout.setBindings({
            {2 * sizeof(float)},                                      // corners
            {5 * sizeof(float), QRhiVertexInputBinding::PerInstance}, // segments
        });
        layout.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float2, 0},
            {1, 1, QRhiVertexInputAttribute::Float2, 0},
            {1, 2, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)},
            {1, 3, QRhiVertexInputAttribute::Float, 4 * sizeof(float)},
        });

        line_.reset(rhi->newGraphicsPipeline());
        line_->setShaderStages(
            {{QRhiShaderStage::Vertex, line_vs}, {QRhiShaderStage::Fragment, line_fs}});
        line_->setVertexInputLayout(layout);
        line_->setShaderResourceBindings(srb_.get());
        line_->setRenderPassDescriptor(rp);
        line_->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        line_->setCullMode(QRhiGraphicsPipeline::None);
        line_->setDepthTest(false);
        line_->setDepthWrite(false);
        line_->setSampleCount(sample_count);
        line_->setTargetBlends({blend});
        if (!line_->create()) return false;
    }

    // ---- fills: even-odd stencil, then one cover quad ------------------------
    //
    // No triangulator, on purpose. A stencil invert over a triangle fan produces
    // the even-odd rule the QPainter backend gets from `Qt::OddEvenFill`, which is
    // what punches a courtyard ring out of its parcel without either backend
    // having to know which ring was declared a hole. Adding CDT for this would be
    // a dependency for a thing the depth-stencil buffer already does.
    QRhiVertexInputLayout flat;
    flat.setBindings({{2 * sizeof(float)}});
    flat.setAttributes({{0, 0, QRhiVertexInputAttribute::Float2, 0}});

    {
        QRhiGraphicsPipeline::StencilOpState op;
        op.failOp      = QRhiGraphicsPipeline::Keep;
        op.depthFailOp = QRhiGraphicsPipeline::Keep;
        op.passOp      = QRhiGraphicsPipeline::Invert;
        op.compareOp   = QRhiGraphicsPipeline::Always;

        QRhiGraphicsPipeline::TargetBlend none;
        none.enable     = false;
        none.colorWrite = {}; // stencil only: this pass writes no pixels

        fill_stencil_.reset(rhi->newGraphicsPipeline());
        fill_stencil_->setShaderStages(
            {{QRhiShaderStage::Vertex, fill_vs}, {QRhiShaderStage::Fragment, fill_fs}});
        fill_stencil_->setVertexInputLayout(flat);
        fill_stencil_->setShaderResourceBindings(srb_.get());
        fill_stencil_->setRenderPassDescriptor(rp);
        fill_stencil_->setTopology(QRhiGraphicsPipeline::Triangles);
        fill_stencil_->setCullMode(QRhiGraphicsPipeline::None);
        fill_stencil_->setDepthTest(false);
        fill_stencil_->setDepthWrite(false);
        fill_stencil_->setStencilTest(true);
        fill_stencil_->setStencilFront(op);
        fill_stencil_->setStencilBack(op);
        fill_stencil_->setStencilReadMask(0xFF);
        fill_stencil_->setStencilWriteMask(0xFF);
        fill_stencil_->setSampleCount(sample_count);
        fill_stencil_->setTargetBlends({none});
        if (!fill_stencil_->create()) return false;
    }

    {
        // The cover pass paints where the stencil is odd AND resets it to zero as
        // it goes, so the next fill starts from a clean buffer without a second
        // clear. `failOp = Zero` is what does the resetting on the even pixels.
        QRhiGraphicsPipeline::StencilOpState op;
        op.failOp      = QRhiGraphicsPipeline::StencilZero;
        op.depthFailOp = QRhiGraphicsPipeline::StencilZero;
        op.passOp      = QRhiGraphicsPipeline::StencilZero;
        op.compareOp   = QRhiGraphicsPipeline::NotEqual;

        fill_cover_.reset(rhi->newGraphicsPipeline());
        fill_cover_->setShaderStages(
            {{QRhiShaderStage::Vertex, fill_vs}, {QRhiShaderStage::Fragment, fill_fs}});
        fill_cover_->setVertexInputLayout(flat);
        fill_cover_->setShaderResourceBindings(srb_.get());
        fill_cover_->setRenderPassDescriptor(rp);
        fill_cover_->setTopology(QRhiGraphicsPipeline::Triangles);
        fill_cover_->setCullMode(QRhiGraphicsPipeline::None);
        fill_cover_->setDepthTest(false);
        fill_cover_->setDepthWrite(false);
        fill_cover_->setStencilTest(true);
        fill_cover_->setStencilFront(op);
        fill_cover_->setStencilBack(op);
        fill_cover_->setStencilReadMask(0xFF);
        fill_cover_->setStencilWriteMask(0xFF);
        fill_cover_->setSampleCount(sample_count);
        fill_cover_->setTargetBlends({blend});
        if (!fill_cover_->create()) return false;
    }

    // ---- the mask, and what draws inside it ---------------------------------
    //
    // A pattern fill is the same even-odd stencil as a solid one, used
    // differently: instead of covering the odd pixels with a colour, the mask is
    // LEFT STANDING and the hatch or the glyph grid is drawn through it. That is
    // the whole clipping mechanism, and it needs no clip rectangle, no scissor
    // and no triangulator — which is the reason a face with a courtyard in it
    // comes out with the hole unhatched.
    {
        QRhiGraphicsPipeline::StencilOpState keep;
        keep.failOp      = QRhiGraphicsPipeline::Keep;
        keep.depthFailOp = QRhiGraphicsPipeline::Keep;
        keep.passOp      = QRhiGraphicsPipeline::Keep;
        keep.compareOp   = QRhiGraphicsPipeline::NotEqual;

        const auto clipped = [&](std::unique_ptr<QRhiGraphicsPipeline>& into,
                                 const QRhiVertexInputLayout& layout, const QShader& vs,
                                 const QShader& fs, QRhiGraphicsPipeline::Topology topology) {
            into.reset(rhi->newGraphicsPipeline());
            into->setShaderStages({{QRhiShaderStage::Vertex, vs}, {QRhiShaderStage::Fragment, fs}});
            into->setVertexInputLayout(layout);
            into->setShaderResourceBindings(srb_.get());
            into->setRenderPassDescriptor(rp);
            into->setTopology(topology);
            into->setCullMode(QRhiGraphicsPipeline::None);
            into->setDepthTest(false);
            into->setDepthWrite(false);
            into->setStencilTest(true);
            into->setStencilFront(keep);
            into->setStencilBack(keep);
            into->setStencilReadMask(0xFF);
            into->setStencilWriteMask(0x00); // reads the mask, never disturbs it
            into->setSampleCount(sample_count);
            into->setTargetBlends({blend});
            return into->create();
        };

        QRhiVertexInputLayout line_layout;
        line_layout.setBindings({
            {2 * sizeof(float)},
            {5 * sizeof(float), QRhiVertexInputBinding::PerInstance},
        });
        line_layout.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float2, 0},
            {1, 1, QRhiVertexInputAttribute::Float2, 0},
            {1, 2, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)},
            {1, 3, QRhiVertexInputAttribute::Float, 4 * sizeof(float)},
        });

        if (!clipped(line_clip_, line_layout, line_vs, line_fs,
                     QRhiGraphicsPipeline::TriangleStrip))
            return false;
        if (!clipped(tri_clip_, flat, fill_vs, fill_fs, QRhiGraphicsPipeline::Triangles))
            return false;

        // The unclipped triangle list: a marker's interior, drawn on the geometry
        // rather than inside a face.
        tri_.reset(rhi->newGraphicsPipeline());
        tri_->setShaderStages(
            {{QRhiShaderStage::Vertex, fill_vs}, {QRhiShaderStage::Fragment, fill_fs}});
        tri_->setVertexInputLayout(flat);
        tri_->setShaderResourceBindings(srb_.get());
        tri_->setRenderPassDescriptor(rp);
        tri_->setTopology(QRhiGraphicsPipeline::Triangles);
        tri_->setCullMode(QRhiGraphicsPipeline::None);
        tri_->setDepthTest(false);
        tri_->setDepthWrite(false);
        tri_->setSampleCount(sample_count);
        tri_->setTargetBlends({blend});
        if (!tri_->create()) return false;

        // The instanced marker pipeline. Two bindings, exactly as the text one
        // has: the glyph's own geometry per vertex, the stamp per instance. The
        // clipped variant reads the same stencil a pattern fill sets, which is
        // how a dot grid stays inside its parcel.
        const QShader marker_vs = load_shader(":/kentos_cad/shaders/marker.vert.qsb");
        const QShader marker_fs = load_shader(":/kentos_cad/shaders/marker.frag.qsb");
        if (!marker_vs.isValid() || !marker_fs.isValid()) return false;

        marker_verts_.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 4096));
        if (!marker_verts_->create()) return false;
        marker_vertex_capacity_ = 4096;

        marker_instances_.reset(
            rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, 65536));
        if (!marker_instances_->create()) return false;
        marker_instance_capacity_ = 65536;

        QRhiVertexInputLayout marker_layout;
        marker_layout.setBindings({
            {2 * sizeof(float)},                                      // glyph-local xy
            {4 * sizeof(float), QRhiVertexInputBinding::PerInstance}, // stamps
        });
        marker_layout.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float2, 0},
            {1, 1, QRhiVertexInputAttribute::Float4, 0},
        });

        if (!clipped(marker_clip_, marker_layout, marker_vs, marker_fs,
                     QRhiGraphicsPipeline::Triangles))
            return false;

        marker_.reset(rhi->newGraphicsPipeline());
        marker_->setShaderStages(
            {{QRhiShaderStage::Vertex, marker_vs}, {QRhiShaderStage::Fragment, marker_fs}});
        marker_->setVertexInputLayout(marker_layout);
        marker_->setShaderResourceBindings(srb_.get());
        marker_->setRenderPassDescriptor(rp);
        marker_->setTopology(QRhiGraphicsPipeline::Triangles);
        marker_->setCullMode(QRhiGraphicsPipeline::None);
        marker_->setDepthTest(false);
        marker_->setDepthWrite(false);
        marker_->setSampleCount(sample_count);
        marker_->setTargetBlends({blend});
        if (!marker_->create()) return false;

        // Clearing the mask afterwards. The same cover quad as a solid fill, with
        // the colour writes off: a mask left standing would clip the next face's
        // pattern to the previous face's shape.
        QRhiGraphicsPipeline::StencilOpState zero;
        zero.failOp      = QRhiGraphicsPipeline::StencilZero;
        zero.depthFailOp = QRhiGraphicsPipeline::StencilZero;
        zero.passOp      = QRhiGraphicsPipeline::StencilZero;
        zero.compareOp   = QRhiGraphicsPipeline::Always;

        QRhiGraphicsPipeline::TargetBlend silent;
        silent.enable     = false;
        silent.colorWrite = {};

        mask_clear_.reset(rhi->newGraphicsPipeline());
        mask_clear_->setShaderStages(
            {{QRhiShaderStage::Vertex, fill_vs}, {QRhiShaderStage::Fragment, fill_fs}});
        mask_clear_->setVertexInputLayout(flat);
        mask_clear_->setShaderResourceBindings(srb_.get());
        mask_clear_->setRenderPassDescriptor(rp);
        mask_clear_->setTopology(QRhiGraphicsPipeline::Triangles);
        mask_clear_->setCullMode(QRhiGraphicsPipeline::None);
        mask_clear_->setDepthTest(false);
        mask_clear_->setDepthWrite(false);
        mask_clear_->setStencilTest(true);
        mask_clear_->setStencilFront(zero);
        mask_clear_->setStencilBack(zero);
        mask_clear_->setStencilReadMask(0xFF);
        mask_clear_->setStencilWriteMask(0xFF);
        mask_clear_->setSampleCount(sample_count);
        mask_clear_->setTargetBlends({silent});
        if (!mask_clear_->create()) return false;
    }

    // ---- published pictures: one instanced textured quad per stamp ----------
    //
    // The SAME vertex shader the text pipeline uses, because the two draw the same
    // thing: a rotated quad at a place, with a rectangle of a texture on it. Only
    // the fragment stage differs — a glyph reconstructs its coverage from a
    // distance field, a published picture is already a picture.
    //
    // The PIPELINES are not built here. Each picture gets its own, beside its own
    // texture and bindings, once it is known — see `Picture`.
    {
        picture_vs_ = load_shader(":/kentos_cad/shaders/text.vert.qsb");
        picture_fs_ = load_shader(":/kentos_cad/shaders/image.frag.qsb");
        if (!picture_vs_.isValid() || !picture_fs_.isValid()) return false;

        picture_quad_.reset(
            rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kQuadCorners)));
        if (!picture_quad_->create()) return false;
        picture_quad_uploaded_ = false;

        // LINEAR and CLAMP. A published glyph is drawn at whatever size the scale
        // asks for, so it is almost never sampled one-to-one; nearest sampling
        // makes the annex's own line work crawl as the user zooms.
        picture_sampler_.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear,
                                               QRhiSampler::None, QRhiSampler::ClampToEdge,
                                               QRhiSampler::ClampToEdge));
        if (!picture_sampler_->create()) return false;

        picture_input_ = {};
        picture_input_.setBindings({
            {2 * sizeof(float)},
            {13 * sizeof(float), QRhiVertexInputBinding::PerInstance},
        });
        picture_input_.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float2, 0},
            {1, 1, QRhiVertexInputAttribute::Float4, 0},
            {1, 2, QRhiVertexInputAttribute::Float4, 4 * sizeof(float)},
            {1, 3, QRhiVertexInputAttribute::Float4, 8 * sizeof(float)},
            {1, 4, QRhiVertexInputAttribute::UNormByte4, 12 * sizeof(float)},
        });

        // Every cached picture's bindings and pipelines named the old uniform
        // buffer and the old render pass, so they are dropped rather than reused.
        pictures_.clear();
    }

#if KENTOS_HAVE_TEXT
    // ---- text: one instanced quad per glyph against the SDF atlas (R8) -------
    ensure_atlas();
    if (atlas_) {
        const QShader text_vs = load_shader(":/kentos_cad/shaders/text.vert.qsb");
        const QShader text_fs = load_shader(":/kentos_cad/shaders/text.frag.qsb");
        if (!text_vs.isValid() || !text_fs.isValid()) return false;

        quad_.reset(
            rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kQuadCorners)));
        if (!quad_->create()) return false;

        // LINEAR, and CLAMP. Linear because the field is meant to be interpolated —
        // nearest sampling turns the median back into a staircase and undoes the
        // whole point. Clamp because two glyphs share the sheet: a repeat at the
        // edge would fetch a neighbour's field and put a sliver of someone else's
        // letter on this one.
        sampler_.reset(rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
                                       QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        if (!sampler_->create()) return false;

        QRhiResourceUpdateBatch* atlas_rub = rhi->nextResourceUpdateBatch();
        if (!ensure_atlas_texture(rhi, atlas_rub)) {
            atlas_rub->release();
            return false;
        }
        // The batch is handed to the first pass of the frame; holding it here would
        // leak it, so it is submitted with the initial upload the next frame does.
        atlas_rub->release();
        atlas_uploaded_ = 0;

        srb_text_.reset(rhi->newShaderResourceBindings());
        srb_text_->setBindings({
            QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                0,
                QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
                uniforms_.get(), sizeof(Uniforms)),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                      atlas_texture_.get(), sampler_.get()),
        });
        if (!srb_text_->create()) return false;

        QRhiVertexInputLayout layout;
        layout.setBindings({
            {2 * sizeof(float)},                                       // corners
            {13 * sizeof(float), QRhiVertexInputBinding::PerInstance}, // glyphs
        });
        layout.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float2, 0},
            {1, 1, QRhiVertexInputAttribute::Float4, 0},
            {1, 2, QRhiVertexInputAttribute::Float4, 4 * sizeof(float)},
            {1, 3, QRhiVertexInputAttribute::Float4, 8 * sizeof(float)},
            {1, 4, QRhiVertexInputAttribute::UNormByte4, 12 * sizeof(float)},
        });

        text_.reset(rhi->newGraphicsPipeline());
        text_->setShaderStages(
            {{QRhiShaderStage::Vertex, text_vs}, {QRhiShaderStage::Fragment, text_fs}});
        text_->setVertexInputLayout(layout);
        text_->setShaderResourceBindings(srb_text_.get());
        text_->setRenderPassDescriptor(rp);
        text_->setTopology(QRhiGraphicsPipeline::TriangleStrip);
        text_->setCullMode(QRhiGraphicsPipeline::None);
        text_->setDepthTest(false);
        text_->setDepthWrite(false);
        text_->setSampleCount(sample_count);
        text_->setTargetBlends({blend});
        if (!text_->create()) return false;
    }
#endif

    return true;
}

#if KENTOS_HAVE_TEXT

void RhiBackend::ensure_atlas()
{
    // ONCE. A missing font directory is a packaging failure, and retrying it every
    // frame would turn one report into sixty a second.
    if (atlas_tried_) return;
    atlas_tried_ = true;

    auto opened = render::TextAtlas::open(data_path("fonts"));
    if (!opened) return;

    atlas_ = std::move(opened.value());
}

bool RhiBackend::ensure_atlas_texture(QRhi* rhi, QRhiResourceUpdateBatch* rub)
{
    if (!atlas_) return false;

    const int side = atlas_->width();
    if (!atlas_texture_ || atlas_side_ != side) {
        atlas_texture_.reset(rhi->newTexture(QRhiTexture::RGBA8, QSize(side, side)));
        if (!atlas_->pixels().empty() && !atlas_texture_->create()) return false;
        atlas_side_     = side;
        atlas_uploaded_ = 0;

        // The bindings name the texture, so a new texture needs a new set. Same
        // LAYOUT, which is what lets the pipeline stay as it is.
        if (srb_text_) {
            srb_text_->setBindings({
                QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                    0,
                    QRhiShaderResourceBinding::VertexStage |
                        QRhiShaderResourceBinding::FragmentStage,
                    uniforms_.get(), sizeof(Uniforms)),
                QRhiShaderResourceBinding::sampledTexture(1,
                                                          QRhiShaderResourceBinding::FragmentStage,
                                                          atlas_texture_.get(), sampler_.get()),
            });
            if (!srb_text_->create()) return false;
        }
    }

    // ONLY WHEN IT MOVED. The atlas fills over the first frames a drawing is on
    // screen and is static after that; re-uploading a 4 MB sheet every frame would
    // cost more than everything else in this file put together.
    if (atlas_->revision() != atlas_uploaded_) {
        QImage image(atlas_->pixels().data(), side, side, static_cast<qsizetype>(side) * 4,
                     QImage::Format_RGBA8888);
        rub->uploadTexture(atlas_texture_.get(), image.copy());
        atlas_uploaded_ = atlas_->revision();
    }

    return true;
}

#endif // KENTOS_HAVE_TEXT

bool RhiBackend::rebind_uniforms()
{
    // IN PLACE, on the objects that already exist — `setBindings` then `create()`
    // again, the way `ensure_atlas_texture` re-points the text bindings when the
    // atlas widens. The layout is unchanged, so every pipeline built against
    // these bindings stays valid. Handing out NEW bindings objects instead would
    // trade one dangling pointer for another: `line_`, `tri_`, `fill_cover_` and
    // the rest were all created against these very ones.
    const auto uniform_binding = [this] {
        return QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            uniforms_.get(), sizeof(Uniforms));
    };

    int bound = 0;

    if (srb_) {
        srb_->setBindings({uniform_binding()});
        if (!srb_->create()) return false;
        ++bound;
    }

#if KENTOS_HAVE_TEXT
    if (srb_text_ && atlas_texture_ && sampler_) {
        srb_text_->setBindings({
            uniform_binding(),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                      atlas_texture_.get(), sampler_.get()),
        });
        if (!srb_text_->create()) return false;
        ++bound;
    }
#endif

    // The cached pictures, which is the half that was missed: a picture's
    // bindings are built once, when its texture is uploaded, and then live as
    // long as the picture is in the cache — across every later growth of the
    // uniform buffer.
    for (auto& item : pictures_) {
        Picture& entry = item.second;
        if (!entry.srb || !entry.texture || !picture_sampler_) continue;
        entry.srb->setBindings({
            uniform_binding(),
            QRhiShaderResourceBinding::sampledTexture(1, QRhiShaderResourceBinding::FragmentStage,
                                                      entry.texture.get(), picture_sampler_.get()),
        });
        if (!entry.srb->create()) return false;
        ++bound;
    }

    // Developer tooling, an environment variable and not a feature (CLAUDE.md
    // 5.17). `scripts/ci-gate-rhi-omur.sh` needs to know this branch was actually
    // taken: a lifetime test run on a scene that never outgrows its uniform
    // buffer proves nothing, and would go on proving nothing quietly.
    if (qEnvironmentVariableIsSet("KENTOS_RHI_OMUR"))
        qInfo("[omur] tekduzen tampon %u bayta buyudu, %d baglama yeniden yonlendirildi",
              uniform_capacity_, bound);

    return true;
}

bool RhiBackend::ensure_capacity(QRhi* rhi, QRhiResourceUpdateBatch* rub)
{
    // Grown to a HIGH-WATER MARK, never per frame. render.md R6 asks for a
    // persistent mapped ring buffer with fences; this is the honest first step
    // toward it — a buffer that is recreated only when a frame needs more than
    // every frame before it did, which on a pan is never.
    //
    // DROPPING THE OLD BUFFER OBJECT IS SAFE BY ITSELF, and it is worth writing
    // down why, because the obvious worry is wrong: `~QRhiBuffer` runs
    // `destroy()`, and QRhi does not hand the native allocation back to the
    // driver there — it queues it until the frames that could still be reading it
    // have gone through. What is NOT safe is leaving something else pointing at
    // the buffer that just went away, which is exactly what the uniform buffer
    // below does to three sets of bindings.
    const auto grow = [&](std::unique_ptr<QRhiBuffer>& buf, quint32& capacity, quint32 needed) {
        if (needed == 0) return true;
        if (buf && capacity >= needed) return true;
        quint32 size = capacity ? capacity : 64u * 1024u;
        while (size < needed)
            size *= 2;

        buf.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, size));
        if (!buf->create()) return false;
        capacity = size;
        return true;
    };

    const quint32 seg_bytes = static_cast<quint32>(segment_data_.size() * sizeof(float));
    const quint32 vtx_bytes = static_cast<quint32>(vertex_data_.size() * sizeof(float));
    const quint32 uni_bytes = static_cast<quint32>(uniform_data_.size());

    if (!grow(segments_, segment_capacity_, seg_bytes)) return false;
    if (!grow(vertices_, vertex_capacity_, vtx_bytes)) return false;

    if (uni_bytes > uniform_capacity_) {
        quint32 size = uniform_capacity_ ? uniform_capacity_ : uniform_stride_ * 64;
        while (size < uni_bytes)
            size *= 2;
        uniforms_.reset(rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, size));
        if (!uniforms_->create()) return false;
        uniform_capacity_ = size;

        // AND EVERY SET OF BINDINGS FOLLOWS IT. This is the crash, not a tidy-up:
        // the uniform buffer was replaced under bindings that still named the old
        // one, and the driver read an allocation that no longer existed —
        //
        //     QRhiWidget::paintEvent -> QRhi::endOffscreenFrame -> libgallium -> SIGSEGV
        //
        // It felt random because this branch is reached only when a scene wants
        // more uniform slots than every scene before it did: importing a 48 MB
        // cadastral DXF is the reliable way there, and a small drawing never gets
        // near it. `srb_` was rebuilt here already; the text bindings and the
        // per-picture bindings were not, and the per-picture ones are built once
        // and then cached for the life of the picture.
        if (!rebind_uniforms()) return false;
    }

    if (!corners_uploaded_) {
        rub->uploadStaticBuffer(corners_.get(), 0, sizeof(kLineCorners), kLineCorners);
        corners_uploaded_ = true;
    }

    // ---- published pictures ------------------------------------------------
    if (!picture_keys_.empty()) {
        const quint32 bytes = static_cast<quint32>(picture_data_.size() * sizeof(float));
        if (!grow(picture_instances_, picture_capacity_, bytes)) return false;

        if (!picture_quad_uploaded_ && picture_quad_) {
            rub->uploadStaticBuffer(picture_quad_.get(), 0, sizeof(kQuadCorners), kQuadCorners);
            picture_quad_uploaded_ = true;
        }
        if (bytes > 0)
            rub->updateDynamicBuffer(picture_instances_.get(), 0, bytes, picture_data_.data());

        // A texture and its bindings are created ONCE per picture and kept: the
        // annex set is eight megabytes of source and a sheet draws a handful of
        // them, so the cache never needs pruning.
        for (const std::uint64_t key : picture_keys_) {
            Picture& entry = pictures_.at(key);
            if (!entry.pending) continue;

            entry.texture.reset(
                rhi->newTexture(QRhiTexture::RGBA8, QSize(entry.width, entry.height)));
            if (!entry.texture->create()) {
                qWarning("[rhi] resim dokusu oluşturulamadı %dx%d", entry.width, entry.height);
                return false;
            }
            rub->uploadTexture(entry.texture.get(), entry.cpu);

            entry.srb.reset(rhi->newShaderResourceBindings());
            entry.srb->setBindings({
                QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
                    0,
                    QRhiShaderResourceBinding::VertexStage |
                        QRhiShaderResourceBinding::FragmentStage,
                    uniforms_.get(), sizeof(Uniforms)),
                QRhiShaderResourceBinding::sampledTexture(
                    1, QRhiShaderResourceBinding::FragmentStage, entry.texture.get(),
                    picture_sampler_.get()),
            });
            if (!entry.srb->create()) {
                qWarning("[rhi] resim bağlamaları oluşturulamadı");
                return false;
            }

            QRhiGraphicsPipeline::TargetBlend blend;
            blend.enable   = true;
            blend.srcColor = QRhiGraphicsPipeline::SrcAlpha;
            blend.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            blend.srcAlpha = QRhiGraphicsPipeline::One;
            blend.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;

            const auto build = [&](std::unique_ptr<QRhiGraphicsPipeline>& into, bool clip) {
                into.reset(rhi->newGraphicsPipeline());
                into->setShaderStages({{QRhiShaderStage::Vertex, picture_vs_},
                                       {QRhiShaderStage::Fragment, picture_fs_}});
                into->setVertexInputLayout(picture_input_);
                into->setShaderResourceBindings(entry.srb.get());
                into->setRenderPassDescriptor(rp_);
                into->setTopology(QRhiGraphicsPipeline::TriangleStrip);
                into->setCullMode(QRhiGraphicsPipeline::None);
                into->setDepthTest(false);
                into->setDepthWrite(false);
                into->setSampleCount(samples_);
                into->setTargetBlends({blend});
                if (clip) {
                    QRhiGraphicsPipeline::StencilOpState keep;
                    keep.failOp      = QRhiGraphicsPipeline::Keep;
                    keep.depthFailOp = QRhiGraphicsPipeline::Keep;
                    keep.passOp      = QRhiGraphicsPipeline::Keep;
                    keep.compareOp   = QRhiGraphicsPipeline::NotEqual;
                    into->setStencilTest(true);
                    into->setStencilFront(keep);
                    into->setStencilBack(keep);
                    into->setStencilReadMask(0xFF);
                    into->setStencilWriteMask(0x00);
                }
                return into->create();
            };

            if (!build(entry.pipe, false)) {
                qWarning("[rhi] resim pipeline'ı oluşturulamadı");
                return false;
            }
            if (!build(entry.pipe_clip, true)) {
                qWarning("[rhi] kırpmalı resim pipeline'ı oluşturulamadı");
                return false;
            }

            entry.pending = false;
        }
    }

#if KENTOS_HAVE_TEXT
    if (atlas_) {
        const quint32 glyph_bytes = static_cast<quint32>(glyph_data_.size() * sizeof(float));
        if (!grow(glyphs_, glyph_capacity_, glyph_bytes)) return false;

        if (!quad_uploaded_ && quad_) {
            rub->uploadStaticBuffer(quad_.get(), 0, sizeof(kQuadCorners), kQuadCorners);
            quad_uploaded_ = true;
        }
        if (glyph_bytes > 0)
            rub->updateDynamicBuffer(glyphs_.get(), 0, glyph_bytes, glyph_data_.data());

        if (!ensure_atlas_texture(rhi, rub)) return false;
    }
#endif
    // The marker buffers. A few dozen bytes of glyph and sixteen bytes a stamp,
    // where the flat path uploaded the glyph's whole geometry once per stamp.
    {
        const quint32 marker_vertex_bytes =
            static_cast<quint32>(marker_vertex_data_.size() * sizeof(float));
        const quint32 marker_instance_bytes =
            static_cast<quint32>(marker_instance_data_.size() * sizeof(float));
        if (!grow(marker_verts_, marker_vertex_capacity_, marker_vertex_bytes)) return false;
        if (!grow(marker_instances_, marker_instance_capacity_, marker_instance_bytes))
            return false;
        if (marker_vertex_bytes > 0)
            rub->updateDynamicBuffer(marker_verts_.get(), 0, marker_vertex_bytes,
                                     marker_vertex_data_.data());
        if (marker_instance_bytes > 0)
            rub->updateDynamicBuffer(marker_instances_.get(), 0, marker_instance_bytes,
                                     marker_instance_data_.data());
    }

    if (seg_bytes > 0)
        rub->updateDynamicBuffer(segments_.get(), 0, seg_bytes, segment_data_.data());
    if (vtx_bytes > 0) rub->updateDynamicBuffer(vertices_.get(), 0, vtx_bytes, vertex_data_.data());
    if (uni_bytes > 0)
        rub->updateDynamicBuffer(uniforms_.get(), 0, uni_bytes, uniform_data_.data());

    return true;
}

// -----------------------------------------------------------------------------
// the frame
// -----------------------------------------------------------------------------

void RhiBackend::render(const render::DrawList& list, const render::Overlay& overlay,
                        const render::FrameContext& ctx)
{
    auto* target = static_cast<RhiFrameTarget*>(ctx.target);
    if (target == nullptr || target->rhi == nullptr || target->cb == nullptr ||
        target->rt == nullptr)
        return;

    QRhi* rhi = target->rhi;
    if (!ensure_resources(rhi, target->rt->renderPassDescriptor(), target->rt->sampleCount()))
        return;

    // WHAT CAN BE SEEN, for the pattern generators.
    //
    // THE MARGIN IS SMALL ON PURPOSE. A screen's worth on each side makes the
    // clipped region three times the width and three times the height — nine
    // times the area, and nine times the pattern — which throws away almost the
    // whole point of clipping. The generators already overshoot by a spacing of
    // their own so nothing pops in at the edge, so this only has to absorb
    // rounding.
    constexpr float kMargin = 8.0f;
    visible_ = render::PixelBox{-kMargin, -kMargin, static_cast<float>(ctx.width_px) + kMargin,
                                static_cast<float>(ctx.height_px) + kMargin};

    const QSize pixels = target->rt->pixelSize();
    if (pixels.isEmpty() || ctx.width_px <= 0 || ctx.height_px <= 0) return;

    // LOGICAL pixels in the matrix, DEVICE pixels in the viewport, and the
    // difference matters on every HiDPI screen. The scene builder measured this
    // frame against the widget's logical size, so that is the space its vertices
    // are in; the render target is device pixels. Mapping logical to clip and
    // letting the viewport stretch is what keeps a 1 px boundary one pixel wide
    // at 200% scaling instead of half of one (render.md R19).
    //
    // The API's own clip-space correction is folded in, so the same shader is
    // right under Vulkan, Metal, D3D and OpenGL.
    QMatrix4x4 mvp = rhi->clipSpaceCorrMatrix();
    mvp.ortho(0.0f, static_cast<float>(ctx.width_px), static_cast<float>(ctx.height_px), 0.0f,
              -1.0f, 1.0f);
    std::memcpy(proto_.mvp, mvp.constData(), sizeof(proto_.mvp));

    // KEEP THE CAPACITY. `clear()` on a vector does not release its buffer, which
    // is what makes the second frame allocation-free (render.md R20, P6).
    segment_data_.clear();
    vertex_data_.clear();
    marker_vertex_data_.clear();
    marker_instance_data_.clear();
    uniform_data_.clear();
    cmds_.clear();
    picture_data_.clear();
    picture_keys_.clear();
#if KENTOS_HAVE_TEXT
    glyph_data_.clear();
    px_range_ = atlas_ ? atlas_->px_range() : 0.0f;
#endif

    // Document coordinates arrive centre-relative with y UP; the vertex buffer
    // wants widget pixels with y DOWN. One conversion, in one place — the same
    // one the QPainter backend performs.
    const double cx = ctx.width_px * 0.5;
    const double cy = ctx.height_px * 0.5;
    last_cx_        = cx;
    last_cy_        = cy;

    // KENTOS_RHI_DEBUG=2 draws the DOCUMENT ALONE, with the overlay left out.
    // Bisecting a frame is the only way to tell "the batch never reached the
    // buffer" from "the batch was drawn and something later covered it", and from
    // a screenshot the two look the same.
    const QByteArray debug = qgetenv("KENTOS_RHI_DEBUG");

    if (debug != "2") paint_ground(overlay);
    emit_document(list, cx, cy);
    if (debug != "2" && debug != "3") paint_aids(list, overlay);

    // Developer tooling, the same category as `KENTOS_FRAME_DUMP`: an environment
    // variable rather than a feature, and there is no user-facing behaviour here
    // to document (CLAUDE.md 5.17). What a GPU frame CONTAINS is otherwise
    // invisible — a batch that never reached the buffer and a batch drawn off
    // screen look identical, and telling them apart by staring at a screenshot is
    // how an afternoon goes.
    if (!qEnvironmentVariableIsEmpty("KENTOS_RHI_DEBUG")) {
        std::size_t fills = 0;
        std::size_t lines = 0;
        std::size_t texts = 0;
        for (const Cmd& cmd : cmds_) {
            if (cmd.kind == Cmd::Kind::Fill) ++fills;
            if (cmd.kind == Cmd::Kind::Line) lines += cmd.count;
            if (cmd.kind == Cmd::Kind::Text) texts += cmd.count;
        }
        const auto span = [](const std::vector<float>& v) {
            if (v.empty()) return std::pair<double, double>{0.0, 0.0};
            auto lo = *std::min_element(v.begin(), v.end());
            auto hi = *std::max_element(v.begin(), v.end());
            return std::pair<double, double>{static_cast<double>(lo), static_cast<double>(hi)};
        };
        const auto seg = span(segment_data_);
        const auto vtx = span(vertex_data_);
        const auto pic = span(picture_data_);
        qWarning("[rhi] aralıklar: segman [%.0f %.0f] üçgen [%.0f %.0f] resim [%.0f %.0f]",
                 seg.first, seg.second, vtx.first, vtx.second, pic.first, pic.second);

        qWarning("[rhi] geçiş %zu · sıra %zu · komut %zu | dolgu %zu · segman %zu · glif %zu"
                 " | metin öğesi %zu · overlay %zu · tuval %dx%d",
                 list.passes.size(), list.order.size(), cmds_.size(), fills, lines, texts,
                 list.texts.size(), overlay.batches.size(), ctx.width_px, ctx.height_px);

        for (std::uint32_t index : list.order) {
            if (index >= list.passes.size()) continue;
            const render::PassStyle& ps     = list.passes[index];
            const render::PolylineBatch& st = list.polylines[index];
            const render::PolygonBatch& fl  = list.polygons[index];
            qWarning("[rhi]   geçiş %u tip=%d kabul=%d | çizgi runs=%zu rgba=%08x w=%.2f"
                     " | dolgu runs=%zu rgba=%08x",
                     index, static_cast<int>(ps.type), handles(ps.type) ? 1 : 0, st.runs.size(),
                     st.rgba, static_cast<double>(st.width_px), fl.runs.size(), fl.rgba);
            if (!st.xs.empty())
                qWarning("[rhi]     ilk nokta ekranda (%.1f, %.1f)",
                         cx + static_cast<double>(st.xs[0]), cy - static_cast<double>(st.ys[0]));
        }
    }

    QRhiResourceUpdateBatch* rub = rhi->nextResourceUpdateBatch();
    if (!ensure_capacity(rhi, rub)) {
        rub->release();
        return;
    }

    float clear[4] = {};
    unpack(overlay.background_rgba, clear);
    const QColor background =
        QColor::fromRgbF(clear[0], clear[1], clear[2], clear[3] == 0.0f ? 1.0f : clear[3]);

    QRhiCommandBuffer* cb = target->cb;
    cb->beginPass(target->rt, background, {1.0f, 0}, rub);
    cb->setViewport(QRhiViewport(0.0f, 0.0f, static_cast<float>(pixels.width()),
                                 static_cast<float>(pixels.height())));

    const QRhiCommandBuffer::VertexInput flat_input[1] = {{vertices_.get(), 0}};

    // BISECTING A FRAME, which is how all three of this backend's drawing defects
    // were found. `KENTOS_RHI_ONLY=resim` draws only the published pictures and
    // `=resimsiz` draws everything else — a batch that never reached the buffer
    // and a batch drawn off screen look identical in a screenshot, and so does a
    // pipeline that corrupts the state of the draws after it. Any single kind
    // works too, by the Turkish name `kind_name` gives it: `=cizgi` keeps only
    // the lines, `=-cizgi` keeps everything BUT the lines. Both directions are
    // needed — one says the kind draws, the other says the kind is what breaks
    // the draws after it. Developer tooling, an environment variable rather than
    // a feature (CLAUDE.md 5.17).
    const QByteArray only = qgetenv("KENTOS_RHI_ONLY");

    // COUNTED WHERE THEY ARE SUBMITTED, not estimated from the command list: a
    // stencil fill is two draws and a plain line is one, and R7's budget is
    // about what the GPU was asked to do rather than about how many commands the
    // scene builder produced.
    std::uint32_t draws = 0;

    // One name per kind, so a frame can be cut down to a single pipeline. A crash
    // inside the GPU driver names no draw call of ours; halving the frame does.
    const auto kind_name = [](Cmd::Kind k) {
        switch (k) {
        case Cmd::Kind::Fill: return "dolgu";
        case Cmd::Kind::Mask: return "maske";
        case Cmd::Kind::Unmask: return "maskesiz";
        case Cmd::Kind::Line: return "cizgi";
        case Cmd::Kind::Tri: return "ucgen";
        case Cmd::Kind::Marker: return "isaretci";
        case Cmd::Kind::Text: return "yazi";
        case Cmd::Kind::Image: return "resim";
        }
        return "?";
    };

    for (const Cmd& cmd : cmds_) {
        if (only == "resim" && cmd.kind != Cmd::Kind::Image) continue;
        if (only == "resimsiz" && cmd.kind == Cmd::Kind::Image) continue;
        if (!only.isEmpty() && only != "resim" && only != "resimsiz") {
            // `=cizgi` keeps only that kind; `=-cizgi` drops it and keeps the rest.
            const QByteArray name = kind_name(cmd.kind);
            if (only.startsWith('-')) {
                if (only.mid(1) == name) continue;
            } else if (only != name) {
                continue;
            }
        }

        const quint32 offset = cmd.uniform * uniform_stride_;
        const QRhiCommandBuffer::DynamicOffset dyn(0, offset);

        if (cmd.kind == Cmd::Kind::Line) {
            // The instance stream starts AT the batch, so every draw asks for
            // instance zero — see `Cmd::first`.
            const QRhiCommandBuffer::VertexInput line_inputs[2] = {{corners_.get(), 0},
                                                                   {segments_.get(), cmd.first}};
            cb->setGraphicsPipeline(cmd.clipped ? line_clip_.get() : line_.get());
            cb->setShaderResources(srb_.get(), 1, &dyn);
            cb->setVertexInput(0, 2, line_inputs);
            if (cmd.clipped) cb->setStencilRef(0);
            cb->draw(4, cmd.count);
            ++draws;
            continue;
        }

        if (cmd.kind == Cmd::Kind::Tri) {
            cb->setGraphicsPipeline(cmd.clipped ? tri_clip_.get() : tri_.get());
            cb->setShaderResources(srb_.get(), 1, &dyn);
            cb->setVertexInput(0, 1, flat_input);
            if (cmd.clipped) cb->setStencilRef(0);
            cb->draw(cmd.count, 1, cmd.first, 0);
            ++draws;
            continue;
        }

        if (cmd.kind == Cmd::Kind::Marker) {
            if (!marker_verts_ || !marker_instances_) continue;
            const QRhiCommandBuffer::VertexInput inputs[2] = {
                // The BYTE offset of the glyph's first vertex. Widened before the
                // multiply, not after: a glyph list is small today and the product
                // of two 32-bit values is a 32-bit value, which is a silent wrap
                // waiting for the day it is not.
                {marker_verts_.get(), static_cast<quint64>(cmd.glyph_first) * 2u * sizeof(float)},
                {marker_instances_.get(), cmd.first}};
            cb->setGraphicsPipeline(cmd.clipped ? marker_clip_.get() : marker_.get());
            cb->setShaderResources(srb_.get(), 1, &dyn);
            cb->setVertexInput(0, 2, inputs);
            if (cmd.clipped) cb->setStencilRef(0);
            cb->draw(cmd.glyph_count, cmd.count);
            ++draws;
            continue;
        }

        if (cmd.kind == Cmd::Kind::Image) {
            if (cmd.image == 0 || cmd.image > picture_keys_.size()) continue;
            const Picture& entry = pictures_.at(picture_keys_[cmd.image - 1]);
            if (!entry.srb || !entry.pipe || !picture_instances_) continue;

            const QRhiCommandBuffer::VertexInput inputs[2] = {
                {picture_quad_.get(), 0}, {picture_instances_.get(), cmd.first}};
            cb->setGraphicsPipeline(cmd.clipped ? entry.pipe_clip.get() : entry.pipe.get());
            cb->setShaderResources(entry.srb.get(), 1, &dyn);
            cb->setVertexInput(0, 2, inputs);
            if (cmd.clipped) cb->setStencilRef(0);
            cb->draw(4, cmd.count);
            ++draws;
            continue;
        }

        if (cmd.kind == Cmd::Kind::Mask) {
            // Stencil the rings and STOP. What draws inside the mask is the next
            // command; the mask is cleared by the matching Unmask.
            cb->setGraphicsPipeline(fill_stencil_.get());
            cb->setShaderResources(srb_.get(), 1, &dyn);
            cb->setVertexInput(0, 1, flat_input);
            cb->setStencilRef(0);
            cb->draw(cmd.count, 1, cmd.first, 0);
            ++draws;
            continue;
        }

        if (cmd.kind == Cmd::Kind::Unmask) {
            cb->setGraphicsPipeline(mask_clear_.get());
            cb->setShaderResources(srb_.get(), 1, &dyn);
            cb->setVertexInput(0, 1, flat_input);
            cb->setStencilRef(0);
            cb->draw(6, 1, cmd.cover, 0);
            ++draws;
            continue;
        }

#if KENTOS_HAVE_TEXT
        if (cmd.kind == Cmd::Kind::Text) {
            if (!text_ || !glyphs_) continue;
            const QRhiCommandBuffer::VertexInput text_inputs[2] = {{quad_.get(), 0},
                                                                   {glyphs_.get(), cmd.first}};
            cb->setGraphicsPipeline(text_.get());
            cb->setShaderResources(srb_text_.get(), 1, &dyn);
            cb->setVertexInput(0, 2, text_inputs);
            cb->draw(4, cmd.count);
            ++draws;
            continue;
        }
#endif

        cb->setGraphicsPipeline(fill_stencil_.get());
        cb->setShaderResources(srb_.get(), 1, &dyn);
        cb->setVertexInput(0, 1, flat_input);
        cb->setStencilRef(0);
        cb->draw(cmd.count, 1, cmd.first, 0);
        ++draws;

        cb->setGraphicsPipeline(fill_cover_.get());
        cb->setShaderResources(srb_.get(), 1, &dyn);
        cb->setVertexInput(0, 1, flat_input);
        cb->setStencilRef(0);
        cb->draw(6, 1, cmd.cover, 0);
        ++draws;
    }

    // WHAT THE FRAME IS MADE OF, when a probe asks. `draw_calls` and `vertices`
    // say a frame is expensive; they do not say WHICH symbol layer made it
    // expensive, and the answer has twice been guessed at from a catalogue entry
    // and twice been wrong. Developer tooling behind an environment variable,
    // same category as KENTOS_FRAME_TIMES — no /docs page, no user-facing flag.
    static const bool breakdown = qEnvironmentVariableIsSet("KENTOS_FRAME_PARTS");
    if (breakdown) {
        struct Part
        {
            const char* name;
            std::uint32_t count;
            std::uint32_t verts;
        };

        Part parts[8]{{"Fill", 0, 0}, {"Mask", 0, 0},   {"Unmask", 0, 0}, {"Line", 0, 0},
                      {"Tri", 0, 0},  {"Marker", 0, 0}, {"Text", 0, 0},   {"Image", 0, 0}};
        for (const Cmd& c : cmds_) {
            Part& part = parts[static_cast<std::size_t>(c.kind)];
            ++part.count;
            part.verts += c.count;
        }
        (void)std::fprintf(stdout, "[parca]");
        for (const Part& part : parts)
            if (part.count != 0)
                (void)std::fprintf(stdout, "  %s %u/%u", part.name, part.count, part.verts);
        (void)std::fprintf(stdout, "\n");
        (void)std::fflush(stdout);
    }

    // Published for R7. The passes and vertices come with it, because a hundred
    // draw calls over five million polygons is batching working and over five is
    // not — the count on its own says nothing.
    stats_.draw_calls = draws;
    stats_.passes     = static_cast<std::uint32_t>(list.passes.size());
    stats_.vertices =
        static_cast<std::uint32_t>(vertex_data_.size() / 2 + segment_data_.size() / 5);

    cb->endPass();
}

} // namespace

void* rhi_frame_target(QRhi* rhi, QRhiCommandBuffer* cb, QRhiRenderTarget* rt)
{
    // A function-local static rather than a new object per frame: the canvas fills
    // this once per paint and the backend reads it inside the same call, so there
    // is exactly one live at a time and no allocation in the draw loop (R20).
    // QRhiWidget renders on the GUI thread, which is the only caller.
    static RhiFrameTarget target;
    target.rhi = rhi;
    target.cb  = cb;
    target.rt  = rt;
    return &target;
}

std::unique_ptr<render::Backend> make_rhi_backend()
{
    return std::make_unique<RhiBackend>();
}

} // namespace kentos::app
