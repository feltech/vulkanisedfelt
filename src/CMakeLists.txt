# vulkanisedfelt
# SPDX-License-Identifier: MIT
# Copyright 2026 David Feltell

add_executable(
  vulkandemo
  backend/api.cpp
  backend/mock_api.cpp
  vulkan/instance.cpp
  vulkan/instance_extensions.cpp
  vulkan/instance_layers.cpp
  vulkan/physical_devices.cpp
  vulkan/physical_device_properties.cpp
  types.cpp
  setup.cpp
  draw.cpp
  Logger.cpp
  vulkandemo.cpp
  main.cpp)
target_include_directories(vulkandemo PRIVATE ${PROJECT_SOURCE_DIR})
target_sources(
  vulkandemo
  PRIVATE FILE_SET
          HEADERS
          FILES
          types.hpp
          backend/api.hpp
          backend/detail/api.hpp
          backend/mock_api.hpp
          vulkan/errors.hpp
          vulkan/managed_handles.hpp
          vulkan/instance.hpp
          vulkan/detail/instance.hpp
          vulkan/instance_extensions.hpp
          vulkan/detail/instance_extensions.hpp
          vulkan/instance_layers.hpp
          vulkan/detail/instance_layers.hpp
          vulkan/physical_devices.hpp
          vulkan/detail/physical_devices.hpp
          vulkan/physical_device_properties.hpp
          vulkan/detail/physical_device_properties.hpp
          setup.hpp
          draw.hpp
          draw/detail.hpp
          Logger.hpp
          vulkandemo.hpp
          hof.hpp
          macros.hpp)
target_link_libraries(
  vulkandemo
  PRIVATE fnfelt::fnfelt
          doctest::doctest
          SDL2::SDL2
          spdlog::spdlog
          fmt::fmt
          Microsoft.GSL::GSL
          Vulkan::Vulkan
          rollbear::strong_type
          rollbear::lift
          range-v3::range-v3
          frozen::frozen
          etl::etl)
target_compile_definitions(
  vulkandemo PRIVATE SPDLOG_ACTIVE_LEVEL=$<IF:$<CONFIG:Debug>,SPDLOG_LEVEL_DEBUG,SPDLOG_LEVEL_INFO>)

if(${PROJECT_NAME}_ENABLE_DEV)
  target_link_libraries(vulkandemo PRIVATE trompeloeil::trompeloeil)
  vulkanisedfelt_target_linters(vulkandemo)
endif()
if(${PROJECT_NAME}_ENABLE_SANITIZER_ASAN)
  target_compile_options(vulkandemo PRIVATE -fsanitize=address,undefined -fno-sanitize-recover=all
                                            -fno-omit-frame-pointer)
  target_link_options(
    vulkandemo
    PRIVATE
    -fsanitize=address,undefined
    -fno-sanitize-recover=all
    -fno-omit-frame-pointer)
endif()

if(${PROJECT_NAME}_ENABLE_DEV)
  add_test(NAME vulkandemo COMMAND vulkandemo --exit=true)
  if(${PROJECT_NAME}_ENABLE_SANITIZER_ASAN)
    # Keep drivers loaded for useful LSan diagnostics and fewer false positives.
    add_library(dlclose_stub SHARED ${PROJECT_SOURCE_DIR}/dlclose_stub.c)
    target_link_libraries(vulkandemo PRIVATE dlclose_stub)
    set_tests_properties(vulkandemo PROPERTIES ENVIRONMENT "LSAN_OPTIONS=verbosity=1:log_threads=1")
  endif()
else()
  target_compile_definitions(vulkandemo PRIVATE DOCTEST_CONFIG_DISABLE)
endif()

install(TARGETS vulkandemo RUNTIME DESTINATION bin)
