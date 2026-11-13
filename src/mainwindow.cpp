#include "mainwindow.h"

#include "l10n.h"
#include "netview.h"
#include "render.h"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDockWidget>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QVBoxLayout>

using l10n::L;

namespace {

QString serializeProject(const Project& prj)
{
    return QString::fromUtf8(QJsonDocument(prj.toJson()).toJson(QJsonDocument::Compact));
}

QJsonObject deserializeProject(const QString& json)
{
    return QJsonDocument::fromJson(json.toUtf8()).object();
}

QJsonObject loadJsonFile(const QString& path, bool* ok)
{
    *ok = false;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return QJsonObject();
    const QByteArray data = f.readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
        return QJsonObject();
    *ok = true;
    return doc.object();
}

QColor defaultLineColor(int n)
{
    static const char* kPalette[] = {
        "#d40000", "#0064c8", "#009f00", "#f0a800", "#8000a0",
        "#008f8f", "#c06000", "#505050", "#d040a0", "#607000",
    };
    return QColor(QLatin1String(kPalette[n % 10]));
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    buildUi();
    afterProjectReplaced();
}

void MainWindow::showEvent(QShowEvent* ev)
{
    QMainWindow::showEvent(ev);
    if (firstShow_) {
        view_->relayout();
        view_->fitView();
        firstShow_ = false;
    }
}

// ---------- Slots ----------

void MainWindow::onUndoPush(const QString& snapshot)
{
    undoStack_.push_back(snapshot);
    if (undoStack_.size() > 100)
        undoStack_.removeFirst();
}

void MainWindow::onLayoutChanged()
{
    view_->relayout();
    lastWarnings_ = view_->layoutResult().warnings;
    warnBtn_->setText(QStringLiteral("%1 (%2)").arg(L("msg.warnings")).arg(lastWarnings_.size()));
}

void MainWindow::showWarnings()
{
    if (lastWarnings_.isEmpty())
        QMessageBox::information(this, L("msg.warnings"), L("msg.nowarnings"));
    else
        QMessageBox::information(this, L("msg.warnings"),
                                 lastWarnings_.join(QStringLiteral("\n")));
}

// ---------- Texte / Sprachumschaltung ----------

void MainWindow::registerText(QWidget* w, const QString& key)
{
    textTargets_.push_back({ w, key });
}

void MainWindow::registerAction(QAction* a, const QString& key)
{
    actionTargets_.push_back({ a, key });
}

void MainWindow::registerMenu(QMenu* m, const QString& key)
{
    menuTargets_.push_back({ m, key });
}

void MainWindow::updateWindowTitle()
{
    const QString name = currentFile_.isEmpty() ? L("msg.untitled")
                                                : QFileInfo(currentFile_).fileName();
    setWindowTitle(QStringLiteral("Liniennetzplan-Editor - ") + name);
}

void MainWindow::updateTexts()
{
    for (const auto& t : textTargets_) {
        if (QLabel* lbl = qobject_cast<QLabel*>(t.first))
            lbl->setText(L(t.second));
        else if (QGroupBox* gb = qobject_cast<QGroupBox*>(t.first))
            gb->setTitle(L(t.second));
        else if (QPushButton* btn = qobject_cast<QPushButton*>(t.first))
            btn->setText(L(t.second));
        else if (QCheckBox* chk = qobject_cast<QCheckBox*>(t.first))
            chk->setText(L(t.second));
        else if (QDockWidget* dw = qobject_cast<QDockWidget*>(t.first))
            dw->setWindowTitle(L(t.second));
    }
    for (const auto& a : actionTargets_)
        a.first->setText(L(a.second));
    for (const auto& m : menuTargets_)
        m.first->setTitle(L(m.second));
    for (int i = 0; i < tabKeys_.size() && i < tabs_->count(); ++i)
        tabs_->setTabText(i, L(tabKeys_[i]));
    langDe_->setChecked(l10n::language() == QStringLiteral("de"));
    langEn_->setChecked(l10n::language() == QStringLiteral("en"));
    {
        const int ci = setTextCol_->currentIndex();
        setTextCol_->blockSignals(true);
        setTextCol_->clear();
        setTextCol_->addItem(L("color.black"));
        setTextCol_->addItem(L("color.white"));
        setTextCol_->setCurrentIndex(ci < 0 ? 0 : ci);
        setTextCol_->blockSignals(false);
    }
    onLayoutChanged();
    updateWindowTitle();
}

// ---------- UI-Aufbau ----------

void MainWindow::buildUi()
{
    resize(1280, 820);

    // Kartenansicht
    view_ = new NetworkView(&prj_, this);
    setCentralWidget(view_);
    connect(view_, &NetworkView::undoPush, this, &MainWindow::onUndoPush);
    connect(view_, &NetworkView::layoutChanged, this, &MainWindow::onLayoutChanged);

    // Dock mit Arbeitsschritten
    tabs_ = new QTabWidget(this);
    tabKeys_ = { QStringLiteral("tab.stops"),      QStringLiteral("tab.boundaries"),
                 QStringLiteral("tab.sections"),   QStringLiteral("tab.nodes"),
                 QStringLiteral("tab.lines"),     QStringLiteral("tab.settings") };
    tabs_->addTab(buildStopsTab(), QString());
    tabs_->addTab(buildBoundariesTab(), QString());
    tabs_->addTab(buildSectionsTab(), QString());
    tabs_->addTab(buildNodesTab(), QString());
    tabs_->addTab(buildLinesTab(), QString());
    tabs_->addTab(buildSettingsTab(), QString());
    QDockWidget* dock = new QDockWidget(this);
    dock->setWidget(tabs_);
    dock->setFeatures(QDockWidget::DockWidgetMovable);
    registerText(dock, QStringLiteral("dock.title"));
    addDockWidget(Qt::LeftDockWidgetArea, dock);

    // Menues
    QMenu* fileMenu = menuBar()->addMenu(QString());
    registerMenu(fileMenu, QStringLiteral("menu.file"));
    QAction* aNew = new QAction(this);
    aNew->setShortcut(QKeySequence::New);
    registerAction(aNew, QStringLiteral("menu.file.new"));
    connect(aNew, &QAction::triggered, this, [this]() { fileNew(); });
    QAction* aOpen = new QAction(this);
    aOpen->setShortcut(QKeySequence::Open);
    registerAction(aOpen, QStringLiteral("menu.file.open"));
    connect(aOpen, &QAction::triggered, this, [this]() { fileOpen(); });
    QAction* aSave = new QAction(this);
    aSave->setShortcut(QKeySequence::Save);
    registerAction(aSave, QStringLiteral("menu.file.save"));
    connect(aSave, &QAction::triggered, this, [this]() { fileSave(false); });
    QAction* aSaveAs = new QAction(this);
    registerAction(aSaveAs, QStringLiteral("menu.file.saveas"));
    connect(aSaveAs, &QAction::triggered, this, [this]() { fileSave(true); });
    QAction* aPng = new QAction(this);
    registerAction(aPng, QStringLiteral("menu.file.export.png"));
    connect(aPng, &QAction::triggered, this, [this]() { exportAs(0); });
    QAction* aSvg = new QAction(this);
    registerAction(aSvg, QStringLiteral("menu.file.export.svg"));
    connect(aSvg, &QAction::triggered, this, [this]() { exportAs(1); });
    QAction* aPdf = new QAction(this);
    registerAction(aPdf, QStringLiteral("menu.file.export.pdf"));
    connect(aPdf, &QAction::triggered, this, [this]() { exportAs(2); });
    QAction* aQuit = new QAction(this);
    aQuit->setShortcut(QKeySequence::Quit);
    registerAction(aQuit, QStringLiteral("menu.file.quit"));
    connect(aQuit, &QAction::triggered, this, &QApplication::quit);
    for (QAction* a : { aNew, aOpen, aSave, aSaveAs })
        fileMenu->addAction(a);
    fileMenu->addSeparator();
    for (QAction* a : { aPng, aSvg, aPdf })
        fileMenu->addAction(a);
    fileMenu->addSeparator();
    fileMenu->addAction(aQuit);

    QMenu* editMenu = menuBar()->addMenu(QString());
    registerMenu(editMenu, QStringLiteral("menu.edit"));
    QAction* aUndo = new QAction(this);
    aUndo->setShortcut(QKeySequence::Undo);
    registerAction(aUndo, QStringLiteral("menu.edit.undo"));
    connect(aUndo, &QAction::triggered, this, [this]() { editUndo(); });
    QAction* aDemo = new QAction(this);
    registerAction(aDemo, QStringLiteral("menu.edit.demo"));
    connect(aDemo, &QAction::triggered, this, [this]() { loadDemo(); });
    editMenu->addAction(aUndo);
    editMenu->addSeparator();
    editMenu->addAction(aDemo);

    QMenu* langMenu = menuBar()->addMenu(QString());
    registerMenu(langMenu, QStringLiteral("menu.language"));
    QActionGroup* langGroup = new QActionGroup(this);
    langDe_ = new QAction(this);
    langDe_->setCheckable(true);
    langDe_->setActionGroup(langGroup);
    registerAction(langDe_, QStringLiteral("menu.language.de"));
    connect(langDe_, &QAction::triggered, this, [this]() { setLang(QStringLiteral("de")); });
    langEn_ = new QAction(this);
    langEn_->setCheckable(true);
    langEn_->setActionGroup(langGroup);
    registerAction(langEn_, QStringLiteral("menu.language.en"));
    connect(langEn_, &QAction::triggered, this, [this]() { setLang(QStringLiteral("en")); });
    langMenu->addAction(langDe_);
    langMenu->addAction(langEn_);

    QMenu* helpMenu = menuBar()->addMenu(QString());
    registerMenu(helpMenu, QStringLiteral("menu.help"));
    QAction* aAbout = new QAction(this);
    registerAction(aAbout, QStringLiteral("menu.help.about"));
    connect(aAbout, &QAction::triggered, this, [this]() { showHelp(); });
    helpMenu->addAction(aAbout);

    // Werkzeugleiste (Ansicht)
    QToolBar* tb = addToolBar(QStringLiteral("mainToolBar"));
    QAction* zIn = new QAction(this);
    registerAction(zIn, QStringLiteral("view.zoomin"));
    connect(zIn, &QAction::triggered, this, [this]() { view_->zoomBy(1.25); });
    QAction* zOut = new QAction(this);
    registerAction(zOut, QStringLiteral("view.zoomout"));
    connect(zOut, &QAction::triggered, this, [this]() { view_->zoomBy(0.8); });
    QAction* zFit = new QAction(this);
    zFit->setShortcut(QKeySequence(QStringLiteral("Ctrl+F")));
    registerAction(zFit, QStringLiteral("view.fit"));
    connect(zFit, &QAction::triggered, this, [this]() { view_->fitView(); });
    tb->addAction(zIn);
    tb->addAction(zOut);
    tb->addAction(zFit);

    // Warnungen in der Statusleiste
    warnBtn_ = new QPushButton(this);
    warnBtn_->setFlat(true);
    connect(warnBtn_, &QPushButton::clicked, this, &MainWindow::showWarnings);
    statusBar()->addPermanentWidget(warnBtn_);
}

QWidget* MainWindow::buildStopsTab()
{
    auto w = new QWidget(this);
    auto lay = new QVBoxLayout(w);
    QLabel* hint = new QLabel(w);
    hint->setWordWrap(true);
    registerText(hint, QStringLiteral("hint.stops"));
    lay->addWidget(hint);
    stopList_ = new QListWidget(w);
    lay->addWidget(stopList_, 1);
    auto row = new QHBoxLayout;
    QPushButton* add = new QPushButton(w);
    registerText(add, QStringLiteral("act.add"));
    connect(add, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, L("tab.stops"), L("label.name"),
                                                   QLineEdit::Normal, QString(), &ok);
        if (!ok)
            return;
        pushUndo();
        prj_.addStop(name.isEmpty() ? QStringLiteral("H%1").arg(prj_.nextId) : name);
        rebuildStops();
        rebuildNodeCandidates();
        onLayoutChanged();
    });
    QPushButton* ren = new QPushButton(w);
    registerText(ren, QStringLiteral("act.rename"));
    connect(ren, &QPushButton::clicked, this, [this]() {
        const int row = stopList_->currentRow();
        if (row < 0 || row >= prj_.stopOrder.size())
            return;
        const Id id = prj_.stopOrder[row];
        bool ok = false;
        const QString name = QInputDialog::getText(this, L("act.rename"), L("label.name"),
                                                   QLineEdit::Normal, prj_.stops[id].name, &ok);
        if (!ok || name.isEmpty())
            return;
        pushUndo();
        prj_.stops[id].name = name;
        rebuildStops();
        rebuildNodeCandidates();
        onLayoutChanged();
    });
    QPushButton* del = new QPushButton(w);
    registerText(del, QStringLiteral("act.delete"));
    connect(del, &QPushButton::clicked, this, [this]() {
        const int row = stopList_->currentRow();
        if (row < 0 || row >= prj_.stopOrder.size())
            return;
        pushUndo();
        prj_.removeStop(prj_.stopOrder[row]);
        rebuildAll();
        onLayoutChanged();
    });
    row->addWidget(add);
    row->addWidget(ren);
    row->addWidget(del);
    lay->addLayout(row);
    return w;
}

