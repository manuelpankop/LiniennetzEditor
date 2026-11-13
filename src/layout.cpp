#include "layout.h"

#include "l10n.h"

#include <QFont>
#include <QFontMetricsF>
#include <QLineF>
#include <QRectF>
#include <QSet>

#include <algorithm>
#include <cmath>

namespace {

using l10n::L;

// ---------- Hop-Routen ----------

// Beste oktilineare Route fuer einen einzelnen Abschnitt (Hop) zwischen zwei
// Elementen: minimiert Verletzungen (90-Grad-Knick bei Nur-45-Regel,
// Knick nahe einem Element), dann Knickzahl, dann Laenge.
QVector<QPointF> bestHopRoute(const QPointF& p, const QPointF& q, bool only45)
{
    const auto cands = routeCandidates(p, q);
    if (cands.isEmpty()) {
        QVector<QPointF> r;
        r.push_back(p);
        r.push_back(q);
        return r;
    }
    QVector<QPointF> best;
    int bestV = 1 << 30;
    int bestB = 1 << 30;
    double bestL = 1e18;
    for (const auto& raw : cands) {
        int viol = 0;
        if (only45 && has90Bend(raw))
            viol += 10;
        for (int i = 1; i + 1 < raw.size(); ++i) {
            if (QLineF(raw[i], p).length() < 5.0 || QLineF(raw[i], q).length() < 5.0)
                ++viol; // Knick zu nahe an einem Element
        }
        const int bends = countBends(raw);
        const double len = polyLength(raw);
        if (viol < bestV || (viol == bestV && (bends < bestB || (bends == bestB && len < bestL - 1e-9)))) {
            best = raw;
            bestV = viol;
            bestB = bends;
            bestL = len;
        }
    }
    return best;
}

// ---------- Traversierung ----------

// Verbundene Folge der Streckenabschnitte einer Linie:
// (Abschnitts-Id, vorwaerts?) verbunden ueber gemeinsame Endelemente.
QVector<QPair<Id, bool>> lineTraverse(const Project& prj, const Line& ln)
{
    QVector<QPair<Id, bool>> chain;
    QVector<Id> secs;
    for (const Id sid : ln.sections)
        if (prj.sections.contains(sid) && !secs.contains(sid))
            secs.push_back(sid);
    if (secs.isEmpty())
        return chain;

    QSet<Id> used;
    chain.push_back({ secs.first(), true });
    used.insert(secs.first());

    auto startElem = [&](const QPair<Id, bool>& st) {
        const QVector<Elem>& es = prj.sections[st.first].elems;
        if (es.isEmpty())
            return Elem{};
        return st.second ? es.first() : es.last();
    };
    auto endElem = [&](const QPair<Id, bool>& st) {
        const QVector<Elem>& es = prj.sections[st.first].elems;
        if (es.isEmpty())
            return Elem{};
        return st.second ? es.last() : es.first();
    };

    bool grew = true;
    while (grew) {
        grew = false;
        const Elem E = startElem(chain.front());
        for (const Id sid : secs) {
            if (used.contains(sid))
                continue;
            const QVector<Elem>& es = prj.sections[sid].elems;
            if (es.isEmpty())
                continue;
            if (sameElem(prj, es.last(), E)) {
                chain.prepend({ sid, true });
                used.insert(sid);
                grew = true;
                break;
            }
            if (sameElem(prj, es.first(), E)) {
                chain.prepend({ sid, false });
                used.insert(sid);
                grew = true;
                break;
            }
        }
    }
    grew = true;
    while (grew) {
        grew = false;
        const Elem E = endElem(chain.last());
        for (const Id sid : secs) {
            if (used.contains(sid))
                continue;
            const QVector<Elem>& es = prj.sections[sid].elems;
            if (es.isEmpty())
                continue;
            if (sameElem(prj, es.first(), E)) {
                chain.push_back({ sid, true });
                used.insert(sid);
                grew = true;
                break;
            }
            if (sameElem(prj, es.last(), E)) {
                chain.push_back({ sid, false });
                used.insert(sid);
                grew = true;
                break;
            }
        }
    }
    return chain;
}

// ---------- Streckenabschnitt platzieren ----------

// Platzriert alle Elemente eines Streckenabschnitts entlang einer oktilinearen
// Achse. Haltestellen liegen im globalen Abstand G vom Referenzanker; nur das
// letzte Intervall zum juenger bewegten Anker hin dehnt sich.
bool placeSection(const Project& prj, const Section& sec, SectionLay& sl, Layout& lay)
{
    sl.section = sec.id;
    const QVector<Elem>& elems = sec.elems;
    if (elems.size() < 2) {
        lay.warnings.push_back(L("warn.sectionunanchored").arg(sec.name));
        return false;
    }

    int ai = -1, aj = -1;
    for (int i = 0; i < elems.size(); ++i) {
        if (elems[i].kind == ElemKind::Boundary) {
            if (ai < 0)
                ai = i;
            aj = i;
        }
    }

    const double G = prj.settings.stopSpacing;

    if (ai < 0 || aj <= ai) {
        // Weniger als zwei verankerte Grenzpunkte: Fallback mit gerader
        // Verlaengerung nach Osten ab dem einzigen Anker.
        lay.warnings.push_back(L("warn.sectionunanchored").arg(sec.name));
        if (ai < 0)
            return false;
        const QPointF A = prj.boundaries[elems[ai].id].pos;
        const QPointF dir(1, 0);
        QVector<QPointF> axis;
        QVector<PlacedElem> placed;
        double arc = 0.0;
        for (int k = 0; k < elems.size(); ++k) {
            const QPointF pos = A + dir * ((ai - k) * G);
            PlacedElem pe;
            pe.e = elems[k];
            pe.pos = pos;
            pe.index = k;
            pe.arc = arc;
            placed.push_back(pe);
            arc += G;
            if (axis.isEmpty() || pos != axis.last())
                axis.push_back(pos);
        }
        sl.axis.pts = axis;
        sl.elems = placed;
        return true;
    }

    const BoundaryPoint& bA = prj.boundaries[elems[ai].id];
    const BoundaryPoint& bB = prj.boundaries[elems[aj].id];
    const QPointF A = bA.pos;
    const QPointF B = bB.pos;

    // Referenzende ist der aeltere Anker (kleinerer Zeitstempel); der
    // juenger bewegte Anker staucht nur das letzte Intervall.
    const bool reversed = bB.moveStamp < bA.moveStamp;
    QVector<int> W;
    if (!reversed) {
        for (int i = ai; i <= aj; ++i)
            W.push_back(i);
    } else {
        for (int i = aj; i >= ai; --i)
            W.push_back(i);
    }
    const int m = int(W.size()) - 2; // innere Elemente
    const int hops = m + 1;

    // Via-Punkte je Hop einsammeln.
    QVector<QVector<QPointF>> hopVias(hops);
    for (const ViaPoint& v : sec.vias) {
        for (int h = 0; h < hops; ++h) {
            if (v.afterElem == std::min(W[h], W[h + 1])) {
                hopVias[h].push_back(v.pos);
                break;
            }
        }
    }

    // Formroute (zur Verteilung der Haltestellen): Anker - Vias - Anker.
    QVector<QPointF> ctrl;
    ctrl.push_back(A);
    for (int h = 0; h < hops; ++h)
        ctrl += hopVias[h];
    ctrl.push_back(B);
    QVector<QPointF> shape;
    shape.push_back(ctrl.first());
    for (int i = 1; i < ctrl.size(); ++i) {
        const QVector<QPointF> r = bestRoute(ctrl[i - 1], ctrl[i]);
        for (int k = 1; k < r.size(); ++k)
            shape.push_back(r[k]);
    }
    Poly shapePoly;
    shapePoly.pts = shape;
    const double polyLength = shapePoly.length();

    const double needed = hops * G;
    if (polyLength < needed - 1e-6)
        lay.warnings.push_back(l10n::L("warn.sectionshort").arg(sec.name));

    // Positionen der Elemente in Laufreihenfolge.
    QVector<QPointF> pos(W.size());
    pos[0] = A;
    pos[W.size() - 1] = B;
    for (int k = 1; k <= m; ++k) {
        const double s = (L >= needed) ? k * G : k * L / hops;
        pos[k] = shapePoly.pointAt(s);
    }

    // Endgueltige Achse: Hop fuer Hop mit Kandidatenverbesserung.
    QVector<QPointF> axis;
    axis.push_back(pos[0]);
    QVector<double> hopLen(hops, 0.0);
    for (int h = 0; h < hops; ++h) {
        const QPointF p = pos[h];
        const QPointF q = pos[h + 1];
        const bool only45 = sec.intervalOnly45.value(std::min(W[h], W[h + 1]), false);
        // Vias dieses Hops nach Abstand vom Start sortieren.
        QVector<QPointF> vias = hopVias[h];
        std::sort(vias.begin(), vias.end(), [&](const QPointF& u, const QPointF& v) {
            return QLineF(p, u).length() < QLineF(p, v).length();
        });
        QVector<QPointF> hop;
        QPointF cur = p;
        hop.push_back(cur);
        for (const QPointF& vp : vias) {
            const QVector<QPointF> r = bestHopRoute(cur, vp, only45);
            for (int k = 1; k < r.size(); ++k)
                hop.push_back(r[k]);
            cur = vp;
        }
        {
            const QVector<QPointF> r = bestHopRoute(cur, q, only45);
            for (int k = 1; k < r.size(); ++k)
                hop.push_back(r[k]);
        }
        hopLen[h] = polyLength(hop);
        for (int k = 1; k < hop.size(); ++k)
            axis.push_back(hop[k]);
    }

    // Kopf-/Schwanzverlaengerung: gerade Verlaengerung entlang der ersten bzw.
    // letzten Achsenrichtung, Elemente im Abstand G vom Anker.
    QVector<QPointF> head;
    if (ai > 0 && axis.size() >= 2) {
        const QPointF d0 = axis[1] - axis[0];
        const double len = std::hypot(d0.x(), d0.y());
        const QPointF dir = len > 1e-9 ? QPointF(d0.x() / len, d0.y() / len) : QPointF(1, 0);
        for (int k = ai - 1; k >= 0; --k)
            head.push_back(A - dir * ((ai - k) * G));
    }
    QVector<QPointF> tail;
    if (aj < elems.size() - 1 && axis.size() >= 2) {
        const QPointF dN = axis.last() - axis[axis.size() - 2];
        const double len = std::hypot(dN.x(), dN.y());
        const QPointF dir = len > 1e-9 ? QPointF(dN.x() / len, dN.y() / len) : QPointF(1, 0);
        for (int k = aj + 1; k < elems.size(); ++k)
            tail.push_back(B + dir * ((k - aj) * G));
    }

    // Achse zusammensetzen und Elemente mit Bogenlaengen eintragen.
    Poly axisPoly;
    axisPoly.pts = head + axis + tail;

    QHash<int, QPointF> posOfIndex;
    QHash<int, double> arcOfIndex;
    // Kopf-Elemente: Bogenlaengen vor dem Anker (negativ).
    double headLen = 0.0;
    for (int k = ai - 1; k >= 0; --k) {
        headLen -= G;
        const QPointF dirv = (axis.size() >= 2)
            ? (axis[1] - axis[0]) / std::max(1e-9, std::hypot((axis[1] - axis[0]).x(), (axis[1] - axis[0]).y()))
            : QPointF(1, 0);
        posOfIndex.insert(k, A + dirv * headLen);
        arcOfIndex.insert(k, headLen);
    }
    {
        double arc = 0.0;
        for (int k = 0; k < int(W.size()); ++k) {
            posOfIndex.insert(W[k], pos[k]);
            arcOfIndex.insert(W[k], arc);
            if (k < hops)
                arc += hopLen[k];
        }
        if (axis.size() >= 2) {
            const QPointF dirv = (axis.last() - axis[axis.size() - 2])
                / std::max(1e-9, std::hypot((axis.last() - axis[axis.size() - 2]).x(),
                                            (axis.last() - axis[axis.size() - 2]).y()));
            double tailArc = arc;
            double tailLen = 0.0;
            for (int k = aj + 1; k < elems.size(); ++k) {
                tailLen += G;
                posOfIndex.insert(k, B + dirv * tailLen);
                arcOfIndex.insert(k, tailArc);
                tailArc += G;
            }
        }
    }

    for (int k = 0; k < elems.size(); ++k) {
        PlacedElem pe;
        pe.e = elems[k];
        pe.pos = posOfIndex.value(k);
        pe.arc = arcOfIndex.value(k);
        pe.index = k;
        sl.elems.push_back(pe);
    }
    std::stable_sort(sl.elems.begin(), sl.elems.end(),
                     [](const PlacedElem& a, const PlacedElem& b) { return a.arc < b.arc; });
    sl.axis = axisPoly;
    return true;
}

// ---------- Buendelreihenfolge und Entflechtung ----------

void computeLanes(const Project& prj, Layout& lay)
{
    for (SectionLay& sl : lay.sections) {
        QVector<Id> ids; // Linien auf diesem Abschnitt, in Erzeugungsreihenfolge
        for (const Id lid : prj.lineOrder)
            if (prj.lines[lid].sections.contains(sl.section))
                ids.push_back(lid);
        const int n = int(ids.size());
        sl.laneCount = n;
        for (int i = 0; i < n; ++i)
            sl.laneIndex.insert(ids[i], i);
        if (n < 2)
            continue;

        // Traversierung je Linie auf diesem Abschnitt.
        QHash<Id, QVector<QPair<Id, bool>>> trav;
        QHash<Id, QHash<Id, int>> posOf;
        for (const Id lid : ids) {
            trav.insert(lid, lineTraverse(prj, prj.lines[lid]));
            QHash<Id, int> po;
            for (int p = 0; p < trav[lid].size(); ++p)
                po.insert(trav[lid][p].first, p);
            posOf.insert(lid, po);
        }

        auto axisHeadingSide = [&](bool endSide) {
            if (sl.axis.pts.size() < 2)
                return 0.0;
            return endSide
                ? degFromDir(sl.axis.pts.last() - sl.axis.pts[sl.axis.pts.size() - 2])
                : degFromDir(sl.axis.pts[1] - sl.axis.pts.first());
        };

        // Austrittsseite der Linie auf diesem Abschnitt (kanonisch):
        // vorwaerts = kanonisches Ende, rueckwaerts = kanonischer Anfang.
        auto exitSide = [&](Id lid) -> bool {
            const auto& tr = trav[lid];
            const int p = posOf[lid].value(sl.section, 0);
            return (p >= 0 && p < tr.size()) ? tr[p].second : true;
        };
        // Winkelabweichung des Austritts von der Achsrichtung an der Seite side.
        auto deltaAt = [&](Id lid, bool endSide) -> double {
            const auto& tr = trav[lid];
            const int p = posOf[lid].value(sl.section, 0);
            const double interior = axisHeadingSide(endSide);
            if (p < 0 || p + 1 >= tr.size())
                return 0.0; // Linie endet hier: kein Knick
            const Id s2 = tr[p + 1].first;
            const bool f2 = tr[p + 1].second;
            const SectionLay* sec2 = findSection(lay, s2);
            if (!sec2 || sec2->axis.pts.size() < 2)
                return 0.0;
            const double exit = f2
                ? degFromDir(sec2->axis.pts[1] - sec2->axis.pts.first())
                : degFromDir(sec2->axis.pts[sec2->axis.pts.size() - 2] - sec2->axis.pts.last());
            return angleDiffDeg(interior, exit);
        };

        // Paarweise Zwaenge: kleinere Abweichung = weiter links = hoeherer Index.
        QHash<Id, QSet<Id>> leftOf;
        for (int i = 0; i < n; ++i) {
            for (int j = i + 1; j < n; ++j) {
                const Id u = ids[i];
                const Id v = ids[j];
                const bool su = exitSide(u);
                const bool sv = exitSide(v);
                if (su != sv)
                    continue; // trennen sich an verschiedenen Enden
                const double du = deltaAt(u, su);
                const double dv = deltaAt(v, sv);
                if (std::abs(du - dv) < 1e-6)
                    continue;
                if (du < dv)
                    leftOf[u].insert(v);
                else
                    leftOf[v].insert(u);
            }
        }

        // Kahn-Topologie mit Erzeugungsreihenfolge als Tie-Break.
        QHash<Id, int> indeg;
        for (const Id id : ids)
            indeg.insert(id, 0);
        for (auto it = leftOf.constBegin(); it != leftOf.constEnd(); ++it)
            for (const Id v : it.value())
                indeg[v]++;
        QVector<Id> order;
        QSet<Id> placed;
        while (order.size() < n) {
            int pick = -1;
            for (int k = 0; k < n; ++k) {
                if (placed.contains(ids[k]) || indeg[ids[k]] > 0)
                    continue;
                pick = k;
                break;
            }
            if (pick < 0)
                break; // Zyklusrest
            const Id id = ids[pick];
            order.push_back(id);
            placed.insert(id);
            if (leftOf.contains(id))
                for (const Id v : leftOf[id])
                    indeg[v]--;
        }
        for (const Id id : ids)
            if (!placed.contains(id))
                order.push_back(id);

        for (int k = 0; k < n; ++k)
            sl.laneIndex[order[k]] = k; // 0 = rechts, n-1 = links
    }
}

// ---------- Linienpfade ----------

void buildLine(const Project& prj, const Layout& lay, Id lid, LineLay& ll)
{
    const Line& ln = prj.lines[lid];
    ll.line = lid;
    const auto chain = lineTraverse(prj, ln);
    if (chain.isEmpty())
        return;
    const double cornerR = prj.settings.cornerRadius;
    for (const auto& st : chain) {
        const SectionLay* sl = findSection(lay, st.first);
        if (!sl || sl->axis.pts.size() < 2)
            continue;
        const int idx = sl->laneIndex.value(lid, 0);
        const int n = sl->laneCount;
        // Kanonischer Querabstand (kein Vorzeichnungsflip bei Gegenrichtung):
        const double d = (idx - (n - 1) / 2.0) * prj.settings.bundleSpacing;
        PieceLay piece;
        piece.section = st.first;
        piece.forward = st.second;
        piece.offset = d;
        piece.path = roundAndOffset(sl->axis.pts, d, cornerR);
        if (piece.path.size() >= 2)
            ll.pieces.push_back(piece);
    }
    if (ll.pieces.isEmpty())
        return;

    // Verbinder zwischen aufeinanderfolgenden Stuecken.
    for (int i = 1; i < ll.pieces.size(); ++i) {
        const QPointF p1 = ll.pieces[i - 1].path.endPos();
        const QPointF p2 = ll.pieces[i].path.startPos();
        if (QLineF(p1, p2).length() < 0.5)
            continue;
        const QVector<QPointF> r = bestRoute(p1, p2);
        for (int k = 1; k < r.size(); ++k)
            ll.connectors.push_back({ r[k - 1], r[k] });
    }

    ll.startPos = ll.pieces.first().path.startPos();
    ll.endPos = ll.pieces.last().path.endPos();
    const Poly& p0 = ll.pieces.first().path;
    const Poly& pN = ll.pieces.last().path;
    if (p0.pts.size() >= 2)
        ll.startHeading = degFromDir(p0.pts.first() - p0.pts[1]); // nach aussen
    if (pN.pts.size() >= 2)
        ll.endHeading = degFromDir(pN.pts.last() - pN.pts[pN.pts.size() - 2]); // nach aussen
    ll.valid = true;
}

// ---------- Beschriftungen ----------

void addLabel(Layout& lay, StopLabelLay::Kind kind, Id id, const QString& text,
              const QPointF& anchor, double heading, const LabelStyle& ls,
              const Settings& st, const QFontMetricsF& fm, int laneCount,
              QSet<QPair<int, Id>>& seen)
{
    const QPair<int, Id> key(int(kind), id);
    if (seen.contains(key))
        return;
    seen.insert(key);

    StopLabelLay lbl;
    lbl.kind = kind;
    lbl.id = id;
    lbl.text = text;
    if (ls.fixed) {
        lbl.angle = ls.angle;
        lbl.pos = anchor + ls.offset;
    } else {
        const double sn = snapOctDeg(heading);
        const bool vert = std::abs(angleDiffDeg(sn, 90.0)) < 1.0
                          || std::abs(angleDiffDeg(sn, 270.0)) < 1.0;
        lbl.angle = vert ? 90.0 : 0.0;
        const double shift = st.labelDistance + st.lineWidth * 0.5
                             + laneCount * st.bundleSpacing * 0.5;
        lbl.pos = anchor + leftNormal(dirFromDeg(heading)) * shift;
    }

    const double w = fm.horizontalAdvance(text) + 4.0;
    const double h = fm.height();
    if (lbl.angle == 0.0)
        lbl.aabb = QRectF(lbl.pos.x() - w / 2, lbl.pos.y() - h / 2, w, h);
    else if (std::abs(lbl.angle - 90.0) < 1.0)
        lbl.aabb = QRectF(lbl.pos.x() - h / 2, lbl.pos.y() - w / 2, h, w);
    else {
        const double d = (w + h) * 0.7071;
        lbl.aabb = QRectF(lbl.pos.x() - d / 2, lbl.pos.y() - d / 2, d, d);
    }
    lay.labels.push_back(lbl);
}

double headingOfStop(const Layout& lay, Id stopId)
{
    for (const SectionLay& sl : lay.sections) {
        for (const PlacedElem& pe : sl.elems) {
            if (pe.e.kind == ElemKind::Stop && pe.e.id == stopId)
                return sl.axis.headingAt(std::max(0.0, pe.arc));
        }
    }
    return 0.0;
}

void placeLabels(const Project& prj, Layout& lay)
{
    const Settings& st = prj.settings;
    QFont stopFont;
    if (!st.stopFontFamily.isEmpty())
        stopFont.setFamily(st.stopFontFamily);
    stopFont.setPointSizeF(std::max(4.0, st.stopFontSize));
    const QFontMetricsF fm(stopFont);
    QSet<QPair<int, Id>> seen;

    for (const SectionLay& sl : lay.sections) {
        for (const PlacedElem& pe : sl.elems) {
            const double heading = sl.axis.headingAt(std::max(0.0, pe.arc));
            if (pe.e.kind == ElemKind::Stop) {
                if (prj.nodeOfStop(pe.e.id) >= 0)
                    continue; // Beschriftung haengt am Knotenpunkt
                const Stop& s = prj.stops[pe.e.id];
                addLabel(lay, StopLabelLay::StopL, pe.e.id, s.name, pe.pos, heading,
                         s.label, st, fm, sl.laneCount, seen);
            } else {
                const BoundaryPoint& b = prj.boundaries[pe.e.id];
                addLabel(lay, StopLabelLay::BoundaryL, pe.e.id, b.name, pe.pos, heading,
                         b.label, st, fm, sl.laneCount, seen);
            }
        }
    }

    for (const Id nid : prj.nodeOrder) {
        const NodeGroup& n = prj.nodes[nid];
        QPointF pos = n.pos;
        if (n.moveStamp <= 0.0) { // Schwerpunkt, solange nicht gezogen
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
        double heading = 0.0;
        int laneCount = 0;
        for (const Id sid : n.stopIds) {
            if (lay.stopPos.contains(sid)) {
                heading = headingOfStop(lay, sid);
                break;
            }
        }
        for (const Id lid : prj.lineOrder) {
            const SectionLay* sl = nullptr;
            for (const SectionLay& s : lay.sections)
                if (s.section == prj.lines[lid].sections.value(0, -1))
                    sl = &s;
            if (sl)
                laneCount = std::max(laneCount, sl->laneCount);
        }
        addLabel(lay, StopLabelLay::NodeL, nid, n.name, pos, heading, n.label, st, fm,
                 laneCount, seen);
    }
}

// ---------- Bounding-Box ----------

QRectF computeBbox(const Layout& lay)
{
    QRectF bb;
    auto addPt = [&](const QPointF& p) {
        bb = bb.isNull() ? QRectF(p, p) : bb.united(QRectF(p, p));
    };
    for (const SectionLay& sl : lay.sections)
        for (const QPointF& p : sl.axis.pts)
            addPt(p);
    for (auto it = lay.lines.constBegin(); it != lay.lines.constEnd(); ++it) {
        for (const PieceLay& pc : it->pieces)
            for (const QPointF& p : pc.path.pts)
                addPt(p);
        for (const auto& cn : it->connectors) {
            addPt(cn.first);
            addPt(cn.second);
        }
    }
    for (const StopLabelLay& lbl : lay.labels)
        bb = bb.united(lbl.aabb);
    return bb;
}

// ---------- Warnungen ----------

void collectWarnings(const Project& prj, Layout& lay)
{
    for (const Id sid : prj.stopOrder) {
        if (!lay.stopPos.contains(sid))
            lay.warnings.push_back(L("warn.stopnosection").arg(prj.stops[sid].name));
    }
    for (const Id nid : prj.nodeOrder) {
        const NodeGroup& n = prj.nodes[nid];
        if (n.stopIds.size() < 2)
            lay.warnings.push_back(L("warn.nodetoofew").arg(n.name));
    }
    for (const Id lid : prj.lineOrder) {
        const Line& ln = prj.lines[lid];
        QVector<Id> secs;
        for (const Id sid : ln.sections)
            if (prj.sections.contains(sid) && !secs.contains(sid))
                secs.push_back(sid);
        if (secs.isEmpty()) {
            lay.warnings.push_back(L("warn.linenosections").arg(ln.name));
            continue;
        }
        if (lineTraverse(prj, ln).size() < secs.size())
            lay.warnings.push_back(L("warn.linenotconnected").arg(ln.name));
    }
}

} // namespace

const SectionLay* findSection(const Layout& lay, Id section)
{
    for (const SectionLay& sl : lay.sections)
        if (sl.section == section)
            return &sl;
    return nullptr;
}

Layout computeLayout(const Project& prj)
{
    Layout lay;

    for (const Id sid : prj.sectionOrder) {
        SectionLay sl;
        if (placeSection(prj, prj.sections[sid], sl, lay))
            lay.sections.push_back(sl);
    }

    for (const SectionLay& sl : lay.sections)
        for (const PlacedElem& pe : sl.elems)
            if (pe.e.kind == ElemKind::Stop)
                lay.stopPos.insert(pe.e.id, pe.pos);

    computeLanes(prj, lay);

    for (const Id lid : prj.lineOrder) {
        LineLay ll;
        buildLine(prj, lay, lid, ll);
        if (ll.valid)
            lay.lines.insert(lid, ll);
    }

    placeLabels(prj, lay);
    lay.bbox = computeBbox(lay);
    collectWarnings(prj, lay);
    return lay;
}
