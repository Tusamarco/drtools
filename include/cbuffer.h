#include <stdio.h>
#include <string.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdint.h>

int cb_debug = 0;
typedef unsigned char byte;

typedef struct { byte value; } ElemType;
typedef struct {
    unsigned long int 	size;   /* maximum number of elements           */
    unsigned long int	start;  /* index of oldest element              */
    unsigned long int	end;    /* index at which to write new element  */
    ElemType		*elems;  /* vector of elements                   */
} CircularBuffer;

void cbInit(CircularBuffer *cb, int size) {
    cb->size  = size + 1; /* include empty elem */
    cb->start = 0;
    cb->end   = 0;
    cb->elems = (ElemType *)calloc(cb->size, sizeof(ElemType));
    if(cb_debug) fprintf(stderr, "cBuffer: %lu bytes are initialized\n", cb->size);
}

void cbFree(CircularBuffer *cb) {
    free(cb->elems); /* OK if null */ 
}
 
int cbIsFull(CircularBuffer *cb) {
	int result =  (cb->end + 1) % cb->size == cb->start;
	if(cb_debug) fprintf(stderr, "cBuffer: Buffer is full = %s\n", result ? "True" : "False");
	return result; 
}
 
int cbIsEmpty(CircularBuffer *cb) {
	if(cb_debug) fprintf(stderr, "cBuffer: Buffer is empty = %s\n", cb->end == cb->start ? "True" : "False");
	return cb->end == cb->start; 
}

/* Write an element, overwriting oldest element if buffer is full. App can
   choose to avoid the overwrite by checking cbIsFull(). */
void cbWrite(CircularBuffer *cb, ElemType *elem) {
	cb->elems[cb->end] = *elem;
	if(cb_debug) fprintf(stderr, "cBuffer: %02X is written at pos %lu\n", elem->value, cb->end);
	cb->end = (cb->end + 1) % cb->size;
	if (cb->end == cb->start)
		cb->start = (cb->start + 1) % cb->size; /* full, overwrite */
	if(cb_debug) fprintf(stderr, "cBuffer: New size [%lu..%lu)\n", cb->start, cb->end);
}
 
/* Read oldest element. App must ensure !cbIsEmpty() first. */
void cbRead(CircularBuffer *cb, ElemType *elem) {
	*elem = cb->elems[cb->start];
	cb->start = (cb->start + 1) % cb->size;
	if(cb_debug) fprintf(stderr, "cBuffer: %02X is read\n", elem->value);
	if(cb_debug) fprintf(stderr, "cBuffer: New size [%lu..%lu)\n", cb->start, cb->end);
}

void cbSetPos(CircularBuffer *cb, unsigned long int pos) {
	cb->start = pos % cb->size;
	if(cb_debug) fprintf(stderr, "cBuffer: position is set to %lu \n", cb->start);
	if(cb_debug) fprintf(stderr, "cBuffer: New size [%lu..%lu)\n", cb->start, cb->end);
}
unsigned long int cbGetPos(CircularBuffer *cb) {
	if(cb_debug) fprintf(stderr, "cBuffer: Current size [%lu..%lu)\n", cb->start, cb->end);
	return cb->start ;
}

void cbFill(CircularBuffer *cb, FILE*f){
	unsigned int disk_buffer_size = cb->size/2;
	byte* disk_buffer = (byte*)malloc(disk_buffer_size);
	unsigned long int i, n;
	ElemType elem;

	if(feof(f)) return;
	n = fread(disk_buffer, 1, disk_buffer_size, f);
	if(errno != 0){
		perror("fread()");
		exit(EXIT_FAILURE);
		}
	for(i = 0; i < n; i++){
		elem.value = disk_buffer[i];
		cbWrite(cb, &elem);
		}
	if(cb_debug) fprintf(stderr, "%lu bytes are written to the buffer\n", n);
	free(disk_buffer);
}

