# gcfuse - Use FUSE To Mount GameCube Filesystems

**Note (February, 2018):** The original build instructions below were written circa 2006 and are a bit obsolete. This source code should still compile and run on modern Linux kernels using this process:

1. Be sure to have FUSE libraries installed (as of Ubuntu 16.04, this package is 'libfuse-dev')
2. Clone this repository
3. Execute the following build command:
 
 ```gcc -D_FILE_OFFSET_BITS=64 -Wall src/gcfs.c src/main.c src/tree.c -o gcfs -lfuse```

This will build the binary executable ```gcfs```. Use the *Usage* section below for instructions on running this utility.

---

gcfuse is a program that allows you to mount a Nintendo GameCube DVD 
disk image as a read-only part of the Linux filesystem. This allows the 
user to browse the directory structure and read the files within. 
Further, gcfuse provides access to the main program .dol and also
creates a special file called .metadata in the root directory of 
the mounted filesystem.

gcfuse accomplishes all this using Filesystem in Userspace (FUSE), 
available at:

  http://fuse.sourceforge.net/

Note that there are likely to be bugs and perhaps even security 
problems. It is currently meant as primarily an experimental research 
tool for studying GameCube discs.


Requirements:
```
	- Linux 2.4.x or 2.6.x (as of 2.6.14 FUSE is part of the
	  kernel, but you still need user libraries)
	- FUSE (http://fuse.sourceforge.net) 2.5.x or higher
```

Build:
```	
	./configure && make
```

Install:
```
	make install
```

Usage:
```
	gcfuse <image_file.gcm> <mount_point>
```

To unmount previously mounted file, use:
```
	fusermount -u <mount_point>
```
