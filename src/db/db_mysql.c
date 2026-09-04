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
#include <mysql.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/log.h"
#include "db_internal.h"

/*
 * MySQL / MariaDB driver (libmariadb). TLS defaults to required.
 */

typedef struct {
    MYSQL *my;
    char last_error[512];
} MyConn;

static const PatmDbDriver my_driver;

static void my_store_err(MyConn *c)
{
    snprintf(c->last_error, sizeof(c->last_error), "%s",
             mysql_error(c->my)[0] ? mysql_error(c->my)
                                   : "unknown MariaDB error");
}

static PatmError my_connect(const PatmConnParams *params, PatmConn **out)
{
    MyConn *c;
    MYSQL *raw;
    PatmError err;
    unsigned int port;


    if (!params->host || !params->user || !params->dbname)
        return patm_error(PATM_ERR_INVALID_ARG,
                          "MariaDB/MySQL: host, user and database required");
    if (!params->password)
        return patm_error(PATM_ERR_INVALID_ARG,
                          "MariaDB/MySQL: password required");

    raw = mysql_init(NULL);
    if (!raw)
        return patm_error(PATM_ERR_MEMORY, "libmariadb init failed");

    /* require = enforce+verify, prefer = if server supports it, disable = plaintext */
    const char *ssl =
        params->ssl_mode ? params->ssl_mode : PATM_SSL_REQUIRE;
    int enforce = strcmp(ssl, PATM_SSL_DISABLE) != 0;
    mysql_options(raw, MYSQL_OPT_SSL_ENFORCE, &enforce);
    if (!strcmp(ssl, PATM_SSL_REQUIRE)) {
        int verify = 1;
        mysql_options(raw, MYSQL_OPT_SSL_VERIFY_SERVER_CERT, &verify);
    }
    port = params->port > 0 ? (unsigned int)params->port : 3306u;

    if (!mysql_real_connect(raw, params->host, params->user,
                            params->password, params->dbname, port, NULL,
                            CLIENT_COMPRESS)) {
        PatmError e =
            patm_error(PATM_ERR_DB_CONNECT, "MariaDB/MySQL: %s",
                       mysql_error(raw)[0] ? mysql_error(raw)
                                           : "connection failed");
        mysql_close(raw);
        return e;
    }

    c = calloc(1, sizeof(*c));
    if (!c) {
        mysql_close(raw);
        return patm_error(PATM_ERR_MEMORY, "driver state allocation failed");
    }
    c->my = raw;
    err = patm_db_conn_new(&my_driver, c, out);
    if (!patm_is_ok(&err)) {
        free(c);
        mysql_close(raw);
        return err;
    }
    PATM_LOG_INFO("connected to MariaDB/MySQL db '%s' at %s:%u as user '%s'",
                  params->dbname, params->host, port, params->user);
    return patm_ok();
}

static void my_close(PatmConn *conn)
{
    MyConn *c;

    if (!conn || !conn->impl)
        return;
    c = conn->impl;
    if (c->my)
        mysql_close(c->my);
    free(c);
}

static const char *my_last_error(PatmConn *conn)
{
    if (!conn || !conn->impl)
        return "not connected";
    return ((MyConn *)conn->impl)->last_error;
}

static PatmError my_query(PatmConn *conn, const char *sql, PatmResult *out)
{
    MyConn *c = conn->impl;
    size_t len = strlen(sql);

    memset(out, 0, sizeof(*out));
    if (mysql_real_query(c->my, sql, len)) {
        my_store_err(c);
        return patm_error(PATM_ERR_DB_QUERY, "MariaDB query: %s",
                          c->last_error);
    }

    MYSQL_RES *res = mysql_store_result(c->my);
    if (!res) {
        if (mysql_field_count(c->my) == 0)
            return patm_ok(); /* statement with no result set */
        my_store_err(c);
        return patm_error(PATM_ERR_DB_QUERY, "MariaDB fetch: %s",
                          c->last_error);
    }

    unsigned int ncols = mysql_num_fields(res);
    unsigned long nrows = mysql_num_rows(res);
    PatmError err = patm_ok();
    size_t idx = 0;

    /* column names */
    MYSQL_FIELD *fields = mysql_fetch_fields(res);
    out->col_names = calloc(ncols ? ncols : 1, sizeof(char *));
    if (!out->col_names) {
        mysql_free_result(res);
        return patm_error(PATM_ERR_MEMORY, "column names alloc failed");
    }
    out->ncols = ncols;
    for (unsigned int colI = 0; colI < ncols; colI++) {
        out->col_names[colI] = strdup(fields[colI].name ? fields[colI].name
                                                        : "");
        if (!out->col_names[colI]) {
            err = patm_error(PATM_ERR_MEMORY, "column name copy failed");
            goto done;
        }
    }

    char **cells = calloc(nrows ? nrows : 1, ncols ? sizeof(char *) : 1);
    if (!cells) {
        err = patm_error(PATM_ERR_MEMORY, "result cells allocation failed");
        goto done;
    }
    if (ncols > 0 && nrows > 0) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res)) != NULL &&
               patm_is_ok(&err)) {
            for (unsigned long col = 0; col < ncols; col++, idx++) {
                if (!row[col])
                    continue; /* SQL NULL */
                cells[idx] = strdup(row[col]);
                if (!cells[idx]) {
                    err = patm_error(PATM_ERR_MEMORY, "cell copy failed");
                    break;
                }
            }
        }
    }

    if (patm_is_ok(&err))
        err = patm_db_result_from_strings(out, nrows, ncols, cells);

