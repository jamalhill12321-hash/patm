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

#ifndef PATM_NET_SSH_TUNNEL_H
#define PATM_NET_SSH_TUNNEL_H

#include "core/error.h"

/*
 * SSH port-forward tunnel. Spawns the system ssh binary (`ssh -N -L`)
 * so we don't have to ship our own SSH library. Requires key/agent auth —
 * no interactive passwords. DB connections then go through 127.0.0.1:<local port>.
 */

typedef struct PatmSshTunnel PatmSshTunnel;

typedef struct {
    const char *ssh_host;
    const char *ssh_user;
    int ssh_port;        /* 0 = 22 */
    const char *db_host; /* as seen FROM the ssh server */
    int db_port;
} PatmSshTunnelParams;

PatmError patm_ssh_tunnel_start(const PatmSshTunnelParams *params,
                                PatmSshTunnel **out);
void patm_ssh_tunnel_stop(PatmSshTunnel *tunnel);
int patm_ssh_tunnel_local_port(const PatmSshTunnel *tunnel);

#endif
