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
#ifndef PATM_ERROR_H
#define PATM_ERROR_H

#include <stddef.h>

/*
 * PATM error type. code == PATM_OK means success.
 * Messages are either static or owned by the msg buffer.
 */

typedef enum {
    PATM_OK = 0,
    PATM_ERR_MEMORY,
    PATM_ERR_INVALID_ARG,
    PATM_ERR_IO,
    PATM_ERR_DB_CONNECT,
    PATM_ERR_DB_QUERY,
    PATM_ERR_DB_DRIVER,
    PATM_ERR_KEYRING,
    PATM_ERR_PYTHON,
    PATM_ERR_INTERNAL
} PatmErrCode;

typedef struct {
    PatmErrCode code;
    char msg[512];
} PatmError;

const char *patm_errcode_name(PatmErrCode code);

/* build an error with a printf-style message */
PatmError patm_error(PatmErrCode code, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 2, 3)))
#endif
    ;

/* success value */
PatmError patm_ok(void);

int patm_is_ok(const PatmError *e);

#endif
