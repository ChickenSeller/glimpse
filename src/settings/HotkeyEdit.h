#pragma once

#include <QKeySequenceEdit>

class QAction;

// QKeySequenceEdit for a single global hotkey. Windows never delivers a key
// press for PrtSc, only the release, so that key is recorded on release.
// Shows at its right whether the hotkey works (see setStatus()).
// Used from SettingsDialog.ui as a promoted widget.
class HotkeyEdit : public QKeySequenceEdit
{
    Q_OBJECT

public:
    enum class Status {
        Unknown,     // no hotkey, or changed and not applied yet: no icon
        Active,      // registered and working
        Taken,       // another program has it
        Unsupported, // global hotkeys are not available on this platform
    };

    explicit HotkeyEdit(QWidget *parent = nullptr);

    void setStatus(Status status);

protected:
    void keyReleaseEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;

private:
    void updateStatusIcon();

    Status m_status = Status::Unknown;
    QAction *m_statusAction = nullptr;
};
