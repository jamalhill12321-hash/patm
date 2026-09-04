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

#include "resultgrid.h"

#include <QHeaderView>
#include <QTableWidget>
#include <QVBoxLayout>

ResultGrid::ResultGrid(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(0, 0, 0, 0);
    m_table = new QTableWidget;
    m_table->setEditTriggers(QTableWidget::NoEditTriggers);
    m_table->setSelectionBehavior(QTableWidget::SelectRows);
    m_table->setSelectionMode(QTableWidget::SingleSelection);
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->verticalHeader()->setVisible(false);
    vbox->addWidget(m_table);
}

void ResultGrid::fill(const PatmResult *res, QString *statusOut)
{
    m_table->clearContents();
    m_table->setRowCount(0);
    m_table->setColumnCount(0);

    if (!res || res->ncols == 0) {
        if (statusOut) *statusOut = "OK (no result set)";
        return;
    }

    size_t nCols = res->ncols > 64 ? 64 : res->ncols;
    bool truncatedCols = res->ncols > 64;
    size_t shown = res->nrows > PATM_RESULT_GRID_MAX_ROWS ? PATM_RESULT_GRID_MAX_ROWS : res->nrows;
    bool truncatedRows = res->nrows > PATM_RESULT_GRID_MAX_ROWS;

    m_table->setColumnCount((int)nCols);
    QStringList headers;
    for (size_t c = 0; c < nCols; c++)
        headers << QString::fromUtf8(res->col_names && res->col_names[c] ? res->col_names[c] : "");
    m_table->setHorizontalHeaderLabels(headers);

    m_table->setRowCount((int)shown);
    for (size_t r = 0; r < shown; r++) {
        for (size_t c = 0; c < nCols; c++) {
            const char *cell = res->cells[r * nCols + c];
            m_table->setItem((int)r, (int)c, new QTableWidgetItem(cell ? QString::fromUtf8(cell) : "(null)"));
        }
    }

    if (statusOut) {
        QString summary = QString("%1 row%2%3%4")
                              .arg(res->nrows)
                              .arg(res->nrows == 1 ? "" : "s")
                              .arg(truncatedRows ? " (truncated)" : "")
                              .arg(truncatedCols ? " (columns truncated)" : "");
        *statusOut = summary;
    }
}
