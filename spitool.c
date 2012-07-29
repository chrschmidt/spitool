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
#include <string.h>
#include <unistd.h>
#include <popt.h>
#include <fnmatch.h>
#include <errno.h>
#include <sys/ioctl.h>

#include "serial.h"
#include "buspirate.h"
#include "spitool_cmdline.h"

static void hexdump (int length, uint8_t * buffer) {
    int i, j;

    for (i=0; i<length; i+=16) {
        printf ("%08X: ", i);
        for (j=0; i+j<length && j<16; j++)
            printf ("%02X ", buffer[i+j]);
        if (j<16)
            for (; j<16; j++)
                printf ("   ");
        for (j=0; i+j<length && j<16; j++)
            printf ("%c", buffer[i+j]>=32 && buffer[i+j]<127 ? buffer[i+j] : '.');
        printf ("\n");
    }
}


static int _spitool_verify (bp_state_t * bp, spitool_action_t * action, uint8_t * buffer) {
    int result;

    printf ("Verifying EEPROM..."); fflush (stdout);
    switch ((result = bp_spi_eeprom_verify (bp, action->start, action->length,
                                            action->device.addresslength, buffer))) {
    case 0: printf (" Successfully verified.\n"); break;
    case 1: printf (" Difference encountered.\n"); break;
    default: printf (" Error occured.\n"); break;
    }

    return result;
}

static uint8_t * _spitool_read_eeprom (bp_state_t * bp, spitool_action_t * action) {
    uint8_t * buffer;

    if (!(buffer = malloc (action->length)))
        return NULL;

    printf ("Reading EEPROM...."); fflush (stdout);
    if (bp_spi_eeprom_read (bp, action->start, action->length, action->device.addresslength,
                            buffer)) {
        free (buffer);
        printf (" Error occured.\n");
        return NULL;
    }
    printf (" Done.\n");

    return buffer;
}

static uint8_t * _spitool_read_file (bp_state_t * bp, spitool_action_t * action) {
    uint8_t * buffer;
    FILE * infile;
    size_t result;

    if (!(buffer = malloc (action->length)))
        return NULL;

    if (!(infile = fopen (action->filename, "r"))) {
        fprintf (stderr, "Can't open file %s", action->filename);
        perror ("");
        free (buffer);
        return NULL;
    }

    result = fread (buffer, sizeof (uint8_t), action->length, infile);
    fclose (infile);
    if (result != action->length) {
        fprintf (stderr, "Failed to read %d bytes from %s\n", (int)action->length, action->filename);
        free (buffer);
        return NULL;
    }

    return buffer;
}

static int spitool_dump (bp_state_t * bp, spitool_action_t * action) {
    uint8_t * buffer;
    FILE * outfile;

    if (!(buffer = _spitool_read_eeprom (bp, action)))
        return 1;

    if (!action->filename) {
        hexdump (action->length, buffer);
    } else {
        if ((outfile = fopen (action->filename, "w+"))) {
            fwrite (buffer, action->length, 1, outfile);
            fclose (outfile);
        }
    }
    free (buffer);
    return 0;
}

static int spitool_verify (bp_state_t * bp, spitool_action_t * action) {
    uint8_t * buffer;
    size_t result;

    if (!(buffer = _spitool_read_file (bp, action)))
        return 1;

    result = _spitool_verify (bp, action, buffer);

    free (buffer);
    if (result)
        return 1;
    return 0;
}

static int spitool_program (bp_state_t * bp, spitool_action_t * action) {
    uint8_t * buffer1 = NULL, * buffer2 = NULL;
    int result = 0;
    int i, j;
    int update;
    int mode = 0;
    unsigned long wipeval = 0xff;
    const char modes[3][9] = {"Writing", "Updating", "Wiping"};

    if (!strcmp (action->command->commandname, "update")) mode = 1;
    else if (!strcmp (action->command->commandname, "wipe")) mode = 2;

    // source file is only needed for mode 0/1 (write/update)
    if (mode < 2 && !(buffer1 = _spitool_read_file (bp, action)))
        return 1;
    // current eeprom content is only needed for mode 1/2 (update/wipe)
    if (mode > 0 && !(buffer2 = _spitool_read_eeprom (bp, action))) {
        free (buffer1);
        return 1;
    }
    if (mode == 2) {
        buffer1 = buffer2;
        buffer2 = NULL;
        if (action->arg[0]) {
            wipeval = strtoul (action->arg[0], NULL, 0);
            if (errno || wipeval > 255) {
                fprintf (stderr, "Parameter %s is invalid for wipe.\n", action->arg[0]);
                return 1;
            }
        }
    }

    printf ("%s EEPROM...\n", modes[mode]); fflush (stdout);
    for (i=0; i<action->device.capacity; i+=action->device.sectorsize) {
        switch (mode) {
        case 0:
            update = 1;
            break;
        case 1:
        case 2:
            update = 0;
            for (j=0; j<action->device.sectorsize; j++) {
                if (buffer1 [i+j] != ((mode == 1) ? buffer2 [i+j] : wipeval)) {
                    if (mode == 2)
                        memset (buffer1+i, wipeval, action->device.sectorsize);
                    update = 1;
                    break;
                }
            }
            break;
        }
        if (update) {
            printf ("  %s sector %d...", modes[mode],
                                       i/action->device.sectorsize); fflush (stdout);
            if ((result = bp_spi_eeprom_write (bp, i, action->device.sectorsize,
                                               action->device.addresslength,
                                               action->device.sectorsize, buffer1+i)))
                break;
            printf ("\n");
        }
    }
    if (result) printf ("Failed.\n");
    else printf ("Done.\n");

    if (!result && action->verify)
        result = _spitool_verify (bp, action, buffer1);

    free (buffer2);
    free (buffer1);
    if (result)
        return 1;
    return 0;
}

