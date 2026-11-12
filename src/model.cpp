#include "model.h"

#include <QJsonArray>
#include <QJsonValue>

namespace {

LabelStyle labelFromJson(const QJsonObject& o)
{
    LabelStyle s;
    s.fixed = o.value(QStringLiteral("fixed")).toBool(false);
    s.angle = o.value(QStringLiteral("angle")).toInt(0);
    s.offset = QPointF(o.value(QStringLiteral("dx")).toDouble(0.0),
                        o.value(QStringLiteral("dy")).toDouble(0.0));
    return s;
}

QJsonObject labelToJson(const LabelStyle& s)
{
    QJsonObject o;
    o.insert(QStringLiteral("fixed"), s.fixed);
    o.insert(QStringLiteral("angle"), s.angle);
    o.insert(QStringLiteral("dx"), s.offset.x());
    o.insert(QStringLiteral("dy"), s.offset.y());
    return o;
}

} // namespace

bool operator==(const Elem& a, const Elem& b)
{
    return a.kind == b.kind && a.id == b.id;
}

bool sameElem(const Project& prj, const Elem& a, const Elem& b)
{
    if (a == b)
        return true;
    if (a.kind != ElemKind::Stop || b.kind != ElemKind::Stop)
        return false;
    const Id na = prj.nodeOfStop(a.id);
    const Id nb = prj.nodeOfStop(b.id);
    return na >= 0 && na == nb;
}

Id Project::addStop(const QString& name)
{
    const Id id = nextId++;
    Stop s;
    s.id = id;
    s.name = name;
    stops.insert(id, s);
    stopOrder.push_back(id);
    return id;
}

Id Project::addBoundary(const QString& name)
{
    const Id id = nextId++;
    BoundaryPoint b;
    b.id = id;
    b.name = name;
    // Anfangsposition: gestreut, damit neue Grenzpunkte nicht uebereinander liegen.
    const int n = boundaryOrder.size();
    b.pos = QPointF(120.0 + 60.0 * (n % 6), 90.0 + 55.0 * (n % 5));
    boundaries.insert(id, b);
    boundaryOrder.push_back(id);
    return id;
}

Id Project::addSection(const QString& name)
{
    const Id id = nextId++;
    Section s;
    s.id = id;
    s.name = name;
    sections.insert(id, s);
    sectionOrder.push_back(id);
    return id;
}

Id Project::addNode(const QString& name, const QVector<Id>& stopIds)
{
    const Id id = nextId++;
    NodeGroup n;
    n.id = id;
    n.name = name;
    n.stopIds = stopIds;
    nodes.insert(id, n);
    nodeOrder.push_back(id);
    return id;
}

Id Project::addLine(const QString& name, const QColor& color)
{
    const Id id = nextId++;
    Line l;
    l.id = id;
    l.name = name;
    l.color = color;
    lines.insert(id, l);
    lineOrder.push_back(id);
    return id;
}

void Project::removeStop(Id id)
{
    stops.remove(id);
    stopOrder.removeOne(id);

    // Aus allen Streckenabschnitten entfernen; zu kurz gewordene Abschnitte
    // fallen mitsamt den Linienverweisen weg (Kaskade).
    QVector<Id> doomedSections;
    for (const Id sid : sectionOrder) {
        Section& sec = sections[sid];
        for (int i = sec.elems.size() - 1; i >= 0; --i) {
            if (sec.elems[i].kind == ElemKind::Stop && sec.elems[i].id == id)
                sec.elems.removeAt(i);
        }
        if (sec.elems.size() < 2)
            doomedSections.push_back(sid);
    }
    for (const Id sid : doomedSections)
        removeSection(sid);

    // Aus Knotenpunkten entfernen; Knoten mit weniger als zwei Haltestellen
    // werden aufgeloest.
    QVector<Id> doomedNodes;
    for (const Id nid : nodeOrder) {
        NodeGroup& n = nodes[nid];
        n.stopIds.removeOne(id);
        if (n.stopIds.size() < 2)
            doomedNodes.push_back(nid);
    }
    for (const Id nid : doomedNodes)
        removeNode(nid);
}

