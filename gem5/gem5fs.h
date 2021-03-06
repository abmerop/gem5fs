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


#ifndef __GEM5FS_GEM5_GEM5FS_H__ 
#define __GEM5FS_GEM5_GEM5FS_H__ 

/*  
 *  All the includes we need for the supported file operations
 *  _should_ be here.
 */
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


/* Headers needed for gem5 types. */
#ifdef __cplusplus

#include "base/types.hh"

class ThreadContext;

#endif


#ifdef __cplusplus
namespace gem5fs {
#endif


typedef enum 
{
    ErrorCode,
    TestGem5,
    GetAttr,
    ReadLink,
    MakeDirectory,
    Unlink,
    RemoveDirectory,
    MakeSymLink,
    Rename,
    ChangePermission,
    ChangeOwner,
    Truncate,
    Open,
    Read,
    Write,
    GetStats,
    Flush,
    Release,
    Fsync,
    SetXAttr,
    GetXAttr,
    ListXAttr,
    RemoveXAttr,
    OpenDir,
    ReadDir,
    ReleaseDir,
    FsyncDir,
    Access,
    Create,
    Ftruncate,
    FGetAttr,
    GetResult,
    SetMountpoint,
    GetMountpoint
} Operation;

typedef enum 
{
    UnknownOperation,
    RequestOperation,
    ResponseOperation
} OperationType;

struct FileOperation 
{
    Operation oper;                // The file operation
    OperationType opType;          // Direction of operation data

    char *path;                    // Path to the file/directory
    unsigned int pathLength;       // Length of the path name

    uint8_t *opStruct;             // Pointer to structures needed for a file operation
    unsigned int structSize;       // Length in bytes of opStruct data

    struct FileOperation *result;  // Pointer to the response 
    int errnum;                    // Copy of errno from host
};

/*
 *  Used for read and write operations
 */
struct DataOperation
{
    int hostfd;
    size_t size;
    off_t offset;
    const char *data;
};

/*
 *  Needed for chown operation
 */
struct ChownOperation
{
    uid_t uid;
    gid_t gid;
};

/*
 *  Needed for sync operation
 */
struct SyncOperation
{
    uint8_t datasync;
    int fd;
};

/*
 *  Needed for extended attributes
 */
struct XAttrOperation
{
    char *name;
    char *value;
    size_t name_size;
    size_t value_size;
    int flags;
};

/*
 *  Needed for ftruncate
 */
struct ftruncOperation
{
    off_t length;
    int fd;
};

/* 
 *  This should include all possible data types being copied into
 *  or out of gem5. This is a quick sanity check to see if these
 *  data types are the same size on both the host and the guest
 *  systems. 
 *
 *  I would expect most to be the same for linux 32/64 bit, but it
 *  is nice to provide an interface to test this.
 */
struct TestOperation
{
    size_t size_t_size;               /**< Used for most return values. */
    size_t mode_t_size;               /**< Used in chmod, mkdir, etc. */
    size_t uid_t_size;                /**< Used in chown */
    size_t gid_t_size;                /**< Used in chown */
    size_t struct_stat_size;          /**< Used for fstat/lstat. */
    size_t struct_statvfs_size;       /**< Used for statfs. */
    size_t char_size;                 /**< Used for most path names. */
    size_t off_t_size;                /**< Used for truncate. */
    size_t int_size;                  /**< Used for file descriptors. */
    size_t DataOperation_size;        /**< Used for read/write . */
    size_t ChownOperation_size;       /**< Used for chown . */
    size_t SyncOperation_size;        /**< Used for fsync. */
    size_t XAttrOperation_size;       /**< Used for extended attributes. */
    size_t ftruncOperation_size;      /**< Used for ftruncate, */

    size_t TestOperation_size;        /**< Meta */
};

/* These prototypes are only needed by gem5, not by FUSE. */
#ifdef __cplusplus
uint64_t ProcessRequest(ThreadContext *tc, Addr inputAddr, Addr requestAddr, Addr resultAddr);

FileOperation* BufferResponse(ThreadContext *tc, Addr resultAddr, FileOperation *fileOperation, bool success, uint8_t *responseData, unsigned int responseSize);
void SendResponse(ThreadContext *tc, Addr resultAddr, FileOperation *fileOperation, bool success, uint8_t *responseData, unsigned int responseSize);

void CleanUp(FileOperation *bufferOp);
#endif

#ifdef __cplusplus
}; // namespace gem5fs
#endif

#endif // __GEM5FS_GEM5_GEM5FS_HH__

