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

#include "gem5fs/gem5/gem5fs.h"

#include "cpu/thread_context.hh"
#include "mem/fs_translating_port_proxy.hh"
#include "debug/gem5fs.hh"

using namespace gem5fs;

uint64_t gem5fs::ProcessRequest(ThreadContext *tc, Addr inputAddr, Addr requestAddr, Addr resultAddr)
{
    uint64_t result = 0;

    /* Get the file operation struct. */
    FileOperation fileOp;
    CopyOut(tc, &fileOp, requestAddr, sizeof(FileOperation));

    /* Get the pathname in the file operation. */
    char *pathname;
    if (fileOp.pathLength > 0)
    {
        pathname = new char[fileOp.pathLength+1];
        CopyOut(tc, pathname, (Addr)(fileOp.path), fileOp.pathLength+1);
    }
    else
    {
        pathname = new char[2];
        strncpy(pathname, "/\0", 2);
    }

    /* TODO: Make these gem5 DPRINTFs */
    DPRINTF(gem5fs, "\ngem5fs: virtual address is %p\n", requestAddr);
    DPRINTF(gem5fs, "gem5fs: oper is %d\n", fileOp.oper);
    DPRINTF(gem5fs, "gem5fs: opType is %d\n", fileOp.opType);
    DPRINTF(gem5fs, "gem5fs: path ptr is %p\n", (void*)(fileOp.path));
    DPRINTF(gem5fs, "gem5fs: fileLength = %d\n", fileOp.pathLength);
    DPRINTF(gem5fs, "gem5fs: result address is %p\n", resultAddr);
    DPRINTF(gem5fs, "gem5fs: gem5fs_call on %s\n", pathname);

    /* Switch the umask for operations that may modify permissions. */
    mode_t saved_mask = umask(0);

    /*
     *  We need a known value for errno to determine if readdir
     *  fails or not. Reset to zero which is the libc recommendation.
     */
    errno = 0;

    switch (fileOp.oper)
    {
        case GetResult:
        {
            /*
             *  The result pointer is in gem5's memory space so we
             *  can get this by setting the pointer to the request's
             *  result field.
             */
            FileOperation *resultOp = fileOp.result;

            DPRINTF(gem5fs, "gem5fs_call: resultOp->opStruct is %p\n", (void*)(resultOp->opStruct));
            DPRINTF(gem5fs, "gem5fs_call: resultOp is %p\n", (void*)(resultOp));
            DPRINTF(gem5fs, "gem5fs_call: writing %d bytes to %p\n", resultOp->structSize, resultAddr);

            /*
             *  The fuse filesystem allocates enough memory in opStruct
             *  to hold the response data, so we copy to this field.
             *  The resultAddr is the virtual address of this field.
             */
            CopyIn(tc, resultAddr, resultOp->opStruct, resultOp->structSize);

            /*
             *  Some operations return structs may contain pointers. Those
             *  would need to be CopyIn'd separately. Do that here.
             */

            /*
             *  We can now safely delete the pointers. opStruct
             *  is also allocated for most operations, so this is
             *  also deleted first.
             */
            CleanUp(resultOp);

            break;
        }
        case TestGem5:
        {
            /* Input is a TestOperation struct. */
            TestOperation testOp;
            CopyOut(tc, &testOp, inputAddr, fileOp.structSize);

            /* Assume true, mark false on any failure. */
            bool test_passed = true; 

            /* 
             *  structSize should be the same as the sizeof 
             *  TestOperation and the input test operation's
             *  TestOperation_size.
             */
            if (fileOp.structSize != sizeof(TestOperation)
                || testOp.TestOperation_size != sizeof(TestOperation))
            {
                warn("gem5fs: TestOperation struct does not match guest's size.\n");
                test_passed = false;
            }

            /*
             *  For anything else just compare the struct value with sizeof.
             */
            if (testOp.mode_t_size != sizeof(mode_t))
            {
                warn("gem5fs: mode_t does not match guest's size.\n");
                test_passed = false;
            }

            if (testOp.uid_t_size != sizeof(uid_t))
            {
                warn("gem5fs: uid_t does not match guest's size.\n");
                test_passed = false;
            }

            if (testOp.gid_t_size != sizeof(gid_t))
            {
                warn("gem5fs: gid_t does not match guest's size.\n");
                test_passed = false;
            }

            if (testOp.struct_stat_size != sizeof(struct stat))
            {
                warn("gem5fs: stat struct does not match guest's size.\n");
                test_passed = false;
            }

            if (testOp.struct_statvfs_size != sizeof(struct statvfs))
            {
                warn("gem5fs: stat struct does not match guest's size.\n");
                test_passed = false;
            }

            if (testOp.char_size != sizeof(char))
            {
                warn("gem5fs: char does not match guest's size.\n");
                test_passed = false;
            }

            if (testOp.off_t_size != sizeof(off_t))
            {
                warn("gem5fs: char does not match guest's size.\n");
                test_passed = false;
            }

            if (testOp.int_size != sizeof(int))
            {
                warn("gem5fs: int does not match guest's size.\n");
                test_passed = false;
            }

            if (testOp.DataOperation_size != sizeof(struct DataOperation))
            {
                warn("gem5fs: DataOperation struct does not match guest's size.\n");
                test_passed = false;
            }

            if (testOp.ChownOperation_size != sizeof(struct ChownOperation))
            {
                warn("gem5fs: ChownOperation struct does not match guest's size.\n");
                test_passed = false;
            }

            /*
             *  If anything failed, we will send back an error code to
             *  the FUSE FS, which will decide to mount or not.
             */
            SendResponse(tc, resultAddr, &fileOp, test_passed, NULL, 0);

            break;
        }
        case GetAttr:
        {
            /*
             *  lstat returns 0 on success, -1 on failure and errno is
             *  set. The stat struct does not appear to contain any pointers
             *  within the struct.
             */
            struct stat *statbuf = new struct stat;
            int rv = ::lstat(pathname, statbuf);

            BufferResponse(tc, resultAddr, &fileOp, (rv == 0), (uint8_t*)statbuf, sizeof(struct stat));

            break;
        }
        case ReadLink:
        {
            /* FUSE FS sends the size of the buffer as input. */
            size_t bufSize;
            CopyOut(tc, &bufSize, inputAddr, fileOp.structSize);

            /* Make a temporary buffer */
            char *link = new char[bufSize];

            /* Call readlink with this size */ 
            int rv = ::readlink(pathname, link, bufSize-1);
            if (rv >= 0)
                link[rv] = '\0'; // readlink doesn't append \0.

            /* Save the response data for GetResult. */
            BufferResponse(tc, resultAddr, &fileOp, (rv >= 0), (uint8_t*)link, bufSize);
            
            break;
        }
        case Unlink:
        {
            /* No input data. */
            int rv = ::unlink(pathname);

            SendResponse(tc, resultAddr, &fileOp, (rv == 0), NULL, 0);

            break;
        }
        case Rename:
        {
            /* New path is the input. */
            char *newpath = new char[fileOp.structSize+1];
            CopyOut(tc, newpath, inputAddr, fileOp.structSize+1);

            int rv = ::rename(pathname, newpath);

            SendResponse(tc, resultAddr, &fileOp, (rv == 0), NULL, 0);

            break;
        }
        case Truncate:
        {
            /* FUSE FS sends the newsize as input. */
            off_t length;
            CopyOut(tc, &length, inputAddr, fileOp.structSize);

            int rv = ::truncate(pathname, length);

            SendResponse(tc, resultAddr, &fileOp, (rv == 0), NULL, 0);

            break;
        }
        case Open:
        {
            /* FUSE FS sends the flags as input. */
            int flags;
            CopyOut(tc, &flags, inputAddr, fileOp.structSize);

            /* Create a pointer to the file descriptor. */
            int *fd = new int;

            *fd = open(pathname, flags);

            DPRINTF(gem5fs, "gem5fs: Open fd is %d\n", *fd);
            
            /* Save the response data for GetResult. */
            BufferResponse(tc, resultAddr, &fileOp, (*fd >= 0), (uint8_t*)fd, sizeof(int));

            break;
        }
        case Read:
        {
            /* FUSE FS sends a DataOperation struct as input. */
            DataOperation dataOp;
            CopyOut(tc, &dataOp, inputAddr, fileOp.structSize);

            uint8_t *tmpBuf = new uint8_t[dataOp.size];
            ssize_t rv = pread(dataOp.hostfd, tmpBuf, dataOp.size, dataOp.offset);

            DPRINTF(gem5fs, "gem5fs: read %d bytes from fd %d\n", rv, dataOp.hostfd);

            /* Save the response data for GetResult. */
            BufferResponse(tc, resultAddr, &fileOp, (rv >= 0), tmpBuf, rv);

            break;
        }
        case Write:
        {
            /* FUSE FS sends a DataOperation struct as input. */
            DataOperation dataOp;
            CopyOut(tc, &dataOp, inputAddr, fileOp.structSize);

            char *tmpBuf = new char[dataOp.size];
            CopyOut(tc, tmpBuf, (Addr)dataOp.data, dataOp.size);

            ssize_t rv = pwrite(dataOp.hostfd, tmpBuf, dataOp.size, dataOp.offset);

            DPRINTF(gem5fs, "gem5fs: Writing %d bytes (%s) to fd %d returned %d\n", dataOp.size, tmpBuf, dataOp.hostfd, rv);

            /* Send the response. */
            BufferResponse(tc, resultAddr, &fileOp, (rv >= 0), (uint8_t*)&rv, sizeof(ssize_t));

            break;
        }
        case GetStats:
        {
            /* success if rv == 0. */
            struct statvfs *statbuf = new struct statvfs;
            int rv = ::statvfs(pathname, statbuf);

            /* Save response for FUSE GetResult. */
            BufferResponse(tc, resultAddr, &fileOp, (rv == 0), (uint8_t*)statbuf, sizeof(struct statvfs));

            break;
        }
        case Release:
        {
            /* FUSE FS sends the file descriptor as input. */
            int fd;
            CopyOut(tc, &fd, inputAddr, fileOp.structSize);

            int rv = close(fd);

            DPRINTF(gem5fs, "gem5fs: close on fd %d returned %d\n", fd, rv);
            
            /* Send the response directly. */
            SendResponse(tc, resultAddr, &fileOp, (rv == 0), NULL, 0);

            break;
        }
        case ReadDir:
        {
            /*
             *  Push back the entires to a vector. Once we knows how many
             *  entires we have, we know how much space to allocate for the
             *  response data.
             */
            std::vector<struct dirent> entries;

            /*
             *  We open the directory here instead of during the OpenDir
             *  operation so that we don't need to track the DIR* pointer.
             */
            DIR *dirp = opendir(pathname);
            struct dirent *de;

            /*
             * readdir returns a non-null pointer on success. On failure, NULL
             *  is returned and errno is set. When there are no more entires,
             *  NULL is returned and errno is still 0.
             *
             *  Note: using *de so that a copy is made. I'm not sure what will
             *  happen since the function isn't re-entrant.
             */
            while ((de = readdir(dirp)) != NULL)
            {
                entries.push_back(*de);
            }

            /*
             *  Close the directory here instead of ReleaseDir since
             *  we opened it here.
             */
            closedir(dirp);

            /*
             *  Allocate enough buffer space for the data to be returned.
             */
            size_t entry_buffer_size = 256 * entries.size();
            char *all_entries = new char[entry_buffer_size];
            char *cur_entry = all_entries;

            for(auto iter = entries.begin(); iter != entries.end(); ++iter)
            {
                char *entry_name = iter->d_name;

                memcpy(cur_entry, entry_name, 256);
                cur_entry += 256;
            }

            /* Save the response data for GetResult. */
            BufferResponse(tc, resultAddr, &fileOp, (entries.size() != 0), (uint8_t*)all_entries, entry_buffer_size);

            break;
        }
        case MakeDirectory:
        {
            /* mkdir requires input data. */
            mode_t dirMode;
            CopyOut(tc, &dirMode, inputAddr, fileOp.structSize);

            DPRINTF(gem5fs, "gem5fs: Making directory with mode %d (%X)\n", dirMode, dirMode);

            /* Call mkdir */ 
            int rv = ::mkdir(pathname, dirMode);

            /* Save the response data for GetResult. */
            SendResponse(tc, resultAddr, &fileOp, (rv == 0), NULL, 0);
            
            break;
        }
        case RemoveDirectory:
        {
            /* Returns 0 on success. */
            int rv = ::rmdir(pathname);

            /* Save the response data for GetResult. */
            SendResponse(tc, resultAddr, &fileOp, (rv == 0), NULL, 0);
            
            break;
        }
        case ChangePermission:
        {
            /* mkdir requires input data. */
            mode_t chmodMode;
            CopyOut(tc, &chmodMode, inputAddr, fileOp.structSize);

            DPRINTF(gem5fs, "gem5fs: Changing %s permissions to mode %d (%X)\n", pathname, chmodMode, chmodMode);

            /* Call mkdir */ 
            int rv = ::chmod(pathname, chmodMode);

            /* Save the response data for GetResult. */
            SendResponse(tc, resultAddr, &fileOp, (rv == 0), NULL, 0);
            
            break;
        }
        case ChangeOwner:
        {
            /* ChownOperation is passed as input. */
            struct ChownOperation chownOp;
            CopyOut(tc, &chownOp, inputAddr, fileOp.structSize);

            /* Success if rv == 0 */
            int rv = ::chown(pathname, chownOp.uid, chownOp.gid);

            /* Send response rv. */
            SendResponse(tc, resultAddr, &fileOp, (rv == 0), NULL, 0);

            break;
        }
        case Access:
        {
            /* FUSE FS sends mask as input. */
            int mask;
            CopyOut(tc, &mask, inputAddr, fileOp.structSize);

            /* Call access */
            int rv = ::access(pathname, mask);

            /* Send the return value back. */
            SendResponse(tc, resultAddr, &fileOp, (rv == 0), NULL, 0);

            break;
        }
        case Create:
        {
            /* FUSE FS sends mask as input. */
            mode_t mode;
            CopyOut(tc, &mode, inputAddr, fileOp.structSize);

            /* Create a pointer to the file descriptor. */
            int *fd = new int;

            /* Call creat */
            *fd = ::creat(pathname, mode);

            DPRINTF(gem5fs, "gem5fs: Create fd is %d\n", *fd);
            
            /* Save the response data for GetResult. */
            BufferResponse(tc, resultAddr, &fileOp, (*fd >= 0), (uint8_t*)fd, sizeof(int));
            break;
        }
        case FGetAttr:
        {
            /* FUSE FS sends file descriptor as input. */
            int fd;
            CopyOut(tc, &fd, inputAddr, fileOp.structSize);

            struct stat *statbuf = new struct stat;
            int rv = ::fstat(fd, statbuf);

            BufferResponse(tc, resultAddr, &fileOp, (rv == 0), (uint8_t*)statbuf, sizeof(struct stat));

            break;
        }
        default:
        {
            break;
        }
    }

    /* Reset the umask to the previous value. */
    (void)umask(saved_mask);

    return result;
}

