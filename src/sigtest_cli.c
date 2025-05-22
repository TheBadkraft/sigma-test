/*
   sigtest_cli.c
   Command-line interface for the SigmaTest framework

   David Boarman
   2025-05-09

   This file provides a command-line interface for the SigmaTest framework.

   Sigtest (CLI) is also a demonstration on building a custom CI tool using 'sigtest'.
*/

#include "sigtest_cli.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <unistd.h> // For getpid; use <process.h> on Windows
#include <sys/stat.h>

// CLI specific declarations
#define SIGTEST_CLI_VERSION "0.02.00"
// For dynamic log level annotation
static const char *DBG_LEVELS[] = {
    "DEBUG",
    "INFO",
    "WARNING",
    "ERROR",
    "FATAL",
    NULL,
};
// CLI State
static CliState cli = {
    .state = START,
    .mode = DEFAULT,
    .test_src = NULL,
    .no_clean = 0,
    .log_level = LOG_MINIMAL,
    .debug_level = DBG_DEBUG,
};

#define BUILD_DIR "build/tmp"

void parse_args(int, char **, FILE *);
int touch_file(const char *, FILE *);
int verify_directory(const char *, FILE *);
void detect_dependencies(const char *, const char **, int *);
void gen_filenames(const char *, char *, char *, size_t);
int compile_suite(const char *[], int, char *[], FILE *);
int link_executable(const char *[], int, const char *, FILE *);
int run_and_cleanup(const char *, const char *);

int main(int argc, char **argv)
{
   parse_args(argc, argv, stderr);

   if (cli.state == ERROR)
   {
      fwritelnf(stdout, "Usage: sigtest -t <path>|[-s|--no-clean|--about|[-v|--verbose]]\n");
      return 1;
   }

   if (cli.state == DONE && cli.mode == VERSION)
   {
      fwritelnf(stdout, "SigmaTest:      v.%s", sigtest_version());
      fwritelnf(stdout, "SigmaTest(CLI): v.%s", SIGTEST_CLI_VERSION);
      if (cli.log_level == LOG_VERBOSE)
      {
         fwritelnf(stdout, "*===============================================================*");
         fwritelnf(stdout, "* Copyright 2025:                  David Boarman (The BadKraft) *");
         fwritelnf(stdout, "* License:                                                  MIT *");
         fwritelnf(stdout, "* GitHub:             https://github.com/TheBadkraft/sigma-test *");
         fwritelnf(stdout, "* Email:                                   theboarman@proton.me *");
         fwritelnf(stdout, "*===============================================================*");
      }

      return 0;
   }

   // char obj_template[MAX_TEMPLATE_LEN];
   // char exe_template[MAX_TEMPLATE_LEN];

   // Validate test source file
   if (touch_file(cli.test_src, stderr) != 0)
   {
      return 1;
   }
   else if (cli.log_level == LOG_VERBOSE)
   {
      fdebugf(stdout, cli.log_level, DBG_INFO, "Verified: source=`%s`\n", cli.test_src);
   }
   // Validate build directory
   if (verify_directory(BUILD_DIR, stderr) != 0)
   {
      return 1;
   }
   else if (cli.log_level == LOG_VERBOSE)
   {
      fdebugf(stdout, cli.log_level, DBG_INFO, "Verified: build directory=`%s`\n", BUILD_DIR);
   }

   // Build ...
   const char *sources[2];
   char *objs[2], exe[256], obj_buffers[2][256];
   int source_count = 0;
   sources[0] = cli.test_src;
   detect_dependencies(cli.test_src, sources + 1, &source_count);
   source_count++;
   // Generate filenames
   for (int i = 0; i < source_count; i++)
   {
      objs[i] = obj_buffers[i]; // Point to fixed-size buffers
      gen_filenames(sources[i], objs[i], i == 0 ? exe : NULL, sizeof(obj_buffers[i]));
   }
   // Compile the test suite
   if (compile_suite(sources, source_count, objs, stderr) != 0)
   {
      return 1;
   }
   fdebugf(stdout, cli.log_level, DBG_INFO, "Compiled: source=`%s`, object=`%s`, executable=`%s`\n", sources[0], objs[0], exe);

   // Link the object files into an executable
   if (link_executable((const char **)objs, source_count, exe, stderr) != 0)
   {
      return 1;
   }
   fdebugf(stdout, cli.log_level, DBG_INFO, "Linked: object=`%s`, executable=`%s`\n", objs[0], exe);
   // Run the test suite
   return run_and_cleanup(exe, objs[0]);
}

