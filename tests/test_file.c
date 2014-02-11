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

#include "gem5/gem5fs.h"
#include "util/m5/m5op.h"

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
const char *testFile = "foo";

void mountpoint(char *mntpt)
{
    struct FileOperation request;
    struct FileOperation response;

    request.oper = GetMountpoint;
    request.opType = RequestOperation;
    request.path = NULL;
    request.pathLength = 0;
    request.opStruct = NULL;
    request.structSize = 0;

    m5_gem5fs_call(NULL, (void*)&request, (void*)&response);

    request.oper = GetResult;
    request.opType = RequestOperation;
    request.path = NULL;
    request.pathLength = 0;
    request.opStruct = (uint8_t*)malloc(response.structSize);
    request.structSize = response.structSize;
    request.result = response.result;

    memset(request.opStruct, 0, response.structSize);

    m5_gem5fs_call(NULL, (void*)&request, (void*)&response);

    memcpy(mntpt, request.opStruct, request.structSize);
}

int fail(const char *testName, const char *flag)
{
    char filepath[PATH_MAX];

    if (flag == NULL)
        printf("%s test FAILED! errno is %d.\n", testName, errno);
    else
        printf("%s test FAILED on flag %s! errno is %d.\n", testName, flag, errno);

    perror(testName);
    errno = 0;

    // Clean up the test directory if it exists.
    snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile);

    (void)unlink(filepath);
    (void)unlink(linkName);
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
        snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile);

        (void)unlink(filepath);
        (void)unlink(linkName);
        (void)rmdir(testPath);

        exit(fail("close", NULL));
    }
}


