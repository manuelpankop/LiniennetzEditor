#include "render.h"

#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QLineF>
#include <QMarginsF>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QPen>
#include <QPolygonF>
#include <QRectF>
#include <QSvgGenerator>

#include <algorithm>
#include <cmath>

namespace {

QRectF withMargin(const QRectF& r, double m)
{
    return r.adjusted(-m, -m, m, m);
}

QRectF networkBbox(const Project& prj, const Layout& lay)
{
    QRectF bb = lay.bbox;
    if (bb.isNull())
        bb = QRectF(0, 0, 100, 100);
    return withMargin(bb, 18.0);
}

QFont makeFont(const QString& family, double size)
{
    QFont f;
    if (!family.isEmpty())
        f.setFamily(family);
    f.setPointSizeF(std::max(4.0, size));
    return f;
}

// Text aufrecht ausrichten: Winkel in (-90, 90] halten.
double uprightDeg(double deg)
{
    double d = normDeg(deg);
    if (d > 90.0)
        d -= 180.0;
    return d;
}

void drawPolyline(QPainter& p, const QVector<QPointF>& pts)
{
    if (pts.size() < 2)
        return;
    p.drawPolyline(pts.data(), pts.size());
}

// Abgerundetes Rechteck mit Linienfarbe als Hintergrund, Text global schwarz/weiss.
void drawBadge(QPainter& p, const QPointF& pos, const QString& text, const QColor& bg,
               const QColor& textCol, const QFont& font, double lineWidth)
{
    const QFontMetricsF fm(font);
    const double w = fm.horizontalAdvance(text) + lineWidth * 1.6;
    const double h = fm.height() + lineWidth * 0.5;
    QRectF r(pos.x() - w / 2, pos.y() - h / 2, w, h);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, h / 2, h / 2);
    p.setPen(QPen(textCol));
    p.setFont(font);
    p.drawText(r, Qt::AlignCenter, text);
}

// Gesamtlaenge der Linienpunkte (Stuecke + Verbinder) und Punkt an Bogenlaenge.
struct LineWalk {
    QVector<QPointF> pts;
    double total = 0.0;
    QPointF pointAt(double s) const
    {
        Poly poly;
        poly.pts = pts;
        return poly.pointAt(s);
    }
    double headingAt(double s) const
    {
        Poly poly;
        poly.pts = pts;
        return poly.headingAt(s);
    }
};

LineWalk assembleLine(const LineLay& ll)
{
    LineWalk w;
    if (ll.pieces.isEmpty())
        return w;
    w.pts = ll.pieces.first().path.pts;
    for (int i = 1; i < ll.pieces.size(); ++i) {
        const QPointF endPrev = ll.pieces[i - 1].path.endPos();
        const QPointF startNext = ll.pieces[i].path.startPos();
        if (QLineF(endPrev, startNext).length() > 0.5)
            w.pts.push_back(startNext);
        w.pts += ll.pieces[i].path.pts;
    }
    Poly p;
    p.pts = w.pts;
    w.total = p.length();
    return w;
}

} // namespace

