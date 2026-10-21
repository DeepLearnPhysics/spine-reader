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