QWidget* MainWindow::buildBoundariesTab()
{
    auto w = new QWidget(this);
    auto lay = new QVBoxLayout(w);
    QLabel* hint = new QLabel(w);
    hint->setWordWrap(true);
    registerText(hint, QStringLiteral("hint.boundaries"));
    lay->addWidget(hint);
    boundaryList_ = new QListWidget(w);
    lay->addWidget(boundaryList_, 1);
    auto row = new QHBoxLayout;
    QPushButton* add = new QPushButton(w);
    registerText(add, QStringLiteral("act.add"));
    connect(add, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, L("tab.boundaries"), L("label.name"),
                                                   QLineEdit::Normal, QString(), &ok);
        if (!ok)
            return;
        pushUndo();
        prj_.addBoundary(name.isEmpty() ? QStringLiteral("G%1").arg(prj_.nextId) : name);
        rebuildBoundaries();
        onLayoutChanged();
    });
    QPushButton* ren = new QPushButton(w);
    registerText(ren, QStringLiteral("act.rename"));
    connect(ren, &QPushButton::clicked, this, [this]() {
        const int row = boundaryList_->currentRow();
        if (row < 0 || row >= prj_.boundaryOrder.size())
            return;
        const Id id = prj_.boundaryOrder[row];
        bool ok = false;
        const QString name = QInputDialog::getText(this, L("act.rename"), L("label.name"),
                                                   QLineEdit::Normal, prj_.boundaries[id].name, &ok);
        if (!ok || name.isEmpty())
            return;
        pushUndo();
        prj_.boundaries[id].name = name;
        rebuildBoundaries();
        onLayoutChanged();
    });
    QPushButton* del = new QPushButton(w);
    registerText(del, QStringLiteral("act.delete"));
    connect(del, &QPushButton::clicked, this, [this]() {
        const int row = boundaryList_->currentRow();
        if (row < 0 || row >= prj_.boundaryOrder.size())
            return;
        pushUndo();
        prj_.removeBoundary(prj_.boundaryOrder[row]);
        rebuildAll();
        onLayoutChanged();
    });
    row->addWidget(add);
    row->addWidget(ren);
    row->addWidget(del);
    lay->addLayout(row);
    return w;
}

