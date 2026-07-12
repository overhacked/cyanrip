/*
 * This file is part of cyanrip.
 *
 * cyanrip is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * cyanrip is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with cyanrip; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#pragma once

#include <stdint.h>

#define CRIP_LOG_FUN512_MARKER "Log FUN512: "

/* base64 of 64 bytes, including padding and NUL */
#define CRIP_FUN512_STR_SIZE 89

/* Compute the FUN512 string of a SHA-512 digest. idx is the index of the
 * output format the log belongs to, each simultaneous output is permuted
 * differently. */
void crip_log_fun512(const uint8_t *sha512_digest, int idx,
                     char digest_str[CRIP_FUN512_STR_SIZE]);

enum CRIPLogVerify {
    CRIP_LOG_VALID = 0,
    CRIP_LOG_MISMATCH,
    CRIP_LOG_NO_CHECKSUM,
    CRIP_LOG_TRAILING_DATA,
    CRIP_LOG_IO_ERROR,
};

/* Check a written log against its FUN512 checksum line */
enum CRIPLogVerify cyanrip_verify_log(const char *path);
