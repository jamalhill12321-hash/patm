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

#ifndef PATM_C_BACKEND_H
#define PATM_C_BACKEND_H

#ifdef __cplusplus
extern "C" {
#endif

#include "core/config.h"
#include "core/error.h"
#include "core/log.h"
#include "core/settings.h"
#include "core/strbuf.h"
#include "db/db.h"
#include "net/ssh_tunnel.h"
#include "net/update_check.h"
#include "pipeline/pipeline.h"
#include "secure/secure.h"

#ifdef __cplusplus
}
#endif

#endif
