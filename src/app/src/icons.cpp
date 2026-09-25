// SPDX-License-Identifier: GPL-3.0-or-later
#include "kentos_cad/app/icons.hpp"

#include <cmath>

#include <QFont>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QString>

namespace kentos::app {
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

/// A drawn face: the fill inside, the shape's stroke around it.
void face(QPainter& p, const QPainterPath& outline, const GlyphInks& k, qreal width = kStroke)
{
    p.save();
    p.setBrush(k.fill);
    p.setPen(stroke(k.shape, width));
    p.drawPath(outline);
    p.restore();
}

QPainterPath polygonPath(const QPolygonF& corners)
{
    QPainterPath path;
    path.addPolygon(corners);
    path.closeSubpath();
    return path;
}

/// `colour` at `alpha` of its strength: a wash under a line, a sheet in the back.
QColor washed(QColor colour, float alpha)
{
    colour.setAlphaF(colour.alphaF() * alpha);
    return colour;
}

/// Diagonal hatch lines 3 grid units apart, clipped to `region`.
void hatchInside(QPainter& p, const QPainterPath& region, const QColor& ink)
{
    p.save();
    p.setClipPath(region, Qt::IntersectClip);
    p.setPen(QPen(ink, 1.2, Qt::SolidLine, Qt::FlatCap));
    for (int i = -8; i <= 8; ++i) {
        const qreal d = i * 3.0;
        p.drawLine(QPointF(d, 24.0), QPointF(d + 24.0, 0.0));
    }
    p.restore();
}

/// The paperclip: one hairpin inside another.
QPainterPath paperclip()
{
    QPainterPath clip;
    clip.moveTo(15.4, 7.0);
    clip.lineTo(7.8, 14.6);
    clip.quadTo(5.4, 17.0, 7.8, 19.0);
    clip.quadTo(10.0, 20.8, 12.2, 18.6);
    clip.lineTo(19.0, 11.8);
    clip.quadTo(21.8, 9.0, 19.0, 6.0);
    clip.quadTo(16.0, 3.0, 13.2, 5.8);
    clip.lineTo(6.6, 12.4);
    return clip;
}

/// One sheet of the layer stack, `dy` below the top one.
QPolygonF layerSheet(qreal dy)
{
    return QPolygonF({QPointF(11.0, 3.4 + dy), QPointF(19.2, 7.6 + dy), QPointF(11.0, 11.8 + dy),
                      QPointF(2.8, 7.6 + dy)});
}

/// A small figure written on a picture — a corner number, a length — `units`
/// tall on the 24-unit grid the picture is drawn on.
void writeSmall(QPainter& p, const QPointF& centre, const QString& text, const QColor& ink,
                int units = 6)
{
    p.save();
    QFont f(QStringLiteral("IBM Plex Sans"));
    f.setPixelSize(units);
    f.setWeight(QFont::DemiBold);
    p.setFont(f);
    p.setPen(ink);
    const QRectF box(centre.x() - 6.0, centre.y() - 4.0, 12.0, 8.0);
    p.drawText(box, Qt::AlignCenter, text);
    p.restore();
}

void draw(QPainter& p, Glyph g, const GlyphInks& k)
{
    // `c` is the ink: every glyph not given roles is drawn in it alone, which is
    // also exactly what a one-ink icon is (`GlyphInks::mono`).
    const QColor& c = k.ink;
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

        p.setPen(QPen(k.paper, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(c);
        p.drawPath(arrow);
        break;
    }
    case Glyph::Line:
        p.setPen(stroke(k.shape));
        p.drawLine(QPointF(6.0, 18.0), QPointF(18.0, 6.0));
        grip(p, QPointF(6.0, 18.0), c);
        grip(p, QPointF(18.0, 6.0), c);
        break;

    case Glyph::Polyline:
        p.setPen(stroke(k.shape));
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
        {
            // The rubber end, the half that touches the sheet, is what erases.
            QPainterPath tip;
            tip.moveTo(3.6, 15.4);
            tip.lineTo(7.5, 11.5);
            tip.lineTo(13.1, 17.1);
            tip.lineTo(9.2, 21.0);
            tip.closeSubpath();
            p.save();
            p.setPen(Qt::NoPen);
            p.setBrush(k.cut);
            p.drawPath(tip);
            p.setBrush(k.paper);
            p.drawPath(body.subtracted(tip));
            p.restore();
        }
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

        p.setPen(stroke(c, 1.2));
        p.setBrush(k.data);
        p.drawPolygon(sheet(0.0));
        break;
    }
    case Glyph::Measure: {
        p.setPen(stroke(c));
        p.setBrush(k.note);
        p.drawRoundedRect(QRectF(2.8, 8.6, 18.4, 6.8), 1.2, 1.2);
        p.setBrush(Qt::NoBrush);
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

        p.setPen(stroke(k.shape));
        p.drawLine(QPointF(8.6, 15.4), QPointF(15.4, 8.6));
        arrowHead(p, QPointF(7.6, 16.4), QPointF(15.4, 8.6), k.shape);
        arrowHead(p, QPointF(16.4, 7.6), QPointF(8.6, 15.4), k.shape);
        break;
    }
    case Glyph::ZoomIn:
    case Glyph::ZoomOut:
        p.setBrush(k.fill);
        p.drawEllipse(QPointF(10.2, 10.2), 6.6, 6.6);
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(c, 2.4));
        p.drawLine(QPointF(15.1, 15.1), QPointF(20.4, 20.4));
        p.setPen(stroke(k.shape, 1.9));
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
        p.setPen(stroke(k.shape, 2.1));
        p.drawPath(loop);
        arrowHead(p, QPointF(4.4, 9.6), QPointF(9.6, 7.4), k.shape, 5.0);
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
        p.setPen(stroke(k.shape));
        p.drawArc(QRectF(3.6, 6.0, 16.8, 16.8), 20 * 16, 140 * 16);
        grip(p, QPointF(5.2, 17.0), c);
        grip(p, QPointF(18.8, 17.0), c);
        break;

    case Glyph::Circle: {
        QPainterPath disc;
        disc.addEllipse(QPointF(12.0, 12.0), 8.4, 8.4);
        face(p, disc, k);
        grip(p, QPointF(12.0, 12.0), c);
    } break;

    case Glyph::Rectangle: {
        QPainterPath box;
        box.addRect(QRectF(4.2, 6.4, 15.6, 11.2));
        face(p, box, k);
    }
        grip(p, QPointF(4.2, 6.4), c);
        grip(p, QPointF(19.8, 17.6), c);
        break;

    case Glyph::Text: {
        p.setPen(stroke(c, 2.1));
        p.drawLine(QPointF(4.6, 6.0), QPointF(19.4, 6.0));
        p.drawLine(QPointF(12.0, 6.0), QPointF(12.0, 18.6));
        p.setPen(stroke(k.note, 2.2));
        p.drawLine(QPointF(7.6, 20.4), QPointF(16.4, 20.4));
        break;
    }
    case Glyph::Point:
        p.setPen(stroke(c, 1.6));
        p.drawLine(QPointF(12.0, 4.4), QPointF(12.0, 19.6));
        p.drawLine(QPointF(4.4, 12.0), QPointF(19.6, 12.0));
        p.setPen(Qt::NoPen);
        p.setBrush(k.shape);
        p.drawEllipse(QPointF(12.0, 12.0), 3.4, 3.4);
        break;

    case Glyph::Move: {
        // Four-way arrows around a held object.
        QPainterPath held;
        held.addRect(QRectF(8.6, 8.6, 6.8, 6.8));
        face(p, held, k, 1.6);
        p.setPen(stroke(c, 1.6));
        p.drawLine(QPointF(12.0, 8.0), QPointF(12.0, 4.4));
        p.drawLine(QPointF(12.0, 16.0), QPointF(12.0, 19.6));
        p.drawLine(QPointF(8.0, 12.0), QPointF(4.4, 12.0));
        p.drawLine(QPointF(16.0, 12.0), QPointF(19.6, 12.0));
        arrowHead(p, QPointF(12.0, 3.0), QPointF(12.0, 8.0), c, 3.4);
        arrowHead(p, QPointF(12.0, 21.0), QPointF(12.0, 16.0), c, 3.4);
        arrowHead(p, QPointF(3.0, 12.0), QPointF(8.0, 12.0), c, 3.4);
        arrowHead(p, QPointF(21.0, 12.0), QPointF(16.0, 12.0), c, 3.4);
        break;
    }

    case Glyph::Stretch:
        // A FRAME WITH ONE CORNER PULLED OUT, which is what the command does: the
        // vertices inside the crossing window move and the rest stay. Deliberately
        // NOT four-way arrows — those belong to `TAŞI`, and sharing them made the
        // two tools one tool in the column.
        p.setPen(stroke(c, 1.6));
        p.drawPolyline(QPolygonF({QPointF(4.4, 19.6), QPointF(4.4, 7.4), QPointF(13.4, 7.4)}));
        p.setPen(stroke(k.shape, 1.9));
        p.drawPolyline(QPolygonF({QPointF(4.4, 19.6), QPointF(16.6, 19.6), QPointF(16.6, 12.0)}));
        p.drawLine(QPointF(13.4, 7.4), QPointF(16.6, 12.0));
        grip(p, QPointF(16.6, 12.0), c);
        arrowHead(p, QPointF(21.2, 6.6), QPointF(16.2, 11.0), c, 3.4);
        p.setPen(stroke(c, 1.6));
        p.drawLine(QPointF(16.6, 11.4), QPointF(20.2, 7.6));
        break;

    case Glyph::Ellipse:
        // AN ELLIPSE, and its two axes: what tells it from a circle at a glance is
        // that it has a long one and a short one.
        {
            QPainterPath oval;
            oval.addEllipse(QPointF(12.0, 12.0), 9.0, 5.6);
            face(p, oval, k);
        }
        p.setPen(stroke(c, 1.2));
        p.drawLine(QPointF(3.0, 12.0), QPointF(21.0, 12.0));
        p.drawLine(QPointF(12.0, 6.4), QPointF(12.0, 17.6));
        grip(p, QPointF(12.0, 12.0), c);
        break;

    case Glyph::Annulus:
        // TWO CONCENTRIC CIRCLES: the ring is the space between them, and the
        // inner one is what makes it a ring rather than a disc.
        {
            // The ring is the face: the band between the two, filled.
            QPainterPath ring;
            ring.addEllipse(QPointF(12.0, 12.0), 9.0, 9.0);
            ring.addEllipse(QPointF(12.0, 12.0), 4.4, 4.4);
            ring.setFillRule(Qt::OddEvenFill);
            face(p, ring, k, 1.6);
        }
        grip(p, QPointF(12.0, 12.0), c);
        break;

    case Glyph::Sector: {
        // A WEDGE CUT FROM A CIRCLE, drawn as the two radii and the arc between
        // them — the shape the command asks for in that order.
        QPainterPath wedge;
        wedge.moveTo(12.0, 12.0);
        wedge.lineTo(20.6, 12.0);
        wedge.arcTo(QRectF(3.4, 3.4, 17.2, 17.2), 0.0, -108.0);
        wedge.closeSubpath();
        face(p, wedge, k);
        grip(p, QPointF(12.0, 12.0), c);
        break;
    }

    case Glyph::PointIntersect:
        // TWO CROSSING LINES AND THE DOT WHERE THEY MEET, which is the whole of
        // what the command answers whichever of its three methods was used.
        p.setPen(stroke(c, 1.4));
        p.drawLine(QPointF(3.4, 6.4), QPointF(20.6, 15.6));
        p.drawLine(QPointF(3.4, 15.6), QPointF(20.6, 6.4));
        p.setPen(Qt::NoPen);
        p.setBrush(k.shape);
        p.drawEllipse(QPointF(12.0, 11.0), 2.8, 2.8);
        break;

    case Glyph::PointAlong:
        // A LINE WITH DOTS SPACED ALONG IT: the station pegs this command places.
        p.setPen(stroke(c, 1.4));
        p.drawLine(QPointF(3.2, 12.0), QPointF(20.8, 12.0));
        for (const qreal x : {7.6, 12.0, 16.4})
            grip(p, QPointF(x, 12.0), k.shape);
        break;

    case Glyph::PerpOffset:
        // A BASELINE, A RIGHT-ANGLE TICK OFF IT, AND THE DOT AT THE END: the two
        // tape readings a Turkish field book takes, drawn as they are measured.
        p.setPen(stroke(c, 1.7));
        p.drawLine(QPointF(3.2, 17.2), QPointF(20.8, 17.2));
        p.setPen(stroke(c, 1.4));
        p.drawLine(QPointF(14.0, 17.2), QPointF(14.0, 6.4));
        // The square that says the corner is a right angle.
        p.drawLine(QPointF(14.0, 14.4), QPointF(11.2, 14.4));
        p.drawLine(QPointF(11.2, 14.4), QPointF(11.2, 17.2));
        p.setPen(stroke(k.shape, 1.6));
        p.drawLine(QPointF(14.0, 17.2), QPointF(14.0, 6.4));
        p.setPen(Qt::NoPen);
        p.setBrush(k.shape);
        p.drawEllipse(QPointF(14.0, 6.4), 2.6, 2.6);
        break;

