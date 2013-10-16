#include "libdfat.h"
#include "list.h"

#include <string.h>
#include <errno.h>

#include <libgen.h>


void debug(const char *format, ...){
  #if DEBUG
	printf("\033[1;32m");
    va_list ap;
    va_start(ap, format);
    vfprintf( stdout, format, ap);
    fflush(stdout);
  #endif
}

void error(const char *format, ...){
  #if DEBUG
    fprintf(stderr, "\033[1;31m");
    va_list ap;
    va_start(ap, format);
    vfprintf( stderr, format, ap);
    fflush(stderr);
    printf("\033[1;32m");
  #endif
}

/* Init operations */
/******************************************************************************************/
int dfat_load(const char *device)
{
	printf("\033[1;32m");

	fd = open(device, O_RDWR);
	if( !fd )
		error("dfat_load() can't open device %s\n", device);
	debug("dfat_load() device '%s' opened (fd=%p)\n", device, fd);

	read(fd, &sinfo, sizeof(sinfo));

	debug("FS\tSector size: %hu, cluster size: %hu, fat size: %u\n", 
	       sinfo.sector_size, sinfo.cluster_size, sinfo.fat_size);

	debug("FS\tLabel: %s\n", sinfo.label);
	debug("FS\tDir records in clusters: %u\n", sinfo.cluster_size/sizeof(dir_record_t));
	debug("FS\t2 cluster offset: 0x%X\n", dfat_cluster_offset(2));

	dfat_fat_load();

	debug("FS\tfree clusters: %u\n", dfat_free_space());
}

void dfat_close()
{
	lseek(fd, sinfo.sector_size, SEEK_SET);
	write(fd, FAT+2, sinfo.fat_size);
	free(FAT);
	close(fd);
}

/*Init FAT*/
int dfat_fat_load()
{
	FAT = (struct fat_record*) malloc(sizeof(struct fat_record)*(sinfo.fat_size+2));


	lseek(fd, sinfo.sector_size, SEEK_SET);
	int readed = read(fd, FAT+2, sinfo.fat_size);

	if(readed < sinfo.fat_size)
	{
		perror("dfat_fat_load()");
		return readed;
	}

	fat_count = sinfo.fat_size/sizeof(struct fat_record);

	dfat_print_fat();

	return readed;
}

/* Comon operations */
/******************************************************************************************/
/*Geting directory record from cluster cluster_num with record_num */
dir_record_t dfat_read_dir_record(cluster_t cluster_num, unsigned char record_num)
{
	dir_record_t dir_record;
	dir_record.name[0]=0x0;

	if(cluster_num < 2)
	{
		error("dfat_read_dir_record() incorrect cluster number: %u\n\n", cluster_num);
		return dir_record;
	}

	if(record_num>=(sinfo.cluster_size)/sizeof(dir_record_t))
	{
		error("dfat_read_dir_record() incorrect record number: %u\n\n", record_num);
		return dir_record;
	}

	laddr_t clusterAddress = dfat_cluster_offset(cluster_num);
	laddr_t recordAddress = clusterAddress + sizeof(dir_record_t)*record_num;

	lseek(fd, recordAddress, SEEK_SET);
	int readed = read(fd, &dir_record, sizeof(dir_record));

	if(readed < sizeof(dir_record))
	{
		perror("dfat_read_dir_record()");
		error("dfat_read_dir_record(): can't read dir record at cluster %u and number %u\n",
		        cluster_num, (cluster_t) record_num);
	}

	return dir_record;
}

/*Writing directory record from cluster cluster_num with record_num */
int dfat_write_dir_record(laddr_t addr, dir_record_t r)
{
	lseek(fd, addr, SEEK_SET);
	int writed = write(fd, &r, sizeof(r));
	fdatasync(fd);

	if(writed < sizeof(r))
	{
		error("dfat_write_dir_record() %d %s record at address %X, fd = %p\n",
		  writed, strerror(errno), addr, fd);

		return -1;
	}

	return 0;
}

/*Get 2 cluster offset*/
laddr_t dfat_cluster_offset(cluster_t cluster_num)
{
	if(cluster_num == 0x0)
		/* Reserved for FREE CLUSTERS */
		return 0;
		/*Reserved for last cluster */
	if(cluster_num == 0x1)
		return 1;
	/*Return linear address for cluster */
	return sinfo.sector_size + sinfo.fat_size + sinfo.cluster_size*(cluster_num-2);
}