QWidget* MainWindow::buildSectionsTab()
{
    auto w = new QWidget(this);
    auto lay = new QVBoxLayout(w);
    QLabel* hint = new QLabel(w);
    hint->setWordWrap(true);
    registerText(hint, QStringLiteral("hint.sections"));
    lay->addWidget(hint);

    sectionList_ = new QListWidget(w);
    connect(sectionList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (updating_)
            return;
        curSection_ = (row >= 0 && row < prj_.sectionOrder.size()) ? prj_.sectionOrder[row] : Id(-1);
        rebuildSectionElems();
    });
    lay->addWidget(sectionList_, 1);

    auto row1 = new QHBoxLayout;
    QPushButton* addsec = new QPushButton(w);
    registerText(addsec, QStringLiteral("act.addsection"));
    connect(addsec, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, L("act.addsection"), L("label.name"),
                                                   QLineEdit::Normal, QString(), &ok);
        if (!ok)
            return;
        pushUndo();
        curSection_ = prj_.addSection(name.isEmpty()
                                          ? QStringLiteral("S%1").arg(prj_.nextId)
                                          : name);
        rebuildSections();
        onLayoutChanged();
    });
    QPushButton* rens = new QPushButton(w);
    registerText(rens, QStringLiteral("act.rename"));
    connect(rens, &QPushButton::clicked, this, [this]() {
        const int row = sectionList_->currentRow();
        if (row < 0 || row >= prj_.sectionOrder.size())
            return;
        const Id id = prj_.sectionOrder[row];
        bool ok = false;
        const QString name = QInputDialog::getText(this, L("act.rename"), L("label.name"),
                                                   QLineEdit::Normal, prj_.sections[id].name, &ok);
        if (!ok || name.isEmpty())
            return;
        pushUndo();
        prj_.sections[id].name = name;
        rebuildSections();
        onLayoutChanged();
    });
    QPushButton* dels = new QPushButton(w);
    registerText(dels, QStringLiteral("act.delete"));
    connect(dels, &QPushButton::clicked, this, [this]() {
        const int row = sectionList_->currentRow();
        if (row < 0 || row >= prj_.sectionOrder.size())
            return;
        pushUndo();
        prj_.removeSection(prj_.sectionOrder[row]);
        if (curLine_ >= 0)
            rebuildLineDetail();
        rebuildAll();
        onLayoutChanged();
    });
    row1->addWidget(addsec);
    row1->addWidget(rens);
    row1->addWidget(dels);
    lay->addLayout(row1);

    elemList_ = new QListWidget(w);
    lay->addWidget(elemList_, 1);

    auto row2 = new QHBoxLayout;
    QPushButton* addstop = new QPushButton(w);
    registerText(addstop, QStringLiteral("act.addstop"));
    connect(addstop, &QPushButton::clicked, this, [this]() {
        if (curSection_ < 0 || !prj_.sections.contains(curSection_))
            return;
        if (prj_.stopOrder.isEmpty()) {
            QMessageBox::information(this, L("act.addstop"), L("msg.nostops"));
            return;
        }
        QStringList names;
        for (const Id id : prj_.stopOrder)
            names << prj_.stops[id].name;
        bool ok = false;
        const QString name = QInputDialog::getItem(this, L("act.addstop"), L("label.name"),
                                                   names, 0, false, &ok);
        if (!ok)
            return;
        const int idx = names.indexOf(name);
        if (idx < 0)
            return;
        pushUndo();
        prj_.sections[curSection_].elems.push_back({ ElemKind::Stop, prj_.stopOrder[idx] });
        rebuildSectionElems();
        onLayoutChanged();
    });
    QPushButton* addbnd = new QPushButton(w);
    registerText(addbnd, QStringLiteral("act.addboundary"));
    connect(addbnd, &QPushButton::clicked, this, [this]() {
        if (curSection_ < 0 || !prj_.sections.contains(curSection_))
            return;
        if (prj_.boundaryOrder.isEmpty()) {
            QMessageBox::information(this, L("act.addboundary"), L("msg.noboundaries"));
            return;
        }
        QStringList names;
        for (const Id id : prj_.boundaryOrder)
            names << prj_.boundaries[id].name;
        bool ok = false;
        const QString name = QInputDialog::getItem(this, L("act.addboundary"), L("label.name"),
                                                   names, 0, false, &ok);
        if (!ok)
            return;
        const int idx = names.indexOf(name);
        if (idx < 0)
            return;
        pushUndo();
        prj_.sections[curSection_].elems.push_back({ ElemKind::Boundary, prj_.boundaryOrder[idx] });
        rebuildSectionElems();
        onLayoutChanged();
    });
    QPushButton* delel = new QPushButton(w);
    registerText(delel, QStringLiteral("act.delete"));
    connect(delel, &QPushButton::clicked, this, [this]() {
        if (curSection_ < 0 || !prj_.sections.contains(curSection_))
            return;
        const int row = elemList_->currentRow();
        if (row < 0 || row >= prj_.sections[curSection_].elems.size())
            return;
        pushUndo();
        prj_.sections[curSection_].elems.removeAt(row);
        rebuildSectionElems();
        onLayoutChanged();
    });
    row2->addWidget(addstop);
    row2->addWidget(addbnd);
    row2->addWidget(delel);
    lay->addLayout(row2);

    auto row3 = new QHBoxLayout;
    QPushButton* up = new QPushButton(w);
    registerText(up, QStringLiteral("act.up"));
    connect(up, &QPushButton::clicked, this, [this]() {
        if (curSection_ < 0 || !prj_.sections.contains(curSection_))
            return;
        const int row = elemList_->currentRow();
        if (row <= 0 || row >= prj_.sections[curSection_].elems.size())
            return;
        pushUndo();
        prj_.sections[curSection_].elems.swapItemsAt(row - 1, row);
        rebuildSectionElems();
        elemList_->setCurrentRow(row - 1);
        onLayoutChanged();
    });
    QPushButton* down = new QPushButton(w);
    registerText(down, QStringLiteral("act.down"));
    connect(down, &QPushButton::clicked, this, [this]() {
        if (curSection_ < 0 || !prj_.sections.contains(curSection_))
            return;
        const int row = elemList_->currentRow();
        if (row < 0 || row + 1 >= prj_.sections[curSection_].elems.size())
            return;
        pushUndo();
        prj_.sections[curSection_].elems.swapItemsAt(row, row + 1);
        rebuildSectionElems();
        elemList_->setCurrentRow(row + 1);
        onLayoutChanged();
    });
    row3->addWidget(up);
    row3->addWidget(down);
    lay->addLayout(row3);
    return w;
}

