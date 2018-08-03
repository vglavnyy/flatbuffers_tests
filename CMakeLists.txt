cmake_minimum_required(VERSION 3.9)

project(FlatBuffersTests CXX)
enable_testing()

if(NOT GOOGLETEST_SRC_DIR)
  set(GOOGLETEST_SRC_DIR "${CMAKE_SOURCE_DIR}/../googletest")
endif()

if(NOT FLATBUFFERS_SRC_DIR)
  set(FLATBUFFERS_SRC_DIR "${CMAKE_SOURCE_DIR}/../flatbuffers")
endif()

# Prevent overriding the parent project's compiler/linker settings on Windows
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Add googletest directly to our build. This defines the gtest and gtest_main targets.
add_subdirectory(${GOOGLETEST_SRC_DIR}
                 ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                 EXCLUDE_FROM_ALL)
# Add flatbuffers
set(FLATBUFFERS_INSTALL OFF CACHE BOOL "")
set(FLATBUFFERS_BUILD_FLATHASH OFF CACHE BOOL "")
set(FLATBUFFERS_BUILD_TESTS ON CACHE BOOL "")
set(FLATBUFFERS_BUILD_PATH "${CMAKE_CURRENT_BINARY_DIR}/flatbuffers-build")
add_subdirectory(${FLATBUFFERS_SRC_DIR} ${FLATBUFFERS_BUILD_PATH})

function(compile_flatbuffers_schema_to_cpp SRC_FBS)
  get_filename_component(SRC_FBS_DIR ${SRC_FBS} PATH)
  string(REGEX REPLACE "\\.fbs$" "_generated.h" GEN_HEADER ${SRC_FBS})
  add_custom_command(
    OUTPUT "${GEN_HEADER}"
    COMMAND $<TARGET_FILE:flatc>
            --cpp
            --no-includes
            --scoped-enums
            --gen-mutable
            --gen-object-api
            --reflect-names
            -o "${CMAKE_CURRENT_SOURCE_DIR}/${SRC_FBS_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}/${SRC_FBS}"
    DEPENDS flatc)
endfunction()

# Update flatbuffers_tests schema
compile_flatbuffers_schema_to_cpp(tests/test.fbs)

# Add executable
add_executable(flatbuffers_tests
  tests/json_parser_1.cpp
  # add generated headers to dependency list for auto update
  tests/test_generated.h
)

# Use global define for reference to fbs files instead of copy to binary dir.
target_compile_definitions(flatbuffers_tests
  PRIVATE
  JSON_SAMPLES_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/json_datasets/\"
)
target_compile_definitions(flatbuffers_tests
  PRIVATE
  FLATBUFFERS_FBS_DIR=\"${CMAKE_CURRENT_SOURCE_DIR}/tests/\"
)

target_link_libraries(flatbuffers_tests PRIVATE gtest gtest_main)
target_link_libraries(flatbuffers_tests PRIVATE flatbuffers)
add_dependencies(flatbuffers_tests flatc)
add_dependencies(flatbuffers_tests flattests)

add_test(NAME FlatbuffersJsonParserTest COMMAND flatbuffers_tests)