/*Print FAT to STDOUT */
void dfat_print_fat() 
{
	printf("FS\tPrinting %u FAT entries\n", fat_count);

	for(cluster_t i=2; i<fat_count+2;)
	{
		for( int j = 0; j<10 && i<fat_count+2; j++, i++)
			printf("%4u:%8u| ", i, FAT[i]);

		printf("\n");
	}
}

/* Read files/folders dir in folder */
struct list *dfat_read_folder(cluster_t cluster_num, struct list* l)
{
	//debug("dfat_read_folder():\n");
	int record_i = 0;
	cluster_t cluster_i = cluster_num;
	cluster_t ecount = sinfo.cluster_size / sizeof(dir_record_t);

	while(1) {
		dir_record_t r = dfat_read_dir_record(cluster_i, record_i);
		/* Readed free dir record */
		
		if(r.name[0] != 0x0) {
			list_append(r, l);
			//debug("\t%s\n", r.name);
		}
		record_i++;

		if(record_i == ecount) {
			record_i = 0;
			cluster_i = FAT[cluster_i].index;

			//debug("\tnext cluster %u\n", cluster_i);

			if( cluster_i == 1 ) 
				break;
		}
	}

	return l;
}

laddr_t dfat_find_dir_record(const char* path, dir_record_t *out_record)
{
	//debug("dfat_find_dir_record() for %s\n", path);
	/* Cluster point to first cluster where located root dir record */
	cluster_t cluster_i = 2; 
	byte_t record_i = 0;
	byte_t ecount = sinfo.cluster_size / sizeof(dir_record_t);

	/*Parse path*/
	char* s[MAX_FILE_COUNT];
	int count = dfat_get_path(path, s);
	int j = count-2;

	/*Preparing root record */
	dir_record_t r;
	r.name[0]='/';
	r.name[1]=0x0;
	r.flags = 0x80;
	r.size=0;
	r.index = 2;


	/* Return root record */
	if(j == -1) {
		if( out_record != NULL)
			memcpy(out_record, &r, sizeof(r));
		//debug("\tfounded root record '/' at 2:0\n");
		return dfat_cluster_offset(2);
	}

	/*Looking for record dir*/
	while(j!=-1) {
		
		while(1) {
			r = dfat_read_dir_record(cluster_i, record_i);

			if( strcmp(r.name, s[j])==0 )
			{
				if(j==0)
				{
					if(out_record != NULL)
						memcpy(out_record, &r, sizeof(r));
					laddr_t addr = dfat_cluster_offset(cluster_i) + record_i*sizeof(dir_record_t);
					//debug("\trecord finded at: %u:%u\n", cluster_i, record_i);
					return addr;
				}

				record_i = 0;
				cluster_i = r.index;
				//debug("\trecord .name=%s record .index=%u\n", r.name, r.index);
				break;
			}


			record_i++;

			if(record_i == ecount)
			{
				record_i = 0;
				cluster_i = FAT[cluster_i].index;
				/* Record not founded*/
				if(cluster_i < 2)
				{
					error("\tdir record with name %s don't exist\n", s[j]);
					errno = ENOENT;
					return 0;
				}

				continue;
			}

		}
		j--;
	}
}

int dfat_exist( const char *path )
{
	return dfat_find_dir_record(path, NULL) != 0;
}

/* Separate path to array. Return array size */
int dfat_get_path(const char *path, char** separated)
{
	char *dir = strdup(path);
	char *base = strdup(path);
	int i = 0;

	while( strcmp(base, "/") != 0 )
	{
		base = basename(base);
		dir = dirname(dir);
		
		separated[i++] = base;
		base = (char*) strdup(dir);
	}

	separated[i++] = basename(base);

	return i;
}

