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

        // ---- shell chrome, design.md 7 ----
        //
        // Authored on the same 24x24 grid as everything above so the title bar, the
        // dock headers and the tool bar read as one set. The mockup names each of
        // these by its Material Symbols name; the shape is redrawn, not traced, so
        // the application carries no icon font and every glyph re-tints with the
        // theme (design.md 5).

    case Glyph::Search:
        p.setPen(stroke(c, 1.8));
        p.drawEllipse(QPointF(10.4, 10.4), 5.8, 5.8);
        p.drawLine(QPointF(14.6, 14.6), QPointF(19.4, 19.4));
        break;

    case Glyph::Cut:
        // Scissors: two blades crossing over two finger rings.
        p.setPen(stroke(c, 1.7));
        p.drawLine(QPointF(6.6, 3.6), QPointF(16.4, 15.2));
        p.drawLine(QPointF(17.4, 3.6), QPointF(7.6, 15.2));
        p.drawEllipse(QPointF(6.2, 18.2), 2.6, 2.6);
        p.drawEllipse(QPointF(17.8, 18.2), 2.6, 2.6);
        break;

    case Glyph::Paste:
        // A clipboard with its clip.
        p.setPen(stroke(c, 1.7));
        p.drawRoundedRect(QRectF(5.0, 5.0, 14.0, 15.6), 2.0, 2.0);
        p.drawRoundedRect(QRectF(9.0, 3.0, 6.0, 4.2), 1.2, 1.2);
        break;

    case Glyph::Duplicate:
        // Two offset sheets: the copy mark.
        p.setPen(stroke(c, 1.7));
        p.drawRoundedRect(QRectF(8.4, 8.4, 11.6, 11.6), 2.0, 2.0);
        p.drawPolyline(QPolygonF({QPointF(15.6, 5.2), QPointF(15.6, 4.0), QPointF(4.0, 4.0),
                                  QPointF(4.0, 15.6), QPointF(5.2, 15.6)}));
        break;

    case Glyph::Close:
        p.setPen(stroke(c, 1.8));
        p.drawLine(QPointF(6.2, 6.2), QPointF(17.8, 17.8));
        p.drawLine(QPointF(17.8, 6.2), QPointF(6.2, 17.8));
        break;

    case Glyph::SplitView:
        p.setPen(stroke(c, 1.7));
        p.drawRect(QRectF(3.6, 5.2, 16.8, 13.6));
        p.drawLine(QPointF(12.0, 5.2), QPointF(12.0, 18.8));
        break;

    case Glyph::Fullscreen:
        // Four corner brackets opening outward.
        p.setPen(stroke(c, 1.8));
        for (int q = 0; q < 4; ++q) {
            const qreal sx = (q & 1) ? -1.0 : 1.0;
            const qreal sy = (q & 2) ? -1.0 : 1.0;
            const QPointF o(12.0 + sx * 8.4, 12.0 + sy * 8.4);
            p.drawLine(o, QPointF(o.x() - sx * 4.2, o.y()));
            p.drawLine(o, QPointF(o.x(), o.y() - sy * 4.2));
        }
        break;

    case Glyph::Grip:
        // Two columns of three dots: the drag handle.
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        for (int row = 0; row < 3; ++row)
            for (int col = 0; col < 2; ++col)
                p.drawEllipse(QPointF(9.4 + col * 5.2, 6.4 + row * 5.6), 1.35, 1.35);
        break;

    case Glyph::Collapse:
        p.setPen(stroke(c, 1.9));
        p.drawLine(QPointF(5.4, 12.0), QPointF(18.6, 12.0));
        break;

    case Glyph::Float:
        // A pane lifting off the one behind it.
        p.setPen(stroke(c, 1.6));
        p.drawRect(QRectF(4.0, 7.6, 12.4, 12.4));
        p.drawPolyline(QPolygonF({QPointF(8.4, 7.6), QPointF(8.4, 4.0), QPointF(20.0, 4.0),
                                  QPointF(20.0, 15.6), QPointF(16.4, 15.6)}));
        break;

    case Glyph::Eye:
        p.setPen(stroke(c, 1.7));
        {
            QPainterPath lid;
            lid.moveTo(2.8, 12.0);
            lid.quadTo(12.0, 4.2, 21.2, 12.0);
            lid.quadTo(12.0, 19.8, 2.8, 12.0);
            p.drawPath(lid);
        }
        p.drawEllipse(QPointF(12.0, 12.0), 2.9, 2.9);
        break;

    case Glyph::EyeOff:
        p.setPen(stroke(c, 1.7));
        {
            QPainterPath lid;
            lid.moveTo(2.8, 12.0);
            lid.quadTo(12.0, 4.2, 21.2, 12.0);
            lid.quadTo(12.0, 19.8, 2.8, 12.0);
            p.drawPath(lid);
        }
        p.drawEllipse(QPointF(12.0, 12.0), 2.9, 2.9);
        p.drawLine(QPointF(4.4, 19.6), QPointF(19.6, 4.4));
        break;

    case Glyph::Lock:
    case Glyph::Unlock:
        p.setPen(stroke(c, 1.7));
        p.drawRoundedRect(QRectF(5.6, 10.6, 12.8, 9.4), 1.8, 1.8);
        // The shackle stands upright when locked and tips open when it is not.
        {
            QPainterPath bow;
            if (g == Glyph::Lock) {
                bow.moveTo(8.4, 10.6);
                bow.lineTo(8.4, 7.6);
                bow.quadTo(8.4, 4.0, 12.0, 4.0);
                bow.quadTo(15.6, 4.0, 15.6, 7.6);
                bow.lineTo(15.6, 10.6);
            } else {
                bow.moveTo(8.4, 10.6);
                bow.lineTo(8.4, 7.6);
                bow.quadTo(8.4, 4.0, 11.6, 4.0);
                bow.quadTo(15.0, 4.0, 15.0, 7.4);
            }
            p.drawPath(bow);
        }
        break;

    case Glyph::Filter:
        p.setPen(stroke(c, 1.8));
        p.drawPolyline(QPolygonF({QPointF(3.6, 5.0), QPointF(20.4, 5.0), QPointF(13.8, 12.6),
                                  QPointF(13.8, 19.6), QPointF(10.2, 17.2), QPointF(10.2, 12.6),
                                  QPointF(3.6, 5.0)}));
        break;

    case Glyph::Plus:
        p.setPen(stroke(c, 1.9));
        p.drawLine(QPointF(12.0, 5.4), QPointF(12.0, 18.6));
        p.drawLine(QPointF(5.4, 12.0), QPointF(18.6, 12.0));
        break;

    case Glyph::Minus:
        p.setPen(stroke(c, 1.9));
        p.drawLine(QPointF(5.4, 12.0), QPointF(18.6, 12.0));
        break;

    case Glyph::Fit:
        // Corner brackets turned inward around a frame: fit to screen.
        p.setPen(stroke(c, 1.7));
        p.drawRect(QRectF(4.2, 6.4, 15.6, 11.2));
        p.setPen(stroke(c, 1.5));
        p.drawLine(QPointF(8.2, 9.4), QPointF(15.8, 9.4));
        p.drawLine(QPointF(8.2, 14.6), QPointF(15.8, 14.6));
        break;

    case Glyph::ChevronDown:
        p.setPen(stroke(c, 1.9));
        p.drawPolyline(QPolygonF({QPointF(6.6, 9.6), QPointF(12.0, 15.0), QPointF(17.4, 9.6)}));
        break;

    case Glyph::ChevronRight:
        p.setPen(stroke(c, 1.9));
        p.drawPolyline(QPolygonF({QPointF(9.6, 6.6), QPointF(15.0, 12.0), QPointF(9.6, 17.4)}));
        break;

    case Glyph::Cloud:
        p.setPen(stroke(c, 1.7));
        {
            QPainterPath cloud;
            cloud.moveTo(6.6, 18.0);
            cloud.quadTo(2.6, 18.0, 2.6, 14.2);
            cloud.quadTo(2.6, 10.8, 6.2, 10.4);
            cloud.quadTo(7.0, 5.6, 11.8, 5.6);
            cloud.quadTo(16.0, 5.6, 17.0, 9.6);
            cloud.quadTo(21.4, 10.0, 21.4, 14.0);
            cloud.quadTo(21.4, 18.0, 17.4, 18.0);
            cloud.closeSubpath();
            p.drawPath(cloud);
        }
        break;

    case Glyph::Locate:
        // The crosshair reticle the status bar uses for the cursor readout.
        p.setPen(stroke(c, 1.7));
        p.drawEllipse(QPointF(12.0, 12.0), 5.4, 5.4);
        p.drawLine(QPointF(12.0, 2.6), QPointF(12.0, 6.0));
        p.drawLine(QPointF(12.0, 18.0), QPointF(12.0, 21.4));
        p.drawLine(QPointF(2.6, 12.0), QPointF(6.0, 12.0));
        p.drawLine(QPointF(18.0, 12.0), QPointF(21.4, 12.0));
        break;

    case Glyph::Function:
        // The italic f of an expression field.
        p.setPen(stroke(c, 1.8));
        {
            QPainterPath f;
            f.moveTo(8.0, 20.0);
            f.lineTo(11.2, 7.4);
            f.quadTo(12.0, 4.2, 15.4, 4.2);
            p.drawPath(f);
        }
        p.drawLine(QPointF(7.2, 10.6), QPointF(14.6, 10.6));
        break;

    case Glyph::Polygon:
        p.setPen(stroke(c, 1.7));
        p.drawPolygon(QPolygonF({QPointF(12.0, 3.4), QPointF(20.6, 9.6), QPointF(17.3, 19.8),
                                 QPointF(6.7, 19.8), QPointF(3.4, 9.6)}));
        break;

    case Glyph::History:
        p.setPen(stroke(c, 1.7));
        {
            // An open dial with the hands set, and an arrow closing it anti-clockwise.
            QPainterPath dial;
            dial.arcMoveTo(QRectF(3.6, 3.6, 16.8, 16.8), 150.0);
            dial.arcTo(QRectF(3.6, 3.6, 16.8, 16.8), 150.0, -300.0);
            p.drawPath(dial);
        }
        p.drawPolyline(QPolygonF({QPointF(2.6, 4.6), QPointF(3.2, 8.6), QPointF(7.2, 7.4)}));
        p.drawPolyline(QPolygonF({QPointF(12.0, 7.6), QPointF(12.0, 12.4), QPointF(15.8, 14.4)}));
        break;

    case Glyph::Palette:
        p.setPen(stroke(c, 1.6));
        {
            QPainterPath pal;
            pal.moveTo(12.0, 3.4);
            pal.quadTo(20.6, 3.4, 20.6, 11.2);
            pal.quadTo(20.6, 15.0, 16.4, 15.0);
            pal.quadTo(13.6, 15.0, 13.6, 17.2);
            pal.quadTo(13.6, 20.6, 11.0, 20.6);
            pal.quadTo(3.4, 20.6, 3.4, 12.0);
            pal.quadTo(3.4, 3.4, 12.0, 3.4);
            p.drawPath(pal);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(8.0, 8.4), 1.5, 1.5);
        p.drawEllipse(QPointF(13.6, 7.0), 1.5, 1.5);
        p.drawEllipse(QPointF(7.0, 14.2), 1.5, 1.5);
        break;

    case Glyph::Help:
        p.setPen(stroke(c, 1.7));
        p.drawEllipse(QPointF(12.0, 12.0), 8.6, 8.6);
        {
            QPainterPath q;
            q.moveTo(9.2, 9.6);
            q.quadTo(9.2, 6.8, 12.0, 6.8);
            q.quadTo(14.8, 6.8, 14.8, 9.4);
            q.quadTo(14.8, 11.6, 12.0, 12.6);
            q.lineTo(12.0, 14.4);
            p.drawPath(q);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(12.0, 17.4), 1.25, 1.25);
        break;

    case Glyph::Document:
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF({QPointF(14.2, 3.4), QPointF(5.6, 3.4), QPointF(5.6, 20.6),
                                  QPointF(18.4, 20.6), QPointF(18.4, 7.6), QPointF(14.2, 3.4),
                                  QPointF(14.2, 7.6), QPointF(18.4, 7.6)}));
        p.setPen(stroke(c, 1.4));
        p.drawLine(QPointF(8.6, 12.4), QPointF(15.4, 12.4));
        p.drawLine(QPointF(8.6, 16.0), QPointF(15.4, 16.0));
        break;

    case Glyph::Globe:
        p.setPen(stroke(c, 1.6));
        p.drawEllipse(QPointF(12.0, 12.0), 8.6, 8.6);
        p.drawLine(QPointF(3.4, 12.0), QPointF(20.6, 12.0));
        p.drawEllipse(QPointF(12.0, 12.0), 4.0, 8.6);
        break;

    case Glyph::Terrain:
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF({QPointF(2.8, 19.2), QPointF(9.0, 8.4), QPointF(13.0, 14.6),
                                  QPointF(15.6, 10.6), QPointF(21.2, 19.2)}));
        p.drawLine(QPointF(2.8, 19.2), QPointF(21.2, 19.2));
        break;

    case Glyph::SelectArea:
        // A dashed marquee with a pointer inside it.
        p.setPen(QPen(c, 1.5, Qt::DashLine, Qt::FlatCap));
        p.drawRect(QRectF(3.4, 3.4, 17.2, 17.2));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(QPolygonF({QPointF(9.0, 7.6), QPointF(17.4, 13.4), QPointF(13.4, 14.2),
                                 QPointF(15.4, 18.6), QPointF(13.0, 19.6), QPointF(11.0, 15.2),
                                 QPointF(8.2, 17.6)}));
        break;

    case Glyph::Trim:
        // Scissors over the line they cut.
        p.setPen(QPen(c, 1.4, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(2.8, 12.0), QPointF(21.2, 12.0));
        p.setPen(stroke(c, 1.6));
        p.drawLine(QPointF(8.0, 4.2), QPointF(17.0, 13.4));
        p.drawLine(QPointF(17.0, 4.2), QPointF(8.0, 13.4));
        p.drawEllipse(QPointF(7.4, 16.4), 2.4, 2.4);
        p.drawEllipse(QPointF(17.6, 16.4), 2.4, 2.4);
        break;

    case Glyph::Union:
        // Two overlapping rings: tevhit.
        p.setPen(stroke(c, 1.7));
        p.drawEllipse(QPointF(9.0, 12.0), 6.0, 6.0);
        p.drawEllipse(QPointF(15.0, 12.0), 6.0, 6.0);
        break;

    case Glyph::ParcelSplit:
        // One boundary branching into two: ifraz.
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF({QPointF(5.0, 19.4), QPointF(5.0, 12.4), QPointF(12.0, 12.4)}));
        p.drawLine(QPointF(12.0, 12.4), QPointF(18.6, 5.8));
        arrowHead(p, QPointF(19.4, 5.0), QPointF(15.0, 9.4), c);
        arrowHead(p, QPointF(5.0, 20.4), QPointF(5.0, 16.0), c);
        break;

    case Glyph::MeasureArea:
        // A set square over a filled corner: the area measure.
        p.setPen(stroke(c, 1.7));
        p.drawPolygon(QPolygonF({QPointF(4.0, 20.0), QPointF(20.0, 20.0), QPointF(4.0, 4.0)}));
        p.setPen(stroke(c, 1.3));
        p.drawLine(QPointF(4.0, 14.0), QPointF(10.0, 20.0));
        p.drawLine(QPointF(4.0, 9.0), QPointF(15.0, 20.0));
        break;

    case Glyph::Coordinate:
        // A reticle around a filled centre: pick a coordinate.
        p.setPen(stroke(c, 1.6));
        p.drawEllipse(QPointF(12.0, 12.0), 7.2, 7.2);
        p.drawLine(QPointF(12.0, 2.6), QPointF(12.0, 5.6));
        p.drawLine(QPointF(12.0, 18.4), QPointF(12.0, 21.4));
        p.drawLine(QPointF(2.6, 12.0), QPointF(5.6, 12.0));
        p.drawLine(QPointF(18.4, 12.0), QPointF(21.4, 12.0));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(12.0, 12.0), 2.3, 2.3);
        break;

    case Glyph::StyleCopy:
        // A pipette: lift a style off one object and put it on another.
        p.setPen(stroke(c, 1.7));
        p.drawLine(QPointF(4.2, 19.8), QPointF(13.0, 11.0));
        p.drawPolyline(QPolygonF({QPointF(11.4, 9.4), QPointF(15.4, 5.4), QPointF(18.6, 8.6),
                                  QPointF(14.6, 12.6), QPointF(11.4, 9.4)}));
        p.setPen(stroke(c, 1.4));
        p.drawLine(QPointF(4.2, 19.8), QPointF(6.4, 19.8));
        break;

    case Glyph::Topology:
        // Three nodes wired into a closed loop: the topology check.
        p.setPen(stroke(c, 1.5));
        p.drawPolygon(QPolygonF({QPointF(12.0, 4.6), QPointF(19.4, 17.6), QPointF(4.6, 17.6)}));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        for (QPointF at : {QPointF(12.0, 4.6), QPointF(19.4, 17.6), QPointF(4.6, 17.6)})
            p.drawEllipse(at, 2.1, 2.1);
        break;

    case Glyph::Settings:
        // Three sliders: the settings mark the reference uses, not a cog.
        p.setPen(stroke(c, 1.7));
        for (int k = 0; k < 3; ++k) {
            const qreal y = 6.4 + k * 5.6;
            p.drawLine(QPointF(3.6, y), QPointF(20.4, y));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(8.2, 6.4), 2.2, 2.2);
        p.drawEllipse(QPointF(15.4, 12.0), 2.2, 2.2);
        p.drawEllipse(QPointF(10.6, 17.6), 2.2, 2.2);
        break;

    case Glyph::Grid:
        p.setPen(stroke(c, 1.5));
        for (int k = 0; k < 4; ++k) {
            const qreal v = 3.6 + k * 5.6;
            p.drawLine(QPointF(v, 3.6), QPointF(v, 20.4));
            p.drawLine(QPointF(3.6, v), QPointF(20.4, v));
        }
        break;
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

QPixmap glyph_pixmap(Glyph glyph, const QColor& colour, int size, qreal dpr)
{
    return render(glyph, colour, size, dpr);
}

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
