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
#include <libpq-fe.h>

#include <stdlib.h>
#include <string.h>

#include "core/log.h"
#include "db_internal.h"

/*
 * PostgreSQL driver (libpq). Uses PQconnectdbParams so user input never
 * gets interpreted as connection-string syntax.
 */

typedef struct {
    PGconn *pg;
    char last_error[512];
} PgConn;

static const PatmDbDriver pg_driver;

static PatmError pg_connect(const PatmConnParams *params, PatmConn **out)
{
    const char *keywords[] = { "host",     "port",     "user",
                               "password", "dbname",   "sslmode",
                               "connect_timeout", NULL };
    char port_buf[16];
    const char *values[7];
    PgConn *c;
    PGconn *pg;
    PatmError err;

    if (!params->host || !params->user || !params->dbname)
        return patm_error(PATM_ERR_INVALID_ARG,
                          "PostgreSQL: host, user and database are required");
    if (!params->password)
        return patm_error(PATM_ERR_INVALID_ARG,
                          "PostgreSQL: password required");

    snprintf(port_buf, sizeof(port_buf), "%d",
             params->port > 0 ? params->port : 5432);
    values[0] = params->host;
    values[1] = port_buf;
    values[2] = params->user;
    values[3] = params->password;
    values[4] = params->dbname;
    values[5] = params->ssl_mode ? params->ssl_mode : PATM_SSL_REQUIRE;
    values[6] = "10";

    pg = PQconnectdbParams(keywords, values, 1);
    if (!pg)
        return patm_error(PATM_ERR_MEMORY, "libpq allocation failed");
    if (PQstatus(pg) != CONNECTION_OK) {
        PatmError e = patm_error(PATM_ERR_DB_CONNECT,
                                 "PostgreSQL: %s", PQerrorMessage(pg));
        PQfinish(pg);
        return e;
    }

    err = patm_db_conn_new(&pg_driver, c = calloc(1, sizeof(*c)), out);
    if (!patm_is_ok(&err)) {
        free(c);
        PQfinish(pg);
        return err;
    }
    c->pg = pg;
    PATM_LOG_INFO("connected to PostgreSQL db '%s' at %s:%d as user '%s'",
                  params->dbname, params->host, params->port, params->user);
    return patm_ok();
}

static void pg_close(PatmConn *conn)
{
    PgConn *c;

    if (!conn || !conn->impl)
        return;
    c = conn->impl;
    if (c->pg)
        PQfinish(c->pg);
    free(c);
}

static const char *pg_last_error(PatmConn *conn)
{
    PgConn *c;

    if (!conn || !conn->impl)
        return "not connected";
    c = conn->impl;
    return c->last_error[0] ? c->last_error : "unknown libpq error";
}

static void pg_store_err(PgConn *c, PGresult *res, const char *fallback)
{
    const char *msg = res ? PQresultErrorMessage(res) : NULL;
    snprintf(c->last_error, sizeof(c->last_error), "%s",
             msg && msg[0] ? msg : fallback);
}

static PatmError pg_fill_col_names(PGresult *res, int ncols,
                                   PatmResult *out)
{
    if (ncols == 0)
        return patm_ok();
    out->col_names = calloc((size_t)ncols, sizeof(char *));
    if (!out->col_names)
        return patm_error(PATM_ERR_MEMORY, "column names alloc failed");
    for (int c = 0; c < ncols; c++) {
        out->col_names[c] = strdup(PQfname(res, c));
        if (!out->col_names[c])
            return patm_error(PATM_ERR_MEMORY, "column name copy failed");
    }
    return patm_ok();
}

