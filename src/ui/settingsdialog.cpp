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

#include "settingsdialog.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>

#include "c_backend.h"
#include "thememanager.h"

struct PatmBuiltinThemeInfo {
    const char *value;
    const char *display;
};
static const PatmBuiltinThemeInfo s_builtins[] = {
    { "system",       "System (use desktop theme)" },
    { "fusion-light", "Fusion Light" },
    { "fusion-dark",  "Fusion Dark" },
    { "breeze-light", "Breeze Light" },
    { "breeze-dark",  "Breeze Dark" },
    { "classic-light", "Classic Light" },
    { "classic-dark",  "Classic Dark" },
    { "win9x",        "Windows 9x" },
};
static constexpr size_t s_nBuiltins = sizeof(s_builtins) / sizeof(s_builtins[0]);

SettingsDialog::SettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle("Appearance");
    setMinimumWidth(400);

    QFormLayout *fl = new QFormLayout(this);

    m_themeCombo = new QComboBox;
    m_iconsCombo = new QComboBox;

    populateThemeList();
    populateIconList();

    fl->addRow("Theme:", m_themeCombo);
    fl->addRow("Icons:", m_iconsCombo);

    QLabel *hint = new QLabel("Changes apply immediately and are remembered.");
    hint->setAlignment(Qt::AlignLeft);
    fl->addRow(hint);

    QPushButton *closeBtn = new QPushButton("Close");
    fl->addRow("", closeBtn);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

    connect(m_themeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onThemeChanged);
    connect(m_iconsCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SettingsDialog::onIconsChanged);
}

void SettingsDialog::populateThemeList()
{
    m_updating = true;

    PatmUiSettings cur = PatmThemeManager::current();

    for (size_t i = 0; i < s_nBuiltins; i++)
        m_themeCombo->addItem(QString::fromUtf8(s_builtins[i].display));

    m_themeCombo->addItem("System");

    int sel = 0;
    for (size_t i = 0; i < s_nBuiltins; i++) {
        if (!strcmp(s_builtins[i].value, cur.theme)) {
            sel = (int)i;
            break;
        }
    }
    if (!strcmp(cur.theme, "system") || !cur.theme[0])
        sel = (int)s_nBuiltins;

    m_themeCombo->setCurrentIndex(sel);
    m_updating = false;
}

void SettingsDialog::populateIconList()
{
    m_updating = true;

    PatmUiSettings cur = PatmThemeManager::current();

    m_iconsCombo->addItems({"Legacy (auto)", "System default"});

    int sel = 0;
    if (!strcmp(cur.icon_theme, "auto-legacy")) sel = 0;
    else if (!strcmp(cur.icon_theme, "system") || !cur.icon_theme[0]) sel = 1;

    m_iconsCombo->setCurrentIndex(sel);
    m_updating = false;
}

void SettingsDialog::onThemeChanged(int index)
{
    if (m_updating) return;
    if (index < 0 || index >= m_themeCombo->count()) return;

    if ((size_t)index < s_nBuiltins) {
        PatmThemeManager::setTheme(s_builtins[index].value, nullptr);
    } else {
        PatmThemeManager::setTheme("system", nullptr);
    }
}

void SettingsDialog::onIconsChanged(int index)
{
    if (m_updating) return;
    if (index < 0 || index >= m_iconsCombo->count()) return;

    const char *value = nullptr;
    if (index == 0) value = "auto-legacy";
    else if (index == 1) value = "system";

    if (value) PatmThemeManager::setTheme(nullptr, value);
}
