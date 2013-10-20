What is gem5fs?
===============

**Short Version**: gem5fs allows you to access files on the host's (machine running gem5) filesystem.

**Long Version**: gem5fs is a FUSE filesystem that runs within gem5 (the "guest") and makes heavy use of a gem5 pseudo instruction to interface files on the system running gem5 (the "host").


How does this work?
===================

You will need 3 changes to the standard gem5 distribution to get gem5fs up and running:

 1. A kernel with FUSE support
 2. Modified disk image with fuse libraries
 3. A patched gem5 with the gem5fs patch

A precompiled linux 2.6.22-smp kernel is provided as well as a tarball with all the extra files needed for the disk image. For quick setup, only the patch in step 3 needs to be applied, and the files extracted to the disk image. The rest can be modified in the command line. 

More advanced instructions are below for anyone wanting to different kernel version or the details on mounting the FUSE filesystem within gem5.

Quick Setup
===========

 1. Download the linux kernel from <http://gem5.nvmain.org/gem5fs/kernels/> and download the disk image files from <http://gem5.nvmain.org/gem5fs/dist/>. 
 2. Mount your gem5 disk image. See the gem5 documentation at <http://gem5.org/Disk_images> for details.
 3. Change directory to the ROOT of the gem5 disk image and extract the disk image files using `tar vxzf /path/to/gem5fs-dist-1.0.0.tar.gz`
 4. Obtain the gem5fs patch using git: `git clone https://bitbucket.org/mrp5060/gem5fs.git`
 5. Change directory to your gem5 installation and apply the patch: `hg qimport /path/to/gem5fs/patch/gem5fs.patch ; hg qpush`
 6. Recompile gem5 using the EXTRAS flag with the path to gem5fs. git repo (e.g., `scons build/X86/gem5.opt -j 8 EXTRAS=/path/to/gem5fs`)

You are now ready to go! You can boot up gem5 using fullsystem mode, for example:

    build/X86/gem5.opt configs/example/fs.py --disk-image=my-modified-disk.img --kernel=x86_64-vmlinux-2.6.22.fuse.smp 

Once the system is booted, you can use m5term to mount the host filesystem:

    /fues/bin/mount.sh

You should now be able to see the host's filesystem mounted at `/host`!


Advanced Setup
==============

Compiling a Linux Kernel
------------------------

If you want to compile your own kernel, it is easiest to enable FUSE in the `.config` file. The configuration files provided on the gem5 documentation do not have FUSE enable by default. However, these files are a good starting point since you only have to modify a single line of the configuration to enable FUSE. Simply add the following line to the kernel config and recompile the kernel.

    CONFIG_FUSE_FS=y

Making the System Files
-----------------------

In addition to the kernel, you will need the FUSE user-space libraries on your gem5 disk image. The FUSE bin utilities may also be useful (you will need at least `fusermount` to unmount gem5fs). You can obtain fuse at http://fuse.sourceforge.net/. Follow the instructions there to build fuse. Copy the `libfuse.so.2` library and the binary files (`fusermount`, `ulockmgr_server`, `mount.fuse`) to your gem5 disk image. For the files provided in the quick setup, we place these under the `/fuse` folder on the root of the disk image. The organization of the files is shown below:

    $ find fuse
    fuse
    fuse/bin
    fuse/bin/fusermount
    fuse/bin/ulockmgr_server
    fuse/bin/gem5fs
    fuse/bin/mount.sh
    fuse/lib
    fuse/lib/libfuse.so.2
    fuse/sbin
    fuse/sbin/mount.fuse

Modifying the Init Scripts
--------------------------

Before mounting gem5fs, you will need to create the FUSE character device. This can be done using the following command:

    mknod /dev/fuse -m 0666 c 10 229

