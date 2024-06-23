from conan import ConanFile
from conan.tools.cmake import CMake
import os


class GchSmallVectorConan(ConanFile):
    name = "small_vector"
    author = "Gene Harvey <gharveymn@gmail.com>"
    version = "0.9.2"
    license = "MIT"
    url = "https://github.com/gharveymn/small_vector"
    description = "A fully featured single header library implementing a vector container with a small buffer optimization."
    topics = "header-only", "small-vector", "container"

    generators = "CMakeToolchain"
    exports_sources = "CMakeLists.txt", "LICENSE", "source/*"
    no_copy_source = True

    def build(self):
        CMake(self).configure({
            "GCH_SMALL_VECTOR_ENABLE_TESTS":      "OFF",
            "GCH_SMALL_VECTOR_ENABLE_BENCHMARKS": "OFF",
        })

    def package(self):
        CMake(self).install(build_type="Release")

    def package_info(self):
        self.cpp_info.bindirs = []
        self.cpp_info.libdirs = []

        # Use the existing CMake configuration files.
        self.cpp_info.set_property("cmake_find_mode", "none")
        self.cpp_info.builddirs.append(os.path.join("lib", "cmake", "small_vector"))
