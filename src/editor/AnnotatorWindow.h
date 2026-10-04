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
class AnnotatorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AnnotatorWindow(const QImage &image, QWidget *parent = nullptr);
    ~AnnotatorWindow() override;

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
    void saveAs();
    void copyToClipboard();
    void updateToolTips();
    void fitToScreen();
    void showPixelExact();

    std::unique_ptr<Ui::AnnotatorWindow> ui;
    kImageAnnotator::KImageAnnotator *m_annotator = nullptr;
    ActionIconTinter m_icons;
    qreal m_devicePixelRatio = 1.0;
    bool m_modified = false;
    bool m_finished = false;
};
