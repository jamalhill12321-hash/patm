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

#include "connectionpropertiesdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include "c_backend.h"
#include "mainwindow.h"

ConnectionPropertiesDialog::ConnectionPropertiesDialog(const char *connId, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Connection Properties");
    setMinimumWidth(460);

    m_originalId = QString::fromUtf8(connId);

    QFormLayout *fl = new QFormLayout(this);

    m_engineCombo = new QComboBox;
    m_engineCombo->addItems({"PostgreSQL", "MySQL", "MariaDB"});
    fl->addRow("Engine:", m_engineCombo);

    m_idEdit = new QLineEdit;
    fl->addRow("Name:", m_idEdit);

    m_hostEdit = new QLineEdit;
    fl->addRow("Host:", m_hostEdit);

    m_portEdit = new QLineEdit;
    fl->addRow("Port:", m_portEdit);

    m_userEdit = new QLineEdit;
    fl->addRow("User:", m_userEdit);

    m_dbEdit = new QLineEdit;
    fl->addRow("Database:", m_dbEdit);

    m_sshHostEdit = new QLineEdit;
    fl->addRow("SSH host:", m_sshHostEdit);

    m_sshUserEdit = new QLineEdit;
    fl->addRow("SSH user:", m_sshUserEdit);

    m_sshPortEdit = new QLineEdit;
    fl->addRow("SSH port:", m_sshPortEdit);

    m_pwEdit = new QLineEdit;
    m_pwEdit->setEchoMode(QLineEdit::Password);
    m_pwEdit->setPlaceholderText("(leave empty to keep current)");
    fl->addRow("Password:", m_pwEdit);

    m_sslCombo = new QComboBox;
    m_sslCombo->addItems({"Disabled (default)", "Preferred", "Required"});
    fl->addRow("TLS:", m_sslCombo);

    m_statusLabel = new QLabel;
    fl->addRow(m_statusLabel);

    QHBoxLayout *btnRow = new QHBoxLayout;
    QPushButton *testBtn = new QPushButton("Test Connection");
    QPushButton *deleteBtn = new QPushButton("Delete");
    btnRow->addWidget(testBtn);
    btnRow->addStretch();
    btnRow->addWidget(deleteBtn);
    fl->addRow(btnRow);

    QDialogButtonBox *bbox = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    fl->addRow(bbox);

    connect(testBtn, &QPushButton::clicked, this, &ConnectionPropertiesDialog::onTest);
    connect(deleteBtn, &QPushButton::clicked, this, [this]() {
        const PatmConfig *cfg = MainWindow::config();
        const PatmConnProfile *prof = patm_config_find(cfg, m_originalId.toUtf8().constData());
        if (!prof) return;

        MainWindow *win = MainWindow::instance();
        if (win) win->disconnectActive();

        PatmError err = patm_config_remove(
            const_cast<PatmConfig *>(cfg), m_originalId.toUtf8().constData());
        if (patm_is_ok(&err)) {
            patm_secure_delete_password(m_originalId.toUtf8().constData());
            const_cast<PatmConfig *>(cfg)->nconns--;
            patm_config_save(cfg);
            QMetaObject::invokeMethod(win, "refreshConnectionList", Qt::QueuedConnection);
            accept();
        } else {
            m_statusLabel->setText(err.msg);
        }
    });

    connect(bbox, &QDialogButtonBox::accepted, this, &ConnectionPropertiesDialog::onSave);
    connect(bbox, &QDialogButtonBox::rejected, this, &QDialog::reject);

    loadProfile();
}

void ConnectionPropertiesDialog::loadProfile()
{
    const PatmConfig *cfg = MainWindow::config();
    const PatmConnProfile *prof = patm_config_find(cfg, m_originalId.toUtf8().constData());
    if (!prof) {
        m_statusLabel->setText("Connection not found.");
        return;
    }

    m_engineCombo->setCurrentIndex((int)prof->engine);
    m_idEdit->setText(QString::fromUtf8(prof->id));
    m_hostEdit->setText(QString::fromUtf8(prof->host));
    m_portEdit->setText(prof->port > 0 ? QString::number(prof->port) : QString());
    m_userEdit->setText(QString::fromUtf8(prof->user));
    m_dbEdit->setText(QString::fromUtf8(prof->dbname));
    m_sshHostEdit->setText(QString::fromUtf8(prof->ssh_host));
    m_sshUserEdit->setText(QString::fromUtf8(prof->ssh_user));
    m_sshPortEdit->setText(prof->ssh_port > 0 ? QString::number(prof->ssh_port) : "22");

    if (!strcmp(prof->ssl_mode, PATM_SSL_PREFER))
        m_sslCombo->setCurrentIndex(1);
    else if (!strcmp(prof->ssl_mode, PATM_SSL_REQUIRE))
        m_sslCombo->setCurrentIndex(2);
    else
        m_sslCombo->setCurrentIndex(0);
}

