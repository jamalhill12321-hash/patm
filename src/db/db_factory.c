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
#include "db.h"

/*
 * Driver registry. Add one line here when a new engine driver is added.
 */

extern const PatmDbDriver *patm_db_driver_pg(void);
extern const PatmDbDriver *patm_db_driver_mysql(void);
extern const PatmDbDriver *patm_db_driver_mariadb(void);

static const PatmDbDriver *g_registry[PATM_DB_ENGINE_COUNT];
static int g_initialized = 0;

static void ensure_init(void)
{
    if (g_initialized)
        return;
    g_registry[PATM_DB_POSTGRESQL] = patm_db_driver_pg();
    g_registry[PATM_DB_MYSQL] = patm_db_driver_mysql();
    g_registry[PATM_DB_MARIADB] = patm_db_driver_mariadb();
    g_initialized = 1;
}

const PatmDbDriver *patm_db_driver_get(PatmDbEngine engine)
{
    ensure_init();
    if ((int)engine < 0 || engine >= PATM_DB_ENGINE_COUNT)
        return NULL;
    return g_registry[engine];
}
