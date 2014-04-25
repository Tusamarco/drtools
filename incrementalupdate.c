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
  InnoDB offline file checksum utility.  85% of the code in this file
  was taken wholesale from the InnoDB codebase.

  The final 15% was originally written by Mark Smith of Danga
  Interactive, Inc. <junior@danga.com>

  Published with a permission.
*/

/*
  This was once a well written tool for InnoDB checkums, I took it and spoiled it
  because I am a programing florist, cut and arrange here and there and suddenly a 
  tool for doing incremental page based updates between tablespace snapshots.
*/

/* needed to have access to 64 bit file functions */
#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE

#define _XOPEN_SOURCE 500 /* needed to include getopt.h on some platforms. */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* all of these ripped from InnoDB code from MySQL 4.0.22 */
#define UT_HASH_RANDOM_MASK     1463735687
#define UT_HASH_RANDOM_MASK2    1653893711
#define FIL_PAGE_LSN          16 
#define FIL_PAGE_FILE_FLUSH_LSN 26
#define FIL_PAGE_OFFSET     4
#define FIL_PAGE_DATA       38
#define FIL_PAGE_END_LSN_OLD_CHKSUM 8
#define FIL_PAGE_SPACE_OR_CHKSUM 0
#define UNIV_PAGE_SIZE          (2 * 8192)

/* command line argument to do page checks (that's it) */
/* another argument to specify page ranges... seek to right spot and go from there */

typedef unsigned long int ulint;
typedef unsigned char byte;

/* innodb function in name; modified slightly to not have the ASM version (lots of #ifs that didn't apply) */
ulint mach_read_from_4(byte *b)
{
  return( ((ulint)(b[0]) << 24)
          + ((ulint)(b[1]) << 16)
          + ((ulint)(b[2]) << 8)
          + (ulint)(b[3])
          );
}

ulint
ut_fold_ulint_pair(
/*===============*/
            /* out: folded value */
    ulint   n1, /* in: ulint */
    ulint   n2) /* in: ulint */
{
    return(((((n1 ^ n2 ^ UT_HASH_RANDOM_MASK2) << 8) + n1)
                        ^ UT_HASH_RANDOM_MASK) + n2);
}

ulint
ut_fold_binary(
/*===========*/
            /* out: folded value */
    byte*   str,    /* in: string of bytes */
    ulint   len)    /* in: length */
{
    ulint   i;
    ulint   fold= 0;

    for (i= 0; i < len; i++)
    {
      fold= ut_fold_ulint_pair(fold, (ulint)(*str));

      str++;
    }

    return(fold);
}

