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

#ifndef __BPSERIAL_H__
#define __BPSERIAL_H__

#include <stdint.h>
#include <termios.h>

#define TERMINAL_BUFFER 4096  // From buspirate firmware, busPirateCore.h

enum BPMODES {
    BPMUNKNOWN,
    BPMTERMINAL,              // Base terminal mode
    BPMBINARY                 // Base binary mode
};

enum BPSUBMODES {
    BPSMUNKNOWN,
    BPSMHIZ,
    BPSMSPI
};

typedef struct bp_state_s {
    char * devicename;
    tcflag_t devicerate;
    int fd;
    int mode;
    int submode;
    int flags;
    int speed;
    int bm_version;
    int hw_version;
    int sw_version;
    int bl_version;
} bp_state_t;

enum BPDEVICEFLAGS {
    BPDFDUMMY  = 0,
    BPDFEEPROM = 1,
    BPDFFLASH  = 2
};

typedef struct bp_device_s {
    char * devicename;
    int capacity;
    int addresslength;
    int sectorsize;
    int pagesize;
    int flags;
} bp_device_t;

enum BPPINS {
    BPPCS              = 0x01,
    BPPMISO            = 0x02,
    BPPCLK             = 0x04,
    BPPMOSI            = 0x08,
    BPPAUX             = 0x10,
    BPPPULLUP          = 0x20,
    BPPVREG            = 0x40
};

enum BPBINARYCMDS {
    BPBCENTER          = 0x00,
    BPBCENTERSPI       = 0x01,
    BPBCENTERI2C       = 0x02,
    BPBCENTERUART      = 0x03,
    BPBCENTER1WIRE     = 0x04,
    BPBCENTERRAWWIRE   = 0x05,
    BPBCENTEROPENOCD   = 0x06,
    BPBCENTERPICPROG   = 0x07,
    BPBCRESET          = 0x0f,

    BPBCSELFTTESTSHORT = 0x10,
    BPBCSELFTTESTLONG  = 0x11,
    BPBCSETPWM         = 0x12,
    BPBCCLEARPWM       = 0x13,
    BPBCREADADCONCE    = 0x14,
    BPBCREADADCCONT    = 0x15,

    BPBCSETPINDIR      = 0x40,
    BPBCSETPINDATA     = 0x80
};

enum BPSPICMDS {
    BPSPIEXIT          = 0x00,
    BPSPIENTER         = 0x01,
    BPSPICSLO          = 0x02,
    BPSPICSHI          = 0x03,
    BPSPIWRITEREADCS   = 0x04,
    BPSPIWRITEREADNOCS = 0x05,
    BPSPISNIFFALL      = 0x0d,
    BPSPISNIFFCSLO     = 0x0e,
    BPSPISNIFFCSHI     = 0x0f,
    BPSPIWRITE         = 0x10,
    BPSPICONFIG1       = 0x40,
    BPSPIREADAUX       = 0x50,
    BPSPISETSPEED      = 0x60,
    BPSPICONFIG2       = 0x80
};

enum BPSPICFG {
    BPSPICFGCS         = 0x01,
    BPSPICFGAUX        = 0x02,
    BPSPICFGPULLUPS    = 0x04,
    BPSPICFGPOWER      = 0x08,
    BPSPICFGSAMPLING   = 0x10,
    BPSPICFGCLOCKEDGE  = 0x20,
    BPSPICFGIDLE       = 0x40,
    BPSPICFGOUTPUT     = 0x80,

    BPSPICFGAUXINPUT   = 0x100,

    BPSPISPEED30K      = 0x00,
    BPSPISPEED125K     = 0x01,
    BPSPISPEED250K     = 0x02,
    BPSPISPEED1M       = 0x03,
    BPSPISPEED2M       = 0x04,
    BPSPISPEED2M6      = 0x05,
    BPSPISPEED4M       = 0x06,
    BPSPISPEED8M       = 0x07
};

enum BPSPISHORTFLAGS {
    WR1RD0             = 0x00,
    WR2RD0             = 0x01,
    WR1RD1             = 0x02,
    WR2RD1             = 0x03
};

int bp_open (bp_state_t * bp);
int bp_reset (bp_state_t * bp);
int bp_mode (bp_state_t * bp, int newmode);

int bp_spi_enter (bp_state_t * bp);
int bp_spi_command (bp_state_t * bp, int writelen, int readlen, uint8_t * buffer);
int bp_spi_command_short (bp_state_t * bp, int flags, uint8_t command, uint8_t data);

int bp_spi_eeprom_rdsr (bp_state_t * bp);
int bp_spi_eeprom_wrsr (bp_state_t * bp, uint8_t data);
int bp_spi_eeprom_wrenable (bp_state_t * bp);
int bp_spi_eeprom_wrdisable (bp_state_t * bp);
int bp_spi_eeprom_read (bp_state_t * bp, int addr, int length, int addrbytes, uint8_t * buffer);
int bp_spi_eeprom_verify (bp_state_t * bp, int addr, int length, int addrbytes, uint8_t * buffer);
int bp_spi_eeprom_write (bp_state_t * bp, int addr, int length, int addrbytes, int pagesize, uint8_t * buffer);

#endif
