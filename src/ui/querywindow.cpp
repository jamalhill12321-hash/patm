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

#include "querywindow.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QTextCursor>
#include <QVBoxLayout>

#include "mainwindow.h"
#include "resultgrid.h"

QueryWindow::QueryWindow(PatmConn *conn, const QString &engineDisplay,
                         const QString &prefillSql, const QString &connId,
                         QWidget *parent)
    : QWidget(parent), m_conn(conn), m_engine(engineDisplay), m_connId(connId)
{
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);

    m_sqlEdit = new QPlainTextEdit;
    m_sqlEdit->setMinimumHeight(110);
    m_sqlEdit->setTabStopDistance(QFontMetricsF(m_sqlEdit->font()).horizontalAdvance(' ') * 4);
    if (!prefillSql.isEmpty())
        m_sqlEdit->setPlainText(prefillSql);
    m_sqlEdit->setFocus();
    vbox->addWidget(m_sqlEdit, 0);

    QHBoxLayout *hbox = new QHBoxLayout;
    QPushButton *runBtn = new QPushButton("Execute (Ctrl+Enter)");
    QPushButton *loadBtn = new QPushButton("Load SQL File");
    QPushButton *saveBtn = new QPushButton("Save to SQL File");
    m_status = new QLabel("Ready.");
    m_status->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_status->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    hbox->addWidget(runBtn);
    hbox->addWidget(loadBtn);
    hbox->addWidget(saveBtn);
    hbox->addWidget(m_status, 1);
    vbox->addLayout(hbox);

    m_resultContainer = new QWidget;
    QVBoxLayout *rbox = new QVBoxLayout(m_resultContainer);
    rbox->setContentsMargins(0, 0, 0, 0);
    vbox->addWidget(m_resultContainer, 1);

    connect(runBtn, &QPushButton::clicked, this, &QueryWindow::onExecute);
    connect(loadBtn, &QPushButton::clicked, this, &QueryWindow::onLoadSql);
    connect(saveBtn, &QPushButton::clicked, this, &QueryWindow::onSaveSql);

    QShortcut *execShortcut = new QShortcut(QKeySequence(Qt::CTRL | Qt::Key_Return), this);
    connect(execShortcut, &QShortcut::activated, this, &QueryWindow::onExecute);
}

QString QueryWindow::sql() const { return m_sqlEdit->toPlainText(); }
QString QueryWindow::connId() const { return m_connId; }

void QueryWindow::onExecute()
{
    QString sqlText;
    QTextCursor cursor = m_sqlEdit->textCursor();
    if (cursor.hasSelection()) {
        sqlText = cursor.selectedText();
        m_status->setText("Executing selection...");
    } else {
        sqlText = m_sqlEdit->toPlainText();
        m_status->setText("Executing all...");
    }
    sqlText = sqlText.trimmed();
    if (sqlText.isEmpty()) {
        m_status->setText("Nothing to execute.");
        return;
    }

    QByteArray sqlBytes = sqlText.toUtf8();
    PatmResult res = {};
    PatmError err = patm_db_query(m_conn, sqlBytes.constData(), &res);
    if (!patm_is_ok(&err)) {
        m_status->setText(err.msg);
        return;
    }

    QLayoutItem *child;
    while ((child = m_resultContainer->layout()->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    ResultGrid *grid = new ResultGrid;
    QString status;
    grid->fill(&res, &status);
    patm_db_result_free(&res);
    m_resultContainer->layout()->addWidget(grid);
    m_status->setText(status);
}

void QueryWindow::onLoadSql()
{
    QString path = QFileDialog::getOpenFileName(this, "Load SQL File",
        QString(), "SQL files (*.sql);;All files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_status->setText("Failed to open file.");
        return;
    }
    QByteArray content = file.readAll();
    file.close();
    m_sqlEdit->setPlainText(QString::fromUtf8(content));
    m_status->setText(QString("Loaded %1.").arg(path));
}

void QueryWindow::onSaveSql()
{
    QString sqlText = m_sqlEdit->toPlainText();
    if (sqlText.trimmed().isEmpty()) {
        m_status->setText("Nothing to save.");
        return;
    }

    QString path = QFileDialog::getSaveFileName(this, "Save SQL File",
        QString(), "SQL files (*.sql);;All files (*)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        m_status->setText("Failed to open file for writing.");
        return;
    }
    file.write(sqlText.toUtf8());
    file.close();
    m_status->setText(QString("Saved to %1.").arg(path));
}
