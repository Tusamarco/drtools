#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <inttypes.h>

#include <error.h>
#include <tables_dict.h>
#include <fil0fil.h>

#include "innochecksum.h"

static time_t timestamp = 0;

// Global flags from getopt
bool deleted_pages_only = 0;
bool debug = 0;
bool process_compact = 0;
bool process_redundant = 0;

dulint filter_id;
bool use_filter_id = 0;

bool count_pages = 0;
bool ignore_crap = 0;
unsigned int page_counters[1000][10000UL]; // FIXME: need a hash here
unsigned long long cache_size = 104857600; // 100M
unsigned long long ib_size = 0;
ulint max_page_id = 0;


void usage();

// Checks if *page is valid InnoDB page
int valid_innodb_page(page_t *page){
    char buffer[32];
    int version = 0; // 1 - new, 0 - old
    unsigned int page_n_heap, oldcsumfield;
    int inf_offset = 0, sup_offset = 0;

    if(debug){
	fprintf(stderr, "Fil Header\n");
	fprintf(stderr, "\tFIL_PAGE_SPACE:                   %08lX\n", mach_read_from_4(page + FIL_PAGE_SPACE_OR_CHKSUM));
	fprintf(stderr, "\tFIL_PAGE_OFFSET:                  %08lX\n", mach_read_from_4(page + FIL_PAGE_OFFSET));
	fprintf(stderr, "\tFIL_PAGE_PREV:                    %08lX\n", mach_read_from_4(page + FIL_PAGE_PREV));
	fprintf(stderr, "\tFIL_PAGE_NEXT:                    %08lX\n", mach_read_from_4(page + FIL_PAGE_NEXT));
	fprintf(stderr, "\tFIL_PAGE_LSN:                     %08lX\n", mach_read_from_4(page + FIL_PAGE_LSN));
	fprintf(stderr, "\tFIL_PAGE_TYPE:                        %04lX\n", mach_read_from_2(page + FIL_PAGE_TYPE));
	fprintf(stderr, "\tFIL_PAGE_FILE_FLUSH_LSN: %08lX %08lX\n", mach_read_from_8(page + FIL_PAGE_FILE_FLUSH_LSN).high, mach_read_from_8(page + FIL_PAGE_FILE_FLUSH_LSN).low);
	fprintf(stderr, "\tFIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID: %08lX\n", mach_read_from_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID));
    	oldcsumfield = mach_read_from_4(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM);
	fprintf(stderr, "\tFIL_PAGE_END_LSN_OLD_CHKSUM:      %08X\n", oldcsumfield);
    	}
    if(mach_read_from_4(page + FIL_PAGE_OFFSET) > max_page_id){
	if(debug) fprintf(stderr, "page_id %lu is bigger than maximum possible %lu\n", mach_read_from_4(page + FIL_PAGE_OFFSET), max_page_id);
        if(debug) fprintf(stderr, "Invalid INDEX page\n\n");
	return 0;
	}
    page_n_heap = mach_read_from_4(page + PAGE_HEADER + PAGE_N_HEAP);
    if(debug) fprintf(stderr, "Page Header\n");
    if(debug) fprintf(stderr, "\tPAGE_N_HEAP: %08X\n", page_n_heap);
    version = ((page_n_heap & 0x80000000) == 0) ? 0 : 1;
    if(debug) {
	    fprintf(stderr, "\tVersion: %s\n", (version == 1)? "COMPACT" : "REDUNDANT");
    	}
    if(version == 1){
        inf_offset = PAGE_NEW_INFIMUM;
        sup_offset = PAGE_NEW_SUPREMUM;
        }
    else{
        inf_offset = PAGE_OLD_INFIMUM;
        sup_offset = PAGE_OLD_SUPREMUM;
        }
    memcpy(buffer, page + inf_offset, 7);
    buffer[7] = '\0';
    if(debug) fprintf(stderr, "Page infimum record: %s, len=%zu, position=%u\n", buffer, strlen(buffer), inf_offset);
    if(strncmp(buffer, "infimum", strlen(buffer)) == 0 ){
        memcpy(buffer, page + sup_offset, 8);
        buffer[8] = '\0';
        if(debug) fprintf(stderr, "Page supremum record: %s, len=%zu, position=%u\n", buffer, strlen(buffer), sup_offset);
        if(strncmp(buffer, "supremum", 8) == 0 ){
            if(debug) fprintf(stderr, "Valid INDEX page\n");
            return 1;
            }
        }
    if(debug) fprintf(stderr, "Invalid INDEX page\n\n");
    return 0;
}

