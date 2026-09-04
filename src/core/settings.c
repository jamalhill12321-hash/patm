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

#include "settings.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static PatmError settings_path(char *out, size_t outsz)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (xdg && xdg[0])
        snprintf(out, outsz, "%s/patm/ui.conf", xdg);
    else if (home && home[0])
        snprintf(out, outsz, "%s/.config/patm/ui.conf", home);
    else
        return patm_error(PATM_ERR_IO, "no config directory available");

    char dir[512];
    snprintf(dir, sizeof(dir), "%s", out);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dir, 0700);
    }
    return patm_ok();
}

void patm_ui_settings_defaults(PatmUiSettings *s)
{
    memset(s, 0, sizeof(*s));
    /* System theme by default — respect the user's desktop. */
    snprintf(s->theme, sizeof(s->theme), "system");
    snprintf(s->icon_theme, sizeof(s->icon_theme), "auto-legacy");
}

PatmError patm_ui_settings_load(PatmUiSettings *s)
{
    char path[512];
    FILE *f;
    char line[512];

    patm_ui_settings_defaults(s);

    PatmError err = settings_path(path, sizeof(path));
    if (!patm_is_ok(&err))
        return err;

    f = fopen(path, "r");
    if (!f)
        return patm_ok(); /* first run */

    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '#' || line[0] == '\0')
            continue;
        char *eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;
        if (!strcmp(key, "theme"))
            snprintf(s->theme, sizeof(s->theme), "%s", val);
        else if (!strcmp(key, "icon_theme"))
            snprintf(s->icon_theme, sizeof(s->icon_theme), "%s", val);
    }
    fclose(f);
    return patm_ok();
}

PatmError patm_ui_settings_save(const PatmUiSettings *s)
{
    char path[512];
    char tmp_path[520];
    FILE *f;

    PatmError err = settings_path(path, sizeof(path));
    if (!patm_is_ok(&err))
        return err;
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    f = fopen(tmp_path, "w");
    if (!f)
        return patm_error(PATM_ERR_IO, "cannot open '%s' for writing",
                          tmp_path);
    fprintf(f, "# PATM UI preferences\n");
    fprintf(f, "theme=%s\n", s->theme);
    fprintf(f, "icon_theme=%s\n", s->icon_theme);
    if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
        fclose(f);
        remove(tmp_path);
        return patm_error(PATM_ERR_IO, "failed to flush ui.conf");
    }
    fclose(f);
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return patm_error(PATM_ERR_IO, "cannot replace '%s'", path);
    }
    return patm_ok();
}
