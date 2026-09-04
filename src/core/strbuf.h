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
#ifndef PATM_STRBUF_H
#define PATM_STRBUF_H

#include <stddef.h>

#include "error.h"

/*
 * Growable string buffer. Zero-init is fine; call free when done.
 */

typedef struct {
    char *data;   /* NULL until first append */
    size_t len;   /* bytes excluding NUL */
    size_t cap;   /* allocated bytes, or 0 */
} PatmStrBuf;

void patm_strbuf_init(PatmStrBuf *sb);
PatmError patm_strbuf_append(PatmStrBuf *sb, const char *s);
PatmError patm_strbuf_append_n(PatmStrBuf *sb, const char *s, size_t n);
PatmError patm_strbuf_append_char(PatmStrBuf *sb, char c);
PatmError patm_strbuf_printf(PatmStrBuf *sb, const char *fmt, ...)
#ifdef __GNUC__
    __attribute__((format(printf, 2, 3)))
#endif
    ;
/* hand off ownership of the string; resets sb */
char *patm_strbuf_release(PatmStrBuf *sb);
/* Reset to empty, keeping the allocation. */
void patm_strbuf_truncate(PatmStrBuf *sb, size_t new_len);
void patm_strbuf_free(PatmStrBuf *sb);

#endif
