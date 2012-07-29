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

#include <popt.h>
#include <fnmatch.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "buspirate.h"
#include "spitool_cmdline.h"

static const bp_device_t spi_devices [] = {
    { "list", 0, 0, 0, BPDFDUMMY },
    { "M95640*", 8192, 2, 32, 0, BPDFEEPROM }
};

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[1]))
#endif

static int parse_flags (const char * flags, int * output) {
    if (!strcmp (flags, "help")) {
	printf ("Valid Flags:\n");
	printf ("================================================================================\n");
	printf ("AUX Pin Control:            a Output Low (0)       A Output High (1) (default)\n");
	printf ("                            @ Input (Z, High Impedance)\n");
	printf ("  Note: Specifying a or A before @ will define the state during CS toggle\n");
	printf ("Voltage Regulators Control: v Off                  V On (default)\n");
	printf ("Pullup Control:             p Off                  P On (default)\n");
	printf ("Output Level Control:       h 0V/Hi-Z (Default)    H 0V/+3.3V\n");
	printf ("CS Pin Control:             c Active Low (default) C Active High\n");
	printf ("SPI Clock Idle Polarity:    i Low (default)        I High\n");
	printf ("SPI Sampling Control:       s Middle (default)     S End\n");
	printf ("SPI Output Clock Edge:      o Idle to Active       O Active to Idle (default)\n");
	return 1;
    }
    while (*flags) {
        switch (*(flags++)) {
        case '@': *output |= BPSPICFGAUXINPUT; break;
        case 'a': *output &= ~BPSPICFGAUX; *output &= ~BPSPICFGAUXINPUT; break;
        case 'A': *output |= BPSPICFGAUX; *output &= ~BPSPICFGAUXINPUT; break;
        case 'c': *output &= ~BPSPICFGCS; break;
        case 'C': *output |= BPSPICFGCS; break;
        case 'h': *output &= ~BPSPICFGOUTPUT; break;
        case 'H': *output |= BPSPICFGOUTPUT; break;
        case 'i': *output &= ~BPSPICFGIDLE; break;
        case 'I': *output |= BPSPICFGIDLE; break;
        case 'o': *output &= ~BPSPICFGCLOCKEDGE; break;
        case 'O': *output |= BPSPICFGCLOCKEDGE; break;
        case 'p': *output &= ~BPSPICFGPULLUPS; break;
        case 'P': *output |= BPSPICFGPULLUPS; break;
        case 's': *output &= ~BPSPICFGSAMPLING; break;
        case 'S': *output |= BPSPICFGSAMPLING; break;
        case 'v': *output &= ~BPSPICFGPOWER; break;
        case 'V': *output |= BPSPICFGPOWER; break;
        default: fprintf (stderr, "Invalid flag %c, allowed are [@aAcChHiIoOpPsSvV]\n", *(flags-1)); return 1;
        }
    }
    return 0;
}

static int check_device (bp_device_t * device) {
    int i;

    if (!device->devicename)
        return 0;
    if (!strcmp (device->devicename, "list")) {
        printf ("Device name          Type        Capacity           Sector Size\n");
        printf ("===============================================================\n");
        for (i=0; i<ARRAY_SIZE (spi_devices); i++)
            if (spi_devices[i].flags != BPDFDUMMY)
                printf ("%-20s SPI %-6s, %11d bytes, %5d bytes\n",
                        spi_devices[i].devicename,
                        spi_devices[i].flags == BPDFEEPROM ? "EEPROM" :
                                                             "FLASH",
                        spi_devices[i].capacity, spi_devices[i].sectorsize);
        return 1;
    }

    for (i=0; i<ARRAY_SIZE (spi_devices); i++)
        if (!fnmatch (spi_devices[i].devicename, device->devicename, FNM_PATHNAME|FNM_CASEFOLD)) {
            if (!device->capacity)
                device->capacity = spi_devices[i].capacity;
            if (!device->sectorsize)
                device->sectorsize = spi_devices[i].sectorsize;
            if (!device->addresslength)
                device->addresslength = spi_devices[i].addresslength;
            break;
        }
    return 0;
}

static int check_speed (int * speed) {
    switch (*speed) {
    case 30:   *speed = BPSPISPEED30K;  break;
    case 125:  *speed = BPSPISPEED125K; break;
    case 250:  *speed = BPSPISPEED250K; break;
    case 1000: *speed = BPSPISPEED1M;   break;
    case 2000: *speed = BPSPISPEED2M;   break;
    case 2600:
    case 2666: *speed = BPSPISPEED2M6;  break;
    case 4000: *speed = BPSPISPEED4M;   break;
    case 8000: *speed = BPSPISPEED8M;   break;
    default:
        fprintf (stderr, "Invalid speed (%dkHz). Valid values are 30, 125, 250, 1000, 2000, 2600, 4000, 8000.\n", *speed);
        return 1;
    }
    return 0;
}

static char * make_commandlist (const spitool_command_t * commands,
                                const char * prefix, const char * postfix) {
    int i, length = 0;
    char * buffer;
    const char argstr[] = "argument";

    if (prefix)
        length += strlen (prefix) + 1;
    if (postfix)
        length += strlen (postfix) + 1;

    for (i=0; commands[i].commandname; i++) {
        length += strlen (commands[i].commandname) + 1;
        if (commands[i].flags & CFNEEDARG)
            length += strlen (argstr) + 1;
    }

    if (!(buffer = malloc (length)))
        return NULL;
    buffer [0] = 0;

    if (prefix)
        strcat (buffer, prefix);
    for (i=0; commands[i].commandname; i++) {
        if (i)
            strcat (buffer, "|");
        strcat (buffer, commands[i].commandname);
        if (commands[i].flags & CFNEEDARG) {
            strcat (buffer, " ");
            strcat (buffer, argstr);
        }
    }
    if (postfix)
        strcat (buffer, postfix);

    if (strlen (buffer) > length) {
        fprintf (stderr, "buffer overrun in make_commandlist()!\n");
        abort ();
    }

    return buffer;
}

