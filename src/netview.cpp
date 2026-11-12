#include "netview.h"

#include "l10n.h"
#include "render.h"

#include <QComboBox>
#include <QContextMenuEvent>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QLabel>
#include <QLineF>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>

namespace {

QString serializeProject(const Project& prj)
{
    return QString::fromUtf8(QJsonDocument(prj.toJson()).toJson(QJsonDocument::Compact));
}

QPointF roundTo1(const QPointF& p)
{
    return QPointF(std::round(p.x()), std::round(p.y()));
}

} // namespace

NetworkView::NetworkView(Project* prj, QWidget* parent)
    : QWidget(parent)
    , prj_(prj)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(400, 300);
    relayout();
}

void NetworkView::relayout()
{
    lay_ = computeLayout(*prj_);
    update();
}

void NetworkView::fitView()
{
    QRectF bb = lay_.bbox;
    if (bb.isNull())
        return;
    bb.adjust(-30, -30, 30, 30);
    const double w = std::max(1.0, double(width()));
    const double h = std::max(1.0, double(height()));
    const double s = std::min(w / bb.width(), h / bb.height());
    scale_ = std::clamp(s, 0.05, 20.0);
    offset_ = bb.center() - QPointF(w, h) / (2.0 * scale_);
    update();
}

void NetworkView::zoomBy(double factor)
{
    const QPointF c(width() / 2.0, height() / 2.0);
    const QPointF m = toModel(c);
    scale_ = std::clamp(scale_ * factor, 0.05, 20.0);
    offset_ = m - c / scale_;
    update();
}

QPointF NetworkView::toWorld(const QPointF& m) const
{
    return m / scale_ + offset_;
}

QPointF NetworkView::toModel(const QPointF& w) const
{
    return (w - offset_) * scale_;
}

QPointF NetworkView::nodePos(const NodeGroup& n) const
{
    if (n.moveStamp > 0.0)
        return n.pos;
    QPointF sum;
    int c = 0;
    for (const Id sid : n.stopIds) {
        if (lay_.stopPos.contains(sid)) {
            sum += lay_.stopPos[sid];
            ++c;
        }
    }
    return c > 0 ? sum / double(c) : n.pos;
}

void NetworkView::beginModelChange()
{
    pendingSnap_ = serializeProject(*prj_);
    moved_ = false;
}

void NetworkView::commitModelChange()
{
    if (!moved_)
        return;
    emit undoPush(pendingSnap_);
    emit layoutChanged();
    pendingSnap_.clear();
    moved_ = false;
}

// ---------- Treffer-Tests ----------

bool NetworkView::hitVia(const QPointF& m, Id* secId, int* viaIdx) const
{
    const double thr = 10.0 / scale_;
    for (const Id sid : prj_->sectionOrder) {
        const Section& sec = prj_->sections[sid];
        for (int i = 0; i < sec.vias.size(); ++i) {
            if (QLineF(m, sec.vias[i].pos).length() <= thr) {
                *secId = sid;
                *viaIdx = i;
                return true;
            }
        }
    }
    return false;
}

bool NetworkView::hitBoundary(const QPointF& m, Id* bndId) const
{
    const double thr = 10.0 / scale_;
    for (const Id bid : prj_->boundaryOrder) {
        if (QLineF(m, prj_->boundaries[bid].pos).length() <= thr) {
            *bndId = bid;
            return true;
        }
    }
    return false;
}

bool NetworkView::hitNode(const QPointF& m, Id* nodeId) const
{
    const double thr = 10.0 / scale_;
    for (const Id nid : prj_->nodeOrder) {
        if (prj_->nodes[nid].stopIds.size() < 2)
            continue;
        if (QLineF(m, nodePos(prj_->nodes[nid])).length() <= thr) {
            *nodeId = nid;
            return true;
        }
    }
    return false;
}

