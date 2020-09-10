# gcfuse - Use FUSE To Mount GameCube Filesystems

gcfuse is a program that allows you to mount a Nintendo GameCube DVD 
disk image as a read-only part of the Linux filesystem. This allows the 
user to browse the directory structure and read the files within. 
Further, gcfuse provides access to the main program .dol and also
creates a special file called .metadata in the root directory of 
the mounted filesystem.

gcfuse accomplishes all this using Filesystem in Userspace (FUSE), 
available at:

  https://github.com/libfuse/libfuse

Note that it is not usually possible to simply read a Nintendo optical discs in
an ordinary computer's DVD-ROM drive. In order to 
mount a filesystem, generally, you will have to rip the proper 
sector image from the disc using special hardware and tools, or contact 
another source who has already done so.

Note also that there are likely to be bugs and perhaps even security 
problems. It is currently meant as primarily an experimental research 
tool for studying GameCube discs.

### Requirements:
- Linux 2.4.x or 2.6.x (as of 2.6.14 FUSE is part of the kernel, but you still need user libraries)
- FUSE (http://fuse.sourceforge.net) 2.5.x or higher
- FUSE development libraries; 'libfuse-dev' on Ubuntu distros

### Build:
    ./autogen.sh
    ./configure
    make

### Install:
    make install

#### Usage:
The basic usage is to supply a Nintendo GameCube disc image and an empty mount point on the filesystem:

    gcfuse <image_file.gcm> <mount_point>

Browsing to the mount point will reveal the directory structure of the disc's filesystems. Further,
it will also expose the root executable .dol file, which is an implicit part of the disc filesystem.
This file will be named after the name of the disc.

Speaking of the name of the disc, the filesystem will also have a '.metadata' file at the root which
contains a few bits of metadata embedded in the filesystem, including:
- Game code
- Publisher code
- Title

The filesystem derives the name of the root executable from that title metadata.

To unmount previously mounted file, use:
    fusermount -u <mount_point>

