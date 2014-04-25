#ifndef __ibdconnect_h
#define __ibdconnect_h


int debug = 1;

#define UNIV_INLINE inline
#define ibool                 ulint
#define TRUE 1
#define FALSE 0

#define FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID  34
#define FIL_PAGE_OFFSET         4
#define FIL_PAGE_PREV           8
#define FIL_PAGE_NEXT           12

#define UNIV_PAGE_SIZE (16*1024)
#define FIL_NULL (0xFFFFFFFF)

#define REC_N_OLD_EXTRA_BYTES 6
#define FIL_PAGE_DATA         38
#define FSEG_PAGE_DATA              FIL_PAGE_DATA
#define PAGE_HEADER     FSEG_PAGE_DATA
#define FSEG_HEADER_SIZE    10
#define PAGE_DATA       (PAGE_HEADER + 36 + 2 * FSEG_HEADER_SIZE)
#define PAGE_OLD_INFIMUM        (PAGE_DATA + 1 + REC_N_OLD_EXTRA_BYTES)
#define PAGE_OLD_SUPREMUM       (PAGE_DATA + 2 + 2 * REC_N_OLD_EXTRA_BYTES + 8)
#define REC_OLD_SHORT             3       /* This is single byte bit-field */
#define REC_OFFS_HEADER_SIZE   2
#define REC_OLD_SHORT_MASK        0x1UL
#define REC_OLD_SHORT_SHIFT       0
#define REC_1BYTE_SQL_NULL_MASK   0x80UL
#define REC_OFFS_SQL_NULL ((ulint) 1 << 31)
#define REC_2BYTE_SQL_NULL_MASK   0x8000UL
#define REC_2BYTE_EXTERN_MASK     0x4000UL
#define REC_OFFS_EXTERNAL ((ulint) 1 << 30)

#define rec_offs_base(offsets) (offsets + REC_OFFS_HEADER_SIZE)

typedef unsigned char byte;
typedef byte page_t;
typedef byte rec_t;
typedef unsigned long int ulint;
typedef unsigned long long int ullint;
typedef unsigned char uchar;

#define MAX_TABLE_FIELDS 500
#define MAX_ENUM_VALUES 100

// Field limits type
typedef struct field_limits {
	// In opposite to field.can_be_null, this field sets
	// limit from the data point of view
	ibool can_be_null;

	// min and max values for FT_INT fields
	long long int int_min_val;
	long long int int_max_val;

	// min and max values for FT_UNT fields
	unsigned long long int uint_min_val;
	unsigned long long int uint_max_val;

	// min and max string length
	long long int char_min_len;
	long long int char_max_len;

	// Should data be forced down to some ASCII sub-set or not
	ibool char_ascii_only;
	ibool char_digits_only;

	char* char_regex;

	// Dates validation
	ibool date_validation;

	// Enum values
	char *enum_values[MAX_ENUM_VALUES];
	uint enum_values_count;
} field_limits_t;

// Table definition types
typedef enum field_type {
	FT_NONE,		// dummy type for stop records
	FT_INTERNAL,		// supported
	FT_CHAR,		// supported (w/o externals)
	FT_INT,			// supported
	FT_UINT,		// supported
	FT_FLOAT,		// supported
	FT_DOUBLE,		// supported
	FT_DATE,		// supported
	FT_TIME,		// supported
	FT_DATETIME,		// supported
	FT_ENUM,		// supported
	FT_SET,
	FT_BLOB,		// supported
	FT_TEXT,		// supported (w/o externals)
	FT_BIT,
	FT_DECIMAL,		// supported
	FT_TIMESTAMP		// supported
} field_type_t;

typedef struct field_def {
	char *name;
	field_type_t type;
	unsigned int min_length;
	unsigned int max_length;

	ibool can_be_null;
	int fixed_length;

    	// For DECIMAL numbers only
	int decimal_precision;
	int decimal_digits;

	ibool has_limits;
	ibool char_rstrip_spaces;
	field_limits_t limits;
} field_def_t;

typedef struct table_def {
	char *name;
	field_def_t fields[MAX_TABLE_FIELDS];
	int fields_count;
	int data_min_size;
	long long int data_max_size;
	int n_nullable;
	int min_rec_header_len;
} table_def_t;

#include "sys_defs.h"

ulint mach_read_from_4(uchar *b)
{
	return( ((ulint)(b[0]) << 24)
		+ ((ulint)(b[1]) << 16)
		+ ((ulint)(b[2]) << 8)
		+ (ulint)(b[3])
		);
}

/************************************************************
The following function is used to fetch data from 8 consecutive
bytes. The most significant byte is at the lowest address. */
UNIV_INLINE
ullint
mach_read_from_8(
/*=============*/
			/* out: dulint integer */
	byte*   b)      /* in: pointer to 8 bytes */
{
	ulint	high;
	ulint	low;

	high = mach_read_from_4(b);
	low = mach_read_from_4(b + 4);
	return (ullint)(high << 31) | (ullint)(low);
}
ulint mach_read_from_2(uchar *b)
{
	return( ((ulint)(b[0]) << 8)
		+ (ulint)(b[1])
		);
}

