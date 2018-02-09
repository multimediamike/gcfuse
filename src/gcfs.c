/*
 *  Copyright (C) 2006 Mike Melanson (mike at multimedia.cx)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*!
 * \file gcfs.c
 * \author Mike Melanson
 * \brief Interpret the Nintendo GameCube filesystem
 */

#include <ctype.h>

#include "tree.h"
#include "gcfs.h"

#define NAME_MAX_SIZE 1024
#define METADATA_FILE_NAME ".metadata"
#define CRLF "\x0D\x0A"

// the filename is a slight variation of game name with "-exe.dol" tacked
// on the end; the game name can be at most 0x3E0, with 9 bytes for the
// extended string + NULL; round out to 0x400
#define MAX_MAIN_DOL_FILENAME_SIZE 0x400

// global file description since main program needs to access it
int gcfs_fd;

//! Extract \c gcfsfile structure from FUSE context.
static inline struct gcfsfile *get_gcfsfile_from_context(void)
{
	return (struct gcfsfile *)fuse_get_context()->private_data;
}

// **********************************************************************
// gcfs operations
// **********************************************************************

// **********************************************************************
// FUSE operations
// Here comes FUSE operations, please consult fuse.h for description of
// each operation.
// **********************************************************************

/*!
 * \brief Get file attributes.
 */
static int gcfs_getattr(const char *path, struct stat *stbuf)
{
	struct stat st;

	// special handling for the metadata file
	if (strcmp(path + 1, METADATA_FILE_NAME) == 0) {
		memset(stbuf, 0, sizeof(struct stat));
		// Set UID and GID to current user
		stbuf->st_uid = getuid();
		stbuf->st_gid = getgid();
		stbuf->st_mode = S_IFREG | S_IRUSR | S_IRGRP | S_IROTH;
		stbuf->st_nlink = 1;
		stbuf->st_size = get_gcfsfile_from_context()->metadata_size;
		// Set all times to the values from PACK file.
		fstat(get_gcfsfile_from_context()->fd, &st);
		stbuf->st_atime = st.st_atime;
		stbuf->st_mtime = st.st_mtime;
		stbuf->st_ctime = st.st_ctime;
		return 0;
	} else
		return tree_getattr(path, stbuf, get_gcfsfile_from_context()->tree,
			get_gcfsfile_from_context()->fd);
}

/*!
 * \brief File open operation.
 */
static int gcfs_open(const char *path, struct fuse_file_info *fi)
{
	// special handling for the metadata file
	if (strcmp(path + 1, METADATA_FILE_NAME) == 0)
		return 0;
	else
		return tree_open(path, fi, get_gcfsfile_from_context()->tree);
}

/*!
 * \brief Read data from an open file.
 */
static int gcfs_read(const char *path, char *buf, size_t size,
		    off_t offset, struct fuse_file_info *fi)
{
	// special handling for the metadata file
	if (strcmp(path + 1, METADATA_FILE_NAME) == 0) {
		if (offset >= get_gcfsfile_from_context()->metadata_size)
			return 0;

		if (offset + size >= get_gcfsfile_from_context()->metadata_size)
			size = get_gcfsfile_from_context()->metadata_size - offset;

		pthread_mutex_lock(&get_gcfsfile_from_context()->mutex);
		memcpy(buf, get_gcfsfile_from_context()->metadata, size);
		pthread_mutex_unlock(&get_gcfsfile_from_context()->mutex);

		return size;
	} else
		return tree_read(path, buf, size, offset, fi,
			 get_gcfsfile_from_context()->tree,
			 get_gcfsfile_from_context()->fd,
			 &get_gcfsfile_from_context()->mutex);
}

/*!
 * Open directory.
 */
static int gcfs_opendir(const char *path, struct fuse_file_info *fi)
{
	return tree_opendir(path, fi, get_gcfsfile_from_context()->tree);
}

/*!
 * \brief Read directory.
 */
static int gcfs_readdir(const char *path, void *buf,
		       fuse_fill_dir_t filler, off_t offset,
		       struct fuse_file_info *fi)
{
	return tree_readdir(path, buf, filler, offset, fi,
			    get_gcfsfile_from_context()->tree);
}

/*!
 * \brief Recurse through a directory structure.
 */
static int gcfs_recurse_directory(
	unsigned char *buffer,
	int starting_index,
	int ending_index,
	char *name_buffer,
	int starting_name_index,
	int fd,
	off_t filename_base_offset,
	struct tree *root)
{
	int i;
	unsigned int filename_offset;
	unsigned int file_offset;
	unsigned int file_size;
	int is_dir;
	int name_index;
	char name_buffer_copy[NAME_MAX_SIZE];

	fprintf (stderr, "*** recursing directory %s @ %d, recurse from %d -> %d\n", 
		name_buffer, starting_index - 1, starting_index, ending_index);

