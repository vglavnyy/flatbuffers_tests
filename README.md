# Standalone tests for C++ Flatbuffers library

## Required
https://github.com/google/flatbuffers
https://github.com/google/googletest

The build script looking for required libraries at one level higher of himself.
For cmake build it is expected that the structure of file directories is:
```
/flatbuffers/
/flatbuffers_tests/
/googletests/
```
All these dependencies should be installed before build.

## Tests for JSON parser
JSON datasets located at `./json_datasets` directory.
Sources:
1) Dataset from `json.org`: [https://www.json.org/JSON_checker/test.zip]
2) Dataset from `seriot.ch`: [https://github.com/nst/JSONTestSuite]
