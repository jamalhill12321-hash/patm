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
#include "db_internal.h"

#include <stdlib.h>
#include <string.h>

PatmError patm_db_conn_new(const PatmDbDriver *driver, void *impl,
                           PatmConn **out)
{
    PatmConn *conn;

    if (!driver || !impl || !out)
        return patm_error(PATM_ERR_INVALID_ARG, "conn_new: bad args");
    conn = calloc(1, sizeof(*conn));
    if (!conn)
        return patm_error(PATM_ERR_MEMORY, "connection allocation failed");
    conn->driver = driver;
    conn->impl = impl;
    *out = conn;
    return patm_ok();
}

PatmError patm_db_result_from_strings(PatmResult *res, size_t nrows,
                                      size_t ncols,
                                      char *const *cells_row_major)
{
    size_t total, i;

    memset(res, 0, sizeof(*res));
    if (nrows == 0 || ncols == 0) {
        res->nrows = nrows;
        res->ncols = ncols;
        return patm_ok();
    }

    total = nrows * ncols;
    res->cells = calloc(total, sizeof(char *));
    if (!res->cells)
        return patm_error(PATM_ERR_MEMORY, "result allocation failed");
    for (i = 0; i < total; i++) {
        const char *src = cells_row_major[i];
        if (!src)
            continue; /* SQL NULL stays NULL */
        res->cells[i] = strdup(src);
        if (!res->cells[i]) {
            res->nrows = nrows;
            res->ncols = ncols;
            patm_db_result_free(res);
            return patm_error(PATM_ERR_MEMORY, "cell copy failed");
        }
    }
    res->nrows = nrows;
    res->ncols = ncols;
    return patm_ok();
}