// Parse command line arguments
void parse_args(int argc, char **argv, FILE *err_stream)
{
   // Parse command line arguments
   for (int i = 1; i < argc; i++)
   {
      switch (cli.state)
      {
      case START:
      {
         if (strcmp(argv[i], "--about") == 0)
         {
            cli.mode = VERSION;
            cli.state = DONE;
         }
         else if (strcmp(argv[i], "-f") == 0)
         {
            fdebugf(stdout, LOG_VERBOSE, DBG_WARNING, "Option '%s' is disabled.\n", argv[i]);

            cli.state = IGNORE;
         }
         else if (strcmp(argv[i], "-t") == 0)
         {
            cli.state = TEST_SRC;
         }
         else if (strcmp(argv[i], "-s") == 0)
         {
            cli.mode = SIMPLE;
         }
         else if (strcmp(argv[i], "--no-clean") == 0)
         {
            cli.no_clean = 1;
            cli.state = START;
         }
         else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
         {
            cli.log_level = LOG_VERBOSE;
            cli.state = START;
         }
         else if (strncmp(argv[i], "--verbose=", 10) == 0)
         {
            // Parse debug level
            char *level_str = argv[i] + 10;
            // convert to int (should be 0-2)
            int level = atoi(level_str);
            if (level < 0 || level > 2)
            {
               cli.state = ERROR;
               fdebugf(err_stream, cli.log_level, DBG_ERROR, "Invalid value: verbose level='%s'\n", level_str);
            }
            else
            {
               cli.log_level = (LogLevel)level;
               cli.state = START;
            }
         }
         else if (strncmp(argv[i], "--debug=", 8) == 0)
         {
            // Parse debug level
            char *level_str = argv[i] + 8;
            // convert to int; range 0-5
            int level = atoi(level_str);
            if (level < 0 || level > 4)
            {
               cli.state = ERROR;
               fdebugf(err_stream, cli.log_level, DBG_ERROR, "Invalid value: debug level='%s'\n", level_str);
            }
            else
            {
               cli.debug_level = (DebugLevel)level;
               cli.state = START;
            }
         }

         break;
      }
      case TEST_SRC:
      {
         if (cli.test_src == NULL)
         {
            cli.test_src = argv[i];
            cli.state = START;
         }
         else
         {
            fdebugf(err_stream, cli.log_level, DBG_ERROR, "Multiple test source files");
            cli.state = ERROR;
         }

         break;
      }
      case IGNORE:
      {
         cli.state = START;

         break;
      }
      case DONE:
      {
         fdebugf(err_stream, LOG_VERBOSE, DBG_ERROR, "Error: Unexpected argument or flag: '%s'", argv[i]);
         cli.state = ERROR;

         break;
      }
      case ERROR:
         // Already in error state, ignore further arguments
         break;
      }
   }

   if (cli.state == TEST_SRC)
   {
      fwritelnf(err_stream, "Error: No test source file provided");
      cli.state = ERROR;
   }
   else if (cli.state == IGNORE && cli.test_src == NULL)
   {
      fdebugf(err_stream, cli.log_level, DBG_ERROR, "No test source or options provided\n");
      cli.state = ERROR;
   }
   else if (cli.state == START && cli.mode != VERSION && cli.test_src == NULL)
   {
      fdebugf(err_stream, LOG_VERBOSE, DBG_ERROR, "No test source or options provided\n");
      cli.state = ERROR;
   }
}
// Validate the target file
int touch_file(const char *target_file, FILE *err_stream)
{
   // Validate file exists and is readable
   FILE *test_file = fopen(target_file, "r");
   if (!test_file)
   {
      fdebugf(err_stream, cli.log_level, DBG_ERROR, "Target inaccessible: file='%s'\n", target_file);
      return 1;
   }
   fclose(test_file);

   // Validate .c extension; okay for now, but what about if we have to verify headers (.h)? or .json?
   if (!strstr(target_file, ".c"))
   {
      fdebugf(err_stream, cli.log_level, DBG_ERROR, "Target extension invalid (must be '.c'): file='%s'", target_file);
      return 1;
   }

   return 0;
}
// Verify the directory
int verify_directory(const char *dir, FILE *err_stream)
{
   // Check if build/tmp/ exists by attempting to create a temporary file
   char tmp_file[256];
   snprintf(tmp_file, sizeof(tmp_file), "%s/.sigtest_check", dir);
   FILE *tmp_check = fopen(tmp_file, "w");
   if (!tmp_check)
   {
      // Directory doesn't exist, create it
#ifdef _WIN32
      if (mkdir(dir) != 0 && errno != EEXIST)
      {
         fdebugf(err_stream, cli.log_level, DBG_ERROR, "Failed to create directory %s", dir);
         exit(1);
      }
#else
      if (mkdir(dir, 0755) != 0 && errno != EEXIST)
      {
         fdebugf(err_stream, cli.log_level, DBG_ERROR, "Failed to create directory %s", dir);
         exit(1);
      }
#endif
      fdebugf(stdout, cli.log_level, DBG_INFO, "Created directory: %s\n", dir);
   }
   else
   {
      fclose(tmp_check);
      remove(tmp_file); // Clean up check file
   }

   return 0;
}
// Detect dependencies
void detect_dependencies(const char *src, const char **deps, int *dep_count)
{
   // hardcoded for now; in the future, we will use a configuration file
   *dep_count = 0;
   deps[(*dep_count)++] = "src/sigtest.c";
}
// Generate filenames
void gen_filenames(const char *src, char *obj, char *exe, size_t len)
{
   char basename[256];
   const char *name = strrchr(src, '/') ? strrchr(src, '/') + 1 : src;
   snprintf(basename, sizeof(basename), "%s_%d", name, getpid());
   snprintf(obj, len, "%s/st_%s.o", BUILD_DIR, basename);
   if (exe)
   {
      snprintf(exe, len, "%s/st_%s.exe", BUILD_DIR, basename);
   }
   if (cli.log_level != LOG_NONE)
   {
      fdebugf(stdout, cli.log_level, DBG_INFO, "Generated filenames: source='%s', object='%s', executable='%s'\n", src, obj, exe);
   }
}
// Compile the test suite
int compile_suite(const char *sources[], int count, char *objs[], FILE *err_stream)
{
   for (int i = 0; i < count; i++)
   {
      char cmd[512];
      snprintf(cmd, sizeof(cmd), "%s -c %s -Iinclude -DSIGTEST_TEST -o %s", getenv("CC") ? getenv("CC") : "gcc", sources[i], objs[i]);
      if (cli.log_level != LOG_NONE)
      {
         fdebugf(stdout, cli.log_level, DBG_INFO, "Compiling: command='%s'\n", cmd);
      }
      int ret = system(cmd);
      if (ret != 0)
      {
         fdebugf(err_stream, cli.log_level, DBG_ERROR, "Build failed: source='%s'\n", sources[i]);
         return 1;
      }
   }

   return 0;
}
// Link the object files into an executable
int link_executable(const char *objs[], int count, const char *exe, FILE *err_stream)
{
   char cmd[512], obj_list[256] = "";
   for (int i = 0; i < count; i++)
   {
      strncat(obj_list, objs[i], sizeof(obj_list) - strlen(obj_list) - 1);
      strncat(obj_list, " ", sizeof(obj_list) - strlen(obj_list) - 1);
   }
   snprintf(cmd, sizeof(cmd), "%s %s -o %s", getenv("CC") ? getenv("CC") : "gcc", obj_list, exe);
   if (cli.log_level != LOG_NONE)
   {
      fdebugf(stdout, cli.log_level, DBG_INFO, "Linking: %s\n", cmd);
   }
   int ret = system(cmd);
   if (ret != 0)
   {
      fdebugf(err_stream, cli.log_level, DBG_ERROR, "Linking failed\n");
      return 1;
   }

   return 0;
}
// Run the test suite and clean up
int run_and_cleanup(const char *exe, const char *obj)
{
   fdebugf(stdout, cli.log_level, DBG_INFO, "Running: %s\n", exe);

   int ret = system(exe);
   if (!cli.no_clean)
   {
      remove(obj);
      remove(exe);
      fdebugf(stdout, cli.log_level, DBG_INFO, "Cleaned: %s, %s\n", obj, exe);
   }

   return ret;
}

