// src/hooks/junit_hooks.c
#include "hooks/junit_hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
   Test hooks for JUnit XML output formatting.

   David Boarman
*/

struct sigtest_hooks_s junit_hooks = {
    .name = "junit_hooks",
    .before_set = junit_before_set,
    .after_set = junit_after_set,
    .before_test = NULL,
    .after_test = NULL,
    .on_start_test = NULL,
    .on_end_test = NULL,
    .on_error = NULL,
    .on_test_result = junit_on_test_result,
    .context = NULL,
};

void junit_before_set(const TestSet set, object context)
{
   struct JunitHookContext *ctx = context;
   ctx->set = set;
   get_timestamp(ctx->timestamp, "%Y-%m-%dT%H:%M:%S");
   set->logger->log("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
   set->logger->log("<testsuites>\n");
   set->logger->log("<testsuite name=\"%s\" tests=\"%d\">\n", set->name, set->count);
}

void junit_after_set(const TestSet set, object context)
{
   struct JunitHookContext *ctx = context;
   set->logger->log("</testsuite>\n");
   set->logger->log("</testsuites>\n");
   ctx->set = NULL;
   junit_hooks.context = NULL; // Reset for next test set
}

void junit_on_test_result(const TestSet set, const TestCase tc, object context)
{
   set->logger->log("<testcase name=\"%s\">\n", tc->name);
   if (tc->test_result.state == FAIL)
   {
      char escaped[512];
      char *dst = escaped;
      const char *src = tc->test_result.message ? tc->test_result.message : "Unknown failure";
      for (; *src && dst < escaped + sizeof(escaped) - 2; src++)
      {
         if (*src == '"')
            *dst++ = '\\';
         *dst++ = *src;
      }
      *dst = '\0';
      set->logger->log("<failure message=\"%s\"/>\n", escaped);
   }
   else if (tc->test_result.state == SKIP)
   {
      set->logger->log("<skipped/>\n");
   }
   set->logger->log("</testcase>\n");
}