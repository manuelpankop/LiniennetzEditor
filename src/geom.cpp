#include "geom.h"

#include <QLineF>
#include <QtMath>

#include <algorithm>
#include <cmath>

double normDeg(double a)
{
    a = std::fmod(a + 360.0, 360.0);
    if (a < 0.0)
        a += 360.0;
    return a;
}

double angleDiffDeg(double a, double b)
{
    double d = std::fmod(b - a, 360.0);
    if (d > 180.0)
        d -= 360.0;
    else if (d <= -180.0)
        d += 360.0;
    return d;
}

double snapOctDeg(double a)
{
    return normDeg(45.0 * std::round(normDeg(a) / 45.0));
}

QPointF dirFromDeg(double a)
{
    const double r = qDegreesToRadians(a);
    return QPointF(std::cos(r), std::sin(r));
}

double degFromDir(const QPointF& d)
{
    return normDeg(qRadiansToDegrees(std::atan2(d.y(), d.x())));
}

QPointF leftNormal(const QPointF& unitDir)
{
    return QPointF(unitDir.y(), -unitDir.x());
}

// ---------- Poly ----------

double Poly::length() const
{
    double L = 0.0;
    for (int i = 1; i < pts.size(); ++i)
        L += QLineF(pts[i - 1], pts[i]).length();
    return L;
}

double Poly::lengthAt(int i) const
{
    double L = 0.0;
    for (int k = 1; k <= i && k < pts.size(); ++k)
        L += QLineF(pts[k - 1], pts[k]).length();
    return L;
}

QPointF Poly::pointAt(double s) const
{
    if (pts.isEmpty())
        return QPointF();
    if (s <= 0.0)
        return pts.first();
    for (int i = 1; i < pts.size(); ++i) {
        const double L = QLineF(pts[i - 1], pts[i]).length();
        if (s <= L) {
            const double t = L > 1e-9 ? s / L : 0.0;
            return pts[i - 1] + (pts[i] - pts[i - 1]) * t;
        }
        s -= L;
    }
    return pts.last();
}

double Poly::headingAt(double s) const
{
    if (pts.size() < 2)
        return 0.0;
    if (s <= 0.0)
        return degFromDir(pts[1] - pts[0]);
    for (int i = 1; i < pts.size(); ++i) {
        const double L = QLineF(pts[i - 1], pts[i]).length();
        if (s <= L)
            return degFromDir(pts[i] - pts[i - 1]);
        s -= L;
    }
    return degFromDir(pts.last() - pts[pts.size() - 2]);
}

// ---------- Abstaende ----------

double distToSegment(const QPointF& p, const QPointF& a, const QPointF& b)
{
    const QPointF d = b - a;
    const double L2 = d.x() * d.x() + d.y() * d.y();
    if (L2 < 1e-12)
        return QLineF(p, a).length();
    double t = ((p.x() - a.x()) * d.x() + (p.y() - a.y()) * d.y()) / L2;
    t = std::clamp(t, 0.0, 1.0);
    return QLineF(p, a + d * t).length();
}

double distToPoly(const QPointF& p, const Poly& poly, QPointF* nearest, double* arcAt)
{
    double best = 1e18;
    double arc = 0.0;
    double bestArc = 0.0;
    QPointF bestPt;
    for (int i = 1; i < poly.pts.size(); ++i) {
        const QPointF a = poly.pts[i - 1];
        const QPointF b = poly.pts[i];
        const QPointF d = b - a;
        const double L2 = d.x() * d.x() + d.y() * d.y();
        double t = 0.0;
        if (L2 > 1e-12)
            t = std::clamp(((p.x() - a.x()) * d.x() + (p.y() - a.y()) * d.y()) / L2, 0.0, 1.0);
        const QPointF q = a + d * t;
        const double dist = QLineF(p, q).length();
        if (dist < best) {
            best = dist;
            bestPt = q;
            bestArc = arc + QLineF(a, b).length() * t;
        }
        arc += QLineF(a, b).length();
    }
    if (nearest)
        *nearest = bestPt;
    if (arcAt)
        *arcAt = bestArc;
    return best;
}

double polyLength(const QVector<QPointF>& pts)
{
    Poly p;
    p.pts = pts;
    return p.length();
}

