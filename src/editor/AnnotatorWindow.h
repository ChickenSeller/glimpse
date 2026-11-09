#pragma once

#include "app/TintedIcon.h"

#include <QImage>
#include <QMainWindow>

#include <memory>

namespace kImageAnnotator {
class KImageAnnotator;
}

namespace Ui {
class AnnotatorWindow;
}

// The FastStone-style editor: kImageAnnotator (draw, text, arrows, numbers,
// blur/pixelate, crop, resize, rotate, undo) plus Done / Save / Copy.
// Layout lives in AnnotatorWindow.ui; the annotator widget is added in code.
//
// It edits either a capture (Done hands the result back) or an image file
// (Save writes it back to the file).
class AnnotatorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AnnotatorWindow(const QImage &image, QWidget *parent = nullptr);
    // Edits the image file at `filePath`, already read into `image`.
    AnnotatorWindow(const QImage &image, const QString &filePath, QWidget *parent = nullptr);
    ~AnnotatorWindow() override;

    // Opens the image file in a new editor window; false (with the reason
    // shown) if it cannot be read.
    static bool openFile(const QString &path);

signals:
    // The edited image, emitted by Done (or by applying when closing).
    void finished(const QImage &image);

protected:
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    QImage editedImage() const;
    void done();
    // Saves to the image file (editing one); false if not saved.
    bool save();
    bool saveAs();
    void setModified(bool modified);
    void updateTitle();
    void copyToClipboard();
    void updateToolTips();
    void fitToScreen();
    void showPixelExact();

    std::unique_ptr<Ui::AnnotatorWindow> ui;
    kImageAnnotator::KImageAnnotator *m_annotator = nullptr;
    ActionIconTinter m_icons;
    qreal m_devicePixelRatio = 1.0;
    QString m_filePath; // the image file edited, if any
    bool m_modified = false;
    bool m_finished = false;
};
