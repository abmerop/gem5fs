
#include "fuse/gem5fusefs.h"
#include "gem5/gem5fs.h"
#include "m5op.h"

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
static int gem5fs_error(char *str)
{
    int ret = -errno;
    
    return ret;
}

/** Get the path to the file on the host filesystem */
static void gem5fs_fullpath(char fpath[PATH_MAX], const char *path)
{
    char *mountpoint = ((struct gem5fs_state *)fuse_get_context()->private_data)->rootdir;

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
}

/** Get file attributes. */
int gem5fs_getattr(const char *path, struct stat *statbuf)
{
    int rv = 0;

    return rv; 
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
    int rv = 0;

    return rv; 
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
    int rv = 0;

    return rv; 
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

    /* Build the file operation struct. */
    oper.oper = OpenDir;
    oper.opType = RequestOperation;
    oper.path = path;
    oper.pathLength = strlen(path);
    oper.opStruct = NULL;
    oper.structSize = 0;

    m5_gem5fs_call((void*)&oper, sizeof(oper));

    return rv; 
}

int gem5fs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
	       struct fuse_file_info *fi)
{
    int rv = 0;

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
    fprintf(stderr, "usage: gem5fs <mountpoint>\n");
    abort();
}

int main(int argc, char *argv[])
{
    int fuse_stat;
    struct gem5fs_state *gem5fs_data;

    /*
     *  Usage is gem5fs <path> 
     */

    if(argc != 2)
    	gem5fs_usage();

    gem5fs_data = malloc(sizeof(struct gem5fs_state));
    if (gem5fs_data == NULL) {
	perror("main calloc");
	abort();
    }

    // Pull the rootdir out of the argument list and save it in my
    // internal data
    gem5fs_data->rootdir = realpath(argv[argc-1], NULL);
    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;
    
    // turn over control to fuse
    fprintf(stderr, "about to call fuse_main\n");
    fuse_stat = fuse_main(argc, argv, &gem5fs_oper, gem5fs_data);
    fprintf(stderr, "fuse_main returned %d\n", fuse_stat);
    
    return fuse_stat;
}
