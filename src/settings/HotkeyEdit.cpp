#include "HotkeyEdit.h"

#include "app/TintedIcon.h"

#include <QAction>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>

HotkeyEdit::HotkeyEdit(QWidget *parent)
    : QKeySequenceEdit(parent)
{
    setMaximumSequenceLength(1);
    setClearButtonEnabled(true);

    // The icon sits inside the field, at its right end.
    if (auto *line = findChild<QLineEdit *>()) {
        m_statusAction = new QAction(this);
        line->addAction(m_statusAction, QLineEdit::TrailingPosition);
    }
    updateStatusIcon();
    // A changed hotkey is only known to work once it is applied.
    connect(this, &QKeySequenceEdit::keySequenceChanged, this, [this] { setStatus(Status::Unknown); });
}

void HotkeyEdit::setStatus(Status status)
{
    m_status = status;
    updateStatusIcon();
}

void HotkeyEdit::updateStatusIcon()
{
    if (!m_statusAction)
        return;
    QString icon;
    QColor color;
    QString tip;
    switch (m_status) {
    case Status::Active:
        icon = QStringLiteral(":/icons/check.svg");
        color = QColor(0x2f, 0x9e, 0x5b);
        tip = tr("Working");
        break;
    case Status::Taken:
        icon = QStringLiteral(":/icons/warning.svg");
        color = QColor(0xe0, 0x8a, 0x1e);
        tip = tr("Already in use by another program; this hotkey does not work");
        break;
    case Status::Unsupported:
        icon = QStringLiteral(":/icons/block.svg");
        color = palette().color(QPalette::PlaceholderText);
        tip = tr("Global hotkeys are not available here");
        break;
    case Status::Unknown:
        break;
    }
    m_statusAction->setVisible(!icon.isEmpty());
    m_statusAction->setIcon(icon.isEmpty() ? QIcon() : tintedIcon(QIcon(icon), color));
    m_statusAction->setToolTip(tip);
}

void HotkeyEdit::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Print) {
        const Qt::KeyboardModifiers modifiers = event->modifiers() & ~Qt::KeypadModifier;
        setKeySequence(QKeySequence(QKeyCombination(modifiers, Qt::Key_Print)));
        emit editingFinished();
        return;
    }
    QKeySequenceEdit::keyReleaseEvent(event);
}

void HotkeyEdit::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange || event->type() == QEvent::PaletteChange)
        updateStatusIcon();
    QKeySequenceEdit::changeEvent(event);
}
