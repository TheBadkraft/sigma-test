/*	sigtest.c
	David Boarman
*/
#include "sigtest.h"
#include <stdlib.h>
#include <setjmp.h>
#include <math.h>
#include <float.h>  //	for FLT_EPSILON && DBL_EPSILON
#include <string.h> // 	for jmp_buf and related functions
#include <strings.h>
#include <stdarg.h>

// Global test set instance
TestSet test_set = {NULL, NULL, NULL, NULL, NULL, NULL};
// Global context for the currently running test
TestCase *current_test = NULL;
// Static buffer for jump
static jmp_buf jmpbuffer;

//	Fail messages
#define MESSAGE_TRUE_FAIL "Expected true, but was false"
#define MESSAGE_FALSE_FAIL "Expected false, but was true"
#define MESSAGE_EQUAL_FAIL "Expected %s, but was %s"
#define EXPECT_FAIL_FAIL "Expected test to fail but it passed"
#define EXPECT_THROW_FAIL "Expected test to throw but it didn't"

//	Implementations for internal helpers
//	generate formatted message
static string format_msg(const string fmt, va_list args)
{
	static char msg_buffer[256];
	vsnprintf(msg_buffer, sizeof(msg_buffer), fmt ? fmt : "", args);

	return msg_buffer;
}
// get defined message or default message
static string get_msg(const string fmt, const string defaultMessage, va_list args)
{
	return fmt ? format_msg(fmt, args) : defaultMessage;
}
// generate message for assertEquals
static string gen_equals_fail_msg(object expected, object actual, AssertType type, const string fmt, va_list args)
{
	static char msg_buffer[256];
	char exp_str[20], act_str[20];

	switch (type)
	{
	case INT:
		snprintf(exp_str, sizeof(exp_str), "%d", *(int *)expected);
		snprintf(act_str, sizeof(act_str), "%d", *(int *)actual);

		break;
	case FLOAT:
		snprintf(exp_str, sizeof(exp_str), "%.5f", *(float *)expected);
		snprintf(act_str, sizeof(act_str), "%.5f", *(float *)actual);

		break;
	case DOUBLE:
		snprintf(exp_str, sizeof(exp_str), "%.5f", *(double *)expected);
		snprintf(act_str, sizeof(act_str), "%.5f", *(double *)actual);

		break;
	case CHAR:
		snprintf(exp_str, sizeof(exp_str), "%c", *(char *)expected);
		snprintf(act_str, sizeof(act_str), "%c", *(char *)actual);

		break;
	case STRING:
		strncpy(exp_str, (string)expected, sizeof(exp_str) - 1);
		exp_str[sizeof(exp_str) - 1] = '\0';
		strncpy(act_str, (string)actual, sizeof(act_str) - 1);
		act_str[sizeof(act_str) - 1] = '\0';

		break;
	case PTR:
		snprintf(exp_str, sizeof(exp_str), "%p", expected);
		snprintf(act_str, sizeof(act_str), "%p", actual);

		break;
	default:
		return "Unsupported type for comparison";
	}

	string user_msg = fmt ? format_msg(fmt, args) : "";
	snprintf(msg_buffer, sizeof(msg_buffer), MESSAGE_EQUAL_FAIL, exp_str, act_str);
	if (user_msg[0] != '\0')
	{
		strncat(msg_buffer, " [", sizeof(msg_buffer) - strlen(msg_buffer) - 1);
		strncat(msg_buffer, user_msg, sizeof(msg_buffer) - strlen(msg_buffer) - 1);
		strncat(msg_buffer, "]", sizeof(msg_buffer) - strlen(msg_buffer) - 1);
	}
	return msg_buffer;
}