	for (i = starting_index; i <= ending_index; i++) {
		filename_offset = BE_32(&buffer[(i-1) * 12 + 0]);
		file_offset = BE_32(&buffer[(i-1) * 12 + 4]);
		file_size = BE_32(&buffer[(i-1) * 12 + 8]);

		is_dir = filename_offset & 0x01000000;
		filename_offset &= ~0x01000000;
		filename_offset += filename_base_offset;

		// get the filename
		lseek(fd, filename_offset, SEEK_SET);
		name_index = starting_name_index;
		do {
			read(fd, name_buffer + name_index, 1);
		} while (isprint(name_buffer[name_index]) &&
			(name_index++ < NAME_MAX_SIZE - 1));
		// make sure the string is NULL-terminated
		name_buffer[name_index] = 0;

		if (is_dir) {
			name_buffer[name_index++] = '/';
			name_buffer[name_index] = 0;
			i += gcfs_recurse_directory(buffer, i + 1, file_size, 
				name_buffer, name_index, fd, filename_base_offset,
				root);
		} else {
			fprintf(stderr, "entry %d: %s; %d bytes, starts @ 0x%X\n", 
				i, name_buffer, file_size, file_offset);
			memcpy(name_buffer_copy, name_buffer, NAME_MAX_SIZE);
			tree_insert(root, name_buffer_copy, strlen(name_buffer_copy),
				    file_offset, file_size);
		}
	}

	return ending_index - starting_index + 1;
}

/*!
 * \brief Support function that returns a publisher's name.
 */
static char *gamecube_publisher_name(char c1, char c2)
{
	if ((c1 == '0') && (c2 == '1'))
		return "Nintendo";
	else if ((c1 == '0') && (c2 == '8'))
		return "Capcom";
	else if ((c1 == '4') && (c2 == '1'))
		return "Ubisoft";
	else if ((c1 == '4') && (c2 == 'F'))
		return "Eidos";
	else if ((c1 == '5') && (c2 == '1'))
		return "Acclaim";
	else if ((c1 == '5') && (c2 == '2'))
		return "Activision";
	else if ((c1 == '5') && (c2 == 'D'))
		return "Midway";
	else if ((c1 == '5') && (c2 == 'G'))
		return "Hudson";
	else if ((c1 == '6') && (c2 == '4'))
		return "LucasArts";
	else if ((c1 == '6') && (c2 == '9'))
		return "Electronic Arts";
	else if ((c1 == '6') && (c2 == 'S'))
		return "TDK Mediactive";
	else if ((c1 == '8') && (c2 == 'P'))
		return "Sega";
	else if ((c1 == 'A') && (c2 == '4'))
		return "Mirage Studios";
	else if ((c1 == 'A') && (c2 == 'F'))
		return "Namco";
	else if ((c1 == 'B') && (c2 == '2'))
		return "Bandai";
	else if ((c1 == 'D') && (c2 == 'A'))
		return "Tomy";
	else if ((c1 == 'E') && (c2 == 'M'))
		return "Konami";
	else
		return "unknown publisher";
}

/*!
 * \brief Initialize filesystem.
 */
