#pragma once

#include "OcrEngine.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QImage>

#include <memory>

namespace Ui {
class OcrResultDialog;
}

// Runs OCR on a captured area in the background and shows the text, editable
// before copying. Layout lives in OcrResultDialog.ui.
class OcrResultDialog : public QDialog
{
    Q_OBJECT

public:
    OcrResultDialog(const QImage &image, const QString &engine, const QStringList &languages,
                    QWidget *parent = nullptr);
    ~OcrResultDialog() override;

protected:
    void changeEvent(QEvent *event) override;

private:
    void onFinished();
    void updateTexts();
    void updatePreview();
    void copyText();

    std::unique_ptr<Ui::OcrResultDialog> ui;
    QImage m_image;
    QString m_engine;
    QFutureWatcher<OcrResult> m_watcher;
    OcrResult m_result;
    bool m_done = false;
};