void Project::removeBoundary(Id id)
{
    boundaries.remove(id);
    boundaryOrder.removeOne(id);

    QVector<Id> doomedSections;
    for (const Id sid : sectionOrder) {
        Section& sec = sections[sid];
        for (int i = sec.elems.size() - 1; i >= 0; --i) {
            if (sec.elems[i].kind == ElemKind::Boundary && sec.elems[i].id == id)
                sec.elems.removeAt(i);
        }
        if (sec.elems.size() < 2)
            doomedSections.push_back(sid);
    }
    for (const Id sid : doomedSections)
        removeSection(sid);
}

void Project::removeSection(Id id)
{
    sections.remove(id);
    sectionOrder.removeOne(id);
    for (const Id lid : lineOrder)
        lines[lid].sections.removeOne(id);
}

void Project::removeNode(Id id)
{
    nodes.remove(id);
    nodeOrder.removeOne(id);
}

void Project::removeLine(Id id)
{
    lines.remove(id);
    lineOrder.removeOne(id);
}

Id Project::nodeOfStop(Id stopId) const
{
    for (const Id nid : nodeOrder) {
        const NodeGroup& n = nodes[nid];
        if (n.stopIds.contains(stopId))
            return nid;
    }
    return -1;
}

QString Project::elemName(const Elem& e) const
{
    if (e.kind == ElemKind::Stop)
        return stops.value(e.id).name;
    return boundaries.value(e.id).name;
}

void Project::clear()
{
    stops.clear();
    boundaries.clear();
    sections.clear();
    nodes.clear();
    lines.clear();
    stopOrder.clear();
    boundaryOrder.clear();
    sectionOrder.clear();
    nodeOrder.clear();
    lineOrder.clear();
    settings = Settings();
    nextId = 1;
    language = QStringLiteral("de");
}

// ---------- JSON ----------

