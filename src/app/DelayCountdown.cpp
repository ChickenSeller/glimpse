#include "DelayCountdown.h"

#include "capture/Platform.h"
#include "capture/ScrollInput.h"

#include <QCursor>
#include <QGuiApplication>
#include <QMouseEvent>
#include <QPainter>
#include <QScreen>

namespace {

constexpr int kSize = 76;
constexpr int kMargin = 24;
// Esc is polled (another application has the focus) a few times a second.
constexpr int kPollMs = 100;

} // namespace

DelayCountdown::DelayCountdown(int seconds, QWidget *parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus)
    , m_remaining(seconds * 1000)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setCursor(Qt::PointingHandCursor);
    setToolTip(tr("Capturing soon. Click or press Esc to cancel."));
    setFixedSize(kSize, kSize);
    Platform::disableWindowAnimations(this);

    QScreen *screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QRect area = screen->availableGeometry();
    move(area.right() - kSize - kMargin, area.bottom() - kSize - kMargin);

    m_timer.setInterval(kPollMs);
    connect(&m_timer, &QTimer::timeout, this, &DelayCountdown::tick);
}

void DelayCountdown::start()
{
    ScrollInput::escapePressed(); // forget an Esc pressed before the countdown
    show();
    m_timer.start();
}

void DelayCountdown::tick()
{
    if (ScrollInput::escapePressed()) {
        cancel();
        return;
    }
    m_remaining -= kPollMs;
    if (m_remaining > 0) {
        update();
        return;
    }
    m_timer.stop();
    hide();
    emit finished();
}

void DelayCountdown::cancel()
{
    m_timer.stop();
    hide();
    emit canceled();
}

void DelayCountdown::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(20, 20, 20, 200));
    p.drawRoundedRect(rect(), 14, 14);

    QFont font = p.font();
    font.setPixelSize(kSize / 2);
    font.setBold(true);
    p.setFont(font);
    p.setPen(Qt::white);
    const int seconds = (m_remaining + 999) / 1000; // 2.3 s left shows "3"
    p.drawText(rect(), Qt::AlignCenter, QString::number(seconds));
}

void DelayCountdown::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        cancel();
}
