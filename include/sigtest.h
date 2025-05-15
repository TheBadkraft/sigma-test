/*	sigtest.h
	Header for the sigma test assert interface

	David Boarman
	2024-09-01
*/
#ifndef SIGTEST_H
#define SIGTEST_H

#include <stdio.h>
#include <stdarg.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

struct sigtest_case_s;
struct sigtest_set_s;
struct sigtest_hooks_s;

typedef void *object;
typedef char *string;

typedef struct sigtest_case_s *TestCase;
typedef struct sigtest_set_s *TestSet;
typedef struct sigtest_hooks_s *SigtestHooks;

typedef void (*TestFunc)(void);																	// Test function pointer
typedef void (*CaseOp)(void);																		// Test case operation function pointer - setup/teardown
typedef void (*ConfigFunc)(FILE **);															// Test set config function pointer
typedef void (*CleanupFunc)(void);																// Test set cleanup function pointer
typedef void (*OutputFunc)(FILE *, const TestSet, const TestCase, const object); // Output formatting function
typedef void (*SetOp)(const TestSet, object);												// Test set operation function pointer

//	Output format types
typedef enum
{
	FORMAT_DEFAULT,
	FORMAT_JUNIT,
	FORMAT_SIMPLE
} OutputFormat;

extern TestSet test_sets; // Global test set registry

/**
 * @brief Type info enums
 */
typedef enum
{
	INT,
	FLOAT,
	DOUBLE,
	CHAR,
	PTR,
	STRING,
	// Add more types as needed
} AssertType;
/**
 * @brief Test state result
 */
typedef enum
{
	PASS,
	FAIL,
	SKIP
} TestState;

/**
 * @brief Assert interface structure with function pointers
 */
typedef struct IAssert
{
	/**
	 * @brief Asserts the given condition is TRUE
	 * @param  condition :the condition to check
	 * @param  fmt :the format message to display if assertion fails
	 */
	void (*isTrue)(int, const string, ...);
	/**
	 * @brief Asserts the given condition is FALSE
	 * @param  condition :the condition to check
	 * @param  fmt :the format message to display if assertion fails
	 */
	void (*isFalse)(int, const string, ...);
	/**
	 * @brief Asserts that a pointer is NULL
	 * @param  ptr :the pointer to check
	 * @param  fmt :the format message to display if assertion fails
	 */
	void (*isNull)(object, const string, ...);
	/**
	 * @brief Asserts that a pointer is not NULL
	 * @param  ptr :the pointer to check
	 * @param  fmt :the format message to display if assertion fails
	 */
	void (*isNotNull)(object, const string, ...);
	/**
	 * @brief Asserts that two values are equal.
	 * @param expected :expected value.
	 * @param actual :actual value to compare.
	 * @param type :the value types
	 * @param fmt :format message to display if assertion fails.
	 */
	void (*areEqual)(object, object, AssertType, const string, ...);
	/**
	 * @brief Asserts that two values are not equal.
	 * @param expected :expected value.
	 * @param actual :actual value to compare.
	 * @param type :the value types
	 * @param fmt :format message to display if assertion fails.
	 */
	void (*areNotEqual)(object, object, AssertType, const string, ...);
	/**
	 * @brief Asserts that a float value is within a specified tolerance.
	 * @param value :the float value to check.
	 * @param min :the minimum tolerance value.
	 * @param max :the maximum tolerance value.
	 * @param fmt :format message to display if assertion fails.
	 */
	void (*floatWithin)(float, float, float, const string, ...);
	/**
	 * @brief Asserts that two strings are equal.
	 * @param expected :expected string.
	 * @param actual :actual string to compare.
	 * @param case_sensitive :case sensitivity flag.
	 * @param fmt :format message to display if assertion fails.
	 */
	void (*stringEqual)(string, string, int, const string, ...);
	/**
	 * @brief Asserts that an exception is thrown during the execution of the test function.
	 * @param test_func :the test function to execute.
	 * @param fmt :format message to display if assertion fails.
	 */
	void (*expectException)(void (*test_func)(void), const string, ...);
	/**
	 * @brief Assert throws an exception
	 * @param fmt :the format message to display if assertion fails
	 */
	void (*throw)(const string, ...);
	/**
	 * @brief Fails a testcase immediately and logs the message
	 * @param fmt :the format message to display
	 */
	void (*fail)(const string, ...);
	/**
	 * @brief Skips the testcase setting the state as skipped and logs the message
	 * @param fmt :the format message to display
	 */
	void (*skip)(const string, ...);
} IAssert;