void drawNetwork(QPainter& p, const Project& prj, const Layout& lay, bool handles)
{
    p.setRenderHint(QPainter::Antialiasing, true);

    const Settings& st = prj.settings;

    // 1) Geisterachsen (gestrichelt)
    QPen ghost(QColor(0, 0, 0, 35), 1.0, Qt::DashLine);
    p.setBrush(Qt::NoBrush);
    for (const SectionLay& sl : lay.sections) {
        p.setPen(ghost);
        drawPolyline(p, sl.axis.pts);
    }

    // 2) Linien
    for (const Id lid : prj.lineOrder) {
        if (!lay.lines.contains(lid))
            continue;
        const LineLay& ll = lay.lines[lid];
        QPen pen(prj.lines[lid].color, st.lineWidth);
        pen.setCapStyle(Qt::RoundCap);
        pen.setJoinStyle(Qt::RoundJoin);
        p.setPen(pen);
        for (const PieceLay& pc : ll.pieces)
            drawPolyline(p, pc.path.pts);
        for (const auto& cn : ll.connectors)
            p.drawLine(cn.first, cn.second);
    }

    // 3) Haltestellen-Ticks (quer ueber das Buendel)
    for (const SectionLay& sl : lay.sections) {
        const double half = st.lineWidth * 0.5 + sl.laneCount * st.bundleSpacing * 0.5;
        QPen tick(Qt::white, std::max(2.0, st.lineWidth * 0.45));
        tick.setCapStyle(Qt::FlatCap);
        for (const PlacedElem& pe : sl.elems) {
            if (pe.e.kind != ElemKind::Stop)
                continue;
            if (prj.nodeOfStop(pe.e.id) >= 0)
                continue; // Knotenhalte: Kreis statt Tick
            const double heading = sl.axis.headingAt(std::max(0.0, pe.arc));
            const QPointF dir = dirFromDeg(heading);
            const QPointF n = leftNormal(dir);
            p.setPen(tick);
            p.drawLine(pe.pos - n * half, pe.pos + n * half);
        }
    }

    // 4) Grenzpunkte: Tick und Pfeil nach aussen
    for (const SectionLay& sl : lay.sections) {
        const double half = st.lineWidth * 0.5 + sl.laneCount * st.bundleSpacing * 0.5;
        QPen tick(Qt::white, std::max(2.0, st.lineWidth * 0.45));
        QPen arrow(prj.lines.isEmpty() ? Qt::black
                                      : prj.lines[prj.lineOrder.first()].color,
                   st.lineWidth);
        for (const PlacedElem& pe : sl.elems) {
            if (pe.e.kind != ElemKind::Boundary)
                continue;
            // Austrittsrichtung: Achsrichtung am Ende bzw. Gegenrichtung am Anfang.
            double out;
            if (pe.arc > sl.axis.length() * 0.5)
                out = sl.axis.headingAt(std::max(0.0, pe.arc));
            else
                out = normDeg(sl.axis.headingAt(std::max(0.0, pe.arc)) + 180.0);
            const QPointF dir = dirFromDeg(out);
            const QPointF n = leftNormal(dir);
            p.setPen(tick);
            p.drawLine(pe.pos - n * half, pe.pos + n * half);
            // Pfeilspitze
            const double a = st.lineWidth * 1.6;
            p.setPen(arrow);
            p.drawLine(pe.pos, pe.pos + dir * a + n * a * 0.6);
            p.drawLine(pe.pos, pe.pos + dir * a - n * a * 0.6);
        }
    }

    // 5) Knotenpunkte: weisser Kreis mit Rand
    for (const Id nid : prj.nodeOrder) {
        const NodeGroup& n = prj.nodes[nid];
        // Position wie im Layout: gezogen oder Schwerpunkt.
        QPointF pos = n.pos;
        if (n.moveStamp <= 0.0) {
            QPointF sum;
            int c = 0;
            for (const Id sid : n.stopIds) {
                if (lay.stopPos.contains(sid)) {
                    sum += lay.stopPos[sid];
                    ++c;
                }
            }
            if (c > 0)
                pos = sum / double(c);
        }
        const double r = std::max(5.0, st.lineWidth * 1.1);
        p.setPen(QPen(QColor(30, 30, 30), 1.6));
        p.setBrush(Qt::white);
        p.drawEllipse(pos, r, r);
    }

    // 6) Beschriftungen (Haltestellen, Knoten, Grenzpunkte)
    const QFont stopFont = makeFont(st.stopFontFamily, st.stopFontSize);
    p.setFont(stopFont);
    for (const StopLabelLay& lbl : lay.labels) {
        p.save();
        p.translate(lbl.pos);
        p.rotate(-lbl.angle);
        p.setPen(QPen(QColor(20, 20, 20)));
        const QFontMetricsF fm(stopFont);
        const double w = fm.horizontalAdvance(lbl.text) + 4.0;
        const double h = fm.height();
        p.drawText(QRectF(-w / 2, -h / 2, w, h), Qt::AlignCenter, lbl.text);
        p.restore();
    }

    // 7) Linienbezeichnungen
    const QFont lineFont = makeFont(st.lineFontFamily, st.lineFontSize);
    const QColor textCol = st.textColor;
    for (const Id lid : prj.lineOrder) {
        if (!lay.lines.contains(lid))
            continue;
        const Line& ln = prj.lines[lid];
        const LineLay& ll = lay.lines[lid];
        const LineWalk w = assembleLine(ll);
        if (w.pts.size() < 2)
            continue;

        if (ln.labelTextOnLine) {
            const double s = w.total * 0.5;
            const QPointF pos = w.pointAt(s);
            const double ang = uprightDeg(w.headingAt(s));
            p.save();
            p.translate(pos);
            p.rotate(ang);
            p.setFont(lineFont);
            p.setPen(QPen(textCol));
            const QFontMetricsF fm(lineFont);
            const double tw = fm.horizontalAdvance(ln.name);
            const double th = fm.height();
            p.drawText(QRectF(-tw / 2 - 2, -th / 2 - 1, tw + 4, th + 2), Qt::AlignCenter, ln.name);
            p.restore();
        }

        if (ln.labelTerminiBadges) {
            const QPointF d0 = dirFromDeg(ll.startHeading);
            const QPointF dN = dirFromDeg(ll.endHeading);
            const QFontMetricsF fm(lineFont);
            const double hw = fm.horizontalAdvance(ln.name) / 2 + st.lineWidth * 0.8;
            drawBadge(p, ll.startPos + d0 * hw, ln.name, ln.color, textCol, lineFont, st.lineWidth);
            drawBadge(p, ll.endPos + dN * hw, ln.name, ln.color, textCol, lineFont, st.lineWidth);
        }

        if (ln.labelAlongBadges && w.total > st.badgeEvery * 0.6) {
            const double margin = st.badgeEvery * 0.45;
            for (double s = st.badgeEvery; s < w.total - margin; s += st.badgeEvery)
                drawBadge(p, w.pointAt(s), ln.name, ln.color, textCol, lineFont, st.lineWidth);
        }
    }

    // 8) Editor-Griffe (Via-Punkte)
    if (handles) {
        for (const Id sid : prj.sectionOrder) {
            const Section& sec = prj.sections[sid];
            p.setPen(QPen(QColor(30, 90, 200), 1.6));
            p.setBrush(Qt::white);
            for (const ViaPoint& v : sec.vias)
                p.drawEllipse(v.pos, 4.5, 4.5);
        }
    }
}