    case Glyph::Survey:
        // THE INSTRUMENT AND TWO RAYS OFF IT, with a dot on the one being read:
        // a station, its backsight and a shot.
        p.setPen(stroke(c, 1.4));
        p.drawLine(QPointF(5.2, 18.0), QPointF(20.4, 9.6));
        p.drawLine(QPointF(5.2, 18.0), QPointF(19.2, 18.0));
        grip(p, QPointF(5.2, 18.0), c);
        p.setPen(Qt::NoPen);
        p.setBrush(k.shape);
        p.drawEllipse(QPointF(20.4, 9.6), 2.6, 2.6);
        break;

    case Glyph::Chamfer:
        // A CORNER WITH ITS POINT CUT OFF, and nothing else: the two edges stop
        // short and the straight cut joins them — the cut is what the tool
        // makes, so it is the drawn line.
        p.setPen(stroke(c, 1.8));
        p.drawLine(QPointF(3.6, 19.6), QPointF(13.0, 19.6));
        p.drawLine(QPointF(19.6, 13.0), QPointF(19.6, 3.6));
        p.setPen(stroke(k.shape, 2.6));
        p.drawLine(QPointF(13.0, 19.6), QPointF(19.6, 13.0));
        break;

    case Glyph::Fillet: {
        // THE SAME CORNER ROUNDED: a quarter circle tangent to both edges —
        // centre (11.6, 11.6), radius 8 — which is what tells it from PAH's
        // straight cut at a glance.
        p.setPen(stroke(c, 1.8));
        p.drawLine(QPointF(3.6, 19.6), QPointF(11.6, 19.6));
        p.drawLine(QPointF(19.6, 11.6), QPointF(19.6, 3.6));
        QPainterPath round;
        round.moveTo(11.6, 19.6);
        round.arcTo(QRectF(3.6, 3.6, 16.0, 16.0), 270.0, 90.0);
        p.setPen(stroke(k.shape, 2.6));
        p.drawPath(round);
        break;
    }

