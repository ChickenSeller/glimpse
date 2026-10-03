#pragma once

#include <QKeySequenceEdit>

// QKeySequenceEdit for a single global hotkey. Windows never delivers a key
// press for PrtSc, only the release, so that key is recorded on release.
// Used from SettingsDialog.ui as a promoted widget.
class HotkeyEdit : public QKeySequenceEdit
{
    Q_OBJECT

public:
    explicit HotkeyEdit(QWidget *parent = nullptr);

protected:
    void keyReleaseEvent(QKeyEvent *event) override;
};
