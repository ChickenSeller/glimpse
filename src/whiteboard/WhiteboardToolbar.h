#pragma once

#include "WhiteboardCanvas.h"

#include <QHash>
#include <QIcon>
#include <QWidget>

#include <functional>
#include <memory>

class QAction;
class QMenu;
class QToolButton;

namespace Ui {
class WhiteboardToolbar;
}

// The screen whiteboard's tools: a small window above its canvas, kept out of
// captures and screen sharing.
class WhiteboardToolbar : public QWidget
{
    Q_OBJECT

public:
    explicit WhiteboardToolbar(WhiteboardCanvas *canvas);
    ~WhiteboardToolbar() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void drawWith(const std::function<void()> &choose);
    void sync();
    void applyIconColor();
    void retranslate();

    std::unique_ptr<Ui::WhiteboardToolbar> ui;
    WhiteboardCanvas *m_canvas;
    QHash<QToolButton *, WhiteboardCanvas::Tool> m_toolButtons;
    QList<QToolButton *> m_colorButtons;
    QHash<QToolButton *, QIcon> m_sourceIcons;
    QMenu *m_widthMenu;
    QList<QAction *> m_widthActions;
    QMenu *m_backgroundMenu;
    QHash<QAction *, WhiteboardCanvas::Background> m_backgroundActions;
};