/* Find free dir record in folder, if don't have - take it! */
/* Lookong for free record in folder cluster and return absolute address*/
laddr_t dfat_find_free_dir_record(cluster_t cluster_num)
{
	int eof = 0;
	/*Record index in cluster */
	int record_i = 0;
	/* Current cluster for looking */
	cluster_t cluster_i = cluster_num;
	/* Count of dir records in cluster */
	cluster_t ecount = sinfo.cluster_size/sizeof(dir_record_t);

	while(1)
	{
		dir_record_t r = dfat_read_dir_record(cluster_i, record_i);
		/* Readed free dir record */
		if(r.name[0] == 0x0) {
			/* Calculating record dir absolute address */
			debug("dfat_find_free_dir_record() %u:%u\n", cluster_i, record_i);
			return ( dfat_cluster_offset(cluster_i) 
					+ record_i*sizeof(dir_record_t));
		}

		record_i++;

		/* Reached the end of cluster, looking for next cluster in FAT */
		if(record_i==ecount)
		{
			record_i = 0;

			if( FAT[cluster_i].index == 1 )
			{

				cluster_t new_cluster = dfat_allocate_cluster(cluster_i);
				debug("dfat_find_free_dir_record() %u:%u\n", new_cluster, 0);
				return dfat_cluster_offset(new_cluster);
			}
			else
				cluster_i = FAT[cluster_i].index;
		}
	}
}
/*Allocate new cluster*/
cluster_t dfat_allocate_cluster(cluster_t prev_cluster)
{
	cluster_t new_cluster = dfat_take_new_cluster(prev_cluster);
	
	if(new_cluster<2)
		return 0;

	if(prev_cluster>1)
	{
		FAT[prev_cluster].index = new_cluster;
	}
	/*Mark cluster as last*/
	FAT[new_cluster].index = 0x1;
	return new_cluster;
}

/* Take a new cluster */
/* Return cluster number 
 * if not free space:	0
 * if not last cluster:	1
 */
cluster_t dfat_take_new_cluster(cluster_t prev_cluster/*Previous last cluster*/)
{
	/*Loking for free cluster */
	for(cluster_t i = prev_cluster+1; i<fat_count+2; i++)
	{
		if(FAT[i].index==0) {
			return i;
		}
	}

	for(cluster_t i=3; i<prev_cluster; i++)
	{
		if(FAT[i].index==0) {
			return i;
		}
	}

	return 0;
}

cluster_t dfat_find_free_cluster(cluster_t cluster_num/*Prev cluster*/)
{
	for(cluster_t i=2; i<fat_count+2; i++)
	{
		if(FAT[i].index == 0)
			return i;
	}

	return 0;
}

size_t dfat_free_space()
{
	size_t space = 0;
	for(cluster_t i=2; i<fat_count+2; i++)
	{
		if(FAT[i].index == 0)
			space++;
	}

	return space;
}

size_t dfat_total_space()
{
	size_t space = 0;

	for(cluster_t i=2; i<fat_count+2; i++)
	{
		if(FAT[i].index != 0)
			space++;
	}

	return space; //cluster counts
}

/* Write operations */
/******************************************************************************************/
int dfat_create(const char* path, byte_t flags, dir_record_t* out)
{
	dir_record_t r;
	r.name[0] = 0x0;
	r.flags =flags;
	r.size = 0x0;
	r.index = 0x0;

	if( dfat_exist(path) )
	{
		error("dfat_create() file/folder exist %s\n", path);
		errno = EEXIST;
		return -EEXIST;
	}

	/* Looking for free cluster */
	cluster_t cluster = dfat_find_free_cluster(2);
	/*Checking for correct cluster number */
	if(cluster < 2)
	{
		error("dfat_create() fs don't have free cluster");
		errno = ENOSPC;
		return -ENOSPC;
	}
	
	debug("dfat_create() cluster assigned with file: %u\n", cluster);

	char *dir = strdup(path);
	char *filename = strdup(path);
	dir = dirname(dir);
	filename = basename(filename);

	dir_record_t parrent_folder;
	laddr_t addr = dfat_find_dir_record(dir, &parrent_folder);
	/* Checking for correct dir record */
	if(addr == 0) {
		error("dfat_create() can't find parrent folder dir record\n");
		errno = ENOENT;
		return -ENOENT;
	}

	debug("dfat_create() finded parrent folder: %s\n", parrent_folder.name);

	FAT[cluster].index = 0x1; /*Fill by EOF*/
	r.index = cluster;

	//fill r.name by null
	memset(r.name, 0, sizeof(r.name));
	/*Fill file name*/
	strcpy(r.name, filename);

	/*Get linear address of free dir record at cluster*/
	addr =  dfat_find_free_dir_record(parrent_folder.index);

	if( addr == 0 )
	{
		error("dfat_create() can't find free dir records at parrent folder\n");
		errno = ENOENT;
		return -ENOENT;
	}
	/*Move write cursor*/
	int seek = lseek(fd, addr, SEEK_SET);
	/* Write record to device */
	int writed = write(fd, &r, sizeof(r));
	fdatasync(fd);

	if( writed < sizeof(r) )
		perror("dfat_create_file()");

	#if DEBUG
		debug("dfat_create(): file/folder %s created at addr 0x%X [%u]\n", 
		        r.name, addr, sizeof(r) );
	#endif

	if(out != NULL)
		memcpy(out, &r, sizeof(r));

	return 0;
}

