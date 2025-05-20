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
#include <time.h>
#include <unistd.h> // For getpid; use <process.h> on Windows
#include <errno.h>
// include embedded resource
#include "templates/main_template.ct"

// CLI specific declarations
#define SIGTEST_CLI_VERSION "0.1.0"

void cleanup_test_runner(void);
void fwritef(FILE *, const char *, ...);

void parse_args(CliState *, int, char **, FILE *);
int gen_filenames(char *, const char *);
int touch_source(const char *, FILE *);
int ensure_tmp_dir(FILE *);
int create_temp_files(char *, char *, FILE *);
int gen_main(char *, const char *, FILE *, int);
int compile_source(int, const char *, const char *, FILE *, int);
int link_executable(int, const char *, const char *, const char *, FILE *, int);
int run_and_cleanup(const char *, const char *, FILE *, int);
static char *replace_tag(const char *, const char *, const char *, int);

// Structure to track source segment and tag replacement location
typedef struct source_segment
{
   const char *src;             // Pointer to the source string
   size_t len;                  // Length of segment
   struct source_segment *next; // Pointer to the next segment
} SourceSegment;

// SigtetsHooks declarations
static void format_junit_output(const TestSet, const TestCase, const object);
OutputFormat parse_output_format(const char *);
static void junit_before_set(const TestSet, object);
static void junit_after_set(const TestSet, object);
void init_cli_hooks(SigtestHooks *, const char *, FILE *);

int main(int argc, char **argv)
{
   SigtestHooks hooks = NULL;
   CliState cli;
   parse_args(&cli, argc, argv, stderr);

   if (cli.state == ERROR)
   {
      fwritelnf(stdout, "Usage: sigtest -t <path>|[-s|--no-clean|--about|[-v|--verbose]]\n");
      return 1;
   }

   if (cli.state == DONE && cli.mode == VERSION)
   {
      if (hooks)
         free(hooks);
      return 0;
   }
   string test_src = (string)cli.test_src;
   char main_template[MAX_TEMPLATE_LEN];
   char main_obj_template[MAX_TEMPLATE_LEN];
   char obj_template[MAX_TEMPLATE_LEN];
   char exe_template[MAX_TEMPLATE_LEN];

   // Validate test source file
   if (touch_source(test_src, stderr) != 0)
   {
      if (hooks)
         free(hooks);
      return 1;
   }
   else if (cli.verbose) // placeholder for future --verbose/--debug option
   {
      fwritelnf(stdout, "Verified source file: %s", test_src);
   }

   // Ensure build/tmp/ directory exists
   if (ensure_tmp_dir(stderr) != 0)
   {
      if (hooks)
         free(hooks);
      return 1;
   }
   else if (cli.verbose) // placeholder for future --verbose/--debug option
   {
      fwritelnf(stdout, "Verified build/tmp/ directory");
   }

   // Create temporary files
   if (create_temp_files(obj_template, exe_template, stderr) != 0)
   {
      if (hooks)
         free(hooks);
      return 1;
   }

   // Initialize hooks for DEFAULT mode
   if (cli.mode == DEFAULT)
   {
      init_cli_hooks(&hooks, cli.output_format, stderr);
      // Generate main template
      if (gen_filenames(main_template, ".c") != 0 || gen_filenames(main_obj_template, ".o") != 0 || gen_main(main_template, cli.output_format, stderr, cli.verbose) != 0)
      {
         remove(main_template);
         remove(main_obj_template);
         if (hooks)
            free(hooks);
         return 1;
      }
      else if (cli.verbose) // placeholder for future --verbose/--debug option
      {
         fwritelnf(stdout, "Generated main template: %s", main_template);
      }
   }
   else if (cli.output_format)
   {
      fwritelnf(stderr, "Error: Output format is only supported in DEFAULT mode");
      if (hooks)
         free(hooks);
      return 1;
   }

   // Compile test source
   if (compile_source(cli.mode, test_src, obj_template, stderr, cli.verbose) != 0 || (cli.mode == DEFAULT && compile_source(cli.mode, main_template, main_obj_template, stderr, cli.verbose) != 0))
   {
      remove(obj_template);
      remove(exe_template);
      if (cli.mode == DEFAULT)
      {
         if (!cli.no_clean)
         {
            remove(main_template);
            remove(main_obj_template);
         }
         if (hooks)
            free(hooks);
      }
      return 1;
   }
   else if (cli.verbose) // placeholder for future --verbose/--debug option
   {
      fwritelnf(stdout, "Compiled test source: %s", test_src);
      if (cli.mode == DEFAULT)
         fwritelnf(stdout, "Compiled main template: %s", main_template);
   }

   // Link executable
   if (link_executable(cli.mode, obj_template, main_obj_template, exe_template, stderr, cli.verbose) != 0)
   {
      remove(obj_template);
      remove(exe_template);
      if (cli.mode == DEFAULT)
      {
         if (!cli.no_clean)
         {
            remove(main_template);
            remove(main_obj_template);
         }
         if (hooks)
            free(hooks);
      }
      return 1;
   }

   // Housekeeping
   if (cli.mode == DEFAULT && !cli.no_clean)
   {
      remove(main_template);
      remove(main_obj_template);
   }

   // Run tests and clean up
   int result = run_and_cleanup(exe_template, obj_template, stderr, cli.verbose);
   if (hooks)
      free(hooks);

   return result;
}

