// test/lib/math_utils.c
#include "math_utils.h"

double add(double a, double b)
{
   return a + b;
}

double subtract(double a, double b)
{
   return a - b;
}

double divide(double a, double b)
{
   return b != 0 ? a / b : 0; // Simplified error handling
}

int is_positive(double value)
{
   return value > 0;
}