bool NetworkView::hitLabelTarget(const QPointF& m, int* kind, Id* id) const
{
    const double slack = 4.0 / scale_;
    for (int i = lay_.labels.size() - 1; i >= 0; --i) {
        const StopLabelLay& lbl = lay_.labels[i];
        if (lbl.aabb.adjusted(-slack, -slack, slack, slack).contains(m)) {
            *kind = int(lbl.kind);
            *id = lbl.id;
            return true;
        }
    }
    // Ausserdem direkter Klick auf ein Element.
    Id id2 = -1;
    if (hitNode(m, &id2)) {
        *kind = 1;
        *id = id2;
        return true;
    }
    if (hitBoundary(m, &id2)) {
        *kind = 2;
        *id = id2;
        return true;
    }
    for (const SectionLay& sl : lay_.sections) {
        for (const PlacedElem& pe : sl.elems) {
            if (pe.e.kind == ElemKind::Stop && prj_->nodeOfStop(pe.e.id) < 0
                && QLineF(m, pe.pos).length() <= 10.0 / scale_) {
                *kind = 0;
                *id = pe.e.id;
                return true;
            }
        }
    }
    return false;
}

bool NetworkView::hitInterval(const QPointF& m, Id* secId, int* gapIdx) const
{
    const double thr = 8.0 / scale_;
    const double elemThr = 12.0 / scale_;
    for (const SectionLay& sl : lay_.sections) {
        if (sl.elems.size() < 2)
            continue;
        for (int i = 0; i + 1 < sl.elems.size(); ++i) {
            const double a0 = sl.elems[i].arc;
            const double a1 = sl.elems[i + 1].arc;
            if (a1 - a0 < 1e-6)
                continue;
            if (QLineF(m, sl.elems[i].pos).length() <= elemThr
                || QLineF(m, sl.elems[i + 1].pos).length() <= elemThr)
                continue;
            // Abstand zur Achse, wenn zwischen den Bogenlaengen gelegen.
            QPointF near_;
            double arc = 0.0;
            const double d = distToPoly(m, sl.axis, &near_, &arc);
            if (d <= thr && arc >= a0 && arc <= a1) {
                *secId = sl.section;
                *gapIdx = std::min(sl.elems[i].index, sl.elems[i + 1].index);
                return true;
            }
        }
    }
    return false;
}

bool NetworkView::hitLine(const QPointF& m, Id* secId, double* arc) const
{
    const double thr = 7.0 / scale_;
    double bestD = 1e18;
    Id bestSec = -1;
    double bestArc = 0.0;
    for (const SectionLay& sl : lay_.sections) {
        QPointF near_;
        double a = 0.0;
        const double d = distToPoly(m, sl.axis, &near_, &a);
        if (d <= thr && d < bestD) {
            bestD = d;
            bestSec = sl.section;
            bestArc = a;
        }
    }
    if (bestSec < 0)
        return false;
    *secId = bestSec;
    *arc = bestArc;
    return true;
}

// ---------- Ereignisse ----------

void NetworkView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::white);
    p.scale(scale_, scale_);
    p.translate(-offset_);
    drawNetwork(p, *prj_, lay_, true);
}

void NetworkView::keyPressEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Space) {
        spaceHeld_ = true;
        setCursor(Qt::OpenHandCursor);
        ev->accept();
        return;
    }
    QWidget::keyPressEvent(ev);
}

void NetworkView::keyReleaseEvent(QKeyEvent* ev)
{
    if (ev->key() == Qt::Key_Space) {
        spaceHeld_ = false;
        unsetCursor();
        ev->accept();
        return;
    }
    QWidget::keyReleaseEvent(ev);
}

