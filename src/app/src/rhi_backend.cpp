// SPDX-License-Identifier: GPL-3.0-or-later
// PiriCAD — app: the QRhi canvas backend (piricad.md §6.3, CLAUDE.md Article 8.1).
//
// WHAT THIS IS. The GPU implementation of `render::Backend`. It consumes exactly
// the same `DrawList` and `Overlay` the QPainter backend consumes, so nothing
// above `render::Backend` was edited to make it exist — which is the claim
// Article 8.1 makes and the one render.md R1 exists to keep true.
//
// WHAT THIS IS NOT, YET. The first slice draws GEOMETRY: polygon fills, strokes
// and the overlay. It does not draw text, published raster symbols, pattern fills
// or marker lines. Those are `handles()`'s job to refuse rather than claim: the
// QGIS backend once decided what it could draw by exclusion and silently dropped
// three raster types that way (`scripts/ci-gate-backends.sh` exists because of
// it), so this one lists what it CAN draw and refuses everything else.
//
// Text specifically waits on render.md R8 — an msdfgen SDF atlas shaped with
// HarfBuzz + FreeType. Drawing it with QPainter here is not an option: render.md
// P5 forbids Qt painting inside the QRhi path, and a second text renderer would
// disagree with the first about where a caption sits.
//
// COORDINATE SPACES, which are two and must not be confused (drawlist.hpp):
//   * document batches are CENTRE-RELATIVE with y UP — what the origin offset of
//     render.md R2 produces, and what a vertex buffer wants;
//   * overlay batches are WIDGET PIXELS with y DOWN — what a cursor position and
//     a viewport already are.
// Both become widget pixels here, once, on the way into the vertex buffer.
#include "piricad/app/backend_factory.hpp"

#include "piricad/render/drawlist.hpp"

#include <rhi/qrhi.h>

#include <QByteArray>
#include <QColor>
#include <QFile>
#include <QMatrix4x4>
#include <QSize>
#include <QString>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace piricad::app {
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
    float params[4]{}; ///< x: half line width in device pixels
};

static_assert(sizeof(Uniforms) == 96, "the shaders declare mat4 + vec4 + vec4");

/// Unit quad for the line pipeline: x runs along the segment, y picks the side.
/// Four vertices as a triangle strip, the cheapest quad there is.
constexpr float kLineCorners[8] = {
    0.0f, -1.0f, //
    0.0f, 1.0f,  //
    1.0f, -1.0f, //
    1.0f, 1.0f,  //
};

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
        Fill, ///< stencil the rings, then cover the bounding box
        Line, ///< instanced segment quads
    };

    Kind kind{Kind::Line};
    std::uint32_t uniform{0}; ///< slot in the uniform buffer
    std::uint32_t first{0};   ///< first vertex (Fill) or first instance (Line)
    std::uint32_t count{0};   ///< vertices (Fill) or instances (Line)
    std::uint32_t cover{0};   ///< Fill only: first vertex of its 6-vertex cover quad
};

class RhiBackend final : public render::Backend
{
public:
    std::string name() const override { return "QRhi (GPU · geometri)"; }

    bool gpu() const override { return true; }

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

    /// Appends one ring as a triangle fan and grows `box`. Returns the vertex count.
    std::uint32_t emit_fan(const float* xs, const float* ys, std::uint32_t count, double cx,
                           double cy, bool flip_y, float box[4]);

    /// Appends the six vertices of an axis-aligned cover quad.
    std::uint32_t emit_cover(const float box[4]);

    /// Appends one segment instance.
    void emit_segment(float x0, float y0, float x1, float y1);

    /// Reserves a uniform slot and fills it.
    std::uint32_t push_uniform(std::uint32_t rgba, float half_width);

    // ---- GPU resources -----------------------------------------------------

    bool ensure_resources(QRhi* rhi, QRhiRenderPassDescriptor* rp, int sample_count);
    bool ensure_capacity(QRhi* rhi, QRhiResourceUpdateBatch* rub);
    void release();

    QRhi* rhi_{nullptr};
    QRhiRenderPassDescriptor* rp_{nullptr};
    int samples_{1};

    std::unique_ptr<QRhiBuffer> corners_;  ///< the four static line-quad corners
    std::unique_ptr<QRhiBuffer> segments_; ///< per-instance segment endpoints
    std::unique_ptr<QRhiBuffer> vertices_; ///< fan and cover triangles
    std::unique_ptr<QRhiBuffer> uniforms_;
    std::unique_ptr<QRhiShaderResourceBindings> srb_;
    std::unique_ptr<QRhiGraphicsPipeline> line_;
    std::unique_ptr<QRhiGraphicsPipeline> fill_stencil_;
    std::unique_ptr<QRhiGraphicsPipeline> fill_cover_;

    quint32 segment_capacity_{0}; ///< bytes
    quint32 vertex_capacity_{0};  ///< bytes
    quint32 uniform_capacity_{0}; ///< bytes
    quint32 uniform_stride_{256};
    bool corners_uploaded_{false};

