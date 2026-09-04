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

#ifndef PATM_UI_SETTINGSDIALOG_H
#define PATM_UI_SETTINGSDIALOG_H

#include <QDialog>

class QComboBox;

class SettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SettingsDialog(QWidget *parent = nullptr);

private slots:
    void onThemeChanged(int index);
    void onIconsChanged(int index);

private:
    void populateThemeList();
    void populateIconList();

    QComboBox *m_themeCombo;
    QComboBox *m_iconsCombo;
    QStringList m_themeNames;
    QStringList m_iconNames;
    bool m_updating = false;
};

#endif
