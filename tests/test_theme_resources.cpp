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

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include <QApplication>
#include <QFile>

/*
 * Regression test: every bundled QSS theme must exist in the Qt
 * resource system and contain a valid palette block.
 */

static const char *const builtin_values[] = {
    "fusion-light",
    "fusion-dark",
    "breeze-light",
    "breeze-dark",
    "classic-light",
    "classic-dark",
    "win9x",
};
static const size_t n_builtins = sizeof(builtin_values) / sizeof(builtin_values[0]);

static void test_bundled_qss_present(void)
{
    for (size_t i = 0; i < n_builtins; i++) {
        char path[256];
        snprintf(path, sizeof(path), ":/org/patm/ui/%s.qss", builtin_values[i]);

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            fprintf(stderr, "FATAL: bundled theme '%s' missing resource %s\n",
                    builtin_values[i], path);
            exit(1);
        }
        QByteArray data = f.readAll();
        if (data.size() < 32) {
            fprintf(stderr, "FATAL: resource %s empty\n", path);
            exit(1);
        }
        if (strncmp(data.constData(), "/*", 2) != 0) {
            fprintf(stderr, "FATAL: resource %s is not QSS text\n", path);
            exit(1);
        }
        /* Every QSS theme should reference color values */
        if (!data.contains('#')) {
            fprintf(stderr, "FATAL: resource %s has no color palette\n", path);
            exit(1);
        }
    }
}

static void test_builtin_values_unique(void)
{
    for (size_t i = 0; i < n_builtins; i++) {
        for (size_t j = i + 1; j < n_builtins; j++) {
            if (!strcmp(builtin_values[i], builtin_values[j])) {
                fprintf(stderr, "FATAL: duplicate builtin value '%s'\n",
                        builtin_values[i]);
                exit(1);
            }
        }
    }
}

static void test_default_is_fusion_light(void)
{
    if (strcmp(builtin_values[0], "fusion-light") != 0) {
        fprintf(stderr, "FATAL: first builtin is '%s', expected "
                        "fusion-light (the documented default)\n",
                builtin_values[0]);
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);
    test_bundled_qss_present();
    test_builtin_values_unique();
    test_default_is_fusion_light();
    puts("theme resources tests passed");
    return 0;
}
