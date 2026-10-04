#pragma once

#include <QDialog>

#include <memory>

namespace Ui {
class RecordingDoneDialog;
}

// Shown when a recording has been written: where it is, how long it is, and
// Open / Show in Folder / Copy Path. Layout lives in RecordingDoneDialog.ui.
class RecordingDoneDialog : public QDialog
{
    Q_OBJECT

public:
    RecordingDoneDialog(const QString &filePath, qint64 durationMs, QWidget *parent = nullptr);
    ~RecordingDoneDialog() override;

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateTexts();

    std::unique_ptr<Ui::RecordingDoneDialog> ui;
    QString m_path;
    qint64 m_durationMs = 0;
};
