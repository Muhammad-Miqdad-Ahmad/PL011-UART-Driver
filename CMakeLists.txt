cmake_minimum_required(VERSION 3.14)
project(PL011)

# GoogleTest requires at least C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(hello_test test/hello_test.cc)
target_link_libraries(hello_test GTest::gtest_main)

# Driver/HAL tests. Compiles the real C sources (no main.c) and links them with
# the test. test/test_hw.c provides a RAM buffer that stands in for the register
# banks, and force-including test/test_hw.h ahead of PL011_config.h redirects
# the driver's base addresses at that buffer - so init/register tests run on the
# host instead of segfaulting on 0x09000000. See test/test_hw.h.
add_executable(driver_test
  test/test_driver.cc
  test/test_hw.c
  src/DRIVER.c
  src/HAL.c
  src/application.c
)
target_include_directories(driver_test PRIVATE src)
set_target_properties(driver_test PROPERTIES C_STANDARD 11)
target_compile_options(driver_test PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/test/test_hw.h)
target_link_libraries(driver_test GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(hello_test)
gtest_discover_tests(driver_test)