/**
 * @brief Global instance of the IAssert interface for use in tests
 */
extern const IAssert Assert;

/**
 * @brief Test case structure
 * @detail Encapsulates the name of the test and the test case function pointer
 */
typedef struct sigtest_case_s
{
	string name;
	TestFunc test_func; /* Test function pointer */
	int expect_fail;	  /* Expect failure flag */
	int expect_throw;	  /* Expect throw flag */
	struct
	{
		TestState state;
		string message;
	} test_result;
	TestCase next; /* Pointer to the next test case */
} sigtest_case_s;

/**
 * @brief Test set structure for global setup and cleanup
 */
typedef struct sigtest_set_s
{
	string name;			/* Test set name */
	CleanupFunc cleanup; /* Test set cleanup function */
	CaseOp setup;			/* Test case setup function */
	CaseOp teardown;		/* Test case teardown function */
	FILE *log_stream;		/* Log stream for the test set */
	TestCase cases;		/* Pointer to the test cases */
	TestCase tail;			/* Pointer to the last test case */
	int count;				/* Number of test cases */
	TestCase current;		/* Current test case */
	TestSet next;			/* Pointer to the next test set */
} sigtest_set_s;

/**
 * @brief Retrieve the SigmaTest version
 */
const char *
sigtest_version(void);
/**
 * @brief Registers a new test into the test array
 * @param  name :the test name
 * @param  func :the test function
 */
void testcase(string name, void (*func)(void));
/**
 * @brief Registers a testcase with fail expectation
 * @param  name :the test name
 * @param  func :the test function
 */
void fail_testcase(string name, void (*func)(void));
/**
 * @brief Registers a test case with expectation to throw
 * @param  name :the test name
 * @param  func :the test function
 */
void testcase_throws(string name, void (*func)(void));
/**
 * @brief Registers the test case setup function
 * @param  setup :the test case setup function
 */
void setup_testcase(void (*setup)(void));
/**
 * @brief Registers the test case teardown function
 * @param  teardown :the test case teardown function
 */
void teardown_testcase(void (*teardown)(void));
/**
 * @brief Registers the test set config & cleanup function
 * @param  config :the test set config function
 * @param  cleanup :the test set cleanup function
 */
void testset(string name, void (*config)(FILE **), void (*cleanup)(void));

/**
 * @brief Logging function with stream flushing
 */
void writef(const char *, ...);
/**
 * @brief Debug logging function with stream flushing
 */
void debugf(const char *, ...);

#ifdef SIGTEST_CLI
/**
 * @brief Test hooks structure
 */
typedef struct sigtest_hooks_s
{
	// Hooks for runtime extensions
	void (*before_test)(void *context);					  // Called before each test case
	void (*after_test)(void *context);					  // Called after each test case
	void (*before_set)(const TestSet, void *context); // Called before each test set
	void (*after_set)(const TestSet, void *context);  // Called after each test set
	OutputFunc format_output;								  // Output formatting function
	OutputFormat format_type;								  // Output format type
	void *context;												  // User-defined data
} sigtest_hooks_s;
#endif // SIGTEST_CLI

/**
 * @brief Initialize default hooks with the specified output format
 * @param format :the desitred output format
 * @return pointer to the initialized SigtestHooks
 */
SigtestHooks init_hooks(OutputFormat);

/**
 * @brief Registers a test set with the given name
 * @param  sets :the test sets under test
 * @param  hooks :the test runner hooks
 * @return 0 if all tests pass, 1 if any test fails
 */
int run_tests(TestSet, SigtestHooks);

#endif // SIGTEST_H
