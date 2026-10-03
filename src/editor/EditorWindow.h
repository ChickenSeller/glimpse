#pragma once

#include <QImage>
#include <QMainWindow>

#include <memory>

namespace Ui {
class EditorWindow;
}

// Shows a finished capture. For now: view, save and copy; annotation tools come later.
// Layout lives in EditorWindow.ui.
class EditorWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit EditorWindow(const QImage &image, QWidget *parent = nullptr);
    ~EditorWindow() override;

protected:
    void changeEvent(QEvent *event) override;

private:
    void saveAs();
    void copyToClipboard();
    void fitToScreen();
    void updateTexts();

    std::unique_ptr<Ui::EditorWindow> ui;
    QImage m_image;
};
