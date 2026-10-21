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

add_test(NAME spine_reader_tests COMMAND spine_reader_tests)
