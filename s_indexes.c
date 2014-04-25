#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include "cbuffer.h"
#include <inttypes.h>

int debug = 0;

void usage(char* p){
        fprintf(stderr, "Usage: \n\
        %s /path/to/ibdata table_id\n", p);
}

int main(int argc, char** argv)
{
	unsigned long int pattern_size = 12;
	byte pattern[pattern_size];
	unsigned long int cb_buffer_size = 10*1024*1024;
	unsigned long int i = 0, j, pos;

	uint64_t table_id = 0;
	uint64_t index_id = 0;

        if(argc != 3){
                usage(argv[0]);
                exit(EXIT_FAILURE);
                }
	FILE *f = fopen(argv[1], "r");
	if(f == NULL){
		fprintf(stderr, "Can't open file '%s'\n", argv[1]);
		perror("fopen()");
		usage(argv[0]);
		exit(EXIT_FAILURE);
		}
	pattern[0] = 0x00;
	pattern[1] = 0x08;
	table_id = (uint64_t)strtoull(argv[2], NULL, 10);
	for(i = 0; i < 8; i++){
		byte r = (table_id >> 8*(7 - i) );
		pattern[2 + i] = r;
		if(debug) fprintf(stderr, "pattern[%lu] = %02X\n", 2 + i, pattern[2 + i]);
		}
	pattern[10] = 0x01;
	pattern[11] = 0x08;
	if(debug){
		fprintf(stderr, "pattern size: %lu\n", pattern_size);
		fprintf(stderr, "pattern\nascii: ");
		for(i = 0; i < pattern_size; i++){
			fprintf(stderr, "%c", pattern[i]);
			}
		fprintf(stderr, "\n");
		fprintf(stderr, "hex: ");
		for(i = 0; i < pattern_size; i++){
			fprintf(stderr, "%02X ", pattern[i]);
			}
		fprintf(stderr, "\n");
		}

	CircularBuffer cb;
	ElemType elem;
	elem.value = 0;
	cbInit(&cb, cb_buffer_size);

	int pattern_found = 0;
	while(!feof(f)){
		pos = cbGetPos(&cb);
		pattern_found = 1;
		for(j = 0; j < pattern_size; j++){
			if(cbIsEmpty(&cb)){
				cbFill(&cb, f);
				}
			if(!cbIsEmpty(&cb)){
				cbRead(&cb, &elem);
				if(pattern[j] != elem.value){
					pattern_found = 0;
					}
				}
			}
		if(pattern_found){
			if(debug) fprintf(stderr, "Pattern found\n");
			// pattern matches if we reached here
			// read next 8 bytes and print
			index_id = 0;
			for(j = 0; j < 8; j++){
				if(cbIsEmpty(&cb)){
                                	cbFill(&cb, f);
                                	}
				if(!cbIsEmpty(&cb)){
					cbRead(&cb, &elem);
					index_id = index_id | (elem.value << (7-j));
					}
				}
			printf("%" PRIu32 "-%" PRIu32 "\n", (uint32_t)(index_id >> 32), (uint32_t)(index_id & 0x00000000FFFFFFFF));
			}
		else{
			if(debug) fprintf(stderr, "Pattern not found\n");
			cbSetPos(&cb, pos+1);
			}
		}
	return EXIT_SUCCESS;
}
