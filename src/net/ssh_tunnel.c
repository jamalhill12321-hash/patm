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

#include "ssh_tunnel.h"

#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "core/log.h"

struct PatmSshTunnel {
    pid_t pid;
    int local_port;
    char ssh_host[256];
};

/* grab a free loopback port */
static int find_free_port(void)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = 0;

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) != 0 ||
        listen(fd, 1) != 0) {
        close(fd);
        return -1;
    }

    socklen_t len = sizeof(addr);
    if (getsockname(fd, (struct sockaddr *)&addr, &len) != 0) {
        close(fd);
        return -1;
    }
    int port = ntohs(addr.sin_port);
    close(fd);
    return port;
}

static void kill_child(pid_t pid)
{
    int status;

    if (pid <= 0)
        return;
    if (kill(pid, SIGTERM) == 0) {
        /* give it a moment, then reap; do not block forever */
        for (int i = 0; i < 20; i++) {
            if (waitpid(pid, &status, WNOHANG) == pid)
                return;
            usleep(50 * 1000);
        }
        kill(pid, SIGKILL);
        waitpid(pid, &status, 0);
    }
}

PatmError patm_ssh_tunnel_start(const PatmSshTunnelParams *params,
                                PatmSshTunnel **out)
{
    if (!params || !out || !params->ssh_host || !params->ssh_user ||
        !params->db_host)
        return patm_error(PATM_ERR_INVALID_ARG,
                          "tunnel: host and user required");
    if (!strcmp(params->db_host, "localhost") ||
        !strcmp(params->db_host, "127.0.0.1"))
        return patm_error(
            PATM_ERR_INVALID_ARG,
            "tunnel db host must be reachable from the SSH server "
            "(use 'localhost' there only if the DB really runs on the "
            "SSH box)");

    PatmSshTunnel *t = calloc(1, sizeof(*t));
    if (!t)
        return patm_error(PATM_ERR_MEMORY,
                          "tunnel allocation failed");

    int port = find_free_port();
    if (port <= 0) {
        free(t);
        return patm_error(PATM_ERR_IO,
                          "no free loopback port for tunnel");
    }
    t->local_port = port;
    snprintf(t->ssh_host, sizeof(t->ssh_host), "%s", params->ssh_host);

    char local_spec[64];
    snprintf(local_spec, sizeof(local_spec),
             "127.0.0.1:%d:%s:%d", port, params->db_host,
             params->db_port > 0 ? params->db_port : 5432);

    char port_opt[32];
    snprintf(port_opt, sizeof(port_opt), "%d",
             params->ssh_port > 0 ? params->ssh_port : 22);

    /* -N: no remote command. ExitOnForwardFailure: bail fast.
     * ServerAliveInterval: detect dead connections. No passwords on CLI. */
    char argv0[] = "ssh";
    char dash_n[] = "-N";
    char opt_exit[] = "-o";
    char opt_exit_val[] = "ExitOnForwardFailure=yes";
    char opt_alive[] = "-o";
    char opt_alive_val[] = "ServerAliveInterval=30";
    char opt_estrict[] = "-o";
    char opt_estrict_val[] = "StrictHostKeyChecking=accept-new";
    char opt_L[] = "-L";
    char opt_p[] = "-p";

    char target[512];
    snprintf(target, sizeof(target), "%s@%s", params->ssh_user,
             params->ssh_host);

    char *argv[] = { argv0,      dash_n,       opt_exit, opt_exit_val,
                     opt_alive,  opt_alive_val, opt_estrict,
                     opt_estrict_val, opt_L,   local_spec, opt_p,
                     port_opt,  target,       NULL };

    pid_t pid = fork();
    if (pid < 0) {
        free(t);
        return patm_error(PATM_ERR_IO, "fork failed for tunnel");
    }
    if (pid == 0) {
        execvp("ssh", argv);
        _exit(127); /* exec failed */
    }
    t->pid = pid;

    /* did ssh die immediately? */
    usleep(200 * 1000);
    int status;
    pid_t r = waitpid(pid, &status, WNOHANG);
    if (r == pid || (r < 0 && errno == ECHILD)) {
        PatmError e = patm_error(
            PATM_ERR_IO,
            "ssh tunnel exited immediately (check key auth / host)");
        t->pid = -1;
        free(t);
        return e;
    }

    PATM_LOG_INFO("ssh tunnel up: 127.0.0.1:%d via %s@%s -> %s:%d",
                  port, params->ssh_user, params->ssh_host,
                  params->db_host,
                  params->db_port > 0 ? params->db_port : 5432);
    *out = t;
    return patm_ok();
}

void patm_ssh_tunnel_stop(PatmSshTunnel *tunnel)
{
    if (!tunnel)
        return;
    kill_child(tunnel->pid);
    PATM_LOG_INFO("ssh tunnel to %s stopped", tunnel->ssh_host);
    free(tunnel);
}

int patm_ssh_tunnel_local_port(const PatmSshTunnel *tunnel)
{
    return tunnel ? tunnel->local_port : -1;
}
