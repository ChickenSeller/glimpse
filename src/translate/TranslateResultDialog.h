#pragma once

#include "ocr/OcrEngine.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QImage>
#include <QList>
#include <QRect>

#include <memory>

namespace Ui {
class TranslateResultDialog;
}

// Screenshot translation: recognizes the text of a capture (with the OCR
// engine from the settings), groups the lines into paragraphs, translates
// them, and shows source and translation side by side. "Translated Image"
// paints each paragraph's translation over the capture where it was.
// Layout lives in TranslateResultDialog.ui.
class TranslateResultDialog : public QDialog
{
    Q_OBJECT

public:
    TranslateResultDialog(const QImage &image, const QString &ocrEngine, const QStringList &ocrLanguages,
                          QWidget *parent = nullptr);
    ~TranslateResultDialog() override;

signals:
    // The capture with the translation painted in, for the result window.
    void translatedImageReady(const QImage &image);

protected:
    void changeEvent(QEvent *event) override;

private:
    enum class State { Recognizing, Translating, Done, Failed };

    void onRecognized();
    void startTranslation();
    QStringList sourceParagraphs() const;
    void updateTexts();
    void updateButtons();

    std::unique_ptr<Ui::TranslateResultDialog> ui;
    QImage m_image;
    QFutureWatcher<OcrResult> m_watcher;
    QList<QRect> m_boxes;          // one per recognized paragraph, image pixels
    QStringList m_translations;    // one per paragraph of the last translation
    QList<int> m_lineHeights;      // per paragraph: its tallest line, image pixels
    State m_state = State::Recognizing;
    QString m_error;
    int m_request = 0;             // ignores replies to superseded requests
};
