/* Copyright (C) 2000-2005 MySQL AB & Innobase Oy

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA */

/*
  InnoDB page printer.

  Written by Andrew Gaul <andrew@gaul.org>.

  Published with permission.
*/

#include <inttypes.h>
#include <stdint.h>
#include "page0page.h"

#define COLUMN_NAME_FMT "%-32s"

static void print_page(uchar *p)
{
  int type = mach_read_from_2(p + FIL_PAGE_TYPE);
  if (type == FIL_PAGE_TYPE_ALLOCATED) {
    return;
  }

  printf(COLUMN_NAME_FMT " %ld\n", "FIL_PAGE_OFFSET",
      mach_read_from_4(p + FIL_PAGE_OFFSET));
  printf(COLUMN_NAME_FMT " 0x%08lX\n", "FIL_PAGE_SPACE_OR_CHKSUM",
      mach_read_from_4(p + FIL_PAGE_SPACE_OR_CHKSUM));
  printf(COLUMN_NAME_FMT " %ld\n", "FIL_PAGE_PREV",
      mach_read_from_4(p + FIL_PAGE_PREV));
  printf(COLUMN_NAME_FMT " %ld\n", "FIL_PAGE_NEXT",
      mach_read_from_4(p + FIL_PAGE_NEXT));
  printf(COLUMN_NAME_FMT " %ld\n", "FIL_PAGE_LSN",
      mach_read_from_4(p + FIL_PAGE_LSN));
  printf(COLUMN_NAME_FMT " %ld\n", "FIL_PAGE_TYPE",
      mach_read_from_2(p + FIL_PAGE_TYPE));
  dulint flush_lsn_tuple = mach_read_from_6(p + FIL_PAGE_FILE_FLUSH_LSN);
  uint64_t flush_lsn = (((uint64_t) flush_lsn_tuple.high) << 32) +
      flush_lsn_tuple.low;
  printf(COLUMN_NAME_FMT " %" PRIu64 "\n", "FIL_PAGE_FILE_FLUSH_LSN",
      flush_lsn);
  printf(COLUMN_NAME_FMT " %ld\n", "FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID",
      mach_read_from_4(p + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID));
  printf(COLUMN_NAME_FMT " 0x%08lX\n", "FIL_PAGE_END_LSN_OLD_CHKSUM",
      mach_read_from_4(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM));

  uchar *pd = p + FIL_PAGE_DATA;
  if (type == FIL_PAGE_TYPE_FSP_HDR) {
    printf(COLUMN_NAME_FMT " %ld\n", "FSEG_HDR_SPACE",
        mach_read_from_4(pd + FSEG_HDR_SPACE));
    printf(COLUMN_NAME_FMT " %ld\n", "FSEG_HDR_PAGE_NO",
        mach_read_from_4(pd + FSEG_HDR_PAGE_NO));
    printf(COLUMN_NAME_FMT " %ld\n", "FSEG_HDR_OFFSET",
        mach_read_from_4(pd + FSEG_HDR_OFFSET));
  } else if (type == FIL_PAGE_INDEX) {
    printf(COLUMN_NAME_FMT " 0x%lX\n", "PAGE_N_HEAP",
        mach_read_from_2(pd + PAGE_N_HEAP));
    printf(COLUMN_NAME_FMT " 0x%lX\n", "PAGE_FREE",
        mach_read_from_2(pd + PAGE_FREE));
    dulint index_id_tuple = mach_read_from_8(pd + PAGE_INDEX_ID);
    uint64_t index_id = (((uint64_t) index_id_tuple.high) << 32) +
        index_id_tuple.low;
    printf(COLUMN_NAME_FMT " %" PRIu64 "\n", "PAGE_INDEX_ID",
        index_id);
    printf(COLUMN_NAME_FMT " %ld\n", "PAGE_BTR_SEG_LEAF",
        mach_read_from_4(pd + PAGE_BTR_SEG_LEAF + FSEG_HDR_SPACE));
    printf(COLUMN_NAME_FMT " %ld\n", "PAGE_BTR_SEG_TOP",
        mach_read_from_4(pd + PAGE_BTR_SEG_TOP + FSEG_HDR_SPACE));
#if 0
    int i;
    for (i = 0; i < 80; i += 4) {
      if (i == PAGE_BTR_SEG_LEAF ||
          i == PAGE_N_HEAP ||
          i == PAGE_INDEX_ID ||
          i == PAGE_INDEX_ID + 4) {
        continue;
      }
      char column_name[256];
      snprintf(column_name, sizeof(column_name), "FIL_PAGE_DATA + %2d", i);
      printf(COLUMN_NAME_FMT " %ld\n", column_name,
             mach_read_from_4(p + FIL_PAGE_DATA + i));
    }
#endif
  }
  printf("\n");
}

int main(int argc, char **argv)
{
  FILE *f;                     /* our input file */
  uchar *p;                    /* storage of pages read */
  int bytes;                   /* bytes read count */
  ulint ct;                    /* current page number (0 based) */
  ulint start_page= 0, end_page= 0, use_end_page= 0; /* for starting and ending at certain pages */
  off_t offset= 0;
  int c;
  int fd;
  fpos_t pos;

  /* remove arguments */
  while ((c= getopt(argc, argv, "s:e:p:")) != -1)
  {
    switch (c)
    {
    case 's':
      start_page= atoi(optarg);
      break;
    case 'e':
      end_page= atoi(optarg);
      use_end_page= 1;
      break;
    case 'p':
      start_page= atoi(optarg);
      end_page= atoi(optarg);
      use_end_page= 1;
      break;
    case ':':
      fprintf(stderr, "option -%c requires an argument\n", optopt);
      return 1;
      break;
    case '?':
      fprintf(stderr, "unrecognized option: -%c\n", optopt);
      return 1;
      break;
    }
  }

  /* make sure we have the right arguments */
  if (optind >= argc)
  {
    printf("InnoDB page printer.\n");
    printf("usage: [-s <start page>] [-e <end page>] [-p <page>] %s <filename>\n", argv[0]);
    printf("\t-s n\tstart on this page number (0 based)\n");
    printf("\t-e n\tend at this page number (0 based)\n");
    printf("\t-p n\tcheck only this page (0 based)\n");
    return 1;
  }

  f = fopen(argv[optind], "r");
  if (!f)
  {
    perror("error opening file");
    return 1;
  }

  /* seek to the necessary position */
  if (start_page)
  {
    fd= fileno(f);
    if (!fd)
    {
      perror("unable to obtain file descriptor number");
      return 1;
    }

    offset= (off_t)start_page * (off_t)UNIV_PAGE_SIZE;

    if (lseek(fd, offset, SEEK_SET) != offset)
    {
      perror("unable to seek to necessary offset");
      return 1;
    }
  }

  /* allocate buffer for reading (so we don't realloc every time) */
  p= (uchar *)malloc(UNIV_PAGE_SIZE);

  ct= start_page;
  while (!feof(f))
  {
    fgetpos(f, &pos);
    bytes= fread(p, 1, UNIV_PAGE_SIZE, f);
    if (!bytes && feof(f)) return 0;
    if (bytes != UNIV_PAGE_SIZE)
    {
      fprintf(stderr, "bytes read (%d) doesn't match universal page size (%d)\n", bytes, UNIV_PAGE_SIZE);
      return 1;
    }

    print_page(p);

    /* end if this was the last page we were supposed to check */
    if (use_end_page && (ct >= end_page))
      return 0;

    ct++;
  }
  return 0;
}
