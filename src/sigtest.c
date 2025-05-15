/*
	sigtest.c
	David Boarman

	David Boarman
	2024-09-01
*/
#include "sigtest.h"
#include <stdlib.h>
#include <setjmp.h>
#include <math.h>
#include <float.h>  //	for FLT_EPSILON && DBL_EPSILON
#include <string.h> // 	for jmp_buf and related functions
#include <strings.h>
#include <stdarg.h>
#include <time.h>

#define SIGTEST_VERSION "0.2.2"

// Global test set "registry"
TestSet test_sets = NULL;
static TestSet current_set = NULL;

// Static buffer for jump
static jmp_buf jmpbuffer;

//	Fail messages
#define MESSAGE_TRUE_FAIL "Expected true, but was false"
#define MESSAGE_FALSE_FAIL "Expected false, but was true"
#define MESSAGE_EQUAL_FAIL "Expected %s, but was %s"
#define EXPECT_FAIL_FAIL "Expected test to fail but it passed"
#define EXPECT_THROW_FAIL "Expected test to throw but it didn't"

//	Implementations for internal helpers
// format write message to stream
void fwritef(FILE *, const char *, ...);
// cleanup test runner
static void cleanup_test_runner(void)
{
	if (!test_sets)
		return; // Already cleaned up

	TestSet set = test_sets;
	while (set)
	{
		TestSet next = set->next;
		// free test cases
		TestCase tc = set->cases;
		while (tc)
		{
			TestCase next_tc = tc->next;
			free(tc->name);
			if (tc->test_result.message)
				free(tc->test_result.message);

			free(tc);
			tc = next_tc;
		}

		// free test set
		free(set->name);
		if (set->log_stream != stdout && set->log_stream)
		{
			fclose(set->log_stream);
			set->log_stream = NULL;
		}

		free(set);
		set = next;
	}

	// Reset the test set registry
	test_sets = NULL;
	current_set = NULL;
}
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
	if (current_set && current_set->current)
	{
		current_set->current->test_result.state = result;
		if (current_set->current->test_result.message)
		{
			free(current_set->current->test_result.message);
		}
		current_set->current->test_result.message = message ? strdup(message) : NULL;
		if (result != PASS)
		{
			// Stop assertions for this test
			longjmp(jmpbuffer, 1);
		}
	}
}

//	Implementations for CLI interface
const char *sigtest_version(void)
{
	return SIGTEST_VERSION;
}

