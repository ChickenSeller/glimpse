#pragma once

#include <QDialog>

#include <memory>

namespace Ui {
class AboutDialog;
}

// Name, version, what Glimpse is, where its source lives and its license.
// Layout lives in AboutDialog.ui.
class AboutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AboutDialog(QWidget *parent = nullptr);
    ~AboutDialog() override;

protected:
    void changeEvent(QEvent *event) override;

private:
    void updateTexts();

    std::unique_ptr<Ui::AboutDialog> ui;
};
