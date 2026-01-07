add_custom_target(OrcRTUnitTests)
set_target_properties(OrcRTUnitTests PROPERTIES FOLDER "orc-rt/Tests")

if(ORC_RT_ENABLE_EXCEPTIONS)
  set(LLVM_REQUIRES_EH ON)
endif()

if(ORC_RT_ENABLE_RTTI)
  set(LLVM_REQUIRES_RTTI ON)
endif()

if (NOT TARGET llvm_gtest)
  message(WARNING "orc-rt unittests disabled due to GTest being unavailable; "
                  "Try LLVM_INSTALL_GTEST=ON for the LLVM build")
  return ()
endif ()
#TODO(jared: this just exists to ensure the target checker doesn't complain)
set(LLVM_OPTIONAL_SOURCES
  StandaloneMachOUnwindInfoRegistrarTest.cpp
)

function(add_orc_rt_unittest test_dirname)
  add_unittest(OrcRTUnitTests ${test_dirname} ${ARGN})
endfunction()
set(ORC_RT_CORE_TEST_SOURCES
  AllocActionTest.cpp
  BitmaskEnumTest.cpp
  BootstrapInfoTest.cpp
  CCallbackTest.cpp
  CallSPSCITest.cpp
  CallableTraitsHelperTest.cpp
  CommandLineTest.cpp
  CompilerTest.cpp
  EndianTest.cpp
  ErrorCAPITest.cpp
  ErrorExceptionInteropTest.cpp
  ErrorTest.cpp
  ExecutorAddressTest.cpp
  ExecutorProcessInfoTest.cpp
  InProcessControllerAccessTest.cpp
  IntervalMapTest.cpp
  IntervalSetTest.cpp
  LockedAccessTest.cpp
  LoggingTest.cpp
  MacroUtilsTest.cpp
  MathTest.cpp
  MemoryAccessSPSCITest.cpp
  MemoryFlagsTest.cpp
  NativeDylibManagerSPSCITest.cpp
  NativeDylibManagerTest.cpp
  QueueingRunnerTest.cpp
  RTTITest.cpp
  SPSAllocActionTest.cpp
  SPSMemoryFlagsTest.cpp
  SPSWrapperFunctionBufferTest.cpp
  SPSWrapperFunctionTest.cpp
  SessionTest.cpp
  SimpleNativeMemoryMapSPSCITest.cpp
  SimpleNativeMemoryMapTest.cpp
  SimplePackedSerializationTest.cpp
  SimpleSymbolTableTest.cpp
  StringExtrasTest.cpp
  TaskGroupTest.cpp
  ThreadPoolRunnerTest.cpp
  WrapperFunctionBufferTest.cpp
  bind-test.cpp
  bit-test.cpp
  iterator_range-test.cpp
  move_only_function-test.cpp
  scope_exit-test.cpp
  span-test.cpp

  DISABLE_LLVM_LINK_LLVM_DYLIB
  )

if(APPLE)
  list(APPEND ORC_RT_CORE_TEST_SOURCES
    StandaloneMachOUnwindInfoRegistrarTest.cpp
  )
endif()

add_orc_rt_unittest(CoreTests
  ${ORC_RT_CORE_TEST_SOURCES}
  DISABLE_LLVM_LINK_LLVM_DYLIB
)

target_compile_options(CoreTests PRIVATE ${ORC_RT_COMPILE_FLAGS})
target_link_libraries(CoreTests PRIVATE
  orc-rt-executor
  orc-rt-executor-impl-headers
  )

# Build a shared library for NativeDylibManager tests.
add_library(NativeDylibManagerTestLib SHARED
  Inputs/NativeDylibManagerTestLib.cpp)
set_target_properties(NativeDylibManagerTestLib PROPERTIES
  PREFIX "")
target_compile_definitions(CoreTests PRIVATE
  "NDM_TEST_LIB_PATH=\"$<TARGET_FILE:NativeDylibManagerTestLib>\"")
add_dependencies(CoreTests NativeDylibManagerTestLib)
