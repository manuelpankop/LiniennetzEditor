#pragma once

#include <QMainWindow>
#include <QPair>
#include <QVector>

#include "model.h"

class NetworkView;
class QTabWidget;
class QListWidget;
class QPushButton;
class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QComboBox;
class QCheckBox;
class QFontComboBox;
class QMenu;
class QAction;
class QActionGroup;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* ev) override;

private slots:
    void onUndoPush(const QString& snapshot);
    void onLayoutChanged();
    void showWarnings();

private:
    // UI
    void buildUi();
    QWidget* buildStopsTab();
    QWidget* buildBoundariesTab();
    QWidget* buildSectionsTab();
    QWidget* buildNodesTab();
    QWidget* buildLinesTab();
    QWidget* buildSettingsTab();
    void registerText(QWidget* w, const QString& key);
    void registerAction(QAction* a, const QString& key);
    void registerMenu(QMenu* m, const QString& key);
    void updateTexts();
    void updateWindowTitle();

    // Aktionen
    void fileNew();
    void fileOpen();
    void fileSave(bool saveAs);
    void editUndo();
    void loadDemo();
    void exportAs(int kind); // 0 = PNG, 1 = SVG, 2 = PDF
    void setLang(const QString& code);
    void showHelp();

    // Listen
    void rebuildStops();
    void rebuildBoundaries();
    void rebuildSections();
    void rebuildSectionElems();
    void rebuildNodes();
    void rebuildNodeCandidates();
    void rebuildLines();
    void rebuildLineDetail();
    void applyLineSections();
    void refreshSettings();
    void rebuildAll();
    void afterProjectReplaced();

    void pushUndo();

    Project prj_;
    NetworkView* view_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    QListWidget* stopList_ = nullptr;
    QListWidget* boundaryList_ = nullptr;
    QListWidget* sectionList_ = nullptr;
    QListWidget* elemList_ = nullptr;
    QListWidget* nodeStopList_ = nullptr;
    QListWidget* nodeList_ = nullptr;
    QListWidget* lineList_ = nullptr;
    QLineEdit* lineNameEdit_ = nullptr;
    QPushButton* lineColorBtn_ = nullptr;
    QListWidget* lineSecList_ = nullptr;
    QCheckBox* lineTextChk_ = nullptr;
    QCheckBox* lineTermChk_ = nullptr;
    QCheckBox* lineAlongChk_ = nullptr;
    QDoubleSpinBox* setSpacing_ = nullptr;
    QDoubleSpinBox* setLineW_ = nullptr;
    QDoubleSpinBox* setBundle_ = nullptr;
    QDoubleSpinBox* setCorner_ = nullptr;
    QDoubleSpinBox* setBadge_ = nullptr;
    QDoubleSpinBox* setLabelDist_ = nullptr;
    QComboBox* setTextCol_ = nullptr;
    QFontComboBox* setStopFont_ = nullptr;
    QDoubleSpinBox* setStopSize_ = nullptr;
    QFontComboBox* setLineFont_ = nullptr;
    QDoubleSpinBox* setLineSize_ = nullptr;
    QPushButton* warnBtn_ = nullptr;
    QAction* langDe_ = nullptr;
    QAction* langEn_ = nullptr;

    QVector<QPair<QWidget*, QString>> textTargets_;
    QVector<QPair<QAction*, QString>> actionTargets_;
    QVector<QPair<QMenu*, QString>> menuTargets_;
    QVector<QString> tabKeys_;
    QVector<QString> undoStack_;
    QVector<QString> lastWarnings_;
    QString currentFile_;
    Id curSection_ = -1;
    Id curLine_ = -1;
    bool updating_ = false;
    bool firstShow_ = true;
};
