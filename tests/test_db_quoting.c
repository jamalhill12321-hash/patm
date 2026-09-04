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
#include <stdlib.h>
#include <string.h>

#include "db/db.h"

#define CHECK(expr)                       \
    do {                                  \
        PatmError e_ = (expr);            \
        assert(patm_is_ok(&e_));          \
    } while (0)

/*
 * Pure-logic driver tests: identifier and literal quoting, no server
 * needed. Connection tests live in the integration suite.
 */

static void test_pg_quote_ident(void)
{
    const PatmDbDriver *d = patm_db_driver_get(PATM_DB_POSTGRESQL);
    char buf[128];

    CHECK(d->quote_ident("users", buf, sizeof(buf)));
    assert(strcmp(buf, "\"users\"") == 0);

    /* Embedded double quote must be doubled, not left injectable. */
    CHECK(d->quote_ident("weird\"name", buf, sizeof(buf)));
    assert(strcmp(buf, "\"weird\"\"name\"") == 0);

    {
        PatmError e = d->quote_ident("", buf, sizeof(buf));
        assert(!patm_is_ok(&e));
    }
}

static void test_mysql_quote_ident(void)
{
    const PatmDbDriver *d = patm_db_driver_get(PATM_DB_MYSQL);
    char buf[128];

    CHECK(d->quote_ident("orders", buf, sizeof(buf)));
    assert(strcmp(buf, "`orders`") == 0);

    CHECK(d->quote_ident("back`tick", buf, sizeof(buf)));
    assert(strcmp(buf, "`back``tick`") == 0);
}

static void test_quote_literal(void)
{
    const PatmDbDriver *pg = patm_db_driver_get(PATM_DB_POSTGRESQL);
    const PatmDbDriver *my = patm_db_driver_get(PATM_DB_MYSQL);
    PatmStrBuf sb;
    char *out;

    patm_strbuf_init(&sb);
    CHECK(pg->quote_literal(&sb, "it's fine"));
    out = patm_strbuf_release(&sb);
    assert(strcmp(out, "'it''s fine'") == 0);
    free(out);

    patm_strbuf_init(&sb);
    CHECK(pg->quote_literal(&sb, NULL));
    out = patm_strbuf_release(&sb);
    assert(strcmp(out, "NULL") == 0);
    free(out);

    patm_strbuf_init(&sb);
    CHECK(my->quote_literal(&sb, "back\\slash's"));
    out = patm_strbuf_release(&sb);
    assert(strcmp(out, "'back\\\\slash\\'s'") == 0);
    free(out);
}

static void test_result_free_zeroed_is_safe(void)
{
    PatmResult res;
    memset(&res, 0, sizeof(res));
    patm_db_result_free(&res); /* must not crash */
}

int main(void)
{
    test_pg_quote_ident();
    test_mysql_quote_ident();
    test_quote_literal();
    test_result_free_zeroed_is_safe();
    puts("driver quoting tests passed");
    return 0;
}
