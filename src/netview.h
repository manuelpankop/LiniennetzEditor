#pragma once

#include <QWidget>

#include "layout.h"
#include "model.h"

class NetworkView : public QWidget
{
    Q_OBJECT

public:
    explicit NetworkView(Project* prj, QWidget* parent = nullptr);

    void relayout();
    void fitView();
    void zoomBy(double factor);

    const Layout& layoutResult() const { return lay_; }

signals:
    // Snapshot des Projektzustands VOR einer Aenderung (fuer Undo).
    void undoPush(const QString& snapshotJson);
    void layoutChanged();

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;
    void contextMenuEvent(QContextMenuEvent* ev) override;
    void mouseDoubleClickEvent(QMouseEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;
    void keyReleaseEvent(QKeyEvent* ev) override;

private:
    enum class Drag { None, Pan, Node, Boundary, Via };

    QPointF toModel(const QPointF& w) const;
    QPointF toWorld(const QPointF& m) const;

    // Treffer-Tests (Modellkoordinaten)
    bool hitVia(const QPointF& m, Id* secId, int* viaIdx) const;
    bool hitBoundary(const QPointF& m, Id* bndId) const;
    bool hitNode(const QPointF& m, Id* nodeId) const;
    bool hitLabelTarget(const QPointF& m, int* kind, Id* id) const;
    bool hitInterval(const QPointF& m, Id* secId, int* gapIdx) const;
    bool hitLine(const QPointF& m, Id* secId, double* arc) const;

    QPointF nodePos(const NodeGroup& n) const;
    void beginModelChange();
    void commitModelChange();

    Project* prj_;
    Layout lay_;
    double scale_ = 1.0;
    QPointF offset_ = QPointF(40.0, 40.0);

    Drag drag_ = Drag::None;
    Id dragId_ = -1;
    Id dragViaSec_ = -1;
    int dragViaIdx_ = -1;
    QPointF panStartWorld_;
    QPointF offsetStart_;
    bool moved_ = false;
    QString pendingSnap_;
    double stampCounter_ = 1.0;
    bool spaceHeld_ = false;
};
