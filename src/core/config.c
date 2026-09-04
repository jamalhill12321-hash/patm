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
#include "config.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/log.h"

#define CONF_DIR_MODE 0700

static PatmError config_path(char *out, size_t outsz, const char *filename)
{
    const char *xdg = getenv("XDG_CONFIG_HOME");
    const char *home = getenv("HOME");

    if (xdg && xdg[0])
        snprintf(out, outsz, "%s/patm/%s", xdg, filename);
    else if (home && home[0])
        snprintf(out, outsz, "%s/.config/patm/%s", home, filename);
    else
        return patm_error(PATM_ERR_IO, "no config directory available");

    /* Create parent dirs best-effort; save() will surface real errors. */
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", out);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        mkdir(dir, CONF_DIR_MODE); /* one level; .config normally exists */
    }
    return patm_ok();
}

PatmError patm_config_load(PatmConfig *cfg)
{
    char path[512];
    FILE *f;
    char line[1024];

    memset(cfg, 0, sizeof(*cfg));

    PatmError err = config_path(path, sizeof(path), "connections.conf");
    if (!patm_is_ok(&err))
        return err;

    f = fopen(path, "r");
    if (!f)
        return patm_ok(); /* first run: empty config */

    PatmConnProfile *cur = NULL;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t')
            p++;
        if (*p == '#' || *p == ';' || !*p)
            continue;

        if (*p == '[') {
            char *close = strchr(p, ']');
            if (!close)
                continue;
            *close = '\0';
            char *name = p + 1;
            /* accept [connection "name"] or [name] */
            char *quote = strchr(name, '"');
            if (quote) {
                char *endq = strrchr(quote + 1, '"');
                if (endq)
                    *endq = '\0';
                name = quote + 1;
            }
            if (cfg->nconns >= PATM_MAX_CONNS)
                break;
            cur = &cfg->conns[cfg->nconns++];
            memset(cur, 0, sizeof(*cur));
            snprintf(cur->id, sizeof(cur->id), "%s", name);
        } else if (cur) {
            char *eq = strchr(p, '=');
            if (!eq)
                continue;
            *eq = '\0';
            char *key = p;
            char *val = eq + 1;
            if (!strcmp(key, "engine")) {
                if (!strcmp(val, "postgresql"))
                    cur->engine = PATM_DB_POSTGRESQL;
                else if (!strcmp(val, "mysql"))
                    cur->engine = PATM_DB_MYSQL;
                else if (!strcmp(val, "mariadb"))
                    cur->engine = PATM_DB_MARIADB;
            } else if (!strcmp(key, "host")) {
                snprintf(cur->host, sizeof(cur->host), "%s", val);
            } else if (!strcmp(key, "user")) {
                snprintf(cur->user, sizeof(cur->user), "%s", val);
            } else if (!strcmp(key, "database")) {
                snprintf(cur->dbname, sizeof(cur->dbname), "%s", val);
            } else if (!strcmp(key, "port")) {
                cur->port = atoi(val);
            } else if (!strcmp(key, "ssl_mode")) {
                snprintf(cur->ssl_mode, sizeof(cur->ssl_mode), "%s",
                         val);
            } else if (!strcmp(key, "ssh_host")) {
                snprintf(cur->ssh_host, sizeof(cur->ssh_host), "%s",
                         val);
            } else if (!strcmp(key, "ssh_user")) {
                snprintf(cur->ssh_user, sizeof(cur->ssh_user), "%s",
                         val);
            } else if (!strcmp(key, "ssh_port")) {
                cur->ssh_port = atoi(val);
            }
        }
    }
    fclose(f);
    return patm_ok();
}

static void write_profile(FILE *f, const PatmConnProfile *p)
{
    fprintf(f, "[connection \"%s\"]\n", p->id);
    fprintf(f, "engine=%s\n",
            p->engine == PATM_DB_POSTGRESQL   ? "postgresql"
            : p->engine == PATM_DB_MARIADB    ? "mariadb"
                                              : "mysql");
    fprintf(f, "host=%s\n", p->host);
    fprintf(f, "port=%d\n", p->port);
    fprintf(f, "user=%s\n", p->user);
    fprintf(f, "database=%s\n", p->dbname);
    fprintf(f, "ssl_mode=%s\n",
            p->ssl_mode[0] ? p->ssl_mode : PATM_SSL_REQUIRE);
    if (p->ssh_host[0]) {
        fprintf(f, "ssh_host=%s\n", p->ssh_host);
        fprintf(f, "ssh_user=%s\n", p->ssh_user);
        fprintf(f, "ssh_port=%d\n", p->ssh_port);
    }
    fprintf(f, "\n");
}

