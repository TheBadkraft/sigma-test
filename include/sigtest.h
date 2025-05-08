/*	sigtest.h
	Header for the sigma test assert interface
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

typedef void *object;
typedef char *string;

/**
 * @brief Type info enums
 */
typedef enum
{
	INT,
	FLOAT,
	DOUBLE,
	CHAR,
	STRING,
	PTR
	// Add more types as needed
} AssertType;
/**
 * @brief Test state result
 */
typedef enum
{
	PASS,
	FAIL
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
	 * @brief Asserts that two values are equal.
	 * @param expected :expected value.
	 * @param actual :actual value to compare.
	 * @param type :the value types
	 * @param fmt :format message to display if assertion fails.
	 */
	void (*areEqual)(object, object, AssertType, const string, ...);
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
} IAssert;

/**
 * @brief Global instance of the IAssert interface for use in tests
 */
extern const IAssert Assert;

/**
 * @brief Test case structure
 * @detail Encapsulates the name of the test and the test case function pointer
 */
typedef struct
{
	string name;
	void (*test_func)(void);
	int expect_fail;	/* Expect failure flag */
	int expect_throw; /* Expect throw flag */
	struct
	{
		TestState state;
		string message;
	} testResult;
} TestCase;

/**
 * @brief Test set structure for global setup and cleanup
 */
typedef struct
{
	string name;				 /* Test set name */
	void (*config)(FILE **); /* Test set config function; optional log stream */
	void (*cleanup)(void);	 /* Test set cleanup function */
	void (*setup)(void);		 /* Test case setup function */
	void (*teardown)(void);	 /* Test case teardown function */
	FILE *log_stream;			 /* Log stream for the test set */
} TestSet;

extern TestSet test_set;
extern TestCase tests[100];
extern int test_count;

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
#endif // SIGTEST_H
