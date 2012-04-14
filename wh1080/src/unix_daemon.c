/*****************************************************************************
 *            Created: 10/04/2012
 *          Copyright: (C) Copyright David Goodwin, 2012
 *            License: GNU General Public License
 *****************************************************************************
 *
 *   This is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This software is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this software; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 ****************************************************************************/

/* This file is UNIX-specific */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "daemon.h"

FILE* logfile;

void launch_daemon() {
    pid_t pid, sid;

    /* Are we already a daemon? Then we have nothing to do. */
    if (getppid() == 1)
        return;

    /* Nope. Fork away from the parent process. */
    pid = fork();
    if (pid < 0)
        exit(EXIT_FAILURE);

    /* We are now the child process */
    umask(0);
    sid = getsid();
    if (sid < 0)
        exit(EXIT_FAILURE);

    /* Change the current working directory to root so we don't end up holding
     * any annoying locks. */
    if (chdir("/") < 0)
        exit(EXIT_FAILURE);

    /* Redirect all output to null - there isn't really anywhere else for them
     * to go. */
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
}

void l_cleanup() {
    cleanup();
    fclose(logfile);

}

void Signal_Handler(int sig) { /* signal handler function */
    switch(sig){
    case SIGTERM:
        /* Tidyup */
        l_cleanup();
        exit(EXIT_SUCCESS);
        break;
    }
}

/* UNIX Daemon entry point */
int main( int argc, char *argv[] ) {
    char* server = NULL;
    char* username = NULL;
    char* password = NULL;
    char* filename = NULL;
    int c;
    extern char *optarg;

    while ((c = getopt(argc, argv, "d:u:p:f:")) != -1) {
        switch(c) {
        case 'd':
            database = optarg;
            break;
        case 'u':
            username = optarg;
            break;
        case 'p':
            password = optarg;
            break;
        case 'f':
            filename = optarg;
            break;
        }
    }

    if (server == NULL)
        fprintf(stderr, "Supply database and server name (-d option)\n");
    if (username == NULL)
        fprintf(stderr, "Supply username (-u option)\n");
    if (password == NULL)
        fprintf(stderr, "Supply password (-p option)\n");
    if (filename == NULL)
        fprintf(stderr, "Supply log filename (-f option)\n");
    if (server == NULL || username == NULL || password == NULL || filename == NULL)
        exit(EXIT_FAILURE);

    logfile = fopen(filename,"w");
    if (logfile == NULL) {
        fprintf(stderr, "Failed to open log file for writing.\n");
        exit(EXIT_FAILURE);
    }

    /* Now go become a daemon and do some work */

    launch_daemon();

    signal(SIGTERM,Signal_Handler);

    daemon(server, username, password, logfile);

    /* If we get this far then something went wrong. */
    l_cleaup();

    return EXIT_FAILURE;
}
