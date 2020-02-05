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

#ifndef FILESYSTEM_H
#define FILESYSTEM_H

#include <stddef.h>

#include "dds/export.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/time.h"

#if _WIN32
#include "dds/ddsrt/filesystem/windows.h"
#else
#include "dds/ddsrt/filesystem/posix.h"
#endif

#if defined (__cplusplus)
extern "C" {
#endif

struct ddsrt_stat {
  ddsrt_mode_t stat_mode;
  size_t stat_size;
  dds_time_t  stat_mtime;
};


struct ddsrt_dirent {
    char d_name[DDSRT_PATH_MAX + 1];
};

/** \brief opendir wrapper
 *
 * Open the directory conform opendir
 *
 * Precondition:
 *   none
 *
 * Possible results:
 * - return DDS_RETCODE_OK if directory 'name' is opened
 * - DDS_RETCODE_ERROR if 'name' could not
 *     be found or is not a directory.
 */
DDS_EXPORT dds_return_t ddsrt_opendir(const char *name, ddsrt_dirHandle *dir);

/** \brief closedir wrapper
 *
 * Close the directory conform closdir
 *
 * Precondition:
 *   none
 *
 * Possible results:
 * - return DDS_RETCODE_OK if directory identified by the handle
 *     is succesfully closed
 * - return DDS_RETCODE_ERROR if the handle is invalid.
 */
DDS_EXPORT dds_return_t ddsrt_closedir(ddsrt_dirHandle d);

/** \brief readdir wrapper
 *
 * Read the directory conform readdir.
 *
 * Precondition:
 *   none
 *
 * Possible results:
 * - return DDS_RETCODE_OK if next directory is found
 * - return DDS_RETCODE_ERROR if no more directories are found.
 */
DDS_EXPORT dds_return_t ddsrt_readdir(ddsrt_dirHandle d, struct ddsrt_dirent *direntp);

/** \brief stat wrapper
 *
 * Gets directory status conform stat.
 *
 * Precondition:
 *   none
 *
 * Possible results:
 * - return DDS_RETCODE_OK if stat is successful
 * - return DDS_RETCODE_ERROR if stat fails.
 */
DDS_EXPORT dds_return_t ddsrt_stat(const char *path, struct ddsrt_stat *buf);

/** \brief Transforms the given filepath into a platform specific filepath.
 *
 * This translation function will replace any platform file seperator into
 * the fileseperator of the current platform.
 *
 * Precondition:
 *   none
 *
 * Possible results:
 * - returns normalized filepath conform current platform
 * - return NULL if out of memory.
 */
DDS_EXPORT char* ddsrt_fileNormalize(const char *filepath);

/** \brief Get file seperator
 *
 * Possible Results:
 * - "<file-seperator-character>"
 */
DDS_EXPORT const char* ddsrt_fileSep(void);

#if defined (__cplusplus)
}
#endif

#endif // FILESYSTEM_H