void ConnectionPropertiesDialog::onTest()
{
    PatmConnProfile probe = {};
    snprintf(probe.id, sizeof(probe.id), "%s", m_idEdit->text().toUtf8().constData());
    probe.engine = (PatmDbEngine)m_engineCombo->currentIndex();
    snprintf(probe.host, sizeof(probe.host), "%s", m_hostEdit->text().toUtf8().constData());
    probe.port = m_portEdit->text().toInt();
    snprintf(probe.user, sizeof(probe.user), "%s", m_userEdit->text().toUtf8().constData());
    snprintf(probe.dbname, sizeof(probe.dbname), "%s", m_dbEdit->text().toUtf8().constData());
    snprintf(probe.ssh_host, sizeof(probe.ssh_host), "%s", m_sshHostEdit->text().toUtf8().constData());
    snprintf(probe.ssh_user, sizeof(probe.ssh_user), "%s", m_sshUserEdit->text().toUtf8().constData());
    probe.ssh_port = m_sshPortEdit->text().toInt();
    static const char *modes[] = {PATM_SSL_DISABLE, PATM_SSL_PREFER, PATM_SSL_REQUIRE};
    snprintf(probe.ssl_mode, sizeof(probe.ssl_mode), "%s", modes[m_sslCombo->currentIndex()]);

    QByteArray pw;
    if (!m_pwEdit->text().isEmpty()) {
        pw = m_pwEdit->text().toUtf8();
    } else {
        char stored[512];
        PatmError err = patm_secure_fetch_password(probe.id, stored, sizeof(stored));
        if (patm_is_ok(&err)) {
            pw = QByteArray(stored);
            memset(stored, 0, sizeof(stored));
        } else {
            m_statusLabel->setText("No stored password. Enter a password to test.");
            return;
        }
    }

    PatmConn *conn = nullptr;
    PatmConnParams params = {
        probe.engine, probe.host, probe.user, pw.constData(),
        probe.dbname, probe.port, probe.ssl_mode,
    };
    PatmError err = patm_db_connect(&params, &conn);
    if (patm_is_ok(&err)) {
        m_statusLabel->setText("Connection OK.");
        patm_db_close(conn);
    } else {
        m_statusLabel->setText(err.msg);
    }
}

void ConnectionPropertiesDialog::onSave()
{
    PatmConnProfile profile = {};
    snprintf(profile.id, sizeof(profile.id), "%s", m_idEdit->text().toUtf8().constData());
    profile.engine = (PatmDbEngine)m_engineCombo->currentIndex();
    snprintf(profile.host, sizeof(profile.host), "%s", m_hostEdit->text().toUtf8().constData());
    profile.port = m_portEdit->text().toInt();
    snprintf(profile.user, sizeof(profile.user), "%s", m_userEdit->text().toUtf8().constData());
    snprintf(profile.dbname, sizeof(profile.dbname), "%s", m_dbEdit->text().toUtf8().constData());
    snprintf(profile.ssh_host, sizeof(profile.ssh_host), "%s", m_sshHostEdit->text().toUtf8().constData());
    snprintf(profile.ssh_user, sizeof(profile.ssh_user), "%s", m_sshUserEdit->text().toUtf8().constData());
    profile.ssh_port = m_sshPortEdit->text().toInt();
    static const char *modes[] = {PATM_SSL_DISABLE, PATM_SSL_PREFER, PATM_SSL_REQUIRE};
    snprintf(profile.ssl_mode, sizeof(profile.ssl_mode), "%s", modes[m_sslCombo->currentIndex()]);

    PatmConfig *cfg = const_cast<PatmConfig *>(MainWindow::config());

    /* If the id changed, remove the old entry first */
    if (m_originalId != QString::fromUtf8(profile.id)) {
        patm_config_remove(cfg, m_originalId.toUtf8().constData());
        patm_secure_delete_password(m_originalId.toUtf8().constData());
    }

    const PatmConnProfile *existing = patm_config_find(cfg, profile.id);
    if (existing) {
        PatmError err = patm_config_update(cfg, profile.id, &profile);
        if (!patm_is_ok(&err)) { m_statusLabel->setText(err.msg); return; }
    } else {
        PatmError err = patm_config_add(cfg, &profile);
        if (!patm_is_ok(&err)) { m_statusLabel->setText(err.msg); return; }
    }

    PatmError err = patm_config_save(cfg);
    if (!patm_is_ok(&err)) { m_statusLabel->setText(err.msg); return; }

    if (!m_pwEdit->text().isEmpty())
        patm_secure_store_password(profile.id, m_pwEdit->text().toUtf8().constData());

    MainWindow *win = MainWindow::instance();
    if (win) {
        win->disconnectActive();
        QMetaObject::invokeMethod(win, "refreshConnectionList", Qt::QueuedConnection);
    }

    accept();
}
