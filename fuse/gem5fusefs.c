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

#include "fuse/gem5fusefs.h"
#include "gem5/gem5fs.h"
#include "util/m5/m5op.h"

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fuse.h>
#include <libgen.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/xattr.h>

/** Return error as -errno to caller */
static int gem5fs_error(const char *operation, int error)
{
    int rv = 0;
    int errno_prev = errno;

    errno = error;
    perror(operation);
    errno = errno_prev;

    return -error;
}

/** Get the path to the file on the host filesystem */
static void gem5fs_fullpath(char fpath[PATH_MAX], const char *path)
{
    char *mountpoint = ((struct gem5fs_state *)fuse_get_context()->private_data)->rootdir;

    printf("path = %s, mount = %s\n", path, mountpoint);

    strncpy(fpath, path, PATH_MAX);

    printf("path = %s, set fpath to %s\n", path, fpath);
}

/*
 *  Sends a request to gem5 via pseudo instruction and returns a response if
 *  specified by the caller. Requests are sent with the input data input_data
 *  of size input_size. If NULL, no input data is sent. The response is placed
 *  in the buffer response_data of size response_size. This buffer is allocated
 *  here and NOT freed. The caller should free this buffer after processing the
 *  response data. If this buffer is NULL, no response data is requested or
 *  allocated.
 */
int gem5fs_syscall(Operation op, const char *path, void *input_data, unsigned int input_size, uint8_t **response_data, unsigned int *response_size)
{
    struct FileOperation request;
    struct FileOperation response;
    char fpath[PATH_MAX];

    /* Get the path on the host. */
    gem5fs_fullpath(fpath, path);

    printf("gem5fs_syscall called on %s (%s)\n", path, fpath);

    /* Build the file operation struct. */
    request.oper = op;
    request.opType = RequestOperation;
    request.path = fpath;
    request.pathLength = strlen(fpath);
    request.opStruct = input_data;
    request.structSize = input_size;

    /* Call the operation on the host. */
    m5_gem5fs_call(input_data, (void*)&request, (void*)&response);

    /* Check the result. */
    if (response.oper == ErrorCode)
    {
        return gem5fs_error(__func__, response.errnum);
    }

    /*
     *  If response_data is NULL we'll assume that the operation
     *  doesn't have any response data, such as mkdir, rmdir, etc
     *  and well return here.
     */
    if (response_data == NULL)
        return 0;

    /* 
     *  Request was successful, but we need to allocate space
     *  in the FUSE filesystem's memory space in to which the 
     *  pseudo op will copy. 
     *
     *  request.result has the pointer to the FileOperation
     *  in gem5's memory space, so we need to send this.
     *
     *  structSize will tell us how much space we need to 
     *  allocate to get the result. 
     */
    request.oper = GetResult;
    request.opType = RequestOperation;
    request.path = fpath;
    request.pathLength = strlen(fpath);
    request.opStruct = (uint8_t*)malloc(response.structSize);
    request.structSize = response.structSize;
    request.result = response.result;

    /*
     *  I suspect that the malloc function is lazy and just 
     *  returns a pointer to a free region of memory and doesn't
     *  actually do things like update the pagetable, etc. For 
     *  some directories with more than a dozen files, the gem5
     *  translation fails. Here i'm using memset to zero out
     *  the data, which in turn accesses it and the pagetable is
     *  updated* preventing a fault in gem5
     *
     *  * = Partially conjecture.
     */
    memset(request.opStruct, 0, response.structSize);

    printf("gem5fs_syscall allocated %d byte buffer at %p\n", response.structSize, request.opStruct);
    
    /* Get the result and place it in request.opStruct. */
    m5_gem5fs_call(NULL, (void*)&request, (void*)request.opStruct);

    printf("gem5fs_syscall returned %d bytes of data.\n", response.structSize); 

    /*
     *  The result's struct contains the stat struct from
     *  the host system, copy this to FUSE's statbuf.
     */
    //memcpy(*response_data, request.opStruct, request.structSize);
    *response_data = request.opStruct;

    if (response_size != NULL)
        *response_size = response.structSize;

    /* We're done with this memory. */
    //free(request.opStruct);

    return 0;
}

