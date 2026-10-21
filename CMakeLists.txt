cmake_minimum_required(VERSION 3.19)
project(spine-reader VERSION 0.1.0 LANGUAGES C CXX)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
  set(SPINE_READER_TOP_LEVEL ON)
else()
  set(SPINE_READER_TOP_LEVEL OFF)
endif()

option(SPINE_READER_BUILD_TESTS "Build the spine-reader unit tests" ${SPINE_READER_TOP_LEVEL})
option(SPINE_READER_BUILD_TOOLS "Build the spine-dump tool" ${SPINE_READER_TOP_LEVEL})
set(SPINE_READER_SANITIZE OFF CACHE STRING
    "Sanitizers: OFF, ON (address and undefined behaviour) or thread (GCC, Clang)")
set_property(CACHE SPINE_READER_SANITIZE PROPERTY STRINGS OFF ON thread)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

if(SPINE_READER_SANITIZE STREQUAL "thread")
  add_compile_options(-fsanitize=thread -fno-omit-frame-pointer)
  add_link_options(-fsanitize=thread)
elseif(SPINE_READER_SANITIZE)
  add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=undefined
                      -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address,undefined)
endif()

# Only the HDF5 C library is used. The HDF5 C++ API is deliberately not a dependency.
find_package(HDF5 1.12 REQUIRED COMPONENTS C)

file(GLOB SPINE_READER_SOURCES CONFIGURE_DEPENDS
  ${PROJECT_SOURCE_DIR}/src/*.cpp
  ${PROJECT_SOURCE_DIR}/src/*/*.cpp)

add_library(spine_reader ${SPINE_READER_SOURCES})
add_library(SpineReader::spine_reader ALIAS spine_reader)
target_compile_features(spine_reader PUBLIC cxx_std_17)
set_target_properties(spine_reader PROPERTIES CXX_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
target_include_directories(spine_reader
  PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
  PRIVATE
    ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(spine_reader PRIVATE HDF5::HDF5)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(spine_reader PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Install and export a CMake package: find_package(SpineReader) -> SpineReader::spine_reader
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
install(TARGETS spine_reader EXPORT SpineReaderTargets
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/spine DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT SpineReaderTargets NAMESPACE SpineReader::
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SpineReader)
configure_package_config_file(cmake/SpineReaderConfig.cmake.in
  ${PROJECT_BINARY_DIR}/SpineReaderConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SpineReader)
write_basic_package_version_file(${PROJECT_BINARY_DIR}/SpineReaderConfigVersion.cmake
  COMPATIBILITY SameMinorVersion)
install(FILES
  ${PROJECT_BINARY_DIR}/SpineReaderConfig.cmake
  ${PROJECT_BINARY_DIR}/SpineReaderConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/SpineReader)

if(SPINE_READER_BUILD_TOOLS)
  add_executable(spine-dump examples/spine_dump.cpp)
  target_link_libraries(spine-dump PRIVATE spine_reader)
  install(TARGETS spine-dump RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

if(SPINE_READER_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
