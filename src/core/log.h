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
#ifndef PATM_LOG_H
#define PATM_LOG_H

/*
 * Logger. Never log passwords or tokens — use patm_log_redact() on
 * anything user-supplied that could be a secret.
 */

typedef enum {
    PATM_LOG_DEBUG = 0,
    PATM_LOG_INFO,
    PATM_LOG_WARN,
    PATM_LOG_ERROR
} PatmLogLevel;

void patm_log_set_level(PatmLogLevel level);
PatmLogLevel patm_log_get_level(void);

void patm_log_write(PatmLogLevel level, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 2, 3)))
#endif
    ;

#define PATM_LOG_DEBUG(...) patm_log_write(PATM_LOG_DEBUG, __VA_ARGS__)
#define PATM_LOG_INFO(...)  patm_log_write(PATM_LOG_INFO, __VA_ARGS__)
#define PATM_LOG_WARN(...)  patm_log_write(PATM_LOG_WARN, __VA_ARGS__)
#define PATM_LOG_ERROR(...) patm_log_write(PATM_LOG_ERROR, __VA_ARGS__)

/* mask most of the string, keeping last 4 chars */
const char *patm_log_redact(char *dst, unsigned long dstsz, const char *s);

#endif
