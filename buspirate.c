/*
 * This file is part of the spitool project.
 *
 * Copyright (C) 2012 Christian Schmidt
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "serial.h"
#include "buspirate.h"

int bp_set_rate (bp_state_t * bp, tcflag_t newrate) {
    char buffer [256];
    int result;
    int r1 = 0, r2 = 0;

    if (newrate == bp->devicerate)
        return 0;
    switch (newrate) {
    case B300: r1 = 1; break;
    case B1200: r1 = 2; break;
    case B2400: r1 = 3; break;
    case B4800: r1 = 4; break;
    case B9600: r1 = 5; break;
    case B19200: r1 = 6; break;
    case B38400: r1 = 7; break;
    case B57600: r1 = 8; break;
    case B115200: r1 = 9; break;
    case B230400: r1 = 10; r2 = 16; break;
    case B460800: r1 = 10; r2 = 8; break;
    case B1000000: r1=10; r2 = 3; break;
    case B2000000: r1=10; r2 = 1; break;
    default: return 1;
    }

    if (bp_mode (bp, BPMTERMINAL))
        return 1;
    serWriteLine (bp->fd, SWLFCECHO, "b\n");
    while ((result = serReadLine (bp->fd, sizeof (buffer), buffer)) >= 0)
        if (!strcmp (buffer, "(9)>"))
            break;
    if (result == -1)
        return 1;
    snprintf (buffer, sizeof (buffer), "%d\n", r1);
    serWriteLine (bp->fd, SWLFCECHO, buffer);
    if (r1 == 10) {
        while ((result = serReadLine (bp->fd, sizeof (buffer), buffer)) >= 0)
            if (!strcmp (buffer, "(34)>"))
                break;
        if (result == -1)
            return 1;
        snprintf (buffer, sizeof (buffer), "%d\n", r2);
        serWriteLine (bp->fd, SWLFCECHO, buffer);
    }
    while ((result = serReadLine (bp->fd, sizeof (buffer), buffer)) >= 0) ;
    if (serSetSpeed (bp->fd, newrate))
        return 1;
    serWriteLine (bp->fd, 0, " ");
    while ((result = serReadLine (bp->fd, sizeof (buffer), buffer)) >= 0)
        if (!strcmp (buffer, "HiZ>"))
            return 0;
        else
            printf ("Received: %s\n", buffer);
    return 1;
}

int bp_open (bp_state_t * bp) {
    char buffer [256];
    tcflag_t rate = bp->devicerate;

/* Open the device */
    if ((bp->fd = serOpenPort (bp->devicename, B115200)) == -1)
        return 1;
/* Deplete buffer */
    while (serReadLine (bp->fd, sizeof (buffer), buffer) >= 0)
        printf ("%s", buffer);

    bp->mode = BPMUNKNOWN;
    if (bp_reset (bp))
        return 1;

    bp->devicerate = B115200;
    if (bp_set_rate (bp, rate)) {
        fprintf (stderr, "WARNING: serial port rate setting failed!\nThe bus pirate may be in undefined state.\n");
        return 1;
    }
    return 0;
}

int bp_reset (bp_state_t * bp) {
    int i, result;
    char buffer [256];
    int s[5];
    int is_reset = 0;

    if (bp_mode (bp, BPMBINARY)) {
        for (i=0; i<20; i++) {
            serWriteChar (bp->fd, '\r');
            do {
                if ((result = serReadLine (bp->fd, sizeof (buffer), buffer)) > 1 &&
                    buffer[result-1] == '>' && buffer[result-2] != ')') {
                    i=21; // break the outer for-loop
                    break;
                }
            } while (result > 0);
        }
        if (i==20)
            return 1;
        if (bp_mode (bp, BPMBINARY))
            return 1;
    }

    for (i=0; i<20 && !is_reset; i++) {
        serWriteChar (bp->fd, BPBCRESET);
        usleep (20000);
        while (serReadLine (bp->fd, sizeof (buffer), buffer) >= 0) {
            if (strstr (buffer, "Bus Pirate"))
                is_reset = 1;
            if (sscanf (buffer, "Bus Pirate v%d.%d", &s[0], &s[1]) == 2)
                bp->hw_version = 10*s[0]+s[1];
            else if (sscanf (buffer, "Firmware v%d.%d r%d  Bootloader v%d.%d",
                             &s[0], &s[1], &s[2], &s[3], &s[4])) {
                bp->sw_version = 10*s[0]+s[1];
                bp->bl_version = 10*s[3]+s[4];
            }
        }

    }
    if (is_reset) {
        bp->mode = BPMTERMINAL;
        bp->submode = BPSMHIZ;
        bp->devicerate = B115200;
        return 0;
    }

    return 1;
}

int bp_mode (bp_state_t * bp, int newmode) {
    int i, result;
    char buffer [256];

    switch (newmode) {
    case BPMBINARY:
        for (i=0; i<20; i++) {
            serWriteChar (bp->fd, BPBCENTER);
            if ((result = serReadTimed (bp->fd, 2000, 5, (uint8_t *) buffer)) > 0) {
                // A BEL (0x07) as return means inappropiate char
                if (result == 1 && buffer[0] == 7)
                    return 1;
                // A 0x01 as result means we are in binary state, but the command is not valid
                if (result == 1 && buffer[0] == 1)
                    return 1;
                buffer[result] = 0;
                if (result == 5 && !(strncmp (buffer, "BBIO", 4))) {
                    bp->mode = BPMBINARY;
                    bp->submode = BPSMHIZ;
                    bp->bm_version = buffer[4] - '0';
                    return 0;
                }
            }
        }
        break;
    case BPMTERMINAL :
        switch (bp->mode) {
        case BPMBINARY:
            if (bp->submode != BPSMHIZ) {
                bp_mode (bp, BPMBINARY);
                usleep (20000);
            }
            serWriteChar (bp->fd, BPBCRESET);
            usleep (20000);
            while (serReadLine (bp->fd, sizeof (buffer), buffer) >= 0);
            bp->mode = BPMTERMINAL;
            bp->submode = BPSMHIZ;
            return 0;
        case BPMTERMINAL:
            return 0;
        }
    }

    return 1;
}
