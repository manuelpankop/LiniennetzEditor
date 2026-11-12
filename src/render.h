#pragma once

#include "layout.h"
#include "model.h"

#include <QString>

class QPainter;

// Zeichnet das komplette Liniennetz in den Painter (Weltkoordinaten).
// handles = true zusaetzlich Editor-Griffe (Via-Punkte).
void drawNetwork(QPainter& p, const Project& prj, const Layout& lay, bool handles);

bool exportPng(const QString& path, const Project& prj, const Layout& lay, double scale = 2.0);
bool exportSvg(const QString& path, const Project& prj, const Layout& lay);
bool exportPdf(const QString& path, const Project& prj, const Layout& lay);