done:
    mysql_free_result(res);
    if (cells)
        for (size_t i = 0; i < (size_t)nrows * ncols; i++)
            free(cells[i]);
    free(cells);
    if (!patm_is_ok(&err))
        patm_db_result_free(out);
    return err;
}

static PatmError my_execute(PatmConn *conn, const char *sql)
{
    MyConn *c = conn->impl;

    if (mysql_real_query(c->my, sql, strlen(sql))) {
        my_store_err(c);
        return patm_error(PATM_ERR_DB_QUERY, "MariaDB execute: %s",
                          c->last_error);
    }
    /* Consume any multi-statement leftovers so the connection stays sane. */
    for (;;) {
        MYSQL_RES *extra = mysql_store_result(c->my);
        if (extra)
            mysql_free_result(extra);
        if (mysql_next_result(c->my) > 0)
            break;
    }
    return patm_ok();
}

static PatmError my_quote_ident(const char *ident, char *out, size_t outsz)
{
    if (!ident || !ident[0])
        return patm_error(PATM_ERR_INVALID_ARG, "empty identifier");
    size_t n = strlen(ident);
    if (outsz < 2 + 2 * n + 1)
        return patm_error(PATM_ERR_INVALID_ARG, "identifier too long");
    size_t j = 0;
    out[j++] = '`';
    for (size_t i = 0; i < n; i++) {
        out[j++] = ident[i];
        if (ident[i] == '`')
            out[j++] = '`';
    }
    out[j++] = '`';
    out[j] = '\0';
    return patm_ok();
}

static PatmError my_quote_literal(PatmStrBuf *sb, const char *value)
{
    PatmError err;

    if (!value)
        return patm_strbuf_append(sb, "NULL");
    err = patm_strbuf_append_char(sb, '\'');
    if (!patm_is_ok(&err))
        return err;
    for (const char *p = value; *p; p++) {
        switch (*p) {
        case '\'':
            err = patm_strbuf_append(sb, "\\'");
            break;
        case '\\':
            err = patm_strbuf_append(sb, "\\\\");
            break;
        default:
            err = patm_strbuf_append_char(sb, *p);
            break;
        }
        if (!patm_is_ok(&err))
            return err;
    }
    return patm_strbuf_append_char(sb, '\'');
}

static PatmError my_list_tables(PatmConn *conn, PatmStrBuf *json_out)
{
    static const char *SQL =
        "SELECT table_name FROM information_schema.tables "
        "WHERE table_schema = DATABASE() AND table_type = 'BASE TABLE' "
        "ORDER BY table_name";
    PatmResult res;
    PatmError err = my_query(conn, SQL, &res);

    if (!patm_is_ok(&err))
        return err;
    err = patm_strbuf_append(json_out, "[");
    for (size_t r = 0; patm_is_ok(&err) && r < res.nrows; r++) {
        if (r > 0)
            err = patm_strbuf_append(json_out, ",");
        if (patm_is_ok(&err))
            err = patm_strbuf_printf(json_out, "\"%s\"",
                                     res.cells[r * res.ncols] ?: "");
    }
    if (patm_is_ok(&err))
        err = patm_strbuf_append(json_out, "]");
    patm_db_result_free(&res);
    return err;
}

static const PatmDbDriver my_driver = {
    .engine = PATM_DB_MYSQL,
    .name = "mysql",
    .display = "MySQL",
    .default_port = 3306,
    .connect = my_connect,
    .close = my_close,
    .query = my_query,
    .execute = my_execute,
    .quote_ident = my_quote_ident,
    .quote_literal = my_quote_literal,
    .list_tables = my_list_tables,
    .last_error = my_last_error,
};

static const PatmDbDriver maria_driver = {
    .engine = PATM_DB_MARIADB,
    .name = "mariadb",
    .display = "MariaDB",
    .default_port = 3306,
    .connect = my_connect,
    .close = my_close,
    .query = my_query,
    .execute = my_execute,
    .quote_ident = my_quote_ident,
    .quote_literal = my_quote_literal,
    .list_tables = my_list_tables,
    .last_error = my_last_error,
};

const PatmDbDriver *patm_db_driver_mysql(void)
{
    return &my_driver;
}

const PatmDbDriver *patm_db_driver_mariadb(void)
{
    return &maria_driver;
}