    case Glyph::Leader:
        // THE KILAVUZ ÇİZGİ (AutoCAD's LEADER): the arrow at the thing it points
        // at, the line up to a short landing, and the note written above it.
        p.setPen(stroke(k.note, 1.8));
        p.drawLine(QPointF(5.6, 18.4), QPointF(12.4, 11.6));
        p.drawLine(QPointF(12.4, 11.6), QPointF(21.0, 11.6));
        arrowHead(p, QPointF(3.4, 20.6), QPointF(12.4, 11.6), k.note, 5.4);
        p.setPen(stroke(c, 1.6));
        p.drawLine(QPointF(13.4, 4.6), QPointF(21.0, 4.6));
        p.drawLine(QPointF(13.4, 8.0), QPointF(18.6, 8.0));
        break;
    case Glyph::Extend:
        // A LINE REACHING ON TO A BOUNDARY: the dashed stretch is what is added,
        // and the upright bar is what it stops at.
        p.setPen(stroke(c, 1.8));
        p.drawLine(QPointF(20.4, 4.0), QPointF(20.4, 20.0));
        p.setPen(stroke(k.shape, 1.8));
        p.drawLine(QPointF(3.2, 12.0), QPointF(11.0, 12.0));
        p.setPen(QPen(k.shape, 1.6, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(11.0, 12.0), QPointF(16.6, 12.0));
        arrowHead(p, QPointF(19.8, 12.0), QPointF(14.0, 12.0), k.shape, 3.8);
        break;

    case Glyph::Break:
        // A LINE WITH A PIECE TAKEN OUT: two runs, a gap, a tick at each cut.
        p.setPen(stroke(k.shape, 1.8));
        p.drawLine(QPointF(3.0, 12.0), QPointF(9.2, 12.0));
        p.drawLine(QPointF(14.8, 12.0), QPointF(21.0, 12.0));
        p.setPen(stroke(k.cut, 1.6));
        p.drawLine(QPointF(9.2, 8.2), QPointF(9.2, 15.8));
        p.drawLine(QPointF(14.8, 8.2), QPointF(14.8, 15.8));
        break;

    case Glyph::Lengthen:
        // A LINE WHOSE END WALKS ON ALONG ITSELF, with nothing to stop at: the
        // difference from UZAT is the missing boundary.
        p.setPen(stroke(k.shape, 1.8));
        p.drawLine(QPointF(3.2, 16.4), QPointF(13.0, 16.4));
        grip(p, QPointF(13.0, 16.4), c);
        p.setPen(stroke(c, 1.4));
        p.drawLine(QPointF(13.0, 9.0), QPointF(19.0, 9.0));
        arrowHead(p, QPointF(21.0, 9.0), QPointF(16.0, 9.0), c, 3.6);
        p.setPen(QPen(k.shape, 1.6, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(13.0, 16.4), QPointF(20.8, 16.4));
        break;

    case Glyph::Join:
        // TWO RUNS MEETING END TO END at the one dot they now share.
        p.setPen(stroke(k.shape, 1.8));
        p.drawPolyline(QPolygonF({QPointF(3.2, 17.6), QPointF(8.4, 8.4), QPointF(12.0, 12.0)}));
        p.drawPolyline(QPolygonF({QPointF(12.0, 12.0), QPointF(16.2, 6.4), QPointF(20.8, 15.2)}));
        p.setPen(Qt::NoPen);
        p.setBrush(k.add);
        p.drawEllipse(QPointF(12.0, 12.0), 2.8, 2.8);
        break;

    case Glyph::Explode:
        // A SQUARE COMING APART into its four edges, each pulled out a little.
        p.setPen(stroke(k.shape, 1.8));
        p.drawLine(QPointF(7.0, 4.0), QPointF(17.0, 4.0));
        p.drawLine(QPointF(20.0, 7.0), QPointF(20.0, 17.0));
        p.drawLine(QPointF(17.0, 20.0), QPointF(7.0, 20.0));
        p.drawLine(QPointF(4.0, 17.0), QPointF(4.0, 7.0));
        p.setPen(stroke(k.cut, 1.5));
        for (const QLineF& ray : {QLineF(12.0, 9.0, 12.0, 7.0), QLineF(12.0, 15.0, 12.0, 17.0),
                                  QLineF(9.0, 12.0, 7.0, 12.0), QLineF(15.0, 12.0, 17.0, 12.0)})
            p.drawLine(ray);
        break;

    case Glyph::Align:
        // A SHAPE CARRIED ONTO A PAIR OF TARGET POINTS: the small square, the
        // two marks it is put on, and the arrow between.
        {
            QPainterPath carried;
            carried.addRect(QRectF(3.4, 13.6, 6.4, 6.4));
            face(p, carried, k, 1.6);
        }
        grip(p, QPointF(15.2, 5.2), k.add);
        grip(p, QPointF(20.6, 10.6), k.add);
        p.setPen(QPen(c, 1.2, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(15.2, 5.2), QPointF(20.6, 10.6));
        p.setPen(stroke(c, 1.4));
        p.drawLine(QPointF(9.8, 13.6), QPointF(14.0, 9.4));
        arrowHead(p, QPointF(15.6, 7.8), QPointF(12.0, 11.4), c, 3.4);
        break;

    case Glyph::Divide:
        // A LINE WITH EQUAL TICKS ACROSS IT: the parts, not the points.
        p.setPen(stroke(k.shape, 1.8));
        p.drawLine(QPointF(3.0, 12.0), QPointF(21.0, 12.0));
        p.setPen(stroke(c, 1.3));
        for (const qreal x : {3.0, 7.5, 12.0, 16.5, 21.0})
            p.drawLine(QPointF(x, 8.4), QPointF(x, 15.6));
        break;

    case Glyph::VertexMove:
        // A CORNER PULLED TO A NEW PLACE: the old corner dashed, the two edges
        // following the grip, and the arrow of the move.
        p.setPen(QPen(c, 1.2, Qt::DashLine, Qt::FlatCap));
        p.drawPolyline(QPolygonF({QPointF(3.4, 20.0), QPointF(11.0, 11.0), QPointF(20.6, 20.0)}));
        p.setPen(stroke(k.shape, 1.8));
        p.drawPolyline(QPolygonF({QPointF(3.4, 20.0), QPointF(15.4, 4.4), QPointF(20.6, 20.0)}));
        grip(p, QPointF(15.4, 4.4), c);
        break;

    case Glyph::VertexAdd:
        // AN EDGE BENT THROUGH A NEW CORNER, with the plus that says it is new.
        p.setPen(stroke(k.shape, 1.8));
        p.drawPolyline(QPolygonF({QPointF(3.0, 19.0), QPointF(12.0, 9.4), QPointF(21.0, 19.0)}));
        grip(p, QPointF(12.0, 9.4), c);
        p.setPen(stroke(k.add, 2.0));
        p.drawLine(QPointF(18.6, 3.4), QPointF(18.6, 9.0));
        p.drawLine(QPointF(15.8, 6.2), QPointF(21.4, 6.2));
        break;

    case Glyph::VertexDelete:
        // THE CORNER THAT WENT: its two old edges dashed, the one straight edge
        // left in their place, and the minus that says a corner was taken.
        p.setPen(QPen(k.cut, 1.4, Qt::DashLine, Qt::FlatCap));
        p.drawPolyline(QPolygonF({QPointF(3.0, 19.0), QPointF(12.0, 9.4), QPointF(21.0, 19.0)}));
        p.setPen(stroke(k.shape, 1.8));
        p.drawLine(QPointF(3.0, 19.0), QPointF(21.0, 19.0));
        p.setPen(stroke(k.cut, 2.0));
        p.drawLine(QPointF(15.8, 6.2), QPointF(21.4, 6.2));
        break;

    case Glyph::EdgeKind:
        // A STRAIGHT EDGE BOWED INTO AN ARC: the chord it was, dashed, and the
        // arc through the handle it bends by.
        p.setPen(QPen(c, 1.2, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(3.0, 18.0), QPointF(21.0, 18.0));
        p.setPen(stroke(k.shape, 1.8));
        p.drawArc(QRectF(3.0, 6.0, 18.0, 24.0), 0, 180 * 16);
        grip(p, QPointF(12.0, 6.0), c);
        break;

    case Glyph::Boundary: {
        // CROSSING LINES AND THE CELL THEY CLOSE: the lines run past each other
        // the way loose linework does, the one face they make is filled, and
        // the click that found it sits inside.
        QColor fill = k.shape;
        fill.setAlphaF(k.fill.alphaF() * 1.6F);
        p.setPen(Qt::NoPen);
        p.setBrush(k.fill.alpha() == 0 ? QColor(Qt::transparent) : fill);
        p.drawRect(QRectF(7.0, 7.0, 10.0, 10.0));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(c, 1.6));
        p.drawLine(QPointF(3.0, 7.0), QPointF(21.0, 7.0));
        p.drawLine(QPointF(3.0, 17.0), QPointF(21.0, 17.0));
        p.drawLine(QPointF(7.0, 3.0), QPointF(7.0, 21.0));
        p.drawLine(QPointF(17.0, 3.0), QPointF(17.0, 21.0));
        p.setPen(stroke(c, 1.3));
        p.drawLine(QPointF(10.2, 12.0), QPointF(13.8, 12.0));
        p.drawLine(QPointF(12.0, 10.2), QPointF(12.0, 13.8));
        break;
    }

    case Glyph::DimChain: {
        // ONE LINE CUT INTO FIGURES: three extension lines, the dimension line
        // between them, a tick at each cut — two figures end to end.
        p.setPen(stroke(c, 1.4));
        for (const qreal x : {4.0, 11.0, 20.0})
            p.drawLine(QPointF(x, 6.0), QPointF(x, 18.0));
        p.setPen(stroke(k.note, 1.7));
        p.drawLine(QPointF(3.0, 15.0), QPointF(21.0, 15.0));
        for (const qreal x : {4.0, 11.0, 20.0})
            p.drawLine(QPointF(x - 1.6, 16.6), QPointF(x + 1.6, 13.4));
        p.setPen(stroke(k.note, 1.5));
        p.drawLine(QPointF(5.8, 11.0), QPointF(9.2, 11.0));
        p.drawLine(QPointF(13.0, 11.0), QPointF(18.0, 11.0));
        break;
    }

    case Glyph::DimBaseline: {
        // LINES STACKED FROM ONE ORIGIN: every figure starts at the left
        // extension line; the lines lie one spacing apart.
        p.setPen(stroke(c, 1.4));
        p.drawLine(QPointF(4.0, 4.0), QPointF(4.0, 20.0));
        p.drawLine(QPointF(12.0, 12.0), QPointF(12.0, 20.0));
        p.drawLine(QPointF(20.0, 6.0), QPointF(20.0, 20.0));
        p.setPen(stroke(k.note, 1.7));
        p.drawLine(QPointF(4.0, 16.0), QPointF(12.0, 16.0));
        p.drawLine(QPointF(4.0, 9.0), QPointF(20.0, 9.0));
        for (const QPointF& tip : {QPointF(12.0, 16.0), QPointF(20.0, 9.0)})
            p.drawLine(QPointF(tip.x() - 1.6, tip.y() + 1.6),
                       QPointF(tip.x() + 1.6, tip.y() - 1.6));
        break;
    }

    case Glyph::DimEdit: {
        // A DIMENSION AND THE PENCIL over its figure: what is written is being
        // changed, not what is measured.
        p.setPen(stroke(c, 1.4));
        p.drawLine(QPointF(3.0, 18.0), QPointF(3.0, 11.0));
        p.drawLine(QPointF(21.0, 18.0), QPointF(21.0, 11.0));
        p.setPen(stroke(k.note, 1.7));
        p.drawLine(QPointF(3.0, 16.0), QPointF(21.0, 16.0));
        p.setPen(stroke(k.note, 1.5));
        p.drawLine(QPointF(6.0, 12.4), QPointF(10.0, 12.4));
        p.setPen(stroke(c, 1.7));
        p.drawLine(QPointF(12.0, 13.0), QPointF(19.5, 5.5));
        p.drawLine(QPointF(19.5, 5.5), QPointF(21.0, 7.0));
        p.drawLine(QPointF(21.0, 7.0), QPointF(13.5, 14.5));
        break;
    }

    case Glyph::ToArea: {
        // AN OPEN RUN CLOSING INTO A FACE: three edges drawn, the fourth dashed
        // in, and the face filled faintly behind them.
        p.setPen(Qt::NoPen);
        p.setBrush(k.fill);
        p.drawPolygon(QPolygonF(
            {QPointF(4.0, 19.6), QPointF(4.0, 6.4), QPointF(20.0, 4.4), QPointF(20.0, 19.6)}));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(k.shape, 1.7));
        p.drawPolyline(QPolygonF(
            {QPointF(4.0, 19.6), QPointF(4.0, 6.4), QPointF(20.0, 4.4), QPointF(20.0, 19.6)}));
        p.setPen(QPen(k.add, 1.6, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(20.0, 19.6), QPointF(4.0, 19.6));
        break;
    }

    case Glyph::PolylineEdit:
        // A RUN AND ITS DIRECTION: what the command closes, opens, reverses and
        // thins is the run itself, so the arrow is on it.
        p.setPen(stroke(k.shape, 1.8));
        p.drawPolyline(QPolygonF(
            {QPointF(3.2, 18.6), QPointF(8.6, 7.4), QPointF(15.0, 15.0), QPointF(19.6, 6.0)}));
        arrowHead(p, QPointF(20.6, 4.0), QPointF(17.8, 9.4), k.shape, 4.0);
        grip(p, QPointF(8.6, 7.4), c);
        grip(p, QPointF(15.0, 15.0), c);
        break;

    case Glyph::TextEdit:
        // A LETTER AND THE PENCIL OVER IT: the words of a caption changed.
        p.setPen(stroke(c, 1.8));
        p.drawLine(QPointF(3.6, 5.0), QPointF(13.0, 5.0));
        p.drawLine(QPointF(8.3, 5.0), QPointF(8.3, 17.0));
        p.setPen(stroke(k.note, 1.9));
        p.drawLine(QPointF(12.4, 20.4), QPointF(20.4, 12.4));
        p.drawLine(QPointF(18.2, 10.2), QPointF(22.0, 14.0));
        p.drawLine(QPointF(12.4, 20.4), QPointF(11.4, 21.6));
        break;

    case Glyph::Scale:
        // A SMALL SQUARE GROWING INTO A LARGE ONE about a fixed corner, which is
        // what the command does about its centre.
        {
            QPainterPath grown;
            grown.addRect(QRectF(4.0, 4.6, 15.4, 15.4));
            face(p, grown, k, 1.8);
        }
        p.setPen(stroke(c, 1.4));
        p.drawRect(QRectF(4.0, 13.0, 7.0, 7.0));
        grip(p, QPointF(4.0, 20.0), c);
        break;

    case Glyph::Mirror:
        // A SHAPE AND ITS REFLECTION, with the axis between them: the one thing
        // the command is about is which way round things end up.
        p.setPen(stroke(c, 1.6));
        p.drawPolyline(QPolygonF({QPointF(3.4, 6.0), QPointF(9.4, 12.0), QPointF(3.4, 18.0)}));
        p.setPen(stroke(k.shape, 1.8));
        p.drawPolyline(QPolygonF({QPointF(20.6, 6.0), QPointF(14.6, 12.0), QPointF(20.6, 18.0)}));
        p.setPen(QPen(c, 1.2, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(12.0, 3.2), QPointF(12.0, 20.8));
        break;

    case Glyph::Array:
        // A GRID OF SMALL SQUARES: rows and columns, which is what the command
        // asks for.
        p.setPen(stroke(k.shape, 1.3));
        p.setBrush(k.fill);
        for (const qreal y : {5.0, 11.0, 17.0})
            for (const qreal x : {5.0, 11.0, 17.0})
                p.drawRect(QRectF(x - 1.9, y - 1.9, 3.8, 3.8));
        p.setPen(stroke(c, 1.3));
        p.setBrush(Qt::NoBrush);
        p.drawRect(QRectF(5.0 - 1.9, 5.0 - 1.9, 3.8, 3.8));
        break;

    case Glyph::BlockInsert:
        // A SQUARE DROPPED ONTO AN INSERTION CROSS: a symbol defined once and
        // placed many times, which is what a block is.
        p.setPen(stroke(c, 1.2));
        p.drawLine(QPointF(4.0, 18.6), QPointF(14.0, 18.6));
        p.drawLine(QPointF(9.0, 13.6), QPointF(9.0, 23.0));
        p.setPen(stroke(c, 1.5));
        p.setBrush(k.data);
        p.drawRect(QRectF(9.0, 3.6, 11.4, 11.4));
        p.setBrush(Qt::NoBrush);
        break;

    case Glyph::BlockEdit:
        // A BLOCK'S SQUARE AND THE PENCIL OVER IT: the definition changed, and
        // every placed copy with it.
        p.setPen(stroke(c, 1.5));
        p.setBrush(k.data);
        p.drawRect(QRectF(3.6, 3.6, 12.0, 12.0));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(k.note, 1.9));
        p.drawLine(QPointF(12.4, 20.4), QPointF(20.4, 12.4));
        p.drawLine(QPointF(18.2, 10.2), QPointF(22.0, 14.0));
        p.drawLine(QPointF(12.4, 20.4), QPointF(11.4, 21.6));
        break;

    case Glyph::BlockBase:
        // A BLOCK'S SQUARE AND ITS BASE CROSS, moved from one corner to another
        // with the square staying where it is: the base changes, the picture
        // does not.
        p.setPen(stroke(c, 1.5));
        p.setBrush(k.data);
        p.drawRect(QRectF(6.0, 4.0, 12.0, 12.0));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(c, 1.0, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(4.0, 16.0), QPointF(8.0, 16.0));
        p.drawLine(QPointF(6.0, 14.0), QPointF(6.0, 18.0));
        p.setPen(stroke(k.note, 1.8));
        p.drawLine(QPointF(15.0, 20.0), QPointF(21.0, 20.0));
        p.drawLine(QPointF(18.0, 17.0), QPointF(18.0, 23.0));
        break;

    case Glyph::Xref:
        // A PAGE AND THE DASHED SQUARE IT BECOMES ON THE SHEET: a drawing kept
        // in its own file, shown here and not owned here — dashed, as nothing
        // of it is this drawing's to edit.
        p.setPen(stroke(c, 1.3));
        p.drawLine(QPointF(3.6, 3.0), QPointF(9.6, 3.0));
        p.drawLine(QPointF(9.6, 3.0), QPointF(12.6, 6.0));
        p.drawLine(QPointF(12.6, 6.0), QPointF(12.6, 13.0));
        p.drawLine(QPointF(3.6, 3.0), QPointF(3.6, 13.0));
        p.drawLine(QPointF(3.6, 13.0), QPointF(7.0, 13.0));
        p.setPen(QPen(c, 1.3, Qt::DashLine, Qt::FlatCap));
        p.drawRect(QRectF(9.6, 10.4, 11.2, 11.2));
        p.setPen(stroke(k.note, 1.6));
        p.drawLine(QPointF(8.0, 9.0), QPointF(13.8, 14.8));
        p.drawLine(QPointF(13.8, 14.8), QPointF(13.8, 11.4));
        p.drawLine(QPointF(13.8, 14.8), QPointF(10.4, 14.8));
        break;

    case Glyph::XrefReload: {
        // THE DASHED SQUARE INSIDE A TURNING ARROW: read from its file again.
        p.setPen(QPen(c, 1.2, Qt::DashLine, Qt::FlatCap));
        p.drawRect(QRectF(8.4, 8.4, 7.2, 7.2));
        p.setPen(stroke(k.note, 1.6));
        p.drawArc(QRectF(3.4, 3.4, 17.2, 17.2), 30 * 16, 300 * 16);
        const QPointF tip(12.0 + 8.6 * 0.866, 12.0 - 8.6 * 0.5);
        p.drawLine(tip, tip + QPointF(-3.4, -0.4));
        p.drawLine(tip, tip + QPointF(0.6, 3.4));
        break;
    }

    case Glyph::XrefLocalCopy:
        // THE DASHED SQUARE AND THE SOLID ONE IT IS COPIED TO: a linked file's
        // object, not this drawing's, and its copy that is — the link stays.
        p.setPen(QPen(c, 1.2, Qt::DashLine, Qt::FlatCap));
        p.drawRect(QRectF(3.4, 3.4, 9.2, 9.2));
        p.setPen(stroke(c, 1.4));
        p.drawRect(QRectF(11.4, 11.4, 9.2, 9.2));
        p.setPen(stroke(k.note, 1.6));
        p.drawLine(QPointF(8.0, 8.0), QPointF(14.6, 14.6));
        p.drawLine(QPointF(14.6, 14.6), QPointF(14.6, 11.2));
        p.drawLine(QPointF(14.6, 14.6), QPointF(11.2, 14.6));
        break;

    case Glyph::BlockClip:
    case Glyph::BlockClipPolygon:
    case Glyph::BlockClipObject: {
        // WHAT A CLIP LEAVES: the block's circle faint where the boundary
        // hides it and whole inside, under the boundary itself — a crop's two
        // brackets, a dashed polygon drawn corner by corner, or a ring that is
        // already on the drawing.
        QPainterPath window;
        if (g == Glyph::BlockClip)
            window.addRect(QRectF(7.0, 7.0, 10.0, 10.0));
        else if (g == Glyph::BlockClipPolygon)
            window =
                polygonPath(QPolygonF({QPointF(6.0, 9.0), QPointF(14.0, 4.6), QPointF(19.6, 10.4),
                                       QPointF(16.4, 19.0), QPointF(7.4, 17.6)}));
        else
            window = polygonPath(
                QPolygonF({QPointF(8.0, 5.0), QPointF(16.0, 5.0), QPointF(20.0, 12.0),
                           QPointF(16.0, 19.0), QPointF(8.0, 19.0), QPointF(4.0, 12.0)}));
        const QRectF circle(4.4, 4.4, 15.2, 15.2);
        p.setPen(stroke(washed(c, 0.35f), 1.3));
        p.drawEllipse(circle);
        p.save();
        p.setClipPath(window, Qt::IntersectClip);
        p.setPen(stroke(c, 1.6));
        p.setBrush(k.data);
        p.drawEllipse(circle);
        p.restore();
        p.setBrush(Qt::NoBrush);
        if (g == Glyph::BlockClip) {
            p.setPen(stroke(k.note, 1.8));
            p.drawPolyline(QPolygonF({QPointF(7.0, 2.4), QPointF(7.0, 17.0), QPointF(21.6, 17.0)}));
            p.drawPolyline(QPolygonF({QPointF(2.4, 7.0), QPointF(17.0, 7.0), QPointF(17.0, 21.6)}));
        } else if (g == Glyph::BlockClipPolygon) {
            p.setPen(QPen(k.note, 1.6, Qt::DashLine, Qt::FlatCap));
            p.drawPath(window);
        } else {
            p.setPen(stroke(k.note, 1.8));
            p.drawPath(window);
        }
        break;
    }

    case Glyph::BlockClipBoundary:
        // THE CROP'S FRAME DRAWN OUT: the dashed boundary, and beside it the
        // solid line it becomes on the active layer.
        p.setPen(QPen(c, 1.2, Qt::DashLine, Qt::FlatCap));
        p.drawRect(QRectF(3.4, 3.4, 11.0, 11.0));
        p.setPen(stroke(k.note, 1.8));
        p.drawRect(QRectF(9.6, 9.6, 11.0, 11.0));
        break;

    case Glyph::BlockUnclip:
        // THE WHOLE CIRCLE AGAIN, the crop's brackets set aside and struck.
        p.setPen(stroke(c, 1.6));
        p.setBrush(k.data);
        p.drawEllipse(QRectF(3.4, 5.4, 13.2, 13.2));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(washed(c, 0.45f), 1.3));
        p.drawPolyline(QPolygonF({QPointF(6.0, 2.4), QPointF(6.0, 16.0), QPointF(19.6, 16.0)}));
        p.setPen(stroke(k.note, 1.9));
        p.drawLine(QPointF(15.0, 15.0), QPointF(21.4, 21.4));
        p.drawLine(QPointF(21.4, 15.0), QPointF(15.0, 21.4));
        break;

    case Glyph::MeasureAngle: {
        // TWO ARMS AND THE SWEEP BETWEEN THEM, which is what the tool measures
        // and what its preview now draws on the canvas.
        p.setPen(stroke(c, 1.6));
        p.drawLine(QPointF(4.4, 19.4), QPointF(20.2, 19.4));
        p.drawLine(QPointF(4.4, 19.4), QPointF(17.0, 6.0));
        p.setPen(stroke(k.note, 1.8));
        QPainterPath sweep;
        sweep.arcMoveTo(QRectF(4.4 - 9.0, 19.4 - 9.0, 18.0, 18.0), 0.0);
        sweep.arcTo(QRectF(4.4 - 9.0, 19.4 - 9.0, 18.0, 18.0), 0.0, 47.0);
        p.drawPath(sweep);
        grip(p, QPointF(4.4, 19.4), c);
        break;
    }

    case Glyph::Copy: {
        // Two offset outlines: the original and its duplicate.
        p.setPen(stroke(c, 1.5));
        p.drawRect(QRectF(3.6, 3.6, 12.0, 12.0));
        QPainterPath twin;
        twin.addRect(QRectF(8.4, 8.4, 12.0, 12.0));
        face(p, twin, k);
        break;
    }

    case Glyph::Rotate: {
        QPainterPath sweep;
        sweep.arcMoveTo(QRectF(4.2, 4.2, 15.6, 15.6), 60);
        sweep.arcTo(QRectF(4.2, 4.2, 15.6, 15.6), 60, 260);
        p.setPen(stroke(k.shape, 2.0));
        p.drawPath(sweep);
        arrowHead(p, QPointF(16.4, 5.4), QPointF(12.6, 8.6), k.shape, 4.6);
        grip(p, QPointF(12.0, 12.0), c);
        break;
    }
    case Glyph::Offset:
        // A shape and its parallel copy: the setback operation.
        p.setPen(stroke(c, 1.8));
        p.drawPolyline(QPolygonF({QPointF(3.8, 17.4), QPointF(3.8, 8.0), QPointF(12.0, 3.4),
                                  QPointF(20.2, 8.0), QPointF(20.2, 17.4)}));
        p.setPen(QPen(k.shape, 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.drawPolyline(QPolygonF({QPointF(7.0, 20.6), QPointF(7.0, 10.2), QPointF(12.0, 7.4),
                                  QPointF(17.0, 10.2), QPointF(17.0, 20.6)}));
        break;

    case Glyph::LayerManager: {
        const auto sheet = [&](qreal dy) {
            return QPolygonF({QPointF(10.0, 3.6 + dy), QPointF(17.4, 7.4 + dy),
                              QPointF(10.0, 11.2 + dy), QPointF(2.6, 7.4 + dy)});
        };
        p.drawPolygon(sheet(7.6));
        p.setPen(stroke(c, 1.2));
        p.setBrush(k.data);
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
        p.setPen(Qt::NoPen);
        p.setBrush(k.data);
        p.drawRect(QRectF(3.2, 5.0, 17.6, 4.6));
        p.setBrush(k.paper);
        p.drawRect(QRectF(3.2, 9.6, 17.6, 9.4));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(c, 1.6));
        p.drawRect(QRectF(3.2, 5.0, 17.6, 14.0));
        p.drawLine(QPointF(3.2, 9.6), QPointF(20.8, 9.6));
        p.drawLine(QPointF(3.2, 14.3), QPointF(20.8, 14.3));
        p.drawLine(QPointF(9.1, 5.0), QPointF(9.1, 19.0));
        p.drawLine(QPointF(15.0, 5.0), QPointF(15.0, 19.0));
        break;

    case Glyph::Identify:
        // Cursor over a feature: "what is this?"
        face(p,
             polygonPath(QPolygonF({QPointF(3.4, 12.0), QPointF(9.6, 4.2), QPointF(17.0, 7.6),
                                    QPointF(14.6, 15.4), QPointF(6.0, 16.2)})),
             k, 1.6);
        p.setPen(QPen(k.paper, 0.9));
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
        p.setPen(stroke(k.note, 2.0));
        p.drawRect(QRectF(8.4, 4.4, 7.2, 7.2));
        break;

    case Glyph::New:
        p.setPen(Qt::NoPen);
        p.setBrush(k.paper);
        p.drawPolygon(QPolygonF({QPointF(13.6, 3.4), QPointF(5.4, 3.4), QPointF(5.4, 20.6),
                                 QPointF(18.6, 20.6), QPointF(18.6, 8.4)}));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF({QPointF(13.6, 3.4), QPointF(5.4, 3.4), QPointF(5.4, 20.6),
                                  QPointF(18.6, 20.6), QPointF(18.6, 8.4)}));
        p.drawPolyline(QPolygonF(
            {QPointF(13.6, 3.4), QPointF(18.6, 8.4), QPointF(13.6, 8.4), QPointF(13.6, 3.4)}));
        break;

    case Glyph::Open: {
        p.setPen(Qt::NoPen);
        p.setBrush(k.data);
        p.drawPolygon(QPolygonF(
            {QPointF(2.8, 19.4), QPointF(6.6, 11.6), QPointF(21.6, 11.6), QPointF(17.8, 19.4)}));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF({QPointF(2.8, 19.4), QPointF(2.8, 5.4), QPointF(9.4, 5.4),
                                  QPointF(11.6, 8.2), QPointF(18.0, 8.2), QPointF(18.0, 11.0)}));
        p.drawPolyline(QPolygonF({QPointF(2.8, 19.4), QPointF(6.6, 11.6), QPointF(21.6, 11.6),
                                  QPointF(17.8, 19.4), QPointF(2.8, 19.4)}));
        break;
    }
    case Glyph::Save:
        // The disk in the shape's blue, its label a white page.
        p.setPen(stroke(c, 1.5));
        p.setBrush(k.shape);
        p.drawPolygon(QPolygonF({QPointF(4.0, 20.2), QPointF(4.0, 3.8), QPointF(16.6, 3.8),
                                 QPointF(20.0, 7.2), QPointF(20.0, 20.2)}));
        p.setBrush(k.paper);
        p.drawRect(QRectF(7.6, 3.8, 8.4, 5.6));
        p.drawRect(QRectF(7.0, 13.0, 10.0, 7.2));
        p.setBrush(Qt::NoBrush);
        break;

    case Glyph::Export:
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF({QPointF(13.4, 3.6), QPointF(5.0, 3.6), QPointF(5.0, 20.4),
                                  QPointF(18.4, 20.4), QPointF(18.4, 13.6)}));
        p.setPen(stroke(k.shape, 1.9));
        p.drawLine(QPointF(11.6, 11.8), QPointF(21.0, 3.6));
        arrowHead(p, QPointF(21.6, 3.0), QPointF(15.0, 8.8), k.shape, 5.2);
        break;

    case Glyph::Print:
        p.setPen(stroke(c, 1.6));
        p.drawPolyline(QPolygonF(
            {QPointF(6.4, 8.6), QPointF(6.4, 3.6), QPointF(17.6, 3.6), QPointF(17.6, 8.6)}));
        p.setBrush(k.fill);
        p.drawRoundedRect(QRectF(3.2, 8.6, 17.6, 7.6), 1.6, 1.6);
        p.setBrush(k.paper);
        p.drawRect(QRectF(6.4, 13.4, 11.2, 7.0));
        p.setPen(Qt::NoPen);
        p.setBrush(k.add);
        p.drawEllipse(QPointF(17.2, 11.4), 1.3, 1.3);
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
        // Scissors: two blades crossing over two finger rings, the rings red.
        p.setPen(stroke(c, 1.7));
        p.drawLine(QPointF(6.6, 3.6), QPointF(16.4, 15.2));
        p.drawLine(QPointF(17.4, 3.6), QPointF(7.6, 15.2));
        p.setPen(stroke(k.cut, 2.0));
        p.drawEllipse(QPointF(6.2, 18.2), 2.6, 2.6);
        p.drawEllipse(QPointF(17.8, 18.2), 2.6, 2.6);
        break;

    case Glyph::Paste:
        // A clipboard with its clip: the board amber, the sheet on it white.
        p.setPen(stroke(c, 1.5));
        p.setBrush(k.data);
        p.drawRoundedRect(QRectF(5.0, 5.0, 14.0, 15.6), 2.0, 2.0);
        p.setBrush(k.paper);
        p.drawRect(QRectF(8.2, 9.2, 7.6, 8.6));
        p.setBrush(c);
        p.drawRoundedRect(QRectF(9.0, 3.0, 6.0, 4.2), 1.2, 1.2);
        p.setBrush(Qt::NoBrush);
        break;

    case Glyph::Duplicate:
        // Two offset sheets: the copy mark, the copy the drawn one.
        p.setPen(stroke(k.shape, 1.7));
        p.setBrush(k.paper);
        p.drawRoundedRect(QRectF(8.4, 8.4, 11.6, 11.6), 2.0, 2.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(c, 1.7));
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
        for (unsigned q = 0; q < 4; ++q) {
            const qreal sx = (q & 1U) != 0U ? -1.0 : 1.0;
            const qreal sy = (q & 2U) != 0U ? -1.0 : 1.0;
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

    case Glyph::ChevronUp:
        p.setPen(stroke(c, 1.9));
        p.drawPolyline(QPolygonF({QPointF(6.6, 14.4), QPointF(12.0, 9.0), QPointF(17.4, 14.4)}));
        break;

    case Glyph::DataObject: {
        // Two braces, drawn rather than typed, so they re-tint and sit on the
        // 24 px grid like every other mark.
        p.setPen(stroke(c, 1.6));
        QPainterPath left;
        left.moveTo(9.8, 4.4);
        left.cubicTo(6.6, 4.4, 7.4, 7.6, 7.4, 9.4);
        left.cubicTo(7.4, 11.2, 5.6, 12.0, 4.6, 12.0);
        left.cubicTo(5.6, 12.0, 7.4, 12.8, 7.4, 14.6);
        left.cubicTo(7.4, 16.4, 6.6, 19.6, 9.8, 19.6);
        p.drawPath(left);
        QPainterPath right;
        right.moveTo(14.2, 4.4);
        right.cubicTo(17.4, 4.4, 16.6, 7.6, 16.6, 9.4);
        right.cubicTo(16.6, 11.2, 18.4, 12.0, 19.4, 12.0);
        right.cubicTo(18.4, 12.0, 16.6, 12.8, 16.6, 14.6);
        right.cubicTo(16.6, 16.4, 17.4, 19.6, 14.2, 19.6);
        p.drawPath(right);
        break;
    }

    case Glyph::ChevronLeft:
        p.setPen(stroke(c, 1.9));
        p.drawPolyline(QPolygonF({QPointF(14.4, 6.6), QPointF(9.0, 12.0), QPointF(14.4, 17.4)}));
        break;

    case Glyph::PageFirst:
        p.setPen(stroke(c, 1.9));
        p.drawPolyline(QPolygonF({QPointF(15.4, 6.6), QPointF(10.0, 12.0), QPointF(15.4, 17.4)}));
        p.drawLine(QPointF(7.0, 6.6), QPointF(7.0, 17.4));
        break;

    case Glyph::PageLast:
        p.setPen(stroke(c, 1.9));
        p.drawPolyline(QPolygonF({QPointF(8.6, 6.6), QPointF(14.0, 12.0), QPointF(8.6, 17.4)}));
        p.drawLine(QPointF(17.0, 6.6), QPointF(17.0, 17.4));
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
        p.save();
        p.setPen(Qt::NoPen);
        p.setBrush(k.note);
        p.drawEllipse(QPointF(12.0, 12.0), 2.0, 2.0);
        p.restore();
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
        face(p,
             polygonPath(QPolygonF({QPointF(12.0, 3.4), QPointF(20.6, 9.6), QPointF(17.3, 19.8),
                                    QPointF(6.7, 19.8), QPointF(3.4, 9.6)})),
             k, 1.7);
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

    // ---- the component standard, `bileşen_standardı.png` ----------------------
    case Glyph::Trash:
        p.setPen(stroke(c, 1.7));
        p.drawLine(QPointF(4.6, 7.0), QPointF(19.4, 7.0));
        p.drawLine(QPointF(9.4, 7.0), QPointF(9.4, 4.6));
        p.drawLine(QPointF(9.4, 4.6), QPointF(14.6, 4.6));
        p.drawLine(QPointF(14.6, 4.6), QPointF(14.6, 7.0));
        p.drawPolyline(QPolygonF(
            {QPointF(6.2, 7.0), QPointF(7.2, 20.0), QPointF(16.8, 20.0), QPointF(17.8, 7.0)}));
        p.drawLine(QPointF(10.2, 10.4), QPointF(10.5, 16.8));
        p.drawLine(QPointF(13.8, 10.4), QPointF(13.5, 16.8));
        break;

    case Glyph::Pencil:
        p.setPen(stroke(c, 1.7));
        p.drawPolygon(QPolygonF({QPointF(4.4, 19.6), QPointF(5.4, 15.2), QPointF(15.8, 4.8),
                                 QPointF(19.2, 8.2), QPointF(8.8, 18.6)}));
        p.drawLine(QPointF(13.6, 7.0), QPointF(17.0, 10.4));
        break;

    case Glyph::Tune:
        p.setPen(stroke(c, 1.7));
        for (const qreal y : {6.6, 12.0, 17.4})
            p.drawLine(QPointF(4.4, y), QPointF(19.6, y));
        p.setBrush(c);
        p.drawEllipse(QPointF(14.6, 6.6), 2.0, 2.0);
        p.drawEllipse(QPointF(9.0, 12.0), 2.0, 2.0);
        p.drawEllipse(QPointF(15.8, 17.4), 2.0, 2.0);
        break;

    case Glyph::Check:
        p.setPen(stroke(c, 2.1));
        p.drawPolyline(QPolygonF({QPointF(5.2, 12.4), QPointF(10.0, 17.2), QPointF(19.0, 7.4)}));
        break;

    case Glyph::Calendar:
        p.setPen(stroke(c, 1.7));
        p.drawRoundedRect(QRectF(4.0, 5.6, 16.0, 14.4), 1.8, 1.8);
        p.drawLine(QPointF(4.0, 10.0), QPointF(20.0, 10.0));
        p.drawLine(QPointF(8.4, 3.6), QPointF(8.4, 7.4));
        p.drawLine(QPointF(15.6, 3.6), QPointF(15.6, 7.4));
        break;

    case Glyph::Warning:
        p.setPen(stroke(c, 1.7));
        p.drawPolygon(QPolygonF({QPointF(12.0, 4.4), QPointF(20.4, 19.2), QPointF(3.6, 19.2)}));
        p.setPen(stroke(c, 1.9));
        p.drawLine(QPointF(12.0, 9.6), QPointF(12.0, 13.8));
        p.setBrush(c);
        p.drawEllipse(QPointF(12.0, 16.4), 1.1, 1.1);
        break;

    case Glyph::Info:
        p.setPen(stroke(c, 1.7));
        p.drawEllipse(QPointF(12.0, 12.0), 8.6, 8.6);
        p.setPen(stroke(c, 1.9));
        p.drawLine(QPointF(12.0, 11.0), QPointF(12.0, 16.4));
        p.setBrush(c);
        p.drawEllipse(QPointF(12.0, 7.8), 1.1, 1.1);
        break;

    case Glyph::Ruler:
        // A measure is something written: the rule in the note's orange.
        p.setPen(stroke(c, 1.5));
        p.setBrush(k.note);
        p.drawRoundedRect(QRectF(3.6, 8.4, 16.8, 7.2), 1.2, 1.2);
        p.setBrush(Qt::NoBrush);
        for (const qreal x : {7.6, 10.6, 13.6, 16.6})
            p.drawLine(QPointF(x, 8.4), QPointF(x, x == 10.6 || x == 16.6 ? 12.6 : 11.2));
        break;

    case Glyph::Sigma:
        p.setPen(stroke(c, 1.8));
        p.drawPolyline(QPolygonF({QPointF(17.6, 5.0), QPointF(6.4, 5.0), QPointF(12.6, 12.0),
                                  QPointF(6.4, 19.0), QPointF(17.6, 19.0)}));
        break;

    case Glyph::Invert:
        p.setPen(stroke(c, 1.7));
        p.drawRect(QRectF(4.0, 4.0, 11.0, 11.0));
        p.setBrush(c);
        p.drawRect(QRectF(9.0, 9.0, 11.0, 11.0));
        break;

    case Glyph::Refresh:
        // Two arcs, each ending in a right-angled head — the loop of "again",
        // drawn so it re-tints with the theme where the platform's reload icon
        // arrived in whatever colour the platform had.
        p.setPen(stroke(c, 1.7));
        p.drawArc(QRectF(4.5, 4.5, 15.0, 15.0), 20 * 16, 160 * 16);
        p.drawArc(QRectF(4.5, 4.5, 15.0, 15.0), 200 * 16, 160 * 16);
        p.drawPolyline(QPolygonF({QPointF(19.0, 4.6), QPointF(19.0, 9.4), QPointF(14.2, 9.4)}));
        p.drawPolyline(QPolygonF({QPointF(5.0, 19.4), QPointF(5.0, 14.6), QPointF(9.8, 14.6)}));
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
        p.setPen(stroke(k.shape, 1.5));
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
        p.setBrush(k.fill);
        p.setPen(QPen(k.shape, 1.5, Qt::DashLine, Qt::FlatCap));
        p.drawRect(QRectF(3.4, 3.4, 17.2, 17.2));
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPolygon(QPolygonF({QPointF(9.0, 7.6), QPointF(17.4, 13.4), QPointF(13.4, 14.2),
                                 QPointF(15.4, 18.6), QPointF(13.0, 19.6), QPointF(11.0, 15.2),
                                 QPointF(8.2, 17.6)}));
        break;

    case Glyph::Trim:
        // Scissors over the line they cut: what stays blue, what goes red.
        p.setPen(stroke(k.shape, 1.8));
        p.drawLine(QPointF(2.8, 12.0), QPointF(10.0, 12.0));
        p.setPen(QPen(k.cut, 1.8, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(14.0, 12.0), QPointF(21.2, 12.0));
        p.setPen(stroke(c, 1.6));
        p.drawLine(QPointF(8.0, 4.2), QPointF(17.0, 13.4));
        p.drawLine(QPointF(17.0, 4.2), QPointF(8.0, 13.4));
        p.drawEllipse(QPointF(7.4, 16.4), 2.4, 2.4);
        p.drawEllipse(QPointF(17.6, 16.4), 2.4, 2.4);
        break;

    case Glyph::Union: {
        // Two overlapping rings: tevhit — two faces become one.
        QPainterPath a;
        a.addEllipse(QPointF(9.0, 12.0), 6.0, 6.0);
        QPainterPath b;
        b.addEllipse(QPointF(15.0, 12.0), 6.0, 6.0);
        face(p, a.united(b), k, 1.8);
        p.setPen(QPen(k.add, 1.3, Qt::DashLine, Qt::FlatCap));
        p.drawPath(a.intersected(b));
        break;
    }

    case Glyph::ParcelSplit:
    case Glyph::AreaSplit: {
        // A parcel cut in two by the ifraz line, the new piece washed in the
        // colour of what is added; ALANİFRAZ writes the area it was cut to.
        const QPolygonF parcel(
            {QPointF(3.4, 6.6), QPointF(18.8, 3.4), QPointF(20.8, 18.6), QPointF(4.6, 20.6)});
        const QPolygonF piece(
            {QPointF(11.6, 4.9), QPointF(18.8, 3.4), QPointF(20.8, 18.6), QPointF(13.8, 19.5)});
        p.setPen(Qt::NoPen);
        p.setBrush(k.fill);
        p.drawPath(polygonPath(parcel));
        p.setBrush(washed(k.add, 0.40F));
        p.drawPath(polygonPath(piece));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(k.shape, 1.6));
        p.drawPath(polygonPath(parcel));
        p.setPen(stroke(k.cut, 1.9));
        p.drawLine(QPointF(11.6, 4.9), QPointF(13.8, 19.5));
        if (g == Glyph::AreaSplit)
            writeSmall(p, QPointF(17.0, 12.0), QStringLiteral("m²"), k.note, 6);
        break;
    }

    case Glyph::Split:
        // A shape with a cut straight through it: the generic BÖL. Deliberately
        // NOT `Trim`'s scissors, which BUDA, UZAT, PAH and YUVARLA already share —
        // on a 46 px column two tools wearing one glyph read as one control drawn
        // twice, and BÖL sat directly under BUDA.
        face(p,
             polygonPath(QPolygonF(
                 {QPointF(4.6, 8.0), QPointF(9.4, 4.4), QPointF(9.4, 19.6), QPointF(4.6, 16.0)})),
             k, 1.7);
        face(p,
             polygonPath(QPolygonF({QPointF(19.4, 8.0), QPointF(14.6, 4.4), QPointF(14.6, 19.6),
                                    QPointF(19.4, 16.0)})),
             k, 1.7);
        p.setPen(QPen(k.cut, 1.6, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(12.0, 2.6), QPointF(12.0, 21.4));
        break;

    case Glyph::MeasureArea: {
        // A set square over a filled corner: the area measure, measured orange.
        QColor wash = k.note;
        wash.setAlphaF(k.fill.alpha() == 0 ? 0.0F : 0.28F);
        p.setBrush(wash);
        p.setPen(stroke(k.note, 1.8));
        p.drawPolygon(QPolygonF({QPointF(4.0, 20.0), QPointF(20.0, 20.0), QPointF(4.0, 4.0)}));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(k.note, 1.3));
        p.drawLine(QPointF(4.0, 14.0), QPointF(10.0, 20.0));
        p.drawLine(QPointF(4.0, 9.0), QPointF(15.0, 20.0));
        break;
    }

    case Glyph::Coordinate:
        // A reticle around a filled centre: pick a coordinate.
        p.setPen(stroke(c, 1.6));
        p.drawEllipse(QPointF(12.0, 12.0), 7.2, 7.2);
        p.drawLine(QPointF(12.0, 2.6), QPointF(12.0, 5.6));
        p.drawLine(QPointF(12.0, 18.4), QPointF(12.0, 21.4));
        p.drawLine(QPointF(2.6, 12.0), QPointF(5.6, 12.0));
        p.drawLine(QPointF(18.4, 12.0), QPointF(21.4, 12.0));
        p.setPen(Qt::NoPen);
        p.setBrush(k.shape);
        p.drawEllipse(QPointF(12.0, 12.0), 2.8, 2.8);
        break;

    case Glyph::StyleCopy:
        // A pipette: lift a style off one object and put it on another.
        p.setPen(stroke(c, 1.7));
        p.drawLine(QPointF(4.2, 19.8), QPointF(13.0, 11.0));
        p.setBrush(k.data);
        p.drawPolygon(QPolygonF(
            {QPointF(11.4, 9.4), QPointF(15.4, 5.4), QPointF(18.6, 8.6), QPointF(14.6, 12.6)}));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(k.shape, 1.8));
        p.drawLine(QPointF(4.2, 19.8), QPointF(6.4, 19.8));
        break;

    case Glyph::Colour:
        // A bucket tipped to the right and the drop falling from its lip: paint
        // poured onto what is picked, the mark every drawing program uses for
        // "give this a colour".
        p.setPen(stroke(c, 1.6));
        p.drawPolygon(QPolygonF(
            {QPointF(4.4, 11.0), QPointF(10.6, 4.8), QPointF(17.2, 11.4), QPointF(11.0, 17.6)}));
        p.drawLine(QPointF(7.4, 8.0), QPointF(4.6, 5.2));
        p.setPen(Qt::NoPen);
        p.setBrush(k.cut);
        {
            QPainterPath drop;
            drop.moveTo(18.6, 13.0);
            drop.quadTo(21.2, 16.6, 21.2, 18.0);
            drop.quadTo(21.2, 20.2, 18.6, 20.2);
            drop.quadTo(16.0, 20.2, 16.0, 18.0);
            drop.quadTo(16.0, 16.6, 18.6, 13.0);
            p.drawPath(drop);
        }
        break;

    case Glyph::Topology:
        // Three nodes wired into a closed loop: the topology check.
        p.setPen(stroke(k.shape, 1.6));
        p.drawPolygon(QPolygonF({QPointF(12.0, 4.6), QPointF(19.4, 17.6), QPointF(4.6, 17.6)}));
        p.setPen(Qt::NoPen);
        p.setBrush(k.add);
        for (QPointF at : {QPointF(12.0, 4.6), QPointF(19.4, 17.6), QPointF(4.6, 17.6)})
            p.drawEllipse(at, 2.1, 2.1);
        break;

    case Glyph::Settings:
        // Three sliders: the settings mark the reference uses, not a cog.
        p.setPen(stroke(c, 1.7));
        for (int i = 0; i < 3; ++i) {
            const qreal y = 6.4 + i * 5.6;
            p.drawLine(QPointF(3.6, y), QPointF(20.4, y));
        }
        p.setPen(Qt::NoPen);
        p.setBrush(k.shape);
        p.drawEllipse(QPointF(8.2, 6.4), 2.4, 2.4);
        p.drawEllipse(QPointF(15.4, 12.0), 2.4, 2.4);
        p.drawEllipse(QPointF(10.6, 17.6), 2.4, 2.4);
        break;

    case Glyph::Grid:
        // The pattern in the shape's blue on its wash; the frame in the ink.
        p.setPen(Qt::NoPen);
        p.setBrush(k.fill);
        p.drawRect(QRectF(3.6, 3.6, 16.8, 16.8));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(k.shape, 1.3));
        for (const qreal v : {9.2, 14.8}) {
            p.drawLine(QPointF(v, 3.6), QPointF(v, 20.4));
            p.drawLine(QPointF(3.6, v), QPointF(20.4, v));
        }
        p.setPen(stroke(c, 1.5));
        p.drawRect(QRectF(3.6, 3.6, 16.8, 16.8));
        break;