int main()
{
    char filepath[PATH_MAX];
    char linkpath[PATH_MAX];
    char readbuf[1024];
    int rv = 0;
    int warnings = 0;
    int fd = 0;

    /* Ignore umask, this can fail the test. */
    mode_t user_mask = umask(0);

    /* Clean up the test directory if it exists. */
    snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile);

    (void)unlink(filepath);
    (void)unlink(linkName);
    (void)rmdir(testPath);

    /* Make a directory, a symlink, and a file for various open flag tests. */
    printf("Making test files...\n");

    errno = 0;
    rv = mkdir(testPath, 0777);

    if (rv < 0)
        return fail("mkdir", NULL);

    errno = 0;
    rv = symlink(testPath, linkName);

    if (rv < 0)
        return fail("symlink", NULL);

    snprintf(filepath, PATH_MAX, "%s/%s", testPath, testFile);
    snprintf(linkpath, PATH_MAX, "%s/%s", linkName, testFile);

    errno = 0;
    rv = creat(filepath, 0777);

    if (rv < 0)
        return fail("creat", NULL);

    closerv(rv);

    /* Test access. */
    printf("Testing access...\n");

    errno = 0;
    rv = access(filepath, R_OK | W_OK | X_OK);

    if (rv < 0)
        return fail("access", NULL);

    /* Test open. */
    printf("Testing open...\n");

    // Test O_DIRECTORY - Fail if path is not a directory.
    printf("\tO_DIRECTORY...\n");

    errno = 0;
    rv = open(filepath, O_DIRECTORY | O_RDONLY);

    if (rv >= 0)
        return fail("open", "O_DIRECTORY (file)");

    errno = 0;
    rv = open(testPath, O_DIRECTORY | O_RDONLY);

    if (rv < 0)
        return fail("open", "O_DIRECTORY (dir)");

    closerv(rv);

    // Test O_NOFOLLOW
    printf("\tO_NOFOLLOW...\n");

    errno = 0;
    rv = open(linkName, O_NOFOLLOW | O_RDONLY);

    if (rv >= 0)
        return fail("open", "O_NOFOLLOW (link)");

    errno = 0;
    rv = open(testPath, O_NOFOLLOW | O_RDONLY);

    if (rv < 0)
        return fail("open", "O_NOFOLLOW (dir)");

    closerv(rv);

    // Test O_WRONLY
    printf("\tO_WRONLY...\n");

    errno = 0;
    rv = open(filepath, O_WRONLY | O_TRUNC);

    if (rv < 0)
        return fail("open", "O_WRONLY");

    fd = rv;

    errno = 0;
    rv = write(fd, "foo", 3);

    if (rv < 0)
        return fail("write", "O_WRONLY");

     if (rv != 3)
     {
         printf("Warning: O_WRONLY expected to write 3 bytes but wrote %d.\n", rv);
         ++warnings;
     }

    memset(readbuf, 0, 1024);

    errno = 0;
    rv = read(fd, &readbuf, 1024);

    if (rv >= 0)
        return fail("read", "O_WRONLY");

    closerv(fd);

    // Test O_RDONLY
    printf("\tO_RDONLY...\n");

    errno = 0;
    rv = open(filepath, O_RDONLY);

    if (rv < 0)
        return fail("open", "O_RDONLY");

    fd = rv;

    errno = 0;
    rv = write(fd, "bar", 3);

    if (rv >= 0)
        return fail("write", "O_RDONLY");

    memset(readbuf, 0, 1024);

    errno = 0;
    rv = read(fd, &readbuf, 1024);

    if (rv < 0)
        return fail("read", "O_RDONLY");

    if (rv != 3)
    {
        printf("Warning: O_RDONLY expected to read 3 bytes but read %d.\n", rv);
        ++warnings;
    }

    readbuf[3] = '\0';

    if (strcmp(readbuf, "foo") != 0)
    {
        printf("Warning: O_RDONLY expected 'foo' but got: '%s'.\n", readbuf);
        ++warnings;
    }

    closerv(fd);

    // Test O_TRUNC
    printf("\tO_TRUNC...\n");

    errno = 0;
    rv = open(filepath, O_TRUNC | O_WRONLY);

    if (rv < 0)
        return fail("open", "O_TRUNC");

    closerv(rv);

    struct stat fileStat;

    errno = 0;
    rv = lstat(filepath, &fileStat);

    if (rv < 0)
        return fail("lstat", NULL);

    if (fileStat.st_size != 0)
    {
        printf("Warning: lstat expected 0 byte size, but saw %d bytes.\n", fileStat.st_size);
        ++warnings;
    }

    // Test O_RDWR
    printf("\tO_RDWR...\n");

    errno = 0;
    rv = open(filepath, O_APPEND | O_RDWR);

    if (rv < 0)
        return fail("open", "O_RDWR");

    fd = rv;

    errno = 0;
    rv = write(fd, "foo", 3);

    if (rv < 0)
        return fail("write", "O_RDWR");

    if (rv != 3)
    {
        printf("Warning: O_RDWR expected to write 3 bytes but wrote %d.\n", rv);
        ++warnings;
    }

    errno = 0;
    rv = lseek(fd, 0, SEEK_SET);

    if (rv < 0)
        return fail("lseek", NULL);

    memset(readbuf, 0, 1024);

    errno = 0;
    rv = read(fd, &readbuf, 3);

    if (rv < 0)
        return fail("read", "O_RDWR");

    if (rv != 3)
    {
        printf("Warning: O_RDWR expected to read 3 bytes but read %d.\n", rv);
        ++warnings;
    }

    readbuf[3] = '\0';

    if (strcmp(readbuf, "foo") != 0)
    {
        printf("Warning: O_RDWR expected 'foo' but got: '%s'.\n", readbuf);
        ++warnings;
    }

    closerv(fd);

    // Test O_APPEND
    printf("\tO_APPEND...\n");

    errno = 0;
    rv = open(filepath, O_APPEND | O_RDWR);

    if (rv < 0)
        return fail("open", "O_APPEND");

    fd = rv;

    errno = 0;
    rv = write(fd, "bar", 3);

    if (rv < 0)
        return fail("write", "O_APPEND");

    if (rv != 3)
    {
        printf("Warning: O_APPEND expected to write 3 bytes but wrote %d.\n", rv);
        ++warnings;
    }

    errno = 0;
    rv = lseek(fd, 0, SEEK_SET);

    if (rv < 0)
        return fail("lseek", NULL);

    memset(readbuf, 0, 1024);

    errno = 0;
    rv = read(fd, &readbuf, 1024);

    if (rv < 0)
        return fail("read", "O_APPEND");

    if (rv != 6)
    {
        printf("Warning: O_APPEND expected to read 6 bytes but read %d.\n", rv);
        ++warnings;
    }

    readbuf[6] = '\0';

    if (strcmp(readbuf, "foobar") != 0)
    {
        printf("Warning: O_APPEND expected 'foobar' but got: '%s'.\n", readbuf);
    }

    closerv(fd);

    // Test partial read
    printf("\tpartial read...\n");

    errno = 0;
    rv = open(filepath, O_RDONLY);

    if (rv < 0)
        return fail("open", "O_RDONLY (part)");

    fd = rv;

    memset(readbuf, 0, 1024);

    errno = 0;
    rv = read(fd, &readbuf, 2);

    if (rv < 0)
        return fail("read", "O_RDONLY (part)");

    if (rv != 2)
    {
        printf("Warning: O_RDONLY expected to read 3 bytes but read %d.\n", rv);
        ++warnings;
    }

    readbuf[2] = '\0';

    if (strcmp(readbuf, "fo") != 0)
    {
        printf("Warning: O_RDONLY expected 'fo' but got: '%s'.\n", readbuf);
        ++warnings;
    }

    closerv(fd);

    // Test O_EXCL
    printf("\tO_EXCL...\n");

    errno = 0;
    rv = open(filepath, O_EXCL | O_CREAT | O_WRONLY);

    if (rv >= 0)
        return fail("open", "O_EXCL (old)");

    /* Test truncate. */
    printf("Testing truncate...\n");

    errno = 0;
    rv = lstat(filepath, &fileStat);

    if (rv < 0)
        return fail("lstat", NULL);

    if (fileStat.st_size == 0)
    {
        printf("Warning: truncate expected a non-zero sized file.\n");
        ++warnings;
    }

    errno = 0;
    rv = truncate(filepath, 3);

    if (rv < 0)
        return fail("truncate", NULL);

    errno = 0;
    rv = lstat(filepath, &fileStat);

    if (rv < 0)
        return fail("lstat", NULL);

    if (fileStat.st_size != 3)
    {
        printf("Warning: truncate expected file of size 3 bytes but saw %d bytes.\n", fileStat.st_size);
        ++warnings;
    }

    /* Test ftruncate. */
    printf("Testing ftruncate...\n");

    errno = 0;
    rv = open(filepath, O_WRONLY);

    if (rv < 0)
        return fail("open", "ftruncate");

    fd = rv;

    errno = 0;
    rv = ftruncate(fd, 1);

    if (rv < 0)
        return fail("ftruncate", NULL);

    errno = 0;
    rv = fstat(fd, &fileStat);

    if (rv < 0)
        return fail("fstat", NULL);

    if (fileStat.st_size != 1)
    {
        printf("Warning: ftruncate expected file of size 1 byte, but saw %d bytes.\n", fileStat.st_size);
        ++warnings;
    }

    closerv(fd);

    /* Clean up the test files. */
    printf("Cleaning up test files...\n");

    errno = 0;
    rv = unlink(filepath);

    if (rv < 0)
        return fail("unlink", NULL);

    // Remove the link
    errno = 0;
    rv = unlink(linkName);

    if (rv < 0)
        return fail("unlink", NULL);

    // Remove the directory
    errno = 0;
    rv = rmdir(testPath);

    if (rv < 0)
        return fail("rmdir", NULL);

    /* Reset umask */
    (void)umask(user_mask);

    /* Print the test result. */
    printf("Test PASSED with 0 errors and %d warnings.\n", warnings);

    return exit_success;
}



