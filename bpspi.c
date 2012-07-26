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

#include <string.h>
#include <unistd.h>

#include "serial.h"
#include "buspirate.h"

int bp_spi_enter (bp_state_t * bp) {
    int result;
    uint8_t buffer [10];

    if (bp->speed < 0 || bp->speed > 7)
        return 1;

    if (bp->mode != BPMBINARY || bp->submode != BPSMHIZ) {
        if (bp_mode (bp, BPMBINARY))
            return 1;
    }

    serWriteChar (bp->fd, BPBCENTERSPI);
    if ((result = serReadTimed (bp->fd, 2000, 4, buffer)) != 4 ||
        strncmp ((char *)buffer, "SPI", 3))
        return 1;
    bp->bm_version = buffer[3] - '0';

    serWriteChar (bp->fd, BPSPISETSPEED | bp->speed);
    if (serReadCharTimed (bp->fd, 1000000) != 1)
        return 1;
    serWriteChar (bp->fd, BPSPICONFIG2 | ((bp->flags >> 4) & 0xf));
    if (serReadCharTimed (bp->fd, 1000000) != 1)
        return 1;
    serWriteChar (bp->fd, BPSPICONFIG1 | ((bp->flags & 0xf) ^ BPSPICFGCS));
    if (serReadCharTimed (bp->fd, 1000000) != 1)
        return 1;
    if (bp->flags & BPSPICFGAUXINPUT) {
        serWriteChar (bp->fd, BPSPIREADAUX);
        if (serReadCharTimed (bp->fd, 1000000) == -1)
            return 1;
    }

    bp->submode = BPSMSPI;

    return 0;
}

static int set_cs (bp_state_t * bp) {
    serWriteChar (bp->fd, BPSPICONFIG1 | (bp->flags & 0xf));
    if (serReadCharTimed (bp->fd, 1000000) != 1)
        return 1;
    if (bp->flags & BPSPICFGAUXINPUT) {
        serWriteChar (bp->fd, BPSPIREADAUX);
        if (serReadCharTimed (bp->fd, 1000000) == -1)
            return 1;
    }
    return 0;
}

static int clear_cs (bp_state_t * bp) {
    serWriteChar (bp->fd, BPSPICONFIG1 | ((bp->flags & 0xf) ^ BPSPICFGCS));
    if (serReadCharTimed (bp->fd, 1000000) != 1)
        return 1;
    if (bp->flags & BPSPICFGAUXINPUT) {
        serWriteChar (bp->fd, BPSPIREADAUX);
        if (serReadCharTimed (bp->fd, 1000000) == -1)
            return 1;
    }
    return 0;
}

int bp_spi_command (bp_state_t * bp, int writelen, int readlen, uint8_t * buffer) {
    int result = 0;

    if (writelen < 0 || writelen > TERMINAL_BUFFER ||
        readlen  < 0 || readlen  > TERMINAL_BUFFER)
        return 1;
    if (bp->mode != BPMBINARY || bp->submode != BPSMSPI)
        return 1;
    if (bp->bm_version != 1)
        return 2;

    if (set_cs (bp))
        return 3;

    serWriteChar (bp->fd, BPSPIWRITEREADNOCS);
    serWriteChar (bp->fd, (writelen >> 8) & 0xff);
    serWriteChar (bp->fd, writelen        & 0xff);
    serWriteChar (bp->fd, (readlen >> 8)  & 0xff);
    serWriteChar (bp->fd, readlen         & 0xff);
// Errors happen right after sending the length... check with a short timeout
    if ((result = serReadCharTimed (bp->fd, 10000)) != 0) {
        if (writelen)
            serWrite (bp->fd, writelen, buffer);
        if (serReadCharTimed (bp->fd, 1000000) == 1 && readlen)
            result = serReadTimed (bp->fd, 1000000, readlen, buffer);
    }

    clear_cs (bp);

    if (result == readlen)
        return 0;
    else
        return 3;
}

int bp_spi_command_short (bp_state_t * bp, int flags, uint8_t command, uint8_t data) {
    uint8_t buffer[2];
    int result;

    if (bp->mode != BPMBINARY || bp->submode != BPSMSPI)
        return -1;

    buffer[0] = command;
    buffer[1] = data;

    result = bp_spi_command (bp, flags&1 ? 2 : 1, flags&2 ? 1 : 0, buffer);
    if (result == -1)
        return -1;
    if (flags & 2)
        return buffer[0];
    return 0;
}
