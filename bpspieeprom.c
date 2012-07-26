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
#include <stdio.h>
#include <unistd.h>

#include "buspirate.h"

enum BPSPIEEPROMCMDS {
    /* Basic Commands - M95640* */
    WRSR     = 0x01,
    WRITE    = 0x02,
    READ     = 0x03,
    WRDI     = 0x04,
    RDSR     = 0x05,
    WREN     = 0x06,
    /* Extended Command - M95640DR* */
    WRIDPAGE = 0x82,
    RDIDPAGE = 0x83
};

enum BPSPIEEPROMSRFLAGS {
    WIP      = 0x01,     // Write in progress
    WEL      = 0x02,     // Write enable latch
    BP0      = 0x04,     // Block Protext 0
    BP1      = 0x08,     // Block Protect 1
    SWRD     = 0x80      // Status Register Write Disable
};

int bp_spi_eeprom_rdsr (bp_state_t * bp) {
    return bp_spi_command_short (bp, WR1RD1, RDSR, 0);
}

int bp_spi_eeprom_wrsr (bp_state_t * bp, uint8_t data) {
    int result;

    if ((result = bp_spi_eeprom_rdsr (bp)) == -1)
        return -1;
    if (!(result & WEL))
        if (bp_spi_eeprom_wrenable (bp) == -1)
            return -1;

    return bp_spi_command_short (bp, WR2RD0, WRSR, data);
}

int bp_spi_eeprom_wrenable (bp_state_t * bp) {
    return bp_spi_command_short (bp, WR1RD0, WREN, 0);
}

int bp_spi_eeprom_wrdisable (bp_state_t * bp) {
    return bp_spi_command_short (bp, WR1RD0, WRDI, 0);
}

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

int bp_spi_eeprom_read (bp_state_t * bp, int addr, int length, int addrbytes, uint8_t * buffer) {
    int result, readbytes, total = 0;
    uint8_t lbuf [TERMINAL_BUFFER];

    while (total < length) {
        readbytes = MIN(length-total, TERMINAL_BUFFER);
        lbuf[0] = READ;
        if (addrbytes == 1) {
            lbuf[1] = addr & 0xff;
        } else {
            lbuf[1] = (addr >> 8) & 0xff;
            lbuf[2] = addr & 0xff;
        }
        result = bp_spi_command (bp, addrbytes+1, readbytes, lbuf);
        if (result == -1)
            return -1;
        memcpy (buffer+total, lbuf, readbytes);
        addr+=readbytes;
        total+=readbytes;
    }

    return 0;
}

int bp_spi_eeprom_verify (bp_state_t * bp, int addr, int length, int addrbytes, uint8_t * buffer) {
    int result, readbytes, total = 0;
    uint8_t lbuf [TERMINAL_BUFFER];

    while (total < length) {
        readbytes = MIN(length-total, TERMINAL_BUFFER);
        lbuf[0] = READ;
        if (addrbytes == 1) {
            lbuf[1] = addr & 0xff;
        } else {
            lbuf[1] = (addr >> 8) & 0xff;
            lbuf[2] = addr & 0xff;
        }
        result = bp_spi_command (bp, addrbytes+1, readbytes, lbuf);
        if (result == -1)
            return -1;
        if (memcmp (buffer+total, lbuf, readbytes))
            return 1;
        addr+=readbytes;
        total+=readbytes;
    }

    return 0;
}

int _bp_spi_eeprom_write (bp_state_t * bp, int addr, int length, int addrbytes, uint8_t * buffer) {
    uint8_t lbuf [TERMINAL_BUFFER];
    int result;

    if ((result = bp_spi_eeprom_rdsr (bp)) == -1)
        return -1;
    if (result & WIP) {
        usleep (5000);
        if ((result = bp_spi_eeprom_rdsr (bp)) == -1)
            return -1;
        if (result & WIP)
            return -2;
    }
    if (bp_spi_eeprom_wrenable (bp) == -1)
        return -1;
    if ((result = bp_spi_eeprom_rdsr (bp)) == -1)
        return -1;
    if (!(result & WEL))
        return -3;

    lbuf[0] = WRITE;
    if (addrbytes == 1) {
        lbuf[1] = addr & 0xff;
    } else {
        lbuf[1] = (addr >> 8) & 0xff;
        lbuf[2] = addr & 0xff;
    }
    memcpy (lbuf+3, buffer, length);
    if (bp_spi_command (bp, length+addrbytes+1, 0, lbuf) == -1)
        return -1;
    do {
        usleep (1000);
        if ((result = bp_spi_eeprom_rdsr (bp)) == -1)
            return -1;
        if (result & WEL)
            return -4;
    } while (result & WIP);

    return 0;
}

int bp_spi_eeprom_write (bp_state_t * bp, int addr, int length, int addrbytes, int pagesize, uint8_t * buffer) {
    int result, l;

    if (addr % pagesize) {
        l = pagesize - (addr % pagesize);
        if ((result = _bp_spi_eeprom_write (bp, addr, l, addrbytes, buffer)))
            return result;
        addr += l;
        buffer += l;
        length -= l;
    }
    while (length > pagesize) {
        if ((result = _bp_spi_eeprom_write (bp, addr, pagesize, addrbytes, buffer)))
            return result;
        addr += pagesize;
        buffer += pagesize;
        length -= pagesize;
    }
    if (length) {
        if ((result = _bp_spi_eeprom_write (bp, addr, length, addrbytes, buffer)))
            return result;
    }

    return 0;
}
