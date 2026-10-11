#pragma once

#include "OcrEngine.h"

#include <QDialog>
#include <QFutureWatcher>
#include <QImage>
#include <QList>
#include <QRect>

#include <memory>

namespace Ui {
class OcrResultDialog;
}

// Text recognition and translation in one window, top to bottom: the capture
// (with the recognized lines marked), the recognized text, editable, and its
// translation. OCR runs in the background right away; translating waits for
// the Translate button. "Translated Image" paints each paragraph's translation
// over the capture where it was. Layout lives in OcrResultDialog.ui.
class OcrResultDialog : public QDialog
{
    Q_OBJECT

public:
    OcrResultDialog(const QImage &image, const QString &engine, const QStringList &languages,
                    QWidget *parent = nullptr);
    ~OcrResultDialog() override;

    // Translates the text now; the owner calls it on translateRequested(),
    // once the translation engine is ready.
    void translate();

signals:
    // Translate was asked for (button, or a new language after a translation).
    void translateRequested();
    // The capture with the translation painted in, for the result window.
    void translatedImageReady(const QImage &image);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Translation { None, Running, Done, Failed };

    void onRecognized();
    // The paragraphs to translate: as recognized while the text is unedited,
    // else split at blank lines.
    QStringList sourceParagraphs() const;
    bool sourceEdited() const;
    void updateTexts();
    void updateButtons();
    void updatePreview();

    std::unique_ptr<Ui::OcrResultDialog> ui;
    QImage m_image;
    QString m_engine;
    QFutureWatcher<OcrResult> m_watcher;
    OcrResult m_result;
    bool m_recognized = false;
    QString m_recognizedText;      // as first shown, to tell edits apart
    QStringList m_paragraphs;      // recognized paragraphs, for translating
    QList<QRect> m_boxes;          // one per recognized paragraph, image pixels
    QList<int> m_lineHeights;      // per paragraph: its tallest line, image pixels
    QStringList m_translations;    // one per paragraph of the last translation
    Translation m_translation = Translation::None;
    QString m_translateError;
    int m_request = 0;             // ignores replies to superseded requests
};
