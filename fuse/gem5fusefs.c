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

#if 0
    /* Make sure the file we are opening is on this mountpoint.. */
    if( strncmp(path, mountpoint, strlen(mountpoint)) == 0 ) 
    {
        /* Remove the mountpoint from the file path. */
        size_t copy_len = strlen(path) - strlen(mountpoint);

        if( copy_len+1 < PATH_MAX )
            strncpy(fpath, path+strlen(mountpoint), copy_len);
        else
            strncpy(fpath, path+strlen(mountpoint), PATH_MAX); // This won't work for paths that are too long
    }
    else
    {
        // Error message ?
    }
#endif

    strncpy(fpath, path, PATH_MAX);

    printf("path = %s, set fpath to %s\n", path, fpath);
}

/** Get file attributes. */
int gem5fs_getattr(const char *path, struct stat *statbuf)
{
    struct FileOperation request;
    struct FileOperation response;
    char fpath[PATH_MAX];

    /* Get the path on the host. */
    gem5fs_fullpath(fpath, path);

    printf("gem5fs_getattr called on %s (%s)\n", path, fpath);

    /* Build the file operation struct. */
    request.oper = GetAttr;
    request.opType = RequestOperation;
    request.path = fpath;
    request.pathLength = strlen(fpath);
    request.opStruct = NULL;
    request.structSize = 0;

    /* Call lstat on the host. */
    m5_gem5fs_call(NULL, (void*)&request, (void*)&response);

    /* Check the result. */
    if (response.oper == ErrorCode)
    {
        return gem5fs_error(__func__, response.errnum);
    }

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

    printf("gem5fs_getattr allocated %d byte buffer at %p\n", response.structSize, request.opStruct);
    
    /* Get the result and place it in request.opStruct. */
    m5_gem5fs_call(NULL, (void*)&request, (void*)request.opStruct);

    printf("gem5fs_getattr returned %d bytes of data.\n", response.structSize); 

    /*
     *  The result's struct contains the stat struct from
     *  the host system, copy this to FUSE's statbuf.
     */
    memcpy(statbuf, request.opStruct, request.structSize);

    /* We're done with this memory. */
    free(request.opStruct);

    return 0; 
}

/** Read the target of a symbolic link */
int gem5fs_readlink(const char *path, char *link, size_t size)
{
    int rv = 0;

    return rv; 
}

/** Create a file node */
int gem5fs_mknod(const char *path, mode_t mode, dev_t dev)
{
    int rv = 0;

    return rv; 
}

/** Create a directory */
int gem5fs_mkdir(const char *path, mode_t mode)
{
    struct FileOperation request;
    struct FileOperation response;
    char fpath[PATH_MAX];

    /* Get the path on the host. */
    gem5fs_fullpath(fpath, path);

    printf("gem5fs_getattr called on %s (%s)\n", path, fpath);

    /* Build the file operation struct. */
    request.oper = MakeDirectory;
    request.opType = RequestOperation;
    request.path = fpath;
    request.pathLength = strlen(fpath);
    request.opStruct = NULL;
    request.structSize = 0;

    /* Call mkdir on the host. */
    m5_gem5fs_call((void*)&mode, (void*)&request, (void*)&response);

    /* Check the result. */
    if (response.oper == ErrorCode)
    {
        return gem5fs_error(__func__, response.errnum);
    }

    return 0; 
}

/** Remove a file */
int gem5fs_unlink(const char *path)
{
    int rv = 0;

    return rv; 
}

/** Remove a directory */
int gem5fs_rmdir(const char *path)
{
    int rv = 0;

    return rv; 
}

/** Create a symbolic link */
int gem5fs_symlink(const char *path, const char *link)
{
    int rv = 0;

    return rv; 
}

/** Rename a file */
int gem5fs_rename(const char *path, const char *newpath)
{
    int rv = 0;

    return rv; 
}

/** Create a hard link to a file */
int gem5fs_link(const char *path, const char *newpath)
{
    int rv = 0;

    return rv; 
}

