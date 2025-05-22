#### **Version 0.3.0**  -- _2025-05-20_  

Decoupled logging from the `sigtest_hooks` structure. Not that logging was directly coupled, but by the appearance of the structure, *logging* appeared to be the sole purpose for altering behaviors.

`sigtest` (LIB)
- The run test process has been decoupled from logging behaviors. Instead of calling the output functions - `writelnf` or `fwritelnf` - the test runner calls the set's `logger->log` (or `->debug`) function.
- Test runner now checks for default `hooks` and puts a priority on the `test_hooks` parameter. If neither are supplied, then the default `hooks` will be used to provide a simple output format.
- The test runner now calls the hooks in the following order, wrapping the test call itself:
  - **`before_test_set`**: called prior to beginning a test set, after the set configuration is called (if registered).
  - **`before_test_case`**: called before test setup (if registered).
  - **`on_start_test`**: handler called after test setup, before test execution.
  - `*** execute test ***`
  - **`on_end_test`**: handler called directly after test execution.
  - **`after_test_case`**: called after test teardown, prior to result processing
  - **`after_test_set`**: called at the end of the test set, after set clean up
- Logging test state is fully decoupled from test runner. All logging is directed via `hooks` behaviors and `logger->log` and `logger->debug`.
-----  

#### **Version 0.2.2**  -- _2025-05-15_  

Implementing groundwork to provide CI (_Continuous Integration_) features. First we had to implement some basic functionality.  

- **`sigtest -s -t path/to/test_set.c`** (mode=`SIMPLE`) compiles the source linked to libsigtest.so in the simplest, most lightweight process possible. output as configured in the test set is respected and simple output is generated. the `-s` may _precede_ or _follow_ the `-t <param>` flag.  
- **`sigtest -t path/to/test_set.c`** (mode=`DEFAULT`) compiles a custom main to expose injection of SigtestHooks for custom output control. More configurations and hooks will be made available.  

If the workflow of Makefile were followed, `make test_<setname>` clearly compiles the `test_setname.c` along with your target library. In this scenario, one would normally link to `libsigtest.so`, but `sigtest` is a self-tested framework so it is also the target library; therefore, we don't link to `libsigtest.so`. This iteration of `sigtest` (CLI) has two modes (`-v` (version) notwithstanding). The `-s` mode for `SIMPLE` is identical to the process described by using `make test_<setname>`.  

The remaining tasks will focus on adding CI features for output, automation, and failure tracking. At the moment, `sigtest` (CLI) does not run an entire suite like `make run_tests` will. That will be added likely following the two planned output formats: **JUnit XML** and **Enhanced Console**.
