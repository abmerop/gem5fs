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
const char *testFile1 = "foo";
const char *testFile2 = "bar";

int fail(const char *testName)
{
    char filepath[PATH_MAX];

    printf("%s test FAILED! errno is %d.\n", testName, errno);

    perror(testName);
    errno = 0;

    // Clean up the test directory if it exists.
    snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile1);
    (void)unlink(filepath);

    snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile2);
    (void)unlink(filepath);

    (void)rmdir(testPath);

    return exit_failure;
}


void closerv(int rv)
{
    char filepath[PATH_MAX];
    int retval;

    errno = 0;
    retval = close(rv);

    if (retval < 0)
    {
        snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile1);
        (void)unlink(filepath);

        snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile2);
        (void)unlink(filepath);

        (void)rmdir(testPath);

        exit(fail("close"));
    }
}


int main()
{
    char filepath[PATH_MAX];
    int rv = 0;
    int warnings = 0;
    int fd = 0;
    int dirCount = 0;

    /* Ignore umask, this can fail the test. */
    mode_t user_mask = umask(0);

    /* Clean up the test directory if it exists. */
    snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile1);
    (void)unlink(filepath);

    snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile2);
    (void)unlink(filepath);

    (void)rmdir(testPath);

    /* Make a directory, a symlink, and a file for various open flag tests. */
    printf("Making test files...\n");

    // Create directory
    errno = 0;
    rv = mkdir(testPath, 0777);

    if (rv < 0)
        return fail("mkdir");

    // Create file 1
    snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile1);

    errno = 0;
    rv = creat(filepath, 0777);

    if (rv < 0)
        return fail("creat");

    closerv(rv);

    // Create file 2
    snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile2);

    errno = 0;
    rv = creat(filepath, 0777);

    if (rv < 0)
        return fail("creat");

    closerv(rv);

    /* Test opendir. */
    printf("Testing opendir...\n");

    DIR *dirp;

    // Make sure this fails
    errno = 0;
    dirp = opendir("dirthatdoesnotexist");

    if (dirp != NULL)
        return fail("opendir");

    // Make sure this succeeds
    errno = 0;
    dirp = opendir(testPath);

    if (dirp == NULL)
        return fail("opendir");


    /* Test readdir. */
    printf("Testing readdir...\n");

    struct dirent *entry;

    // Read the two test files plus . and ..
    for (dirCount = 0; dirCount < 4; ++dirCount)
    {
        errno = 0;
        entry = readdir(dirp);

        if (entry == NULL && errno != 0)
            return fail("readdir");

        if (strcmp(entry->d_name, testFile1) != 0 && strcmp(entry->d_name, testFile2) != 0 
            && entry->d_name[0] != '.')
        {
            printf("Warning: Expected dir named '.', '..', '%s', or '%s' but saw '%s'.\n", 
                   testFile1, testFile2, entry->d_name);
            ++warnings;
        }
    }

    // Make sure there are no other files read
    errno = 0;
    entry = readdir(dirp);

    if (entry == NULL && errno != 0)
        return fail("readdir");

    if (entry != NULL)
    {
        printf("Warning: Expected no more directories but saw %s.\n", entry->d_name);
    }



    /* Test closedir. */
    printf("Testing closedir...\n");

    // Close the real directory pointer
    errno = 0;
    rv = closedir(dirp);

    if (rv < 0)
        return fail("opendir");

    // Make sure this fails
    errno = 0;
    rv = closedir(NULL);

    if (rv >= 0)
        return fail("opendir");


    /* Test readdir_r. */
    printf("Testing readdir_r...\n");

    struct dirent nextDir;
    struct dirent *resultDir = NULL;
    dirCount = 0;

    // This should fail
    while ((rv = readdir_r(dirp, &nextDir, &resultDir)) == 0)
    {
        if (resultDir == NULL)
            break;

        if (strcmp(resultDir->d_name, testFile1) != 0 && strcmp(resultDir->d_name, testFile2) != 0 
            && resultDir->d_name[0] != '.')
        {
            printf("Warning: Expected dir named '.', '..', '%s', or '%s' but saw '%s'.\n", 
                   testFile1, testFile2, entry->d_name);
            ++warnings;
        }

        dirCount++; 
    }

    if (resultDir != NULL || rv <= 0 || dirCount > 0)
        return fail("readdir_r");

    // re-open after closedir test
    errno = 0;
    dirp = opendir(testPath);

    if (dirp == NULL)
        return fail("opendir");

    // recursively read files
    while ((rv = readdir_r(dirp, &nextDir, &resultDir)) == 0)
    {
        if (resultDir == NULL)
            break;

        if (strcmp(resultDir->d_name, testFile1) != 0 && strcmp(resultDir->d_name, testFile2) != 0 
            && resultDir->d_name[0] != '.')
        {
            printf("Warning: Expected dir named '.', '..', '%s', or '%s' but saw '%s'.\n", 
                   testFile1, testFile2, entry->d_name);
            ++warnings;
        }

        dirCount++; 
    }

    if (rv != 0 || resultDir != NULL || dirCount != 4)
        return fail("readdir_r");


    /* Clean up the test files. */
    // Remove file 1
    printf("Cleaning up test files...\n");

    snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile1);

    errno = 0;
    rv = unlink(filepath);

    if (rv < 0)
        return fail("unlink");

    // Remove file 2
    snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile2);

    errno = 0;
    rv = unlink(filepath);

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