// ---------- Ausrunden + Versatz ----------

Poly roundAndOffset(const QVector<QPointF>& axis, double offset, double baseR)
{
    Poly out;
    const int n = axis.size();
    if (n == 0)
        return out;
    if (n == 1) {
        out.pts.push_back(axis[0]);
        return out;
    }

    QVector<QPointF> dir(n - 1);
    for (int i = 0; i < n - 1; ++i) {
        const QPointF d = axis[i + 1] - axis[i];
        const double L = std::hypot(d.x(), d.y());
        dir[i] = L > 1e-9 ? QPointF(d.x() / L, d.y() / L) : QPointF(1, 0);
    }
    auto shift = [](const QPointF& p, const QPointF& d, double off) {
        return p + leftNormal(d) * off;
    };

    out.pts.push_back(shift(axis[0], dir.front(), offset));

    for (int i = 1; i < n - 1; ++i) {
        const QPointF d0 = dir[i - 1];
        const QPointF d1 = dir[i];
        const double cross = d0.x() * d1.y() - d0.y() * d1.x();
        const double dot = d0.x() * d1.x() + d0.y() * d1.y();
        const double turn = std::atan2(cross, dot); // rad, mathematisch
        if (std::abs(turn) < 1e-4)
            continue; // gerade Durchfuehrung

        const QPointF B = axis[i];
        const double segPrev = QLineF(axis[i - 1], axis[i]).length();
        const double segNext = QLineF(axis[i], axis[i + 1]).length();
        double tlen = baseR / std::tan(std::abs(turn) / 2.0);
        tlen = std::min({ tlen, segPrev * 0.5, segNext * 0.5 });
        double R = tlen * std::tan(std::abs(turn) / 2.0);
        if (R < 0.5)
            continue; // Ecke bleibt scharf

        const QPointF T0 = B - d0 * tlen;
        const QPointF T1 = B + d1 * tlen;
        QPointF bis = d1 - d0;
        const double bl = std::hypot(bis.x(), bis.y());
        if (bl < 1e-9)
            continue;
        bis /= bl;
        const QPointF C = B + bis * (R / std::sin(std::abs(turn) / 2.0));

        // Liegt der Mittelpunkt visuell links der einfuehrenden Richtung?
        const QPointF nrm = leftNormal(d0);
        const bool centerLeft = (nrm.x() * (C.x() - B.x()) + nrm.y() * (C.y() - B.y())) > 0.0;
        const double r2 = R - (centerLeft ? offset : -offset);

        const QPointF T0o = shift(T0, d0, offset);
        const QPointF T1o = shift(T1, d1, offset);
        if (r2 < 0.75) {
            // Bogen entartet: scharfe Ecke am versetzten Tangentenpunkt.
            if (T1o != out.pts.last())
                out.pts.push_back(T1o);
            continue;
        }

        const double a0 = std::atan2(T0.y() - C.y(), T0.x() - C.x());
        const double span = turn; // gleiche Drehrichtung wie die Ecke
        const int steps = std::max(2, int(std::ceil(std::abs(span) / qDegreesToRadians(6.0))));
        if (T0o != out.pts.last())
            out.pts.push_back(T0o);
        for (int k = 1; k <= steps; ++k) {
            const double ang = a0 + span * double(k) / double(steps);
            out.pts.push_back(C + QPointF(std::cos(ang), std::sin(ang)) * r2);
        }
    }

    const QPointF tail = shift(axis.last(), dir.back(), offset);
    if (tail != out.pts.last())
        out.pts.push_back(tail);

    // Entartete Punkte entfernen.
    Poly clean;
    for (const QPointF& p : out.pts) {
        if (clean.pts.isEmpty() || QLineF(clean.pts.last(), p).length() > 0.05)
            clean.pts.push_back(p);
    }
    return clean;
}

// ---------- oktilineare Routen ----------

