# CMake build for ORC-RT.

#===============================================================================
# Setup Project
#===============================================================================

cmake_minimum_required(VERSION 3.20.0)
set(LLVM_COMMON_CMAKE_UTILS ${CMAKE_CURRENT_SOURCE_DIR}/../cmake)
include(${LLVM_COMMON_CMAKE_UTILS}/Modules/CMakePolicy.cmake
  NO_POLICY_SCOPE)

project(OrcRT LANGUAGES C CXX ASM)

if (NOT DEFINED LLVM_BINARY_DIR)
    set(LLVM_STANDALONE_BUILD TRUE)
endif()

#standalone stuff
if(DEFINED LLVM_STANDALONE_BUILD)
    set(LLVM_MAIN_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../llvm)
    set(LLVM_CMAKE_DIR ${LLVM_MAIN_SRC_DIR}/cmake)
    list(INSERT CMAKE_MODULE_PATH 0
      "${LLVM_CMAKE_DIR}"
      "${LLVM_CMAKE_DIR}/Modules"
    )
    include(AddLLVM)
    set(LLVM_INSTALL_GTEST ON)
    set(STANDALONE_GTESTS ON)
    set(UNITTEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../third-party/unittest)
    
    set(LLVM_USE_GMAIN ON)
    
    add_subdirectory(${UNITTEST_DIR} third-party/unittest)
    target_compile_definitions(llvm_gtest PUBLIC GTEST_NO_LLVM_SUPPORT=1)
    target_compile_definitions(llvm_gtest_main PUBLIC GTEST_NO_LLVM_SUPPORT=1)
endif()

list(INSERT CMAKE_MODULE_PATH 0
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake"
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules"
  "${CMAKE_CURRENT_SOURCE_DIR}/../runtimes/cmake/Modules"
  "${LLVM_COMMON_CMAKE_UTILS}"
  "${LLVM_COMMON_CMAKE_UTILS}/Modules"
  )

include(GNUInstallDirs)

#===============================================================================
# Setup CMake Options
#===============================================================================

option(ORC_RT_INCLUDE_DOCS "Build the ORC-RT documentation." ON)
option(ORC_RT_ENABLE_ASSERTIONS "Enable assertions independent of build mode." ON)
option(ORC_RT_ENABLE_PEDANTIC "Compile with pedantic enabled." ON)
option(ORC_RT_ENABLE_WERROR "Fail and stop if a warning is triggered." OFF)
option(ORC_RT_ENABLE_RTTI "Enable RTTI." ON)
option(ORC_RT_ENABLE_EXCEPTIONS "Enable exceptions." ON)
option(ORC_RT_INCLUDE_TESTS "Build ORC-RT tests." ${LLVM_INCLUDE_TESTS})

set(CMAKE_CXX_STANDARD 17 CACHE STRING "C++ standard to conform to")
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_CXX_EXTENSIONS NO)
set(CMAKE_FOLDER "orc-rt")

set(ORC_RT_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(ORC_RT_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

if(ORC_RT_ENABLE_RTTI)
  if(MSVC)
    list(APPEND ORC_RT_COMPILE_FLAGS "/GR")
  else()
    list(APPEND ORC_RT_COMPILE_FLAGS "-frtti")
  endif()
else()
  if(MSVC)
    list(APPEND ORC_RT_COMPILE_FLAGS "/GR-")
  else()
    list(APPEND ORC_RT_COMPILE_FLAGS "-fno-rtti")
  endif()
endif()

if(ORC_RT_ENABLE_EXCEPTIONS)
  if(MSVC)
    list(APPEND ORC_RT_COMPILE_FLAGS "/EHsc")
  else()
    list(APPEND ORC_RT_COMPILE_FLAGS "-fexceptions")
  endif()
else()
  if(NOT MSVC)
    list(APPEND ORC_RT_COMPILE_FLAGS "-fno-exceptions")
  endif()
endif()

if(LLVM_STANDALONE_BUILD AND ORC_RT_ENABLE_RTTI)
    set(LLVM_ENABLE_RTTI ON CACHE BOOL "Forcing LLVM RTTI because ORC Runtime requires it" FORCE)
endif()


if(LLVM_STANDALONE_BUILD AND ORC_RT_ENABLE_EXCEPTIONS)
    set(LLVM_ENABLE_EH ON CACHE BOOL "Forcing LLVM exceptions because ORC Runtime requires it" FORCE)
endif()

configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/include/orc-rt-c/config.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/include/orc-rt-c/config.h
  @ONLY
)

#===============================================================================
# Setup Source Code
#===============================================================================

if (ORC_RT_INCLUDE_DOCS)
    add_subdirectory(docs)
endif()

add_subdirectory(include)
add_subdirectory(lib)
add_subdirectory(tools)

if(ORC_RT_INCLUDE_TESTS)
  add_subdirectory(test)
  add_subdirectory(unittests)
endif()