void NetworkView::mousePressEvent(QMouseEvent* ev)
{
    const QPointF m = toModel(ev->position());
    if (ev->button() == Qt::MiddleButton || (spaceHeld_ && ev->button() == Qt::LeftButton)) {
        drag_ = Drag::Pan;
        panStartWorld_ = ev->position();
        offsetStart_ = offset_;
        setCursor(Qt::ClosedHandCursor);
        ev->accept();
        return;
    }
    if (ev->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(ev);
        return;
    }

    Id secId = -1;
    int viaIdx = -1;
    if (hitVia(m, &secId, &viaIdx)) {
        beginModelChange();
        drag_ = Drag::Via;
        dragViaSec_ = secId;
        dragViaIdx_ = viaIdx;
        ev->accept();
        return;
    }
    Id id = -1;
    if (hitBoundary(m, &id)) {
        beginModelChange();
        drag_ = Drag::Boundary;
        dragId_ = id;
        prj_->boundaries[id].moveStamp = stampCounter_++;
        ev->accept();
        return;
    }
    if (hitNode(m, &id)) {
        beginModelChange();
        drag_ = Drag::Node;
        dragId_ = id;
        prj_->nodes[id].moveStamp = stampCounter_++;
        ev->accept();
        return;
    }
    QWidget::mousePressEvent(ev);
}

void NetworkView::mouseMoveEvent(QMouseEvent* ev)
{
    if (drag_ == Drag::Pan) {
        offset_ = offsetStart_ - (ev->position() - panStartWorld_) / scale_;
        update();
        ev->accept();
        return;
    }
    const QPointF m = toModel(ev->position());
    if (drag_ == Drag::Via) {
        prj_->sections[dragViaSec_].vias[dragViaIdx_].pos = roundTo1(m);
        moved_ = true;
        relayout();
        ev->accept();
        return;
    }
    if (drag_ == Drag::Boundary) {
        prj_->boundaries[dragId_].pos = roundTo1(m);
        moved_ = true;
        relayout();
        ev->accept();
        return;
    }
    if (drag_ == Drag::Node) {
        prj_->nodes[dragId_].pos = roundTo1(m);
        moved_ = true;
        relayout();
        ev->accept();
        return;
    }
    QWidget::mouseMoveEvent(ev);
}

void NetworkView::mouseReleaseEvent(QMouseEvent* ev)
{
    if (drag_ == Drag::Pan) {
        drag_ = Drag::None;
        if (!spaceHeld_)
            unsetCursor();
        ev->accept();
        return;
    }
    if (drag_ == Drag::Via || drag_ == Drag::Boundary || drag_ == Drag::Node) {
        commitModelChange();
        drag_ = Drag::None;
        dragId_ = -1;
        dragViaSec_ = -1;
        dragViaIdx_ = -1;
        ev->accept();
        return;
    }
    QWidget::mouseReleaseEvent(ev);
}

void NetworkView::wheelEvent(QWheelEvent* ev)
{
    const QPointF pos = ev->position();
    const QPointF m = toModel(pos);
    const double f = ev->angleDelta().y() > 0 ? 1.15 : (1.0 / 1.15);
    scale_ = std::clamp(scale_ * f, 0.05, 20.0);
    offset_ = m - pos / scale_;
    update();
    ev->accept();
}

void NetworkView::mouseDoubleClickEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(ev);
        return;
    }
    const QPointF m = toModel(ev->position());
    Id secId = -1;
    double arc = 0.0;
    if (!hitLine(m, &secId, &arc))
        return;

    const SectionLay* sl = nullptr;
    for (const SectionLay& s : lay_.sections)
        if (s.section == secId)
            sl = &s;
    if (!sl || sl->elems.isEmpty())
        return;

    // afterElem: Index des letzten Elements vor der Bogenlaenge.
    int afterElem = sl->elems.first().index;
    for (const PlacedElem& pe : sl->elems) {
        if (pe.arc <= arc + 1e-6)
            afterElem = pe.index;
    }

    beginModelChange();
    ViaPoint v;
    v.pos = roundTo1(m);
    v.afterElem = afterElem;
    prj_->sections[secId].vias.push_back(v);
    moved_ = true;
    relayout();
    commitModelChange();
    ev->accept();
}