// Implementations for assertions (public interface)
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
		debugf("Assertion failed: %s", errMessage);
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
		debugf("Assertion failed: %s", errMessage);
		set_test_context(FAIL, errMessage);
	}
	else
	{
		set_test_context(PASS, NULL);
	}

	va_end(args);
}
/*
	Asserts the pointer is NULL
*/
static void assert_is_null(object ptr, const string fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	if (ptr != NULL)
	{
		string errMessage = get_msg(fmt, "Pointer is not NULL", args);
		debugf("Assertion failed: %s", errMessage);
		set_test_context(FAIL, errMessage);
	}
	else
	{
		set_test_context(PASS, NULL);
	}

	va_end(args);
}
/*
	Asserts the pointer is not NULL
*/
static void assert_is_not_null(object ptr, const string fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	if (ptr == NULL)
	{
		string errMessage = get_msg(fmt, "Pointer is NULL", args);
		debugf("Assertion failed: %s", errMessage);
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
	case PTR:
		if (expected != actual)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
	case STRING:
		failMessage = "Use Assert.stringEqual for string comparison";
		result = FAIL;

		break;
	default:

		failMessage = "Unsupported type for comparison";
		result = FAIL;

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
	Asserts two values are not equal
*/
static void assert_are_not_equal(object expected, object actual, AssertType type, const string fmt, ...)
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
		if (*(int *)expected == *(int *)actual)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
	case FLOAT:
		if (fabs(*(float *)expected - *(float *)actual) <= FLT_EPSILON)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
	case DOUBLE:
		if (fabs(*(double *)expected - *(double *)actual) <= DBL_EPSILON)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
	case CHAR:
		if (*(char *)expected == *(char *)actual)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
	case PTR:
		if (expected == actual)
		{
			failMessage = gen_equals_fail_msg(expected, actual, type, fmt, args);
			result = FAIL;
		}

		break;
	case STRING:
		failMessage = "Use Assert.stringEqual for string comparison";
		result = FAIL;

		break;
	default:

		failMessage = "Unsupported type for comparison";
		result = FAIL;

		break; // Add cases for other types as needed
	}

	if (result == FAIL)
	{
		debugf("Assertion failed: %s", failMessage); /* Log failure */
	}

	set_test_context(result, failMessage);

	va_end(args);
}
/*
	Asserts that a float value is within a specified tolerance
*/
static void assert_float_within(float value, float min, float max, const string fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	if (value < min || value > max)
	{
		string errMessage = get_msg(fmt, "Value out of range", args);
		debugf("Assertion failed: %s", errMessage);
		set_test_context(FAIL, errMessage);
	}
	else
	{
		set_test_context(PASS, NULL);
	}

	va_end(args);
}
/*
	Asserts that two strings are equal with respect to case sensitivity
*/
static void assert_string_equal(string expected, string actual, int case_sensitive, const string fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	int equal = case_sensitive ? strcmp(expected, actual) == 0 : strcasecmp(expected, actual) == 0;
	if (!equal)
	{
		string failMessage = gen_equals_fail_msg(expected, actual, STRING, fmt, args);
		debugf("Assertion failed: %s", failMessage);
		set_test_context(FAIL, failMessage);
	}
	else
	{
		set_test_context(PASS, NULL);
	}

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
	writef("Throw triggered: %s", err_message);
	set_test_context(FAIL, err_message);

	va_end(args);
}
/*
	Assert fail
*/
static void assert_fail(const string fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	string errMessage = get_msg(fmt, "Explicit failure triggered", args);
	debugf("Assertion failed: %s", errMessage);
	set_test_context(FAIL, errMessage);

	va_end(args);
}
/*
	Assert skip
*/
static void assert_skip(const string fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	string errMessage = get_msg(fmt, "Testcase skipped", args);
	debugf("Control Triggered: %s", errMessage);
	set_test_context(SKIP, errMessage);

	va_end(args);
}

/*
	IAssert interface
*/
const IAssert Assert = {
	 .isTrue = assert_is_true,
	 .isFalse = assert_is_false,
	 .isNull = assert_is_null,
	 .isNotNull = assert_is_not_null,
	 .areEqual = assert_are_equal,
	 .areNotEqual = assert_are_not_equal,
	 .floatWithin = assert_float_within,
	 .stringEqual = assert_string_equal,
	 .throw = assert_throw,
	 .fail = assert_fail,
	 .skip = assert_skip,
};

