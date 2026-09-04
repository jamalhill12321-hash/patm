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
#ifndef PATM_PIPELINE_H
#define PATM_PIPELINE_H

#include "core/error.h"
#include "db/db.h"

/*
 * Runs user-editable Python tools with source/target DB connections.
 * Tools get a `_patm` module injected into their globals:
 *   _patm.source_query(sql)      -> list of tuples (SELECT only)
 *   _patm.source_tables()        -> list of table names
 *   _patm.target_exec(sql)       -> DDL/DML on target
 *   _patm.target_insert(t, cols, rows) -> row count
 *   _patm.log(msg)               -> appends to tool log
 *   _patm.config                 -> dict of run parameters
 *
 * Tools never see credentials — connections live on the C side.
 */

typedef struct {
    PatmConn *source;
    PatmConn *target; /* may be NULL for source-only tools */
} PatmPipelineCtx;

PatmError patm_pipeline_init(void);
void patm_pipeline_finalize(void);

/* Run a tool script. config_json is a JSON string (NULL/empty = {}). */
PatmError patm_pipeline_run_tool(const char *tool_path,
                                 const PatmPipelineCtx *ctx,
                                 const char *config_json,
                                 PatmStrBuf *log_out);

#endif