/** Get file attributes. */
int gem5fs_getattr(const char *path, struct stat *statbuf)
{
    int rv;
    int response_size;
    struct stat *tmpStat;

    if ((rv = gem5fs_syscall(GetAttr, path, NULL, 0, (uint8_t**)&tmpStat, &response_size)) == 0)
    {
        memcpy(statbuf, tmpStat, response_size);
        free(tmpStat);
    }

    return rv;
}

/** Read the target of a symbolic link */
int gem5fs_readlink(const char *path, char *link, size_t size)
{
    int rv;
    char *buf;
    unsigned int bufsiz;
    size_t modsize;
    char *mountpoint = ((struct gem5fs_state *)fuse_get_context()->private_data)->rootdir;

    modsize = size - strlen(mountpoint);

    /* Just pass in the size of the buffer. */
    if ((rv = gem5fs_syscall(ReadLink, path, (void*)&modsize, sizeof(size_t), (uint8_t**)&buf, &bufsiz)) == 0)
    {
        if (buf[0] == '/')
        {
            strcpy(link, mountpoint);
            strcat(link, buf);
        }
        else
        {
            strcpy(link, buf);
        }
        free(buf);
    }

    return rv; 
}

/** Create a file node */
int gem5fs_mknod(const char *path, mode_t mode, dev_t dev)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

/** Create a directory */
int gem5fs_mkdir(const char *path, mode_t mode)
{
    return gem5fs_syscall(MakeDirectory, path, (void*)&mode, sizeof(mode_t), NULL, NULL); 
}

/** Remove a file */
int gem5fs_unlink(const char *path)
{
    return gem5fs_syscall(Unlink, path, NULL, 0, NULL, NULL);
}

/** Remove a directory */
int gem5fs_rmdir(const char *path)
{
    return gem5fs_syscall(RemoveDirectory, path, NULL, 0, NULL, NULL);
}

/** Create a symbolic link */
int gem5fs_symlink(const char *path, const char *link)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

/** Rename a file */
int gem5fs_rename(const char *path, const char *newpath)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

/** Create a hard link to a file */
int gem5fs_link(const char *path, const char *newpath)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

/** Change the permission bits of a file */
int gem5fs_chmod(const char *path, mode_t mode)
{
    return gem5fs_syscall(ChangePermission, path, NULL, 0, NULL, NULL); 
}

/** Change the owner and group of a file */
int gem5fs_chown(const char *path, uid_t uid, gid_t gid)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

/** Change the size of a file */
int gem5fs_truncate(const char *path, off_t newsize)
{
    return gem5fs_syscall(Truncate, path, (void*)&newsize, sizeof(off_t), NULL, NULL); 
}

/** Change the access and/or modification times of a file */
int gem5fs_utimens(const char *path, const struct timespec tv[2])
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

/** File open operation */
int gem5fs_open(const char *path, struct fuse_file_info *fi)
{
    int rv;
    int *hostfd;

    if ((rv = gem5fs_syscall(Open, path, (void*)&(fi->flags), sizeof(int), (uint8_t**)&hostfd, NULL)) == 0)
    {
        printf("gem5fs_open got fd %d\n", *hostfd);
        fi->fh = *hostfd;
        free(hostfd);
    }

    return rv;
}

/** Read data from an open file */
int gem5fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int rv = 0;
    struct DataOperation dataOp;
    char *tmpBuf;
    unsigned int bufSize;

    printf("gem5fs_read setting up dataOp\n");

    dataOp.hostfd = fi->fh;
    dataOp.size = size;
    dataOp.offset = offset;
    dataOp.data = NULL;

    printf("gem5fs_read dataOp address is %p\n", &dataOp);

    if ((rv = gem5fs_syscall(Read, path, (void*)&dataOp, sizeof(struct DataOperation), (uint8_t**)&tmpBuf, &bufSize)) == 0)
    {
        printf("gem5fs_read got %d bytes: '%s'\n", bufSize, tmpBuf);
        memcpy(buf, tmpBuf, bufSize);
        free(tmpBuf);
    }

    return bufSize; 
}