void parse_args(CliState *cli, int argc, char **argv, FILE *err_stream)
{
   // Initialize state
   cli->state = START;
   cli->mode = DEFAULT;
   cli->test_src = NULL;
   cli->output_format = NULL;
   cli->no_clean = 0; // Default to clean up after execution

   // Parse command line arguments
   for (int i = 1; i < argc; i++)
   {
      switch (cli->state)
      {
      case START:
      {
         if (strcmp(argv[i], "--about") == 0)
         {
            cli->mode = VERSION;
            cli->state = DONE;
            fwritelnf(stdout, "SigmaTest:      v.%s", sigtest_version());
            fwritelnf(stdout, "SigmaTest(CLI): v.%s", SIGTEST_CLI_VERSION);
         }
         else if (strcmp(argv[i], "-f") == 0)
         {
            cli->state = FORMAT;
         }
         else if (strcmp(argv[i], "-t") == 0)
         {
            cli->state = TEST_SRC;
         }
         else if (strcmp(argv[i], "-s") == 0)
         {
            cli->mode = SIMPLE;
         }
         else if (strcmp(argv[i], "--no-clean") == 0)
         {
            cli->no_clean = 1;
            cli->state = START;
         }
         else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
         {
            cli->verbose = 1;
            cli->state = START;
         }
         else
         {
            cli->state = ERROR;
            fwritelnf(err_stream, "Error: Unknown option '%s'", argv[i]);
         }

         break;
      }
      case TEST_SRC:
      {
         if (cli->test_src == NULL)
         {
            cli->test_src = argv[i];
            cli->state = START;
            if (cli->verbose)
            {
               fwritelnf(stdout, "Test source file: %s", cli->test_src);
            }
         }
         else
         {
            fwritelnf(err_stream, "Error: Multiple test source files provided");
            cli->state = ERROR;
         }

         break;
      }
      case FORMAT:
      {
         if (cli->output_format == NULL)
         {
            OutputFormat format = parse_output_format(argv[i]);
            if (format == FORMAT_DEFAULT && strcmp(argv[i], "default") != 0)
            {
               fwritelnf(err_stream, "Error: Invalid output format '%s'", argv[i]);
               cli->state = ERROR;
            }
            else
            {
               cli->output_format = argv[i];
               cli->state = START;
            }
         }
         else
         {
            fwritelnf(err_stream, "Error: Multiple output formats provided");
            cli->state = ERROR;
         }

         break;
      }
      case DONE:
         fwritelnf(err_stream, "Error: Unexpected argument or flag: '%s'", argv[i]);
         cli->state = ERROR;

         break;
      case ERROR:
         // Already in error state, ignore further arguments
         break;
      }
   }

   if (cli->state == TEST_SRC)
   {
      fwritelnf(err_stream, "Error: No test source file provided");
      cli->state = ERROR;
   }
   else if (cli->state == START && cli->mode != VERSION && cli->test_src == NULL)
   {
      fwritelnf(err_stream, "Error: No test source or options provided");
      cli->state = ERROR;
   }
}
int touch_source(const char *test_src, FILE *err_stream)
{
   // Validate file exists and is readable
   FILE *test_file = fopen(test_src, "r");
   if (!test_file)
   {
      fwritelnf(err_stream, "Error: Test file %s is not readable", test_src);
      return 1;
   }
   fclose(test_file);

   // Validate .c extension
   if (!strstr(test_src, ".c"))
   {
      fwritelnf(err_stream, "Error: Test source '%s' must be a .c file", test_src);
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
         fwritelnf(err_stream, "Error: Failed to create build directory");
         return 1;
      }
      if (system("mkdir build/tmp") != 0 && errno != EEXIST)
      {
         fwritelnf(err_stream, "Error: Failed to create build/tmp directory");
         return 1;
      }
      // Verify creation
      tmp_check = fopen("build/tmp/.sigtest_check", "w");
      if (!tmp_check)
      {
         fwritelnf(err_stream, "Error: Failed to create build/tmp directory");
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
      fwritelnf(stderr, "Error: Template name too long");
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
      fwritelnf(stderr, "Error: Failed to create template file %s", template);
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
      fwritelnf(err_stream, "Error: Failed to generate object file name");
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
      fwritelnf(err_stream, "Error: Failed to generate executable file name");
      remove(obj_template); // Clean up object file
      return 1;
   }

   return 0;
}
int gen_main(char *main_template, const char *output_format, FILE *err_stream, int verbose)
{
   // Generate main function template
   FILE *main_file = fopen(main_template, "w");
   if (!main_file)
   {
      fwritelnf(err_stream, "Error: Failed to generate `main.c` at %s", main_template);
      return 1;
   }

   // allocate from our embedded main template
   char *template = malloc(main_data_size + 1);
   if (!template)
   {
      fclose(main_file);
      fwritelnf(err_stream, "Error: Failed to allocate memory for main template");
      return 1;
   }
   memcpy(template, main_data, main_data_size);
   template[main_data_size] = '\0'; // Null-terminate the string

   // process parameters
   const char *format_str = (output_format && strcmp(output_format, "junit") == 0) ? "FORMAT_JUNIT" : "FORMAT_DEFAULT";
   char format_id[32];
   snprintf(format_id, sizeof(format_id), "%d", FORMAT_JUNIT);
   char fail_state[32];
   snprintf(fail_state, sizeof(fail_state), "%d", FAIL);
   char skip_state[32];
   snprintf(skip_state, sizeof(skip_state), "%d", SKIP);

   // replace placeholders in the template
   char *result = replace_tag(template, "%FORMAT%", format_str, verbose);
   char *result2 = replace_tag(result, "%JUNIT_FORMAT%", format_id, verbose);
   char *result3 = replace_tag(result2, "%FAIL_STATE%", fail_state, verbose);
   char *result4 = replace_tag(result3, "%SKIP_STATE%", skip_state, verbose);
   if (!result4)
   {
      if (result3)
         free(result3);
      if (result2)
         free(result2);
      if (result)
         free(result);

      free(template);
      fwritelnf(err_stream, "Error: Failed to process main template");
      fclose(main_file);

      return 1;
   }

   // Write the processed template to the file
   fwritef(main_file, "%s", result4);

   // Clean up
   free(template);
   free(result);
   free(result2);
   free(result3);
   free(result4);
   fclose(main_file);

   return 0;
}
int compile_source(int mode, const char *src, const char *obj, FILE *err_stream, int verbose)
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

   if (verbose)
   {
      fwritelnf(stdout, "***: %s", compile_cmd);
   }

   if (system(compile_cmd) != 0)
   {
      fwritef(err_stream, "Error: Compilation failed for '%s'", src);
      return 1;
   }
   return 0;
}
int link_executable(int mode, const char *obj_template, const char *main_template, const char *exe_template, FILE *err_stream, int verbose)
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

   if (verbose)
   {
      fwritelnf(stdout, "***: %s", link_cmd);
   }

   if (system(link_cmd) != 0)
   {
      fwritelnf(err_stream, "Error: Linking failed for '%s'", obj_template);
      return 1;
   }
   return 0;
}
int run_and_cleanup(const char *exe_template, const char *obj_template, FILE *err_stream, int verbose)
{
   // Build the command
   char cmd[512];
   snprintf(cmd, sizeof(cmd), "%s", exe_template);
   if (verbose)
   {
      fwritef(stdout, "Running command: %s", cmd);
   }

   // Run the executable
   int status = system(cmd);

   // Clean up temporary files
   remove(obj_template);
   remove(exe_template);

   // Assume normal exit; return status as exit code (0 for success, 1 for failure)
   return status;
}
static char *replace_tag(const char *buffer, const char *tag, const char *value, int verbose)
{
   if (!buffer || !tag || !value)
   {
      return NULL;
   }

   size_t tag_len = strlen(tag);
   size_t value_len = strlen(value);
   size_t src_len = strlen(buffer);
   size_t count = 0;

   // set first segment to the start of the buffer
   SourceSegment *head = malloc(sizeof(SourceSegment));
   if (!head)
   {
      return NULL;
   }
   head->src = buffer;
   head->len = 0;
   head->next = NULL;
   SourceSegment *current = head;

   // build segments in single pass
   const char *pos = buffer;
   while (pos < buffer + src_len)
   {
      const char *found = strstr(pos, tag);
      if (!found)
      {
         // No more tags found, add remaining buffer
         current->len = (buffer + src_len) - pos;
         break;
      }

      // set curent segment length
      current->len = found - pos;
      count++;
      if (verbose)
      {
         // found
         fwritelnf(stdout, "Tag [@ %ld] '%s' -> '%s'\n", (long)(pos - buffer), tag, value);
      }

      // add replacement segment
      SourceSegment *segment = malloc(sizeof(SourceSegment));
      if (!segment)
      {
         SourceSegment *tmp = head;
         while (tmp)
         {
            SourceSegment *next = tmp->next;
            free(tmp);
            tmp = next;
         }
         return NULL;
      }
      segment->src = value;
      segment->len = value_len;
      segment->next = NULL;
      // link the new segment to the current segment
      current->next = segment;
      current = segment;

      // is there a next tag?
      pos = found + tag_len;
      if (pos < buffer + src_len)
      {
         // add segment for the text between the tags
         current->next = malloc(sizeof(SourceSegment));
         if (!current->next)
         {
            SourceSegment *tmp = head;
            while (tmp)
            {
               SourceSegment *next = tmp->next;
               free(tmp);
               tmp = next;
            }
            return NULL;
         }
         // update the current segment
         current->next->src = pos;
         current->next->len = 0;
         current->next->next = NULL;
         current = current->next;
      }
   }

   // Calculate new length
   size_t new_len = 0;
   for (SourceSegment *seg = head; seg; seg = seg->next)
   {
      new_len += seg->len;
   }
   new_len += 1; // for null terminator

   // allocate the new string
   char *result = malloc(new_len);
   if (!result)
   {
      SourceSegment *tmp = head;
      while (tmp)
      {
         SourceSegment *next = tmp->next;
         free(tmp);
         tmp = next;
      }
      return NULL;
   }

   // assemble the string
   char *ptr = result;
   for (SourceSegment *seg = head; seg; seg = seg->next)
   {
      memcpy(ptr, seg->src, seg->len);
      ptr += seg->len;
   }

   // Free segments
   SourceSegment *tmp = head;
   while (tmp)
   {
      SourceSegment *next = tmp->next;
      free(tmp);
      tmp = next;
   }

   if (verbose)
   {
      fwritelnf(stdout, "Generated string length: %zu\n", new_len - 1);
   }

   return result;
}

