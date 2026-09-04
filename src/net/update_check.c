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

#include "update_check.h"

#include <stdio.h>
#include <string.h>

#include "version.h" /* generated into build/generated */

/*
 * Dormant implementation: there is no release yet, so there is nothing
 * to check and this code deliberately performs zero network activity.
 * Flip PATM_ENABLE_UPDATE_CHECK at build time when the first release
 * exists; then replace this body with the HTTPS manifest fetcher.
 */
PatmUpdateStatus patm_update_check(char *version_out,
                                   unsigned long outsz)
{
    if (version_out && outsz > strlen(PATM_VERSION))
        snprintf(version_out, outsz, "%s", PATM_VERSION);
    else if (version_out && outsz)
        version_out[0] = '\0';
    return PATM_UPDATE_DISABLED;
}
