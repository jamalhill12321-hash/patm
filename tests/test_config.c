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
#include <sys/stat.h>

#include "core/config.h"

#define CHECK(expr)                       \
    do {                                  \
        PatmError e_ = (expr);            \
        assert(patm_is_ok(&e_));          \
    } while (0)

static char g_tmpdir[] = "/tmp/patm-test-XXXXXX";

/* Redirect config path resolution into a temp dir for the test run. */
static void setup_env(void)
{
    char *dir = mkdtemp(g_tmpdir);
    assert(dir != NULL);
    setenv("XDG_CONFIG_HOME", dir, 1);

    char sub[512];
    snprintf(sub, sizeof(sub), "%s/patm", dir);
    mkdir(sub, 0700);
}

static void write_config_file(const char *content)
{
    char path[512];
    const char *xdg = getenv("XDG_CONFIG_HOME");

    snprintf(path, sizeof(path), "%s/patm/connections.conf", xdg);
    FILE *f = fopen(path, "w");
    assert(f);
    fputs(content, f);
    fclose(f);
}

static void test_roundtrip(void)
{
    PatmConfig cfg;

    write_config_file(
        "# comment\n"
        "[connection \"prod\"]\n"
        "engine=postgresql\n"
        "host=db.example.com\n"
        "port=5433\n"
        "user=alice\n"
        "database=sales\n"
        "\n"
        "[connection \"local\"]\n"
        "engine=mysql\n"
        "host=localhost\n"
        "port=3306\n"
        "user=bob\n"
        "database=shop\n");

    CHECK(patm_config_load(&cfg));
    assert(cfg.nconns == 2);
    assert(!strcmp(cfg.conns[0].id, "prod"));
    assert(cfg.conns[0].engine == PATM_DB_POSTGRESQL);
    assert(cfg.conns[0].port == 5433);
    assert(!strcmp(cfg.conns[1].id, "local"));
    assert(cfg.conns[1].engine == PATM_DB_MYSQL);

    /* Save + reload keeps everything. */
    CHECK(patm_config_save(&cfg));
    PatmConfig reloaded;
    CHECK(patm_config_load(&reloaded));
    assert(reloaded.nconns == 2);
    assert(!strcmp(reloaded.conns[0].dbname, "sales"));
}

static void test_add_remove_find(void)
{
    PatmConfig cfg;
    memset(&cfg, 0, sizeof(cfg));

    PatmConnProfile p = { 0 };
    strcpy(p.id, "test1");
    p.engine = PATM_DB_POSTGRESQL;
    strcpy(p.host, "h");
    strcpy(p.user, "u");
    strcpy(p.dbname, "d");

    CHECK(patm_config_add(&cfg, &p));
    assert(patm_config_find(&cfg, "test1") != NULL);

    /* Duplicate id rejected. */
    {
        PatmError e = patm_config_add(&cfg, &p);
        assert(!patm_is_ok(&e));
    }

    /* Missing fields rejected. */
    PatmConnProfile bad = p;
    bad.host[0] = '\0';
    {
        PatmError e = patm_config_add(&cfg, &bad);
        assert(!patm_is_ok(&e));
    }

    CHECK(patm_config_remove(&cfg, "test1"));
    assert(patm_config_find(&cfg, "test1") == NULL);
    {
        PatmError e = patm_config_remove(&cfg, "test1");
        assert(!patm_is_ok(&e));
    }
}

static void test_session_roundtrip(void)
{
    PatmSession s;
    memset(&s, 0, sizeof(s));

    snprintf(s.last_conn, sizeof(s.last_conn), "test_pg");
    s.n_tabs = 2;
    snprintf(s.tabs[0].type, sizeof(s.tabs[0].type), "query");
    snprintf(s.tabs[0].conn_id, sizeof(s.tabs[0].conn_id), "test_pg");
    snprintf(s.tabs[0].sql, sizeof(s.tabs[0].sql), "SELECT 1");
    snprintf(s.tabs[1].type, sizeof(s.tabs[1].type), "terminal");
    snprintf(s.tabs[1].conn_id, sizeof(s.tabs[1].conn_id), "test_pg");

    CHECK(patm_session_save(&s));

    /* Verify file is non-empty */
    {
        char spath[512];
        snprintf(spath, sizeof(spath), "%s/patm/session.conf", getenv("XDG_CONFIG_HOME"));
        FILE *f = fopen(spath, "r");
        assert(f != NULL);
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fclose(f);
        assert(sz > 0);
    }

    /* Load it back */
    PatmSession loaded;
    CHECK(patm_session_load(&loaded));
    assert(!strcmp(loaded.last_conn, "test_pg"));
    assert(loaded.n_tabs == 2);
    assert(!strcmp(loaded.tabs[0].type, "query"));
    assert(!strcmp(loaded.tabs[0].conn_id, "test_pg"));
    assert(!strcmp(loaded.tabs[0].sql, "SELECT 1"));
    assert(!strcmp(loaded.tabs[1].type, "terminal"));
    assert(!strcmp(loaded.tabs[1].conn_id, "test_pg"));

    /* Overwrite with empty session — file should be 0 bytes */
    memset(&s, 0, sizeof(s));
    CHECK(patm_session_save(&s));
    {
        char spath[512];
        snprintf(spath, sizeof(spath), "%s/patm/session.conf", getenv("XDG_CONFIG_HOME"));
        FILE *f = fopen(spath, "r");
        assert(f != NULL);
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        fclose(f);
        assert(sz == 0);
    }
}

int main(void)
{
    setup_env();
    test_roundtrip();
    test_add_remove_find();
    test_session_roundtrip();
    puts("config tests passed");
    return 0;
}
