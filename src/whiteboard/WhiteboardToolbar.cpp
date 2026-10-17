#include "WhiteboardToolbar.h"
#include "ui_WhiteboardToolbar.h"

#include "app/TintedIcon.h"

#include <QActionGroup>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QToolButton>
#include <QWindow>

namespace {

// A round swatch of `color`, ringed so white and black show on any theme.
QIcon swatchIcon(const QColor &color)
{
    QPixmap pixmap(QSize(44, 44));
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(128, 128, 128), 2));
    p.setBrush(color);
    p.drawEllipse(QRectF(6, 6, 32, 32));
    return QIcon(pixmap);
}

// A line as thick as `width`, for the width menu.
QIcon widthIcon(qreal width, const QColor &color)
{
    QPixmap pixmap(QSize(48, 48));
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(color, std::min(width * 1.5, 24.0), Qt::SolidLine, Qt::RoundCap));
    p.drawLine(QPointF(10, 24), QPointF(38, 24));
    return QIcon(pixmap);
}

} // namespace

WhiteboardToolbar::WhiteboardToolbar(WhiteboardCanvas *canvas)
    : QWidget(canvas, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    , ui(std::make_unique<Ui::WhiteboardToolbar>())
    , m_canvas(canvas)
{
    ui->setupUi(this);
    setAttribute(Qt::WA_AlwaysShowToolTips);

    const auto buttons = findChildren<QToolButton *>();
    for (QToolButton *button : buttons)
        m_sourceIcons.insert(button, button->icon());

    using Tool = WhiteboardCanvas::Tool;
    m_toolButtons = {
        {ui->penButton, Tool::Pen},           {ui->highlighterButton, Tool::Highlighter},
        {ui->lineButton, Tool::Line},         {ui->arrowButton, Tool::Arrow},
        {ui->rectangleButton, Tool::Rectangle}, {ui->ellipseButton, Tool::Ellipse},
        {ui->eraserButton, Tool::Eraser},
    };
    for (auto it = m_toolButtons.cbegin(); it != m_toolButtons.cend(); ++it) {
        const Tool tool = it.value();
        connect(it.key(), &QToolButton::clicked, this, [this, tool] { drawWith([this, tool] { m_canvas->setTool(tool); }); });
    }

    const QList<QColor> &colors = WhiteboardCanvas::colors();
    for (qsizetype i = 0; i < colors.size(); ++i) {
        auto *button = new QToolButton(this);
        button->setCheckable(true);
        button->setAutoRaise(true);
        button->setIconSize(QSize(22, 22));
        button->setIcon(swatchIcon(colors.at(i)));
        const QColor color = colors.at(i);
        connect(button, &QToolButton::clicked, this, [this, color] { drawWith([this, color] { m_canvas->setColor(color); }); });
        ui->colorsLayout->addWidget(button);
        m_colorButtons.append(button);
    }

    m_widthMenu = new QMenu(this);
    auto *widthGroup = new QActionGroup(this);
    for (const qreal width : WhiteboardCanvas::penWidths()) {
        QAction *action = m_widthMenu->addAction(QString());
        action->setCheckable(true);
        widthGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, width] { m_canvas->setPenWidth(width); });
        m_widthActions.append(action);
    }
    ui->widthButton->setMenu(m_widthMenu);
    ui->widthButton->setPopupMode(QToolButton::InstantPopup);

    using Background = WhiteboardCanvas::Background;
    m_backgroundMenu = new QMenu(this);
    auto *backgroundGroup = new QActionGroup(this);
    for (const Background background : {Background::Live, Background::Frozen, Background::White}) {
        QAction *action = m_backgroundMenu->addAction(QString());
        action->setCheckable(true);
        backgroundGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, background] { m_canvas->setBackground(background); });
        m_backgroundActions.insert(action, background);
    }
    ui->backgroundButton->setMenu(m_backgroundMenu);
    ui->backgroundButton->setPopupMode(QToolButton::InstantPopup);

    connect(ui->passThroughButton, &QToolButton::clicked, m_canvas, &WhiteboardCanvas::setPassThrough);
    connect(ui->undoButton, &QToolButton::clicked, m_canvas, &WhiteboardCanvas::undo);
    connect(ui->redoButton, &QToolButton::clicked, m_canvas, &WhiteboardCanvas::redo);
    connect(ui->clearButton, &QToolButton::clicked, m_canvas, &WhiteboardCanvas::clear);
    connect(ui->copyButton, &QToolButton::clicked, m_canvas, &WhiteboardCanvas::copy);
    connect(ui->saveButton, &QToolButton::clicked, m_canvas, &WhiteboardCanvas::save);
    connect(ui->closeButton, &QToolButton::clicked, m_canvas, &QWidget::close);
    connect(m_canvas, &WhiteboardCanvas::stateChanged, this, &WhiteboardToolbar::sync);

    // No title bar: the grip moves the window.
    ui->dragHandle->setCursor(Qt::SizeAllCursor);
    ui->dragHandle->installEventFilter(this);

    retranslate();
    applyIconColor();
    sync();
}