QWidget* MainWindow::buildNodesTab()
{
    auto w = new QWidget(this);
    auto lay = new QVBoxLayout(w);
    QLabel* hint = new QLabel(w);
    hint->setWordWrap(true);
    registerText(hint, QStringLiteral("hint.nodes"));
    lay->addWidget(hint);

    nodeStopList_ = new QListWidget(w);
    nodeStopList_->setSelectionMode(QAbstractItemView::MultiSelection);
    lay->addWidget(nodeStopList_, 1);

    auto row1 = new QHBoxLayout;
    QPushButton* make = new QPushButton(w);
    registerText(make, QStringLiteral("act.makenode"));
    connect(make, &QPushButton::clicked, this, [this]() {
        QVector<Id> ids;
        for (QListWidgetItem* it : nodeStopList_->selectedItems())
            ids.push_back(it->data(Qt::UserRole).toInt());
        if (ids.isEmpty()) {
            QMessageBox::information(this, L("act.makenode"), L("msg.pickstops"));
            return;
        }
        
        QString name;
        if (ids.size() == 1) {
            // Bei einer Haltestelle: Name der Haltestelle uebernehmen
            name = prj_.stops[ids.first()].name;
        } else {
            // Bei mehreren Haltestellen: Frei waehlbaren Namen abfragen
            bool ok = false;
            name = QInputDialog::getText(this, L("act.makenode"), L("label.name"),
                                               QLineEdit::Normal,
                                               QStringLiteral("Knoten %1").arg(prj_.nodes.size() + 1),
                                               &ok);
            if (!ok)
                return;
        }
        pushUndo();
        prj_.addNode(name, ids);
        rebuildNodes();
        rebuildNodeCandidates();
        onLayoutChanged();
    });
    QPushButton* dissolve = new QPushButton(w);
    registerText(dissolve, QStringLiteral("act.delnode"));
    connect(dissolve, &QPushButton::clicked, this, [this]() {
        const int row = nodeList_->currentRow();
        if (row < 0 || row >= prj_.nodeOrder.size())
            return;
        pushUndo();
        prj_.removeNode(prj_.nodeOrder[row]);
        rebuildNodes();
        rebuildNodeCandidates();
        onLayoutChanged();
    });
    QPushButton* ren = new QPushButton(w);
    registerText(ren, QStringLiteral("act.rename"));
    connect(ren, &QPushButton::clicked, this, [this]() {
        const int row = nodeList_->currentRow();
        if (row < 0 || row >= prj_.nodeOrder.size())
            return;
        const Id id = prj_.nodeOrder[row];
        bool ok = false;
        const QString name = QInputDialog::getText(this, L("act.rename"), L("label.name"),
                                                   QLineEdit::Normal, prj_.nodes[id].name, &ok);
        if (!ok || name.isEmpty())
            return;
        pushUndo();
        prj_.nodes[id].name = name;
        rebuildNodes();
        onLayoutChanged();
    });
    row1->addWidget(make);
    row1->addWidget(dissolve);
    row1->addWidget(ren);
    lay->addLayout(row1);

    nodeList_ = new QListWidget(w);
    lay->addWidget(nodeList_, 1);
    
    // Initial die Haltestellen-Liste füllen
    rebuildNodeCandidates();
    return w;
}

