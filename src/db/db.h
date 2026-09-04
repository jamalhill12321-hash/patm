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
#ifndef PATM_DB_H
#define PATM_DB_H

#include <stddef.h>

#include "core/error.h"
#include "core/strbuf.h"

/*
 * Database driver interface. Each engine gets its own file implementing
 * PatmDbDriver, registered in db_factory.c.
 *
 * Table/column names go through quote_ident, literals through quote_literal.
 * Always. No exceptions.
 */

typedef enum {
    PATM_DB_POSTGRESQL = 0,
    PATM_DB_MYSQL,
    PATM_DB_MARIADB,
    PATM_DB_ENGINE_COUNT
} PatmDbEngine;

typedef struct {
    PatmDbEngine engine;
    const char *host;
    const char *user;
    const char *password;
    const char *dbname;
    int port; /* 0 = engine default */
    /* "require" (default), "prefer", or "disable" */
    const char *ssl_mode;
} PatmConnParams;

#define PATM_SSL_REQUIRE "require"
#define PATM_SSL_PREFER "prefer"
#define PATM_SSL_DISABLE "disable"

typedef struct PatmConn PatmConn;

/* Result set of string cells; cell == NULL means SQL NULL. */
typedef struct {
    char **col_names; /* ncols entries, owned */
    char **cells;     /* nrows * ncols, row-major, owned */
    size_t nrows;
    size_t ncols;
} PatmResult;

typedef struct PatmDbDriver {
    PatmDbEngine engine;
    const char *name;      /* stable id, e.g. "postgresql" */
    const char *display;   /* human label, e.g. "PostgreSQL" */
    int default_port;
    PatmError (*connect)(const PatmConnParams *params, PatmConn **out);
    void (*close)(PatmConn *conn);
    PatmError (*query)(PatmConn *conn, const char *sql, PatmResult *out);
    PatmError (*execute)(PatmConn *conn, const char *sql);
/* quoted identifier into out buffer */
PatmError (*quote_ident)(const char *ident, char *out, size_t outsz);
    /* escaped literal appended to sb */
PatmError (*quote_literal)(PatmStrBuf *sb, const char *value);
    /* table names as JSON array appended to sb */
PatmError (*list_tables)(PatmConn *conn, PatmStrBuf *json_out);
    const char *(*last_error)(PatmConn *conn);
} PatmDbDriver;

const PatmDbDriver *patm_db_driver_get(PatmDbEngine engine);

/* wrappers that dispatch through the driver vtable */
PatmError patm_db_connect(const PatmConnParams *params, PatmConn **out);
void patm_db_close(PatmConn *conn);
PatmError patm_db_query(PatmConn *conn, const char *sql, PatmResult *out);
PatmError patm_db_execute(PatmConn *conn, const char *sql);
PatmError patm_db_list_tables(PatmConn *conn, PatmStrBuf *json_out);
const char *patm_db_last_error(PatmConn *conn);

/* free a query result (ok to call on a zeroed struct) */
void patm_db_result_free(PatmResult *res);

#endif
