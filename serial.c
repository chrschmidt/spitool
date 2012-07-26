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

#include <sys/types.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include "serial.h"

#define TIMEOUT 100000

int serOpenPort (const char *devicename, tcflag_t rate) {
    int fd;
    struct termios newtio;

    fd = open(devicename, O_RDWR | O_NOCTTY);
    if (fd < 0) {
	perror("open");
	return -1;
    }

    // set new port settings for canonical input processing
    bzero (&newtio, sizeof (newtio));
    newtio.c_cflag = rate;
    newtio.c_iflag = IGNPAR;
    newtio.c_cc[VMIN] = 0;
    newtio.c_cc[VTIME] = 1;
    tcflush(fd, TCIFLUSH);
    if (tcsetattr(fd, TCSANOW, &newtio)) {
	perror("tcsetattr");
	return -1;
    }
    return fd;
}

int serSetSpeed (int fd, tcflag_t newrate) {
   struct termios tio;

   if (tcgetattr(fd, &tio)) {
       perror("tcgetattr");
       return -1;
   }
   if (cfsetispeed (&tio, newrate)) {
       perror("cfsetispeed");
       return -1;
   }
   if (cfsetospeed (&tio, newrate)) {
       perror("cfsetospeed");
       return -1;
   }
   if (tcsetattr(fd, TCSANOW, &tio)) {
       perror("tcsetattr");
       return -1;
   }
   return 0;
}

int serRead (int fd, int len, uint8_t *buf) {
    return serReadTimed (fd, TIMEOUT, len, buf);
}

int serReadTimed (int fd, int timeout, int len, uint8_t *buf) {
    fd_set set;
    struct timeval tv;
    int total = 0, result;

    do {
	FD_ZERO(&set);
	FD_SET(fd, &set);
	tv.tv_sec = timeout / 1000000;
	tv.tv_usec = timeout % 1000000;
	result = select (fd + 1, &set, NULL, NULL, &tv);
	switch (result) {
        case -1: // Error
            perror ("select");
            return -1;
        case 0: // Timeout
            return total;
        case 1: // Data available
            if ((result = read (fd, buf+total, len-total)) == -1) {
                perror ("read");
                return -1;
            }
            total += result;
        }
    } while (total < len);
    return total;
}

int serReadCharTimed (int fd, int timeout) {
    uint8_t c;

    if (serReadTimed (fd, timeout, 1, &c) == 1)
        return c;
    else
        return -1;
}


int serReadLine (int fd, int maxlen, char *line) {
    uint8_t inputChar;
    int pos = 0;
    int finished = 0;
    int result;

    line[0] = 0;

    do {
        result = serRead (fd, 1, &inputChar);
	switch (result) {
        case -1: // Error
            return -1;
        case 0: // Timeout
            if (pos)
                return pos;
            else
                return -1;
        case 1: // Data read
            switch (inputChar) {
            case 0x0d:	// Ignore Linefeed
                break;
            case 0x0a:	// Carriage return
                finished = 1;
                break;
            default:
                line[pos++] = inputChar;
                line[pos] = 0;
                if (pos == maxlen)
                    finished = 1;
            }
            break;
	}
    } while (!finished);

    return strlen(line);
}

int serWrite (int fd, int length, const uint8_t *buffer) {
    int result, written = 0;

    while (written<length) {
        if ((result = write (fd, buffer+written, length-written)) == -1) {
            perror ("serWriteLine/write");
            return -1;
        }
        written += result;
    }
    return written;
}

int serWriteChar (int fd, const uint8_t c) {
    return serWrite (fd, 1, &c);
}

int serWriteLine (int fd, int flags, const char *line) {
    char buf[256];
    int written=0;

    written = serWrite (fd, strlen(line), (const uint8_t *) line);
    if (written > 0 && (flags & 1))
        serReadLine(fd, sizeof(buf), buf);
    return written;
}
