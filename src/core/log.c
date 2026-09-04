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
#include "log.h"

#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static PatmLogLevel g_level = PATM_LOG_INFO;
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;

void patm_log_set_level(PatmLogLevel level)
{
    g_level = level;
}

PatmLogLevel patm_log_get_level(void)
{
    return g_level;
}

static const char *level_tag(PatmLogLevel level)
{
    switch (level) {
    case PATM_LOG_DEBUG: return "DEBUG";
    case PATM_LOG_INFO: return "INFO";
    case PATM_LOG_WARN: return "WARN";
    case PATM_LOG_ERROR: return "ERROR";
    }
    return "?????";
}

void patm_log_write(PatmLogLevel level, const char *fmt, ...)
{
    char stamp[32];
    struct timespec ts;
    struct tm tm_buf;
    va_list ap;

    if (level < g_level)
        return;

    clock_gettime(CLOCK_REALTIME, &ts);
    localtime_r(&ts.tv_sec, &tm_buf);
    strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm_buf);

    pthread_mutex_lock(&g_lock);
    fprintf(stderr, "[%s.%03ld] [%s] ", stamp, ts.tv_nsec / 1000000L,
            level_tag(level));
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
    pthread_mutex_unlock(&g_lock);
}

const char *patm_log_redact(char *dst, unsigned long dstsz, const char *s)
{
    unsigned long n;

    if (!dst || dstsz == 0)
        return "";
    if (!s) {
        dst[0] = '\0';
        return dst;
    }

    n = strlen(s);
    if (n > 8 && dstsz >= 9 + 1) {
        memset(dst, '*', 9);
        strncpy(dst + 9, s + n - 4, dstsz - 10);
        dst[dstsz - 1] = '\0';
    } else if (dstsz >= 9) {
        memset(dst, '*', 8);
        dst[8] = '\0';
    } else {
        memset(dst, '*', dstsz);
        dst[dstsz - 1] = '\0';
    }
    return dst;
}