QWidget* MainWindow::buildLinesTab()
{
    auto w = new QWidget(this);
    auto lay = new QVBoxLayout(w);
    QLabel* hint = new QLabel(w);
    hint->setWordWrap(true);
    registerText(hint, QStringLiteral("hint.lines"));
    lay->addWidget(hint);

    lineList_ = new QListWidget(w);
    connect(lineList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (updating_)
            return;
        curLine_ = (row >= 0 && row < prj_.lineOrder.size()) ? prj_.lineOrder[row] : Id(-1);
        rebuildLineDetail();
    });
    lay->addWidget(lineList_, 1);

    auto row1 = new QHBoxLayout;
    QPushButton* addline = new QPushButton(w);
    registerText(addline, QStringLiteral("act.addline"));
    connect(addline, &QPushButton::clicked, this, [this]() {
        bool ok = false;
        const QString name = QInputDialog::getText(this, L("act.addline"), L("label.name"),
                                                   QLineEdit::Normal,
                                                   QStringLiteral("Linie %1").arg(prj_.lines.size() + 1),
                                                   &ok);
        if (!ok)
            return;
        pushUndo();
        curLine_ = prj_.addLine(name.isEmpty() ? QStringLiteral("Linie %1").arg(prj_.nextId)
                                               : name,
                                defaultLineColor(prj_.lines.size()));
        rebuildLines();
        onLayoutChanged();
    });
    QPushButton* ren = new QPushButton(w);
    registerText(ren, QStringLiteral("act.rename"));
    connect(ren, &QPushButton::clicked, this, [this]() {
        const int row = lineList_->currentRow();
        if (row < 0 || row >= prj_.lineOrder.size())
            return;
        const Id id = prj_.lineOrder[row];
        bool ok = false;
        const QString name = QInputDialog::getText(this, L("act.rename"), L("label.name"),
                                                   QLineEdit::Normal, prj_.lines[id].name, &ok);
        if (!ok || name.isEmpty())
            return;
        pushUndo();
        prj_.lines[id].name = name;
        rebuildLines();
        onLayoutChanged();
    });
    QPushButton* del = new QPushButton(w);
    registerText(del, QStringLiteral("act.delete"));
    connect(del, &QPushButton::clicked, this, [this]() {
        const int row = lineList_->currentRow();
        if (row < 0 || row >= prj_.lineOrder.size())
            return;
        pushUndo();
        prj_.removeLine(prj_.lineOrder[row]);
        rebuildLines();
        onLayoutChanged();
    });
    QPushButton* up = new QPushButton(w);
    registerText(up, QStringLiteral("act.up"));
    connect(up, &QPushButton::clicked, this, [this]() {
        const int row = lineList_->currentRow();
        if (row <= 0 || row >= prj_.lineOrder.size())
            return;
        pushUndo();
        prj_.lineOrder.swapItemsAt(row - 1, row);
        rebuildLines();
        lineList_->setCurrentRow(row - 1);
        onLayoutChanged();
    });
    QPushButton* down = new QPushButton(w);
    registerText(down, QStringLiteral("act.down"));
    connect(down, &QPushButton::clicked, this, [this]() {
        const int row = lineList_->currentRow();
        if (row < 0 || row + 1 >= prj_.lineOrder.size())
            return;
        pushUndo();
        prj_.lineOrder.swapItemsAt(row, row + 1);
        rebuildLines();
        lineList_->setCurrentRow(row + 1);
        onLayoutChanged();
    });
    row1->addWidget(addline);
    row1->addWidget(ren);
    row1->addWidget(del);
    row1->addWidget(up);
    row1->addWidget(down);
    lay->addLayout(row1);

    QGroupBox* detail = new QGroupBox(w);
    registerText(detail, QStringLiteral("label.linedetail"));
    auto form = new QFormLayout(detail);
    lineNameEdit_ = new QLineEdit(detail);
    connect(lineNameEdit_, &QLineEdit::editingFinished, this, [this]() {
        if (updating_ || curLine_ < 0 || !prj_.lines.contains(curLine_))
            return;
        if (lineNameEdit_->text().isEmpty())
            return;
        pushUndo();
        prj_.lines[curLine_].name = lineNameEdit_->text();
        rebuildLines();
    });
    lineColorBtn_ = new QPushButton(detail);
    connect(lineColorBtn_, &QPushButton::clicked, this, [this]() {
        if (curLine_ < 0 || !prj_.lines.contains(curLine_))
            return;
        const QColor c = QColorDialog::getColor(prj_.lines[curLine_].color, this,
                                                L("label.color"));
        if (!c.isValid())
            return;
        pushUndo();
        prj_.lines[curLine_].color = c;
        lineColorBtn_->setStyleSheet(
            QStringLiteral("background-color: %1; min-height: 18px;").arg(c.name()));
        onLayoutChanged();
    });
    lineSecList_ = new QListWidget(detail);
    connect(lineSecList_, &QListWidget::itemChanged, this, [this](QListWidgetItem*) {
        if (updating_)
            return;
        if (curLine_ < 0 || !prj_.lines.contains(curLine_))
            return;
        pushUndo();
        applyLineSections();
    });
    form->addRow(L("label.name"), lineNameEdit_);
    form->addRow(L("label.color"), lineColorBtn_);
    form->addRow(L("label.sections"), lineSecList_);

    QGroupBox* style = new QGroupBox(detail);
    registerText(style, QStringLiteral("label.linestyle"));
    auto slay = new QVBoxLayout(style);
    lineTextChk_ = new QCheckBox(style);
    registerText(lineTextChk_, QStringLiteral("label.textonline"));
    connect(lineTextChk_, &QCheckBox::toggled, this, [this](bool on) {
        if (updating_ || curLine_ < 0 || !prj_.lines.contains(curLine_))
            return;
        pushUndo();
        prj_.lines[curLine_].labelTextOnLine = on;
        onLayoutChanged();
    });
    lineTermChk_ = new QCheckBox(style);
    registerText(lineTermChk_, QStringLiteral("label.termini"));
    connect(lineTermChk_, &QCheckBox::toggled, this, [this](bool on) {
        if (updating_ || curLine_ < 0 || !prj_.lines.contains(curLine_))
            return;
        pushUndo();
        prj_.lines[curLine_].labelTerminiBadges = on;
        onLayoutChanged();
    });
    lineAlongChk_ = new QCheckBox(style);
    registerText(lineAlongChk_, QStringLiteral("label.along"));
    connect(lineAlongChk_, &QCheckBox::toggled, this, [this](bool on) {
        if (updating_ || curLine_ < 0 || !prj_.lines.contains(curLine_))
            return;
        pushUndo();
        prj_.lines[curLine_].labelAlongBadges = on;
        onLayoutChanged();
    });
    slay->addWidget(lineTextChk_);
    slay->addWidget(lineTermChk_);
    slay->addWidget(lineAlongChk_);
    form->addRow(QString(), style);
    lay->addWidget(detail, 1);
    return w;
}

