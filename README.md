# LiniennetzplanEditor

Ein Desktop-Editor für Liniennetzpläne des öffentlichen Verkehrs, geschrieben in
C++17 mit Qt6 (Widgets und Svg). Der Editor deckt den vollständigen Arbeitsablauf
ab und zeichnet das Liniennetz automatisch, einschliesslich der Reihenfolge
gebündelter Linien und ihrer Entflechtung an Verzweigungen.

## Bauen

Voraussetzungen: CMake 3.16+, ein C++17-Compiler, Qt 6.2 oder höher
(Qt6 Widgets und Qt6 Svg), Ninja (empfohlen).

Debian/Ubuntu:

```bash
sudo apt install build-essential cmake ninja-build qt6-base-dev qt6-svg-dev libgl1-mesa-dev
```

Bauen und starten:

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/LiniennetzplanEditor
```

Unter Windows mit Qt6 und Ninja entsprechend:

```bat
cmake -B build -G Ninja -DCMAKE_PREFIX_PATH=C:\Qt\6.x.x\mingw_64 -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Arbeitsablauf

1. Haltestellen festlegen (Name genügt).
2. Grenzpunkte festlegen; an Grenzpunkten verlassen Linien die Kartendarstellung.
3. Streckenabschnitte festlegen: Haltestellen in der richtigen Reihenfolge,
   je Abschnitt zwei Grenzpunkte als Verankerung.
4. Knotenpunkte (gruppierte Haltestellen) festlegen.
5. Linien festlegen: Bezeichnung, Farbe und ein oder mehrere Streckenabschnitte.
6. Positionen auf der Karte per Drag&Drop anpassen (Grenzpunkte, Knotenpunkte
   und Zwischenpunkte).

Anschliessend zeichnet das Programm das Liniennetz automatisch.

## Automatisches Zeichnen

- Nur gerade Linien in den Winkeln 0°, 45°, 90°, 135°, 180°, 225°, 270° und
  315° (oktilinear).
- Knicke liegen stets zwischen zwei Haltestellen und werden an der
  Streckenachse ausgerundet; innen liegende Linien sind enger, aussen liegende
  entsprechend weiter gerundet.
- Reihenfolge gebündelter Linien und Entflechtungen an Verzweigungen werden
  automatisch ermittelt.
- Haltestellenabstand G ist global fix; wird ein Abschnitt zu kurz, kann der
  gegenüberliegende Grenzpunkt auf der Karte weggezogen werden, wobei nur das
  jeweils letzte Intervall gedehnt wird.

## Bedienung auf der Karte

- Linke Maustaste: Elemente (Grenzpunkte, Knotenpunkte, Zwischenpunkte)
  verschieben.
- Doppelklick auf eine Linie: Zwischenpunkt für die Feinjustierung hinzufügen.
- Rechtsklick auf einen Zwischenpunkt: Zwischenpunkt entfernen.
- Rechtsklick auf einen Abschnitt zwischen zwei Haltestellen: Auswahl, ob dort
  nur 45°-Knicke oder auch 90°-Knicke erlaubt sind.
- Rechtsklick auf eine Haltestelle oder einen Knotenpunkt: Beschriftung
  festlegen (Winkel 0°, 45° oder 90° und Versatz) oder automatisch ermitteln.
- Mausrad: Zoomen; mittlere Maustaste oder Leertaste gedrückt: Schwenken.
- Strg+Z: Rückgängig.

## Darstellung der Linienbezeichnung

Pro Linie mehrfach auswählbar:

- Text auf der Linie,
- Rechteck mit abgerundeten Ecken an den Endhaltestellen,
- Rechtecke mit abgerundeten Ecken entlang der Linie (Abstand einstellbar).

Die Rechtecke haben keinen Rand und die Linienfarbe als Hintergrund. Die
Textfarbe der Linienbezeichnung ist global schwarz oder weiss einstellbar.

## Oberfläche und Export

- Zweisprachige Oberfläche (Deutsch/Englisch, Menü Sprache).
- Export als PNG, SVG oder PDF über das Menü Datei.
- Projektdateien im Format `.lnz.json` (lesbares JSON mit allen Daten,
  inklusive der festgelegten Beschriftungen und Zwischenpunkte).

## Projektstruktur

```
src/
  main.cpp        Einstiegspunkt
  l10n.*          Übersetzungen (Deutsch/Englisch)
  geom.*          Geometrie: Streckenzüge, Ausrundung, Kandidatenrouten
  model.*         Datenmodell, Demo-Netz, JSON-Serialisierung
  layout.*        Automatisches Layout: Routing, Bündelung, Entflechtung,
                  Beschriftungsplatzierung
  render.*        Zeichnen der Karte und Export nach PNG/SVG/PDF
  netview.*       Kartenansicht mit Interaktion (Drag&Drop, Kontextmenüs)
  mainwindow.*    Hauptfenster mit den sechs Arbeitsschritt-Tabs
```

## Lizenz

Private Nutzung. Copyright (c) 2026 Manuel Pankop.
