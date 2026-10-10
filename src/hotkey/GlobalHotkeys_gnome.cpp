#include "GlobalHotkeys.h"

#include "capture/Platform.h"

#include <QCoreApplication>
#include <QMap>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>

// GNOME (including Wayland, where no application may grab keys): the
// hotkeys become custom keyboard shortcuts in GNOME's settings that run
// "glimpse --hotkey <id>". A second Glimpse passes that on to the running
// one (see main.cpp), or Glimpse starts with it. GNOME 46 has no
// GlobalShortcuts portal yet.
//
// Shortcuts stay in GNOME's settings while Glimpse is not running, so a
// hotkey also starts it. clear() removes them (Glimpse pauses its hotkeys
// while the settings are open).

namespace {

const QString kMediaKeys = QStringLiteral("org.gnome.settings-daemon.plugins.media-keys");
const QString kCustomSchema = QStringLiteral("org.gnome.settings-daemon.plugins.media-keys.custom-keybinding");
const QString kCustomDir = QStringLiteral("/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/");
const QString kOurPrefix = QStringLiteral("glimpse-");

// Where GNOME keeps its own key bindings, checked for conflicts.
const QStringList kBindingSchemas = {
    QStringLiteral("org.gnome.desktop.wm.keybindings"),
    QStringLiteral("org.gnome.shell.keybindings"),
    QStringLiteral("org.gnome.mutter.keybindings"),
    QStringLiteral("org.gnome.mutter.wayland.keybindings"),
    kMediaKeys,
};

QString run(const QString &program, const QStringList &arguments)
{
    QProcess process;
    process.start(program, arguments);
    if (!process.waitForFinished(3000) || process.exitCode() != 0)
        return {};
    return QString::fromUtf8(process.readAllStandardOutput());
}

// A GVariant text string literal.
QString variantString(const QString &text)
{
    QString escaped = text;
    escaped.replace(QLatin1Char('\\'), QLatin1String("\\\\")).replace(QLatin1Char('"'), QLatin1String("\\\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

// The strings of a GVariant text array such as ['a', 'b'] or @as [].
QStringList variantStrings(const QString &text)
{
    QStringList strings;
    static const QRegularExpression quoted(QStringLiteral(R"('((?:[^'\\]|\\.)*)')"));
    for (auto it = quoted.globalMatch(text); it.hasNext();)
        strings << it.next().captured(1);
    return strings;
}

// "<Control><Alt>t", "<Primary><Alt>T" and the like as one comparable form.
QString normalizedAccelerator(const QString &accelerator)
{
    static const QRegularExpression modifier(QStringLiteral("<([^>]+)>"));
    QStringList modifiers;
    for (auto it = modifier.globalMatch(accelerator); it.hasNext();) {
        const QString name = it.next().captured(1).toLower();
        if (name == QLatin1String("control") || name == QLatin1String("ctrl") || name == QLatin1String("primary"))
            modifiers << QStringLiteral("C");
        else if (name == QLatin1String("alt") || name == QLatin1String("mod1"))
            modifiers << QStringLiteral("A");
        else if (name == QLatin1String("shift"))
            modifiers << QStringLiteral("S");
        else if (name == QLatin1String("super") || name == QLatin1String("mod4") || name == QLatin1String("meta"))
            modifiers << QStringLiteral("W");
        else
            modifiers << name;
    }
    modifiers.sort();
    modifiers.removeDuplicates();
    const QString key = accelerator.section(QLatin1Char('>'), -1).toLower();
    return modifiers.join(QString()) + QLatin1Char('|') + key;
}

// The first combination of `sequence` as a GTK accelerator, or empty.
QString toAccelerator(const QKeySequence &sequence)
{
    if (sequence.isEmpty())
        return {};
    const QKeyCombination combination = sequence[0];
    const Qt::Key key = combination.key();
    QString name;
    if (key >= Qt::Key_A && key <= Qt::Key_Z)
        name = QChar(QLatin1Char('a' + (key - Qt::Key_A)));
    else if (key >= Qt::Key_0 && key <= Qt::Key_9)
        name = QChar(QLatin1Char('0' + (key - Qt::Key_0)));
    else if (key >= Qt::Key_F1 && key <= Qt::Key_F35)
        name = QStringLiteral("F%1").arg(key - Qt::Key_F1 + 1);
    else {
        static const QMap<Qt::Key, QString> names = {
            {Qt::Key_Print, QStringLiteral("Print")},       {Qt::Key_Space, QStringLiteral("space")},
            {Qt::Key_Return, QStringLiteral("Return")},     {Qt::Key_Enter, QStringLiteral("KP_Enter")},
            {Qt::Key_Tab, QStringLiteral("Tab")},           {Qt::Key_Escape, QStringLiteral("Escape")},
            {Qt::Key_Insert, QStringLiteral("Insert")},     {Qt::Key_Delete, QStringLiteral("Delete")},
            {Qt::Key_Home, QStringLiteral("Home")},         {Qt::Key_End, QStringLiteral("End")},
            {Qt::Key_PageUp, QStringLiteral("Page_Up")},    {Qt::Key_PageDown, QStringLiteral("Page_Down")},
            {Qt::Key_Left, QStringLiteral("Left")},         {Qt::Key_Right, QStringLiteral("Right")},
            {Qt::Key_Up, QStringLiteral("Up")},             {Qt::Key_Down, QStringLiteral("Down")},
            {Qt::Key_Pause, QStringLiteral("Pause")},       {Qt::Key_Backspace, QStringLiteral("BackSpace")},
            {Qt::Key_Minus, QStringLiteral("minus")},       {Qt::Key_Equal, QStringLiteral("equal")},
            {Qt::Key_Comma, QStringLiteral("comma")},       {Qt::Key_Period, QStringLiteral("period")},
            {Qt::Key_Slash, QStringLiteral("slash")},       {Qt::Key_Backslash, QStringLiteral("backslash")},
            {Qt::Key_Semicolon, QStringLiteral("semicolon")}, {Qt::Key_Apostrophe, QStringLiteral("apostrophe")},
            {Qt::Key_BracketLeft, QStringLiteral("bracketleft")}, {Qt::Key_BracketRight, QStringLiteral("bracketright")},
            {Qt::Key_QuoteLeft, QStringLiteral("grave")},
        };
        name = names.value(key);
    }
    if (name.isEmpty())
        return {};
    const Qt::KeyboardModifiers modifiers = combination.keyboardModifiers();
    QString accelerator;
    if (modifiers & Qt::ControlModifier)
        accelerator += QLatin1String("<Control>");
    if (modifiers & Qt::AltModifier)
        accelerator += QLatin1String("<Alt>");
    if (modifiers & Qt::ShiftModifier)
        accelerator += QLatin1String("<Shift>");
    if (modifiers & Qt::MetaModifier)
        accelerator += QLatin1String("<Super>");
    return accelerator + name;
}

class GnomeGlobalHotkeys : public GlobalHotkeys
{
public:
    explicit GnomeGlobalHotkeys(QObject *parent)
        : GlobalHotkeys(parent)
    {
        m_flushTimer.setSingleShot(true);
        m_flushTimer.setInterval(0);
        connect(&m_flushTimer, &QTimer::timeout, this, [this] { flush(); });
    }

    bool isSupported() const override { return true; }

    bool add(int id, const QKeySequence &key, const QString &name) override
    {
        const QString accelerator = toAccelerator(key);
        if (accelerator.isEmpty())
            return false;
        if (m_taken.isEmpty())
            m_taken = gnomeBindings();
        if (m_taken.contains(normalizedAccelerator(accelerator)))
            return false;
        m_wanted.insert(id, {accelerator, QStringLiteral("Glimpse: %1").arg(name)});
        m_flushTimer.start(); // all of a registration round written at once
        return true;
    }

    void clear() override
    {
        m_wanted.clear();
        m_taken.clear(); // read again: the user may have changed GNOME's
        m_flushTimer.start();
    }

private:
    struct Shortcut {
        QString binding;
        QString name;
    };

    static QString pathFor(int id) { return kCustomDir + kOurPrefix + QString::number(id) + QLatin1Char('/'); }

    // Every accelerator GNOME or another custom shortcut uses, normalized.
    static QSet<QString> gnomeBindings()
    {
        QSet<QString> taken;
        for (const QString &schema : kBindingSchemas) {
            for (const QString &accelerator : variantStrings(run(QStringLiteral("gsettings"), {QStringLiteral("list-recursively"), schema})))
                taken.insert(normalizedAccelerator(accelerator));
        }
        const QStringList others = variantStrings(
            run(QStringLiteral("gsettings"), {QStringLiteral("get"), kMediaKeys, QStringLiteral("custom-keybindings")}));
        for (const QString &path : others) {
            if (path.startsWith(kCustomDir + kOurPrefix))
                continue;
            const QString binding = run(QStringLiteral("gsettings"), {QStringLiteral("get"), kCustomSchema + QLatin1Char(':') + path,
                                                                       QStringLiteral("binding")});
            for (const QString &accelerator : variantStrings(binding))
                taken.insert(normalizedAccelerator(accelerator));
        }
        // Not key bindings, but strings like '' or 'disabled' come along.
        taken.remove(normalizedAccelerator(QString()));
        return taken;
    }

    // Brings GNOME's settings in line with m_wanted: ours added, changed or
    // removed, everybody else's left alone. Runs the commands one after
    // another without blocking.
    void flush()
    {
        QStringList paths = variantStrings(
            run(QStringLiteral("gsettings"), {QStringLiteral("get"), kMediaKeys, QStringLiteral("custom-keybindings")}));
        QList<QStringList> commands;
        for (const QString &path : std::as_const(paths)) {
            if (path.startsWith(kCustomDir + kOurPrefix))
                commands << QStringList{QStringLiteral("dconf"), QStringLiteral("reset"), QStringLiteral("-f"), path};
        }
        paths.removeIf([](const QString &path) { return path.startsWith(kCustomDir + kOurPrefix); });

        QString program = QCoreApplication::applicationFilePath();
        program.replace(QLatin1Char('\''), QLatin1String("'\\''")); // for GLib's shell-like parsing
        // GNOME starts a command in a scope named after it, from which the
        // portals take the app's identity; one they have no permission for
        // gets its screenshots refused. In a scope of its own Glimpse has no
        // identity, as when started from a terminal.
        const QString launcher = QStandardPaths::findExecutable(QStringLiteral("systemd-run")).isEmpty()
                                     ? QString()
                                     : QStringLiteral("systemd-run --user --scope --quiet -- ");
        for (auto it = m_wanted.cbegin(); it != m_wanted.cend(); ++it) {
            const QString path = pathFor(it.key());
            const QString schema = kCustomSchema + QLatin1Char(':') + path;
            const QString command = QStringLiteral("%1'%2' --hotkey %3").arg(launcher, program).arg(it.key());
            commands << QStringList{QStringLiteral("gsettings"), QStringLiteral("set"), schema, QStringLiteral("name"), variantString(it->name)}
                     << QStringList{QStringLiteral("gsettings"), QStringLiteral("set"), schema, QStringLiteral("command"), variantString(command)}
                     << QStringList{QStringLiteral("gsettings"), QStringLiteral("set"), schema, QStringLiteral("binding"), variantString(it->binding)};
            paths << path;
        }
        QStringList quoted;
        for (const QString &path : std::as_const(paths))
            quoted << variantString(path);
        commands << QStringList{QStringLiteral("gsettings"), QStringLiteral("set"), kMediaKeys, QStringLiteral("custom-keybindings"),
                                QStringLiteral("@as [%1]").arg(quoted.join(QStringLiteral(", ")))};
        m_queue = commands;
        runNext();
    }

    void runNext()
    {
        if (m_running || m_queue.isEmpty())
            return;
        const QStringList command = m_queue.takeFirst();
        auto *process = new QProcess(this);
        m_running = true;
        connect(process, &QProcess::finished, this, [this, process] {
            process->deleteLater();
            m_running = false;
            runNext();
        });
        connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
            if (error != QProcess::FailedToStart)
                return;
            process->deleteLater();
            m_running = false;
            runNext();
        });
        process->start(command.first(), command.mid(1));
    }

    QMap<int, Shortcut> m_wanted;
    QSet<QString> m_taken;
    QTimer m_flushTimer;
    QList<QStringList> m_queue;
    bool m_running = false;
};

// Elsewhere (KDE, X11 desktops): not implemented yet.
class NullGlobalHotkeys : public GlobalHotkeys
{
public:
    using GlobalHotkeys::GlobalHotkeys;

    bool isSupported() const override { return false; }
    bool add(int, const QKeySequence &, const QString &) override { return false; }
    void clear() override {}
};

} // namespace

GlobalHotkeys *GlobalHotkeys::create(QObject *parent)
{
    if (Platform::supportsGlobalHotkeys())
        return new GnomeGlobalHotkeys(parent);
    return new NullGlobalHotkeys(parent);
}