PatmError patm_config_save(const PatmConfig *cfg)
{
    char path[512];
    char tmp_path[520];
    FILE *f;

    PatmError err = config_path(path, sizeof(path), "connections.conf");
    if (!patm_is_ok(&err))
        return err;
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    f = fopen(tmp_path, "w");
    if (!f)
        return patm_error(PATM_ERR_IO, "cannot open '%s' for writing",
                          tmp_path);

    for (size_t i = 0; i < cfg->nconns; i++)
        write_profile(f, &cfg->conns[i]);

    if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
        fclose(f);
        remove(tmp_path);
        return patm_error(PATM_ERR_IO, "failed to flush config to disk");
    }
    fclose(f);

    /* Atomic replace so a crash never leaves a truncated config. */
    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return patm_error(PATM_ERR_IO, "cannot replace '%s'", path);
    }
    return patm_ok();
}

PatmError patm_config_add(PatmConfig *cfg, const PatmConnProfile *profile)
{
    if (!profile || !profile->id[0] || !profile->host[0] ||
        !profile->user[0] || !profile->dbname[0])
        return patm_error(PATM_ERR_INVALID_ARG,
                          "connection needs id, host, user and database");
    if ((int)profile->engine < 0 || profile->engine >= PATM_DB_ENGINE_COUNT)
        return patm_error(PATM_ERR_INVALID_ARG, "unknown engine");
    if (patm_config_find(cfg, profile->id))
        return patm_error(PATM_ERR_INVALID_ARG,
                          "connection id '%s' already exists", profile->id);
    if (cfg->nconns >= PATM_MAX_CONNS)
        return patm_error(PATM_ERR_INVALID_ARG, "connection list is full");

    cfg->conns[cfg->nconns++] = *profile;
    return patm_ok();
}

PatmError patm_config_update(PatmConfig *cfg, const char *id,
                             const PatmConnProfile *profile)
{
    if (!id || !id[0] || !profile)
        return patm_error(PATM_ERR_INVALID_ARG, "config update: bad args");
    if (!profile->id[0] || !profile->host[0] ||
        !profile->user[0] || !profile->dbname[0])
        return patm_error(PATM_ERR_INVALID_ARG,
                          "connection needs id, host, user and database");
    if ((int)profile->engine < 0 || profile->engine >= PATM_DB_ENGINE_COUNT)
        return patm_error(PATM_ERR_INVALID_ARG, "unknown engine");

    for (size_t i = 0; i < cfg->nconns; i++) {
        if (!strcmp(cfg->conns[i].id, id)) {
            /* If id changed, check new id is not a duplicate */
            if (strcmp(id, profile->id) != 0 && patm_config_find(cfg, profile->id))
                return patm_error(PATM_ERR_INVALID_ARG,
                                  "connection id '%s' already exists", profile->id);
            cfg->conns[i] = *profile;
            return patm_ok();
        }
    }
    return patm_error(PATM_ERR_INVALID_ARG, "no connection named '%s'", id);
}

PatmError patm_config_remove(PatmConfig *cfg, const char *id)
{
    for (size_t i = 0; i < cfg->nconns; i++) {
        if (!strcmp(cfg->conns[i].id, id)) {
            memmove(&cfg->conns[i], &cfg->conns[i + 1],
                    (cfg->nconns - i - 1) * sizeof(PatmConnProfile));
            cfg->nconns--;
            memset(&cfg->conns[cfg->nconns], 0,
                   sizeof(PatmConnProfile));
            return patm_ok();
        }
    }
    return patm_error(PATM_ERR_INVALID_ARG, "no connection named '%s'", id);
}

const PatmConnProfile *patm_config_find(const PatmConfig *cfg,
                                        const char *id)
{
    for (size_t i = 0; i < cfg->nconns; i++)
        if (!strcmp(cfg->conns[i].id, id))
            return &cfg->conns[i];
    return NULL;
}

/* ---- /tmp session persistence ---- */

static PatmError session_path(char *out, size_t outsz)
{
    return config_path(out, outsz, "session.conf");
}

