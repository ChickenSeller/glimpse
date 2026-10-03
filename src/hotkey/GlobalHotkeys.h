#pragma once

#include <QKeySequence>
#include <QObject>

// System-wide hotkeys that fire while other applications have focus.
class GlobalHotkeys : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    // Picks the backend for the current platform; it may support nothing.
    static GlobalHotkeys *create(QObject *parent = nullptr);

    // False where no backend exists yet; add() then always fails.
    virtual bool isSupported() const = 0;
    // Registers the first key combination of `key` under `id`. Returns false if
    // the platform cannot do it or another application already owns the combination.
    virtual bool add(int id, const QKeySequence &key) = 0;
    virtual void clear() = 0;

signals:
    void activated(int id);
};
