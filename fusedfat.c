#define _USE_GNU
#define _POSIX_C_SOURCE 199309
#define FUSE_USE_VERSION  26
#define USERDATA ((struct user_data*)(fuse_get_context()->private_data))
#define LOGFILE USERDATA->logfile

#include <fuse.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/stat.h>
#include "libdfat.h"

/******************************************************/
struct user_data {
  FILE *logfile;
};

FILE* log_open()
{
  FILE *logfile = fopen("fusedfat.log", "w");
  setvbuf(logfile, NULL, _IOLBF, 0);;
  return logfile;
}

void log_msg(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    vfprintf( LOGFILE, format, ap);
    fflush(LOGFILE);
}

/******************************************************/

int dfuse_usage()
{
    printf("dfuse_fuse <device> <mountpoint>\n\n");
    return 0;
}

void *dfuse_init(struct fuse_conn_info *conn)
{   
    return NULL;
}

void dfuse_destroy(void *userdata)
{
    fflush(LOGFILE);
    fclose(LOGFILE);
    dfat_close();
}

static int dfuse_getattr(const char *path, struct stat *stbuf)
{

  int res = 0; /* temporary result */
  memset(stbuf, 0, sizeof(struct stat));
  dir_record_t r;

  if( !dfat_find_dir_record(path, &r) )
    return -ENOENT;

  if( (r.flags & 0x80) ) {
    stbuf->st_mode = 0x4000 | 0777;
    stbuf->st_size = 0;
    stbuf->st_blocks = dfat_total_space();
    stbuf->st_blksize = sinfo.cluster_size;
  }
  else {
    stbuf->st_mode = 0x8000 | 0777;
    stbuf->st_size = r.size;
  }

  return 0;
}

static int dfuse_error(char *str)
{
    int ret = -errno;
    log_msg("    ERROR %s: %s\n", str, strerror(errno));
    return ret;
}

int dfuse_mkdir(const char *path, mode_t mode)
{
    debug("* dfuse_mkdir(path=\"%s\", mode=0%3o)\n", path, mode);

    return dfat_create(path, 0x80, NULL);
}

int dfuse_unlink(const char *path)
{
    int retstat = 0;

    debug("* dfuse_unlink(path=\"%s\")\n", path);

    retstat = dfat_unlink(path);

    if (retstat < 0)

    retstat = dfuse_error("unlink");

    return retstat;
}

int dfuse_rmdir(const char *path)
{
    int retstat = 0;
    debug("* dfuse_rmdir(path=\"%s\")\n", path);
    return dfat_rmdir(path);
}

int dfuse_rename(const char *path, const char *newpath)
{
    return dfat_rename(path, newpath);
}

int dfuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{  

  debug("* dfuse_create() %s\n", path);
  return dfat_create(path, 0x0, NULL);
}

int dfuse_open(const char *path, struct fuse_file_info *fi)
{
  if(!dfat_exist(path)) 
  {
    if(fi->flags & O_CREAT)
    {
      debug("* dfuse_open() creating file\n");    
      dfat_create(path, 0x0, NULL);
      return 0;
    }
    error("* dfuse_open() file not exists\n");
    return -ENOENT;
  }
  debug("* dfuse_open() %s: flags 0x%X\n", path, fi->flags);
  return 0;
}

int dfuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
  debug("* dfuse_read() %s\n", path);
  int readed = dfat_read(path, buf, size, offset);
  return readed;
}

int dfuse_write(const char *path, const char *buf, size_t size, off_t offset,
       struct fuse_file_info *fi)
{
  debug("* dfuse_write() %s\n", path);
  int writed = dfat_write(path, buf, size, offset);;
  return writed;
}

int dfuse_truncate (const char *path, off_t offset)
{
  debug("* dfuse_truncate() %s\n", path);
  dfat_unlink(path);
  dfat_create(path, 0x0, NULL);
  return 0;
}


int dfuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
               struct fuse_file_info *fi)
{

    debug("* dfuse_readdir() %s\n", path);

    int retstat = 0;
    struct list l;
    list_clear(&l);
    dfat_read_folder_by_path(path, &l);

    for(unsigned int i=0; i<l.count; i++) {
        filler(buf, l.array[i].name , NULL, 0);
    }
    return retstat;
}

/******************************************************************/
/* The fuse struct for storing FS operations functions addresses */
struct fuse_operations dfuse_oper = {
  .getattr = dfuse_getattr,
  .mkdir = dfuse_mkdir,
  .unlink = dfuse_unlink,
  .readdir = dfuse_readdir,
  .rmdir = dfuse_rmdir,
  .rename = dfuse_rename,
  .open = dfuse_open,
  .read = dfuse_read,
  .write = dfuse_write,
  .destroy = dfuse_destroy,
  .create = dfuse_create,
  .truncate = dfuse_truncate,
};

int main(int argc, char **argv)
{
    if ((argc < 3))
        return dfuse_usage();

    struct user_data *data = (struct user_data*) malloc(sizeof(struct user_data));
    data->logfile = log_open();

    dfat_load(argv[argc-2]);

    argv[argc-2] = argv[argc-1];
    argv[argc-1] = NULL;
    argc--;

    return fuse_main(argc, argv, &dfuse_oper, data);  
}