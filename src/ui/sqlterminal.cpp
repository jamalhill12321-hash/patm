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

#include "sqlterminal.h"

#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextEdit>
#include <QVBoxLayout>

#define CELL_CLIP 48

SqlTerminal::SqlTerminal(PatmConn *conn, const QString &connLabel,
                         const QString &connId, QWidget *parent)
    : QWidget(parent), m_conn(conn), m_prompt(connLabel), m_connId(connId)
{
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 8, 8, 8);

    m_output = new QTextEdit;
    m_output->setReadOnly(true);
    m_output->setLineWrapMode(QTextEdit::NoWrap);
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    m_output->setFont(mono);
    vbox->addWidget(m_output, 1);

    m_entry = new QPlainTextEdit;
    m_entry->setMaximumHeight(90);
    m_entry->setFont(mono);
    m_entry->setTabChangesFocus(true);
    m_entry->installEventFilter(this);
    vbox->addWidget(m_entry, 0);

    append(QString("PATM SQL terminal (%1). Enter executes; Up/Down walks "
                   "history.\nType a statement and press Enter.\n\n")
               .arg(connLabel));
}

QString SqlTerminal::connId() const { return m_connId; }

bool SqlTerminal::eventFilter(QObject *obj, QEvent *event)
{
    if (obj != m_entry || event->type() != QEvent::KeyPress)
        return QWidget::eventFilter(obj, event);

    QKeyEvent *ke = static_cast<QKeyEvent *>(event);
    if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
        onExecute();
        return true;
    }
    if (ke->key() == Qt::Key_Up && !m_history.isEmpty()) {
        if (m_histPos < 0) m_histPos = m_history.size() - 1;
        else if (m_histPos > 0) m_histPos--;
        m_entry->setPlainText(m_history[m_histPos]);
        return true;
    }
    if (ke->key() == Qt::Key_Down && m_histPos >= 0) {
        m_histPos++;
        if (m_histPos >= m_history.size()) {
            m_histPos = -1;
            m_entry->setPlainText(QString());
        } else {
            m_entry->setPlainText(m_history[m_histPos]);
        }
        return true;
    }
    return false;
}

void SqlTerminal::append(const QString &text)
{
    m_output->moveCursor(QTextCursor::End);
    m_output->insertPlainText(text);
    QScrollBar *sb = m_output->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void SqlTerminal::historyPush(const QString &sql)
{
    if (sql.isEmpty()) return;
    if (!m_history.isEmpty() && m_history.last() == sql) return;
    if (m_history.size() >= 100) m_history.removeFirst();
    m_history.append(sql);
}

void SqlTerminal::onExecute()
{
    QString sql = m_entry->toPlainText().trimmed();
    if (sql.isEmpty()) return;

    append(QString("%1> %2\n").arg(m_prompt, sql));

    QByteArray sqlBytes = sql.toUtf8();
    PatmResult res = {};
    PatmError err = patm_db_query(m_conn, sqlBytes.constData(), &res);
    if (!patm_is_ok(&err)) {
        append(QString("ERROR: %1\n\n").arg(err.msg));
    } else if (res.ncols == 0 || res.cells == nullptr) {
        append(QString("OK (%1 row%2 affected/returned)\n\n")
                   .arg(res.nrows).arg(res.nrows == 1 ? "" : "s"));
    } else {
        appendResultTable(&res);
    }
    patm_db_result_free(&res);

    historyPush(sql);
    m_histPos = -1;
    m_entry->setPlainText(QString());
}

// TODO: maybe add tab completion someday
void SqlTerminal::appendResultTable(const PatmResult *res)
{
    QVector<size_t> widths(res->ncols, 0);
    for (size_t c = 0; c < res->ncols; c++) {
        widths[c] = res->col_names && res->col_names[c] ? strlen(res->col_names[c]) : 0;
        if (widths[c] > CELL_CLIP) widths[c] = CELL_CLIP;
    }
    size_t maxRows = res->nrows > 200 ? 200 : res->nrows;
    for (size_t c = 0; c < res->ncols; c++) {
        for (size_t r = 0; r < maxRows; r++) {
            const char *cell = res->cells[r * res->ncols + c];
            size_t len = cell ? strlen(cell) : 4;
            if (len > CELL_CLIP) len = CELL_CLIP;
            if (len > widths[c]) widths[c] = len;
        }
        if (widths[c] == 0) widths[c] = 1;
    }

    // separator
    QString sep = "-";
    for (size_t c = 0; c < res->ncols; c++) {
        sep += "+";
        for (size_t i = 0; i < widths[c] + 2; i++) sep += "-";
    }
    sep += "-\n";
    append(sep);

    QString hdr = "|";
    for (size_t c = 0; c < res->ncols; c++) {
        const char *name = res->col_names ? res->col_names[c] : "";
        hdr += QString(" %1 |").arg(QString::fromUtf8(name ? name : ""), -(int)widths[c]);
    }
    hdr += "\n";
    append(hdr);
    append(sep);

    for (size_t r = 0; r < maxRows; r++) {
        QString row = "|";
        for (size_t c = 0; c < res->ncols; c++) {
            const char *cell = res->cells[r * res->ncols + c];
            QString shown = cell ? QString::fromUtf8(cell) : "(null)";
            if (shown.size() > CELL_CLIP) shown = shown.left(CELL_CLIP);
            row += QString(" %1 |").arg(shown, -(int)widths[c]);
        }
        row += "\n";
        append(row);
    }
    if (maxRows < res->nrows)
        append(QString("(%1 more rows not shown)\n").arg(res->nrows - maxRows));
    append(QString("%1 row%2\n\n").arg(res->nrows).arg(res->nrows == 1 ? "" : "s"));
}
