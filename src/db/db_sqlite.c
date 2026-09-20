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
#include <sqlite3.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/log.h"
#include "db_internal.h"

/*
 * SQLite driver. The database parameter in PatmConnParams.dbname is treated
 * as a file path. Host, user, password, port, and ssl_mode are ignored.
 */

typedef struct {
    sqlite3 *db;
    char last_error[512];
} SqliteConn;

static const PatmDbDriver sqlite_driver;

static PatmError sqlite_connect(const PatmConnParams *params, PatmConn **out)
{
    SqliteConn *c;
    int rc;
    PatmError err;

    if (!params->dbname || !params->dbname[0])
        return patm_error(PATM_ERR_INVALID_ARG,
                          "SQLite: database file path is required");

    c = calloc(1, sizeof(*c));
    if (!c)
        return patm_error(PATM_ERR_MEMORY, "SQLite: allocation failed");

    rc = sqlite3_open_v2(params->dbname, &c->db,
                         SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE |
                         SQLITE_OPEN_FULLMUTEX,
                         NULL);
    if (rc != SQLITE_OK) {
        snprintf(c->last_error, sizeof(c->last_error),
                 "sqlite3_open_v2: %s", sqlite3_errmsg(c->db));
        PatmError e = patm_error(PATM_ERR_DB_CONNECT, "SQLite: %s",
                                 c->last_error);
        sqlite3_close(c->db);
        free(c);
        return e;
    }

    err = patm_db_conn_new(&sqlite_driver, c, out);
    if (!patm_is_ok(&err)) {
        sqlite3_close(c->db);
        free(c);
        return err;
    }
    PATM_LOG_INFO("connected to SQLite db '%s'", params->dbname);
    return patm_ok();
}

static void sqlite_close(PatmConn *conn)
{
    SqliteConn *c;

    if (!conn || !conn->impl)
        return;
    c = conn->impl;
    if (c->db)
        sqlite3_close(c->db);
    free(c);
}

static const char *sqlite_last_error(PatmConn *conn)
{
    SqliteConn *c;

    if (!conn || !conn->impl)
        return "not connected";
    c = conn->impl;
    return c->last_error[0] ? c->last_error : "unknown sqlite error";
}

static void sqlite_store_err(SqliteConn *c, const char *fallback)
{
    const char *msg = sqlite3_errmsg(c->db);
    snprintf(c->last_error, sizeof(c->last_error), "%s",
             msg && msg[0] ? msg : fallback);
}

static PatmError sqlite_query(PatmConn *conn, const char *sql, PatmResult *out)
{
    SqliteConn *c = conn->impl;
    sqlite3_stmt *stmt = NULL;
    int rc;
    int ncols, nrows = 0;
    size_t i;
    PatmError err;

    rc = sqlite3_prepare_v2(c->db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite_store_err(c, "prepare failed");
        PatmError e = patm_error(PATM_ERR_DB_QUERY, "SQLite query: %s",
                                 c->last_error);
        return e;
    }

    ncols = sqlite3_column_count(stmt);

    /* Save column names before finalizing */
    char **col_names = NULL;
    if (ncols > 0) {
        col_names = calloc((size_t)ncols, sizeof(char *));
        if (!col_names) {
            sqlite3_finalize(stmt);
            return patm_error(PATM_ERR_MEMORY, "column names alloc failed");
        }
        for (int col = 0; col < ncols; col++) {
            const char *name = sqlite3_column_name(stmt, col);
            col_names[col] = strdup(name ? name : "");
            if (!col_names[col]) {
                for (int k = 0; k < col; k++)
                    free(col_names[k]);
                free(col_names);
                sqlite3_finalize(stmt);
                return patm_error(PATM_ERR_MEMORY, "column name copy failed");
            }
        }
    }

    /* Count rows and collect data */
    int alloc_rows = 64;
    char **cells = NULL;
    if (ncols > 0) {
        cells = calloc((size_t)alloc_rows * (size_t)ncols, sizeof(char *));
        if (!cells) {
            for (int col = 0; col < ncols; col++)
                free(col_names[col]);
            free(col_names);
            sqlite3_finalize(stmt);
            return patm_error(PATM_ERR_MEMORY, "result cells allocation failed");
        }
    }

    while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
        if (nrows >= alloc_rows) {
            int new_alloc = alloc_rows * 2;
            char **tmp = realloc(cells,
                                 (size_t)new_alloc * (size_t)ncols * sizeof(char *));
            if (!tmp) {
                for (i = 0; i < (size_t)nrows * (size_t)ncols; i++)
                    free(cells[i]);
                free(cells);
                for (int col = 0; col < ncols; col++)
                    free(col_names[col]);
                free(col_names);
                sqlite3_finalize(stmt);
                return patm_error(PATM_ERR_MEMORY, "result cells realloc failed");
            }
            cells = tmp;
            memset(&cells[(size_t)nrows * (size_t)ncols], 0,
                   (size_t)(new_alloc - nrows) * (size_t)ncols * sizeof(char *));
            alloc_rows = new_alloc;
        }
        for (int col = 0; col < ncols; col++) {
            if (sqlite3_column_type(stmt, col) == SQLITE_NULL)
                continue;
            const unsigned char *text = sqlite3_column_text(stmt, col);
            if (text)
                cells[(size_t)nrows * (size_t)ncols + (size_t)col] =
                    strdup((const char *)text);
        }
        nrows++;
    }

    if (rc != SQLITE_DONE) {
        sqlite_store_err(c, "step failed");
        PatmError e = patm_error(PATM_ERR_DB_QUERY, "SQLite query: %s",
                                 c->last_error);
        sqlite3_finalize(stmt);
        for (i = 0; i < (size_t)nrows * (size_t)ncols; i++)
            free(cells[i]);
        free(cells);
        for (int col = 0; col < ncols; col++)
            free(col_names[col]);
        free(col_names);
        return e;
    }

    sqlite3_finalize(stmt);

    if (ncols == 0 || nrows == 0) {
        memset(out, 0, sizeof(*out));
        out->nrows = (size_t)nrows;
        out->ncols = (size_t)ncols;
        out->col_names = col_names;
        free(cells);
        return patm_ok();
    }

    out->cells = NULL;
    out->col_names = NULL;
    err = patm_db_result_from_strings(out, (size_t)nrows, (size_t)ncols, cells);

    /* Use saved column names */
    if (patm_is_ok(&err))
        out->col_names = col_names;
    else {
        for (int col = 0; col < ncols; col++)
            free(col_names[col]);
        free(col_names);
    }

    for (i = 0; i < (size_t)nrows * (size_t)ncols; i++)
        free(cells[i]);
    free(cells);

    if (!patm_is_ok(&err)) {
        PatmResult tmp = *out;
        memset(out, 0, sizeof(*out));
        patm_db_result_free(&tmp);
    }
    return err;
}

