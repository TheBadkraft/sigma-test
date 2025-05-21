// include/hooks/json_hooks.h
#ifndef JSON_HOOKS_H
#define JSON_HOOKS_H

#include "sigtest.h"

struct JsonHookContext
{
   int count;
   int verbose;
   ts_time start;
   ts_time end;
   TestSet set;
};

extern struct sigtest_hooks_s json_hooks;

void json_before_set(const TestSet set, object context);
void json_after_set(const TestSet set, object context);
void json_before_test(object context);
void json_after_test(object context);
void json_on_start_test(object context);
void json_on_end_test(object context);
void json_on_error(const char *message, object context);
void json_on_test_result(const TestSet set, const TestCase tc, object context);

#endif // JSON_HOOKS_H