/*
 *  Clean up any malloc'd data.
 */
void gem5fs::CleanUp(FileOperation *bufferOp)
{
    if (bufferOp->opStruct != NULL )
    {
        /*
         *  I *think* free will work properly even if the
         *  type of opStruct isn't the same as what it was
         *  cast to when memory was allocated (malloc does
         *  return void after all).
         */
        delete bufferOp->opStruct;
    }
    delete bufferOp;
}

/*
 *  Allocates response data and "buffers" it by setting the result pointer
 *  to itself, allowing the FUSE fs to call GetResult with said pointer and
 *  access this data again without the pointer being lost.
 */
FileOperation* gem5fs::BufferResponse(ThreadContext *tc, Addr resultAddr, FileOperation *fileOperation, bool success, uint8_t *responseData, unsigned int responseSize)
{
    FileOperation *bufferOp = new FileOperation;

    /* Build the buffered response struct. */
    bufferOp->oper = (success) ? fileOperation->oper : ErrorCode;
    bufferOp->opType = ResponseOperation;
    bufferOp->path = fileOperation->path;
    bufferOp->pathLength = fileOperation->pathLength;
    bufferOp->opStruct = responseData;
    bufferOp->structSize = responseSize;
    bufferOp->result = bufferOp;
    bufferOp->errnum = errno;

    DPRINTF(gem5fs, "gem5fs: bufferOp->opStruct is %p\n", (void*)(bufferOp->opStruct));
    DPRINTF(gem5fs, "gem5fs: bufferOp is %p\n", (void*)(bufferOp));
    DPRINTF(gem5fs, "gem5fs: writing %d bytes to %p\n", bufferOp->structSize, resultAddr);

    CopyIn(tc, (Addr)(resultAddr), bufferOp, sizeof(FileOperation));

    /* Trash response data on error. The FUSE FS won't request after errors. */
    if (!success)
    {
        CleanUp(bufferOp);
        bufferOp = NULL;
    }

    return bufferOp;
}


/*
 *  Send the response and delete the local buffered operation for the 
 *  case of operation that do not require a response.
 */
void gem5fs::SendResponse(ThreadContext *tc, Addr resultAddr, FileOperation *fileOperation, bool success, uint8_t *responseData, unsigned int responseSize)
{
    FileOperation *bufferOp = BufferResponse(tc, resultAddr, fileOperation, success, responseData, responseSize);

    delete bufferOp;
}


