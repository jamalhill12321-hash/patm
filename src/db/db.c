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

#include <stdlib.h>
#include <string.h>

#include "db_internal.h"

/* dispatch through driver vtable */

PatmError patm_db_connect(const PatmConnParams *params, PatmConn **out)
{
    const PatmDbDriver *drv;

    if (!params || !out)
        return patm_error(PATM_ERR_INVALID_ARG, "connect: bad arguments");
    drv = patm_db_driver_get(params->engine);
    if (!drv)
        return patm_error(PATM_ERR_INVALID_ARG, "unknown database engine %d",
                          (int)params->engine);
    return drv->connect(params, out);
}

void patm_db_close(PatmConn *conn)
{
    if (!conn)
        return;
    conn->driver->close(conn);
    free(conn);
}

PatmError patm_db_query(PatmConn *conn, const char *sql, PatmResult *out)
{
    if (!conn || !sql || !out)
        return patm_error(PATM_ERR_INVALID_ARG, "query: bad arguments");
    memset(out, 0, sizeof(*out));
    return conn->driver->query(conn, sql, out);
}

PatmError patm_db_execute(PatmConn *conn, const char *sql)
{
    if (!conn || !sql)
        return patm_error(PATM_ERR_INVALID_ARG, "execute: bad arguments");
    return conn->driver->execute(conn, sql);
}

PatmError patm_db_list_tables(PatmConn *conn, PatmStrBuf *json_out)
{
    if (!conn || !json_out)
        return patm_error(PATM_ERR_INVALID_ARG, "list_tables: bad args");
    return conn->driver->list_tables(conn, json_out);
}

const char *patm_db_last_error(PatmConn *conn)
{
    if (!conn)
        return "no connection";
    return conn->driver->last_error(conn);
}

void patm_db_result_free(PatmResult *res)
{
    size_t i;

    if (!res)
        return;
    if (res->col_names) {
        for (i = 0; i < res->ncols; i++)
            free(res->col_names[i]);
        free(res->col_names);
        res->col_names = NULL;
    }
    if (!res->cells) {
        res->nrows = 0;
        res->ncols = 0;
        return;
    }
    for (i = 0; i < res->nrows * res->ncols; i++)
        free(res->cells[i]);
    free(res->cells);
    res->cells = NULL;
    res->nrows = 0;
    res->ncols = 0;
}