QWidget* MainWindow::buildSettingsTab()
{
    auto w = new QWidget(this);
    auto form = new QFormLayout(w);

    auto spin = [&](double min, double max, double dec, double step) {
        auto s = new QDoubleSpinBox(w);
        s->setRange(min, max);
        s->setDecimals(int(dec));
        s->setSingleStep(step);
        return s;
    };
    auto row = [&](const QString& key, QWidget* field) {
        QLabel* lbl = new QLabel(w);
        registerText(lbl, key);
        form->addRow(lbl, field);
    };

    setSpacing_ = spin(10, 500, 0, 10);
    setLineW_ = spin(1, 20, 1, 0.5);
    setBundle_ = spin(1, 30, 1, 0.5);
    setCorner_ = spin(0, 60, 1, 1);
    setBadge_ = spin(40, 1000, 0, 20);
    setLabelDist_ = spin(0, 60, 1, 1);
    setStopSize_ = spin(4, 40, 1, 0.5);
    setLineSize_ = spin(4, 40, 1, 0.5);
    setTextCol_ = new QComboBox(w);
    setTextCol_->addItem(L("color.black"));
    setTextCol_->addItem(L("color.white"));
    setStopFont_ = new QFontComboBox(w);
    setLineFont_ = new QFontComboBox(w);

    connect(setSpacing_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (updating_) return; prj_.settings.stopSpacing = v; onLayoutChanged(); });
    connect(setLineW_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (updating_) return; prj_.settings.lineWidth = v; onLayoutChanged(); });
    connect(setBundle_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (updating_) return; prj_.settings.bundleSpacing = v; onLayoutChanged(); });
    connect(setCorner_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (updating_) return; prj_.settings.cornerRadius = v; onLayoutChanged(); });
    connect(setBadge_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (updating_) return; prj_.settings.badgeEvery = v; onLayoutChanged(); });
    connect(setLabelDist_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (updating_) return; prj_.settings.labelDistance = v; onLayoutChanged(); });
    connect(setStopSize_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (updating_) return; prj_.settings.stopFontSize = v; onLayoutChanged(); });
    connect(setLineSize_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (updating_) return; prj_.settings.lineFontSize = v; onLayoutChanged(); });
    connect(setTextCol_, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (updating_) return;
        prj_.settings.textColor = idx == 1 ? Qt::white : Qt::black;
        onLayoutChanged();
    });
    connect(setStopFont_, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
        if (updating_) return; prj_.settings.stopFontFamily = f.family(); onLayoutChanged(); });
    connect(setLineFont_, &QFontComboBox::currentFontChanged, this, [this](const QFont& f) {
        if (updating_) return; prj_.settings.lineFontFamily = f.family(); onLayoutChanged(); });

    row(QStringLiteral("set.spacing"), setSpacing_);
    row(QStringLiteral("set.linewidth"), setLineW_);
    row(QStringLiteral("set.bundlespacing"), setBundle_);
    row(QStringLiteral("set.cornerradius"), setCorner_);
    row(QStringLiteral("set.badgeevery"), setBadge_);
    row(QStringLiteral("set.labeldist"), setLabelDist_);
    row(QStringLiteral("set.textcolor"), setTextCol_);
    row(QStringLiteral("set.stopfont"), setStopFont_);
    row(QStringLiteral("set.stopfontsize"), setStopSize_);
    row(QStringLiteral("set.linefont"), setLineFont_);
    row(QStringLiteral("set.linefontsize"), setLineSize_);
    return w;
}

