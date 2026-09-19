/*
 * This file is part of PATM.
 *
 * PATM (Pipeline Automation Tool Manager) is free software: you can
 * redistribute it and/or modify it under the terms of the GNU General
 * Public License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * PATM is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PATM. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <QApplication>
#include <QIcon>
#include <QMessageBox>
#include <QProcess>

#include "installerwizard.h"
#include "ui/c_backend.h"
#include "ui/mainwindow.h"
#include "ui/thememanager.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);
    app.setApplicationName("PATM");
    app.setOrganizationName("patm");
    app.setApplicationDisplayName("PATM");
    app.setWindowIcon(QIcon(":/org/patm/assets/patm-icon.svg"));

    bool uninstallMode = false;
    for (int i = 1; i < argc; i++) {
        QString arg = argv[i];
        if (arg == "--uninstall" || arg == "-u")
            uninstallMode = true;
    }

    if (uninstallMode) {
        UninstallerWizard wizard;
        wizard.show();
        int ret = wizard.exec();
        if (ret == QDialog::Accepted) {
            QMessageBox::information(
                nullptr, "PATM",
                "PATM has been uninstalled successfully.");
        }
        return ret;
    }

    InstallerWizard wizard;
    wizard.show();
    int ret = wizard.exec();
    if (ret == QDialog::Accepted) {
        bool shouldOpen = wizard.autoOpenApp();
        QMessageBox::information(
            nullptr, "PATM",
            "Installation complete! You can now launch PATM "
            "from your application menu or terminal.");
        if (shouldOpen)
            QProcess::startDetached(wizard.installPath() + "/patm");
    }
    return ret;
}
