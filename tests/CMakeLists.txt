file(GLOB SPINE_READER_TEST_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp)

add_executable(spine_reader_tests main.cpp ${SPINE_READER_TEST_SOURCES})
find_package(Threads REQUIRED)
target_link_libraries(spine_reader_tests PRIVATE spine_reader HDF5::HDF5 Threads::Threads)
target_include_directories(spine_reader_tests PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party
  ${PROJECT_SOURCE_DIR}/src)
target_compile_definitions(spine_reader_tests PRIVATE
  SPINE_READER_TEST_DATA="${CMAKE_CURRENT_SOURCE_DIR}/data")

# The unit tests. The real-file test is left to its own entry below.
add_test(NAME spine_reader_tests COMMAND spine_reader_tests "--test-case-exclude=real files*")

# Real SPINE output. Point SPINE_READER_REAL_FILES (a CMake variable, or an environment variable
# when ctest runs) at a directory of *.h5 files. Without it this entry reports "Skipped" instead
# of passing: a test that did not read anything is not a pass.
set(SPINE_READER_REAL_FILES "" CACHE PATH
    "Directory of real SPINE *.h5 files for the real-file test")
add_test(NAME spine_reader_real_files
         COMMAND spine_reader_tests --no-skip "--test-case=real files*")
set_tests_properties(spine_reader_real_files PROPERTIES
  SKIP_REGULAR_EXPRESSION "SPINE_READER_REAL_FILES is not set")
if(SPINE_READER_REAL_FILES)
  set_tests_properties(spine_reader_real_files PROPERTIES
    ENVIRONMENT "SPINE_READER_REAL_FILES=${SPINE_READER_REAL_FILES}")
endif()

# The generated model (include/spine/model, src/model) must be what tools/gen produces from the
# committed schema, which was dumped from SPINE: this is what ties every default value, field
# type and assign pointer of the model to SPINE. It needs only a Python 3 interpreter.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
  add_test(NAME spine_reader_generated_model
           COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/gen/generate.py --check
                   --root ${PROJECT_SOURCE_DIR})
  # tests/golden_members.inc, the member list test_golden.cpp compares object by object, comes
  # from the same schema
  add_test(NAME spine_reader_golden_members
           COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/fixtures/make_member_list.py
                   --check)
else()
  message(WARNING "No Python 3 interpreter: the generated-model checks are not registered")
endif()

# spine-dump on the Reader test files: what it prints, and its exit codes.
if(TARGET spine-dump)
  add_test(NAME spine_dump_output
           COMMAND ${CMAKE_COMMAND} -DSPINE_DUMP=$<TARGET_FILE:spine-dump>
                   -DDATA=${CMAKE_CURRENT_SOURCE_DIR}/data
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/check_spine_dump.cmake)
endif()
