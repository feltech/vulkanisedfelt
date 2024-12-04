cmake_minimum_required(VERSION 3.28)
project(vulkandemo CXX)

####################################################################################################
# Options

option(${PROJECT_NAME}_ENABLE_TESTS "Enable unit tests" OFF)


####################################################################################################
# Dependencies

find_package(doctest REQUIRED)
find_package(SDL2 REQUIRED)
find_package(spdlog REQUIRED)
find_package(Vulkan REQUIRED)


####################################################################################################
# Create target

set(_exe_target vulkandemo)

add_executable(${_exe_target} src/vulkandemo.cpp src/main.cpp)

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
    doctest::doctest SDL2::SDL2 spdlog::spdlog Vulkan::Vulkan
)

target_compile_definitions(
    ${_exe_target}
    PRIVATE
    SPDLOG_ACTIVE_LEVEL=$<IF:$<CONFIG:Debug>,SPDLOG_LEVEL_TRACE,SPDLOG_LEVEL_INFO>
)

# Testing

if (${PROJECT_NAME}_ENABLE_TESTS)
    enable_testing()
    add_test(
        ${_exe_target}
        ${_exe_target}
        # Do not continue on to run the application after running tests.
        --exit=true
    )
else ()
    target_compile_definitions(${_exe_target} PRIVATE DOCTEST_CONFIG_DISABLE)
endif ()

