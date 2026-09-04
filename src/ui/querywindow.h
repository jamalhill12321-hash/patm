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

#ifndef PATM_UI_QUERYWINDOW_H
#define PATM_UI_QUERYWINDOW_H

#include <QWidget>

#include "c_backend.h"

class QPlainTextEdit;
class QLabel;

class QueryWindow : public QWidget
{
    Q_OBJECT
public:
    explicit QueryWindow(PatmConn *conn, const QString &engineDisplay,
                         const QString &prefillSql,
                         const QString &connId,
                         QWidget *parent = nullptr);

    QString sql() const;
    QString connId() const;

private slots:
    void onExecute();
    void onLoadSql();
    void onSaveSql();

private:
    QPlainTextEdit *m_sqlEdit;
    QLabel *m_status;
    QWidget *m_resultContainer;
    PatmConn *m_conn;
    QString m_engine;
    QString m_connId;
};

#endif