        // ---- the conversation and the agent server -------------------------------

    case Glyph::Send:
        // A paper plane, as one outline and one fold line, so the mark still
        // reads at 16 px where a filled plane becomes a blob.
        p.setPen(stroke(c, 1.6));
        p.drawPolygon(QPolygonF(
            {QPointF(3.2, 11.0), QPointF(20.8, 3.6), QPointF(13.6, 20.4), QPointF(10.6, 13.4)}));
        p.drawLine(QPointF(10.6, 13.4), QPointF(20.8, 3.6));
        break;

    case Glyph::Attach:
        // A paperclip: one hairpin inside another, which is the shape a reader
        // recognises as "a file came with this".
        p.setPen(stroke(c, 1.7));
        p.drawPath(paperclip());
        break;

    case Glyph::Stop:
        // A FILLED SQUARE, not a cross: a cross closes something, a square stops
        // it, and what this button stops is still on screen afterwards.
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(QRectF(6.4, 6.4, 11.2, 11.2), 1.6, 1.6);
        break;

    case Glyph::Chat:
        // A rounded bubble with a tail at the lower left, and three dots in it.
        p.setPen(stroke(c, 1.6));
        {
            QPainterPath bubble;
            bubble.moveTo(6.0, 4.2);
            bubble.lineTo(18.0, 4.2);
            bubble.quadTo(20.8, 4.2, 20.8, 7.0);
            bubble.lineTo(20.8, 13.6);
            bubble.quadTo(20.8, 16.4, 18.0, 16.4);
            bubble.lineTo(9.6, 16.4);
            bubble.lineTo(5.6, 20.2);
            bubble.lineTo(5.6, 16.4);
            bubble.quadTo(3.2, 16.2, 3.2, 13.6);
            bubble.lineTo(3.2, 7.0);
            bubble.quadTo(3.2, 4.2, 6.0, 4.2);
            p.drawPath(bubble);
        }
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        for (int i = 0; i < 3; ++i)
            p.drawEllipse(QPointF(8.0 + i * 4.0, 10.3), 1.15, 1.15);
        break;

