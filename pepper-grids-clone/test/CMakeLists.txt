cmake_minimum_required(VERSION 3.14)
project(pepper_grids_clone_tests CXX)
set(CMAKE_CXX_STANDARD 17)

add_executable(tests
  test_engine.cpp
  ../project/GridsEngine.cpp
)
target_include_directories(tests PRIVATE ../project)

enable_testing()
add_test(NAME engine COMMAND tests)
