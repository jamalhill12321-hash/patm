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
#ifndef PATM_CONFIG_H
#define PATM_CONFIG_H

#include <stddef.h>

#include "core/error.h"
#include "core/strbuf.h"
#include "db/db.h"

/*
 * Saved connections live in $XDG_CONFIG_HOME/patm/connections.conf using a
 * flat INI-like format. Passwords are NEVER written here; they go to the
 * system keyring (see secure.h).
 */

#define PATM_MAX_CONNS 64
#define PATM_ID_MAX 64

typedef struct {
    char id[PATM_ID_MAX]; /* user-chosen unique name */
    PatmDbEngine engine;
    char host[256];
    char user[256];
    char dbname[256];
    int port; /* 0 = default */
    char ssl_mode[16]; /* "require" (default), "prefer", "disable" */
    /* Optional SSH tunnel (see src/net/ssh_tunnel.h). Empty ssh_host
     * means direct connection. */
    char ssh_host[256];
    char ssh_user[64];
    int ssh_port; /* 0 = 22 */
} PatmConnProfile;

typedef struct {
    PatmConnProfile conns[PATM_MAX_CONNS];
    size_t nconns;
} PatmConfig;

/* Load config from disk. Missing file yields an empty config (not an error). */
PatmError patm_config_load(PatmConfig *cfg);

/* Atomically save config to disk. */
PatmError patm_config_save(const PatmConfig *cfg);

/* Validate and append a profile; rejects duplicate ids. */
PatmError patm_config_add(PatmConfig *cfg, const PatmConnProfile *profile);

/* Update an existing profile by id. */
PatmError patm_config_update(PatmConfig *cfg, const char *id,
                             const PatmConnProfile *profile);

/* Remove by id. Returns error if not found. */
PatmError patm_config_remove(PatmConfig *cfg, const char *id);

const PatmConnProfile *patm_config_find(const PatmConfig *cfg,
                                        const char *id);

/* ---- session persistence (open tabs + last connection) ---- */

#define PATM_SESSION_MAX_TABS 32

typedef struct {
    char type[16];     /* "query", "terminal", "tool" */
    char conn_id[64];  /* connection profile id */
    char sql[4096];    /* SQL content (query tabs) */
    char tool[256];    /* tool name (tool tabs) */
} PatmSessionTab;

typedef struct {
    char last_conn[64];
    PatmSessionTab tabs[PATM_SESSION_MAX_TABS];
    size_t n_tabs;
} PatmSession;

PatmError patm_session_load(PatmSession *s);
PatmError patm_session_save(const PatmSession *s);

/* ---- auto-reconnect (encrypted file in /tmp) ---- */

/* Save connection details for auto-reconnect on next launch. */
PatmError patm_reconnect_save(const char *conn_id);

/* Load connection details. Returns empty conn_id if no file or error. */
PatmError patm_reconnect_load(char *conn_id, size_t conn_id_sz);

/* Delete the reconnect file (called on manual disconnect). */
void patm_reconnect_clear(void);

#endif
