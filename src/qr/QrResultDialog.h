#pragma once

#include "BarcodeScanner.h"

#include <QDialog>
#include <QImage>

#include <memory>

namespace Ui {
class QrResultDialog;
}

// Shows what a QR scan found: the scanned area with each code outlined and
// numbered, the decoded texts, and Copy / Open Link. Layout lives in QrResultDialog.ui.
class QrResultDialog : public QDialog
{
    Q_OBJECT

public:
    QrResultDialog(const QImage &image, const QList<ScannedCode> &codes, QWidget *parent = nullptr);
    ~QrResultDialog() override;

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateTexts();
    void showCode(int row);
    void copySelected();
    void openSelected();
    QString selectedText() const;

    std::unique_ptr<Ui::QrResultDialog> ui;
    QList<ScannedCode> m_codes;
};
