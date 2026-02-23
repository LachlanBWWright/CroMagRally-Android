# SDL3Config.cmake - build-tree shim
#
# When SDL3 is compiled via add_subdirectory("extern/SDL") the SDL3::SDL3
# CMake target already exists in the global target registry.  This shim lets
# any submodule (e.g. Pomme) that calls find_package(SDL3 REQUIRED) succeed
# without needing SDL3 to be installed or for an SDL3Config.cmake to exist in
# the binary output directory.
#
# SDL3_DIR is set to this directory (cmake/) in CMakeLists.txt immediately
# after the add_subdirectory(SDL3) call.

cmake_minimum_required(VERSION 3.13)

if(TARGET SDL3::SDL3)
    # Already set up by add_subdirectory – nothing to do.
    set(SDL3_FOUND TRUE)
elseif(TARGET SDL3-shared)
    if(NOT TARGET SDL3::SDL3-shared)
        add_library(SDL3::SDL3-shared ALIAS SDL3-shared)
    endif()
    if(NOT TARGET SDL3::SDL3)
        add_library(SDL3::SDL3 ALIAS SDL3-shared)
    endif()
    set(SDL3_FOUND TRUE)
elseif(TARGET SDL3)
    if(NOT TARGET SDL3::SDL3)
        add_library(SDL3::SDL3 ALIAS SDL3)
    endif()
    set(SDL3_FOUND TRUE)
else()
    set(SDL3_FOUND FALSE)
    if(SDL3_FIND_REQUIRED)
        message(FATAL_ERROR
            "SDL3Config.cmake shim: SDL3 was expected to be built via "
            "add_subdirectory but neither SDL3::SDL3, SDL3-shared nor SDL3 "
            "target was found.  Make sure add_subdirectory(extern/SDL) runs "
            "before add_subdirectory(extern/Pomme).")
    endif()
endif()

set(SDL3_VERSION "3.0.0")  # placeholder; version doesn't matter – targets already exist
