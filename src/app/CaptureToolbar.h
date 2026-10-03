#pragma once

#include <QHash>
#include <QIcon>
#include <QKeySequence>
#include <QWidget>

#include <memory>

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

    // Appends the global hotkeys to the button tooltips; an empty sequence means none.
    void setHotkeyHints(const QKeySequence &window, const QKeySequence &region,
                        const QKeySequence &fullScreen);

signals:
    void windowRequested();
    void fullScreenRequested();
    void regionRequested();
    void settingsRequested();
    void closed();

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    void applyIconColor();
    void updateToolTips();
    void savePosition();

    std::unique_ptr<Ui::CaptureToolbar> ui;
    QHash<QToolButton *, QString> m_baseToolTips; // as set in the .ui
    QHash<QToolButton *, QKeySequence> m_hotkeys;
    QHash<QToolButton *, QIcon> m_sourceIcons; // icons as set in the .ui, before tinting
};