    case Glyph::Server:
        // Two stacked units with a status lamp each: the picture every operator
        // already reads as a listening service.
        p.setPen(stroke(c, 1.6));
        p.drawRoundedRect(QRectF(3.6, 4.6, 16.8, 6.2), 1.6, 1.6);
        p.drawRoundedRect(QRectF(3.6, 13.2, 16.8, 6.2), 1.6, 1.6);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(7.2, 7.7), 1.2, 1.2);
        p.drawEllipse(QPointF(7.2, 16.3), 1.2, 1.2);
        break;

    // ---- the ribbon's own marks ------------------------------------------
    case Glyph::Guide:
        // A construction line: it runs off both edges, and the one point it
        // was drawn through sits on it.
        p.setPen(QPen(k.shape, 1.6, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(4.6, 17.2), QPointF(19.4, 6.8));
        arrowHead(p, QPointF(2.4, 18.8), QPointF(8.0, 14.9), k.shape, 3.6);
        arrowHead(p, QPointF(21.6, 5.2), QPointF(16.0, 9.1), k.shape, 3.6);
        grip(p, QPointF(12.0, 12.0), c);
        break;

    case Glyph::Spline: {
        // The control polygon, faint and dashed, and the curve it pulls.
        p.setPen(QPen(washed(c, 0.55F), 1.0, Qt::DashLine, Qt::FlatCap));
        p.drawPolyline(QPolygonF(
            {QPointF(3.6, 18.6), QPointF(7.2, 4.8), QPointF(16.8, 19.2), QPointF(20.4, 5.4)}));
        QPainterPath curve;
        curve.moveTo(3.6, 18.6);
        curve.cubicTo(7.2, 4.8, 16.8, 19.2, 20.4, 5.4);
        p.setPen(stroke(k.shape, 2.0));
        p.drawPath(curve);
        grip(p, QPointF(3.6, 18.6), c);
        grip(p, QPointF(20.4, 5.4), c);
        break;
    }
    case Glyph::Traverse: {
        // Stations joined by the measured legs, and the angle turned at one.
        const QPointF s[] = {QPointF(3.4, 17.8), QPointF(9.4, 7.4), QPointF(15.6, 14.6),
                             QPointF(20.6, 5.0)};
        p.setPen(stroke(k.shape, 1.7));
        p.drawPolyline(s, 4);
        p.setPen(stroke(k.note, 1.4));
        p.drawArc(QRectF(9.4 - 3.8, 7.4 - 3.8, 7.6, 7.6), static_cast<int>(239.5 * 16),
                  static_cast<int>(71.2 * 16));
        p.setPen(stroke(c, 1.3));
        p.setBrush(k.paper);
        for (const QPointF& at : s)
            p.drawEllipse(at, 2.0, 2.0);
        break;
    }
    case Glyph::Helmert: {
        // The drawing as it came, dashed and askew, and where the common points
        // set it down.
        p.save();
        p.translate(8.4, 8.8);
        p.rotate(-16.0);
        p.setPen(QPen(k.note, 1.5, Qt::DashLine, Qt::FlatCap));
        p.drawRect(QRectF(-5.4, -5.4, 10.8, 10.8));
        p.restore();
        p.setPen(stroke(k.shape, 1.8));
        p.setBrush(k.fill);
        p.drawRect(QRectF(10.4, 10.2, 10.2, 10.2));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(c, 1.5));
        p.drawLine(QPointF(8.6, 9.0), QPointF(13.4, 13.8));
        arrowHead(p, QPointF(14.8, 15.2), QPointF(8.6, 9.0), c, 3.8);
        break;
    }
    case Glyph::Contour: {
        // Level lines round a summit: the outer one in ink, the higher ones in
        // the colour of what the command draws.
        const auto ring = [&p](qreal r, const QColor& ink, qreal width) {
            const qreal x = 11.6;
            const qreal y = 12.4;
            QPainterPath line;
            line.moveTo(x - 8.8 * r, y);
            line.cubicTo(x - 8.8 * r, y - 6.8 * r, x + 1.8 * r, y - 8.6 * r, x + 8.2 * r,
                         y - 4.0 * r);
            line.cubicTo(x + 10.2 * r, y + 1.8 * r, x + 3.4 * r, y + 8.2 * r, x - 3.0 * r,
                         y + 7.0 * r);
            line.cubicTo(x - 7.2 * r, y + 6.2 * r, x - 8.8 * r, y + 3.2 * r, x - 8.8 * r, y);
            p.setPen(stroke(ink, width));
            p.drawPath(line);
        };
        ring(1.0, c, 1.4);
        ring(0.64, k.shape, 1.5);
        ring(0.28, k.shape, 1.6);
        break;
    }
    case Glyph::Volume: {
        // The ground as it is over the level it is taken to: above the level is
        // cut, below it fill.
        const qreal level = 12.8;
        QPainterPath ground;
        ground.moveTo(2.6, 15.6);
        ground.cubicTo(6.6, 5.2, 10.2, 6.8, 12.6, 12.4);
        ground.cubicTo(15.0, 18.0, 18.4, 20.6, 21.4, 9.8);
        QPainterPath under = ground;
        under.lineTo(21.4, 24.0);
        under.lineTo(2.6, 24.0);
        under.closeSubpath();
        QPainterPath above;
        above.addRect(QRectF(0.0, 0.0, 24.0, level));
        QPainterPath below;
        below.addRect(QRectF(2.6, level, 18.8, 24.0 - level));
        p.setPen(Qt::NoPen);
        p.setBrush(washed(k.cut, 0.45F));
        p.drawPath(under.intersected(above));
        p.setBrush(washed(k.add, 0.45F));
        p.drawPath(below.subtracted(under));
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(k.shape, 1.4, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(2.6, level), QPointF(21.4, level));
        p.setPen(stroke(c, 1.7));
        p.drawPath(ground);
        break;
    }
    case Glyph::Buffer:
        // A line and the band a distance wide around it.
        p.setPen(QPen(k.note, 1.4, Qt::DashLine, Qt::FlatCap));
        p.setBrush(washed(k.note, 0.18F));
        p.drawRoundedRect(QRectF(2.8, 6.2, 18.4, 11.6), 5.8, 5.8);
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(k.shape, 1.9));
        p.drawPolyline(QPolygonF({QPointF(7.2, 13.6), QPointF(11.4, 10.2), QPointF(16.8, 12.0)}));
        grip(p, QPointF(7.2, 13.6), c);
        grip(p, QPointF(16.8, 12.0), c);
        break;

    case Glyph::NumberVertices: {
        // A face whose corners are numbered in order.
        const QPolygonF corners(
            {QPointF(5.4, 17.6), QPointF(8.2, 6.8), QPointF(18.6, 8.6), QPointF(17.4, 18.4)});
        face(p, polygonPath(corners), k, 1.5);
        const QPointF labels[] = {QPointF(3.4, 21.2), QPointF(5.0, 3.4), QPointF(21.0, 5.2),
                                  QPointF(20.6, 21.2)};
        for (int i = 0; i < 4; ++i)
            writeSmall(p, labels[i], QString::number(i + 1), k.note);
        break;
    }
    case Glyph::LabelLength:
        // An edge with its length written along it.
        p.setPen(stroke(k.shape, 1.8));
        p.drawLine(QPointF(3.4, 17.0), QPointF(20.6, 17.0));
        grip(p, QPointF(3.4, 17.0), c);
        grip(p, QPointF(20.6, 17.0), c);
        writeSmall(p, QPointF(12.0, 10.4), QStringLiteral("12,5"), k.note, 7);
        break;

    case Glyph::Detach:
        // The paperclip of BAĞLA, struck through.
        p.setPen(stroke(c, 1.7));
        p.drawPath(paperclip());
        p.setPen(stroke(k.cut, 2.0));
        p.drawLine(QPointF(4.2, 4.2), QPointF(19.8, 19.8));
        break;

    case Glyph::Cleanup: {
        // A broom sweeping, and two sparks where it has been.
        p.setPen(stroke(c, 1.8));
        p.drawLine(QPointF(19.6, 3.2), QPointF(12.6, 10.8));
        QPainterPath head;
        head.moveTo(9.4, 9.8);
        head.lineTo(14.6, 13.8);
        head.lineTo(10.8, 20.6);
        head.lineTo(3.2, 15.8);
        head.closeSubpath();
        p.setBrush(k.note);
        p.setPen(stroke(c, 1.4));
        p.drawPath(head);
        p.setPen(stroke(c, 1.1));
        p.drawLine(QPointF(5.4, 17.2), QPointF(10.6, 13.0));
        p.drawLine(QPointF(8.0, 18.8), QPointF(12.6, 15.2));
        const auto spark = [&p, &k](QPointF at, qreal r) {
            p.setPen(stroke(k.data, 1.4));
            p.drawLine(at - QPointF(r, 0.0), at + QPointF(r, 0.0));
            p.drawLine(at - QPointF(0.0, r), at + QPointF(0.0, r));
        };
        spark(QPointF(18.4, 15.4), 2.4);
        spark(QPointF(20.6, 20.4), 1.5);
        break;
    }
    case Glyph::DimStyle:
    case Glyph::DimRefresh: {
        // A dimension: two extension lines, the arrowed line between them.
        p.setPen(stroke(c, 1.3));
        p.drawLine(QPointF(3.6, 3.4), QPointF(3.6, 10.4));
        p.drawLine(QPointF(15.4, 3.4), QPointF(15.4, 10.4));
        p.setPen(stroke(k.note, 1.5));
        p.drawLine(QPointF(5.4, 6.4), QPointF(13.6, 6.4));
        arrowHead(p, QPointF(3.8, 6.4), QPointF(9.5, 6.4), k.note, 3.2);
        arrowHead(p, QPointF(15.2, 6.4), QPointF(9.5, 6.4), k.note, 3.2);
        if (g == Glyph::DimStyle) {
            // ... over the list of styles it is drawn from.
            for (int i = 0; i < 3; ++i) {
                const qreal y = 13.6 + i * 3.6;
                p.setPen(Qt::NoPen);
                p.setBrush(i == 0 ? k.note : washed(c, 0.5F));
                p.drawRect(QRectF(3.6, y - 1.1, 2.2, 2.2));
                p.setBrush(Qt::NoBrush);
                p.setPen(stroke(c, 1.3));
                p.drawLine(QPointF(8.0, y), QPointF(i == 1 ? 16.4 : 20.4, y));
            }
        } else {
            // ... and the arrow that goes round: set to the sheet's scale again.
            p.setPen(stroke(k.shape, 1.7));
            p.drawArc(QRectF(9.6, 11.2, 9.8, 9.8), 90 * 16, 270 * 16);
            arrowHead(p, QPointF(14.5, 11.0), QPointF(11.4, 11.6), k.shape, 3.4);
        }
        break;
    }
    case Glyph::LayerCurrent:
    case Glyph::LayerMove:
    case Glyph::LayerAdd: {
        p.setPen(stroke(c, 1.5));
        p.drawPolygon(layerSheet(6.4));
        p.setPen(stroke(c, 1.2));
        p.setBrush(k.data);
        p.drawPolygon(layerSheet(1.6));
        p.setBrush(Qt::NoBrush);
        if (g == Glyph::LayerCurrent) {
            // The tick of "this is the one in use".
            p.setPen(stroke(k.add, 2.2));
            p.drawPolyline(
                QPolygonF({QPointF(13.6, 18.0), QPointF(16.4, 20.6), QPointF(21.4, 14.6)}));
        } else if (g == Glyph::LayerMove) {
            // An object dropping onto the sheet.
            p.setPen(stroke(k.shape, 1.8));
            p.drawLine(QPointF(18.8, 13.0), QPointF(18.8, 18.4));
            arrowHead(p, QPointF(18.8, 21.4), QPointF(18.8, 14.0), k.shape, 3.6);
            p.setPen(Qt::NoPen);
            p.setBrush(k.shape);
            p.drawRect(QRectF(17.0, 9.4, 3.6, 3.6));
        } else {
            p.setPen(stroke(k.add, 2.2));
            p.drawLine(QPointF(18.2, 13.4), QPointF(18.2, 21.0));
            p.drawLine(QPointF(14.4, 17.2), QPointF(22.0, 17.2));
        }
        break;
    }
    case Glyph::LayerIsolate:
        // The one sheet left bright between the faded rest.
        p.setPen(stroke(washed(c, 0.4F), 1.2));
        p.drawPolygon(layerSheet(8.6));
        p.drawPolygon(layerSheet(0.0));
        p.setPen(stroke(c, 1.4));
        p.setBrush(k.data);
        p.drawPolygon(layerSheet(4.3));
        break;

    case Glyph::IslandNormal:
    case Glyph::IslandOuter:
    case Glyph::IslandIgnore: {
        const QRectF outer(3.2, 3.2, 17.6, 17.6);
        const QRectF middle(7.4, 7.4, 9.2, 9.2);
        const QRectF inner(10.4, 10.4, 3.2, 3.2);
        QPainterPath outerPath;
        outerPath.addRect(outer);
        QPainterPath middlePath;
        middlePath.addRect(middle);
        QPainterPath innerPath;
        innerPath.addRect(inner);
        if (g == Glyph::IslandIgnore) {
            hatchInside(p, outerPath, k.shape);
        } else {
            hatchInside(p, outerPath.subtracted(middlePath), k.shape);
            if (g == Glyph::IslandNormal) hatchInside(p, innerPath, k.shape);
        }
        p.setPen(stroke(c, 1.4));
        p.setBrush(Qt::NoBrush);
        p.drawRect(outer);
        p.drawRect(middle);
        p.drawRect(inner);
        break;
    }

    case Glyph::CopyBase:
        // The copy mark, and the base point it is copied from.
        p.setPen(stroke(k.shape, 1.7));
        p.setBrush(k.paper);
        p.drawRoundedRect(QRectF(8.4, 8.4, 11.6, 11.6), 2.0, 2.0);
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF({QPointF(15.6, 5.2), QPointF(15.6, 4.0), QPointF(4.0, 4.0),
                                  QPointF(4.0, 15.6), QPointF(5.2, 15.6)}));
        p.setPen(stroke(k.note, 1.8));
        p.drawLine(QPointF(8.4, 16.4), QPointF(8.4, 23.0));
        p.drawLine(QPointF(5.1, 19.7), QPointF(11.7, 19.7));
        break;

    case Glyph::Hatch: {
        // A closed shape and the pattern it is filled with.
        QPainterPath blob;
        blob.moveTo(4.0, 9.0);
        blob.cubicTo(4.0, 4.4, 11.0, 2.8, 15.4, 4.6);
        blob.cubicTo(20.6, 6.6, 21.4, 12.6, 19.4, 16.6);
        blob.cubicTo(17.0, 21.2, 8.6, 21.6, 5.4, 17.6);
        blob.cubicTo(3.4, 15.2, 4.0, 12.0, 4.0, 9.0);
        p.setPen(Qt::NoPen);
        p.setBrush(k.fill);
        p.drawPath(blob);
        hatchInside(p, blob, k.shape);
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(c, 1.6));
        p.drawPath(blob);
        break;
    }
    case Glyph::Label: {
        // A parcel with its label written inside: the number and the area.
        face(p,
             polygonPath(QPolygonF(
                 {QPointF(3.2, 6.4), QPointF(19.6, 3.6), QPointF(20.8, 19.4), QPointF(4.2, 20.6)})),
             k, 1.5);
        writeSmall(p, QPointF(12.2, 10.0), QStringLiteral("112"), k.note, 6);
        p.setPen(stroke(k.note, 1.3));
        p.drawLine(QPointF(8.6, 15.2), QPointF(15.8, 15.2));
        break;
    }
    case Glyph::Ortho:
        // Two arrows at a right angle, the square between them marked.
        p.setPen(stroke(k.shape, 1.8));
        p.drawLine(QPointF(5.4, 18.6), QPointF(5.4, 6.4));
        p.drawLine(QPointF(5.4, 18.6), QPointF(17.6, 18.6));
        arrowHead(p, QPointF(5.4, 3.4), QPointF(5.4, 12.0), k.shape, 3.8);
        arrowHead(p, QPointF(20.6, 18.6), QPointF(12.0, 18.6), k.shape, 3.8);
        p.setPen(stroke(c, 1.3));
        p.drawPolyline(QPolygonF({QPointF(5.4, 13.6), QPointF(10.4, 13.6), QPointF(10.4, 18.6)}));
        break;

    case Glyph::SurfaceNormal:
        // A slanted edge and the arrow standing square off it.
        p.setPen(stroke(c, 1.8));
        p.drawLine(QPointF(3.2, 18.4), QPointF(20.8, 9.6));
        p.setPen(stroke(k.shape, 1.8));
        p.drawLine(QPointF(12.0, 14.0), QPointF(8.9, 7.8));
        arrowHead(p, QPointF(7.6, 5.2), QPointF(10.6, 11.2), k.shape, 3.8);
        p.setPen(stroke(c, 1.2));
        p.drawPolyline(QPolygonF({QPointF(10.9, 11.8), QPointF(13.1, 10.7), QPointF(14.2, 12.9)}));
        break;

    case Glyph::Theme: {
        // A circle half dark, half light: the two themes.
        p.setPen(stroke(c, 1.6));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QPointF(12.0, 12.0), 8.2, 8.2);
        QPainterPath half;
        half.moveTo(12.0, 3.8);
        half.arcTo(QRectF(3.8, 3.8, 16.4, 16.4), 90.0, 180.0);
        half.closeSubpath();
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawPath(half);
        break;
    }
    case Glyph::Hud:
        // A frame-time chart in its box: what the developer overlay shows.
        p.setPen(stroke(c, 1.5));
        p.drawRoundedRect(QRectF(3.2, 4.2, 17.6, 15.6), 2.0, 2.0);
        p.setPen(Qt::NoPen);
        p.setBrush(k.shape);
        p.drawRect(QRectF(6.4, 12.4, 2.4, 4.6));
        p.drawRect(QRectF(10.2, 9.2, 2.4, 7.8));
        p.drawRect(QRectF(14.0, 11.0, 2.4, 6.0));
        p.setPen(QPen(k.note, 1.2, Qt::DashLine, Qt::FlatCap));
        p.drawLine(QPointF(5.2, 8.0), QPointF(18.8, 8.0));
        break;

    case Glyph::CommandLine:
        // A prompt and its cursor.
        p.setPen(stroke(c, 1.5));
        p.drawRoundedRect(QRectF(2.8, 5.0, 18.4, 14.0), 2.0, 2.0);
        p.setPen(stroke(k.shape, 1.9));
        p.drawPolyline(QPolygonF({QPointF(6.6, 9.2), QPointF(9.8, 12.0), QPointF(6.6, 14.8)}));
        p.setPen(stroke(c, 1.9));
        p.drawLine(QPointF(11.6, 15.0), QPointF(16.8, 15.0));
        break;

    case Glyph::Database: {
        // Three discs stacked: the server the layers are written to.
        p.setPen(stroke(c, 1.5));
        p.setBrush(k.fill);
        const QRectF top(4.6, 3.2, 14.8, 5.2);
        QPainterPath body;
        body.moveTo(4.6, 5.8);
        body.lineTo(4.6, 18.2);
        body.arcTo(QRectF(4.6, 15.6, 14.8, 5.2), 180.0, 180.0);
        body.lineTo(19.4, 5.8);
        p.drawPath(body);
        p.setBrush(k.paper);
        p.drawEllipse(top);
        p.setBrush(Qt::NoBrush);
        p.drawArc(QRectF(4.6, 7.4, 14.8, 5.2), 180 * 16, 180 * 16);
        p.drawArc(QRectF(4.6, 11.6, 14.8, 5.2), 180 * 16, 180 * 16);
        break;
    }
    case Glyph::Import:
        // Something arriving: an arrow dropping into a tray.
        p.setPen(stroke(c, 1.7));
        p.drawPolyline(QPolygonF(
            {QPointF(3.6, 13.6), QPointF(3.6, 20.2), QPointF(20.4, 20.2), QPointF(20.4, 13.6)}));
        p.setPen(stroke(k.shape, 1.9));
        p.drawLine(QPointF(12.0, 3.2), QPointF(12.0, 13.2));
        arrowHead(p, QPointF(12.0, 16.6), QPointF(12.0, 9.0), k.shape, 4.2);
        break;

    case Glyph::ProjectSettings: {
        // The project's sheet with the settings mark on it.
        QPainterPath sheet;
        sheet.moveTo(5.0, 3.0);
        sheet.lineTo(14.0, 3.0);
        sheet.lineTo(18.6, 7.6);
        sheet.lineTo(18.6, 13.0);
        sheet.moveTo(11.0, 21.0);
        sheet.lineTo(5.0, 21.0);
        sheet.lineTo(5.0, 3.0);
        p.setPen(stroke(c, 1.6));
        p.drawPath(sheet);
        p.drawLine(QPointF(8.0, 8.4), QPointF(12.4, 8.4));
        p.drawLine(QPointF(8.0, 11.8), QPointF(13.6, 11.8));
        p.setPen(stroke(k.shape, 1.6));
        p.drawEllipse(QPointF(17.0, 17.4), 2.6, 2.6);
        for (int i = 0; i < 6; ++i) {
            const qreal a = i * 60.0 * 3.14159265358979 / 180.0;
            p.drawLine(QPointF(17.0 + 3.4 * std::cos(a), 17.4 + 3.4 * std::sin(a)),
                       QPointF(17.0 + 4.6 * std::cos(a), 17.4 + 4.6 * std::sin(a)));
        }
        break;
    }
    case Glyph::Layout:
        // A pafta: the sheet, its map frame and its title block.
        p.setPen(stroke(c, 1.6));
        p.setBrush(k.paper);
        p.drawRect(QRectF(3.4, 4.2, 17.2, 15.6));
        p.setBrush(k.fill);
        p.setPen(stroke(k.shape, 1.4));
        p.drawRect(QRectF(5.8, 6.6, 12.4, 7.4));
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(c, 1.3));
        p.drawLine(QPointF(11.6, 16.2), QPointF(18.2, 16.2));
        p.drawLine(QPointF(13.4, 18.0), QPointF(18.2, 18.0));
        break;

    case Glyph::More:
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        for (int i = 0; i < 3; ++i)
            p.drawEllipse(QPointF(6.0 + i * 6.0, 12.0), 1.8, 1.8);
        break;

    case Glyph::DimAligned: {
        // THE EDGE AND ITS DIMENSION ALONG IT: two points on a slant and the
        // line that measures them parallel to the slant, arrowed at both ends.
        const QPointF a(9.0, 21.0);
        const QPointF b(21.0, 9.0);
        const QPointF n(-4.6, -4.6); ///< toward the side the dimension line lies on
        p.setPen(stroke(k.shape, 1.4));
        p.drawLine(a, b);
        p.setPen(stroke(c, 1.1));
        p.drawLine(a + (n * 0.25), a + (n * 1.2));
        p.drawLine(b + (n * 0.25), b + (n * 1.2));
        p.setPen(stroke(k.note, 1.6));
        p.drawLine(a + n, b + n);
        arrowHead(p, a + n, b + n, k.note, 3.6);
        arrowHead(p, b + n, a + n, k.note, 3.6);
        grip(p, a, c);
        grip(p, b, c);
        break;
    }

    case Glyph::DimLinear: {
        // TWO POINTS AT TWO HEIGHTS, MEASURED STRAIGHT ACROSS: the slant
        // between them is not what is measured, so it is only dashed in.
        const QPointF a(5.0, 19.6);
        const QPointF b(19.0, 12.6);
        p.setPen(QPen(k.shape, 1.1, Qt::DashLine, Qt::FlatCap));
        p.drawLine(a, b);
        p.setPen(stroke(c, 1.1));
        p.drawLine(QPointF(a.x(), a.y() - 2.2), QPointF(a.x(), 4.2));
        p.drawLine(QPointF(b.x(), b.y() - 2.2), QPointF(b.x(), 4.2));
        p.setPen(stroke(k.note, 1.6));
        p.drawLine(QPointF(a.x(), 6.6), QPointF(b.x(), 6.6));
        arrowHead(p, QPointF(a.x(), 6.6), QPointF(b.x(), 6.6), k.note, 3.6);
        arrowHead(p, QPointF(b.x(), 6.6), QPointF(a.x(), 6.6), k.note, 3.6);
        grip(p, a, c);
        grip(p, b, c);
        break;
    }

    case Glyph::DimRadius: {
        // THE CIRCLE AND ONE RADIUS: from the centre, the arrow on the rim, the
        // line running on to where its R is written.
        const QPointF centre(9.6, 14.4);
        p.setPen(stroke(k.shape, 1.6));
        p.drawEllipse(centre, 7.4, 7.4);
        const QPointF rim(centre.x() + 5.23, centre.y() - 5.23);
        p.setPen(stroke(k.note, 1.6));
        p.drawLine(centre, QPointF(19.4, 4.6));
        arrowHead(p, rim, centre, k.note, 3.6);
        grip(p, centre, c);
        writeSmall(p, QPointF(20.2, 10.4), QStringLiteral("R"), k.note, 8);
        break;
    }

    case Glyph::DimDiameter: {
        // THE CIRCLE AND THE DIAMETER THROUGH IT, arrowed at both rims, and Ø.
        const QPointF centre(10.8, 13.2);
        p.setPen(stroke(k.shape, 1.6));
        p.drawEllipse(centre, 7.6, 7.6);
        const QPointF far(centre.x() - 5.37, centre.y() + 5.37);
        const QPointF near(centre.x() + 5.37, centre.y() - 5.37);
        p.setPen(stroke(k.note, 1.6));
        p.drawLine(far, near);
        arrowHead(p, far, near, k.note, 3.6);
        arrowHead(p, near, far, k.note, 3.6);
        writeSmall(p, QPointF(19.8, 4.4), QStringLiteral("Ø"), k.note, 8);
        break;
    }

    case Glyph::DimAngular: {
        // TWO ARMS AND THE ARROWED ARC BETWEEN THEM: the angle, measured.
        const QPointF v(4.0, 19.6);
        p.setPen(stroke(k.shape, 1.6));
        p.drawLine(v, QPointF(21.0, 19.6));
        p.drawLine(v, QPointF(16.4, 5.2));
        constexpr qreal kReach = 12.0;
        const QRectF round(v.x() - kReach, v.y() - kReach, 2.0 * kReach, 2.0 * kReach);
        QPainterPath sweep;
        sweep.arcMoveTo(round, 0.0);
        sweep.arcTo(round, 0.0, 49.3);
        p.setPen(stroke(k.note, 1.6));
        p.drawPath(sweep);
        arrowHead(p, QPointF(v.x() + kReach, v.y()), QPointF(v.x() + kReach, v.y() - 3.0), k.note,
                  3.4);
        const QPointF end = sweep.currentPosition();
        arrowHead(p, end, QPointF(end.x() + 2.3, end.y() + 2.0), k.note, 3.4);
        grip(p, v, c);
        break;
    }

    case Glyph::DimOrdinate: {
        // AN ORIGIN'S TWO AXES AND A POINT, the jogged line that carries its
        // figure out to where it is written.
        p.setPen(stroke(c, 1.1));
        p.drawLine(QPointF(3.6, 20.4), QPointF(20.8, 20.4));
        p.drawLine(QPointF(3.6, 20.4), QPointF(3.6, 3.2));
        grip(p, QPointF(3.6, 20.4), c);
        const QPointF feature(12.6, 14.6);
        p.setPen(stroke(k.note, 1.6));
        p.drawPolyline(
            QPolygonF({feature, QPointF(12.6, 9.6), QPointF(16.2, 6.0), QPointF(20.8, 6.0)}));
        p.setPen(stroke(k.note, 1.3));
        p.drawLine(QPointF(15.8, 3.2), QPointF(20.8, 3.2));
        grip(p, feature, k.shape);
        break;
    }

    case Glyph::DimArcLength: {
        // THE ARC AND THE ARC BESIDE IT THAT MEASURES IT — the length along,
        // not the chord across — with the arc mark over the figure.
        const QPointF centre(12.0, 25.0);
        const auto ring = [&centre](qreal r) {
            return QRectF(centre.x() - r, centre.y() - r, 2.0 * r, 2.0 * r);
        };
        p.setPen(stroke(k.shape, 1.6));
        p.drawArc(ring(10.0), 50 * 16, 80 * 16);
        QPainterPath along;
        along.arcMoveTo(ring(14.6), 50.0);
        const QPointF right = along.currentPosition();
        along.arcTo(ring(14.6), 50.0, 80.0);
        const QPointF left = along.currentPosition();
        p.setPen(stroke(k.note, 1.6));
        p.drawPath(along);
        arrowHead(p, right, QPointF(right.x() - 2.5, right.y() - 2.1), k.note, 3.4);
        arrowHead(p, left, QPointF(left.x() + 2.5, left.y() - 2.1), k.note, 3.4);
        p.setPen(stroke(k.note, 1.3));
        p.drawArc(QRectF(8.8, 3.0, 6.4, 4.4), 0, 180 * 16);
        break;
    }

    case Glyph::Dependency: {
        // A SOURCE, THE ARROW TO WHAT WAS MADE FROM IT, AND THE MARK ON THAT:
        // is the result still what its source says?
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(6.0, 18.0), 2.6, 2.6);
        p.setBrush(Qt::NoBrush);
        p.setPen(stroke(k.shape, 1.5));
        p.drawRect(QRectF(11.5, 3.5, 9.0, 9.0));
        p.setPen(stroke(k.note, 1.6));
        p.drawLine(QPointF(8.0, 16.0), QPointF(12.6, 11.4));
        arrowHead(p, QPointF(12.6, 11.4), QPointF(8.0, 16.0), k.note, 3.4);
        p.setPen(stroke(k.note, 1.7));
        p.drawLine(QPointF(16.0, 5.6), QPointF(16.0, 8.4));
        p.drawLine(QPointF(16.0, 10.0), QPointF(16.0, 10.4));
        break;
    }

    case Glyph::Plug:
        // A two-pin plug on its lead: a client that is actually connected.
        p.setPen(stroke(c, 1.7));
        p.drawLine(QPointF(9.0, 2.8), QPointF(9.0, 7.0));
        p.drawLine(QPointF(15.0, 2.8), QPointF(15.0, 7.0));
        p.drawRoundedRect(QRectF(6.2, 7.0, 11.6, 6.0), 1.4, 1.4);
        {
            QPainterPath lead;
            lead.moveTo(12.0, 13.0);
            lead.lineTo(12.0, 16.4);
            lead.quadTo(12.0, 20.6, 16.6, 20.6);
            p.drawPath(lead);
        }
        break;
    }
}

