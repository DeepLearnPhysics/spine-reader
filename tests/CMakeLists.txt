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

# The unit tests. The real-file test and the consumer contracts are left to their own entries.
add_test(NAME spine_reader_tests COMMAND spine_reader_tests
         "--test-case-exclude=real files*,contract*")

# What the CAFMakers rely on (test_contracts.cpp): the units and defaults of the fields they read,
# and the enum values. A failure means the SPINE change is reviewed with the CAFMaker maintainers.
add_test(NAME spine_reader_contracts COMMAND spine_reader_tests "--test-case=contract*")

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
# SPINE_READER_REQUIRE_PYTHON (CI turns it on) makes a missing interpreter a configuration
# error instead of silently leaving these checks out.
option(SPINE_READER_REQUIRE_PYTHON
       "Fail the configuration if Python 3 is missing, instead of skipping the Python-based checks"
       OFF)
if(SPINE_READER_REQUIRE_PYTHON)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
else()
  find_package(Python3 COMPONENTS Interpreter QUIET)
endif()
if(Python3_Interpreter_FOUND)
  add_test(NAME spine_reader_generated_model
           COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/gen/generate.py --check
                   --root ${PROJECT_SOURCE_DIR})
  # tests/golden_members.inc, the member list test_golden.cpp compares object by object, comes
  # from the same schema
  add_test(NAME spine_reader_golden_members
           COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/tools/fixtures/make_member_list.py
                   --check)
  # The unit tests of tools/release (classify.py, follow_spine.py): no SPINE, no network. The
  # release tools need Python 3.10, as SPINE does; older interpreters leave them out.
  if(Python3_VERSION VERSION_GREATER_EQUAL 3.10)
    add_test(NAME spine_reader_release_tools
             COMMAND ${Python3_EXECUTABLE} -m unittest discover
                     -s ${PROJECT_SOURCE_DIR}/tools/release/tests
                     -t ${PROJECT_SOURCE_DIR}/tools/release)
  endif()
else()
  message(WARNING "No Python 3 interpreter: the generated-model checks are not registered")
endif()

# spine-dump on the Reader test files: what it prints, and its exit codes. The script compares
# the versions it prints with the library's (project(VERSION)) and with the model's SPINE
# release, read here from the generated model header; a regenerated header reconfigures.
if(TARGET spine-dump)
  set(model_header ${PROJECT_SOURCE_DIR}/include/spine/model.hpp)
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${model_header})
  file(STRINGS ${model_header} model_version_line
       REGEX "^#define SPINE_READER_MODEL_SPINE_VERSION ")
  string(REGEX REPLACE ".*\"([^\"]*)\".*" "\\1" model_version "${model_version_line}")
  if(NOT model_version MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(FATAL_ERROR "no SPINE_READER_MODEL_SPINE_VERSION in ${model_header}")
  endif()
  add_test(NAME spine_dump_output
           COMMAND ${CMAKE_COMMAND} -DSPINE_DUMP=$<TARGET_FILE:spine-dump>
                   -DDATA=${CMAKE_CURRENT_SOURCE_DIR}/data
                   -DREADER_VERSION=${PROJECT_VERSION} -DMODEL_VERSION=${model_version}
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/check_spine_dump.cmake)
endif()
