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

#ifndef PATM_UI_SQLTERMINAL_H
#define PATM_UI_SQLTERMINAL_H

#include <QWidget>

#include "c_backend.h"

class QTextEdit;
class QPlainTextEdit;

class SqlTerminal : public QWidget
{
    Q_OBJECT
public:
    explicit SqlTerminal(PatmConn *conn, const QString &connLabel,
                         const QString &connId,
                         QWidget *parent = nullptr);

    QString connId() const;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onExecute();

private:
    void append(const QString &text);
    void appendResultTable(const PatmResult *res);
    void historyPush(const QString &sql);

    QTextEdit *m_output;
    QPlainTextEdit *m_entry;
    PatmConn *m_conn;
    QString m_prompt;
    QStringList m_history;
    int m_histPos = -1;
    QString m_connId;
};

#endif
