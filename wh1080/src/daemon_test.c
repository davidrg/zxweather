/*****************************************************************************
 *            Created: 14/04/2012
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

/* This is a program for testing the basic daemon code as a console app. It
 * is not platform-specific. */

#include "daemon.h"
#include <stdlib.h>
#include <stdio.h>

/* Use the bundled version of getopt on non-POSIX systems */
#ifdef __WIN32
#include "getopt/getopt.h"
#elif _MSC_VER
#include "getopt/getopt.h"
#else
#include <unistd.h>
#endif

/* UNIX Daemon entry point */
int main( int argc, char *argv[] ) {
    char* server = NULL;
    char* username = NULL;
    char* password = NULL;
    char* station = NULL;
    int c;
    extern char *optarg;

    FILE* logfile = stderr;

    printf("WH1080 Daemon console test app\n");

    while ((c = getopt(argc, argv, "d:u:p:s:")) != -1) {
        printf("%c",c);
        switch(c) {
        case 'd':
            server = optarg;
            break;
        case 'u':
            username = optarg;
            break;
        case 'p':
            password = optarg;
            break;
        case 's':
            station = optarg;
        }
    }

    if (server == NULL)
        fprintf(stderr, "Supply database and server name (-d option)\n");
    if (username == NULL)
        fprintf(stderr, "Supply username (-u option)\n");
    if (password == NULL)
        fprintf(stderr, "Supply password (-p option)\n");
    if (station == NULL)
        fprintf(stderr, "Supply station code (-s option)\n");
    if (server == NULL || username == NULL || password == NULL || station == NULL)
        exit(EXIT_FAILURE);

    printf("Go!\n");
    fprintf(logfile, "Launch...\n");
    daemon_main(server, username, password, station, logfile);

    /* If we get this far then something went wrong. */
    cleanup();

    return EXIT_FAILURE;
}