/*
	Register test set
*/
void testset(string name, ConfigFunc config, CleanupFunc cleanup)
{
	// ensure cleanup_test_runner is registered only once
	static int atexit_registered = 0;
	if (!atexit_registered)
	{
		if (atexit(cleanup_test_runner) != 0)
		{
			writef("Failed to register `cleanup_test_runner` with atexit\n");
			exit(EXIT_FAILURE);
		}
		atexit_registered = 1;
	}

	TestSet set = malloc(sizeof(struct sigtest_set_s));
	if (!set)
	{
		writef("Failed to allocate memory for test set\n");
		exit(EXIT_FAILURE);
	}

	set->name = strdup(name);
	set->cleanup = cleanup;
	set->setup = NULL;
	set->teardown = NULL;
	set->log_stream = stdout;
	set->cases = NULL;
	set->tail = NULL;
	set->count = 0;
	set->current = NULL;
	set->next = test_sets;

	// Execute config immediately if provided
	if (config)
	{
		writef("Configuring test set: %s", name);
		config(&set->log_stream);
		if (!set->log_stream)
		{
			set->log_stream = stdout; // Fallback to stdout if config fails
		}
	}
	// Handle allocation failure after config
	if (!set->name)
	{
		if (set->log_stream != stdout && set->log_stream)
			fclose(set->log_stream);
		free(set);
		writef("Failed to allocate memory for test set name\n");
		exit(EXIT_FAILURE);
	}

	test_sets = set;
	current_set = set;
}
/*
	Register test to test registry
*/
void testcase(string name, TestFunc func)
{
	if (!current_set)
	{
		testset("default", NULL, NULL);
	}

	TestCase tc = malloc(sizeof(struct sigtest_case_s));
	if (!tc)
	{
		writef("Failed to allocate memory for test case `%s`\n", name);
		exit(EXIT_FAILURE);
	}
	tc->name = strdup(name);
	if (!tc->name)
	{
		writef("Failed to allocate memory for test case name `%s`\n", name);
		exit(EXIT_FAILURE); // cleanup_test_sets will handle freeing
	}
	tc->test_func = func;
	tc->expect_fail = FALSE;
	tc->expect_throw = FALSE;
	tc->test_result.state = PASS;
	tc->test_result.message = NULL;
	tc->next = NULL;

	if (!current_set->cases)
	{
		current_set->cases = tc;
		current_set->tail = tc;
	}
	else
	{
		current_set->tail->next = tc;
		current_set->tail = tc;
	}

	current_set->count++;
}
/*
	Register test to test registry with expectation to fail
*/
void fail_testcase(string name, void (*func)(void))
{
	if (!current_set)
	{
		testset("default", NULL, NULL);
	}

	TestCase tc = malloc(sizeof(struct sigtest_case_s));
	if (!tc)
	{
		writef("Failed to allocate memory for test case `%s`\n", name);
		exit(EXIT_FAILURE);
	}
	tc->name = strdup(name);
	if (!tc->name)
	{
		writef("Failed to allocate memory for test case name `%s`\n", name);
		exit(EXIT_FAILURE); // cleanup_test_sets will handle freeing
	}
	tc->test_func = func;
	tc->expect_fail = TRUE;
	tc->expect_throw = FALSE;
	tc->test_result.state = PASS; // Set to PASS initially, evaluated in main
	tc->test_result.message = NULL;
	tc->next = NULL;

	if (!current_set->cases)
	{
		current_set->cases = tc;
		current_set->tail = tc;
	}
	else
	{
		current_set->tail->next = tc;
		current_set->tail = tc;
	}

	current_set->count++;
}
/*
	Register test to test registry with expectation to throw
*/
void testcase_throws(string name, void (*func)(void))
{
	if (!current_set)
	{
		testset("default", NULL, NULL);
	}

	TestCase tc = malloc(sizeof(struct sigtest_case_s));
	if (!tc)
	{
		writef("Failed to allocate memory for test case `%s`\n", name);
		exit(EXIT_FAILURE);
	}
	tc->name = strdup(name);
	if (!tc->name)
	{
		writef("Failed to allocate memory for test case name `%s`\n", name);
		exit(EXIT_FAILURE); // cleanup_test_sets will handle freeing
	}
	tc->test_func = func;
	tc->expect_fail = FALSE;
	tc->expect_throw = TRUE;
	tc->test_result.state = PASS; // Set to PASS initially, evaluated in main
	tc->test_result.message = NULL;
	tc->next = NULL;

	if (!current_set->cases)
	{
		current_set->cases = tc;
		current_set->tail = tc;
	}
	else
	{
		current_set->tail->next = tc;
		current_set->tail = tc;
	}

	current_set->count++;
}
/*
	Setup test case
*/
void setup_testcase(CaseOp setup)
{
	if (current_set)
	{
		current_set->setup = setup;
	}
}
/*
	Teardown test case
*/
void teardown_testcase(CaseOp teardown)
{
	if (current_set)
	{
		current_set->teardown = teardown;
	}
}

#ifdef SIGTEST_TEST
/*
	test executor entry point
*/
int main(void)
{
	int retResult = run_tests(test_sets, NULL);
	cleanup_test_runner();

	return retResult;
}
#endif // SIGTEST_TEST

