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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "db/db.h"

/*
 * PostgreSQL integration test.
 *
 * Requires a local server with:
 *   database: patmtest
 *   table:    demo(id int, name text, amount numeric)
 *             containing exactly 3 rows where row 3 has NULLs
 *
 * Connection settings can be overridden via environment:
 *   PATM_IT_HOST, PATM_IT_PORT, PATM_IT_USER, PATM_IT_PASSWORD,
 *   PATM_IT_DB
 *
 * If no server answers, the test exits with code 77 (skipped), so plain
 * `ctest` stays green on machines without a database.
 */

static PatmError try_connect(PatmConn **out)
{
    const char *host = getenv("PATM_IT_HOST");
    const char *port = getenv("PATM_IT_PORT");
    const char *user = getenv("PATM_IT_USER");
    const char *pw = getenv("PATM_IT_PASSWORD");
    const char *db = getenv("PATM_IT_DB");
    const char *ssl = getenv("PATM_IT_SSL");

    if (!host) host = "localhost";
    if (!user) user = getenv("USER");
    if (!pw) pw = "patm_test_pw";
    if (!db) db = "patmtest";
    /* dev servers usually lack SSL certs; production default is require */
    if (!ssl) ssl = PATM_SSL_PREFER;

    PatmConnParams p = { .engine = PATM_DB_POSTGRESQL,
                         .host = host,
                         .user = user,
                         .password = pw,
                         .dbname = db,
                         .port = port ? atoi(port) : 5432,
                         .ssl_mode = ssl };
    return patm_db_connect(&p, out);
}

static int failures = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            fprintf(stderr, "FAIL: %s\n", (msg));                     \
            failures++;                                               \
        }                                                             \
    } while (0)

int main(void)
{
    PatmConn *conn = NULL;
    PatmError err = try_connect(&conn);

    if (!patm_is_ok(&err)) {
        printf("SKIP: no PostgreSQL test server (%s)\n", err.msg);
        return 77;
    }

    /* --- list_tables contains 'demo' ------------------------------- */
    PatmStrBuf tables = { 0 };
    err = patm_db_list_tables(conn, &tables);
    CHECK(patm_is_ok(&err), "list_tables failed");
    CHECK(tables.data && strstr(tables.data, "\"demo\"") != NULL,
          "table list does not contain demo");
    patm_strbuf_free(&tables);

    /* --- SELECT returns column names + values + NULL --------------- */
    PatmResult res;
    err = patm_db_query(conn, "SELECT id, name, amount FROM demo "
                              "ORDER BY id", &res);
    CHECK(patm_is_ok(&err), "SELECT failed");
    CHECK(res.nrows == 3 && res.ncols == 3, "wrong result shape");
    CHECK(res.col_names && res.col_names[0] &&
              !strcmp(res.col_names[0], "id"),
          "column name 'id' missing");
    CHECK(res.col_names[1] && !strcmp(res.col_names[1], "name"),
          "column name 'name' missing");
    CHECK(res.col_names[2] && !strcmp(res.col_names[2], "amount"),
          "column name 'amount' missing");
    CHECK(!strcmp(res.cells[0], "1"), "cell(0,0) wrong");
    CHECK(!strcmp(res.cells[1], "alpha"), "cell(0,1) wrong");
    /* row 3 (index 2): name and amount are NULL, id is not */
    CHECK(res.cells[6] && !strcmp(res.cells[6], "3"),
          "row 3 id cell wrong");
    CHECK(res.cells[7] == NULL && res.cells[8] == NULL,
          "NULL cells not preserved");
    patm_db_result_free(&res);

    /* --- identifier quoting is injection-proof end-to-end ---------- */
    char quoted[128];
    const PatmDbDriver *drv = patm_db_driver_get(PATM_DB_POSTGRESQL);
    err = drv->quote_ident("we\"rd; DROP TABLE demo;--",
                           quoted, sizeof(quoted));
    CHECK(patm_is_ok(&err), "quote_ident failed");

    PatmStrBuf sql = { 0 };
    patm_strbuf_printf(
        &sql,
        "DROP TABLE IF EXISTS %s", quoted);
    err = patm_db_execute(conn, sql.data);
    CHECK(patm_is_ok(&err), "cleanup drop failed");

    patm_strbuf_truncate(&sql, 0);
    patm_strbuf_printf(&sql, "CREATE TABLE %s (v text)", quoted);
    err = patm_db_execute(conn, sql.data);
    CHECK(patm_is_ok(&err), "create quoted table failed");

    PatmStrBuf ins = { 0 };
    const char *evil_value = "it'; DROP TABLE demo; --";
    drv->quote_literal(&ins, evil_value);
    PatmStrBuf ins_sql = { 0 };
    patm_strbuf_printf(&ins_sql, "INSERT INTO %s (v) VALUES (%s);",
                       quoted, ins.data ? ins.data : "NULL");
    err = patm_db_execute(conn, ins_sql.data);
    if (!patm_is_ok(&err))
        fprintf(stderr, "note: insert error: %s\n", err.msg);
    CHECK(patm_is_ok(&err), "insert literal failed");

    err = patm_db_query(conn, "SELECT v FROM \"we\"\"rd; DROP TABLE "
                              "demo;--\"", &res);
    if (!patm_is_ok(&err))
        fprintf(stderr, "note: select error: %s\n", err.msg);
    CHECK(patm_is_ok(&err), "select from quoted table failed");
    CHECK(res.nrows == 1 && res.cells[0] &&
              !strcmp(res.cells[0], evil_value),
          "literal did not survive round trip");
    patm_db_result_free(&res);

    PatmStrBuf drop = { 0 };
    patm_strbuf_printf(&drop, "DROP TABLE %s", quoted);
    err = patm_db_execute(conn, drop.data);
    CHECK(patm_is_ok(&err), "drop quoted table failed");

    err = patm_db_query(conn,
                        "SELECT count(*) AS n FROM demo", &res);
    CHECK(patm_is_ok(&err), "demo survived injection attempts");
    CHECK(patm_is_ok(&err) && res.nrows == 1 &&
              !strcmp(res.cells[0], "3"),
          "demo table was modified by injection attempts");
    patm_db_result_free(&res);

    /* --- error surfacing -------------------------------------------- */
    err = patm_db_query(conn, "SELECT * FROM does_not_exist_xyz",
                        &res);
    CHECK(!patm_is_ok(&err), "querying garbage should fail");
    CHECK(err.msg[0] != '\0', "error message empty");

    patm_strbuf_free(&sql);
    patm_strbuf_free(&ins);
    patm_strbuf_free(&ins_sql);
    patm_strbuf_free(&drop);
    patm_db_close(conn);

    if (failures) {
        fprintf(stderr, "%d integration check(s) FAILED\n", failures);
        return 1;
    }
    puts("pg integration tests passed");
    return 0;
}
