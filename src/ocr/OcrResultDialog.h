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
    // The translation engine chosen in the window (Translate::engineIds()),
    // and for the local engine its model (see LocalModel).
    QString translator() const;
    QString localModel() const;
    // Recognizes the text again with `engine`, which the window then shows
    // as chosen. A translation made is made again for the new text.
    void recognize(const QString &engine);
    // The engine shown as chosen (back to the one in use when the owner
    // could not prepare the new one).
    QString ocrEngine() const { return m_engine; }
    void showOcrEngine(const QString &engine);

signals:
    // Translate was asked for (button, or a new language after a translation).
    void translateRequested();
    // The capture with the translation painted in, for the result window.
    void translatedImageReady(const QImage &image);
    // Another recognition engine was chosen; the owner calls recognize()
    // once it is ready (its models downloaded).
    void recognizeRequested(const QString &engine);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    enum class Translation { None, Running, Done, Failed };

    void onRecognized();
    // The paragraphs to translate: as recognized while the text is unedited,
    // else each line of it.
    QStringList sourceParagraphs() const;
    // One text per recognized paragraph, laid out as the paragraphs were.
    QString layOut(const QStringList &texts) const;
    QString followLayout(const QString &source, const QStringList &translations) const;
    // One text per recognized paragraph to paint: the translation as edited
    // in the window, each line of it one paragraph.
    QStringList paintedTranslations() const;
    bool sourceEdited() const;
    bool translationEdited() const;
    void updateTexts();
    void updateButtons();
    void updatePreview();
    void fillEngines();
    void fillOcrEngines();
    QString translatorName() const;

    std::unique_ptr<Ui::OcrResultDialog> ui;
    QImage m_image;
    QString m_engine;
    QStringList m_languages;
    bool m_translateAgain = false; // after recognizing anew
    QFutureWatcher<OcrResult> m_watcher;
    OcrResult m_result;
    bool m_recognized = false;
    QString m_recognizedText;      // as first shown, to tell edits apart
    QStringList m_paragraphs;      // recognized paragraphs, for translating
    QList<QRect> m_boxes;          // one per recognized paragraph, image pixels
    QList<int> m_lineHeights;      // per paragraph: its tallest line, image pixels
    QStringList m_translations;    // one per paragraph of the last translation, one line each
    QString m_translatedText;      // the translation as shown, to tell edits apart
    Translation m_translation = Translation::None;
    QString m_translateError;
    int m_request = 0;             // ignores replies to superseded requests
};