QJsonObject Project::toJson() const
{
    QJsonObject root;
    root.insert(QStringLiteral("nextId"), nextId);
    root.insert(QStringLiteral("language"), language);

    {
        QJsonArray arr;
        for (const Id id : stopOrder) {
            const Stop& s = stops[id];
            QJsonObject o;
            o.insert(QStringLiteral("id"), id);
            o.insert(QStringLiteral("name"), s.name);
            o.insert(QStringLiteral("label"), labelToJson(s.label));
            arr.append(o);
        }
        root.insert(QStringLiteral("stops"), arr);
    }
    {
        QJsonArray arr;
        for (const Id id : boundaryOrder) {
            const BoundaryPoint& b = boundaries[id];
            QJsonObject o;
            o.insert(QStringLiteral("id"), id);
            o.insert(QStringLiteral("name"), b.name);
            o.insert(QStringLiteral("x"), b.pos.x());
            o.insert(QStringLiteral("y"), b.pos.y());
            o.insert(QStringLiteral("moveStamp"), b.moveStamp);
            o.insert(QStringLiteral("label"), labelToJson(b.label));
            arr.append(o);
        }
        root.insert(QStringLiteral("boundaries"), arr);
    }
    {
        QJsonArray arr;
        for (const Id id : sectionOrder) {
            const Section& s = sections[id];
            QJsonObject o;
            o.insert(QStringLiteral("id"), id);
            o.insert(QStringLiteral("name"), s.name);
            QJsonArray el;
            for (const Elem& e : s.elems) {
                QJsonObject eo;
                eo.insert(QStringLiteral("kind"),
                          e.kind == ElemKind::Stop ? QStringLiteral("stop")
                                                   : QStringLiteral("boundary"));
                eo.insert(QStringLiteral("id"), e.id);
                el.append(eo);
            }
            o.insert(QStringLiteral("elems"), el);
            QJsonArray vi;
            for (const ViaPoint& v : s.vias) {
                QJsonObject vo;
                vo.insert(QStringLiteral("x"), v.pos.x());
                vo.insert(QStringLiteral("y"), v.pos.y());
                vo.insert(QStringLiteral("afterElem"), v.afterElem);
                vi.append(vo);
            }
            o.insert(QStringLiteral("vias"), vi);
            QJsonObject pol;
            for (auto it = s.intervalOnly45.constBegin(); it != s.intervalOnly45.constEnd(); ++it)
                pol.insert(QString::number(it.key()), it.value());
            o.insert(QStringLiteral("intervalOnly45"), pol);
            arr.append(o);
        }
        root.insert(QStringLiteral("sections"), arr);
    }
    {
        QJsonArray arr;
        for (const Id id : nodeOrder) {
            const NodeGroup& n = nodes[id];
            QJsonObject o;
            o.insert(QStringLiteral("id"), id);
            o.insert(QStringLiteral("name"), n.name);
            QJsonArray st;
            for (const Id sid : n.stopIds)
                st.append(sid);
            o.insert(QStringLiteral("stopIds"), st);
            o.insert(QStringLiteral("x"), n.pos.x());
            o.insert(QStringLiteral("y"), n.pos.y());
            o.insert(QStringLiteral("moveStamp"), n.moveStamp);
            o.insert(QStringLiteral("label"), labelToJson(n.label));
            arr.append(o);
        }
        root.insert(QStringLiteral("nodes"), arr);
    }
    {
        QJsonArray arr;
        for (const Id id : lineOrder) {
            const Line& l = lines[id];
            QJsonObject o;
            o.insert(QStringLiteral("id"), id);
            o.insert(QStringLiteral("name"), l.name);
            o.insert(QStringLiteral("color"), l.color.name());
            QJsonArray sc;
            for (const Id sid : l.sections)
                sc.append(sid);
            o.insert(QStringLiteral("sections"), sc);
            o.insert(QStringLiteral("labelTextOnLine"), l.labelTextOnLine);
            o.insert(QStringLiteral("labelTerminiBadges"), l.labelTerminiBadges);
            o.insert(QStringLiteral("labelAlongBadges"), l.labelAlongBadges);
            arr.append(o);
        }
        root.insert(QStringLiteral("lines"), arr);
    }
    {
        const Settings& s = settings;
        QJsonObject o;
        o.insert(QStringLiteral("stopSpacing"), s.stopSpacing);
        o.insert(QStringLiteral("lineWidth"), s.lineWidth);
        o.insert(QStringLiteral("bundleSpacing"), s.bundleSpacing);
        o.insert(QStringLiteral("cornerRadius"), s.cornerRadius);
        o.insert(QStringLiteral("labelDistance"), s.labelDistance);
        o.insert(QStringLiteral("badgeEvery"), s.badgeEvery);
        o.insert(QStringLiteral("textWhite"), s.textColor == Qt::white);
        o.insert(QStringLiteral("stopFontFamily"), s.stopFontFamily);
        o.insert(QStringLiteral("stopFontSize"), s.stopFontSize);
        o.insert(QStringLiteral("lineFontFamily"), s.lineFontFamily);
        o.insert(QStringLiteral("lineFontSize"), s.lineFontSize);
        root.insert(QStringLiteral("settings"), o);
    }
    return root;
}

