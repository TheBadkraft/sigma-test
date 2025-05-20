/* tools/objectify.c */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static int verbose = 0;

#define GCC_COMMAND "gcc -Wall -g -Iinclude -w -x c -c %s -o %s"
#define SRC_TEMPLATES_DIR "src/templates/"
#define RESOURCES_DIR "resources/"
#define TEMPLATE_SUFFIX "_template.ct"
#define RESOURCES_SUFFIX "_template.ro"
#define DATA_SUFFIX "_data"

char *to_upper(char *);
char *to_lower(char *);

int main(int argc, char *argv[])
{
   const char *input_file = NULL;

   // Parse arguments
   for (int i = 1; i < argc; i++)
   {
      if (strcmp(argv[i], "-v") == 0)
      {
         verbose = 1;
      }
      else if (argv[i][0] != '-')
      {
         input_file = argv[i];
      }
   }

   if (!input_file)
   {
      fprintf(stderr, "Usage: %s [-v] <input.txt>\n", argv[0]);
      return 1;
   }
   else if (verbose)
   {
      fprintf(stdout, "Verbose mode enabled\n");
      fprintf(stdout, "input=%s\n", input_file);
   }

   // Copy input_file and extract base name
   char *base_name = strdup(input_file);
   if (!base_name)
   {
      perror("Error duplicating input_file");
      return 1;
   }

   // Trim leading directories
   char *slash = strrchr(base_name, '/');
   if (slash)
   {
      memmove(base_name, slash + 1, strlen(slash));
   }

   // Trim extension
   char *dot = strrchr(base_name, '.');
   size_t base_len = dot ? (size_t)(dot - base_name) : strlen(base_name);
   if (dot)
   {
      *dot = '\0';
   }

   // Dynamically allocate output_file
   size_t output_len = strlen(SRC_TEMPLATES_DIR) + base_len + strlen(TEMPLATE_SUFFIX) + 1;
   char *output_file = malloc(output_len);
   if (!output_file)
   {
      perror("Error allocating output_file");
      free(base_name);
      return 1;
   }
   snprintf(output_file, output_len, "%s%.*s%s", SRC_TEMPLATES_DIR, (int)base_len, base_name, TEMPLATE_SUFFIX);
   if (verbose)
   {
      fprintf(stdout, "output=%s\n", output_file);
   }

   // Dynamically allocate object_file
   size_t object_len = strlen(RESOURCES_DIR) + base_len + strlen(RESOURCES_SUFFIX) + 1;
   char *object_file = malloc(object_len);
   if (!object_file)
   {
      perror("Error allocating object_file");
      free(output_file);
      free(base_name);
      return 1;
   }
   snprintf(object_file, object_len, "%s%.*s%s", RESOURCES_DIR, (int)base_len, base_name, RESOURCES_SUFFIX);
   if (verbose)
   {
      fprintf(stdout, "object=%s\n", object_file);
   }

   // Open the input file for reading
   FILE *in = fopen(input_file, "rb");
   if (!in)
   {
      char err_msg[256];
      snprintf(err_msg, sizeof(err_msg), "Error opening input file: %s", input_file);
      perror(err_msg);
      free(output_file);
      free(object_file);
      free(base_name);
      return 1;
   }

   // Open the output file for writing
   FILE *out = fopen(output_file, "w");
   if (!out)
   {
      perror("Error opening output file");
      fclose(in);
      free(output_file);
      free(object_file);
      free(base_name);
      return 1;
   }

   fprintf(out, "/* Auto-generated from %s */\n", input_file);
   fprintf(out, "#ifndef %.*s_TEMPLATE_CT\n", (int)base_len, to_upper(base_name));
   fprintf(out, "#define %.*s_TEMPLATE_CT\n", (int)base_len, to_upper(base_name));
   fprintf(out, "#include <stddef.h>\n");
   fprintf(out, "const unsigned char %.*s%s[] = {\n", (int)base_len, to_lower(base_name), DATA_SUFFIX);

   // Encode the input file into a C byte array
   if (verbose)
   {
      fprintf(stdout, "Encoding %s -> %s\n", input_file, output_file);
   }
   unsigned char buffer[1024];
   size_t bytes_read;
   int first = 1;
   size_t total_bytes = 0;

   while ((bytes_read = fread(buffer, 1, sizeof(buffer), in)) > 0)
   {
      if (!first)
      {
         fprintf(out, ",\n");
      }
      for (size_t i = 0; i < bytes_read; ++i)
      {
         fprintf(out, "0x%02x", buffer[i]);
         if (i < bytes_read - 1)
         {
            fprintf(out, ", ");
         }
         total_bytes++;
      }
      first = 0;
   }

   fprintf(out, "\n};\n");
   fprintf(out, "const size_t %.*s%s_size = %zu;\n", (int)base_len, to_lower(base_name), DATA_SUFFIX, total_bytes);
   fprintf(out, "#endif /* %.*s_TEMPLATE_CT */\n", (int)base_len, to_upper(base_name));

   fclose(in);
   fclose(out);

   // Dynamically allocate compile_cmd
   size_t cmd_len = strlen(GCC_COMMAND) + output_len + object_len + 1;
   char *compile_cmd = malloc(cmd_len);
   if (!compile_cmd)
   {
      perror("Error allocating compile_cmd");
      free(output_file);
      free(object_file);
      free(base_name);
      return 1;
   }
   snprintf(compile_cmd, cmd_len, GCC_COMMAND, output_file, object_file);
   if (verbose)
   {
      fprintf(stdout, "%s\n", compile_cmd);
   }

   // Flush stdout to ensure verbose output appears before GCC
   fflush(stdout);

   // Ensure resources/ exists
   system("mkdir -p resources");
   // Compile the output file to an object file
   int compile_result = system(compile_cmd);
   if (verbose)
   {
      fprintf(stdout, "success=%s\n", compile_result == 0 ? "true" : "false");
   }
   if (compile_result != 0)
   {
      fprintf(stderr, "Error: Failed to compile %s to %s\n", output_file, object_file);
      free(compile_cmd);
      free(output_file);
      free(object_file);
      free(base_name);
      return 1;
   }

   // Clean up
   free(compile_cmd);
   free(output_file);
   free(object_file);
   free(base_name);

   return 0;
}

// string utilities
char *to_upper(char *str)
{
   char *s = str;
   while (*s)
   {
      *s = toupper((unsigned char)*s);
      s++;
   }
   return str;
}
char *to_lower(char *str)
{
   char *s = str;
   while (*s)
   {
      *s = tolower((unsigned char)*s);
      s++;
   }
   return str;
}