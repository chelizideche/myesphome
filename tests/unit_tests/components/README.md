# How to write C++ ESPHome unit tests

1. Copy the `example` folder as a starting template and rename it with your component name.
2. Rename `test_example.yaml` to a suitable name. All final test configurations must begin with `test_`.
3. Edit the test yaml file to add the simplest possible configuration for your component. That configuration won't be used nor ran. It is just there so ESPHome pulls the component code into the test.
4. Rename `example.cpp` to a suitable name and write the tests
5. Reference all your `*.cpp` and `*.h` files in your test yaml under the `includes:` list

## Running a single test yaml

To run a single test yaml and see the output, simply run the configuration:

```bash
esphome run test_example.yaml
```

The process will return `0` for succcess or nonzero for failure. In case of failure, the errors will be printed out to the console.

## Running all the tests

To run all the C++ tests, simply run `test_cpp_components.py` in the `unit_tests` parent folder.  