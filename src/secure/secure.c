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
#include "secure.h"

#include <libsecret/secret.h>

#include "core/log.h"

static const SecretSchema g_schema = {
    PATM_SCHEMA,
    SECRET_SCHEMA_DONT_MATCH_NAME,
    { { "id", SECRET_SCHEMA_ATTRIBUTE_STRING },
      { "NULL", 0 } },
    0, NULL, NULL, NULL, NULL, NULL, NULL, NULL
};

PatmError patm_secure_store_password(const char *conn_id,
                                     const char *password)
{
    GError *err = NULL;

    if (!conn_id || !conn_id[0] || !password)
        return patm_error(PATM_ERR_INVALID_ARG, "keyring store: bad args");

    secret_password_store_sync(&g_schema, SECRET_COLLECTION_DEFAULT,
                               conn_id, password, NULL, &err, "id",
                               conn_id, NULL);
    if (err) {
        PatmError e = patm_error(PATM_ERR_KEYRING,
                                 "storing password for '%s': %s", conn_id,
                                 err->message);
        g_error_free(err);
        return e;
    }
    PATM_LOG_INFO("stored password in keyring for connection '%s'", conn_id);
    return patm_ok();
}

PatmError patm_secure_fetch_password(const char *conn_id, char *out,
                                     unsigned long outsz)
{
    gchar *password = NULL;
    GError *err = NULL;
    PatmError ret = patm_ok();

    if (!conn_id || !out || outsz == 0)
        return patm_error(PATM_ERR_INVALID_ARG, "keyring fetch: bad args");
    out[0] = '\0';

    password =
        secret_password_lookup_sync(&g_schema, NULL, &err, "id", conn_id,
                                    NULL);
    if (err) {
        ret = patm_error(PATM_ERR_KEYRING,
                         "looking up password for '%s': %s", conn_id,
                         err->message);
        g_error_free(err);
        return ret;
    }
    if (!password)
        return patm_error(PATM_ERR_KEYRING,
                          "no stored password for connection '%s'",
                          conn_id);

    if (strlen(password) >= outsz) {
        ret = patm_error(PATM_ERR_KEYRING, "stored password too long");
    } else {
        strcpy(out, password);
    }
    secret_password_free(password);
    return ret;
}

PatmError patm_secure_delete_password(const char *conn_id)
{
    GError *err = NULL;

    if (!conn_id || !conn_id[0])
        return patm_error(PATM_ERR_INVALID_ARG, "keyring delete: bad args");

    secret_password_clear_sync(&g_schema, NULL, &err, "id", conn_id, NULL);
    if (err) {
        PatmError e = patm_error(PATM_ERR_KEYRING,
                                 "deleting password for '%s': %s", conn_id,
                                 err->message);
        g_error_free(err);
        return e;
    }
    return patm_ok();
}