Project Project::fromJson(const QJsonObject& root)
{
    Project prj;
    prj.nextId = root.value(QStringLiteral("nextId")).toInt(1);
    prj.language = root.value(QStringLiteral("language")).toString(QStringLiteral("de"));

    for (const QJsonValue& v : root.value(QStringLiteral("stops")).toArray()) {
        const QJsonObject o = v.toObject();
        Stop s;
        s.id = o.value(QStringLiteral("id")).toInt();
        s.name = o.value(QStringLiteral("name")).toString();
        s.label = labelFromJson(o.value(QStringLiteral("label")).toObject());
        prj.stops.insert(s.id, s);
        prj.stopOrder.push_back(s.id);
    }
    for (const QJsonValue& v : root.value(QStringLiteral("boundaries")).toArray()) {
        const QJsonObject o = v.toObject();
        BoundaryPoint b;
        b.id = o.value(QStringLiteral("id")).toInt();
        b.name = o.value(QStringLiteral("name")).toString();
        b.pos = QPointF(o.value(QStringLiteral("x")).toDouble(),
                        o.value(QStringLiteral("y")).toDouble());
        b.moveStamp = o.value(QStringLiteral("moveStamp")).toDouble(0.0);
        b.label = labelFromJson(o.value(QStringLiteral("label")).toObject());
        prj.boundaries.insert(b.id, b);
        prj.boundaryOrder.push_back(b.id);
    }
    for (const QJsonValue& v : root.value(QStringLiteral("sections")).toArray()) {
        const QJsonObject o = v.toObject();
        Section s;
        s.id = o.value(QStringLiteral("id")).toInt();
        s.name = o.value(QStringLiteral("name")).toString();
        for (const QJsonValue& ev : o.value(QStringLiteral("elems")).toArray()) {
            const QJsonObject eo = ev.toObject();
            Elem e;
            e.kind = eo.value(QStringLiteral("kind")).toString() == QStringLiteral("stop")
                         ? ElemKind::Stop
                         : ElemKind::Boundary;
            e.id = eo.value(QStringLiteral("id")).toInt();
            s.elems.push_back(e);
        }
        for (const QJsonValue& vv : o.value(QStringLiteral("vias")).toArray()) {
            const QJsonObject vo = vv.toObject();
            ViaPoint p;
            p.pos = QPointF(vo.value(QStringLiteral("x")).toDouble(),
                            vo.value(QStringLiteral("y")).toDouble());
            p.afterElem = vo.value(QStringLiteral("afterElem")).toInt(0);
            s.vias.push_back(p);
        }
        const QJsonObject pol = o.value(QStringLiteral("intervalOnly45")).toObject();
        for (auto it = pol.constBegin(); it != pol.constEnd(); ++it)
            s.intervalOnly45.insert(it.key().toInt(), it.value().toBool());
        prj.sections.insert(s.id, s);
        prj.sectionOrder.push_back(s.id);
    }
    for (const QJsonValue& v : root.value(QStringLiteral("nodes")).toArray()) {
        const QJsonObject o = v.toObject();
        NodeGroup n;
        n.id = o.value(QStringLiteral("id")).toInt();
        n.name = o.value(QStringLiteral("name")).toString();
        for (const QJsonValue& sv : o.value(QStringLiteral("stopIds")).toArray())
            n.stopIds.push_back(sv.toInt());
        n.pos = QPointF(o.value(QStringLiteral("x")).toDouble(),
                        o.value(QStringLiteral("y")).toDouble());
        n.moveStamp = o.value(QStringLiteral("moveStamp")).toDouble(0.0);
        n.label = labelFromJson(o.value(QStringLiteral("label")).toObject());
        prj.nodes.insert(n.id, n);
        prj.nodeOrder.push_back(n.id);
    }
    for (const QJsonValue& v : root.value(QStringLiteral("lines")).toArray()) {
        const QJsonObject o = v.toObject();
        Line l;
        l.id = o.value(QStringLiteral("id")).toInt();
        l.name = o.value(QStringLiteral("name")).toString();
        l.color = QColor(o.value(QStringLiteral("color")).toString(QStringLiteral("#d40000")));
        for (const QJsonValue& sv : o.value(QStringLiteral("sections")).toArray())
            l.sections.push_back(sv.toInt());
        l.labelTextOnLine = o.value(QStringLiteral("labelTextOnLine")).toBool(false);
        l.labelTerminiBadges = o.value(QStringLiteral("labelTerminiBadges")).toBool(true);
        l.labelAlongBadges = o.value(QStringLiteral("labelAlongBadges")).toBool(false);
        prj.lines.insert(l.id, l);
        prj.lineOrder.push_back(l.id);
    }
    {
        const QJsonObject o = root.value(QStringLiteral("settings")).toObject();
        Settings s;
        s.stopSpacing = o.value(QStringLiteral("stopSpacing")).toDouble(70.0);
        s.lineWidth = o.value(QStringLiteral("lineWidth")).toDouble(5.0);
        s.bundleSpacing = o.value(QStringLiteral("bundleSpacing")).toDouble(7.0);
        s.cornerRadius = o.value(QStringLiteral("cornerRadius")).toDouble(14.0);
        s.labelDistance = o.value(QStringLiteral("labelDistance")).toDouble(8.0);
        s.badgeEvery = o.value(QStringLiteral("badgeEvery")).toDouble(260.0);
        s.textColor = o.value(QStringLiteral("textWhite")).toBool(false) ? Qt::white : Qt::black;
        s.stopFontFamily = o.value(QStringLiteral("stopFontFamily")).toString();
        s.stopFontSize = o.value(QStringLiteral("stopFontSize")).toDouble(9.0);
        s.lineFontFamily = o.value(QStringLiteral("lineFontFamily")).toString();
        s.lineFontSize = o.value(QStringLiteral("lineFontSize")).toDouble(9.0);
        prj.settings = s;
    }
    return prj;
}

