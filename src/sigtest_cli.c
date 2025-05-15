/*
   sigtest_cli.c
   Command-line interface for the SigmaTest framework

   David Boarman
   2025-05-09

   This file provides a command-line interface for the SigmaTest framework.
*/

#include "sigtest_cli.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h> // For getpid; use <process.h> on Windows
#include <errno.h>

// CLI specific declarations
void cleanup_test_runner(void);
void fwritef(FILE *, const char *, ...);

void parse_args(CliState *, int, char **, FILE *);
int gen_filenames(char *, const char *);
int touch_source(const char *, FILE *);
int ensure_tmp_dir(FILE *);
int create_temp_files(char *, char *, FILE *);
int gen_main(char *, FILE *);
int compile_source(int, const char *, const char *, FILE *);
int link_executable(int, const char *, const char *, const char *, FILE *);
int run_and_cleanup(const char *, const char *, FILE *);

static int verbose = 0; // Placeholder for future --verbose flag

int main(int argc, char **argv)
{
   CliState cli;
   parse_args(&cli, argc, argv, stderr);

   if (cli.state == ERROR)
      goto errUsage;

   if (cli.state == DONE && cli.mode == VERSION)
      return 0;

   string test_src = (string)cli.test_src;
   char main_template[MAX_TEMPLATE_LEN];
   char main_obj_template[MAX_TEMPLATE_LEN];
   char obj_template[MAX_TEMPLATE_LEN];
   char exe_template[MAX_TEMPLATE_LEN];

   // Validate test source file
   if (touch_source(test_src, stderr) != 0)
   {
      return 1;
   }
   else if (verbose) // placeholder for future --verbose/--debug option
   {
      fwritef(stdout, "Verified source file: %s", test_src);
   }

   // Ensure build/tmp/ directory exists
   if (ensure_tmp_dir(stderr) != 0)
   {
      return 1;
   }
   else if (verbose) // placeholder for future --verbose/--debug option
   {
      fwritef(stdout, "Verified build/tmp/ directory");
   }

   // Create temporary files
   if (create_temp_files(obj_template, exe_template, stderr) != 0)
   {
      return 1;
   }

   if (cli.mode == DEFAULT)
   {
      // Generate main template
      if (gen_filenames(main_template, ".c") != 0 || gen_filenames(main_obj_template, ".o") != 0 || gen_main(main_template, stderr) != 0)
      {
         remove(main_template);
         remove(main_obj_template);
         return 1;
      }
      else if (verbose) // placeholder for future --verbose/--debug option
      {
         fwritef(stdout, "Generated main template: %s", main_template);
      }
   }

   // Compile test source
   if (compile_source(cli.mode, test_src, obj_template, stderr) != 0 || (cli.mode == DEFAULT && compile_source(cli.mode, main_template, main_obj_template, stderr) != 0))
   {
      remove(obj_template);
      remove(exe_template);
      if (cli.mode == DEFAULT)
      {
         remove(main_template);
         remove(main_obj_template);
      }
      return 1;
   }
   else if (verbose) // placeholder for future --verbose/--debug option
   {
      fwritef(stdout, "Compiled test source: %s", test_src);
      if (cli.mode == DEFAULT)
         fwritef(stdout, "Compiled main template: %s", main_template);
   }

   // Link executable
   if (link_executable(cli.mode, obj_template, main_obj_template, exe_template, stderr) != 0)
   {
      remove(obj_template);
      remove(exe_template);
      if (cli.mode == DEFAULT)
      {
         remove(main_template);
         remove(main_obj_template);
      }
      return 1;
   }

   if (cli.mode == DEFAULT)
   {
      remove(main_template);
      remove(main_obj_template);
   }

   // Run tests and clean up
   return run_and_cleanup(exe_template, obj_template, stderr);

errUsage:
   fwritef(stdout, "Usage: sigtest [-s|-t <path>|-v]\n");
   return 1;
}