// ---------- Export ----------

bool exportPng(const QString& path, const Project& prj, const Layout& lay, double scale)
{
    const QRectF bb = networkBbox(prj, lay);
    const int w = std::max(1, int(std::ceil(bb.width() * scale)));
    const int h = std::max(1, int(std::ceil(bb.height() * scale)));
    QImage img(w, h, QImage::Format_ARGB32);
    img.fill(Qt::white);
    QPainter p(&img);
    p.scale(scale, scale);
    p.translate(-bb.topLeft());
    drawNetwork(p, prj, lay, false);
    p.end();
    return img.save(path, "PNG");
}

bool exportSvg(const QString& path, const Project& prj, const Layout& lay)
{
    const QRectF bb = networkBbox(prj, lay);
    QSvgGenerator gen;
    gen.setFileName(path);
    gen.setSize(QSize(std::max(1, int(std::ceil(bb.width()))),
                      std::max(1, int(std::ceil(bb.height())))));
    gen.setViewBox(bb);
    gen.setTitle(QStringLiteral("Liniennetzplan"));
    QPainter p(&gen);
    p.translate(-bb.topLeft());
    drawNetwork(p, prj, lay, false);
    p.end();
    return true;
}

bool exportPdf(const QString& path, const Project& prj, const Layout& lay)
{
    const QRectF bb = networkBbox(prj, lay);
    QPdfWriter writer(path);
    writer.setResolution(96);
    const double wPt = bb.width() * 72.0 / 96.0;
    const double hPt = bb.height() * 72.0 / 96.0;
    writer.setPageSize(QPageSize(QSizeF(std::max(10.0, wPt), std::max(10.0, hPt)), QPageSize::Point));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    QPainter p(&writer);
    p.translate(-bb.topLeft());
    drawNetwork(p, prj, lay, false);
    p.end();
    return true;
}