QPixmap render(Glyph g, const GlyphInks& inks, int size, qreal dpr)
{
    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.scale(size / kGrid, size / kGrid);
    draw(p, g, inks);
    return pm;
}

QPixmap render(Glyph g, const QColor& colour, int size, qreal dpr)
{
    return render(g, GlyphInks::mono(colour), size, dpr);
}

} // namespace

QPixmap glyph_pixmap(Glyph glyph, const QColor& colour, int size, qreal dpr)
{
    return render(glyph, colour, size, dpr);
}

Glyph glyph_named(std::string_view name)
{
    struct Named
    {
        std::string_view word;
        Glyph glyph;
    };

    static constexpr Named kNamed[] = {
        {"cetvel", Glyph::Ruler},
        {"koordinat", Glyph::Coordinate},
        {"yazi", Glyph::Text},
        {"alan", Glyph::Polygon},
        {"cizgi", Glyph::Line},
        {"nokta", Glyph::Point},
        {"sigma", Glyph::Sigma},
        {"tampon", Glyph::Buffer},
        {"kose_no", Glyph::NumberVertices},
        {"uzunluk", Glyph::LabelLength},
        {"bagla", Glyph::Attach},
        {"bag_coz", Glyph::Detach},
        {"alan_uret", Glyph::ToArea},
        {"alan_duzenle", Glyph::VertexMove},
    };
    for (const Named& n : kNamed)
        if (n.word == name) return n.glyph;
    return Glyph::Function;
}

