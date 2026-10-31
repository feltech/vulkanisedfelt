# vulkanisedfelt
# SPDX-License-Identifier: MIT
# Copyright 2026 David Feltell

cmake_minimum_required(VERSION 3.30)
project(
  vulkanisedfelt
  VERSION 1.0.0
  LANGUAGES CXX C)

option(${PROJECT_NAME}_ENABLE_DEV "Enable tests and linting" OFF)
option(${PROJECT_NAME}_ENABLE_SANITIZER_ASAN "Enable ASan and UBSan" OFF)

if(${PROJECT_NAME}_ENABLE_DEV)
  enable_testing()
  include(cmake/linters.cmake)
endif()

# Set the standard before fnfelt's reflection capability probe, which cannot
# inherit the interface target's compile features before that target exists.
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
# fnfelt owns pinned stdexec through CPM; Conan supplies Immer and libfork.
add_subdirectory(vendor/fnfelt EXCLUDE_FROM_ALL)

find_package(doctest REQUIRED)
if(${PROJECT_NAME}_ENABLE_DEV)
  find_package(trompeloeil REQUIRED)
endif()
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(Microsoft.GSL REQUIRED)
find_package(strong_type REQUIRED)
find_package(range-v3 REQUIRED)
find_package(frozen REQUIRED)
find_package(etl REQUIRED)
find_package(Vulkan REQUIRED)
add_subdirectory(vendor/lift EXCLUDE_FROM_ALL)
add_library(rollbear::lift ALIAS lift)

add_subdirectory(src)

if(${PROJECT_NAME}_ENABLE_DEV)
  add_subdirectory(tests)
endif()
