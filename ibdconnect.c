
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>

#include <unistd.h>
#include <string.h>

#include "include/ibdconnect.h"

void usage(char *p){
	fprintf(stderr, "Usage: %s -o <ibdata1> -f <.ibd> -d <database> -t <table>\n", p);
}

ulint get_space_id(page_t* p){
	return mach_read_from_4(p + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID);
}

static inline int ibrec_init_offsets_old(page_t *page, rec_t* rec, table_def_t* table, ulint* offsets) {
	ulint i = 0;
	ulint offs;

	// First field is 0 bytes from origin point
	rec_offs_base(offsets)[0] = 0;
	
	// Init first bytes
	rec_offs_set_n_fields(offsets, table->fields_count);
		
	/* Old-style record: determine extra size and end offsets */
	offs = REC_N_OLD_EXTRA_BYTES;
	if (rec_get_1byte_offs_flag(rec)) {
		offs += rec_offs_n_fields(offsets);
		*rec_offs_base(offsets) = offs;
		/* Determine offsets to fields */
		do {
			offs = rec_1_get_field_end_info(rec, i);
			if (offs & REC_1BYTE_SQL_NULL_MASK) {
				offs &= ~REC_1BYTE_SQL_NULL_MASK;
				offs |= REC_OFFS_SQL_NULL;
			}

            offs &= 0xffff;
    		if (rec + offs - page > UNIV_PAGE_SIZE) {
    			if (debug) printf("Invalid offset for field %lu: %lu\n", i, offs);
    			return 0;
    		}

			rec_offs_base(offsets)[1 + i] = offs;
		} while (++i < rec_offs_n_fields(offsets));
	} else {
		offs += 2 * rec_offs_n_fields(offsets);
		*rec_offs_base(offsets) = offs;
		/* Determine offsets to fields */
		do {
			offs = rec_2_get_field_end_info(rec, i);
			if (offs & REC_2BYTE_SQL_NULL_MASK) {
				offs &= ~REC_2BYTE_SQL_NULL_MASK;
				offs |= REC_OFFS_SQL_NULL;
			}

			if (offs & REC_2BYTE_EXTERN_MASK) {
				offs &= ~REC_2BYTE_EXTERN_MASK;
				offs |= REC_OFFS_EXTERNAL;
			}

            offs &= 0xffff;
    		if (rec + offs - page > UNIV_PAGE_SIZE) {
    			if (debug) printf("Invalid offset for field %lu: %lu\n", i, offs);
    			return 0;
    		}

			rec_offs_base(offsets)[1 + i] = offs;
		} while (++i < rec_offs_n_fields(offsets));
	}
	
	return 1;	
}
ullint update_sys_tables(int f, ulint space_id, char* db, char* t){
	const ulint root_page_pos = 8;
	page_t page[UNIV_PAGE_SIZE];
	ulint offsets[MAX_TABLE_FIELDS + 2];
	ulint next_page_pos;
	ulint page_id = 0;
	char buffer[UNIV_PAGE_SIZE] = "";
	char dbtpair[UNIV_PAGE_SIZE/2] = "";
	table_def_t* sys_tables;
	ullint table_id = 0;

	init_table_defs();
	sys_tables = &(table_definitions[0]);

	printf("Setting SPACE=%lu in SYS_TABLE for `%s`.`%s`\n", space_id, db, t);
	memcpy(dbtpair, strncat(db, "/", sizeof(dbtpair)), sizeof(dbtpair));
	memcpy(dbtpair, strncat(dbtpair, t, sizeof(dbtpair)), sizeof(dbtpair));

	// First, check if SPACE=space_id is already used
	printf("Check if space id %lu is already used\n", space_id);
	next_page_pos = root_page_pos;
	if((off_t)-1 == lseek(f, UNIV_PAGE_SIZE * root_page_pos, SEEK_SET)){
		fprintf(stderr, "Couldn't set file pointer to root page of ibdata\n");
		perror("update_sys_tables(): lseek()");
		exit(EXIT_FAILURE);
		}
	while(next_page_pos != FIL_NULL){
		if((off_t)-1 == lseek(f, UNIV_PAGE_SIZE * next_page_pos, SEEK_SET)){
			fprintf(stderr, "Couldn't set file pointer to root page of ibdata\n");
			perror("update_sys_indexes(): lseek()");
			exit(EXIT_FAILURE);
			}
		if(UNIV_PAGE_SIZE != read(f, page, UNIV_PAGE_SIZE)){
			fprintf(stderr, "Couldn't read %u bytes from ibdata file\n", UNIV_PAGE_SIZE);
			perror("update_sys_tables(): read()");
			exit(EXIT_FAILURE);
			}
		page_id = mach_read_from_4(page + FIL_PAGE_OFFSET);
		next_page_pos = mach_read_from_4(page + FIL_PAGE_NEXT);
		printf("Page_id: %lu, next page_id: %lu\n", page_id, next_page_pos);
		
		ulint rec_pos = PAGE_OLD_INFIMUM;
		while(rec_pos != PAGE_OLD_SUPREMUM){
			printf("Record position: %lX\n", rec_pos);
			ibrec_init_offsets_old(page, page + rec_pos, sys_tables, offsets);
			if (debug) {
				printf("Checking field lengths for a row (%s): ", sys_tables->name);
				printf("OFFSETS: ");
				int i;
				for(i = 0; i < rec_offs_n_fields(offsets); i++) {
					printf("%lu ", rec_offs_base(offsets)[i]);
					}
				bzero(buffer, UNIV_PAGE_SIZE);
				}
				strncpy(buffer, page + rec_pos, rec_offs_base(offsets)[1]);
				printf("\nDb/table: %s\n", buffer);
				printf("Space id: %lu (0x%lX)\n", mach_read_from_4(page + rec_pos + rec_offs_base(offsets)[9]),
						mach_read_from_4(page + rec_pos + rec_offs_base(offsets)[9]));
				if(space_id == mach_read_from_4(page + rec_pos + rec_offs_base(offsets)[9])){
					fprintf(stderr, "Error: Space id: %lu is already used in InnoDB dictionary for table %s\n",mach_read_from_4(page + rec_pos + rec_offs_base(offsets)[9]), buffer);
					exit(EXIT_FAILURE);
					}
			rec_pos = mach_read_from_2(page + rec_pos - 2);
			printf("Next record at offset: %lX\n", rec_pos);
			}
		}
	printf("Space id %lu is not used in any of the records in SYS_TABLES\n", space_id);
	// Update SPACE
	next_page_pos = root_page_pos;
	if((off_t)-1 == lseek(f, UNIV_PAGE_SIZE * root_page_pos, SEEK_SET)){
		fprintf(stderr, "Couldn't set file pointer to root page of ibdata\n");
		perror("update_sys_tables(): lseek()");
		exit(EXIT_FAILURE);
		}
	while(next_page_pos != FIL_NULL){
		if((off_t)-1 == lseek(f, UNIV_PAGE_SIZE * next_page_pos, SEEK_SET)){
			fprintf(stderr, "Couldn't set file pointer to root page of ibdata\n");
			perror("update_sys_indexes(): lseek()");
			exit(EXIT_FAILURE);
			}
		if(UNIV_PAGE_SIZE != read(f, page, UNIV_PAGE_SIZE)){
			fprintf(stderr, "Couldn't read %u bytes from ibdata file\n", UNIV_PAGE_SIZE);
			perror("update_sys_tables(): read()");
			exit(EXIT_FAILURE);
			}
		page_id = mach_read_from_4(page + FIL_PAGE_OFFSET);
		next_page_pos = mach_read_from_4(page + FIL_PAGE_NEXT);
		printf("Page_id: %lu, next page_id: %lu\n", page_id, next_page_pos);
		
		ulint rec_pos = PAGE_OLD_INFIMUM;
		while(rec_pos != PAGE_OLD_SUPREMUM){
			printf("Record position: %lX\n", rec_pos);
			ibrec_init_offsets_old(page, page + rec_pos, sys_tables, offsets);
			if (debug) {
				printf("Checking field lengths for a row (%s): ", sys_tables->name);
				printf("OFFSETS: ");
				int i;
				for(i = 0; i < rec_offs_n_fields(offsets); i++) {
					printf("%lu ", rec_offs_base(offsets)[i]);
					}
				bzero(buffer, UNIV_PAGE_SIZE);
				strncpy(buffer, page + rec_pos, rec_offs_base(offsets)[1]);
				printf("\nDb/table: %s\n", buffer);
				printf("Space id: %lu (0x%lX)\n", mach_read_from_4(page + rec_pos + rec_offs_base(offsets)[9]),
						mach_read_from_4(page + rec_pos + rec_offs_base(offsets)[9]));
				if(strncmp(buffer, dbtpair, sizeof(dbtpair)) == 0){
					unsigned int s;
					s = mach_write_to_4(space_id);
					table_id = mach_read_from_8(page + rec_pos + rec_offs_base(offsets)[3]);
					printf("Updating %s (table_id %llu) with id 0x%08X\n", buffer, table_id, s);
					memcpy(page + rec_pos + rec_offs_base(offsets)[9], &s, sizeof(s));
					if((off_t)-1 == lseek(f, -UNIV_PAGE_SIZE, SEEK_CUR)){
						fprintf(stderr, "Couldn't set file pointer to root page of ibdata\n");
						perror("update_sys_tables(): lseek()");
						exit(EXIT_FAILURE);
						}
					if(UNIV_PAGE_SIZE != write(f, page, UNIV_PAGE_SIZE)){
						fprintf(stderr, "Couldn't write %u bytes to ibdata file\n", UNIV_PAGE_SIZE);
						perror("update_sys_tables(): write()");
						exit(EXIT_FAILURE);
						}
					printf("SYS_TABLES is updated successfully\n");
					return table_id;
					}
				}
			rec_pos = mach_read_from_2(page + rec_pos - 2);
			printf("Next record at offset: %lX\n", rec_pos);
			}
		}
	return 0;
}

