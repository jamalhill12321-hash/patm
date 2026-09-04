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
#include "error.h"

#include <stdarg.h>
#include <stdio.h>

const char *patm_errcode_name(PatmErrCode code)
{
    switch (code) {
    case PATM_OK: return "ok";
    case PATM_ERR_MEMORY: return "memory";
    case PATM_ERR_INVALID_ARG: return "invalid-argument";
    case PATM_ERR_IO: return "io";
    case PATM_ERR_DB_CONNECT: return "db-connect";
    case PATM_ERR_DB_QUERY: return "db-query";
    case PATM_ERR_DB_DRIVER: return "db-driver";
    case PATM_ERR_KEYRING: return "keyring";
    case PATM_ERR_PYTHON: return "python";
    case PATM_ERR_INTERNAL: return "internal";
    }
    return "unknown";
}

PatmError patm_ok(void)
{
    PatmError e = { PATM_OK, { 0 } };
    return e;
}

int patm_is_ok(const PatmError *e)
{
    return e && e->code == PATM_OK;
}

PatmError patm_error(PatmErrCode code, const char *fmt, ...)
{
    PatmError e;
    va_list ap;

    e.code = code;
    va_start(ap, fmt);
    vsnprintf(e.msg, sizeof(e.msg), fmt, ap);
    va_end(ap);
    return e;
}
