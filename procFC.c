/****************************************************************
//Steps to compile: 
//		gcc -Wall `pkg-config fuse --cflags --libs` -o procFS procFS.c
//	
//Steps to run
//		1. Create a temp directory myproc
//			mkdir myproc
//
//		2. Mount the procFS filesystem by executing
//			./procFS myproc/	 	
//
//	Now myproc is the my procFS directory
//		ls myproc --> lists all the files. Each file correspond to a 
//						process running on the system. filename is same as process id.
//
//		cat myproc/"Any process id you see in myproc directory"
//			--> This would show a status file for each proc. 
//              With current implementation it outputs the 
//				the information from /proc/"pid"/status file
// 
//  Assumptions: 
//		1. "cat" should be used only for available file in myproc directory. 
//			If cat used to display content of a file which does not exist then 
//			for current implementation the file system mount is corrupted. In order
//			to recover the filesytem should be unmounted (may be from different terminal/shell)
//			before trying anyother command.
//			Unmount using:  fusermount -u myproc/
//		
//		2. The max file path name is assumed to be 256.
//
//		3. The max file size for each proc status file is assumed to be 64KB	
//			
*/
			
#define FUSE_USE_VERSION 27
#include <fuse.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#define MAX_STAT_INFO 64*1024
#define MAX_FILE_NAME 256

/*
	isValidFileName --> checks if the filename passed is valid or not.
						With current implementation, only numeric file names are valid
						Valid Filenames: 123 4567
						Invalid filenames:  12ad abcd
*/
unsigned int isValidFileName(const char *filename){
	          

	unsigned int lenFile = strlen(filename);
	unsigned int index = 0;
      	for(index = 0; index < lenFile; index++){
        	if(((filename[index] < 48) || (filename[index] > 57)) && (filename[index] != '/'))
                	break;
       	}

       	if(index < lenFile)
        	return 0;
       	else
		return 1;
}

/* readProcStatFile --> reads the content of file into buffer and returns the size
						of bytes read.
*/	
unsigned int readProcStatFile(const char *filename, char *buffer){
	
	unsigned int size = 0;
	char fullpath[MAX_FILE_NAME];
	sprintf(fullpath, "/proc%s/status", filename);

	FILE *fp = fopen(fullpath, "r");

	if(fp != NULL){
		size = fread(buffer, sizeof(char), MAX_STAT_INFO, fp);
		
		if(size == 0){
			fputs("File reading error !!!", stderr);
		}	
	}	

	fclose(fp);		
		
	return size;
}
					
static int procFS_getattr(const char *path, struct stat *stbuf)
{
	int res = 0;

	memset(stbuf, 0, sizeof(struct stat));
	if (strcmp(path, "/") == 0) {
		stbuf->st_mode = S_IFDIR | 0755;
		stbuf->st_nlink = 2;
	} else if (isValidFileName(path)) {
		stbuf->st_mode = S_IFREG | 0444;
		stbuf->st_nlink = 1;
		char infobuffer[MAX_STAT_INFO];
		stbuf->st_size = readProcStatFile(path, &infobuffer[0]);
	} 
	else
		res = -ENOENT;

	return res;
}

static int procFS_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi)
{

	(void) offset;
	(void) fi;

	if (strcmp(path, "/") != 0)
		return -ENOENT;

	filler(buf, ".", NULL, 0);
	filler(buf, "..", NULL, 0);

	char *procPath = "/proc/";
	DIR *dp;
    	struct dirent *de;
    
	dp = opendir(procPath);
    
	if (dp == NULL)
        	return -errno;

    	while ((de = readdir(dp)) != NULL) {

		if(isValidFileName(de->d_name))
					filler(buf, de->d_name, NULL, 0);
		else 
			continue;
	}

        closedir(dp);


	return 0;
}

static int procFS_open(const char *path, struct fuse_file_info *fi)
{
	if (!isValidFileName(path + 1)) 
		return -ENOENT;

	if ((fi->flags & 3) != O_RDONLY)
		return -EACCES;

	return 0;
}

static int procFS_read(const char *path, char *buf, size_t size, off_t offset,
		      struct fuse_file_info *fi)
{
	(void) fi;
	size_t filesize = 0;
	char infobuffer[MAX_STAT_INFO];
	if(isValidFileName(path)){
		filesize = readProcStatFile(path, &infobuffer[0]);
		
		if(offset < filesize){
			if(offset + size > filesize)
				size = filesize - offset;
			memcpy(buf, &infobuffer[offset], size);
		}
		else{
			size = 0;
		}

		return size;
	}	
	else{
		return 0;
	}
}

static struct fuse_operations procFS_oper = {
	.getattr	= procFS_getattr,
	.readdir	= procFS_readdir,
	.open		= procFS_open,
	.read		= procFS_read,
};



int main(int argc, char *argv[])
{
	return fuse_main(argc, argv, &procFS_oper, NULL);
}