/** Change the permission bits of a file */
int gem5fs_chmod(const char *path, mode_t mode)
{
    struct FileOperation request;
    struct FileOperation response;
    char fpath[PATH_MAX];

    /* Get the path on the host. */
    gem5fs_fullpath(fpath, path);

    printf("gem5fs_chmod called on %s (%s)\n", path, fpath);

    /* Build the file operation struct. */
    request.oper = ChangePermission;
    request.opType = RequestOperation;
    request.path = fpath;
    request.pathLength = strlen(fpath);
    request.opStruct = NULL;
    request.structSize = 0;

    /* Call mkdir on the host. */
    m5_gem5fs_call((void*)&mode, (void*)&request, (void*)&response);

    /* Check the result. */
    if (response.oper == ErrorCode)
    {
        return gem5fs_error(__func__, response.errnum);
    }

    return 0; 
}

/** Change the owner and group of a file */
int gem5fs_chown(const char *path, uid_t uid, gid_t gid)
{
    int rv = 0;

    return rv; 
}

/** Change the size of a file */
int gem5fs_truncate(const char *path, off_t newsize)
{
    int rv = 0;

    return rv; 
}

/** Change the access and/or modification times of a file */
int gem5fs_utimens(const char *path, const struct timespec tv[2])
{
    int rv = 0;

    return rv; 
}

/** File open operation */
int gem5fs_open(const char *path, struct fuse_file_info *fi)
{
    int rv = 0;

    return rv; 
}

/** Read data from an open file */
int gem5fs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int rv = 0;

    return rv; 
}

/** Write data to an open file */
int gem5fs_write(const char *path, const char *buf, size_t size, off_t offset,
	     struct fuse_file_info *fi)
{
    int rv = 0;

    return rv; 
}

/** Get file system statistics */
int gem5fs_statfs(const char *path, struct statvfs *statv)
{
    int rv = 0;

    return rv; 
}

/** Possibly flush cached data */
int gem5fs_flush(const char *path, struct fuse_file_info *fi)
{
    int rv = 0;

    return rv; 
}

/** Release an open file */
int gem5fs_release(const char *path, struct fuse_file_info *fi)
{
    int rv = 0;

    return rv; 
}

/** Synchronize file contents */
int gem5fs_fsync(const char *path, int datasync, struct fuse_file_info *fi)
{
    int rv = 0;

    return rv; 
}

/** Set extended attributes */
int gem5fs_setxattr(const char *path, const char *name, const char *value, size_t size, int flags)
{
    int rv = 0;

    return rv; 
}

/** Get extended attributes */
int gem5fs_getxattr(const char *path, const char *name, char *value, size_t size)
{
    int rv = 0;

    return rv; 
}

/** List extended attributes */
int gem5fs_listxattr(const char *path, char *list, size_t size)
{
    int rv = 0;

    return rv; 
}

/** Remove extended attributes */
int gem5fs_removexattr(const char *path, const char *name)
{
    int rv = 0;

    return rv; 
}

int gem5fs_opendir(const char *path, struct fuse_file_info *fi)
{
    int rv = 0;
    struct FileOperation oper;
    struct FileOperation response;
    char fpath[PATH_MAX];

#if 0
    gem5fs_fullpath(fpath, path);

    /* Build the file operation struct. */
    oper.oper = OpenDir;
    oper.opType = RequestOperation;
    oper.path = fpath;
    oper.pathLength = strlen(path);
    oper.opStruct = NULL;
    oper.structSize = 0;

    error = m5_gem5fs_call((void*)&oper, (void*)&response);

    /* Check the result. */
    if (result.oper == ErrorCode)
    {
        return gem5fs_error(__func__, response.errnum);
    }
#endif

    return 0; 
}