/** Write data to an open file */
int gem5fs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int rv;
    struct DataOperation dataOp;
    ssize_t *bytes_written;

    printf("gem5fs_write called path %s buf %p size %d offset %d\n", path, buf, size, offset);

    dataOp.hostfd = fi->fh;
    dataOp.size = size;
    dataOp.offset = offset;
    dataOp.data = buf;

    printf("gem5fs_write calling gem5fs_syscall\n");

    if ((rv = gem5fs_syscall(Write, path, (void*)&dataOp, sizeof(struct DataOperation), (uint8_t**)&bytes_written, NULL)) == 0)
    {
        rv = *bytes_written;
        free(bytes_written);

        printf("gem5fs_write wrote %d bytes\n", rv);
    }

    return rv;
}

/** Get file system statistics */
int gem5fs_statfs(const char *path, struct statvfs *statv)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

/** Possibly flush cached data */
int gem5fs_flush(const char *path, struct fuse_file_info *fi)
{
    /* We don't cache data. */
    return 0; 
}

/** Release an open file */
int gem5fs_release(const char *path, struct fuse_file_info *fi)
{
    return gem5fs_syscall(Release, path, (void*)&(fi->fh), sizeof(int), NULL, NULL);
}

/** Synchronize file contents */
int gem5fs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

/** Set extended attributes */
int gem5fs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

/** Get extended attributes */
int gem5fs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

/** List extended attributes */
int gem5fs_listxattr(const char *path, char *list, size_t size)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

/** Remove extended attributes */
int gem5fs_removexattr(const char *path, const char *name)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

int gem5fs_opendir(const char *path, struct fuse_file_info *fi)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

int gem5fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int rv;
    int entry_count, entry;
    char *all_entries, *cur_entry;

    if ((rv = gem5fs_syscall(ReadDir, path, NULL, 0, (uint8_t**)&all_entries, &entry_count)) == 0)
    {
        /*
         *  The response is multiple 256 byte directory names 
         *  concatenated into a single array.
         */
        entry_count /= 256;
        cur_entry = all_entries;

        for (entry = 0; entry < entry_count; ++entry)
        {
            char d_name[256];
            memcpy(d_name, cur_entry, 256);
            printf("gem5fs_readdir filling directory %s\n", d_name);

            if (filler(buf, d_name, NULL, 0) != 0)
                return gem5fs_error(__func__, ENOMEM);

            cur_entry += 256;
        }

        free(all_entries);
    }

    return rv;
}

int gem5fs_releasedir(const char *path, struct fuse_file_info *fi)
{
    int rv = 0;

    return rv; 
}

int gem5fs_fsyncdir(const char *path, int datasync, struct fuse_file_info *fi)
{
    int rv = 0;

    return rv; 
}

void *gem5fs_init(struct fuse_conn_info *conn)
{
    return ((struct gem5fs_state *)fuse_get_context()->private_data);
}

void gem5fs_destroy(void *userdata)
{
    printf("%s called\n", __func__);

}

int gem5fs_access(const char *path, int mask)
{
    return gem5fs_syscall(Access, path, (void*)&mask, sizeof(int), NULL, NULL); 
}

int gem5fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int rv;
    int *hostfd;

    if ((rv = gem5fs_syscall(Create, path, (void*)&mode, sizeof(mode_t), (uint8_t**)&hostfd, NULL)) == 0)
    {
        printf("gem5fs_open got fd %d\n", *hostfd);
        fi->fh = *hostfd;
        free(hostfd);
    }

    return rv;
}

int gem5fs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

int gem5fs_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int rv;
    int response_size;
    struct stat *tmpStat;

    if ((rv = gem5fs_syscall(GetAttr, path, (void*)&(fi->fh), sizeof(fi->fh), (uint8_t**)&tmpStat, &response_size)) == 0)
    {
        memcpy(statbuf, tmpStat, response_size);
        free(tmpStat);
    }

    return rv;
}

