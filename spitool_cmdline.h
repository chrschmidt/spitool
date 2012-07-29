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

#ifndef __SPITOOL_CMDLINE_H__
#define __SPITOOL_CMDLINE_H__

#include <inttypes.h>
#include "buspirate.h"

enum SPITOOLCMDFLAGS {
    CFNEEDAS   = 0x0001,      // Command requires Address Size info
    CFNEEDDS   = 0x0002,      // Command requires Device Size info
    CFNEEDSS   = 0x0004,      // Command requires Sector Size info
    CFNEEDFILE = 0x0008,      // Command requires a filename for input/output
    CFNEEDARG  = 0x0010,      // Command requires an argument
    CFOPTARG   = 0x0020       // Command can have an argument
};

typedef struct spitool_command_s spitool_command_t;

typedef struct spitool_action_s {
    char * filename;
    int start;
    size_t length;
    int verify;
    const char * arg;
    bp_device_t device;
    const spitool_command_t * command;
} spitool_action_t;

typedef int (*action_t) (bp_state_t *, spitool_action_t *);

typedef struct spitool_command_s {
    char * commandname;
    action_t action;
    int flags;
} spitool_command_t;

spitool_action_t * parse_commandline (int argc, const char ** argv,
                                      const spitool_command_t * commands,
                                      bp_state_t * bp);

#endif
