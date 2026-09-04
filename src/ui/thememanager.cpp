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

#include "thememanager.h"

#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QTextStream>

#include "c_backend.h"

static PatmUiSettings g_settings;

struct ThemeColors {
    const char *windowBg;
    const char *windowText;
    const char *base;
    const char *text;
    const char *button;
    const char *buttonText;
    const char *highlight;
    const char *highlightText;
    const char *light;
    const char *dark;
    const char *placeholder;
};

static const ThemeColors s_themes[] = {
    /* fusion-light */
    { "#eff0f1", "#232629", "#fcfcfc", "#232629", "#eff0f1", "#232629",
      "#308cc6", "#ffffff", "#ffffff", "#565b60", "#6e7175" },
    /* fusion-dark */
    { "#31363b", "#fcfcfc", "#232629", "#fcfcfc", "#31363b", "#fcfcfc",
      "#3daee9", "#fcfcfc", "#565b60", "#282d31", "#76797c" },
    /* breeze-light */
    { "#f7f7f7", "#232629", "#ffffff", "#232629", "#fbfbfc", "#232629",
      "#3daee9", "#fcfcfc", "#ffffff", "#e0e3e6", "#9aa1a8" },
    /* breeze-dark */
    { "#2a2e32", "#fcfcfc", "#1b1e20", "#fcfcfc", "#31363b", "#fcfcfc",
      "#3daee9", "#fcfcfc", "#464b50", "#26292c", "#66696d" },
    /* classic-light */
    { "#ecebe8", "#1a1a1a", "#ffffff", "#1a1a1a", "#ecebe8", "#1a1a1a",
      "#35507c", "#ffffff", "#ffffff", "#d6d4ce", "#8d8c88" },
    /* classic-dark */
    { "#2b2b28", "#e4e3df", "#232320", "#e4e3df", "#37362f", "#e4e3df",
      "#31517e", "#ffffff", "#6d6c66", "#1a1a1a", "#767571" },
    /* win9x */
    { "#c0c0c0", "#000000", "#ffffff", "#000000", "#c0c0c0", "#000000",
      "#000080", "#ffffff", "#dfdfdf", "#808080", "#808080" },
};

static const char *s_themeNames[] = {
    "fusion-light", "fusion-dark", "breeze-light", "breeze-dark",
    "classic-light", "classic-dark", "win9x"
};

static QPalette buildPalette(const ThemeColors &c)
{
    QColor windowBg(c.windowBg);
    QColor windowText(c.windowText);
    QColor base(c.base);
    QColor text(c.text);
    QColor button(c.button);
    QColor buttonText(c.buttonText);
    QColor highlight(c.highlight);
    QColor highlightText(c.highlightText);
    QColor light(c.light);
    QColor dark(c.dark);
    QColor placeholder(c.placeholder);

    QPalette pal;
    pal.setColor(QPalette::Window, windowBg);
    pal.setColor(QPalette::WindowText, windowText);
    pal.setColor(QPalette::Base, base);
    pal.setColor(QPalette::AlternateBase, windowBg);
    pal.setColor(QPalette::ToolTipBase, base);
    pal.setColor(QPalette::ToolTipText, text);
    pal.setColor(QPalette::Text, text);
    pal.setColor(QPalette::Button, button);
    pal.setColor(QPalette::ButtonText, buttonText);
    pal.setColor(QPalette::BrightText, Qt::red);
    pal.setColor(QPalette::Link, highlight);
    pal.setColor(QPalette::LinkVisited, highlightText);

    pal.setColor(QPalette::Light, light);
    pal.setColor(QPalette::Midlight, light.lighter(120));
    pal.setColor(QPalette::Dark, dark);
    pal.setColor(QPalette::Mid, dark);
    pal.setColor(QPalette::Shadow, dark.darker(150));

    pal.setColor(QPalette::PlaceholderText, placeholder);

    pal.setColor(QPalette::HighlightedText, highlightText);

    QPalette::ColorGroup groups[] = { QPalette::Active, QPalette::Disabled, QPalette::Inactive };
    for (auto group : groups) {
        if (group == QPalette::Disabled) {
            pal.setColor(group, QPalette::WindowText, dark);
            pal.setColor(group, QPalette::Text, dark);
            pal.setColor(group, QPalette::ButtonText, dark);
        } else {
            pal.setColor(group, QPalette::WindowText, windowText);
            pal.setColor(group, QPalette::Text, text);
            pal.setColor(group, QPalette::ButtonText, buttonText);
        }
        pal.setColor(group, QPalette::Window, windowBg);
        pal.setColor(group, QPalette::Base, base);
        pal.setColor(group, QPalette::Button, button);
        pal.setColor(group, QPalette::Highlight, highlight);
        pal.setColor(group, QPalette::HighlightedText, highlightText);
    }

    return pal;
}

void PatmThemeManager::apply(const PatmUiSettings &settings)
{
    patm_ui_settings_defaults(&g_settings);
    if (settings.theme[0])
        snprintf(g_settings.theme, sizeof(g_settings.theme), "%s", settings.theme);
    if (settings.icon_theme[0])
        snprintf(g_settings.icon_theme, sizeof(g_settings.icon_theme), "%s", settings.icon_theme);

    QString themeValue = QString::fromUtf8(g_settings.theme);

    if (themeValue == "system" || themeValue.isEmpty()) {
        qApp->setPalette(QPalette());
        qApp->setStyleSheet(QString());
        return;
    }

    // Find matching palette
    for (int i = 0; i < 7; i++) {
        if (themeValue == s_themeNames[i]) {
            qApp->setPalette(buildPalette(s_themes[i]));
            break;
        }
    }

    // Load QSS from resource
    QString resourcePath = QString(":/org/patm/ui/%1.qss").arg(themeValue);
    QFile f(resourcePath);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream ts(&f);
        qApp->setStyleSheet(ts.readAll());
    }
}

PatmUiSettings PatmThemeManager::current() { return g_settings; }

PatmError PatmThemeManager::setTheme(const char *theme, const char *iconTheme)
{
    PatmUiSettings next = g_settings;
    if (theme && theme[0])
        snprintf(next.theme, sizeof(next.theme), "%s", theme);
    if (iconTheme && iconTheme[0])
        snprintf(next.icon_theme, sizeof(next.icon_theme), "%s", iconTheme);

    if (!strcmp(next.theme, g_settings.theme) && !strcmp(next.icon_theme, g_settings.icon_theme))
        return patm_ok();

    apply(next);
    PatmError err = patm_ui_settings_save(&next);
    if (patm_is_ok(&err))
        g_settings = next;
    return err;
}
