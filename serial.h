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

#ifndef __SERIAL_H__
#define __SERIAL_H__

#include <termios.h>
#include <stdint.h>

enum serWriteLineFlags {
    SWLFCECHO = 1             /* Cancel echoed chracters by reading back the sent line */
};

int serOpenPort (const char *devicename, tcflag_t rate);
int serSetSpeed (int fd, tcflag_t newrate);
int serRead (int fd, int len, uint8_t *buf);
int serReadTimed (int fd, int timeout, int len, uint8_t *buf);
int serReadCharTimed (int fd, int timeout);
int serReadLine (int fd, int maxlen, char *line);
int serWrite (int fd, int length, const uint8_t *buffer);
int serWriteChar (int fd, const uint8_t c);
int serWriteLine (int fd, int flags, const char *line);

#endif
