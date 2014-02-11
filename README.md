What is gem5fs?
===============

**Short Version**: gem5fs allows you to access files on the host's (machine running gem5) filesystem.

**Long Version**: gem5fs is a FUSE filesystem that runs within gem5 (the "guest") and makes heavy use of a gem5 pseudo instruction to interface files on the system running gem5 (the "host").


How does this work?
===================

Currently gem5fs works with the X86 architecture. You will need to make 3 changes to the standard gem5 distribution to get gem5fs up and running:

 1. A kernel with FUSE support
 2. Modified disk image with fuse libraries
 3. A patched gem5 with the gem5fs patch

A precompiled linux 2.6.22-smp kernel is provided as well as a tarball with all the extra files needed for the disk image. For quick setup, only the patch in step 3 needs to be applied and the files extracted to the disk image. The rest can be modified in the gem5 command line. 

More advanced instructions are below for anyone wanting a different kernel version and/or the details on mounting the FUSE filesystem within gem5.

Quick Setup
===========

 1. Download the linux kernel from <http://gem5.nvmain.org/gem5fs/kernels/> and download the disk image files from <http://gem5.nvmain.org/gem5fs/dist/>. 
 2. Mount your gem5 disk image. See the gem5 documentation at <http://gem5.org/Disk_images> for details.
 3. Change directory to the ROOT of the gem5 disk image and extract the disk image files using `tar vxzf /path/to/gem5fs-dist-1.0.0.tar.gz`
 4. Obtain the gem5fs patch using git: `git clone https://bitbucket.org/mrp5060/gem5fs.git`
 5. Change directory to your gem5 installation and apply the patch: `hg qimport /path/to/gem5fs/patch/gem5fs.patch ; hg qpush`
 6. Recompile gem5 using the EXTRAS flag with the path to gem5fs git repository (e.g., `scons build/X86/gem5.opt -j 8 EXTRAS=/path/to/gem5fs`)

You are now ready to go! You can boot up gem5 using fullsystem mode, for example:

    build/X86/gem5.opt configs/example/fs.py --disk-image=my-modified-disk.img --kernel=x86_64-vmlinux-2.6.22.fuse.smp 

Once the system is booted, you can mount the host filesystem from within m5term:

    /fuse/bin/mount.sh

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

You can place this in your `/etc/init.d/rcS` file (the standard boot script for gem5 disk images), or within a script to mount the FUSE filesystem. Below are the contents of the file `/fuse/bin/mount.sh` provided by the quick setup distribution:

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

The FUSE API provides over 40 different types of file operations (i.e., `read()`, `write()`, `open()`, `close()`, etc). gem5fs currently provides the most common operations, but there are a few operations that are not supported, are not planned to be supported, or have some caveats of which to be aware.

Some operations may have undefined behavior if they operate outside of the gem5 disk image. These operations are `mknod`, `link`, `ioctl`, and `lock`. 

 * `mknod` - This is normally used to create non-standard types of files, such as character devices. This is currently not allow from within gem5.
 * `link` - This creates a hardlink. It is not clear what will happen if a hardlink between a file on the gem5 disk image and the host is created and gem5 exits.
 * `lock` - Similar to link, it is not clear what will happen if a file outside of gem5 is locked and gem5 exits.
 * `ioctl` - This is normally used to control register devices, which will not be supported on the host from within gem5.

These operations *may* be useful, so they have some planned implementation: `poll`, `utimensat`, `fallocate`, and `bmap`.

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
 * test_link - This tests `readlink`, `unlink`, and `symlink`
 * test_dir - This tests `opendir`, `readdir`, `readdir_r`, and `closedir`

Untested Operations
-------------------

The following commands have no test programs: `statvfs`, `flush`, `fsync`, `fsyncdir`. 

 * `statvfs` - Used for programs like `df`.
 * `flush` - gem5fs does not cache data, so testing is not necessary.
 * `fsync/fsyncdir` - It is difficult to get a before and after of unsynced data to confirm this works.

Long Tests
----------

Some real-world tests were also performed. SPEC2006 benchmarks located on the host's filesystem were run from within gem5. The outputs of the test inputs were compared to the test outputs provided with the SPEC distribution. Below is an example of the testing process for 403.gcc:

    # ./403.gcc_base.x86-gcc ../403.gcc/data/test/input/cccp.i -o cccp.s

The outputs were compared outside of gem5 using the diff tool:

    $ diff cccp.s ../403.gcc/data/test/output/cccp.s
    $

(No output implies the files are the same, i.e., the test passed!)

Other Architectures
===================

Currently only X86 is supported. However, other architectures can easily be added. The only requirement is that the architecture supports pseudo instructions with 3 operands. You will need to cross-compile the kernel for your architecture with fuse support, and use the `--target` option for scons to give the base name of the toolchain (for example, if alpha's GCC binary is `alpha-unknown-linux-gcc`, use `--target=alpha-unknown-linux` and be sure this binary is in your `$PATH` environment variable).

Debugging
=========

gem5fs supports debugging in two ways. The FUSE filesystem can be mounted within gem5 in debug mode using `/fuse/bin/mount.sh -d`. Additionally, gem5 debug flags can be used to output debugging data using `build/X86/gem5.opt --debug-flags=gem5fs`. Further debugging support is currently planned by writing to an on-host log file but this is not yet implemented.

Changelog
=========

 * 0.99.0 - First release of gem5fs. Test tools are not available in this distribution. Poll, fallocate, and bmap are not supported. On-host debug logging is not yet complete.
