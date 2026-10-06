#pragma once

#include "CaptureMode.h"

#include <QHash>
#include <QIcon>
#include <QKeySequence>
#include <QWidget>

#include <memory>

class QActionGroup;
class QMenu;
class QToolButton;

namespace Ui {
class CaptureToolbar;
}

// The small always-on-top capture bar, modelled on FastStone Capture's.
// Layout lives in CaptureToolbar.ui.
class CaptureToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit CaptureToolbar(QWidget *parent = nullptr);
    ~CaptureToolbar() override;

    // Appends the global hotkeys to the button tooltips; modes without one show none.
    void setHotkeyHints(const QHash<CaptureMode, QKeySequence> &hotkeys);
    // Orders, shows and sizes the buttons as set in Settings > Toolbar.
    void applyLayoutSettings();

signals:
    void captureRequested(CaptureMode mode);
    void settingsRequested();
    void closed();
    void minimizeRequested();

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void applyIconColor();
    void updateToolTips();
    void applyPlatformLimits();
    void setupDelayMenu();
    void updateDelayButton();
    void chooseCustomDelay();
    void savePosition();
    void keepOnScreen();

    std::unique_ptr<Ui::CaptureToolbar> ui;
    QHash<QToolButton *, QString> m_baseToolTips; // as set in the .ui
    QHash<CaptureMode, QToolButton *> m_modeButtons;
    QMenu *m_delayMenu = nullptr;
    QActionGroup *m_delayGroup = nullptr;
    QHash<QToolButton *, QKeySequence> m_hotkeys;
    QHash<QToolButton *, QIcon> m_sourceIcons; // icons as set in the .ui, before tinting
    int m_baseSpacing = -1;                     // the layout's, as set in the .ui (at 100%)
    QMargins m_baseMargins;
};
