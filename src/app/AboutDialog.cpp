#include "AboutDialog.h"
#include "ui_AboutDialog.h"

#include "AppIcon.h"

#include <QtGlobal>

namespace {

QString link(const QString &url, const QString &text)
{
    return QStringLiteral("<a href=\"%1\">%2</a>").arg(url.toHtmlEscaped(), text.toHtmlEscaped());
}

} // namespace

AboutDialog::AboutDialog(QWidget *parent)
    : QDialog(parent)
    , ui(std::make_unique<Ui::AboutDialog>())
{
    ui->setupUi(this);
    setAttribute(Qt::WA_DeleteOnClose);
    ui->iconLabel->setPixmap(appIcon().pixmap(64, 64));
    updateTexts();
}

AboutDialog::~AboutDialog() = default;

void AboutDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        updateTexts();
    }
    QDialog::changeEvent(event);
}

void AboutDialog::updateTexts()
{
    const QString github = QStringLiteral(GLIMPSE_GITHUB_URL);
    ui->titleLabel->setText(QStringLiteral("<span style=\"font-size: 12pt; font-weight: 600;\">Glimpse</span>"
                                           "&nbsp;&nbsp;%1")
                                .arg(tr("Version %1").arg(QStringLiteral(GLIMPSE_VERSION)).toHtmlEscaped()));
#ifdef GLIMPSE_FORUM_BUILD
    // No links (see CMakeLists.txt); the license and notices are beside glimpse.exe.
    ui->linksLabel->hide();
    ui->licenseLabel->setText(
        tr("Copyright © 2026 Kaguya. Released under the %1; built with Qt %2 and other open-source components (%3).")
            .arg(tr("MIT License").toHtmlEscaped(), QString::fromLatin1(qVersion()),
                 tr("third-party notices").toHtmlEscaped()));
    return;
#endif
    ui->linksLabel->setText(QStringLiteral("%1: %2<br>%3: %4")
                                .arg(tr("Source code").toHtmlEscaped(), link(github, github),
                                     tr("Homepage").toHtmlEscaped(),
                                     link(QStringLiteral(GLIMPSE_HOMEPAGE_URL), QStringLiteral(GLIMPSE_HOMEPAGE_URL))));
    ui->licenseLabel->setText(
        tr("Copyright © 2026 Kaguya. Released under the %1; built with Qt %2 and other open-source components (%3).")
            .arg(link(github + QStringLiteral("/blob/master/LICENSE"), tr("MIT License")),
                 QString::fromLatin1(qVersion()),
                 link(github + QStringLiteral("/blob/master/THIRD_PARTY_NOTICES.md"), tr("third-party notices"))));
}