static PatmError pg_query(PatmConn *conn, const char *sql, PatmResult *out)
{
    PgConn *c = conn->impl;
    PGresult *res;
    ExecStatusType status;
    int nrows, ncols, r, col;
    size_t i;
    PatmError err;

    res = PQexec(c->pg, sql);
    status = res ? PQresultStatus(res) : PGRES_FATAL_ERROR;
    if (status != PGRES_TUPLES_OK && status != PGRES_COMMAND_OK) {
        pg_store_err(c, res, "query failed");
        PatmError e = patm_error(PATM_ERR_DB_QUERY, "PostgreSQL query: %s",
                                 c->last_error);
        PQclear(res);
        return e;
    }

    nrows = PQntuples(res);
    ncols = PQnfields(res);

    if (status != PGRES_TUPLES_OK || nrows == 0 || ncols == 0) {
        /* DDL/DML success or an empty set: no rows, keep names if any */
        memset(out, 0, sizeof(*out));
        out->nrows = (size_t)(status == PGRES_TUPLES_OK ? nrows : 0);
        out->ncols =
            (size_t)(status == PGRES_TUPLES_OK ? ncols : 0);
        PatmError ferr =
            status == PGRES_TUPLES_OK
                ? pg_fill_col_names(res, ncols, out)
                : patm_ok();
        PQclear(res);
        return ferr;
    }

    char **cells = calloc((size_t)nrows * (size_t)ncols, sizeof(char *));
    if (!cells) {
        PQclear(res);
        return patm_error(PATM_ERR_MEMORY, "result cells allocation failed");
    }
    for (r = 0; r < nrows; r++) {
        for (col = 0; col < ncols; col++) {
            if (PQgetisnull(res, r, col))
                continue;
            cells[r * ncols + col] = strdup(PQgetvalue(res, r, col));
        }
    }

    out->cells = NULL;
    out->col_names = NULL;
    /* result_from_strings memsets *out, so column names go in AFTER */
    err = patm_db_result_from_strings(out, (size_t)nrows,
                                      (size_t)ncols, cells);
    if (patm_is_ok(&err))
        err = pg_fill_col_names(res, ncols, out);
    PQclear(res);
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

static PatmError pg_execute(PatmConn *conn, const char *sql)
{
    PgConn *c = conn->impl;
    PGresult *res;
    ExecStatusType status;
    PatmError ret = patm_ok();

    res = PQexec(c->pg, sql);
    status = res ? PQresultStatus(res) : PGRES_FATAL_ERROR;
    if (status != PGRES_COMMAND_OK && status != PGRES_TUPLES_OK) {
        pg_store_err(c, res, "execution failed");
        ret = patm_error(PATM_ERR_DB_QUERY, "PostgreSQL execute: %s",
                         c->last_error);
    }
    PQclear(res);
    return ret;
}

static PatmError pg_quote_ident(const char *ident, char *out, size_t outsz)
{
    size_t n;

    if (!ident || !ident[0])
        return patm_error(PATM_ERR_INVALID_ARG, "empty identifier");
    n = strlen(ident);
    /* '"' + ident with each '"' doubled + '"' must fit. */
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

PatmError patm_pg_quote_literal_impl(PatmStrBuf *sb, const char *value)
{
    PatmError err;

    /* quote a literal without a live connection (doubling ' is enough) */
    if (!value)
        return patm_strbuf_append(sb, "NULL");
    err = patm_strbuf_append_char(sb, '\'');
    if (!patm_is_ok(&err))
        return err;
    for (const char *p = value; *p; p++) {
        err = patm_strbuf_append_char(sb, *p);
        if (!patm_is_ok(&err))
            return err;
        if (*p == '\'') {
            err = patm_strbuf_append_char(sb, '\'');
            if (!patm_is_ok(&err))
                return err;
        }
    }
    err = patm_strbuf_append_char(sb, '\'');
    return err;
}

static PatmError pg_quote_literal(PatmStrBuf *sb, const char *value)
{
    return patm_pg_quote_literal_impl(sb, value);
}

static PatmError pg_list_tables(PatmConn *conn, PatmStrBuf *json_out)
{
    static const char *SQL =
        "SELECT table_name FROM information_schema.tables "
        "WHERE table_schema = current_schema() AND table_type = 'BASE TABLE' "
        "ORDER BY table_name";
    PatmResult res;
    PatmError err;

    err = pg_query(conn, SQL, &res);
    if (!patm_is_ok(&err))
        return err;

    err = patm_strbuf_append(json_out, "[");
    for (size_t r = 0; patm_is_ok(&err) && r < res.nrows; r++) {
        if (r > 0)
            err = patm_strbuf_append(json_out, ",");
        if (patm_is_ok(&err)) {
            err = patm_strbuf_printf(json_out, "\"%s\"",
                                     res.cells[r * res.ncols]);
        }
    }
    if (patm_is_ok(&err))
        err = patm_strbuf_append(json_out, "]");
    patm_db_result_free(&res);
    return err;
}

static const PatmDbDriver pg_driver = {
    .engine = PATM_DB_POSTGRESQL,
    .name = "postgresql",
    .display = "PostgreSQL",
    .default_port = 5432,
    .connect = pg_connect,
    .close = pg_close,
    .query = pg_query,
    .execute = pg_execute,
    .quote_ident = pg_quote_ident,
    .quote_literal = pg_quote_literal,
    .list_tables = pg_list_tables,
    .last_error = pg_last_error,
};

const PatmDbDriver *patm_db_driver_pg(void)
{
    return &pg_driver;
}