// Debug logging function
void fdebugf(FILE *stream, LogLevel log_level, DebugLevel debug_level, const char *fmt, ...)
{
   if (log_level == LOG_NONE)
   {
      return; // No output for NONE
   }

   va_list args;
   va_start(args, fmt);

   if (log_level == LOG_MINIMAL)
   {
      // Minimal: No debug label
      vfprintf(stream, fmt, args);
   }
   else if (log_level == LOG_VERBOSE && debug_level >= cli.debug_level)
   {
      // Verbose: Include debug label (e.g., [DEBUG], [ERROR])
      if (log_level == LOG_MINIMAL && (debug_level > DBG_INFO))
      { // Only INFO+ for LOG_MINIMAL
         vfprintf(stream, fmt, args);
      }
      else if (debug_level >= 0 && debug_level < sizeof(DBG_LEVELS) / sizeof(DBG_LEVELS[0]) - 1)
      {
         char dbg_label[16];
         snprintf(dbg_label, sizeof(dbg_label), "[%s]", DBG_LEVELS[debug_level]);
         fprintf(stream, "%-10s", dbg_label);
      }
      else
      {
         fprintf(stream, "[UNKNOWN] ");
      }
      vfprintf(stream, fmt, args);
   }

   va_end(args);

   fflush(stream);
}