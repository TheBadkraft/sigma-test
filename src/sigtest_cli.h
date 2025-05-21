/*	src/sigtest_cli.h
   Header for the sigma test CLI component

   David Boarman
   2025-05-11
*/
#ifndef SIGTEST_CLI_H
#define SIGTEST_CLI_H

#include "sigtest.h"
#include <stdio.h>

// CLI specific declarations
void cleanup_test_runner(void);
void fwritef(FILE *, const char *, ...);

// Initialize CLI hooks
void init_cli_hooks(SigtestHooks *, FILE *);

// CLI state structure
typedef struct
{
   enum
   {
      START,
      TEST_SRC,
      // FORMAT,
      DONE,
      ERROR
   } state;
   enum
   {
      DEFAULT,
      SIMPLE,
      VERSION,
   } mode;
   const char *test_src;
   int no_clean;
   int verbose;
} CliState;

#define MAX_TEMPLATE_LEN 64

#endif // SIGTEST_CLI_H