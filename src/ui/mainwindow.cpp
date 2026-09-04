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

#include "mainwindow.h"

#include <QApplication>
#include <QCloseEvent>
#include <QComboBox>
#include <QDialog>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "c_backend.h"
#include "connectionpropertiesdialog.h"
#include "querywindow.h"
#include "settingsdialog.h"
#include "sqlterminal.h"
#include "toolrunner.h"

#include "version.h"

MainWindow *MainWindow::s_instance = nullptr;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    s_instance = this;
    patm_config_load(&m_cfg);
    setWindowTitle("PATM");
    setMinimumSize(900, 560);
    showMaximized();
    setupUi();
    refreshConnectionList();
    restoreSession();
}

MainWindow::~MainWindow() = default;

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveSession();
    if (m_activeConn && m_lastConnId[0])
        patm_reconnect_save(m_lastConnId);
    event->accept();
}

MainWindow *MainWindow::instance() { return s_instance; }
PatmConn *MainWindow::activeConnection() {
    return s_instance ? s_instance->m_activeConn : nullptr;
}
const PatmConfig *MainWindow::config() { return &s_instance->m_cfg; }
QTabWidget *MainWindow::tabWidget() { return s_instance->m_tabWidget; }
const char *MainWindow::lastConnId() const {
    return m_lastConnId[0] ? m_lastConnId : nullptr;
}

void MainWindow::setStatus(const QString &msg) { m_statusBar->setText(msg); }

void MainWindow::setupUi()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *vbox = new QVBoxLayout(central);
    vbox->setContentsMargins(4, 4, 4, 4);

    setupToolBar();
    QWidget *leftPane = setupLeftPane();
    setupRightPane();

    QSplitter *splitter = new QSplitter(Qt::Horizontal);
    splitter->addWidget(leftPane);
    QWidget *rightPane = new QWidget;
    QVBoxLayout *rvbox = new QVBoxLayout(rightPane);
    rvbox->setContentsMargins(0, 0, 0, 0);
    rvbox->addWidget(m_tabWidget);
    splitter->addWidget(rightPane);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setSizes({240, 660});
    vbox->addWidget(splitter, 1);

    m_statusBar = new QLabel("Ready.");
    m_statusBar->setAlignment(Qt::AlignLeft);
    m_statusBar->setContentsMargins(8, 4, 8, 4);
    vbox->addWidget(m_statusBar);

    QLabel *versionLabel = new QLabel(QString("%1  |  Credit: Jamama").arg(PATM_VERSION));
    versionLabel->setAlignment(Qt::AlignRight);
    versionLabel->setContentsMargins(8, 2, 8, 2);
    versionLabel->setStyleSheet("color: gray;");
    vbox->addWidget(versionLabel);

    setCentralWidget(central);
}

void MainWindow::setupToolBar()
{
    QToolBar *toolbar = addToolBar("Main");
    toolbar->setMovable(false);

    auto addBtn = [toolbar](const QString &text, auto slot, QToolButton **out = nullptr) {
        QToolButton *btn = new QToolButton();
        btn->setText(text);
        toolbar->addWidget(btn);
        QObject::connect(btn, &QToolButton::clicked, slot);
        if (out) *out = btn;
        return btn;
    };

    addBtn("New Connection", [this]() { onNewConnection(); });
    addBtn("Connect", [this]() { onConnect(); }, &m_btnConnect);
    addBtn("Disconnect", [this]() { onDisconnect(); });
    addBtn("Delete", [this]() { onDeleteConnection(); });
    toolbar->addSeparator();
    addBtn("Run Tool", [this]() { onRunTool(); }, &m_btnRunTool);
    addBtn("SQL Query", [this]() { onSqlQuery(); });
    addBtn("SQL Terminal", [this]() { onSqlTerminal(); });
    toolbar->addSeparator();
    addBtn("Settings", [this]() { onSettings(); });
}