inline int valid_innodb_checksum(page_t *p){
	ulint oldcsum, oldcsumfield, csum, csumfield;
	int result = 0;

	// Checking old style checksums
	oldcsum= buf_calc_page_old_checksum(p);
	oldcsumfield= mach_read_from_4(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM);
	if(debug) fprintf(stderr, "Old checksum: calculated=%lu, stored=%lu\n", oldcsum, oldcsumfield);
	if (oldcsumfield != oldcsum){
		result = 0;
		goto valid_innodb_checksum_exit;
		}
	// Checking new style checksums
	csum= buf_calc_page_new_checksum(p);
	csumfield= mach_read_from_4(p + FIL_PAGE_SPACE_OR_CHKSUM);
	if (csumfield != 0 && csum != csumfield){
		result = 0;
		goto valid_innodb_checksum_exit;
		}
	// return success
	result = 1;
valid_innodb_checksum_exit:
	return result;
}

/*******************************************************************/
void process_ibpage(page_t *page, int is_index) {
    ulint page_id;
    dulint index_id;
    int fn;

    // Get page info
    page_id = mach_read_from_4(page + FIL_PAGE_OFFSET);
    index_id = mach_read_from_8(page + PAGE_HEADER + PAGE_INDEX_ID);
    
    // Skip empty pages
    if (ignore_crap && index_id.high == 0 && index_id.low == 0) goto process_ibpage_exit;
    
    // Skip tables if filter used
    if (use_filter_id && (index_id.low != filter_id.low || index_id.high != filter_id.high)) goto process_ibpage_exit;
        
    if (count_pages) {
        if (index_id.high >= 1000) {
            if (ignore_crap) goto process_ibpage_exit;
            printf("ERROR: Too high tablespace id! %ld >= 1000!\n", index_id.high);
            exit(1);
        }

        if (index_id.low >= 10000) {
            if (ignore_crap) goto process_ibpage_exit;
            printf("ERROR: Too high index id! %ld >= 10000!\n", index_id.low);
            exit(1);
        }
        
        page_counters[index_id.high][index_id.low]++;
        goto process_ibpage_exit;
    }
        
    // Create table directory
    char dir_prefix[256];
    char dir_name[256];
    char file_name[256];
    if(is_index){
            sprintf(dir_prefix, "pages-%u/FIL_PAGE_INDEX", (unsigned int)timestamp);
            if(mkdir(dir_prefix, 0755)&& errno != EEXIST) { 
                fprintf(stderr, "Can't make a directory %s: %s\n", dir_prefix, strerror(errno)); 
                exit(-1);
                }
            sprintf(dir_name, "%s/%lu-%lu", dir_prefix, index_id.high, index_id.low);
            if(mkdir(dir_name, 0755) && errno != EEXIST) {
                fprintf(stderr, "Can't make a directory %s: %s\n", dir_name, strerror(errno)); 
                exit(-1);
                }
    	}
    else{
            sprintf(dir_name, "pages-%u/FIL_PAGE_TYPE_BLOB", (unsigned int)timestamp);
            if(mkdir(dir_name, 0755)&& errno != EEXIST) { 
                fprintf(stderr, "Can't make a directory %s: %s\n", dir_name, strerror(errno));
                exit(-1);
                }

        }
    
    // Compose page file_name
    if(is_index){
        sprintf(file_name, "%s/%08du-%08lu.page", dir_name, 0, page_id);
        }
    else{
        sprintf(file_name, "%s/page-%08lu.page", dir_name, page_id);
        }
    
    if(debug) fprintf(stderr, "Read page #%lu.. saving it to %s\n", page_id, file_name);

    // Save page data
    fn = open(file_name, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (!fn) error("Can't open file to save page!");
    if(-1 == write(fn, page, UNIV_PAGE_SIZE)){
	fprintf(stderr, "Can't write a page on disk: %s\n", strerror(errno));
	exit(-1);	
	}
    close(fn);
process_ibpage_exit:
	return;
}

// Prints size in human readable form
char* h_size(unsigned long long int size, char* buf){
	unsigned int power = 0;
	double d_size = size;
	while(d_size >= 1024){
		d_size /=1024;
		power += 3;
	}
	sprintf(buf, "%3.3f", d_size);
	switch(power){
		case 3: sprintf(buf, "%s %s", buf, "kiB"); break;
		case 6: sprintf(buf, "%s %s", buf, "MiB"); break;
		case 9: sprintf(buf, "%s %s", buf, "GiB"); break;
		case 12: sprintf(buf, "%s %s", buf, "TiB"); break;
		default: sprintf(buf, "%s exp(+%u)", buf, power); break;
	}
	return buf;
}
/*******************************************************************/
void process_ibfile(int fn) {
    unsigned int read_bytes;
    page_t *cache = malloc(cache_size);
    char tmp[20];
    off_t pos = 0;
    off_t offset = 0;
    off_t pos_prev = 0;
    time_t ts, ts_prev;
    int index_page_flag = 0;
    int checksum_flag = 0;
    
    if (!cache){
        fprintf(stderr, "Can't allocate memory (%s) for disk cache\n", h_size(cache_size, tmp));
        error("Disk cache allocation failed");
        }
    

    // Create pages directory
    timestamp = time(0);
    if (!count_pages) {
        sprintf(tmp, "pages-%u", (unsigned int)timestamp);
        mkdir(tmp, 0755);
    }
    
    if(cache_size > SSIZE_MAX){
        fprintf(stderr, "Cache can't be bigger than %lu bytes\n", SSIZE_MAX);
        error("Disk cache size is too big");
        }
    // Read pages to the end of file
    ts = time(0);
    ts_prev = ts;
    while ((read_bytes = read(fn, cache, cache_size)) != -1) {
        unsigned long i = 0;
	if(read_bytes == 0) break;
        if(debug) fprintf(stderr, "Read %u bytes from disk to RAM\n\n", read_bytes);
	offset = lseek(fn, 0, SEEK_CUR) - read_bytes;
        while (i < read_bytes) {
            // don't process a piece of file if its size < an InnoDB page
            if(read_bytes - i < UNIV_PAGE_SIZE) break;
            if(debug) fprintf(stderr, "Offset %ju(0x%jX) bytes\n", offset, offset);
	    index_page_flag = valid_innodb_page(cache + i)? 1 : 0;
            if(debug) fprintf(stderr, "Checksum... ");
	    checksum_flag = valid_innodb_checksum(cache + i)? 1 : 0;
            if(debug) {if (checksum_flag) {fprintf(stderr, "OK\n\n"); } else { fprintf(stderr, "Not OK\n\n"); }}

            if (checksum_flag || index_page_flag) {
                process_ibpage(cache + i, index_page_flag);
            	i+=UNIV_PAGE_SIZE;
            	pos += UNIV_PAGE_SIZE;
		offset += UNIV_PAGE_SIZE;
                }
            else{
		i++;
            	pos++;
		offset++;
                }
            if ((pos - pos_prev) > 0.01 * ib_size) {
		ts = time(0);
		if((ts != ts_prev)){
                	char buf[32];
			struct tm timeptr;
			time_t t = (ib_size - pos)/((pos - pos_prev)/(ts - ts_prev)) + ts;
			memcpy(&timeptr,localtime(&t), sizeof(timeptr));
			strftime(buf, sizeof(buf), "%F %T", &timeptr);
                	fprintf(stderr, "%.2f%% done. %s ETA(in %02lu:%02lu hours). Processing speed: %ju B/sec\n", 
				100.0 * pos / ib_size, 
				buf,
				(t - ts)/3600,
				((t - ts) - ((t - ts)/3600)*3600)/60,
				((pos - pos_prev))/(ts - ts_prev)
				);
        	        pos_prev = pos;
			ts_prev = ts;
        	        }
		}
            }
        }
}

/*******************************************************************/
int open_ibfile(char *fname) {
    struct stat st;
    int fn;
    char buf[255];

    fprintf(stderr, "Opening file: %s\n", fname);
    fprintf(stderr, "File information:\n\n");
       
    if(stat(fname, &st) != 0) {
	printf("Errno = %d, Error = %s\n", errno, strerror(errno));
	usage();
	exit(-1);
       }
    fprintf(stderr, "ID of device containing file: %12ju\n", st.st_dev);
    fprintf(stderr, "inode number:                 %12ju\n", st.st_ino);
    fprintf(stderr, "protection:                   %12o ", st.st_mode);
    switch (st.st_mode & S_IFMT) {
        case S_IFBLK:  fprintf(stderr, "(block device)\n");            break;
        case S_IFCHR:  fprintf(stderr, "(character device)\n");        break;
        case S_IFDIR:  fprintf(stderr, "(directory)\n");               break;
        case S_IFIFO:  fprintf(stderr, "(FIFO/pipe)\n");               break;
        case S_IFLNK:  fprintf(stderr, "(symlink)\n");                 break;
        case S_IFREG:  fprintf(stderr, "(regular file)\n");            break;
        case S_IFSOCK: fprintf(stderr, "(socket)\n");                  break;
        default:       fprintf(stderr, "(unknown file type?)\n");                break;
        }
    fprintf(stderr, "number of hard links:        %12zu\n", st.st_nlink);
    fprintf(stderr, "user ID of owner:             %12u\n", st.st_uid);
    fprintf(stderr, "group ID of owner:            %12u\n", st.st_gid);
    fprintf(stderr, "device ID (if special file):  %12ju\n", st.st_rdev);
    fprintf(stderr, "blocksize for filesystem I/O: %12lu\n", st.st_blksize);
    fprintf(stderr, "number of blocks allocated:   %12ju\n", st.st_blocks);
    fprintf(stderr, "time of last access:          %12lu %s", st.st_atime, ctime(&(st.st_atime)));
    fprintf(stderr, "time of last modification:    %12lu %s", st.st_mtime, ctime(&(st.st_mtime)));
    fprintf(stderr, "time of last status change:   %12lu %s", st.st_ctime, ctime(&(st.st_ctime)));
    h_size(st.st_size, buf);
    fprintf(stderr, "total size, in bytes:         %12jd (%s)\n\n", (intmax_t)st.st_size, buf);

    fn = open(fname, O_RDONLY, 0);
    if (fn == -1){
        perror("Can't open file");
        exit(-1);
        }
    if(ib_size == 0){ // determine tablespace size if not given
    	if(st.st_size != 0){
    		ib_size = st.st_size;
   	 	}
    	}
    if(ib_size == 0){
        fprintf(stderr, "Can't determine size of %s. Specify it manually with -t option\n", fname);
        exit(-1);
        }
    fprintf(stderr, "Size to process:              %12llu (%s)\n", ib_size, h_size(ib_size, buf));
    max_page_id = ib_size/UNIV_PAGE_SIZE;
       
    return fn;
}

/*******************************************************************/
void init_page_counters() {
    int i, j;
    
    for (i = 0; i < 1000; i++)
        for (j = 0; j < 10000; j++)
            page_counters[i][j] = 0;
}

/*******************************************************************/
void dump_page_counters() {
    int i, j;
    
    for (i = 0; i < 1000; i++)
        for (j = 0; j < 10000; j++) {
            if (page_counters[i][j] == 0) continue;
            printf("%d:%d\t%u\n", i, j, page_counters[i][j]);
        }
}

/*******************************************************************/
void set_filter_id(char *id) {
    int cnt = sscanf(id, "%lu:%lu", &filter_id.high, &filter_id.low);
    if (cnt < 2) {
        error("Invalid index id provided! It should be in N:M format, where N and M are unsigned integers");
    }
    use_filter_id = 1;
}

/*******************************************************************/
void usage() {
    error(
      "Usage: ./page_parser -4|-5 [-dDhcCV] -f <innodb_datafile> [-T N:M] [-s size] [-t size]\n"
      "  Where\n"
      "    -h  -- Print this help\n"
      "    -V  -- Print debug information\n"
      "    -d  -- Process only those pages which potentially could have deleted records (default = NO)\n"
      "    -s size -- Amount of memory used for disk cache (allowed examples 1G 10M). Default 100M\n"
      "    -T  -- retrieves only pages with index id = NM (N - high word, M - low word of id)\n"
      "    -c  -- count pages in the tablespace and group them by index id\n"
      "    -C  -- count pages in the tablespace and group them by index id (and ignore too high/zero indexes)\n"
      "    -t  size -- Size of InnoDB tablespace to scan. Use it only if the parser can't determine it by himself.\n"
    );
}

/*******************************************************************/
int main(int argc, char **argv) {
	int fn = 0, ch;
        unsigned long long m;
        char suffix;
	char buf[255];
	char ibfile[1024];

	while ((ch = getopt(argc, argv, "45VhdcCf:T:s:t:")) != -1) {
		switch (ch) {
			case 'd':
				deleted_pages_only = 1;
				break;
			case 'f':
				strncpy(ibfile, optarg, sizeof(ibfile));
				break;
			case 'V':
				debug = 1;
				break;

			case '4':
				process_redundant = 1;
				break;
			case '5':
				process_compact = 1;
				break;

			case 's':
				sscanf(optarg, "%llu%c", &m, &suffix);
				switch (suffix) {
					case 'k':
					case 'K':
						cache_size = m * 1024;
						break;
					case 'm':
					case 'M':
						cache_size = m * 1024 * 1024;
						break;
					case 'g':
					case 'G':
						cache_size = m * 1024 * 1024 * 1024;
						break;
					default:
						fprintf(stderr, "Unrecognized size suffix %c\n", suffix);
						usage();
						exit(-1);
					}
				if(cache_size < UNIV_PAGE_SIZE){
					fprintf(stderr, "Disk cache size %llu can't be less than %u\n", cache_size, UNIV_PAGE_SIZE);
					usage();
					exit(-1);
					}
				cache_size = (cache_size / UNIV_PAGE_SIZE ) * UNIV_PAGE_SIZE;
				fprintf(stderr, "Disk cache:                   %12llu (%s)\n\n", cache_size, h_size(cache_size, buf));
				break;
			case 't':
				sscanf(optarg, "%llu%c", &m, &suffix);
				switch (suffix) {
					case 'k':
					case 'K':
						ib_size = m * 1024;
						break;
					case 'm':
					case 'M':
						ib_size = m * 1024 * 1024;
						break;
					case 'g':
					case 'G':
						ib_size = m * 1024 * 1024 * 1024;
						break;
					case 't':
					case 'T':
						ib_size = m * 1024 * 1024 * 1024 * 1024;
						break;
					default:
						fprintf(stderr, "Unrecognized size suffix %c\n", suffix);
						usage();
						exit(-1);
					}
				break;
			case 'T':
				set_filter_id(optarg);
				break;
			case 'c':
				count_pages = 1;
				ignore_crap = 0;
				init_page_counters();
				break;
			case 'C':
				count_pages = 1;
				ignore_crap = 1;
				init_page_counters();
				break;
			default:
			case '?':
			case 'h':
				usage();
				exit(-1);
			}
		}
	fn = open_ibfile(ibfile);
	if (fn != 0) {
		process_ibfile(fn);
		close(fn);
		if (count_pages) {
			dump_page_counters();
			}
		} 
	else usage();
	return 0;
}