void parse_args(CliState *cli, int argc, char **argv, FILE *err_stream)
{
   // Initialize state
   cli->state = START;
   cli->mode = DEFAULT;
   cli->test_src = NULL;

   // Parse command line arguments
   for (int i = 1; i < argc; i++)
   {
      switch (cli->state)
      {
      case START:
         if (strcmp(argv[i], "-v") == 0)
         {
            cli->mode = VERSION;
            cli->state = DONE;
            fwritef(stdout, "SigmaTest: version %s", sigtest_version());
         }
         else if (strcmp(argv[i], "-t") == 0)
         {
            cli->state = TEST_SRC;
         }
         else if (strcmp(argv[i], "-s") == 0)
         {
            cli->mode = SIMPLE;
         }
         else
         {
            cli->state = ERROR;
            fwritef(err_stream, "Error: Unknown option '%s'", argv[i]);
         }

         break;
      case TEST_SRC:
         if (cli->test_src == NULL)
         {
            cli->test_src = argv[i];
            cli->state = DONE;
         }
         else
         {
            fwritef(err_stream, "Error: Multiple test source files provided");
            cli->state = ERROR;
         }

         break;
      case DONE:
         fwritef(err_stream, "Error: Unexpected argument or flag: '%s'", argv[i]);
         cli->state = ERROR;

         break;
      case ERROR:
         // Already in error state, ignore further arguments
         break;
      }
   }

   if (cli->state == TEST_SRC)
   {
      fwritef(err_stream, "Error: No test source file provided");
      cli->state = ERROR;
   }
   else if (cli->state == START && cli->mode != VERSION)
   {
      fwritef(err_stream, "Error: No test source or options provided");
      cli->state = ERROR;
   }
}
int touch_source(const char *test_src, FILE *err_stream)
{
   // Validate file exists and is readable
   FILE *test_file = fopen(test_src, "r");
   if (!test_file)
   {
      fwritef(err_stream, "Error: Test file %s is not readable", test_src);
      return 1;
   }
   fclose(test_file);

   // Validate .c extension
   if (!strstr(test_src, ".c"))
   {
      fwritef(err_stream, "Error: Test source '%s' must be a .c file", test_src);
      return 1;
   }

   return 0;
}
int ensure_tmp_dir(FILE *err_stream)
{
   // Check if build/tmp/ exists by attempting to create a temporary file
   FILE *tmp_check = fopen("build/tmp/.sigtest_check", "w");
   if (!tmp_check)
   {
      // Directory doesn't exist, create it
      if (system("mkdir build") != 0 && errno != EEXIST)
      {
         fwritef(err_stream, "Error: Failed to create build directory");
         return 1;
      }
      if (system("mkdir build/tmp") != 0 && errno != EEXIST)
      {
         fwritef(err_stream, "Error: Failed to create build/tmp directory");
         return 1;
      }
      // Verify creation
      tmp_check = fopen("build/tmp/.sigtest_check", "w");
      if (!tmp_check)
      {
         fwritef(err_stream, "Error: Failed to create build/tmp directory");
         return 1;
      }
   }
   fclose(tmp_check);
   remove("build/tmp/.sigtest_check"); // Clean up check file

   return 0;
}
int gen_filenames(char *template, const char *suffix)
{
   static int counter = 0;
   // Initialize random seed for additional uniqueness
   static int initialized = 0;
   if (!initialized)
   {
      srand((unsigned int)time(NULL));
      initialized = 1;
   }

   // Generate unique name: build/tmp/sigtest_<timestamp>_<pid>_<counter><suffix>
   snprintf(template, MAX_TEMPLATE_LEN, "build/tmp/sigtest_%ld_%d_%d%s",
            (long)time(NULL), (int)getpid(), counter++, suffix);
   if (strlen(template) >= MAX_TEMPLATE_LEN)
   {
      return -1; // Name too long
   }

   // Check if file exists to avoid collisions
   FILE *file = fopen(template, "r");
   if (file)
   {
      fclose(file);
      return -1; // File exists, unlikely due to timestamp+pid+counter
   }

   // Create file to reserve name
   file = fopen(template, "w");
   if (!file)
   {
      return -1; // File creation failed
   }
   fclose(file);

   return 0;
}
int create_temp_files(char *obj_template, char *exe_template, FILE *err_stream)
{
   // Generate object file name
   if (gen_filenames(obj_template, ".o") != 0)
   {
      fwritef(err_stream, "Error: Failed to generate object file name");
      return 1;
   }

   // Generate executable file name
   if (gen_filenames(exe_template,
#ifdef _WIN32
                     ".exe"
#else
                     ""
#endif
                     ) != 0)
   {
      fwritef(err_stream, "Error: Failed to generate executable file name");
      remove(obj_template); // Clean up object file
      return 1;
   }

   return 0;
}
int gen_main(char *main_template, FILE *err_stream)
{
   // Generate main function template
   FILE *main_file = fopen(main_template, "w");
   if (!main_file)
   {
      fwritef(err_stream, "Error: Failed to generate `main.c` at %s", main_template);
      return 1;
   }

   fwritef(main_file, "#include \"sigtest.h\"");
   fwritef(main_file, "#include <stdlib.h>\n");

   fwritef(main_file, "extern TestSet test_sets;\n");

   fwritef(main_file, "int main(void) {");
   fwritef(main_file, "    SigtestHooks hooks = malloc(sizeof(struct sigtest_hooks_s));");
   fwritef(main_file, "    if (hooks == NULL) {");
   fwritef(main_file, "        fprintf(stderr, \"Error: Test Hooks allocation failed\\n\");\n");

   fwritef(main_file, "        return 1;");
   fwritef(main_file, "    }\n");

   fwritef(main_file, "    int retRun = run_tests(test_sets, hooks);");
   fwritef(main_file, "    free(hooks);\n");

   fwritef(main_file, "    return retRun;");
   fwritef(main_file, "}");

   fclose(main_file);
   return 0;
}
int compile_source(int mode, const char *src, const char *obj, FILE *err_stream)
{
   char compile_cmd[512];

   if (mode == DEFAULT && strstr(src, "build/tmp/") && strstr(src, ".c"))
   {
      snprintf(compile_cmd, sizeof(compile_cmd),
               "gcc -Wall -g -Iinclude -DSIGTEST_CLI -c %s -o %s", src, obj);
   }
   else
   {
      snprintf(compile_cmd, sizeof(compile_cmd),
               "gcc -Wall -g -Iinclude -c %s -o %s", src, obj);
   }
   if (system(compile_cmd) != 0)
   {
      fwritef(err_stream, "Error: Compilation failed for '%s'", src);
      return 1;
   }
   return 0;
}
// if mode == SIMPLE, pass NULL for main_template
int link_executable(int mode, const char *obj_template, const char *main_template, const char *exe_template, FILE *err_stream)
{
   char link_cmd[512];
   if (mode == SIMPLE)
   {
      snprintf(link_cmd, sizeof(link_cmd),
               "gcc %s -Lbin/lib -lsigtest -Wl,-rpath,bin/lib -o %s",
               obj_template, exe_template);
   }
   else
   {
      snprintf(link_cmd, sizeof(link_cmd),
               "gcc %s %s -Lbin/lib -lsigtest -Wl,-rpath,bin/lib -o %s",
               obj_template, main_template, exe_template);
   }
   if (system(link_cmd) != 0)
   {
      fwritef(err_stream, "Error: Linking failed for '%s'", obj_template);
      return 1;
   }
   return 0;
}
int run_and_cleanup(const char *exe_template, const char *obj_template, FILE *err_stream)
{
   // Run the executable
   int status = system(exe_template);

   // Clean up temporary files
   remove(obj_template);
   remove(exe_template);

   // Assume normal exit; return status as exit code (0 for success, 1 for failure)
   return status;
}