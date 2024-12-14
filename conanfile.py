from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class vulkandemoRecipe(ConanFile):
    name = "vulkandemo"
    version = "1.0.0"
    package_type = "application"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*"

    requires = [
        "doctest/2.4.11",
        "sdl/2.30.8",
        "spdlog/1.14.1",
        "fmt/10.2.1",
        "ms-gsl/4.1.0",
        "strong_type/v14"
    ]

    settings = "build_type", "os"

    def layout(self):
        cmake_layout(self)

    def generate(self):
        deps = CMakeDeps(self)
        deps.generate()
        tc = CMakeToolchain(self)
        tc.variables["CMAKE_C_VISIBILITY_PRESET"] = "default"
        tc.variables["CMAKE_CXX_VISIBILITY_PRESET"] = "hidden"
        tc.variables["CMAKE_INTERPROCEDURAL_OPTIMIZATION"] = "ON"
        tc.generate()

    def build(self):
        cmake = CMake(self)
        cmake.configure()
        cmake.build()

    def package(self):
        cmake = CMake(self)
        cmake.install()

    

    