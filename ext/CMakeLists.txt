cmake_minimum_required(VERSION 3.8)
project(ckdl-rb C)

if(RUBY_HDR_DIR)
  set(RUBY_INCLUDE_DIR ${RUBY_HDR_DIR} ${RUBY_ARCH_HDR_DIR})
else()
  find_package(Ruby 3.1.0 REQUIRED)
  message(STATUS "RUBY_VERSION: ${RUBY_VERSION}")
  message(STATUS "RUBY_EXECUTABLE: ${RUBY_EXECUTABLE}")
endif()

add_subdirectory(libkdl)
set_target_properties(
  ckdl-cat
  ckdl-cat-app
  ckdl-parse-events
  ckdl-tokenize
  PROPERTIES EXCLUDE_FROM_ALL TRUE)

add_library(ckdl SHARED ckdl.c)
target_include_directories(ckdl PRIVATE SYSTEM ${RUBY_INCLUDE_DIR})
target_link_libraries(ckdl PRIVATE kdl)
if(RUBY_LIBRUBY)
  target_link_directories(ckdl PRIVATE "${RUBY_LIBRARY_DIR}")
  target_link_libraries(ckdl PRIVATE "${RUBY_LIBRUBY}")
endif()

if(CMAKE_CXX_COMPILER_ID MATCHES ".*Clang")
  target_compile_options(ckdl PRIVATE -fdeclspec)
endif()

if(APPLE)
  target_link_libraries(ckdl PRIVATE -Wl,-undefined,dynamic_lookup)
endif()