int gem5fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    struct FileOperation request;
    struct FileOperation response;
    char fpath[PATH_MAX];
    int entry_count, entry;
    //struct dirent *all_entries, *cur_entry;
    char *all_entries, *cur_entry;

    /* Get the path on the host. */
    gem5fs_fullpath(fpath, path);

    printf("gem5fs_readdir called on %s (%s) with buffer %p and offset %d\n", path, fpath, buf, offset);
    printf("gem5fs_readdir sizeof dirent struct is %d\n", sizeof(struct dirent));

    /* Build the file operation struct. */
    request.oper = ReadDir;
    request.opType = RequestOperation;
    request.path = fpath;
    request.pathLength = strlen(fpath);
    request.opStruct = NULL;
    request.structSize = 0;

    /* Call readdir recursively on host. */
    m5_gem5fs_call(NULL, (void*)&request, (void*)&response);

    /* Check the result. */
    if (response.oper == ErrorCode)
    {
        return gem5fs_error(__func__, response.errnum);
    }

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

    printf("gem5fs_readdir allocated %d byte buffer at %p\n", response.structSize, request.opStruct);
    
    /* Get the result and place it in request.opStruct. */
    m5_gem5fs_call(NULL, (void*)&request, (void*)request.opStruct);

    printf("gem5fs_readdir returned %d bytes of data.\n", response.structSize); 

    /*
     *  The struct is a pack sequence of dirent structs. We
     *  can figure out how many there are by dividing by the
     *  size of one struct, and use pointer addition to get
     *  each individual dirent struct.
     */
    entry_count = response.structSize / 256;
    all_entries = (char *)request.opStruct;
    cur_entry = all_entries;

    printf("gem5fs_readdir found %d directories.\n", entry_count);

    for (entry = 0; entry < entry_count; ++entry)
    {
        char d_name[256];
        memcpy(d_name, cur_entry, 256);
        printf("gem5fs_readdir filling directory %s\n", d_name);

        if (filler(buf, d_name, NULL, 0) != 0)
            return gem5fs_error(__func__, ENOMEM);

        cur_entry += 256;
    }

    /* We're done with this memory. */
    free(request.opStruct);

    /* Return 0 on success */
    return 0; 
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
}

int gem5fs_access(const char *path, int mask)
{
    int rv = 0;

    return rv; 
}

int gem5fs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int rv = 0;

    return rv; 
}

int gem5fs_ftruncate(const char *path, off_t offset, struct fuse_file_info *fi)
{
    int rv = 0;

    return rv; 
}

int gem5fs_fgetattr(const char *path, struct stat *statbuf, struct fuse_file_info *fi)
{
    int rv = 0;

    return rv; 
}

int gem5fs_lock(const char *path, struct fuse_file_info *ffi, int cmd, struct flock *f_lock)
{
    int rv = 0;

    return rv;
}

int gem5fs_bmap(const char *path, size_t blocksize, uint64_t *idx)
{
    int rv = 0;

    return rv; 
}

int gem5fs_ioctl(const char *path, int cmd, void *arg, struct fuse_file_info *ffi, unsigned int flags, void *data)
{
    int rv = 0;

    return rv;
}

int gem5fs_poll(const char *path, struct fuse_file_info *ffi, struct fuse_pollhandle *ph, unsigned *reventsp)
{
    int rv = 0;

    return rv;
}

int gem5fs_write_buf(const char *path, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *ffi)
{
    int rv = 0;

    return rv;
}

int gem5fs_read_buf(const char *path, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *ffi)
{
    int rv = 0;

    return rv;
}

int gem5fs_flock(const char *path, struct fuse_file_info *ffi, int op)
{
    int rv = 0;

    return rv;
}

int gem5fs_fallocate(const char *path, int mode, off_t off, off_t len, struct fuse_file_info *ffi)
{
    int rv = 0;

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
  .ftruncate = gem5fs_ftruncate,
  .fgetattr = gem5fs_fgetattr,
  .lock = gem5fs_lock,
  .utimens = gem5fs_utimens,
  .bmap = gem5fs_bmap,
  .ioctl = gem5fs_ioctl,
  .poll = gem5fs_poll,
  .write_buf = gem5fs_write_buf,
  .read_buf = gem5fs_read_buf,
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
