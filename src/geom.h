#pragma once

#include <QPointF>
#include <QVector>

// ---------- Winkel (Bildschirmkoordinaten: y zeigt nach unten) ----------

// Normalisiert auf [0, 360).
double normDeg(double a);

// Signierte Differenz b - a, normalisiert auf (-180, 180].
double angleDiffDeg(double a, double b);

// Naechstes Vielfaches von 45 Grad, normalisiert auf [0, 360).
double snapOctDeg(double a);

// Richtungsvektor zum Winkel (Einheitsvektor).
QPointF dirFromDeg(double a);

// Winkel eines Vektors, normalisiert auf [0, 360).
double degFromDir(const QPointF& d);

// Visuell linker Normalenvektor (Bildschirm: links der Richtung).
QPointF leftNormal(const QPointF& unitDir);

// ---------- Polylinie mit Bogenlaengen-Index ----------

struct Poly {
    QVector<QPointF> pts;

    bool isEmpty() const { return pts.isEmpty(); }
    int size() const { return pts.size(); }
    QPointF startPos() const { return pts.isEmpty() ? QPointF() : pts.first(); }
    QPointF endPos() const { return pts.isEmpty() ? QPointF() : pts.last(); }

    double length() const;
    double lengthAt(int i) const;        // Bogenlaenge bis pts[i]
    QPointF pointAt(double s) const;     // Punkt an Bogenlaenge s
    double headingAt(double s) const;    // Richtung in Grad an Bogenlaenge s
};

// Abstand Punkt - Segment.
double distToSegment(const QPointF& p, const QPointF& a, const QPointF& b);

// Abstand Punkt - Polylinie; optional naechster Punkt und dessen Bogenlaenge.
double distToPoly(const QPointF& p, const Poly& poly, QPointF* nearest = nullptr,
                  double* arcAt = nullptr);

// Laenge einer Punktfolge.
double polyLength(const QVector<QPointF>& pts);

// ---------- Ausrunden + Versatz ----------

// Rundet die Ecken der Polylinie axis mit Radius baseR und verschiebt die
// gesamte Kurve um offset nach links (positiv = visuell links, in px).
// Die Rundungsbogen bleiben konzentrisch: innen liegende Linien werden
// enger, aussen liegende weiter gerundet (R' = R - d * sLeft, geklemmt).
Poly roundAndOffset(const QVector<QPointF>& axis, double offset, double baseR);

// ---------- oktilineare Routen ----------

// Alle oktilinearen Kandidatenrouten zwischen zwei Punkten
// (L-, Z-/HVH-/VHV-, H-D-, D-H-, V-D-, D-V-Familien mit Slider-Varianten).
QVector<QVector<QPointF>> routeCandidates(const QPointF& a, const QPointF& b);

// Beste Route: minimale Knickzahl, dann minimale Laenge.
QVector<QPointF> bestRoute(const QPointF& a, const QPointF& b);

// Knickzahl einer Punktfolge (kollineare/identische Punkte zaehlen nicht).
int countBends(const QVector<QPointF>& pts);

// Enthaelt die Punktfolge einen 90-Grad-Knick?
bool has90Bend(const QVector<QPointF>& pts);
