# CMAKE generated file: DO NOT EDIT!
# Generated by CMake Version 3.22
cmake_policy(SET CMP0009 NEW)

# SRC_FILES at auth/CMakeLists.txt:5 (FILE)
file(GLOB_RECURSE NEW_GLOB LIST_DIRECTORIES false "/Users/awsjohns/credentials-fetcher2/credentials-fetcher/no-grpc-mode/auth/kerberos/src/*.c")
set(OLD_GLOB
  )
if(NOT "${NEW_GLOB}" STREQUAL "${OLD_GLOB}")
  message("-- GLOB mismatch!")
  file(TOUCH_NOCREATE "/Users/awsjohns/credentials-fetcher2/credentials-fetcher/no-grpc-mode/build/CMakeFiles/cmake.verify_globs")
endif()

# SRC_FILES at auth/CMakeLists.txt:5 (FILE)
file(GLOB_RECURSE NEW_GLOB LIST_DIRECTORIES false "/Users/awsjohns/credentials-fetcher2/credentials-fetcher/no-grpc-mode/auth/kerberos/src/*.cpp")
set(OLD_GLOB
  "/Users/awsjohns/credentials-fetcher2/credentials-fetcher/no-grpc-mode/auth/kerberos/src/krb.cpp"
  )
if(NOT "${NEW_GLOB}" STREQUAL "${OLD_GLOB}")
  message("-- GLOB mismatch!")
  file(TOUCH_NOCREATE "/Users/awsjohns/credentials-fetcher2/credentials-fetcher/no-grpc-mode/build/CMakeFiles/cmake.verify_globs")
endif()

# SRC_FILES at auth/CMakeLists.txt:5 (FILE)
file(GLOB_RECURSE NEW_GLOB LIST_DIRECTORIES false "/Users/awsjohns/credentials-fetcher2/credentials-fetcher/no-grpc-mode/auth/kinit_client/*.c")
set(OLD_GLOB
  "/Users/awsjohns/credentials-fetcher2/credentials-fetcher/no-grpc-mode/auth/kinit_client/kinit.c"
  "/Users/awsjohns/credentials-fetcher2/credentials-fetcher/no-grpc-mode/auth/kinit_client/kinit_kdb.c"
  )
if(NOT "${NEW_GLOB}" STREQUAL "${OLD_GLOB}")
  message("-- GLOB mismatch!")
  file(TOUCH_NOCREATE "/Users/awsjohns/credentials-fetcher2/credentials-fetcher/no-grpc-mode/build/CMakeFiles/cmake.verify_globs")
endif()
