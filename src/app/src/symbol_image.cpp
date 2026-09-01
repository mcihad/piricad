// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/symbol_image.hpp"

#include <QByteArray>
#include <QPainter>
#include <QSvgRenderer>

#include <algorithm>
#include <cmath>

namespace piricad::app {
namespace {

/// True when the bytes open an SVG document; see `core::sniff_image_format`,
/// which asks the same question of the same bytes when they are interned.
bool looks_like_svg(std::span<const std::byte> bytes)
{
    const std::size_t look = bytes.size() < 512 ? bytes.size() : 512;
    const QByteArray head(reinterpret_cast<const char*>(bytes.data()),
                          static_cast<qsizetype>(look));
    return head.contains("<svg");
}

/// Draws an SVG at `size` pixels tall, on transparency.
///
/// `QSvgRenderer` is Qt's own SVG engine and it is the one QGIS rasterises its SVG
/// markers through as well, so a symbol authored for one draws the same in the
/// other. Nothing is hand-rolled here and nothing needs to be.
QImage rasterise(std::span<const std::byte> bytes, int size)
{
    const QByteArray data(reinterpret_cast<const char*>(bytes.data()),
                          static_cast<qsizetype>(bytes.size()));

    QSvgRenderer renderer;
    if (!renderer.load(data)) return {};

    const QSizeF box = renderer.defaultSize();
    if (box.isEmpty()) return {};

    const double ratio = box.width() / box.height();
    const int h        = std::max(1, size);
    const int w        = std::max(1, static_cast<int>(std::lround(h * ratio)));

    QImage out(w, h, QImage::Format_ARGB32_Premultiplied);
    out.fill(Qt::transparent);

    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, w, h));
    painter.end();
    return out;
}

/// Gives a picture with no alpha channel one, from how WHITE each pixel is.
///
/// MPYY's annex pictures are JPEG, which cannot carry alpha, so every glyph and
/// line type arrives sitting on an opaque white rectangle. The first answer to
/// that was to draw them in Multiply, which leaves white alone — but JPEG's white
/// is not 255, it is 250 with ringing around every stroke, so what actually
/// reached the canvas was a faint grey box at every stamp, visible along both
/// boundaries of any real drawing.
///
/// `alpha = 255 - min(r,g,b)` and nothing else. It is CONTINUOUS, so it makes no
/// decision about which greys are ink — which was the objection to keying white
/// out, and it is a fair objection to a THRESHOLD. Paper goes fully transparent, a
/// black stroke fully opaque, JPEG's ringing fades in proportion to how close to
/// paper it is. Keying on the smallest channel rather than on luminance keeps a
/// saturated colour opaque: MPYY's red boundary dots stay red at full strength
/// instead of being read as half-dark.
QImage keyed(QImage image)
{
    if (image.isNull() || image.hasAlphaChannel()) return image;

    QImage out = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < out.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(out.scanLine(y));
        for (int x = 0; x < out.width(); ++x) {
            const QRgb p    = row[x];
            const int paper = std::min({qRed(p), qGreen(p), qBlue(p)});
            row[x]          = qRgba(qRed(p), qGreen(p), qBlue(p), 255 - paper);
        }
    }
    return out;
}

} // namespace

QImage decode_symbol_image(std::span<const std::byte> bytes, int wanted_px)
{
    if (bytes.empty()) return {};

    if (looks_like_svg(bytes)) return rasterise(bytes, std::clamp(wanted_px, 8, 512));

    QImage image;
    image.loadFromData(reinterpret_cast<const uchar*>(bytes.data()),
                       static_cast<int>(bytes.size()));
    return keyed(std::move(image));
}

} // namespace piricad::app