static void *gcfs_init(void)
{
	int i, j;
	char c;
	unsigned char workspace[0x440];
	unsigned int fst_chunk;
	int file_record_count;
	int file_records_size;
	unsigned char *file_records;
	off_t filename_base_offset;
	char name_buffer[NAME_MAX_SIZE];
	char main_dol_filename[MAX_MAIN_DOL_FILENAME_SIZE];
	unsigned int main_dol_offset;
	unsigned char dol_header[256];
	unsigned int max_dol_section_offset;
	unsigned int section_offset;
	unsigned int section_size;

	struct gcfsfile *gcfs = (struct gcfsfile *)malloc(sizeof(struct gcfsfile));
	if (!gcfs) {
		fprintf(stderr,"not enough memory\n");
		close(gcfs_fd);
		fuse_exit(fuse_get_context()->fuse);
		return NULL;
	}

	gcfs->fd = gcfs_fd;
	// Read 64 ID bytes from the front of the resource
	if (read(gcfs->fd, workspace, 0x440) != 0x440) {
		fprintf(stderr, "file too small to be a GCFS file\n");
		close(gcfs->fd);
		free(gcfs);
		fuse_exit(fuse_get_context()->fuse);
		return NULL;
	}

	gcfs->size = lseek(gcfs->fd, 0, SEEK_END);

	pthread_mutex_init(&gcfs->mutex, NULL);

	gcfs->tree = tree_empty();

	// load the metadata
	gcfs->metadata[0] = 0;
	strcat(gcfs->metadata, "Game code: ");
	gcfs->metadata_size = strlen(gcfs->metadata);
	for (i = 0; i < 4; i++)
		gcfs->metadata[gcfs->metadata_size++] = 
			isprint(workspace[i]) ? workspace[i] : '?';
	strncat(gcfs->metadata, CRLF, strlen(CRLF));
	strcat(gcfs->metadata, "Publisher code: ");
	gcfs->metadata_size = strlen(gcfs->metadata);
	for (i = 4; i < 6; i++)
		gcfs->metadata[gcfs->metadata_size++] = 
			isprint(workspace[i]) ? workspace[i] : '?';
	strcat(gcfs->metadata, " (");
	strcat(gcfs->metadata, gamecube_publisher_name(workspace[4], workspace[5]));
	strcat(gcfs->metadata, ")");
	strncat(gcfs->metadata, CRLF, strlen(CRLF));
	strcat(gcfs->metadata, "Title: ");
	strncat(gcfs->metadata, &workspace[32], 0x3E0);
	strncat(gcfs->metadata, CRLF, strlen(CRLF));

	gcfs->metadata[METADATA_FILE_MAX_SIZE - 1] = 0;
	gcfs->metadata_size = strlen(gcfs->metadata);

	tree_insert(gcfs->tree, METADATA_FILE_NAME, sizeof(METADATA_FILE_NAME),
		0, gcfs->metadata_size);

	// decide on a filename for the main executable-- lowercase
	// all characters; replace spaces with dashes; discard non-alphanumeric 
	// characters
	j = 0;
	for (i = 0; i < 0x3E0; i++) {
		c = workspace[32 + i];
		if (c == ' ')
			main_dol_filename[j++] = '-';
		else if (isalnum(c)) {
			if (isupper(c))
				main_dol_filename[j++] = tolower(c);
			else
				main_dol_filename[j++] = c;
		}
	}
	main_dol_filename[j] = 0;
	strcat(main_dol_filename, "-exe.dol");

	// find the main executable file-- this involves seeking to the location
	// of the DOL, reading the first 256 bytes, and deciding which section of
	// text or data section extends the farthest
	main_dol_offset = BE_32(&workspace[0x420]);
	lseek(gcfs->fd, main_dol_offset, SEEK_SET);
	if (read(gcfs->fd, dol_header, 256) != 256) {
		fprintf(stderr, "no main executable file found\n");
		close(gcfs->fd);
		free(gcfs);
		fuse_exit(fuse_get_context()->fuse);
		return NULL;
	}
	max_dol_section_offset = 0;
	// iterate through the 7 code segments
	for (i = 0; i < 7; i++) {
		section_offset = BE_32(&dol_header[i * 4]);
		section_size = BE_32(&dol_header[0x90 + i * 4]);
		if (section_offset + section_size > max_dol_section_offset)
			max_dol_section_offset = section_offset + section_size;
	}
	// iterate through the 10 code segments
	for (i = 0; i < 10; i++) {
		section_offset = BE_32(&dol_header[0x1C + i * 4]);
		section_size = BE_32(&dol_header[0xAC + i * 4]);
		if (section_offset + section_size > max_dol_section_offset)
			max_dol_section_offset = section_offset + section_size;
	}
	tree_insert(gcfs->tree, main_dol_filename, strlen(main_dol_filename),
		main_dol_offset, max_dol_section_offset);

	// find the filesystem pointer at 0x424
	fst_chunk = BE_32(&workspace[0x424]);
	lseek(gcfs->fd, fst_chunk, SEEK_SET);

	// read the first file record
	if (read(gcfs->fd, workspace, 12) != 12) {
		fprintf(stderr, "file too small to be a GCFS file\n");
		close(gcfs->fd);
		free(gcfs);
		fuse_exit(fuse_get_context()->fuse);
		return NULL;
	}
	file_record_count = BE_32(&workspace[8]);
	file_records_size = file_record_count * 12;
	file_records = (unsigned char *)malloc(file_records_size);
	if (!file_records) {
		fprintf(stderr,"not enough memory\n");
		close(gcfs->fd);
		fuse_exit(fuse_get_context()->fuse);
		return NULL;
	}

	// copy the first record over
	memcpy(file_records, workspace, 12);

	// load the remaining records
	if (read(gcfs->fd, file_records + 12, file_records_size - 12) != 
		file_records_size - 12) {
		fprintf(stderr, "file too small to be a GCFS file\n");
		close(gcfs->fd);
		free(gcfs);
		fuse_exit(fuse_get_context()->fuse);
		return NULL;
	}
	filename_base_offset = lseek(gcfs->fd, 0, SEEK_CUR);
	name_buffer[0] = 0;

	// build the tree
	gcfs_recurse_directory(file_records, 2, file_record_count,
		name_buffer, 0, gcfs->fd, filename_base_offset,
		gcfs->tree);

	return (void *)gcfs;
}

/*!
 * \brief Clean up filesystem.
 */
static void gcfs_destroy(void *context)
{
	struct gcfsfile *gcfs = (struct gcfsfile *)context;

	if (gcfs) {
		close(gcfs->fd);
		tree_free(gcfs->tree);
		free(gcfs);
	}
}

/*!
 * \brief The FUSE file system operations.
 */
struct fuse_operations gcfs_operations = {
	.getattr = gcfs_getattr,
	.open = gcfs_open,
	.read = gcfs_read,
	.opendir = gcfs_opendir,
	.readdir = gcfs_readdir,
	.init = gcfs_init,
	.destroy = gcfs_destroy,
};