// ---------- Listen ----------

void MainWindow::rebuildStops()
{
    updating_ = true;
    const int row = stopList_->currentRow();
    stopList_->clear();
    for (const Id id : prj_.stopOrder)
        stopList_->addItem(prj_.stops[id].name);
    stopList_->setCurrentRow(row >= 0 && row < stopList_->count() ? row : -1);
    updating_ = false;
}

void MainWindow::rebuildBoundaries()
{
    updating_ = true;
    const int row = boundaryList_->currentRow();
    boundaryList_->clear();
    for (const Id id : prj_.boundaryOrder)
        boundaryList_->addItem(prj_.boundaries[id].name);
    boundaryList_->setCurrentRow(row >= 0 && row < boundaryList_->count() ? row : -1);
    updating_ = false;
}

void MainWindow::rebuildSections()
{
    updating_ = true;
    sectionList_->clear();
    int selectRow = -1;
    for (const Id id : prj_.sectionOrder) {
        QListWidgetItem* it = new QListWidgetItem(prj_.sections[id].name);
        it->setData(Qt::UserRole, id);
        sectionList_->addItem(it);
        if (id == curSection_)
            selectRow = sectionList_->count() - 1;
    }
    sectionList_->setCurrentRow(selectRow);
    updating_ = false;
    rebuildSectionElems();
}

void MainWindow::rebuildSectionElems()
{
    updating_ = true;
    elemList_->clear();
    if (curSection_ >= 0 && prj_.sections.contains(curSection_)) {
        const Section& sec = prj_.sections[curSection_];
        for (const Elem& e : sec.elems) {
            const QString marker = e.kind == ElemKind::Stop
                ? QStringLiteral("\u25A0 ") // kleiner Block: Haltestelle
                : QStringLiteral("\u2192 "); // Pfeil: Grenzpunkt
            elemList_->addItem(marker + prj_.elemName(e));
        }
    }
    updating_ = false;
}

void MainWindow::rebuildNodes()
{
    updating_ = true;
    const int row = nodeList_->currentRow();
    nodeList_->clear();
    for (const Id id : prj_.nodeOrder) {
        QListWidgetItem* it = new QListWidgetItem(prj_.nodes[id].name);
        it->setData(Qt::UserRole, id);
        nodeList_->addItem(it);
    }
    nodeList_->setCurrentRow(row >= 0 && row < nodeList_->count() ? row : -1);
    updating_ = false;
}

void MainWindow::rebuildNodeCandidates()
{
    updating_ = true;
    nodeStopList_->clear();
    for (const Id id : prj_.stopOrder) {
        QListWidgetItem* it = new QListWidgetItem(prj_.stops[id].name);
        it->setData(Qt::UserRole, id);
        it->setFlags(it->flags() | Qt::ItemIsSelectable | Qt::ItemIsEnabled);
        nodeStopList_->addItem(it);
    }
    updating_ = false;
}

void MainWindow::rebuildLines()
{
    updating_ = true;
    lineList_->clear();
    int selectRow = -1;
    for (const Id id : prj_.lineOrder) {
        QListWidgetItem* it = new QListWidgetItem(
            QStringLiteral("\342\257\205 ") + prj_.lines[id].name);
        it->setData(Qt::UserRole, id);
        lineList_->addItem(it);
        if (id == curLine_)
            selectRow = lineList_->count() - 1;
    }
    lineList_->setCurrentRow(selectRow);
    updating_ = false;
    rebuildLineDetail();
}

void MainWindow::rebuildLineDetail()
{
    updating_ = true;
    const bool valid = curLine_ >= 0 && prj_.lines.contains(curLine_);
    lineNameEdit_->setEnabled(valid);
    lineColorBtn_->setEnabled(valid);
    lineSecList_->setEnabled(valid);
    lineTextChk_->setEnabled(valid);
    lineTermChk_->setEnabled(valid);
    lineAlongChk_->setEnabled(valid);
    if (valid) {
        const Line& ln = prj_.lines[curLine_];
        lineNameEdit_->setText(ln.name);
        lineColorBtn_->setStyleSheet(
            QStringLiteral("background-color: %1; min-height: 18px;").arg(ln.color.name()));
        lineSecList_->clear();
        for (const Id sid : prj_.sectionOrder) {
            QListWidgetItem* it = new QListWidgetItem(prj_.sections[sid].name);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            it->setCheckState(ln.sections.contains(sid) ? Qt::Checked : Qt::Unchecked);
            it->setData(Qt::UserRole, sid);
            lineSecList_->addItem(it);
        }
        lineTextChk_->setChecked(ln.labelTextOnLine);
        lineTermChk_->setChecked(ln.labelTerminiBadges);
        lineAlongChk_->setChecked(ln.labelAlongBadges);
    } else {
        lineNameEdit_->clear();
        lineSecList_->clear();
        lineTextChk_->setChecked(false);
        lineTermChk_->setChecked(false);
        lineAlongChk_->setChecked(false);
    }
    updating_ = false;
}

