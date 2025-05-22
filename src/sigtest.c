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
#include <stdio.h>
#include <assert.h>

#define SIGTEST_VERSION "0.03.01"

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
// For dynamic test state annotation
const char *TEST_STATES[] = {
	 "PASS",
	 "FAIL",
	 "SKIP",
	 NULL,
};
// For dynamic log level annotation
static const char *DBG_LEVELS[] = {
	 "DEBUG",
	 "INFO",
	 "WARNING",
	 "ERROR",
	 "FATAL",
	 NULL,
};
//	system clock structures
#define SYS_CLOCK SYS_clock_gettime
#define CLOCK_MONOTONIC 1
int sys_gettime(ts_time *ts)
{
	return clock_gettime(CLOCK_MONOTONIC, ts);
}
double get_elapsed_ms(ts_time *start, ts_time *end)
{
	return ((double)(end->tv_sec - start->tv_sec) * 1000.0) + ((double)(end->tv_nsec - start->tv_nsec) / 1000000.0);
}
//	internal logger declarations
static void log_message(const char *, ...);
static void log_debug(DebugLevel, const char *, ...);
// hooks registry
static HookRegistry *hook_registry = NULL;

//	Implementations for internal helpers
/**
 * Formats the current time into a buffer using the specified format
 */
void get_timestamp(char *buffer, const char *format)
{
	time_t now = time(NULL);
	strftime(buffer, 32, format, localtime(&now));
}
// format write message to stream
void fwritef(FILE *, const char *, ...);
// Initialize hooks with the given name/label
SigtestHooks init_hooks(const char *name)
{
	// Check if the name is NULL or empty
	if (!name || !*name)
	{
		fwritelnf(stderr, "Error: Hook name cannot be NULL or empty");
		return NULL; // Invalid name
	}
	//	look first for named hooks in registry
	for (HookRegistry *entry = hook_registry; entry; entry = entry->next)
	{
		if (entry->hooks->name && strcmp(entry->hooks->name, name) == 0)
		{
			return entry->hooks;
		}
	}
	// or create a new one
	SigtestHooks hooks = malloc(sizeof(struct sigtest_hooks_s));
	if (!hooks)
	{
		fwritelnf(stderr, "Error: Failed to allocate memory for hooks");
		return NULL; // Memory allocation failed
	}
	hooks->name = strdup(name);
	if (!hooks->name)
	{
		fwritelnf(stderr, "Error: Failed to duplicate hook name");
		free(hooks);
		return NULL; // Memory allocation failed
	}
	*hooks = (struct sigtest_hooks_s){
		 .name = strdup(name),
		 .before_set = NULL,
		 .after_set = NULL,
		 .before_test = NULL,
		 .after_test = NULL,
		 .on_start_test = NULL,
		 .on_end_test = NULL,
		 .on_error = NULL,
		 .on_test_result = NULL,
		 .context = NULL,
	};

	return hooks;
}
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
	if (current_set && current_set->logger)
		free(current_set->logger);

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
static string format_message(const string fmt, const string defaultMessage, va_list args)
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
		string errMessage = format_message(fmt, MESSAGE_TRUE_FAIL, args);
		// debugf("Assertion failed: %s", errMessage);
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
		string errMessage = format_message(fmt, MESSAGE_TRUE_FAIL, args);
		// debugf("Assertion failed: %s", errMessage);
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
		string errMessage = format_message(fmt, "Pointer is not NULL", args);
		// debugf("Assertion failed: %s", errMessage);
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
		string errMessage = format_message(fmt, "Pointer is NULL", args);
		// debugf("Assertion failed: %s", errMessage);
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

	// if (result == FAIL)
	// {
	// 	debugf("Assertion failed: %s", failMessage); /* Log failure */
	// }

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

	// if (result == FAIL)
	// {
	// 	debugf("Assertion failed: %s", failMessage); /* Log failure */
	// }

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
		string errMessage = format_message(fmt, "Value out of range", args);
		// debugf("Assertion failed: %s", errMessage);
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
		// debugf("Assertion failed: %s", failMessage);
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

	string err_message = format_message(fmt, "Explicit throw triggered", args);
	// writef("Throw triggered: %s", err_message);
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

	string errMessage = format_message(fmt, "Explicit failure triggered", args);
	// debugf("Assertion failed: %s", errMessage);
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

	string errMessage = format_message(fmt, "Testcase skipped", args);
	// debugf("Control Triggered: %s", errMessage);
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
			fwritelnf(stdout, "Failed to register `cleanup_test_runner` with atexit\n");
			exit(EXIT_FAILURE);
		}
		atexit_registered = 1;
	}

	TestSet set = malloc(sizeof(struct sigtest_set_s));
	if (!set)
	{
		fwritelnf(stdout, "Failed to allocate memory for test set\n");
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
	set->passed = 0;
	set->failed = 0;
	set->skipped = 0;
	set->current = NULL;
	set->next = test_sets;
	set->logger = malloc(sizeof(struct sigtest_logger_s));
	if (!set->logger)
	{
		fwritelnf(stdout, "Failed to allocate memory for test set logger\n");
		free(set);
		exit(EXIT_FAILURE);
	}

	// Execute config immediately if provided
	if (config)
	{
		char timestamp[32];
		get_timestamp(timestamp, "%Y-%m-%d  %H:%M:%S");
		writelnf("[%s]   Test Set: %30s", timestamp, name);

		config(&set->log_stream);
		if (!set->log_stream)
		{
			set->log_stream = stdout; // Fallback to stdout if config fails
		}
		set->logger->log = log_message;
		set->logger->debug = log_debug;
	}
	// Handle allocation failure after config
	if (!set->name)
	{
		if (set->log_stream != stdout && set->log_stream)
			fclose(set->log_stream);
		free(set);
		writelnf("Failed to allocate memory for test set name\n");
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
		free(tc);

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
		free(tc);

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
		free(tc);

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
/*
	Register test hooks
*/
void register_hooks(SigtestHooks hooks)
{
	HookRegistry *entry = malloc(sizeof(HookRegistry));
	if (!entry)
	{
		fwritelnf(stderr, "Error: Failed to allocate hook registry entry");
		return; // Memory allocation failed
	}

	entry->hooks = hooks;
	entry->next = hook_registry;
	hook_registry = entry;

	if (current_set && !current_set->hooks)
	{
		current_set->hooks = hooks;
	}
}

//	default test hooks
static void default_before_test(object context)
{
	struct
	{
		int count;
		int verbose;
		ts_time start;
		ts_time end;
	} *ctx = context;

	ctx->count++;
}
static void default_on_start_test(object context)
{
	struct
	{
		int count;
		int verbose;
		ts_time start;
		ts_time end;
	} *ctx = context;

	if (sys_gettime(&ctx->start) == -1)
	{
		fwritelnf(stderr, "Error: Failed to get system start time");
		exit(EXIT_FAILURE);
	}
	// zero out the end time
	ctx->end = (ts_time){0, 0};

	if (ctx->verbose && current_set)
	{
		current_set->logger->log("Starting test: %s\n", current_set->current->name);
	}
}
static void default_on_end_test(object context)
{
	struct
	{
		int count;
		int verbose;
		ts_time start;
		ts_time end;
	} *ctx = context;

	if (sys_gettime(&ctx->end) == -1)
	{
		fwritelnf(stderr, "Error: Failed to get system end time");
		exit(EXIT_FAILURE);
	}
	if (ctx->verbose && current_set)
	{
		current_set->logger->log("Finished test: %s\n", current_set->current->name);
	}
}
static void default_after_test(object context)
{
	struct
	{
		int count;
		int verbose;
		ts_time start;
		ts_time end;
	} *ctx = context;

	ctx->count--;
}
static void default_on_test_result(const TestSet set, const TestCase tc, object context)
{
	struct
	{
		int count;
		int verbose;
		ts_time start;
		ts_time end;
	} *ctx = context;

	const char *status = TEST_STATES[tc->test_result.state];
	//	if we have a zero end time, we need to set it
	if (ctx->end.tv_sec == 0 && ctx->end.tv_nsec == 0)
	{
		if (sys_gettime(&ctx->end) == -1)
		{
			fwritelnf(stderr, "Error: Failed to get system end time");
			exit(EXIT_FAILURE);
		}
	}
	// calculate elapsed time
	double elapsed_ms = get_elapsed_ms(&ctx->start, &ctx->end);
	// Log duration: show "< 0.0001 ms" if negative or too small
	if (elapsed_ms < 0.0001)
	{
		set->logger->log("Running: %-36s  < 0.1 us  [%s]\n", tc->name, status);
	}
	else
	{
		set->logger->log("Running: %-37s  %6.3f us  [%s]\n", tc->name, elapsed_ms * 1000.0, status);
	}

	if (ctx->verbose && tc->test_result.message)
	{
		DebugLevel level = (tc->test_result.state == PASS) ? DBG_INFO : DBG_DEBUG;
		set->logger->debug(level, "\tmessage= %s\n", tc->test_result.message ? tc->test_result.message : "NULL");
	}
	if (ctx->verbose)
	{
		set->logger->debug(DBG_DEBUG, "\tstart= %ld.%04ld", ctx->start.tv_sec, ctx->start.tv_nsec);
		set->logger->log("\tend=   %ld.%04ld\n", ctx->end.tv_sec, ctx->end.tv_nsec);
	}
}
static void default_on_error(const char *message, object context)
{
	struct
	{
		int count;
		int verbose;
		ts_time start;
		ts_time end;
	} *ctx = context;

	if (ctx->verbose && current_set)
	{
		current_set->logger->log("Error in test [%s]: %s\n", current_set->current->name, message);
	}
}

static struct
{
	int count;
	int verbose;
	ts_time start;
	ts_time end;
} default_ctx = {0, 0, {0, 0}, {0, 0}};
static const sigtest_hooks_s default_hooks = {
	 .name = "default",
	 .before_set = NULL,
	 .after_set = NULL,
	 .before_test = default_before_test,
	 .on_start_test = default_on_start_test,
	 .on_end_test = default_on_end_test,
	 .after_test = default_after_test,
	 .on_error = default_on_error,
	 .on_test_result = default_on_test_result,
	 .context = &default_ctx};
//	 initialize on start up
__attribute__((constructor)) static void init_default_hooks(void)
{
	HookRegistry *entry = malloc(sizeof(HookRegistry));
	// if we don't have a valid hooks registry, we exit
	if (!entry)
	{
		fwritelnf(stderr, "Error: Failed to allocate hooks registry entry");
		exit(EXIT_FAILURE);
	}

	entry->hooks = (SigtestHooks)&default_hooks;
	entry->next = hook_registry;
	hook_registry = entry;
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
int run_tests(TestSet sets, SigtestHooks test_hooks)
{
	int total_tests = 0;
	int set_sequence = 1;
	char timestamp[32];

	// Log total registered test sets for debugging
	int total_sets = 0;
	SigtestHooks hooks = NULL;
	for (TestSet set = sets; set; set = set->next)
	{
		/*
			We need to check if we have a test_hooks set; if not we need to use the default hooks.
			If we have a set->hooks, and a test_hooks, we need to prioritize the test_hooks.

			CLI options:
			  `-s`: simple mode, no hooks will be provided by the test runner; however, the test
					  set could register hooks and those will be used.
				default (no flag): the test runner can provide hooks intended to override the test
					  set hooks. If NULL is passed, then the default hooks will be used.
		 */
		if (!test_hooks && !set->hooks)
		{
			//	set the default hooks
			hooks = hook_registry->hooks;
		}
		else if (test_hooks)
		{
			hooks = test_hooks;
		}
		else
		{
			hooks = set->hooks;
		}

		total_sets++;
	}
	if (total_sets == 0)
	{
		return 0;
	}

	for (TestSet set = sets; set; set = set->next, set_sequence++)
	{
		int tc_total = 0, tc_passed = 0, tc_failed = 0, tc_skipped = 0;
		if (!set->log_stream || !set->logger)
		{
			set->log_stream = stdout;
		}
		// Set current_set to the executing set for writef/debugf
		current_set = set;

		// Call before_set hook if defined
		if (hooks && hooks->before_set)
		{
			hooks->before_set(set, hooks->context);
		}
		else
		{
			get_timestamp(timestamp, "%Y-%m-%d  %H:%M:%S");
			fwritelnf(set->log_stream, "[%d] %-25s:%4d %-10s%s",
						 set_sequence, set->name, set->count, ":", timestamp);
			fwritelnf(set->log_stream, "=================================================================");
		}

		for (TestCase tc = set->cases; tc; tc = tc->next)
		{
			set->current = tc; // Set current test for set_test_context
			//	before test case setup
			if (hooks && hooks->before_test)
			{
				hooks->before_test(hooks->context);
			}
			//	test case setup
			if (set->setup)
			{
				set->setup();
			}
			// on start test handler
			if (hooks && hooks->on_start_test)
			{
				hooks->on_start_test(hooks->context);
			}
			//	test case execution
			if (setjmp(jmpbuffer) == 0)
			{
				tc->test_func();
			}
			else
			{
				// Longjmp triggered by an assertion failure (FAIL, SKIP, etc.)
				/*
					We can add a custom handler for `on_exception`, `on_fail`, `on_skip`
				 */
			}
			// on end test handler
			if (hooks && hooks->on_end_test)
			{
				hooks->on_end_test(hooks->context);
			}
			//	test case teardown
			if (set->teardown)
			{
				set->logger->log("Running teardown");
				set->teardown();
			}
			//	after test case teardown
			if (hooks && hooks->after_test)
			{
				hooks->after_test(hooks->context);
			}

			// process test result
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
			//	process test result
			if (tc->test_result.state == PASS)
			{
				if (hooks && hooks->on_test_result)
				{
					hooks->on_test_result(set, tc, hooks->context);
				}
				else
				{
					set->logger->log("[PASS]\n");
				}
				tc_passed++;
				set->passed++;
			}
			else if (tc->test_result.state == SKIP)
			{
				if (hooks && hooks->on_test_result)
				{
					hooks->on_test_result(set, tc, hooks->context);
				}
				else
				{
					set->logger->log("[SKIP]\n");
				}
				tc_skipped++;
				set->skipped++;
			}
			else
			{
				if (hooks && hooks->on_test_result)
				{
					hooks->on_test_result(set, tc, hooks->context);
				}
				else
				{
					set->logger->log("[FAIL]\n     %s", tc->test_result.message ? tc->test_result.message : "Unknown");
				}
				tc_failed++;
				set->failed++;
			}
			tc_total++;
			total_tests++;
			set->current = NULL;
		}
		// Call after_set hook if defined
		if (hooks && hooks->after_set)
		{
			hooks->after_set(set, hooks->context);
		}
		else
		{
			fwritelnf(set->log_stream, "=================================================================");
			fwritelnf(set->log_stream, "[%d]     TESTS=%3d        PASS=%3d        FAIL=%3d        SKIP=%3d",
						 set_sequence, tc_total, tc_passed, tc_failed, tc_skipped);
		}

		if (set->cleanup)
		{
			set->cleanup();
		}
	}

	// Final output to stdout
	fwritelnf(stdout, "=================================================================");
	fwritelnf(stdout, "Tests run: %d, Passed: %d, Failed: %d, Skipped: %d",
				 total_tests, current_set->passed, current_set->failed, current_set->skipped);
	fwritelnf(stdout, "Total test sets registered: %d", total_sets);

	return current_set->failed > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

/*
	Helper function to write formatted output to the log stream
*/
static void log_message(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	FILE *stream = current_set && current_set->log_stream ? current_set->log_stream : stdout;
	vfprintf(stream, fmt, args);
	fflush(stream);

	va_end(args);
}
static void log_debug(DebugLevel level, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	FILE *stream = current_set && current_set->log_stream ? current_set->log_stream : stdout;
	fprintf(stream, "[%s] ", DBG_LEVELS[level]);
	vfprintf(stream, fmt, args);
	fflush(stream);

	va_end(args);
}
// This function is used to write formatted messages to the log stream
void writef(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	FILE *stream = (current_set && current_set->log_stream) ? current_set->log_stream : stdout;
	vfprintf(stream, fmt, args);
	fflush(stream);

	va_end(args);
}
// This function is used to write formatted messages with a newline to the log stream
void writelnf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	FILE *stream = (current_set && current_set->log_stream) ? current_set->log_stream : stdout;
	vfprintf(stream, fmt, args);
	fprintf(stream, "\n");
	fflush(stream);

	va_end(args);
}
// This function is used to write formatted messages to the given stream
void fwritef(FILE *stream, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	stream = stream ? stream : stdout;
	vfprintf(stream, fmt, args);
	fflush(stream);

	va_end(args);
}
// This function is used to write formatted messages with a newline to the given stream
void fwritelnf(FILE *stream, const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	stream = stream ? stream : stdout;
	vfprintf(stream, fmt, args);
	fprintf(stream, "\n");
	fflush(stream);

	va_end(args);
}