// the actual test runner
int run_tests(TestSet sets, SigtestHooks hooks)
{
	int total_tests = 0, passed_tests = 0, failed_tests = 0, skipped_tests = 0;
	int set_sequence = 1;

	// Log total registered test sets for debugging
	int total_sets = 0;
	for (TestSet set = sets; set; set = set->next)
		total_sets++;
	fwritef(stdout, "Total test sets registered: %d", total_sets);

	for (TestSet set = sets; set; set = set->next, set_sequence++)
	{
		int tc_total = 0, tc_passed = 0, tc_failed = 0, tc_skipped = 0;
		if (!set->log_stream)
			set->log_stream = stdout;

		// Set current_set to the executing set for writef/debugf
		current_set = set;

		fwritef(set->log_stream, "TestSet %d: %s, registered %d tests", set_sequence, set->name, set->count);
		fwritef(set->log_stream, "=======================================================");
		for (TestCase tc = set->cases; tc; tc = tc->next)
		{
			set->current = tc; // Set current test for set_test_context
			if (set->setup)
			{
				fwritef(set->log_stream, "Running setup");
				set->setup();
			}
			fwritef(set->log_stream, "Running test: %s ", tc->name);

			if (setjmp(jmpbuffer) == 0)
			{
				tc->test_func();
			}

			if (set->teardown)
			{
				fwritef(set->log_stream, "Running teardown");
				set->teardown();
			}

			// Handle expect_fail and expect_throw
			if (tc->expect_fail)
			{
				if (tc->test_result.state == FAIL)
				{
					tc->test_result.state = PASS;
					if (tc->test_result.message)
					{
						free(tc->test_result.message);
						tc->test_result.message = strdup("Expected failure occurred");
					}
				}
				else if (tc->test_result.state != SKIP)
				{
					tc->test_result.state = FAIL;
					if (tc->test_result.message)
						free(tc->test_result.message);
					tc->test_result.message = strdup("Expected failure but passed");
				}
			}
			else if (tc->expect_throw)
			{
				if (tc->test_result.state == FAIL)
				{
					tc->test_result.state = PASS;
					if (tc->test_result.message)
					{
						free(tc->test_result.message);
						tc->test_result.message = strdup("Expected throw occurred");
					}
				}
				else if (tc->test_result.state != SKIP)
				{
					tc->test_result.state = FAIL;
					if (tc->test_result.message)
						free(tc->test_result.message);
					tc->test_result.message = strdup("Expected throw but passed");
				}
			}

			if (tc->test_result.state == PASS)
			{
				fwritef(set->log_stream, "[PASS]");
				tc_passed++;
				passed_tests++;
			}
			else if (tc->test_result.state == SKIP)
			{
				fwritef(set->log_stream, "[SKIP]");
				tc_skipped++;
				skipped_tests++;
			}
			else
			{
				fwritef(set->log_stream, "[FAIL] %s", tc->test_result.message ? tc->test_result.message : "");
				tc_failed++;
				failed_tests++;
			}
			tc_total++;
			total_tests++;
			set->current = NULL;
		}

		if (set->cleanup)
		{
			fwritef(set->log_stream, "Running set cleanup");
			set->cleanup();
		}
		fwritef(set->log_stream, "=======================================================");
		fwritef(set->log_stream, "TestSet %d: %s, %d tests run, %d passed, %d failed, %d skipped",
				  set_sequence, set->name, tc_total, tc_passed, tc_failed, tc_skipped);
	}

	// Final output to stdout
	fwritef(stdout, "=======================================================");
	fwritef(stdout, "Tests run: %d, Passed: %d, Failed: %d, Skipped: %d\n",
			  total_tests, passed_tests, failed_tests, skipped_tests);

	return failed_tests > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

/*
	Helper function to write formatted output to the log stream
*/
// This function is used internally to write formatted messages to the given stream
void vfwritef(FILE *stream, const char *fmt, va_list args)
{
	stream = stream ? stream : stdout;

	vfprintf(stream, fmt, args);
	fprintf(stream, "\n");

	/* Flush the specific stream we’re writing to */
	fflush(stream);
}
// This function is used to write formatted messages to the given stream
void fwritef(FILE *stream, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	vfwritef(stream, fmt, args);

	va_end(args);
}
// This function is used to write formatted messages to the log stream
void writef(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vfwritef(current_set ? current_set->log_stream : stdout, fmt, args);
	va_end(args);
}
// This function is used to write formatted debug messages to the log stream
void debugf(const char *fmt, ...)
{
	char msg_buffer[64];

	va_list args;
	va_start(args, fmt);
	snprintf(msg_buffer, sizeof(msg_buffer), "[DEBUG] %s", fmt);
	vfwritef(current_set ? current_set->log_stream : stdout, msg_buffer, args);
	va_end(args);
}