ulint mach_read_from_1(uchar *b)
{
	return( ((ulint)(b[0])) );
}

UNIV_INLINE
ulint
mach_write_to_4(
	ulint   n)      /* in: ulint integer to be stored */
{
	ulint   b = 0;
	b |= ((n << 24) & 0xFF000000);
	b |= ((n << 8) & 0x00FF0000);
	b |= ((n >> 8) & 0x0000FF00);
	b |= ((n >> 24) & 0x000000FF);
	return b;
}


/**********************************************************
Gets a bit field from within 1 byte. */
UNIV_INLINE
ulint
rec_get_bit_field_1(
/*================*/
	rec_t*	rec,	/* in: pointer to record origin */
	ulint	offs,	/* in: offset from the origin down */
	ulint	mask,	/* in: mask used to filter bits */
	ulint	shift)	/* in: shift right applied after masking */
{
	return((mach_read_from_1(rec - offs) & mask) >> shift);
}

/**********************************************************
The following function sets the number of fields in offsets. */
UNIV_INLINE
void
rec_offs_set_n_fields(
/*==================*/
	ulint*	offsets,	/* in: array returned by rec_get_offsets() */
	ulint	n_fields)	/* in: number of fields */
{
	offsets[1] = n_fields;
}

/**********************************************************
The following function is used to test whether the data offsets in the record
are stored in one-byte or two-byte format. */
UNIV_INLINE
ibool
rec_get_1byte_offs_flag(
/*====================*/
			/* out: TRUE if 1-byte form */
	rec_t*	rec)	/* in: physical record */
{
#if TRUE != 1
#error "TRUE != 1"
#endif

	return(rec_get_bit_field_1(rec, REC_OLD_SHORT, REC_OLD_SHORT_MASK,
							REC_OLD_SHORT_SHIFT));
}

/************************************************************** 
The following function returns the number of fields in a record. */
UNIV_INLINE
ulint
rec_offs_n_fields(
/*===============*/
				/* out: number of fields */
	const ulint*	offsets)/* in: array returned by rec_get_offsets() */
{
	ulint	n_fields;
	n_fields = offsets[1];
	return(n_fields);
}

/**********************************************************
Returns the offset of nth field end if the record is stored in the 1-byte
offsets form. If the field is SQL null, the flag is ORed in the returned
value. */
UNIV_INLINE
ulint
rec_1_get_field_end_info(
/*=====================*/
 			/* out: offset of the start of the field, SQL null
 			flag ORed */
 	rec_t*	rec, 	/* in: record */
 	ulint	n)	/* in: field index */
{
	return(mach_read_from_1(rec - (REC_N_OLD_EXTRA_BYTES + n + 1)));
}

/**********************************************************
Returns the offset of nth field end if the record is stored in the 2-byte
offsets form. If the field is SQL null, the flag is ORed in the returned
value. */
UNIV_INLINE
ulint
rec_2_get_field_end_info(
/*=====================*/
 			/* out: offset of the start of the field, SQL null
 			flag and extern storage flag ORed */
 	rec_t*	rec, 	/* in: record */
 	ulint	n)	/* in: field index */
{
	return(mach_read_from_2(rec - (REC_N_OLD_EXTRA_BYTES + 2 * n + 2)));
}
/*******************************************************************/
void init_table_defs() {
	int i, j;
	int table_definitions_cnt;

	if (debug) printf("Initializing table definitions...\n");
	table_definitions_cnt = sizeof(table_definitions) / sizeof(table_def_t);
    
	for (i = 0; i < table_definitions_cnt; i++) {
		table_def_t *table = &(table_definitions[i]);
		if (debug) printf("Processing table: %s\n", table->name);
		
		table->n_nullable = 0;
		table->min_rec_header_len = 0;
		table->data_min_size = 0;
		table->data_max_size = 0;
		
		for(j = 0; j < MAX_TABLE_FIELDS; j++) {
			if (table->fields[j].type == FT_NONE) {
				table->fields_count = j;
				break;
			}

			if (table->fields[j].can_be_null) {
				table->n_nullable++;
			} else {
    			table->data_min_size += table->fields[j].min_length + table->fields[j].fixed_length;
				int size = (table->fields[j].fixed_length ? table->fields[j].fixed_length : table->fields[j].max_length);
				table->min_rec_header_len += (size > 255 ? 2 : 1);
			}

			table->data_max_size += table->fields[j].max_length + table->fields[j].fixed_length;
		}
		
		table->min_rec_header_len += (table->n_nullable + 7) / 8;
		
		if (debug) {
			printf(" - total fields: %i\n", table->fields_count);
			printf(" - nullable fields: %i\n", table->n_nullable);
			printf(" - minimum header size: %i\n", table->min_rec_header_len);
			printf(" - minimum rec size: %i\n", table->data_min_size);
			printf(" - maximum rec size: %lli\n", table->data_max_size);
			printf("\n");
		}
	}
}

#endif