PatmError patm_session_load(PatmSession *s)
{
    char line[4096];
    char path[512];
    memset(s, 0, sizeof(*s));

    PatmError perr = session_path(path, sizeof(path));
    if (!patm_is_ok(&perr)) return perr;

    FILE *f = fopen(path, "r");
    if (!f)
        return patm_ok();

    PatmSessionTab *cur = NULL;
    while (fgets(line, sizeof(line), f)) {
        line[strcspn(line, "\r\n")] = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || *p == ';' || !*p) continue;

        if (*p == '[') {
            char *close = strchr(p, ']');
            if (!close) continue;
            *close = '\0';
            char *type = p + 1;
            if (!strcmp(type, "query") || !strcmp(type, "terminal") ||
                !strcmp(type, "tool")) {
                if (s->n_tabs < PATM_SESSION_MAX_TABS) {
                    cur = &s->tabs[s->n_tabs++];
                    memset(cur, 0, sizeof(*cur));
                    snprintf(cur->type, sizeof(cur->type), "%s", type);
                }
            }
        } else if (!strncmp(p, "last_connection=", 16)) {
            snprintf(s->last_conn, sizeof(s->last_conn), "%s", p + 16);
        } else if (cur) {
            char *eq = strchr(p, '=');
            if (!eq) continue;
            *eq = '\0';
            char *key = p;
            char *val = eq + 1;
            if (!strcmp(key, "connection"))
                snprintf(cur->conn_id, sizeof(cur->conn_id), "%s", val);
            else if (!strcmp(key, "sql"))
                snprintf(cur->sql, sizeof(cur->sql), "%s", val);
            else if (!strcmp(key, "tool"))
                snprintf(cur->tool, sizeof(cur->tool), "%s", val);
        }
    }
    fclose(f);
    return patm_ok();
}

PatmError patm_session_save(const PatmSession *s)
{
    char path[512];
    PatmError perr = session_path(path, sizeof(path));
    if (!patm_is_ok(&perr)) return perr;

    char tmp_path[520];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", path);

    FILE *f = fopen(tmp_path, "w");
    if (!f)
        return patm_error(PATM_ERR_IO, "cannot open '%s' for writing", tmp_path);

    if (s->last_conn[0])
        fprintf(f, "last_connection=%s\n", s->last_conn);

    for (size_t i = 0; i < s->n_tabs; i++) {
        const PatmSessionTab *t = &s->tabs[i];
        fprintf(f, "\n[%s]\n", t->type);
        if (t->conn_id[0])
            fprintf(f, "connection=%s\n", t->conn_id);
        if (t->sql[0])
            fprintf(f, "sql=%s\n", t->sql);
        if (t->tool[0])
            fprintf(f, "tool=%s\n", t->tool);
    }

    if (fflush(f) != 0 || fsync(fileno(f)) != 0) {
        fclose(f);
        remove(tmp_path);
        return patm_error(PATM_ERR_IO, "failed to flush session to disk");
    }
    fclose(f);

    if (rename(tmp_path, path) != 0) {
        remove(tmp_path);
        return patm_error(PATM_ERR_IO, "cannot replace '%s'", path);
    }
    return patm_ok();
}

/* ---- auto-reconnect ---- */

static PatmError reconnect_path(char *out, size_t outsz)
{
    return config_path(out, outsz, "reconnect.dat");
}

/* simple XOR obfuscation — not real crypto, just prevents plaintext on disk */
static void xor_buf(char *buf, size_t len)
{
    const unsigned char key[] = {0x50, 0x41, 0x54, 0x4D}; /* "PATM" */
    for (size_t i = 0; i < len; i++)
        buf[i] ^= key[i % 4];
}

PatmError patm_reconnect_save(const char *conn_id)
{
    char path[512];
    PatmError perr = reconnect_path(path, sizeof(path));
    if (!patm_is_ok(&perr)) return perr;

    FILE *f = fopen(path, "w");
    if (!f)
        return patm_error(PATM_ERR_IO, "cannot write reconnect file");

    char buf[256];
    snprintf(buf, sizeof(buf), "%s", conn_id);
    size_t len = strlen(buf);
    xor_buf(buf, len);

    unsigned char key_len = (unsigned char)len;
    fwrite(&key_len, 1, 1, f);
    fwrite(buf, 1, len, f);
    fclose(f);
    return patm_ok();
}

PatmError patm_reconnect_load(char *conn_id, size_t conn_id_sz)
{
    conn_id[0] = '\0';
    char path[512];
    PatmError perr = reconnect_path(path, sizeof(path));
    if (!patm_is_ok(&perr)) return perr;

    FILE *f = fopen(path, "r");
    if (!f)
        return patm_ok();

    unsigned char key_len = 0;
    if (fread(&key_len, 1, 1, f) != 1 || key_len == 0 || key_len >= conn_id_sz) {
        fclose(f);
        return patm_ok();
    }

    char buf[256];
    if (fread(buf, 1, key_len, f) != key_len) {
        fclose(f);
        return patm_ok();
    }
    fclose(f);

    xor_buf(buf, key_len);
    buf[key_len] = '\0';
    snprintf(conn_id, conn_id_sz, "%s", buf);
    return patm_ok();
}

void patm_reconnect_clear(void)
{
    char path[512];
    PatmError perr = reconnect_path(path, sizeof(path));
    if (patm_is_ok(&perr))
        remove(path);
}
