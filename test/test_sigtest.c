/*	sigtest_test.c
	David Boarman

	Tests the sigma test library and test runner
*/
#include "sigtest.h"

/**
 *	@brief	tests isTrue assertion
 */
void test_True(void)
{
	Assert.isTrue(1 == 1, "1 should equal 1");
	Assert.isTrue(0 == 0, "0 should equal 0");
}
/**
 *	@brief	tests isFalse assertion
 */
void test_False(void)
{
	Assert.isFalse(1 == 0, "1 should not equal 0");
}
/**
 *	@brief	tests areEqual assertion
 */
void test_Equals(void)
{
	int exp = 5;
	int act = 5;

	Assert.areEqual(&exp, &act, INT, "5 should equal 5");
}
/**
 *	@brief	equals fails on a float v int comparison
 */
void test_EqualsFail(void)
{
	float exp = 3.14528;
	float act = 3.0f;

	Assert.areEqual(&exp, &act, FLOAT, "%.5f is not equal to %.5f", exp, act);
}
/**
 *	@brief	equals fails on a float comparison
 */
void test_EqualsFloatsFail(void)
{
	float exp = 3.14528;
	float act = 3.5;

	Assert.areEqual(&exp, &act, FLOAT, "%.5f is not equal to %.5f", exp, act);
}
/**
 *	@brief	tests char equals
 */
void test_charEquals(void)
{
	char exp = 'a';
	char act = 'a';

	Assert.areEqual(&exp, &act, CHAR, NULL);
}
/**
 *	@brief	tests string equals
 */
void test_stringEquals(void)
{
	string exp = "foo";
	string act = "foo";

	Assert.areEqual(&exp, &act, STRING, NULL);
}
/**
 *	@brief	tests strings equal fails
 */
void test_stringEqualsFail(void)
{
	string exp = "foo";
	string act = "bar";

	Assert.areEqual(exp, act, STRING, "'%s' is not equal to '%s'", exp, act);
}

void test_pointersEqual(void)
{
	int value = 42;
	int *exp = &value;
	int *act = &value;

	Assert.areEqual(exp, act, PTR, NULL);
}

void test_pointersNotEqual(void)
{
	int val1 = 42;
	int val2 = 42;
	int *exp = &val1;
	int *act = &val2;

	Assert.areEqual(exp, act, PTR, "Pointers should not be equal");
}

//	register test cases
__attribute__((constructor)) void init_sigtest_tests(void)
{
	testcase("assertTrue", test_True);
	testcase("assertFalse", test_False);
	testcase("assertEquals", test_Equals);
	testcase("equalsFail", test_EqualsFail);
	testcase("equalsFloatsFail", test_EqualsFloatsFail);
	testcase("charEquals", test_charEquals);
	testcase("stringEquals", test_stringEquals);
	testcase("stringEqualsFail", test_stringEqualsFail);
	testcase("pointersEqual", test_pointersEqual);
	testcase("pointersNotEqual", test_pointersNotEqual);
}
