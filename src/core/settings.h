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

#ifndef PATM_SETTINGS_H
#define PATM_SETTINGS_H

#include "core/error.h"

/*
 * UI preferences (theme, icons). Saved to ~/.config/patm/ui.conf.
 */

#define PATM_THEME_VALUE_MAX 96

typedef struct {
    /* "classic-light", "classic-dark", "system", or installed theme name */
    char theme[PATM_THEME_VALUE_MAX];
    /* "system", "auto-legacy", or an installed icon theme name */
    char icon_theme[PATM_THEME_VALUE_MAX];
} PatmUiSettings;

void patm_ui_settings_defaults(PatmUiSettings *s);

PatmError patm_ui_settings_load(PatmUiSettings *s);
PatmError patm_ui_settings_save(const PatmUiSettings *s);

#endif
