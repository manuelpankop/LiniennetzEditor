#pragma once

#include <QPointF>
#include <QRectF>
#include <QVector>

#include "geom.h"
#include "model.h"

// Berechnetes Element (Haltestelle oder Grenzpunkt) auf einem Streckenabschnitt.
struct PlacedElem {
    Elem e;
    QPointF pos;
    double arc = 0.0;
    int index = 0;
};

// Ein Linienstueck auf einem Streckenabschnitt (bereits versetzt und gerundet).
struct PieceLay {
    Id section = -1;
    bool forward = true;
    Poly path;
    double offset = 0.0; // Querabstand zur Achse (positiv = links der Achse)
};

struct LineLay {
    Id line = -1;
    QVector<PieceLay> pieces;
    QVector<QPair<QPointF, QPointF>> connectors;
    QPointF startPos, endPos;
    double startHeading = 0.0; // Grad, Richtung ausserhalb der Linie am Anfang
    double endHeading = 0.0;
    bool valid = false;
};

struct StopLabelLay {
    enum Kind { StopL = 0, NodeL = 1, BoundaryL = 2 };
    Kind kind = StopL;
    Id id = -1;
    QPointF pos;
    double angle = 0.0;
    QString text;
    QRectF aabb;
};

struct SectionLay {
    Id section = -1;
    Poly axis;
    QVector<PlacedElem> elems;   // in Abschnitts-Reihenfolge
    QHash<Id, int> laneIndex;    // Linie -> Buendelindex (0 = rechts, hoechster = links)
    int laneCount = 0;
};

struct Layout {
    QVector<SectionLay> sections;
    QHash<Id, LineLay> lines;
    QHash<Id, QPointF> stopPos;  // Haltestellenpositionen (inkl. Knoten-Haltestellen)
    QVector<StopLabelLay> labels;
    QVector<QString> warnings;
    QRectF bbox;
};

const SectionLay* findSection(const Layout& lay, Id section);

// Berechnet das komplette Liniennetz:
// oktilineare Führung, Haltestellen im globalen Abstand,
// Bündelreihenfolge und Entflechtung, Beschriftungsplatzierung.
Layout computeLayout(const Project& prj);
