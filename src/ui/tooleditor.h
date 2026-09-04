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

#ifndef PATM_UI_TOOLEDITOR_H
#define PATM_UI_TOOLEDITOR_H

#include <QDialog>

class QPlainTextEdit;
class QLabel;

class ToolEditor : public QDialog
{
    Q_OBJECT
public:
    explicit ToolEditor(const QString &toolPath, QWidget *parent = nullptr);

private slots:
    void onSave();
    void onRevert();

private:
    void loadFile();

    QPlainTextEdit *m_editor;
    QLabel *m_status;
    QString m_originalPath;
    QString m_userPath;
};

#endif
