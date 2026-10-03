#pragma once

#include <QColor>
#include <QDialog>

#include <memory>

class QLineEdit;

namespace Ui {
class ColorResultDialog;
}

// Shows a color picked from the screen in common notations, each with a Copy
// button. The HEX value is copied right away. Layout lives in ColorResultDialog.ui.
class ColorResultDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ColorResultDialog(const QColor &color, QWidget *parent = nullptr);
    ~ColorResultDialog() override;

signals:
    // The user wants another color; the dialog has already closed.
    void pickAgainRequested();

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateTexts();
    void updateIcons();
    void copy(const QLineEdit *field);

    std::unique_ptr<Ui::ColorResultDialog> ui;
    QColor m_color;
    QString m_copied; // the text last copied, for the status line
};
