cmake_minimum_required(VERSION 3.13.4)
project(UnitTest)

#===============================================================================
# 1. LOAD LLVM CONFIGURATION
#===============================================================================
# Set this to a valid LLVM installation dir
set(LT_LLVM_INSTALL_DIR "" CACHE PATH "LLVM installation directory")

# Add the location of LLVMConfig.cmake to CMake search paths (so that
# find_package can locate it)
list(APPEND CMAKE_PREFIX_PATH "${LT_LLVM_INSTALL_DIR}/lib/cmake/llvm/")

# FIXME: This is a warkaround for #25. Remove once resolved and use
# find_package(LLVM 11.0.0 REQUIRED CONFIG) instead.
find_package(LLVM REQUIRED CONFIG)

# UnitTest includes headers from LLVM - update the include paths accordingly
include_directories(SYSTEM ${LLVM_INCLUDE_DIRS}, "${CMAKE_CURRENT_SOURCE_DIR}/../include")

#===============================================================================
# 2. LLVM-TUTOR BUILD CONFIGURATION
#===============================================================================
# Use the same C++ standard as LLVM does
set(CMAKE_CXX_STANDARD 14 CACHE STRING "")

# LLVM is normally built without RTTI. Be consistent with that.
if(NOT LLVM_ENABLE_RTTI)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

#===============================================================================
# 3. ADD THE TARGET
#===============================================================================
enable_testing()

add_executable(UnitTest 
    # List your source files here.
    UnitTest.cpp
)


# Allow undefined symbols in shared objects on Darwin (this is the default
# behaviour on Linux)
target_link_libraries(UnitTest
  "$<$<PLATFORM_ID:Darwin>:-undefined dynamic_lookup>")
  target_link_libraries(
  UnitTest
  gtest_main
)
include(GoogleTest)
gtest_discover_tests(UnitTest)