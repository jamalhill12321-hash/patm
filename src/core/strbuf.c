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
#include "strbuf.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PATM_STRBUF_MIN_CAP 64

static PatmError grow(PatmStrBuf *sb, size_t needed)
{
    size_t new_cap;
    char *p;

    if (needed <= sb->cap)
        return patm_ok();

    new_cap = sb->cap ? sb->cap : PATM_STRBUF_MIN_CAP;
    while (new_cap < needed) {
        if (new_cap > (size_t)-1 / 2)
            return patm_error(PATM_ERR_MEMORY, "string buffer too large");
        new_cap *= 2;
    }
    p = realloc(sb->data, new_cap);
    if (!p)
        return patm_error(PATM_ERR_MEMORY, "string buffer allocation failed");
    sb->data = p;
    sb->cap = new_cap;
    return patm_ok();
}

void patm_strbuf_init(PatmStrBuf *sb)
{
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}

PatmError patm_strbuf_append_n(PatmStrBuf *sb, const char *s, size_t n)
{
    PatmError err;

    if (!sb || (!s && n > 0))
        return patm_error(PATM_ERR_INVALID_ARG, "strbuf_append_n: bad args");
    if (n == 0)
        return patm_ok();
    err = grow(sb, sb->len + n + 1);
    if (!patm_is_ok(&err))
        return err;
    memcpy(sb->data + sb->len, s, n);
    sb->len += n;
    sb->data[sb->len] = '\0';
    return patm_ok();
}

PatmError patm_strbuf_append(PatmStrBuf *sb, const char *s)
{
    return patm_strbuf_append_n(sb, s, s ? strlen(s) : 0);
}

PatmError patm_strbuf_append_char(PatmStrBuf *sb, char c)
{
    return patm_strbuf_append_n(sb, &c, 1);
}

PatmError patm_strbuf_printf(PatmStrBuf *sb, const char *fmt, ...)
{
    va_list ap, ap2;
    int needed;
    PatmError err;

    va_start(ap, fmt);
    va_copy(ap2, ap);
    needed = vsnprintf(NULL, 0, fmt, ap);
    va_end(ap);
    if (needed < 0) {
        va_end(ap2);
        return patm_error(PATM_ERR_INTERNAL, "strbuf_printf: format error");
    }
    err = grow(sb, sb->len + (size_t)needed + 1);
    if (!patm_is_ok(&err)) {
        va_end(ap2);
        return err;
    }
    vsnprintf(sb->data + sb->len, (size_t)needed + 1, fmt, ap2);
    va_end(ap2);
    sb->len += (size_t)needed;
    return patm_ok();
}

char *patm_strbuf_release(PatmStrBuf *sb)
{
    char *out;

    if (!sb)
        return NULL;
    out = sb->data;
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
    return out;
}

void patm_strbuf_truncate(PatmStrBuf *sb, size_t new_len)
{
    if (!sb)
        return;
    if (new_len > sb->len)
        return;
    sb->len = new_len;
    if (sb->data)
        sb->data[sb->len] = '\0';
}

void patm_strbuf_free(PatmStrBuf *sb)
{
    if (!sb)
        return;
    free(sb->data);
    sb->data = NULL;
    sb->len = 0;
    sb->cap = 0;
}
