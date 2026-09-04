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

#ifndef PATM_UI_MAINWINDOW_H
#define PATM_UI_MAINWINDOW_H

#include <QMainWindow>
#include <QMap>

#include "c_backend.h"

class QListWidget;
class QListWidgetItem;
class QTabWidget;
class QSplitter;
class QLabel;
class QToolButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    static const PatmConfig *config();
    static QTabWidget *tabWidget();
    static MainWindow *instance();
    static PatmConn *activeConnection();

    int addTab(QWidget *page, const QString &title);
    void removeTab(QWidget *page);
    void saveSession();
    void restoreSession();
    const char *lastConnId() const;

public slots:
    void setStatus(const QString &msg);
    void refreshConnectionList();
    void disconnectActive();

private slots:
    void onNewConnection();
    void onConnect();
    void onDisconnect();
    void onDeleteConnection();
    void onSqlQuery();
    void onSqlTerminal();
    void onRunTool();
    void onSettings();
    void onTableDoubleClicked(QListWidgetItem *item);

private:
    void setupUi();
    void setupToolBar();
    QWidget *setupLeftPane();
    void setupRightPane();
    void refreshTableList();
    PatmError tryConnectIndex(int index, const QString &password);
    void showPasswordPrompt(int profileIndex);
    void closeEvent(QCloseEvent *event) override;

    static MainWindow *s_instance;
    PatmConfig m_cfg;

    QListWidget *m_connList;
    QListWidget *m_tableList;
    QTabWidget *m_tabWidget;
    QLabel *m_statusBar;
    QToolButton *m_btnConnect;
    QToolButton *m_btnRunTool;
    QWidget *m_welcomePage;

    const PatmConnProfile *m_activeProfile = nullptr;
    PatmConn *m_activeConn = nullptr;
    PatmSshTunnel *m_activeTunnel = nullptr;
    char m_lastConnId[64] = {};
    QMap<QString, QString> m_passwordCache;
};

#endif
