#pragma once

#include "app/TintedIcon.h"

#include <QImage>
#include <QMainWindow>
#include <QPointer>

#include <memory>

class AnnotatorWindow;

namespace Ui {
class EditorWindow;
}

// Shows a finished capture: save, copy, or open it in the annotation editor.
// Layout lives in EditorWindow.ui.
class EditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit EditorWindow(const QImage &image, QWidget *parent = nullptr);
    ~EditorWindow() override;

    // Copies the image and says so in the status bar.
    void copyToClipboard();

protected:
    void changeEvent(QEvent *event) override;

private:
    void saveAs();
    void saveAndCopyPath();
    void edit();
    void pinToScreen();
    void setImage(const QImage &image);
    void fitToScreen();
    void updateTexts();
    void updateToolTips();

    std::unique_ptr<Ui::EditorWindow> ui;
    QImage m_image;
    ActionIconTinter m_icons;
    QPointer<AnnotatorWindow> m_annotator;
};
