#### **Version 0.2.2**  _2025-05-15_  

Implementing groundwork to provide CI (_Continuous Integration_) features. First we had to implement some basic functionality.  

- **`sigtest -s -t path/to/test_set.c`** (mode=`SIMPLE`) compiles the source linked to libsigtest.so in the simplest, most lightweight process possible. output as configured in the test set is respected and simple output is generated. the `-s` may _precede_ or _follow_ the `-t <param>` flag.  
- `sigtest -t path/to/test_set.c` (mode=`DEFAULT`) compiles a custom main to expose injection of SigtestHooks for custom output control. More configurations and hooks will be made available.  

If the workflow of Makefile were followed, `make test_<setname>` clearly compiles the `test_setname.c` along with your target library. In this scenario, one would normally link to `libsigtest.so`, but `sigtest` is a self-tested framework so it is also the target library; therefore, we don't link to `libsigtest.so`. This iteration of `sigtest` (CLI) has two modes (`-v` (version) notwithstanding). The `-s` mode for `SIMPLE` is identical to the process described by using `make test_<setname>`.  

The remaining tasks will focus on adding CI features for output, automation, and failure tracking. At the moment, `sigtest` (CLI) does not run an entire suite like `make run_tests` will. That will be added likely following the two planned output formats: **JUnit XML** and **Enhanced Console**.