static PatmError sqlite_execute(PatmConn *conn, const char *sql)
{
    SqliteConn *c = conn->impl;
    char *errmsg = NULL;
    int rc;

    rc = sqlite3_exec(c->db, sql, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        snprintf(c->last_error, sizeof(c->last_error), "%s",
                 errmsg ? errmsg : "exec failed");
        sqlite3_free(errmsg);
        return patm_error(PATM_ERR_DB_QUERY, "SQLite execute: %s",
                          c->last_error);
    }
    sqlite3_free(errmsg);
    return patm_ok();
}

static PatmError sqlite_quote_ident(const char *ident, char *out, size_t outsz)
{
    size_t n;

    if (!ident || !ident[0])
        return patm_error(PATM_ERR_INVALID_ARG, "empty identifier");
    n = strlen(ident);
    if (outsz < 2 + 2 * n + 1)
        return patm_error(PATM_ERR_INVALID_ARG, "identifier too long");
    out[0] = '"';
    size_t j = 1;
    for (size_t i = 0; i < n; i++) {
        out[j++] = ident[i];
        if (ident[i] == '"')
            out[j++] = '"';
    }
    out[j++] = '"';
    out[j] = '\0';
    return patm_ok();
}

static PatmError sqlite_quote_literal(PatmStrBuf *sb, const char *value)
{
    PatmError err;

    if (!value)
        return patm_strbuf_append(sb, "NULL");
    err = patm_strbuf_append_char(sb, '\'');
    if (!patm_is_ok(&err))
        return err;
    for (const char *p = value; *p; p++) {
        if (*p == '\'') {
            err = patm_strbuf_append_char(sb, '\'');
            if (!patm_is_ok(&err))
                return err;
        }
        err = patm_strbuf_append_char(sb, *p);
        if (!patm_is_ok(&err))
            return err;
    }
    err = patm_strbuf_append_char(sb, '\'');
    return err;
}

static PatmError sqlite_list_tables(PatmConn *conn, PatmStrBuf *json_out)
{
    static const char *SQL =
        "SELECT name FROM sqlite_master "
        "WHERE type = 'table' AND name NOT LIKE 'sqlite_%' "
        "ORDER BY name";
    PatmResult res;
    PatmError err;

    err = sqlite_query(conn, SQL, &res);
    if (!patm_is_ok(&err))
        return err;

    err = patm_strbuf_append(json_out, "[");
    for (size_t r = 0; patm_is_ok(&err) && r < res.nrows; r++) {
        if (r > 0)
            err = patm_strbuf_append(json_out, ",");
        if (patm_is_ok(&err))
            err = patm_strbuf_printf(json_out, "\"%s\"",
                                     res.cells[r * res.ncols]);
    }
    if (patm_is_ok(&err))
        err = patm_strbuf_append(json_out, "]");
    patm_db_result_free(&res);
    return err;
}

static const PatmDbDriver sqlite_driver = {
    .engine = PATM_DB_SQLITE,
    .name = "sqlite",
    .display = "SQLite",
    .default_port = 0,
    .connect = sqlite_connect,
    .close = sqlite_close,
    .query = sqlite_query,
    .execute = sqlite_execute,
    .quote_ident = sqlite_quote_ident,
    .quote_literal = sqlite_quote_literal,
    .list_tables = sqlite_list_tables,
    .last_error = sqlite_last_error,
};

const PatmDbDriver *patm_db_driver_sqlite(void)
{
    return &sqlite_driver;
}
