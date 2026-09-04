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

#ifndef PATM_NET_UPDATE_CHECK_H
#define PATM_NET_UPDATE_CHECK_H

/*
 * Version update checker.
 *
 * Currently compiled out — no network I/O happens unless you build with
 * -DPATM_ENABLE_UPDATE_CHECK=1. When enabled: HTTPS only, signed manifest,
 * version query only, no user data sent.
 */

typedef enum {
    PATM_UPDATE_DISABLED = 0, /* build flag off (current default) */
    PATM_UPDATE_UP_TO_DATE,
    PATM_UPDATE_AVAILABLE,
    PATM_UPDATE_ERROR
} PatmUpdateStatus;

PatmUpdateStatus patm_update_check(char *version_out,
                                   unsigned long outsz);

#endif
