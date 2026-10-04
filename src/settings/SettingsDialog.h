#pragma once

#include "app/CaptureMode.h"

#include <QColor>
#include <QDialog>
#include <QHash>

#include <memory>

class HotkeyEdit;

namespace Ui {
class SettingsDialog;
}

// Application settings. Layout lives in SettingsDialog.ui; values are read on
// construction and written to AppSettings by Apply or OK.
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget *parent = nullptr);
    ~SettingsDialog() override;

    void accept() override;

signals:
    // Settings were saved; the language has already been switched.
    void applied();

protected:
    void changeEvent(QEvent *event) override;

private:
    // Validates and saves; false (with the problem shown) if nothing was saved.
    bool apply();
    void setModified(bool modified);
    HotkeyEdit *hotkeyEdit(CaptureMode mode) const;
    void restoreDefaults();
    QString selectedLanguage() const;
    QStringList selectedOcrLanguages() const;
    void setOcrLanguages(const QStringList &languages);
    void updateOcrEngineNote();
    void applyPlatformLimits();
    QString systemDefaultLabel() const;
    void setFreehandColor(const QColor &color);
    void showTranslateEngine();

    std::unique_ptr<Ui::SettingsDialog> ui;
    bool m_modified = false;
    QColor m_freehandColor;
    // API keys as edited, per engine (the key field shows the current engine's).
    QHash<QString, QString> m_translateKeys;
    QString m_translateKeyEngine;
};
