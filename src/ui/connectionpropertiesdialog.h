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

#ifndef PATM_UI_CONNECTIONPROPERTIESDIALOG_H
#define PATM_UI_CONNECTIONPROPERTIESDIALOG_H

#include <QDialog>

class QComboBox;
class QLineEdit;
class QLabel;

class ConnectionPropertiesDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConnectionPropertiesDialog(const char *connId, QWidget *parent = nullptr);

private slots:
    void onTest();
    void onSave();

private:
    void loadProfile();

    QString m_originalId;
    QComboBox *m_engineCombo;
    QLineEdit *m_idEdit;
    QLineEdit *m_hostEdit;
    QLineEdit *m_portEdit;
    QLineEdit *m_userEdit;
    QLineEdit *m_dbEdit;
    QLineEdit *m_sshHostEdit;
    QLineEdit *m_sshUserEdit;
    QLineEdit *m_sshPortEdit;
    QLineEdit *m_pwEdit;
    QComboBox *m_sslCombo;
    QLabel *m_statusLabel;
};

#endif
