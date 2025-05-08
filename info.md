# **Why Choose Sigma Test Over Other C Testing Frameworks?**  

When it comes to unit testing in C, developers have several established options—**Check**, **Unity**, **Google Test (with C support)**, and **CppUTest**, to name a few. Yet, **Sigma Test** offers a uniquely streamlined, self-contained, and developer-friendly approach that makes it a compelling alternative.  

Here’s why Sigma Test stands out:  

---

## **1. Minimalist Design, Maximum Flexibility**  
Many C testing frameworks require complex setup, external dependencies, or cumbersome build systems. Sigma Test is **a single-header, single-source framework** that compiles directly into your project without requiring:  
- **Python scripts** (like Check)  
- **External test runners** (like Google Test)  
- **C++ compatibility layers** (like CppUTest)  

### **Key Advantages:**  
✅ **No external dependencies** – Just `sigtest.h` and `sigtest.c`.  
✅ **No build system hacks** – Works with any standard C compiler.  
✅ **No mandatory test discovery** – Tests register themselves via `__attribute__((constructor))`.  

---

## **2. Self-Testing Framework (A Rare Feature)**  
Unlike many frameworks that rely on manual verification, **Sigma Test tests itself**—proving its reliability in real-world use.  

### **Why This Matters:**  
🔹 **No hidden bugs in test infrastructure** – If the framework fails, it fails visibly.  
🔹 **Confidence in assertions** – The assertions you use are the same ones the framework uses to validate itself.  
🔹 **Dogfooding at its best** – If it’s good enough for testing itself, it’s good enough for your code.  

---

## **3. Exception Handling Without C++ or setjmp/longjmp Boilerplate**  
C lacks native exceptions, forcing developers to use:  
- **Error codes** (clutters function signatures)  
- **setjmp/longjmp hacks** (risky if not handled properly)  
- **Abort/assert** (terminates tests prematurely)  

### **Sigma Test Solves This With:**  
🚀 **Built-in test-scoped exception handling** via `longjmp` (but safely abstracted away).  
🚀 **`Assert.throw()`** for explicitly failing tests with messages.  
🚀 **No silent crashes** – Tests cleanly report failures instead of aborting.  

**Example:**  
```c
void test_divide_by_zero() {
    float result = divide(1.0f, 0.0f);
    if (result == 0.0f) {
        Assert.throw("Division by zero should be caught!");
    }
}
```

---

## **4. No Magic Macros – Clean, Explicit Assertions**  
Many frameworks rely on **macro-heavy syntax** (e.g., `TEST_CASE`, `CHECK_EQUAL`), which can:  
- **Obfuscate failures** (poor error messages)  
- **Break IDE tooling** (confusing autocomplete)  
- **Introduce hidden behavior** (unexpected side effects)  

### **Sigma Test’s Approach:**  
🔹 **Function-pointer-based assertions** (`Assert.areEqual()`, `Assert.isTrue()`)  
🔹 **Type-safe comparisons** (no `void*` abuse)  
🔹 **Clear failure messages** (with optional formatting)  

**Comparison:**  
| Framework       | Assertion Style | Type Safety | Custom Messages |  
|----------------|----------------|------------|----------------|  
| **Sigma Test** | `Assert.areEqual(&exp, &act, INT, "msg")` | ✅ Yes | ✅ Yes |  
| **Unity**      | `TEST_ASSERT_EQUAL_INT(exp, act)` | ✅ Yes | ❌ No |  
| **Check**      | `ck_assert_int_eq(exp, act)` | ✅ Yes | ❌ No |  

---

## **5. Built-in Test Lifecycle Management**  
Most C testing frameworks require **manual test registration** or **macro-based registration**, leading to:  
- **Boilerplate code**  
- **Fragile test discovery**  
- **No easy setup/teardown control**  

### **Sigma Test’s Solution:**  
🔹 **Automatic test registration** via `__attribute__((constructor))`.  
🔹 **Per-test setup/teardown** (`setup_testcase()`, `teardown_testcase()`).  
🔹 **Global test set configuration** (`testset()` for logging/cleanup).  

**Example:**  
```c
void setup() { printf("Before test\n"); }
void teardown() { printf("After test\n"); }

__attribute__((constructor)) 
void register_tests() {
    setup_testcase(setup);
    teardown_testcase(teardown);
    testcase("my_test", test_function);
}
```

---

## **6. First-Class Logging & Debugging Support**  
Many frameworks dump all output to `stdout`, making it hard to:  
- **Separate test logs from application logs**  
- **Debug failing tests**  
- **Save test reports**  

### **Sigma Test’s Logging Features:**  
📜 **Custom log streams** (redirect to files, `stderr`, etc.).  
📜 **Structured debug output** (`debugf()` for extra context).  
📜 **No mixing with application logs** (clean separation).  

**Example:**  
```c
void config(FILE **log) {
    *log = fopen("test.log", "w"); // Log to file
}

testset("MySuite", config, NULL);
```

---

## **7. Expected Failures & Expected Exceptions**  
Most frameworks **fail loudly** when a test crashes, but sometimes you **want a test to fail** (e.g., testing error handling).  

### **Sigma Test’s Solution:**  
🔹 **`fail_testcase()`** – Marks a test as **expected to fail**.  
🔹 **`testcase_throws()`** – Marks a test as **expected to throw**.  

**Example:**  
```c
void test_that_should_fail() {
    Assert.isTrue(0, "This should fail");
}

__attribute__((constructor)) 
void register_failing_test() {
    fail_testcase("expected_failure", test_that_should_fail);
}
```

---

## **8. No Artificial Limits (Unlike Some Frameworks)**  
Some C testing frameworks impose arbitrary restrictions like:  
- **Fixed-size test registries** (e.g., Unity’s `RUN_TEST` limit)  
- **No dynamic test generation**  
- **No runtime test filtering**  

### **Sigma Test’s Approach:**  
🔹 **Dynamic test array** (no hardcoded limits).  
🔹 **No macro-based registration** (tests are plain functions).  
🔹 **No hidden global state** (clean test isolation).  

---

## **Conclusion: When Should You Use Sigma Test?**  

### **✅ Best For:**  
✔ **Embedded systems** (no dependencies, no C++).  
✔ **Legacy C codebases** (easy integration).  
✔ **Developers who hate macro magic** (explicit assertions).  
✔ **Self-validating frameworks** (trust your test framework).  

### **❌ Not Ideal For:**  
✖ **C++ projects** (use Google Test instead).  
✖ **Massive test suites** (no parallel execution yet).  

### **Final Verdict:**  
If you want a **lightweight, self-testing, macro-free, and dependency-free** C testing framework, **Sigma Test is a perfect fit**. It’s **simple enough for small projects** but **powerful enough for serious testing**.  

**Give it a try—your C tests will thank you.** 🚀