void set_test_context(TestState result, const string message)
{
	if (current_test)
	{
		current_test->testResult.state = result;
		if (current_test->testResult.message)
		{
			free(current_test->testResult.message);
		}

		current_test->testResult.message = message ? strdup(message) : NULL;
		if (result == FAIL)
		{
			// If the test fails, we stop further assertions for this test
			longjmp(jmpbuffer, 1); // Assumes jmpbuffer is set up elsewhere
		}
	}
}

// 	Implementations for assertions (public interface)
/*
	Asserts the condition is TRUE
*/
static void assert_is_true(int condition, const string fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	if (!condition)
	{
		string errMessage = get_msg(fmt, MESSAGE_TRUE_FAIL, args);
		debugf("Assertion failed: %s\n", errMessage);
		set_test_context(FAIL, errMessage);
	}
	else
	{
		set_test_context(PASS, NULL);
	}

	va_end(args);
}
/*
	Asserts the condition is FALSE
*/
static void assert_is_false(int condition, const string fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	if (condition)
	{
		string errMessage = get_msg(fmt, MESSAGE_TRUE_FAIL, args);
		debugf("Assertion failed: %s\n", errMessage);
		set_test_context(FAIL, errMessage);
	}
	else
	{
		set_test_context(PASS, NULL);
	}

	va_end(args);
}
/*
	Asserts two values are equal
*/
static void assert_are_equal(object expected, object actual, AssertType type, const string fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	// char messageBuffer[256];
	enum
	{
		PASS,
		FAIL
	} result = PASS;
	string failMessage = NULL;

	switch (type)
	{
	case INT:
		if (*(int *)expected != *(int *)actual)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
	case FLOAT:
		if (fabs(*(float *)expected - *(float *)actual) > FLT_EPSILON)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
	case DOUBLE:
		if (fabs(*(double *)expected - *(double *)actual) > DBL_EPSILON)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
	case CHAR:
		if (*(char *)expected != *(char *)actual)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
	case STRING:
		if (strcasecmp((string)expected, (string)actual) != 0)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
	case PTR:
		if (expected != actual)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
		// Add cases for other types as needed
	}
	if (result == FAIL)
	{
		debugf("Assertion failed: %s", failMessage); /* Log failure */
	}

	set_test_context(result, failMessage);

	va_end(args);
}
/*
	Assert throws
*/
static void assert_throw(const string fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	string err_message = get_msg(fmt, "Explicit throw triggered", args);
	writef("Throw triggered: %s\n", err_message);
	set_test_context(FAIL, err_message);

	va_end(args);
}

/*
	IAssert interface
*/
const IAssert Assert = {
	 .isTrue = assert_is_true,
	 .isFalse = assert_is_false,
	 .areEqual = assert_are_equal,
	 .throw = assert_throw,
};

// Test registry
TestCase tests[100];
int test_count = 0;

/*
	Register test to test registry
*/
void testcase(string name, void (*func)(void))
{
	tests[test_count].name = name;
	tests[test_count].test_func = func;
	tests[test_count].expect_fail = FALSE;
	tests[test_count].expect_throw = FALSE;
	test_count++;
}
/*
	Register test to test registry with expectation to fail
*/
void fail_testcase(string name, void (*func)(void))
{
	tests[test_count].name = name;
	tests[test_count].test_func = func;
	tests[test_count].expect_fail = TRUE;
	tests[test_count].expect_throw = FALSE;
	test_count++;
}
/*
	Register test to test registry with expectation to throw
*/
void testcase_throws(string name, void (*func)(void))
{
	tests[test_count].name = name;
	tests[test_count].test_func = func;
	tests[test_count].expect_fail = FALSE;
	tests[test_count].expect_throw = TRUE;
	test_count++;
}
/*
	Setup test case
*/
void setup_testcase(void (*setup)(void))
{
	test_set.setup = setup;
}
/*
	Teardown test case
*/
void teardown_testcase(void (*teardown)(void))
{
	test_set.teardown = teardown;
}
/*
	Setup test set
*/
void testset(string name, void (*config)(FILE **), void (*cleanup)(void))
{
	test_set.name = name;
	test_set.config = config;
	test_set.cleanup = cleanup;
	test_set.log_stream = stdout;
}

