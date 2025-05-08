# Build and Test Procedures for `sigtest`

## Prerequisites

- Ensure you have `gcc` and `make` installed on your system.

## Makefile Targets

### Building the Shared Library

- **Command:**
  make lib

- **Description:** This target builds the `libsigtest.so` shared library. It compiles `sigtest.c` into a shared object file that can be linked against other projects.

### Building the Test Executable

- **Command:**
  make test

- **Description:** This target builds the test suite executable `sigtest_test`. It compiles `sigtest_test.c` and links it with `libsigtest.so`, preparing it for execution but not running the tests.

### Running the Tests

- **Command:**
  make run_tests

- **Description:** This target first builds the test suite if not already compiled (equivalent to `make test`) and then executes `sigtest_test` to run all registered tests.

### Building Everything

- **Command:**
  make all

- **Description:** The default target when you just run `make`. It builds both the shared library (`libsigtest.so`) and the test executable (`sigtest_test`).

### Cleaning Up

- **Command:**
  make clean


## Managing `libsigtest.so` After Compilation

After you've compiled `libsigtest.so`, you need to make it accessible for linking and runtime use:

1. **Copy to System Library Directory:**

   - On Linux systems, one of the default locations where the linker looks is `/usr/lib`. You might need superuser privileges:
     ```
     sudo cp bin/libsigtest.so /usr/lib
     ```

   - **Update the Shared Library Cache:**
     ```
     sudo [ldconfig](https://x.com/i/grok?text=ldconfig)
     ```
     This command updates the shared library cache, which is necessary for the system to recognize new libraries without a system reboot.

2. **Or, Specify Library Path at Compile Time:**

   If you don't want to move the library to a system-wide location, you can specify the path to the library at compile time:

   - When compiling another project that uses `sigtest`:
     ```
     gcc -o your_program your_program.c -I/path/to/sigtest/include -L/path/to/sigtest/bin -lsigtest
     ```

3. **Set Runtime Library Path:**

   - If `libsigtest.so` isn't in a standard location, you can use the `-Wl,-rpath` linker option to include the library's path in the executable so it knows where to find the library at runtime:
     ```
     gcc -o your_program your_program.c -I/path/to/sigtest/include -L/path/to/sigtest/bin -lsigtest -Wl,-rpath,/path/to/sigtest/bin
     ```

4. **Use Environment Variables (For Development or Testing):**

   - Before running your program, you can set the `LD_LIBRARY_PATH` environment variable:
     ```
     export LD_LIBRARY_PATH=/path/to/sigtest/bin:$LD_LIBRARY_PATH
     ```
     Then run your program:
     ```
     ./your_program
     ```

   Remember, setting `LD_LIBRARY_PATH` for runtime is usually for development or testing; for production, embedding the library path with `rpath` or moving the library to a standard location like `/usr/lib` is preferred.

**Note:** The exact steps might vary depending on your operating system, the permissions you have on the system, and your project's structure. Always ensure you have the right permissions when copying files to system directories, and consider the implications of modifying system-wide settings.

- **Description:** Removes all object files (`*.o`) and the binary directory containing both the shared library and the test executable. Use this to clean up before a fresh build.

## Logging Build Output

To log the output of `make` commands:

- **Command for Logging:**
  make <target> > <log_file_name>.log 2>&1

Replace `<target>` with the `make` target you want to run (e.g., `lib`, `test`, `run_tests`, `all`) and `<log_file_name>` with your preferred log file name. This will save both standard output and standard error to the specified log file.

- Example to log `make all`:
  ```
  make all > build_log_all.log 2>&1
  ```

## Using `sigtest` in Another Project

1. **Include the Header:** Add `#include "sigtest.h"` in your source files where you'll use `sigtest`.
 
2. **Link Against the Library:** When compiling your project, link against `libsigtest.so`:
   gcc -o your_program your_program.c -I/path/to/sigtest/include -L/path/to/sigtest/bin -lsigtest

Adjust the paths to match your directory structure.

3. **Write Tests:** Write your tests using the `sigtest` framework, similar to how `sigtest_test.c` is structured. Register tests with `register_test` and ensure they're linked with `libsigtest.so`.

## Notes

- Ensure the `include` directory from `sigtest` is in your include path or adjust your Makefile or compilation command accordingly.
- If you're distributing `sigtest` for others to use, consider providing installation instructions or scripts to place `libsigtest.so` in a standard location like `/usr/lib` or `/usr/local/lib` and `sigtest.h` in `/usr/include` or `/usr/local/include`.


Here are two examples demonstrating how to use sigtest for internal and external purposes:  
## Example 1: Internal Build and Test of `sigtest`

To build and test `sigtest` internally:

1. **Navigate to the `sigtest` directory:**
   cd sigma-test/sigtest

2. **Build the shared library:**
   make lib
This compiles `sigtest.c` into `libsigtest.so`.

3. **Build and Run Tests:**
   make run_tests
This will compile both `sigtest.c` and `sigtest_test.c`, link them together, and then execute the resulting test binary to run all tests.

4. **Clean Up:**
   make clean
Useful if you need to start over with a fresh build.

If you want to see the build process:

- **Log the build output:**
  make run_tests > sigtest_build_and_test.log 2>&1

## Example 2: Using `sigtest` with Your Own Library

Let's say you have a library called `MyLib` and you want to use `sigtest` for testing:

1. **Install or Link `sigtest`:**
 - Ensure `sigtest.h` is accessible or copy it to your project's include directory.
 - Place `libsigtest.so` in a location where your linker can find it or specify the path during compilation.

2. **Write Your Tests:**

 Create a test file `mylib_test.c` in your project:

 ```c
 #include "sigtest.h"
 #include "mylib.h" // Assuming this is your library's header

 void test_myFunction(void) {
     // Example test case
     Assert.assertTrue(myFunction() == EXPECTED_RESULT, "myFunction should return EXPECTED_RESULT");
 }

 __attribute__((constructor)) void init_mylib_tests(void) {
     [register_test](https://x.com/i/grok?text=register_test)("test_myFunction", test_myFunction);
 }

 int main(int argc, char **argv) {
     for (int i = 0; i < [test_count](https://x.com/i/grok?text=test_count); ++i) {
         printf("Running test: %s\n", tests[i].name);
         tests[i].test_func();
     }
     printf("Completed running %d tests.\n", test_count);
     return 0;
 }

Compile Your Test:
If you've placed libsigtest.so in a standard location:
[gcc](https://x.com/i/grok?text=gcc) -o mylib_test mylib_test.c -I/path/to/sigtest/include [-lsigtest](https://x.com/i/grok?text=-lsigtest)  
If you need to specify the path to libsigtest.so:
gcc -o mylib_test mylib_test.c -I/path/to/sigtest/include -L/path/to/sigtest/bin -lsigtest
Run Your Tests:
./mylib_test

This setup allows you to use sigtest's assertion framework to test your own library functions. Remember to adjust paths according to your actual directory structure.

These examples should give you and others a clear path for both internal testing of `sigtest` and using it to test other libraries.
