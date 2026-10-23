cmake_minimum_required(VERSION 3.20)
project(gravsim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release)
endif()

option(GRAVSIM_BUILD_TESTS "Build unit/validation tests" ON)
option(GRAVSIM_BUILD_CLI "Build the command-line tool" ON)
option(GRAVSIM_SANITIZE "Enable ASan+UBSan" OFF)

add_library(gravsim_core STATIC
  core/src/orbit.cpp
  core/src/system.cpp
  core/src/integrators.cpp
  core/src/scenarios.cpp)
target_include_directories(gravsim_core PUBLIC core/include)
target_compile_features(gravsim_core PUBLIC cxx_std_20)
if(MSVC)
  target_compile_options(gravsim_core PRIVATE /W4)
else()
  # Deliberately no -ffast-math: it breaks the floating-point reproducibility physics tests rely on.
  target_compile_options(gravsim_core PRIVATE -Wall -Wextra -Wpedantic)
endif()
if(GRAVSIM_SANITIZE AND NOT MSVC)
  target_compile_options(gravsim_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(gravsim_core PUBLIC -fsanitize=address,undefined)
endif()

if(EMSCRIPTEN)
  add_executable(gravsim_wasm wasm/bindings.cpp)
  target_link_libraries(gravsim_wasm PRIVATE gravsim_core)
  set_target_properties(gravsim_wasm PROPERTIES OUTPUT_NAME gravsim SUFFIX ".js")
  target_link_options(gravsim_wasm PRIVATE
    -O3 --no-entry
    -sMODULARIZE=1 -sEXPORT_NAME=createGravsim
    -sALLOW_MEMORY_GROWTH=1 -sENVIRONMENT=web
    -sEXPORTED_RUNTIME_METHODS=UTF8ToString,HEAPF64)
else()
  if(GRAVSIM_BUILD_CLI)
    add_executable(gravsim cli/main.cpp)
    target_link_libraries(gravsim PRIVATE gravsim_core)
  endif()
  if(GRAVSIM_BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(Catch2
      GIT_REPOSITORY https://github.com/catchorg/Catch2.git
      GIT_TAG v3.5.2)
    FetchContent_MakeAvailable(Catch2)
    enable_testing()
    add_executable(gravsim_tests
      core/tests/test_basics.cpp
      core/tests/test_kepler.cpp
      core/tests/test_integrators.cpp
      core/tests/test_nbody.cpp
      core/tests/test_relativity.cpp
      core/tests/test_scenarios.cpp)
    target_link_libraries(gravsim_tests PRIVATE gravsim_core Catch2::Catch2WithMain)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)
    catch_discover_tests(gravsim_tests)
  endif()
endif()