// hooks & output format functions
// parses the output format from the command line
OutputFormat parse_output_format(const char *format)
{
   if (!format || strcmp(format, "default") == 0)
   {
      return FORMAT_DEFAULT;
   }
   else if (strcmp(format, "junit") == 0)
   {
      return FORMAT_JUNIT;
   }

   fwritelnf(stderr, "Warning: Invalid output format '%s', defaulting to 'default'", format);
   return FORMAT_DEFAULT;
}
// Initialize CLI hooks
void init_cli_hooks(SigtestHooks *hooks, const char *output_format, FILE *err_stream)
{
   OutputFormat format = parse_output_format(output_format);
   *hooks = init_hooks("junit");
   if (!*hooks)
   {
      fwritelnf(err_stream, "Error: Failed to initialize hooks");
      exit(EXIT_FAILURE);
   }

   if (format == FORMAT_JUNIT)
   {
      (*hooks)->before_set = junit_before_set;
      (*hooks)->after_set = junit_after_set;
      // (*hooks)->format_output = format_junit_output;
   }
}
// JUnit XML output formattter - these are moving to a separate file for better accessibility
// NOTE: using `fwrite*f` here is not ideal; we should use the test set's logger
static void format_junit_output(const TestSet set, const TestCase tc, const object context)
{
   static int first_test = 1;
   static char timestamp[32];

   if (first_test)
   {
      get_timestamp(timestamp, "%Y-%m-%dT%H:%M:%S");
      // fwritef(stream, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
      set->logger->log("<?xml version=\"1.0\" encoding=\"UTF-8\"?>");
      // fwritef(stream, "<testsuites>");
      set->logger->log("<testsuites>");

      first_test = 0;
   }

   // suite header is printed in the before_set hook
   // fwritef(stream, "<testcase name=\"%s\">", tc->name);
   set->logger->log("<testcase name=\"%s\">", tc->name);

   if (tc->test_result.state == FAIL)
   {
      // fwritef(stream, "<failure message=\"%s\"/>\n", tc->test_result.message ? tc->test_result.message : "Unknown failure");
      set->logger->log("<failure message=\"%s\"/>\n", tc->test_result.message ? tc->test_result.message : "Unknown failure");
   }
   else if (tc->test_result.state == SKIP)
   {
      // fwritef(stream, "<skipped/>");
      set->logger->log("<skipped/>");
   }
   // fwritef(stream, "</testcase>\n");
   set->logger->log("</testcase>\n");
}
// Before test set hook for JUnit XML
static void junit_before_set(const TestSet set, object context)
{
   if (set->log_stream)
   {
      // fwritef(set->log_stream, "<testsuite name\"%s\" tests=\"%d\">", set->name, set->count);
      set->logger->log("<testsuite name=\"%s\" tests=\"%d\">", set->name, set->count);
   }
}
// After test set hook for JUnit XML
static void junit_after_set(const TestSet set, object context)
{
   if (set->log_stream)
   {
      // fwritef(set->log_stream, "</testsuite>");
      set->logger->log("</testsuite>");
   }
}