#include "l10n.h"

namespace l10n {

namespace {

// Schluessel, Deutsch, Englisch (UTF-8-oktal maskiert)
struct Entry
{
    const char* key;
    const char* de;
    const char* en;
};

const Entry kTable[] = {
    // Menues
    {"menu.file", "Datei", "&File"},
    {"menu.file.new", "Neu", "New"},
    {"menu.file.open", "\303\226ffnen...", "Open..."},
    {"menu.file.save", "Speichern", "Save"},
    {"menu.file.saveas", "Speichern unter...", "Save As..."},
    {"menu.file.export.png", "Als PNG exportieren...", "Export as PNG..."},
    {"menu.file.export.svg", "Als SVG exportieren...", "Export as SVG..."},
    {"menu.file.export.pdf", "Als PDF exportieren...", "Export as PDF..."},
    {"menu.file.quit", "Beenden", "Quit"},
    {"menu.edit", "Bearbeiten", "Edit"},
    {"menu.edit.undo", "R\303\274ckg\303\244ngig", "Undo"},
    {"menu.edit.demo", "Beispielnetz laden", "Load example network"},
    {"menu.language", "Sprache", "Language"},
    {"menu.language.de", "Deutsch", "German"},
    {"menu.language.en", "English", "English"},
    {"menu.help", "Hilfe", "Help"},
    {"menu.help.about", "\303\234ber Liniennetzplan-Editor", "About Liniennetzplan Editor"},

    // Ansicht
    {"view.zoomin", "Einzoomen", "Zoom In"},
    {"view.zoomout", "Auszoomen", "Zoom Out"},
    {"view.fit", "Alles einpassen", "Fit All"},

    // Tabs / Dock
    {"tab.stops", "1: Haltestellen", "1: Stops"},
    {"tab.boundaries", "2: Grenzpunkte", "2: Boundary Points"},
    {"tab.sections", "3: Streckenabschnitte", "3: Sections"},
    {"tab.nodes", "4: Knotenpunkte", "4: Interchange Nodes"},
    {"tab.lines", "5: Linien", "5: Lines"},
    {"tab.settings", "6: Einstellungen", "6: Settings"},
    {"dock.title", "Arbeitsschritte", "Workflow"},

    // Hinweistexte
    {"hint.stops",
     "Schritt 1: Haltestellen mit Namen anlegen. Ihre Position ergibt sich "
     "sp\303\244ter aus Grenzpunkten und Streckenabschnitten.",
     "Step 1: Create stops by name. Their positions are derived later from "
     "boundary points and sections."},
    {"hint.boundaries",
     "Schritt 2: Grenzpunkte anlegen. Streckenabschnitte werden zwischen "
     "zwei Grenzpunkten verankert; Linien verlassen dort die Karte.",
     "Step 2: Create boundary points. Sections are anchored between two "
     "boundary points; lines leave the map there."},
    {"hint.sections",
     "Schritt 3: Streckenabschnitte anlegen und die Haltestellen in der "
     "richtigen Reihenfolge hinzuf\303\274gen. Jeder Abschnitt erh\303\244lt "
     "einen Start- und einen Ziel-Grenzpunkt.",
     "Step 3: Create sections and add the stops in the correct order. Each "
     "section gets a start and an end boundary point."},
    {"hint.nodes",
     "Schritt 4: Knotenpunkte aus mindestens zwei Haltestellen bilden "
     "(Haltestellen markieren und Knotenpunkt erstellen).",
     "Step 4: Group at least two stops into an interchange node "
     "(select stops and create the node)."},
    {"hint.lines",
     "Schritt 5: Linien mit Bezeichnung, Farbe und Streckenabschnitten "
     "festlegen. Anschliessend Positionen auf der Karte per Drag&Drop "
     "anpassen.",
     "Step 5: Define lines with name, colour and sections. Afterwards "
     "adjust positions on the map via drag&drop."},

    // Aktionen
    {"act.add", "Hinzuf\303\274gen", "Add"},
    {"act.rename", "Umbenennen", "Rename"},
    {"act.delete", "L\303\266schen", "Delete"},
    {"act.addstop", "Haltestelle hinzuf\303\274gen", "Add Stop"},
    {"act.addboundary", "Grenzpunkt hinzuf\303\274gen", "Add Boundary Point"},
    {"act.addsection", "Streckenabschnitt hinzuf\303\274gen", "Add Section"},
    {"act.makenode", "Knotenpunkt erstellen", "Create Node"},
    {"act.delnode", "Knotenpunkt aufl\303\266sen", "Dissolve Node"},
    {"act.addline", "Linie hinzuf\303\274gen", "Add Line"},
    {"act.up", "Nach oben", "Move Up"},
    {"act.down", "Nach unten", "Move Down"},

    // Beschriftungen in der Seitenleiste
    {"label.name", "Name", "Name"},
    {"label.color", "Farbe", "Colour"},
    {"label.sections", "Streckenabschnitte", "Sections"},
    {"label.linestyle", "Darstellung der Linienbezeichnung", "Line Label Style"},
    {"label.textonline", "Text auf der Linie", "Text on the line"},
    {"label.termini", "Rechteck an den Endhaltestellen", "Badge at the termini"},
    {"label.along", "Rechtecke entlang der Linie", "Badges along the line"},
    {"label.linedetail", "Liniendetails", "Line Details"},

    // Einstellungen
    {"set.spacing", "Haltestellenabstand G", "Stop Spacing G"},
    {"set.linewidth", "Linienbreite", "Line Width"},
    {"set.bundlespacing", "B\303\274ndelabstand", "Bundle Spacing"},
    {"set.cornerradius", "Ausrundungsradius", "Corner Radius"},
    {"set.badgeevery", "Rechteck-Abstand (Abschnitte)", "Badge Spacing (sections)"},
    {"set.labeldist", "Beschriftungsabstand", "Label Distance"},
    {"set.textcolor", "Textfarbe der Linienbezeichnung", "Line Label Text Colour"},
    {"set.stopfont", "Haltestellenschrift", "Stop Font"},
    {"set.stopfontsize", "Schriftgr\303\266sse Haltestellen", "Stop Font Size"},
    {"set.linefont", "Linienschrift", "Line Font"},
    {"set.linefontsize", "Schriftgr\303\266sse Linien", "Line Font Size"},
    {"color.black", "Schwarz", "Black"},
    {"color.white", "Weiss", "White"},

    // Kontextmenue der Karte
    {"ctx.label", "Beschriftung festlegen...", "Fix Label..."},
    {"ctx.via.remove", "Zwischenpunkt entfernen", "Remove Via Point"},
    {"ctx.45", "Nur 45\302\260-Knicke", "Only 45\302\260 bends"},
    {"ctx.4590", "45\302\260- und 90\302\260-Knicke", "45\302\260 and 90\302\260 bends"},

    // Beschriftungsdialog
    {"dlg.stoptitle", "Beschriftung", "Label"},
    {"dlg.auto", "Automatisch", "Automatic"},
    {"dlg.fixed", "Festgelegt", "Fixed"},
    {"dlg.angle", "Winkel", "Angle"},
    {"dlg.dx", "Versatz x", "Offset x"},
    {"dlg.dy", "Versatz y", "Offset y"},

    // Meldungen
    {"msg.untitled", "Unbenannt", "Untitled"},
    {"msg.warnings", "Warnungen", "Warnings"},
    {"msg.nowarnings", "Es liegen keine Warnungen vor.", "There are no warnings."},
    {"msg.noundo", "Es gibt nichts R\303\274ckg\303\244ngiges.", "Nothing to undo."},
    {"msg.nostops",
     "Es sind noch keine Haltestellen vorhanden. Bitte zuerst im Tab "
     "\342\200\2361: Haltestellen\342\200\234 anlegen.",
     "There are no stops yet. Please create them in the \342\200\2361: Stops\342\200\234 tab first."},
    {"msg.noboundaries",
     "Es sind noch keine Grenzpunkte vorhanden. Bitte zuerst im Tab "
     "\342\200\2362: Grenzpunkte\342\200\234 anlegen.",
     "There are no boundary points yet. Please create them in the "
     "\342\200\2362: Boundary Points\342\200\234 tab first."},
    {"msg.pick2", "Bitte mindestens zwei Haltestellen ausw\303\244hlen.",
     "Please select at least two stops."},
    {"msg.pickstops", "Bitte mindestens eine Haltestelle ausw\303\244hlen.",
     "Please select at least one stop."},
    {"msg.demo", "Beispielnetz geladen.", "Example network loaded."},
    {"err.open", "Die Datei konnte nicht ge\303\266ffnet werden.",
     "The file could not be opened."},
    {"err.save", "Die Datei konnte nicht gespeichert werden.",
     "The file could not be saved."},
    {"err.export", "Der Export ist fehlgeschlagen.", "The export failed."},

    // Hilfe
    {"help.title", "Hilfe", "Help"},
    {"help.text",
     "Liniennetzplan-Editor\n\n"
     "Arbeitsablauf:\n"
     "1. Haltestellen anlegen (nur Name).\n"
     "2. Grenzpunkte anlegen (Linien verlassen dort die Karte).\n"
     "3. Streckenabschnitte anlegen: Haltestellen in der richtigen "
     "Reihenfolge ausw\303\244hlen, je Abschnitt zwei Grenzpunkte.\n"
     "4. Knotenpunkte aus mindestens zwei Haltestellen bilden.\n"
     "5. Linien festlegen: Bezeichnung, Farbe, Streckenabschnitte.\n"
     "6. Positionen auf der Karte per Drag&Drop anpassen.\n\n"
     "Bedienung der Karte:\n"
     "- Ziehen mit linker Maustaste: Element verschieben.\n"
     "- Doppelklick auf eine Linie: Zwischenpunkt hinzuf\303\274gen.\n"
     "- Rechtsklick auf Haltestelle oder Knotenpunkt: Beschriftung "
     "festlegen (Winkel und Versatz) oder automatisch ermitteln.\n"
     "- Rechtsklick auf einen Abschnitt zwischen zwei Haltestellen: "
     "nur 45\302\260-Knicke oder zus\303\244tzlich 90\302\260-Knicke erlauben.\n"
     "- Rechtsklick auf einen Zwischenpunkt: Zwischenpunkt entfernen.\n"
     "- Mausrad: Zoomen, mittlere Maustaste oder Leertaste: Schwenken.\n\n"
     "Die Reihenfolge geb\303\274ndelter Linien und ihre Entflechtung an "
     "Verzweigungen werden automatisch ermittelt. Export als PNG, SVG "
     "oder PDF \303\274ber das Men\303\274 Datei.",
     "Liniennetzplan Editor\n\n"
     "Workflow:\n"
     "1. Create stops (name only).\n"
     "2. Create boundary points (lines leave the map there).\n"
     "3. Create sections: pick the stops in the correct order and two "
     "boundary points per section.\n"
     "4. Group stops into interchange nodes (at least two stops).\n"
     "5. Define lines: name, colour, sections.\n"
     "6. Adjust positions on the map via drag&drop.\n\n"
     "Map controls:\n"
     "- Drag with the left mouse button: move an element.\n"
     "- Double-click on a line: add a via point.\n"
     "- Right-click on a stop or node: fix the label (angle and offset) "
     "or place it automatically.\n"
     "- Right-click on an interval between two stops: allow only 45\302\260 "
     "bends or additionally 90\302\260 bends.\n"
     "- Right-click on a via point: remove it.\n"
     "- Mouse wheel: zoom, middle button or space: pan.\n\n"
     "The order of bundled lines and their disentanglement at junctions "
     "is computed automatically. Export as PNG, SVG or PDF via the File "
     "menu."},

    // Warnungen
    {"warn.sectionshort",
     "Streckenabschnitt %1 ist zu kurz f\303\274r alle Haltestellen.",
     "Section %1 is too short for all of its stops."},
    {"warn.toofewpoints",
     "Streckenabschnitt %1 hat weniger als zwei Punkte.",
     "Section %1 has fewer than two points."},
    {"warn.noanchors",
     "Streckenabschnitt %1 hat keine verankerten Punkte (Grenzpunkte oder Knoten).",
     "Section %1 has no anchored points (boundaries or nodes)."},
    {"warn.stopnosection",
     "Haltestelle %1 geh\303\266rt zu keinem Streckenabschnitt.",
     "Stop %1 is not part of any section."},
    {"warn.nodetoofew",
     "Knotenpunkt %1 enth\303\244lt weniger als zwei Haltestellen.",
     "Node %1 contains fewer than two stops."},
    {"warn.linenosections",
     "Linie %1 hat keine Streckenabschnitte.",
     "Line %1 has no sections."},
    {"warn.linenotconnected",
     "Linie %1: Die Streckenabschnitte sind nicht verbunden.",
     "Line %1: the sections are not connected."},
};

const int kTableSize = int(sizeof(kTable) / sizeof(kTable[0]));

QString g_lang = QStringLiteral("de");

const char* pick(const Entry& e)
{
    return g_lang == QStringLiteral("en") ? e.en : e.de;
}

} // namespace

void setLanguage(const QString& code)
{
    if (code == QStringLiteral("en") || code == QStringLiteral("de"))
        g_lang = code;
}

QString language()
{
    return g_lang;
}

QString L(const QString& key)
{
    for (int i = 0; i < kTableSize; ++i) {
        if (key == QLatin1String(kTable[i].key))
            return QString::fromUtf8(pick(kTable[i]));
    }
    return key;
}

} // namespace l10n