namespace {

// Entfernt identische und kollineare Punkte.
QVector<QPointF> tidy(const QVector<QPointF>& in)
{
    QVector<QPointF> out;
    for (const QPointF& p : in) {
        if (out.isEmpty()) {
            out.push_back(p);
            continue;
        }
        if (QLineF(out.last(), p).length() < 1e-6)
            continue;
        if (out.size() >= 2) {
            const QPointF d0 = out[out.size() - 1] - out[out.size() - 2];
            const QPointF d1 = p - out[out.size() - 1];
            const double cross = d0.x() * d1.y() - d0.y() * d1.x();
            const double dot = d0.x() * d1.x() + d0.y() * d1.y();
            if (std::abs(std::atan2(cross, dot)) < 1e-6) {
                out.last() = p; // kollinear: verlaengern
                continue;
            }
        }
        out.push_back(p);
    }
    return out;
}

} // namespace

QVector<QVector<QPointF>> routeCandidates(const QPointF& a, const QPointF& b)
{
    QVector<QVector<QPointF>> out;
    const double dx = b.x() - a.x();
    const double dy = b.y() - a.y();
    const double adx = std::abs(dx);
    const double ady = std::abs(dy);
    if (adx < 1e-9 && ady < 1e-9)
        return out;
    if (adx < 1e-9 || ady < 1e-9 || std::abs(adx - ady) < 1e-9) {
        out.push_back(QVector<QPointF>{ a, b }); // gerade Verbindung
        if (std::abs(adx - ady) > 1e-9)
            return out;
        // Diagonale: zusaetzlich L-Kandidaten erlauben.
        out.push_back(QVector<QPointF>{ a, QPointF(b.x(), a.y()), b });
        out.push_back(QVector<QPointF>{ a, QPointF(a.x(), b.y()), b });
        return out;
    }
    const double sx = dx > 0 ? 1.0 : -1.0;
    const double sy = dy > 0 ? 1.0 : -1.0;

    // L-Familie (90-Grad-Knick)
    out.push_back(QVector<QPointF>{ a, QPointF(b.x(), a.y()), b });
    out.push_back(QVector<QPointF>{ a, QPointF(a.x(), b.y()), b });

    // H-D / D-H (nur 45-Grad-Knicke)
    if (adx >= ady) {
        const double hx = dx - sx * ady;
        out.push_back(QVector<QPointF>{ a, QPointF(a.x() + hx, a.y()), b });
        out.push_back(QVector<QPointF>{ a, QPointF(a.x() + sx * ady, b.y()), b });
    }
    // V-D / D-V (nur 45-Grad-Knicke)
    if (ady >= adx) {
        const double vy = dy - sy * adx;
        out.push_back(QVector<QPointF>{ a, QPointF(a.x(), a.y() + vy), b });
        out.push_back(QVector<QPointF>{ a, QPointF(b.x(), a.y() + sy * adx), b });
    }
    // HVH / VHV mit Slider-Varianten
    for (const double t : { 0.25, 0.5, 0.75 }) {
        const double mx = a.x() + dx * t;
        out.push_back(QVector<QPointF>{ a, QPointF(mx, a.y()), QPointF(mx, b.y()), b });
        const double my = a.y() + dy * t;
        out.push_back(QVector<QPointF>{ a, QPointF(a.x(), my), QPointF(b.x(), my), b });
    }
    return out;
}

int countBends(const QVector<QPointF>& pts)
{
    return std::max(0, tidy(pts).size() - 2);
}

bool has90Bend(const QVector<QPointF>& pts)
{
    const QVector<QPointF> t = tidy(pts);
    for (int i = 1; i < t.size() - 1; ++i) {
        const double turn = std::abs(angleDiffDeg(degFromDir(t[i] - t[i - 1]),
                                                   degFromDir(t[i + 1] - t[i])));
        if (std::abs(turn - 90.0) < 1.0)
            return true;
    }
    return false;
}

QVector<QPointF> bestRoute(const QPointF& a, const QPointF& b)
{
    QVector<QPointF> best;
    best.push_back(a);
    best.push_back(b);
    int bestBends = 2;
    double bestLen = 1e18;
    const auto cands = routeCandidates(a, b);
    for (const auto& raw : cands) {
        const QVector<QPointF> c = tidy(raw);
        const int bends = std::max(0, c.size() - 2);
        const double len = polyLength(c);
        if (bends < bestBends || (bends == bestBends && len < bestLen - 1e-9)) {
            best = c;
            bestBends = bends;
            bestLen = len;
        }
    }
    return best;
}
