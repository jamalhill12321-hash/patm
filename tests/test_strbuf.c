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
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core/strbuf.h"

#define CHECK(expr)                       \
    do {                                  \
        PatmError e_ = (expr);            \
        assert(patm_is_ok(&e_));          \
    } while (0)

static void test_append(void)
{
    PatmStrBuf sb;
    patm_strbuf_init(&sb);
    CHECK(patm_strbuf_append(&sb, "hello"));
    CHECK(patm_strbuf_append_char(&sb, ' '));
    CHECK(patm_strbuf_printf(&sb, "%s=%d", "x", 42));
    assert(strcmp(sb.data, "hello x=42") == 0);
    assert(sb.len == strlen("hello x=42"));
    patm_strbuf_free(&sb);
}

static void test_release(void)
{
    PatmStrBuf sb;
    char *owned;

    patm_strbuf_init(&sb);
    patm_strbuf_append(&sb, "take ownership");
    owned = patm_strbuf_release(&sb);
    assert(owned && strcmp(owned, "take ownership") == 0);
    assert(sb.data == NULL && sb.len == 0 && sb.cap == 0);
    free(owned);
}

static void test_large_growth(void)
{
    PatmStrBuf sb;
    char chunk[4096];

    memset(chunk, 'a', sizeof(chunk));
    patm_strbuf_init(&sb);
    for (int i = 0; i < 1000; i++)
        CHECK(patm_strbuf_append_n(&sb, chunk, sizeof(chunk)));
    assert(sb.len == 1000u * sizeof(chunk));
    assert(sb.data[sb.len] == '\0');
    patm_strbuf_free(&sb);
}

static void test_empty_and_null(void)
{
    PatmStrBuf sb;

    patm_strbuf_init(&sb);
    CHECK(patm_strbuf_append(&sb, NULL));
    CHECK(patm_strbuf_append_n(&sb, "abc", 0));
    assert(sb.data == NULL);
    patm_strbuf_free(&sb);
    /* double free is safe */
    patm_strbuf_free(&sb);
}

int main(void)
{
    test_append();
    test_release();
    test_large_growth();
    test_empty_and_null();
    puts("strbuf tests passed");
    return 0;
}