// ---------- Beispielnetz ----------

void makeDemo(Project& prj)
{
    prj.clear();

    const Id bNord = prj.addBoundary(QStringLiteral("Richtung Nordstadt"));
    const Id bOst = prj.addBoundary(QStringLiteral("Richtung Osten"));
    const Id bSued = prj.addBoundary(QStringLiteral("Richtung Sueden"));
    prj.boundaries[bNord].pos = QPointF(300.0, 80.0);
    prj.boundaries[bOst].pos = QPointF(780.0, 430.0);
    prj.boundaries[bSued].pos = QPointF(300.0, 780.0);

    const Id sHbf = prj.addStop(QStringLiteral("Hauptbahnhof"));
    const Id sRathaus = prj.addStop(QStringLiteral("Rathaus"));
    const Id sMuseum = prj.addStop(QStringLiteral("Museum"));
    const Id sAltstadt = prj.addStop(QStringLiteral("Altstadt"));
    const Id sHafen = prj.addStop(QStringLiteral("Hafen"));
    const Id sKreuz = prj.addStop(QStringLiteral("Kreuz"));

    auto stop = [&](Id id) { return Elem{ ElemKind::Stop, id }; };
    auto bnd = [&](Id id) { return Elem{ ElemKind::Boundary, id }; };

    const Id secHaupt = prj.addSection(QStringLiteral("Hauptachse"));
    {
        Section& s = prj.sections[secHaupt];
        s.elems = { bnd(bNord), stop(sHbf), stop(sRathaus), stop(sMuseum), bnd(bSued) };
    }
    const Id secOst = prj.addSection(QStringLiteral("Ostast"));
    {
        Section& s = prj.sections[secOst];
        s.elems = { bnd(bNord), stop(sHbf), stop(sKreuz), bnd(bOst) };
    }
    const Id secAlt = prj.addSection(QStringLiteral("Suedast Altstadt"));
    {
        Section& s = prj.sections[secAlt];
        s.elems = { bnd(bSued), stop(sMuseum), stop(sAltstadt), bnd(bOst) };
    }
    const Id secHafen = prj.addSection(QStringLiteral("Suedast Hafen"));
    {
        Section& s = prj.sections[secHafen];
        s.elems = { bnd(bSued), stop(sMuseum), stop(sHafen), bnd(bOst) };
    }

    const Id l1 = prj.addLine(QStringLiteral("Linie 1"), QColor(QStringLiteral("#d40000")));
    prj.lines[l1].sections = { secHaupt };
    const Id l2 = prj.addLine(QStringLiteral("Linie 2"), QColor(QStringLiteral("#0064c8")));
    prj.lines[l2].sections = { secOst, secHaupt };
    const Id l3 = prj.addLine(QStringLiteral("Linie 3"), QColor(QStringLiteral("#009f00")));
    prj.lines[l3].sections = { secHaupt, secAlt };
    const Id l4 = prj.addLine(QStringLiteral("Linie 4"), QColor(QStringLiteral("#f0a800")));
    prj.lines[l4].sections = { secHafen };
}
