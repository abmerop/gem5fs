/*
 * Copyright (c) 2013, The Microsystems Design Laboratory (MDL)
 * Department of Computer Science and Engineering, The Pennsylvania State University
 * All rights reserved
 *
 * The license below extends only to copyright in the software and shall
 * not be construed as granting a license to any other intellectual
 * property including but not limited to intellectual property relating
 * to a hardware implementation of the functionality of the software
 * licensed hereunder.  You may use the software subject to the license
 * terms below provided that you ensure that this notice is replicated
 * unmodified and in its entirety in all distributions of the software,
 * modified or unmodified, in source code or in binary form.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met: redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer;
 * redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution;
 * neither the name of the copyright holders nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Authors: Matt Poremba
 */

/* Include everything - May be overkill. */
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/xattr.h>

const int exit_success = 0;
const int exit_warning = 0;
const int exit_failure = 1;

const char *testPath = "sandbox";

int fail(const char *testName)
{
    printf("%s test FAILED! errno is %d.\n", testName, errno);
    perror(testName);
    errno = 0;

    // Clean up the test directory if it exists.
    (void)rmdir(testPath);

    return exit_failure;
}


int main()
{
    int specialMode, readMode, writeMode, accessMode;
    int rv = 0;
    int warnings = 0;

    /* Ignore umask, this can fail the test. */
    mode_t user_mask = umask(0);

    /* Create a directory under each mode and check that the mode is correct. */
    printf("Testing mkdir, lstat, rmdir...\n");

    for (readMode = 0; readMode < 8; ++readMode)
    {
        for (writeMode = 0; writeMode < 8; ++writeMode)
        {
            for (accessMode = 0; accessMode < 8; ++accessMode)
            {
                mode_t dirMode = (readMode << 6) | (writeMode << 3) | accessMode;

                // Check for mkdir failure
                errno = 0;
                rv = mkdir(testPath, dirMode);

                if (rv < 0)
                    return fail("mkdir");

                // Check for lstat/permissions failure
                struct stat dirStat;
                
                errno = 0;
                rv = lstat(testPath, &dirStat);

                if (rv < 0)
                    return fail("lstat");

                // Only look at the lower 9 bits
                if ((dirStat.st_mode & 0777) != dirMode)
                {
                    printf("warning: mkdir mode is %d. Expected %d.\n", dirMode, dirStat.st_mode);
                    ++warnings;
                }

                // Check for rmdir failure
                errno = 0;
                rv = rmdir(testPath);

                if (rv < 0)
                    return fail("rmdir");
            }
        }
    }

    /* mkdir works, now test chmod. */
    printf("Testing chmod...\n");

    errno = 0;
    rv = mkdir(testPath, 0777);

    if (rv < 0)
        return fail("mkdir");

    for (specialMode = 0; specialMode < 8; ++specialMode)
    {
        for (readMode = 0; readMode < 8; ++readMode)
        {
            for (writeMode = 0; writeMode < 8; ++writeMode)
            {
                for (accessMode = 0; accessMode < 8; ++accessMode)
                {
                    mode_t dirMode = (specialMode << 9) | (readMode << 6) | (writeMode << 3) | accessMode;

                    // Check chmod for failure
                    errno = 0;
                    rv = chmod(testPath, dirMode);

                    if (rv < 0)
                        return fail("chmod");

                    // Check for permissions failure
                    struct stat dirStat;
                    
                    errno = 0;
                    rv = lstat(testPath, &dirStat);

                    if (rv < 0)
                        return fail("lstat");

                    // Only look at the lower 12 bits
                    if ((dirStat.st_mode & 07777) != dirMode)
                    {
                        printf("warning: chmod mode is %d. Expected %d.\n", dirMode, dirStat.st_mode);
                        ++warnings;
                    }
                }
            }
        }
    }

    errno = 0;
    rv = rmdir(testPath);

    if (rv < 0)
        return fail("rmdir");

    /* Test chown. */
    printf("Testing chown...\n");

    // Make a dummy directory
    errno = 0;
    rv = mkdir(testPath, 0777);

    if (rv < 0)
        return fail("mkdir");

    // Stat the directory to get the current uid/gid
    struct stat dirStat;

    errno = 0;
    rv = lstat(testPath, &dirStat);

    if (rv < 0)
        return fail("mkdir");

    // Just add one to each to make sure they change.
    uid_t owner = dirStat.st_uid + 1;
    gid_t group = dirStat.st_gid + 1;
    owner = 0;
    group = 0;
    
    // Change the uid and gid
    errno = 0;
    rv = chown(testPath, owner, group);

    if (rv < 0)
        return fail("chown");

    // Stat the directory again to check the change
    errno = 0;
    rv = lstat(testPath, &dirStat);

    if (rv < 0)
        return fail("lstat");

    if (dirStat.st_uid != owner)
    {
        printf("warning: chown uid is %d. Expected %d.\n", owner, dirStat.st_uid);
        ++warnings;
    }

    if (dirStat.st_gid != group)
    {
        printf("warning: chown gid is %d. Expected %d.\n", group, dirStat.st_gid);
        ++warnings;
    }

    // Remove the directory
    errno = 0;
    rv = rmdir(testPath);

    if (rv < 0)
        return fail("rmdir");

    /* Reset umask */
    (void)umask(user_mask);

    /* Print the test result. */
    printf("Test PASSED with 0 errors and %d warnings.\n", warnings);

    return exit_success;
}



