
#ifndef _PARAMS_H_
#define _PARAMS_H_

#define FUSE_USE_VERSION 26

// need this to get pwrite().  I have to use setvbuf() instead of
// setlinebuf() later in consequence.
#define _XOPEN_SOURCE 500

// maintain bbfs state in here
struct gem5fs_state {
    char *rootdir;
};

#endif

