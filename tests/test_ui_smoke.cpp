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

#include <unistd.h>

#include <QApplication>
#include <QTimer>

#include "ui/c_backend.h"
#include "ui/mainwindow.h"
#include "ui/settingsdialog.h"
#include "ui/thememanager.h"
#include "ui/toolrunner.h"
#include "ui/tooleditor.h"

/*
 * UI smoke test (requires a display; run under xvfb-run in CI):
 *
 * Regression test: build the main window + settings dialog + tool runner,
 * let them map, then destroy them and drain events so the full widget
 * disposal chain runs while ASan is watching.
 */

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);

    PatmUiSettings ui;
    patm_ui_settings_defaults(&ui);
    snprintf(ui.theme, sizeof(ui.theme), "fusion-dark");
    PatmThemeManager::apply(ui);

    MainWindow win;
    win.resize(800, 500);
    win.show();

    /* Process events to let the window map */
    QApplication::processEvents();
    QApplication::processEvents();

    /* Settings dialog */
    SettingsDialog dlg(&win);
    dlg.show();
    QApplication::processEvents();
    QApplication::processEvents();
    dlg.close();
    QApplication::processEvents();

    /* Tool runner tab */
    ToolRunner *tr = new ToolRunner;
    win.addTab(tr, "Run Tool");
    QApplication::processEvents();
    QApplication::processEvents();
    win.removeTab(tr);
    QApplication::processEvents();

    /* Tool editor */
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        char tool[1100];
        snprintf(tool, sizeof(tool), "%s/tools/export_csv.py", cwd);
        ToolEditor editor(QString::fromUtf8(tool), &win);
        editor.show();
        QApplication::processEvents();
        QApplication::processEvents();
        editor.close();
        QApplication::processEvents();
    }

    win.close();
    QApplication::processEvents();
    QApplication::processEvents();

    puts("UI smoke test passed");
    return 0;
}