int update_sys_indexes(int f, ullint table_id, ulint space_id){
	const ulint root_page_pos = 11;
	page_t page[UNIV_PAGE_SIZE];
	ulint offsets[MAX_TABLE_FIELDS + 2];
	ulint next_page_pos = root_page_pos;
	ulint page_id = 0;
	table_def_t* sys_indexes;
	init_table_defs();
	sys_indexes = &(table_definitions[1]);

	printf("Setting SPACE=%lu in SYS_INDEXES for TABLE_ID = %llu\n", space_id, table_id);
	
	if((off_t)-1 == lseek(f, UNIV_PAGE_SIZE * root_page_pos, SEEK_SET)){
		fprintf(stderr, "Couldn't set file pointer to root page of ibdata\n");
		perror("update_sys_indexes(): lseek()");
		exit(EXIT_FAILURE);
		}
	while(next_page_pos != FIL_NULL){
		if((off_t)-1 == lseek(f, UNIV_PAGE_SIZE * next_page_pos, SEEK_SET)){
			fprintf(stderr, "Couldn't set file pointer to root page of ibdata\n");
			perror("update_sys_indexes(): lseek()");
			exit(EXIT_FAILURE);
			}
		if(UNIV_PAGE_SIZE != read(f, page, UNIV_PAGE_SIZE)){
			fprintf(stderr, "Couldn't read %u bytes from ibdata file\n", UNIV_PAGE_SIZE);
			perror("update_sys_indexes(): read()");
			exit(EXIT_FAILURE);
			}
		page_id = mach_read_from_4(page + FIL_PAGE_OFFSET);
		next_page_pos = mach_read_from_4(page + FIL_PAGE_NEXT);
		printf("Page_id: %lu, next page_id: %lu\n", page_id, next_page_pos);
		
		ulint rec_pos = PAGE_OLD_INFIMUM;
		while(rec_pos != PAGE_OLD_SUPREMUM){
			printf("Record position: %lX\n", rec_pos);
			ibrec_init_offsets_old(page, page + rec_pos, sys_indexes, offsets);
			if (debug) {
				printf("Checking field lengths for a row (%s): ", sys_indexes->name);
				printf("OFFSETS: ");
				int i;
				for(i = 0; i < rec_offs_n_fields(offsets); i++) {
					printf("%lu ", rec_offs_base(offsets)[i]);
					}
				printf("\nTABLE_ID: %llu\n",  mach_read_from_8(page + rec_pos));
				printf("SPACE: %lu\n", mach_read_from_4(page + rec_pos + rec_offs_base(offsets)[7]));
				if(mach_read_from_8(page + rec_pos) == table_id){
					unsigned int s;
					s = mach_write_to_4(space_id);
					printf("Updating SPACE(0x%08lX , 0x%08X) for TABLE_ID: %llu\n", space_id, s, table_id);
					printf("sizeof(s)=%lu\n", sizeof(s));
					memcpy(page + rec_pos + rec_offs_base(offsets)[7], &s, sizeof(s));
					}
				}
			rec_pos = mach_read_from_2(page + rec_pos - 2);
			printf("Next record at offset: %lX\n", rec_pos);
			}
		if((off_t)-1 == lseek(f, -UNIV_PAGE_SIZE, SEEK_CUR)){
			fprintf(stderr, "Couldn't set file pointer to root page of ibdata\n");
			perror("update_sys_indexes(): lseek()");
			exit(EXIT_FAILURE);
			}
		if(UNIV_PAGE_SIZE != write(f, page, UNIV_PAGE_SIZE)){
			fprintf(stderr, "Couldn't write %u bytes to ibdata file\n", UNIV_PAGE_SIZE);
			perror("update_sys_indexes(): write()");
			exit(EXIT_FAILURE);
			}
		printf("SYS_INDEXES is updated successfully\n");
		}
	return 0;

}

