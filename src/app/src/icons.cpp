// SPDX-License-Identifier: GPL-3.0-or-later
#include "piricad/app/icons.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

namespace piricad::app {
namespace {

/// Every glyph is authored on a 24×24 grid with a 2 px safe margin, then scaled.
/// Stroke weight is uniform so the palette reads as one set at 20 px.
constexpr qreal kGrid   = 24.0;
constexpr qreal kStroke = 1.9;

QPen stroke(const QColor& c, qreal width = kStroke)
{
    QPen pen(c, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    return pen;
}

/// Vertex handle: a filled square knocked out of the background, the way CAD
/// grips read at small sizes.
void grip(QPainter& p, QPointF at, const QColor& c)
{
    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawRect(QRectF(at.x() - 1.9, at.y() - 1.9, 3.8, 3.8));
    p.restore();
}

void arrowHead(QPainter& p, QPointF tip, QPointF from, const QColor& c, qreal len = 4.2)
{
    QLineF axis(tip, from);
    axis.setLength(len);

    QLineF a = axis;
    a.setAngle(axis.angle() + 32.0);
    QLineF b = axis;
    b.setAngle(axis.angle() - 32.0);

    QPainterPath head;
    head.moveTo(tip);
    head.lineTo(a.p2());
    head.lineTo(b.p2());
    head.closeSubpath();

    p.save();
    p.setPen(Qt::NoPen);
    p.setBrush(c);
    p.drawPath(head);
    p.restore();
}

void draw(QPainter& p, Glyph g, const QColor& c)
{
    p.setPen(stroke(c));
    p.setBrush(Qt::NoBrush);

    switch (g) {
    case Glyph::Select: {
        // A solid cursor reads far better than an outline at 20 px.
        QPainterPath arrow;
        arrow.moveTo(6.5, 3.2);
        arrow.lineTo(6.5, 18.4);
        arrow.lineTo(10.3, 14.7);
        arrow.lineTo(12.7, 20.6);
        arrow.lineTo(15.3, 19.5);
        arrow.lineTo(13.0, 13.8);
        arrow.lineTo(18.1, 13.3);
        arrow.closeSubpath();

        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPath(arrow);
        break;
    }
    case Glyph::Line:
        p.drawLine(QPointF(6.0, 18.0), QPointF(18.0, 6.0));
        grip(p, QPointF(6.0, 18.0), c);
        grip(p, QPointF(18.0, 6.0), c);
        break;

    case Glyph::Polyline:
        p.drawPolyline(QPolygonF(
            {QPointF(4.5, 17.5), QPointF(9.5, 8.5), QPointF(14.5, 15.0), QPointF(19.5, 6.5)}));
        grip(p, QPointF(4.5, 17.5), c);
        grip(p, QPointF(19.5, 6.5), c);
        break;

    case Glyph::Erase: {
        // A tilted eraser block with its wiped baseline.
        QPainterPath body;
        body.moveTo(3.6, 15.4);
        body.lineTo(11.4, 7.6);
        body.lineTo(17.0, 13.2);
        body.lineTo(9.2, 21.0);
        body.closeSubpath();
        p.drawPath(body);
        p.drawLine(QPointF(7.5, 11.5), QPointF(13.1, 17.1));
        p.setPen(stroke(c, 1.5));
        p.drawLine(QPointF(3.0, 21.0), QPointF(21.0, 21.0));
        break;
    }
    case Glyph::Layer: {
        // Three sheets; the top one filled so the stack reads instantly.
        const auto sheet = [&](qreal dy) {
            return QPolygonF({QPointF(12, 3.4 + dy), QPointF(20.2, 7.6 + dy),
                              QPointF(12, 11.8 + dy), QPointF(3.8, 7.6 + dy)});
        };
        p.drawPolygon(sheet(8.6));
        p.drawPolygon(sheet(4.3));

        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(sheet(0.0));
        break;
    }
    case Glyph::Measure: {
        p.drawRoundedRect(QRectF(2.8, 8.6, 18.4, 6.8), 1.2, 1.2);
        p.setPen(stroke(c, 1.5));
        const qreal ticks[] = {6.6, 9.6, 12.6, 15.6, 18.6};
        for (int i = 0; i < 5; ++i)
            p.drawLine(QPointF(ticks[i], 8.6), QPointF(ticks[i], (i % 2) ? 11.4 : 12.6));
        break;
    }
    case Glyph::ZoomExtents: {
        // A frame with corner brackets: "fit everything".
        p.setPen(stroke(c, 1.5));
        const qreal x0 = 2.8, y0 = 4.4, x1 = 21.2, y1 = 19.6, n = 4.4;
        p.drawPolyline(QPolygonF({QPointF(x0, y0 + n), QPointF(x0, y0), QPointF(x0 + n, y0)}));
        p.drawPolyline(QPolygonF({QPointF(x1 - n, y0), QPointF(x1, y0), QPointF(x1, y0 + n)}));
        p.drawPolyline(QPolygonF({QPointF(x1, y1 - n), QPointF(x1, y1), QPointF(x1 - n, y1)}));
        p.drawPolyline(QPolygonF({QPointF(x0 + n, y1), QPointF(x0, y1), QPointF(x0, y1 - n)}));

        p.setPen(stroke(c));
        p.drawLine(QPointF(8.6, 15.4), QPointF(15.4, 8.6));
        arrowHead(p, QPointF(7.6, 16.4), QPointF(15.4, 8.6), c);
        arrowHead(p, QPointF(16.4, 7.6), QPointF(8.6, 15.4), c);
        break;
    }
    case Glyph::ZoomIn:
    case Glyph::ZoomOut:
        p.drawEllipse(QPointF(10.2, 10.2), 6.6, 6.6);
        p.setPen(stroke(c, 2.4));
        p.drawLine(QPointF(15.1, 15.1), QPointF(20.4, 20.4));
        p.setPen(stroke(c, 1.8));
        p.drawLine(QPointF(7.0, 10.2), QPointF(13.4, 10.2));
        if (g == Glyph::ZoomIn) p.drawLine(QPointF(10.2, 7.0), QPointF(10.2, 13.4));
        break;

    case Glyph::Pan: {
        // A four-way move cross reads at 20 px where a hand silhouette does not.
        p.setPen(stroke(c, 1.7));
        p.drawLine(QPointF(12.0, 4.6), QPointF(12.0, 19.4));
        p.drawLine(QPointF(4.6, 12.0), QPointF(19.4, 12.0));
        arrowHead(p, QPointF(12.0, 3.2), QPointF(12.0, 9.0), c, 4.0);
        arrowHead(p, QPointF(12.0, 20.8), QPointF(12.0, 15.0), c, 4.0);
        arrowHead(p, QPointF(3.2, 12.0), QPointF(9.0, 12.0), c, 4.0);
        arrowHead(p, QPointF(20.8, 12.0), QPointF(15.0, 12.0), c, 4.0);
        break;
    }
    case Glyph::Undo:
    case Glyph::Redo: {
        p.save();
        if (g == Glyph::Redo) {
            p.translate(kGrid, 0.0);
            p.scale(-1.0, 1.0);
        }
        // Half-loop returning to the left, with a solid head on the return.
        QPainterPath loop;
        loop.moveTo(5.4, 9.2);
        loop.cubicTo(9.0, 4.6, 16.4, 5.0, 18.4, 10.0);
        loop.cubicTo(20.0, 14.2, 17.2, 18.6, 12.4, 19.2);
        p.drawPath(loop);
        arrowHead(p, QPointF(4.4, 9.6), QPointF(9.6, 7.4), c, 5.0);
        p.restore();
        break;
    }
    case Glyph::Script:
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(
            QPolygonF({QPointF(9.4, 4.2), QPointF(6.2, 7.4), QPointF(6.2, 10.6), QPointF(3.6, 12.0),
                       QPointF(6.2, 13.4), QPointF(6.2, 16.6), QPointF(9.4, 19.8)}));
        p.drawPolyline(QPolygonF({QPointF(14.6, 4.2), QPointF(17.8, 7.4), QPointF(17.8, 10.6),
                                  QPointF(20.4, 12.0), QPointF(17.8, 13.4), QPointF(17.8, 16.6),
                                  QPointF(14.6, 19.8)}));
        grip(p, QPointF(12.0, 12.0), c);
        break;

    case Glyph::Arc:
        p.drawArc(QRectF(3.6, 6.0, 16.8, 16.8), 20 * 16, 140 * 16);
        grip(p, QPointF(5.2, 17.0), c);
        grip(p, QPointF(18.8, 17.0), c);
        break;

    case Glyph::Circle:
        p.drawEllipse(QPointF(12.0, 12.0), 8.4, 8.4);
        grip(p, QPointF(12.0, 12.0), c);
        break;

    case Glyph::Rectangle:
        p.drawRect(QRectF(4.2, 6.4, 15.6, 11.2));
        grip(p, QPointF(4.2, 6.4), c);
        grip(p, QPointF(19.8, 17.6), c);
        break;

    case Glyph::Text: {
        p.setPen(stroke(c, 2.1));
        p.drawLine(QPointF(4.6, 6.0), QPointF(19.4, 6.0));
        p.drawLine(QPointF(12.0, 6.0), QPointF(12.0, 18.6));
        p.setPen(stroke(c, 1.6));
        p.drawLine(QPointF(8.6, 18.6), QPointF(15.4, 18.6));
        break;
    }
    case Glyph::Point:
        p.setPen(stroke(c, 1.6));
        p.drawLine(QPointF(12.0, 4.4), QPointF(12.0, 19.6));
        p.drawLine(QPointF(4.4, 12.0), QPointF(19.6, 12.0));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(12.0, 12.0), 3.0, 3.0);
        break;

    case Glyph::Move:
        // Four-way arrows around a held object.
        p.setPen(stroke(c, 1.6));
        p.drawRect(QRectF(8.6, 8.6, 6.8, 6.8));
        p.drawLine(QPointF(12.0, 8.0), QPointF(12.0, 4.4));
        p.drawLine(QPointF(12.0, 16.0), QPointF(12.0, 19.6));
        p.drawLine(QPointF(8.0, 12.0), QPointF(4.4, 12.0));
        p.drawLine(QPointF(16.0, 12.0), QPointF(19.6, 12.0));
        arrowHead(p, QPointF(12.0, 3.0), QPointF(12.0, 8.0), c, 3.4);
        arrowHead(p, QPointF(12.0, 21.0), QPointF(12.0, 16.0), c, 3.4);
        arrowHead(p, QPointF(3.0, 12.0), QPointF(8.0, 12.0), c, 3.4);
        arrowHead(p, QPointF(21.0, 12.0), QPointF(16.0, 12.0), c, 3.4);
        break;

    case Glyph::Copy:
        // Two offset outlines: the original and its duplicate.
        p.setPen(stroke(c, 1.5));
        p.drawRect(QRectF(3.6, 3.6, 12.0, 12.0));
        p.setPen(stroke(c, 1.9));
        p.drawRect(QRectF(8.4, 8.4, 12.0, 12.0));
        break;

    case Glyph::Rotate: {
        QPainterPath sweep;
        sweep.arcMoveTo(QRectF(4.2, 4.2, 15.6, 15.6), 60);
        sweep.arcTo(QRectF(4.2, 4.2, 15.6, 15.6), 60, 260);
        p.drawPath(sweep);
        arrowHead(p, QPointF(16.4, 5.4), QPointF(12.6, 8.6), c, 4.6);
        grip(p, QPointF(12.0, 12.0), c);
        break;
    }
    case Glyph::Offset:
        // A shape and its parallel copy: the setback operation.
        p.setPen(stroke(c, 1.8));
        p.drawPolyline(QPolygonF({QPointF(3.8, 17.4), QPointF(3.8, 8.0), QPointF(12.0, 3.4),
                                  QPointF(20.2, 8.0), QPointF(20.2, 17.4)}));
        p.setPen(QPen(c, 1.5, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({QPointF(7.0, 20.6), QPointF(7.0, 10.2), QPointF(12.0, 7.4),
                                  QPointF(17.0, 10.2), QPointF(17.0, 20.6)}));
        break;

    case Glyph::LayerManager: {
        const auto sheet = [&](qreal dy) {
            return QPolygonF({QPointF(10.0, 3.6 + dy), QPointF(17.4, 7.4 + dy),
                              QPointF(10.0, 11.2 + dy), QPointF(2.6, 7.4 + dy)});
        };
        p.drawPolygon(sheet(7.6));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(sheet(0.0));
        // The settings mark that turns a stack into a manager.
        p.setPen(stroke(c, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(18.4, 17.6), 3.4, 3.4);
        p.drawLine(QPointF(18.4, 13.6), QPointF(18.4, 21.6));
        p.drawLine(QPointF(14.4, 17.6), QPointF(22.4, 17.6));
        break;
    }
    case Glyph::Table:
        p.setPen(stroke(c, 1.6));
        p.drawRect(QRectF(3.2, 5.0, 17.6, 14.0));
        p.drawLine(QPointF(3.2, 9.6), QPointF(20.8, 9.6));
        p.drawLine(QPointF(3.2, 14.3), QPointF(20.8, 14.3));
        p.drawLine(QPointF(9.1, 5.0), QPointF(9.1, 19.0));
        p.drawLine(QPointF(15.0, 5.0), QPointF(15.0, 19.0));
        break;

    case Glyph::Identify:
        // Cursor over a feature: "what is this?"
        p.setPen(stroke(c, 1.6));
        p.drawPolygon(QPolygonF({QPointF(3.4, 12.0), QPointF(9.6, 4.2), QPointF(17.0, 7.6),
                                 QPointF(14.6, 15.4), QPointF(6.0, 16.2)}));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        {
            QPainterPath arrow;
            arrow.moveTo(12.4, 11.2);
            arrow.lineTo(12.4, 21.6);
            arrow.lineTo(15.0, 19.0);
            arrow.lineTo(16.8, 22.4);
            arrow.lineTo(18.6, 21.4);
            arrow.lineTo(16.9, 18.1);
            arrow.lineTo(20.4, 17.7);
            arrow.closeSubpath();
            p.drawPath(arrow);
        }
        break;

    case Glyph::Snap:
        // The osnap marker: a square on a vertex where two lines meet.
        p.setPen(stroke(c, 1.6));
        p.drawLine(QPointF(3.4, 18.6), QPointF(12.0, 8.0));
        p.drawLine(QPointF(12.0, 8.0), QPointF(20.6, 18.6));
        p.setPen(stroke(c, 1.8));
        p.drawRect(QRectF(8.4, 4.4, 7.2, 7.2));
        break;

    case Glyph::New:
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF({QPointF(13.6, 3.4), QPointF(5.4, 3.4), QPointF(5.4, 20.6),
                                  QPointF(18.6, 20.6), QPointF(18.6, 8.4)}));
        p.drawPolyline(QPolygonF(
            {QPointF(13.6, 3.4), QPointF(18.6, 8.4), QPointF(13.6, 8.4), QPointF(13.6, 3.4)}));
        break;

    case Glyph::Open: {
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF({QPointF(2.8, 19.4), QPointF(2.8, 5.4), QPointF(9.4, 5.4),
                                  QPointF(11.6, 8.2), QPointF(18.0, 8.2), QPointF(18.0, 11.0)}));
        p.drawPolyline(QPolygonF({QPointF(2.8, 19.4), QPointF(6.6, 11.6), QPointF(21.6, 11.6),
                                  QPointF(17.8, 19.4), QPointF(2.8, 19.4)}));
        break;
    }
    case Glyph::Save:
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF({QPointF(4.0, 20.2), QPointF(4.0, 3.8), QPointF(16.6, 3.8),
                                  QPointF(20.0, 7.2), QPointF(20.0, 20.2), QPointF(4.0, 20.2)}));
        p.drawRect(QRectF(7.6, 3.8, 8.4, 5.6));
        p.drawRect(QRectF(7.0, 13.0, 10.0, 7.2));
        break;

    case Glyph::Export:
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF({QPointF(13.4, 3.6), QPointF(5.0, 3.6), QPointF(5.0, 20.4),
                                  QPointF(18.4, 20.4), QPointF(18.4, 13.6)}));
        p.drawLine(QPointF(11.6, 11.8), QPointF(21.0, 3.6));
        arrowHead(p, QPointF(21.6, 3.0), QPointF(15.0, 8.8), c, 5.2);
        break;

