#pragma once

#include "CaptureMode.h"

#include <QHash>
#include <QIcon>
#include <QKeySequence>
#include <QPointer>
#include <QWidget>

#include <memory>

class QActionGroup;
class QMenu;
class QScreen;
class QTimer;
class QToolButton;
class QVariantAnimation;

namespace Ui {
class CaptureToolbar;
}

// The small always-on-top capture bar, modelled on FastStone Capture's.
// Layout lives in CaptureToolbar.ui: the buttons sit on a panel inside the
// window, so that docked to a screen edge the window can shrink to a strip
// while the panel slides with it.
//
// Docking: dropped against an edge of the screen under the pointer, the bar
// sticks to that edge and slides into it a while after the pointer leaves;
// touching the strip slides it out. The window never leaves its screen, so
// at an edge between two screens nothing shows on the other one.
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
    // Brought up on purpose (tray, second start): docked, it slides out for a while.
    void reveal();

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
    void showEvent(QShowEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    bool nativeEvent(const QByteArray &eventType, void *message, qintptr *result) override;

private:
    void applyIconColor();
    void updateToolTips();
    void applyPlatformLimits();
    void setupDelayMenu();
    void updateDelayButton();
    void chooseCustomDelay();
    void savePosition();
    void keepOnScreen();

    enum class DockEdge { None, Left, Right, Top, Bottom };
    void beginDrag();
    void endDrag();
    void dockTo(QScreen *screen, DockEdge edge, QPoint position);
    void undock();
    void restoreDock();
    void setHiddenAmount(qreal amount); // 0 out, 1 slid in
    void slideTo(qreal amount);
    void scheduleSlideIn();
    void slideInIfIdle();

    std::unique_ptr<Ui::CaptureToolbar> ui;
    QHash<QToolButton *, QString> m_baseToolTips; // as set in the .ui
    QHash<CaptureMode, QToolButton *> m_modeButtons;
    QMenu *m_delayMenu = nullptr;
    QActionGroup *m_delayGroup = nullptr;
    QHash<QToolButton *, QKeySequence> m_hotkeys;
    QHash<QToolButton *, QIcon> m_sourceIcons; // icons as set in the .ui, before tinting
    int m_baseSpacing = -1;                     // the layout's, as set in the .ui (at 100%)
    QMargins m_baseMargins;

    DockEdge m_dockEdge = DockEdge::None;
    QPointer<QScreen> m_dockScreen;
    QPoint m_dockPosition; // top-left of the whole bar, slid out
    qreal m_hiddenAmount = 0;
    bool m_dragging = false;
    QVariantAnimation *m_slide = nullptr;
    QTimer *m_slideInTimer = nullptr;
    QTimer *m_dragEndTimer = nullptr; // where the end of a move is not reported
};
