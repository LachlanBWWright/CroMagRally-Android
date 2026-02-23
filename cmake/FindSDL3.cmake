# FindSDL3.cmake - Find SDL3 when it was built via add_subdirectory("extern/SDL")
#
# When SDL3 is compiled from source via add_subdirectory, its targets
# (SDL3-shared, SDL3::SDL3-shared, SDL3::SDL3, SDL3::Headers) are present in
# the global CMake target registry.  Module-mode finder scripts in
# CMAKE_MODULE_PATH are resolved BEFORE config-mode search paths, so this
# module is found even when CMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY (as set by
# the Android NDK toolchain), which would otherwise prevent CMake from locating
# the build-tree SDL3Config.cmake via the config-mode search.

if(TARGET SDL3::SDL3)
    set(SDL3_FOUND TRUE)
    set(SDL3_LIBRARIES SDL3::SDL3)
elseif(TARGET SDL3-shared)
    if(NOT TARGET SDL3::SDL3-shared)
        add_library(SDL3::SDL3-shared ALIAS SDL3-shared)
    endif()
    if(NOT TARGET SDL3::SDL3)
        add_library(SDL3::SDL3 ALIAS SDL3-shared)
    endif()
    set(SDL3_FOUND TRUE)
    set(SDL3_LIBRARIES SDL3::SDL3)
elseif(TARGET SDL3::SDL3-shared)
    if(NOT TARGET SDL3::SDL3)
        add_library(SDL3::SDL3 ALIAS SDL3::SDL3-shared)
    endif()
    set(SDL3_FOUND TRUE)
    set(SDL3_LIBRARIES SDL3::SDL3)
else()
    set(SDL3_FOUND FALSE)
    if(SDL3_FIND_REQUIRED)
        message(FATAL_ERROR
            "FindSDL3.cmake: SDL3 targets not found. "
            "Ensure add_subdirectory(extern/SDL) runs before find_package(SDL3).")
    endif()
endif()

if(SDL3_FOUND)
    set(SDL3_VERSION "3.0.0")
endif()