int main(int argc, char** argv)
{
	int ibd, ibdata;
	char page[UNIV_PAGE_SIZE];
	ulint space_id = 0;
	int c;
	char ibd_file[1024] = "";
	char ibdata_file[1024] = "";
	char db[1024] = "";
	char table[1024] = "";
	ullint table_id = 0;

	while(-1 != (c = getopt(argc, argv, "f:o:d:t:"))){
		switch(c){
			case 'f':
				strncpy(ibd_file, optarg, sizeof(ibd_file));
				break;
			case 'o':
				strncpy(ibdata_file, optarg, sizeof(ibdata_file));
				break;
			case 'd':
				strncpy(db, optarg, sizeof(db));
				break;
			case 't':
				strncpy(table, optarg, sizeof(table));
				break;
			default:
				usage(argv[0]);
			}
		}
	if(strncmp(ibd_file, "", sizeof(ibd_file)) == 0 ||
		strncmp(ibdata_file, "", sizeof(ibdata_file)) == 0 ||
		strncmp(db, "", sizeof(db)) == 0 ||
		strncmp(table, "", sizeof(table)) == 0 ){
		usage(argv[0]);
		exit(EXIT_FAILURE);
		}
	ibd = open(ibd_file, O_RDONLY);
	if(ibd == -1){
		fprintf(stderr, "Couldn't open %s\n", ibd_file);
		perror(argv[0]);
		exit(EXIT_FAILURE);
		}
	ibdata = open(ibdata_file, O_RDWR);
	if(ibdata == -1){
		fprintf(stderr, "Couldn't open %s\n", ibdata_file);
		perror(argv[0]);
		exit(EXIT_FAILURE);
		}
	if(UNIV_PAGE_SIZE != read(ibd, page, UNIV_PAGE_SIZE)){
		fprintf(stderr, "Couldn't read %u bytes from %s\n", UNIV_PAGE_SIZE, ibd_file);
		perror(argv[0]);
		exit(EXIT_FAILURE);
		}
	space_id = get_space_id(page);
	printf("%s belongs to space #%lu\n", ibd_file, space_id);
	table_id = update_sys_tables(ibdata, space_id, db, table);
	if(table_id == 0){
		fprintf(stderr, "TABLE_ID of `%s`.`%s` can not be 0\n", db, table);
		exit(EXIT_FAILURE);
		}
	update_sys_indexes(ibdata, table_id, space_id);
	exit(EXIT_SUCCESS);
}
