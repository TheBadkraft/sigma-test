// include/hooks/junit_hooks.h
#ifndef JUNIT_HOOKS_H
#define JUNIT_HOOKS_H

#include "sigtest.h"

struct JunitHookContext
{
   int verbose;
   char timestamp[32];
   TestSet set;
};

extern struct sigtest_hooks_s junit_hooks;

void junit_before_set(const TestSet set, object context);
void junit_after_set(const TestSet set, object context);
void junit_on_test_result(const TestSet set, const TestCase tc, object context);

#endif // JUNIT_HOOKS_H