const spitool_command_t * check_command (poptContext * context, const spitool_command_t * commands) {
    int i;
    const char * command;

    if (!(command = poptGetArg (*context))) {
        fprintf (stderr, "No command given.\n");
        return NULL;
    }

    for (i=0; commands[i].commandname; i++)
        if (!strcmp (command, commands[i].commandname))
            return &commands[i];

    fprintf (stderr, "Invalid command \"%s\" given.\n", command);
    return NULL;
}

spitool_action_t * parse_commandline (int argc, const char ** argv,
                                      const spitool_command_t * commands,
                                      bp_state_t * bp) {
    spitool_action_t * action;
    int intarg;
    const struct poptOption cmdlineopts [] = {
        { "clockspeed", 'c', POPT_ARG_INT | POPT_ARGFLAG_SHOW_DEFAULT, &intarg, 'c',
          "SPI clock speed in kHz", "<30, 125, 250, 1000, 2000, 2600, 4000, 8000>" },
        { "flags", 'a', POPT_ARG_STRING, NULL, 'a',
          "SPI operation flags", "[@aAcChHiIoOpPsSvV]" },
        { "port", 'p', POPT_ARG_STRING, NULL, 'p',
          "path to bus pirate serial port's device node", "<string>" },
        { "portspeed", 'P', POPT_ARG_INT, &intarg, 'P',
          "Extended serial port speed", "<1..4>" },
        { "filename", 'f', POPT_ARG_STRING, NULL, 'f',
          "file to read/write data to", "<string>" },

        { "device", 'd', POPT_ARG_STRING, NULL, 'd',
          "devicetype that is connected", "<string|list>" },
        { "as", 0, POPT_ARG_INT, &intarg, 0x100,
          "device address length in bytes", "<integer>" },
        { "ds", 0, POPT_ARG_INT, &intarg, 0x101,
          "device size in bytes", "<integer>" },
        { "ss", 0, POPT_ARG_INT, &intarg, 0x102,
          "device sector size in bytes", "<integer>" },

        { "verify", 'v', POPT_ARG_NONE, NULL, 'v',
          "verify after write", NULL },

        POPT_AUTOHELP
        POPT_TABLEEND
    };
    poptContext optcon;
    int c;
    char * commandlist = make_commandlist (commands, "<", ">");

    if (!(action = calloc (1, sizeof (spitool_action_t))))
        return NULL;

    optcon = poptGetContext (NULL, argc, argv, cmdlineopts, 0);

    if (commandlist);
        poptSetOtherOptionHelp (optcon, commandlist);

    while ((c = poptGetNextOpt (optcon)) >= 0) {
        switch (c) {
        case 'a': if (parse_flags (poptGetOptArg (optcon), &bp->flags)) goto errout; break;
        case 'c': bp->speed = intarg; break;
        case 'd': action->device.devicename = poptGetOptArg (optcon); break;
        case 'f': action->filename = poptGetOptArg (optcon); break;
        case 'p': bp->devicename = poptGetOptArg (optcon); break;
        case 'P': switch (intarg) {
            case 1: bp->devicerate = B230400; break;
            case 2: bp->devicerate = B460800; break;
            case 3: bp->devicerate = B1000000; break;
            case 4: bp->devicerate = B2000000; break;
            default: fprintf (stderr, "Invalid extended serial port speed %d\n", intarg); goto errout;
            }
            break;
        case 'v': action->verify = 1; break;
        case 0x100: action->device.addresslength = intarg; break;
        case 0x101: action->device.capacity = intarg; break;
        case 0x102: action->device.sectorsize = intarg; break;
        }
    }
    if (c < -1) {
        poptPrintUsage (optcon, stderr, 0);
        fprintf(stderr, "%s: %s\n",
                poptBadOption(optcon, POPT_BADOPTION_NOALIAS),
                poptStrerror(c));
        goto errout;
    }

    if (check_device (&action->device) ||
        check_speed (&bp->speed))
        goto errout;

    if (!(action->command = check_command (&optcon, commands))) {
        poptPrintUsage (optcon, stderr, 0);
        goto errout;
    }

    if (action->command->flags & CFNEEDARG && !action->filename) {
        action->arg = poptGetArg (optcon);
        if (!action->arg) {
            fprintf (stderr, "Command %s needs an argument, but none supplied.\n",
                     action->command->commandname);
            goto errout;
        }
    }
    if (action->command->flags & CFNEEDAS && !action->device.addresslength) {
        fprintf (stderr, "Command %s needs SPI address length information.\n",
                 action->command->commandname);
        goto errout;
    }
    if (action->command->flags & CFNEEDDS && !action->device.capacity) {
        fprintf (stderr, "Command %s needs device capacity information.\n",
                 action->command->commandname);
        goto errout;
    }
    if (action->command->flags & CFNEEDSS && !action->device.sectorsize) {
        fprintf (stderr, "Command %s needs device sector size information.\n",
                 action->command->commandname);
        goto errout;
    }
    if (action->command->flags & CFNEEDFILE && !action->filename) {
        fprintf (stderr, "Command %s needs a filename for I/O.\n",
                 action->command->commandname);
        goto errout;
    }

    if (action->length == 0)
        action->length = action->device.capacity;

    poptFreeContext(optcon);
    return action;

errout:
    if (commandlist) free (commandlist);
    if (action) free (action);
    poptFreeContext(optcon);
    return NULL;
}
