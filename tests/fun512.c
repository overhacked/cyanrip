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
#include <stdlib.h>
#include <string.h>

#include <libavutil/sha512.h>
#include <libavutil/mem.h>

#include "fun512.h"

#define BODY "cyanrip FUN512 test vector\n"

/* Precomputed FUN512 of BODY for output indices 0 and 1 */
#define BODY_FUN512_0 \
    "yeSm9QaxaDSB2ZGPkY9tC.dSvAdPrPSAzIEp0y_sIyYH7B9w0lbIuQmYoimZ0Mf6CbLwzYm_za_y4MKvwSQS4Q"
#define BODY_FUN512_1 \
    "yeel9gWyazeC2pKMkoxuCORRvwRMr_eDz4Iq0CzvICUE7xxz0VXLugqboSqa08T5CrHzzoq8zqzx48GswicR4g"

static int fails = 0;

static void check_str(const char *what, const char *got, const char *want)
{
    if (strcmp(got, want)) {
        printf("FAIL: %s:\n    got:  %s\n    want: %s\n", what, got, want);
        fails++;
    }
}

static void check_verify(const char *what, const char *contents,
                         enum CRIPLogVerify want)
{
    char path[] = "/tmp/crip_fun512_XXXXXX";
    int fd = mkstemp(path);
    if (fd < 0) {
        printf("FAIL: mkstemp\n");
        fails++;
        return;
    }
    FILE *f = fdopen(fd, "wb");
    fwrite(contents, 1, strlen(contents), f);
    fclose(f);

    enum CRIPLogVerify got = cyanrip_verify_log(path);
    if (got != want) {
        printf("FAIL: %s: verification returned %i, wanted %i\n",
               what, got, want);
        fails++;
    }
    remove(path);
}

int main(void)
{
    uint8_t digest[64];
    char str[CRIP_FUN512_STR_SIZE];
    char log[1024];

    /* The checksum computation itself, against known vectors */
    struct AVSHA512 *shactx = av_sha512_alloc();
    if (!shactx)
        return 1;
    av_sha512_init(shactx, 512);
    av_sha512_update(shactx, (const uint8_t *)BODY, strlen(BODY));
    av_sha512_final(shactx, digest);
    av_free(shactx);

    crip_log_fun512(digest, 0, str);
    check_str("FUN512 idx 0", str, BODY_FUN512_0);

    crip_log_fun512(digest, 1, str);
    check_str("FUN512 idx 1", str, BODY_FUN512_1);

    /* Log verification, all outcomes */
    snprintf(log, sizeof(log), "%s" CRIP_LOG_FUN512_MARKER "%s\n",
             BODY, BODY_FUN512_0);
    check_verify("valid log", log, CRIP_LOG_VALID);

    snprintf(log, sizeof(log), "%s" CRIP_LOG_FUN512_MARKER "%s\n",
             BODY, BODY_FUN512_1);
    check_verify("valid log, second output", log, CRIP_LOG_VALID);

    snprintf(log, sizeof(log), "not the body\n" CRIP_LOG_FUN512_MARKER "%s\n",
             BODY_FUN512_0);
    check_verify("tampered log", log, CRIP_LOG_MISMATCH);

    snprintf(log, sizeof(log), "%s" CRIP_LOG_FUN512_MARKER "%s\nappended\n",
             BODY, BODY_FUN512_0);
    check_verify("appended data", log, CRIP_LOG_TRAILING_DATA);

    check_verify("no checksum", BODY, CRIP_LOG_NO_CHECKSUM);

    if (cyanrip_verify_log("/nonexistent/log") != CRIP_LOG_IO_ERROR) {
        printf("FAIL: missing file did not report an I/O error\n");
        fails++;
    }

    if (fails) {
        printf("%i check(s) failed\n", fails);
        return 1;
    }
    printf("all checks passed\n");
    return 0;
}