GlyphInks GlyphInks::mono(const QColor& colour)
{
    return GlyphInks{colour, colour, QColor(Qt::transparent), colour, colour,
                     colour, colour, QColor(Qt::transparent)};
}

QIcon colour_icon(Glyph glyph, const GlyphInks& inks, int size)
{
    const qreal dpr = 2.0; // rendered above the highest common ratio, then downscaled

    // A DISABLED PICTURE FADES rather than greys: the same drawing at a third of
    // its strength, so it is still the same tool, just not now.
    const auto faded = [](QColor colour) {
        colour.setAlphaF(colour.alphaF() * 0.35F);
        return colour;
    };
    const GlyphInks off{faded(inks.ink),  faded(inks.shape), faded(inks.fill), faded(inks.cut),
                        faded(inks.note), faded(inks.data),  faded(inks.add),  faded(inks.paper)};

    const QPixmap normal = render(glyph, inks, size, dpr);
    QIcon out;
    out.addPixmap(normal, QIcon::Normal, QIcon::Off);
    out.addPixmap(normal, QIcon::Normal, QIcon::On);
    out.addPixmap(normal, QIcon::Active, QIcon::Off);
    out.addPixmap(normal, QIcon::Active, QIcon::On);
    out.addPixmap(normal, QIcon::Selected, QIcon::Off);
    out.addPixmap(render(glyph, off, size, dpr), QIcon::Disabled, QIcon::Off);
    return out;
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

} // namespace kentos::app
