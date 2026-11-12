#pragma once

#include <QColor>
#include <QHash>
#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>

using Id = int;

enum class ElemKind { Stop, Boundary };

struct Elem {
    ElemKind kind = ElemKind::Stop;
    Id id = -1;
};
bool operator==(const Elem& a, const Elem& b);
inline bool operator!=(const Elem& a, const Elem& b) { return !(a == b); }

// Automatische oder verbindlich festgelegte Beschriftungsplatzierung.
struct LabelStyle {
    bool fixed = false;
    int angle = 0;      // 0, 45 oder 90 Grad
    QPointF offset;     // fester Versatz zur Haltestelle
};

struct Stop {
    Id id = -1;
    QString name;
    LabelStyle label;
};

struct BoundaryPoint {
    Id id = -1;
    QString name;
    QPointF pos;            // Position auf der Karte (Drag&Drop)
    double moveStamp = 0.0; // Zeitstempel des letzten Verschiebens
    LabelStyle label;
};

struct ViaPoint {
    QPointF pos;
    int afterElem = 0;      // liegt zwischen elems[afterElem] und elems[afterElem+1]
};

struct Section {
    Id id = -1;
    QString name;
    QVector<Elem> elems;    // geordnete Folge: Haltestellen und Grenzpunkte
    QVector<ViaPoint> vias; // manuelle Stuetzpunkte (Nachbearbeitung der Führung)
    QHash<int, bool> intervalOnly45; // Abschnitt-Index -> true: nur 45-Grad-Knicke
};

struct NodeGroup {
    Id id = -1;
    QString name;
    QVector<Id> stopIds;
    QPointF pos;            // Kartenposition (Drag&Drop, Schritt 5)
    double moveStamp = 0.0;
    LabelStyle label;
};

struct Line {
    Id id = -1;
    QString name;
    QColor color;
    QVector<Id> sections;
    // Darstellung der Linienbezeichnung (Mehrfachauswahl)
    bool labelTextOnLine = false;
    bool labelTerminiBadges = true;
    bool labelAlongBadges = false;
};

struct Settings {
    double stopSpacing = 70.0;    // globaler Haltestellenabstand
    double lineWidth = 5.0;
    double bundleSpacing = 7.0;   // Querabstand gebuendelter Linien
    double cornerRadius = 14.0;   // Ausrundungsradius an der Streckenachse
    double labelDistance = 8.0;   // Abstand Haltestellenbeschriftung
    double badgeEvery = 260.0;    // Abstand der Rechtecke entlang der Linie
    QColor textColor = Qt::black; // Textfarbe der Linienbezeichnung (global)
    QString stopFontFamily;
    double stopFontSize = 9.0;
    QString lineFontFamily;
    double lineFontSize = 9.0;
};

class Project
{
public:
    QHash<Id, Stop> stops;
    QHash<Id, BoundaryPoint> boundaries;
    QHash<Id, Section> sections;
    QHash<Id, NodeGroup> nodes;
    QHash<Id, Line> lines;
    QVector<Id> stopOrder, boundaryOrder, sectionOrder, nodeOrder, lineOrder;
    Settings settings;
    Id nextId = 1;
    QString language = QStringLiteral("de");

    Id addStop(const QString& name);
    Id addBoundary(const QString& name);
    Id addSection(const QString& name);
    Id addNode(const QString& name, const QVector<Id>& stopIds);
    Id addLine(const QString& name, const QColor& color);

    void removeStop(Id id);
    void removeBoundary(Id id);
    void removeSection(Id id);
    void removeNode(Id id);
    void removeLine(Id id);

    Id nodeOfStop(Id stopId) const;
    QString elemName(const Elem& e) const;

    void clear();

    QJsonObject toJson() const;
    static Project fromJson(const QJsonObject& obj);
};

// Zwei Elemente sind gleich, wenn identisch oder beides Haltestellen
// desselben Knotenpunkts sind.
bool sameElem(const Project& prj, const Elem& a, const Elem& b);

// Kleines Beispielnetz.
void makeDemo(Project& prj);