int dfat_unlink(const char* path)
{
	debug("dfat_unlink() path=%s\n", path);
	dir_record_t r;
	laddr_t addr = dfat_find_dir_record(path, &r);

	if( !addr )
	{
		error("dfat_unlink() don't exist %s\n", path);
		errno = ENOENT;
		return -1;
	}

	r.name[0] = 0x0;
	cluster_t c_next = r.index;
	cluster_t c_prev;
	cluster_t counter = 0;
	debug("\tfreeing clusters: ");
	while(c_next > 2)
	{
		counter++;
		debug("%u ", c_next);
		c_prev = c_next;
		c_next = FAT[c_prev].index;
		FAT[c_prev].index = 0x0;
	}
	debug("\n\tcleared %u cluster => %u kB\n", counter, counter*sinfo.cluster_size/1024);
	dfat_write_dir_record(addr, r);

	return 0;
}

int dfat_rmdir(const char* path)
{
	dir_record_t r;
	laddr_t addr = dfat_find_dir_record(path, &r);

	if( !addr )
	{
		errno = ENOENT;
		return -1;
	}

	struct list l;
	list_clear(&l);
	dfat_read_folder(r.index, &l);
	
	if(l.count)
	{
		errno = EEXIST;
		return -ENOTEMPTY;
	}

	r.name[0] = 0x0;

	cluster_t c_next = r.index;
	cluster_t c_prev;

	cluster_t counter = 0x0;
	debug("\tfreeing clusters: ");
	while(c_next > 2)
	{
		counter++;
		debug("%u ", c_next);
		c_prev = c_next;
		c_next = FAT[c_prev].index;
		FAT[c_prev].index = 0x0;
	}
	debug("\n\tcleared %u cluster => %u kB\n", counter, counter*sinfo.cluster_size/1024);
	dfat_write_dir_record(addr, r);

	return 0;
}

int dfat_rename(const char* path, const char* newpath)
{
	dir_record_t r;
	laddr_t addr = dfat_find_dir_record(path, &r);
	
	if(addr == 0 )
	{
		error("dfat_rename() %s don't exist\n", path);
		errno = ENOENT;
		return -ENOENT;
	}

	char *bname = strdup(newpath);
	bname = basename(bname);
	debug("\tnew name %s\n", bname);

	char *odir = strdup(path);
	char *ndir = strdup(newpath);
	odir = dirname(odir);
	ndir = dirname(ndir);
	
	if(strlen(bname) > 119 ) {
		errno = ENAMETOOLONG;
		return -ENAMETOOLONG;
	}	

	strcpy(r.name, bname);

	if(strcmp(odir, ndir) != 0) {
		/*Creating record */
		dir_record_t or;
		dfat_create(newpath, r.flags, &or);
		FAT[or.index].index = 0x0;

		laddr_t naddr = dfat_find_dir_record(newpath, NULL);
		if(naddr == 0x0) {
			errno = ENOSPC;
			return -ENOSPC;
		}

		dfat_write_dir_record(naddr, r);
		/*Deleting old record*/
		r.name[0] = 0x0;
		dfat_write_dir_record(addr, r);
		return 0;
	}
	else {
		dfat_write_dir_record(addr, r);
		return 0;
	}
}


