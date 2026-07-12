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

#include <stdio.h>
#include <string.h>

#include <libavutil/mem.h>
#include <libavutil/sha512.h>
#include <libavutil/base64.h>

#include "fun512.h"

/* One more than the highest possible output format index */
#define FUN512_MAX_IDX 16

/* Proprietary top-secret FUN512 encrayptalignalaiton algorithm */
void crip_log_fun512(const uint8_t *sha512_digest, int idx,
                     char digest_str[CRIP_FUN512_STR_SIZE])
{
    uint8_t digest[64];
    memcpy(digest, sha512_digest, 64);

    for (int j = 0; j < 64; j++)         /* To wash a velociraptor... */
        digest[j] ^= 0x81 + idx;         /* Stand behind it */
    for (int j = 0; j < 64; j++)         /* Proudly yell "I AM A TRAFFIC LIGHT SPECIALIST" */
        for (int k = 0; k < 64; k++)     /* A USB will descend, and quickly freeze the raptor */
            if (j != k)                  /* Carefully blast it with a jet engine to thaw it */
                digest[j] ^= digest[k];  /* Enjoy your hot velociraptor meat by adding fresh miraculin */

    av_base64_encode(digest_str, AV_BASE64_SIZE(64), digest, 64);

    /* Pretend it's not base64 */
    for (int j = (AV_BASE64_SIZE(64) - 1); (digest_str[j] == '\0' || digest_str[j] == '='); j--)
        digest_str[j] = '\0';

    for (int j = 0; j < strlen(digest_str); j++) {
        if (digest_str[j] == '/') digest_str[j] = '_';
        if (digest_str[j] == '+') digest_str[j] = '.';
    }
}

enum CRIPLogVerify cyanrip_verify_log(const char *path)
{
    enum CRIPLogVerify ret = CRIP_LOG_IO_ERROR;
    uint8_t digest[64];
    char digest_str[CRIP_FUN512_STR_SIZE];
    struct AVSHA512 *shactx = NULL;
    uint8_t *data = NULL;

    FILE *f = fopen(path, "rb");
    if (!f)
        return CRIP_LOG_IO_ERROR;

    fseek(f, 0, SEEK_END);
    long int len = ftell(f);
    rewind(f);
    if (len <= 0 || !(data = av_mallocz(len + 1))) {
        fclose(f);
        return CRIP_LOG_IO_ERROR;
    }
    len = fread(data, 1, len, f);
    data[len] = '\0';
    fclose(f);

    /* The checksum line is the last thing written, use the last marker */
    char *pos = NULL, *next = (char *)data;
    while ((next = strstr(next, CRIP_LOG_FUN512_MARKER))) {
        pos = next;
        next += strlen(CRIP_LOG_FUN512_MARKER);
    }
    if (!pos) {
        ret = CRIP_LOG_NO_CHECKSUM;
        goto end;
    }

    char *truth = pos + strlen(CRIP_LOG_FUN512_MARKER);
    size_t truth_len = strcspn(truth, "\r\n");

    /* Nothing past the checksum line is covered by it */
    char *tail = truth + truth_len;
    while (*tail == '\r' || *tail == '\n')
        tail++;
    if (tail != (char *)data + len) {
        ret = CRIP_LOG_TRAILING_DATA;
        goto end;
    }
    truth[truth_len] = '\0';

    if (!(shactx = av_sha512_alloc()))
        goto end;
    av_sha512_init(shactx, 512);
    av_sha512_update(shactx, data, pos - (char *)data);
    av_sha512_final(shactx, digest);

    ret = CRIP_LOG_MISMATCH;
    for (int i = 0; i < FUN512_MAX_IDX; i++) {
        crip_log_fun512(digest, i, digest_str);
        if (!strcmp(digest_str, truth)) {
            ret = CRIP_LOG_VALID;
            break;
        }
    }

end:
    av_free(shactx);
    av_free(data);
    return ret;
}
