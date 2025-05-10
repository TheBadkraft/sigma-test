/*
   sigtest_cli.c
   Command-line interface for the SigmaTest framework

   David Boarman
   2025-05-09

   This file provides a command-line interface for the SigmaTest framework.
*/

#include "sigtest.h"
#include <stdio.h>
#include <string.h>

int main(int argc, char *argv[])
{
   if (argc > 1)
   {
      if (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)
      {
         writef("SigmaTest: version %s", sigtest_version());
         return 0;
      }

      writef("Unknown option: %s", argv[1]);
      return 1;
   }

   writef("Usage: sigtest [-v|--version]\n");
   return 1;
}