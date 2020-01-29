/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <string.h>

#include "dds/ddsrt/filesystem.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"

dds_return_t ddsrt_opendir(const char *name, ddsrt_dirHandle *dir)
{
    dds_return_t result;
    DIR *d;

    result = DDS_RETCODE_ERROR;
    if (dir) {
        d = opendir(name);
        if (d) {
            *dir = (ddsrt_dirHandle)d;
            result = DDS_RETCODE_OK;
        }
    }

    return result;
}

dds_return_t ddsrt_readdir(ddsrt_dirHandle d, struct ddsrt_dirent *direntp)
{
    dds_return_t result;
    DIR *dp = (DIR *)d;
    struct dirent *d_entry;

    result = DDS_RETCODE_ERROR;
    if (dp && direntp) {
        d_entry = readdir(dp);
        if (d_entry) {
            ddsrt_strlcpy(direntp->d_name, d_entry->d_name, sizeof(direntp->d_name));
            result = DDS_RETCODE_OK;
        }
    }

    return result;
}

dds_return_t ddsrt_closedir(ddsrt_dirHandle d)
{
    dds_return_t result;
    DIR *dp = (DIR *)d;

    result = DDS_RETCODE_ERROR;
    if (dp) {
        if (closedir(dp) == 0) {
            result = DDS_RETCODE_OK;
        }
    }

    return result;
}

dds_return_t ddsrt_stat(const char *path, struct ddsrt_stat *buf)
{
    dds_return_t result;
    struct stat _buf;
    int r;

    r = stat(path, &_buf);
    if (r == 0) {
        buf->stat_mode = _buf.st_mode;
        buf->stat_size = (size_t) _buf.st_size;
        buf->stat_mtime = DDS_SECS(_buf.st_mtime);
        result = DDS_RETCODE_OK;
    } else {
        result = DDS_RETCODE_ERROR;
    }

    return result;
}

char * ddsrt_fileNormalize(const char *filepath)
{
    char *norm;
    const char *fpPtr;
    char *normPtr;

    norm = NULL;
    if ((filepath != NULL) && (*filepath != '\0')) {
        norm = ddsrt_malloc(strlen(filepath) + 1);
        /* replace any / or \ by OS_FILESEPCHAR */
        fpPtr = (char *) filepath;
        normPtr = norm;
        while (*fpPtr != '\0') {
            *normPtr = *fpPtr;
            if ((*fpPtr == '/') || (*fpPtr == '\\')) {
                *normPtr = DDSRT_FILESEPCHAR;
                normPtr++;
            } else {
                if (*fpPtr != '\"') {
                    normPtr++;
                }
            }
            fpPtr++;
        }
        *normPtr = '\0';
    }

    return norm;
}

const char *ddsrt_fileSep(void)
{
    return "/";
}