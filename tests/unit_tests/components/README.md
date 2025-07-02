# How to write C++ ESPHome unit tests

1. Copy the `example` folder as a starting template and rename it with your component name.
2. Rename `example.cpp` in your folder to a suitable name and write the tests. You can add as many .cpp files as you need to organize your tests
3. Find a suitable testxxx.yaml in this folder and add the simplest possible configuration for your component. That configuration won't be used nor ran. It is just there so ESPHome pulls the component code into the test.
4. Add your folder to the `includes:` list ending with a slash `/`

## Running a single test yaml

To run a single test yaml and see the output, simply run the configuration:

```bash
esphome run test001.yaml
```

To run a specific test suite, you can provide a Google Test filter:

```bash
GTEST_FILTER='UART*' esphome run test001.yaml
```

The process will return `0` for succcess or nonzero for failure. In case of failure, the errors will be printed out to the console.

## Running all the tests

To run all the C++ tests, simply run `test_cpp_components.py` in the `unit_tests` parent folder.