static int spitool_rdsr (bp_state_t * bp, spitool_action_t * action) {
    int result;

    result = bp_spi_eeprom_rdsr (bp);
    if (result >= 0) {
        printf ("Status Register is 0x%02X\n", result);
        return 0;
    } else {
        printf ("Status Register read failed.\n");
        return 1;
    }
}

static int spitool_wrsr (bp_state_t * bp, spitool_action_t * action) {
    unsigned long parameter;

    errno = 0;
    parameter = strtoul (action->arg[0], NULL, 0);
    if (errno || parameter > 255) {
        fprintf (stderr, "Parameter %s is invalid for wrsr.\n", action->arg[0]);
        return 1;
    }
    if (bp_spi_eeprom_wrsr (bp, parameter))
        return 1;

    return 0;
}

static int spitool_sniff (bp_state_t * bp, spitool_action_t * action) {
    uint8_t buffer [5];
    int result;
    fd_set set;
    struct termio orig, new;

/*    if (!(buffer = malloc (1<<20)))
        return 1;
*/
    serWriteChar (bp->fd, BPSPISNIFFALL);
    result = serReadCharTimed (bp->fd, 10000);
    if (result == 1) {
        ioctl (STDIN_FILENO, TCGETA, &orig);
        ioctl (STDIN_FILENO, TCGETA, &new);
        new.c_lflag &= ~ECHO & ~ICANON;
        ioctl (STDIN_FILENO, TCSETA, &new);
        result = 0;
        printf ("Press any key to abort\n");
        do {
            FD_ZERO (&set);
            FD_SET (bp->fd, &set);
            FD_SET (STDIN_FILENO, &set);
            result = select (bp->fd+1, &set, NULL, NULL, NULL);
            if (FD_ISSET (bp->fd, &set)) {
                result = serReadTimed (bp->fd, 10000, 5, buffer);
                printf ("%c%c '%c' %02X %3d - '%c' %02X %3d %c\n",
                        buffer[0], buffer[1],
                        buffer[2]>=32 && buffer[2]<=127 ? buffer[2] : '.', buffer[2], buffer[2],
                        buffer[3]>=32 && buffer[3]<=127 ? buffer[3] : '.', buffer[3], buffer[3],
                        buffer[4]);
            }
        } while (!FD_ISSET (STDIN_FILENO, &set));
        serWriteChar (bp->fd, 0);
        getchar ();
        ioctl (STDIN_FILENO, TCSETA, &orig);
    } else {
        result = 1;
    }
//    free (buffer);

    if (result == 0)
        return 0;
    else
        return 1;
}

const spitool_command_t commands [] = {
    { "dump", spitool_dump, CFNEEDAS | CFNEEDDS },
    { "program", spitool_program, CFNEEDAS | CFNEEDDS | CFNEEDSS | CFNEEDFILE },
    { "update", spitool_program, CFNEEDAS | CFNEEDDS | CFNEEDSS | CFNEEDFILE },
    { "wipe", spitool_program, CFNEEDAS | CFNEEDDS | CFNEEDSS | CFOPTARG },
    { "verify", spitool_verify, CFNEEDAS | CFNEEDDS | CFNEEDFILE },
    { "rdsr", spitool_rdsr, 0 },
    { "wrsr", spitool_wrsr, CFNEEDARG },
    { "sniff", spitool_sniff, 0 },
    { NULL, NULL, 0 }
};

int main (int argc, const char ** argv) {
    bp_state_t bp = {
        .speed = 1000,
        .devicename = "/dev/ttyUSB0",
        .devicerate = B115200,
        .flags = BPSPICFGAUX | BPSPICFGOUTPUT | BPSPICFGPOWER | BPSPICFGCLOCKEDGE
    };
    spitool_action_t * action;

    action = parse_commandline (argc, argv, commands, &bp);

    if (action && !bp_open (&bp)) {
        printf ("Bus Pirate %d.%d, Firmware %d.%d (r%d), Bootloader %d.%d found.\n",
                bp.hw_version/100, bp.hw_version%100,
                bp.sw_version/100, bp.sw_version%100,
                bp.sw_revision,
                bp.bl_version/100, bp.bl_version%100);

        if (bp_spi_enter (&bp))
            return 1;
        printf ("Entered binary SPI mode version %d.\n", bp.bm_version);

        if (!action->command->action (&bp, action))
            printf ("Command %s completed successfully.\n", action->command->commandname);
        else
            printf ("Command %s failed.\n", action->command->commandname);

        bp_mode (&bp, BPMTERMINAL);
        close (bp.fd);
    }
    return 0;
}