    // ---- per-frame CPU buffers, capacity kept between frames ----------------

    std::vector<float> segment_data_; ///< x0,y0,x1,y1 per instance
    std::vector<float> vertex_data_;  ///< x,y per vertex
    std::vector<char> uniform_data_;  ///< `uniform_stride_` bytes per slot
    std::vector<Cmd> cmds_;

    Uniforms proto_{}; ///< this frame's mvp, copied into every slot
};

// -----------------------------------------------------------------------------
// what it draws
// -----------------------------------------------------------------------------

bool RhiBackend::handles(core::SymbolLayerType type)
{
    switch (type) {
    // Geometry, which is what the first slice draws.
    case core::SymbolLayerType::SimpleFill: return true;
    case core::SymbolLayerType::SimpleLine: return true;

    // Everything below needs something this slice does not have yet: an SDF atlas
    // for text and glyphs (render.md R8), a pattern pipeline, or the document's
    // decoded image store on the GPU. Refused rather than claimed and skipped.
    default: return false;
    }
}

// -----------------------------------------------------------------------------
// frame building
// -----------------------------------------------------------------------------

std::uint32_t RhiBackend::push_uniform(std::uint32_t rgba, float half_width)
{
    const std::size_t slot = uniform_data_.size() / uniform_stride_;

    Uniforms u = proto_;
    unpack(rgba, u.colour);
    u.params[0] = half_width;

    uniform_data_.resize(uniform_data_.size() + uniform_stride_, 0);
    std::memcpy(uniform_data_.data() + slot * uniform_stride_, &u, sizeof(u));
    return static_cast<std::uint32_t>(slot);
}