void NetworkView::contextMenuEvent(QContextMenuEvent* ev)
{
    const QPointF m = toModel(ev->position());

    // 1) Via-Punkt
    Id secId = -1;
    int viaIdx = -1;
    if (hitVia(m, &secId, &viaIdx)) {
        QMenu menu(this);
        QAction* rm = menu.addAction(l10n::L("ctx.via.remove"));
        QAction* sel = menu.exec(ev->globalPos());
        if (sel == rm) {
            beginModelChange();
            prj_->sections[secId].vias.removeAt(viaIdx);
            moved_ = true;
            relayout();
            commitModelChange();
        }
        ev->accept();
        return;
    }

    // 2) Haltestelle / Knotenpunkt / Grenzpunkt: Beschriftung festlegen
    int kind = -1;
    Id id = -1;
    if (hitLabelTarget(m, &kind, &id)) {
        QMenu menu(this);
        QAction* act = menu.addAction(l10n::L("ctx.label"));
        if (menu.exec(ev->globalPos()) != act) {
            ev->accept();
            return;
        }
        const LabelStyle old = kind == 0 ? prj_->stops[id].label
                              : kind == 1 ? prj_->nodes[id].label
                                          : prj_->boundaries[id].label;
        QDialog dlg(this);
        dlg.setWindowTitle(l10n::L("dlg.stoptitle"));
        QRadioButton* autoR = new QRadioButton(l10n::L("dlg.auto"), &dlg);
        QRadioButton* fixR = new QRadioButton(l10n::L("dlg.fixed"), &dlg);
        QComboBox* angle = new QComboBox(&dlg);
        angle->addItems({ QStringLiteral("0\302\260"), QStringLiteral("45\302\260"),
                          QStringLiteral("90\302\260") });
        angle->setCurrentIndex(old.angle / 45 <= 2 ? old.angle / 45 : 0);
        QDoubleSpinBox* dx = new QDoubleSpinBox(&dlg);
        QDoubleSpinBox* dy = new QDoubleSpinBox(&dlg);
        for (QDoubleSpinBox* s : { dx, dy }) {
            s->setRange(-60.0, 60.0);
            s->setDecimals(1);
        }
        dx->setValue(old.offset.x());
        dy->setValue(old.offset.y());
        autoR->setChecked(!old.fixed);
        fixR->setChecked(old.fixed);
        auto btns = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
        connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
        connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
        auto form = new QFormLayout;
        form->addRow(QString(), autoR);
        form->addRow(QString(), fixR);
        form->addRow(l10n::L("dlg.angle"), angle);
        form->addRow(l10n::L("dlg.dx"), dx);
        form->addRow(l10n::L("dlg.dy"), dy);
        auto lay = new QVBoxLayout(&dlg);
        lay->addLayout(form);
        lay->addWidget(btns);
        if (dlg.exec() != QDialog::Accepted)
            return;
        LabelStyle ls;
        ls.fixed = fixR->isChecked();
        ls.angle = fixR->isChecked() ? angle->currentIndex() * 45 : 0;
        ls.offset = QPointF(dx->value(), dy->value());
        beginModelChange();
        if (kind == 0)
            prj_->stops[id].label = ls;
        else if (kind == 1)
            prj_->nodes[id].label = ls;
        else
            prj_->boundaries[id].label = ls;
        moved_ = true;
        relayout();
        commitModelChange();
        ev->accept();
        return;
    }

    // 3) Abschnitt zwischen zwei Haltestellen: Knickregel
    int gapIdx = -1;
    if (hitInterval(m, &secId, &gapIdx)) {
        QMenu menu(this);
        QAction* a45 = menu.addAction(l10n::L("ctx.45"));
        QAction* a90 = menu.addAction(l10n::L("ctx.4590"));
        QAction* sel = menu.exec(ev->globalPos());
        if (sel == a45 || sel == a90) {
            beginModelChange();
            prj_->sections[secId].intervalOnly45.insert(gapIdx, sel == a45);
            moved_ = true;
            relayout();
            commitModelChange();
        }
        ev->accept();
        return;
    }

    QWidget::contextMenuEvent(ev);
}