void MainWindow::applyLineSections()
{
    if (updating_ || curLine_ < 0 || !prj_.lines.contains(curLine_))
        return;
    Line& ln = prj_.lines[curLine_];
    ln.sections.clear();
    for (int i = 0; i < lineSecList_->count(); ++i) {
        QListWidgetItem* it = lineSecList_->item(i);
        if (it->checkState() == Qt::Checked)
            ln.sections.push_back(it->data(Qt::UserRole).toInt());
    }
    onLayoutChanged();
}

void MainWindow::refreshSettings()
{
    updating_ = true;
    const Settings& s = prj_.settings;
    setSpacing_->setValue(s.stopSpacing);
    setLineW_->setValue(s.lineWidth);
    setBundle_->setValue(s.bundleSpacing);
    setCorner_->setValue(s.cornerRadius);
    setBadge_->setValue(s.badgeEvery);
    setLabelDist_->setValue(s.labelDistance);
    setTextCol_->setCurrentIndex(s.textColor == Qt::white ? 1 : 0);
    QFont sf;
    if (!s.stopFontFamily.isEmpty())
        sf.setFamily(s.stopFontFamily);
    setStopFont_->setCurrentFont(sf);
    setStopSize_->setValue(s.stopFontSize);
    QFont lf;
    if (!s.lineFontFamily.isEmpty())
        lf.setFamily(s.lineFontFamily);
    setLineFont_->setCurrentFont(lf);
    setLineSize_->setValue(s.lineFontSize);
    updating_ = false;
}

void MainWindow::rebuildAll()
{
    rebuildStops();
    rebuildBoundaries();
    rebuildSections();
    rebuildNodes();
    rebuildNodeCandidates();
    rebuildLines();
    refreshSettings();
}

void MainWindow::afterProjectReplaced()
{
    l10n::setLanguage(prj_.language);
    curSection_ = -1;
    curLine_ = -1;
    rebuildAll();
    updateTexts();
    view_->relayout();
    view_->fitView();
    updateWindowTitle();
}

// ---------- Aktionen ----------

void MainWindow::pushUndo()
{
    undoStack_.push_back(serializeProject(prj_));
    if (undoStack_.size() > 100)
        undoStack_.removeFirst();
}

void MainWindow::fileNew()
{
    pushUndo();
    prj_.clear();
    currentFile_.clear();
    afterProjectReplaced();
}

void MainWindow::fileOpen()
{
    const QString path = QFileDialog::getOpenFileName(
        this, L("menu.file.open"), QString(), QStringLiteral("Liniennetzplan (*.lnz.json)"));
    if (path.isEmpty())
        return;
    bool ok = false;
    const QJsonObject obj = loadJsonFile(path, &ok);
    if (!ok) {
        QMessageBox::warning(this, L("err.open"), L("err.open"));
        return;
    }
    prj_ = Project::fromJson(obj);
    currentFile_ = path;
    undoStack_.clear();
    afterProjectReplaced();
}

void MainWindow::fileSave(bool saveAs)
{
    QString path = currentFile_;
    if (saveAs || path.isEmpty()) {
        path = QFileDialog::getSaveFileName(
            this, L("menu.file.saveas"), QString(),
            QStringLiteral("Liniennetzplan (*.lnz.json)"));
        if (path.isEmpty())
            return;
    }
    prj_.language = l10n::language();
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::warning(this, L("err.save"), L("err.save"));
        return;
    }
    const QByteArray data = QJsonDocument(prj_.toJson()).toJson(QJsonDocument::Indented);
    if (f.write(data) != data.size()) {
        QMessageBox::warning(this, L("err.save"), L("err.save"));
        return;
    }
    currentFile_ = path;
    updateWindowTitle();
}

void MainWindow::editUndo()
{
    if (undoStack_.isEmpty()) {
        statusBar()->showMessage(L("msg.noundo"), 3000);
        return;
    }
    const QString snap = undoStack_.takeLast();
    const QJsonObject obj = deserializeProject(snap);
    if (obj.isEmpty())
        return;
    prj_ = Project::fromJson(obj);
    afterProjectReplaced();
}

void MainWindow::loadDemo()
{
    pushUndo();
    makeDemo(prj_);
    currentFile_.clear();
    afterProjectReplaced();
    statusBar()->showMessage(L("msg.demo"), 4000);
}

void MainWindow::exportAs(int kind)
{
    view_->relayout();
    const QString key = kind == 0 ? QStringLiteral("menu.file.export.png")
                  : kind == 1 ? QStringLiteral("menu.file.export.svg")
                              : QStringLiteral("menu.file.export.pdf");
    const QString filter = kind == 0 ? QStringLiteral("PNG (*.png)")
                          : kind == 1 ? QStringLiteral("SVG (*.svg)")
                                      : QStringLiteral("PDF (*.pdf)");
    QString path = QFileDialog::getSaveFileName(this, L(key), QString(), filter);
    if (path.isEmpty())
        return;
    bool ok = false;
    if (kind == 0)
        ok = exportPng(path, prj_, view_->layoutResult());
    else if (kind == 1)
        ok = exportSvg(path, prj_, view_->layoutResult());
    else
        ok = exportPdf(path, prj_, view_->layoutResult());
    if (!ok)
        QMessageBox::warning(this, L("err.export"), L("err.export"));
}

void MainWindow::setLang(const QString& code)
{
    l10n::setLanguage(code);
    prj_.language = code;
    updateTexts();
    view_->relayout();
}

void MainWindow::showHelp()
{
    QMessageBox::information(this, L("help.title"), L("help.text"));
}
