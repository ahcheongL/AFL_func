/*
   american fuzzy lop - sample argv fuzzing wrapper
   ------------------------------------------------

   Written by Michal Zalewski <lcamtuf@google.com>

   Copyright 2015 Google Inc. All rights reserved.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at:

     http://www.apache.org/licenses/LICENSE-2.0

   This file shows a simple way to fuzz command-line parameters with stock
   afl-fuzz. To use, add:

   #include "/path/to/argv-fuzz-inl.h"

   ...to the file containing main(), ideally placing it after all the 
   standard includes. Next, put AFL_INIT_ARGV(); near the very beginning of
   main().

   This will cause the program to read NUL-delimited input from stdin and
   put it in argv[]. Two subsequent NULs terminate the array. Empty
   params are encoded as a lone 0x02. Lone 0x02 can't be generated, but
   that shouldn't matter in real life.

   If you would like to always preserve argv[0], use this instead:
   AFL_INIT_SET0("prog_name");

*/

#ifndef _HAVE_ARGV_FUZZ_INL
#define _HAVE_ARGV_FUZZ_INL

#include <unistd.h>

#define AFL_INIT_ARGV() do { argv = afl_init_argv(&argc); } while (0)

#define AFL_INIT_SET0(_p) do { \
    argv = afl_init_argv(&argc); \
    argv[0] = (_p); \
    if (!argc) argc = 1; \
  } while (0)

#define MAX_CMDLINE_LEN 100000
#define MAX_CMDLINE_PAR 1000


#define AFL_INIT_SIR() do { \
		char in_buf_afl [100000]; \
		if (read(0, in_buf_afl, 100000 - 2) < 0); \
		afl_init_argv_SIR(&argc, in_buf_afl, argv); \
		if (!argc) argc = 1; \
	} while (0)

#define AFL_READ() do { \
		char in_buf_afl [100000];\
		afl_read_testcase(&argc, in_buf_afl, argv); \
		if (!argc) argc = 1; \
} while (0)

static char** afl_init_argv(int* argc) {

  static char  in_buf[MAX_CMDLINE_LEN];
  static char* ret[MAX_CMDLINE_PAR];

  char* ptr = in_buf;
  //int   rc  = 0;
  int   rc  = 1;

  if (read(0, in_buf, MAX_CMDLINE_LEN - 2) < 0);

  while (*ptr) {

    ret[rc] = ptr;
    if (ret[rc][0] == 0x02 && !ret[rc][1]) ret[rc]++;
    rc++;

    while (*ptr) ptr++;
    ptr++;

  }

  *argc = rc;

  return ret;

}

static void afl_read_testcase(int* argc, char * in_buf, char ** argv){
	FILE * f;
	char * fname = argv[1];
	f = fopen(fname, "r");
	if (f == NULL) {printf("Can't read the test case : %s\n", fname); exit(1);}
	char * tmp;
	tmp = fgets(in_buf, 100000, f);
	char * ptr = in_buf;
	int rc = 0;
	char * tmpptr;
	char input_read = 0;

	while (*ptr) {
    if (input_read && *ptr != ']'){
      argv[rc] = ptr;
      rc++;
      tmpptr = ptr;
			//remove last ']'
      while(*tmpptr != ']' && *tmpptr != '\0' && *tmpptr != ' ') tmpptr++;
      if (*tmpptr == ']') {
        *tmpptr = '\0';
        break;
      }
    }
    if (*ptr == '-' && *(ptr+1) == 'P'){
			//input arguments start
      input_read++;
      rc++;
      while( *ptr != '[' && *ptr != '\0') ptr++;
      ptr++;
      while (*ptr == ' ') ptr++;
      continue;
    }
    while (*ptr != ' ' && *ptr != '\0') ptr++;
		if (*ptr == '\0'){
			break;
		}
    *ptr = '\0';
    ptr++;
    while (*ptr == ' ') ptr++;
  }
  *argc = rc;

	/*
	while(*ptr){
		argv[rc] = ptr;
		rc ++;
		while(*ptr != ' ' && *ptr != '\0') ptr++;
		*ptr = '\0';
		ptr ++;
		while(*ptr == ' ') ptr++;
	}
	*argc = rc;
	*/
}

static void afl_init_argv_SIR(int* argc, char *in_buf, char ** argv) {
  char* ptr = in_buf;
  char *tmpptr;
  int   rc  = 0;
  char input_read = 0;
	
	// tmp reading from file
	//FILE * f;
	//char * fname = argv[1];
	//f = fopen(fname,"r");
	//char * tmp = fgets(in_buf, sizeof(in_buf), f);

  while (*ptr) {
    if (input_read && *ptr != ']'){
      argv[rc] = ptr;
      rc++;
      tmpptr = ptr;
			//remove last ']'
      while(*tmpptr != ']' && *tmpptr != '\0' && *tmpptr != ' ') tmpptr++;
      if (*tmpptr == ']') {
        *tmpptr = '\0';
        break;
      }
    }
    if (*ptr == '-' && *(ptr+1) == 'P'){
			//input arguments start
      input_read++;
      rc++;
      while( *ptr != '[' && *ptr != '\0') ptr++;
      ptr++;
      while (*ptr == ' ') ptr++;
      continue;
    }
    while (*ptr != ' ' && *ptr != '\0') ptr++;
    *ptr = '\0';
    ptr++;
    while (*ptr == ' ') ptr++;
  }
  *argc = rc;
}


#undef MAX_CMDLINE_LEN
#undef MAX_CMDLINE_PAR

#endif /* !_HAVE_ARGV_FUZZ_INL */
