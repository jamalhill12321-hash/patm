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

    PatmUiSettings ui;
    PatmError load_err = patm_ui_settings_load(&ui);
    if (patm_is_ok(&load_err))
        PatmThemeManager::apply(ui);

    PatmError err = patm_pipeline_init();
    if (!patm_is_ok(&err))
        qWarning("Python engine init failed: %s", err.msg);

    char latest[64];
    if (patm_update_check(latest, sizeof(latest)) ==
        PATM_UPDATE_AVAILABLE)
        qWarning("A newer PATM version is available: %s", latest);

    MainWindow *win = new MainWindow();
    win->setPalette(qApp->palette());
    win->show();

    int ret = app.exec();

    patm_pipeline_finalize();
    delete win;
    return ret;
}