void RhiBackend::emit_segment(float x0, float y0, float x1, float y1)
{
    segment_data_.push_back(x0);
    segment_data_.push_back(y0);
    segment_data_.push_back(x1);
    segment_data_.push_back(y1);
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

void RhiBackend::emit_document(const render::DrawList& list, double cx, double cy)
{
    // IN DRAW ORDER, which is not index order: MPYY prescribes a draw order for
    // plan sheets and a symbol's stack runs bottom layer first. The scene builder
    // sorted it; this loop obeys it, exactly as the QPainter backend does.
    for (std::uint32_t index : list.order) {
        if (index >= list.passes.size()) continue;

        const render::PassStyle& ps = list.passes[index];
        if (!handles(ps.type)) continue;

        if (ps.wants_fill) {
            const render::PolygonBatch& batch = list.polygons[index];
            if (!batch.runs.empty() && batch.rgba != 0) {
                float box[4]              = {1e30f, 1e30f, -1e30f, -1e30f};
                const std::uint32_t first = static_cast<std::uint32_t>(vertex_data_.size() / 2);

                std::size_t offset  = 0;
                std::uint32_t total = 0;
                for (std::uint32_t run : batch.runs) {
                    total += emit_fan(batch.xs.data() + offset, batch.ys.data() + offset, run, cx,
                                      cy, /*flip_y=*/true, box);
                    offset += run;
                }

                if (total > 0) {
                    Cmd cmd;
                    cmd.kind    = Cmd::Kind::Fill;
                    cmd.uniform = push_uniform(batch.rgba, 0.0f);
                    cmd.first   = first;
                    cmd.count   = total;
                    cmd.cover   = emit_cover(box);
                    cmds_.push_back(cmd);
                }
            }
        }

        if (ps.wants_stroke) {
            const render::PolylineBatch& batch = list.polylines[index];
            if (!batch.runs.empty() && batch.rgba != 0) {
                const std::uint32_t first = static_cast<std::uint32_t>(segment_data_.size() / 4);

                std::size_t offset = 0;
                for (std::uint32_t run : batch.runs) {
                    for (std::uint32_t v = 0; v + 1 < run; ++v) {
                        const std::size_t a = offset + v;
                        emit_segment(static_cast<float>(cx + static_cast<double>(batch.xs[a])),
                                     static_cast<float>(cy - static_cast<double>(batch.ys[a])),
                                     static_cast<float>(cx + static_cast<double>(batch.xs[a + 1])),
                                     static_cast<float>(cy - static_cast<double>(batch.ys[a + 1])));
                    }
                    offset += run;
                }

                const std::uint32_t count =
                    static_cast<std::uint32_t>(segment_data_.size() / 4) - first;
                if (count > 0) {
                    Cmd cmd;
                    cmd.kind    = Cmd::Kind::Line;
                    cmd.uniform = push_uniform(batch.rgba, std::max(0.5f, batch.width_px * 0.5f));
                    cmd.first   = first;
                    cmd.count   = count;
                    cmds_.push_back(cmd);
                }
            }
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

        const std::uint32_t first = static_cast<std::uint32_t>(segment_data_.size() / 4);

        std::size_t offset = 0;
        for (std::size_t r = 0; r < batch.runs.size(); ++r) {
            const std::uint32_t run = batch.runs[r];
            for (std::uint32_t v = 0; v + 1 < run; ++v) {
                const std::size_t a = offset + v;
                emit_segment(batch.xs[a], batch.ys[a], batch.xs[a + 1], batch.ys[a + 1]);
            }
            // A closed run's seam is a segment like any other; leaving it out is
            // what draws a selection rectangle with one side missing.
            if (run >= 3 && r < batch.closed.size() && batch.closed[r] != 0)
                emit_segment(batch.xs[offset + run - 1], batch.ys[offset + run - 1],
                             batch.xs[offset], batch.ys[offset]);
            offset += run;
        }

        const std::uint32_t count = static_cast<std::uint32_t>(segment_data_.size() / 4) - first;
        if (count > 0) {
            Cmd cmd;
            cmd.kind    = Cmd::Kind::Line;
            cmd.uniform = push_uniform(batch.rgba, std::max(0.5f, batch.width_px * 0.5f));
            cmd.first   = first;
            cmd.count   = count;
            cmds_.push_back(cmd);
        }
    }
}

void RhiBackend::paint_ground(const render::Overlay& overlay)
{
    emit_overlay(overlay, 0, overlay.beneath);
}

void RhiBackend::paint_aids(const render::DrawList& list, const render::Overlay& overlay)
{
    // `list` carries the captions. They wait on the SDF atlas of render.md R8;
    // taking the argument now keeps the call site the same when they arrive.
    (void)list;
    emit_overlay(overlay, overlay.beneath, overlay.batches.size());
}

// -----------------------------------------------------------------------------
// GPU resources
// -----------------------------------------------------------------------------

void RhiBackend::release()
{
    line_.reset();
    fill_stencil_.reset();
    fill_cover_.reset();
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

    const QShader line_vs = load_shader(":/piricad/shaders/line.vert.qsb");
    const QShader line_fs = load_shader(":/piricad/shaders/line.frag.qsb");
    const QShader fill_vs = load_shader(":/piricad/shaders/fill.vert.qsb");
    const QShader fill_fs = load_shader(":/piricad/shaders/fill.frag.qsb");
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
            {4 * sizeof(float), QRhiVertexInputBinding::PerInstance}, // segments
        });
        layout.setAttributes({
            {0, 0, QRhiVertexInputAttribute::Float2, 0},
            {1, 1, QRhiVertexInputAttribute::Float2, 0},
            {1, 2, QRhiVertexInputAttribute::Float2, 2 * sizeof(float)},
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

    return true;
}

bool RhiBackend::ensure_capacity(QRhi* rhi, QRhiResourceUpdateBatch* rub)
{
    // Grown to a HIGH-WATER MARK, never per frame. render.md R6 asks for a
    // persistent mapped ring buffer with fences; this is the honest first step
    // toward it — a buffer that is recreated only when a frame needs more than
    // every frame before it did, which on a pan is never.
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

        srb_.reset(rhi->newShaderResourceBindings());
        srb_->setBindings({QRhiShaderResourceBinding::uniformBufferWithDynamicOffset(
            0, QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage,
            uniforms_.get(), sizeof(Uniforms))});
        if (!srb_->create()) return false;
    }

    if (!corners_uploaded_) {
        rub->uploadStaticBuffer(corners_.get(), 0, sizeof(kLineCorners), kLineCorners);
        corners_uploaded_ = true;
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
    uniform_data_.clear();
    cmds_.clear();

    // Document coordinates arrive centre-relative with y UP; the vertex buffer
    // wants widget pixels with y DOWN. One conversion, in one place — the same
    // one the QPainter backend performs.
    const double cx = ctx.width_px * 0.5;
    const double cy = ctx.height_px * 0.5;

    paint_ground(overlay);
    emit_document(list, cx, cy);
    paint_aids(list, overlay);

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

    const QRhiCommandBuffer::VertexInput line_inputs[2] = {{corners_.get(), 0},
                                                           {segments_.get(), 0}};
    const QRhiCommandBuffer::VertexInput flat_input[1]  = {{vertices_.get(), 0}};

    for (const Cmd& cmd : cmds_) {
        const quint32 offset = cmd.uniform * uniform_stride_;
        const QRhiCommandBuffer::DynamicOffset dyn(0, offset);

        if (cmd.kind == Cmd::Kind::Line) {
            cb->setGraphicsPipeline(line_.get());
            cb->setShaderResources(srb_.get(), 1, &dyn);
            cb->setVertexInput(0, 2, line_inputs);
            cb->draw(4, cmd.count, 0, cmd.first);
            continue;
        }

        cb->setGraphicsPipeline(fill_stencil_.get());
        cb->setShaderResources(srb_.get(), 1, &dyn);
        cb->setVertexInput(0, 1, flat_input);
        cb->setStencilRef(0);
        cb->draw(cmd.count, 1, cmd.first, 0);

        cb->setGraphicsPipeline(fill_cover_.get());
        cb->setShaderResources(srb_.get(), 1, &dyn);
        cb->setVertexInput(0, 1, flat_input);
        cb->setStencilRef(0);
        cb->draw(6, 1, cmd.cover, 0);
    }

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

} // namespace piricad::app
