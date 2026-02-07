Run full test suite:
1. cmake --build build --target tests
2. cd build && ctest --output-on-failure

If any fail: identify root cause, fix source code (not test unless test is wrong), re-run until all pass.