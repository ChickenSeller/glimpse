#include "HotkeyEdit.h"

#include <QKeyEvent>

HotkeyEdit::HotkeyEdit(QWidget *parent)
    : QKeySequenceEdit(parent)
{
    setMaximumSequenceLength(1);
    setClearButtonEnabled(true);
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