QWidget *MainWindow::setupLeftPane()
{
    QWidget *leftPane = new QWidget;
    QVBoxLayout *lbox = new QVBoxLayout(leftPane);
    lbox->setContentsMargins(0, 0, 0, 0);

    QLabel *connLabel = new QLabel("Connections");
    connLabel->setContentsMargins(4, 2, 4, 2);
    lbox->addWidget(connLabel);

    m_connList = new QListWidget;
    m_connList->setMinimumWidth(150);
    m_connList->setContextMenuPolicy(Qt::CustomContextMenu);
    lbox->addWidget(m_connList, 1);

    connect(m_connList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *) { onConnect(); });
    connect(m_connList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = m_connList->itemAt(pos);
        if (!item) return;
        int index = m_connList->row(item);
        if ((size_t)index >= m_cfg.nconns) return;

        QMenu menu(this);
        menu.addAction("Connect", [this]() { onConnect(); });
        menu.addAction("Properties", [this, index]() {
            ConnectionPropertiesDialog dlg(m_cfg.conns[index].id, this);
            dlg.exec();
        });
        menu.addSeparator();
        menu.addAction("Delete", [this]() { onDeleteConnection(); });
        menu.exec(m_connList->mapToGlobal(pos));
    });

    QLabel *tableLabel = new QLabel("Tables");
    tableLabel->setContentsMargins(4, 2, 4, 2);
    lbox->addWidget(tableLabel);

    m_tableList = new QListWidget;
    lbox->addWidget(m_tableList, 1);

    connect(m_tableList, &QListWidget::itemDoubleClicked, this, &MainWindow::onTableDoubleClicked);

    return leftPane;
}