int main(int argc, char **argv)
{
  FILE *f;                     /* our input file */
  FILE *b;                     /* our output file */
  byte *p;                     /* storage of pages read input file*/
  byte *op;                    /* storage of pages read output file*/
  int bytes;                   /* bytes read count */
  ulint ct;                    /* current page number (0 based) */
  int now;                     /* current time */
  int lastt;                   /* last time */
  ulint oldcsum, noldcsum, oldcsumfield, noldcsumfield, csum, ncsum, csumfield, ncsumfield, logseq, nlogseq, logseqfield, nlogseqfield; /* ulints for checksum storage */
  struct stat st;              /* for stat, if you couldn't guess */
  unsigned long long int size; /* size of file (has to be 64 bits) */
  ulint pages;                 /* number of pages in file */
  ulint start_page= 0, end_page= 0, use_end_page= 0; /* for starting and ending at certain pages */
  off_t offset= 0;
  off_t boffset= 0;
  int just_count= 0;          /* if true, just print page count */
  int verbose= 0;
  int debug= 0;
  int c;
  int fd;
  int fn;
  int posstat;
  off_t pos;


  /* make sure we have the right arguments */
  if (argc != 3)
  {
    printf("InnoDB offline file checksum utility.\n");
    printf("usage: %s  <backupfilename> <filename>\n", argv[0]);
    return 1;
  }

  /* stat the file to get size and page count */
  if (stat(argv[2], &st))
  {
    perror("error statting file");
    return 1;
  }
  size= st.st_size;
  pages= size / UNIV_PAGE_SIZE;
  if (just_count)
  {
    printf("%lu\n", pages);
    return 0;
  }
  else if (verbose)
  {
    printf("file %s= %llu bytes (%lu pages)...\n", argv[1], size, pages);
    printf("checking pages in range %lu to %lu\n", start_page, use_end_page ? end_page : (pages - 1));
  }


  /* open the file for reading */
  f= fopen(argv[2], "rb");
  if (!f)
  {
    perror("error opening ibdata file");
    return 1;
  }

  /* open the file for readwrite */
  b= fopen(argv[1], "rb+");
  if (!b)
  {
    perror("error opening backup file");
    return 1;
  }

printf("Comparing files %s and %s\n", argv[1], argv[2]);
  /* seek to the necessary position */
  if (start_page)
  {
    fd= fileno(f);
    fn= fileno(b);
    if (!fd || !fn)
    {
      perror("unable to obtain file descriptor number");
      return 1;
    }


    offset= (off_t)start_page * (off_t)UNIV_PAGE_SIZE;
    boffset= (off_t)start_page * (off_t)UNIV_PAGE_SIZE;


    if (lseek(fd, offset, SEEK_SET) != offset)
    {
      perror("unable to seek to necessary offset");
      return 1;
    }

    if (lseek(fn, boffset, SEEK_SET) != boffset)
    {
      perror("unable to seek to necessary offset");
      return 1;
    }

  }

  /* allocate buffer for reading (so we don't realloc every time) */
  p= (byte *)malloc(UNIV_PAGE_SIZE);
  op= (byte *)malloc(UNIV_PAGE_SIZE);

  /* main checksumming loop */
  ct= start_page;
  lastt= 0;
  while (!feof(f))
  {

    bytes= fread(p, 1, UNIV_PAGE_SIZE, f);

    if (!bytes && feof(f)) return 0;
    if (bytes != UNIV_PAGE_SIZE)
    {
      fprintf(stderr, "bytes read (%d) doesn't match universal page size (%d)\n", bytes, UNIV_PAGE_SIZE);
      return 1;
    }

    /* get position before reading page from backup file */
    pos = ftell(b);

    /* read page from backup file */
    bytes= fread(op, 1, UNIV_PAGE_SIZE, b);

    /* check the "stored log sequence numbers" */
    logseq= mach_read_from_4(p + FIL_PAGE_LSN + 4);
    nlogseq= mach_read_from_4(op + FIL_PAGE_LSN + 4);
    logseqfield= mach_read_from_4(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM + 4);
    nlogseqfield= mach_read_from_4(op + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM + 4);

    /* Make sure our page LSNs make sense */
    if (logseq != logseqfield || nlogseq != nlogseqfield)
    {
      fprintf(stderr, "page %lu invalid (fails log sequence number check)\n", ct);
      return 1;
    }

    /* get old method checksums */
    oldcsumfield= mach_read_from_4(p + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM);
    noldcsumfield= mach_read_from_4(op + UNIV_PAGE_SIZE - FIL_PAGE_END_LSN_OLD_CHKSUM);


    /* get new method checksums */
    csumfield= mach_read_from_4(p + FIL_PAGE_SPACE_OR_CHKSUM);
    ncsumfield= mach_read_from_4(op + FIL_PAGE_SPACE_OR_CHKSUM);
	/* If any of the LSNs or checksums dont make assume we need the newer page */
        if(logseq != nlogseq || oldcsumfield != noldcsumfield || csumfield != ncsumfield) {
                printf("%lu:%lu:%lu:%lu:%lu\t Is different from \t%lu:%lu:%lu:%lu:%lu\n", ct, logseq, logseqfield,oldcsumfield,csumfield,ct, nlogseq, nlogseqfield,noldcsumfield,ncsumfield);
                printf("Page start position is %lu\n", pos);
		/* seek back to the start of this page */
                fseek(b, -UNIV_PAGE_SIZE, SEEK_CUR);
                printf("Successfully rewound position to %lu\n", pos);
		/* write the page from the newer file to the backup */
                fwrite(p, 1, UNIV_PAGE_SIZE, b);
		/* we should now be back at the end of the page */
                pos = ftell(b);
                printf("Wrote new page and back at %lu\n", pos);
exit;
        }
    /* do counter increase and progress printing */
    ct++;
  }
  return 0;
}
