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
#ifndef PATM_DB_INTERNAL_H
#define PATM_DB_INTERNAL_H

/*
 * Internal helpers for drivers. Not public API.
 */

#include <stddef.h>

#include "core/error.h"
#include "db.h"

struct PatmConn {
    const PatmDbDriver *driver;
    void *impl;
};

/* wrap a driver connection in a PatmConn */
PatmError patm_db_conn_new(const PatmDbDriver *driver, void *impl,
                           PatmConn **out);

/* populate result from a flat array of strings (copies everything) */
PatmError patm_db_result_from_strings(PatmResult *res, size_t nrows,
                                      size_t ncols,
                                      char *const *cells_row_major);

#endif
