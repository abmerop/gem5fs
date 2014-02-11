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
const char *linkName = "sandbox2";

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
    int rv = 0;
    int warnings = 0;

    /* Ignore umask, this can fail the test. */
    mode_t user_mask = umask(0);

    /* mkdir works, now test chmod. */
    printf("Testing symlink...\n");

    errno = 0;
    rv = mkdir(testPath, 0777);

    if (rv < 0)
        return fail("mkdir");

    // Make a symlink to the directory.
    errno = 0;
    rv = symlink(testPath, linkName);

    if (rv < 0)
        return fail("symlink");

    /* Test readlink. */
    printf("Testing readlink...\n");

    // should be only strlen(linkName) + 1, but test for extraneous characters
    char readName[PATH_MAX];

    errno = 0;
    rv = readlink(linkName, readName, PATH_MAX);

    if (rv < 0)
        return fail("readlink");

    if (strcmp(testPath, readName) != 0)
    {
        printf("Warning: readlink read %s. Expected %s.\n", readName, testPath);
        ++warnings;
    }


    // Remove the link
    errno = 0;
    rv = unlink(linkName);

    if (rv < 0)
        return fail("unlink");

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



