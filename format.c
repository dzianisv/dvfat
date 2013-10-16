#include <string.h>
#include <stdio.h>
#include <sys/fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include "libdfat.h"

#define DEBUG 1

int get_file_size(int fd);

int main(int argc, char** argv)
{
	struct superblock_info sinfo;
	/* Init by default */
	sinfo.magic = 0xDEDE;
	sinfo.label[0] = 0x0;
	sinfo.cluster_size = 1024;
	sinfo.sector_size = 512;
	sinfo.fat_size = 0x0;


	if(argc<6 || !strcmp(argv[0], "--help")) {
		printf("mkfs.dfat <device> -s <sector size> -c <cluster size> -n <label>\n");
		return -1;
	}
	//reading arguments
	for(int i = 0; i< argc; i++) {
		if(strcmp("-s", argv[i]) == 0)
		{
			sscanf(argv[++i], "%hu", &sinfo.sector_size);
		}

		else if(strcmp("-c", argv[i]) == 0)
		{
			sscanf(argv[++i], "%hu", &sinfo.cluster_size);
		}


		else if(strcmp("-n", argv[i]) == 0)
		{
			strcpy(sinfo.label, argv[++i]);
		}
	}

	#if DEBUG
		printf("Sector size: %hu, cluster size %hu\nLabel: %s\n", 
		       sinfo.sector_size, sinfo.cluster_size, sinfo.label);
		printf("record_info size: %u\n", sizeof(dir_record_t));		
	#endif

		printf("Starting formating...\n");
		
		int fd = open(argv[1], O_WRONLY | O_CREAT);
		if(fd == -1 ) {
			perror("Device open error");
			return -2;
		}

		int n =   (get_file_size(fd) - sinfo.sector_size)/(sinfo.cluster_size + sizeof(struct fat_record));
		printf("FAT size: %u B; FAT entries: %u\n", n*sizeof(struct fat_record), n);

		int writed = 0x0;

		printf("Writing superblock...\n");
		sinfo.fat_size = n*sizeof(struct fat_record);
		writed += write(fd, &sinfo, sizeof(sinfo));
		/* Fill leave free space by null */
		unsigned int null = 0x0;
		while( writed < sinfo.sector_size )
		      writed +=  write(fd, &null, sizeof(null));

		printf("Writing inittialy FAT table\n");
		struct fat_record fr;
		fr.index = 1;
		writed += write(fd, &fr, sizeof(fr));

		printf("\t\tClearing file system...\n");
		int size = get_file_size(fd);  
		while( writed < size )
		      writed +=  write(fd, &null, sizeof(null));
		printf("Formated!\n\n");
		return close(fd);
}

int get_file_size(int fd) 
{
	struct stat buf;
    fstat(fd, &buf);
    return buf.st_size;
}