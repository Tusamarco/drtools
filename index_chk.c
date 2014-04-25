#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <dirent.h>

#include <sys/types.h>
#include <sys/stat.h>

typedef unsigned char byte;
typedef unsigned long int ulint;
typedef unsigned int uint;
typedef unsigned long long int ullint;
typedef unsigned char uchar;
typedef byte page_t;

#define UNIV_PAGE_SIZE (16*1024)
#define FIL_NULL (0xFFFFFFFF)

#define FIL_PAGE_OFFSET         4
#define FIL_PAGE_PREV           8
#define FIL_PAGE_NEXT           12

#define FIL_PAGE_DATA         38
#define FSEG_PAGE_DATA              FIL_PAGE_DATA
#define PAGE_HEADER     FSEG_PAGE_DATA

#define PAGE_LEVEL       26

void usage(char *prg){
	printf("Usage: %s -f /path/to/dir-pages\n", prg);
}

ulint mach_read_from_2(uchar *b){
	return( ((ulint)(b[0]) << 8)
			+ (ulint)(b[1]));
}

ulint mach_read_from_4(uchar *b){
	return( ((ulint)(b[0]) << 24)
			+ ((ulint)(b[1]) << 16)
			+ ((ulint)(b[2]) << 8)
			+ (ulint)(b[3]));
}
int main(int argc, char** argv){
	int c;
	int dir;
	int is_dir = 0;
	byte page[UNIV_PAGE_SIZE];
	char pages_dir[1024] = "";
	struct stat st;

	while(-1 != (c = getopt(argc, argv, "f:h"))){
		switch(c){
			case 'f':
				strncpy(pages_dir, optarg, sizeof(pages_dir));
				break;
			case 'h':
			default:
				usage(argv[0]); exit(EXIT_FAILURE);
		}
	}
	if(strncmp(pages_dir, "", sizeof(pages_dir)) == 0){
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	if(stat(pages_dir, &st) == 0){
		if(S_ISDIR(st.st_mode)){
			is_dir = 1;
		}
	}else{
		perror("stat");
		fprintf(stderr, "Can't stat %s\n", pages_dir);
		exit(EXIT_FAILURE);
	}
	if(is_dir){
		DIR *src_dir;
		struct dirent *de;
		src_dir = opendir(pages_dir);
		int page_found = 0;
		char page_file[1024];
		uint page_level = 0;

		while(NULL != (de = readdir(src_dir))){
			if(!strncmp(de->d_name, ".", sizeof(de->d_name))) continue;
			if(!strncmp(de->d_name, "..", sizeof(de->d_name))) continue;
			snprintf(page_file, sizeof(page_file), "%s/%s", pages_dir, de->d_name); // get file with a page
			page_level = mach_read_from_2(page + PAGE_HEADER + PAGE_LEVEL);
			if(page_level == 0){
				page_found = 1;
				break;
			}
		}
		if(!page_found){
			fprintf(stderr, "Directory %s doesn't contain any leaf pages\n", pages_dir);
			exit(EXIT_FAILURE);
			}
		int fn = open(page_file, O_RDONLY, 0);
		page_t *page = malloc(UNIV_PAGE_SIZE);
		int read_bytes = read(fn, page, UNIV_PAGE_SIZE);
		if(read_bytes != UNIV_PAGE_SIZE){
			fprintf(stderr, "Couldn't read %u bytes from %s\n", UNIV_PAGE_SIZE, page_file);
			exit(EXIT_FAILURE);
			}
		close(fn);
		uint page_id = mach_read_from_4(page + FIL_PAGE_OFFSET);
		uint page_prev = mach_read_from_4(page + FIL_PAGE_PREV);
		while(page_prev != FIL_NULL){
			snprintf(page_file, sizeof(page_file), "%s/%08lu-%08u.page", pages_dir, 0UL, page_prev);
			if(-1 == (fn = open(page_file, O_RDONLY, 0))){
				fprintf(stderr, "Couldn't open file %s\n", page_file);
				exit(EXIT_FAILURE);
			}
			read_bytes = read(fn, page, UNIV_PAGE_SIZE);
			if(read_bytes != UNIV_PAGE_SIZE){
				fprintf(stderr, "Couldn't read %u bytes from %s\n", UNIV_PAGE_SIZE, page_file);
				exit(EXIT_FAILURE);
			}
			close(fn);
			page_prev = mach_read_from_4(page + FIL_PAGE_PREV);
			}
		// So, now in page_prev we have page_id of the first page from the list
		// and the page itself in the memory buffer `page`
		// Check if we can get from the beginning to the end of list
		uint page_next = mach_read_from_4(page + FIL_PAGE_NEXT);
		while(page_next != FIL_NULL){
			snprintf(page_file, sizeof(page_file), "%s/%08lu-%08u.page", pages_dir, 0UL, page_next);
			if(-1 == (fn = open(page_file, O_RDONLY, 0))){
				fprintf(stderr, "Couldn't open file %s\n", page_file);
				exit(EXIT_FAILURE);
			}
			read_bytes = read(fn, page, UNIV_PAGE_SIZE);
			if(read_bytes != UNIV_PAGE_SIZE){
				fprintf(stderr, "Couldn't read %u bytes from %s\n", UNIV_PAGE_SIZE, page_file);
				exit(EXIT_FAILURE);
			}
			close(fn);
			page_next = mach_read_from_4(page + FIL_PAGE_NEXT);
		}
		// if we got here - the directory contains all leaf pages from the index
		printf("OK\n");
		exit(EXIT_SUCCESS);

	}else{
		fprintf(stderr, "%s must be a directory with InnoDB pages(e.g. pages-1374663129/FIL_PAGE_INDEX/0-410)\n", pages_dir);
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}
	exit(EXIT_SUCCESS);
}						
