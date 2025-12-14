# Distributed under the OSI-approved BSD 3-Clause License.  See accompanying
# file Copyright.txt or https://cmake.org/licensing for details.

cmake_minimum_required(VERSION ${CMAKE_VERSION}) # this file comes with cmake

# If CMAKE_DISABLE_SOURCE_CHANGES is set to true and the source directory is an
# existing directory in our source tree, calling file(MAKE_DIRECTORY) on it
# would cause a fatal error, even though it would be a no-op.
if(NOT EXISTS "D:/projects/pet/ecss_benchmarks/build_clang/_deps/entt-src")
  file(MAKE_DIRECTORY "D:/projects/pet/ecss_benchmarks/build_clang/_deps/entt-src")
endif()
file(MAKE_DIRECTORY
  "D:/projects/pet/ecss_benchmarks/build_clang/_deps/entt-build"
  "D:/projects/pet/ecss_benchmarks/build_clang/_deps/entt-subbuild/entt-populate-prefix"
  "D:/projects/pet/ecss_benchmarks/build_clang/_deps/entt-subbuild/entt-populate-prefix/tmp"
  "D:/projects/pet/ecss_benchmarks/build_clang/_deps/entt-subbuild/entt-populate-prefix/src/entt-populate-stamp"
  "D:/projects/pet/ecss_benchmarks/build_clang/_deps/entt-subbuild/entt-populate-prefix/src"
  "D:/projects/pet/ecss_benchmarks/build_clang/_deps/entt-subbuild/entt-populate-prefix/src/entt-populate-stamp"
)

set(configSubDirs )
foreach(subDir IN LISTS configSubDirs)
    file(MAKE_DIRECTORY "D:/projects/pet/ecss_benchmarks/build_clang/_deps/entt-subbuild/entt-populate-prefix/src/entt-populate-stamp/${subDir}")
endforeach()
if(cfgdir)
  file(MAKE_DIRECTORY "D:/projects/pet/ecss_benchmarks/build_clang/_deps/entt-subbuild/entt-populate-prefix/src/entt-populate-stamp${cfgdir}") # cfgdir has leading slash
endif()
