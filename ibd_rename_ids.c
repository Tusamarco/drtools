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
  InnoDB tablespace, table, and index id renamer.

  Written by Andrew Gaul <andrew@gaul.org>.

  Published with permission.
*/

/* TODO: batch IO */
/* TODO: requires innodb_file_per_table; confused tablespace and table id */

#include <inttypes.h>
#include <stdint.h>
#include <unistd.h>
#include "page0page.h"
#include "innochecksum.h"

static void usage(const char *progname)
{
  printf("InnoDB tablespace, table, and index id renamer.\n"
         "usage: %s [OPTIONS]... FILENAME\n"
         "\t-t ID\tset new tablespace id\n"
         "\t-I ID\tmap from this index id\n"
         "\t-i ID\tmap to this index id\n"
         "\t-v\tverbose output\n",
         progname);
}

static uint64_t dulint_to_uint64_t(dulint x)
{
  return (((uint64_t) x.high) << 32) | x.low;
}

static void write_page(FILE *file, fpos_t pos, const uchar *page)
{
  if (0 != fsetpos(file, &pos)) {
    perror("couldn't set a position to the start of the page");
    exit(1);
  }
  if (1 != fwrite(page, UNIV_PAGE_SIZE, 1, file)) {
    perror("couldn't update the page");
    exit(1);
  }
}

int main(int argc, char **argv)
{
  FILE *file = NULL;           /* input file */
  uchar *page = (uchar *)malloc(UNIV_PAGE_SIZE);  /* storage for pages read */
  int bytes;                   /* bytes read count */
  ulint page_no = 0;           /* current page number (0-based) */
  int32_t tablespace_id_to = -1;
  int64_t *index_id_from = malloc(0);
  size_t index_id_from_size = 0;
  int64_t *index_id_to = malloc(0);
  size_t index_id_to_size = 0;
  int64_t index_id;
  size_t i;
  int verbose = 0;
  int c;
  char *endptr;
  fpos_t pos;
  int retval = EXIT_FAILURE;

  /* parse arguments */
  while ((c = getopt(argc, argv, "hvt:I:i:")) != -1) {
    switch (c) {
    case 'v':
      verbose = 1;
      break;
    case 't':
      tablespace_id_to = strtol(optarg, &endptr, 10);
      if (*endptr != '\0') {
        fprintf(stderr, "invalid value %s\n", optarg);
        goto out;
      }
      break;
    case 'I':
      index_id = strtoll(optarg, &endptr, 10);
      if (*endptr != '\0') {
        fprintf(stderr, "invalid value %s\n", optarg);
        goto out;
      }
      index_id_from = realloc(index_id_from,
              sizeof(*index_id_from) * (index_id_from_size + 1));
      index_id_from[index_id_from_size++] = index_id;
      break;
    case 'i':
      index_id = strtoll(optarg, &endptr, 10);
      if (*endptr != '\0') {
        fprintf(stderr, "invalid value %s\n", optarg);
        goto out;
      }
      index_id_to = realloc(index_id_to,
              sizeof(*index_id_to) * (index_id_to_size + 1));
      index_id_to[index_id_to_size++] = index_id;
      break;
    case ':':
      fprintf(stderr, "option -%c requires an argument\n", optopt);
      goto out;
    case '?':
      fprintf(stderr, "unrecognized option: -%c\n", optopt);
      goto out;
    case 'h':
    default:
      usage(argv[0]);
      goto out;
    }
  }

  /* make sure we have the right arguments */
  if (optind >= argc) {
    usage(argv[0]);
    goto out;
  }

  if (index_id_from_size != index_id_to_size) {
    fprintf(stderr, "Mismatched -T and -t parameters.\n");
    goto out;
  }

  file = fopen(argv[optind], "r+");
  if (!file) {
    perror("error opening file");
    goto out;
  }

  while (!feof(file)) {
    fgetpos(file, &pos);
    bytes = fread(page, 1, UNIV_PAGE_SIZE, file);
    if (!bytes && feof(file)) break;
    if (bytes != UNIV_PAGE_SIZE) {
      fprintf(stderr,
              "bytes read (%d) does not match universal page size (%d)\n",
              bytes, UNIV_PAGE_SIZE);
      goto out;
    }

    uint16_t type = mach_read_from_2(page + FIL_PAGE_TYPE);
    uint32_t id = mach_read_from_4((uchar*)&tablespace_id_to);
    int modified = FALSE;
    if (type != FIL_PAGE_TYPE_ALLOCATED) {
      if (tablespace_id_to != -1) {
        if (verbose) {
          printf("changing tablespace id from %lu to %u at page %lu\n",
                 mach_read_from_4(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID),
                 tablespace_id_to, page_no);
        }
        memcpy(page + FIL_PAGE_ARCH_LOG_NO_OR_SPACE_ID, &id, sizeof(id));
        modified = TRUE;
      }
    }
    if (type == FIL_PAGE_TYPE_FSP_HDR) {
      if (tablespace_id_to != -1) {
        memcpy(page + FIL_PAGE_DATA + FSEG_HDR_SPACE, &id, sizeof(id));
        modified = TRUE;
      }
    } else if (type == FIL_PAGE_INDEX) {
      uint64_t on_disk_index_id = dulint_to_uint64_t(mach_read_from_8(
           page + FIL_PAGE_DATA + PAGE_INDEX_ID));
      for (i = 0; i < index_id_from_size; ++i) {
        if (on_disk_index_id == index_id_from[i]) {
          if (verbose) {
            printf("changing index id from %" PRIu64 " to %" PRIi64
                   " at page %lu\n", index_id_from[i], index_id_to[i], page_no);
          }
          uint64_t index_id_to_swapped = dulint_to_uint64_t(mach_read_from_8(
                (uchar*)&index_id_to[i]));
          memcpy(page + FIL_PAGE_DATA + PAGE_INDEX_ID, &index_id_to_swapped,
                 sizeof(index_id_to_swapped));
          modified = TRUE;
          break;
        }
      }

      if (tablespace_id_to != -1) {
        memcpy(page + FIL_PAGE_DATA + FSEG_HDR_SPACE + PAGE_BTR_SEG_LEAF, &id,
               sizeof(id));
        memcpy(page + FIL_PAGE_DATA + FSEG_HDR_SPACE + PAGE_BTR_SEG_TOP, &id,
               sizeof(id));
        modified = TRUE;
      }
    }

    if (modified) {
      /* recalculate new-style checksum */
      uint32_t new_checksum = buf_calc_page_new_checksum(page);
      new_checksum = mach_read_from_4((uchar *)&new_checksum);
      memcpy(page + FIL_PAGE_SPACE_OR_CHKSUM, &new_checksum,
             sizeof(new_checksum));

      /* recalculate old-style checksum */
      uint32_t old_checksum = buf_calc_page_old_checksum(page);
      old_checksum = mach_read_from_4((uchar *)&old_checksum);
      memcpy(page + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM, &old_checksum,
             sizeof(old_checksum));

      write_page(file, pos, page);
    }

    ++page_no;
  }

  retval = EXIT_SUCCESS;

out:
  free(index_id_from);
  free(index_id_to);
  free(page);
  if (file != NULL) {
    fclose(file);
  }
  return retval;
}
