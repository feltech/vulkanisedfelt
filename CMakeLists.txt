cmake_minimum_required(VERSION 3.28)
project(vulkandemo CXX C)

####################################################################################################
# Options

option(${PROJECT_NAME}_ENABLE_TESTS "Enable unit tests" OFF)
option(${PROJECT_NAME}_ENABLE_SANITIZER_ASAN "Enable ASan and UBSan" ON)


####################################################################################################
# Dependencies

find_package(doctest REQUIRED)
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(Microsoft.GSL REQUIRED)
find_package(strong_type REQUIRED)
find_package(Vulkan REQUIRED)


####################################################################################################
# Create target

set(_exe_target vulkandemo)

add_executable(
    ${_exe_target}
    src/VulkanApp.cpp
    src/Logger.cpp
    src/vulkandemo.cpp
    src/vulkandemo.hpp
    src/main.cpp
)

install(
    TARGETS ${_exe_target}
    DESTINATION "."
    RUNTIME DESTINATION bin
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
)

target_compile_features(${_exe_target} PRIVATE cxx_std_23)
set_target_properties(${_exe_target} PROPERTIES CXX_EXTENSIONS OFF)

target_link_libraries(
    ${_exe_target}
    PRIVATE
    doctest::doctest
    SDL2::SDL2
    spdlog::spdlog
    fmt::fmt
    Microsoft.GSL::GSL
    Vulkan::Vulkan
    rollbear::strong_type
)

target_compile_definitions(
    ${_exe_target}
    PRIVATE
    SPDLOG_ACTIVE_LEVEL=$<IF:$<CONFIG:Debug>,SPDLOG_LEVEL_DEBUG,SPDLOG_LEVEL_INFO>
)

if (${PROJECT_NAME}_ENABLE_SANITIZER_ASAN)
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(
            ${_exe_target}
            PUBLIC
            -fsanitize=address,undefined
            -fno-sanitize-recover=all
            -fno-omit-frame-pointer
        )
        target_link_options(
            ${_exe_target}
            PUBLIC
            -fsanitize=address,undefined
            -fno-sanitize-recover=all
            -fno-omit-frame-pointer
        )
    endif ()
endif ()

# Testing

if (${PROJECT_NAME}_ENABLE_TESTS)
    enable_testing()

    add_test(
        NAME ${_exe_target}
        COMMAND
        ${_exe_target}
        # Do not continue on to run the application after running tests.
        --exit=true
        # Also report successful tests (useful due to volume of logging).
        #        --success=true
    )

    if (${PROJECT_NAME}_ENABLE_SANITIZER_ASAN)
        # Create a library that stubs out dlclose. This is for two reasons:
        # * It resolves LSan leak detection false positives in vulkan and nvidia drivers.
        # * If LSan does detect a leak, the name of the library should be reported in the stack
        #   trace
        add_library(dlclose_stub SHARED dlclose_stub.c)
        target_link_libraries(${_exe_target} PRIVATE dlclose_stub)

        list(APPEND _envvars.lsan_opts "verbosity=1")
        list(APPEND _envvars.lsan_opts "log_threads=1")
        list(JOIN _envvars.lsan_opts ":" _envvars.lsan_opts)
        list(APPEND _envvars "LSAN_OPTIONS=${_envvars.lsan_opts}")
        # Uncomment below to provide more reporting (but run slower).
        # list(APPEND _envvars "ASAN_OPTIONS=fast_unwind_on_malloc=0")

        set_tests_properties(
            ${_exe_target}
            PROPERTIES
            ENVIRONMENT
            "${_envvars}"
        )
    endif ()

else ()
    target_compile_definitions(${_exe_target} PRIVATE DOCTEST_CONFIG_DISABLE)
endif ()