    case Glyph::Print:
        p.setPen(stroke(c, 1.6));
        p.drawPolyline(QPolygonF(
            {QPointF(6.4, 8.6), QPointF(6.4, 3.6), QPointF(17.6, 3.6), QPointF(17.6, 8.6)}));
        p.drawRoundedRect(QRectF(3.2, 8.6, 17.6, 7.6), 1.6, 1.6);
        p.drawRect(QRectF(6.4, 13.4, 11.2, 7.0));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(17.2, 11.4), 1.2, 1.2);
        break;

    case Glyph::Ai: {
        // A four-point sparkle: the conventional "assisted" mark.
        const auto spark = [&](qreal cx, qreal cy, qreal r, qreal waist) {
            QPainterPath s;
            s.moveTo(cx, cy - r);
            s.quadTo(cx + waist, cy - waist, cx + r, cy);
            s.quadTo(cx + waist, cy + waist, cx, cy + r);
            s.quadTo(cx - waist, cy + waist, cx - r, cy);
            s.quadTo(cx - waist, cy - waist, cx, cy - r);
            return s;
        };
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPath(spark(10.2, 10.4, 7.4, 1.9));
        p.drawPath(spark(18.0, 17.6, 4.0, 1.0));
        break;
    }
    }
}

QPixmap render(Glyph g, const QColor& colour, int size, qreal dpr)
{
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(size / kGrid, size / kGrid);
    draw(p, g, colour);
    return pm;
}

} // namespace

QIcon icon(Glyph glyph, const QColor& colour, const QColor& accent, int size)
{
    const qreal dpr = 2.0; // rendered above the highest common ratio, then downscaled

    QColor disabled = colour;
    disabled.setAlpha(70);

    QIcon out;
    out.addPixmap(render(glyph, colour, size, dpr), QIcon::Normal, QIcon::Off);
    out.addPixmap(render(glyph, accent, size, dpr), QIcon::Normal, QIcon::On);
    out.addPixmap(render(glyph, accent, size, dpr), QIcon::Active, QIcon::Off);
    out.addPixmap(render(glyph, disabled, size, dpr), QIcon::Disabled, QIcon::Off);
    return out;
}

} // namespace piricad::app
