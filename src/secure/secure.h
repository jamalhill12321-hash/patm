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
#ifndef PATM_SECURE_H
#define PATM_SECURE_H

#include "core/error.h"

/*
 * Password storage via libsecret (GNOME Keyring / KDE Wallet).
 * Passwords never touch config files, logs, or argv.
 */

#define PATM_SCHEMA "org.patm.Credentials"

/* store password for connection (overwrites if exists) */
PatmError patm_secure_store_password(const char *conn_id,
                                     const char *password);

/* fetch password into out buffer (returns error if not found) */
PatmError patm_secure_fetch_password(const char *conn_id, char *out,
                                     unsigned long outsz);

/* remove password (no-op if not found) */
PatmError patm_secure_delete_password(const char *conn_id);

#endif
