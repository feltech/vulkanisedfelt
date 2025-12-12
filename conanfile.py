from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake, cmake_layout, CMakeDeps


class vulkandemoRecipe(ConanFile):
    name = "vulkandemo"
    version = "1.0.0"
    package_type = "application"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "src/*"

    # Keep other deps here; remove Boost from this list
    requires = [
        "doctest/2.4.11",
        "sdl/2.30.8",
        "spdlog/1.14.1",
        "fmt/10.2.1",
        "ms-gsl/4.1.0",
        "strong_type/v14",
        "range-v3/0.12.0",
        "frozen/1.2.0",
        "etl/20.39.4",
        "di/1.3.0",
        "reflect-cpp/0.11.1",
        "immer/0.8.1"
    ]

    settings = "build_type", "os"

    def requirements(self):
        # Set True to DISABLE a Boost component (maps to Boost's "without_<comp>=True").
        # NOTE: causes error
        # > ConanException: These libraries were built, but were not used in any boost module:
        # > {'boost_process', 'boost_iostreams', ...
        # So went with "header_only" instead.

        # disable = dict(
        #     atomic=True,
        #     charconv=True,
        #     chrono=True,
        #     cobalt=True,
        #     container=True,
        #     context=True,
        #     contract=True,
        #     coroutine=True,
        #     date_time=True,
        #     exception=True,
        #     fiber=True,
        #     filesystem=True,
        #     graph=True,
        #     graph_parallel=True,
        #     iostreams=True,
        #     json=True,
        #     locale=True,
        #     log=True,
        #     math=True,
        #     mpi=True,
        #     nowide=True,
        #     process=True,
        #     program_options=True,
        #     python=True,
        #     random=True,
        #     regex=True,
        #     serialization=True,
        #     stacktrace=True,
        #     test=True,
        #     thread=True,
        #     timer=True,
        #     type_erasure=True,
        #     url=True,
        #     wave=True,
        # )
        # opts = {f"without_{k}": v for k, v in disable.items() if v}
        opts = {"header_only": True}
        self.requires("boost/1.89.0", options=opts)

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