You can place this in your `/etc/init.d/rcS` file (the standard boot script for gem5 disk images), or within a script to mount the FUSE filesystem. Below is the contents of the file `/fuse/bin/mount.sh` provided by the quick setup distribution:

    #!/bin/bash

    if [ ! -e /dev/fuse ]; then
        echo "Could not find fuse device, creating this. You will need to be root."
        mknod /dev/fuse -m 0666 c 10 229
    fi


    if [ "$1" = "-d" ]; then
        echo "Mounting FUSE FS in debug mode"
        LD_LIBRARY_PATH=/fuse/lib /fuse/bin/gem5fs -d /host &
    else
        echo "Mounting FUSE FS"
        LD_LIBRARY_PATH=/fuse/lib /fuse/bin/gem5fs /host
    fi

Here you can change `/host` to something else. You can also paste this into the `/etc/init.d/rcS` file to always have the host filesystem mounted when gem5 finishes booting linux. 

Limitations
===========

The FUSE API provides over 40 different types of file operations (i.e., `read()`, `write()`, `open()`, `close()`, etc). gem5fs currently provides the most common operations, but there are a few operations that are not supported, are not planned to be supported, or have some caveats to be aware of. These are listed below.

Some operations may have undefined behavior if they operate outside of the gem5 disk image. These operations are `mknod`, `link`, `ioctl`, and `lock`. 

 * `mknod` - This is normal used to create non-standard types of files, such as character devices. This is currently not allow from within gem5.
 * `link` - This creates a hardlink. It is not clear what will happen if a hardlink between a file on the gem5 disk image and the host is created and gem5 exits.
 * `lock` - Similar to link, it is not clear what will happen if a file outside of gem5 is locked and gem5 exits.
 * `ioctl` - This is normally used to control register devices, which will not be supported on the host from within gem5.

These operations *may* be useful, so they have some planned implementation: `poll`, `fallocate`, and `bmap`.

 * `bmap` - This function gets a block map of the filesystem. It may be used by some tools such as `df`.
 * `fallocate` - Use to resize a files space without adding contents to the file.
 * `poll` - Used to alert changes on a file descriptor.
 * `utimensat` - Used to change the modification and access time of a file. Current the time within gem5 is not synced with the time of the host, and therefore timestamps would be closer to January 1st, 1970.

Limited operations are working, however they may not act exactly as a program would expect. These operations are `opendir` and `closedir`.

`opendir` and `closedir` always return that there was no error. If a directory does not exist, the error will not be seen until `readdir` or `readdir_r` is called. If an application is using `opendir` as the error indicator and ignoring the `readdir` return values, there may be some issues.

Testing
=======

gem5fs provides a few extra tools to test the filesystem to make sure it works. The most basic check is performed at mount time. gem5fs makes heavy use of passing data types around as void pointers. Therefore, these sizes must match between the host and guest systems. If any of the sizes do not match, gem5fs will refuse to mount.

Tested Operations and Tools
---------------------------

After gem5fs is mounted, additional tools are provided to ensure that individual operations are working. These tools are listed here.

 * test_mkdir - This tests `mkdir`, `rmdir`, `chmod`, `chown`, and `lstat`
 * test_file - This tests `open`, `read`, `write`, `close`, `unlink`, `truncate`, `ftrunacte`, `access`, and `create`
 * test_xattr - This tests `lsetxattr`, `lgetxattr`, `llistxattr`, and `lremovexattr`
 * test_link - This tests `readlink`, and `symlink`
 * test_dir - This tests `opendir`, `readdir`, `readdir_r`, and `closedir`

Untested Operations
-------------------

The following commands have no test programs: `statvfs`, `flush`, `fsync`, `fsyncdir`. 

 * `statvfs` - Used for program like `df`.
 * `flush` - gem5fs does not cache data, so testing is not necessary.
 * `fsync/fsyncdir` - It is difficult to get a before and after of unsynced data to confirm this works.

Long Tests
----------

Some real-world tests were also performed. We ran some spec2006 benchmarks located on the host.s filesystem from within gem5 and compared the outputs of the test inputs to the test outputs. Below is the process of testing 403.gcc:

    # ./403.gcc_base.x86-gcc ../403.gcc/data/test/input/cccp.i -o cccp.s

Checking from outside gem5:

    $ diff cccp.s ../403.gcc/data/test/output/cccp.s
    $

(No output implies the files are the same, i.e., the test passed!)