int gem5fs_lock(const char *path, struct fuse_file_info *ffi, int cmd, struct flock *f_lock)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv;
}

int gem5fs_bmap(const char *path, size_t blocksize, uint64_t *idx)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv; 
}

int gem5fs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *ffi, unsigned int flags, void *data)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv;
}

int gem5fs_poll(const char *path, struct fuse_file_info *ffi, struct fuse_pollhandle *ph, unsigned *reventsp)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv;
}

int gem5fs_write_buf(const char *path, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *ffi)
{
    int rv = 0;

    printf("gem5fs_write_buf called\n");

    return rv;
}

int gem5fs_read_buf(const char *path, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *ffi)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv;
}

int gem5fs_flock(const char *path, struct fuse_file_info *ffi, int op)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv;
}

int gem5fs_fallocate(const char *path, int mode, off_t off, off_t len, struct fuse_file_info *ffi)
{
    int rv = 0;

    printf("%s called\n", __func__);

    return rv;
}

struct fuse_operations gem5fs_oper = {
  .getattr = gem5fs_getattr,
  .readlink = gem5fs_readlink,
  .mknod = gem5fs_mknod,
  .mkdir = gem5fs_mkdir,
  .unlink = gem5fs_unlink,
  .rmdir = gem5fs_rmdir,
  .symlink = gem5fs_symlink,
  .rename = gem5fs_rename,
  .link = gem5fs_link,
  .chmod = gem5fs_chmod,
  .chown = gem5fs_chown,
  .truncate = gem5fs_truncate,
  .open = gem5fs_open,
  .read = gem5fs_read,
  .write = gem5fs_write,
  .statfs = gem5fs_statfs,
  .flush = gem5fs_flush,
  .release = gem5fs_release,
  .fsync = gem5fs_fsync,
  .setxattr = gem5fs_setxattr,
  .getxattr = gem5fs_getxattr,
  .listxattr = gem5fs_listxattr,
  .removexattr = gem5fs_removexattr,
  .opendir = gem5fs_opendir,
  .readdir = gem5fs_readdir,
  .releasedir = gem5fs_releasedir,
  .fsyncdir = gem5fs_fsyncdir,
  .init = gem5fs_init,
  .destroy = gem5fs_destroy,
  .access = gem5fs_access,
  .create = gem5fs_create,
  .ftruncate = NULL,              // Call truncate instead
  .fgetattr = gem5fs_fgetattr,
  .lock = gem5fs_lock,
  .utimens = gem5fs_utimens,
  .bmap = gem5fs_bmap,
  .ioctl = gem5fs_ioctl,
  .poll = gem5fs_poll,
  .write_buf = NULL,              // Call write instead
  .read_buf = NULL,               // Call read instead
  .flock = gem5fs_flock,
  .fallocate = gem5fs_fallocate

};

void gem5fs_usage()
{
    fprintf(stderr, "Usage: gem5fs <mountpoint>\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct gem5fs_state *gem5fs_data;

    /*
     *  Usage is gem5fs <path> 
     */
    if(argc < 2)
    	gem5fs_usage();

    gem5fs_data = malloc(sizeof(struct gem5fs_state));
    if (gem5fs_data == NULL) {
        fprintf(stderr, "Could not allocate memory for internal data.\n");
        abort();
    }

    /* Check sizes */
    printf("sizeof(FileOperation) is %d\n", sizeof(struct FileOperation));

    /* Determine the mountpoint of the filesystem, e.g., '/host' */
    gem5fs_data->rootdir = realpath(argv[argc-1], NULL);
    printf("gem5fs mounted at '%s'.\n", gem5fs_data->rootdir);
    
    /* Turn over control to fuse. */
    fuse_stat = fuse_main(argc, argv, &gem5fs_oper, gem5fs_data);
    fprintf(stderr, "fuse_main returned %d.\n", fuse_stat);
    
    return fuse_stat;
}
