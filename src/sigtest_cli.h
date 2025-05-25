/*	src/sigtest_cli.h
   Header for the sigma test CLI component

   David Boarman
   2025-05-11

   SIGTEST_CLI_VERSION "0.02.01"
*/
#ifndef SIGTEST_CLI_H
#define SIGTEST_CLI_H

#include "sigtest.h"
#include <stdio.h>

#define MAX_TEMPLATE_LEN 64

// Output log levels
typedef enum
{
   LOG_NONE,    // No logging
   LOG_MINIMAL, // Minimal logging
   LOG_VERBOSE, // Verbose logging
} LogLevel;

// CLI state structure
typedef struct
{
   enum
   {
      START,
      TEST_SRC,
      DONE,
      ERROR,
      IGNORE,
   } state;
   enum
   {
      DEFAULT,
      SIMPLE,
      VERSION,
   } mode;
   const char *test_src;
   int no_clean;
   LogLevel log_level;
   DebugLevel debug_level;
} CliState;

/**
 * @brief Debug logging function
 * @param stream :the output stream to write to
 * @param log_level :the log level
 * @param debug_level :the debug level
 * @param fmt :the format message to display
 * @param ... :the variable arguments for the format message
 */
void fdebugf(FILE *, LogLevel, DebugLevel, const char *, ...);

#endif // SIGTEST_CLI_H