/*
	test runner entry point
*/
int main(int argc, string *argv)
{
	int passed_tests = 0, failed_tests = 0;

	writef("Starting test set: %s, registered %d tests\n", test_set.name ? test_set.name : "unnamed", test_count);
	fflush(stdout);

	if (test_set.config)
	{
		writef("Running set configuration\n");
		fflush(stdout);

		test_set.config(&test_set.log_stream);
	}

	if (test_count == 0)
	{
		writef("No tests registered, exiting\n");
		fflush(stdout);

		return 1;
	}

	for (int i = 0; i < test_count; ++i)
	{
		current_test = &tests[i];

		if (setjmp(jmpbuffer) == 0)
		{
			writef("Running test: %s ", tests[i].name);
			fflush(stdout);

			if (test_set.setup)
			{
				test_set.setup();
			}
			tests[i].test_func();
			if (tests[i].expect_fail && tests[i].testResult.state == PASS)
			{
				writef("Expectation failed: %s\n", EXPECT_FAIL_FAIL);
				tests[i].testResult.state = FAIL;
				if (tests[i].testResult.message)
				{
					free(tests[i].testResult.message);
				}
				tests[i].testResult.message = strdup(EXPECT_FAIL_FAIL);
			}
			else if (tests[i].expect_throw && tests[i].testResult.state == PASS)
			{
				writef("Expectation failed: %s\n", EXPECT_THROW_FAIL);
				tests[i].testResult.state = FAIL;
				if (tests[i].testResult.message)
				{
					free(tests[i].testResult.message);
				}
				tests[i].testResult.message = strdup(EXPECT_THROW_FAIL);
			}
		}
		else
		{
			if (tests[i].expect_fail)
			{
				tests[i].testResult.state = PASS;
				if (tests[i].testResult.message)
				{
					free(tests[i].testResult.message);
				}
				tests[i].testResult.message = NULL;
			}
			else if (tests[i].expect_throw)
			{
				tests[i].testResult.state = PASS;
				if (tests[i].testResult.message)
				{
					free(tests[i].testResult.message);
				}
				tests[i].testResult.message = NULL;
			}
		}
		if (test_set.teardown)
		{
			test_set.teardown();
		}

		writef("[%s]\n", tests[i].testResult.state == PASS ? "PASS" : "FAIL");
		if (tests[i].testResult.state == FAIL)
		{
			writef("    %s\n", tests[i].testResult.message);
			failed_tests++;
		}
		else
		{
			passed_tests++;
		}
		fflush(stdout);
	}

	if (test_set.cleanup)
	{
		if (test_set.log_stream && test_set.log_stream != stdout && test_set.log_stream != stderr)
		{
			fflush(test_set.log_stream);
			fclose(test_set.log_stream);
			test_set.log_stream = stdout;
		}

		writef("Running set cleanup\n");
		test_set.cleanup();
	}

	writef("Tests run: %d, Passed: %d, Failed: %d\n", test_count, passed_tests, failed_tests);

	return failed_tests == 0 ? PASS : FAIL;
}

void vfwritef(FILE *stream, const char *fmt, va_list args)
{
	stream = stream ? stream : stdout;

	vfprintf(stream, fmt, args);
	fprintf(stream, "\n");

	/* Flush the specific stream we’re writing to */
	fflush(stream);
}

void writef(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfwritef(test_set.log_stream, fmt, args);
	va_end(args);
}

void debugf(const char *fmt, ...)
{
	char msg_buffer[64];

	va_list args;
	va_start(args, fmt);
	snprintf(msg_buffer, sizeof(msg_buffer), "[DEBUG] %s", fmt);
	vfwritef(test_set.log_stream, msg_buffer, args);
	va_end(args);
}