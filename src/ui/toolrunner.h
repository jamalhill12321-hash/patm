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

#ifndef PATM_UI_TOOLRUNNER_H
#define PATM_UI_TOOLRUNNER_H

#include <QWidget>

#include "c_backend.h"

class QComboBox;
class QFrame;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QFormLayout;

class ToolRunner : public QWidget
{
    Q_OBJECT
public:
    explicit ToolRunner(QWidget *parent = nullptr);

    QString sourceConnectionId() const;
    QString toolName() const;
    void restoreState(const QString &toolName, const QString &connId);

private slots:
    void onToolChanged(int index);
    void onSourceChanged(int index);
    void onRun();
    void onEditTool();
    void onBrowseOutput();

private:
    void rebuildParams();
    void clearParamGrid();
    void appendLog(const QString &text);
    void refreshTableList();

    struct ToolParam {
        QString key;
        QString label;
        QString defval;
    };

    struct ToolMeta {
        QString name;
        QString desc;
        QList<ToolParam> params;
    };

    QComboBox *m_toolCombo;
    QComboBox *m_sourceCombo;
    QComboBox *m_tableCombo = nullptr;
    QWidget *m_paramWidget;
    QFormLayout *m_paramLayout;
    QLineEdit *m_paramEntries[8] = {};

    QLineEdit *m_tgtHost;
    QLineEdit *m_tgtPort;
    QLineEdit *m_tgtUser;
    QLineEdit *m_tgtPass;
    QLineEdit *m_tgtDb;
    QComboBox *m_tgtEngine;

    QLabel *m_status;
    QPlainTextEdit *m_logView;

    QStringList m_toolPaths;
    QList<ToolMeta> m_knownTools;
    const PatmConfig *m_cfg;
};

#endif
