#ifndef _LIBDFAT_H
#define _LIBDFAT_H

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#define SIZE_NAME 119
#define LIST_SIZE 300
#define MAX_FILE_COUNT 1024

#define DEBUG 1

typedef unsigned long laddr_t;
typedef unsigned int cluster_t;
typedef unsigned char byte_t;


/* File record information */
typedef struct dir_record_struct 
{
	/* Name of record: 119 bytes */
	char name[SIZE_NAME];
	/* Record flags: 1 byte */
	/* bit 7: 1 - dir, 0 - file */
	/* bit 6-0 rwx hsa */
	unsigned char flags;
	/* First file block index: 4 bytes */
	cluster_t index;
	/* File size: 4 bytes */
	/*For folders - child record count */
	unsigned int size;
} dir_record_t; /* 128 bytes */


/* FAT record */
struct fat_record
{
	/* Next block index 
		if 0x0 than block is free
		if 0x1than is last file block
	*/

	cluster_t index; /* 32 bits  = 4 bytes*/
};

/*Superblock information*/
/* This struct will be located at first sector on device */

struct superblock_info
{
	union { /* 2 bytes */
		unsigned char magic_bytes[2];
		unsigned short magic;
	};
	/* FS Cluster Size: 2 bytes*/
	unsigned short cluster_size;
	/*Sector size: 2 bytes*/
	unsigned short sector_size;
	/*FAT size: 4 bytes */
	cluster_t fat_size;
	/* File System Label 80 bytes*/
	char label[80];

};

/* list for folder items */
struct list {
	dir_record_t array[LIST_SIZE];
	unsigned int count;
};


struct superblock_info sinfo;
int fd;
char *device_file;


struct fat_record *FAT;
cluster_t fat_count;

/*STD debug*/
void debug(const char *format, ...);
void error(const char *format, ...);
/*Functions for work with files list */
void list_append(dir_record_t r, struct list* l);
void list_clear(struct list *l);

/*Init FS*/
int dfat_load(const char *device);

void dfat_close();

/*Init FAT*/
int dfat_fat_load();

/*Geting directory record from cluster cluster_num with record_num */
dir_record_t dfat_read_dir_record(cluster_t cluster_num, unsigned char record_num);

/*Get 2 cluster offset*/
laddr_t dfat_cluster_offset(cluster_t cluster_num);

/*Writing directory record from cluster cluster_num with record_num */
int dfat_write_dir_record(laddr_t addr, dir_record_t r);

/*Print FAT to STDOUT */
void dfat_print_fat();

/*Read directory entries*/
/* Function allocate memory and return array of dir_record_t */
struct list *dfat_read_folder(cluster_t, struct list*);

/*Looking for dir record by full name*/
laddr_t dfat_find_dir_record(const char* path, dir_record_t *out_record);

/*File/dir exist */
int dfat_exist( const char *path );

/* Find free dir record in folder */
/* Lookong for free record in folder cluster*/
laddr_t dfat_find_free_dir_record(cluster_t cluster_num);

/* Separate path to array. Return array size */
int dfat_get_path(const char *path, char** separated);

cluster_t dfat_allocate_cluster(cluster_t prev_cluster);

/* Take a new cluster */
/* Return cluster number 
 * if not free space:	0
 */
cluster_t dfat_take_new_cluster(cluster_t prev_cluster/*Previous last cluster*/);

/* Looking for free fat record */
cluster_t dfat_find_free_cluster(cluster_t cluster_num/*Prev cluster*/);

/*Calculating free space*/
 size_t dfat_free_space();
 size_t dfat_total_space();

/* Write operations */
/****/
int dfat_create(const char* path, byte_t flags, dir_record_t *r);
int dfat_rmdir(const char* path);
int dfat_unlink(const char* path);
int dfat_rename(const char* path, const char* newpath);
int dfat_write(const char* path, void* buf, size_t size, off_t offset);
/*****/

int dfat_read_folder_by_path(const char *path, struct list* l);
int dfat_read(const char* path, void* buf, size_t size, off_t offset);

#endif