void MainWindow::setupRightPane()
{
    m_tabWidget = new QTabWidget;
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);

    m_welcomePage = new QWidget;
    QVBoxLayout *wbox = new QVBoxLayout(m_welcomePage);
    wbox->setAlignment(Qt::AlignCenter);

    QLabel *logo = new QLabel;
    logo->setAlignment(Qt::AlignCenter);
    logo->setPixmap(QPixmap(":/org/patm/assets/patm-icon.svg").scaled(
        96, 96, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    wbox->addWidget(logo);

    QLabel *title = new QLabel("PATM");
    title->setObjectName("welcome-title");
    title->setAlignment(Qt::AlignCenter);
    QLabel *sub = new QLabel("Pipeline Automation Tool Manager");
    sub->setObjectName("welcome-sub");
    sub->setAlignment(Qt::AlignCenter);
    QLabel *hint = new QLabel(
        "Connect to a database, then:\n"
        "  \u2022  Double-click a table to query it\n"
        "  \u2022  Open a SQL terminal from the toolbar\n"
        "  \u2022  Run a tool from the toolbar");
    hint->setObjectName("welcome-hint");
    hint->setAlignment(Qt::AlignCenter);
    wbox->addWidget(title);
    wbox->addWidget(sub);
    wbox->addSpacing(12);
    wbox->addWidget(hint);
    m_tabWidget->addTab(m_welcomePage, "Welcome");

    connect(m_tabWidget, &QTabWidget::tabCloseRequested, this, [this](int index) {
        QWidget *w = m_tabWidget->widget(index);
        if (w == m_welcomePage) return;
        m_tabWidget->removeTab(index);
        w->deleteLater();
        saveSession();
        if (m_tabWidget->count() <= 1) {
            m_welcomePage->setVisible(true);
            m_tabWidget->setCurrentIndex(0);
        }
    });
}

void MainWindow::refreshConnectionList()
{
    m_connList->clear();
    for (size_t i = 0; i < m_cfg.nconns; i++)
        m_connList->addItem(QString::fromUtf8(m_cfg.conns[i].id));
}

void MainWindow::refreshTableList()
{
    m_tableList->clear();
    if (!m_activeConn) return;

    PatmStrBuf json = {};
    PatmError err = patm_db_list_tables(m_activeConn, &json);
    if (!patm_is_ok(&err)) {
        setStatus(QString("Listing tables failed: %1").arg(err.msg));
        patm_strbuf_free(&json);
        return;
    }

    const char *p = json.data ? json.data : "";
    char name[256];
    while ((p = strchr(p, '"')) != nullptr) {
        p++;
        const char *end = strchr(p, '"');
        if (!end) break;
        size_t len = (size_t)(end - p);
        if (len >= sizeof(name)) len = sizeof(name) - 1;
        memcpy(name, p, len);
        name[len] = '\0';
        m_tableList->addItem(QString::fromUtf8(name));
        p = end + 1;
    }
    patm_strbuf_free(&json);
}

void MainWindow::disconnectActive()
{
    if (m_activeConn) { patm_db_close(m_activeConn); m_activeConn = nullptr; }
    if (m_activeTunnel) { patm_ssh_tunnel_stop(m_activeTunnel); m_activeTunnel = nullptr; }
    m_activeProfile = nullptr;
    m_lastConnId[0] = '\0';
    m_tableList->clear();
    patm_reconnect_clear();
}

PatmError MainWindow::tryConnectIndex(int index, const QString &password)
{
    if ((size_t)index >= m_cfg.nconns)
        return patm_error(PATM_ERR_INVALID_ARG, "bad connection index");

    disconnectActive();
    const PatmConnProfile *profile = &m_cfg.conns[index];

    PatmSshTunnel *tunnel = nullptr;
    if (profile->ssh_host[0]) {
        PatmSshTunnelParams tp = {
            profile->ssh_host, profile->ssh_user, profile->ssh_port,
            profile->host,
            profile->port > 0 ? profile->port
                              : patm_db_driver_get(profile->engine)->default_port,
        };
        PatmError terr = patm_ssh_tunnel_start(&tp, &tunnel);
        if (!patm_is_ok(&terr)) return terr;
    }

    int effective_port = profile->port;
    const char *effective_host = profile->host;
    if (tunnel) { effective_host = "127.0.0.1"; effective_port = patm_ssh_tunnel_local_port(tunnel); }

    QByteArray pwBytes = password.toUtf8();
    PatmConnParams params = {
        profile->engine, effective_host, profile->user,
        pwBytes.constData(), profile->dbname, effective_port,
        profile->ssl_mode[0] ? profile->ssl_mode : PATM_SSL_DISABLE,
    };
    PatmError err = patm_db_connect(&params, &m_activeConn);
    if (!patm_is_ok(&err)) {
        m_activeConn = nullptr;
        if (tunnel) { patm_ssh_tunnel_stop(tunnel); tunnel = nullptr; }
        return err;
    }
    m_activeProfile = profile;
    m_activeTunnel = tunnel;
    snprintf(m_lastConnId, sizeof(m_lastConnId), "%s", profile->id);
    setStatus(QString("Connected to %1 (%2).").arg(profile->id, patm_db_driver_get(profile->engine)->display));
    refreshTableList();
    saveSession();
    return patm_ok();
}

void MainWindow::showPasswordPrompt(int profileIndex)
{
    QDialog dlg(this);
    dlg.setWindowTitle("Password");
    dlg.setMinimumWidth(360);
    QFormLayout *fl = new QFormLayout(&dlg);

    QLineEdit *pwEdit = new QLineEdit;
    pwEdit->setEchoMode(QLineEdit::Password);
    fl->addRow("Password:", pwEdit);

    QHBoxLayout *btns = new QHBoxLayout;
    QPushButton *cancelBtn = new QPushButton("Cancel");
    QPushButton *goBtn = new QPushButton("Connect");
    btns->addWidget(cancelBtn);
    btns->addWidget(goBtn);
    fl->addRow(btns);

    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(goBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

    if (dlg.exec() == QDialog::Accepted) {
        PatmError err = tryConnectIndex(profileIndex, pwEdit->text());
        if (patm_is_ok(&err)) {
            if ((size_t)profileIndex < m_cfg.nconns) {
                const char *id = m_cfg.conns[profileIndex].id;
                patm_secure_store_password(id, pwEdit->text().toUtf8().constData());
                m_passwordCache[id] = pwEdit->text();
            }
        } else {
            setStatus(QString("Connect failed: %1").arg(err.msg));
        }
    } else {
        setStatus("Connection cancelled.");
    }
}

void MainWindow::onNewConnection()
{
    QDialog dlg(this);
    dlg.setWindowTitle("New connection");
    dlg.setMinimumWidth(440);
    QFormLayout *fl = new QFormLayout(&dlg);

    QComboBox *engineCombo = new QComboBox;
    engineCombo->addItems({"PostgreSQL", "MySQL", "MariaDB"});
    fl->addRow("Engine:", engineCombo);

    QLineEdit *idEdit = new QLineEdit;
    QLineEdit *hostEdit = new QLineEdit("127.0.0.1");
    QLineEdit *portEdit = new QLineEdit;
    QLineEdit *userEdit = new QLineEdit;
    QLineEdit *dbEdit = new QLineEdit;
    QLineEdit *sshHostEdit = new QLineEdit;
    QLineEdit *sshUserEdit = new QLineEdit;
    QLineEdit *sshPortEdit = new QLineEdit("22");
    QLineEdit *pwEdit = new QLineEdit;
    pwEdit->setEchoMode(QLineEdit::Password);

    fl->addRow("Name:", idEdit);
    fl->addRow("Host:", hostEdit);
    fl->addRow("Port:", portEdit);
    fl->addRow("User:", userEdit);
    fl->addRow("Database:", dbEdit);
    fl->addRow("SSH host:", sshHostEdit);
    fl->addRow("SSH user:", sshUserEdit);
    fl->addRow("SSH port:", sshPortEdit);
    fl->addRow("Password:", pwEdit);

    QComboBox *sslCombo = new QComboBox;
    sslCombo->addItems({"Disabled (default)", "Preferred", "Required"});
    fl->addRow("TLS:", sslCombo);

    QLabel *testLabel = new QLabel;
    fl->addRow(testLabel);

    QHBoxLayout *btns = new QHBoxLayout;
    QPushButton *testBtn = new QPushButton("Test Connection");
    QPushButton *saveBtn = new QPushButton("Save");
    QPushButton *closeBtn = new QPushButton("Close");
    btns->addWidget(testBtn);
    btns->addWidget(saveBtn);
    btns->addWidget(closeBtn);
    fl->addRow(btns);

    connect(testBtn, &QPushButton::clicked, &dlg, [&]() {
        PatmConnProfile probe = {};
        snprintf(probe.id, sizeof(probe.id), "%s", idEdit->text().toUtf8().constData());
        probe.engine = (PatmDbEngine)engineCombo->currentIndex();
        snprintf(probe.host, sizeof(probe.host), "%s", hostEdit->text().toUtf8().constData());
        probe.port = portEdit->text().toInt();
        snprintf(probe.user, sizeof(probe.user), "%s", userEdit->text().toUtf8().constData());
        snprintf(probe.dbname, sizeof(probe.dbname), "%s", dbEdit->text().toUtf8().constData());
        snprintf(probe.ssh_host, sizeof(probe.ssh_host), "%s", sshHostEdit->text().toUtf8().constData());
        snprintf(probe.ssh_user, sizeof(probe.ssh_user), "%s", sshUserEdit->text().toUtf8().constData());
        probe.ssh_port = sshPortEdit->text().toInt();
        static const char *modes[] = {PATM_SSL_DISABLE, PATM_SSL_PREFER, PATM_SSL_REQUIRE};
        snprintf(probe.ssl_mode, sizeof(probe.ssl_mode), "%s", modes[sslCombo->currentIndex()]);

        PatmConn *conn = nullptr;
        QByteArray pw = pwEdit->text().toUtf8();
        PatmConnParams params = {
            probe.engine, probe.host, probe.user, pw.constData(),
            probe.dbname, probe.port, probe.ssl_mode,
        };
        PatmError err = patm_db_connect(&params, &conn);
        if (patm_is_ok(&err)) {
            testLabel->setText("Connection OK.");
            patm_db_close(conn);
        } else {
            testLabel->setText(err.msg);
        }
    });

    connect(saveBtn, &QPushButton::clicked, &dlg, [&]() {
        PatmConnProfile profile = {};
        snprintf(profile.id, sizeof(profile.id), "%s", idEdit->text().toUtf8().constData());
        profile.engine = (PatmDbEngine)engineCombo->currentIndex();
        snprintf(profile.host, sizeof(profile.host), "%s", hostEdit->text().toUtf8().constData());
        profile.port = portEdit->text().toInt();
        snprintf(profile.user, sizeof(profile.user), "%s", userEdit->text().toUtf8().constData());
        snprintf(profile.dbname, sizeof(profile.dbname), "%s", dbEdit->text().toUtf8().constData());
        snprintf(profile.ssh_host, sizeof(profile.ssh_host), "%s", sshHostEdit->text().toUtf8().constData());
        snprintf(profile.ssh_user, sizeof(profile.ssh_user), "%s", sshUserEdit->text().toUtf8().constData());
        profile.ssh_port = sshPortEdit->text().toInt();
        static const char *modes[] = {PATM_SSL_DISABLE, PATM_SSL_PREFER, PATM_SSL_REQUIRE};
        snprintf(profile.ssl_mode, sizeof(profile.ssl_mode), "%s", modes[sslCombo->currentIndex()]);

        PatmError err = patm_config_add(&m_cfg, &profile);
        if (!patm_is_ok(&err)) { testLabel->setText(err.msg); return; }
        err = patm_config_save(&m_cfg);
        if (!patm_is_ok(&err)) { patm_config_remove(&m_cfg, profile.id); testLabel->setText(err.msg); return; }

        QByteArray pw = pwEdit->text().toUtf8();
        if (!pw.isEmpty()) {
            patm_secure_store_password(profile.id, pw.constData());
            m_passwordCache[profile.id] = pwEdit->text();
        }

        refreshConnectionList();
        dlg.accept();
        setStatus(QString("Saved connection '%1'.").arg(profile.id));
    });

    connect(closeBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    dlg.exec();
}

void MainWindow::onConnect()
{
    QListWidgetItem *sel = m_connList->currentItem();
    if (!sel) { setStatus("Select a connection first."); return; }
    int index = m_connList->row(sel);
    if ((size_t)index >= m_cfg.nconns) return;

    const char *connId = m_cfg.conns[index].id;

    char password[512];
    PatmError err = patm_secure_fetch_password(connId, password, sizeof(password));

    if (!patm_is_ok(&err) && m_passwordCache.contains(connId)) {
        QByteArray cached = m_passwordCache.value(connId).toUtf8();
        snprintf(password, sizeof(password), "%s", cached.constData());
        err = patm_ok();
    }

    if (patm_is_ok(&err)) {
        PatmError cerr = tryConnectIndex(index, QString::fromUtf8(password));
        memset(password, 0, sizeof(password));
        if (patm_is_ok(&cerr)) return;
        setStatus(QString("Stored password rejected: %1").arg(cerr.msg));
    } else {
        setStatus(QString("No stored password for '%1'.").arg(connId));
    }
    showPasswordPrompt(index);
}

void MainWindow::onDisconnect()
{
    disconnectActive();
    setStatus("Disconnected.");
}

void MainWindow::onDeleteConnection()
{
    QListWidgetItem *sel = m_connList->currentItem();
    if (!sel) { setStatus("Select a connection first."); return; }
    int index = m_connList->row(sel);
    if ((size_t)index >= m_cfg.nconns) return;

    const char *id = m_cfg.conns[index].id;
    disconnectActive();
    PatmError err = patm_config_remove(&m_cfg, id);
    if (patm_is_ok(&err)) {
        patm_secure_delete_password(id);
        patm_config_save(&m_cfg);
        refreshConnectionList();
        setStatus(QString("Removed connection '%1'.").arg(id));
    } else {
        setStatus(err.msg);
    }
}

void MainWindow::onSqlQuery()
{
    if (!m_activeConn || !m_activeProfile) {
        setStatus("Connect to a database first.");
        return;
    }
    QueryWindow *qw = new QueryWindow(m_activeConn,
        patm_db_driver_get(m_activeProfile->engine)->display,
        QString(), QString::fromUtf8(m_activeProfile->id));
    int idx = addTab(qw, QString("SQL - %1").arg(patm_db_driver_get(m_activeProfile->engine)->display));
    m_tabWidget->setCurrentIndex(idx);
}

void MainWindow::onSqlTerminal()
{
    if (!m_activeConn || !m_activeProfile) {
        setStatus("Connect to a database first.");
        return;
    }
    SqlTerminal *st = new SqlTerminal(m_activeConn,
        QString::fromUtf8(m_activeProfile->id),
        QString::fromUtf8(m_activeProfile->id));
    int idx = addTab(st, QString("Terminal - %1").arg(m_activeProfile->id));
    m_tabWidget->setCurrentIndex(idx);
}

void MainWindow::onRunTool()
{
    ToolRunner *tr = new ToolRunner;
    int idx = addTab(tr, "Run Tool");
    m_tabWidget->setCurrentIndex(idx);
}

void MainWindow::onSettings()
{
    SettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::onTableDoubleClicked(QListWidgetItem *item)
{
    if (!m_activeConn || !m_activeProfile || !item) return;

    QByteArray tableBytes = item->text().toUtf8();
    char quoted[512];
    PatmError err = patm_db_driver_get(m_activeProfile->engine)->quote_ident(
        tableBytes.constData(), quoted, sizeof(quoted));
    if (!patm_is_ok(&err)) { setStatus(err.msg); return; }

    char sql[600];
    snprintf(sql, sizeof(sql), "SELECT * FROM %s LIMIT 200;", quoted);

    QueryWindow *qw = new QueryWindow(m_activeConn,
        patm_db_driver_get(m_activeProfile->engine)->display,
        QString::fromUtf8(sql), QString::fromUtf8(m_activeProfile->id));
    int idx = addTab(qw, QString("SQL - %1").arg(patm_db_driver_get(m_activeProfile->engine)->display));
    m_tabWidget->setCurrentIndex(idx);
}

int MainWindow::addTab(QWidget *page, const QString &title)
{
    m_welcomePage->setVisible(false);
    int idx = m_tabWidget->addTab(page, title);
    m_tabWidget->setTabToolTip(idx, title);
    saveSession();
    return idx;
}

void MainWindow::removeTab(QWidget *page)
{
    int idx = m_tabWidget->indexOf(page);
    if (idx >= 0) {
        m_tabWidget->removeTab(idx);
        page->deleteLater();
        saveSession();
        if (m_tabWidget->count() <= 1) {
            m_welcomePage->setVisible(true);
            m_tabWidget->setCurrentIndex(0);
        }
    }
}

void MainWindow::saveSession()
{
    PatmSession s = {};
    snprintf(s.last_conn, sizeof(s.last_conn), "%s", m_lastConnId);

    for (int i = 0; i < m_tabWidget->count() && s.n_tabs < PATM_SESSION_MAX_TABS; i++) {
        QWidget *w = m_tabWidget->widget(i);
        if (w == m_welcomePage) continue;

        PatmSessionTab *tab = &s.tabs[s.n_tabs];
        auto *qw = qobject_cast<QueryWindow *>(w);
        auto *st = qobject_cast<SqlTerminal *>(w);
        auto *tr = qobject_cast<ToolRunner *>(w);
        if (qw) {
            snprintf(tab->type, sizeof(tab->type), "query");
            QByteArray sql = qw->sql().toUtf8();
            snprintf(tab->sql, sizeof(tab->sql), "%s", sql.constData());
            QByteArray cid = qw->connId().toUtf8();
            snprintf(tab->conn_id, sizeof(tab->conn_id), "%s", cid.constData());
            s.n_tabs++;
        } else if (st) {
            snprintf(tab->type, sizeof(tab->type), "terminal");
            QByteArray cid = st->connId().toUtf8();
            snprintf(tab->conn_id, sizeof(tab->conn_id), "%s", cid.constData());
            s.n_tabs++;
        } else if (tr) {
            snprintf(tab->type, sizeof(tab->type), "tool");
            QByteArray tn = tr->toolName().toUtf8();
            snprintf(tab->tool, sizeof(tab->tool), "%s", tn.constData());
            QByteArray cid = tr->sourceConnectionId().toUtf8();
            snprintf(tab->conn_id, sizeof(tab->conn_id), "%s", cid.constData());
            s.n_tabs++;
        }
    }
    patm_session_save(&s);
}

void MainWindow::restoreSession()
{
    PatmSession s;
    PatmError load_err = patm_session_load(&s);
    if (!patm_is_ok(&load_err)) return;

    if (s.last_conn[0]) {
        const PatmConnProfile *prof = patm_config_find(&m_cfg, s.last_conn);
        if (prof) {
            /* Try keyring password first (silent). */
            char password[512];
            PatmError err = patm_secure_fetch_password(prof->id, password, sizeof(password));
            if (!patm_is_ok(&err)) {
                /* Keyring miss — check in-memory cache first */
                if (m_passwordCache.contains(prof->id)) {
                    QByteArray cached = m_passwordCache.value(prof->id).toUtf8();
                    snprintf(password, sizeof(password), "%s", cached.constData());
                    err = patm_ok();
                }
            }

            if (!patm_is_ok(&err)) {
                /* Still no password — prompt user */
                QDialog dlg(this);
                dlg.setWindowTitle(QString("Password for '%1'").arg(prof->id));
                dlg.setMinimumWidth(360);
                QFormLayout *fl = new QFormLayout(&dlg);
                QLabel *hint = new QLabel(QString("Enter password to reconnect to '%1':").arg(prof->id));
                fl->addRow(hint);
                QLineEdit *pwEdit = new QLineEdit;
                pwEdit->setEchoMode(QLineEdit::Password);
                fl->addRow("Password:", pwEdit);
                QHBoxLayout *btns = new QHBoxLayout;
                QPushButton *cancelBtn = new QPushButton("Skip");
                QPushButton *goBtn = new QPushButton("Connect");
                btns->addWidget(cancelBtn);
                btns->addWidget(goBtn);
                fl->addRow(btns);
                connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
                connect(goBtn, &QPushButton::clicked, &dlg, &QDialog::accept);

                if (dlg.exec() == QDialog::Accepted && !pwEdit->text().isEmpty()) {
                    QByteArray pw = pwEdit->text().toUtf8();
                    snprintf(password, sizeof(password), "%s", pw.constData());
                    patm_secure_store_password(prof->id, pw.constData());
                    m_passwordCache[prof->id] = pwEdit->text();
                    err = patm_ok();
                }
            }

            if (patm_is_ok(&err)) {
                PatmConnParams params = {
                    prof->engine, prof->host, prof->user, password,
                    prof->dbname,
                    prof->port > 0 ? prof->port : patm_db_driver_get(prof->engine)->default_port,
                    prof->ssl_mode[0] ? prof->ssl_mode : PATM_SSL_DISABLE,
                };
                err = patm_db_connect(&params, &m_activeConn);
                memset(password, 0, sizeof(password));
                if (patm_is_ok(&err)) {
                    m_activeProfile = prof;
                    snprintf(m_lastConnId, sizeof(m_lastConnId), "%s", prof->id);
                    refreshTableList();
                    setStatus(QString("Reconnected to %1.").arg(prof->id));
                } else {
                    setStatus(QString("Reconnect failed: %1").arg(err.msg));
                }
            }
        }
    }

    for (size_t i = 0; i < s.n_tabs; i++) {
        const PatmSessionTab *t = &s.tabs[i];

        if (!strcmp(t->type, "tool") && t->tool[0]) {
            auto *tr = new ToolRunner();
            tr->restoreState(QString::fromUtf8(t->tool), QString::fromUtf8(t->conn_id));
            int idx = addTab(tr, QString("Tool - %1").arg(QString::fromUtf8(t->tool)));
            m_tabWidget->setCurrentIndex(idx);
            continue;
        }

        if (!t->conn_id[0]) continue;

        const PatmConnProfile *prof = patm_config_find(&m_cfg, t->conn_id);
        if (!prof) continue;

        if (!strcmp(t->type, "query") && m_activeConn &&
            m_activeProfile && !strcmp(m_activeProfile->id, t->conn_id)) {
            auto *qw = new QueryWindow(m_activeConn,
                patm_db_driver_get(prof->engine)->display,
                t->sql[0] ? QString::fromUtf8(t->sql) : QString(),
                QString::fromUtf8(t->conn_id));
            int idx = addTab(qw, QString("SQL - %1").arg(patm_db_driver_get(prof->engine)->display));
            m_tabWidget->setCurrentIndex(idx);
        } else if (!strcmp(t->type, "terminal") && m_activeConn &&
                   m_activeProfile && !strcmp(m_activeProfile->id, t->conn_id)) {
            auto *st = new SqlTerminal(m_activeConn,
                QString::fromUtf8(prof->id), QString::fromUtf8(t->conn_id));
            int idx = addTab(st, QString("Terminal - %1").arg(prof->id));
            m_tabWidget->setCurrentIndex(idx);
        }
    }
}
