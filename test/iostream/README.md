# POSIX child-pipe regression tests

Requires a C++20 compiler, CMake and Boost headers with Boost.Process v1.

```sh
cmake -S test/iostream -B build-pipe-tests -DCMAKE_BUILD_TYPE=Debug
cmake --build build-pipe-tests
ctest --test-dir build-pipe-tests --output-on-failure
```

On Homebrew installations, add `-DCMAKE_PREFIX_PATH=/opt/homebrew` to the
configure command if needed. Each test has an eight-second CTest watchdog.

The harness compiles the production `ChildPipeIOStream.cpp` and `IOStream.cpp`.
Only the logger is replaced with a no-op shim to avoid requiring OpenCV/ONNX
and the full framework dependency bundle. OS pipes, child launch/termination,
EOF, buffering, deadlines and process reaping are not mocked.

The executable launches itself as the controlled writer. Tests cover silent
and partial-output stalls, missing delimiters, continuous output, a complete
binary payload larger than the read buffer, exact byte counts, delimiter
boundaries, reading again after timeout, stdout closing before child exit,
nonzero and empty successful exits, inherited stdout held by a descendant,
interrupted polling, and stdin/stdout round trips. Direct-child cleanup is
checked with `waitpid`. The inherited-writer descendant exits after two seconds;
the reader does not claim to terminate arbitrary process trees.

`ChildPipeIOStream::eof()` reports successful stdout completion separately from
`is_open()`: a timed read is still open, and a permanent I/O error is closed but
not complete. The tests check this query for timeout, EOF and resumed reads.
The query is nonvirtual and adds no class data members; consumers that use it
must link a rebuilt MaaUtils library.

The three primary regressions (`silent`, `partial`, `delimiter_timeout`) fail
against the old blocking reader: a requested 100 ms read waits roughly two
seconds for the writer to exit. The patched reader returns near 100 ms while
preserving partial bytes. Timing assertions allow scheduler slack; this is a
bounded-wait implementation, not a hard real-time guarantee.

To run the same cases against a full framework build, link its real library
instead of compiling the standalone sources. Use the **same Boost headers** as
that build (the C++ class contains Boost.Process objects):

```sh
cmake -S test/iostream -B build-pipe-linked \
  -DMAAUTILS_LIBRARY=/path/to/framework/install/bin/libMaaUtils.so \
  -DCMAKE_PREFIX_PATH=/path/to/MaaDeps/vcpkg/installed/maa-arm64-linux
cmake --build build-pipe-linked
ctest --test-dir build-pipe-linked --output-on-failure
```

On macOS use `libMaaUtils.dylib` and the `maa-arm64-osx` dependency directory.
This mode uses the production logger and the compiled framework library;
the standalone logging shim is not included.

The fix is POSIX-only. The Windows reader is not covered by these tests and is
not converted to nonblocking I/O. Run the suite on both macOS and Linux before
merging; a standalone passing run does not replace full framework integration
testing with the project's dependency bundle.