int dfat_write(const char* path, void* buf, size_t size, off_t offset)
{
	debug("dfat_write() path=%s size=%u offset=%u\n", path, size, offset);
	int readed = 0;
	dir_record_t record;
	laddr_t addr = dfat_find_dir_record(path, &record);

	if(addr == 0) {
		errno = ENOENT;
		return -ENOENT;
	}

	/* Buffer offset*/
	off_t b_off=0;
	/* File offset */
	off_t f_off=0;
	/* Integer number of  chain cluster */
	int cluster_count = offset/sinfo.cluster_size;

	/* Offset in cluster */
	off_t cluster_offset = offset%sinfo.cluster_size;
	debug("\tcluster chain count: %u, cluster offset: %u\n", cluster_count, cluster_offset);
	/* Calculating file offset*/
	f_off = offset;
	/*Data size for read from FS*/
	size_t write_size;
	/* Run by cluster chain in FAT */
	cluster_t cluster = record.index;
	cluster_t prev_cluster;

	//alocated cluster counter
	cluster_t counter = 0;

	for(int i=0; i<cluster_count; i++) {
		prev_cluster = cluster;
		cluster = FAT[cluster].index;
		//allocate new cluster
		if(cluster<2) {
			counter++;
			cluster = dfat_allocate_cluster(prev_cluster);
			debug("\tallocated new cluster: %u\n", cluster);
			if(cluster < 2)
			{
				errno = ENOSPC;
				return -ENOSPC;
			}
		}
	}

	laddr_t data_addr = dfat_cluster_offset(cluster) + cluster_offset;
	write_size = (sinfo.cluster_size - cluster_offset > size)?(size):(sinfo.cluster_size - cluster_offset);
	lseek(fd, data_addr, SEEK_SET);
	int writed =  write(fd, buf, write_size);
	b_off += writed;
	f_off += writed;

	/*Seeking for next cluster and read available data*/
	while( size>b_off )
	{

		prev_cluster = cluster;
		cluster = FAT[cluster].index;

		//alocate space, if need
		if(cluster == 1) {
			cluster = dfat_allocate_cluster(prev_cluster);
			debug("\tallocated new cluster: %u\n", cluster);
			if(cluster < 2) {
				errno = ENOSPC;
				return -ENOSPC;
			}
			counter++;
		}
		laddr_t data_addr = dfat_cluster_offset(cluster);
		/*Calculating data size for read*/
		write_size = ( size>(b_off+sinfo.cluster_size) )?(sinfo.cluster_size):(size-b_off);
		/*Read data from cluster*/

		writed =  write(fd, buf+b_off, write_size);
		b_off += writed;
		f_off += writed;
	}

	if(f_off > record.size)
	{
		record.size = f_off;
		dfat_write_dir_record(addr, record);
		debug("\twritig new record at address 0x%X, new file size %u\n", addr, record.size);
	}


	fdatasync(fd);

	debug("\twrited %u Bytes. Allocated %u clusters\n", b_off, counter);

	return b_off;
}

/*Read operations */
/******************************************************************************************/
int dfat_read_folder_by_path(const char *path, struct list* l)
{
	dir_record_t r;
	
	if( !dfat_find_dir_record(path, &r) ) {
		errno = ENOENT;
		return -ENOENT;
	}
	dfat_read_folder(r.index, l);

	return 0;
}

int dfat_read(const char* path, void* buf, size_t size, off_t offset)
{
	dir_record_t record;
	laddr_t addr = dfat_find_dir_record(path, &record);

	if(addr == 0) {
		errno = ENOENT;
		return -ENOENT;
	}

	if(offset>record.size)
		return 0;

	/* Buffer offset*/
	off_t b_off=0;
	/* File offset */
	off_t f_off=0;
	/* Integer number of  chain cluster */
	int cluster_count = offset/sinfo.cluster_size;

	/* Offset in cluster */
	off_t cluster_offset = offset%sinfo.cluster_size;
	debug("\tcluster chain count: %u, cluster offset: %u\n", cluster_count, cluster_offset);

	/* Calculating file offset*/
	f_off = offset;
	/*Data size for read from FS*/
	size_t read_size;
	/* Run by cluster chain in FAT */
	cluster_t cluster = record.index;

	for(int i=0; i<cluster_count; i++) {
		cluster = FAT[cluster].index;
		if(cluster<2)
			return  -1;
	}

	laddr_t data_addr = dfat_cluster_offset(cluster) + cluster_offset;
	read_size = (record.size- offset > sinfo.cluster_size - cluster_offset)?(sinfo.cluster_size - cluster_offset):(record.size-offset);
	lseek(fd, data_addr, SEEK_SET);
	int readed =  read(fd, buf, read_size);;
	b_off += readed;
	f_off += readed;

	/*Seeking for next cluster and read available data*/
	while( size>b_off && f_off<record.size )
	{
		cluster = FAT[cluster].index;

		if(cluster == 1)
			return b_off;
		debug("\treading from next cluster: %u\n", cluster);

		laddr_t data_addr = dfat_cluster_offset(cluster);
		/*Calculating data size for read*/
		read_size = ( size>(b_off+sinfo.cluster_size) )?(sinfo.cluster_size):(size-b_off);
		read_size = ((read_size+f_off)<record.size)?(read_size):(record.size-f_off);
		/*Read data from cluster*/

		readed =  read(fd, buf+b_off, read_size);;
		b_off += readed;
		f_off += readed;
	}

	debug("dfat_read() path=%s size=%u offset=%u b_off=%u\n\tfile size = %u\n", 
		path, size, offset, b_off, record.size);
	return b_off;
}
/******************************************************************************************/