WhiteboardToolbar::~WhiteboardToolbar() = default;

// Choosing a tool or a color means drawing again, so pass-through ends.
void WhiteboardToolbar::drawWith(const std::function<void()> &choose)
{
    if (m_canvas->passThrough())
        m_canvas->setPassThrough(false);
    choose();
}

void WhiteboardToolbar::sync()
{
    for (auto it = m_toolButtons.cbegin(); it != m_toolButtons.cend(); ++it)
        it.key()->setChecked(it.value() == m_canvas->tool());
    const QList<QColor> &colors = WhiteboardCanvas::colors();
    for (qsizetype i = 0; i < m_colorButtons.size(); ++i)
        m_colorButtons.at(i)->setChecked(m_canvas->tool() != WhiteboardCanvas::Tool::Eraser
                                         && colors.at(i) == m_canvas->color());
    const QList<qreal> &widths = WhiteboardCanvas::penWidths();
    for (qsizetype i = 0; i < m_widthActions.size(); ++i)
        m_widthActions.at(i)->setChecked(widths.at(i) == m_canvas->penWidth());
    for (auto it = m_backgroundActions.cbegin(); it != m_backgroundActions.cend(); ++it)
        it.key()->setChecked(it.value() == m_canvas->background());
    ui->passThroughButton->setChecked(m_canvas->passThrough());
    ui->passThroughButton->setEnabled(m_canvas->background() == WhiteboardCanvas::Background::Live);
    ui->undoButton->setEnabled(m_canvas->canUndo());
    ui->redoButton->setEnabled(m_canvas->canRedo());
    ui->clearButton->setEnabled(!m_canvas->isEmpty());
}

void WhiteboardToolbar::applyIconColor()
{
    const QColor color = palette().color(QPalette::ButtonText);
    for (auto it = m_sourceIcons.cbegin(); it != m_sourceIcons.cend(); ++it)
        it.key()->setIcon(tintedIcon(it.value(), color));
    const QList<qreal> &widths = WhiteboardCanvas::penWidths();
    for (qsizetype i = 0; i < m_widthActions.size(); ++i)
        m_widthActions.at(i)->setIcon(widthIcon(widths.at(i), color));
}

void WhiteboardToolbar::retranslate()
{
    for (qsizetype i = 0; i < m_colorButtons.size(); ++i)
        m_colorButtons.at(i)->setToolTip(tr("Color (%1)").arg(i + 1));
    const QList<qreal> &widths = WhiteboardCanvas::penWidths();
    for (qsizetype i = 0; i < m_widthActions.size(); ++i)
        m_widthActions.at(i)->setText(tr("%1 px").arg(widths.at(i)));
    using Background = WhiteboardCanvas::Background;
    for (auto it = m_backgroundActions.cbegin(); it != m_backgroundActions.cend(); ++it) {
        switch (it.value()) {
        case Background::Live: it.key()->setText(tr("&Live Screen")); break;
        case Background::Frozen: it.key()->setText(tr("&Frozen Screen")); break;
        case Background::White: it.key()->setText(tr("&Whiteboard")); break;
        }
    }
}

void WhiteboardToolbar::paintEvent(QPaintEvent *)
{
    // Without a frame the bar needs its own edge to stand out from what is behind it.
    QPainter painter(this);
    painter.fillRect(rect(), palette().window());
    painter.setPen(palette().color(QPalette::Mid));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
}

void WhiteboardToolbar::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::PaletteChange) {
        applyIconColor();
    } else if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        retranslate();
    }
    QWidget::changeEvent(event);
}

bool WhiteboardToolbar::eventFilter(QObject *watched, QEvent *event)
{
    // The window system moves the window, which also works on Wayland.
    if (watched == ui->dragHandle && event->type() == QEvent::MouseButtonPress
        && static_cast<QMouseEvent *>(event)->button() == Qt::LeftButton && windowHandle()) {
        windowHandle()->startSystemMove();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}
