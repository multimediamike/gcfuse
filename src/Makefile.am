bin_PROGRAMS = gcfuse
gcfuse_SOURCES = tree.c gcfs.c main.c
noinst_HEADERS = tree.h gcfs.h
gcfuse_CFLAGS = $(CFLAGS) $(FUSE_CFLAGS)
gcfuse_LDADD = $(LDFLAGS) $(FUSE_LDFLAGS) $(FUSE_LIBS)
